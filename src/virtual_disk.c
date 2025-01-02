/* 
 * Copyright (c) 2023,2024 Yuichi Nakamura (@yunkya2)
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
 *
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "hardware/watchdog.h"

#include "smb2.h"
#include "libsmb2.h"

#include "main.h"
#include "virtual_disk.h"
#include "config_file.h"
#include "remoteserv.h"
#include "fileop.h"

#include "bootloader.inc"
#include "zremotedrv_boot.inc"
#include "zremoteimg_boot.inc"
#include "settingui.inc"

//****************************************************************************
// Global variables
//****************************************************************************

//****************************************************************************
// Static variables
//****************************************************************************

#define DTYPE_NOTUSED       0
#define DTYPE_REMOTEHDS     1
#define DTYPE_REMOTEDRV     2

struct diskinfo {
    int type;
    struct hdsinfo *hds;
    uint32_t size;
} diskinfo[7];

#define DISKSIZE(di)    ((di)->hds && (di)->hds->sfh ? (di)->hds->size : (di)->size)

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
        (((2025 - 1980) << 9) | (1 << 5) | (1));
    entry->fstClusHI = cluster >> 16;
    entry->fstClusLO = cluster & 0xffff;
    entry->fileSize = len;
}

//****************************************************************************
// Virtual FAT32 functions
//****************************************************************************

static uint32_t fat[SECTOR_SIZE];
static uint8_t rootdir[32 * 8];
static uint8_t x68zdir[32 * 8];
static uint8_t imagedir[32 * 16];
static uint8_t pscsiini[256];
static int imagedir_init = false;

static void vd_sync(void)
{
    static bool synced = false;
    if (!synced) {
        xTaskNotifyWait(0, 0, NULL, portMAX_DELAY);
        synced = true;
    }
}

int vd_init(void)
{
    uint32_t nvalue;
    struct dir_entry *dirent;
    int len;

    setenv("TZ", config.tz, true);

    if (strlen(config.wifi_ssid) == 0 || strlen(config.smb2_server) == 0) {
        /* not configured */
        for (int i = 0; i < 7; i++) {
            diskinfo[i].type = DTYPE_REMOTEDRV;
            diskinfo[i].size = 0x40000;
        }
    } else {
        int id;

        /* Set up remote drive */
        id = (config.bootmode == 1) ? N_HDS : 0;
        diskinfo[id].type = DTYPE_REMOTEDRV;
        diskinfo[id].size = 0x40000;

        /* Set up remote HDS */
        id = (config.bootmode == 1) ? 0 : 1;
        if (config.hdsscsi) {
            for (int i = 0; i < config.hdsunit; i++, id++) {
                diskinfo[id].type = DTYPE_REMOTEHDS;
                diskinfo[id].hds = &hdsinfo[i];
                diskinfo[id].size = 0xfffffe00; /* tentative size */
            }
        } else {
            diskinfo[id].type = DTYPE_REMOTEHDS;
            diskinfo[id].size = 0x40000;
        }
    }

    strcpy(pscsiini, "[pscsi]\r\n");
    for (int i = 0; i < 7; i++) {
        if (diskinfo[i].type != DTYPE_NOTUSED) {
            char str[32];
            sprintf(str, "ID%d=image/%s%d.hds\r\n",
                i,
                diskinfo[i].type == DTYPE_REMOTEDRV ? "rmtdrv" : "rmtimg",
                i);
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
    fat[7] = 0x0fffffff;    /* cluster 7: X68000Z/image directory */

    /* Initialize root directory */

    memset(rootdir, 0, sizeof(rootdir));
    dirent = (struct dir_entry *)rootdir;
    init_dir_entry(dirent++, "LOG     TXT", 0, 0x18, 5, LOGSIZE);
    init_dir_entry(dirent++, "CONFIG  TXT", 0, 0x18, 6, strlen(configtxt));
    init_dir_entry(dirent++, "X68000Z    ", ATTR_DIR, 0, 3, 0);

    /* Initialize "X68000Z" directory */

    memset(x68zdir, 0,  sizeof(x68zdir));
    dirent = (struct dir_entry *)x68zdir;
    init_dir_entry(dirent++, ".          ", ATTR_DIR, 0, 3, 0);
    init_dir_entry(dirent++, "..         ", ATTR_DIR, 0, 0, 0);
    if (config.bootmode < 2)
        init_dir_entry(dirent++, "PSCSI   INI", 0, 0x18, 4, strlen(pscsiini));
    init_dir_entry(dirent++, "IMAGE      ", ATTR_DIR, 0x18, 7, 0);

    return 0;
}

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
            struct diskinfo *di = &diskinfo[id];
            if (di->type != DTYPE_NOTUSED) {
                uint32_t size = DISKSIZE(di);
                // 1セクタ分のFAT領域(512/4=128エントリ)が占めるディスク領域 (128*32kB = 4MB)
                int fatdsz = FATENTS_SECT * CLUSTER_SIZE;
                // ファイルに使用するFAT領域のセクタ数(1セクタ未満切り捨て)
                int fatsects = size / fatdsz;
                // 1セクタに満たない分のFATエントリ数
                int fatmod = (size % fatdsz) / CLUSTER_SIZE;
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

    if (lba >= 0x4120 && lba < 0x4124) {
        // "config.txt" file
        memcpy(buf, &configtxt[(lba - 0x4120) * SECTOR_SIZE], SECTOR_SIZE);
        return 0;
    }

    if (lba == 0x4160) {
        // "X68000Z/image" directory
        if (!imagedir_init) {
            /* Lazy initialization of "X68000Z/image" directory */
            struct dir_entry *dirent;
            vd_sync();
            memset(imagedir, 0,  sizeof(imagedir));
            dirent = (struct dir_entry *)imagedir;
            init_dir_entry(dirent++, ".          ", ATTR_DIR, 0, 4, 0);
            init_dir_entry(dirent++, "..         ", ATTR_DIR, 0, 0, 0);
            for (int i = 0; i < 7; i++) {
                struct diskinfo *di = &diskinfo[i];
                if (di->type != DTYPE_NOTUSED) {
                    char fn[16];
                    sprintf(fn, "%s%d HDS",
                        diskinfo[i].type == DTYPE_REMOTEDRV ? "RMTDRV" : "RMTIMG", i);
                    init_dir_entry(dirent++, fn, 0, 0x18, 0x20000 + 0x20000 * i, DISKSIZE(di));
                }
            }
            imagedir_init = true;
        }
        memcpy(buf, imagedir, sizeof(imagedir));
        return 0;
    }

    if (lba >= 0x00803fa0) {
        // "disk0～6.hds" file read
        lba -= 0x00803fa0;
        int id = lba / 0x800000;
        lba %= 0x800000;
        struct diskinfo *di = &diskinfo[id];
        if (di->type == DTYPE_NOTUSED)
            return -1;
        if (lba >= (DISKSIZE(di) + SECTOR_SIZE - 1) / SECTOR_SIZE)
            return -1;
        DPRINTF3("disk %d: read 0x%x\n", id, lba);

        vd_sync();

        // HDS SCSI remote drive
        if (di->hds != NULL && di->hds->sfh) {
            if (hds_cache_read(di->hds->smb2, di->hds->sfh, lba, buf) < 0)
                return -1;
            return 0;
        }

        if ((diskinfo[id].type == DTYPE_REMOTEDRV) ||
            (diskinfo[id].type == DTYPE_REMOTEHDS && !config.hdsscsi)) {
            int ishds = diskinfo[id].type != DTYPE_REMOTEDRV;
            if (lba == 0) {
                // SCSI disk signature
                memcpy(buf, "X68SCSI1", 8);
                memcpy(&buf[16], ishds ? "ZREMOTEIMG boot " : "ZREMOTEDRV boot ", 16);
                return 0;
            } else if (lba == 2) {
                // boot loader
                memcpy(buf, bootloader, sizeof(bootloader));
                buf[5] = sysstatus != STAT_CONFIGURED;
                return 0;
            }
            if (lba == 4) {
                // SCSI partition signature
                memcpy(buf, "X68K", 4);
                memcpy(buf + 16, "Human68k", 8);
                return 0;
            } else if (lba >= (0x0c00 / 512) && lba < (0x8000 / 512)) {
                // zremotedrv/zremotehds device driver
                size_t size = ishds ? sizeof(zremoteimg_boot) : sizeof(zremotedrv_boot);
                const uint8_t *driver = ishds ? zremoteimg_boot : zremotedrv_boot;

                lba -= 0xc00 / 512;
                if (lba <= size / 512) {
                    size_t remain = size - lba * 512;
                    memcpy(buf, &driver[lba * 512], remain >= 512 ? 512 : remain);
                }
                return 0;
            }
            if (lba >= (0x8000 / 512) && lba < (0x20000 / 512)) {
                // HUMAN.SYS
                lba -= 0x8000 / 512;

                if (ishds) {
                    // リモートイメージの自動起動パーティションのルートディレクトリからHUMAN.SYSを探す
                    static int32_t humanlba = 0;
                    static size_t humanlen = 0;
                    if (humanlba == 0 && hdsinfo[0].sfh != NULL) {
                        // SCSI disk signatureを確認
                        if (hds_cache_read(hdsinfo[0].smb2, hdsinfo[0].sfh, 0, buf) < 0)
                            return -1;
                        if (memcmp(buf, "X68SCSI1", 8) != 0) {
                            humanlba = -1;
                        }

                        // Partition tableからHuman68k自動起動パーティションの先頭セクタ番号を取得
                        uint32_t partsect;
                        if (humanlba == 0) {
                            if (hds_cache_read(hdsinfo[0].smb2, hdsinfo[0].sfh, 2 * 2, buf) < 0)
                                return -1;
                            if (memcmp(buf, "X68K", 4) != 0) {
                                humanlba = -1;
                            } else {
                                uint8_t *p = buf + 16;
                                int i;
                                for (i = 0; i < 15; i++, p += 16) {
                                    if (memcmp(p, "Human68k", 8) == 0 && p[8] == 0) { // 自動起動
                                        partsect = be32toh(*(uint32_t *)&p[8]) & 0xffffff;
                                        DPRINTF1("boot partition sect=%u\n", partsect);
                                        break;
                                    }
                                }
                                if (i == 15)
                                    humanlba = -1;  // 自動起動パーティションが見つからなかった
                            }
                        }

                        // Partitionの先頭セクタからルートディレクトリのセクタ番号を取得
                        uint32_t rootsect;
                        int clusect;
                        int rootent;
                        if (humanlba == 0) {
                            if (hds_cache_read(hdsinfo[0].smb2, hdsinfo[0].sfh, partsect * 2, buf) < 0)
                                return -1;
                            if (buf[0] != 0x60) {
                                humanlba = -1;
                            } else {
                                rootsect = buf[0x1d] * buf[0x15];               // FAT領域のセクタ数 * 個数
                                rootsect += be16toh(*(uint16_t *)&buf[0x16]);   // 予約セクタ数
                                rootsect += partsect;                           // パーティションの先頭セクタ番号
                                clusect = buf[0x14];                            // 1クラスタあたりのセクタ数
                                rootent = be16toh(*(uint16_t *)&buf[0x18]);     // ルートディレクトリのエントリ数
                                DPRINTF1("root directory sect=%u\n", rootsect);
                            }
                        }

                        // ルートディレクトリからHUMAN.SYSを探してファイル先頭のLBAを得る
                        if (humanlba == 0) {
                            for (int i = 0; i < rootent / 16; i++) {
                                if (hds_cache_read(hdsinfo[0].smb2, hdsinfo[0].sfh, rootsect * 2 + i, buf) < 0)
                                    return -1;
                                for (int j = 0; j < 512; j += 32) {
                                    for (int k = 0; k < 11; k++)
                                        buf[j + k] |= 0x20;
                                    if (memcmp(&buf[j], "human   sys", 11) == 0) {
                                        humanlen = *(uint32_t *)&buf[j + 0x1c];
                                        humanlba = *(uint16_t *)&buf[j + 0x1a] - 2;
                                        humanlba = humanlba * clusect + rootsect + rootent / 32;
                                        humanlba *= 2;
                                        DPRINTF1("HUMAN.SYS len=%u lba=%u\n", humanlen, humanlba);
                                        break;
                                    }
                                }
                                if (humanlba)
                                    break;
                            }
                            if (humanlba == 0) {
                                humanlba = -1;
                            }
                        }
                    }
                    if (humanlba > 0 && lba <= humanlen / 512) {
                        if (hds_cache_read(hdsinfo[0].smb2, hdsinfo[0].sfh, humanlba + lba, buf) < 0)
                            return -1;
                        return 0;
                    }
                } else {
                    // リモートディレクトリのルートディレクトリからHUMAN.SYSを探す
                    uint64_t cur;
                    static uint32_t humanlbamax = (uint32_t)-1;
                    static struct smb2_context *smb2 = NULL;
                    static struct smb2fh *sfh = NULL;
                    if (lba <= humanlbamax && sfh == NULL) {
                        strcpy(buf, rootpath[0]);
                        strcat(buf, "/HUMAN.SYS");
                        if ((sfh = smb2_open(rootsmb2[0], buf, O_RDONLY)) == NULL) {
                            DPRINTF1("HUMAN.SYS open failure.\n");
                        } else {
                            smb2 = rootsmb2[0];
                            DPRINTF1("HUMAN.SYS opened.\n");
                        }
                    }
                    if (sfh != NULL &&
                        smb2_lseek(smb2, sfh, lba * 512, SEEK_SET, &cur) >= 0) {
                        if (smb2_read(smb2, sfh, buf, 512) != 512) {
                            smb2_close(smb2, sfh);
                            sfh = NULL;
                            humanlbamax = lba;
                            DPRINTF1("HUMAN.SYS closed.\n");
                        }
                        return 0;
                    }
                }
                memset(buf, 0, 512);
                return 0;
            } else if (lba >= (0x20000 / 512)) {
                // settingui
                lba -= 0x20000 / 512;
                size_t size = sizeof(settingui);
                if (lba <= size / 512) {
                    size_t remain = size - lba * 512;
                    memcpy(buf, &settingui[lba * 512], remain >= 512 ? 512 : remain);
                }
            }
        }
    }

    return -1;
}

static int configtxtlen = 0;

int vd_write_block(uint32_t lba, uint8_t *buf)
{
    if (lba == 0x4020) {
        // Root directory
        if (memcmp(&buf[32], "CONFIG  TXT", 11) == 0) {
            configtxtlen = *(uint32_t *)&buf[60];
        }
        return 0;
    }

    if (lba >= 0x4120 && lba < 0x4124) {
        // "config.txt" file update
        memcpy(&configtxt[(lba - 0x4120) * SECTOR_SIZE], buf, SECTOR_SIZE);
        if (configtxtlen > 0 &&
            (lba - 0x4120) == (configtxtlen - 1) / SECTOR_SIZE) {
                configtxt[configtxtlen] = '\0';
                configtxt[sizeof(configtxt) - 1] = '\0';
            config_parse(configtxt);
            config_write();

            // reboot by watchdog
            watchdog_enable(500, 1);
            while (1)
                ;
        }
    }

    if (lba >= 0x00803fa0) {
        // "disk0～6.hds" file write
        lba -= 0x00803fa0;
        int id = lba / 0x800000;
        lba %= 0x800000;
        struct diskinfo *di = &diskinfo[id];
        if (di->type == DTYPE_NOTUSED)
            return -1;
        if (lba >= (DISKSIZE(di) + SECTOR_SIZE - 1) / SECTOR_SIZE)
            return -1;
        DPRINTF3("disk %d: write 0x%x\n", id, lba);

        vd_sync();

        if (di->hds != NULL && di->hds->sfh) {
            if (hds_cache_write(di->hds->smb2, di->hds->sfh, lba, buf) < 0)
                return -1;
            return 0;
        }
    }

    return -1;
}
