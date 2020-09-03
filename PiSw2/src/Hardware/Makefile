#
# Makefile
#

CIRCLEHOME = ../../circle

OBJS	= HwManager.o HwBase.o HwRAMROM.o

INCLUDE	+= -I ../system -I ../TargetBus -I ../comms -I ../fonts

brHardware.a: $(OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) cr $@ $(OBJS)

include ../Rules.mk

-include $(DEPS)
