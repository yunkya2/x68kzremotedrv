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

#ifndef _VIRTUAL_DISK_H
#define _VIRTUAL_DISK_H

#include <stdint.h>

/* virtual disk volume contstants */

#define SECTOR_SIZE         512
#define CLUSTER_SIZE        32768

#define MAX_CLUSTER         0x100000

#define CLUS_PER_SECT       (CLUSTER_SIZE / SECTOR_SIZE)        // 64
#define FATENTS_SECT        (SECTOR_SIZE / sizeof(uint32_t))    // 128
#define FAT_SECTORS         (MAX_CLUSTER / FATENTS_SECT)        // 0x2000
#define VOLUME_SECTOR_COUNT (0x20 + FAT_SECTORS * 2 + (MAX_CLUSTER - 2) * CLUS_PER_SECT)
                                                                // 0x4003fa0

/* virtual disk function prototypes */

int vd_init(void);
int vd_mount(void);
int vd_read_block(uint32_t lba, uint8_t *buf);
int vd_write_block(uint32_t lba, uint8_t *buf);

/* remote disk information */

#define DTYPE_NOTUSED       0
#define DTYPE_HDS           1
#define DTYPE_REMOTEBOOT    2
#define DTYPE_REMOTECOMM    3

struct diskinfo {
    int type;
    struct smb2fh *sfh;
    struct smb2_context *smb2;
    uint32_t size;
    int sects;
};

extern struct diskinfo diskinfo[7];

#endif  /* _VIRTUAL_DISK_H */
