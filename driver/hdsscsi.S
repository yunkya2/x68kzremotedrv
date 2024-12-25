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
hds_selected:
    .byte   0                   // リモートHDSドライバのIDが選択されているか

    .even

    .global scsidrv_hds
scsidrv_hds:
    tst.b   %d1                 // _S_RESET
    beq     scsidrv_orgcmd

    cmpi.b  #0x03,%d1           // _S_SELECT, _S_SELECTA
    bcs     scsidrv_select

    cmpi.b  #0x09,%d1           // 0x03-0x08
    bcs     scsidrv_low

    cmpi.b  #0x0b,%d1           // _S_PHASE, _S_LEVEL
    bcs     scsidrv_orgcmd

    cmpi.b  #0x20,%d1
    bcs     scsidrv_low         // 0x0c-0x1f
scsidrv_command:
    moveq.l #1,%d0
    lsl.b   %d4,%d0
    and.b   %pc@(hdsscsi_mask),%d0
    beq     scsidrv_orgcmd      // SCSI IDがHDSドライバのものでなければ変更前の処理を実行
scsidrv_hook:
    movem.l %d1-%d5/%a1-%a3,%sp@-
    jsr     hdsscsi
    movem.l %sp@+,%d1-%d5/%a1-%a3
    rts

scsidrv_select:                 // セレクションフェーズのターゲットがHDSドライバか確認
    moveq.l #1,%d0
    lsl.b   %d4,%d0
    and.b   %pc@(hdsscsi_mask),%d0
    sne.b   %d0
    move.b  %d0,hds_selected    // 後続のlow level commandをHDSドライバが処理するか
    bne     scsidrv_hook        // HDSドライバが選択されている
    bra     scsidrv_orgcmd      // HDSドライバが選択されていない

scsidrv_low:                    // low level commandの実行
    tst.b   hds_selected
    bne     scsidrv_hook        // HDSドライバが選択されている
scsidrv_orgcmd:                 // 変更前のIOCS _SCSIDRVを実行する
    move.l  %pc@(scsidrv_org),%sp@-
    rts

    .end
