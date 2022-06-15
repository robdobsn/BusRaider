#
# Makefile
#

CIRCLEHOME = ../../circle

OBJS	= StepTracer.o libz80/z80.o

INCLUDE	+= -I ../system -I ../BusRawAccess -I ../CPUHandler -I ../comms -I ../BusControl -I ../Hardware

brStepTracer.a: $(OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) cr $@ $(OBJS)

include ../Rules.mk

-include $(DEPS)
