// Bus Raider
// Rob Dobson 2018-2019

#include "BusRawAccess.h"
#include "PiWiring.h"
#include "lowlev.h"
#include "lowlib.h"
#include <circle/bcm2835.h>

// Module name
static const char MODULE_PREFIX[] = "BusRawBusRq";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Request bus, wait until available and take control
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BR_RETURN_TYPE BusRawAccess::busRequestAndTake()
{
    // Request
    busReqStart();

    // Check for ack
    if (!busReqWaitForAck(true))
    {
        // We didn't get the bus
        busReqRelease();
        return BR_NO_BUS_ACK;
    }

    // Take control
    busReqTakeControl();
    return BR_OK;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Bus release
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusRawAccess::busReqRelease()
{
    // Prime flip-flop that skips refresh cycles
    // So that the very first MREQ cycle after a BUSRQ/BUSACK causes a WAIT to be generated
    // (if memory waits are enabled)

    if (_hwVersionNumber == 17)
    {
        // Set M1 high via the PIB
        // Need to make data direction out in case FF OE is active
        write32(ARM_GPIO_GPCLR0, BR_DATA_DIR_IN_MASK);
        pibSetOut();
        write32(ARM_GPIO_GPSET0, BR_V17_M1_PIB_BAR_MASK | BR_IORQ_BAR_MASK);
        lowlev_cycleDelay(CYCLES_DELAY_FOR_M1_SETTLING);
        write32(ARM_GPIO_GPCLR0, BR_MREQ_BAR_MASK);
        lowlev_cycleDelay(CYCLES_DELAY_FOR_MREQ_FF_RESET);
        write32(ARM_GPIO_GPSET0, BR_MREQ_BAR_MASK);
        pibSetIn();
    }
    else
    {   
        // Set M1 high (and other control lines)
        write32(ARM_GPIO_GPSET0, BR_V20_M1_BAR_MASK | BR_IORQ_BAR_MASK | BR_RD_BAR_MASK | BR_WR_BAR_MASK | BR_MREQ_BAR_MASK);
        // Pulse MREQ to prime the FF
        // For V2.0 hardware this also clears the FF that controls data bus output enables (i.e. disables data bus output)
        // but that doesn't work on V1.7 hardware as BUSRQ holds the FF active
        lowlev_cycleDelay(CYCLES_DELAY_FOR_MREQ_FF_RESET);
        write32(ARM_GPIO_GPCLR0, BR_MREQ_BAR_MASK);
        lowlev_cycleDelay(CYCLES_DELAY_FOR_MREQ_FF_RESET);
        write32(ARM_GPIO_GPSET0, BR_MREQ_BAR_MASK);
    }

#ifdef ATTEMPT_TO_CLEAR_WAIT_FF_WHILE_STILL_IN_BUSRQ

    // Set M1 high (inactive)
    // Since in V1.7 hardware this is PUSH_ADDR then this is also ok for that case
    write32(ARM_GPIO_GPSET0, BR_V20_M1_BAR_MASK);

    // Pulse MREQ to prime the FF
    // For V2.0 hardware this also clears the FF that controls data bus output enables (i.e. disables data bus output)
    // but that doesn't work on V1.7 hardware as BUSRQ holds the FF active
    digitalWrite(BR_IORQ_BAR, 1);
    lowlev_cycleDelay(CYCLES_DELAY_FOR_MREQ_FF_RESET);
    digitalWrite(BR_MREQ_BAR, 0);
    lowlev_cycleDelay(CYCLES_DELAY_FOR_MREQ_FF_RESET);
    digitalWrite(BR_MREQ_BAR, 1);

#endif
    // Go back to data inwards
    pibSetIn();
    write32(ARM_GPIO_GPSET0, 1 << BR_DATA_DIR_IN);

    // Clear the mux to deactivate all signals
    muxClear();

    // Address bus disabled
    if (_hwVersionNumber == 17)
        digitalWrite(BR_V17_PUSH_ADDR_BAR, 1);

    // Clear wait detected in case we created some MREQ or IORQ cycles that
    // triggered wait
    // LogWrite("BusAccess", LOG_DEBUG, "controlRelease");
    waitResetFlipFlops(true);
    
    // Clear wait detection - only does something if Pi interrupts are used
    waitClearDetected();

    // Re-establish wait generation
    waitSuspend(false);

    // TODO 2020 removed
    // Check if we need to assert any new bus requests
    // busActionHandleStart();

    // No longer request bus & set all control lines high (inactive)
    uint32_t setMask = BR_BUSRQ_BAR_MASK | BR_WR_BAR_MASK | BR_RD_BAR_MASK | BR_IORQ_BAR_MASK | BR_MREQ_BAR_MASK | BR_WAIT_BAR_MASK;
    if (_hwVersionNumber != 17)
        setMask = setMask | BR_V20_M1_BAR_MASK;
    write32(ARM_GPIO_GPSET0, setMask);

    // Control bus lines to input
    pinMode(BR_WR_BAR, INPUT);
    pinMode(BR_RD_BAR, INPUT);
    pinMode(BR_MREQ_BAR, INPUT);
    pinMode(BR_IORQ_BAR, INPUT);
    pinMode(BR_WAIT_BAR_PIN, INPUT);

    if (_hwVersionNumber != 17)
    {
        // For V2.0 set GPIO3 which is M1 to an input
        pinMode(BR_V20_M1_BAR, INPUT);
    }

    // // Wait until BUSACK is released
    busReqWaitForAck(false);

    // Bus no longer under BusRaider control
    _busReqAcknowledged = false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Start bus request
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusRawAccess::busReqStart()
{
    // Set the PIB to input
    pibSetIn();
    // Set data bus to input and ensure the WR, RD, MREQ, IORQ lines are high
    write32(ARM_GPIO_GPSET0, BR_DATA_DIR_IN_MASK | BR_IORQ_BAR_MASK | BR_MREQ_BAR_MASK | BR_WR_BAR_MASK | BR_RD_BAR_MASK | (_hwVersionNumber == 17 ? BR_V20_M1_BAR_MASK : 0));
    // Request the bus
    digitalWrite(BR_BUSRQ_BAR, 0);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Take control of bus
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusRawAccess::busReqTakeControl()
{
    // Bus is under BusRaider control
    _busReqAcknowledged = true;

    // Disable wait generation while in control of bus
    waitSuspend(true);

    // Set the PIB to input
    pibSetIn();
    // Set data bus to input
    write32(ARM_GPIO_GPSET0, 1 << BR_DATA_DIR_IN);

    // Control bus
    setPinOut(BR_WR_BAR, 1);
    setPinOut(BR_RD_BAR, 1);
    setPinOut(BR_MREQ_BAR, 1);
    setPinOut(BR_IORQ_BAR, 1);
    setPinOut(BR_WAIT_BAR_PIN, 1);

    // V2.0 hardware uses GPIO3 as the M1 line whereas V1.7 uses it for
    // PUSH_ADDR - in either case it is needed as an output during BUSRQ
    setPinOut(BR_V20_M1_BAR, 1);

    // Address bus enabled (note this is using GPIO3 mentioned above in the V1.7 case)
    // On V2.0 hardware address push is done automatically
    if (_hwVersionNumber == 17)
    {
        digitalWrite(BR_V17_PUSH_ADDR_BAR, 0);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Wait for bus ack
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool BusRawAccess::busReqWaitForAck(bool ack)
{
    // Initially check very frequently so the response is fast
    for (int j = 0; j < 1000; j++)
        if (rawBUSAKActive() == ack)
            break;

    // Fall-back to slower checking which can be timed against target clock speed
    if (rawBUSAKActive() != ack)
    {
        uint32_t maxUsToWait = 500000; //BusSocketInfo::getUsFromTStates(BR_MAX_WAIT_FOR_BUSACK_T_STATES, clockCurFreqHz());
        if (maxUsToWait <= 0)
            maxUsToWait = 1;
        for (uint32_t j = 0; j < maxUsToWait; j++)
        {
            if (rawBUSAKActive() == ack)
                break;
            microsDelay(1);
        }
    }

    // LogWrite(MODULE_PREFIX, LOG_DEBUG, "waitForBusAck controlBusReqAcknowledged %d ack %d GPIO %d", 
    //         controlBusReqAcknowledged(), ack, read32(ARM_GPIO_GPLEV0));

    // Check we succeeded
    return rawBUSAKActive() == ack;
}


