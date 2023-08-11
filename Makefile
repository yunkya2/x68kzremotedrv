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

RELFILE := x68kzrmthds-$(shell date +%Y%m%d)

release: build
	cp README.md build/README.txt
	(cd build; zip -r $(RELFILE).zip *.uf2 README.txt)

.PHONY: all clean update release
