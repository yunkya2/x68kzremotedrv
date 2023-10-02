#!/usr/bin/env python3
import sys

with open(sys.argv[1], 'rb') as f:
    data = bytearray(f.read())
    print("/* automatically created by bindump.py */")
    print("#include <stdint.h>")
    print("static const uint8_t "+sys.argv[2]+"[] = {")
    i = 0
    for a in data:
        if (i % 16) == 0:
            print("  ", end='')
        print("0x{:02x},".format(a), end='')
        if (i % 16) == 15:
            print("")
        i += 1
    print("")
    print("};")
