#
# Makefile
#

CIRCLEHOME = ../../circle

OBJS	= MiniHDLC.o CommandHandler.o CommsManager.o

INCLUDE	+= -I ../system

brcomms.a: $(OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) cr $@ $(OBJS)

include ../Rules.mk

-include $(DEPS)
