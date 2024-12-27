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

#ifndef _ZREMOTEIMG_H_
#define _ZREMOTEIMG_H_

#include <stdint.h>

//****************************************************************************
// Human68k structure definitions
//****************************************************************************

struct dos_req_header {
  uint8_t magic;       // +0x00.b  Constant (26)
  uint8_t unit;        // +0x01.b  Unit number
  uint8_t command;     // +0x02.b  Command code
  uint8_t errl;        // +0x03.b  Error code low
  uint8_t errh;        // +0x04.b  Error code high
  uint8_t reserved[8]; // +0x05 .. +0x0c  not used
  uint8_t attr;        // +0x0d.b  Attribute / Seek mode
  void *addr;          // +0x0e.l  Buffer address
  uint32_t status;     // +0x12.l  Bytes / Buffer / Result status
  void *fcb;           // +0x16.l  FCB
} __attribute__((packed, aligned(2)));

struct dos_bpb {
  uint16_t sectbytes;  // +0x00.b  Bytes per sector
  uint8_t sectclust;   // +0x02.b  Sectors per cluster
  uint8_t fatnum;      // +0x03.b  Number of FATs
  uint16_t resvsects;  // +0x04.w  Reserved sectors
  uint16_t rootent;    // +0x06.w  Root directory entries
  uint16_t sects;      // +0x08.w  Total sectors
  uint8_t mediabyte;   // +0x0a.b  Media byte
  uint8_t fatsects;    // +0x0b.b  Sectors per FAT
  uint32_t sectslong;  // +0x0c.l  Total sectors (long)
  uint32_t firstsect;  // +0x10.l  Partition first sector
};

//****************************************************************************
// Private structure definitions
//****************************************************************************

#endif /* _ZREMOTEIMG_H_ */
