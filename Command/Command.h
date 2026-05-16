#ifndef COMMAND_H
#define COMMAND_H


//コマンド専用タスクを作ること。
//コマンドは標準入力を受け付け待ちする。受付状態が分かるように
// >
//を表示して待つ。
//コマンドの詳細は.c側で定義すること。不要な関数を公開しないこと
//文字列と関数ポインタのペアをテーブルとして持ち、コマンド入力後に該当する関数ポインタを実行すること
//引数を設定できること
//対応コマンド一覧

// temp 実温度
//  I2cDrv_DummySetReadDataを用いてダミー温度を設定する

// speed モーターの速度
//   Motor_SetSpeedを呼び出す

//　mode モータのモード
//   Motor_SetModeを呼び出す

// test
//  Test_MotorTestを呼び出す
void Command_Init(void);

void Command_SyncTest(void);

#endif
