#
# Makefile
#

CIRCLEHOME = ../../circle

OBJS	= McTRS80CmdFormat.o McZXSpectrumSNAFormat.o McZXSpectrumTZXFormat.o \
	      McZXSpectrumZ80Format.o srecparser.o

INCLUDE	+= -I ../system

brFileFormats.a: $(OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) cr $@ $(OBJS)

include ../Rules.mk

-include $(DEPS)
