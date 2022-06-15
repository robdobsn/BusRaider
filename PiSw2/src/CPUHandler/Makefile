#
# Makefile
#

CIRCLEHOME = ../../circle

OBJS	= CPUHandler.o CPUHandler_Debugger.o \
		CPUHandler_Cycle.o \
		CPUHandler_Disasm.o CPUHandler_Z80.o \
		mdZ80.o

INCLUDE	+= -I ../system -I ../Hardware -I ../BusRawAccess

brCPUHandler.a: $(OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) cr $@ $(OBJS)

include $(CIRCLEHOME)/Rules.mk

-include $(DEPS)
