#
# Makefile
#

CIRCLEHOME = ../../circle

OBJS	= TermAnsi.o TermEmu.o TermH19.o

INCLUDE	+= -I ../system

brTermEmu.a: $(OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) cr $@ $(OBJS)

include $(CIRCLEHOME)/Rules.mk

-include $(DEPS)
