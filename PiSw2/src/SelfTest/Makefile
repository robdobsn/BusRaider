#
# Makefile
#

CIRCLEHOME = ../../circle

OBJS	= SelfTestMemory.o memoryTests.o

INCLUDE	+= -I ../system -I ../BusControl -I ../comms -I ../Hardware -I ../fonts -I ../TerminalEmulation -I ../FileFormats -I ../Display -I ../ -I ../BusRawAccess -I ../CPUHandler -I ../McManager

brSelfTest.a: $(OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) cr $@ $(OBJS)

include ../Rules.mk

-include $(DEPS)
