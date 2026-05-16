#include "Test_Charge.h"
#include "UnitTestFrame.h"
#include "ChargeCtrl.h"
#include "I2cDrv.h"
#include "OsTestLayer.h"
#include <time.h>
#include <stdint.h>

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

static void set_temperature(int temperature)
{
    int16_t raw = (int16_t)temperature;
    uint16_t packed = (uint16_t)(raw << 4);
    uint8_t buffer[2] = { (uint8_t)(packed & 0xFF), (uint8_t)(packed >> 8) };
    I2cDrv_DummySetReadData(0x48, 0x00, buffer, sizeof(buffer));
}

void Test_ChargeTest(void)
{
    UnitTestFrame_Init();

    ChargeCtrl_Init();

    ChargeCtrl_SetUsbChargeable(1);
    ChargeCtrl_NotifyUsbInserted();
    sleepwrapper_ms(250);
    ASSERT_EQ(CHARGE_STATE_WAIT_START, ChargeCtrl_GetState());

    ChargeCtrl_NotifyBatteryAllowRequest();
    sleepwrapper_ms(600);
    ASSERT_EQ(CHARGE_STATE_CHARGING, ChargeCtrl_GetState());
    ASSERT_EQ(1, ChargeCtrl_IsChargingActive());

    set_temperature(60);
    sleepwrapper_ms(1400);
    ASSERT_EQ(CHARGE_STATE_TEMP_STOP, ChargeCtrl_GetState());
    ASSERT_EQ(0, ChargeCtrl_IsChargingActive());

    set_temperature(40);
    sleepwrapper_ms(1400);
    ASSERT_EQ(CHARGE_STATE_CHARGING, ChargeCtrl_GetState());

    ChargeCtrl_NotifyHostStop();
    sleepwrapper_ms(200);
    ASSERT_EQ(CHARGE_STATE_HOST_STOP, ChargeCtrl_GetState());

    ChargeCtrl_NotifyHostAllow();
    sleepwrapper_ms(600);
    ASSERT_EQ(CHARGE_STATE_CHARGING, ChargeCtrl_GetState());

    ChargeCtrl_NotifyBatteryComplete();
    sleepwrapper_ms(200);
    ASSERT_EQ(CHARGE_STATE_COMPLETE, ChargeCtrl_GetState());

    ChargeCtrl_NotifyUsbRemoved();
    sleepwrapper_ms(200);
    ASSERT_EQ(CHARGE_STATE_DISCONNECTED, ChargeCtrl_GetState());

    ChargeCtrl_SetUsbChargeable(0);
    ChargeCtrl_NotifyUsbInserted();
    sleepwrapper_ms(250);
    ChargeCtrl_NotifyBatteryAllowRequest();
    sleepwrapper_ms(600);
    ASSERT_EQ(CHARGE_STATE_WAIT_START, ChargeCtrl_GetState());

    ChargeCtrl_NotifyUsbRemoved();
    sleepwrapper_ms(200);

    ChargeCtrl_SetUsbChargeable(1);
    ChargeCtrl_NotifyUsbInserted();
    sleepwrapper_ms(250);
    ChargeCtrl_NotifyBatteryAllowRequest();
    sleepwrapper_ms(600);
    ASSERT_EQ(CHARGE_STATE_CHARGING, ChargeCtrl_GetState());

    ChargeCtrl_NotifyFatalError();
    sleepwrapper_ms(200);
    ASSERT_EQ(CHARGE_STATE_ERROR, ChargeCtrl_GetState());

    ChargeCtrl_NotifyUsbRemoved();
    sleepwrapper_ms(200);
    ASSERT_EQ(CHARGE_STATE_DISCONNECTED, ChargeCtrl_GetState());

    UnitTestFrame_ReportResult();
}
