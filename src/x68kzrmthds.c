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

#include "pico/cyw43_arch.h"
#include "pico/stdio.h"
#include "pico/stdlib.h"
#include "pico/stdio/driver.h"
#include "FreeRTOS.h"
#include "task.h"
#include "smb2.h"
#include "libsmb2.h"
#include "bsp/board.h"
#include "tusb.h"

#include "x68kzrmthds.h"
#include "virtual_disk.h"
#include "config_file.h"

//****************************************************************************
// for debug log
//****************************************************************************

char log_txt[LOGSIZE];
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
// Main task
//****************************************************************************

struct smb2_context *smb2;
struct smb2fh *sfh;
static const char *vd_path;

static void service_main(void)
{
    /* Set up WiFi connection */

    if (cyw43_arch_init()) {
        printf("Failed to initialize Pico W\n");
        return;
    }

    cyw43_arch_enable_sta_mode();
    
    tusb_init();        /* Start TinyUSB MSC */

    printf("Connecting to WiFi...\n");

    if (cyw43_arch_wifi_connect_timeout_ms(config_wifi_ssid, config_wifi_passwd,
                                           CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("Failed to connect.\n");
        return;
    }

    ip4_addr_t *address = &(cyw43_state.netif[0].ip_addr);
    printf("Connected to %s as %d.%d.%d.%d as host %s\n",
           config_wifi_ssid,
           ip4_addr1_16(address), ip4_addr2_16(address), ip4_addr3_16(address), ip4_addr4_16(address),
           cyw43_state.netif[0].hostname);

    /* Start SMB2 client */

    struct smb2_url *url;

    smb2 = smb2_init_context();
    if (smb2 == NULL) {
        printf("Failed to init SMB2 context\n");
        return;
    }

    if (strlen(config_smb2_user))
        smb2_set_user(smb2, config_smb2_user);
    if (strlen(config_smb2_passwd))
        smb2_set_password(smb2, config_smb2_passwd);

    url = smb2_parse_url(smb2, config_smb2_url);
    if (url == NULL) {
        printf("Failed to parse url: %s\n", smb2_get_error(smb2));
        return;
    }

    smb2_set_security_mode(smb2, SMB2_NEGOTIATE_SIGNING_ENABLED);

    printf("SMB2 connection server:%s share:%s\n", url->server, url->share);

    if (smb2_connect_share(smb2, url->server, url->share, url->user) < 0) {
        printf("smb2_connect_share failed. %s\n", smb2_get_error(smb2));
        return;
    }

    printf("SMB2 connection established.\n");
    vd_path = url->path;
}

static void main_task(void *params)
{
    service_main();

    printf("Start USB MSC device.\n");

    vd_init(vd_path);

    /* USB MSC main loop */

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
    TaskHandle_t task;

    board_init();
    stdio_init_all();
    log_out_init();
    config_read();

    printf("X68000Z remote HDS service\n");

    xTaskCreate(main_task, "MainThread", configMINIMAL_STACK_SIZE, NULL, 1, &task);
    vTaskStartScheduler();

    return 0;
}
