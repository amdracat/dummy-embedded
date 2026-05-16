#include <time.h>
#include "PosixOs.h"
#include "GpioDrv.h"
#include "I2cDrv.h"
#include "Motor.h"
#include "TemperatureSensor.h"
#include "ChargeCtrl.h"
#include "Command.h"

#include "OsTestLayer.h"

int main(void)
{
    PosixOs_Init();
    Motor_Init();
    TemperatureSensor_Init();
    ChargeCtrl_Init();
    Command_Init();

#if defined OS_TEST_LAYER_ENABLE
    Command_SyncTest();
#else
    while (1) {
        struct timespec ts = {1, 0};
        nanosleep(&ts, NULL);
    }
#endif
    return 0;
}
