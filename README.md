# x68kzrmthds - X68000Z から Windows PC 内の SCSI ディスクイメージファイルを利用する

## 概要

[X68000Z](https://www.zuiki.co.jp/x68000z/) の ver.1.3.1 エミュレータでサポートされた Pseudo SCSI 機能を用いて、ネットワーク上の Windows PC 内の SCSI ディスクイメージファイル (HDSファイル) を利用します。

X68000Z にはネットワーク機能がありませんが、無線 LAN を搭載したボードコンピュータ [Raspberry Pi Pico W](https://www.raspberrypi.com/documentation/microcontrollers/raspberry-pi-pico.html) がネットワーク共有されたファイルを USB メモリ上の HDS ファイルに見せかけることで、X68000Z から間接的にネットワーク上のディスクイメージを利用できるようにしています。

## 必要なもの

X68000Z 本体に加えて以下が必要です。

* Raspberry Pi Pico W (以下、ラズパイ Pico W)
  * ラズパイ Pico にはいくつか製品バリエーションがありますが、W が付かないものは無線 LAN 機能を持たないため使用できません
  * Pico W (ヘッダピンなし)、Pico WH (ヘッダピン付き) のどちらでも使用できます
* USB micro-B ケーブル
  * ラズパイ Pico W を X68000Z に接続したり、ファームウェア書き込みのため PC に接続したりするために必要です
* Raspberry Pi Pico SDK のセットアップ
  * 現バージョンは WiFi の接続情報やネットワーク共有の情報がソースコード埋め込みになっているため、各自の環境に合わせてソースコードからのビルドが必要です
  * [Getting started with Raspberry Pi Pico](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf) (PDFファイル) に従って、SDK 環境のセットアップを行います
  * 日本語版のドキュメントもあるようです。[Raspberry Pi Pico をセットアップしよう](https://datasheets.raspberrypi.com/pico/getting-started-with-pico-JP.pdf) (PDFファイル)

## ビルドと書き込み

1. Pico SDK をセットアップした PC に本リポジトリを clone します。
2. `config.txt` ファイルを編集して、WiFi 接続情報や利用したい HDS ファイルの場所を設定します。
    ```
    #
    # WiFi, Windows ファイル共有の設定ファイル
    #
    
    # WiFi 接続先の SSID と パスワードを設定
    set(WIFI_SSID ssid_name)
    set(WIFI_PASSWORD wifi_name)
    
    # Windows ファイル共有の URL とユーザ名、パスワードを設定
    set(SMB2_URL smb://myserver.lan/myshare/path/hdsfile.hds)
    set(SMB2_USERNAME username)
    set(SMB2_PASSWORD password)
    ```
3. 設定した HDS ファイルは、事前に Windows 側で適切にネットワーク共有設定を行ってファイル共有できるようにしておきます。
4. `make` を実行すると、追加で必要なリポジトリを clone してビルドを開始します。
5. ビルドが完了すると生成されるファイルのうち、`build/x68kzrmthds.uf2` をラズパイ Pico W に書き込みます。本体の BOOTSEL ボタンを押しながら PC に接続すると USB メモリとして認識されるので、そこに UF2 ファイルをコピーすれば OK です。

## 実行

1. まずは正常にネットワークにつながることを確認するため、PC に接続します。UF2 ファイル書き込み後、BOOTSEL ボタンを押さずに PC に接続してしばらくすると、USB メモリとして認識されます。
2. 認識されたドライブの中には `X68000Z` フォルダと `log.txt` ファイルがあるはずです。`log.txt` をメモ帳などで開いて、以下のような内容が書かれていれば正常に認識されています。
    ```
    X68000Z remote HDS service
    Connecting to WiFi...
    Connected to <接続先のSSID> as <IPアドレス> as host PicoW
    SMB2 connection server:<接続先サーバ> share:<接続先共有名>
    SMB2 connection established.
    Start USB MSC device.
    File <HDSファイル名> size=<ファイルサイズ>
    ```
    * エラーが記録されている場合は、`config.txt` の設定内容を確認して再度ビルドを行ってください。
3. `X68000Z` フォルダの中には `pscsi.ini` ファイルと `disk0.hds` ファイルがあります。`disk0.hds` ファイルはネットワーク共有先の HDS ファイルと同じサイズ、同じ内容になっているはずです。
4. 正常な接続を確認できたら、ラズパイ Pico W を X68000Z 本体のUSB に接続して X68000Z を起動します。後は通常の SCSI イメージファイルと同じ使い方になります。

## 注意

* ネットワーク接続のパスワード情報などはラズパイ Pico W の中に平文で記録されます。接続情報を設定した Pico W の管理にはご注意ください。
* ラズパイ Pico W の USB 機能がフルスピード (12Mbps) までと遅いため、通常の USB メモリからの起動に比べるともっさりします(FD イメージと比べれば十分速いですが…)。

## ライセンス

本プログラムは、オリジナルで開発したソースコードについては MIT ライセンスとします。その他利用されている以下のソフトウェアについてはそれぞれ開発元のライセンス条件に従います。

* Pico SDK (3-clause BSD)
* TinyUSB (MIT)
* lwIP (3-clause BSD)
* FreeRTOS kernel (MIT)
* libsmb2 (LGPL-2.1)

本プログラムは LGPL を採用している libsmb2 を静的リンクしているため、ビルド生成物のバイナリ配布の際には LGPL が適用されます。独自の修正を加えてビルドしたバイナリを配布する際にはソースコードの開示が必要となります。
