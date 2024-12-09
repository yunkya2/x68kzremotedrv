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
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <setjmp.h>
#include <x68k/iocs.h>
#include <x68k/dos.h>

#include <zusb.h>

#include <config.h>
#include <vd_command.h>
#include <x68kremote.h>
#include <remotedrv.h>

#include "zusbcomm.h"

//****************************************************************************
// Constants
//****************************************************************************

#define PROGNAME  "zremote"

//****************************************************************************
// Global variables
//****************************************************************************

#ifdef DEBUG
int debuglevel = 0;
#endif

struct zusb_rmtdata *zusb_rmtdata;
jmp_buf jenv;                       //タイムアウト時のジャンプ先

struct config_data config_data;

//****************************************************************************
// for debugging
//****************************************************************************

#ifdef DEBUG
#define DPRINTF(...)  printf(__VA_ARGS__)
#else
#define DPRINTF(...)
#endif

//****************************************************************************
// Communication
//****************************************************************************

static void com_cmdonly(void *wbuf, size_t wsize)
{
  while (1) {
    *(uint32_t *)zusbbuf = wsize;
    memcpy(zusbbuf + 4, wbuf, wsize);

    zusb_set_ep_region(1, zusbbuf, wsize + 4);

    zusb->stat = 0xffff;
    zusb_send_cmd(ZUSB_CMD_SUBMITXFER(1));

    uint16_t stat;
    do {
      stat = zusb->stat;
      if (stat & ZUSB_STAT_ERROR) {
        break;
      }
    } while ((stat & (ZUSB_STAT_PCOMPLETE(1))) !=
                     (ZUSB_STAT_PCOMPLETE(1)));

    if (stat & ZUSB_STAT_ERROR) {
      int err = zusb->err & 0xff;
      if (err == ZUSB_ENOTCONN || err == ZUSB_ENODEV) {
        zusb_disconnect_device();
        if (connect_device() > 0) {
          continue;
        } else {
          zusb_send_cmd(ZUSB_CMD_CANCELXFER(0));
          zusb_send_cmd(ZUSB_CMD_CANCELXFER(1));
        }
        longjmp(jenv, -1);
      }
    }

    return;
  }
}

//****************************************************************************
// Utility routine
//****************************************************************************

static void terminate(int code)
{
  zusb_disconnect_device();
  if (zusb_rmtdata == NULL) {
    zusb_close();
  }
  exit(code);
}

static int getdbpunit(int drive, uint8_t *drvname)
{
  struct dos_dpbptr dpb;
  if (_dos_getdpb(drive, &dpb) < 0) {
    return -1;
  }
  char *p = (char *)dpb.driver + 14;
  if (memcmp(p, drvname, 8) != 0) {
    return -1;
  }
  return dpb.unit;
}

static int getpasswd(const char *prompt, char *passwd, int len)
{
  int res = -1;
  int count = 0;
  int hide = 1;
  char *p = passwd;
  _iocs_b_print(prompt);

  while (1) {
    int k = _iocs_b_keyinp();
    int c = k & 0xff;
    if (c == '\r') {                          // CR
      *p = '\0';
      res = 0;
      break;
    } else if (c == '\x1b' || c == '\x03') {  // ESC or CTRL+C
      break;
    } else if (c == '\t') {                   // TAB
      hide = 1 - hide;
      for (int i = 0; i < count; i++) {
        _iocs_b_print("\b \b");
      }
      for (int i = 0; i < count; i++) {
        _iocs_b_putc(hide ? '*' : passwd[i]);
      }
    } else if (c == '\x17' || c == '\x15' || k == 0x3f00) { // CTRL+W or CTRL+U or CLR
      for (int i = 0; i < count; i++) {
        _iocs_b_print("\b \b");
      }
      count = 0;
      p = passwd;
    } else if (c == '\b') {                   // BS
      if (count > 0) {
        _iocs_b_print("\b \b");
        count--;
        p--;
      }
    } else if (c >= ' ' && c < 0x7f) {
      if (count < len - 1) {
        *p++ = c;
        _iocs_b_putc(hide ? '*' : c);
        count++;
      }
    }
  }

  _iocs_b_print("\r\n");
  return res;
}

//****************************************************************************
// zremote config
//****************************************************************************

void cmd_config_usage(void)
{
  printf(
    "Usage:\t" PROGNAME " config [スイッチ]\n"
  );
  terminate(1);
}

int cmd_config_show(void)
{
  printf("TZ=%s\n", config_data.tz);
  printf("tadjust=%s\n", config_data.tadjust);
  printf("fastconnect=%s\n", config_data.fastconnect);
}

void cmd_config(int argc, char **argv)
{
  if (argc == 1) {
    cmd_config_show();
    return;
  }

  cmd_config_usage();
}

//****************************************************************************
// zremote wifi
//****************************************************************************

void cmd_wifi_usage(void)
{
  printf(
    "Usage:\t" PROGNAME " wifi [オプション]\n"
    "WiFiアクセスポイントへの接続設定を行います\n"
    "\t" PROGNAME " wifi\n"
    "\t\t現在のWiFiアクセスポイント接続状態を表示します\n"
    "\t" PROGNAME " wifi <SSID> [<オプション>]\n"
    "\t\tWiFiアクセスポイントへ接続します\n"
    "\t\t-p <パスワード>\t\t接続時のパスワードを平文で指定します\n"
    "\t" PROGNAME " wifi -l\n"
    "\t\t接続可能なWiFiアクセスポイントのリストを表示します\n"
  );
  terminate(1);
}

int cmd_wifi_show(void)
{
  struct cmd_getstatus rcmd;
  struct res_getstatus rres;
  rcmd.command = CMD_GETSTATUS;
  com_cmdres(&rcmd, sizeof(rcmd), &rres, sizeof(rres));

  printf("status=%d\n", rres.status);
  printf("SSID=%-32s\n", config_data.wifi_ssid);
}

void cmd_wifi(int argc, char **argv)
{
  if (--argc <= 0) {
    cmd_wifi_show();
    return;
  }
  argv++;

  if (strcmp(*argv, "-l") == 0) {
    struct cmd_wifi_scan rcmd;
    struct res_wifi_scan rres;
    rcmd.command = CMD_WIFI_SCAN;
    rcmd.clear = 1;
    com_cmdres(&rcmd, sizeof(rcmd), &rres, sizeof(rres));

    int timeout = 100 * 5;
    struct iocs_time tm = _iocs_ontime();
    while (_iocs_b_keysns() == 0) {
      struct iocs_time tm2 = _iocs_ontime();
      if ((timeout >= 0) && (tm2.sec - tm.sec > timeout)) {
        break;
      }
    }

    rcmd.command = CMD_WIFI_SCAN;
    rcmd.clear = 0;
    com_cmdres(&rcmd, sizeof(rcmd), &rres, sizeof(rres));

    for (int i = 0; i < rres.n_items; i++) {
      printf("%s\n", rres.ssid[i]);
    }
    return;
  } else if (argv[0][0] == '-') {
    cmd_wifi_usage();
  }

  struct cmd_wifi_config rcmd;
  struct res_wifi_config rres;
  rcmd.command = CMD_WIFI_CONFIG;
  strncpy(rcmd.wifi_ssid, *argv, sizeof(rcmd.wifi_ssid));
  rcmd.wifi_ssid[sizeof(rcmd.wifi_ssid) - 1] = '\0';
  argv++;

  if (--argc > 0) {
    if (strcmp(*argv, "-p") == 0) {
      if (--argc <= 0) {
        cmd_wifi_usage();
      }
      argv++;
      strncpy(rcmd.wifi_passwd, *argv, sizeof(rcmd.wifi_passwd));
      rcmd.wifi_passwd[sizeof(rcmd.wifi_passwd) - 1] = '\0';
    }
  } else {
    if (getpasswd("Password: ", rcmd.wifi_passwd, sizeof(rcmd.wifi_passwd)) < 0) {
      return;
    }
  }

  printf("ssid=%s passwd=%s\n", rcmd.wifi_ssid, rcmd.wifi_passwd);

  com_cmdres(&rcmd, sizeof(rcmd), &rres, sizeof(rres));
}

//****************************************************************************
// zremote server
//****************************************************************************

void cmd_server_usage(void)
{
  printf(
    "Usage:\t" PROGNAME " server [オプション]\n"
    "Windowsファイル共有サーバへの接続設定を行います\n"
    "\t" PROGNAME " server\n"
    "\t\t現在のWindowsファイル共有サーバ接続設定状態を表示します\n"
    "\t" PROGNAME " server <サーバ名> <ユーザ名> [<ワークグループ名>] [<オプション>]\n"
    "\t\tWindowsファイル共有サーバへ接続します\n"
    "\t\t-p <パスワード>\t\t接続時のパスワードを平文で指定します\n"
  );
  terminate(1);
}

int cmd_server_show(void)
{
  printf("Server=%-32s\n", config_data.smb2_server);
  printf("User=%-16s\n", config_data.smb2_user);
  printf("Workgroup=%-16s\n", config_data.smb2_workgroup);
}

void cmd_server(int argc, char **argv)
{
  if (--argc <= 0) {
    cmd_server_show();
    return;
  }
  argv++;

  if (argv[0][0] == '-') {
    cmd_wifi_usage();
  }

  struct cmd_smb2_config rcmd;
  struct res_smb2_config rres;
  rcmd.command = CMD_SMB2_CONFIG;
  strncpy(rcmd.smb2_server, *argv, sizeof(rcmd.smb2_server));
  rcmd.smb2_server[sizeof(rcmd.smb2_server) - 1] = '\0';
  argv++;

  if (--argc <= 0) {
    cmd_server_usage();
  }
  strncpy(rcmd.smb2_user, *argv, sizeof(rcmd.smb2_user));
  rcmd.smb2_user[sizeof(rcmd.smb2_user) - 1] = '\0';
  argv++;

  rcmd.smb2_passwd[0] = '\0';
  strcpy(rcmd.smb2_workgroup, "WORKGROUP");

  while (--argc > 0) {
    if (strcmp(*argv, "-p") == 0) {
      if (--argc <= 0) {
        cmd_wifi_usage();
      }
      argv++;
      strncpy(rcmd.smb2_passwd, *argv, sizeof(rcmd.smb2_passwd));
      rcmd.smb2_passwd[sizeof(rcmd.smb2_passwd) - 1] = '\0';
      argv++;
    } else {
      strncpy(rcmd.smb2_workgroup, *argv, sizeof(rcmd.smb2_workgroup));
      rcmd.smb2_workgroup[sizeof(rcmd.smb2_workgroup) - 1] = '\0';
      argv++;
    }
  }

  if (rcmd.smb2_passwd[0] == '\0') {
    if (getpasswd("Password: ", rcmd.smb2_passwd, sizeof(rcmd.smb2_passwd)) < 0) {
      return;
    }
  }

  printf("server=%s user=%s workgroup=%s passwd=%s\n", rcmd.smb2_server, rcmd.smb2_user, rcmd.smb2_workgroup, rcmd.smb2_passwd);

  com_cmdres(&rcmd, sizeof(rcmd), &rres, sizeof(rres));
}

//****************************************************************************
// zremote mount
//****************************************************************************

void cmd_mount_usage(void)
{
  printf(
    "Usage:\t" PROGNAME " mount [オプション]\n"
    "指定したドライブ名にリモートディレクトリをマウントします\n"
    "\t" PROGNAME " mount\n"
    "\t\t現在のリモートドライブ設定状態を表示します\n"
    "\t" PROGNAME " mount ドライブ名:\n"
    "\t\t指定したドライブのリモートドライブ設定状態を表示します\n"
    "\t" PROGNAME " mount ドライブ名: リモートパス\n"
    "\t\t指定したドライブにリモートパスを接続します\n"
    "\t" PROGNAME " mount -D ドライブ名:\n"
    "\t" PROGNAME " umount ドライブ名:\n"
    "\t\t指定のドライブのリモートドライブ設定を解除します\n"
    "\t" PROGNAME " mount -n ドライブ数\n"
    "\t\tリモートドライブの数を設定します (0-8)\n"
    "\t" PROGNAME " mount -l\n"
    "\t\t接続可能なWindowsファイル共有のリストを表示します\n"
  );
  terminate(1);
}

void cmd_mount_show(void)
{
  for (int drive = 1; drive <= 26; drive++) {
    int unit = getdbpunit(drive, "\x01ZUSBRMT");
    if (unit < 0) {
      continue;
    }
    printf("%c: %s\n", 'A' + drive - 1, config_data.remote[unit]);
  }
}

void cmd_mount(int argc, char **argv)
{
  int umount = 0;

  if (--argc <= 0) {
    cmd_mount_show();
    return;
  }
  argv++;

  if (strcmp(*argv, "-D") == 0) {
    umount = 1;
    argc--;
    argv++;
  } else if (strcmp(*argv, "-n") == 0) {
    if (--argc <= 0) {
      cmd_mount_usage();
    }
    argv++;
    int n = atoi(*argv);
    if (n < 0 || n > 8) {
      cmd_mount_usage();
    }
    printf("set remoteunit=%d\n", n);
    return;
  } else if (strcmp(*argv, "-l") == 0) {
    struct cmd_smb2_enum rcmd;
    struct res_smb2_enum rres;
    rcmd.command = CMD_SMB2_ENUM;
    com_cmdres(&rcmd, sizeof(rcmd), &rres, sizeof(rres));
    if (rres.status == VDERR_OK) {
      for (int i = 0; i < rres.n_items; i++) {
        printf("%-64s\n", rres.share[i]);
      }
      return;
    } else {
      printf(PROGNAME ": ファイル共有リストの取得に失敗しました\n");
      terminate(1);
    }
  }
  
  int drive = (*argv)[0] & 0xdf;
  if (!(strlen(*argv) == 2 &&
        drive >= 'A' && drive <= 'Z' &&
        (*argv)[1] == ':')) {
    cmd_mount_usage();
  }

  int unit = getdbpunit(drive - 'A' + 1, "\x01ZUSBRMT");
  if (unit < 0) {
    printf(PROGNAME ": ドライブ%c:はリモートドライブではありません\n", drive);
    terminate(1);
  }

  if (!umount && --argc <= 0) {
    printf("%c: %s\n", drive, config_data.remote[unit]);
  } else {
    argv++;
    struct cmd_setrmtdrv rcmd;
    struct res_setrmtdrv rres;
    rcmd.command = CMD_SETRMTDRV;
    rcmd.unit = unit;
    if (umount) {
      rcmd.path[0] = '\0';
    } else {
      strncpy(rcmd.path, *argv, sizeof(rcmd.path) - 1);
      rcmd.path[sizeof(rcmd.path) - 1] = '\0';
    }
    com_cmdres(&rcmd, sizeof(rcmd), &rres, sizeof(rres));

    if (rres.status != 0) {
      printf(PROGNAME ": ドライブ%c:のマウントに失敗しました\n", drive);
      terminate(1);
    }
  }
}

//****************************************************************************
// zremote hds
//****************************************************************************

void cmd_hds_usage(void)
{
  printf(
    "Usage:\t" PROGNAME " hds [オプション]\n"
    "指定したドライブ名にリモートHDSを接続します\n"
    "\t" PROGNAME " hds\n"
    "\t\t現在のリモートHDS設定状態を表示します\n"
    "\t" PROGNAME " hds ドライブ名:\n"
    "\t\t指定したドライブのリモートHDS設定状態を表示します\n"
    "\t" PROGNAME " hds ドライブ名: リモートパス\n"
    "\t\t指定したドライブにリモートHDSを接続します\n"
    "\t" PROGNAME " hds -D ドライブ名:\n"
    "\t" PROGNAME " umount ドライブ名:\n"
    "\t\t指定のドライブのリモートHDS設定を解除します\n"
    "\t" PROGNAME " hds -n ドライブ数\n"
    "\t\tリモートHDSの数を設定します (0-4)\n"
    "\t" PROGNAME " hds -l\n"
    "\t\t接続可能なWindowsファイル共有のリストを表示します\n"
  );
  terminate(1);
}

void cmd_hds_show(void)
{
  for (int drive = 1; drive <= 26; drive++) {
    int unit = getdbpunit(drive, "\x01ZUSBHDS");
    if (unit < 0) {
      continue;
    }
    printf("%c: %s\n", 'A' + drive - 1, config_data.hds[unit]);
  }
}

void cmd_hds(int argc, char **argv)
{
  int umount = 0;

  if (--argc <= 0) {
    cmd_hds_show();
    return;
  }
  argv++;

  if (strcmp(*argv, "-D") == 0) {
    umount = 1;
    argc--;
    argv++;
  } else if (strcmp(*argv, "-n") == 0) {
    if (--argc <= 0) {
      cmd_hds_usage();
    }
    argv++;
    int n = atoi(*argv);
    if (n < 0 || n > 4) {
      cmd_hds_usage();
    }
    printf("set remotehds=%d\n", n);
    return;
  } else if (strcmp(*argv, "-l") == 0) {
    struct cmd_smb2_enum rcmd;
    struct res_smb2_enum rres;
    rcmd.command = CMD_SMB2_ENUM;
    com_cmdres(&rcmd, sizeof(rcmd), &rres, sizeof(rres));
    if (rres.status == VDERR_OK) {
      for (int i = 0; i < rres.n_items; i++) {
        printf("%-64s\n", rres.share[i]);
      }
      return;
    } else {
      printf(PROGNAME ": ファイル共有リストの取得に失敗しました\n");
      terminate(1);
    }
  }
  
  int drive = (*argv)[0] & 0xdf;
  if (!(strlen(*argv) == 2 &&
        drive >= 'A' && drive <= 'Z' &&
        (*argv)[1] == ':')) {
    cmd_hds_usage();
  }

  int unit = getdbpunit(drive - 'A' + 1, "\x01ZUSBHDS");
  if (unit < 0) {
    printf(PROGNAME ": ドライブ%c:はリモートHDSではありません\n", drive);
    terminate(1);
  }

  if (!umount && --argc <= 0) {
    printf("%c: %s\n", drive, config_data.hds[unit]);
  } else {
    argv++;
    struct cmd_setrmthds rcmd;
    struct res_setrmthds rres;
    rcmd.command = CMD_SETRMTHDS;
    rcmd.unit = unit;
    if (umount) {
      rcmd.path[0] = '\0';
    } else {
      strncpy(rcmd.path, *argv, sizeof(rcmd.path) - 1);
      rcmd.path[sizeof(rcmd.path) - 1] = '\0';
    }
    com_cmdres(&rcmd, sizeof(rcmd), &rres, sizeof(rres));

    if (rres.status != 0) {
      printf(PROGNAME ": ドライブ%c:への接続に失敗しました\n", drive);
      terminate(1);
    }
  }
}

//****************************************************************************
// zremote umount
//****************************************************************************

void cmd_umount_usage(void)
{
  printf(
    "Usage:\t" PROGNAME " umount ドライブ名:\n"
    "指定したドライブ名のリモートディレクトリやHDSをマウント解除します\n"
  );
  terminate(1);
}

void cmd_umount(int argc, char **argv)
{
  if (--argc <= 0) {
    cmd_umount_usage();
    return;
  }
  argv++;

  int drive = (*argv)[0] & 0xdf;
  if (!(strlen(*argv) == 2 &&
        drive >= 'A' && drive <= 'Z' &&
        (*argv)[1] == ':')) {
    cmd_umount_usage();
  }

  int res = 0;
  int unit;
  if ((unit = getdbpunit(drive - 'A' + 1, "\x01ZUSBRMT")) >= 0) {
    struct cmd_setrmtdrv rcmd;
    struct res_setrmtdrv rres;
    rcmd.command = CMD_SETRMTDRV;
    rcmd.unit = unit;
    rcmd.path[0] = '\0';
    com_cmdres(&rcmd, sizeof(rcmd), &rres, sizeof(rres));
    res = rres.status;
  } else if ((unit = getdbpunit(drive - 'A' + 1, "\x01ZUSBHDS")) >= 0) {
    struct cmd_setrmthds rcmd;
    struct res_setrmthds rres;
    rcmd.command = CMD_SETRMTHDS;
    rcmd.unit = unit;
    rcmd.path[0] = '\0';
    com_cmdres(&rcmd, sizeof(rcmd), &rres, sizeof(rres));
    res = rres.status;
  } else {
    printf(PROGNAME ": ドライブ%c:はリモートドライブ/HDSではありません\n", drive);
    terminate(1);
  }

  if (res != 0) {
    printf(PROGNAME ": ドライブ%c:のマウント解除に失敗しました\n", drive);
    terminate(1);
  }
}

//****************************************************************************
// zremote date
//****************************************************************************

void cmd_date(int argc, char **argv)
{
  struct cmd_getinfo cmd;
  struct res_getinfo res;
  cmd.command = CMD_GETINFO;
  com_cmdres(&cmd, sizeof(cmd), &res, sizeof(res));

  if (res.year > 0) {
    printf("%04d/%02d/%02d %02d:%02d:%02d\n",
           res.year, res.mon, res.day, res.hour, res.min, res.sec);

    _iocs_timeset(_iocs_timebcd((res.hour << 16) | (res.min << 8) | res.sec));
    _iocs_bindateset(_iocs_bindatebcd((res.year << 16) | (res.mon << 8) | res.day));
  }
}

//****************************************************************************
// Other func
//****************************************************************************

void show_all(void)
{
  cmd_wifi_show();
  cmd_server_show();
  printf("Remote drive:\n");
  cmd_mount_show();;
  printf("Remote HDS:\n");
  cmd_hds_show();;
  cmd_config_show();
}

void usage(void)
{
  printf(
    "使用法:" PROGNAME " <command> [引数]\n"
    "\n"
    PROGNAME " wifi\n"
    PROGNAME " server\n"
    PROGNAME " mount\n"
    PROGNAME " hds\n"
  );
  terminate(1);
}

//****************************************************************************
// Main routine
//****************************************************************************

int main(int argc, char **argv)
{
  int8_t *zusb_channels = NULL;

  _dos_super(0);

  // ZUSB デバイスをオープンする
  // 既にリモートドライブを使うドライバが存在する場合は、そのチャネルを使う
  if ((zusb_rmtdata = find_zusbrmt()) == NULL) {
    if (zusb_open() < 0) {
      printf(PROGNAME ": ZUSB デバイスが見つかりません\n");
      exit(1);
    }
  }

  if (setjmp(jenv)) {
    zusb_disconnect_device();
    zusb_close();
    printf(PROGNAME ": ZUSB デバイスが切断されました\n");
    terminate(1);
  }

  struct cmd_getconfig cmd;
  struct res_getconfig res;
  cmd.command = CMD_GETCONFIG;
  com_cmdres(&cmd, sizeof(cmd), &res, sizeof(res));
  config_data = res.data;

  if (argc < 2) {
    show_all();
    terminate(0);
  }

  static const struct {
    const char *name;
    void (*func)(int argc, char **argv);
  } cmds[] = {
    { "config", cmd_config },
    { "wifi", cmd_wifi },
    { "server", cmd_server },
    { "mount", cmd_mount },
    { "hds", cmd_hds },
    { "umount", cmd_umount },
    { "date", cmd_date },
  };

  for (int i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
    if (strcmp(argv[1], cmds[i].name) == 0) {
      cmds[i].func(argc - 1, &argv[1]);
      terminate(0);
    }
  }

  usage();
}
