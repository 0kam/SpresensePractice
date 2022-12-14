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
- video.ino
Spresenseの[VideoStream機能](https://developer.sony.com/develop/spresense/docs/arduino_developer_guide_ja.html#_camera%E3%81%AEpreview%E3%82%92%E5%8F%96%E5%BE%97%E3%81%99%E3%82%8Bvideostream%E6%A9%9F%E8%83%BD)を使って動画(MotionJPEG、AVIファイルとして保存)を作成するスクリプト。[AVILibrary](https://github.com/YoshinoTaro/AviLibrary_Arduino)を使用。事前にこれをインストール（GithubからZIPでダウンロード -> ArduinoIDEでZIPファイルからライブラリをインポート）しておくこと。HDRカメラのQuadVGAサイズで13fpsくらい出る。
