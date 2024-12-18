/*
 * Copyright (c) 2023 Yuichi Nakamura (@yunkya2)
 *
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <x68k/iocs.h>
#include <x68k/dos.h>

#include <zusb.h>

#include "config.h"
#include "vd_command.h"
#include "settinguisub.h"

#include "zusbcomm.h"

//****************************************************************************
// Global variables
//****************************************************************************

struct zusb_rmtdata *zusb_rmtdata;
jmp_buf jenv;                       //タイムアウト時のジャンプ先

int sysstatus = STAT_WIFI_DISCONNECTED;
struct config_data config
#ifdef XTEST
= {
  .selfboot = 1,
  .tz = "JST-9",
  .tadjust= 2,
}
#endif
;
int menumode;

#ifndef BOOTSETTING
int crtmode;
#endif

//****************************************************************************
// Definition
//****************************************************************************

#define isvisible(n)  (((itemtbl[menumode][n].stat) & 0xf) <= sysstatus && \
                       !((itemtbl[menumode][n].stat & 0x20) && \
                         (config.remoteunit <= ((itemtbl[menumode][n].stat & 0xf00) >> 8))) && \
                       !((itemtbl[menumode][n].stat & 0x10000) && \
                         (config.hdsunit <= ((itemtbl[menumode][n].stat & 0xf000) >> 12))))
#define istabstop(n)  ((itemtbl[menumode][n].stat) & 0x10)
#define issetconf(n)  ((itemtbl[menumode][n].stat) & 0x40)
#define isupdconf(n)  ((itemtbl[menumode][n].stat) & 0x80)

int flash_config(struct itemtbl *it, void *v);
int flash_clear(struct itemtbl *it, void *v);

int switch_menu(struct itemtbl *it, void *v);

//****************************************************************************
// Menu data
//****************************************************************************

static struct numlist_opt opt_bool = { 0, 1 };
static struct numlist_opt opt_rmtunit = { 0, 8 };
static struct numlist_opt opt_hdsunit = { 0, 4 };
static struct numlist_opt opt_tadjust = { 0, 4 };

struct itemtbl itemtbl0[] = {
  { 0x010, 4, 4, -1,   "SELFBOOT",
    "リモートドライブ/HDSからの起動を行うかどうかを設定します",
    "リモートドライブ/HDSからの起動を行うかどうかを選択してください (0=他のUSBメモリから起動)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 8, (char *)&config.selfboot,  sizeof(config.selfboot),       input_numlist, &opt_bool },

  { 0x010, 4, 7, -1,   "SSID",
    "WiFi 接続先の SSID を設定します",
    "WiFi 接続先の SSID を選択してください",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 28, config.wifi_ssid,       sizeof(config.wifi_ssid),      input_wifiap },
  { 0x040, 4, 8, -1,   "PASSWORD",
    "WiFi 接続先のパスワードを設定します",
    "WiFi 接続先のパスワードを入力してください",
    "#e  (確定) #f   (前に戻る) #g   (パスワードを表示)",
    16, 16, config.wifi_passwd,     sizeof(config.wifi_passwd),    input_passwd, (void *)CONNECT_WIFI },

  { 0x012, 4, 11, -1,   "USERNAME",
    "Windows ファイル共有のユーザ名を設定します",
    "Windows ファイル共有のユーザ名を入力してください",
    "#e  (確定) #f   (前に戻る)",
    16, 16, config.smb2_user,       sizeof(config.smb2_user),      input_entry },
  { 0x002, 4, 12, -1,   "PASSWORD",
    "Windows ファイル共有のパスワードを設定します",
    "Windows ファイル共有のパスワードを入力してください",
    "#e  (確定) #f   (前に戻る) #g   (パスワードを表示)",
    16, 16, config.smb2_passwd,     sizeof(config.smb2_passwd),    input_passwd },
  { 0x002, 4, 13, -1,  "WORKGROUP",
    "Windows ファイル共有のワークグループを設定します",
    "Windows ファイル共有のワークグループを入力してください",
    "#e  (確定) #f   (前に戻る)",
    16, 16, config.smb2_workgroup,  sizeof(config.smb2_workgroup), input_entry },
  { 0x042, 4, 14, -1,  "SERVER",
    "Windows ファイル共有のサーバ名を設定します",
    "Windows ファイル共有のサーバ名または IP アドレスを入力してください",
    "#e  (確定) #f   (前に戻る)",
    16, 28, config.smb2_server,     sizeof(config.smb2_server),    input_entry, (void *)CONNECT_SMB2 },

  { 0x010, 4, 17, -1,   "TZ",
    "ファイル共有サーバから取得する時刻のタイムゾーンを設定します",
    "ファイル共有サーバから取得する時刻のタイムゾーンを入力してください",
    "#e  (確定) #f   (前に戻る)",
    16, 16, config.tz,              sizeof(config.tz),             input_entry },
  { 0x000, 4, 18, -1,   "TADJUST",
    "ファイル共有サーバから取得した時刻を X68000 Z に設定する際のオフセット値を設定します",
    "ファイル共有サーバからの取得時刻設定時のオフセット値を選択してください (0=設定しない)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 8, (char *)&config.tadjust,  sizeof(config.tadjust),       input_numlist, &opt_tadjust },

  { 0x014, 4, 26, 10,  " リモート設定へ ",
    "リモート設定画面に切り替えます",
    NULL, NULL,
    -1, -1, NULL, 0, switch_menu },
  { 0x080, 82, 26, 9, "設定クリア",
    "保存されている設定内容をすべてクリアします",
    "保存されている設定内容をすべてクリアします  よろしいですか？",
    "#h (クリアする) #i #f  (前に戻る)",
    -1, -1, NULL, 0, flash_clear },
};

struct itemtbl itemtbl1[] = {
  { 0x014, 4, 4, -1,  "RMTBOOT",
    "リモートドライブ/HDSの優先順位を設定します",
    "リモートドライブ/HDSのどちらを優先するかを選択してください (0=HDS/1=リモートドライブ)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 76, (char *)&config.remoteboot, sizeof(config.remoteboot), input_numlist, &opt_bool },
  { 0x084, 4, 5, -1,  "RMTUNIT",
    "リモートドライブのユニット数を設定します (0-8)",
    "リモートドライブのユニット数を選択してください (0=リモートドライブは使用しない)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 76, (char *)&config.remoteunit, sizeof(config.remoteunit), input_numlist, &opt_rmtunit },
  { 0x024, 4, 6, -1,  "REMOTE0",
    "リモートドライブ 0 のファイル共有のパス名を設定します",
    "リモートドライブ 0 のファイル共有のパス名を選択してください (ディレクトリ内で \"./\" を選択)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 76, config.remote[0],       sizeof(config.remote[0]),      input_dirfile },
  { 0x124, 4, 7, -1,  "REMOTE1",
    "リモートドライブ 1 のファイル共有のパス名を設定します",
    "リモートドライブ 1 のファイル共有のパス名を選択してください (ディレクトリ内で \"./\" を選択)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 76, config.remote[1],       sizeof(config.remote[1]),      input_dirfile },
  { 0x224, 4, 8, -1,  "REMOTE2",
    "リモートドライブ 2 のファイル共有のパス名を設定します",
    "リモートドライブ 2 のファイル共有のパス名を選択してください (ディレクトリ内で \"./\" を選択)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 76, config.remote[2],       sizeof(config.remote[2]),      input_dirfile },
  { 0x324, 4, 9, -1,  "REMOTE3",
    "リモートドライブ 3 のファイル共有のパス名を設定します",
    "リモートドライブ 3 のファイル共有のパス名を選択してください (ディレクトリ内で \"./\" を選択)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 76, config.remote[3],       sizeof(config.remote[3]),      input_dirfile },
  { 0x424, 4, 10, -1,  "REMOTE4",
    "リモートドライブ 4 のファイル共有のパス名を設定します",
    "リモートドライブ 4 のファイル共有のパス名を選択してください (ディレクトリ内で \"./\" を選択)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 76, config.remote[4],       sizeof(config.remote[4]),      input_dirfile },
  { 0x524, 4, 11, -1,  "REMOTE5",
    "リモートドライブ 5 のファイル共有のパス名を設定します",
    "リモートドライブ 5 のファイル共有のパス名を選択してください (ディレクトリ内で \"./\" を選択)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 76, config.remote[5],       sizeof(config.remote[5]),      input_dirfile },
  { 0x624, 4, 12, -1,  "REMOTE6",
    "リモートドライブ 6 のファイル共有のパス名を設定します",
    "リモートドライブ 6 のファイル共有のパス名を選択してください (ディレクトリ内で \"./\" を選択)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 76, config.remote[6],       sizeof(config.remote[6]),      input_dirfile },
  { 0x724, 4, 13, -1,  "REMOTE7",
    "リモートドライブ 7 のファイル共有のパス名を設定します",
    "リモートドライブ 7 のファイル共有のパス名を選択してください (ディレクトリ内で \"./\" を選択)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 76, config.remote[7],       sizeof(config.remote[7]),      input_dirfile },

  { 0x014, 4, 16, -1,  "HDSSCSI",
    "リモートHDSの動作モードを設定します ※設定変更後は再起動が必要",
    "リモートHDSの動作モードを設定します (0=リモートHDSドライバ/1=純正SCSIドライバ)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 76, (char *)&config.hdsscsi, sizeof(config.hdsscsi), input_numlist, &opt_bool },
  { 0x084, 4, 17, -1,  "HDSUNIT",
    "リモートHDSのユニット数を設定します (0-4)",
    "リモートHDSのユニット数を選択してください (0=リモートHDSは使用しない)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 76, (char *)&config.hdsunit, sizeof(config.hdsunit), input_numlist, &opt_hdsunit },
  { 0x10004, 4, 18, -1,  "HDS0",
    "HDS ファイル 0 を設定します",
    "HDS ファイル 0 を選択してください (空文字列にすると HDS ファイルを割り当てません)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 76, config.hds[0],          sizeof(config.hds[0]),         input_dirfile, (void *)1},
  { 0x11004, 4, 19, -1,  "HDS1",
    "HDS ファイル 1 を設定します",
    "HDS ファイル 1 を選択してください (空文字列にすると HDS ファイルを割り当てません)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 76, config.hds[1],          sizeof(config.hds[1]),         input_dirfile, (void *)1},
  { 0x12004, 4, 20, -1,  "HDS2",
    "HDS ファイル 2 を設定します",
    "HDS ファイル 2 を選択してください (空文字列にすると HDS ファイルを割り当てません)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 76, config.hds[2],          sizeof(config.hds[2]),         input_dirfile, (void *)1},
  { 0x13004, 4, 21, -1,  "HDS3",
    "HDS ファイル 3 を設定します",
    "HDS ファイル 3 を選択してください (空文字列にすると HDS ファイルを割り当てません)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 76, config.hds[3],          sizeof(config.hds[3]),         input_dirfile, (void *)1},

  { 0x014, 4, 26, 17,  " 設定終了 ",
    "設定を登録して終了します",
    "設定を登録して終了します  よろしいですか？",
#ifndef BOOTSETTING
    "#h (登録して終了) #i (登録せずに終了) #f  (前に戻る)",
#else
    "#h (登録して終了) #i #f  (前に戻る)",
#endif
    -1, -1, NULL, 0, flash_config },
  { 0x080, 78, 26, 16,  " サーバ設定へ ",
    "サーバ設定画面に切り替えます",
    NULL, NULL,
    -1, -1, NULL, 0, switch_menu },
};

struct itemtbl *itemtbl[] = { itemtbl0, itemtbl1 };
int n_itemtbl[] = { countof(itemtbl0), countof(itemtbl1) };

//****************************************************************************
// Top view
//****************************************************************************

int topview(void)
{
  bool wstat = false;
  bool sstat = false;

  _iocs_b_color(3);
  _iocs_b_locate(0, 3);
  _iocs_b_clr_ed();

  switch (menumode) {
  case 0:
    switch (sysstatus) {
    default:
      /* fall through */

    case STAT_SMB2_CONNECTED:
      drawmsg(38, 10, 3, "接続済");
      sstat = true;
      drawframe3(2, 26, 20, 1, 2, -1);
      /* fall through */

    case STAT_SMB2_CONNECTING:
      if (!sstat) {
        drawmsg(38, 10, 2, "接続中");
        sstat = true;
      }
      /* fall through */

    case STAT_WIFI_CONNECTED:
      drawmsg(38, 6, 3, "接続済");
      wstat = true;

      drawmsg(4, 10, 3, "Windows ファイル共有設定");
      if (!sstat)
        drawmsg(38, 10, 2, "未接続");
      drawframe3(2, 11, 44, 4, 2, 10);
      /* fall through */

    case STAT_WIFI_CONNECTING:
      if (!wstat) {
        drawmsg(38, 6, 2, "接続中");
        wstat = true;
      }
      /* fall through */

    case STAT_WIFI_DISCONNECTED:
      drawmsg(4, 3, 3, "基本設定");
      drawframe3(2, 4, 44, 1, 2, 10);

      drawmsg(4, 6, 3, "WiFi 設定");
      if (!wstat)
        drawmsg(38, 6, 2, "未接続");
      drawframe3(2, 7, 44, 2, 2, 10);

      drawmsg(4, 16, 3, "時刻同期設定");
      drawframe3(2, 17, 44, 2, 2, 10);

      drawframe3(80, 26, 14, 1, 2, -1);
    }
    break;

  case 1:
    drawmsg(4, 3, 3, "リモートドライブ設定");
    drawframe3(2, 4, 92, config.remoteunit + 2, 2, 10);

    drawmsg(4, 15, 3, "HDS (SCSI ディスクイメージ) 設定");
    drawframe3(2, 16, 92, config.hdsunit + 2, 2, 10);

    drawframe3(2, 26, 14, 1, 2, -1);
    drawframe3(76, 26, 18, 1, 2, -1);
    break;
  }

  drawframe2(1, 27, 94, 4, 1, -1);

  for (int i = 0; i < n_itemtbl[menumode]; i++) {
    struct itemtbl *it = &itemtbl[menumode][i];
    if (isvisible(i)) {
      drawmsg(it->x, it->y, 3, it->msg);
      if (it->xd >= 0) {
        drawvalue(3, it, it->value, it->func == input_passwd);
      }
    }
  }
}

void show_help1(struct itemtbl *it)
{
  _iocs_b_putmes(3, 3, 28, 89, it->help1);
  drawhelp(3, 3, 29, 89, "#a #b (選択) #e  (確定)"
#ifndef BOOTSETTING
           " #f   (終了)"
#endif
          );
}

int escape_menu(void)
{
  _iocs_b_putmes(3, 3, 28, 89, "設定を登録せずに終了します");
  drawhelp(3, 3, 29, 89, "何かキーを押してください  #f   (前に戻る)");
  return ((keyinp(-1) & 0xff) == '\x1b');   // ESC
}

//****************************************************************************
// Command functions
//****************************************************************************

int switch_menu(struct itemtbl *it, void *v)
{
  menumode = 1 - menumode;
  return 2;
}

int flash_config(struct itemtbl *it, void *v)
{
  while (1) {
    /* キー入力処理 */
    int k = keyinp(-1);
    int c = k & 0xff;
    if (c == 'y' || c == 'Y') {         // Y
#ifndef XTEST
      {
        struct cmd_flashconfig cmd;
        struct res_flashconfig res;
        cmd.command = CMD_FLASHCONFIG;
        com_cmdres(&cmd, sizeof(cmd), &res, sizeof(res));
      }
      {
        struct cmd_setconfig cmd;
        struct res_setconfig res;
        cmd.command = CMD_SETCONFIG;
        cmd.mode = CONNECT_REMOUNT;
        cmd.data = config;
        com_cmdres(&cmd, sizeof(cmd), &res, sizeof(res));
      }
#endif
#ifndef BOOTSETTING
      return 3;
#else
      _iocs_b_putmes(3, 3, 28, 89, "設定を登録しました  X68000 Zの電源を一度切って再投入してください");
      _iocs_b_putmes(3, 3, 29, 89, "");
      while (1)
        ;
#endif
    } else if (c == 'n' || c == 'N') {  // N
#ifndef BOOTSETTING
      return 3;
#else
      return 0;
#endif
    } else if (c == '\x1b') {           // ESC
      return 0;
    }
  }
}

int flash_clear(struct itemtbl *it, void *v)
{
  while (1) {
    /* キー入力処理 */
    int k = keyinp(-1);
    int c = k & 0xff;
    if (c == 'y' || c == 'Y') {                         // Y
#ifndef XTEST
      {
        struct cmd_flashclear cmd;
        struct res_flashclear res;
        cmd.command = CMD_FLASHCLEAR;
        com_cmdres(&cmd, sizeof(cmd), &res, sizeof(res));
      }
      {
        struct cmd_getconfig cmd;
        struct res_getconfig res;
        cmd.command = CMD_GETCONFIG;
        com_cmdres(&cmd, sizeof(cmd), &res, sizeof(res));
        config = res.data;
      }
#endif
      return 2;
    } else if (c == 'n' || c == 'N' || c == '\x1b') {   // N or ESC
      return 0;
    }
  }
}

//****************************************************************************
// Main
//****************************************************************************

void terminate(int waitkey)
{
#ifndef BOOTSETTING
  if (waitkey) {
    _iocs_b_putmes(3, 3, 29, 89, "何かキーを押すと終了します");
    keyinp(-1);
  }
  _iocs_b_color(3);
  _iocs_os_curon();
  _dos_c_width(crtmode);
  exit(0);
#else
  while (1)
    ;
#endif
}

int main()
{
#ifndef BOOTSETTING
  crtmode = _dos_c_width(-1);
  _dos_c_width(0);
  _iocs_os_curof();
#endif

  char title[200];
  strcpy(title, "Ｒｅｍｏｔｅ　Ｄｒｉｖｅ　Ｓｅｒｖｉｃｅ　ｆｏｒ　Ｘ６８０００ Ｚ  Version " GIT_REPO_VERSION);
  title[88] = '\0';
  drawframe2(0, 0, strlen(title) + 6, 3, 1, -1);
  drawmsg(3, 1, 3, title);

#ifndef XTEST
  int8_t *zusb_channels = NULL;

  _dos_super(0);

  if (setjmp(jenv)) {
    zusb_disconnect_device();
    zusb_close();
    _iocs_b_putmes(3, 3, 28, 89, "リモートドライブ デバイスが切断されました");
    _iocs_b_putmes(3, 3, 29, 89, "");
    terminate(true);
  }

  // ZUSB デバイスをオープンする
  // 既にリモートドライブを使うドライバが存在する場合は、そのチャネルを使う
  if ((zusb_rmtdata = find_zusbrmt()) == NULL) {
    if (zusb_open() < 0) {
       drawframe2(1, 26, 94, 5, 1, -1);
      _iocs_b_putmes(3, 3, 27, 89, "X68000 Z Remote Drive Service が見つかりません");
      _iocs_b_putmes(3, 3, 28, 89, "リモートドライブ ファームウェアを書き込んだ Raspberry Pi Pico W を接続してください");
      terminate(true);
    }
  }

  {
    struct cmd_getinfo cmd;
    struct res_getinfo res;
    cmd.command = CMD_GETINFO;
    com_cmdres(&cmd, sizeof(cmd), &res, sizeof(res));
    if (res.version != PROTO_VERSION) {
       drawframe2(1, 26, 94, 5, 1, -1);
      _iocs_b_putmes(3, 3, 27, 89, "X68000 Z Remote Drive Service のファームウェアバージョンが合致しません");
      _iocs_b_putmes(3, 3, 28, 89, "同一バージョンのファームウェアを使用してください");
      terminate(true);
    }
  }

  {
    struct cmd_getconfig cmd;
    struct res_getconfig res;
    cmd.command = CMD_GETCONFIG;
    com_cmdres(&cmd, sizeof(cmd), &res, sizeof(res));
    config = res.data;
  }

  {
    struct cmd_getstatus cmd;
    struct res_getstatus res;
    cmd.command = CMD_GETSTATUS;
    com_cmdres(&cmd, sizeof(cmd), &res, sizeof(res));
    sysstatus = res.status;
  }
#endif

  /***********************************************************/

  menumode = (sysstatus >= STAT_SMB2_CONNECTED);

  int n = 0;
  int pren = -1;
  bool update = true;
  while (1) {
    if (update) {
      topview();
      while (!isvisible(n)) {
        n = (n + n_itemtbl[menumode] - 1) % n_itemtbl[menumode];
      }
      pren = -1;
      update = false;
    }
    struct itemtbl *it = &itemtbl[menumode][n];
    drawmsg(it->x, it->y, 10, it->msg);
    if (pren != n) {
      show_help1(it);
      pren = n;
    }

    /* キー入力を待ちながら2秒おきに設定状態をチェックする */
    int k;
    do {
#ifndef XTEST
      struct cmd_getstatus cmd;
      struct res_getstatus res;
      cmd.command = CMD_GETSTATUS;
      com_cmdres(&cmd, sizeof(cmd), &res, sizeof(res));
      if (sysstatus != res.status) {
        sysstatus = res.status;           // 状態が変化したので画面を更新する
        update = true;
        break;
      }
#endif
    } while ((k = keyinp(200)) < 0);
    if (update)
      continue;

    int c = k & 0xff;
    if (c == '\r') {                          /* CR */
      if (sysstatus == STAT_SMB2_CONNECTING || sysstatus == STAT_WIFI_CONNECTING) {
        continue;                             // 接続中なら何もしない
      }
      if (it->func == input_wifiap && sysstatus == STAT_SMB2_CONNECTED) {
        continue;
      }
      if (it->func == NULL) {
        continue;
      }

      drawmsg(it->x, it->y, 7, it->msg);
      if (it->help2) _iocs_b_putmes(3, 3, 28, 89, it->help2);
      if (it->help3) drawhelp(3, 3, 29, 89, it->help3);

      int res = it->func(it, it->opt);
      show_help1(it);

      if (res == 1) {
        update = isupdconf(n);
        if (issetconf(n)) {
#ifndef XTEST
          struct cmd_setconfig cmd;
          struct res_setconfig res;
          cmd.command = CMD_SETCONFIG;
          cmd.mode = (int)it->opt;
          cmd.data = config;
          com_cmdres(&cmd, sizeof(cmd), &res, sizeof(res));
#endif
        }
        n++;
        if (n >= n_itemtbl[menumode] || !isvisible(n)) {
          n--;
        }
      } else if (res == 2) {
        update = true;
        n = 0;
      } else if (res == 3) {
        terminate(false);
      }

#ifndef BOOTSETTING
    } else if (c == '\x1b') {                 /* ESC */
      if (!escape_menu())
        terminate(false);
      show_help1(it);
#endif
    }

    drawmsg(it->x, it->y, 3, it->msg);
    if (c == '\x0e' || k == 0x3e00) {  /* CTRL+N or ↓ */
      do {
        n = (n + 1) % n_itemtbl[menumode];
      } while (!isvisible(n));
    } else if (c == '\x10' || k == 0x3c00) {  /* CTRL+P or ↑ */
      do {
        n = (n + n_itemtbl[menumode] - 1) % n_itemtbl[menumode];
      } while (!isvisible(n));
    } else if (c == '\x02' || k == 0x3b00) {  /* CTRL+B or ← */
      n = it->xn >= 0 ? it->xn : n;
    } else if (c == '\x06' || k == 0x3d00) {  /* CTRL+F or → */
      n = it->xn >= 0 ? it->xn : n;
    } else if (c == '\t') {                   /* TAB */
      do {
        n = (n + 1) % n_itemtbl[menumode];
      } while (!isvisible(n) || !istabstop(n));
#ifdef XTEST
    } else if (c == '+') {
      sysstatus = (sysstatus + 1) > STAT_CONFIGURED ? STAT_CONFIGURED : sysstatus + 1;
      update = true;
    } else if (c == '-') {
      sysstatus = (sysstatus - 1) < STAT_WIFI_DISCONNECTED ? STAT_WIFI_DISCONNECTED : sysstatus - 1;
      update = true;
#endif
    }
  }
}
