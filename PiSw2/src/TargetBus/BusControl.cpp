// Bus Raider
// Rob Dobson 2018-2020

#include "BusControl.h"
#include "PiWiring.h"
#include <circle/interrupt.h>
#include "lowlib.h"
#include <circle/bcm2835.h>
#include "logging.h"
#include "circle/timer.h"

// Module name
static const char MODULE_PREFIX[] = "BusControl";

// Constructor

BusControl::BusControl()
{
    // Not init yet
    _isInitialized = false;
    _busReqAcknowledged = false;
    _pageIsActive = false;
    _waitIsActive = false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Initialise the hardware
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusControl::init()
{
    if (!_isInitialized)
    {
        // Clock
        _clockGenerator.setOutputPin();
        _clockGenerator.setFrequency(1000000);
        _clockGenerator.enable(true);
        
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

        // Now initialized
        _isInitialized = true;

    }
    
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Wait system initialzation code
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusControl::waitSystemInit()
{
    // Use PWM to generate pulses as the behaviour of the processor is non-deterministic in terms
    // of time taken between consecutive instructions and when MREQ/IORQ enable is low wait
    // states are disabled and bus operations can be missed

    // Clock for PWM 
    uint32_t clockSource = ARM_CM_CTL_CLKSRC_PLLD;
    uint32_t freqReqd = 31250000;

    // Disable the clock (without changing SRC and FLIP)
    write32(ARM_CM_PWMCTL, ARM_CM_PASSWD | clockSource);

    // Wait a little if clock is busy
    int busyCount = 0;
    static const int MAX_BUSY_WAIT_COUNT = 100000;
    uint32_t lastBusy = 0;
    for (int i = 0; i < MAX_BUSY_WAIT_COUNT; i++)
    {
        if ((read32(ARM_CM_PWMCTL) & ARM_CM_CTL_BUSY) == 0)
            break;
        microsDelay(1);
        busyCount++;
        lastBusy = read32(ARM_CM_PWMCTL);
    }
    uint32_t afterKill = 0;
    if (busyCount == MAX_BUSY_WAIT_COUNT)
    {
        write32(ARM_CM_PWMCTL, ARM_CM_PASSWD | ARM_CM_CTL_KILL | clockSource);
        microsDelay(1);
        afterKill = read32(ARM_CM_PWMCTL);
    }

    // Set output pins
    // MREQ will use PWM channel 2 and IORQ PWM channel 1
    pinMode(BR_MREQ_WAIT_EN, PINMODE_ALT0);
    pinMode(BR_IORQ_WAIT_EN, PINMODE_ALT0);

    // Clear status on PWM generator
    write32(ARM_PWM_STA, 0xffffffff);

    // Set the divisor for PWM clock
    uint32_t divisor = ARM_CM_CTL_PLLD_FREQ / freqReqd;
    if (divisor > 4095)
        divisor = 4095;
    write32(ARM_CM_PWMDIV, ARM_CM_PASSWD | divisor << 12);
    microsDelay(1);

    // Enable PWM clock
    write32(ARM_CM_PWMCTL, ARM_CM_PASSWD | ARM_CM_CTL_ENAB | clockSource);
    microsDelay(1);

    // Clear wait state enables initially - leaving LOW disables wait state generation
    // Enable PWM two channels (one for IORQ and one for MREQ)
    // FIFO is cleared so pins for IORQ and MREQ enable will be low
    write32(ARM_PWM_CTL, ARM_PWM_CTL_CLRF1 | ARM_PWM_CTL_USEF2 | ARM_PWM_CTL_MODE2 | ARM_PWM_CTL_PWEN2 |
                                          ARM_PWM_CTL_USEF1 | ARM_PWM_CTL_MODE1 | ARM_PWM_CTL_PWEN1);
    microsDelay(1);

    // Debug
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "PWM div %d, busyCount %d, lastBusy %08x afterKill %08x", divisor, busyCount, lastBusy, afterKill);
}
