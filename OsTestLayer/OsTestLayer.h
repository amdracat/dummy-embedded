#ifndef OS_TEST_LAYER_H
#define OS_TEST_LAYER_H

#include <stdint.h>
#include <stdbool.h>
#define OS_TEST_LAYER_ENABLE

typedef void (*os_job_fn_t)(void *arg);
typedef uint32_t TimerHandle;

/* 初期化 */
void OsTestLayer_Init(void);

/* 非同期実行 */
void OsTestLayer_Post(os_job_fn_t fn, void *arg, uint16_t size);

/* タイマー */
TimerHandle OsTestLayer_SetTimer(uint32_t delay_ms, os_job_fn_t fn, void *arg);
void OsTestLayer_CancelTimer(TimerHandle handle);

/* 手動スケジューリング（OS_MODE_MANUAL） */
void OsTestLayer_Sim_RunOne(void);
void OsTestLayer_Sim_RunAll(void);
bool OsTestLayer_Sim_HasPending(void);
uint32_t OsTestLayer_Sim_NowMs(void);
void OsTestLayer_Sim_AdvanceTime(uint32_t ms);

#endif /* OS_TEST_LAYER_H */