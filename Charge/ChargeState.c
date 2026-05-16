#include "ChargeCtrl.h"
#include "ChargeState.h"
#include "PosixOs.h"
#include "OsTestLayer.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "Queue.h"
#include <stdbool.h>
#define CHARGE_QUEUE_ID 3
#define USB_TYPE_CHECK_DELAY_MS 100
#define CHARGE_START_DELAY_MS 200
#define CHARGE_STOP_DELAY_MS 300

typedef struct State {
    const char *name;
    void (*on_enter)(void);
    void (*on_exit)(void);
    void (*on_event[MAX_CHG_EVENTS])(void);
} State;

/*
# Charge Control State Transitions

| From | Event | To | Notes |
| --- | --- | --- | --- |
| DISCONNECTED | USB_INSERTED | WAIT_START | USB接続後、USB種別判明待ち |
| WAIT_START | USB_TYPE_KNOWN | WAIT_START / CHARGING | 充電可能かつ許可済みなら充電開始 |
| WAIT_START | BATTERY_ALLOW_REQUEST | CHARGING | すぐに充電可能なら遷移 |
| CHARGING | BATTERY_STOP_REQUEST | TEMP_STOP | バッテリー停止要求で一時停止 |
| CHARGING | TEMP_HIGH | TEMP_STOP | 温度高で一時停止 |
| TEMP_STOP | TEMP_LOW | CHARGING | 温度低下で再開条件を満たせば再開 |
| CHARGING | BATTERY_COMPLETE | COMPLETE | 充電完了 |
| 任意 | USB_REMOVED | DISCONNECTED | USB抜去で未接続に戻る |
| 任意 | FATAL | ERROR | 致命的異常でエラー遷移 |
| HOST_STOP | HOST_ALLOW | CHARGING / WAIT_START | ホスト許可で再開または待機 |




stateDiagram-v2
    [*] --> DISCONNECTED

    DISCONNECTED --> WAIT_START : USB_INSERTED

    WAIT_START --> WAIT_START : USB_TYPE_KNOWN\n(充電不可)
    WAIT_START --> CHARGING : USB_TYPE_KNOWN\n(充電可能 & 許可済)
    WAIT_START --> CHARGING : BATTERY_ALLOW_REQUEST

    CHARGING --> TEMP_STOP : BATTERY_STOP_REQUEST
    CHARGING --> TEMP_STOP : TEMP_HIGH

    TEMP_STOP --> CHARGING : TEMP_LOW

    CHARGING --> COMPLETE : BATTERY_COMPLETE

    DISCONNECTED --> DISCONNECTED : USB_REMOVED
    WAIT_START --> DISCONNECTED : USB_REMOVED
    CHARGING --> DISCONNECTED : USB_REMOVED
    TEMP_STOP --> DISCONNECTED : USB_REMOVED
    COMPLETE --> DISCONNECTED : USB_REMOVED
    ERROR --> DISCONNECTED : USB_REMOVED

    DISCONNECTED --> ERROR : FATAL
    WAIT_START --> ERROR : FATAL
    CHARGING --> ERROR : FATAL
    TEMP_STOP --> ERROR : FATAL
    COMPLETE --> ERROR : FATAL

    HOST_STOP --> CHARGING : HOST_ALLOW\n(再開)
    HOST_STOP --> WAIT_START : HOST_ALLOW\n(待機へ)




*/

#define CMD_CHG_START 1
#define CMD_CHG_STOP 2

static ChargeCtrl_State s_state = CHARGE_STATE_DISCONNECTED;
static State s_states[CHARGE_STATE_MAX];
static State *s_current_state = NULL;
static Queue cmdQueue;
static bool isChgIcDoing=false;
static void ProcessQueue(void);

static int s_usb_connected = 0;
static int s_usb_chargeable = 0;
static int s_battery_allow = 0;
static int s_temp_low = 1;
static int s_host_stop = 0;
static bool s_charge_active = 0;

static void transition_state(ChargeCtrl_State next);
static void try_enter_charging(void);
static void schedule_usb_type_known(void);
static void schedule_charge_start(void);
static void schedule_charge_stop(void);
static void start_charge_callback(void *arg);
static void stop_charge_callback(void *arg);
static void usb_type_known_callback(void *arg);
static void SendMsgWrapper_TaskCharge(uint32_t msgId, void *data);
static bool can_start_charging(void);

static void handle_usb_inserted(void);
static void handle_usb_removed(void);
static void handle_usb_type_known(void);
static void handle_battery_stop_request(void);
static void handle_battery_allow_request(void);
static void handle_battery_complete(void);
static void handle_host_stop(void);
static void handle_host_allow(void);
static void handle_temp_high(void);
static void handle_temp_low(void);
static void handle_fatal_error(void);

static void on_enter_disconnected(void);
static void on_enter_charging(void);
static void on_exit_charging(void);
static void on_enter_error(void);

static void set_temp_low(int low);

static void initialize_state_table(void)
{
    memset(s_states, 0, sizeof(s_states));

    s_states[CHARGE_STATE_DISCONNECTED].name = "DISCONNECTED";
    s_states[CHARGE_STATE_DISCONNECTED].on_enter = on_enter_disconnected;
    s_states[CHARGE_STATE_DISCONNECTED].on_event[CHARGE_EVENT_USB_INSERTED] = handle_usb_inserted;
    s_states[CHARGE_STATE_DISCONNECTED].on_event[CHARGE_EVENT_USB_REMOVED] = handle_usb_removed;
    s_states[CHARGE_STATE_DISCONNECTED].on_event[CHARGE_EVENT_USB_TYPE_KNOWN] = NULL;
    s_states[CHARGE_STATE_DISCONNECTED].on_event[CHARGE_EVENT_BATTERY_STOP_REQUEST] = NULL;
    s_states[CHARGE_STATE_DISCONNECTED].on_event[CHARGE_EVENT_BATTERY_ALLOW_REQUEST] = NULL;
    s_states[CHARGE_STATE_DISCONNECTED].on_event[CHARGE_EVENT_BATTERY_COMPLETE] = NULL;
    s_states[CHARGE_STATE_DISCONNECTED].on_event[CHARGE_EVENT_HOST_STOP] = NULL;
    s_states[CHARGE_STATE_DISCONNECTED].on_event[CHARGE_EVENT_HOST_ALLOW] = NULL;
    s_states[CHARGE_STATE_DISCONNECTED].on_event[CHARGE_EVENT_TEMP_HIGH] = NULL;
    s_states[CHARGE_STATE_DISCONNECTED].on_event[CHARGE_EVENT_TEMP_LOW] = NULL;
    s_states[CHARGE_STATE_DISCONNECTED].on_event[CHARGE_EVENT_FATAL] = handle_fatal_error;

    s_states[CHARGE_STATE_WAIT_START].name = "WAIT_START";
    s_states[CHARGE_STATE_WAIT_START].on_event[CHARGE_EVENT_USB_INSERTED] = NULL;
    s_states[CHARGE_STATE_WAIT_START].on_event[CHARGE_EVENT_USB_REMOVED] = handle_usb_removed;
    s_states[CHARGE_STATE_WAIT_START].on_event[CHARGE_EVENT_USB_TYPE_KNOWN] = handle_usb_type_known;
    s_states[CHARGE_STATE_WAIT_START].on_event[CHARGE_EVENT_BATTERY_STOP_REQUEST] = NULL;
    s_states[CHARGE_STATE_WAIT_START].on_event[CHARGE_EVENT_BATTERY_ALLOW_REQUEST] = handle_battery_allow_request;
    s_states[CHARGE_STATE_WAIT_START].on_event[CHARGE_EVENT_BATTERY_COMPLETE] = NULL;
    s_states[CHARGE_STATE_WAIT_START].on_event[CHARGE_EVENT_HOST_STOP] = handle_host_stop;
    s_states[CHARGE_STATE_WAIT_START].on_event[CHARGE_EVENT_HOST_ALLOW] = handle_host_allow;
    s_states[CHARGE_STATE_WAIT_START].on_event[CHARGE_EVENT_TEMP_HIGH] = NULL;
    s_states[CHARGE_STATE_WAIT_START].on_event[CHARGE_EVENT_TEMP_LOW] = handle_temp_low;
    s_states[CHARGE_STATE_WAIT_START].on_event[CHARGE_EVENT_FATAL] = handle_fatal_error;

    s_states[CHARGE_STATE_CHARGING].name = "CHARGING";
    s_states[CHARGE_STATE_CHARGING].on_enter = on_enter_charging;
    s_states[CHARGE_STATE_CHARGING].on_exit = on_exit_charging;
    s_states[CHARGE_STATE_CHARGING].on_event[CHARGE_EVENT_USB_INSERTED] = NULL;
    s_states[CHARGE_STATE_CHARGING].on_event[CHARGE_EVENT_USB_REMOVED] = handle_usb_removed;
    s_states[CHARGE_STATE_CHARGING].on_event[CHARGE_EVENT_USB_TYPE_KNOWN] = NULL;
    s_states[CHARGE_STATE_CHARGING].on_event[CHARGE_EVENT_BATTERY_STOP_REQUEST] = handle_battery_stop_request;
    s_states[CHARGE_STATE_CHARGING].on_event[CHARGE_EVENT_BATTERY_ALLOW_REQUEST] = handle_battery_allow_request;
    s_states[CHARGE_STATE_CHARGING].on_event[CHARGE_EVENT_BATTERY_COMPLETE] = handle_battery_complete;
    s_states[CHARGE_STATE_CHARGING].on_event[CHARGE_EVENT_HOST_STOP] = handle_host_stop;
    s_states[CHARGE_STATE_CHARGING].on_event[CHARGE_EVENT_HOST_ALLOW] = handle_host_allow;
    s_states[CHARGE_STATE_CHARGING].on_event[CHARGE_EVENT_TEMP_HIGH] = handle_temp_high;
    s_states[CHARGE_STATE_CHARGING].on_event[CHARGE_EVENT_TEMP_LOW] = handle_temp_low;
    s_states[CHARGE_STATE_CHARGING].on_event[CHARGE_EVENT_FATAL] = handle_fatal_error;

    s_states[CHARGE_STATE_COMPLETE].name = "COMPLETE";
    s_states[CHARGE_STATE_COMPLETE].on_event[CHARGE_EVENT_USB_INSERTED] = NULL;
    s_states[CHARGE_STATE_COMPLETE].on_event[CHARGE_EVENT_USB_REMOVED] = handle_usb_removed;
    s_states[CHARGE_STATE_COMPLETE].on_event[CHARGE_EVENT_USB_TYPE_KNOWN] = NULL;
    s_states[CHARGE_STATE_COMPLETE].on_event[CHARGE_EVENT_BATTERY_STOP_REQUEST] = NULL;
    s_states[CHARGE_STATE_COMPLETE].on_event[CHARGE_EVENT_BATTERY_ALLOW_REQUEST] = NULL;
    s_states[CHARGE_STATE_COMPLETE].on_event[CHARGE_EVENT_BATTERY_COMPLETE] = NULL;
    s_states[CHARGE_STATE_COMPLETE].on_event[CHARGE_EVENT_HOST_STOP] = handle_host_stop;
    s_states[CHARGE_STATE_COMPLETE].on_event[CHARGE_EVENT_HOST_ALLOW] = handle_host_allow;
    s_states[CHARGE_STATE_COMPLETE].on_event[CHARGE_EVENT_TEMP_HIGH] = NULL;
    s_states[CHARGE_STATE_COMPLETE].on_event[CHARGE_EVENT_TEMP_LOW] = NULL;
    s_states[CHARGE_STATE_COMPLETE].on_event[CHARGE_EVENT_FATAL] = handle_fatal_error;

    s_states[CHARGE_STATE_TEMP_STOP].name = "TEMP_STOP";
    s_states[CHARGE_STATE_TEMP_STOP].on_event[CHARGE_EVENT_USB_INSERTED] = NULL;
    s_states[CHARGE_STATE_TEMP_STOP].on_event[CHARGE_EVENT_USB_REMOVED] = handle_usb_removed;
    s_states[CHARGE_STATE_TEMP_STOP].on_event[CHARGE_EVENT_USB_TYPE_KNOWN] = NULL;
    s_states[CHARGE_STATE_TEMP_STOP].on_event[CHARGE_EVENT_BATTERY_STOP_REQUEST] = NULL;
    s_states[CHARGE_STATE_TEMP_STOP].on_event[CHARGE_EVENT_BATTERY_ALLOW_REQUEST] = handle_battery_allow_request;
    s_states[CHARGE_STATE_TEMP_STOP].on_event[CHARGE_EVENT_BATTERY_COMPLETE] = NULL;
    s_states[CHARGE_STATE_TEMP_STOP].on_event[CHARGE_EVENT_HOST_STOP] = handle_host_stop;
    s_states[CHARGE_STATE_TEMP_STOP].on_event[CHARGE_EVENT_HOST_ALLOW] = handle_host_allow;
    s_states[CHARGE_STATE_TEMP_STOP].on_event[CHARGE_EVENT_TEMP_HIGH] = handle_temp_high;
    s_states[CHARGE_STATE_TEMP_STOP].on_event[CHARGE_EVENT_TEMP_LOW] = handle_temp_low;
    s_states[CHARGE_STATE_TEMP_STOP].on_event[CHARGE_EVENT_FATAL] = handle_fatal_error;

    s_states[CHARGE_STATE_HOST_STOP].name = "HOST_STOP";
    s_states[CHARGE_STATE_HOST_STOP].on_event[CHARGE_EVENT_USB_INSERTED] = NULL;
    s_states[CHARGE_STATE_HOST_STOP].on_event[CHARGE_EVENT_USB_REMOVED] = handle_usb_removed;
    s_states[CHARGE_STATE_HOST_STOP].on_event[CHARGE_EVENT_USB_TYPE_KNOWN] = NULL;
    s_states[CHARGE_STATE_HOST_STOP].on_event[CHARGE_EVENT_BATTERY_STOP_REQUEST] = NULL;
    s_states[CHARGE_STATE_HOST_STOP].on_event[CHARGE_EVENT_BATTERY_ALLOW_REQUEST] = handle_battery_allow_request;
    s_states[CHARGE_STATE_HOST_STOP].on_event[CHARGE_EVENT_BATTERY_COMPLETE] = NULL;
    s_states[CHARGE_STATE_HOST_STOP].on_event[CHARGE_EVENT_HOST_STOP] = NULL;
    s_states[CHARGE_STATE_HOST_STOP].on_event[CHARGE_EVENT_HOST_ALLOW] = handle_host_allow;
    s_states[CHARGE_STATE_HOST_STOP].on_event[CHARGE_EVENT_TEMP_HIGH] = NULL;
    s_states[CHARGE_STATE_HOST_STOP].on_event[CHARGE_EVENT_TEMP_LOW] = NULL;
    s_states[CHARGE_STATE_HOST_STOP].on_event[CHARGE_EVENT_FATAL] = handle_fatal_error;

    s_states[CHARGE_STATE_ERROR].name = "ERROR";
    s_states[CHARGE_STATE_ERROR].on_event[CHARGE_EVENT_USB_INSERTED] = NULL;
    s_states[CHARGE_STATE_ERROR].on_event[CHARGE_EVENT_USB_REMOVED] = handle_usb_removed;
    s_states[CHARGE_STATE_ERROR].on_event[CHARGE_EVENT_USB_TYPE_KNOWN] = NULL;
    s_states[CHARGE_STATE_ERROR].on_event[CHARGE_EVENT_BATTERY_STOP_REQUEST] = NULL;
    s_states[CHARGE_STATE_ERROR].on_event[CHARGE_EVENT_BATTERY_ALLOW_REQUEST] = NULL;
    s_states[CHARGE_STATE_ERROR].on_event[CHARGE_EVENT_BATTERY_COMPLETE] = NULL;
    s_states[CHARGE_STATE_ERROR].on_event[CHARGE_EVENT_HOST_STOP] = NULL;
    s_states[CHARGE_STATE_ERROR].on_event[CHARGE_EVENT_HOST_ALLOW] = NULL;
    s_states[CHARGE_STATE_ERROR].on_event[CHARGE_EVENT_TEMP_HIGH] = NULL;
    s_states[CHARGE_STATE_ERROR].on_event[CHARGE_EVENT_TEMP_LOW] = NULL;
    s_states[CHARGE_STATE_ERROR].on_event[CHARGE_EVENT_FATAL] = NULL;

    Queue_Init(&cmdQueue);
}

void ChargeState_Init(void)
{
    initialize_state_table();
    s_state = CHARGE_STATE_DISCONNECTED;
    s_current_state = &s_states[s_state];
    if (s_current_state->on_enter) {
        s_current_state->on_enter();
    }
}

void ChargeState_ProcessEvent(ChargeCtrl_Event event)
{
    if (event < 0 || event >= MAX_CHG_EVENTS || s_current_state == NULL) {
        return;
    }

    void (*handler)(void) = s_current_state->on_event[event];
    if (handler) {
        handler();
    }
}

void ChargeState_SetUsbChargeable(int chargeable)
{
    s_usb_chargeable = chargeable ? 1 : 0;
}

ChargeCtrl_State ChargeState_GetState(void)
{
    return s_state;
}

bool ChargeState_IsChargingActive(void)
{
    return s_charge_active;
}

static void transition_state(ChargeCtrl_State next)
{
    if (next == s_state || next < 0 || next >= CHARGE_STATE_MAX) {
        return;
    }

    if (s_current_state && s_current_state->on_exit) {
        s_current_state->on_exit();
    }

    s_state = next;
    s_current_state = &s_states[s_state];

    if (s_current_state && s_current_state->on_enter) {
        s_current_state->on_enter();
    }
}

static void try_enter_charging(void)
{
    if (!can_start_charging()) {
        return;
    }

    transition_state(CHARGE_STATE_CHARGING);
}

static void schedule_usb_type_known(void)
{
    ChargeCtrl_DelayProc(usb_type_known_callback, USB_TYPE_CHECK_DELAY_MS, NULL);
}

static void schedule_charge_start(void)
{
    Queue_Enqueue(&cmdQueue, CMD_CHG_START);
    ProcessQueue();
    //ChargeCtrl_DelayProc(start_charge_callback, CHARGE_START_DELAY_MS, NULL);
}

static void schedule_charge_stop(void)
{
    Queue_Enqueue(&cmdQueue, CMD_CHG_STOP);
    ProcessQueue();
    //ChargeCtrl_DelayProc(stop_charge_callback, CHARGE_STOP_DELAY_MS, NULL);
}

// キュー処理
static void ProcessQueue(void)
{
    int cmd;

    // 処理中は次のコマンドを実行しない
    if (isChgIcDoing) {
        return;
    }

    if (!Queue_Dequeue(&cmdQueue, &cmd)) {
        return;
    }

    switch (cmd) {
    case CMD_CHG_START:
        isChgIcDoing = true;
        //printf(">>start\n");
        ChargeCtrl_DelayProc(start_charge_callback, CHARGE_START_DELAY_MS, NULL);
        break;

    case CMD_CHG_STOP:
        isChgIcDoing = true;
        //printf("-->>stop\n");
        ChargeCtrl_DelayProc(stop_charge_callback, CHARGE_STOP_DELAY_MS, NULL);
        break;
    }
}


static void start_charge_callback(void *arg)
{
    s_charge_active = true;
    isChgIcDoing=false;
    //printf("<<comp start\n");
    ProcessQueue();
}
static void stop_charge_callback(void *arg)
{
    s_charge_active = false;
    isChgIcDoing=false;
    //printf("<<--comp stop\n");
    ProcessQueue();
}

static void usb_type_known_callback(void *arg)
{
    SendMsgWrapper_TaskCharge(CHARGE_MSG_STATE, (void *)(intptr_t)CHARGE_EVENT_USB_TYPE_KNOWN);
}

static void HandleChargeStateMessage(void *data)
{
    ChargeState_ProcessEvent((ChargeCtrl_Event)(intptr_t)data);
}

static void SendMsgWrapper_TaskCharge(uint32_t msgId, void *data)
{
#if defined(OS_TEST_LAYER_ENABLE)
    void (*handler)(void *) = NULL;
    switch (msgId) {
    case CHARGE_MSG_STATE:
        handler = HandleChargeStateMessage;
        break;
    default:
        break;
    }

    if (handler != NULL) {
        OsTestLayer_Post(handler, data, sizeof(data));
    }
#else
    PosixOs_SendMsg(CHARGE_QUEUE_ID, msgId, data);
#endif
}

static void handle_usb_inserted(void)
{
    if (s_state != CHARGE_STATE_DISCONNECTED) {
        return;
    }

    s_usb_connected = 1;
    s_host_stop = 0;
    s_battery_allow = 0;
    transition_state(CHARGE_STATE_WAIT_START);
    schedule_usb_type_known();
}

static void handle_usb_removed(void)
{
    transition_state(CHARGE_STATE_DISCONNECTED);
}

static void handle_usb_type_known(void)
{
    try_enter_charging();
}

static void handle_battery_stop_request(void)
{
    transition_state(CHARGE_STATE_TEMP_STOP);
}

static void handle_battery_allow_request(void)
{
    s_battery_allow = 1;
    try_enter_charging();
}

static void handle_battery_complete(void)
{
    transition_state(CHARGE_STATE_COMPLETE);
}

static void handle_host_stop(void)
{
    s_host_stop = 1;
    transition_state(CHARGE_STATE_HOST_STOP);
}

static void handle_host_allow(void)
{
    s_host_stop = 0;
    if (can_start_charging()) {
        transition_state(CHARGE_STATE_CHARGING);
    } else {
        transition_state(CHARGE_STATE_WAIT_START);
    }
}
static bool can_start_charging(void){
    if (s_usb_connected && s_usb_chargeable && s_battery_allow && s_temp_low && !s_host_stop) {
        return true;
    }
    return false;
}
static void handle_temp_high(void)
{
    set_temp_low(0);
    transition_state(CHARGE_STATE_TEMP_STOP);
}

static void handle_temp_low(void)
{
    set_temp_low(1);
    try_enter_charging();
}

static void handle_fatal_error(void)
{
    transition_state(CHARGE_STATE_ERROR);
}

static void on_enter_disconnected(void)
{
    s_usb_connected = 0;
    s_host_stop = 0;
    s_battery_allow = 0;
}

static void on_enter_charging(void)
{
    schedule_charge_start();
}

static void on_exit_charging(void)
{
    //printf("on_exit_charging\n");
    schedule_charge_stop();
}

static void set_temp_low(int low)
{
    s_temp_low = low ? 1 : 0;
}
