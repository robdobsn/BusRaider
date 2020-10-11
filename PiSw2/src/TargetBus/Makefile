#
# Makefile
#

CIRCLEHOME = ../../circle

OBJS	= BusAccess.o BusAccess_Control.o BusAccess_Block.o \
		TargetBreakpoints.o \
		TargetCPUZ80.o TargetProgrammer.o memoryTests.o

INCLUDE	+= -I ../system

brTargetBus.a: $(OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) cr $@ $(OBJS)

include $(CIRCLEHOME)/Rules.mk

-include $(DEPS)
