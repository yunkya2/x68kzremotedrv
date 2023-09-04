/*
 * Copyright (c) 2023 Yuichi Nakamura (@yunkya2)
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
#include <stddef.h>
#include "iconv_table.h"

int iconv_s2u(char **src_buf, size_t *src_len,
              char **dst_buf, size_t *dst_len)
{
  while ((*src_len) > 0) {
    uint16_t s;
    uint8_t c = *(*src_buf)++;
    (*src_len)--;
    if ((c >= 0x80 && c <= 0x9f) || (c >= 0xe0 && c <= 0xff)) {
      if ((*src_len) <= 0)
        return -1;
      uint8_t c2 = *(*src_buf)++;
      (*src_len)--;
      s = c << 8 | c2;
    } else {
      s = c;
    }

    if (s2u_upper[s >> 8] < 0)
      return -1;
    uint16_t u = s2u_lower[s2u_upper[s >> 8]][s & 0xff];
    if (s != 0 && u == 0)
      return -1;

    if (u <= 0x7f) {
      if ((*dst_len) < 1)
        return -1;
      *(*dst_buf)++ = u;
      (*dst_len)--;
    } else if (u <= 0x7ff) {
      if ((*dst_len) < 2)
        return -1;
      *(*dst_buf)++ = ((u >> 6) & 0x1f) | 0xc0;
      *(*dst_buf)++ = (u & 0x3f) | 0x80;
      (*dst_len) -= 2;
    } else {
      if ((*dst_len) < 3)
        return -1;
      *(*dst_buf)++ = ((u >> 12) & 0x0f) | 0xe0;
      *(*dst_buf)++ = ((u >> 6) & 0x3f) | 0x80;
      *(*dst_buf)++ = (u & 0x3f) | 0x80;
      (*dst_len) -= 3;
    }
  }
  return 0;
}

int iconv_u2s(char **src_buf, size_t *src_len,
              char **dst_buf, size_t *dst_len)
{
  while ((*src_len) > 0) {
    uint16_t u;
    uint8_t c = *(*src_buf)++;
    (*src_len)--;
    if (c < 0x80) {
      u = c;
    } else if (c >= 0xc2 && c < 0xe0) {
      if ((*src_len) < 1)
        return -1;
      uint8_t c2 = *(*src_buf)++;
      (*src_len)--;
      if (c2 >= 0x80 && c2 < 0xc0) {
        u = ((c & 0x1f) << 6) | (c2 & 0x3f);
      } else {
        return -1;
      }
    } else if (c >= 0xe0 && c < 0xf0) {
      if ((*src_len) < 2)
        return -1;
      uint8_t c2 = *(*src_buf)++;
      uint8_t c3 = *(*src_buf)++;
      (*src_len) -= 2;
      if ((c2 >= 0x80 && c2 < 0xc0) && (c3 >= 0x80 && c3 < 0xc0)) {
        u = ((c & 0x0f) << 12) | ((c2 & 0x3f) << 6) | (c3 & 0x3f);
      } else {
        return -1;
      }
    } else {
      return -1;
    }

    if (u2s_upper[u >> 8] < 0)
      return -1;
    uint16_t s = u2s_lower[u2s_upper[u >> 8]][u & 0xff];

    if (u != 0 && s == 0)
      return -1;
    if (s < 0x100) {
      if ((*dst_len) < 1)
        return -1;
      *(*dst_buf)++ = s;
      (*dst_len)--;
    } else {
      if ((*dst_len) < 2)
        return -1;
      *(*dst_buf)++ = s >> 8;
      *(*dst_buf)++ = s & 0xff;
      (*dst_len) -= 2;
    }
  }
  return 0;
}
