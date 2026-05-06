
typedef enum Port{
    PORT_LEFT,
    PORT_RIGHT,
    PORT_FRONT,
    PORT_BACK,
    PORT_HIGHSPEED,
    PORT_ENBALE_MOTOR
} Port;

typedef struct {
    void *ctx;   // HAL / PC Dummy の内部状態
    int (*read)(Port port);
    void (*write)(Port port, int value);
} gpio_t;

void GpioDrv_Init(gpio_t*);

int GpioDrv_ReadPin(gpio_t*,Port port);
void GpioDrv_WritePin(gpio_t*,Port port, int value);


//以下はPC実行用のダミー関数セット
//gpio_tに以下を与える
int GpioDrv_DummyReadPin(Port port);
void GpioDrv_DummyWritePin(Port port, int value);


//以下はGpioDrv_DummyReadPinの挙動を変えるデバッグ関数
void GpioDrv_DummySetReadData(Port port, int value);