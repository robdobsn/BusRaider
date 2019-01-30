#
# Makefile
#

SERIALPORT  ?= COM39

CIRCLEHOME = ./circle

EXTRACLEAN = src/*.o src/CommandHandler/*.o src/Machines/*.o src/Target/*.o src/System/*.o *.d src/*.d src/CommandHandler/*.d src/Machines/*.d src/Target/*.d src/System/*.d

# INCLUDE_USB = _INCLUDE_USB_
ifdef INCLUDE_USB
CFLAGS  = -D$(INCLUDE_USB)
endif

OBJS	= src/main.o src/kernel.o src/rdserial.o src/BusRaider.o \
			src/System/LogHandler.o src/System/StringUtils.o src/System/JsonUtils.o src/System/jsmnR.o \
			src/CommandHandler/CommandHandler.o src/CommandHandler/MiniHDLC.o 

#OBJS	= src/main.o src/kernel.o src/System/lowlev.o \
#			src/Target/BusRaider.o src/Target/TargetMemory.o src/Target/TargetScreen.o \
#			src/Target/TargetFonts.o \
#			src/Fonts/FontTRS80Level1.o \
#			src/CommandInterface/MiniHDLC.o src/CommandInterface/CommandHandler.o

ifdef INCLUDE_USB
LIBS	= $(CIRCLEHOME)/lib/usb/libusb.a \
	  $(CIRCLEHOME)/lib/input/libinput.a \
	  $(CIRCLEHOME)/lib/fs/libfs.a \
	  $(CIRCLEHOME)/lib/libcircle.a
else
LIBS	= $(CIRCLEHOME)/lib/libcircle.a
endif

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
