#!/usr/bin/env python3
import sys
from struct import unpack, pack

def s2u(s):
    try:
        enc = 'B' if s < 0x100 else 'H'
        (d,) = unpack('>H', pack('>'+enc, s).decode('ms932').encode('utf-16be'))
    except:
        return -1
    return d

def u2s(u):
    try:
        s = pack('>H', u).decode('utf-16be').encode('ms932')
        enc = 'B' if len(s) == 1 else 'H'
        (d,) = unpack('>'+enc, s)
    except:
        return -1
    return d

def getindex(f):
    index = []
    p = 0
    for h in range(0, 0x100):
        for c in range(h * 256, (h * 256) + 0x100):
            if f(c) >= 0:
                index.append(p)
                p += 1
                break
        else:
            index.append(-1)
    return index

def dump(name):
    f = eval(name)
    print('static int8_t '+name+'_upper[] = {')

    index = getindex(f)
    for h in range(0, 0x100):
        if h % 16 == 0:
            print('    ', end='')
        print('{:3d}, '.format(index[h]), end='')
        if h % 16 == 15:
            print(' /* 0x{:04x} */'.format((h * 256) & 0xf000))

    print('};')
    print()

    print('static uint16_t '+name+'_lower[][256] = {')
    for h in range(0, 0x100):
        if index[h] < 0:
            continue
        print("  {")
        for c in range(h * 256, (h * 256) + 0x100):
            if c % 16 == 0:
                print('    ', end='')
            x = f(c)
            if x < 0:
                print('  {:4d}, '.format(0), end='')
            else:
                print('0x{:04x}, '.format(x), end='')
            if c % 16 == 15:
                print(' /* 0x{:04x} */'.format(c & 0xfff0))
        print("  },")
    print('};')

if __name__ == '__main__':
    print('/* automatically created by createtable.py */')
    print('#include <stdint.h>')
    print()
    dump('s2u')
    dump('u2s')
