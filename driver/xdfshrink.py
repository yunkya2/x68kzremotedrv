#!/usr/bin/env python3
import sys

if len(sys.argv) != 3:
    print("Usage: xdfshrink.py <input file> <output file>")
    sys.exit(1)

with open(sys.argv[1], 'rb') as f:
    with open(sys.argv[2], 'wb') as g:
        g.write(f.read().rstrip(b'\x00'))
