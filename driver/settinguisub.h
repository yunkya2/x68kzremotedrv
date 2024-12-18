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
    // bit 11-8: 0xf00 remoteunitがこの値のとき表示
    // bit 7:   0x80 isupdconf()    更新後画面を再描画する
    // bit 6:   0x40 isusetconf()   設定を反映させる
    // bit 5:   0x20 remoteunit
    // bit 4:   0x10 istabstop()    TABでここまで飛ぶ
    // bit 3-0: 0x0f sysstatusがこの値以上のとき表示
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
  int (*func)(struct itemtbl *, void *v);
  void *opt;
};

struct numlist_opt {
  int min;
  int max;
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
int input_entry(struct itemtbl *it, void *v);
int input_passwd(struct itemtbl *it, void *v);
int input_numlist(struct itemtbl *it, void *v);
int input_wifiap(struct itemtbl *it, void *v);
int input_dirfile(struct itemtbl *it, void *v);

int topview(void);

#endif /* _SETTINGUISUB_H_ */
