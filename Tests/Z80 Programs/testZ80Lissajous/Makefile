
all: lissajous.c
	zcc +z80 -vn -O3 -startup=0 -clib=new lissajous.c -o lissajous.bin -lm -create-app
	curl http://192.168.86.67/targetcmd/cleartarget
	curl -F "file=@lissajous_CODE.bin" http://192.168.86.67/uploadandrun

build: lissajous.c
	zcc +z80 -vn -O3 -startup=0 -clib=new lissajous.c -o lissajous.bin -lm -create-app
