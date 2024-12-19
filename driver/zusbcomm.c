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
#include <string.h>
#include <setjmp.h>
#include <x68k/iocs.h>
#include <x68k/dos.h>

#include <zusb.h>

#include "remotedrv.h"
#include "zusbcomm.h"

//****************************************************************************
// for debugging
//****************************************************************************

#ifdef DEBUG
#define DPRINTF(...)  printf(__VA_ARGS__)
#else
#define DPRINTF(...)
#endif

//****************************************************************************
// Global variables
//****************************************************************************

struct zusb_rmtdata *com_rmtdata = NULL;

//****************************************************************************
// Local variables
//****************************************************************************

static bool selfopen;

//****************************************************************************
// USB device connection
//****************************************************************************

// ZUSBRMT デバイスドライバが既に存在すれば zsub_rmtdata へのポインタを返す
static struct zusb_rmtdata *find_zusbrmt(void)
{
  const char *devh = (const char *)0x006800;
  // Human68k初期化中はDOS _GETDPBが使えないため、直接デバイスドライバのリンクリストを検索する
  while (memcmp(devh, "NUL     ", 8) != 0) {
    devh += 2;
  }
  devh -= 14;
  do {
    const char *p = devh + 14;
    if (memcmp(p, "\x01ZUSBRMT", 8) == 0 ||
        memcmp(p, "\x01ZUSBHDS", 8) == 0) {
      struct zusb_rmtdata *rd = &(*(struct zusb_rmtdata **)(p - 4))[-1];
      zusb_set_channel(rd->zusb_ch);
      return rd;
    }
    devh = *(const char **)devh;
  } while (devh != (char *)-1);
  return NULL;
}

static const zusb_endpoint_config_t epcfg_tmpl[ZUSB_N_EP] = {
  { ZUSB_DIR_IN,  ZUSB_XFER_BULK, 0 },
  { ZUSB_DIR_OUT, ZUSB_XFER_BULK, 0 },
  { 0, 0, -1 },
};

// デバイスの接続処理を行う
static int connect_device(void)
{
  int devid;
  zusb_endpoint_config_t epcfg[ZUSB_N_EP];

  if ((devid = zusb_find_device_with_vid_pid(0xcafe, 0x4012, 0)) <= 0) {
    return -1;
  }

  memcpy(epcfg, epcfg_tmpl, sizeof(epcfg));
  if (zusb_connect_device(devid, 1, 0xff, -1, -1, epcfg) <= 0) {
    return -1;
  }

  return devid;
}

//****************************************************************************
// Communication APIs
//****************************************************************************

// ZUSB デバイスをオープンしてチャネル番号を返す
int com_connect(int protected)
{
  int ch;

  // 既にリモートドライブを使うドライバが存在する場合は、そのチャネルを使う
  selfopen = false;
  if (com_rmtdata = find_zusbrmt()) {
    return com_rmtdata->zusb_ch;
  }

  // オープンされていなければ新規にオープンする
  ch = (protected ? zusb_open_protected() : zusb_open());
  if (ch < 0) {
    return -1;
  }
  selfopen = true;
  connect_device();
  return ch;
}

// 自分で ZUSB デバイスをオープンしていたらクローズする
void com_disconnect(void)
{
  if (selfopen) {
    zusb_disconnect_device();
    zusb_close();
  }
  com_rmtdata = NULL;
}

void com_cmdres(void *wbuf, size_t wsize, void *rbuf, size_t rsize)
{
  while (1) {
    *(uint32_t *)zusbbuf = wsize;
    memcpy(zusbbuf + 4, wbuf, wsize);

    zusb_set_ep_region(0, zusbbuf, rsize);
    zusb_set_ep_region(1, zusbbuf, wsize + 4);

    zusb->stat = 0xffff;
    zusb_send_cmd(ZUSB_CMD_SUBMITXFER(0));
    zusb_send_cmd(ZUSB_CMD_SUBMITXFER(1));

    uint16_t stat;
    do {
      stat = zusb->stat;
      if (stat & ZUSB_STAT_ERROR) {
        break;
      }
    } while ((stat & (ZUSB_STAT_PCOMPLETE(0)|ZUSB_STAT_PCOMPLETE(1))) !=
                     (ZUSB_STAT_PCOMPLETE(0)|ZUSB_STAT_PCOMPLETE(1)));

    if (stat & ZUSB_STAT_ERROR) {
      int err = zusb->err & 0xff;
      if (err == ZUSB_ENOTCONN || err == ZUSB_ENODEV) {
        // USBデバイスが繋ぎ変えられていたら一度切断して再接続を試みる
        zusb_disconnect_device();
        if (connect_device() > 0) {
          continue;
        } else {
          zusb_send_cmd(ZUSB_CMD_CANCELXFER(0));
          zusb_send_cmd(ZUSB_CMD_CANCELXFER(1));
        }
        longjmp(jenv, -1);
      }
    }

    rsize = zusb->pcount[0];
    memcpy(rbuf, zusbbuf, rsize);
    return;
  }
}
