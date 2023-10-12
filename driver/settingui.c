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
#include <x68k/iocs.h>

#include "config.h"
#include "vd_command.h"
#include "settinguisub.h"

//****************************************************************************
// Global variables
//****************************************************************************

int sysstatus = STAT_WIFI_DISCONNECTED;
int remoteunit = 0;
struct config_data config
#ifdef XTEST
= {
  .tz = "JST-9",
  .tadjust= "2",
}
#endif
;

//****************************************************************************
// Definition
//****************************************************************************

#define isvisible(n)  (((itemtbl[n].stat) & 0xf) <= sysstatus && \
                       !((itemtbl[n].stat & 0x20) && \
                         (remoteunit <= ((itemtbl[n].stat & 0xf00) >> 8))))
#define istabstop(n)  ((itemtbl[n].stat) & 0x10)
#define issetconf(n)  ((itemtbl[n].stat) & 0x40)
#define isupdconf(n)  ((itemtbl[n].stat) & 0x80)

//****************************************************************************
// Menu data
//****************************************************************************

static struct numlist_opt opt_rmtboot = { 0, 1 };
static struct numlist_opt opt_rmtunit = { 0, 4 };
static struct numlist_opt opt_tadjust = { 0, 4 };

struct itemtbl itemtbl[] = {
  { 0x010, 4, 4, 17,   "SSID",
    "WiFi 接続先の SSID を設定します",
    "WiFi 接続先の SSID を選択してください",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 28, config.wifi_ssid,       sizeof(config.wifi_ssid),      input_wifiap },
  { 0x040, 4, 5, 18,   "PASSWORD",
    "WiFi 接続先のパスワードを設定します",
    "WiFi 接続先のパスワードを入力してください",
    "#e  (確定) #f   (前に戻る) #g   (パスワードを表示)",
    16, 16, config.wifi_passwd,     sizeof(config.wifi_passwd),    input_passwd },

  { 0x012, 4, 8, -1,   "USERNAME",
    "Windows ファイル共有のユーザ名を設定します",
    "Windows ファイル共有のユーザ名を入力してください",
    "#e  (確定) #f   (前に戻る)",
    16, 16, config.smb2_user,       sizeof(config.smb2_user),      input_entry },
  { 0x002, 4, 9, -1,   "PASSWORD",
    "Windows ファイル共有のパスワードを設定します",
    "Windows ファイル共有のパスワードを入力してください",
    "#e  (確定) #f   (前に戻る) #g   (パスワードを表示)",
    16, 16, config.smb2_passwd,     sizeof(config.smb2_passwd),    input_passwd },
  { 0x002, 4, 10, -1,  "WORKGROUP",
    "Windows ファイル共有のワークグループを設定します",
    "Windows ファイル共有のワークグループを入力してください",
    "#e  (確定) #f   (前に戻る)",
    16, 16, config.smb2_workgroup,  sizeof(config.smb2_workgroup), input_entry },
  { 0x042, 4, 11, -1,  "SERVER",
    "Windows ファイル共有のサーバ名を設定します",
    "Windows ファイル共有のサーバ名または IP アドレスを入力してください",
    "#e  (確定) #f   (前に戻る)",
    16, 28, config.smb2_server,     sizeof(config.smb2_server),    input_entry },

  { 0x014, 4, 14, -1,  "HDS0",
    "HDS ファイル 0 を設定します",
    "HDS ファイル 0 を選択してください (空文字列にすると HDS ファイルを割り当てません)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 76, config.hds[0],          sizeof(config.hds[0]),         input_dirfile, (void *)1},
  { 0x004, 4, 15, -1,  "HDS1",
    "HDS ファイル 1 を設定します",
    "HDS ファイル 1 を選択してください (空文字列にすると HDS ファイルを割り当てません)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 76, config.hds[1],          sizeof(config.hds[1]),         input_dirfile, (void *)1},
  { 0x004, 4, 16, -1,  "HDS2",
    "HDS ファイル 2 を設定します",
    "HDS ファイル 2 を選択してください (空文字列にすると HDS ファイルを割り当てません)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 76, config.hds[2],          sizeof(config.hds[2]),         input_dirfile, (void *)1},
  { 0x004, 4, 17, -1,  "HDS3",
    "HDS ファイル 3 を設定します",
    "HDS ファイル 3 を選択してください (空文字列にすると HDS ファイルを割り当てません)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 76, config.hds[3],          sizeof(config.hds[3]),         input_dirfile, (void *)1},

  { 0x014, 4, 20, -1,  "RMTBOOT",
    "リモートドライブからの起動を行うかどうかを設定します",
    "リモートドライブからの起動を行うかどうかを選択してください (0=行わない/1=行う)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 76, config.remoteboot,      sizeof(config.remoteboot),     input_numlist, &opt_rmtboot },
  { 0x084, 4, 21, -1,  "RMTUNIT",
    "リモートドライブの個数を設定します (0-4)",
    "リモートドライブの個数を選択してください (0=リモートドライブは使用しない)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 76, config.remoteunit,      sizeof(config.remoteunit),     input_numlist, &opt_rmtunit },
  { 0x024, 4, 22, -1,  "REMOTE0",
    "リモートドライブ 0 のファイル共有のパス名を設定します",
    "リモートドライブ 0 のファイル共有のパス名を選択してください (ディレクトリ内で \"./\" を選択)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 76, config.remote[0],       sizeof(config.remote[0]),      input_dirfile },
  { 0x124, 4, 23, -1,  "REMOTE1",
    "リモートドライブ 1 のファイル共有のパス名を設定します",
    "リモートドライブ 1 のファイル共有のパス名を選択してください (ディレクトリ内で \"./\" を選択)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 76, config.remote[1],       sizeof(config.remote[1]),      input_dirfile },
  { 0x224, 4, 24, -1,  "REMOTE2",
    "リモートドライブ 2 のファイル共有のパス名を設定します",
    "リモートドライブ 2 のファイル共有のパス名を選択してください (ディレクトリ内で \"./\" を選択)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 76, config.remote[2],       sizeof(config.remote[2]),      input_dirfile },
  { 0x324, 4, 25, -1,  "REMOTE3",
    "リモートドライブ 3 のファイル共有のパス名を設定します",
    "リモートドライブ 3 のファイル共有のパス名を選択してください (ディレクトリ内で \"./\" を選択)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 76, config.remote[3],       sizeof(config.remote[3]),      input_dirfile },

  { 0x014, 4, 27, 19,  " 設定終了 ",
    "設定を反映して終了します",
    NULL, NULL,
    -1, -1, NULL, 0, NULL },

  { 0x010, 52, 4, 0,   "TZ",
    "Windows から取得した時刻のタイムゾーンを設定します",
    "Windows から取得した時刻のタイムゾーンを入力してください",
    "#e  (確定) #f   (前に戻る)",
    64, 16, config.tz,              sizeof(config.tz),             input_entry },
  { 0x000, 52, 5, 1,   "TADJUST",
    "Windows から取得した時刻を X68000Z に設定する際のオフセット値を設定します",
    "Windows から取得した時刻を設定する際のオフセット値を選択してください (0=設定しない)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    64, 8, config.tadjust,          sizeof(config.tadjust),        input_numlist, &opt_tadjust },

  { 0x000, 82, 27, 16, "設定クリア",
    "保存されている設定内容をクリアします",
    NULL, NULL,
    -1, -1, NULL, 0, NULL },
};

#define N_ITEMTBL (countof(itemtbl))

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

  switch (sysstatus) {
  default:
    /* fall through */

  case STAT_SMB2_CONNECTED:
    drawmsg(38, 7, 3, "接続済");
    sstat = true;

    drawmsg(4, 13, 3, "HDS (SCSI ディスクイメージ) 設定");
    drawframe3(2, 14, 92, 4, 2, 10);

    drawmsg(4, 19, 3, "リモートドライブ設定");
    drawframe3(2, 20, 92, remoteunit + 2, 2, 10);

    drawframe3(2, 27, 14, 1, 2, -1);
    /* fall through */

  case STAT_SMB2_CONNECTING:
    if (!sstat) {
      drawmsg(38, 7, 2, "接続中");
      sstat = true;
    }
    /* fall through */

  case STAT_WIFI_CONNECTED:
    drawmsg(38, 3, 3, "接続済");
    wstat = true;

    drawmsg(4, 7, 3, "Windows ファイル共有設定");
    if (!sstat)
      drawmsg(38, 7, 2, "未接続");
    drawframe3(2, 8, 44, 4, 2, 10);
    /* fall through */

  case STAT_WIFI_CONNECTING:
    if (!wstat) {
      drawmsg(38, 3, 2, "接続中");
      wstat = true;
    }
    /* fall through */

  case STAT_WIFI_DISCONNECTED:
    drawmsg(4, 3, 3, "WiFi 設定");
    if (!wstat)
      drawmsg(38, 3, 2, "未接続");
    drawframe3(2, 4, 44, 2, 2, 10);

    drawmsg(52, 3, 3, "その他の設定");
    drawframe3(50, 4, 44, 2, 2, 10);

    drawframe3(80, 27, 14, 1, 2, -1);
  }
  drawframe2(1, 28, 94, 4, 1, -1);

  for (int i = 0; i < N_ITEMTBL; i++) {
    struct itemtbl *it = &itemtbl[i];
    if (isvisible(i)) {
      drawmsg(it->x, it->y, 3, it->msg);
      if (it->xd >= 0) {
        drawvalue(3, it, it->value, it->func == input_passwd);
      }
    }
  }
}

//****************************************************************************
// Main
//****************************************************************************

int main()
{
#ifdef XTEST
  printf("\x1b[2J\x1b[>1h");
  fflush(stdout);
#endif
  _iocs_os_curof();

#ifndef XTEST
  com_init();

  {
    struct cmd_getconfig cmd;
    struct res_getconfig res;
    cmd.command = CMD_GETCONFIG;
    com_cmdres(&cmd, sizeof(cmd), &res, sizeof(res));
    config = res.data;
  }
  remoteunit = atoi(config.remoteunit);

  {
    struct cmd_getstatus cmd;
    struct res_getstatus res;
    cmd.command = CMD_GETSTATUS;
    com_cmdres(&cmd, sizeof(cmd), &res, sizeof(res));
    sysstatus = res.status;
  }
#endif

  _iocs_b_clr_st();
  char title[200];
  strcpy(title, "Ｒｅｍｏｔｅ　Ｄｒｉｖｅ　Ｓｅｒｖｉｃｅ　ｆｏｒ　Ｘ６８０００ Ｚ  Version " GIT_REPO_VERSION);
  title[88] = '\0';
  drawframe2(0, 0, strlen(title) + 6, 3, 1, -1);
  drawmsg(3, 1, 3, title);

  /***********************************************************/

  int n = 0;
  bool update = true;
  while (1) {
    if (update) {
      topview();
      while (!isvisible(n)) {
        n = (n + N_ITEMTBL - 1) % N_ITEMTBL;
      }
      update = false;
    }
    struct itemtbl *it = &itemtbl[n];
    drawmsg(it->x, it->y, 10, it->msg);
    _iocs_b_putmes(3, 3, 29, 89, it->help1);
    drawhelp(3, 3, 30, 89, "#a #b (選択) #e  (確定)");

    /* キー入力を待ちながら2秒おきに設定状態をチェックする */
    int k;
    do {
#ifndef XTEST
      struct cmd_getstatus cmd;
      struct res_getstatus res;
      cmd.command = CMD_GETSTATUS;
      com_cmdres(&cmd, sizeof(cmd), &res, sizeof(res));
      if (sysstatus != res.status) {
        sysstatus = res.status;
        update = true;
        break;
      }
#endif
    } while ((k = keyinp(200)) < 0);
    if (update)
      continue;

    int c = k & 0xff;
    if (c == '\r') {                          /* CR */
      drawmsg(it->x, it->y, 7, it->msg);
      if (it->xd < 0) {
        break;
      }
      if (it->func) {
        if (it->help2) _iocs_b_putmes(3, 3, 29, 89, it->help2);
        if (it->help3) drawhelp(3, 3, 30, 89, it->help3);
        int res = it->func(it, it->opt);
        _iocs_b_putmes(3, 3, 29, 89, it->help1);
        drawhelp(3, 3, 30, 89, "#a #b (選択) #e  (確定)");
        remoteunit = atoi(config.remoteunit);
        if (res == 1) {
          update = isupdconf(n);
          if (issetconf(n)) {
#ifndef XTEST
            struct cmd_setconfig cmd;
            struct res_setconfig res;
            cmd.command = CMD_SETCONFIG;
            cmd.data = config;
            com_cmdres(&cmd, sizeof(cmd), &res, sizeof(res));
#endif
          }
          n++;
          if (n >= N_ITEMTBL || !isvisible(n)) {
            n--;
          }
        }
      }
#ifdef XTEST
    } else if (c == '\x1b') {                 /* ESC */
      break;
#endif
    }

    drawmsg(it->x, it->y, 3, it->msg);
    if (c == '\x0e' || k == 0x3e00) {  /* CTRL+N or ↓ */
      do {
        n = (n + 1) % N_ITEMTBL;
      } while (!isvisible(n));
    } else if (c == '\x10' || k == 0x3c00) {  /* CTRL+P or ↑ */
      do {
        n = (n + N_ITEMTBL - 1) % N_ITEMTBL;
      } while (!isvisible(n));
    } else if (c == '\x02' || k == 0x3b00) {  /* CTRL+B or ← */
      n = it->xn >= 0 ? it->xn : n;
    } else if (c == '\x06' || k == 0x3d00) {  /* CTRL+F or → */
      n = it->xn >= 0 ? it->xn : n;
    } else if (c == '\t') {                   /* TAB */
      do {
        n = (n + 1) % N_ITEMTBL;
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

#ifndef XTEST
  while (1)
    ;
#endif
#ifdef XTEST
  printf("\x1b[0m\x1b[2J\x1b[>1l");
  _iocs_os_curon();
#endif

  return 0;
}
