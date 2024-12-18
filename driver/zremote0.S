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

    .text

    .global		_start
_start:

    lea.l   %a0@(0x0010),%a0    // PDBアドレス
    suba.l  %a0,%a1
    movem.l %a0-%a1,%sp@-
    .short  0xff4a              // DOS _SETBLOCK
    addq.l  #8,%sp

    lea.l   %pc@(filename),%a3
    lea.l   %a0@(0x0070),%a4    // 実行ファイルのドライブ名+パス名
1:
    move.b  %a4@+,%a3@+
    bne     1b
    subq.l  #1,%a3
    lea.l   %a0@(0x00b4),%a4    // 実行ファイルのファイル名
2:
    move.b  %a4@+,%a3@+
    bne     2b

    clr.l   %sp@-               // 環境ポインタ
    move.l  %a2,%sp@-           // コマンドライン
    pea.l   %pc@(filename)      // 実行ファイル名
    move.w  #0x0200,%sp@-       // モジュール2を起動 (settingui)

    addq.l  #1,%a2
5:
    move.b  %a2@+,%d0
    beq     6f
    cmp.b   #' ',%d0
    beq     5b
    cmp.b   #'\t',%d0
    beq     5b
    move.w  #0x0100,%sp@        // モジュール1を起動 (zremotecli)
6:
    .short  0xff4b              // DOS _EXEC    
    lea.l   %sp@(14),%sp

    .short  0xff00              // DOS _EXIT

filename:
    .space  128

    .end
