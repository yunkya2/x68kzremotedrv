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

#include "vd_command.h"
#include "settinguisub.h"
#include "settinguipat.h"

//****************************************************************************
// Global variables
//****************************************************************************

//****************************************************************************
// Drawing
//****************************************************************************

void drawframe(int x, int y, int w, int h, int c, int h2)
{
  char line[100];

  strcpy(line, "┏");
  for (int i = 0; i < w - 4; i += 2) {
    if (i != h2)
      strcat(line, "━");
    else
      strcat(line, "┳");
  }
  strcat(line, "┓");
  _iocs_b_color(c);
  _iocs_b_locate(x, y);
  _iocs_b_print(line);

  strcpy(line, "┃");
  for (int i = 0; i < w - 4; i += 2) {
    if (i != h2)
      strcat(line, "  ");
    else
      strcat(line, "┃");
  }
  strcat(line, "┃");
  for (int j = y + 1; j < y + h - 1; j++) {
    _iocs_b_locate(x, j);
    _iocs_b_print(line);
  }

  strcpy(line, "┗");
  for (int i = 0; i < w - 4; i += 2) {
    if (i != h2)
      strcat(line, "━");
    else
      strcat(line, "┻");
  }
  strcat(line, "┛");
  _iocs_b_locate(x, y + h - 1);
  _iocs_b_print(line);
  _iocs_b_color(3);
}

void drawframe2(int x, int y, int w, int h, int c, int h2)
{
  struct iocs_tboxptr box;
  box.vram_page = c == 1 ? 0 : 1;
  box.line_style = 0xffff;

  box.x = x * 8 + 7;
  box.y = y * 16 + 7;
  box.x1 = w * 8 - 16 + 2;
  box.y1 = h * 16 - 16 + 2;
  _iocs_txbox(&box);
  box.x = x * 8 + 8;
  box.y = y * 16 + 8;
  box.x1 = w * 8 - 16;
  box.y1 = h * 16 - 16;
  _iocs_txbox(&box);
  if (h2 >= 0) {
    box.x = (x + 2 + h2) * 8 + 8;
    box.y = y * 16 + 8;
    box.x1 = 2;
    box.y1 = h * 16 - 16 + 2;
  _iocs_txbox(&box);
  }
}

void drawframe3(int x, int y, int w, int h, int c, int h2)
{
  struct iocs_tboxptr box;
  box.vram_page = c == 1 ? 0 : 1;
  box.line_style = 0xffff;

  box.x = x * 8 + 7;
  box.y = y * 16 - 2;
  box.x1 = w * 8 - 16 + 2;
  box.y1 = h * 16 + 4;
  _iocs_txbox(&box);
  box.x = x * 8 + 8;
  box.y = y * 16 - 1;
  box.x1 = w * 8 - 16;
  box.y1 = h * 16 + 2;
  _iocs_txbox(&box);
  if (h2 >= 0) {
    box.x = (x + 2 + h2) * 8 + 8;
    box.y = y * 16 - 2;
    box.x1 = 2;
    box.y1 = h * 16 + 4;
  _iocs_txbox(&box);
  }
}

void drawhline(int x, int y, int w, int c)
{
  char line[100];

  _iocs_b_color(c);
  _iocs_b_locate(x, y);

  strcpy(line, "");
  for (int i = 0; i < w; i += 2) {
    strcat(line, "─");
  }
  _iocs_b_print(line);
}

void drawmsg(int x, int y, int c, const char *msg)
{
  _iocs_b_color(c);
  _iocs_b_locate(x, y);
  _iocs_b_print(msg);
}

void drawvalue(int c, struct itemtbl *it, const char *s, int mask)
{
  if (mask) {
    _iocs_b_locate(it->xd, it->y);
    _iocs_b_color(c);
    int len = min(strlen(s), it->wd - 1);
    int i;
    for (i = 0; i < len; i++)
      _iocs_b_putc('*');
    for (; i < it->wd - 1; i++)
    _iocs_b_putc(' ');
  } else {
    if (it->func != input_numlist) {
      _iocs_b_putmes(c, it->xd, it->y, it->wd - 1, s);
    } else {
      char str[4];
      sprintf(str, "%u", *(uint8_t *)s);
      _iocs_b_putmes(c, it->xd, it->y, 0, str);
    }
  }
}

void drawhelp(int c, int x, int y, int w, const char *s)
{
  char msg[100];
  const char *p;
  char *q;

  for (p = s, q = msg; *q = *p; p++, q++) {
    if (*p == '#') {
      *q++ = ' ';
      *q = ' ';
      p++;
    }
  }
  _iocs_b_putmes(c, x, y, w, msg);

  int xg = x * 8;
  for (p = s; *p; p++, xg += 8) {
    if (*p == '#') {
      int n = p[1] - 'a';
      _iocs_textput(xg, y * 16, &keybdpat[n]);
    }
  }
}

//****************************************************************************
// Input
//****************************************************************************

int keyinp(int timeout)
{
  struct iocs_time tm = _iocs_ontime();
  while (_iocs_b_keysns() == 0) {
    struct iocs_time tm2 = _iocs_ontime();
    if ((timeout >= 0) && (tm2.sec - tm.sec > timeout)) {
      return -1;
    }
  }
  return _iocs_b_keyinp();
}

static int isfbyte(unsigned char *s, int p)
{
  int res = 1;
  while (p-- > 0) {
    if (res == 0) {
      res = 1;
    } else if ((*s >= 0x80 && *s <= 0x9f) || (*s >= 0xe0 && *s <= 0xff)) {
      res = 0;
    }
    s++;
  }
  return res;
}

static struct cmd_wifi_scan cmd_wifi_scan;
static struct res_wifi_scan wifi_scan
#ifdef XTEST
= { .n_items = 3, .ssid = { "wifi_ap1", "wifi_ap2", "wifi_ap3" }}
#endif
;

static int input_entry_main(struct itemtbl *it, bool mask, bool wifi)
{
  int res = 0;
  char temp[256];
  bool done = false;
  bool hide = mask;

  strcpy(temp, it->value);
  int cur = strlen(temp);   // カーソルの初期位置を入力前の文字列の末尾に設定する

  _iocs_os_curon();

  do {
    int len = strlen(temp);
    /* カーソルが文字列の表示範囲に収まるようにする(SJISの2バイト目が先頭になったらさらに微調整) */
    int pos = (cur > it->wd - 1) ? it->wd - 1 : cur;
    int head = cur - pos;
    if (!isfbyte(temp, head)) {
      pos--;
      head++;
    }

    /* 入力文字列を表示する */
    _iocs_b_curoff();
    drawvalue(2, it, temp + head, hide);
    _iocs_b_locate(it->xd + pos, it->y);
    _iocs_b_curon();

    /* キー入力処理 */
    while (1) {
      int k;
      if (wifi) {
        while ((k = keyinp(200)) < 0) {
#ifndef XTEST
          cmd_wifi_scan.command = CMD_WIFI_SCAN;
          cmd_wifi_scan.clear = 0;
          com_cmdres(&cmd_wifi_scan, sizeof(cmd_wifi_scan), &wifi_scan, sizeof(wifi_scan));
#endif
          for (int i = 0; i < 4; i++)
            _iocs_b_putmes(3, it->xd, it->y + 2 + i, it->wd - 1,
                           i < wifi_scan.n_items ? (const char *)wifi_scan.ssid[i] : "");
        }
      } else {
        k = keyinp(-1);
      }
      int c = k & 0xff;
      if (c == '\r') {                          // CR
        strcpy(it->value, temp);
        res = 1;
        done = true;
      } else if (c == '\x1b') {                 // ESC
        done = true;
      } else if (mask && (c == '\t')) {         // TAB
        hide = 1 - hide;
      } else if (c == '\x17' || k == 0x3f00) {  // CTRL+W or CLR
        temp[0] = '\0';
        cur = 0;
      } else if (k == '\x15' || k == 0x3a00) {  // CTRL+U or UNDO
        strcpy(temp, it->value);
        cur = strlen(temp);
      } else if (c == '\x01' || k == 0x3900 || k == 0x3600) { // CTRL+A or ROLLDOWN or HOME
        cur = 0;
      } else if (c == '\x05' || k == 0x3800) {  // CTRL+E or ROLLUP
        cur = len;
      } else if (c == '\x02' || k == 0x3b00) {  // CTRL+B or ←
        cur = cur > 0 ? cur - 1 : 0;
        if (!isfbyte(temp, cur))
          cur--;
      } else if (c == '\x06' || k == 0x3d00) {  // CTRL+F or →
        cur = cur < len ? cur + 1 : len;
        if (!isfbyte(temp, cur))
          cur++;
      } else if (wifi && (c == '\x0e' || k == 0x3e00)) {  // CTRL+N or ↓
        res = -1;
        done = true;
      } else if (wifi && (c == '\x10' || k == 0x3c00)) {  // CTRL+N or ↑
        res = -2;
        done = true;
      } else if (c == '\b') {                   // BS
        if (cur > 0) {
          if (!isfbyte(temp, cur - 1)) {
            strcpy(&temp[cur - 2], &temp[cur]);
            cur -= 2;
          } else {
            strcpy(&temp[cur - 1], &temp[cur]);
            cur--;
          }
        }
      } else if (c == '\x04' || k == 0x3700) {  // CTRL+D or DEL
        if (cur < len) {
          if (!isfbyte(temp, cur + 1)) {
            strcpy(&temp[cur], &temp[cur + 2]);
          } else {
            strcpy(&temp[cur], &temp[cur + 1]);
          }
        }
      } else if (c >= ' ') {
        if (len < it->valuesz - 2) {
          memmove(&temp[cur + 1], &temp[cur], strlen(&temp[cur]) + 1);
          temp[cur++] = k & 0xff;
        }
      } else {
        continue;
      }
      break;
    }
  } while (!done);

  _iocs_os_curof();
  drawvalue(3, it, it->value, mask);

  return res;
}

/* 1項目をキー入力する */

int input_entry(struct itemtbl *it, void *v)
{
  return input_entry_main(it, false, false);
}

/* 1項目をキー入力する(パスワード) */

int input_passwd(struct itemtbl *it, void *v)
{
  return input_entry_main(it, true, false);
}


/* 数値をリストから選択する */

int input_numlist(struct itemtbl *it, void *v)
{
  struct numlist_opt *opt = (struct numlist_opt *)v;
  int res = 0;
  uint8_t value = *(uint8_t *)it->value;

  value = max(value, opt->min);
  value = min(value, opt->max);

  while (1) {
    drawvalue(10, it, (char *)&value, 0);

    /* キー入力処理 */
    int k = keyinp(-1);
    int c = k & 0xff;
    if (c == '\r') {                          // CR
      *(uint8_t *)it->value = value;
      res = 1;
      break;
    } else if (c == '\x1b') {                 // ESC
      break;
    } else if (c == '\x0e' || k == 0x3e00) {  // CTRL+N or ↓
      value = min(value + 1, opt->max);
    } else if (c == '\x10' || k == 0x3c00) {  // CTRL+P or ↑
      value = max(value - 1, opt->min);
    } else if (c == '\x01' || k == 0x3900 || k == 0x3600) { // CTRL+A or ROLLDOWN or HOME
      value = opt->min;
    } else if (c == '\x05' || k == 0x3800) {  // CTRL+E or ROLLUP
      value = opt->max;
    }
  }

  drawvalue(3, it, it->value, 0);
  return res;
}

/* WiFiアクセスポイントリストから選択する */

int input_wifiap(struct itemtbl *it, void *v)
{
  int res = 0;
  int top = 0;
  int cur = -1;

#ifndef XTEST
  cmd_wifi_scan.command = CMD_WIFI_SCAN;
  cmd_wifi_scan.clear = 1;
  com_cmdres(&cmd_wifi_scan, sizeof(cmd_wifi_scan), &wifi_scan, sizeof(wifi_scan));
#endif

  drawframe(it->xd - 2, it->y - 1, 32, 8, 2, -1);
  drawhline(it->xd, it->y + 1, 28, 2);

  while (1) {
    if (cur < 0) {
      /* WiFiアクセスポイントを直接入力 */
      for (int i = 0; i < 4; i++)
        _iocs_b_putmes(3, it->xd, it->y + 2 + i, it->wd - 1,
                       i < wifi_scan.n_items ? (const char *)wifi_scan.ssid[i] : "");
      int res = input_entry_main(it, false, true);
      if (res == 0 || res == 1) {
        break;
      }
      if (wifi_scan.n_items == 0)
        continue;

      cur = (res == -1) ? 0 : wifi_scan.n_items - 1;
    }

    /* リスト表示範囲がカーソル位置からはみ出さないようにする */
    top = (top > cur) ? cur : ((top + 4 <= cur) ? cur - 3 : top);

    /* WiFiアクセスポイントをリスト表示 */
    for (int i = 0; i < 4; i++) {
      _iocs_b_putmes(top + i == cur ? 10 : 2, 
                     it->xd, it->y + 2 + i, it->wd - 1,
                     (top + i < wifi_scan.n_items) ? (const char *)wifi_scan.ssid[top + i] : "");
    }
    _iocs_b_putmes(3, it->xd, it->y, it->wd - 1, wifi_scan.ssid[cur]);

    /* キー入力を待ちながら2秒おきにWiFiアクセスポイントをスキャンする */
    int k = keyinp(200);
    if (k < 0) {
#ifndef XTEST
      cmd_wifi_scan.command = CMD_WIFI_SCAN;
      cmd_wifi_scan.clear = 0;
      com_cmdres(&cmd_wifi_scan, sizeof(cmd_wifi_scan), &wifi_scan, sizeof(wifi_scan));
#endif
      continue;
    }
    int c = k & 0xff;
    if (c == '\r') {                          // CR
      strcpy(it->value, cur < wifi_scan.n_items ? (const char *)wifi_scan.ssid[cur] : "");
      res = 1;
      break;
    } else if (c == '\x1b') {                 // ESC
      break;
    } else if (c == '\x0e' || k == 0x3e00) {  // CTRL+N or ↓
      cur = cur < wifi_scan.n_items - 1 ? cur + 1 : -1;
    } else if (c == '\x10' || k == 0x3c00) {  // CTRL+P or ↑
      cur = cur > 0 ? cur - 1 : -1;
    } else if (c == '\x01' || k == 0x3900 || k == 0x3600) { // CTRL+A or ROLLDOWN or HOME
      cur = 0;
    } else if (c == '\x05' || k == 0x3800) {  // CTRL+E or ROLLUP
      cur = wifi_scan.n_items - 1;
    }
  }

  topview();

  return res;
}

/* リストからディレクトリまたはファイルを選択する */

struct cmd_smb2_enum cmd_smb2_enum;
struct cmd_smb2_list cmd_smb2_list;
struct res_smb2_enum smb2_enum;
struct res_smb2_list smb2_list;
const char *filelist[256];
int nfile;

int input_dirfile(struct itemtbl *it, void *v)
{
  int res = 0;
  char value[256];
  bool seldir = (v == NULL);
  int ity = min(it->y, 20);

  strcpy(value, it->value);

  cmd_smb2_enum.command = CMD_SMB2_ENUM;
  cmd_smb2_list.command = CMD_SMB2_LIST;
  memset(cmd_smb2_list.share, 0, sizeof(cmd_smb2_list.share));
  memset(cmd_smb2_list.path, 0, sizeof(cmd_smb2_list.path));

  bool sharelist = true;

  /* 現在値を共有名とパス名に分離する */

  char *p = strchr(value, '/');
  if (p != NULL) {
    strncpy(cmd_smb2_list.share, value, p - value);
    strcpy(cmd_smb2_list.path, p + 1);
    sharelist = false;
  }

  drawframe(it->xd - 2, ity - 1, it->wd + 4, 9, 2, -1);
  drawhline(it->xd, ity + 1, it->wd, 2);

  int done = 0;
  bool updir = false;
  do {
    int top = 0;
    int cur = -1;

#ifndef XTEST
    strcpy(value, "");
    if (!sharelist) {     // 選択するファイル名/ディレクトリ名リストを用意
      if (!updir) {
        /* 与えられたパス名でファイル一覧を取得してみる */
        /* パス名がディレクトリ名なら成功するが、ファイル名なら失敗するのでそのファイルが存在する
         * ディレクトリでファイル一覧を取得し直す */
        com_cmdres(&cmd_smb2_list, sizeof(cmd_smb2_list), &smb2_list, sizeof(smb2_list));
      }
      if (smb2_list.status != 0 || updir) {
        /* ディレクトリ取得に失敗 or 親ディレクトリに移動 */
        /* パス名から親ディレクトリを得てファイル一覧を取得する */
        int len = strlen(cmd_smb2_list.path);
        if (len > 1) {
          char *p = &cmd_smb2_list.path[len - 1];
          if (*p == '/')                              // "<path>/<dir>/" のケースでは末尾の'/'はスキップ
            p--;

          while (p != (char *)cmd_smb2_list.path) {   // "<path>/<name>" から <path>/ と <name> を分離する
            if (*p == '/')
              break;
            p--;
          }
          if (p != (char *)cmd_smb2_list.path) {      // <name> の部分をコピー
            strcpy(value, p + 1);
            p[1] = '\0';
          } else {                                    // <name> の部分がない場合
            strcpy(value, p);
            cmd_smb2_list.path[0] = '\0';
          }
          com_cmdres(&cmd_smb2_list, sizeof(cmd_smb2_list), &smb2_list, sizeof(smb2_list));
        } else {
          sharelist = true;                           // パス名部分が存在しない場合は共有名一覧へ
        }
        updir = false;
      }

      if (!sharelist && smb2_list.status == 0) {      // 取得したファイル名一覧をリスト化
        cur = -1;
        char *ep = smb2_list.list; 
        while (*ep != '\0') {           // ファイル名リストの末尾を得る
          ep += strlen(ep) + 1;
        }
        filelist[0] = "./";
        filelist[1] = "../";
        for (nfile = 2; nfile < countof(filelist); nfile++) {
          char *np = smb2_list.list;    // libsmb2から得られるファイルが逆順なので並び変える
          char *p = ep;
          while (np != ep) {
            if (!(seldir && np[strlen(np) -1] != '/'))
              p = np;
            np += strlen(np) + 1;
          }
          if (p == ep)
            break;
          ep = p;
          if (strcmp(value, p) == 0) {          // <name> と同じエントリをカーソルの初期位置に
            cur = nfile;
          }
          filelist[nfile] = p;
        }
        cur = cur < 0 ? 0 : cur;                // 初期位置を定められない場合はリスト先頭へ
      } else {
        sharelist = true;
      }
    }

    if (sharelist) {     // 選択する共有名リストを用意
      cmd_smb2_list.path[0] = '\0';
      com_cmdres(&cmd_smb2_enum, sizeof(cmd_smb2_enum), &smb2_enum, sizeof(smb2_enum));
      nfile = smb2_enum.n_items + 1;
      cur = -1;

      filelist[0] = "";
      for (int i = 1; i < nfile; i++) {
        filelist[i] = smb2_enum.share[i - 1];
        if (strcmp(smb2_enum.share[i - 1], cmd_smb2_list.share) == 0) {
          cur = i;
        }
      }
      cur = cur < 0 ? 0 : cur;
    }
#else
    cur = 0;
    nfile = 1;
    filelist[0] = "";
#endif

    while (1) {
      /* リスト表示範囲がカーソル位置からはみ出さないようにする */
      top = (top > cur) ? cur : ((top + 5 <= cur) ? cur - 4 : top);

      /* ディレクトリをリスト表示 */
      for (int i = 0; i < 5; i++) {
        _iocs_b_putmes(top + i == cur ? 10 : 2, 
                       it->xd, ity + 2 + i, it->wd - 1,
                       (top + i < nfile) ? filelist[top + i] : "");
      }

      /* 現在選択されているパス名を表示 */
      if (sharelist) {
        strcpy(value, filelist[cur]);
      } else {
        strcpy(value, cmd_smb2_list.share);
        strcat(value, "/");
        strcat(value, cmd_smb2_list.path);
        strcat(value, filelist[cur]);
      }
      _iocs_b_putmes(3, it->xd, ity, it->wd - 1, value);

      /* キー入力処理 */
      int k = keyinp(-1);
      int c = k & 0xff;
      if (c == '\r') {                          // CR 
        if (sharelist) {
          strcpy(cmd_smb2_list.share, filelist[cur]);
          if (cur == 0) {
            res = 1;
            done = 1;           // 最初のエントリだったら空文字列で終了
          } else {
            sharelist = false;  // 2番目以降のエントリはその共有名のファイル一覧へ移動
          }
          break;
        } else {
          int len = strlen(filelist[cur]);
          if (filelist[cur][len - 1] != '/') {            // ファイルが選択された
            /* file */
            strcat(cmd_smb2_list.path, filelist[cur]);
            res = 1;
            done = 1;
            break;
          } else if (strcmp("./", filelist[cur]) == 0) {  // ディレクトリが選択された
            if (seldir) {
              res = 1;
              done = 1;
            }
            break;
          } else if (strcmp("../", filelist[cur]) == 0) { // 親ディレクトリへ移動
            updir = true;
            break;
          } else {                                        // 子ディレクトリへ移動
            strcat(cmd_smb2_list.path, filelist[cur]);
          }
        }
        break;
      } else if (c == '\x1b') {                 // ESC */
        done = 2;
        break;
      } else if (c == '\x0e' || k == 0x3e00) {  // CTRL+N or ↓
        cur = cur < nfile - 1 ? cur + 1 : 0;
      } else if (c == '\x10' || k == 0x3c00) {  // CTRL+P or ↑
        cur = cur > 0 ? cur - 1 : nfile - 1;
      } else if (c == '\x01' || k == 0x3600) {  // CTRL+A or HOME
        cur = 0;
      } else if (k == '\x15' || k == 0x3a00) {  // CTRL+U or UNDO
        updir = true;
        break;
      } else if (k == 0x3900) {                 // ROLLDOWN
        cur -= 5;
        cur = cur > 0 ? cur : 0;
      } else if (k == 0x3800) {                 // ROLLUP
        cur += 5;
        cur = cur < nfile - 1 ? cur : nfile - 1;
      }
    }

  } while (!done);

  /* 選択された結果を値に設定する */
  if (done == 1) {
    strcpy(it->value, cmd_smb2_list.share);
    if (strlen(cmd_smb2_list.share)) {
      strcat(it->value, "/");
      strcat(it->value, cmd_smb2_list.path);
    }
  }

  topview();

  return res;
}
