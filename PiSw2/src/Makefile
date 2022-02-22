#
# Makefile
#

CIRCLEHOME = ../circle

OBJS	= 	main.o kernel.o BusRaiderApp.o

LIBS	= 	$(CIRCLEHOME)/lib/usb/libusb.a \
			$(CIRCLEHOME)/lib/input/libinput.a \
			$(CIRCLEHOME)/lib/fs/fat/libfatfs.a \
			$(CIRCLEHOME)/lib/fs/libfs.a \
			$(CIRCLEHOME)/lib/net/libnet.a \
			$(CIRCLEHOME)/lib/sched/libsched.a \
			$(CIRCLEHOME)/lib/libcircle.a \
			system/brsys.a \
			comms/brcomms.a \
			web/brweb.a \
			BusRawAccess/brBusRawAccess.a \
			TargetBus/brTargetBus.a \
			Hardware/brHardware.a \
			ControlAPI/brBusAPI.a \
			Machines/brMachines.a \
			SelfTest/brSelfTest.a \
			fonts/brFonts.a \
			TerminalEmulation/brTermEmu.a \
			FileFormats/brFileFormats.a \
			Display/brDisplay.a

INCLUDE	+= -I system -I comms -I web -I TargetBus -I Display -I Machines -I TerminalEmulation -I SelfTest -I Hardware -I BusRawAccess

include Rules.mk

-include $(DEPS)
