#!/usr/bin/env python3
import sys
import struct

if len(sys.argv) < 3:
    print("Usage: bind.py <output file> <input file>...")
    sys.exit(1)

data = bytearray()
offset = 0
flist = bytearray()
for f in sys.argv[2:]:
    with open(f, 'rb') as g:
        d = g.read()
        data += d

        fname, fext = f.rsplit('.', 1)
        fname1 = fname[:8].ljust(8)
        fname2 = fname[8:].ljust(10, '\0')
        fext = fext[:3].ljust(3)
        flist += fname1.encode() + fext.encode() + b'\x20' + fname2.encode()
        flist += b'\0' * (4 + 2) + struct.pack('>L', offset)

        offset += len(d)
data[0x3c:0x40] = struct.pack('>L', len(data))

with open(sys.argv[1], 'wb') as g:
    g.write(data + flist)
