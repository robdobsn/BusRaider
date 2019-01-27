#
# Makefile
#

CIRCLEHOME = ./circle

OBJS	= src/main.o src/kernel.o src/System/lowlev.o src/TargetBus/BusRaider.o src/TargetBus/TargetMemory.o

LIBS	= $(CIRCLEHOME)/lib/usb/libusb.a \
	  $(CIRCLEHOME)/lib/input/libinput.a \
	  $(CIRCLEHOME)/lib/fs/libfs.a \
	  $(CIRCLEHOME)/lib/libcircle.a

OPTIMIZE = -O0

include ./circle/Rules.mk
