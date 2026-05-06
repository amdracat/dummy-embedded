#include <time.h>
#include "PosixOs.h"
#include "GpioDrv.h"
#include "I2cDrv.h"
#include "Motor.h"
#include "TemperatureSensor.h"
#include "Command.h"

int main(void)
{
    PosixOs_Init();
    Motor_Init();
    TemperatureSensor_Init();
    Command_Init();

    while (1) {
        struct timespec ts = {1, 0};
        nanosleep(&ts, NULL);
    }

    return 0;
}
