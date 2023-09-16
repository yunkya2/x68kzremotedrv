#!/usr/bin/env python3
import sys
from struct import unpack, pack

with open(sys.argv[1], 'wb') as f:
    data = bytearray(512)
    data[0:16] = pack('<4L', 0x0a324655, 0x9e5d5157, 0x00002000, 0x101f0000)
    data[16:32] = pack('<4L', 256, 0, 1, 0xe48bff56)
    data[508:512] = pack('<L', 0x0ab16f30)
    f.write(data)
