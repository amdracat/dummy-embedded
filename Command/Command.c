#include "Command.h"
#include "ChargeCtrl.h"
#include "Motor.h"
#include "I2cDrv.h"
#include "Test_Motor.h"
#include "Test_Charge.h"
#include "PosixOs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static void command_task(void);
static void execute_command(char *line);
static void cmd_temp(int argc, char *argv[]);
static void cmd_speed(int argc, char *argv[]);
static void cmd_mode(int argc, char *argv[]);
static void cmd_charge(int argc, char *argv[]);
static void cmd_test(void);

void Command_Init(void)
{
    PosixOs_CreateTask(command_task, "Command");
}

static void command_task(void)
{
    char line[256];

    while (1) {
        printf("> ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            break;
        }

        execute_command(line);
    }
}

static void execute_command(char *line)
{
    char *argv[16];
    int argc = 0;
    char *token = strtok(line, " \t\r\n");

    while (token && argc < (int)(sizeof(argv) / sizeof(argv[0]))) {
        argv[argc++] = token;
        token = strtok(NULL, " \t\r\n");
    }

    if (argc == 0) {
        return;
    }

    if (strcmp(argv[0], "temp") == 0) {
        cmd_temp(argc, argv);
    } else if (strcmp(argv[0], "speed") == 0) {
        cmd_speed(argc, argv);
    } else if (strcmp(argv[0], "mode") == 0) {
        cmd_mode(argc, argv);
    } else if (strcmp(argv[0], "charge") == 0) {
        cmd_charge(argc, argv);
    } else if (strcmp(argv[0], "test") == 0) {
        cmd_test();
    } else {
        printf("unknown command: %s\n", argv[0]);
    }
}

static void cmd_temp(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: temp <temperature>\n");
        return;
    }

    int temperature = atoi(argv[1]);
    int16_t raw = (int16_t)temperature;
    uint16_t packed = (uint16_t)(raw << 4);
    uint8_t buffer[2] = { (uint8_t)(packed & 0xFF), (uint8_t)(packed >> 8) };
    I2cDrv_DummySetReadData(0x48, 0x00, buffer, sizeof(buffer));
    printf("temperature set to %d\n", temperature);
}

static void cmd_speed(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: speed <stop|slow|fast>\n");
        return;
    }

    if (strcmp(argv[1], "stop") == 0) {
        Motor_SetSpeed(MOTOR_SPEED_STOP);
    } else if (strcmp(argv[1], "slow") == 0) {
        Motor_SetSpeed(MOTOR_SPEED_SLOW);
    } else if (strcmp(argv[1], "fast") == 0) {
        Motor_SetSpeed(MOTOR_SPEED_FAST);
    } else {
        printf("unknown speed: %s\n", argv[1]);
    }
}

static void cmd_mode(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: mode <stop|left|right|front|back>\n");
        return;
    }

    if (strcmp(argv[1], "stop") == 0) {
        Motor_SetMode(MOTOR_MODE_STOP);
    } else if (strcmp(argv[1], "left") == 0) {
        Motor_SetMode(MOTOR_MODE_LEFT);
    } else if (strcmp(argv[1], "right") == 0) {
        Motor_SetMode(MOTOR_MODE_RIGHT);
    } else if (strcmp(argv[1], "front") == 0) {
        Motor_SetMode(MOTOR_MODE_FRONT);
    } else if (strcmp(argv[1], "back") == 0) {
        Motor_SetMode(MOTOR_MODE_BACK);
    } else {
        printf("unknown mode: %s\n", argv[1]);
    }
}

static void cmd_charge(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: charge <0-7>\n");
        printf(" 0=usb insert\n");
        printf(" 1=usb remove\n");
        printf(" 2=battery stop request\n");
        printf(" 3=battery allow request\n");
        printf(" 4=battery complete\n");
        printf(" 5=host stop\n");
        printf(" 6=host allow\n");
        printf(" 7=fatal error\n");
        return;
    }

    int command = atoi(argv[1]);
    switch (command) {
        case 0:
            ChargeCtrl_NotifyUsbInserted();
            break;
        case 1:
            ChargeCtrl_NotifyUsbRemoved();
            break;
        case 2:
            ChargeCtrl_NotifyBatteryStopRequest();
            break;
        case 3:
            ChargeCtrl_NotifyBatteryAllowRequest();
            break;
        case 4:
            ChargeCtrl_NotifyBatteryComplete();
            break;
        case 5:
            ChargeCtrl_NotifyHostStop();
            break;
        case 6:
            ChargeCtrl_NotifyHostAllow();
            break;
        case 7:
            ChargeCtrl_NotifyFatalError();
            break;
        default:
            printf("unknown charge command: %s\n", argv[1]);
            break;
    }
}

static void cmd_test(void)
{
    Test_MotorTest();
    Test_ChargeTest();
}
void Command_SyncTest(void)
{
    cmd_test();
}