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

RELFILE := x68kzremotedrv-$(shell date +%Y%m%d)

release: all
	./md2txtconv.py README.md
	mv README.txt build
	cp QUICKSTART.pdf build
	cp driver/*.uf2 build
	cp driver/*.xdf build
	(cd build; zip -r ../$(RELFILE).zip *.uf2 README.txt *.pdf *.xdf)

.PHONY: all clean update release
