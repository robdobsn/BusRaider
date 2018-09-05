
all: lissajous.c
	zcc +z80 -vn -O3 -startup=0 -clib=new lissajous.c -o lissajous.bin -lm -create-app
	curl http://192.168.86.89/targetcmd/cleartarget
	curl -F "file=@lissajous_CODE.bin" http://192.168.86.89/upload
	curl http://192.168.86.89/targetcmd/programtarget
	curl http://192.168.86.89/targetcmd/resettarget
