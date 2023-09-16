# x68kzremotedrv - X68000Z Remote Drive service with Raspberry Pi Pico W

## 概要

[X68000Z](https://www.zuiki.co.jp/x68000z/) の ver.1.3.1 エミュレータでサポートされた Pseudo SCSI 機能を用いて、ネットワーク上の Windows PC 内のファイルを SCSI ディスクイメージやリモートドライブとして利用します。

X68000Z にはネットワーク機能がありませんが、無線 LAN を搭載したボードコンピュータ [Raspberry Pi Pico W](https://www.raspberrypi.com/documentation/microcontrollers/raspberry-pi-pico.html) がネットワーク共有されたファイルを USB メモリ上の HDS ファイルに見せかけることで、X68000Z から間接的にネットワーク上のリソースを利用できるようにしています。

* SCSI ディスクイメージファイル (HDSファイル)\
Windows PC 内にある HDS ファイルを SCSI ディスクイメージとして X68000Z から参照でき、ここから起動することもできます。
既存の X68k エミュレータで使用している HDS ファイルを PC 上に置いたまま利用することができます。
* リモートドライブ\
Windows PC 内の特定のフォルダ以下のファイルをリモートドライブとして X68000Z から参照できるようにします。
PC 上のファイルを直接 X68000Z から利用できます。HUMAN.SYS や CONFIG.SYS など、起動に必要なファイルがあればリモートドライブからの起動も可能です。

(従来 x68kzrmthds として公開していたアプリを、リモートドライブ対応に伴い名称を変更しました)

## 必要なもの

X68000Z 本体に加えて以下が必要です。

* Raspberry Pi Pico W (以下、ラズパイ Pico W)
  * ラズパイ Pico にはいくつか製品バリエーションがありますが、W が付かないものは無線 LAN 機能を持たないため使用できません
  * Pico W (ヘッダピンなし)、Pico WH (ヘッダピン付き) のどちらでも使用できます
* USB micro-B ケーブル
  * ラズパイ Pico W を X68000Z に接続したり、ファームウェア書き込みのため PC に接続したりするために必要です

## 使用方法

1. ラズパイ Pico W の BOOTSEL ボタンを押しながら PC に接続すると USB メモリとして認識され、ファームウェア書き込みモードになります。`x68kzremotedrv.uf2` を認識されたドライブにドロップするとファームウェアが書き込まれます。
2. 書き込みが完了すると、再び USB メモリとして認識されます。認識されたドライブにはファイル `config.txt` と `log.txt` があるはずです。このうち `config.txt` をメモ帳などで開きます。
    ```
    [X68000Z Remote Drive Service Configuration]

    # WiFi 接続先の SSID、パスワード
    WIFI_SSID: <ssid>
    WIFI_PASSWORD: ********

    # Windows ファイル共有のユーザ名、パスワード、ワークグループ名
    SMB2_USERNAME: <user>
    SMB2_PASSWORD: ********
    SMB2_WORKGROUP: WORKGROUP
    # Windows ファイル共有のサーバ名、共有名
    SMB2_SERVER: <server>
    SMB2_SHARE: <share>

    # X68000Z に見せるリモートドライブ/HDSの場所
    # SCSI ID 0～6 について Windows ファイル共有のパスを指定する
    # HDS ファイル名を指定した場合はリモートHDS
    # ディレクトリ名を指定した場合はリモートドライブとして扱う
    ID0: 
    ID1: 
    ID2: 
    ID3: 
    ID4: 
    ID5: 
    ID6: 

    # タイムゾーン設定
    TZ: JST-9
    ```
3. ファイルを編集して、WiFi 接続情報や Windows ファイル共有の情報、利用したいリモートドライブや HDS ファイルの場所を設定します。
    * 元ファイルの `<ssid>` や `********` などの項目を消して実際の設定で上書きしてください。
    * 設定変更後に `config.txt` を再度開くと、パスワード以外の項目は過去の設定内容で置き換えられます。パスワードは常に `********` のままですが、パスワード変更が不要であればこのままにしておいて大丈夫です。
    * ID0～ID6 には、X68000Z の SCSI ID 0～6 のそれぞれに割り当てるリモートドライブや HDS ファイルの場所 (Windows ファイル共有の共有名から下のディレクトリ名) を指定します。
      * HDS ファイルの名前を指定した場合はディスクイメージがその ID に割り当てられます。
      * ディレクトリ名を指定した場合はその ID を通信用に使用して、ディレクトリ以下をリモートドライブとして割り当てます。X68000Z には起動時にリモートドライブアクセス用のデバイスドライバが起動時に自動的に組み込まれます。
      (現状、リモートドライブは 1 つしか設定できません)

4. リモートドライブやHDS ファイルは、事前に Windows 側で適切にネットワーク共有設定を行ってファイル共有できるようにしておきます。
5. ファイルを上書き保存すると、USB メモリの認識が一度解除された後、設定を反映して再度認識されるようになります。`log.txt` をメモ帳などで開いて、以下のような内容が書かれていれば正常に認識されています。
    ```
    X68000Z Remote Drive Service (version xxxxxxxx)
    Connecting to WiFi...
    Connected to <接続先のSSID> as <IPアドレス> as host PicoW
    SMB2 connection server:<接続先サーバ> share:<接続先共有名>
    SMB2 connection established.
    Start USB MSC device.
    ID=0 dir:<リモートドライブディレクトリ名>         (リモートドライブの場合)
    ID=1 file:<HDSファイル名> size:<ファイルサイズ>  (HDSファイルの場合)
    ```
    * エラーが記録されている場合は、`config.txt` の設定内容を確認して修正を行ってください。
6. 正常に認識されていれば `X68000Z` フォルダができてその中に `pscsi.ini` ファイルと `disk0.hds` ～ `disk6.hds` ファイルがあるはずです。HDS ファイルを割り当てた場合、ネットワーク共有先の HDS ファイルと同じサイズ、同じ内容になります。
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
* `config.txt` の設定に問題があるとUSBメモリとして認識されなくなってしまい、それ以降設定の書き換えもできなくなってしまうことがあるようです。このような場合は、同梱の `clrconfig.uf2` を Pico W のファームウェア書き換えモードで書き込んでください。アプリはそのままに、記録されている設定内容のみをクリアして再起動します。

## 注意と制約事項

* ネットワーク接続のパスワード情報などはラズパイ Pico W の中に平文で記録されます。接続情報を設定した Pico W の管理にはご注意ください。
* ラズパイ Pico W の USB 機能がフルスピード (12Mbps) までと遅いため、通常の USB メモリからの起動に比べるともっさりします(FD イメージと比べれば十分速いですが…)。
* リモートドライブには現状、以下の制約事項があります。
  * リモートドライブ上ではファイルアトリビュートの隠しファイルやシステム属性、書き込み禁止属性などは無視されます
  * Human68k の DSKFRE が 2GB 以上のディスクサイズを想定していないため、ドライブの残容量表示は不正確です
  * リモートドライブ上のファイルは作成日時を変更できません。

## ビルド方法

ソースコードからのビルドを行う際には、事前に Raspberry Pi Pico SDK のセットアップが必要です。

1. Pico SDK をセットアップした PC に本リポジトリを clone します。
2. `make` を実行すると、追加で必要なリポジトリを clone してビルドを開始します。
3. ビルドが完了すると生成されるファイル、`build/x68kremotedrv.uf2` がラズパイ Pico W へ書き込むファイルとなります。

## 謝辞

Human68k のリモートドライブの実装は以下を参考にしています。開発者の皆様に感謝します。

* [ぷにぐらま～ずまにゅある](https://github.com/kg68k/puni) by 立花@桑島技研 氏
  * [filesystem.txt](https://github.com/kg68k/puni/blob/main/filesystem.txt)
* [XEiJ (X68000 Emulator in Java)](https://stdkmd.net/xeij/) by Makoto Kamada 氏
  * ソースコード [HFS.java](https://stdkmd.net/xeij/source/HFS.htm)
* [XM6 TypeG](http://retropc.net/pi/xm6/index.html) by PI. 氏 & GIMONS 氏
  * XM6 version 2.06 ソースコード

リモートドライブ起動機能は、X68k エミュレータ XEiJ のホストファイルシステム機能 (HFS) に大きく触発されて開発されました。
ホストマシンのファイルシステムからの直接起動という素晴らしい機能を実装された Makoto Kamada 氏に感謝します。

Windows ファイル共有のアクセスについては、ライブラリ libsmb2 の存在に大きく助けられました。
当初ラズパイ Zero W を用いて Linux ベースでの開発を検討していましたが、起動速度や消費電力の点で現実的でないことが分かり、ターゲットをラズパイ Pico W に変更して FreeRTOS ベースでの開発に切り替えました。これが可能だったのも libsmb2 の存在あってのことでした。開発者の Ronnie Sahlberg 氏に感謝します。

## ライセンス

本プログラムは、オリジナルで開発したソースコードについては MIT ライセンスとします。その他利用されている以下のソフトウェアについてはそれぞれ開発元のライセンス条件に従います。

* Pico SDK (3-clause BSD)
* TinyUSB (MIT)
* lwIP (3-clause BSD)
* FreeRTOS kernel (MIT)
* libsmb2 (LGPL-2.1)

本プログラムは LGPL を採用している libsmb2 を静的リンクしているため、ビルド生成物のバイナリ配布の際には LGPL が適用されます。独自の修正を加えてビルドしたバイナリを配布する際にはソースコードの開示が必要となります。
