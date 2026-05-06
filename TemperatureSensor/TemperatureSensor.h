#ifndef TEMPERATURE_SENSOR_H
#define TEMPERATURE_SENSOR_H

#define TEMPERATURE_UPDATE_EVENT 1

//現在の実装は温度計タスクは存在しない
//温度系専用タスクとメッセージキューを生成すること。
//1秒ごとに温度を取得する際も必ず温度計専用タスクで処理すること。
//初期化のみ呼び出しコンテキストで実施してよい


void TemperatureSensor_Init();
int TemperatureSensor_GetTemperature();
#endif // TEMPERATURE_SENSOR_H