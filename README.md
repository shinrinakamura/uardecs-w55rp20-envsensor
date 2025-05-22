# UARDECS for W55RP20

このライブラリは、元々 Arduino UNO / Mega 向けに開発された「UARDECS」ライブラリを、W55RP20-EVB-PICO 向けに移植したものです。  
W55RP20 に内蔵された Ethernet 機能と、軽量 TCP/IP スタック（lwIP）を活用することで、外付けの Ethernet シールドなしで UECS 通信が可能です。

---

## 🔧 対応環境

| 項目             | 内容                                      |
|------------------|-------------------------------------------|
| 対応ハードウェア | W55RP20-EVB-PICO                          |
| 開発環境         | Arduino IDE 2.x                           |
| 使用ライブラリ   | lwIP, EthernetCompat                      |
| 元ライブラリ     | UARDECS（Ken-ichiro Yasuba・Hideto Kurosaki）作         |
| 移植・改変       | Shinri Nakamura（2025年5月20日）                  |

---

## 📌 概要（元の README より引用、一部注釈付き）

> UARDECS は日本の施設園芸用の通信規格である "UECS" 準拠のプロトコルを Arduino に実装するためのライブラリです。  
> Arduino UNO または Arduino Mega に対応しています（AVR 系 Arduino 専用）。  
> このライブラリを実行するには Ethernet Shield2 あるいは W5500 を搭載したシールドを装着する必要があります。  
> このライブラリを使うときは一緒に Ethernet2 ライブラリをインストールしてください。  
> ※ 本移植版では上記のハードウェアとライブラリは必要ありません。

📘 詳細な日本語マニュアル：[https://uecs.org/arduino/uardecs.html](https://uecs.org/arduino/uardecs.html)

---

## ✅ 本移植版の特徴

- W55RP20 の内蔵 Ethernet（lwIP）を使用（外付けシールド不要）
- UECS プロトコル（CCM）の送信・受信処理に対応
- Webブラウザからアクセス可能な設定画面を追加
- `EthernetCompat.h` を用いて Arduino 互換の Ethernet API に対応

---

## 🔌 I²Cピン設定例

| 信号 | ピン番号 |
|------|----------|
| SCL  | GPIO5    |
| SDA  | GPIO4    |
| VCC  | 3.3V     |
| GND  | GND      |

※デフォルト設定ではこのようにしていますが、inoファイルにマクロ定義で書いていますので、変更する必要がある際は変更してください。詳細はRP2040のデータシートを確認してください。

---

## 🛠️ 使用方法

1. **W55RP20-EVB-PICO 用のボードマネージャをインストールしてください。**  
   👉 [参考リンク：Karakuri Musha](https://karakuri-musha.com/inside-technology/arduino-raspberrypi-pico-helloworld01/)

2. **GPIO14 を GND に接続して Safe Mode に入ります。**

3. **Safe Mode 中のデフォルトネットワーク設定は `.ino` ファイル内で調整してください。**

4. **ネットワーク設定を保存後は、Safe Mode に入る必要はありません。**

---

## 🔧 主な技術的変更点

- `#include <EthernetCompat.h>` を追加
- `#include <W55RP20lwIP.h>` を追加
- `#include <pico/unique_id.h>` を追加
- `#include <WiFi.h>`、`#include <WiFiUdp.h>` を追加
- `PROGMEM` 使用を廃止
- クライアント生成を `ArduinoEthernet<Wiznet55rp20> Ethernet(1);` に変更
- MAC アドレスをユニーク ID に基づいて自動生成するように変更
- `UECSlogserver.available()` を `UECSlogserver.accept()` に変更
- バッファサイズを 500 → 1024 に拡張
- 疑似 EEPROM（フラッシュ保存）を使用
- safe mode確認用のU_InitPin_Senseはローに設定

---

## 🔄 変更履歴

- **2025/05/20**  
  - W55RP20 対応としてコードを移植  
  - lwIP ベースの通信スタックに対応  
  - UECS CCM パケットの送信・受信処理を確認  

---

## ⚠ 注意事項

- Arduino IDE にて RP2040 ボードのサポートを有効にしてください。
- `.ino` ファイル内の設定でネットワークや初期動作を調整可能です。
- ソースコードは MIT ライセンスに基づき公開されています（下記参照）。

---

## 📜 ライセンス

このライブラリは MIT ライセンスのもとで公開されています。  
元の著作権表示を保持したうえで、以下の通り改変を行いました<br>
Copyright 2022 Ken-ichiro Yasuba, Hideto Kurosaki<br>
Modified 2025 by Shin Nakamura<br>
詳細は `LICENSE` ファイルをご参照ください。

