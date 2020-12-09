#
# Makefile
#

CIRCLEHOME = ../../circle

OBJS	= McBase.o McManager.o \
		  McTRS80.o \
		  McTerminal.o \
		  McZXSpectrum.o
		#   McRobsZ80.o 

INCLUDE	+= -I ../system -I ../TargetBus -I ../comms -I ../Hardware -I ../fonts -I ../TerminalEmulation -I ../FileFormats -I ../Display

brMachines.a: $(OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) cr $@ $(OBJS)

include ../Rules.mk

-include $(DEPS)
