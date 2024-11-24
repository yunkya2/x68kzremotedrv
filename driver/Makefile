#
# Copyright (c) 2023 Yuichi Nakamura (@yunkya2)
#
# The MIT License (MIT)
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

CROSS = m68k-xelf-
CC = $(CROSS)gcc
AS = $(CROSS)gcc
LD = $(CROSS)gcc
AR = $(CROSS)ar
RANLIB = $(CROSS)ranlib
OBJCOPY = $(CROSS)objcopy

GIT_REPO_VERSION=$(shell git describe --tags --always)

RMTINC = ../x68kserremote/include
RMTSRC = ../x68kserremote/driver

vpath %.h $(RMTINC):$(RMTSRC)
vpath %.c $(RMTSRC)
vpath %.S $(RMTSRC)

CFLAGS = -g -m68000
CFLAGS += -I. -I../include
CFLAGS += -I $(RMTINC)
CFLAGS += -I $(RMTSRC)
CFLAGS +=  -Os -DGIT_REPO_VERSION=\"$(GIT_REPO_VERSION)\"
CFLAGS += -finput-charset=utf-8 -fexec-charset=cp932
CFLAGS += $(CFLAGS_XTEST)
ASFLAGS = -m68000 -I. -I../include

ifneq ($(DEBUG),)
CFLAGS += -DDEBUG
endif

all: bootloader.inc hdsboot.inc clrconfig.uf2

uitest:
	$(MAKE) CFLAGS_XTEST=-DXTEST clean settingui.x

zrmtrescue.xdf: rescueboot.bin settingui.bin
	./createrescue.py $^ $@

head_rmt.o: head.S
	$(AS) $(ASFLAGS) -DCONFIG_DEVNAME=\"\\x01ZUSBRMT\" -DCONFIG_DEVTYPE=0x2000 -c -o $@ $<

head_rmt_boot.o: head.S
	$(AS) $(ASFLAGS) -DCONFIG_DEVNAME=\"\\x01ZUSBRMT\" -DCONFIG_DEVTYPE=0x2000 -DCONFIG_BOOTDRIVER -c -o $@ $<

remotedrv.o:  ../include/config.h remotedrv.h x68kremote.h
settingui.o:  ../include/config.h ../include/vd_command.h settinguipat.h settinguisub.h
settinguisub.o:  ../include/config.h ../include/vd_command.h settinguipat.h settinguisub.h

bootloader.bin: bootloader.o
	$(OBJCOPY) -O binary $< $@

hdsboot.bin: hdsboot.o
	$(OBJCOPY) -O binary $< $@

rescueboot.bin: rescueboot.o
	$(OBJCOPY) -O binary $< $@

settingui.bin: settingui.o settinguisub.o
	$(LD) -o $@ $^ -Ttext=0x6800 -s -specs=x68knodos.specs

settingui.x: settingui.o settinguisub.o
	$(LD) -o $@ $^

clrconfig.uf2: clrconfig.py
	./clrconfig.py $@

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(AS) $(ASFLAGS) -c -o $@ $<

%.inc: %.bin
	./bindump.py $< $(basename $@) > $@

clean:
	-rm -f *.o *.x *.elf* *.sys *.bin *.inc *.uf2 *.xdf

.PHONY: all clean uitest
