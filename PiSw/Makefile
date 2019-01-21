
ARMGNU ?= arm-none-eabi
CFLAGS = -mcpu=arm6 -mfpu=vfp -Wall -Wextra -O3 -g -nostdlib -nostartfiles -fno-stack-limit -ffreestanding -mfloat-abi=hard
CPPFLAGS = -fno-rtti -fno-exceptions -fno-sized-deallocation


## Important!!! vectors.o must be the first object to be linked!
OOB = vectors.o pi_bus_raider.o busraider.o systemfont.o target_memory_map.o cmd_handler.o \
			uart.o irq.o utils.o framebuffer.o postman.o wgfx.o dma.o nmalloc.o \
			uspios_wrapper.o ee_printf.o raspihwconfig.o timer.o piwiring.o rdutils.o \
			minihdlc.o srecparser.o jsmnR.o rdstdcpp.o \
			McBase.o McManager.o \
			McTRS80.o mc_trs80l3font.o \
			McRobsZ80.o McDebugZ80.o McTRS80CmdFormat.o \
			McZXSpectrum.o McZXSpectrumTZXFormat.o \
			McTerminal.o

BUILD_DIR = build
SRC_DIR = src
BUILD_VERSION = $(shell git describe --all --long | cut -d "-" -f 3)

OBJS=$(patsubst %.o,$(BUILD_DIR)/%.o,$(OOB))

LIBGCC="$(shell $(ARMGNU)-gcc -print-libgcc-file-name)"
LIBUSPI=uspi/lib/libuspi.a

all: pi_bus_raider.elf kernel list
	$(MAKE) -C ./uspi/lib 
	arm-none-eabi-objcopy --srec-forceS3 pi_bus_raider.elf -O srec pi_bus_raider.srec

run: pi_bus_raider.img kernel
	curl -F "file=@pi_bus_raider.img" http://192.168.86.192/uploadpisw

kernel: pi_bus_raider.img
ifdef OS
	copy pi_bus_raider.img bin\kernel.img
else
	cp pi_bus_raider.img bin/kernel.img
endif

list: pi_bus_raider.elf
	$(ARMGNU)-objdump -D pi_bus_raider.elf > pi_bus_raider.list

debug: pi_bus_raider.elf
	cd JTAG && ./run_gdb.sh

$(BUILD_DIR)/%.o : $(SRC_DIR)/%.c	
	@$(ARMGNU)-gcc $(CFLAGS) -c $< -o $@
	@echo "CC $<"

$(BUILD_DIR)/%.o : $(SRC_DIR)/%.cpp
	@$(ARMGNU)-gcc $(CFLAGS) $(CPPFLAGS) -c $< -o $@
	@echo "CC $<"

$(BUILD_DIR)/%.o : $(SRC_DIR)/%.s 
	@$(ARMGNU)-as $< -o $@
	@echo "AS $<"

%.img : %.elf 
	@$(ARMGNU)-objcopy $< -O binary $@
	@echo "OBJCOPY $< -> $@"

$(LIBUSPI):
	$(MAKE) -C ./uspi/lib 

pi_bus_raider.elf : $(OBJS) $(LIBUSPI) 
	@$(ARMGNU)-ld $(OBJS) $(LIBGCC) $(LIBUSPI) -T memmap -o $@
	@echo "LD $@"

.PHONY clean :
ifdef OS
	del $(BUILD_DIR)\*.o
	del *.elf
	del *.img
else
	rm -f $(BUILD_DIR)/*.o
	rm -f *.elf
	rm -f *.img
endif
