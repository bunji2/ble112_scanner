# bled112_scanner

作成者：Bunji2
更新日時：2015/09/24

## 概要

BGAPI による BLE アドバタイズパケットをスキャンするサンプルプログラム

## 履歴

* Version 1.1: 時刻をミリ秒での表示に変更

## ble_scanner.exe の使い方

###●コマンドラインヘルプ

```
C:\work\td>ble_scanner
ble_scanner <list|COMx> <info|scan>
```

###●BLEDデバイスのリスト表示

```
C:\work\td>ble_scanner list
Bluegiga Bluetooth Low Energy (COM3)
```

###●BLEDデバイスの情報表示

```
C:\work\td>ble_scanner COM3 info
#ble_rsp_system_get_info
major=1, minor=2, patch=2, build=100, ll_version=3, protocol_version=1, hw=3
```

###●スキャン（Ctrl-C で終了）
```
C:\work\td>ble_scanner COM3 scan
YYYY/MM/DD hh:mm:ss.zzz,XX:XX:XX:XX:XX:XX,RSSI:XX,Flags:X,Name:X,Services:[X,X,X]
YYYY/MM/DD hh:mm:ss.zzz,XX:XX:XX:XX:XX:XX,RSSI:XX,iBeacon,UUID:X,major:X,minor:X,txpower:X
Ctrl-C!
```
