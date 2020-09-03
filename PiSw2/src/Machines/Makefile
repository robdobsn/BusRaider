#
# Makefile
#

CIRCLEHOME = ../../circle

OBJS	= McBase.o McManager.o McRobsZ80.o McTerminal.o McTRS80.o \
		  McZXSpectrum.o

INCLUDE	+= -I ../system -I ../TargetBus -I ../comms -I ../Hardware -I ../fonts -I ../TerminalEmulation -I ../FileFormats

brMachines.a: $(OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) cr $@ $(OBJS)

include ../Rules.mk

-include $(DEPS)
