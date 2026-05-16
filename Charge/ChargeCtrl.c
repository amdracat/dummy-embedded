#include "ChargeCtrl.h"
#include "ChargeState.h"
#include "PosixOs.h"
#include "OsTestLayer.h"
#include "TemperatureSensor.h"
#include <stdio.h>
#include <stdint.h>

#define CHARGE_QUEUE_ID 3
#define CHARGE_QUEUE_COUNT 1
#define CHARGE_TEMP_CHECK_MS 1000

static int s_initialized = 0;

typedef void (*ChargeTaskFunc)(void *data);

static void charge_task(void);
static void process_message(const Message *msg);
static void temperature_timer_callback(void *arg);
static void HandleChargeMessageState(void *data);
static void HandleChargeMessageUsbChargeable(void *data);
static ChargeTaskFunc search_charge_function(uint32_t msgId);
static void SendMsgWrapper_TaskCharge(uint32_t msgId, void *data);

static const struct {
    uint32_t msgId;
    ChargeTaskFunc handler;
} s_charge_message_table[] = {
    { CHARGE_MSG_STATE, HandleChargeMessageState },
    { CHARGE_MSG_SET_USB_CHARGEABLE, HandleChargeMessageUsbChargeable },
};

void ChargeCtrl_Init(void)
{
    if (s_initialized) {
        return;
    }

    PosixOs_CreateMsgQueues(CHARGE_QUEUE_ID, CHARGE_QUEUE_COUNT);
    PosixOs_CreateTask(charge_task, "ChargeCtrl");
    ChargeState_Init();
    OsTestLayer_SetTimer(CHARGE_TEMP_CHECK_MS, temperature_timer_callback, NULL);
    s_initialized = 1;
}

void ChargeCtrl_SetUsbChargeable(int chargeable)
{
    if (!s_initialized) {
        return;
    }
    SendMsgWrapper_TaskCharge(CHARGE_MSG_SET_USB_CHARGEABLE, (void *)(intptr_t)chargeable);
}

void ChargeCtrl_NotifyUsbInserted(void)
{
    if (!s_initialized) {
        return;
    }
    SendMsgWrapper_TaskCharge(CHARGE_MSG_STATE, (void *)(intptr_t)CHARGE_EVENT_USB_INSERTED);
}

void ChargeCtrl_NotifyUsbRemoved(void)
{
    if (!s_initialized) {
        return;
    }
    SendMsgWrapper_TaskCharge(CHARGE_MSG_STATE, (void *)(intptr_t)CHARGE_EVENT_USB_REMOVED);
}

void ChargeCtrl_NotifyBatteryStopRequest(void)
{
    if (!s_initialized) {
        return;
    }
    SendMsgWrapper_TaskCharge(CHARGE_MSG_STATE, (void *)(intptr_t)CHARGE_EVENT_BATTERY_STOP_REQUEST);
}

void ChargeCtrl_NotifyBatteryAllowRequest(void)
{
    if (!s_initialized) {
        return;
    }
    SendMsgWrapper_TaskCharge(CHARGE_MSG_STATE, (void *)(intptr_t)CHARGE_EVENT_BATTERY_ALLOW_REQUEST);
}

void ChargeCtrl_NotifyBatteryComplete(void)
{
    if (!s_initialized) {
        return;
    }
    SendMsgWrapper_TaskCharge(CHARGE_MSG_STATE, (void *)(intptr_t)CHARGE_EVENT_BATTERY_COMPLETE);
}

void ChargeCtrl_NotifyHostStop(void)
{
    if (!s_initialized) {
        return;
    }
    SendMsgWrapper_TaskCharge(CHARGE_MSG_STATE, (void *)(intptr_t)CHARGE_EVENT_HOST_STOP);
}

void ChargeCtrl_NotifyHostAllow(void)
{
    if (!s_initialized) {
        return;
    }
    SendMsgWrapper_TaskCharge(CHARGE_MSG_STATE, (void *)(intptr_t)CHARGE_EVENT_HOST_ALLOW);
}

void ChargeCtrl_NotifyFatalError(void)
{
    if (!s_initialized) {
        return;
    }
    SendMsgWrapper_TaskCharge(CHARGE_MSG_STATE, (void *)(intptr_t)CHARGE_EVENT_FATAL);
}

void ChargeCtrl_DelayProc(void (*callback)(void *), uint32_t intervalMs, void *arg)
{
    if (callback == NULL) {
        return;
    }

    OsTestLayer_SetTimer(intervalMs, callback, arg);
}

ChargeCtrl_State ChargeCtrl_GetState(void)
{
    return ChargeState_GetState();
}

int ChargeCtrl_IsChargingActive(void)
{
    return ChargeState_IsChargingActive();
}

static void charge_task(void)
{
    Message msg;

    while (1) {
        PosixOs_GetMsg(CHARGE_QUEUE_ID, &msg);
        process_message(&msg);
    }
}

static void process_message(const Message *msg)
{
    if (msg == NULL) {
        return;
    }

    switch (msg->MsgId) {
    case CHARGE_MSG_STATE:
        ChargeState_ProcessEvent((ChargeCtrl_Event)(intptr_t)msg->Data);
        break;

    case CHARGE_MSG_SET_USB_CHARGEABLE:
        ChargeState_SetUsbChargeable((int)(intptr_t)msg->Data);
        break;

    default:
        break;
    }
}

static void HandleChargeMessageState(void *data)
{
    ChargeState_ProcessEvent((ChargeCtrl_Event)(intptr_t)data);
}

static void HandleChargeMessageUsbChargeable(void *data)
{
    ChargeState_SetUsbChargeable((int)(intptr_t)data);
}

static ChargeTaskFunc search_charge_function(uint32_t msgId)
{
    for (size_t i = 0; i < sizeof(s_charge_message_table) / sizeof(s_charge_message_table[0]); ++i) {
        if (s_charge_message_table[i].msgId == msgId) {
            return s_charge_message_table[i].handler;
        }
    }
    return NULL;
}

static void SendMsgWrapper_TaskCharge(uint32_t msgId, void *data)
{
#if defined(OS_TEST_LAYER_ENABLE)
    ChargeTaskFunc handler = search_charge_function(msgId);
    if (handler != NULL) {
        OsTestLayer_Post(handler, data, sizeof(data));
    }
#else
    PosixOs_SendMsg(CHARGE_QUEUE_ID, msgId, data);
#endif
}


static void temperature_timer_callback(void *arg)
{
    (void)arg;
    int temperature = TemperatureSensor_GetTemperature();

    if (temperature >= 50) {
        SendMsgWrapper_TaskCharge(CHARGE_MSG_STATE, (void *)(intptr_t)CHARGE_EVENT_TEMP_HIGH);
    } else if (temperature <= 45) {
        SendMsgWrapper_TaskCharge(CHARGE_MSG_STATE, (void *)(intptr_t)CHARGE_EVENT_TEMP_LOW);
    }
    OsTestLayer_SetTimer(CHARGE_TEMP_CHECK_MS, temperature_timer_callback, NULL);
}
