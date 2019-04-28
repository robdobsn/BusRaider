#
# Makefile
#

PROJECTHOME = .
CIRCLEHOME = circle

OBJS	= src/main.o src/kernel.o

LIBS	= $(CIRCLEHOME)/lib/libcircle.a

include $(CIRCLEHOME)/Rules.mk

send: $(TARGET).img
	python $(PROJECTHOME)/tools/UploadToBusRaider.py 0 hdlc COM7 921600 $(PROJECTHOME)
