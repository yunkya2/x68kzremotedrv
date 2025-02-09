/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Yuichi Nakamura (@yunkya2)
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

#ifndef _ZUSBMACRO_H_
#define _ZUSBMACRO_H_

#include <stdint.h>
#include "zusbregs.h"
#include "zusbtypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------*/

volatile struct zusb_regs *zusb __attribute__((common));
uint8_t *zusbbuf __attribute__((common));

/*--------------------------------------------------------------------------*/

static inline void zusb_set_region(void *buf, int count)
{
    zusb->caddr = (uint32_t)buf;
    zusb->ccount = count;
}

static inline void zusb_set_ep_region(int epno, void *buf, int count)
{
    zusb->paddr[epno] = (uint32_t)buf;
    zusb->pcount[epno] = count;
}

static inline void zusb_set_ep_region_isoc(int epno, void *buf, struct zusb_isoc_desc *desc, int count)
{
    zusb->paddr[epno] = (uint32_t)desc;
    zusb->pcount[epno] = count;
    zusb->pdaddr[epno] = (uint32_t)buf;
}

static inline int zusb_send_cmd(int cmd)
{
    zusb->cmd = cmd;
    while (zusb->stat & ZUSB_STAT_BUSY) {
        if (zusb->stat & ZUSB_STAT_ERROR) {
            return -1;
        }
    }
    if (zusb->stat & ZUSB_STAT_ERROR) {
        return -1;
    }
    return 0;
}

static inline int zusb_get_descriptor(uint8_t *buf)
{
    zusb_set_region(buf, 1);
    if (zusb_send_cmd(ZUSB_CMD_GETDESC) < 0) {
        return -1;
    }
    if (zusb->ccount == 0) {
        return 0;
    }
    int len = buf[0];
    zusb_set_region(&zusbbuf[1], len - 1);
    if (zusb_send_cmd(ZUSB_CMD_GETDESC) < 0 ||
        zusb->ccount != len - 1) {
        return -1;
    }
    return len;
}

static inline void zusb_rewind_descriptor(void)
{
    zusb->devid = zusb->devid;
}

static inline int zusb_send_control(int bmRequestType, int bRequest, int wValue, int wIndex, int wLength, void *data)
{
    zusb->param = (bmRequestType<< 8) | bRequest;
    zusb->value = wValue;
    zusb->index = wIndex;
    zusb_set_region(data, wLength);
    int res = zusb_send_cmd(ZUSB_CMD_CONTROL);
    return (res == 0) ? zusb->ccount : res;
}

/*--------------------------------------------------------------------------*/

static inline void zusb_set_channel(int ch)
{
    zusb = (volatile struct zusb_regs *)(ZUSB_BASEADDR + ch * ZUSB_SZ_CH);
    zusbbuf = (uint8_t *)(ZUSB_BASEADDR + ch * ZUSB_SZ_CH + ZUSB_SZ_REGS);
}

static inline int zusb_open(int ch)
{
    for (; ch < ZUSB_N_CH; ch++) {
        uint16_t magic;
        zusb_set_channel(ch);
        if (_dos_bus_err((void *)zusb, &magic, 2) != 0 || magic != ZUSB_MAGIC) {
            return -1;      /* ZUSB not exists */
        }
        if (zusb->stat & ZUSB_STAT_PROTECTED) {
            continue;
        }
        zusb_send_cmd(ZUSB_CMD_OPENCH);
        return ch;
    }
    return -2;      /* device busy */
}

static inline int zusb_open_protected(void)
{
    int ch;
    for (ch = ZUSB_N_CH - 1; ch >= 0; ch--) {
        uint16_t magic;
        zusb_set_channel(ch);
        if (_dos_bus_err((void *)zusb, &magic, 2) != 0 || magic != ZUSB_MAGIC) {
            return -1;      /* ZUSB not exists */
        }
        if (!(zusb->stat & ZUSB_STAT_INUSE)) {
            zusb_send_cmd(ZUSB_CMD_OPENCHP);
            return ch;
        }
    }
    return -2;      /* device busy */
}

static inline void zusb_close(void)
{
    zusb_send_cmd(ZUSB_CMD_CLOSECH);
}

static inline int zusb_version(void)
{
    zusb_send_cmd(ZUSB_CMD_GETVER);
    return zusb->err;
}

/*--------------------------------------------------------------------------*/

struct zusb_match_with_vid_pid_arg {
    uint16_t vid;
    uint16_t pid;
};
static inline int zusb_match_with_vid_pid(int devid, int type, uint8_t *desc, void *arg)
{
    struct zusb_match_with_vid_pid_arg *a = (struct zusb_match_with_vid_pid_arg *)arg;
    zusb_desc_device_t *ddev = (zusb_desc_device_t *)desc;
    if (type != ZUSB_DESC_DEVICE || ddev->bLength != sizeof(zusb_desc_device_t)) {
        return 0;
    }
    if (zusb_le16toh(ddev->idVendor) == a->vid && zusb_le16toh(ddev->idProduct) == a->pid) {
        return 1;
    }
    return 0;
}

struct zusb_match_with_devclass_arg {
    int devclass;
    int subclass;
    int protocol;
};
static inline int zusb_match_with_devclass(int devid, int type, uint8_t *desc, void *arg)
{
    struct zusb_match_with_devclass_arg *a = (struct zusb_match_with_devclass_arg *)arg;
    zusb_desc_interface_t *dintf = (zusb_desc_interface_t *)desc;
    if (type != ZUSB_DESC_INTERFACE || dintf->bLength != sizeof(zusb_desc_interface_t)) {
        return 0;
    }
    if ((a->devclass < 0 || (dintf->bInterfaceClass == a->devclass)) &&
        (a->subclass < 0 || (dintf->bInterfaceSubClass == a->subclass)) &&
        (a->protocol < 0 || (dintf->bInterfaceProtocol == a->protocol))) {
        return 1;
    }
    return 0;
}

typedef int zusb_match_func(int devid, int type, uint8_t *desc, void *arg);

static inline int zusb_find_device(zusb_match_func *fn, void *arg, int pdev)
{
    if (zusb_send_cmd(ZUSB_CMD_GETDEV) < 0) {
        return -1;
    }

    if (pdev > 0) {
        while (zusb->devid != pdev && zusb->devid != 0) {
            if (zusb_send_cmd(ZUSB_CMD_NEXTDEV) < 0) {
                return -1;
            }
        }
        while (zusb->devid == pdev) {
            if (zusb_send_cmd(ZUSB_CMD_NEXTDEV) < 0) {
                return -1;
            }
        }
    }

    while (zusb->devid != 0) {
        while (zusb_get_descriptor(zusbbuf) > 0) {
            if (fn(zusb->devid, zusbbuf[1], zusbbuf, arg)) {
                int res = zusb->devid;
                do {
                    if (zusb_send_cmd(ZUSB_CMD_NEXTDEV) < 0) {
                        break;
                    }
                } while (zusb->devid != 0);
                zusb->devid = res;
                return res;
            }
        }

        if (zusb_send_cmd(ZUSB_CMD_NEXTDEV) < 0) {
            return -1;
        }
    }
    return 0;
}

static inline int zusb_find_device_with_vid_pid(int vid, int pid, int pdev)
{
    struct zusb_match_with_vid_pid_arg arg = { vid, pid };
    return zusb_find_device(zusb_match_with_vid_pid, &arg, pdev);
}

static inline int zusb_find_device_with_devclass(int devclass, int subclass, int protocol, int pdev)
{
    struct zusb_match_with_devclass_arg arg = { devclass, subclass, protocol };
    return zusb_find_device(zusb_match_with_devclass, &arg, pdev);
}

static inline int zusb_get_string_descriptor(char *str, int len, int index)
{
    uint8_t *buf = &zusbbuf[ZUSB_SZ_USBBUF - 256];

    zusb->param = (ZUSB_DIR_IN << 8) | ZUSB_REQ_GET_DESCRIPTOR;
    zusb->value = (ZUSB_DESC_STRING << 8) | index;
    zusb->index = 0x0409;   /* language ID (English) */
    zusb_set_region(buf, 256);
    if (zusb_send_cmd(ZUSB_CMD_CONTROL) < 0) {
        return -1;
    }

    for (int i = 2; i < zusb->ccount; i += 2) {
        if (len <= 0) {
            break;
        }
        *str++ = buf[i];
        len--;
        if (buf[i] == 0) {
            break;
        }
    }
    *str = '\0';
    return zusb->ccount;
}

/*--------------------------------------------------------------------------*/

typedef struct zusb_endpoint_config {
    uint8_t address;
    uint8_t attribute;
    uint16_t maxpacketsize;
} zusb_endpoint_config_t;

static inline int zusb_connect_device(int devid,
                                      int config, int devclass, int subclass, int protocol,
                                      zusb_endpoint_config_t epcfg[])
{
    int result = 0;
    int use_config = 0;
    int use_intf = 0;

    zusb->devid = devid;
    while (zusb_get_descriptor(zusbbuf) > 0) {
        uint8_t *desc = zusbbuf;
        if (desc[1] == ZUSB_DESC_CONFIGURATION) {
            zusb_desc_configuration_t *dconf = (zusb_desc_configuration_t *)desc;
            use_config = (dconf->bConfigurationValue == config);
        }
        if (!use_config) {
            continue;
        }

        switch (desc[1]) {
        case ZUSB_DESC_INTERFACE:
            zusb_desc_interface_t *dintf = (zusb_desc_interface_t *)desc;
            use_intf = ((devclass < 0 || (dintf->bInterfaceClass == devclass)) &&
                        (subclass < 0 || (dintf->bInterfaceSubClass == subclass)) &&
                        (protocol < 0 || (dintf->bInterfaceProtocol == protocol)));
            if (use_intf && (dintf->bAlternateSetting == 0)) {
                zusb->param = (config << 8) | dintf->bInterfaceNumber;
                if (zusb_send_cmd(ZUSB_CMD_CONNECT) < 0) {
                    use_intf = 0;
                    break;
                }
                result++;
            }
            break;
        case ZUSB_DESC_ENDPOINT:
            if (!use_intf) {
                break;
            }
            zusb_desc_endpoint_t *dendp = (zusb_desc_endpoint_t *)desc;
            uint16_t cfg = (dendp->bEndpointAddress << 8) | dendp->bmAttributes;
            for (int i = 0; i < ZUSB_N_EP; i++) {
                uint16_t ncfg = zusb->pcfg[i];
                if (epcfg[i].maxpacketsize == 0xffff) {
                    break;
                }
                if (ncfg == cfg) {
                    break;
                } else if (ncfg == 0xffff) {
                    if (((dendp->bEndpointAddress & ZUSB_DIR_MASK) == (epcfg[i].address & ZUSB_DIR_MASK)) &&
                        ((dendp->bmAttributes & ZUSB_XFER_MASK) == (epcfg[i].attribute & ZUSB_XFER_MASK))) {
                        zusb->pcfg[i] = cfg;
                        zusb->pcount[i] = zusb_le16toh(dendp->wMaxPacketSize);
                        epcfg[i].address = dendp->bEndpointAddress;
                        epcfg[i].attribute = dendp->bmAttributes;
                        epcfg[i].maxpacketsize = zusb_le16toh(dendp->wMaxPacketSize);
                        break;
                    }
                }
            }
            break;
        }
    }
    return result;
}

static inline void zusb_disconnect_device(void)
{
    zusb_send_cmd(ZUSB_CMD_DISCONNECT);
}

#ifdef __cplusplus
}
#endif

#endif /* _ZUSBMACRO_H_ */
