
ARMGNU ?= arm-none-eabi
CFLAGS = -Wall -Wextra -O3 -g -nostdlib -nostartfiles -fno-stack-limit -ffreestanding -mfloat-abi=hard


## Important!!! asm.o must be the first object to be linked!
OOB = asm.o pi_bus_raider.o busraider.o systemfont.o mc_generic.o mc_trs80.o mc_trs80l3font.o target_memory_map.o cmd_handler.o uart.o irq.o utils.o framebuffer.o postman.o wgfx.o dma.o nmalloc.o uspios_wrapper.o ee_printf.o raspihwconfig.o timer.o piwiring.o rdutils.o minihdlc.o srecparser.o

BUILD_DIR = build
SRC_DIR = src
BUILD_VERSION = $(shell git describe --all --long | cut -d "-" -f 3)

OBJS=$(patsubst %.o,$(BUILD_DIR)/%.o,$(OOB))

LIBGCC="$(shell $(ARMGNU)-gcc -print-libgcc-file-name)"
LIBUSPI=uspi/lib/libuspi.a

all: pi_bus_raider.elf pi_bus_raider.hex kernel list
	$(MAKE) -C ./uspi/lib 
	ctags src/.
	arm-none-eabi-objcopy --srec-forceS3 pi_bus_raider.elf -O srec pi_bus_raider.srec

run: pi_bus_raider.elf
	./launch_qemu.bash

kernel: pi_bus_raider.img
	cp pi_bus_raider.img bin/kernel.img

list: pi_bus_raider.elf
	$(ARMGNU)-objdump -D pi_bus_raider.elf > pi_bus_raider.list

debug: pi_bus_raider.elf
	cd JTAG && ./run_gdb.sh

dump: pi_bus_raider.elf
	@$(ARMGNU)-objdump --disassemble-zeroes -D pi_bus_raider.elf > pi_bus_raider.dump
	@echo "OBJDUMP $<"

$(BUILD_DIR)/%.o : $(SRC_DIR)/%.c 
	@$(ARMGNU)-gcc $(CFLAGS) -c $< -o $@
	@echo "CC $<"

$(BUILD_DIR)/%.o : $(SRC_DIR)/%.s 
	@$(ARMGNU)-as $< -o $@
	@echo "AS $<"

%.hex : %.elf 
	@$(ARMGNU)-objcopy $< -O ihex $@
	@echo "OBJCOPY $< -> $@"

%.img : %.elf 
	@$(ARMGNU)-objcopy $< -O binary $@
	@echo "OBJCOPY $< -> $@"

$(LIBUSPI):
	$(MAKE) -C ./uspi/lib 

pi_bus_raider.elf : $(OBJS) $(LIBUSPI) 
	@$(ARMGNU)-ld $(OBJS) $(LIBGCC) $(LIBUSPI) -T memmap -o $@
	@echo "LD $@"

.PHONY clean :
	rm -f $(BUILD_DIR)/*.o
	rm -f *.hex
	rm -f *.elf
	rm -f *.img
	rm -f *.dump
	rm -f tags
