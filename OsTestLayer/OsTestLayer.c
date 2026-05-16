#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "OsTestLayer.h"

/* ネイティブOS関数*/
#include "PosixOs.h"

/* =========================
 * 内部ジョブキュー
 * ========================= */
#define JOB_DATA_SIZE 64
typedef struct job {
    os_job_fn_t fn;
    void *arg;
    struct job *next;
} job_t;

static job_t *job_head = NULL;
static job_t *job_tail = NULL;

static uint32_t g_time_ms = 0;

/* =========================
 * API実装
 * ========================= */

void OsTestLayer_Init(void) {
#if defined (OS_TEST_LAYER_ENABLE)
    /*処理があれば記載*/
#endif /* OS_TEST_LAYER_ENABLE */
}


/* 非同期実行 */
void OsTestLayer_Post(os_job_fn_t fn, void *arg, uint16_t size) {
#if defined (OS_TEST_LAYER_ENABLE)
    job_t *job = malloc(sizeof(job_t));
    job->fn = fn;
    job->arg = arg;
    job->next = NULL;

    if (job_tail) {
        job_tail->next = job;
    } else {
        job_head = job;
    }
    job_tail = job;
#endif
}


/* =========================
 * タイマー管理
 * ========================= */

typedef struct timer {
    uint32_t trigger_time;
    os_job_fn_t fn;
    void *arg;
    uint32_t handle;
    struct timer *next;
} timer_t;

static timer_t *timer_head = NULL;
static uint32_t s_next_timer_handle = 1;


#include <stdio.h>

TimerHandle OsTestLayer_SetTimer(uint32_t delay_ms, os_job_fn_t fn, void *arg) {
#if defined (OS_TEST_LAYER_ENABLE)
    timer_t *timer = malloc(sizeof(timer_t));
    if (timer == NULL) {
        return 0;
    }
    timer->trigger_time = g_time_ms + delay_ms;
    timer->fn = fn;
    timer->arg = arg;
    timer->handle = s_next_timer_handle++;
    timer->next = timer_head;
    timer_head = timer;
    return timer->handle;
#else
    /* OSのネイティブタイマーを呼び出す */
    return PosixOs_SetupTimer(fn, delay_ms, arg);
#endif /* OS_TEST_LAYER_ENABLE */
}

void OsTestLayer_CancelTimer(TimerHandle handle)
{
#if defined (OS_TEST_LAYER_ENABLE)
    if (handle == 0) return;
    timer_t **pp = &timer_head;
    while (*pp) {
        timer_t *t = *pp;
        if (t->handle == handle) {
            *pp = t->next;
            free(t);
            return;
        }
        pp = &t->next;
    }
    /* not found -> maybe already fired or posted; nothing to do */
#else
    PosixOs_CancelTimer(handle);
#endif
}

void OsTestLayer_SyncSleep(uint32_t ms)
{
#if defined (OS_TEST_LAYER_ENABLE)
#else
    PosixOs_Sleep(ms);
#endif
}

/* 手動スケジューリング（OS_MODE_MANUAL） */

void OsTestLayer_Sim_RunOne(void)
{
#if defined (OS_TEST_LAYER_ENABLE)
    if (job_head == NULL) {
        return;
    }

    job_t *job = job_head;
    job_head = job->next;
    if (job_head == NULL) {
        job_tail = NULL;
    }
    job->fn(job->arg);
    free(job);
#endif /* OS_TEST_LAYER_ENABLE */
}

void OsTestLayer_Sim_RunAll(void)
{
#if defined (OS_TEST_LAYER_ENABLE)
    while (OsTestLayer_Sim_HasPending()) {
        OsTestLayer_Sim_RunOne();
    }
#endif /* OS_TEST_LAYER_ENABLE */
}

bool OsTestLayer_Sim_HasPending(void)
{
#if defined (OS_TEST_LAYER_ENABLE)
    bool has = (job_head != NULL);
    return has;
#else
    return false;
#endif /* OS_TEST_LAYER_ENABLE */
}

uint32_t OsTestLayer_Sim_NowMs(void)
{
#if defined (OS_TEST_LAYER_ENABLE)
    return g_time_ms;
#else
    return 0;
#endif /* OS_TEST_LAYER_ENABLE */
}

void OsTestLayer_Sim_AdvanceTime(uint32_t ms)
{
#if defined (OS_TEST_LAYER_ENABLE)
    uint32_t target_time = g_time_ms + ms;

    // 期限切れのタイマーを発火時刻の昇順で処理する。
    // 各タイマー発火時点で g_time_ms を更新してからジョブを投入し、
    // ジョブが新たなタイマーを登録する可能性があるためループで再検査する。
    while (1) {
        timer_t *prev = NULL;
        timer_t *cur = timer_head;
        timer_t *earliest = NULL;
        timer_t *earliest_prev = NULL;

        // 最も早い期限切れタイマーを探す
        while (cur) {
            if (cur->trigger_time <= target_time) {
                if (earliest == NULL || cur->trigger_time < earliest->trigger_time) {
                    earliest = cur;
                    earliest_prev = prev;
                }
            }
            prev = cur;
            cur = cur->next;
        }

        if (earliest == NULL) {
            break; // 期限切れタイマーはもうない
        }

        // リストから earliest を削除
        if (earliest_prev) {
            earliest_prev->next = earliest->next;
        } else {
            timer_head = earliest->next;
        }

        // 時刻をタイマーの発火時刻に合わせ、ジョブを投入して実行
        g_time_ms = earliest->trigger_time;
        OsTestLayer_Post(earliest->fn, earliest->arg, sizeof(earliest->arg));
        free(earliest);

        // ジョブを実行して、コールバック内で登録されたタイマーを処理可能にする
        OsTestLayer_Sim_RunAll();
    }

    // 最後に目標時刻まで進める
    g_time_ms = target_time;
#else
    (void)ms;
#endif /* OS_TEST_LAYER_ENABLE */
}
