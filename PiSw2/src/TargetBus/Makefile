#
# Makefile
#

CIRCLEHOME = ../../circle

OBJS	= BusControl.o BusSocketManager.o \
		BusRawAccess.o BusRawAccess_Wait.o \
		BusRawAccess_Busrq.o BusRawAccess_Addr.o \
		BusRawAccess_Block.o BusRawAccess_Mux.o \
		TargetControl.o \
		TargetControl_Prog.o TargetControl_Cycle.o \
		TargetBreakpoints.o \
		TargetCPUZ80.o TargetProgrammer.o memoryTests.o \
		MemoryController.o \
		DebugVals.o
		# BusAccess.o BusAccess_Control.o BusAccess_Block.o \
		# BusAccess_Raw.o BusAccess_Wait.o BusAccess_Addr.o \
		# BusAccess_Cycle.o

INCLUDE	+= -I ../system

brTargetBus.a: $(OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) cr $@ $(OBJS)

include $(CIRCLEHOME)/Rules.mk

-include $(DEPS)
