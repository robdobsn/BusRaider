#
# Makefile
#

CIRCLEHOME = ../../circle

OBJS	= HwManager.o HwBase.o MemoryInterface.o
# HwRAMROM.o

INCLUDE	+= -I ../system -I ../BusControl -I ../Hardware -I ../BusRawAccess -I ../CPUHandler

brHardware.a: $(OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) cr $@ $(OBJS)

include $(CIRCLEHOME)/Rules.mk

-include $(DEPS)
