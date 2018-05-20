
ARMGNU ?= arm-none-eabi
#ARMGNU ?= arm-linux-gnueabi

COPS = -Wall -O2 -nostdlib -nostartfiles -ffreestanding 

all : kernel.img 

clean :
	rm -f *.o
	rm -f *.bin
	rm -f *.hex
	rm -f *.elf
	rm -f *.list
	rm -f *.img

vectors.o : vectors.s
	$(ARMGNU)-as vectors.s -o vectors.o

bootloader1mbps.o : bootloader1mbps.c
	$(ARMGNU)-gcc $(COPS) -c bootloader1mbps.c -o bootloader1mbps.o

periph.o : periph.c 
	$(ARMGNU)-gcc $(COPS) -c periph.c -o periph.o

kernel.img : loader vectors.o periph.o bootloader1mbps.o 
	$(ARMGNU)-ld vectors.o periph.o bootloader1mbps.o -T loader -o bootloader1mbps.elf
	$(ARMGNU)-objdump -D bootloader1mbps.elf > bootloader1mbps.list
	$(ARMGNU)-objcopy bootloader1mbps.elf -O binary kernel.img

