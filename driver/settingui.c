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

jmp_buf jenv;                       //タイムアウト時のジャンプ先

int sysstatus = STAT_WIFI_DISCONNECTED;
struct config_data config
#ifdef XTEST
= {
  .tz = "JST-9",
  .tadjust= 2,
}
#endif
;
int menumode;

#ifndef BOOTSETTING
int crtmode;
int needreboot;
#endif

//****************************************************************************
// Definition
//****************************************************************************

#define istabstop(n)  (itemtbl[menumode][n].stat & 0x10)
#define issetconf(n)  (itemtbl[menumode][n].stat & 0x40)
#define isupdconf(n)  (itemtbl[menumode][n].stat & 0x80)
#define isremote(n)   (itemtbl[menumode][n].stat & 0x20)
#define ishds(n)      (itemtbl[menumode][n].stat & 0x10000)
#define unitremote(n) ((itemtbl[menumode][n].stat & 0xf00) >> 8)
#define unithds(n)    ((itemtbl[menumode][n].stat & 0xf000) >> 12)
#define isneedreboot(n) (itemtbl[menumode][n].stat & 0x80000)

#define isvisible(n)  (((itemtbl[menumode][n].stat) & 0xf) <= sysstatus && \
                       !(isremote(n) && (config.remoteunit <= unitremote(n))) && \
                       !(ishds(n) && (config.hdsunit <= unithds(n))))

int switch_menu(struct itemtbl *it);
int flash_config(struct itemtbl *it);
int flash_clear(struct itemtbl *it);

#ifdef BOOTSETTING
int _dos_bus_err(void *src, void *dst, int size)
{
  memcpy(dst, src, size);
  return 0;
}
#endif

//****************************************************************************
// Menu data
//****************************************************************************

static struct numlist_opt opt_bool = { 0, 1 };
static struct numlist_opt opt_rmtunit = { 0, 8 };
static struct numlist_opt opt_hdsunit = { 0, 4 };
static struct numlist_opt opt_tadjust = { 0, 4 };

static const char *opt_bootmode_labels[] = {
  "リモートドライブから起動",
  "リモートHDSから起動",
  "USBメモリから起動",
};
static struct labellist_opt opt_bootmode = { countof(opt_bootmode_labels), opt_bootmode_labels };

struct itemtbl itemtbl0[] = {
  { 0x80010, 4, 4, -1,   "BOOTMODE",
    "どのドライブから起動するかを設定します",
    "どのドライブから起動するかを選択してください",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 28, (char *)&config.bootmode,  sizeof(config.bootmode),     input_labellist, &opt_bootmode },

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
  { 0x80094, 4, 4, -1,  "RMTUNIT",
    "リモートドライブのユニット数を設定します (0-8)",
    "リモートドライブのユニット数を選択してください (0=リモートドライブは使用しない)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 76, (char *)&config.remoteunit, sizeof(config.remoteunit), input_numlist, &opt_rmtunit },
  { 0x024, 4, 5, -1,  "REMOTE0",
    "リモートドライブ 0 のファイル共有のパス名を設定します",
    "リモートドライブ 0 のファイル共有のパス名を選択してください (ディレクトリ内で \"./\" を選択)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 76, config.remote[0],       sizeof(config.remote[0]),      input_dirfile },
  { 0x124, 4, 6, -1,  "REMOTE1",
    "リモートドライブ 1 のファイル共有のパス名を設定します",
    "リモートドライブ 1 のファイル共有のパス名を選択してください (ディレクトリ内で \"./\" を選択)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 76, config.remote[1],       sizeof(config.remote[1]),      input_dirfile },
  { 0x224, 4, 7, -1,  "REMOTE2",
    "リモートドライブ 2 のファイル共有のパス名を設定します",
    "リモートドライブ 2 のファイル共有のパス名を選択してください (ディレクトリ内で \"./\" を選択)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 76, config.remote[2],       sizeof(config.remote[2]),      input_dirfile },
  { 0x324, 4, 8, -1,  "REMOTE3",
    "リモートドライブ 3 のファイル共有のパス名を設定します",
    "リモートドライブ 3 のファイル共有のパス名を選択してください (ディレクトリ内で \"./\" を選択)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 76, config.remote[3],       sizeof(config.remote[3]),      input_dirfile },
  { 0x424, 4, 9, -1,  "REMOTE4",
    "リモートドライブ 4 のファイル共有のパス名を設定します",
    "リモートドライブ 4 のファイル共有のパス名を選択してください (ディレクトリ内で \"./\" を選択)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 76, config.remote[4],       sizeof(config.remote[4]),      input_dirfile },
  { 0x524, 4, 10, -1,  "REMOTE5",
    "リモートドライブ 5 のファイル共有のパス名を設定します",
    "リモートドライブ 5 のファイル共有のパス名を選択してください (ディレクトリ内で \"./\" を選択)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 76, config.remote[5],       sizeof(config.remote[5]),      input_dirfile },
  { 0x624, 4, 11, -1,  "REMOTE6",
    "リモートドライブ 6 のファイル共有のパス名を設定します",
    "リモートドライブ 6 のファイル共有のパス名を選択してください (ディレクトリ内で \"./\" を選択)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 76, config.remote[6],       sizeof(config.remote[6]),      input_dirfile },
  { 0x724, 4, 12, -1,  "REMOTE7",
    "リモートドライブ 7 のファイル共有のパス名を設定します",
    "リモートドライブ 7 のファイル共有のパス名を選択してください (ディレクトリ内で \"./\" を選択)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 76, config.remote[7],       sizeof(config.remote[7]),      input_dirfile },

  { 0x80094, 4, 15, -1,  "HDSUNIT",
    "リモートHDSのユニット数を設定します (0-4)",
    "リモートHDSのユニット数を選択してください (0=リモートHDSは使用しない)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 76, (char *)&config.hdsunit, sizeof(config.hdsunit), input_numlist, &opt_hdsunit },
  { 0x10004, 4, 16, -1,  "HDS0",
    "HDS ファイル 0 を設定します",
    "HDS ファイル 0 を選択してください (空文字列にすると HDS ファイルを割り当てません)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 76, config.hds[0],          sizeof(config.hds[0]),         input_dirfile, (void *)1},
  { 0x11004, 4, 17, -1,  "HDS1",
    "HDS ファイル 1 を設定します",
    "HDS ファイル 1 を選択してください (空文字列にすると HDS ファイルを割り当てません)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 76, config.hds[1],          sizeof(config.hds[1]),         input_dirfile, (void *)1},
  { 0x12004, 4, 18, -1,  "HDS2",
    "HDS ファイル 2 を設定します",
    "HDS ファイル 2 を選択してください (空文字列にすると HDS ファイルを割り当てません)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 76, config.hds[2],          sizeof(config.hds[2]),         input_dirfile, (void *)1},
  { 0x13004, 4, 19, -1,  "HDS3",
    "HDS ファイル 3 を設定します",
    "HDS ファイル 3 を選択してください (空文字列にすると HDS ファイルを割り当てません)",
    "#a #b (選択) #e  (確定) #f   (前に戻る)",
    16, 76, config.hds[3],          sizeof(config.hds[3]),         input_dirfile, (void *)1},

  { 0x014, 4, 26, 15,  " 設定終了 ",
    "設定を登録して終了します",
    "設定を登録して終了します  よろしいですか？",
#ifndef BOOTSETTING
    "#h (登録して終了) #i (登録せずに終了) #f  (前に戻る)",
#else
    "#h (登録して終了) #i #f  (前に戻る)",
#endif
    -1, -1, NULL, 0, flash_config },
  { 0x080, 78, 26, 14,  " サーバ設定へ ",
    "サーバ設定画面に切り替えます",
    NULL, NULL,
    -1, -1, NULL, 0, switch_menu },
};

struct itemtbl itemtbl2[countof(itemtbl1)];

struct itemtbl * const itemtbl[] = { itemtbl0, itemtbl1, itemtbl2 };
const int n_itemtbl[] = { countof(itemtbl0), countof(itemtbl1), countof(itemtbl2) };

#ifndef BOOTSETTING
static char unit2drive(int unit, int ishds)
{
  struct dos_dpbptr dpb;

  for (int drive = 1; drive <= 26; drive++) {
    if (_dos_getdpb(drive, &dpb) < 0) {
      continue;
    }
    char *p = (char *)dpb.driver + 14;
    if (memcmp(p, ishds ? "\x01ZUSBHDS" : "\x01ZUSBRMT", 8) != 0) {
      continue;
    }
    if (dpb.unit == unit) {
      return drive + 'A' - 1;
    }
  }
  return '?';
}
#endif

const char *getlabel(const struct itemtbl *it, int n)
{
  static char label[16];
  if (isremote(n)) {
#ifndef BOOTSETTING
    sprintf(label, "#%u (%c:)", unitremote(n), unit2drive(unitremote(n), false));
#else 
    sprintf(label, "REMOTE%u", unitremote(n));
#endif
    return label;
  } else if (ishds(n)) {
#ifndef BOOTSETTING
    sprintf(label, "#%u (%c:)", unithds(n), unit2drive(unithds(n), true));
#else 
    sprintf(label, "HDS%u", unithds(n));
#endif
    return label;
  } else {
    return it->msg;
  }
}

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
    drawframe3(2, 4, 92, config.remoteunit + 1, 2, 10);

    drawmsg(4, 14, 3, "HDS (SCSI ディスクイメージ) 設定");
    drawframe3(2, 15, 92, config.hdsunit + 1, 2, 10);

    drawframe3(2, 26, 14, 1, 2, -1);
    drawframe3(76, 26, 18, 1, 2, -1);
    break;

  case 2:
    drawmsg(4, 3, 3, "HDS (SCSI ディスクイメージ) 設定");
    drawframe3(2, 4, 92, config.hdsunit + 1, 2, 10);

    drawmsg(4, 10, 3, "リモートドライブ設定");
    drawframe3(2, 11, 92, config.remoteunit + 1, 2, 10);

    drawframe3(2, 26, 14, 1, 2, -1);
    drawframe3(76, 26, 18, 1, 2, -1);
    break;
  }

  drawframe2(1, 27, 94, 4, 1, -1);

  for (int i = 0; i < n_itemtbl[menumode]; i++) {
    struct itemtbl *it = &itemtbl[menumode][i];
    if (isvisible(i)) {
      drawmsg(it->x, it->y, 3, getlabel(it, i));
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

int switch_menu(struct itemtbl *it)
{
  menumode = (menumode == 0) ? ((config.bootmode != 1) ? 1 : 2) : 0;
  return 2;
}

int flash_config(struct itemtbl *it)
{
  while (1) {
    /* キー入力処理 */
    int k = keyinp(-1);
    int c = k & 0xff;
    if (c == 'y' || c == 'Y') {         // Y
#ifndef XTEST
      {
        struct cmd_setconfig cmd;
        struct res_setconfig res;
        cmd.command = CMD_SETCONFIG;
        cmd.mode = CONNECT_REMOUNT;
        cmd.data = config;
        com_cmdres(&cmd, sizeof(cmd), &res, sizeof(res));
      }
      {
        struct cmd_flashconfig cmd;
        struct res_flashconfig res;
        cmd.command = CMD_FLASHCONFIG;
        com_cmdres(&cmd, sizeof(cmd), &res, sizeof(res));
      }
#ifndef BOOTSETTING
      // ドライブの設定変更を検出させる
      if (com_rmtdata) {
        _dos_fflush();
        com_rmtdata->hds_changed = 0xff;
        for (int i = 0; i < N_HDS; i++) {
          if (strlen(config.hds[i])) {
            com_rmtdata->hds_ready |= (1 << i);
          }
        }
      }
#endif
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
      needreboot = false;
      return 3;
#else
      return 0;
#endif
    } else if (c == '\x1b') {           // ESC
      return 0;
    }
  }
}

int flash_clear(struct itemtbl *it)
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
  com_disconnect();
#ifndef BOOTSETTING
  if (waitkey) {
    drawframe2(1, 26, 94, 5, 1, -1);
    _iocs_b_putmes(3, 3, 29, 89, "何かキーを押すと終了します");
    keyinp(-1);
  }
  _iocs_b_color(3);
  _iocs_os_curon();
  _dos_c_width(crtmode);
  if (needreboot) {
    printf("※設定変更を反映させるためには再起動が必要です\n");
  }
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

  _dos_super(0);
#endif

  {
    // itemtbl1[] のリモートドライブとリモートHDSを交換したitemtbl2[] を作る

    for (int i = 0; i < 5; i++) {
      itemtbl2[i] = itemtbl1[9 + i];
      itemtbl2[i].y = 4 + i;
    }

    for (int i = 0; i < 9; i++) {
      itemtbl2[i + 5] = itemtbl1[i];
      itemtbl2[i + 5].y = 11 + i;
    }

    for (int i = 14; i < 16; i++) {
      itemtbl2[i] = itemtbl1[i];
    }
  }

  char title[200];
  strcpy(title, "Ｒｅｍｏｔｅ　Ｄｒｉｖｅ　Ｓｅｒｖｉｃｅ　ｆｏｒ　Ｘ６８０００ Ｚ  Version " GIT_REPO_VERSION);
  title[88] = '\0';
  drawframe2(0, 0, strlen(title) + 6, 3, 1, -1);
  drawmsg(3, 1, 3, title);

#ifndef XTEST
  int8_t *zusb_channels = NULL;

  if (setjmp(jenv)) {
    _iocs_b_putmes(3, 3, 27, 89, "X68000 Z Remote Drive Service が見つかりません");
    _iocs_b_putmes(3, 3, 28, 89, "リモートドライブ ファームウェアを書き込んだ Raspberry Pi Pico W を接続してください");
    terminate(true);
  }

  if (com_connect(false) < 0) {
    _iocs_b_putmes(3, 3, 27, 89, "ZUSB デバイスが見つかりません");
    _iocs_b_putmes(3, 3, 28, 89, "X68000 Z 本体のファームウェアを ZUSB 対応に更新してください");
    terminate(true);
  }

  {
    struct cmd_getinfo cmd;
    struct res_getinfo res;
    cmd.command = CMD_GETINFO;
    com_cmdres(&cmd, sizeof(cmd), &res, sizeof(res));
    if (res.version != PROTO_VERSION) {
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

  menumode = (sysstatus >= STAT_SMB2_CONNECTED) ? ((config.bootmode != 1) ? 1 : 2) : 0;

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
    drawmsg(it->x, it->y, 10, getlabel(it, n));
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

      drawmsg(it->x, it->y, 7, getlabel(it, n));
      if (it->help2) _iocs_b_putmes(3, 3, 28, 89, it->help2);
      if (it->help3) drawhelp(3, 3, 29, 89, it->help3);

      int res = it->func(it);
      show_help1(it);
      drawmsg(it->x, it->y, 3, getlabel(it, n));

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
#ifndef BOOTSETTING
        if (isneedreboot(n)) {
          needreboot = true;
        }
#endif
        n++;
        if (n >= n_itemtbl[menumode] || !isvisible(n)) {
          n--;
        }
        continue;
      } else if (res == 2) {
        update = true;
        n = 0;
        continue;
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

    drawmsg(it->x, it->y, 3, getlabel(it, n));
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
