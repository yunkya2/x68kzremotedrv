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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hardware/sync.h>
#include <hardware/flash.h>

#include "main.h"
#include "vd_command.h"
#include "virtual_disk.h"
#include "config_file.h"

//****************************************************************************
// Configuration file template
//****************************************************************************

__asm__ (
    ".section .rodata\n"
    ".balign 4\n"
    ".global config_template\n"
    "config_template:\n"
    ".incbin \"config.tmpl.txt\"\n"
    ".byte 0\n"
    ".balign 4\n"
);

extern char config_template[];

//****************************************************************************
// Configuration data
//****************************************************************************

char configtxt[2048];
struct config_data config;

#define CF_HIDDEN   1
#define CF_URL      2

const struct config_item {
    const char *item;
    const char *defval;
    char *value;
    size_t valuesz;
    int flag;
} config_items[] = {
    { "WIFI_SSID:",                 "<ssid>",
      config.wifi_ssid,             sizeof(config.wifi_ssid),       0 },
    { "WIFI_PASSWORD:",             NULL,
      config.wifi_passwd,           sizeof(config.wifi_passwd),     CF_HIDDEN },

    { "SMB2_USERNAME:",             "<user>",
      config.smb2_user,             sizeof(config.smb2_user),       0 },
    { "SMB2_PASSWORD:",             NULL,
      config.smb2_passwd,           sizeof(config.smb2_passwd),     CF_HIDDEN },
    { "SMB2_WORKGROUP:",            "WORKGROUP",
      config.smb2_workgroup,        sizeof(config.smb2_workgroup),  0 },

    { "SMB2_SERVER:",               "<server>",
      config.smb2_server,           sizeof(config.smb2_server),     0 },
    { "SMB2_SHARE:",                "<share>",
      config.smb2_share,            sizeof(config.smb2_share),      0 },
    { "ID0:",                       NULL,
      config.id[0],                 sizeof(config.id[0]),           CF_URL },
    { "ID1:",                       NULL,
      config.id[1],                 sizeof(config.id[1]),           CF_URL },
    { "ID2:",                       NULL,
      config.id[2],                 sizeof(config.id[2]),           CF_URL },
    { "ID3:",                       NULL,
      config.id[3],                 sizeof(config.id[3]),           CF_URL },
    { "ID4:",                       NULL,
      config.id[4],                 sizeof(config.id[4]),           CF_URL },
    { "ID5:",                       NULL,
      config.id[5],                 sizeof(config.id[5]),           CF_URL },
    { "ID6:",                       NULL,
      config.id[6],                 sizeof(config.id[6]),           CF_URL },

    { "TZ:",                        "JST-9",
      config.tz,                    sizeof(config.tz),              0 },
    { "TADJUST:",                   "2",
      config.tadjust,               sizeof(config.tadjust),         0 },
};

//****************************************************************************
// Configuration functions
//****************************************************************************

#define CONFIG_ITEMS    (sizeof(config_items) / sizeof(config_items[0]))

#define CONFIG_FLASH_OFFSET     (0x1f0000)
#define CONFIG_FLASH_ADDR       ((uint8_t *)(0x10000000 + CONFIG_FLASH_OFFSET))
#define CONFIG_FLASH_MAGIC      "X68000Z Remote Drive Config v2"
#define CONFIG_FLASH_MAGIC_v1   "X68000Z Remote Drive Config v1"

void config_read(void)
{
    int i;
    for (i = 0; i < CONFIG_ITEMS; i++) {
        const struct config_item *c = &config_items[i];
        memset(c->value, 0, c->valuesz);
    }

    const uint8_t *config_flash_addr = CONFIG_FLASH_ADDR;
    const char *p = &config_flash_addr[32];
    if (memcmp(&config_flash_addr[0], CONFIG_FLASH_MAGIC, sizeof(CONFIG_FLASH_MAGIC)) == 0) {
        for (i = 0; i < CONFIG_ITEMS; i++) {
            const struct config_item *c = &config_items[i];
            memcpy(c->value, p, c->valuesz);
            p += c->valuesz;
        }
    } else if (memcmp(&config_flash_addr[0], CONFIG_FLASH_MAGIC_v1, sizeof(CONFIG_FLASH_MAGIC_v1)) == 0) {
        for (i = 0; i < CONFIG_ITEMS - 1; i++) {
            const struct config_item *c = &config_items[i];
            memcpy(c->value, p, c->valuesz);
            p += c->valuesz;
        }
        for (; i < CONFIG_ITEMS; i++) {
            const struct config_item *c = &config_items[i];
            if (c->defval)
                strcpy(c->value, c->defval);
        }
    } else {
        for (i = 0; i < CONFIG_ITEMS; i++) {
            const struct config_item *c = &config_items[i];
            if (c->defval)
                strcpy(c->value, c->defval);
        }
    }

    for (i = 0; i < 7; i++) {
        for (char *p = config.id[i]; *p != '\0'; p++) {
            if (*p == '/')
                *p = '\\';
        }
    }

    memset(configtxt, 0, sizeof(configtxt));
    snprintf(configtxt, sizeof(configtxt) - 1 , config_template,
             config.wifi_ssid,
             config.smb2_user, config.smb2_workgroup,
             config.smb2_server, config.smb2_share,
             config.id[0],
             config.id[1],
             config.id[2],
             config.id[3],
             config.id[4],
             config.id[5],
             config.id[6],
             config.tz,
             config.tadjust);

    for (i = 0; i < 7; i++) {
        for (char *p = config.id[i]; *p != '\0'; p++) {
            if (*p == '\\')
                *p = '/';
        }
    }
}

void config_write(void)
{
    uint8_t flash_data[SECTOR_SIZE * 4];
    memcpy(&flash_data[0], CONFIG_FLASH_MAGIC, sizeof(CONFIG_FLASH_MAGIC));
    char *p = &flash_data[32];
    for (int i = 0; i < CONFIG_ITEMS; i++) {
        const struct config_item *c = &config_items[i];
        memcpy(p, c->value, c->valuesz);
        p += c->valuesz;
    }

    uint32_t stat = save_and_disable_interrupts();
    flash_range_erase(CONFIG_FLASH_OFFSET, FLASH_SECTOR_SIZE * 4);
    flash_range_program(CONFIG_FLASH_OFFSET, flash_data, sizeof(flash_data));
    restore_interrupts(stat);
}

void config_erase(void)
{
    uint32_t stat = save_and_disable_interrupts();
    flash_range_erase(CONFIG_FLASH_OFFSET, FLASH_SECTOR_SIZE * 4);
    restore_interrupts(stat);
}

void config_parse(uint8_t *buf)
{
    char *p = buf;
    while (*p != '\0') {
        int i;
        if (*p < ' ') {
            p++;
            continue;
        }
        for (i = 0; i < CONFIG_ITEMS; i++) {
            const struct config_item *c = &config_items[i];
            if (strncmp(p, c->item, strlen(c->item)) == 0) {
                p += strlen(c->item);
                while (*p == ' ')
                    p++;
                char *q = c->value;
                if (c->flag & CF_HIDDEN) {
                    char *r = p;
                    while (*r == '*')
                        r++;
                    if (*r < ' ')
                        continue;
                }
                for (int j = 0; j < c->valuesz - 1; j++) {
                    if (*p < ' ')
                        break;
                    if (c->flag & CF_URL) {
                        char c = *p++;
                        if (c == '"')
                            continue;
                        *q++ = (c == '\\') ? '/' : c;
                    } else {
                        *q++ = *p++;
                    }
                }
                *q = '\0';
                p++;
                break;
            }
        }
        if (i >= CONFIG_ITEMS) {
            while (*p >= ' ')
                p++;
            while (*p < ' ' && *p != '\0')
                p++;
        }
    }
}
