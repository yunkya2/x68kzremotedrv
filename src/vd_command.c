/*
 * Copyright (c) 2023,2024 Yuichi Nakamura (@yunkya2)
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

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "pico/cyw43_arch.h"
#include "pico/stdio.h"
#include "pico/stdlib.h"
#include "hardware/watchdog.h"

#include "smb2.h"
#include "libsmb2.h"
#include "libsmb2-raw.h"

typedef unsigned int nfds_t;
struct pollfd
{
  int fd;
  short events;
  short revents;
};
int lwip_poll(struct pollfd *fds, nfds_t nfds, int timeout);

#include "main.h"
#include "virtual_disk.h"
#include "vd_command.h"
#include "config_file.h"
#include "remoteserv.h"
#include "fileop.h"

//****************************************************************************
// Callback functions
//****************************************************************************

static struct res_wifi_scan wifi_scan_data;
static int16_t wifi_rssi[countof(wifi_scan_data.ssid)];

static int scan_result(void *env, const cyw43_ev_scan_result_t *result)
{
  if (result == NULL)
    return 0;

  printf("ssid: %-16s rssi:%3d chan:%3d mac: %02x:%02x:%02x:%02x:%02x:%02x sec: %u\n",
    result->ssid, result->rssi, result->channel,
    result->bssid[0], result->bssid[1], result->bssid[2], result->bssid[3], result->bssid[4], result->bssid[5],
    result->auth_mode);

  if (strlen(result->ssid) == 0) {
    return 0;
  }
  for (int i = 0; i < wifi_scan_data.n_items; i++) {
    if (strcmp(wifi_scan_data.ssid[i], result->ssid) == 0) {
      return 0;
    }
  }

  if (wifi_scan_data.n_items < countof(wifi_scan_data.ssid)) {
    wifi_rssi[wifi_scan_data.n_items] = result->rssi;
    strcpy(wifi_scan_data.ssid[wifi_scan_data.n_items++], result->ssid);
  }

  for (int i = 0; i < wifi_scan_data.n_items; i++) {
    int maxrssi = -100;
    int maxndx = -1;
    for (int j = i; j < wifi_scan_data.n_items; j++) {
      if (wifi_rssi[j] > maxrssi) {
        maxrssi = wifi_rssi[j];
        maxndx = j;
      }
    }
    if (maxndx >= 0 && maxndx != i) {
      int tmp = wifi_rssi[i];
      wifi_rssi[i] = wifi_rssi[maxndx];
      wifi_rssi[maxndx] = tmp;
      char tmps[32];
      strcpy(tmps, wifi_scan_data.ssid[i]);
      strcpy(wifi_scan_data.ssid[i], wifi_scan_data.ssid[maxndx]);
      strcpy(wifi_scan_data.ssid[maxndx], tmps);
    }
  }
  return 0;
}

static volatile bool smb2_enum_finished;
static struct res_smb2_enum *smb2_enum_ptr;

static void se_cb(struct smb2_context *smb2, int status,
                  void *command_data, void *private_data)
{
  struct srvsvc_NetrShareEnum_rep *rep = command_data;

  if (smb2_enum_ptr == NULL)
    return;

  if (status) {
    printf("failed to enumerate shares (%s) %s\n",
           strerror(-status), smb2_get_error(smb2));
    smb2_enum_ptr->status = -1;
    smb2_enum_finished = true;
    return;
  }

  printf("Number of shares:%d\n", rep->ses.ShareInfo.Level1.EntriesRead);
  for (int i = 0; i < rep->ses.ShareInfo.Level1.EntriesRead; i++) {
    if ((rep->ses.ShareInfo.Level1.Buffer->share_info_1[i].type & 3) == SHARE_TYPE_DISKTREE &&
        !(rep->ses.ShareInfo.Level1.Buffer->share_info_1[i].type & SHARE_TYPE_HIDDEN)) {
      if (smb2_enum_ptr->n_items < countof(smb2_enum_ptr->share)) {
        smb2_enum_ptr->share[smb2_enum_ptr->n_items][sizeof(smb2_enum_ptr->share[0]) - 1] = '\0';
        strncpy(smb2_enum_ptr->share[smb2_enum_ptr->n_items++], 
                rep->ses.ShareInfo.Level1.Buffer->share_info_1[i].netname.utf8, sizeof(smb2_enum_ptr->share[0]) - 1);
      }
    }

    printf("%-20s %-20s", rep->ses.ShareInfo.Level1.Buffer->share_info_1[i].netname.utf8,
           rep->ses.ShareInfo.Level1.Buffer->share_info_1[i].remark.utf8);
    if ((rep->ses.ShareInfo.Level1.Buffer->share_info_1[i].type & 3) == SHARE_TYPE_DISKTREE) {
      printf(" DISKTREE");
    }
    if ((rep->ses.ShareInfo.Level1.Buffer->share_info_1[i].type & 3) == SHARE_TYPE_PRINTQ) {
      printf(" PRINTQ");
    }
    if ((rep->ses.ShareInfo.Level1.Buffer->share_info_1[i].type & 3) == SHARE_TYPE_DEVICE) {
      printf(" DEVICE");
    }
    if ((rep->ses.ShareInfo.Level1.Buffer->share_info_1[i].type & 3) == SHARE_TYPE_IPC) {
      printf(" IPC");
    }
    if (rep->ses.ShareInfo.Level1.Buffer->share_info_1[i].type & SHARE_TYPE_TEMPORARY) {
      printf(" TEMPORARY");
    }
    if (rep->ses.ShareInfo.Level1.Buffer->share_info_1[i].type & SHARE_TYPE_HIDDEN) {
      printf(" HIDDEN");
    }
    printf("\n");
  }

  smb2_free_data(smb2, rep);
  smb2_enum_ptr->status = 0;
  smb2_enum_finished = true;
}

//****************************************************************************
// vd_command service
//****************************************************************************

int vd_command(uint8_t *cbuf, uint8_t *rbuf)
{
  if (cbuf[0] != 0xff) {
    return -1;
  }

  DPRINTF2("----VDCommand: 0x%04x\n", (cbuf[0] << 8) | cbuf[1]);
  int rsize = -1;

  switch ((cbuf[0] << 8) | cbuf[1]) {
  case CMD_GETINFO:
    {
      struct cmd_getinfo *cmd = (struct cmd_getinfo *)cbuf;
      struct res_getinfo *res = (struct res_getinfo *)rbuf;
      rsize = sizeof(*res);

      memset(res, 0, rsize);

      if (boottime != 0 && config.tadjust != 0) {
        time_t tt = (time_t)((boottime + to_us_since_boot(get_absolute_time())) / 1000000);
        tt += config.tadjust;
        struct tm *tm = localtime(&tt);
        res->year = htobe16(tm->tm_year + 1900);
        res->mon = tm->tm_mon + 1;
        res->day = tm->tm_mday;
        res->hour = tm->tm_hour;
        res->min = tm->tm_min;
        res->sec = tm->tm_sec;
      }

      res->remoteunit = config.remoteunit;
      res->hdsunit = config.hdsunit;

      res->version = PROTO_VERSION;
      strncpy(res->verstr, GIT_REPO_VERSION, sizeof(res->verstr) - 1);
      break;
    }

  case CMD_GETCONFIG:
    {
      struct cmd_getconfig *cmd = (struct cmd_getconfig *)cbuf;
      struct res_getconfig *res = (struct res_getconfig *)rbuf;
      rsize = sizeof(*res);
      res->data = config;
      break;
    }

  case CMD_SETCONFIG:
    {
      struct cmd_setconfig *cmd = (struct cmd_setconfig *)cbuf;
      struct res_setconfig *res = (struct res_setconfig *)rbuf;
      config = cmd->data;
      res->status = VDERR_OK;
      rsize = sizeof(*res);
      if (cmd->mode != CONNECT_NONE) {
        xTaskNotify(connect_th, cmd->mode | CONNECT_WAIT, eSetBits);
      }
      break;
    }

  case CMD_GETSTATUS:
    {
      struct cmd_getstatus *cmd = (struct cmd_getstatus *)cbuf;
      struct res_getstatus *res = (struct res_getstatus *)rbuf;
      rsize = sizeof(*res);
      res->status = sysstatus;
      break;
    }

  case CMD_FLASHCONFIG:
    {
      struct cmd_flashconfig *cmd = (struct cmd_flashconfig *)cbuf;
      struct res_flashconfig *res = (struct res_flashconfig *)rbuf;
      config_write();
      res->status = VDERR_OK;
      rsize = sizeof(*res);
      break;
    }

  case CMD_FLASHCLEAR:
    {
      struct cmd_flashclear *cmd = (struct cmd_flashclear *)cbuf;
      struct res_flashclear *res = (struct res_flashclear *)rbuf;
      config_erase();
      config_read();
      res->status = VDERR_OK;
      rsize = sizeof(*res);
      xTaskNotify(connect_th, CONNECT_WAIT, eSetBits);
      break;
    }

  case CMD_REBOOT:
    {
      // reboot by watchdog
      watchdog_enable(500, 1);
      while (1)
        ;
    }

  ////////////////////////////////////////////////////////////////////////////
  // WiFi AP confguration

  case CMD_WIFI_CONFIG:
    {
      struct cmd_wifi_config *cmd = (struct cmd_wifi_config *)cbuf;
      struct res_wifi_config *res = (struct res_wifi_config *)rbuf;

      memcpy(config.wifi_ssid, cmd->wifi_ssid, sizeof(config.wifi_ssid));
      memcpy(config.wifi_passwd, cmd->wifi_passwd, sizeof(config.wifi_passwd));

      res->status = VDERR_OK;
      rsize = sizeof(*res);
      xTaskNotify(connect_th, CONNECT_WIFI|CONNECT_WAIT, eSetBits);
      break;
    }

  case CMD_WIFI_SCAN:
    {
      struct cmd_wifi_scan *cmd = (struct cmd_wifi_scan *)cbuf;
      struct res_wifi_scan *res = (struct res_wifi_scan *)rbuf;

      rsize = sizeof(*res);
      if (cmd->clear) {
        memset(&wifi_scan_data, 0, sizeof(wifi_scan_data));
      }

      printf("scan status %d\n", cyw43_wifi_scan_active(&cyw43_state));

      cyw43_wifi_scan_options_t scan_options = {0};
      int err = cyw43_wifi_scan(&cyw43_state, &scan_options, NULL, scan_result);
      if (err != 0) {
        printf("Failed to start scan: %d\n", err);
        res->status = VDERR_EIO;
        break;
      }
      printf("Performing wifi scan\n");
      while (cyw43_wifi_scan_active(&cyw43_state)) {
        vTaskDelay(pdMS_TO_TICKS(200));
      }
      *res = wifi_scan_data;
      res->status = VDERR_OK;
      break;
    }

  ////////////////////////////////////////////////////////////////////////////
  // SMB2 server configuration

  case CMD_SMB2_CONFIG:
    {
      struct cmd_smb2_config *cmd = (struct cmd_smb2_config *)cbuf;
      struct res_smb2_config *res = (struct res_smb2_config *)rbuf;

      memcpy(config.smb2_server, cmd->smb2_server, sizeof(config.smb2_server));
      memcpy(config.smb2_user, cmd->smb2_user, sizeof(config.smb2_user));
      memcpy(config.smb2_workgroup, cmd->smb2_workgroup, sizeof(config.smb2_workgroup));
      memcpy(config.smb2_passwd, cmd->smb2_passwd, sizeof(config.smb2_passwd));

      res->status = VDERR_OK;
      rsize = sizeof(*res);
      xTaskNotify(connect_th, CONNECT_SMB2|CONNECT_WAIT, eSetBits);
      break;
    }

  case CMD_SMB2_ENUM:
    {
      struct cmd_smb2_enum *cmd = (struct cmd_smb2_enum *)cbuf;
      struct res_smb2_enum *res = (struct res_smb2_enum *)rbuf;
      struct pollfd pfd;
      struct smb2_context *smb2ipc;

      rsize = sizeof(*res);
      memset(res, 0, rsize);
      res->status = VDERR_EIO;

      if ((smb2ipc = connect_smb2("IPC$")) == NULL) {
        break;
      }

      smb2_enum_finished = false;
      smb2_enum_ptr = res;
      if (smb2_share_enum_async(smb2ipc, 1, se_cb, NULL) != 0) {
        printf("smb2_share_enum failed. %s\n", smb2_get_error(smb2ipc));
        goto errout_enum;
      }

      while (!smb2_enum_finished) {
        pfd.fd = smb2_get_fd(smb2ipc);
        pfd.events = smb2_which_events(smb2ipc);

        if (lwip_poll(&pfd, 1, 1000) < 0) {
          printf("Poll failed");
          goto errout_enum;
        }
        if (pfd.revents == 0) {
          continue;
        }
        if (smb2_service(smb2ipc, pfd.revents) < 0) {
          printf("smb2_service failed with : %s\n", smb2_get_error(smb2ipc));
          goto errout_enum;
        }
      }
    errout_enum:
      smb2_enum_finished = false;
      smb2_enum_ptr = NULL;
      disconnect_smb2(smb2ipc);
      break;
    }

  case CMD_SMB2_LIST:
    {
      struct cmd_smb2_list *cmd = (struct cmd_smb2_list *)cbuf;
      struct res_smb2_list *res = (struct res_smb2_list *)rbuf;
      struct smb2_context *smb2;

      rsize = sizeof(*res);
      memset(res, 0, rsize);

      char path[256];
      char *dst_buf = path;
      size_t dst_len = sizeof(path) - 1;
      char *src_buf = cmd->path;
      size_t src_len = strlen(cmd->path);
      if (iconv_s2u(&src_buf, &src_len, &dst_buf, &dst_len) < 0) {
        dst_buf = path;
      }
      *dst_buf = '\0';

      res->status = VDERR_EIO;

      if ((smb2 = connect_smb2(cmd->share)) == NULL) {
        break;
      }

      struct smb2dir *dir;
      struct smb2dirent *ent;
      char *p = res->list;
      char *q = &res->list[sizeof(res->list)];

      res->status = VDERR_ENOENT;

      dir = smb2_opendir(smb2, path);
      if (dir != NULL) {
        while ((ent = smb2_readdir(smb2, dir))) {
          if (ent->st.smb2_type == SMB2_TYPE_DIRECTORY ||
              ent->st.smb2_type == SMB2_TYPE_FILE) {
            if (strcmp(ent->name, ".") == 0 || strcmp(ent->name, "..") == 0)
              continue;

            /* 拡張子 .HDS のファイルのみ返す */
            if (ent->st.smb2_type == SMB2_TYPE_FILE) {
              int len;
              if ((len = strlen(ent->name)) <= 4)
                continue;
              if (!(((ent->name[len - 4] == '.' &&
                     (ent->name[len - 3] & 0xdf) == 'H' &&
                     (ent->name[len - 2] & 0xdf) == 'D' &&
                     (ent->name[len - 1] & 0xdf) == 'S')) ||
                    ((ent->name[len - 4] == '.' &&
                     (ent->name[len - 3] & 0xdf) == 'M' &&
                     (ent->name[len - 2] & 0xdf) == 'O' &&
                     (ent->name[len - 1] & 0xdf) == 'S'))))
                continue;
            }

            printf("%s\n", ent->name);
            char buf[128];
            char *dst_buf = buf;
            size_t dst_len = sizeof(buf) - 1;
            char *src_buf = (char *)ent->name;
            size_t src_len = strlen(ent->name);
            if (iconv_u2s(&src_buf, &src_len, &dst_buf, &dst_len) < 0) {
              continue;
            }
            *dst_buf = '\0';
            int l = strlen(buf);
            if (p + l + 3 < q) {
              strcpy(p, buf);
              if (ent->st.smb2_type == SMB2_TYPE_DIRECTORY) {
                strcat(p, "/");
              }
              p += strlen(p) + 1;
            }
          }
        }
        smb2_closedir(smb2, dir);
        *p++ = '\0';
        res->status = VDERR_OK;
      }

      disconnect_smb2(smb2);
      break;
    }

  ////////////////////////////////////////////////////////////////////////////
  // Remote drive & HDS configuration

  case CMD_SETRMTDRV:
    {
      struct cmd_setrmtdrv *cmd = (struct cmd_setrmtdrv *)cbuf;
      struct res_setrmtdrv *res = (struct res_setrmtdrv *)rbuf;
      rsize = sizeof(*res);
      res->status = remote_mount(cmd->unit, cmd->path);
      break;
    }

  case CMD_SETRMTHDS:
    {
      struct cmd_setrmthds *cmd = (struct cmd_setrmthds *)cbuf;
      struct res_setrmthds *res = (struct res_setrmthds *)rbuf;
      rsize = sizeof(*res);
      res->status = hds_mount(cmd->unit, cmd->path);
      break;
    }

  case CMD_SETRMTCFG:
    {
      struct cmd_setrmtcfg *cmd = (struct cmd_setrmtcfg *)cbuf;
      struct res_setrmtcfg *res = (struct res_setrmtcfg *)rbuf;
      rsize = sizeof(*res);
      config.selfboot = cmd->selfboot;
      config.remoteboot = cmd->remoteboot;
      config.remoteunit = cmd->remoteunit;
      config.hdsscsi = cmd->hdsscsi;
      config.hdsunit = cmd->hdsunit;
      res->status = VDERR_OK;
      break;
    }

  ////////////////////////////////////////////////////////////////////////////
  // Remote HDS read/write

  case CMD_HDSREAD:
    {
      struct cmd_hdsread *cmd = (struct cmd_hdsread *)cbuf;
      struct res_hdsread *res = (struct res_hdsread *)rbuf;
      rsize = sizeof(*res) + cmd->nsect * 512;

      DPRINTF2("CMD_HDSREAD: unit=%d pos=0x%x nsect=%d\n", cmd->unit, be32toh(cmd->pos), cmd->nsect);

      res->nsect = cmd->nsect;
      if (hdsinfo[cmd->unit].smb2 == NULL) {
        res->status = VDERR_EINVAL;
        break;
      }

      struct hdsinfo *hds = &hdsinfo[cmd->unit];
      for (int i = 0; i < cmd->nsect; i++) {
        res->status = hds_cache_read(hds->smb2, hds->sfh,
                                     be32toh(cmd->pos) + i, res->data + i * 512);
        if (res->status < 0) {
          break;
        }
      }
      break;
    }

  case CMD_HDSWRITE:
    {
      struct cmd_hdswrite *cmd = (struct cmd_hdswrite *)cbuf;
      struct res_hdswrite *res = (struct res_hdswrite *)rbuf;
      rsize = sizeof(*res);

      DPRINTF2("CMD_HDSWRITE: unit=%d pos=0x%x nsect=%d\n", cmd->unit, be32toh(cmd->pos), cmd->nsect);

      if (hdsinfo[cmd->unit].smb2 == NULL) {
        res->status = VDERR_EINVAL;
        break;
      }

      struct hdsinfo *hds = &hdsinfo[cmd->unit];
      for (int i = 0; i < cmd->nsect; i++) {
        res->status = hds_cache_write(hds->smb2, hds->sfh,
                                      be32toh(cmd->pos) + i, cmd->data + i * 512);
        if (res->status < 0) {
          break;
        }
      }
      break;
    }

  default:
    break;
  }

  return rsize;
}
