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

#include <stdio.h>
#include <stdint.h>
#include <iconv.h>

/* ShiftJIS -> UTF-16 conversion */
static int s2u(int s)
{
  uint8_t inbuf[2];
  uint8_t outbuf[2];
  char *src_buf = inbuf;
  char *dst_buf = outbuf;
  size_t src_len;
  size_t dst_len = sizeof(outbuf);

  if (s < 0x100) {
    inbuf[0] = s;
    src_len = 1;
  } else {
    inbuf[0] = s >> 8;
    inbuf[1] = s;
    src_len = 2;
  }

  int res;
  iconv_t cd = iconv_open("UTF-16BE", "CP932");
  res = iconv(cd, &src_buf, &src_len, &dst_buf, &dst_len);
  iconv_close(cd);

  if (res < 0)
    return -1;
  return (outbuf[0] << 8) | outbuf[1];
}

/* UTF-16 -> ShiftJIS conversion */
static int u2s(int u)
{
  uint8_t inbuf[2];
  uint8_t outbuf[2];
  char *src_buf = inbuf;
  char *dst_buf = outbuf;
  size_t src_len;
  size_t dst_len = sizeof(outbuf);

  inbuf[0] = u >> 8;
  inbuf[1] = u;
  src_len = 2;

  int res;
  iconv_t cd = iconv_open("CP932", "UTF-16BE");
  res = iconv(cd, &src_buf, &src_len, &dst_buf, &dst_len);
  iconv_close(cd);

  if (res < 0)
    return -1;
  if (dst_len == sizeof(outbuf) - 1)
    return outbuf[0];
  else
    return (outbuf[0] << 8) | outbuf[1];
}

static void create_upper(FILE *fp, int8_t *ix, int (*conv)(int))
{
  int p = 0;
	for (int i = 0; i < 0x100; i++) {
    int j;
    ix[i] = -1;
    for (j = 0; j < 0x100; j++) {
        if (conv(i * 256 + j) >= 0)
          break;
    }
    if (j < 0x100)
      ix[i] = p++;
    if ((i % 16) == 0) fprintf(fp, "    ");
    fprintf(fp, "%3d, ", ix[i]);
    if ((i % 16) == 15) fprintf(fp, " /* 0x%02x00 */\n", i & 0xf0);
  }
}

static void create_lower(FILE *fp, int8_t *ix, int (*conv)(int))
{
  for (int i = 0; i < 0x100; i++) {
    if (ix[i] < 0)
      continue;
    fprintf(fp, "  {\n");
    for (int j = 0; j < 0x100; j++) {
      int x;
      if ((j % 16) == 0) fprintf(fp, "    ");
      if ((x = conv(i * 256 + j)) >= 0) {
        fprintf(fp, "0x%04x, ", x);
      } else {
        fprintf(fp, "     0, ");
      }
      if ((j % 16) == 15) fprintf(fp, " /* 0x%04x */\n", i * 256 + (j & 0xf0));
    }
    fprintf(fp, "  },\n");
  }
}

int main()
{
  FILE *fp;
  int8_t ix[256];

  fp = fopen("iconv_table.h", "w");
  fprintf(fp, "/* automatically created by createtable.c */\n");
  fprintf(fp, "#include <stdint.h>\n\n");

  /* create ShiftJIS to UTF-16 table */

  fprintf(fp, "static int8_t s2u_upper[] = {\n");
  create_upper(fp, ix, s2u);
  fprintf(fp, "};\n\n");
  fprintf(fp, "static uint16_t s2u_lower[][256] = {\n");
  create_lower(fp, ix, s2u);
  fprintf(fp, "};\n\n");

  /* create UTF-16 to ShiftJIS table */

  fprintf(fp, "static int8_t u2s_upper[] = {\n");
  create_upper(fp, ix, u2s);
  fprintf(fp, "};\n\n");
  fprintf(fp, "static uint16_t u2s_lower[][256] = {\n");
  create_lower(fp, ix, u2s);
  fprintf(fp, "};\n\n");

  fclose(fp);

  return 0;
}
