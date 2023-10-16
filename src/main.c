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

#include <time.h>

#include "pico/cyw43_arch.h"
#include "pico/stdio.h"
#include "pico/stdlib.h"
#include "pico/stdio/driver.h"
#include "pico/time.h"
#include "hardware/watchdog.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "smb2.h"
#include "libsmb2.h"
#include "bsp/board.h"
#include "tusb.h"

#include "main.h"
#include "vd_command.h"
#include "virtual_disk.h"
#include "config_file.h"

volatile int sysstatus = STAT_WIFI_DISCONNECTED;

//****************************************************************************
// Global variables
//****************************************************************************

char log_txt[LOGSIZE];

uint64_t boottime = 0;

TaskHandle_t main_th;
TaskHandle_t connect_th;

//****************************************************************************
// for debug log
//****************************************************************************

static char *log_txtp = log_txt;

static void log_out_chars(const char *buffer, int length)
{
    int remain = 1024 - (log_txtp - log_txt);
    length = length > remain ? remain : length;
    memcpy(log_txtp, buffer, length);
    log_txtp += length;
}

static stdio_driver_t stdio_log = {
    .out_chars = log_out_chars,
};

static void log_out_init(void)
{
    memset(log_txt, ' ', sizeof(log_txt));
    stdio_set_driver_enabled(&stdio_log, true);
}

//****************************************************************************
// Connect task
//****************************************************************************

struct smb2_context *smb2ipc;

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

        if ((smb2ipc = connect_smb2("IPC$")) == NULL) {
            sysstatus = STAT_WIFI_CONNECTED;
            break;
        }

        sysstatus = STAT_SMB2_CONNECTED;

        boottime = (smb2_get_system_time(smb2ipc) / 10) - (11644473600 * 1000000) - to_us_since_boot(get_absolute_time());
        time_t tt = (time_t)((boottime + to_us_since_boot(get_absolute_time())) / 1000000);
        struct tm *tm = localtime(&tt);
        printf("Boottime UTC %04d/%02d/%02d %02d:%02d:%02d\n", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);

        /* fall through */

    default:
        break;
    }
}

static void connect_task(void *params)
{
    uint32_t nvalue;

    /* Set up WiFi connection */

    if (cyw43_arch_init()) {
        printf("Failed to initialize Pico W\n");
        while (1)
            taskYIELD();
    }

    cyw43_arch_enable_sta_mode();

    int mode = CONNECT_WIFI;
    while (1) {
        connection(mode);
        xTaskNotify(main_th, 1, eSetBits);
        while (1) {
            xTaskNotifyWait(1, 0, &nvalue, portMAX_DELAY);
            if (nvalue & CONNECT_WAIT)
                break;
            taskYIELD();
        }
        mode = nvalue & CONNECT_MASK;
    }
}

//****************************************************************************
// Main task
//****************************************************************************

static void main_task(void *params)
{
    xTaskCreate(connect_task, "ConnectThread", configMINIMAL_STACK_SIZE, NULL, 1, &connect_th);

    vd_init();

    printf("Start USB MSC device.\n");

    /* USB MSC main loop */

    tusb_init();
    while (1) {
        tud_task();
        taskYIELD();
    }
}

//****************************************************************************
// MSC device callbacks
//****************************************************************************

// Invoked when device is mounted
void tud_mount_cb(void)
{
}
// Invoked when device is unmounted
void tud_umount_cb(void)
{
}

// Invoked when usb bus is suspended
void tud_suspend_cb(bool remote_wakeup_en)
{
}
// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
}

//****************************************************************************
// main
//****************************************************************************

int main(void)
{
    board_init();
    stdio_init_all();
    log_out_init();
    config_read();

    printf("\nX68000Z Remote Drive Service (version %s)\n", GIT_REPO_VERSION);

    xTaskCreate(main_task, "MainThread", configMINIMAL_STACK_SIZE, NULL, 1, &main_th);
    vTaskStartScheduler();

    return 0;
}
