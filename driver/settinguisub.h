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

#ifndef _SETTINGUISUB_H_
#define _SETTINGUISUB_H_

//****************************************************************************
// Definition
//****************************************************************************

#define min(a, b)     (((a) < (b)) ? (a) : (b))
#define max(a, b)     (((a) > (b)) ? (a) : (b))

struct itemtbl {
  int stat;
    // bit 19:    0x80000 この項目変更の反映には再起動が必要
    // bit 16:    0x10000 bit15-12が有効
    // bit 15-12: 0x0f000 hdsunitがこの値のとき表示
    // bit 11-8:  0x00f00 remoteunitがこの値のとき表示
    // bit 7:     0x00080 isupdconf()    更新後画面を再描画する
    // bit 6:     0x00040 isusetconf()   設定を反映させる
    // bit 5:     0x00020 bit11-8が有効
    // bit 4:     0x00010 istabstop()    TABでここまで飛ぶ
    // bit 3-0:   0x0000f sysstatusがこの値以上のとき表示
  int x;
  int y;
  int xn;
  const char *msg;
  const char *help1;
  const char *help2;
  const char *help3;
  int xd;
  int wd;
  char *value;
  int valuesz;
  int (*func)(struct itemtbl *);
  void *opt;
};

struct numlist_opt {
  int min;
  int max;
};

struct labellist_opt {
  int nlabels;
  const char **label;
};

//****************************************************************************
// Function prototype
//****************************************************************************

/* Communication */
void com_init(void);
void com_cmdres(void *wbuf, size_t wsize, void *rbuf, size_t rsize);

/* Drawing */
void drawframe(int x, int y, int w, int h, int c, int h2);
void drawframe2(int x, int y, int w, int h, int c, int h2);
void drawframe3(int x, int y, int w, int h, int c, int h2);
void drawhline(int x, int y, int w, int c);
void drawmsg(int x, int y, int c, const char *msg);
void drawvalue(int c, struct itemtbl *it, const char *s, int mask);
void drawhelp(int c, int x, int y, int w, const char *s);

/* Input */
int keyinp(int timeout);
int input_entry(struct itemtbl *it);
int input_passwd(struct itemtbl *it);
int input_numlist(struct itemtbl *it);
int input_labellist(struct itemtbl *it);
int input_wifiap(struct itemtbl *it);
int input_dirfile(struct itemtbl *it);

int topview(void);

#endif /* _SETTINGUISUB_H_ */
