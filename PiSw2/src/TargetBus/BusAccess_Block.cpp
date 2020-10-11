// Bus Raider
// Rob Dobson 2018-2019

#include "BusAccess.h"
#include "PiWiring.h"
#include "lowlev.h"
#include "lowlib.h"
#include <circle/bcm2835.h>

// Module name
static const char MODULE_PREFIX[] = "BusAccBlock";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Block access
// Read a consecutive block of memory from host
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BR_RETURN_TYPE BusAccess::blockRead(uint32_t addr, uint8_t* pData, uint32_t len, BlockAccessType accessType)
{
    // Check if we need to request bus
    bool busRqAndRelease = !_busIsUnderControl;
    if (busRqAndRelease) {
        // Request bus and take control after ack
        BR_RETURN_TYPE ret = controlRequestAndTake();
        if (ret != BR_OK)
            return ret;
    }

    // Set PIB to input
    pibSetIn();

    // Data direction for data bus drivers inward
    write32(ARM_GPIO_GPSET0, BR_DATA_DIR_IN_MASK);

    // Set the address to initial value
    addrSet(addr);
    microsDelay(1);

    // Calculate bit patterns outside loop
    uint32_t reqLinePlusRead = ((accessType == ACCESS_IO) ? BR_IORQ_BAR_MASK : BR_MREQ_BAR_MASK) | (1 << BR_RD_BAR);

    // Iterate data
    for (uint32_t i = 0; i < len; i++)
    {
        // IORQ_BAR / MREQ_BAR and RD_BAR both active
        write32(ARM_GPIO_GPCLR0, reqLinePlusRead);

        // Start mux data bus output - see comment about end of this in the loop
        muxDataBusOutputStart();

        // Delay to allow data bus to settle
        //TODO 2020
        lowlev_cycleDelay(CYCLES_DELAY_FOR_READ_FROM_PIB);
        // microsDelay(1);
        
        // // TODO 2020
        // lowlev_cycleDelay(10000);

        // TODO 2020
        // pibGetValue()
        if (pibGetValue() != *pData)
        {
            digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
            microsDelay(1);
            digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
        }

        // Get the data
        *pData = pibGetValue();

        // Enable data bus driver output - must be done each time round the loop as it is
        // cleared by IORQ or MREQ rising edge
        muxDataBusOutputFinish();

        // // TODO 2020
        // lowlev_cycleDelay(10000);

        // if (i == 0)
        // {
        //     LogWrite(MODULE_PREFIX, LOG_NOTICE, "blockRead %08x", read32(ARM_GPIO_GPLEV0));
        // }

        // Deactivate IORQ/MREQ and RD and clock the low address
        write32(ARM_GPIO_GPSET0, reqLinePlusRead);

        // Start mux data bus output - see comment about end of this at start of loop
        if (i+1 != len)
            muxDataBusOutputStart();

        // Inc low address
        addrLowIncStart();

        // Increment addresses
        pData++;
        addr++;

        // Check if we've rolled over the lowest 8 bits
        if ((addr & 0xff) == 0) {

            // Set the address again
            addrSet(addr);
        }
    }

    // Check if we need to release bus
    if (busRqAndRelease) {
        // release bus
        controlRelease();
    }
    return BR_OK;
}