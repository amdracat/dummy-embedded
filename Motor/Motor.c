#include "Motor.h"
#include "PosixOs.h"
#include "GpioDrv.h"
#include "TemperatureSensor.h"
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#define MOTOR_QUEUE_ID 2
#define MOTOR_QUEUE_COUNT 1
#define MOTOR_CONTROL_INTERVAL_MS 200

enum {
    MOTOR_MSG_SET_SPEED = 1,
    MOTOR_MSG_SET_MODE = 2,
    MOTOR_MSG_CONTROL = 3,
    MOTOR_MSG_TEMPERATURE_UPDATE = 4
};

static gpio_t s_gpio = {0};
static MotorSpeed s_current_speed = MOTOR_SPEED_STOP;
static MotorSpeed s_target_speed = MOTOR_SPEED_STOP;
static MotorMode s_current_mode = MOTOR_MODE_STOP;
static int s_last_temperature = 0;

static void motor_task(void);
static void motor_timer_callback(void *arg);
static void motor_temperature_updated(void *arg);
static void motor_sleep_ms(uint32_t ms);
static void set_motor_outputs_off(void);
static void apply_motor_mode_sequence(MotorMode mode);
static void update_motor_speed_control(void);

void Motor_Init(void)
{
    GpioDrv_Init(&s_gpio);
    PosixOs_CreateMsgQueues(MOTOR_QUEUE_ID, MOTOR_QUEUE_COUNT);
    PosixOs_CreateTask(motor_task, "Motor");
    PosixOs_EventSubscribe(TEMPERATURE_UPDATE_EVENT, motor_temperature_updated, NULL);
    PosixOs_SetupTimer(motor_timer_callback, MOTOR_CONTROL_INTERVAL_MS, NULL);
}

void Motor_SetSpeed(MotorSpeed speed)
{
    s_target_speed = speed;
    PosixOs_SendMsg(MOTOR_QUEUE_ID, MOTOR_MSG_SET_SPEED, (void *)(uintptr_t)speed);
}

MotorSpeed Motor_GetSpeed(void)
{
    return s_current_speed;
}

void Motor_SetMode(MotorMode mode)
{
    PosixOs_SendMsg(MOTOR_QUEUE_ID, MOTOR_MSG_SET_MODE, (void *)(uintptr_t)mode);
}

MotorMode Motor_GetMode(void)
{
    return s_current_mode;
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
    PosixOs_SendMsg(MOTOR_QUEUE_ID, MOTOR_MSG_CONTROL, NULL);
    PosixOs_SetupTimer(motor_timer_callback, MOTOR_CONTROL_INTERVAL_MS, NULL);
}

static void motor_temperature_updated(void *arg)
{
    (void)arg;
    PosixOs_SendMsg(MOTOR_QUEUE_ID, MOTOR_MSG_TEMPERATURE_UPDATE, NULL);
}

static void motor_sleep_ms(uint32_t ms)
{
    struct timespec ts = {ms / 1000, (ms % 1000) * 1000000};
    nanosleep(&ts, NULL);
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
    switch (mode) {
    case MOTOR_MODE_STOP:
        motor_sleep_ms(100);
        break;
    case MOTOR_MODE_LEFT:
        motor_sleep_ms(30);
        GpioDrv_WritePin(&s_gpio, PORT_LEFT, 1);
        motor_sleep_ms(50);
        GpioDrv_WritePin(&s_gpio, PORT_ENBALE_MOTOR, 1);
        break;
    case MOTOR_MODE_RIGHT:
        motor_sleep_ms(30);
        GpioDrv_WritePin(&s_gpio, PORT_RIGHT, 1);
        motor_sleep_ms(50);
        GpioDrv_WritePin(&s_gpio, PORT_ENBALE_MOTOR, 1);
        break;
    case MOTOR_MODE_FRONT:
        motor_sleep_ms(100);
        GpioDrv_WritePin(&s_gpio, PORT_FRONT, 1);
        motor_sleep_ms(50);
        GpioDrv_WritePin(&s_gpio, PORT_ENBALE_MOTOR, 1);
        break;
    case MOTOR_MODE_BACK:
        motor_sleep_ms(200);
        GpioDrv_WritePin(&s_gpio, PORT_BACK, 1);
        motor_sleep_ms(50);
        GpioDrv_WritePin(&s_gpio, PORT_ENBALE_MOTOR, 1);
        break;
    }
    s_current_mode = mode;
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
