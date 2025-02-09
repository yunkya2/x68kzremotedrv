/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Yuichi Nakamura (@yunkya2)
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

#ifndef _ZUSBREGS_H_
#define _ZUSBREGS_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* constatnt definitions */

#define ZUSB_MAGIC              0x5a55    /* 'ZU' */
#define ZUSB_BASEADDR           0xec0000
#define ZUSB_N_CH               8
#define ZUSB_N_EP               8
#define ZUSB_SHIFT_CH           12
#define ZUSB_SZ_CH              (1 << ZUSB_SHIFT_CH)        /* 4096bytes */
#define ZUSB_SZ_REGS            128
#define ZUSB_SZ_USBBUF          (ZUSB_SZ_CH - ZUSB_SZ_REGS)
#define ZUSB_REG(c)             (ZUSB_BASEADDR + (c) * ZUSB_SZ_CH)

/* status bit */

#define ZUSB_STAT_INUSE         0x8000
#define ZUSB_STAT_PROTECTED     0x4000
#define ZUSB_STAT_CONNECTED     0x2000
#define ZUSB_STAT_BUSY          0x1000
#define ZUSB_STAT_HOTPLUG       0x0400
#define ZUSB_STAT_ERROR         0x0200
#define ZUSB_STAT_COMPLETE      0x0100
#define ZUSB_STAT_PCOMPLETE(n)  (1 << (n))
#define ZUSB_STAT_MUTABLE       0x0fff

/* command code */

#define ZUSB_CMD_GETVER         0x00
#define ZUSB_CMD_OPENCH         0x01
#define ZUSB_CMD_CLOSECH        0x02
#define ZUSB_CMD_OPENCHP        0x03
#define ZUSB_CMD_CLOSECHP       0x04
#define ZUSB_CMD_SETIVECT       0x05
#define ZUSB_CMD_GETIVECT       0x06

#define ZUSB_CMD_GETDEV         0x10
#define ZUSB_CMD_NEXTDEV        0x11
#define ZUSB_CMD_GETDESC        0x12
#define ZUSB_CMD_CONTROL        0x13
#define ZUSB_CMD_CONNECT        0x14
#define ZUSB_CMD_DISCONNECT     0x15
#define ZUSB_CMD_SETIFACE       0x16

/* command code (asynchronous) */

#define ZUSB_CMD_ASYNC          0x80
#define ZUSB_CMD_SUBMITXFER(e)  (0x80 + (e))
#define ZUSB_CMD_CANCELXFER(e)  (0x90 + (e))
#define ZUSB_CMD_CLEARHALT(e)   (0xa0 + (e))

/* error code */

#define ZUSB_ENOERR         0       /* no error */
#define ZUSB_EBUSY          1       /* device busy */
#define ZUSB_EFAULT         2       /* bad address */
#define ZUSB_ENOTCONN       3       /* device not connected */
#define ZUSB_ENOTINUSE      4       /* device not in use */
#define ZUSB_EINVAL         5       /* invalid argument */
#define ZUSB_ENODEV         6       /* no such device */
#define ZUSB_EIO            7       /* device I/O error */

/* registers */

struct zusb_regs {
  uint16_t cmd;                 /* 0xeaX000- */
  uint16_t err;
  uint16_t _reserved0[2];
  uint16_t stat;
  uint16_t inten;
  uint16_t _reserved1[2];

  uint16_t ccount;              /* 0xeaX010- */
  uint16_t caddr;
  uint16_t _reserved2[2];
  uint16_t devid;
  uint16_t param;
  uint16_t value;
  uint16_t index;

  uint16_t pcfg[ZUSB_N_EP];     /* 0xeaX020- */
  uint16_t pcount[ZUSB_N_EP];   /* 0xeaX030- */
  uint32_t paddr[ZUSB_N_EP];    /* 0xeaX040- */
  uint32_t pdaddr[ZUSB_N_EP];   /* 0xeaX060- */
};

struct zusb_isoc_desc {
    uint16_t size;
    uint16_t actual;
};

#ifdef __cplusplus
}
#endif

#endif /* _ZUSBREGS_H_ */
