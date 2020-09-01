#
# Makefile
#

CIRCLEHOME = ../../circle

OBJS	= FastScreen.o UartMaxiSerialDevice.o \
			lowlib.o rdutils.o jsmnR.o Utils.o

brsys.a: $(OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) cr $@ $(OBJS)

include $(CIRCLEHOME)/Rules.mk

-include $(DEPS)
