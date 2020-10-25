#
# Makefile
#

CIRCLEHOME = ../../circle

OBJS	= UartMaxiSerialDevice.o \
			lowlib.o rdutils.o jsmnR.o Utils.o \
			logging.o \
			PiWiring.o lowlev.o KeyConversion.o \
			DebugHelper.o

brsys.a: $(OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) cr $@ $(OBJS)

include $(CIRCLEHOME)/Rules.mk

-include $(DEPS)
