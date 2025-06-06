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
#include "pico/cyw43_arch.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "bsp/board_api.h"
#include "tusb.h"

#include "main.h"
#include "vd_command.h"
#include "virtual_disk.h"
#include "config_file.h"

//****************************************************************************
// Global variables
//****************************************************************************

char log_txt[LOGSIZE];

TaskHandle_t main_th;
TaskHandle_t connect_th;
TaskHandle_t keepalive_th;
SemaphoreHandle_t remote_sem;

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
// Main task
//****************************************************************************

static void main_task(void *params)
{
    if (cyw43_arch_init()) {
        printf("Failed to initialize Pico W\n");
        while (1)
            taskYIELD();
    }

    cyw43_arch_enable_sta_mode();

    remote_sem = xSemaphoreCreateBinary();
    xSemaphoreGive(remote_sem);
    xTaskCreate(connect_task, "ConnectThread", 2048, NULL, 1, &connect_th);
    xTaskCreate(keepalive_task, "KeepAliveThread", 1024, NULL, 1, &keepalive_th);

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

    xTaskCreate(main_task, "MainThread", 2048, NULL, 1, &main_th);
    vTaskStartScheduler();

    return 0;
}
