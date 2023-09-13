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
#include <stdio.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <sys/fcntl.h>
#include <string.h>
#include "hardware/watchdog.h"

#include "smb2.h"
#include "libsmb2.h"

#include "x68kzrmthds.h"
#include "virtual_disk.h"
#include "config_file.h"

#include "scsiremote.inc"
#include "bootloader.inc"

//****************************************************************************
// static variables
//****************************************************************************

static struct smb2fh *sfh = NULL;
static size_t filesz = 0;
static size_t filesects = 0;

static struct smb2fh *sfh_h = NULL;

//****************************************************************************
// BPB
//****************************************************************************

#define lsb_hword(x)    ((x) & 0xff), (((x) >> 8) & 0xff)
#define lsb_word(x)     ((x) & 0xff), (((x) >> 8) & 0xff), \
                        (((x) >> 16) & 0xff), (((x) >> 24) & 0xff)

#define MEDIA_TYPE 0xf8

static const uint8_t boot_sector[] = {
    0xeb, 0x58, 0x90,                           //  +0 JmpBoot
    'M', 'S', 'W', 'I', 'N', '4', '.', '1',     //  +3 OEMName
    lsb_hword(512),                             // +11 BytsPerSec
    (CLUSTER_SIZE / SECTOR_SIZE),               // +13 SecPerClus
    lsb_hword(32),                              // +14 RsvdSecCnt
    2,                                          // +16 NumFATs
    lsb_hword(0),                               // +17 RootEntCnt
    lsb_hword(0),                               // +19 TotSec16
    MEDIA_TYPE,                                 // +21 Media
    lsb_hword(0),                               // +22 FATSz16
    lsb_hword(0x3f),                            // +24 SecPerTrk
    lsb_hword(0xff),                            // +26 NumHeads
    lsb_word(0),                                // +28 HiddSec
    lsb_word(VOLUME_SECTOR_COUNT),              // +32 TotSec32
    lsb_word(FAT_SECTORS),                      // +36 FATSz32
    lsb_hword(0),                               // +40 ExtFlags
    lsb_hword(0),                               // +42 FSVer
    lsb_word(2),                                // +44 RootClus
    lsb_hword(1),                               // +48 FSInfo
    lsb_hword(6),                               // +50 BkBootSec
    0,0,0,0,0,0,0,0,0,0,0,0,                    // +52 Reserved
    0x80,                                       // +64 DrvNum
    0,                                          // +65 Reserved
    0x29,                                       // +66 BootSig
    lsb_word(0x12345678),                       // +67 VolID
    'N', 'O', ' ', 'N', 'A', 'M', 'E', ' ', ' ', ' ', ' ',  // +71 VolLab
    'F', 'A', 'T', '3', '2', ' ', ' ', ' ',                 // +82 FilSysType
    0xeb, 0xfe // while(1)                      // +90 BootCode32
};

static const uint8_t fsinfo1[] = {
    lsb_word(0x41615252)            //   +0 LeadSig
};
static const uint8_t fsinfo2[] = {
    lsb_word(0x61417272),           // +484 StrucSig
    lsb_word(0xffffffff),           // +488 Free_Count
    lsb_word(0xffffffff),           // +492 Nxt_Free
    lsb_word(0),                    // +496 reserved
    lsb_word(0),                    // +500 reserved
    lsb_word(0),                    // +504 reserved
    lsb_word(0xaa550000)            // +508 TrailSig
};

//****************************************************************************
// Directory entry
//****************************************************************************

#define ATTR_READONLY       0x01
#define ATTR_HIDDEN         0x02
#define ATTR_SYSTEM         0x04
#define ATTR_VOLUME_LABEL   0x08
#define ATTR_DIR            0x10
#define ATTR_ARCHIVE        0x20

struct dir_entry {
    uint8_t name[11];
    uint8_t attr;
    uint8_t ntRes;
    uint8_t crtTimeTenth;
    uint16_t crtTime;
    uint16_t crtDate;
    uint16_t lstAccDate;
    uint16_t fstClusHI;
    uint16_t wrtTime;
    uint16_t wrtDate;
    uint16_t fstClusLO;
    uint32_t fileSize;
};

void init_dir_entry(struct dir_entry *entry, const char *fn,
                    int attr, int ntres, int cluster, int len)
{
    memcpy(entry->name, fn, 11);
    entry->attr = (attr == 0) ? ATTR_ARCHIVE : attr;
    entry->ntRes = ntres;
    entry->crtTimeTenth = 0;
    entry->crtTime =  entry->wrtTime =
        ((12 << 11) | (0 << 5) | (0 >> 1));
    entry->crtDate = entry->wrtDate = entry->lstAccDate =
        (((2023 - 1980) << 9) | (1 << 5) | (1));
    entry->fstClusHI = cluster >> 16;
    entry->fstClusLO = cluster & 0xffff;
    entry->fileSize = len;
}

//****************************************************************************
// Disk cache
//****************************************************************************

#define DISK_CACHE_SECTS    8
#define DISK_CACHE_SIZE     (DISK_CACHE_SECTS * SECTOR_SIZE)
#define DISK_CACHE_SETS     2

static struct cache {
    uint8_t data[DISK_CACHE_SIZE];
    uint32_t lba;
    size_t sects;
} cache[DISK_CACHE_SETS];
static int cache_lru = 0;

static void cache_init(void)
{
    for (int i = 0; i < DISK_CACHE_SETS; i++) {
        cache[i].lba = 0xffffffff;
        cache[i].sects = 0;
    }
}

int cache_read(uint32_t lba, uint8_t *buf)
{
    for (int i = 0; i < DISK_CACHE_SETS; i++) {
        struct cache *c = &cache[i];
        if (lba >= c->lba && lba < c->lba + c->sects) {
            memcpy(buf, &c->data[(lba - c->lba) * SECTOR_SIZE], SECTOR_SIZE);
            return 0;
        }
    }

    struct cache *c = &cache[cache_lru];
    uint64_t cur;
    if (smb2_lseek(smb2, sfh, lba * SECTOR_SIZE, SEEK_SET, &cur) < 0)
        return -1;
    c->sects = 0;
    int sz = smb2_read(smb2, sfh, c->data, DISK_CACHE_SIZE);
    if (sz < 0)
        return -1;
    c->lba = lba;
    c->sects = sz / SECTOR_SIZE;
    cache_lru = (cache_lru + 1) % DISK_CACHE_SETS;
    memcpy(buf, c->data, SECTOR_SIZE);
    return 0;
}

int cache_write(uint32_t lba, uint8_t *buf)
{
    for (int i = 0; i < DISK_CACHE_SETS; i++) {
        struct cache *c = &cache[i];
        if (lba >= c->lba && lba < c->lba + c->sects) {
            memcpy(&c->data[(lba - c->lba) * SECTOR_SIZE], buf, SECTOR_SIZE);
            break;
        }
    }

    uint64_t cur;
    if (smb2_lseek(smb2, sfh, lba * SECTOR_SIZE, SEEK_SET, &cur) < 0)
        return -1;
    int sz = smb2_write(smb2, sfh, buf, SECTOR_SIZE);
    if (sz < 0)
        return -1;
    return 0;
}

//****************************************************************************
// Virtual FAT32 functions
//****************************************************************************

static struct {
    uint32_t size;
    int sects;
    struct smb2fh *sfh;
    struct smb2fh *sfh_h;
} diskinfo[7];

static uint32_t fat[SECTOR_SIZE];
static uint8_t rootdir[32 * 8];
static uint8_t x68zdir[32 * 16];
static uint8_t pscsiini[256];

int vd_init(const char *path)
{
    struct dir_entry *dirent;
    int len;

    setenv("TZ", "JST-9", true);

    /* Open HDS file */

    if (path) {
        struct smb2_stat_64 st;
        if (smb2_stat(smb2, path, &st) < 0) {
            printf("File %s not found.\n", path);
        } else {
            printf("File %s size=%lld\n", path, st.smb2_size);
            filesz = st.smb2_size;
            filesects = filesz / SECTOR_SIZE;

            if ((sfh = smb2_open(smb2, path, O_RDWR)) == NULL) {
                printf("File %s open failure.\n", path);
                sfh = NULL;
            }
        }
    }
    if ((sfh_h = smb2_open(smb2, "HFS/HUMAN.SYS", O_RDONLY)) == NULL) {
        printf("human.sys open failure.\n");
    }

    diskinfo[1].size = filesz;
    diskinfo[1].sfh = sfh;
    diskinfo[0].size = 0x80000000;
    diskinfo[0].sfh_h = sfh_h;
    for (int i = 0; i < 7; i++) {
        diskinfo[i].sects = (diskinfo[i].size + SECTOR_SIZE - 1) / SECTOR_SIZE;
    }

    strcpy(pscsiini, "[pscsi]\r\n");
    for (int i = 0; i < 7; i++) {
        if (diskinfo[i].size) {
            char str[32] = "IDx=diskx.hds\r\n";
            str[2] = '0' + i;
            str[8] = '0' + i;
            strcat(pscsiini, str);
        }
    }

    /* Initialize FAT */

    memset(fat, 0, sizeof(fat));
    fat[0] = 0x0fffff00u | MEDIA_TYPE;
    fat[1] = 0x0fffffff;
    fat[2] = 0x0ffffff8;    /* cluster 2: root directory */
    fat[3] = 0x0fffffff;    /* cluster 3: X68000Z directory */
    fat[4] = 0x0fffffff;    /* cluster 4: pscsi.ini */
    fat[5] = 0x0fffffff;    /* cluster 5: log.txt */
    fat[6] = 0x0fffffff;    /* cluster 6: config.txt */

    /* Initialize root directory */

    memset(rootdir, 0, sizeof(rootdir));
    dirent = (struct dir_entry *)rootdir;
    init_dir_entry(dirent++, "LOG     TXT", 0, 0x18, 5, LOGSIZE);
    init_dir_entry(dirent++, "CONFIG  TXT", 0, 0x18, 6, strlen(configtxt));
    init_dir_entry(dirent++, "X68000Z    ", ATTR_DIR, 0, 3, 0);

    /* Initialize "X68000Z" directory */

    memset(x68zdir, 0,  sizeof(x68zdir));
    dirent = (struct dir_entry *) x68zdir;
    init_dir_entry(dirent++, ".          ", ATTR_DIR, 0, 3, 0);
    init_dir_entry(dirent++, "..         ", ATTR_DIR, 0, 0, 0);
    init_dir_entry(dirent++, "PSCSI   INI", 0, 0x18, 4, strlen(pscsiini));
    for (int i = 0; i < 7; i++) {
        if (diskinfo[i].size) {
            char fn[12] = "DISKx   HDS";
            fn[4] = '0' + i;
            init_dir_entry(dirent++, fn, 0, 0x18, 0x20000 + 0x20000 * i, diskinfo[i].size);
        }
    }

    return 0;
}


uint8_t vdbuf_read[(512 - 16) * 7];
uint8_t vdbuf_write[(512 - 16) * 7];
int vdbuf_rpages;
int vdbuf_wpages;

// vdbuf header
//   0.. 3  "X68Z" signature
//   4.. 7  session ID
//   8..11  sequence count
//  12      max page (0..6)
//  13..15  reserved
uint8_t vdbuf_header[12];

int vd_read_block(uint32_t lba, uint8_t *buf)
{
    memset(buf, 0, 512);

    if (lba < 0x20) {
        if (lba == 0 || lba == 6) {
            // BPB
            memcpy(buf, boot_sector, sizeof(boot_sector));
            buf[0x1fe] = 0x55;
            buf[0x1ff] = 0xaa;
        } else if (lba == 1) {
            // FSINFO
            memcpy(buf, fsinfo1, sizeof(fsinfo1));
            memcpy(&buf[484], fsinfo2, sizeof(fsinfo2));
        }
        return 0;
    }

    if (lba < 0x4020) {
        // FAT
        lba -= 0x20;
        if (lba >= 0x2000) {
            lba -= 0x2000;
        }
        if (lba == 0) {
            // FAT for directory and small files
            memcpy(buf, fat, SECTOR_SIZE);
        } else if (lba >= 0x400) {
            // "disk0～6.hds"ファイル用のFATデータを作る
            uint32_t *lbuf = (uint32_t *)buf;
            int id = (lba - 0x400) / 0x400;
            lba %= 0x400;
            if (diskinfo[id].size) {
                // 1セクタ分のFAT領域(512/4=128エントリ)が占めるディスク領域 (128*32kB = 4MB)
                int fatdsz = FATENTS_SECT * CLUSTER_SIZE;
                // ファイルに使用するFAT領域のセクタ数(1セクタ未満切り捨て)
                int fatsects = diskinfo[id].size / fatdsz;
                // 1セクタに満たない分のFATエントリ数
                int fatmod = (diskinfo[id].size % fatdsz) / CLUSTER_SIZE;
                // アクセスしようとしているFAT領域先頭のクラスタ番号
                int clsno = 0x20000 + id * 0x20000 + FATENTS_SECT * lba;
                if (lba < fatsects) {
                    // アクセスしようとしているFAT領域のセクタはすべて使用中
                    for (int i = 0; i < FATENTS_SECT; i++) {
                        lbuf[i] = clsno + i + 1;    // クラスタチェインを作る
                    }
                } else if (lba == fatsects) {
                    // アクセスしようとしているFAT領域のセクタは部分的に使われている
                    for (int i = 0; i < fatmod; i++) {
                        lbuf[i] = clsno + i + 1;    // クラスタチェインを作る
                    }
                    lbuf[fatmod] = 0x0fffffff;      // クラスタ末尾
                }
            }
        }
        return 0;
    }

    if (lba == 0x4020) {
        // Root directory
        memcpy(buf, rootdir, sizeof(rootdir));
        return 0;
    }
    if (lba == 0x4060) {
        // "X68000Z" directory
        memcpy(buf, x68zdir, sizeof(x68zdir));
        return 0;
    }
    if (lba == 0x40a0) {
        // "pscsi.ini" file
        strcpy(buf, pscsiini);
        return 0;
    }

    if (lba == 0x40e0 || lba == 0x40e1) {
        // "log.txt" file
        memcpy(buf, &log_txt[(lba - 0x40e0) * SECTOR_SIZE], SECTOR_SIZE);
        return 0;
    }

    if (lba == 0x4120) {
        // "config.txt" file
        memcpy(buf, configtxt, SECTOR_SIZE);
        return 0;
    }

    if (lba >= 0x00803fa0) {
        // "disk0～6.hds" file read
        lba -= 0x00803fa0;
        int id = lba / 0x800000;
        lba %= 0x800000;
        if (lba >= diskinfo[id].sects)
            return -1;
        printf("disk %d: read 0x%x\n", id, lba);
        if (diskinfo[id].sfh) {
            if (cache_read(lba, buf) < 0)   /* TBD: id */
                return -1;
            xTaskNotify(blink_th, 2, eSetBits);
            return 0;
        }
        if (diskinfo[id].sfh_h) {
            if (lba == 0) {
                // SCSI disk signature
                memcpy(buf, "X68SCSI1", 8);
            } else if (lba == 2) {
                // boot loader
                memcpy(buf, bootloader, sizeof(bootloader));
            } else if (lba == 4) {
                // SCSI partition signature
                memcpy(buf, "X68K", 4);
                memcpy(buf + 16 , "Human68k", 8);
                memcpy(buf + 256, "X68K", 4);
            } else if (lba >= (0x0c00 / 512) && lba < (0x4000 / 512)) {
                // SCSI device driver
                lba -= 0xc00 / 512;
                if (lba <= sizeof(scsiremote) / 512) {
                    memcpy(buf, &scsiremote[lba * 512], 512);
                }
            } else if (lba >= (0x8000 / 512) && lba < (0x18000 / 512)) {
                // HUMAN.SYS
                lba -= 0x8000 / 512;
                uint64_t cur;
                if (smb2_lseek(smb2, sfh_h, lba * 512, SEEK_SET, &cur) >= 0) {
                    smb2_read(smb2, sfh_h, buf, 512);
                }
            } else {
                int page = lba % 8;
                if (page == 7)
                    return -1;

                memcpy(buf, vdbuf_header, 12);
                buf[12] = vdbuf_rpages;
                memcpy(buf + 16, &vdbuf_read[page * (512 - 16)], 512 - 16);

#if 0
                for (int i = 0; i < 64; i++) {
                    printf("%02x ", buf[i]);
                    if ((i % 16) == 15) printf("\n");
                }
#endif
            }
            return 0;
        }
    }

    return -1;
}

int vd_write_block(uint32_t lba, uint8_t *buf)
{
    if (lba == 0x4120) {
        // "config.txt" file update
        config_parse(buf);
        config_write();

        // reboot by watchdog
        watchdog_enable(500, 1);
        while (1)
            ;
    }

    if (lba >= 0x00803fa0) {
        // "disk0～6.hds" file write
        lba -= 0x00803fa0;
        int id = lba / 0x800000;
        lba %= 0x800000;
        if (lba >= diskinfo[id].sects)
            return -1;
        printf("disk %d: write 0x%x\n", id, lba);
        if (diskinfo[id].sfh) {
            if (cache_write(lba, buf) < 0)      /* TBD: id */
                return -1;
            xTaskNotify(blink_th, 2, eSetBits);
            return 0;
        }
        if (diskinfo[id].sfh_h) {
            int page = lba % 8;
            if (page == 7)
                return -1;
            if (memcmp(buf, "X68Z", 4) != 0) {
                printf("skip\n");
                return -1;
            }
            if (page == 0) {
                memcpy(vdbuf_header, buf, 12);
                vdbuf_wpages = buf[12];
            } else {
                if (memcmp(vdbuf_header, buf, 12) != 0) {
                    printf("skip\n");
                    return -1;
                }
            }

            memcpy(&vdbuf_write[page * (512 - 16)], buf + 16, 512 - 16);
            if (page == vdbuf_wpages) {
                // last page copy
#if 0
                for (int i = 0; i < 64; i++) {
                    printf("%02x ", buf[i]);
                    if ((i % 16) == 15) printf("\n");
                }
                printf("\n");
#endif
                int remote_serv(uint8_t *wbuf, uint8_t *rbuf);
                int rsize = remote_serv(vdbuf_write, vdbuf_read);
                vdbuf_rpages = (rsize - 1) / (512 - 16);
                printf("vdbuf_rpages=%d\n", vdbuf_rpages);
#if 0
                for (int i = 0; i < 48; i++) {
                    printf("%02x ", vdbuf_read[i]);
                    if ((i % 16) == 15) printf("\n");
                }
#endif
            }
            return 0;
        }
    }

    return -1;
}
