// Bus Raider
#include "busraider.h"
#include "piwiring.h"

// Following pins are jumpered - to allow SPI bus usage if not needed
// Uncomment the following lines to enable these pins
// #define BR_ENABLE_NMI 1
// #define BR_ENABLE_IRQ 1
// #define BR_ENABLE_WAIT 1

// Set a pin to be an output and set initial value for that pin
void br_set_out(int pin, int val)
{
	digitalWrite(pin, val);
	pinMode(pin, OUTPUT);
	digitalWrite(pin, val);
}

// Initialise the bus raider
void br_init()
{
	// Pins not set here as outputs will be inputs (inc data bus)
	// Bus Request
	br_set_out(BR_BUSRQ, 0);
	// RESET Z80
	br_set_out(BR_RESET, 0);
	// NMI
	#ifdef BR_ENABLE_NMI
		br_set_out(BR_NMI, 0);
	#endif
	// IRQ
	#ifdef BR_ENABLE_IRQ
		br_set_out(BR_IRQ, 0);
	#endif
	// WAIT
	#ifdef BR_ENABLE_WAIT
		br_set_out(BR_WAIT, 0);
	#endif
	// Address push
	br_set_out(BR_PUSH_ADDR_BAR, 1);
	// High address clock
	br_set_out(BR_HADDR_CK, 0);
	// High address serial
	br_set_out(BR_HADDR_SER, 0);
	// Low address clock
	br_set_out(BR_LADDR_CK, 0);
	// Low address clear
	br_set_out(BR_LADDR_CLR_BAR, 0);
	// Data bus direction 
	br_set_out(BR_DATA_DIR_IN, 1);
	// Data bus output enable
	br_set_out(BR_DATA_OE_BAR, 1);
}

// Request access to the bus
void br_request_bus()
{
	digitalWrite(BR_BUSRQ, 1);
}

// Check if bus request has been acknowledged
int br_bus_acknowledged()
{
	return (R32(GPLEV0) & (1 << BR_BUSACK_BAR)) == 0;
}

// Take control of bus
void br_take_control()
{
	// control bus
	br_set_out(BR_WR_BAR, 1);
	br_set_out(BR_RD_BAR, 1);
	br_set_out(BR_MREQ_BAR, 1);
	br_set_out(BR_IORQ_BAR, 1);
	// Data bus not enabled
	digitalWrite(BR_DATA_OE_BAR, 1);
	// Address bus enabled
	digitalWrite(BR_PUSH_ADDR_BAR, 0);	
}

// Release control of bus
void br_release_control()
{
	// Control bus
	pinMode(BR_WR_BAR, INPUT);
	pinMode(BR_RD_BAR, INPUT);
	pinMode(BR_MREQ_BAR, INPUT);
	pinMode(BR_IORQ_BAR, INPUT);
	// Data bus not enabled
	digitalWrite(BR_DATA_OE_BAR, 1);
	// Address bus not enabled
	digitalWrite(BR_PUSH_ADDR_BAR, 1);	
	// No longer request bus
	digitalWrite(BR_BUSRQ, 0);
}


