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

## 使用方法

1. ラズパイ Pico W の BOOTSEL ボタンを押しながら PC に接続すると USB メモリとして認識され、ファームウェア書き込みモードになります。`x68kzrmthds.uf2` を認識されたドライブにドロップするとファームウェアが書き込まれます。
2. 書き込みが完了すると、再び USB メモリとして認識されます。認識されたドライブにはファイル `config.txt` と `log.txt` があるはずです。このうち `config.txt` をメモ帳などで開きます。
    ```
    [X68000Z Remote HDS service configuration]
    
    # WiFi 接続先の SSID
    WIFI_SSID: <ssid>
    # WiFi 接続先のパスワード
    WIFI_PASSWORD: ********
    
    # Windows ファイル共有の URL
    SMB2_URL: \\<server>\<share>\<path>\<file>.HDS
    # Windows ファイル共有のユーザ名
    SMB2_USERNAME: <user>
    # Windows ファイル共有のパスワード
    SMB2_PASSWORD: ********
    # Windows のワークグループ名
    SMB2_WORKGROUP: WORKGROUP
    ```
3. ファイルを編集して、WiFi 接続情報や利用したい HDS ファイルの場所を設定します。
    * 元ファイルの `<ssid>` や `********` などの項目を消して実際の設定で上書きしてください。
    * 設定変更後に `config.txt` を再度開くと、パスワード以外の項目は過去の設定内容で置き換えられます。パスワードは常に `********` のままですが、パスワード変更が不要であればこのままにしておいて大丈夫です。
4. HDS ファイルは、事前に Windows 側で適切にネットワーク共有設定を行ってファイル共有できるようにしておきます。
5. ファイルを上書き保存すると、USB メモリの認識が一度解除された後、設定を反映して再度認識されるようになります。`log.txt` をメモ帳などで開いて、以下のような内容が書かれていれば正常に認識されています。
    ```
    X68000Z remote HDS service
    Connecting to WiFi...
    Connected to <接続先のSSID> as <IPアドレス> as host PicoW
    SMB2 connection server:<接続先サーバ> share:<接続先共有名>
    SMB2 connection established.
    Start USB MSC device.
    File <HDSファイル名> size=<ファイルサイズ>
    ```
    * エラーが記録されている場合は、`config.txt` の設定内容を確認して修正を行ってください。
6. 正常に認識されていれば `X68000Z` フォルダができてその中に `pscsi.ini` ファイルと `disk0.hds` ファイルがあるはずです。`disk0.hds` ファイルはネットワーク共有先の HDS ファイルと同じサイズ、同じ内容になります。
7. 正常な接続を確認できたら、ラズパイ Pico W を X68000Z 本体のUSB に接続して X68000Z を起動します。後は通常の SCSI イメージファイルと同じ使い方になります。

## トラブルシューティング
* ラズパイ Pico W の LED が高速点滅して(0.3秒周期) 'config.txt` に以下の内容が書かれている。
    ```
    Connecting to WiFi...
    Failed to connect.
    ```
    * ==> WiFi の SSID またはパスワード設定に誤りがあります。
    * ラズパイ Pico W の WiFi は 2.4GHz 帯のみをサポートしています。5GHz 帯を使用する 802.11a のアクセスポイントには接続できないことに注意してください
* ラズパイ Pico W の LED が低速点滅して(1秒周期) 'config.txt` に以下の内容が書かれている。
    ```
    Connecting to WiFi...
    Connected to <接続先のSSID> as <IPアドレス> as host PicoW
    SMB2 connection server:<接続先サーバ> share:<接続先共有名>
    smb2_connect_share failed. XXXXXX
    ```
    * ==> Windows ファイル共有 (SMB2) のURLやユーザ名、パスワード設定に誤りがあります。
    * 接続先サーバ名の名前解決ができない場合は IP アドレスを指定してみてください。
    * 共有名と実際のフォルダ名の大文字小文字が合っていないとファイル共有が上手く行かないことがあるようです。その場合は共有名を Windows 上の実際のフォルダ名と同じ名前にしてみてください。
* `config.txt` の設定に問題があるとUSBメモリとして認識されなくなってしまい、それ以降設定の書き換えもできなくなってしまうことがあるようです。このような場合は一度、以下の場所にあるファイル `flash_nuke.uf2` を Pico W のファームウェア書き換えモードで書き込んでみてください。内蔵フラッシュメモリが初期化されるので、再度本アプリを書きこむと初回設定時と同じ状態に戻ります。
    * [https://datasheets.raspberrypi.com/soft/flash_nuke.uf2](https://datasheets.raspberrypi.com/soft/flash_nuke.uf2)



## ビルド方法

ソースコードからのビルドを行う際には、事前に Raspberry Pi Pico SDK のセットアップが必要です。

1. Pico SDK をセットアップした PC に本リポジトリを clone します。
2. `make` を実行すると、追加で必要なリポジトリを clone してビルドを開始します。
3. ビルドが完了すると生成されるファイル、`build/x68kzrmthds.uf2` がラズパイ Pico W へ書き込むファイルとなります。

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
