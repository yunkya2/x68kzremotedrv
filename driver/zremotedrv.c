/*
 * Copyright (c) 2024 Yuichi Nakamura (@yunkya2)
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
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <x68k/iocs.h>
#include <x68k/dos.h>

#include <zusb.h>
#include <config.h>
#include <vd_command.h>
#include <x68kremote.h>
#include <remotedrv.h>

#include "zusbcomm.h"

//****************************************************************************
// Global variables
//****************************************************************************

extern struct zusb_rmtdata zusb_rmtdata;  // リモートドライブドライバ間で共有するデータ

#ifdef DEBUG
int debuglevel = 0;
#endif

#ifdef CONFIG_BOOTDRIVER
#define _dos_putchar(...)   _iocs_b_putc(__VA_ARGS__)
#define _dos_print(...)     _iocs_b_print(__VA_ARGS__)
#endif

//****************************************************************************
// for debugging
//****************************************************************************

#ifdef DEBUG
char heap[1024];                // temporary heap for debug print
void *_HSTA = heap;
void *_HEND = heap + 1024;
void *_PSP;

#ifndef CONFIG_BOOTDRIVER
void DPRINTF(int level, char *fmt, ...)
{
  char buf[256];
  va_list ap;

  if (debuglevel < level)
    return;

  va_start(ap, fmt);
  vsiprintf(buf, fmt, ap);
  va_end(ap);
  _iocs_b_print(buf);
}
#else
void DPRINTF(int level, char *fmt, ...)
{
}
#endif

void DNAMEPRINT(void *n, bool full, char *head)
{
  struct dos_namestbuf *b = (struct dos_namestbuf *)n;

  DPRINTF1("%s%c:", head, b->drive + 'A');
  for (int i = 0; i < 65 && b->path[i]; i++) {
      DPRINTF1("%c", (uint8_t)b->path[i] == 9 ? '\\' : (uint8_t)b->path[i]);
  }
  if (full)
    DPRINTF1("%.8s%.10s.%.3s", b->name1, b->name2, b->ext);
}
#endif

//****************************************************************************
// Device driver interrupt rountine
//****************************************************************************

int com_timeout(struct dos_req_header *req)
{
  zusb_disconnect_device();
  return 0x7002;
}

int com_init(struct dos_req_header *req)
{
  _dos_print("\r\nX68000 Z Remote Drive Driver (version " GIT_REPO_VERSION ")\r\n");

  int units = 0;

  int ch = com_connect(true);
  if (ch < 0) {
    _dos_print("ZUSB デバイスが見つかりません\r\n");
    return -0x700d;
  } else if (com_rmtdata == NULL) {
    com_rmtdata = &zusb_rmtdata;
    com_rmtdata->zusb_ch = ch;
  }

  if (setjmp(jenv)) {
    com_disconnect();
    _dos_print("リモートドライブ用 Raspberry Pi Pico W が接続されていません\r\n");
    return -0x700d;
  }
  {
    struct cmd_getinfo cmd;
    struct res_getinfo res;
    cmd.command = CMD_GETINFO;
    com_cmdres(&cmd, sizeof(cmd), &res, sizeof(res));

    if (res.version != PROTO_VERSION) {
      com_disconnect();
      _dos_print("リモートドライブ用 Raspberry Pi Pico W のバージョンが異なります\r\n");
      return -0x700d;
    }

    // ファイル共有サーバから取得した現在時刻を設定する
    if (res.year > 0 && !(com_rmtdata->rmtflag & 0x80)) {
      *(volatile uint8_t *)0xe8e000 = 'T';
      *(volatile uint8_t *)0xe8e000 = 'W';
      *(volatile uint8_t *)0xe8e000 = 0;    // disable RTC auto adjust
      _iocs_timeset(_iocs_timebcd((res.hour << 16) | (res.min << 8) | res.sec));
      _iocs_bindateset(_iocs_bindatebcd((res.year << 16) | (res.mon << 8) | res.day));
      com_rmtdata->rmtflag |= 0x80;   // RTC adjusted
    }
    units = res.remoteunit;
  }
  {
    struct cmd_init cmd;
    struct res_init res;
    cmd.command = 0x00; /* init */
    com_cmdres(&cmd, sizeof(cmd), &res, sizeof(res));
  }

  if (units == 0) {
    com_disconnect();
    return -0x700d;   // リモートドライブが1台もないので登録しない
  }

#ifndef CONFIG_BOOTDRIVER
  _dos_print("ドライブ");
  _dos_putchar('A' + *(char *)&req->fcb);
  _dos_putchar(':');
  if (units > 1) {
    _dos_putchar('-');
    _dos_putchar('A' + *(char *)&req->fcb + units - 1);
    _dos_putchar(':');
  }
  _dos_print("でリモートドライブが利用可能です\r\n");
#endif
  DPRINTF1("Debug level: %d\r\n", debuglevel);

#ifdef CONFIG_BOOTDRIVER
  /* SCSI ROM のデバイスドライバ組み込み処理から渡される値
   *「何番目のパーティションから起動するか」を Human68k のドライバ初期化処理に返す
   * この値に基づいてどのドライブから起動するか (CONFIG.SYSをどのドライブから読むか) が決まる
   */
  extern uint8_t bootpart;
  *(char *)&req->fcb = bootpart;
#endif

  return units;
}
