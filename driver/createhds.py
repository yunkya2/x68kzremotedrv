#!/usr/bin/env python3
import sys

if len(sys.argv) != 5:
    print("Usage: createhds.py [output file] [bootloader] [HUMAN.SYS] [device driver]")
    print("Create SCSI disk image file")
    sys.exit(1)

# Create a 1MB hard disk image
img=bytearray(1024*1024*1)

# Add a X68k SCSI HDD signature
img[0:8]=b'X68SCSI1'
img[16:32]=b'ZUSBRMTDRV boot '

# Add a simple bootloader
s=1024*1
with open(sys.argv[2],'rb') as f:
   l=f.seek(0,2)
   f.seek(0)
   img[s:s+l]=f.read()

# Add a X68k partition table
s=1024*2
img[s:s+4]=b'X68K'
img[s+16:s+16+8]=b'Human68k'

# Add Human68k image
s=0x8000
with open(sys.argv[3],'rb') as f:
   l=f.seek(0,2)
   f.seek(0)
   img[s:s+l]=f.read()

# Add device driver image
s=1024*3
with open(sys.argv[4],'rb') as f:
   l=f.seek(0,2)
   f.seek(0)
   img[s:s+l]=f.read()

# Create HDS image file

with open(sys.argv[1],'wb') as f:
    f.write(img)
