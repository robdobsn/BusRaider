#
# Makefile
#

CIRCLEHOME = ../../circle

OBJS	= McBase.o McManager.o \
		  McTRS80.o \
		  McTerminal.o \
		  McZXSpectrum.o
		#   McRobsZ80.o 

INCLUDE	+= -I ../system -I ../BusControl -I ../comms -I ../Hardware -I ../fonts -I ../TerminalEmulation -I ../FileFormats -I ../Display -I ../BusRawAccess -I ../CPUHandler

brMachines.a: $(OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) cr $@ $(OBJS)

include ../Rules.mk

-include $(DEPS)
