// Bus Raider
// Rob Dobson 2018-2020

#include "BusRawAccess.h"
#include "PiWiring.h"
#include <circle/interrupt.h>
#include "lowlib.h"
#include <circle/bcm2835.h>
#include "logging.h"
#include "circle/timer.h"

// Module name
static const char MODULE_PREFIX[] = "BusRawAccess";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BusRawAccess::BusRawAccess()
{
    _busReqAcknowledged = false;
    _pageIsActive = false;
    _waitIsActive = false;
}

void BusRawAccess::init()
{
    // Pins that are not setup here as outputs will be inputs
    // This includes the "bus" used for address-low/high and data (referred to as the PIB)

    // Setup the multiplexer
    setPinOut(BR_MUX_0, 0);
    setPinOut(BR_MUX_1, 0);
    setPinOut(BR_MUX_2, 0);

    // Set the MUX enable on V2.0 hardware - on V1.7 this is LADDR_CLK but it is an
    // output either way which is good as this code is only run once so can't take
    // into account the hardware version which comes from the ESP32 later on
    setPinOut(BR_MUX_EN_BAR, 1);
    
    // Bus Request
    setPinOut(BR_BUSRQ_BAR, 1);
    _busReqAcknowledged = false;
        
    // High address clock
    setPinOut(BR_HADDR_CK, 0);
    
    // Data bus driver direction inward
    setPinOut(BR_DATA_DIR_IN, 1);

    // Paging initially inactive (actually this is the wrong way around
    // for V1.7 hardware but will be resolved later)
    setPinOut(BR_PAGING_RAM_PIN, 1);
    _pageIsActive = false;

    // Setup MREQ and IORQ enables
    waitSystemInit();
    _waitIsActive = false;

    // Remove edge detection
#ifdef ISR_TEST
    write32(ARM_GPIO_GPREN0, 0);
    write32(ARM_GPIO_GPAREN0, 0);
    write32(ARM_GPIO_GPFEN0, 0);
    write32(ARM_GPIO_GPAFEN0, 0); //read32(ARM_GPIO_GPFEN0) & (~(1 << BR_WAIT_BAR)));

    // // Clear any currently detected edge
    write32(ARM_GPIO_GPEDS0, BR_ANY_EDGE_MASK);

    CInterrupts::connectFIQ(52, stepTimerISR, 0);
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusRawAccess::service()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Reset target
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusRawAccess::targetReset(uint32_t ms)
{
    muxSet(BR_MUX_RESET_Z80_BAR_LOW);
    microsDelay(100000);
    muxClear();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Utility Functions
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Set a pin to be an output and set initial value for that pin
void BusRawAccess::setPinOut(int pinNumber, bool val)
{
    digitalWrite(pinNumber, val);
    pinMode(pinNumber, OUTPUT);
    digitalWrite(pinNumber, val);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Set signal
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusRawAccess::setBusSignal(BR_BUS_ACTION busAction, bool assert)
{
    switch (busAction)
    {
        case BR_BUS_ACTION_RESET: 
            assert ? muxSet(BR_MUX_RESET_Z80_BAR_LOW) : muxClear();
            // LogWrite(MODULE_PREFIX, LOG_DEBUG, "RESET"); 
            // ISR_VALUE(ISR_ASSERT_CODE_DEBUG_E, assert);
            break;
        case BR_BUS_ACTION_NMI: 
            assert ? muxSet(BR_MUX_NMI_BAR_LOW) : muxClear(); 
            // ISR_VALUE(ISR_ASSERT_CODE_DEBUG_F, assert);
            break;
        case BR_BUS_ACTION_IRQ: 
            assert ? muxSet(BR_MUX_IRQ_BAR_LOW) : muxClear(); 
            // LogWrite(MODULE_PREFIX, LOG_DEBUG, "IRQ"); 
            // ISR_VALUE(ISR_ASSERT_CODE_DEBUG_I, assert);
            break;
        case BR_BUS_ACTION_BUSRQ: 
            // LogWrite(MODULE_PREFIX, LOG_DEBUG, "BUSRQ"); 
            // ISR_VALUE(ISR_ASSERT_CODE_DEBUG_B, assert);
            assert ? busReqStart() : busReqRelease(); 
            break;
        default: break;
    }
}