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
#include <stddef.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include "pico/time.h"

#include "main.h"
#include "virtual_disk.h"
#include "vd_command.h"
#include "config_file.h"
#include "remoteserv.h"
#include "fileop.h"

//****************************************************************************
// vd_command service
//****************************************************************************

int vd_command(uint8_t *cbuf, uint8_t *rbuf)
{
  DPRINTF2("----VDCommand: 0x%02x\n", cbuf[0]);
  int rsize = -1;

  switch (cbuf[0]) {
  case CMD_GETTIME:
    {
      struct cmd_gettime *cmd = (struct cmd_gettime *)cbuf;
      struct res_gettime *res = (struct res_gettime *)rbuf;
      rsize = sizeof(*res);

      if (boottime == 0 || config_tadjust[0] == '\0') {
        res->year = 0;
      } else {
        time_t tt = (time_t)((boottime + to_us_since_boot(get_absolute_time())) / 1000000);
        tt += atoi(config_tadjust);
        struct tm *tm = localtime(&tt);
        res->year = htobe16(tm->tm_year + 1900);
        res->mon = tm->tm_mon + 1;
        res->day = tm->tm_mday;
        res->hour = tm->tm_hour;
        res->min = tm->tm_min;
        res->sec = tm->tm_sec;
      }
      break;
    }
  default:
    break;
  }

  return rsize;
}
