#
# Rules.mk
#

ifeq ($(strip $(CIRCLEHOME)),)
CIRCLEHOME = ../circle
endif

include $(CIRCLEHOME)/Rules.mk
