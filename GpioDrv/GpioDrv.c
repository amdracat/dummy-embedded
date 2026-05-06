#include "GpioDrv.h"
#include <stdio.h>
#include <stdlib.h>

static int s_dummy_values[6];
static gpio_t *s_gpio = NULL;

void GpioDrv_Init(gpio_t *gpio)
{
    if (gpio == NULL) {
        return;
    }
    gpio->ctx = NULL;
    gpio->read = GpioDrv_DummyReadPin;
    gpio->write = GpioDrv_DummyWritePin;
    s_gpio = gpio;
    for (int i = 0; i < 6; i++) {
        s_dummy_values[i] = 0;
    }
}

int GpioDrv_ReadPin(gpio_t *gpio, Port port)
{
    if (gpio == NULL || gpio->read == NULL) {
        return 0;
    }
    return gpio->read(port);
}

void GpioDrv_WritePin(gpio_t *gpio, Port port, int value)
{
    if (gpio == NULL || gpio->write == NULL) {
        return;
    }
    gpio->write(port, value);
}

int GpioDrv_DummyReadPin(Port port)
{
    if (port < PORT_LEFT || port > PORT_ENBALE_MOTOR) {
        return 0;
    }
    return s_dummy_values[port];
}

void GpioDrv_DummyWritePin(Port port, int value)
{
    if (port < PORT_LEFT || port > PORT_ENBALE_MOTOR) {
        return;
    }
    s_dummy_values[port] = value;
    printf("[GPIO] port=%d value=%d\n", (int)port, value);
}

void GpioDrv_DummySetReadData(Port port, int value)
{
    if (port < PORT_LEFT || port > PORT_ENBALE_MOTOR) {
        return;
    }
    s_dummy_values[port] = value;
}
