#include "Motor.h"
#include "PosixOs.h"
#include "OsTestLayer.h"
#include "GpioDrv.h"
#include "TemperatureSensor.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#define MOTOR_QUEUE_ID 2
#define MOTOR_QUEUE_COUNT 1
#define MOTOR_CONTROL_INTERVAL_MS 200

enum {
    MOTOR_MSG_SET_SPEED = 1,
    MOTOR_MSG_SET_MODE = 2,
    MOTOR_MSG_CONTROL = 3,
    MOTOR_MSG_TEMPERATURE_UPDATE = 4,
    MOTOR_MSG_MODE_STEP = 5
};

static gpio_t s_gpio = {0};
static MotorSpeed s_current_speed = MOTOR_SPEED_STOP;
static MotorSpeed s_target_speed = MOTOR_SPEED_STOP;
static MotorMode s_current_mode = MOTOR_MODE_STOP;
static int s_last_temperature = 0;

typedef struct {
    MotorMode mode;
    int stage;
    int sequence_id;
} ModeSequenceContext;

typedef void (*MotorTaskFunc)(void *data);

static void motor_task(void);
static void motor_timer_callback(void *arg);
static void motor_temperature_updated(void *arg);
static void motor_mode_sequence_timer(void *arg);
static void HandleMotorMessageSetSpeed(void *data);
static void HandleMotorMessageSetMode(void *data);
static void HandleMotorMessageControl(void *data);
static void HandleMotorMessageTemperatureUpdate(void *data);
static void HandleMotorMessageModeStep(void *data);
static MotorTaskFunc search_motor_function(uint32_t msgId);
static void SendMsgWrapper_TaskMotor(uint32_t msgId, void *data);
static void set_motor_outputs_off(void);
static void schedule_mode_timer(MotorMode mode, int stage, uint32_t delay_ms);
static void apply_motor_mode_sequence(MotorMode mode);
static void update_motor_speed_control(void);

static const struct {
    uint32_t msgId;
    MotorTaskFunc handler;
} s_motor_message_table[] = {
    { MOTOR_MSG_SET_SPEED, HandleMotorMessageSetSpeed },
    { MOTOR_MSG_SET_MODE, HandleMotorMessageSetMode },
    { MOTOR_MSG_CONTROL, HandleMotorMessageControl },
    { MOTOR_MSG_TEMPERATURE_UPDATE, HandleMotorMessageTemperatureUpdate },
    { MOTOR_MSG_MODE_STEP, HandleMotorMessageModeStep },
};

static int s_mode_sequence_id = 0;
static int s_mode_sequence_active = 0;

void Motor_Init(void)
{
    GpioDrv_Init(&s_gpio);
    PosixOs_CreateMsgQueues(MOTOR_QUEUE_ID, MOTOR_QUEUE_COUNT);
    PosixOs_CreateTask(motor_task, "Motor");
    PosixOs_EventSubscribe(TEMPERATURE_UPDATE_EVENT, motor_temperature_updated, NULL);
    OsTestLayer_SetTimer(MOTOR_CONTROL_INTERVAL_MS, motor_timer_callback, NULL);
}

void Motor_SetSpeed(MotorSpeed speed)
{
    s_target_speed = speed;
    SendMsgWrapper_TaskMotor(MOTOR_MSG_SET_SPEED, (void *)(uintptr_t)speed);
}

MotorSpeed Motor_GetSpeed(void)
{
    return s_current_speed;
}

void Motor_SetMode(MotorMode mode)
{
    SendMsgWrapper_TaskMotor(MOTOR_MSG_SET_MODE, (void *)(uintptr_t)mode);
}

MotorMode Motor_GetMode(void)
{
    return s_current_mode;
}

static void HandleMotorMessageSetSpeed(void *data)
{
    MotorSpeed speed = (MotorSpeed)(uintptr_t)data;
    s_target_speed = speed;
    s_current_speed = s_target_speed;
    update_motor_speed_control();
}

static void HandleMotorMessageSetMode(void *data)
{
    apply_motor_mode_sequence((MotorMode)(uintptr_t)data);
}

static void HandleMotorMessageControl(void *data)
{
    (void)data;
    update_motor_speed_control();
}

static void HandleMotorMessageTemperatureUpdate(void *data)
{
    (void)data;
    update_motor_speed_control();
}

static void HandleMotorMessageModeStep(void *data)
{
    ModeSequenceContext *ctx = (ModeSequenceContext *)data;
    if (!ctx) {
        return;
    }

    if (ctx->sequence_id != s_mode_sequence_id) {
        free(ctx);
        return;
    }

    if (ctx->stage == 1) {
        switch (ctx->mode) {
        case MOTOR_MODE_STOP:
            set_motor_outputs_off();
            s_current_mode = ctx->mode;
            s_mode_sequence_active = 0;
            break;
        case MOTOR_MODE_LEFT:
            GpioDrv_WritePin(&s_gpio, PORT_LEFT, 1);
            schedule_mode_timer(ctx->mode, 2, 50);
            break;
        case MOTOR_MODE_RIGHT:
            GpioDrv_WritePin(&s_gpio, PORT_RIGHT, 1);
            schedule_mode_timer(ctx->mode, 2, 50);
            break;
        case MOTOR_MODE_FRONT:
            GpioDrv_WritePin(&s_gpio, PORT_FRONT, 1);
            schedule_mode_timer(ctx->mode, 2, 50);
            break;
        case MOTOR_MODE_BACK:
            GpioDrv_WritePin(&s_gpio, PORT_BACK, 1);
            schedule_mode_timer(ctx->mode, 2, 50);
            break;
        }
    } else if (ctx->stage == 2) {
        GpioDrv_WritePin(&s_gpio, PORT_ENBALE_MOTOR, 1);
        s_current_mode = ctx->mode;
        s_mode_sequence_active = 0;
    }
    free(ctx);
}

static MotorTaskFunc search_motor_function(uint32_t msgId)
{
    for (size_t i = 0; i < sizeof(s_motor_message_table) / sizeof(s_motor_message_table[0]); ++i) {
        if (s_motor_message_table[i].msgId == msgId) {
            return s_motor_message_table[i].handler;
        }
    }
    return NULL;
}

static void SendMsgWrapper_TaskMotor(uint32_t msgId, void *data)
{
#if defined(OS_TEST_LAYER_ENABLE)
    MotorTaskFunc handler = search_motor_function(msgId);
    if (handler != NULL) {
        OsTestLayer_Post(handler, data, sizeof(data));
    }
#else
    PosixOs_SendMsg(MOTOR_QUEUE_ID, msgId, data);
#endif
}

static void motor_task(void)
{
    Message msg;
    while (1) {
        PosixOs_GetMsg(MOTOR_QUEUE_ID, &msg);
        switch (msg.MsgId) {
        case MOTOR_MSG_SET_SPEED:
            s_target_speed = (MotorSpeed)(uintptr_t)msg.Data;
            s_current_speed = s_target_speed;
            update_motor_speed_control();
            break;
        case MOTOR_MSG_SET_MODE:
            apply_motor_mode_sequence((MotorMode)(uintptr_t)msg.Data);
            break;
        case MOTOR_MSG_MODE_STEP: {
            ModeSequenceContext *ctx = (ModeSequenceContext *)msg.Data;
            if (!ctx) {
                break;
            }
            if (ctx->sequence_id != s_mode_sequence_id) {
                free(ctx);
                break;
            }

            if (ctx->stage == 1) {
                switch (ctx->mode) {
                case MOTOR_MODE_STOP:
                    set_motor_outputs_off();
                    s_current_mode = ctx->mode;
                    s_mode_sequence_active = 0;
                    break;
                case MOTOR_MODE_LEFT:
                    GpioDrv_WritePin(&s_gpio, PORT_LEFT, 1);
                    schedule_mode_timer(ctx->mode, 2, 50);
                    break;
                case MOTOR_MODE_RIGHT:
                    GpioDrv_WritePin(&s_gpio, PORT_RIGHT, 1);
                    schedule_mode_timer(ctx->mode, 2, 50);
                    break;
                case MOTOR_MODE_FRONT:
                    GpioDrv_WritePin(&s_gpio, PORT_FRONT, 1);
                    schedule_mode_timer(ctx->mode, 2, 50);
                    break;
                case MOTOR_MODE_BACK:
                    GpioDrv_WritePin(&s_gpio, PORT_BACK, 1);
                    schedule_mode_timer(ctx->mode, 2, 50);
                    break;
                }
            } else if (ctx->stage == 2) {
                GpioDrv_WritePin(&s_gpio, PORT_ENBALE_MOTOR, 1);
                s_current_mode = ctx->mode;
                s_mode_sequence_active = 0;
            }
            free(ctx);
            break;
        }
        case MOTOR_MSG_CONTROL:
        case MOTOR_MSG_TEMPERATURE_UPDATE:
            update_motor_speed_control();
            break;
        default:
            break;
        }
    }
}

static void motor_timer_callback(void *arg)
{
    (void)arg;
    SendMsgWrapper_TaskMotor(MOTOR_MSG_CONTROL, NULL);
    OsTestLayer_SetTimer(MOTOR_CONTROL_INTERVAL_MS, motor_timer_callback, NULL);
}

static void motor_temperature_updated(void *arg)
{
    (void)arg;
    SendMsgWrapper_TaskMotor(MOTOR_MSG_TEMPERATURE_UPDATE, NULL);
}

static void motor_mode_sequence_timer(void *arg)
{
    ModeSequenceContext *ctx = (ModeSequenceContext *)arg;
    if (!ctx) {
        return;
    }

    SendMsgWrapper_TaskMotor(MOTOR_MSG_MODE_STEP, ctx);
}

static void schedule_mode_timer(MotorMode mode, int stage, uint32_t delay_ms)
{
    ModeSequenceContext *ctx = (ModeSequenceContext *)malloc(sizeof(ModeSequenceContext));
    if (!ctx) {
        return;
    }
    ctx->mode = mode;
    ctx->stage = stage;
    ctx->sequence_id = s_mode_sequence_id;

    OsTestLayer_SetTimer(delay_ms, motor_mode_sequence_timer, ctx);
}

static void set_motor_outputs_off(void)
{
    GpioDrv_WritePin(&s_gpio, PORT_LEFT, 0);
    GpioDrv_WritePin(&s_gpio, PORT_RIGHT, 0);
    GpioDrv_WritePin(&s_gpio, PORT_FRONT, 0);
    GpioDrv_WritePin(&s_gpio, PORT_BACK, 0);
    GpioDrv_WritePin(&s_gpio, PORT_ENBALE_MOTOR, 0);
}

static void apply_motor_mode_sequence(MotorMode mode)
{
    set_motor_outputs_off();
    s_mode_sequence_id++;
    s_mode_sequence_active = 1;
    switch (mode) {
    case MOTOR_MODE_STOP:
        schedule_mode_timer(mode, 1, 100);
        break;
    case MOTOR_MODE_LEFT:
        schedule_mode_timer(mode, 1, 30);
        break;
    case MOTOR_MODE_RIGHT:
        schedule_mode_timer(mode, 1, 30);
        break;
    case MOTOR_MODE_FRONT:
        schedule_mode_timer(mode, 1, 100);
        break;
    case MOTOR_MODE_BACK:
        schedule_mode_timer(mode, 1, 200);
        break;
    }
}

static void update_motor_speed_control(void)
{
    int temperature = TemperatureSensor_GetTemperature();
    s_last_temperature = temperature;

    int high_speed = 0;
    if (s_target_speed == MOTOR_SPEED_FAST && temperature >= -10 && temperature <= 40) {
        high_speed = 1;
    }

    GpioDrv_WritePin(&s_gpio, PORT_HIGHSPEED, high_speed);
}
