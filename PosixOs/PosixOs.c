#include "PosixOs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#define MAX_MESSAGE_QUEUES 32
#define MESSAGE_QUEUE_CAPACITY 32
#define MAX_EVENTS 32

typedef struct {
    Message queue[MESSAGE_QUEUE_CAPACITY];
    int head;
    int tail;
    int count;
    int capacity;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    int initialized;
} InternalQueue;

typedef struct {
    void (*taskFunc)(void);
} TaskContext;

typedef struct {
    TimerCallback callback;
    uint32_t intervalMs;
    void *arg;
} TimerContext;

typedef struct {
    os_event_id_t id;
    os_event_cb_t cb;
    void *arg;
    int used;
} EventSubscriber;

static InternalQueue s_queues[MAX_MESSAGE_QUEUES];
static EventSubscriber s_subscribers[MAX_EVENTS];
static pthread_mutex_t s_event_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t s_timer_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_timer_cond = PTHREAD_COND_INITIALIZER;
static int s_timer_count = 0;
static int s_initialized = 0;
static pthread_mutex_t s_init_lock = PTHREAD_MUTEX_INITIALIZER;

static void *task_entry(void *arg)
{
    TaskContext *context = (TaskContext *)arg;
    if (context && context->taskFunc) {
        context->taskFunc();
    }
    free(context);
    return NULL;
}

static void *timer_entry(void *arg)
{
    TimerContext *context = (TimerContext *)arg;
    if (!context) {
        return NULL;
    }

    struct timespec delay;
    delay.tv_sec = context->intervalMs / 1000;
    delay.tv_nsec = (context->intervalMs % 1000) * 1000000;
    nanosleep(&delay, NULL);

    context->callback(context->arg);

    pthread_mutex_lock(&s_timer_lock);
    s_timer_count--;
    pthread_cond_signal(&s_timer_cond);
    pthread_mutex_unlock(&s_timer_lock);

    free(context);
    return NULL;
}

static void ensure_initialized(void)
{
    pthread_mutex_lock(&s_init_lock);
    if (!s_initialized) {
        for (int i = 0; i < MAX_MESSAGE_QUEUES; i++) {
            s_queues[i].initialized = 0;
        }
        for (int i = 0; i < MAX_EVENTS; i++) {
            s_subscribers[i].used = 0;
        }
        s_timer_count = 0;
        s_initialized = 1;
    }
    pthread_mutex_unlock(&s_init_lock);
}

void PosixOs_Init(void)
{
    ensure_initialized();
}

void PosixOs_CreateTask(void (*taskFunc)(void), const char *name)
{
    ensure_initialized();
    if (!taskFunc) {
        return;
    }

    TaskContext *context = (TaskContext *)malloc(sizeof(TaskContext));
    if (!context) {
        return;
    }
    context->taskFunc = taskFunc;

    pthread_t thread;
    if (pthread_create(&thread, NULL, task_entry, context) == 0) {
        pthread_detach(thread);
    } else {
        free(context);
    }
}

void PosixOs_CreateMsgQueues(uint32_t queueId, uint32_t queueCount)
{
    ensure_initialized();
    for (uint32_t i = 0; i < queueCount; i++) {
        uint32_t index = queueId + i;
        if (index >= MAX_MESSAGE_QUEUES) {
            break;
        }
        InternalQueue *queue = &s_queues[index];
        if (!queue->initialized) {
            queue->head = 0;
            queue->tail = 0;
            queue->count = 0;
            queue->capacity = MESSAGE_QUEUE_CAPACITY;
            pthread_mutex_init(&queue->lock, NULL);
            pthread_cond_init(&queue->not_empty, NULL);
            pthread_cond_init(&queue->not_full, NULL);
            queue->initialized = 1;
        }
    }
}

void PosixOs_GetMsg(uint32_t queueId, Message *msg)
{
    ensure_initialized();
    if (queueId >= MAX_MESSAGE_QUEUES || !msg) {
        return;
    }

    InternalQueue *queue = &s_queues[queueId];
    if (!queue->initialized) {
        return;
    }

    pthread_mutex_lock(&queue->lock);
    while (queue->count == 0) {
        pthread_cond_wait(&queue->not_empty, &queue->lock);
    }

    *msg = queue->queue[queue->head];
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;
    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->lock);
}

void PosixOs_SendMsg(uint32_t queueId, uint32_t msgId, void *data)
{
    ensure_initialized();
    if (queueId >= MAX_MESSAGE_QUEUES) {
        return;
    }

    InternalQueue *queue = &s_queues[queueId];
    if (!queue->initialized) {
        return;
    }

    pthread_mutex_lock(&queue->lock);
    while (queue->count == queue->capacity) {
        pthread_cond_wait(&queue->not_full, &queue->lock);
    }

    queue->queue[queue->tail].MsgId = msgId;
    queue->queue[queue->tail].Data = data;
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->count++;
    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->lock);
}

void PosixOs_SetupTimer(TimerCallback callback, uint32_t intervalMs, void *arg)
{
    ensure_initialized();
    if (!callback) {
        return;
    }

    pthread_mutex_lock(&s_timer_lock);
    while (s_timer_count >= OS_TIMER_MAX) {
        pthread_cond_wait(&s_timer_cond, &s_timer_lock);
    }
    s_timer_count++;
    pthread_mutex_unlock(&s_timer_lock);

    TimerContext *context = (TimerContext *)malloc(sizeof(TimerContext));
    if (!context) {
        pthread_mutex_lock(&s_timer_lock);
        s_timer_count--;
        pthread_cond_signal(&s_timer_cond);
        pthread_mutex_unlock(&s_timer_lock);
        return;
    }
    context->callback = callback;
    context->intervalMs = intervalMs;
    context->arg = arg;

    pthread_t thread;
    if (pthread_create(&thread, NULL, timer_entry, context) == 0) {
        pthread_detach(thread);
    } else {
        free(context);
        pthread_mutex_lock(&s_timer_lock);
        s_timer_count--;
        pthread_cond_signal(&s_timer_cond);
        pthread_mutex_unlock(&s_timer_lock);
    }
}

void PosixOs_EventSubscribe(os_event_id_t id, os_event_cb_t cb, void *arg)
{
    ensure_initialized();
    if (!cb) {
        return;
    }

    pthread_mutex_lock(&s_event_lock);
    for (int i = 0; i < MAX_EVENTS; i++) {
        if (!s_subscribers[i].used) {
            s_subscribers[i].used = 1;
            s_subscribers[i].id = id;
            s_subscribers[i].cb = cb;
            s_subscribers[i].arg = arg;
            break;
        }
    }
    pthread_mutex_unlock(&s_event_lock);
}

void PosixOs_EventPublish(os_event_id_t id)
{
    ensure_initialized();
    EventSubscriber copy[MAX_EVENTS];
    int count = 0;

    pthread_mutex_lock(&s_event_lock);
    for (int i = 0; i < MAX_EVENTS; i++) {
        if (s_subscribers[i].used && s_subscribers[i].id == id) {
            copy[count++] = s_subscribers[i];
        }
    }
    pthread_mutex_unlock(&s_event_lock);

    for (int i = 0; i < count; i++) {
        if (copy[i].cb) {
            copy[i].cb(copy[i].arg);
        }
    }
}
