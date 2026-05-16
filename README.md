# dummy-embedded

## 概要
組み込みソフトウェアを模擬したプログラムです。

## ビルド方法
make clean
make

## 実行方法
./main.exe

## 動作モード
### 実時間モード
タスク、タイマーが実時間で動作します。
ログを垂れ流しながらの動作確認できます。

```c:OsTestLayer\OsTestLayer.h
//#define OS_TEST_LAYER_ENABLE
```

### テストモード
OSは全てスタブに置き換わり、非同期タイマー、メッセージ界面のみ別キューに保持します。実行と同時にテストのみ実行します。

```c:OsTestLayer\OsTestLayer.h
#define OS_TEST_LAYER_ENABLE
```