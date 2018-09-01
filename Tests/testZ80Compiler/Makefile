
COMPIL ?= zcc
CFLAGS = +trs80 -lndos -lm -create-app -subtype=disk

all: test.cmd
	zcc  +trs80 -lndos -lm -create-app -subtype=disk test.c -o test
	curl http://192.168.86.89/targetcmd/cleartarget
	curl -F "file=@test.cmd" http://192.168.86.89/upload
	curl http://192.168.86.89/targetcmd/programtarget
	curl http://192.168.86.89/targetcmd/resettarget

# %.cmd : %.c 
# 	@$(COMPIL) $(CFLAGS) $< -o $@
# 	@echo "Compiling $<"
