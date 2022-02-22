#
# Makefile
#

CIRCLEHOME = ../../circle

OBJS	= BusControl.o BusSocketManager.o \
		BusClockGenerator.o \
		TargetControl.o TargetControl_Debugger.o \
		TargetControl_Prog.o TargetControl_Cycle.o \
		TargetControl_Disasm.o \
		TargetBreakpoints.o \
		TargetCPUZ80.o TargetImager.o memoryTests.o \
		MemoryController.o \
		mdZ80.o

INCLUDE	+= -I ../system -I ../Hardware -I ../TargetBus -I ../BusRawAccess

brTargetBus.a: $(OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) cr $@ $(OBJS)

include $(CIRCLEHOME)/Rules.mk

-include $(DEPS)
