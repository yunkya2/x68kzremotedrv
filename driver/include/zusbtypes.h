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

#ifndef _ZUSB_TYPES_H_
#define _ZUSB_TYPES_H_

#include <stdint.h>

#ifdef __cplusplus
 extern "C" {
#endif

//----------------------------------------------------------------------------
// Little Endian Macros
//----------------------------------------------------------------------------

typedef uint16_t ule16_t;
typedef uint32_t ule32_t;

#define zusb_bswap16(u16)   ((((u16) & 0xff00) >> 8) | (((u16) & 0x00ff) << 8))
#define zusb_bswap32(u32)   ((((u32) & 0xff000000) >> 24) | (((u32) & 0x00ff0000) >> 8) | \
                             (((u32) & 0x0000ff00) << 8) | (((u32) & 0x000000ff) << 24))
#define zusb_htole16(u16)   (zusb_bswap16(u16))
#define zusb_le16toh(u16)   (zusb_bswap16(u16))
#define zusb_htole32(u32)   (zusb_bswap32(u32))
#define zusb_le32toh(u32)   (zusb_bswap32(u32))


//----------------------------------------------------------------------------
// USB Constants
//----------------------------------------------------------------------------

#define ZUSB_DIR_OUT                  0x00
#define ZUSB_DIR_IN                   0x80
#define ZUSB_DIR_MASK                 0x80
#define ZUSB_EP_MASK                  0x0f

#define ZUSB_XFER_CONTROL             0
#define ZUSB_XFER_ISOCHRONOUS         1
#define ZUSB_XFER_BULK                2
#define ZUSB_XFER_INTERRUPT           3
#define ZUSB_XFER_MASK                3

#define ZUSB_CLASS_NONE                 0x00
#define ZUSB_CLASS_AUDIO                0x01
#define ZUSB_CLASS_CDC                  0x02
#define ZUSB_CLASS_HID                  0x03
#define ZUSB_CLASS_PHYSICAL             0x05
#define ZUSB_CLASS_IMAGE                0x06
#define ZUSB_CLASS_PRINTER              0x07
#define ZUSB_CLASS_MSC                  0x08
#define ZUSB_CLASS_HUB                  0x09
#define ZUSB_CLASS_CDC_DATA             0x0a
#define ZUSB_CLASS_SMART_CARD           0x0b
#define ZUSB_CLASS_CONTENT_SECURITY     0x0d
#define ZUSB_CLASS_VIDEO                0x0e
#define ZUSB_CLASS_PERSONAL_HEALTHCARE  0x0f
#define ZUSB_CLASS_AUDIO_VIDEO          0x10
#define ZUSB_CLASS_MISC                 0xef
#define ZUSB_CLASS_AUDIO_VIDEO          0x10
#define ZUSB_CLASS_APP_SPECIFIC         0xfe
#define ZUSB_CLASS_VENDOR_SPECIFIC      0xff

//----------------------------------------------------------------------------
// USB Control Request
//----------------------------------------------------------------------------

#define ZUSB_REQ_DIR_OUT              0x00
#define ZUSB_REQ_DIR_IN               0x80
#define ZUSB_REQ_DIR_MASK             0x80

#define ZUSB_REQ_TYPE_STANDARD        (0 << 5)
#define ZUSB_REQ_TYPE_CLASS           (1 << 5)
#define ZUSB_REQ_TYPE_VENDOR          (2 << 5)
#define ZUSB_REQ_TYPE_INVALID         (3 << 5)
#define ZUSB_REQ_TYPE_MASK            (3 << 5)

#define ZUSB_REQ_RCPT_DEVICE          0x00
#define ZUSB_REQ_RCPT_INTERFACE       0x01
#define ZUSB_REQ_RCPT_ENDPOINT        0x02
#define ZUSB_REQ_RCPT_OTHER           0x03
#define ZUSB_REQ_RCPT_MASK            0x1f

#define ZUSB_REQ_GET_STATUS           0
#define ZUSB_REQ_CLEAR_FEATURE        1
#define ZUSB_REQ_SET_FEATURE          3
#define ZUSB_REQ_SET_ADDRESS          5
#define ZUSB_REQ_GET_DESCRIPTOR       6
#define ZUSB_REQ_SET_DESCRIPTOR       7
#define ZUSB_REQ_GET_CONFIGURATION    8
#define ZUSB_REQ_SET_CONFIGURATION    9
#define ZUSB_REQ_GET_INTERFACE        10
#define ZUSB_REQ_SET_INTERFACE        11
#define ZUSB_REQ_SYNCH_FRAME          12

// Class Specific Request
#define ZUSB_REQ_CS_IF_OUT      (ZUSB_REQ_DIR_OUT|ZUSB_REQ_TYPE_CLASS|ZUSB_REQ_RCPT_INTERFACE)
#define ZUSB_REQ_CS_IF_IN       (ZUSB_REQ_DIR_IN |ZUSB_REQ_TYPE_CLASS|ZUSB_REQ_RCPT_INTERFACE)
#define ZUSB_REQ_CS_EP_OUT      (ZUSB_REQ_DIR_OUT|ZUSB_REQ_TYPE_CLASS|ZUSB_REQ_RCPT_ENDPOINT)
#define ZUSB_REQ_CS_EP_IN       (ZUSB_REQ_DIR_IN |ZUSB_REQ_TYPE_CLASS|ZUSB_REQ_RCPT_ENDPOINT)

typedef struct __attribute__((packed)) zusb_control_request_t {
  uint8_t bmRequestType;        // +0
  uint8_t bRequest;             // +1
  ule16_t wValue;               // +2
  ule16_t wIndex;               // +4
  ule16_t wLength;              // +6
} zusb_control_request_t;

//----------------------------------------------------------------------------
// USB Descriptors
//----------------------------------------------------------------------------

#define ZUSB_DESC_DEVICE                  0x01
#define ZUSB_DESC_CONFIGURATION           0x02
#define ZUSB_DESC_STRING                  0x03
#define ZUSB_DESC_INTERFACE               0x04
#define ZUSB_DESC_ENDPOINT                0x05
#define ZUSB_DESC_INTERFACE_ASSOCIATION   0x0b
#define ZUSB_DESC_CS_DEVICE               0x21
#define ZUSB_DESC_CS_CONFIGURATION        0x22
#define ZUSB_DESC_CS_STRING               0x23
#define ZUSB_DESC_CS_INTERFACE            0x24
#define ZUSB_DESC_CS_ENDPOINT             0x25

// USB Descriptor Common Header
typedef struct __attribute__((packed)) zusb_desc_header {
  uint8_t bLength;              // +0
  uint8_t bDescriptorType;      // +1
} zusb_desc_header_t;

// USB Device Descriptor (0x01)
typedef struct __attribute__((packed)) zusb_desc_device_t {
  uint8_t bLength;              // +0
  uint8_t bDescriptorType;      // +1
  ule16_t bcdUSB;               // +2
  uint8_t bDeviceClass;         // +4
  uint8_t bDeviceSubClass;      // +5
  uint8_t bDeviceProtocol;      // +6
  uint8_t bMaxPacketSize0;      // +7
  ule16_t idVendor;             // +8
  ule16_t idProduct;            // +10
  ule16_t bcdDevice;            // +12
  uint8_t iManufacturer;        // +14
  uint8_t iProduct;             // +15
  uint8_t iSerialNumber;        // +16
  uint8_t bNumConfigurations;   // +17
} zusb_desc_device_t;

// USB Configuration Descriptor (0x02)
typedef struct __attribute__((packed)) zusb_desc_configuration {
  uint8_t bLength;              // +0
  uint8_t bDescriptorType;      // +1
  ule16_t wTotalLength;         // +2
  uint8_t bNumInterfaces;       // +4
  uint8_t bConfigurationValue;  // +5
  uint8_t iConfiguration;       // +6
  uint8_t bmAttributes;         // +7
  uint8_t bMaxPower;            // +8
} zusb_desc_configuration_t;

// USB String Descriptor (0x03)
typedef struct __attribute__((packed)) zusb_desc_string {
  uint8_t bLength;              // +0
  uint8_t bDescriptorType;      // +1
  ule16_t bString[];            // +2
} zusb_desc_string_t;

// USB Interface Descriptor (0x04)
typedef struct __attribute__((packed)) zusb_desc_interface {
  uint8_t bLength;              // +0
  uint8_t bDescriptorType;      // +1
  uint8_t bInterfaceNumber;     // +2
  uint8_t bAlternateSetting;    // +3
  uint8_t bNumEndpoints;        // +4
  uint8_t bInterfaceClass;      // +5
  uint8_t bInterfaceSubClass;   // +6
  uint8_t bInterfaceProtocol;   // +7
  uint8_t iInterface;           // +8
} zusb_desc_interface_t;

// USB Endpoint Descriptor (0x05)
typedef struct __attribute__((packed)) zusb_desc_endpoint {
  uint8_t bLength;              // +0
  uint8_t bDescriptorType;      // +1
  uint8_t bEndpointAddress;     // +2
  uint8_t bmAttributes;         // +3
  ule16_t wMaxPacketSize;       // +4
  uint8_t bInterval;            // +6
} zusb_desc_endpoint_t;

/// USB Interface Association Descriptor (0x0b)
typedef struct __attribute__((packed)) zusb_desc_interface_assoc_t {
  uint8_t bLength;              // +0
  uint8_t bDescriptorType;      // +1
  uint8_t bFirstInterface;      // +2
  uint8_t bInterfaceCount;      // +3
  uint8_t bFunctionClass;       // +4
  uint8_t bFunctionSubClass;    // +5
  uint8_t bFunctionProtocol;    // +6
  uint8_t iFunction;            // +7
} zusb_desc_interface_assoc_t;

#ifdef __cplusplus
 }
#endif

#endif // _ZUSB_TYPES_H_
