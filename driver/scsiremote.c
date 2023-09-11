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

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <setjmp.h>
#include <x68k/dos.h>
#include <x68k/iocs.h>

#include <config.h>
#include <x68kremote.h>
#include <remotedrv.h>

//****************************************************************************
// Global variables
//****************************************************************************

int scsiid;

#ifdef DEBUG
int debuglevel = 0;
#endif

//****************************************************************************
// for debugging
//****************************************************************************

#ifdef DEBUG
char heap[1024];                // temporary heap for debug print
void *_HSTA = heap;
void *_HEND = heap + 1024;
void *_PSP;

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
// Communication
//****************************************************************************

uint8_t vdbuf_read[512 * 7];
uint8_t vdbuf_write[512 * 7];

int seqno = 0;
int seqtim = 0;
int sect = 0x10000;

void com_cmdres(void *wbuf, size_t wsize, void *rbuf, size_t rsize)
{
  int wcnt = (wsize - 1) / (512 - 16);
  int rcnt = (rsize - 1) / (512 - 16);

  for (int i = 0; i <= wcnt; i++) {
    uint8_t *wp = &vdbuf_write[512 * i];
    memcpy(&wp[0], "X68Z", 4);
    *(uint32_t *)&wp[4] = seqno;
    *(uint32_t *)&wp[8] = seqtim;
    wp[12] = wcnt;
    int s = wsize > (512 - 16) ? 512 - 16 : wsize;
    memcpy(&wp[16], wbuf, s);
    wsize -= s;
    wbuf += s;
    for (int i = 0; i < 128; i++) {
      DPRINTF1("%02x ", wp[i]);
      if ((i % 16) == 15)
        DPRINTF1("\r\n");
    }
  }

  _iocs_s_writeext(0, wcnt + 1, scsiid, 1, vdbuf_write);
  sect &= ~7;
  sect = (sect - 8) % 0x200000;

  while (1) {
    int i;
    DPRINTF1("sect=0x%x %d\r\n", sect, rcnt);
    _iocs_s_readext(sect, rcnt + 1, scsiid, 1, vdbuf_read);

    for (i = 0; i <= rcnt; i++) {
      uint8_t *rp = &vdbuf_read[512 * i];
      for (int i = 0; i < 64; i++) {
        DPRINTF1("%02x ", rp[i]);
        if ((i % 16) == 15)
          DPRINTF1("\r\n");
      }
      if (memcmp(rp, vdbuf_write, 12) != 0)
        break;
#if 0
      if (rp[12] != rcnt)
        break;
#endif
      int s = rsize > (512 - 16) ? 512 - 16 : rsize;
      memcpy(rbuf, &rp[16], s);
      rsize -= s;
      rbuf += s;
    }
    if (i > rcnt)
      break;
    sect = (sect - 8 * 8) % 0x200000;
  }
  seqno++;
}

//****************************************************************************
// Utility routine
//****************************************************************************

static int my_atoi(char *p)
{
  int res = 0;
  while (*p >= '0' && *p <= '9') {
    res = res * 10 + *p++ - '0';
  }
  return res;
}

//****************************************************************************
// Device driver interrupt rountine
//****************************************************************************

void com_timeout(struct dos_req_header *req)
{
}

int com_init(struct dos_req_header *req)
{
#ifdef CONFIG_BOOTDRIVER
  _iocs_b_print
#else
  _dos_print
#endif
    ("\r\nX68000Z Remote Drive Driver (version " GIT_REPO_VERSION ") ID=");

  extern uint8_t scsiidd2;
  scsiid = scsiidd2 - 1;

  volatile uint8_t *scsidrvflg = (volatile uint8_t *)0x000cec;
  *scsidrvflg |= (1 << scsiid);

#ifdef CONFIG_BOOTDRIVER
  _iocs_b_putc
#else
  _dos_putchar
#endif
    ('0' + scsiid);

#ifdef CONFIG_BOOTDRIVER
  _iocs_b_print
#else
  _dos_print
#endif
    ("\r\n");

#ifndef CONFIG_BOOTDRIVER
  char *p = (char *)req->status;
  p += strlen(p) + 1;
  while (*p != '\0') {
    if (*p == '/' || *p =='-') {
      p++;
      switch (*p | 0x20) {
#ifdef DEBUG
      case 'd':         // /D .. デバッグレベル増加
        debuglevel++;
        break;
#endif
    }
    p += strlen(p) + 1;
  }
#endif

#ifndef CONFIG_BOOTDRIVER
  _dos_print("ドライブ");
  _dos_putchar('A' + *(char *)&req->fcb);
  _dos_print(":でRS-232Cに接続したリモートドライブが利用可能です (");
  _dos_print(baudstr);
  _dos_print("bps)\r\n");
#endif
  DPRINTF1("Debug level: %d\r\n", debuglevel);

  return 1;
}
