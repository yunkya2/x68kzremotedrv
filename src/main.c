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

#include "pico/stdio.h"
#include "pico/stdlib.h"
#include "pico/stdio/driver.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "bsp/board_api.h"
#include "tusb.h"

#include "main.h"
#include "vd_command.h"
#include "virtual_disk.h"
#include "config_file.h"
#include "remoteserv.h"

//****************************************************************************
// Global variables
//****************************************************************************

char log_txt[LOGSIZE];

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
// Vendor task
//****************************************************************************

static uint8_t buf_read[1024 * 4];
static uint8_t buf_write[1024 * 4];
static uint8_t *buf_wptr = NULL;
static int buf_wsize = 0;

void vendor_task(void)
{
    if (tud_vendor_available()) {
        if (buf_wptr == NULL) {
            buf_wptr = buf_write;
            buf_wsize = 0;
        }
        uint32_t count = tud_vendor_read(buf_wptr, 64);
        buf_wptr += count;
        buf_wsize += count;
        uint32_t total = buf_write[0] << 24 | buf_write[1] << 16 | buf_write[2] << 8 | buf_write[3];
        if (buf_wsize >= 4 && total <= buf_wsize - 4) {
            buf_wptr = NULL;
            int rsize;
            if ((rsize = vd_command(&buf_write[4], buf_read)) < 0) {
                rsize = remote_serv(&buf_write[4], buf_read);
            }
            tud_vendor_write(buf_read, rsize);
            tud_vendor_flush();
        }
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
        vendor_task();
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
