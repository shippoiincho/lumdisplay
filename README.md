# LUMDisplay

秋月電子のお楽しみ袋に入っていた、ROHM 製 LUM LED　ディスプレイを使った電光掲示板です。

Yahoo! ヘッドラインニュースを 10 分毎に取得して表示します。
マイコンは ESP-32E の 16MB 版を使っていますが、4MB 版でもパーティションを `Minimal SPIFFS` にすれば収まるはずです。
開発環境は Arduino を使っています。

データについては以下を使用しています。

- [k8x12](https://littlelimit.net/k8x12.htm) 日本語フォント
- [UTF16 to JIS 変換テーブル](http://ash.jp/code/unitbl21.htm)

一部以下のコードを使用しています。

- [efont Unicode BDF Font Data](https://github.com/tanakamasayuki/efont) UTF8 to UTF16 変換
- [mgo-tec さんの ESP32_WebGET](https://github.com/mgo-tec/ESP32_WebGet) Yahoo! ニュースの取得

詳しくは[製作記](https://shippoiincho.github.io/posts/23/)を参照してください。
