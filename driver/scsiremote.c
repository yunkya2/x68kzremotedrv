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
#include <vd_command.h>
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

struct vdbuf vdbuf_read;
struct vdbuf vdbuf_write;

int seqno = 0;
int seqtim = 0;
int sect = 0x400000;

#define SCSICOMMID    6

void com_cmdres(void *wbuf, size_t wsize, void *rbuf, size_t rsize)
{
  struct vdbuf_header *h;
  int wcnt = (wsize - 1) / (512 - 16);
  int rcnt = (rsize - 1) / (512 - 16);

  h = &vdbuf_write.header;
  h->signature = 0x5836385a;    /* "X68Z" */
  h->session = seqtim;
  h->seqno = seqno;
  h->maxpage = wcnt;
  for (int i = 0; i <= wcnt; i++) {
    h->page = i;
    int s = wsize > (512 - 16) ? 512 - 16 : wsize;
    memcpy(vdbuf_write.buf, wbuf, s);
    wsize -= s;
    wbuf += s;
    _iocs_s_writeext(0x20, 1, SCSICOMMID, 1, &vdbuf_write);
    for (int i = 0; i < 128; i++) {
      DPRINTF1("%02x ", vdbuf_write[i]);
      if ((i % 16) == 15)
        DPRINTF1("\r\n");
    }
  }

  sect = ((sect - 8) % 0x200000) + 0x200000;
  h = &vdbuf_read.header;
  for (int i = 0; i <= rcnt; i++) {
    while (1) {
      DPRINTF1("sect=0x%x\r\n", sect);
      _iocs_s_readext(sect + (i & 7), 1, SCSICOMMID, 1, &vdbuf_read);
      for (int i = 0; i < 64; i++) {
        DPRINTF1("%02x ", vdbuf_read[i]);
        if ((i % 16) == 15)
          DPRINTF1("\r\n");
      }
      if (memcmp(&vdbuf_read, &vdbuf_write, 12) == 0)
        break;
      sect = ((sect - 0x10000) % 0x200000) + 0x200000;
    }
    int s = rsize > (512 - 16) ? 512 - 16 : rsize;
    memcpy(rbuf, vdbuf_read.buf, s);
    rcnt = h->maxpage;
    rsize -= s;
    rbuf += s;
    if ((i & 7) == 7) {
      sect = ((sect - 8) % 0x200000) + 0x200000;
    }
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
    ("\r\nX68000 Z Remote Drive Driver (version " GIT_REPO_VERSION ") ID=");

  extern uint8_t scsiidd2;
  scsiid = scsiidd2 - 1;

  int unit = 0;

  volatile uint8_t *scsidrvflg = (volatile uint8_t *)0x000cec;
  *scsidrvflg |= (1 << scsiid);

  seqtim = _iocs_bindateget();
  seqtim ^= _iocs_timeget() << 8;
  struct iocs_time it = _iocs_ontime();
  seqtim ^= it.sec;

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

  {
    struct cmd_getinfo cmd;
    struct res_getinfo res;
    cmd.command = CMD_GETINFO;
    com_cmdres(&cmd, sizeof(cmd), &res, sizeof(res));
    if (res.year > 0) {
      _iocs_timeset(_iocs_timebcd((res.hour << 16) | (res.min << 8) | res.sec));
      _iocs_bindateset(_iocs_bindatebcd((res.year << 16) | (res.mon << 8) | res.day));
    }
    unit = res.unit;
  }
  {
    struct cmd_init cmd;
    struct res_init res;
    cmd.command = 0x00; /* init */
    com_cmdres(&cmd, sizeof(cmd), &res, sizeof(res));
  }

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
  _dos_print(":でSCSIに接続したリモートドライブが利用可能です\r\n");
#endif
  DPRINTF1("Debug level: %d\r\n", debuglevel);

  return unit;
}
