/* 
 * Copyright (c) 2024,2025 Yuichi Nakamura (@yunkya2)
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
#include <time.h>
#include <fcntl.h>

#include "pico/cyw43_arch.h"
#include "pico/stdio.h"
#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "smb2.h"
#include "libsmb2.h"
#include "bsp/board_api.h"
#include "tusb.h"

#include "main.h"
#include "virtual_disk.h"
#include "config_file.h"

//****************************************************************************
// Global variables
//****************************************************************************

uint64_t boottime = 0;

volatile int sysstatus = STAT_WIFI_DISCONNECTED;

const char *rootpath[8];

struct diskinfo diskinfo[7];

//****************************************************************************
// Static variables
//****************************************************************************

//****************************************************************************
// Private functions
//****************************************************************************

static void connection(int mode)
{
    switch (mode) {
    case CONNECT_WIFI:
        printf("Connecting to WiFi...\n");

        sysstatus = STAT_WIFI_CONNECTING;

        if (strlen(config.wifi_ssid) == 0 ||
            cyw43_arch_wifi_connect_timeout_ms(config.wifi_ssid, config.wifi_passwd,
                                               CYW43_AUTH_WPA2_AES_PSK, 30000)) {
            sysstatus = STAT_WIFI_DISCONNECTED;
            printf("Failed to connect.\n");
            break;
        }

        sysstatus = STAT_WIFI_CONNECTED;

        ip4_addr_t *address = &(cyw43_state.netif[0].ip_addr);
        printf("Connected to %s as %d.%d.%d.%d as host %s\n",
               config.wifi_ssid,
               ip4_addr1_16(address), ip4_addr2_16(address), ip4_addr3_16(address), ip4_addr4_16(address),
               cyw43_state.netif[0].hostname);

        /* fall through */

    case CONNECT_SMB2:
        if (strlen(config.smb2_server) == 0) {
            printf("Failed to connect SMB2 server\n");
            break;
        }

        sysstatus = STAT_SMB2_CONNECTING;

        struct smb2_context *smb2ipc;

        if ((smb2ipc = connect_smb2("IPC$")) == NULL) {
            sysstatus = STAT_WIFI_CONNECTED;
            break;
        }

        sysstatus = STAT_SMB2_CONNECTED;

        boottime = (smb2_get_system_time(smb2ipc) / 10) - (11644473600 * 1000000) - to_us_since_boot(get_absolute_time());
        time_t tt = (time_t)((boottime + to_us_since_boot(get_absolute_time())) / 1000000);
        struct tm *tm = localtime(&tt);
        printf("Boottime UTC %04d/%02d/%02d %02d:%02d:%02d\n", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);

        disconnect_smb2(smb2ipc);

        /* fall through */

    default:
        break;
    }
}

static int vd_mount(void)
{
    uint32_t nvalue;
    struct dir_entry *dirent;
    int len;

    int remoteunit = atoi(config.remoteunit);
    int remoteboot = atoi(config.remoteboot);

    /* Set up remote drive */
    for (int i = 0; i < remoteunit; i++) {
        struct smb2_context *smb2;
        const char *shpath;
        if ((smb2 = connect_smb2_path(config.remote[i], &shpath)) == NULL)
            continue;

        struct smb2_stat_64 st;
        if (smb2_stat(smb2, shpath, &st) < 0 || st.smb2_type != SMB2_TYPE_DIRECTORY) {
            printf("%s is not directory.\n", config.remote[i]);
            continue;
        }
        rootpath[i] = config.remote[i];
        printf("REMOTE%u: %s\n", i, config.remote[i]);
    }

    int id = remoteboot ? 1 : 0;

    /* Set up remote HDS */
    for (int i = 0; i < countof(config.hds); i++, id++) {
        struct smb2_context *smb2;
        const char *shpath;
        if ((smb2 = connect_smb2_path(config.hds[i], &shpath)) == NULL)
            continue;

        struct smb2_stat_64 st;
        if (smb2_stat(smb2, shpath, &st) < 0 || st.smb2_type != SMB2_TYPE_FILE) {
            printf("File %s not found.\n", config.hds[i]);
            continue;
        }
        if ((diskinfo[id].sfh = smb2_open(smb2, shpath, O_RDWR)) == NULL) {
            printf("File %s open failure.\n", config.hds[i]);
            continue;
        }
        diskinfo[id].smb2 = smb2;
        diskinfo[id].size = st.smb2_size;
        printf("HDS%u: %s size=%lld\n", i, config.hds[i], st.smb2_size);
    }

    for (int i = 0; i < 7; i++) {
        diskinfo[i].sects = (diskinfo[i].size + SECTOR_SIZE - 1) / SECTOR_SIZE;
    }

    sysstatus = STAT_CONFIGURED;
}

//****************************************************************************
// WiFi connection task
//****************************************************************************

void connect_task(void *params)
{
    /* Set up WiFi connection */

    if (cyw43_arch_init()) {
        printf("Failed to initialize Pico W\n");
        while (1)
            taskYIELD();
    }

    cyw43_arch_enable_sta_mode();

    xSemaphoreTake(remote_sem, portMAX_DELAY);
    connection(CONNECT_WIFI);
    if (sysstatus >= STAT_SMB2_CONNECTED) {
        vd_mount();
    }
    xSemaphoreGive(remote_sem);
    xTaskNotify(main_th, 1, eSetBits);

    while (1) {
        uint32_t nvalue;

        xTaskNotifyWait(1, 0, &nvalue, portMAX_DELAY);
        if (!(nvalue & CONNECT_WAIT))
            continue;
<<<<<<< HEAD
        connection(nvalue & CONNECT_MASK);
=======
        xSemaphoreTake(remote_sem, portMAX_DELAY);
        disconnectall();
        connection(nvalue & CONNECT_MASK);
        if (sysstatus >= STAT_SMB2_CONNECTED) {
            mountall();
        }
        xSemaphoreGive(remote_sem);
    }
}

//****************************************************************************
// Remote connection keepalive task
//****************************************************************************

void keepalive_task(void *params)
{
    while (1) {
        TickType_t delay;
        xSemaphoreTake(remote_sem, portMAX_DELAY);
        if (sysstatus >= STAT_SMB2_CONNECTED) {
            keepalive_smb2_all();
            delay = pdMS_TO_TICKS(5 * 60 * 1000);
        } else {
            delay = pdMS_TO_TICKS(30 * 1000);
        }
        xSemaphoreGive(remote_sem);
        vTaskDelay(delay);
>>>>>>> 0389fd4... Add keepalive task to avoid user session deletion
    }
}
