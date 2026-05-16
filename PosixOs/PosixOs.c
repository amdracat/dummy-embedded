#include "PosixOs.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#define MAX_EVENTS 32

typedef struct {
    os_event_id_t id;
    os_event_cb_t cb;
    void *arg;
    int used;
} EventSubscriber;

static EventSubscriber s_subscribers[MAX_EVENTS];
static pthread_mutex_t s_event_lock = PTHREAD_MUTEX_INITIALIZER;
static int s_event_initialized = 0;
static uint32_t s_next_timer_handle = 1;

static void ensure_event_initialized(void)
{
    if (s_event_initialized) return;
    memset(s_subscribers, 0, sizeof(s_subscribers));
    s_event_initialized = 1;
}

#if defined(OS_TEST_LAYER_ENABLE)

void PosixOs_Init(void)
{
    ensure_event_initialized();
}

void PosixOs_CreateTask(void (*taskFunc)(void), const char *name)
{
    (void)taskFunc;
    (void)name;
}

void PosixOs_CreateMsgQueues(uint32_t queueId, uint32_t queueCount)
{
    (void)queueId;
    (void)queueCount;
}

void PosixOs_GetMsg(uint32_t queueId, Message *msg)
{
    (void)queueId;
    (void)msg;
}

void PosixOs_SendMsg(uint32_t queueId, uint32_t msgId, void *data)
{
    (void)queueId;
    (void)msgId;
    (void)data;
}

TimerHandle PosixOs_SetupTimer(TimerCallback callback, uint32_t intervalMs, void *arg)
{
    (void)callback;
    (void)intervalMs;
    (void)arg;
    return s_next_timer_handle++;
}

void PosixOs_CancelTimer(TimerHandle handle)
{
    (void)handle;
}

void PosixOs_EventSubscribe(os_event_id_t id, os_event_cb_t cb, void *arg)
{
    ensure_event_initialized();
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
    ensure_event_initialized();
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

void PosixOs_Lock(LockID id) { (void)id; }
void PosixOs_Unlock(LockID id) { (void)id; }
#else
#define MAX_MESSAGE_QUEUES 32
#define MESSAGE_QUEUE_CAPACITY 32

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

typedef struct TimerContext {
    TimerCallback callback;
    uint32_t intervalMs;
    void *arg;
    TimerHandle handle;
    int active;
    struct TimerContext *next;
} TimerContext;
static InternalQueue s_queues[MAX_MESSAGE_QUEUES];
static TimerContext *s_timer_contexts = NULL;
static pthread_mutex_t s_timer_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_timer_cond = PTHREAD_COND_INITIALIZER;
static int s_timer_count = 0;
static int s_initialized = 0;
static pthread_mutex_t s_init_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t s_lock_objects[LOCK_ID_MAX] = {
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER
};

void PosixOs_Lock(LockID id)
{
    if (id < 0 || id >= LOCK_ID_MAX) {
        return;
    }
    pthread_mutex_lock(&s_lock_objects[id]);
}

void PosixOs_Unlock(LockID id)
{
    if (id < 0 || id >= LOCK_ID_MAX) {
        return;
    }
    pthread_mutex_unlock(&s_lock_objects[id]);
}

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

    int should_fire = 0;
    pthread_mutex_lock(&s_timer_lock);
    if (context->active) {
        should_fire = 1;
        context->active = 0;
    }
    // Remove from active timer list
    TimerContext **pp = &s_timer_contexts;
    while (*pp && *pp != context) {
        pp = &(*pp)->next;
    }
    if (*pp == context) {
        *pp = context->next;
    }
    pthread_mutex_unlock(&s_timer_lock);

    if (should_fire) {
        context->callback(context->arg);
    }

    pthread_mutex_lock(&s_timer_lock);
    s_timer_count--;
    pthread_cond_signal(&s_timer_cond);
    pthread_mutex_unlock(&s_timer_lock);

    free(context);
    return NULL;
}

void PosixOs_CancelTimer(TimerHandle handle)
{
    if (handle == 0) {
        return;
    }

    pthread_mutex_lock(&s_timer_lock);
    TimerContext *cur = s_timer_contexts;
    while (cur) {
        if (cur->handle == handle) {
            cur->active = 0;
            break;
        }
        cur = cur->next;
    }
    pthread_mutex_unlock(&s_timer_lock);
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

TimerHandle PosixOs_SetupTimer(TimerCallback callback, uint32_t intervalMs, void *arg)
{
    ensure_initialized();
    if (!callback) {
        return 0;
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
        return 0;
    }
    context->callback = callback;
    context->intervalMs = intervalMs;
    context->arg = arg;
    context->handle = s_next_timer_handle++;
    context->active = 1;
    context->next = NULL;

    pthread_mutex_lock(&s_timer_lock);
    context->next = s_timer_contexts;
    s_timer_contexts = context;
    pthread_mutex_unlock(&s_timer_lock);

    pthread_t thread;
    if (pthread_create(&thread, NULL, timer_entry, context) == 0) {
        pthread_detach(thread);
    } else {
        pthread_mutex_lock(&s_timer_lock);
        // keep count until thread cleanup? remove from list and decrement count immediately
        TimerContext **pp = &s_timer_contexts;
        while (*pp && *pp != context) {
            pp = &(*pp)->next;
        }
        if (*pp == context) {
            *pp = context->next;
        }
        s_timer_count--;
        pthread_cond_signal(&s_timer_cond);
        pthread_mutex_unlock(&s_timer_lock);
        free(context);
        return 0;
    }

    return context->handle;
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
#endif
