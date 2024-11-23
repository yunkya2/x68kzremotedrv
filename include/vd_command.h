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

#ifndef _VD_COMMAND_H_
#define _VD_COMMAND_H_

#include <stdint.h>

int vd_command(uint8_t *cbuf, uint8_t *rbuf);

/* configuration data structure */

struct config_data {
    char wifi_ssid[32];
    char wifi_passwd[16];

    char smb2_user[16];
    char smb2_passwd[16];
    char smb2_workgroup[16];
    char smb2_server[32];

    char remoteboot[4];
    char remoteunit[4];
    char remote[8][128];
    char hds[4][128];

    char tz[16];
    char tadjust[4];
    char fastconnect[4];
};

/* remote communication protocol definition */

#define PROTO_VERSION   1

#define CMD_GETINFO     0xff00
#define CMD_GETCONFIG   0xff01
#define CMD_SETCONFIG   0xff02
#define CMD_GETSTATUS   0xff03
#define CMD_WIFI_SCAN   0xff04
#define CMD_SMB2_ENUM   0xff05
#define CMD_SMB2_LIST   0xff06
#define CMD_FLASHCONFIG 0xff07
#define CMD_FLASHCLEAR  0xff08
#define CMD_REBOOT      0xff09

#define STAT_WIFI_DISCONNECTED      0
#define STAT_WIFI_CONNECTING        1
#define STAT_WIFI_CONNECTED         2
#define STAT_SMB2_CONNECTING        3
#define STAT_SMB2_CONNECTED         4
#define STAT_SMB2_CONNECTED_SAFE    5
#define STAT_CONFIGURED             6

#define CONNECT_WIFI                0
#define CONNECT_WIFI_FAST           1
#define CONNECT_SMB2                2
#define CONNECT_NONE                3
#define CONNECT_MASK                0x0f
#define CONNECT_WAIT                0x10

struct cmd_getinfo {
    uint16_t command;
};
struct res_getinfo {
    uint16_t year;
    uint8_t mon;
    uint8_t day;
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
    uint8_t unit;
    uint8_t version;
    uint8_t verstr[16];
};

struct cmd_getconfig {
    uint16_t command;
};
struct res_getconfig {
    struct config_data data;
};

struct cmd_setconfig {
    uint16_t command;
    uint8_t mode;
    struct config_data data;
};
struct res_setconfig {
    uint8_t status;
};

struct cmd_getstatus {
    uint16_t command;
};
struct res_getstatus {
    uint8_t status;
};

struct cmd_wifi_scan {
    uint16_t command;
    uint8_t clear;
};
struct res_wifi_scan {
    uint8_t status;
    uint8_t n_items;
    uint8_t ssid[16][32];
};

struct cmd_smb2_enum {
    uint16_t command;
};
struct res_smb2_enum {
    uint8_t status;
    uint8_t n_items;
    uint8_t share[16][64];
};

struct cmd_smb2_list {
    uint16_t command;
    uint8_t share[64];
    uint8_t path[256];
};
struct res_smb2_list {
    uint8_t status;
    uint8_t list[1024];
};

struct cmd_flashconfig {
    uint16_t command;
};
struct res_flashconfig {
    uint8_t status;
};

struct cmd_flashclear {
    uint16_t command;
};
struct res_flashclear {
    uint8_t status;
};

struct cmd_reboot {
    uint16_t command;
};
struct res_reboot {
    uint8_t status;
};

#define countof(array)      (sizeof(array) / sizeof(array[0]))

#endif  /* _VD_COMMAND_H_ */
