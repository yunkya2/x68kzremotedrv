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

    .text

/* FD boot 1st bootloader for rescue disk */

entry:
    bra.w   start
    .space  4*15

start:
    lea.l   0xe8e000,%a0
    move.b  #'Z',%a0@
    cmp.b   #'X',%a0@
    bne     error_noz

    lea.l   0xea0020,%a0
    movea.l %a0@,%a0
    movea.l %a0@(-12),%a1
    move.l  #0x80,%d0           /* B_INTVCS */
    move.l  #0x1f5,%d1          /* 0x100 + _SCSIDRV */
    trap    #15
    move.l  #0xf5,d0            /* SCSIDRV */
    moveq.l #0,%d1              /* S_RESET */
    trap    #15

    move.l  #0x8e,%d0           /* BOOTINF */
    trap    #15
    lsl.w   #8,%d0
    moveq.l #0x70,%d7
    or.w    %d0,%d7

    move.l  %d7,%d1
    move.l  #0x03000002,%d2     /* 1024bytes/sector, track #0, side #0, sector #2 */
    move.l  #0x400,%d3
    movea.w #0x67c0.w,%a1
    moveq.l #0x46,%d0           /* B_READ */
    trap    #15                 /* Read the first sector */

    cmpi.w  #0x4855,0x67c0.w
    bne     error

    move.l  0x67cc.w,%d3        /* text size */
    add.l   0x67d0.w,%d3        /* + data size */
    add.l   #0x40,%d3           /* + header size */
    movea.w #0x67c0.w,%a1
    move.l  %d7,%d1
    move.l  #0x03000002,%d2     /* 1024bytes/sector, track #0, side #0, sector #2 */
    moveq.l #0x46,%d0           /* B_READ */
    trap    #15                 /* Read Setting UI */

    movea.l 0x67c4.w,%a0        /* start address (0x6800) */
    adda.l  0x67cc.w,%a0        /* + text size */
    adda.l  0x67d0.w,%a0        /* + data size -> bss address */
    move.l  0x67d4.w,%d0        /* bss size */
    addq.l  #3,%d0
    lsr.l   #2,%d0              /* long word count */
    bra     3f
2:
    clr.l   %a0@+               /* clear bss */
3:
    dbra    %d0,2b

    movea.l 0x67c8.w,%a0        /* entry address */
    jmp     %a0@                /* Setting UI entry */

error:
    lea.l   %pc@(errormsg),%a1
    moveq.l #0x21,%d0
    trap    #15
4:
    bra     4b

error_noz:
    lea.l   %pc@(error_nozmsg),%a1
    moveq.l #0x21,%d0
    trap    #15
5:
    bra     5b

errormsg:
    .byte   0x1b
    .ascii  "[47m"
    .byte   0x1b
    .ascii  "[13;26H                                             "
    .byte   0x1b
    .ascii  "[14;26H                                             "
    .byte   0x1b
    .ascii  "[14;35Hsettingui "
    .byte   0x82,0xaa,0x20
    .byte   0x8c,0xa9,0x82,0xc2,0x82,0xa9,0x82,0xe8,0x82,0xdc,0x82,0xb9,0x82,0xf1
    .byte   0

error_nozmsg:
    .byte   0x1b
    .ascii  "[47m"
    .byte   0x1b
    .ascii  "[13;26H                                             "
    .byte   0x1b
    .ascii  "[14;26H                                             "
    .byte   0x1b
    .ascii  "[14;33HX68000 Z "
    .byte   0x88,0xC8,0x8A,0x4F,0x82,0xC5,0x82,0xCD,0x8B,0x4E,0x93,0xAE,0x82,0xC5
    .byte   0x82,0xAB,0x82,0xDC,0x82,0xB9,0x82,0xF1
    .byte   0

    .end
