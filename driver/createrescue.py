#!/usr/bin/env python3
import sys
from struct import unpack, pack

with open(sys.argv[1], 'rb') as f1, open(sys.argv[2], 'rb') as f2:
    boot = f1.read()
    settingui = f2.read()

    xdf = bytearray(1024 * 2 * 8 * 77)
    xdf[0:len(boot)] = boot
    xdf[1024:1024 + len(settingui)] = settingui

    with open(sys.argv[3], 'wb') as g:
        g.write(xdf)
