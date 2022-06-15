#
# Makefile
#

CIRCLEHOME = ../../circle

OBJS	= BusControl.o BusSocketManager.o \
		TargetBreakpoints.o \
		TargetImager.o \
		TargetProgrammer.o 

INCLUDE	+= -I ../system -I ../Hardware -I ../BusControl -I ../BusRawAccess -I ../CPUHandler

brBusControl.a: $(OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) cr $@ $(OBJS)

include $(CIRCLEHOME)/Rules.mk

-include $(DEPS)
