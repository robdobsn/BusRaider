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
    // Mux inactive
    write32(ARM_GPIO_GPSET0, BR_MUX_EN_BAR_MASK);
    microsDelay(1);

    // Calculate bit patterns outside loop
    uint32_t reqLinePlusRead = ((accessType == ACCESS_IO) ? BR_IORQ_BAR_MASK : BR_MREQ_BAR_MASK) | (1 << BR_RD_BAR);

    // Check version
    if (_hwVersionNumber != 17)
    {
        // Form GPIO control values
        uint32_t startReqRdClrVals = reqLinePlusRead | BR_MUX_CTRL_BIT_MASK;
        uint32_t startDataOESetVals = BR_MUX_DATA_OE_BAR_LOW << BR_MUX_LOW_BIT_POS | BR_MUX_EN_BAR_MASK;
        uint32_t startOutputEnableVals = BR_MUX_EN_BAR_MASK;
        uint32_t lowAddrIncClockLow = BR_MUX_CTRL_BIT_MASK | BR_MUX_EN_BAR_MASK;
        uint32_t endReqRdSetVals = reqLinePlusRead | BR_MUX_EN_BAR_MASK;

        // Iterate data
        for (uint32_t i = 0; i < len; i++)
        {
            // IORQ_BAR / MREQ_BAR and RD_BAR both active, MUX cleared
            write32(ARM_GPIO_GPCLR0, startReqRdClrVals);
            write32(ARM_GPIO_GPSET0, startDataOESetVals);
            write32(ARM_GPIO_GPCLR0, startOutputEnableVals);

            // Pulse width required to reset DATA_OE_BAR flip-flop
            // TODO 2020 - may require delay here

            // Prepare to inc low address by setting low-addr counter clock
            // line to low - repeated to increase settling time
            // TODO 2020 - evaluate limits on this timing
            write32(ARM_GPIO_GPCLR0, lowAddrIncClockLow);
            write32(ARM_GPIO_GPCLR0, lowAddrIncClockLow);
            write32(ARM_GPIO_GPCLR0, lowAddrIncClockLow);
            write32(ARM_GPIO_GPCLR0, lowAddrIncClockLow);
            write32(ARM_GPIO_GPCLR0, lowAddrIncClockLow);
            write32(ARM_GPIO_GPCLR0, lowAddrIncClockLow);
            write32(ARM_GPIO_GPCLR0, lowAddrIncClockLow);
            write32(ARM_GPIO_GPCLR0, lowAddrIncClockLow);

            // TODO 2020
            // pibGetValue()
            // if (pibGetValue() != *pData)
            // {
            //     digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
            //     microsDelay(1);
            //     digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
            // }

            // Get the data
            *pData = pibGetValue();

            // Deactivate IORQ/MREQ and RD and clock the low address
            write32(ARM_GPIO_GPSET0, endReqRdSetVals);

            // Increment addresses
            pData++;
            addr++;

            // Check if we've rolled over the lowest 8 bits
            if ((addr & 0xff) == 0) {

                // Set the address again
                addrSet(addr);
            }
        }
    }
    else
    {
        // Iterate data
        for (uint32_t i = 0; i < len; i++)
        {
            // Enable data bus driver output - must be done each time round the loop as it is
            // cleared by IORQ or MREQ rising edge
            muxDataBusOutputEnable();
                        
            // IORQ_BAR / MREQ_BAR and RD_BAR both active
            write32(ARM_GPIO_GPCLR0, reqLinePlusRead);
            
            // Delay to allow data bus to settle
            lowlev_cycleDelay(CYCLES_DELAY_FOR_READ_FROM_PIB);        

            // Get the data
            *pData = pibGetValue();

            // Deactivate IORQ/MREQ and RD and clock the low address
            write32(ARM_GPIO_GPSET0, reqLinePlusRead);

            // Inc low address
            addrLowInc();

            // Increment addresses
            pData++;
            addr++;

            // Check if we've rolled over the lowest 8 bits
            if ((addr & 0xff) == 0) 
            {
                // Set the address again
                addrSet(addr);
            }
        }
    }

    // Check if we need to release bus
    if (busRqAndRelease) {
        // release bus
        controlRelease();
    }
    return BR_OK;
}

// Write a consecutive block of memory to host
BR_RETURN_TYPE BusAccess::blockWrite(uint32_t addr, const uint8_t* pData, uint32_t len, BlockAccessType accessType)
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

    // Set the address to initial value
    addrSet(addr);

    // Set the PIB to output
    pibSetOut();

    // Check version
    if (_hwVersionNumber != 17)
    {
        // Calculate bit patterns outside loop
        uint32_t reqLinePlusDataInMux = ((accessType == ACCESS_IO) ? BR_IORQ_BAR_MASK : BR_MREQ_BAR_MASK) | BR_DATA_DIR_IN_MASK | BR_MUX_CTRL_BIT_MASK;
        uint32_t lowAddrIncClockLow = BR_MUX_CTRL_BIT_MASK | BR_MUX_EN_BAR_MASK;

        // microsDelay(1);

        // Iterate data
        for (uint32_t i = 0; i < len; i++)
        {
            // microsDelay(1);

            // Set the data onto the PIB
            pibSetValue(*pData);

            // microsDelay(1);

            // Perform the write
            // Clear DIR_IN (so make direction out), enable data output onto data bus and MREQ/IORQ active
            write32(ARM_GPIO_GPCLR0, reqLinePlusDataInMux);
            write32(ARM_GPIO_GPSET0, BR_MUX_DATA_OE_BAR_LOW << BR_MUX_LOW_BIT_POS);
            // Set WR_BAR active to start the write cycle
            write32(ARM_GPIO_GPCLR0, BR_WR_BAR_MASK | BR_MUX_EN_BAR_MASK);

            // Prepare to inc low address by setting low-addr counter clock
            // line to low - repeated to increase settling time
            // TODO 2020 - evaluate limits on this timing
            write32(ARM_GPIO_GPCLR0, lowAddrIncClockLow);
            write32(ARM_GPIO_GPCLR0, lowAddrIncClockLow);
            write32(ARM_GPIO_GPCLR0, lowAddrIncClockLow);
            write32(ARM_GPIO_GPCLR0, lowAddrIncClockLow);
            write32(ARM_GPIO_GPCLR0, lowAddrIncClockLow);
            write32(ARM_GPIO_GPCLR0, lowAddrIncClockLow);
            write32(ARM_GPIO_GPCLR0, lowAddrIncClockLow);
            write32(ARM_GPIO_GPCLR0, lowAddrIncClockLow);

            // microsDelay(1);

            // Deactivate, clock the low address and leave data direction set to inwards
            write32(ARM_GPIO_GPSET0, BR_WR_BAR_MASK );

            // microsDelay(1);

            write32(ARM_GPIO_GPSET0, BR_MUX_EN_BAR_MASK | reqLinePlusDataInMux);

            // microsDelay(1);

            // Increment addresses
            pData++;
            addr++;

            // Check if we've rolled over the lowest 8 bits
            if ((addr & 0xff) == 0) 
            {
                // Set the address again
                addrSet(addr);
            }
        }

        // microsDelay(1);

    }
    else
    {
        // Iterate data
        for (uint32_t i = 0; i < len; i++)
        {
            // Write byte
            byteWrite(*pData, accessType);

            // Increment the lower address counter
            addrLowInc();

            // Increment addresses
            pData++;
            addr++;

            // Check if we've rolled over the lowest 8 bits
            if ((addr & 0xff) == 0) 
            {
                // Set the address again
                addrSet(addr);
            }
        }
    }

    // Set the PIB back to INPUT
    pibSetIn();

    // Check if we need to release bus
    if (busRqAndRelease) {
        // release bus
        controlRelease();
    }
    return BR_OK;
}

// // Read a consecutive block of memory from host
// // Assumes:
// // - control of host bus has been requested and acknowledged
// BR_RETURN_TYPE BusAccess::blockRead(uint32_t addr, uint8_t* pData, uint32_t len, BlockAccessType accessType)
// {
//     // Check if we need to request bus
//     bool busRqAndRelease = !_busIsUnderControl;
//     if (busRqAndRelease) {
//         // Request bus and take control after ack
//         BR_RETURN_TYPE ret = controlRequestAndTake();
//         if (ret != BR_OK)
//             return ret;
//     }

//     // Set PIB to input
//     pibSetIn();

//     // Data direction for data bus drivers inward
//     write32(ARM_GPIO_GPSET0, BR_DATA_DIR_IN_MASK);

//     // Set the address to initial value
//     addrSet(addr);

//     // Calculate bit patterns outside loop
//     uint32_t reqLinePlusRead = ((accessType == ACCESS_IO) ? BR_IORQ_BAR_MASK : BR_MREQ_BAR_MASK) | (1 << BR_RD_BAR);

//     // Iterate data
//     for (uint32_t i = 0; i < len; i++)
//     {

//         // Enable data bus driver output - must be done each time round the loop as it is
//         // cleared by IORQ or MREQ rising edge
//         muxDataBusOutputEnable();

//         // IORQ_BAR / MREQ_BAR and RD_BAR both active
//         write32(ARM_GPIO_GPCLR0, reqLinePlusRead);
        
//         // Delay to allow data bus to settle
//         //TODO 2020
//         // lowlev_cycleDelay(CYCLES_DELAY_FOR_READ_FROM_PIB);
//         microsDelay(1);
        
//         // // TODO 2020
//         // lowlev_cycleDelay(10000);

//         // TODO 2020
//         // pibGetValue()
//         if (pibGetValue() != *pData)
//         {
//             digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
//             microsDelay(1);
//             digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
//         }

//         // Get the data
//         *pData = pibGetValue();

//         // // TODO 2020
//         // lowlev_cycleDelay(10000);

//         // if (i == 0)
//         // {
//         //     LogWrite(MODULE_PREFIX, LOG_NOTICE, "blockRead %08x", read32(ARM_GPIO_GPLEV0));
//         // }

//         // Deactivate IORQ/MREQ and RD and clock the low address
//         write32(ARM_GPIO_GPSET0, reqLinePlusRead);

//         // Inc low address
//         addrLowInc();

//         // Increment addresses
//         pData++;
//         addr++;

//         // Check if we've rolled over the lowest 8 bits
//         if ((addr & 0xff) == 0) {

//             // Set the address again
//             addrSet(addr);
//         }
//     }

//     // Check if we need to release bus
//     if (busRqAndRelease) {
//         // release bus
//         controlRelease();
//     }
//     return BR_OK;
// }
