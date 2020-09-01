#
# Makefile
#

CIRCLEHOME = ../../circle

OBJS	= httpbootserver.o tftpbootserver.o

INCLUDE	+= -I ../comms

brweb.a: $(OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) cr $@ $(OBJS)

CONTENT	= index.h style.h favicon.h

EXTRACLEAN = $(CONTENT) converttool

%.h: %.html
	@echo "  GEN   $@"
	@./converttool $< > $@

%.h: %.css
	@echo "  GEN   $@"
	@./converttool $< > $@

%.h: %.ico
	@echo "  GEN   $@"
	@./converttool -b $< > $@

httpbootserver.o: $(CONTENT)

$(CONTENT): converttool

converttool: converttool.c
	@echo "  TOOL  $@"
	@gcc -o converttool converttool.c


include $(CIRCLEHOME)/Rules.mk

-include $(DEPS)
