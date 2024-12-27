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
 *
 */

#include <stdint.h>
#include <time.h>
#include <fcntl.h>
#include <malloc.h>
#include <unistd.h>

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

#include "config.h"
#include "main.h"
#include "virtual_disk.h"
#include "config_file.h"

//****************************************************************************
// Global variables
//****************************************************************************

uint64_t boottime = 0;

volatile int sysstatus = STAT_WIFI_DISCONNECTED;

const char *rootpath[N_REMOTE];
struct smb2_context *rootsmb2[N_REMOTE];
struct hdsinfo hdsinfo[N_HDS];

//****************************************************************************
// Static variables
//****************************************************************************

//****************************************************************************
// Remote drive and HDS
//****************************************************************************

static int remote_disconnect(int unit)
{
    if (unit < 0 || unit >= N_REMOTE) {
        return VDERR_EINVAL;
    }
    if (!rootsmb2[unit]) {
        return 0;
    }

    void op_closeall(int unit);
    op_closeall(unit);
    disconnect_smb2_smb2(rootsmb2[unit]);
    rootsmb2[unit] = NULL;
    rootpath[unit] = NULL;
    return 0;
}

static int remote_umount(int unit)
{
    int res = remote_disconnect(unit);
    if (res == 0) {
        strcpy(config.remote[unit], "");
    }
    return res;
}

int remote_mount(int unit, const char *path)
{
    if (unit < 0 || unit >= N_REMOTE) {
        return VDERR_EINVAL;
    }
    if (rootsmb2[unit]) {
        remote_umount(unit);
    }
    if (strlen(path) == 0) {        // umount drive
        return VDERR_OK;
    }

    struct smb2_context *smb2;
    const char *shpath;
    if ((smb2 = connect_smb2_path(path, &shpath)) == NULL) {
        return VDERR_ENOENT;
    }
    struct smb2_stat_64 st;
    if (smb2_stat(smb2, shpath, &st) < 0 || st.smb2_type != SMB2_TYPE_DIRECTORY) {
        printf("%s is not directory.\n", path);
        return VDERR_ENOENT;
    }
    strcpy(config.remote[unit], path);
    path2smb2(config.remote[unit], &shpath);
    rootsmb2[unit] = smb2;
    rootpath[unit] = shpath;
    printf("REMOTE%u: %s %s\n", unit, config.remote[unit], shpath);
    return VDERR_OK;
}

static int hds_disconnect(int unit)
{
    if (unit < 0 || unit >= N_HDS) {
        return VDERR_EINVAL;
    }
    if (!hdsinfo[unit].smb2) {
        return 0;
    }

    smb2_close(hdsinfo[unit].smb2, hdsinfo[unit].sfh);
    disconnect_smb2_smb2(hdsinfo[unit].smb2);
    hdsinfo[unit].smb2 = NULL;
    hdsinfo[unit].sfh = NULL;
    return 0;
}

static int hds_umount(int unit)
{
    int res = hds_disconnect(unit);
    if (res == 0) {
        strcpy(config.hds[unit], "");
    }
    return res;
}

int hds_mount(int unit, const char *path)
{
    int len;

    if (unit < 0 || unit >= N_HDS) {
        return VDERR_EINVAL;
    }
    if (hdsinfo[unit].smb2) {
        hds_umount(unit);
    }
    if ((len = strlen(path)) == 0) {        // umount HDS
        return VDERR_OK;
    }

    struct smb2_context *smb2;
    const char *shpath;
    if ((smb2 = connect_smb2_path(path, &shpath)) == NULL) {
        return VDERR_ENOENT;
    }
    struct smb2_stat_64 st;
    if (smb2_stat(smb2, shpath, &st) < 0 || st.smb2_type != SMB2_TYPE_FILE) {
        printf("File %s not found.\n", path);
        return VDERR_ENOENT;
    }
    if ((hdsinfo[unit].sfh = smb2_open(smb2, shpath, O_RDWR)) == NULL) {
        if ((hdsinfo[unit].sfh = smb2_open(smb2, shpath, O_RDONLY)) == NULL) {
            printf("File %s open failure.\n", path);
            return VDERR_EIO;
        }
        hdsinfo[unit].type = 1;
    } else {
        hdsinfo[unit].type = 0;
    }

    if (len > 4 &&
        path[len - 4] == '.' &&
        (path[len - 3] & 0xdf) == 'M' &&
        (path[len - 2] & 0xdf) == 'O' &&
        (path[len - 1] & 0xdf) == 'S') {
        hdsinfo[unit].type |= 0x80;
    }

    strcpy(config.hds[unit], path);
    hdsinfo[unit].smb2 = smb2;
    hdsinfo[unit].size = st.smb2_size;
    printf("HDS%u: %s size=%lld type=0x%02x\n", unit, config.hds[unit], st.smb2_size, hdsinfo[unit].type);
    return VDERR_OK;
}

static int mountall(void)
{
    /* Set up remote drive */
    for (int i = 0; i < N_REMOTE; i++) {
        remote_mount(i, config.remote[i]);
    }

    /* Set up remote HDS */
    for (int i = 0; i < N_HDS; i++) {
        hds_mount(i, config.hds[i]);
    }

    sysstatus = STAT_CONFIGURED;
}

static int disconnectall(void)
{
    /* Unmount remote drive */
    for (int i = 0; i < N_REMOTE; i++) {
        remote_disconnect(i);
    }

    /* Unmount remote HDS */
    for (int i = 0; i < N_HDS; i++) {
        hds_disconnect(i);
    }

    sysstatus = STAT_SMB2_CONNECTED;
}

//****************************************************************************
// WiFi and SMB2 connection
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
        mountall();
    }
    xSemaphoreGive(remote_sem);
    xTaskNotify(main_th, 1, eSetBits);

    while (1) {
        uint32_t nvalue;

        xTaskNotifyWait(1, 0, &nvalue, portMAX_DELAY);
        if (!(nvalue & CONNECT_WAIT))
            continue;
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
#ifdef DEBUG
        extern char __HeapLimit;
        struct mallinfo mi = mallinfo();
        printf("arena=%d used=%d free=%d", mi.arena, mi.uordblks, mi.fordblks);
        printf(" heapfree=%d\n", &__HeapLimit - (char *)sbrk(0));

        static TaskStatus_t pxTaskStatusArray[8];
        unsigned long ulTotalRunTime;
        int uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, count_of(pxTaskStatusArray), &ulTotalRunTime);
        printf("ID Task Name        S Pr Stack\n");
        for (int x = 0; x < uxArraySize; x++) {
            printf("%2u %-16s %c %2u %5u\n",
                   pxTaskStatusArray[x].xTaskNumber,
                   pxTaskStatusArray[x].pcTaskName,
                   "RRBSD"[pxTaskStatusArray[x].eCurrentState],
                   pxTaskStatusArray[x].uxCurrentPriority,
                   pxTaskStatusArray[x].usStackHighWaterMark);
        }
#endif
    }
}
