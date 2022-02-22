#
# Makefile
#

CIRCLEHOME = ../../circle

OBJS	= BusRawAccess.o BusRawAccess_Wait.o \
		BusRawAccess_Busrq.o BusRawAccess_Addr.o \
		BusRawAccess_Block.o BusRawAccess_Mux.o \
		BusClockGenerator.o

INCLUDE	+= -I ../system -I ../BusRawAccess

brBusRawAccess.a: $(OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) cr $@ $(OBJS)

include $(CIRCLEHOME)/Rules.mk

-include $(DEPS)
