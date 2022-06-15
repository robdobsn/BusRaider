#
# Makefile
#

CIRCLEHOME = ../../circle

OBJS	= ControlAPI.o

INCLUDE	+= -I ../system -I ../BusControl -I ../Hardware -I ../comms -I ../Machines -I ../Display -I ../BusRawAccess -I ../CPUHandler

brBusAPI.a: $(OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) cr $@ $(OBJS)

include ../Rules.mk

-include $(DEPS)
