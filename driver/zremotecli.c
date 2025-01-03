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
#include <ctype.h>
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

jmp_buf jenv;                       //タイムアウト時のジャンプ先
struct config_data config_data;

int needreboot = false;

//****************************************************************************
// Command table
//****************************************************************************

void cmd_wifi(int argc, char **argv);
void cmd_server(int argc, char **argv);
void cmd_mount(int argc, char **argv);
void cmd_umount(int argc, char **argv);
void cmd_bootmode(int argc, char **argv);
void cmd_imgscsi(int argc, char **argv);
void cmd_erase(int argc, char **argv);
void cmd_stat(int argc, char **argv);

const struct {
  const char *name;
  void (*func)(int argc, char **argv);
  const char *usage;
} cmd_table[] = {
  { "mount",    cmd_mount,    "リモートディレクトリ/イメージの接続設定" },
  { "umount",   cmd_umount,   "リモートディレクトリ/イメージの接続解除" },
  { "wifi",     cmd_wifi,     "WiFiアクセスポイントへの接続設定" },
  { "server",   cmd_server,   "Windowsファイル共有サーバへの接続設定" },
  { "bootmode", cmd_bootmode, "起動モードの設定" },
  { "imgscsi",  cmd_imgscsi,  "#リモートイメージの接続モード設定" },
  { "erase",    cmd_erase,    "保存されている設定内容の全消去" },
  { "stat",     cmd_stat,     "現在の設定内容一覧表示" },
};

struct usage_message {
  const char *cmdline;
  const char *message;
};

//****************************************************************************
// for debugging
//****************************************************************************

#ifdef DEBUG
#define DPRINTF(...)  printf(__VA_ARGS__)
#else
#define DPRINTF(...)
#endif

//****************************************************************************
// Utility routine
//****************************************************************************

static void terminate(int code)
{
  com_disconnect();
  if (needreboot) {
    printf("※設定変更を反映させるためには再起動が必要です\n");
  }
  exit(code);
}

static int getdbpunit(int drive, int ishds)
{
  struct dos_dpbptr dpb;
  if (_dos_getdpb(drive, &dpb) < 0) {
    return -1;
  }
  char *p = (char *)dpb.driver + 14;
  if (memcmp(p, ishds ? "\x01ZRMTIMG" : "\x01ZRMTDRV", 8) != 0) {
    return -1;
  }

  if (ishds) {
    // リモートHDSの場合はデバイスドライバのユニット番号をHDSユニット番号に変換する
    int unit;
    int firstdrive = 0;
    for (unit = 0; unit < N_HDS; unit++) {
      if (firstdrive <= dpb.unit &&
          dpb.unit < firstdrive + com_rmtdata->hds_parts[unit]) {
        break;
      }
      firstdrive += com_rmtdata->hds_parts[unit];
    }
    return unit;
  } else {
    return dpb.unit;
  }
}

static char *normalize_path(char *path)
{
  char c;
  char *dest = path;
  char *res = path;

  while (*path == '\\' || *path == '/') {
    path++;
  }

  while ((c = *path++) != '\0') {
    if ((c >= 0x80 && c < 0xa0) || (c >= 0xe0)) {
      *dest++ = c;
      if ((c = *path++) == '\0') {
        break;
      }
    } else if (c == '\\') {
      c = '/';
    }
    *dest++ = c;
  }
  *dest = '\0';

  return res;
}

static void init_rmtcfg(struct cmd_setrmtcfg *rcmd)
{
    rcmd->bootmode = config_data.bootmode;
    rcmd->remoteunit = config_data.remoteunit;
    rcmd->hdsscsi = config_data.hdsscsi;
    rcmd->hdsunit = config_data.hdsunit;
}

static void save_config(void)
{
  com_cmdres_init(flashconfig, CMD_FLASHCONFIG);
  com_cmdres_exec();
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

static void show_usage(char *name, struct usage_message *m, int w)
{
  int i;
  for (i = 0; i < sizeof(cmd_table) / sizeof(cmd_table[0]); i++) {
    if (strcmp(name, cmd_table[i].name) == 0) {
      break;
    }
  }
  if (i == sizeof(cmd_table) / sizeof(cmd_table[0])) {
    return;
  }

  printf(PROGNAME " %s -- %s\n使用法:\n", cmd_table[i].name, cmd_table[i].usage);

  for (; m->message != NULL; m++) {
    if (m->cmdline && m->cmdline[0] != '#') {
      printf("  " PROGNAME " %s %*s %s\n", cmd_table[i].name, -w, m->cmdline, m->message);
    } else if (m->cmdline) {
      printf("  " PROGNAME " %*s %s\n", -w, m->cmdline + 1, m->message);
    } else {
      printf("%*s %s\n", 4 + strlen(PROGNAME) + strlen(cmd_table[i].name) + w, "", m->message);
    }
  }
}

//****************************************************************************
// zremote wifi
//****************************************************************************

void cmd_wifi_usage(void)
{
  struct usage_message m[] = {
    { "",                        "WiFiアクセスポイントの接続状態を表示します" },
    { "-l",                      "接続可能なWiFiアクセスポイントのリストを表示します" },
    { "<SSID> [-p パスワード]", "WiFiアクセスポイントへ接続します" },
    { NULL, NULL }
  };
  show_usage("wifi", m, 25);
  terminate(1);
}

int cmd_wifi_stat(void)
{
  com_cmdres_init(getstatus, CMD_GETSTATUS);
  com_cmdres_exec();

  printf("[WiFi]\n");
  printf("接続状態:");
  switch (res->status) {
  case STAT_WIFI_DISCONNECTED:
    printf("未接続");
    break;
  case STAT_WIFI_CONNECTING:
    printf("接続中");
    break;
  default:
    printf("接続済");
    break;
  }
  printf("\nSSID:%s\n", config_data.wifi_ssid);
}

void cmd_wifi(int argc, char **argv)
{
  int opt_list = 0;
  const char *opt_ssid = NULL;
  const char *opt_passwd = NULL;

  int i;
  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-l") == 0) {
      opt_list = 1;
    } else if (strcmp(argv[i], "-p") == 0) {
      if (++i >= argc) {
        break;
      }
      opt_passwd = argv[i];
    } else if (argv[i][0] == '-') {
      break;
    } else if (opt_ssid == NULL) {
      opt_ssid = argv[i];
    } else {
      break;
    }
  }
  if (i < argc) {
    cmd_wifi_usage();
  }
 
  if (opt_list) {
    {
      com_cmdres_init(wifi_scan, CMD_WIFI_SCAN); 
      cmd->clear = 1;
      com_cmdres_exec();
    }

    printf("WiFiアクセスポイントを検索中です。しばらくお待ちください...");
    fflush(stdout);

    int timeout = 100 * 5;
    struct iocs_time tm = _iocs_ontime();
    while (1) {
      if (_iocs_b_keysns() > 0) {
        printf("\n中断しました\n");
        return;
      }
      struct iocs_time tm2 = _iocs_ontime();
      if (tm2.sec - tm.sec > timeout) {
        break;
      }
    }

    {
      com_cmdres_init(wifi_scan, CMD_WIFI_SCAN);
      cmd->clear = 0;
      com_cmdres_exec();

      printf("\033[2K\r");
      for (int i = 0; i < res->n_items; i++) {
        printf("%s\n", res->ssid[i]);
      }
    }
    return;
  }

  if (opt_ssid == NULL) {
    cmd_wifi_stat();
    return;
  }

  com_cmdres_init(wifi_config, CMD_WIFI_CONFIG);
  strncpy(cmd->wifi_ssid, opt_ssid, sizeof(cmd->wifi_ssid));
  cmd->wifi_ssid[sizeof(cmd->wifi_ssid) - 1] = '\0';

  if (opt_passwd) {
    strncpy(cmd->wifi_passwd, opt_passwd, sizeof(cmd->wifi_passwd));
    cmd->wifi_passwd[sizeof(cmd->wifi_passwd) - 1] = '\0';
  } else {
    if (getpasswd("Password: ", cmd->wifi_passwd, sizeof(cmd->wifi_passwd)) < 0) {
      return;
    }
  }

  com_cmdres_exec();
  save_config();

  printf("WiFiアクセスポイントへ接続中です...");
  fflush(stdout);

  while (1) {
    com_cmdres_init(getstatus, CMD_GETSTATUS);
    com_cmdres_exec();
    if (res->status == STAT_WIFI_DISCONNECTED) {
      printf("接続に失敗しました\n");
      return;
    }
    if (res->status >= STAT_WIFI_CONNECTED) {
      printf("接続しました\n");
      return;
    }

    int timeout = 50;
    struct iocs_time tm = _iocs_ontime();
    while (1) {
      struct iocs_time tm2 = _iocs_ontime();
      if (tm2.sec - tm.sec > timeout) {
        break;
      }
    }
  }
}

//****************************************************************************
// zremote server
//****************************************************************************

void cmd_server_usage(void)
{
  struct usage_message m[] = {
    { "",                         "サーバの接続状態を表示します" },
    { "-l",                       "接続中のサーバで利用可能な共有名のリストを表示します" },
    { "-s",                       "接続中のサーバとの時刻同期を行います" },
    { "-t オフセット [-z タイムゾーン文字列]", "" },
    { NULL,                       "サーバとの時刻同期設定を行います" },
    { "サーバ名 ユーザ名 [ワークグループ名] [-p パスワード]", "" },
    { NULL,                       "Windowsファイル共有サーバへ接続します" },
    { NULL, NULL }
  };
  show_usage("server", m, 25);
  terminate(1);
}

int cmd_server_stat(void)
{
  com_cmdres_init(getstatus, CMD_GETSTATUS);
  com_cmdres_exec();

  printf("[ファイル共有サーバ]\n");
  printf("接続状態:");
  switch (res->status) {
  case STAT_WIFI_DISCONNECTED:
  case STAT_WIFI_CONNECTING:
  case STAT_WIFI_CONNECTED:
    printf("未接続");
    break;
  case STAT_SMB2_CONNECTING:
    printf("接続中");
    break;
  default:
    printf("接続済");
    break;
  }

  printf("\nファイル共有サーバ:%s", config_data.smb2_server);
  printf(" ユーザ名:%s", config_data.smb2_user);
  printf(" ワークグループ:%s\n", config_data.smb2_workgroup);
  printf("時刻同期設定: %u (%s)\n", config_data.tadjust, config_data.tz);
}

void cmd_server(int argc, char **argv)
{
  int opt_list = 0;
  int opt_sync = 0;
  int opt_offset = 0;
  const char *opt_tz = NULL;
  const char *opt_server = NULL;
  const char *opt_user = NULL;
  const char *opt_workgroup = "WORKGROUP";
  const char *opt_passwd = NULL;

  int i;
  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-l") == 0) {
      opt_list = 1;
    } else if (strcmp(argv[i], "-s") == 0) {
      opt_sync = 1;
    } else if (strcmp(argv[i], "-t") == 0) {
      if (++i >= argc) {
        break;
      }
      opt_offset = atoi(argv[i]);
    } else if (strcmp(argv[i], "-z") == 0) {
      if (++i >= argc) {
        break;
      }
      opt_tz = argv[i];
    } else if (strcmp(argv[i], "-p") == 0) {
      if (++i >= argc) {
        break;
      }
      opt_passwd = argv[i];
    } else if (argv[i][0] == '-') {
      break;
    } else {
      if (opt_server == NULL) {
        opt_server = argv[i];
      } else if (opt_user == NULL) {
        opt_user = argv[i];
      } else if (opt_workgroup == NULL) {
        opt_workgroup = argv[i];
      } else {
        break;
      }
    }
  }
  if (i < argc) {
    cmd_server_usage();
  }

  if (opt_list) {
    com_cmdres_init(smb2_enum, CMD_SMB2_ENUM);
    com_cmdres_exec();
    if (res->status == VDERR_OK) {
      for (int i = 0; i < res->n_items; i++) {
        printf("%-64s\n", res->share[i]);
      }
      return;
    } else {
      printf(PROGNAME ": ファイル共有リストの取得に失敗しました\n");
      terminate(1);
    }
    return;
  }

  if (opt_sync) {
    com_cmdres_init(getinfo, CMD_GETINFO);
    com_cmdres_exec();

    if (res->year > 0) {
      printf("現在時刻: %04d/%02d/%02d %02d:%02d:%02d\n",
             res->year, res->mon, res->day, res->hour, res->min, res->sec);

      _iocs_timeset(_iocs_timebcd((res->hour << 16) | (res->min << 8) | res->sec));
      _iocs_bindateset(_iocs_bindatebcd((res->year << 16) | (res->mon << 8) | res->day));
    }
    return;
  }

  if (opt_offset != 0) {
    config_data.tadjust = opt_offset;
    if (opt_tz) {
      strncpy(config_data.tz, opt_tz, sizeof(config_data.tz));
      config_data.tz[sizeof(config_data.tz) - 1] = '\0';
    }
    save_config();
    com_cmdres_init(setconfig, CMD_SETCONFIG);
    cmd->data = config_data;
    cmd->mode = CONNECT_NONE;
    com_cmdres_exec();
    return;
  }

  if (opt_server == NULL || opt_user == NULL) {
    cmd_server_stat();
    return;
  }

  com_cmdres_init(smb2_config, CMD_SMB2_CONFIG);
  strncpy(cmd->smb2_server, opt_server, sizeof(cmd->smb2_server));
  cmd->smb2_server[sizeof(cmd->smb2_server) - 1] = '\0';
  strncpy(cmd->smb2_user, opt_user, sizeof(cmd->smb2_user));
  cmd->smb2_user[sizeof(cmd->smb2_user) - 1] = '\0';
  strncpy(cmd->smb2_workgroup, opt_workgroup, sizeof(cmd->smb2_workgroup));
  cmd->smb2_workgroup[sizeof(cmd->smb2_workgroup) - 1] = '\0';

  if (opt_passwd) {
    strncpy(cmd->smb2_passwd, opt_passwd, sizeof(cmd->smb2_passwd));
    cmd->smb2_passwd[sizeof(cmd->smb2_passwd) - 1] = '\0';
  } else {
    if (getpasswd("Password: ", cmd->smb2_passwd, sizeof(cmd->smb2_passwd)) < 0) {
      return;
    }
  }

  com_cmdres_exec();
  save_config();

  printf("ファイル共有サーバへ接続中です...");
  fflush(stdout);

  while (1) {
    com_cmdres_init(getstatus, CMD_GETSTATUS);
    com_cmdres_exec();
    if (res->status < STAT_SMB2_CONNECTING) {
      printf("接続に失敗しました\n");
      return;
    }
    if (res->status >= STAT_SMB2_CONNECTED) {
      printf("接続しました\n");
      return;
    }

    int timeout = 50;
    struct iocs_time tm = _iocs_ontime();
    while (1) {
      struct iocs_time tm2 = _iocs_ontime();
      if (tm2.sec - tm.sec > timeout) {
        break;
      }
    }
  }
}

//****************************************************************************
// zremote mount
//****************************************************************************

void cmd_mount_usage(void)
{
  struct usage_message m[] = {
    { "",                     "リモートディレクトリ/イメージの接続状態を表示します" },
    { "ドライブ名:",          "指定したドライブ名の接続状態を表示します\n" },
    { "ドライブ名: リモートパス名", "指定したドライブ名にリモートディレクトリ/イメージを接続します" },
    { "-D ドライブ名:",   "指定したドライブ名の接続を解除します" },
    { "#umount ドライブ名:", "\t\t〃" },
    { "-n ユニット数",        "リモートディレクトリのユニット数を設定します (0-8)" },
    { "-m ユニット数",        "リモートイメージのユニット数を設定します (0-4)" },
    { NULL,                   "※設定変更の反映には再起動が必要です" },
    { NULL, NULL }
  };
  show_usage("mount", m, 25);
  terminate(1);
}

void cmd_mount_stat(void)
{
  char unit2drive[N_REMOTE];

  for (int ishds = 0; ishds < 2; ishds++) {
    printf("%s\n", ishds ? "[リモートイメージ]" : "[リモートディレクトリ]");

    for (int i = 0; i < N_REMOTE; i++) {
      unit2drive[i] = '?';
    }

    for (int drive = 26; drive >= 1; drive--) {
      int unit = getdbpunit(drive, ishds);
      if (unit >= 0) {
        unit2drive[unit] = drive + 'A' - 1;
      }
    }

    for (int i = 0; i < (ishds ? config_data.hdsunit : config_data.remoteunit); i++) {
      char *s = ishds ? config_data.hds[i] : config_data.remote[i];
      printf("%c: %s\n", unit2drive[i], s);
    }
  }
}

void cmd_mount(int argc, char **argv)
{
  int opt_umount = 0;
  int opt_drives_remote = -1;
  int opt_drives_hds = -1;
  const char *opt_path = NULL;  
  int unit = -1;
  int drive;
  int ishds;

  int i;
  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-D") == 0) {
      opt_umount = 1;
    } else if (strcmp(argv[i], "-n") == 0) {
      if (++i >= argc) {
        break;
      }
      opt_drives_remote = atoi(argv[i]);
      if (opt_drives_remote < 0 || opt_drives_remote > N_REMOTE) {
        cmd_mount_usage();
      }
    } else if (strcmp(argv[i], "-m") == 0) {
      if (++i >= argc) {
        break;
      }
      opt_drives_hds = atoi(argv[i]);
      if (opt_drives_hds < 0 || opt_drives_hds > N_HDS) {
        cmd_mount_usage();
      }
    } else if (argv[i][0] == '-') {
      break;
    } else if (unit < 0) {
      drive = argv[i][0] & 0xdf;
      if (strlen(argv[i]) == 2 && drive >= 'A' && drive <= 'Z' && argv[i][1] == ':') {
        if ((unit = getdbpunit(drive - 'A' + 1, false)) >= 0) {
          ishds = false;
        } else if ((unit = getdbpunit(drive - 'A' + 1, true)) >= 0) {
          ishds = true;
        } else {
          printf(PROGNAME ": ドライブ%c:はリモートディレクトリ/イメージではありません\n", drive);
          terminate(1);
        }
      } else {
        break;
      }
    } else if (opt_path == NULL) {
      opt_path = normalize_path(argv[i]);
    } else {
      break;
    }
  }
  if (i < argc) {
    cmd_mount_usage();
  }

  if (opt_drives_remote >= 0 || opt_drives_hds >= 0 ) {
    if (unit >= 0 || opt_umount || opt_path) {
      cmd_mount_usage();
    }
    com_cmdres_init(setrmtcfg, CMD_SETRMTCFG);
    init_rmtcfg(cmd);
    if (opt_drives_remote >= 0) {
      cmd->remoteunit = opt_drives_remote;
    }
    if (opt_drives_hds >= 0) {
      cmd->hdsunit = opt_drives_hds;
    }
    com_cmdres_exec();
    save_config();
    needreboot = true;
    return;
  }

  if (unit < 0) {
    cmd_mount_stat();
    return;
  }

  if (!opt_umount && !opt_path) {
    char *s = ishds ? config_data.hds[unit] : config_data.remote[unit];
    printf("%c: %s\n", drive, s);
    return;
  } else if (opt_umount && opt_path) {
    cmd_mount_usage();
  }

  if (drive != '?' && _dos_drvctrl(9, drive - '@') < 0) {
    printf(PROGNAME ": ドライブ%c:でオープンしているファイルがあります\n", drive);
    terminate(1);
  }

  _dos_fflush();

  com_cmdres_init(setrmtdrv, ishds ? CMD_SETRMTHDS : CMD_SETRMTDRV);
  cmd->unit = unit;
  if (opt_umount) {
    cmd->path[0] = '\0';
  } else {
    strncpy(cmd->path, opt_path, sizeof(cmd->path) - 1);
    cmd->path[sizeof(cmd->path) - 1] = '\0';
  }
  com_cmdres_exec();
  if (res->status != 0) {
    printf(PROGNAME ": ドライブ%c:のマウントに失敗しました\n", drive);
    terminate(1);
  }
  save_config();

  if (ishds && com_rmtdata) {
    com_rmtdata->hds_changed |= (1 << unit);
    com_rmtdata->hds_ready &= ~(1 << unit);
    if (!opt_umount) {
      com_rmtdata->hds_ready |= (1 << unit);
    }
  }
}

//****************************************************************************
// zremote umount
//****************************************************************************

void cmd_umount_usage(void)
{
  struct usage_message m[] = {
    { "ドライブ名:", "指定したドライブ名の接続を解除します" },
    { NULL, NULL }
  };
  show_usage("umount", m, 20);
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

  int unit;
  int ishds;

  if ((unit = getdbpunit(drive - 'A' + 1, false)) >= 0) {
    ishds = false;
  } else if ((unit = getdbpunit(drive - 'A' + 1, true)) >= 0) {
    ishds = true;
  } else {
    printf(PROGNAME ": ドライブ%c:はリモートディレクトリ/イメージではありません\n", drive);
    terminate(1);
  }

  com_cmdres_init(setrmtdrv, ishds ? CMD_SETRMTHDS : CMD_SETRMTDRV);
  cmd->unit = unit;
  cmd->path[0] = '\0';
  com_cmdres_exec();
  if (res->status != 0) {
    printf(PROGNAME ": ドライブ%c:のマウント解除に失敗しました\n", drive);
    terminate(1);
  }
  save_config();

  if (ishds && com_rmtdata) {
    com_rmtdata->hds_changed |= (1 << unit);
    com_rmtdata->hds_ready &= ~(1 << unit);
  }
}

//****************************************************************************
// zremote bootmode
// zremote hdsscsi
//****************************************************************************

void cmd_bootmode_usage(void)
{
  struct usage_message m[] = {
    { "",     "現在の設定状態を表示します" },
    { "0",    "リモートディレクトリから起動します" },
    { "1",    "リモートイメージから起動します" },
    { "2",    "他のUSBメモリから起動します" },
    { NULL,   "※設定変更の反映には再起動が必要です" },
    { NULL, NULL }
  };
  show_usage("bootmode", m, 16);
  terminate(1);
}

void cmd_bootmode_stat(void)
{
  printf("[起動モード]\n");
  switch (config_data.bootmode) {
  case 0:
    printf("リモートディレクトリから起動");
    break;
  case 1:
    printf("リモートイメージから起動");
    break;
  case 2:
    printf("USBメモリから起動");
    break;
  }
  printf("\n");
}

void cmd_bootmode(int argc, char **argv)
{
  int mode = -1;

  if (argc <= 1) {
    cmd_bootmode_stat();
    return;
  }

  if (strlen(argv[1]) == 1 && argv[1][0] >= '0' && argv[1][0] >= '1') {
    mode = argv[1][0] - '0';
  } else {
    cmd_bootmode_usage();
  }

  com_cmdres_init(setrmtcfg, CMD_SETRMTCFG);
  init_rmtcfg(cmd);
  cmd->bootmode = mode;
  com_cmdres_exec();
  save_config();
  needreboot = true;
}

void cmd_imgscsi_usage(void)
{
  struct usage_message m[] = {
    { "",     "現在の設定状態を表示します" },
    { "on",   "リモートイメージを純正SCSIドライバで使用します" },
    { "off",  "リモートイメージをリモートイメージドライバで使用します" },
    { NULL,   "※設定変更の反映には再起動が必要です" },
    { NULL, NULL }
  };
  show_usage("imgscsi", m, 16);
  terminate(1);
}

void cmd_imgscsi_stat(void)
{
  printf("[リモートイメージ]\n");
  printf("%sドライバ\n", config_data.hdsscsi ? "純正SCSI" : "リモートイメージ");
}

void cmd_imgscsi(int argc, char **argv)
{
  int onoff = -1;

  if (argc <= 1) {
    cmd_imgscsi_stat();
    return;
  }

  if (strcmp(argv[1], "on") == 0) {
    onoff = 1;
  } else if (strcmp(argv[1], "off") == 0) {
    onoff = 0;
  } else {
    cmd_imgscsi_usage();
  }

  com_cmdres_init(setrmtcfg, CMD_SETRMTCFG);
  init_rmtcfg(cmd);
  cmd->hdsscsi = onoff;
  com_cmdres_exec();
  save_config();
  needreboot = true;
}

//****************************************************************************
// zremote erase
//****************************************************************************

void cmd_erase_usage(void)
{
  struct usage_message m[] = {
    { "", "不揮発メモリに保存されている設定内容を全消去します" },
    { NULL, NULL }
  };
  show_usage("erase", m, 19);
  terminate(1);
}

void cmd_erase(int argc, char **argv)
{
  if (argc != 1) {
    cmd_erase_usage();
  }

  printf("保存されている設定内容を全消去します。よろしいですか? (y/n):");
  fflush(stdout);

  int k = _iocs_b_keyinp() & 0xff;
  if (k != 'y') {
    printf("\n中止しました\n");
    terminate(1);
  }

  com_cmdres_init(flashclear, CMD_FLASHCLEAR);
  com_cmdres_exec();
  printf("\n設定内容を全消去しました\n");
}

//****************************************************************************
// zremote stat
//****************************************************************************

void cmd_stat_usage(void)
{
  struct usage_message m[] = {
    { "", "現在の設定内容一覧を表示します" },
    { NULL, NULL }
  };
  show_usage("show", m, 20);
  terminate(1);
}

void cmd_stat(int argc, char **argv)
{
  if (argc > 1) {
    cmd_stat_usage();
  }
  cmd_bootmode_stat();
  cmd_wifi_stat();
  cmd_server_stat();
  cmd_mount_stat();
}

//****************************************************************************
// Main routine
//****************************************************************************

void usage(void)
{
  printf(
    "X68000 Z Remote Drive Service version " GIT_REPO_VERSION "\n" 
    "使用法: " PROGNAME " サブコマンド名 [引数]\n\n"
    "以下のサブコマンドが利用できます\n"
  );
  for (int i = 0; i < sizeof(cmd_table) / sizeof(cmd_table[0]); i++) {
    if (cmd_table[i].usage[0] != '#') {
      printf("  " PROGNAME " %-12s%s\n", cmd_table[i].name, cmd_table[i].usage);
    }
  }
  terminate(1);
}

int main(int argc, char **argv)
{
  int8_t *zusb_channels = NULL;

  _dos_super(0);

  if (com_connect(false) < 0) {
    printf(PROGNAME ": ZUSB デバイスが見つかりません\n");
    exit(1);
  }

  if (setjmp(jenv)) {
    printf(PROGNAME ": ZUSB デバイスが切断されました\n");
    terminate(1);
  }

  {
    com_cmdres_init(getinfo, CMD_GETINFO);
    com_cmdres_exec();
    if (res->version != PROTO_VERSION) {
      printf(PROGNAME ": X68000 Z Remote Drive Service のファームウェアバージョンが合致しません\n");
      terminate(1);
    }
  }

  {
    com_cmdres_init(getconfig, CMD_GETCONFIG);
    com_cmdres_exec();
    config_data = res->data;
  }

  if (argc < 2) {
    cmd_stat(0, NULL);
    terminate(0);
  }

  for (int i = 0; i < sizeof(cmd_table) / sizeof(cmd_table[0]); i++) {
    if (strcmp(argv[1], cmd_table[i].name) == 0) {
      cmd_table[i].func(argc - 1, &argv[1]);
      terminate(0);
    }
  }

  usage();
}
