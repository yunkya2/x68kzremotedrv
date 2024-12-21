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

    .global scsidrv_org
scsidrv_org:
    .long   0                   // 変更前のIOCS _SCSIDRV処理アドレス

    .global hdsscsi_mask
hdsscsi_mask:
    .byte   0                   // リモートHDSドライバのSCSI ID処理マスク

    .even

    .global scsidrv_hds
scsidrv_hds:
    cmpi.b  #0x20,%d1
    bcs     999f                // low level commandは変更前の処理を実行
    cmpi.b  #0x30,%d1
    bcc     999f

    moveq.l #1,%d0
    lsl.b   %d4,%d0
    and.b   %pc@(hdsscsi_mask),%d0
    beq     999f                // SCSI IDがHDSドライバのものでなければ変更前の処理を実行

    movem.l %d1-%d5/%a1-%a3,%sp@-
    jsr     hdsscsi
    movem.l %sp@+,%d1-%d5/%a1-%a3
    rts

999:                            // 変更前のIOCS _SCSIDRVを実行する
    move.l  %pc@(scsidrv_org),%sp@-
    rts

    .end
