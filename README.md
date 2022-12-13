# SpresensePractice
Spresenseの練習用に作成したArduinoスケッチ。

## Audio
- continuous_recorder.ino  
一定時間の録音を繰り返して連番のmp3 or wavで吐き出すスクリプト。
録音が失敗することがあるので、Watchdogを使った監視も導入した。

## Camera
- camera.ino
任意のパラメータ（ホワイトバランスモード、ISO、露光時間、HDR）で写真を撮るためのスクリプト。
複数枚のJPEG写真を連番で保存する。