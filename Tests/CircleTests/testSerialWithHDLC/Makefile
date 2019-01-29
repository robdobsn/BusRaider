#
# Makefile
#

SERIALPORT  ?= COM39

CIRCLEHOME = ../../../PiCircle/circle

OBJS	= main.o kernel.o rdserial.o MiniHDLC.o

LIBS = $(CIRCLEHOME)/lib/usb/libusb.a \
	  $(CIRCLEHOME)/lib/input/libinput.a \
	  $(CIRCLEHOME)/lib/fs/libfs.a \
	  $(CIRCLEHOME)/lib/libcircle.a

include $(CIRCLEHOME)/Rules.mk

DEP = $(OBJS:%.o=%.d)

# Include all .d files
-include $(DEP)

%.o: %.cpp
	@echo "  CPP   $@"
	mkdir -p $(@D)
	@$(CPP) $(CPPFLAGS) -MMD -c -o $@ $<
	
$(TARGET).srec: $(TARGET).img
	@echo "  COPY  $(TARGET).srec"
	@$(PREFIX)objcopy $(TARGET).elf -O srec $(TARGET).srec

run: $(TARGET).srec
	python $(CIRCLEHOME)/tools/flasher.py $(TARGET).srec $(SERIALPORT) $(FLASHBAUD)

test:
	python $(CIRCLEHOME)/sample/03-screentext/testserial.py ABC $(SERIALPORT) $(FLASHBAUD)

ft1: $(TARGET).hex
	python $(CIRCLEHOME)/tools/flasher.py $(TARGET).hex $(SERIALPORT) $(FLASHBAUD)
	python testserial.py test1 $(SERIALPORT) $(FLASHBAUD)

ft2: $(TARGET).hex
	python $(CIRCLEHOME)/tools/flasher.py $(TARGET).hex $(SERIALPORT) $(FLASHBAUD)
	python testserial.py test2 $(SERIALPORT) $(FLASHBAUD)
