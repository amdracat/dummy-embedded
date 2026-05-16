#include <time.h>
#include "Test_TimerCancel.h"
#include "UnitTestFrame.h"
#include "OsTestLayer.h"

static int s_fired = 0;

static void timer_callback(void *arg)
{
    (void)arg;
    s_fired = 1;
}

static void sleep_ms(int ms)
{
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000 };
    nanosleep(&ts, NULL);
}

static void sleepwrapper_ms(int ms)
{
#if defined(OS_TEST_LAYER_ENABLE)
    OsTestLayer_Sim_RunAll();
    OsTestLayer_Sim_AdvanceTime(ms);
    OsTestLayer_Sim_RunAll();
#else
    sleep_ms(ms);
#endif
}

void Test_TimerCancel(void)
{
    UnitTestFrame_Init();

    s_fired = 0;
    TimerHandle h = OsTestLayer_SetTimer(100, timer_callback, NULL);
    OsTestLayer_CancelTimer(h);

    sleepwrapper_ms(200);

    ASSERT_EQ(0, s_fired);

    UnitTestFrame_ReportResult();
}
