#ifndef POSIX_OS_H
#define POSIX_OS_H

#include <pthread.h>
#include <stdint.h>

#define OS_TIMER_MAX 10

typedef struct {
    uint32_t MsgId;
    void *Data;
} Message;

typedef struct {
    Message *queue;
    int head;
    int tail;
    int count;
    int capacity;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
} MessageQueue;

typedef void (*TimerCallback)(void* arg);

void PosixOs_Init(void);
void PosixOs_CreateTask(void (*taskFunc)(void), const char *name);
void PosixOs_CreateMsgQueues(uint32_t queueId, uint32_t queueCount);
void PosixOs_GetMsg(uint32_t queueId, Message *msg);
void PosixOs_SendMsg(uint32_t queueId, uint32_t msgId, void *data);

//同時実行数はOS_TIMER_MAXまで。そこまでは同時実行を許容すること
void PosixOs_SetupTimer(TimerCallback callback, uint32_t intervalMs, void* arg);

/* イベント */
typedef int os_event_id_t;
typedef void (*os_event_cb_t)(void *arg);

void PosixOs_EventSubscribe(os_event_id_t id, os_event_cb_t cb, void *arg);
void PosixOs_EventPublish(os_event_id_t id);


#endif /* POSIX_OS_H */
