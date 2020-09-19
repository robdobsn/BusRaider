#
# Makefile
#

CIRCLEHOME = ../../circle

OBJS	= SelfTestMemory.o

INCLUDE	+= -I ../system -I ../TargetBus -I ../comms -I ../Hardware -I ../fonts -I ../TerminalEmulation -I ../FileFormats -I ../Display -I ../

brSelfTest.a: $(OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) cr $@ $(OBJS)

include ../Rules.mk

-include $(DEPS)
