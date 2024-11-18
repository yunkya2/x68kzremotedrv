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

#ifndef _SCSI_CMD_H_
#define _SCSI_CMD_H_

#include <stdint.h>

#define ATTR_PACKED   __attribute__ ((packed))

#define SCSI_CMD_TEST_UNIT_READY          0x00
#define SCSI_CMD_REZERO_UNIT              0x01
#define SCSI_CMD_REQUEST_SENSE            0x03
#define SCSI_CMD_FORMAT_UNIT              0x04
#define SCSI_CMD_INQUIRY                  0x12
#define SCSI_CMD_READ_FORMAT_CAPACITIES   0x23
#define SCSI_CMD_READ_CAPACITY_10         0x25
#define SCSI_CMD_READ_10                  0x28
#define SCSI_CMD_WRITE_10                 0x2a
#define SCSI_CMD_MODE_SENSE_10            0x5a

/*
 * SCSI command structure definition
 */

// 0x00: TEST UNIT READY

typedef struct ATTR_PACKED scsi_test_unit_ready {
  uint8_t cmd_code;
  uint8_t lun;
  uint8_t _reserved1[3];
  uint8_t control;
} scsi_test_unit_ready_t;

// 0x01: REZERO UNIT

typedef struct ATTR_PACKED scsi_rezero_unit {
  uint8_t cmd_code;
  uint8_t lun;
  uint8_t _reserved1[4];
} scsi_rezero_unit_t;

// 0x03: REQUEST SENSE

typedef struct ATTR_PACKED scsi_request_sense {
  uint8_t cmd_code;
  uint8_t _reserved1;
  uint8_t page_code;
  uint8_t _reserved2;
  uint8_t alloc_length;
  uint8_t control;
} scsi_request_sense_t;

typedef struct ATTR_PACKED scsi_request_sense_resp {
  uint8_t response_code;
  uint8_t _reserved1;
  uint8_t sense_key;
  uint32_t information;
  uint8_t add_sense_len;
  uint32_t command_specific_info;
  uint8_t add_sense_code;
  uint8_t add_sense_qualifier;
  uint8_t field_replaceable_unit_code;
  uint8_t sense_key_specific[3];
} scsi_request_sense_resp_t;

// 0x04: FORMAT UNIT  (UFI specific)

typedef struct ATTR_PACKED scsi_format_unit {
  uint8_t cmd_code;
  uint8_t defect_list_format;
  uint8_t track_number;
  uint16_t interleave;
  uint8_t _reserved1[2];
  uint16_t alloc_length;
  uint8_t _reserved2[3];
} scsi_format_unit_t;

typedef struct ATTR_PACKED scsi_format_unit_param {
  uint8_t _reserved1;
  uint8_t flag;
  uint16_t defect_list_length;

  uint32_t block_num;
  uint8_t _reserved2[2];
  uint16_t block_size;
} scsi_format_unit_param_t;

// 0x12: INQUIRY

typedef struct ATTR_PACKED scsi_inquiry {
  uint8_t cmd_code;
  uint8_t _reserved1;
  uint8_t page_code;
  uint8_t _reserved2;
  uint8_t alloc_length;
  uint8_t control;
} scsi_inquiry_t;

typedef struct ATTR_PACKED scsi_inquiry_resp {
  uint8_t peripheral_device_type;
  uint8_t is_removable;
  uint8_t version;
  uint8_t response_data_format;
  uint8_t additional_length;
  uint8_t flag_5;
  uint8_t flag_6;
  uint8_t flag_7;
  uint8_t vendor_id[8];
  uint8_t product_id[16];
  uint8_t product_rev[4];
} scsi_inquiry_resp_t;

// 0x23: READ FORMAT CAPACITIIES  (UFI specific)

typedef struct ATTR_PACKED scsi_read_format_capacities {
  uint8_t cmd_code;
  uint8_t _reserved1;
  uint8_t _reserved2[5];
  uint16_t alloc_length;
  uint8_t control;
} scsi_read_format_capacities_t;

typedef struct ATTR_PACKED scsi_read_format_capacities_resp {
  uint8_t _reserved1[3];
  uint8_t list_length;

  uint32_t block_num;
  uint8_t descriptor_type;
  uint8_t _reserved2;
  uint16_t block_size;
} scsi_read_format_capacities_resp_t;

// 0x25: READ CAPACITY (10)

typedef struct ATTR_PACKED scsi_read_capacity10 {
  uint8_t cmd_code;
  uint8_t _reserved1;
  uint32_t lba;
  uint8_t _reserved2[2];
  uint8_t partial_medium_indicator;
  uint8_t control;
} scsi_read_capacity10_t;

typedef struct ATTR_PACKED scsi_read_capacity10_resp {
  uint32_t last_lba;
  uint32_t block_size;
} scsi_read_capacity10_resp_t;

// 0x28: READ (10)

typedef struct ATTR_PACKED scsi_read10 {
  uint8_t cmd_code;
  uint8_t _reserved1;
  uint32_t lba;
  uint8_t _reserved2;
  uint16_t block_count;
  uint8_t control;
} scsi_read10_t;

// 0x2a: WRITE (10)

typedef struct ATTR_PACKED scsi_write10 {
  uint8_t cmd_code;
  uint8_t _reserved1;
  uint32_t lba;
  uint8_t _reserved2;
  uint16_t block_count;
  uint8_t control;
} scsi_write10_t;

// 0x5a: MODE SENSE (10)

typedef struct ATTR_PACKED scsi_mode_sense10 {
  uint8_t  cmd_code;
  uint8_t  _reserved1;
  uint8_t  page_code;
  uint8_t  subpage_code;
  uint8_t  _reserved2[3];
  uint16_t alloc_length;
  uint8_t control;
} scsi_mode_sense10_t;

typedef struct ATTR_PACKED scsi_mode_sense10_resp {
  uint16_t mode_data_length;
  uint8_t  medium_type_code;
  uint8_t  wp_flag;
  uint8_t  _reserved1[4];
} scsi_mode_sense10_resp_t;

#endif  // _SCSI_CMD_H_
