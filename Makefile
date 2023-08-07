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
	-rm -rf build

build: FreeRTOS-Kernel/.git libsmb2/.git pico-sdk/.git

FreeRTOS-Kernel/.git libsmb2/.git pico-sdk/.git update:
	git submodule update --init --recursive

.PHONY: all clean update
