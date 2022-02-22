// Bus Raider
// Rob Dobson 2018-2019

#include "BusRawAccess.h"
#include "PiWiring.h"
#include "lowlev.h"
#include "lowlib.h"
#include <circle/bcm2835.h>

// Module name
static const char MODULE_PREFIX[] = "BusRawMux";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Multiplexer functions
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Set the MUX
void BusRawAccess::muxSet(int muxVal)
{
    if (_hwVersionNumber == 17)
    {
        // Clear first as this is a safe setting - sets HADDR_SER low
        write32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
        // Now set bits required
        write32(ARM_GPIO_GPSET0, muxVal << BR_MUX_LOW_BIT_POS);
    }
    else
    {
        // Disable mux initially
        write32(ARM_GPIO_GPSET0, BR_MUX_EN_BAR_MASK);
        // Clear all the mux bits
        write32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
        // Now set bits required
        write32(ARM_GPIO_GPSET0, muxVal << BR_MUX_LOW_BIT_POS);
        // Enable the mux
        write32(ARM_GPIO_GPCLR0, BR_MUX_EN_BAR_MASK);
    }
}

// Clear the MUX
void BusRawAccess::muxClear()
{
    if (_hwVersionNumber == 17)
    {
        // Clear to a safe setting - sets HADDR_SER low
        write32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
    }
    else
    {
        // Disable the mux
        write32(ARM_GPIO_GPSET0, BR_MUX_EN_BAR_MASK);
        // Clear to a safe setting - sets LADDR_CK low
        write32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
    }
}


// Mux set data bus driver output enable
void BusRawAccess::muxDataBusOutputEnable()
{
    // Start pulse
    muxDataBusOutputStart();
    // Time pulse width
    lowlev_cycleDelay(CYCLES_DELAY_FOR_OUT_FF_SET);
    // Finish pulse
    muxDataBusOutputFinish();
}

// Mux set data bus driver output start
void BusRawAccess::muxDataBusOutputStart()
{
    if (_hwVersionNumber == 17)
    {
        // Clear first as this is a safe setting - sets HADDR_SER low
        write32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
        // Now set OE bit
        write32(ARM_GPIO_GPSET0, BR_MUX_DATA_OE_BAR_LOW << BR_MUX_LOW_BIT_POS);
    }
    else
    {
        // Clear then set the output enable
        write32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
        write32(ARM_GPIO_GPSET0, BR_MUX_DATA_OE_BAR_LOW << BR_MUX_LOW_BIT_POS);
        // Mux enable active
        write32(ARM_GPIO_GPCLR0, BR_MUX_EN_BAR_MASK);
    }
}

// Mux set data bus driver output finish
void BusRawAccess::muxDataBusOutputFinish()
{
    if (_hwVersionNumber == 17)
    {
        // Clear again
        write32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
    }
    else
    {
        write32(ARM_GPIO_GPSET0, BR_MUX_EN_BAR_MASK);
    }
}

// Mux clear low address
void BusRawAccess::muxClearLowAddr()
{
    if (_hwVersionNumber == 17)
    {
        write32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
        write32(ARM_GPIO_GPSET0, BR_MUX_LADDR_CLR_BAR_LOW << BR_MUX_LOW_BIT_POS);
        // Time pulse width
        lowlev_cycleDelay(CYCLES_DELAY_FOR_OUT_FF_SET);
        // Clear again
        write32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
    }
    else
    {
        // Clear then set the low address clear line
        write32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
        write32(ARM_GPIO_GPSET0, BR_MUX_LADDR_CLR_BAR_LOW << BR_MUX_LOW_BIT_POS);
        // Pulse mux enable
        write32(ARM_GPIO_GPCLR0, BR_MUX_EN_BAR_MASK);
        lowlev_cycleDelay(CYCLES_DELAY_FOR_CLEAR_LOW_ADDR);
        write32(ARM_GPIO_GPSET0, BR_MUX_EN_BAR_MASK);
    }    
}
