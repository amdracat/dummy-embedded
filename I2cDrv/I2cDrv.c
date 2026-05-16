#include "I2cDrv.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t s_dummy_read_data[256][256];
static pthread_mutex_t s_dummy_lock = PTHREAD_MUTEX_INITIALIZER;

void I2cDrv_Init(i2c_bus_t *bus)
{
    if (!bus) {
        return;
    }
    bus->ctx = NULL;
    bus->read = I2cDrv_DummyRead;
    bus->write = I2cDrv_DummyWrite;
}

void I2cDrv_Read(i2c_bus_t *bus, uint8_t dev_addr, uint8_t reg_addr, uint8_t *buf, size_t len)
{
    if (!bus || !bus->read || !buf) {
        return;
    }
    bus->read(bus->ctx, dev_addr, reg_addr, buf, len);
}

void I2cDrv_Write(i2c_bus_t *bus, uint8_t dev_addr, uint8_t reg_addr, const uint8_t *data, size_t len)
{
    if (!bus || !bus->write || !data) {
        return;
    }
    bus->write(bus->ctx, dev_addr, reg_addr, data, len);
}

void I2cDrv_DummyRead(void *ctx, uint8_t dev, uint8_t reg, uint8_t *buf, size_t len)
{
    (void)ctx;
    if (!buf) {
        return;
    }

    pthread_mutex_lock(&s_dummy_lock);
    for (size_t i = 0; i < len; i++) {
        buf[i] = s_dummy_read_data[dev][reg + i];
    }
    pthread_mutex_unlock(&s_dummy_lock);
}

void I2cDrv_DummyWrite(void *ctx, uint8_t dev, uint8_t reg, const uint8_t *data, size_t len)
{
    (void)ctx;
    if (!data) {
        return;
    }

    pthread_mutex_lock(&s_dummy_lock);
    for (size_t i = 0; i < len; i++) {
        s_dummy_read_data[dev][reg + i] = data[i];
    }
    pthread_mutex_unlock(&s_dummy_lock);
    printf("[I2C] WRITE dev=0x%02X reg=0x%02X len=%zu\n", dev, reg, len);
}

void I2cDrv_DummySetReadData(uint8_t dev, uint8_t reg, const uint8_t *data, size_t len)
{
    if (!data) {
        return;
    }

    pthread_mutex_lock(&s_dummy_lock);
    for (size_t i = 0; i < len; i++) {
        s_dummy_read_data[dev][reg + i] = data[i];
    }
    pthread_mutex_unlock(&s_dummy_lock);
}
