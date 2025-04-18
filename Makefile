#
# Copyright (c) 2025 Yuichi Nakamura (@yunkya2)
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

GIT_REPO_VERSION=$(shell git describe --tags --always)

all: build
	(cd build; make -j)

build:
	-rm -rf build
	mkdir build
	(cd build; cmake ..)

build-debug:
	-rm -rf build
	mkdir build
	(cd build; cmake -DCMAKE_BUILD_TYPE=Debug ..)

clean:
	(cd driver; make clean)
	-rm -rf build

build: FreeRTOS-Kernel/.git libsmb2/.git pico-sdk/.git

FreeRTOS-Kernel/.git libsmb2/.git pico-sdk/.git update:
	git submodule update --init --recursive

RELFILE := x68kzremotedrv-$(GIT_REPO_VERSION)

release: all
	./md2txtconv.py README.md && mv README.txt build
	./md2txtconv.py Reference.md && mv Reference.txt build
	cp QUICKSTART.pdf build
	cp driver/zremotetools.xdf build
	mkdir build/tools && cp driver/zremote.x driver/zremotedrv.sys driver/zremoteimg.sys build/tools
	(cd build; zip -r ../$(RELFILE).zip *.uf2 README.txt Reference.txt *.pdf *.xdf tools)

.PHONY: all clean update release
