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

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "smb2.h"
#include "libsmb2.h"

#include "main.h"
#include "virtual_disk.h"
#include "vd_command.h"

//****************************************************************************
// Static variables
//****************************************************************************

#define DISK_CACHE_SECTS    8
#define DISK_CACHE_SIZE     (DISK_CACHE_SECTS * SECTOR_SIZE)
#define DISK_CACHE_SETS     4

static struct cache {
    uint8_t data[DISK_CACHE_SIZE];
    struct smb2_context *smb2;
    struct smb2fh *sfh;
    uint32_t lba;
    size_t sects;
} cache[DISK_CACHE_SETS];
static int cache_next = 0;

//****************************************************************************
// HDS Disk cache
//****************************************************************************

void hds_cache_init(void)
{
    for (int i = 0; i < DISK_CACHE_SETS; i++) {
        cache[i].sfh = NULL;
        cache[i].lba = 0xffffffff;
        cache[i].sects = 0;
    }
}

int hds_cache_read(struct smb2_context *smb2, struct smb2fh *sfh, uint32_t lba, uint8_t *buf)
{
    for (int i = 0; i < DISK_CACHE_SETS; i++) {
        struct cache *c = &cache[i];
        if (c->sfh == sfh && c->smb2 == smb2 && lba >= c->lba && lba < c->lba + c->sects) {
            memcpy(buf, &c->data[(lba - c->lba) * SECTOR_SIZE], SECTOR_SIZE);
            return VDERR_OK;
        }
    }

    struct cache *c = &cache[cache_next];
    uint64_t cur;
    if (smb2_lseek(smb2, sfh, lba * SECTOR_SIZE, SEEK_SET, &cur) < 0)
        return VDERR_EIO;
    c->sects = 0;
    int sz = smb2_read(smb2, sfh, c->data, DISK_CACHE_SIZE);
    if (sz < 0)
        return VDERR_EIO;
    c->sfh = sfh;
    c->smb2 = smb2;
    c->lba = lba;
    c->sects = sz / SECTOR_SIZE;
    cache_next =(cache_next + 1) % DISK_CACHE_SETS;
    memcpy(buf, c->data, SECTOR_SIZE);
    return VDERR_OK;
}

int hds_cache_write(struct smb2_context *smb2, struct smb2fh *sfh, uint32_t lba, uint8_t *buf)
{
    for (int i = 0; i < DISK_CACHE_SETS; i++) {
        struct cache *c = &cache[i];
        if (c->sfh == sfh && c->smb2 == smb2 && lba >= c->lba && lba < c->lba + c->sects) {
            memcpy(&c->data[(lba - c->lba) * SECTOR_SIZE], buf, SECTOR_SIZE);
            break;
        }
    }

    uint64_t cur;
    if (smb2_lseek(smb2, sfh, lba * SECTOR_SIZE, SEEK_SET, &cur) < 0)
        return VDERR_EIO;
    int sz = smb2_write(smb2, sfh, buf, SECTOR_SIZE);
    if (sz < 0)
        return VDERR_EIO;
    return VDERR_OK;
}
