#include "Test_Motor.h"
#include "UnitTestFrame.h"
#include "Motor.h"
#include "I2cDrv.h"
#include <time.h>
#include <stdint.h>

static void sleep_ms(int ms)
{
    struct timespec ts = {ms / 1000, (ms % 1000) * 1000000};
    nanosleep(&ts, NULL);
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
    Motor_SetMode(MOTOR_MODE_LEFT);
    sleep_ms(500);

    ASSERT_EQ(MOTOR_SPEED_FAST, Motor_GetSpeed());
    ASSERT_EQ(MOTOR_MODE_LEFT, Motor_GetMode());

    UnitTestFrame_ReportResult();
}
