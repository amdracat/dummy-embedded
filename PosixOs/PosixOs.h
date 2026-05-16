#ifndef POSIX_OS_H
#define POSIX_OS_H

#include <stdint.h>

#define OS_TIMER_MAX 10

typedef struct {
    uint32_t MsgId;
    void *Data;
} Message;

typedef void (*TimerCallback)(void* arg);
typedef uint32_t TimerHandle;

typedef enum {
    LOCK_ID_TEMPERATURE_SENSOR,
    LOCK_ID_CHARGE_STATE,
    LOCK_ID_MAX
} LockID;

void PosixOs_Init(void);
void PosixOs_CreateTask(void (*taskFunc)(void), const char *name);
void PosixOs_CreateMsgQueues(uint32_t queueId, uint32_t queueCount);
void PosixOs_GetMsg(uint32_t queueId, Message *msg);
void PosixOs_SendMsg(uint32_t queueId, uint32_t msgId, void *data);

//同時実行数はOS_TIMER_MAXまで。そこまでは同時実行を許容すること
TimerHandle PosixOs_SetupTimer(TimerCallback callback, uint32_t intervalMs, void* arg);
void PosixOs_CancelTimer(TimerHandle handle);

/* イベント */
typedef int os_event_id_t;
typedef void (*os_event_cb_t)(void *arg);

void PosixOs_EventSubscribe(os_event_id_t id, os_event_cb_t cb, void *arg);
void PosixOs_EventPublish(os_event_id_t id);

void PosixOs_Lock(LockID id);
void PosixOs_Unlock(LockID id);


#endif /* POSIX_OS_H */
