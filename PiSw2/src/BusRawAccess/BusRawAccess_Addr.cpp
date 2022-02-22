// Bus Raider
// Rob Dobson 2018-2019

#include "BusRawAccess.h"
#include "PiWiring.h"
#include "lowlev.h"
#include "lowlib.h"
#include <circle/bcm2835.h>
#include "DebugHelper.h"

// Module name
static const char MODULE_PREFIX[] = "BusRawAddr";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Address handling
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Set the full address
void BusRawAccess::addrSet(unsigned int addr)
{
    addrHighSet(addr >> 8);
    addrLowSet(addr & 0xff);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Address Bus Functions
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Set low address value by clearing and counting
void BusRawAccess::addrLowSet(uint32_t lowAddrByte)
{
    // Clear initially
    muxClearLowAddr();
    // Clock the required value in - requires one more count than
    // expected as the output register is one clock pulse behind the counter
    if (_hwVersionNumber == 17)
    {
        for (uint32_t i = 0; i < (lowAddrByte & 0xff) + 1; i++) {
            write32(ARM_GPIO_GPSET0, BR_V17_LADDR_CK_MASK);
            lowlev_cycleDelay(CYCLES_DELAY_FOR_LOW_ADDR_SET);
            write32(ARM_GPIO_GPCLR0, BR_V17_LADDR_CK_MASK);
            lowlev_cycleDelay(CYCLES_DELAY_FOR_LOW_ADDR_SET);
        }
    }
    else
    {
        write32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
        write32(ARM_GPIO_GPSET0, BR_MUX_LADDR_CLK << BR_MUX_LOW_BIT_POS);
        for (uint32_t i = 0; i < (lowAddrByte & 0xff) + 1; i++) {
            write32(ARM_GPIO_GPCLR0, BR_MUX_EN_BAR_MASK);
            lowlev_cycleDelay(CYCLES_DELAY_FOR_CLOCK_LOW_ADDR);
            write32(ARM_GPIO_GPSET0, BR_MUX_EN_BAR_MASK);
            lowlev_cycleDelay(CYCLES_DELAY_FOR_LOW_ADDR_SET);
        }
    }
}

// Set the high address value
void BusRawAccess::addrHighSet(uint32_t highAddrByte)
{
    if (_hwVersionNumber == 17)
    {
        // Shift the value into the register
        // Takes one more shift than expected as output reg is one pulse behind shift
        for (uint32_t i = 0; i < 9; i++) {
            // Set or clear serial pin to shift register
            if (highAddrByte & 0x80)
                muxSet(BR_V17_MUX_HADDR_SER_HIGH);
            else
                muxSet(BR_V17_MUX_HADDR_SER_LOW);
            // Delay to allow settling
            lowlev_cycleDelay(CYCLES_DELAY_FOR_HIGH_ADDR_SET);
            // Shift the address value for next bit
            highAddrByte = highAddrByte << 1;
            // Clock the bit
            write32(ARM_GPIO_GPSET0, 1 << BR_HADDR_CK);
            lowlev_cycleDelay(CYCLES_DELAY_FOR_HIGH_ADDR_SET);
            write32(ARM_GPIO_GPCLR0, 1 << BR_HADDR_CK);
        }
        muxClear();
    }
    else
    {
        // Disable mux initially and clear the clock line, then set mux to 
        // low address clear as this line doubles as high address serial in
        write32(ARM_GPIO_GPSET0, BR_MUX_EN_BAR_MASK);
        write32(ARM_GPIO_GPCLR0, 1 << BR_HADDR_CK | BR_MUX_CTRL_BIT_MASK);
        write32(ARM_GPIO_GPSET0, BR_MUX_LADDR_CLR_BAR_LOW << BR_MUX_LOW_BIT_POS);

        // Shift the value into the register
        // Takes one more shift than expected as output reg is one pulse behind shift
        for (uint32_t i = 0; i < 9; i++) {
            // Clock in each bit
            if (highAddrByte & 0x80)
            {
                // Disable the mux and set the clock low
                write32(ARM_GPIO_GPSET0, BR_MUX_EN_BAR_MASK);
                write32(ARM_GPIO_GPCLR0, 1 << BR_HADDR_CK);
            }
            else
            {
                // Enable the mux (we set the mux outside the loop to be the high-address-serial-in)
                // and set the clock low
                write32(ARM_GPIO_GPCLR0, BR_MUX_EN_BAR_MASK | 1 << BR_HADDR_CK);
            }

            // Repeat here to handle pulse timing
            write32(ARM_GPIO_GPCLR0, 1 << BR_HADDR_CK);
            write32(ARM_GPIO_GPCLR0, 1 << BR_HADDR_CK);
            write32(ARM_GPIO_GPCLR0, 1 << BR_HADDR_CK);
            write32(ARM_GPIO_GPCLR0, 1 << BR_HADDR_CK);
            write32(ARM_GPIO_GPCLR0, 1 << BR_HADDR_CK);
            write32(ARM_GPIO_GPCLR0, 1 << BR_HADDR_CK);
            write32(ARM_GPIO_GPCLR0, 1 << BR_HADDR_CK);

            // Delay to allow settling
            // TODO 2020
            // microsDelay(1);
            // lowlev_cycleDelay(CYCLES_DELAY_FOR_HIGH_ADDR_SET);
            // Shift the address value for next bit
            highAddrByte = highAddrByte << 1;
            // // Clock the bit
            // write32(ARM_GPIO_GPSET0, 1 << BR_HADDR_CK);
            // // TODO 2020
            // microsDelay(1);
            // lowlev_cycleDelay(CYCLES_DELAY_FOR_HIGH_ADDR_SET);
            // Clock the bit by setting the clock high
            write32(ARM_GPIO_GPSET0, 1 << BR_HADDR_CK);
        }

        // Clean up
        write32(ARM_GPIO_GPSET0, BR_MUX_EN_BAR_MASK);
        write32(ARM_GPIO_GPCLR0, 1 << BR_HADDR_CK | BR_MUX_CTRL_BIT_MASK);
    }

    // Clear multiplexer
    // lowlev_cycleDelay(CYCLES_DELAY_FOR_HIGH_ADDR_SET);
    // muxClear();
}

// Increment low address value by clocking the counter
void BusRawAccess::addrLowInc()
{
    if (_hwVersionNumber == 17)
    {
        write32(ARM_GPIO_GPSET0, BR_V17_LADDR_CK_MASK);
        lowlev_cycleDelay(CYCLES_DELAY_FOR_LOW_ADDR_SET);
        write32(ARM_GPIO_GPCLR0, BR_V17_LADDR_CK_MASK);
        lowlev_cycleDelay(CYCLES_DELAY_FOR_LOW_ADDR_SET);
    }
    else
    {
        // This sets the low address clock low as it is MUX0
        write32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
        // Use repetition to handle timing
        // for (uint32_t i = 0; i < CYCLES_REPEAT_FOR_CLOCK_LOW_ADDR; i++)
        //     write32(ARM_GPIO_GPCLR0, BR_MUX_EN_BAR_MASK);
        lowlev_cycleDelay(CYCLES_DELAY_FOR_CLOCK_LOW_ADDR);
        write32(ARM_GPIO_GPSET0, BR_MUX_EN_BAR_MASK);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Address & Data Bus Functions
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusRawAccess::addrAndDataBusRead(uint32_t& addr, uint32_t& dataBusVals)
{
    if (_hwVersionNumber == 17)
    {
        // Set data bus driver direction outward - so it doesn't conflict with the PIB
        // if FF OE is set
        write32(ARM_GPIO_GPCLR0, BR_DATA_DIR_IN_MASK);            
    }

    // Set PIB to input
    pibSetIn();

    // Enable the high address onto the PIB
    muxSet(BR_MUX_HADDR_OE_BAR);

    // Delay to allow data to settle
    lowlev_cycleDelay(CYCLES_DELAY_FOR_HIGH_ADDR_READ);

    // High address
    addr = (pibGetValue() & 0xff) << 8;

    // Enable the low address onto the PIB
    muxSet(BR_MUX_LADDR_OE_BAR);

    // Delay to allow data to settle
    lowlev_cycleDelay(CYCLES_DELAY_FOR_READ_FROM_PIB);

    // Or in low address value
    addr |= pibGetValue() & 0xff;

    // Clear the mux to deactivate output enables
    muxClear();

    // Delay to allow data to settle
    lowlev_cycleDelay(CYCLES_DELAY_FOR_READ_FROM_PIB);

    // Read the data bus
    // If the target machine is writing then this will be the data it wants to write
    // If reading then the memory/IO system may have placed its data onto the data bus 

    // Due to hardware limitations reading from the data bus MUST be done last as the
    // output of the data bus is latched onto the PIB (or out onto the data bus in the case
    // where the ISR provides data for the processor to read)

    // Set data bus driver direction inward - onto PIB
    write32(ARM_GPIO_GPSET0, BR_DATA_DIR_IN_MASK);

    // Set output enable on data bus driver by resetting flip-flop
    // Note that the outputs of the data bus buffer are enabled from this point until
    // a rising edge of IORQ or MREQ
    // This can cause a bus conflict if BR_DATA_DIR_IN is set low before this happens
    muxDataBusOutputEnable();

    // Delay to allow data to settle
    lowlev_cycleDelay(CYCLES_DELAY_FOR_READ_FROM_PIB);

    // Read the data bus values
    dataBusVals = pibGetValue() & 0xff;
}
