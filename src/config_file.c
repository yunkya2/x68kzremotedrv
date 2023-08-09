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

#include "x68kzrmthds.h"
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
    "config_template_end:\n"
    ".balign 4\n"
    ".global config_template_size\n"
    "config_template_size:\n"
    ".word config_template_end - config_template\n"
);

extern char config_template[];
extern int config_template_size;

//****************************************************************************
// Configuration data
//****************************************************************************

char config_wifi_ssid[32];
char config_wifi_passwd[16];
char config_smb2_url[256];
char config_smb2_user[16];
char config_smb2_passwd[16];

const struct config_item {
    const char *item;
    char *value;
    size_t valuesz;
    bool hidden;
} config_items[] = {
    { "WIFI_SSID: ",     config_wifi_ssid,   sizeof(config_wifi_ssid),   false },
    { "WIFI_PASSWORD: ", config_wifi_passwd, sizeof(config_wifi_passwd), true  },
    { "SMB2_URL: ",      config_smb2_url,    sizeof(config_smb2_url),    false },
    { "SMB2_USERNAME: ", config_smb2_user,   sizeof(config_smb2_user),   false },
    { "SMB2_PASSWORD: ", config_smb2_passwd, sizeof(config_smb2_passwd), true  },
};

char configtxt[SECTOR_SIZE];

//****************************************************************************
// Configuration functions
//****************************************************************************

#define CONFIG_ITEMS    (sizeof(config_items) / sizeof(config_items[0]))

#define CONFIG_FLASH_OFFSET     (0x1f0000)
#define CONFIG_FLASH_ADDR       ((uint8_t *)(0x10000000 + CONFIG_FLASH_OFFSET))
#define CONFIG_FLASH_MAGIC      "X68000Z Remote HDS Config"

void config_read(void)
{
    for (int i = 0; i < CONFIG_ITEMS; i++) {
        const struct config_item *c = &config_items[i];
        memset(c->value, 0, c->valuesz);
    }

    const uint8_t *config_flash_addr = CONFIG_FLASH_ADDR;
    if (memcmp(&config_flash_addr[0], CONFIG_FLASH_MAGIC, sizeof(CONFIG_FLASH_MAGIC)) != 0) {
//        strcpy(config_wifi_ssid, "<ssid>");
//        strcpy(config_smb2_url, "smb://<server>/<share>/<path>/<file>.HDS");
//        strcpy(config_smb2_user, "<user>");
    } else {
        const char *p = &config_flash_addr[32];
        for (int i = 0; i < CONFIG_ITEMS; i++) {
            const struct config_item *c = &config_items[i];
            memcpy(c->value, p, c->valuesz);
            p += c->valuesz;
        }
    }

    memset(configtxt, 0, sizeof(configtxt));
    snprintf(configtxt, sizeof(configtxt) -1 , config_template,
             config_wifi_ssid, config_smb2_url, config_smb2_user);
}

void config_write(void)
{
    uint8_t flash_data[SECTOR_SIZE];
    memcpy(&flash_data[0], CONFIG_FLASH_MAGIC, sizeof(CONFIG_FLASH_MAGIC));
    char *p = &flash_data[32];
    for (int i = 0; i < CONFIG_ITEMS; i++) {
        const struct config_item *c = &config_items[i];
        memcpy(p, c->value, c->valuesz);
        p += c->valuesz;
    }

    uint32_t stat = save_and_disable_interrupts();
    flash_range_erase(CONFIG_FLASH_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(CONFIG_FLASH_OFFSET, flash_data, sizeof(flash_data));
    restore_interrupts(stat);
}

void config_erase(void)
{
    uint32_t stat = save_and_disable_interrupts();
    flash_range_erase(CONFIG_FLASH_OFFSET, FLASH_SECTOR_SIZE);
    restore_interrupts(stat);
}

void config_parse(uint8_t *buf)
{
    char *p = buf;
    buf[SECTOR_SIZE - 1] = '\0';
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
                char *q = c->value;
                if (c->hidden) {
                    char *r = p;
                    while (*r == '*')
                        r++;
                    if (*r < ' ')
                        continue;
                }
                for (int j = 0; j < c->valuesz - 1; j++) {
                    if (*p < ' ')
                        break;
                    *q++ = *p++;
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

    printf("WIFI_SSID: %s\r\n",     config_wifi_ssid);
    printf("WIFI_PASSWORD: %s\r\n", config_wifi_passwd);
    printf("SMB2_URL: %s\r\n",      config_smb2_url);
    printf("SMB2_USERNAME: %s\r\n", config_smb2_user);
    printf("SMB2_PASSWORD: %s\r\n", config_smb2_passwd);
}
