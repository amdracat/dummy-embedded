
#include <stdint.h>
#include <stddef.h>

typedef struct {
    void *ctx;   // HAL / PC Dummy の内部状態
    void (*read)(void *ctx, uint8_t dev, uint8_t reg, uint8_t *buf, size_t len);
    void (*write)(void *ctx, uint8_t dev, uint8_t reg, const uint8_t *data, size_t len);
} i2c_bus_t;


//PC実行のためにダミーを設定すること
void I2cDrv_Init(i2c_bus_t *bus);


void I2cDrv_Read(
    i2c_bus_t *bus,
    uint8_t dev_addr,
    uint8_t reg_addr,
    uint8_t *buf,
    size_t len
);

void I2cDrv_Write(
    i2c_bus_t *bus,
    uint8_t dev_addr,
    uint8_t reg_addr,
    const uint8_t *data,
    size_t len
);

//以下はPC実行用のダミー関数セット
//i2c_bus_tに以下を与える
void I2cDrv_DummyRead(void *ctx, uint8_t dev, uint8_t reg, uint8_t *buf, size_t len);
void I2cDrv_DummyWrite(void *ctx, uint8_t dev, uint8_t reg, const uint8_t *data, size_t len);

//以下はI2cDrv_DummyReadの挙動を変えるデバッグ関数
void I2cDrv_DummySetReadData(uint8_t dev, uint8_t reg, const uint8_t *data, size_t len);
