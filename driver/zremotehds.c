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

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <setjmp.h>

#include <x68k/iocs.h>
#include <x68k/dos.h>

#include <zusb.h>
#include <config.h>
#include <vd_command.h>

#include "zremotehds.h"
#include "zusbcomm.h"

//****************************************************************************
// Global variables
//****************************************************************************

struct dos_req_header *reqheader;   // Human68kからのリクエストヘッダ

jmp_buf jenv;

extern struct zusb_rmtdata zusb_rmtdata;  // リモートドライブドライバ間で共有するデータ

#ifdef DEBUG
int debuglevel = 0;
#endif

static union {
  struct cmd_hdsread  cmd_hdsread;
  struct res_hdsread  res_hdsread;

  struct cmd_hdswrite cmd_hdswrite;
  struct res_hdswrite res_hdswrite;

  struct res_hdsread_full  res_hdsread_full;
  struct cmd_hdswrite_full cmd_hdswrite_full;
} b;

const struct dos_bpb defaultbpb =
{ 512, 1, 2, 1, 224, 2880, 0xf7, 9, 0, 0 };

struct dos_bpb bpb[N_HDS];
struct dos_bpb *bpbtable[N_HDS];

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
  _iocs_b_print(buf);   // _dos_print()だとリダイレクト時に動作しなくなる
}
#else
void DPRINTF(int level, char *fmt, ...)
{
}
#endif
#else
#define DPRINTF(...)
#endif

#define DPRINTF1(...)  DPRINTF(1, __VA_ARGS__)
#define DPRINTF2(...)  DPRINTF(2, __VA_ARGS__)
#define DPRINTF3(...)  DPRINTF(3, __VA_ARGS__)

//****************************************************************************
// Private functions
//****************************************************************************

static int sector_read(int unit, uint8_t *buf, uint32_t pos, int nsect)
{
  struct cmd_hdsread *cmd = &b.cmd_hdsread;
  struct res_hdsread *res = &b.res_hdsread;

  cmd->command = CMD_HDSREAD;
  cmd->unit = unit;
  cmd->nsect = nsect;
  cmd->pos = pos;
  com_cmdres(cmd, sizeof(*cmd), res, sizeof(*res) + nsect * 512);

  if (res->status == VDERR_EINVAL) {
    return 0x1002;      // Drive not ready
  }
  if (res->status != VDERR_OK) {
    return 0x7007;      // Medium error
  }

  memcpy(buf, res->data, nsect * 512);
  return 0;
}

static int sector_write(int unit, uint8_t *buf, uint32_t pos, int nsect)
{
  struct cmd_hdswrite *cmd = &b.cmd_hdswrite;
  struct res_hdswrite *res = &b.res_hdswrite;

  cmd->command = CMD_HDSWRITE;
  cmd->unit = unit;
  cmd->nsect = nsect;
  cmd->pos = pos;
  memcpy(cmd->data, buf, 512 * nsect);
  com_cmdres(cmd, sizeof(*cmd) + 512 * nsect, res, sizeof(*res));

  if (res->status == VDERR_EINVAL) {
    return 0x1002;      // Drive not ready
  }
  if (res->status != VDERR_OK) {
    return 0x7007;      // Medium error
  }

  return 0;
}

static int read_bpb(int unit)
{
  uint8_t sector[512];

  bpb[unit] = defaultbpb;    // BPBが取得できなかった場合のデフォルト値

  // SCSIイメージ signature を確認する
  if (sector_read(unit, sector, 0, 1) != 0) {
    return -1;
  }
  if (memcmp(sector, "X68SCSI1", 8) != 0) {
    return -1;
  }

  // パーティション情報を取得する
  if (sector_read(unit, sector, 2 * 2, 1) != 0) {
    return -1;
  }
  if (memcmp(sector, "X68K", 4) != 0) {
    return -1;
  }

  // 最初の使用可能なHuman68kパーティションを検索してBPBを取得する
  uint8_t *p = sector + 16;
  for (int i = 0; i < 15; i++, p += 16) {
    if (memcmp(p, "Human68k", 8) == 0) {
      if (p[8] & 1) {   // パーティションフラグのbit0が立っている場合は使用不可
        continue;
      }

      uint32_t sect = *(uint32_t *)(p + 8) & 0xffffff;
      uint8_t bootsect[512];

      if (sector_read(unit, bootsect, sect * 2, 1) != 0) {
        return -1;
      }
      memcpy(&bpb[unit], &bootsect[0x12], sizeof(*bpb));
      return 1;

    }
  }

  return 0;
}

//****************************************************************************
// Device driver interrupt rountine
//****************************************************************************

int com_init(struct dos_req_header *req)
{
  _dos_print("\r\nX68000 Z Remote HDS Driver (version " GIT_REPO_VERSION ")\r\n");

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
    _dos_print("リモートHDS用 Raspberry Pi Pico W が接続されていません\r\n");
    return -0x700d;
  }

  {
    struct cmd_getinfo cmd;
    struct res_getinfo res;
    cmd.command = CMD_GETINFO;
    com_cmdres(&cmd, sizeof(cmd), &res, sizeof(res));

    if (res.version != PROTO_VERSION) {
      com_disconnect();
      _dos_print("リモートHDS用 Raspberry Pi Pico W のバージョンが異なります\r\n");
      return -0x700d;
    }

    units = res.hdsunit;
  }

  if (units == 0) {
    com_disconnect();
    return -0x700d;   // リモートHDSが1台もないので登録しない
  }

  com_rmtdata->hds_changed = 0xff;
  com_rmtdata->hds_ready = 0;

  // 全ドライブの最初の利用可能なパーティションのBPBを読み込む
  for (int i = 0; i < units; i++) {
    if (read_bpb(i) > 0) {
      com_rmtdata->hds_ready |= (1 << i);   // BPBが読めたので利用可能
    }
    bpbtable[i] = &bpb[i];
  }
  req->status = (uint32_t)bpbtable;

  if (*(char *)&req->fcb + units > 26) {
    com_disconnect();
    _dos_print("ドライブ数が多すぎます\r\n");
    return -0x700d;
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
  _dos_print("でリモートHDSが利用可能です\r\n");
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

int interrupt(void)
{
  uint16_t err = 0;
  struct dos_req_header *req = reqheader;

  //--------------------------------------------------------------------------
  // Initialization
  //--------------------------------------------------------------------------

  if (req->command == 0x00) {
    // Initialize
    int r = com_init(req);
    if (r >= 0) {
      req->attr = r; /* Number of units */
      extern char _end;
      req->addr = &_end;
      return 0;
    } else {
      return -r;
    }
  }

  //--------------------------------------------------------------------------
  // Command request
  //--------------------------------------------------------------------------

  DPRINTF1("[%d]", req->command);

  if (setjmp(jenv)) {
    // USBデバイスが切り離された
    zusb_disconnect_device();
    return 0x7002;      // ドライブの準備が出来ていない
  }

  int unit = req->unit;

  switch (req->command) {
  case 0x01: /* disk check */
  {
    if (!(com_rmtdata->hds_changed & (1 << req->unit))) {
      *(int8_t *)&req->addr = 1;    // media not changed
    } else {
      DPRINTF1("media changed\r\n");
      *(int8_t *)&req->addr = -1;   // media changed
      com_rmtdata->hds_changed &= ~(1 << req->unit);
    }
    break;
  }

  case 0x02: /* rebuild BPB */
  {
    read_bpb(req->unit);
    req->status = (uint32_t)&bpbtable[req->unit];
    break;
  }

  case 0x05: /* drive control & sense */
  {
    if (!(com_rmtdata->hds_ready & (1 << req->unit))) {
      req->attr = 0x04;   // drive not ready
    } else {
      req->attr = 0x02;
    }
    break;
  }

  case 0x04: /* read */
  {
    DPRINTF1("Read #%06x %04x:", (uint32_t)req->fcb, req->status);

    int sectors = req->status * 2;
    uint32_t pos = (uint32_t)req->fcb * 2 + bpbtable[req->unit]->firstsect * 2;
    uint8_t *p = req->addr;

    while (sectors > 0) {
      int nsect = sectors > HDS_MAX_SECT ? HDS_MAX_SECT : sectors;

      if ((err = sector_read(unit, p, pos, nsect)) != 0) {
        break;
      }
      p += 512 * nsect;
      pos += nsect;
      sectors -= nsect;
    }
    break;
  }

  case 0x08: /* write */
  case 0x09: /* write+verify */
  {
    DPRINTF1("Write #%06x %04x:", (uint32_t)req->fcb, req->status);

    int sectors = req->status * 2;
    uint32_t pos = (uint32_t)req->fcb * 2 + bpbtable[req->unit]->firstsect * 2;
    uint8_t *p = req->addr;

    while (sectors > 0) {
      int nsect = sectors > HDS_MAX_SECT ? HDS_MAX_SECT : sectors;

      if ((err = sector_write(unit, p, pos, nsect)) != 0) {
        break;
      }
      p += 512 * nsect;
      pos += nsect;
      sectors -= nsect;
    }
    break;
  }

  case 0x03: /* ioctl in */
  {
    DPRINTF1("Ioctl in\r\n");
    break;
  }

  case 0x0c: /* ioctl out */
  {
    DPRINTF1("Ioctl out\r\n");
    break;
  }

  default:
    DPRINTF1("Invalid command\r\n");
    err = 0x1003;   // Invalid command
    break;
  }

  return err;
}

//****************************************************************************
// Dummy program entry
//****************************************************************************

void _start(void)
{}
