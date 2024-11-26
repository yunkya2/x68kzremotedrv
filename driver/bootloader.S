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

    .text

/* HUMAN.SYS simple bootloader */

entry:
    bra.w   start

sysstat:
    .word   0                   /* system status */

start:
#if 0
    move.w  %pc@(sysstat),%d0
    cmpi.w  #5,%d0              /* STAT_CONFIGURED */
    blt     loadsetting         /* not configured -> setting UI */

    moveq.l #0x04,%d0           /* B_BITSNS */
    moveq.l #0x0c,%d1
    trap    #15
    andi.b  #0x08,%d0           /* check F1 key */
    bne     loadsetting
#endif

loadhuman:
    move.l  #(0x8000/512),%d2   /* HUMAN.SYS */
    bra     1f

loadsetting:
    move.l  #(0x20000/512),%d2  /* Setting UI menu */
    moveq.l #6,%d4              /* SCSI ID 6 */

1:
    moveq.l #1,%d3
    moveq.l #1,%d5
    movea.w #0x67c0.w,%a1
    move.l  #0xf5,%d0           /* SCSIDRV */
    moveq.l #0x26,%d1           /* S_READEXT */
    trap    #15                 /* Read the first sector */

    cmpi.w  #0x4855,0x67c0.w
    bne     error

    move.l  0x67cc.w,%d3        /* text size */
    add.l   0x67d0.w,%d3        /* + data size */
    add.l   #(0x40+511),%d3     /* round up to the sector size */
    lsr.l   #8,%d3
    lsr.l   #1,%d3              /* # of sectors */
    movea.w #0x67c0.w,%a1
    move.l  #0xf5,%d0           /* SCSIDRV */
    moveq.l #0x26,%d1           /* S_READEXT */
    trap    #15                 /* Read HUMAN.SYS */

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
    jmp     %a0@                /* HUMAN.SYS entry */

error:
    lea.l   %pc@(errormsg),%a1
    moveq.l #0x21,%d0
    trap    #15
4:
    bra     4b

errormsg:
    .byte   0x1b
    .ascii  "[47m"
    .byte   0x1b
    .ascii  "[13;26H                                             "
    .byte   0x1b
    .ascii  "[14;26H                                             "
    .byte   0x1b
    .ascii  "[14;35HHuman.sys "
    .byte   0x82,0xaa,0x20
    .byte   0x8c,0xa9,0x82,0xc2,0x82,0xa9,0x82,0xe8,0x82,0xdc,0x82,0xb9,0x82,0xf1
    .byte   0

    .end
