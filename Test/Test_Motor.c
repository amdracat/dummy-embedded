#include "Test_Motor.h"
#include "UnitTestFrame.h"
#include "Motor.h"
#include "I2cDrv.h"
#include "OsTestLayer.h"
#include <time.h>
#include <stdint.h>

static void sleep_ms(int ms)
{
    struct timespec ts = {ms / 1000, (ms % 1000) * 1000000};
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

void Test_MotorTest()
{
    UnitTestFrame_Init();

    int temperature = 20;
    int16_t raw = (int16_t)temperature;
    uint16_t packed = (uint16_t)(raw << 4);
    uint8_t buffer[2] = { (uint8_t)(packed & 0xFF), (uint8_t)(packed >> 8) };
    I2cDrv_DummySetReadData(0x48, 0x00, buffer, sizeof(buffer));

    Motor_SetSpeed(MOTOR_SPEED_FAST);
    sleepwrapper_ms(100);
    ASSERT_EQ(MOTOR_SPEED_FAST, Motor_GetSpeed());

    const MotorMode modes[] = {
        MOTOR_MODE_STOP,
        MOTOR_MODE_LEFT,
        MOTOR_MODE_RIGHT,
        MOTOR_MODE_FRONT,
        MOTOR_MODE_BACK
    };
    const int mode_delays[] = {150, 400, 400, 500, 600};

    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i) {
        Motor_SetMode(modes[i]);
        sleepwrapper_ms(mode_delays[i]);
        ASSERT_EQ(modes[i], Motor_GetMode());
    }

    UnitTestFrame_ReportResult();
}
