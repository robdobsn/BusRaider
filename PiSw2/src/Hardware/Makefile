#
# Makefile
#

CIRCLEHOME = ../../circle

OBJS	= HwManager.o HwBase.o 
# HwRAMROM.o

INCLUDE	+= -I ../system -I ../TargetBus -I ../Hardware -I ../BusRawAccess

brHardware.a: $(OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) cr $@ $(OBJS)

include $(CIRCLEHOME)/Rules.mk

-include $(DEPS)
