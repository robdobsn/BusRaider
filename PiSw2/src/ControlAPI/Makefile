#
# Makefile
#

CIRCLEHOME = ../../circle

OBJS	= ControlAPI.o

INCLUDE	+= -I ../system -I ../TargetBus -I ../Hardware -I ../comms

brBusAPI.a: $(OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) cr $@ $(OBJS)

include ../Rules.mk

-include $(DEPS)
