#
# Makefile
#

CIRCLEHOME = ../../circle

OBJS	= font12x16.o mc_trs80l1font.o mc_trs80l3font.o SystemFont.o ZXSpectrumFont.o

INCLUDE	+= -I ../system -I ../BusControl -I ../comms

brFonts.a: $(OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) cr $@ $(OBJS)

include ../Rules.mk

-include $(DEPS)
