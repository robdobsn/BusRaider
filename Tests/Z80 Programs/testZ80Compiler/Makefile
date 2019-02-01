
all: test.c
	zcc +z80 -vn -O3 -startup=0 -clib=new test.c -o test.bin -lm -create-app
	curl http://192.168.86.89/targetcmd/cleartarget
	curl -F "file=@test_CODE.bin" http://192.168.86.89/uploadandrun

build: test.c
	zcc +z80 -vn -O3 -startup=0 -clib=new test.c -o test.bin -lm -create-app

# %.cmd : %.c 
# 	@$(COMPIL) $(CFLAGS) $< -o $@
# 	@echo "Compiling $<"
