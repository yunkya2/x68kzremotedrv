#!/usr/bin/env python3
import sys
from struct import unpack, pack

if len(sys.argv) != 3:
    print("Usage: fixupsys.py [SYS file] [output file]")
    print("Device driver fixup tool to embed into the SCSI disk image")
    sys.exit(1)

with open(sys.argv[1], 'rb') as f:
    data = bytearray(f.read())
    f.seek(12)
    (textsz, datasz, bsssz, relsz) = unpack('>4L', f.read(16))
    data[0x5a:0x5a + 12] = pack('>3L', textsz + datasz, relsz, bsssz)
    with open(sys.argv[2], 'wb') as g:
        g.write(data[0x40:])
