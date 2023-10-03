/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 Yuichi Nakamura
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
 *
 */

#ifndef _VD_COMMAND_H_
#define _VD_COMMAND_H_

#include <stdint.h>

/* virtual disk buffer definition */

struct vdbuf_header {
    uint32_t signature;         // "X68Z" signature
    uint32_t session;           // session ID
    uint32_t seqno;             // sequence count
    uint8_t page;               // page number
    uint8_t maxpage;            // max page
    uint8_t reserved[2];
};

struct vdbuf {
    struct vdbuf_header header;
    uint8_t buf[512 - sizeof(struct vdbuf_header)];
};

int vd_command(uint8_t *cbuf, uint8_t *rbuf);

/* scsiremote.sys communication protocol definition */

#define CMD_GETTIME     0x00

struct cmd_gettime {
    uint8_t command;
};
struct res_gettime {
    uint16_t year;
    uint8_t mon;
    uint8_t day;
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
};

#endif  /* _VD_COMMAND_H_ */
