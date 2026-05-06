#include "TemperatureSensor.h"
#include "PosixOs.h"
#include "I2cDrv.h"
#include <stdio.h>
#include <stdint.h>

#define TEMP_QUEUE_ID 1
#define TEMP_QUEUE_COUNT 1
#define TEMP_REFRESH_MS 1000

static i2c_bus_t s_i2c_bus;
static int s_current_temperature = 0;

static void temperature_task(void);
static void temperature_timer_callback(void *arg);

void TemperatureSensor_Init()
{
    I2cDrv_Init(&s_i2c_bus);
    PosixOs_CreateMsgQueues(TEMP_QUEUE_ID, TEMP_QUEUE_COUNT);
    PosixOs_CreateTask(temperature_task, "TemperatureSensor");
    PosixOs_SetupTimer(temperature_timer_callback, TEMP_REFRESH_MS, NULL);
}

int TemperatureSensor_GetTemperature()
{
    return s_current_temperature;
}

static void temperature_task(void)
{
    Message msg;
    while (1) {
        PosixOs_GetMsg(TEMP_QUEUE_ID, &msg);
        uint8_t buffer[2] = {0, 0};
        I2cDrv_Read(&s_i2c_bus, 0x48, 0x00, buffer, sizeof(buffer));
        int16_t raw = (int16_t)((buffer[1] << 8) | buffer[0]);
        raw >>= 4;
        s_current_temperature = raw;
        printf("[TEMP] temperature=%d\n", s_current_temperature);
        PosixOs_EventPublish(TEMPERATURE_UPDATE_EVENT);
        PosixOs_SetupTimer(temperature_timer_callback, TEMP_REFRESH_MS, NULL);
    }
}

static void temperature_timer_callback(void *arg)
{
    (void)arg;
    PosixOs_SendMsg(TEMP_QUEUE_ID, 0, NULL);
}
