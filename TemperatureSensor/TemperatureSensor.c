#include "TemperatureSensor.h"
#include "PosixOs.h"
#include "OsTestLayer.h"
#include "I2cDrv.h"
#include <stdio.h>
#include <stdint.h>

#define TEMP_QUEUE_ID 1
#define TEMP_QUEUE_COUNT 1
#define TEMP_REFRESH_MS 1000

static i2c_bus_t s_i2c_bus;
static int s_current_temperature = 0;

typedef void (*TemperatureTaskFunc)(void *data);

static void temperature_task(void);
static void temperature_timer_callback(void *arg);
static void HandleTemperatureMessageRefresh(void *data);
static TemperatureTaskFunc search_temperature_function(uint32_t msgId);
static void SendMsgWrapper_TaskTemperature(uint32_t msgId, void *data);

static const struct {
    uint32_t msgId;
    TemperatureTaskFunc handler;
} s_temperature_message_table[] = {
    { 0, HandleTemperatureMessageRefresh },
};

void TemperatureSensor_Init()
{
    I2cDrv_Init(&s_i2c_bus);
    PosixOs_CreateMsgQueues(TEMP_QUEUE_ID, TEMP_QUEUE_COUNT);
    PosixOs_CreateTask(temperature_task, "TemperatureSensor");
    OsTestLayer_SetTimer(TEMP_REFRESH_MS, temperature_timer_callback, NULL);
}

int TemperatureSensor_GetTemperature()
{
    uint8_t buffer[2] = {0, 0};
    I2cDrv_Read(&s_i2c_bus, 0x48, 0x00, buffer, sizeof(buffer));
    int16_t raw = (int16_t)((buffer[1] << 8) | buffer[0]);
    raw >>= 4;

    PosixOs_Lock(LOCK_ID_TEMPERATURE_SENSOR);
    s_current_temperature = raw;
    PosixOs_Unlock(LOCK_ID_TEMPERATURE_SENSOR);

    return s_current_temperature;
}

static void temperature_task(void)
{
    Message msg;
    while (1) {
        PosixOs_GetMsg(TEMP_QUEUE_ID, &msg);
        if (msg.MsgId == 0) {
            HandleTemperatureMessageRefresh(NULL);
        }
    }
}

static void HandleTemperatureMessageRefresh(void *data)
{
    (void)data;
    uint8_t buffer[2] = {0, 0};
    I2cDrv_Read(&s_i2c_bus, 0x48, 0x00, buffer, sizeof(buffer));
    int16_t raw = (int16_t)((buffer[1] << 8) | buffer[0]);
    raw >>= 4;

    PosixOs_Lock(LOCK_ID_TEMPERATURE_SENSOR);
    s_current_temperature = raw;
    PosixOs_Unlock(LOCK_ID_TEMPERATURE_SENSOR);

    PosixOs_EventPublish(TEMPERATURE_UPDATE_EVENT);
    OsTestLayer_SetTimer(TEMP_REFRESH_MS, temperature_timer_callback, NULL);
}

static TemperatureTaskFunc search_temperature_function(uint32_t msgId)
{
    for (size_t i = 0; i < sizeof(s_temperature_message_table) / sizeof(s_temperature_message_table[0]); ++i) {
        if (s_temperature_message_table[i].msgId == msgId) {
            return s_temperature_message_table[i].handler;
        }
    }
    return NULL;
}

static void SendMsgWrapper_TaskTemperature(uint32_t msgId, void *data)
{
#if defined(OS_TEST_LAYER_ENABLE)
    TemperatureTaskFunc handler = search_temperature_function(msgId);
    if (handler != NULL) {
        OsTestLayer_Post(handler, data, sizeof(data));
    }
#else
    PosixOs_SendMsg(TEMP_QUEUE_ID, msgId, data);
#endif
}

static void temperature_timer_callback(void *arg)
{
    (void)arg;
    SendMsgWrapper_TaskTemperature(0, NULL);
}
