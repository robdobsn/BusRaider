#
# Makefile
#

CIRCLEHOME = ./circle

OBJS	= src/main.o src/kernel.o src/System/lowlev.o \
			src/Target/BusRaider.o src/Target/TargetMemory.o src/Target/TargetScreen.o \
			src/Target/TargetFonts.o \
			src/Fonts/FontTRS80Level1.o

LIBS	= $(CIRCLEHOME)/lib/usb/libusb.a \
	  $(CIRCLEHOME)/lib/input/libinput.a \
	  $(CIRCLEHOME)/lib/fs/libfs.a \
	  $(CIRCLEHOME)/lib/libcircle.a

# OPTIMIZE = -O0

include ./circle/Rules.mk
