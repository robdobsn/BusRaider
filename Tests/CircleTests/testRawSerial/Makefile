#
# Makefile
#

SERIALPORT  ?= COM39

CIRCLEHOME = ../../../PiCircle/circle

OBJS	= main.o kernel.o rdserial.o

LIBS	= $(CIRCLEHOME)/lib/libcircle.a

include $(CIRCLEHOME)/Rules.mk

$(TARGET).srec: $(TARGET).img
	@echo "  COPY  $(TARGET).srec"
	@$(PREFIX)objcopy $(TARGET).elf -O srec $(TARGET).srec

run: $(TARGET).srec
	python $(CIRCLEHOME)/tools/flasher.py $(TARGET).srec $(SERIALPORT) $(FLASHBAUD)

test:
	python $(CIRCLEHOME)/sample/03-screentext/testserial.py ABC $(SERIALPORT) $(FLASHBAUD)

ft: $(TARGET).hex
	python $(CIRCLEHOME)/tools/flasher.py $(TARGET).hex $(SERIALPORT) $(FLASHBAUD)
	python testserial.py ABC $(SERIALPORT) $(FLASHBAUD)
