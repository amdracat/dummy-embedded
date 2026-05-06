//疑似的なMotor制御を行うダミーモジュール
//PosixOs_SetupTimerは目標速度である。
//温度が-10～40度のときだけHIGHSPEEDが出せる（PORT_HIGHSPEEDを1にする）
//それ以外の温度は全てLOWSPEEDになる（PORT_HIGHSPEEDは0にする）

// Motor専用タスクとメッセージキューを生成する
//PosixOs_EventSubscribeでTEMPERATURE_UPDATE_EVENTを購読する
//PosixOs_SetupTimerで200msの制御周期を作る
//初期化のみ呼び出しコンテキストで実施してよい
void Motor_Init(void);

typedef enum MotorSpeed {
    MOTOR_SPEED_STOP = 0,
    MOTOR_SPEED_SLOW = 1,
    MOTOR_SPEED_FAST = 2
} MotorSpeed;

//指示は必ずMotor専用タスクに送ること。
void Motor_SetSpeed(MotorSpeed speed);

//同期的に取得する。つまり内部変数をそのまま返すこと。時間はかけない
MotorSpeed Motor_GetSpeed(void);

//時間はPosixOs_SetupTimerで作ること。処理を実行する際は必ずMotor専用タスクに送ること。
typedef enum MotorMode {
    MOTOR_MODE_STOP, //100ms待ち、PORT_ENBALE_MOTORを0にする。(PORT_LEFTなどすべて０にする)
    MOTOR_MODE_LEFT, //PORT_ENBALE_MOTORを0、PORT_LEFTなどすべて０にする。30msまち、PORT_LEFTを1にし、その後さらに50ms待ち、PORT_ENBALE_MOTORを1にする
    MOTOR_MODE_RIGHT, //PORT_ENBALE_MOTORを0、PORT_LEFTなどすべて０にする。30msまち、PORT_RIGHT1にし、その後さらに50ms待ち、PORT_ENBALE_MOTORを1にする
    MOTOR_MODE_FRONT,//PORT_ENBALE_MOTORを0、PORT_LEFTなどすべて０にする。100msまち、PORT_FRONTを1にし、その後さらに50ms待ち、PORT_ENBALE_MOTORを1にする
    MOTOR_MODE_BACK//PORT_ENBALE_MOTORを0、PORT_LEFTなどすべて０にする。200msまち、PORT_BACKを1にし、その後さらに50ms待ち、PORT_ENBALE_MOTORを1にする
} MotorMode;

//指示は必ずMotor専用タスクに送ること。
//内部的に状態変数を持ち、状態変数は状態切り替えが完了した際に書き換えること（時間がかかる）
//各モードの詳細はMotorModeのコメントを参照すること
void Motor_SetMode(MotorMode mode);

////同期的に取得する。つまり内部変数をそのまま返すこと。時間はかけない
MotorMode Motor_GetMode(void);  
