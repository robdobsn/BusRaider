#
# Makefile
#

CIRCLEHOME = ../../circle

OBJS	= FastScreen.o Display.o DisplayFX.o

INCLUDE	+= -I ../system -I ../fonts

brDisplay.a: $(OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) cr $@ $(OBJS)

include $(CIRCLEHOME)/Rules.mk

-include $(DEPS)
