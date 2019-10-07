// Bus Raider
// Rob Dobson 2018-2019

#include "BusAccess.h"
#include "../System/PiWiring.h"
#include "../System/CInterrupts.h"
#include "../System/lowlev.h"
#include "../System/lowlib.h"
#include "../System/BCM2835.h"
#include "../TargetBus/TargetClockGenerator.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Variables
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Bus under BusRaider control
volatile bool BusAccess::_busIsUnderControl = false;

// Clock generator
TargetClockGenerator BusAccess::_clockGenerator;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Bus Sockets
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Add a bus socket
int BusAccess::busSocketAdd(BusSocketInfo& busSocketInfo)
{
    // Check if all used
    if (_busSocketCount >= MAX_BUS_SOCKETS)
        return -1;

    // Add in available space
    _busSockets[_busSocketCount] = busSocketInfo;
    int tmpCount = _busSocketCount++;

    // Update wait state generation
    // LogWrite("BusAccess", LOG_DEBUG, "busSocketAdd");
    waitEnablementUpdate();

    return tmpCount;
}

void BusAccess::busSocketEnable(int busSocket, bool enable)
{
    // Check validity
    if ((busSocket < 0) || (busSocket >= _busSocketCount))
        return;

    // Enable/disable
    _busSockets[busSocket].enabled = enable;

    // Update wait state generation
    // LogWrite("BusAccess", LOG_DEBUG, "busSocketEnable");
    waitEnablementUpdate();
}

bool BusAccess::busSocketIsEnabled(int busSocket)
{
    // Check validity
    if ((busSocket < 0) || (busSocket >= _busSocketCount))
        return false;
    return _busSockets[busSocket].enabled;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Status
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusAccess::getStatus(BusAccessStatusInfo& statusInfo)
{
    statusInfo = _statusInfo;
}

void BusAccess::clearStatus()
{
    _statusInfo.clear();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Bus Request / Acknowledge
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Is under control
bool BusAccess::isUnderControl()
{
    return _busIsUnderControl;
}

// Request access to the bus
void BusAccess::controlRequest()
{
    // Set the PIB to input
    pibSetIn();
    // Set data bus to input
    WR32(ARM_GPIO_GPSET0, BR_DATA_DIR_IN_MASK);
    // Request the bus
    digitalWrite(BR_BUSRQ_BAR, 0);
}

// Take control of bus
void BusAccess::controlTake()
{
    // Bus is under BusRaider control
    _busIsUnderControl = true;

    // Disable wait generation while in control of bus
    waitGenerationDisable();

    // Set the PIB to input
    pibSetIn();
    // Set data bus to input
    WR32(ARM_GPIO_GPSET0, 1 << BR_DATA_DIR_IN);

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

// Release control of bus
void BusAccess::controlRelease()
{
    // Prime flip-flop that skips refresh cycles
    // So that the very first MREQ cycle after a BUSRQ/BUSACK causes a WAIT to be generated
    // (if memory waits are enabled)

    if (_hwVersionNumber == 17)
    {
        // Set M1 high via the PIB
        // Need to make data direction out in case FF OE is active
        WR32(ARM_GPIO_GPCLR0, BR_DATA_DIR_IN_MASK);
        pibSetOut();
        WR32(ARM_GPIO_GPSET0, BR_V17_M1_PIB_BAR_MASK | BR_IORQ_BAR_MASK);
        lowlev_cycleDelay(CYCLES_DELAY_FOR_M1_SETTLING);
        WR32(ARM_GPIO_GPCLR0, BR_MREQ_BAR_MASK);
        lowlev_cycleDelay(CYCLES_DELAY_FOR_MREQ_FF_RESET);
        WR32(ARM_GPIO_GPSET0, BR_MREQ_BAR_MASK);
        pibSetIn();
    }
    else
    {   
        // Set M1 high (and other control lines)
        WR32(ARM_GPIO_GPSET0, BR_V20_M1_BAR_MASK | BR_IORQ_BAR_MASK | BR_RD_BAR_MASK | BR_WR_BAR_MASK | BR_MREQ_BAR_MASK);
        // Pulse MREQ to prime the FF
        // For V2.0 hardware this also clears the FF that controls data bus output enables (i.e. disables data bus output)
        // but that doesn't work on V1.7 hardware as BUSRQ holds the FF active
        lowlev_cycleDelay(CYCLES_DELAY_FOR_MREQ_FF_RESET);
        WR32(ARM_GPIO_GPCLR0, BR_MREQ_BAR_MASK);
        lowlev_cycleDelay(CYCLES_DELAY_FOR_MREQ_FF_RESET);
        WR32(ARM_GPIO_GPSET0, BR_MREQ_BAR_MASK);
    }

#ifdef ATTEMPT_TO_CLEAR_WAIT_FF_WHILE_STILL_IN_BUSRQ

    // Set M1 high (inactive)
    // Since in V1.7 hardware this is PUSH_ADDR then this is also ok for that case
    WR32(ARM_GPIO_GPSET0, BR_V20_M1_BAR_MASK);

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
    WR32(ARM_GPIO_GPSET0, 1 << BR_DATA_DIR_IN);

    // Clear the mux to deactivate all signals
    muxClear();

    // Address bus disabled
    if (_hwVersionNumber == 17)
        digitalWrite(BR_V17_PUSH_ADDR_BAR, 1);

    // Clear wait detected in case we created some MREQ or IORQ cycles that
    // triggered wait
    waitResetFlipFlops(true);

    // Clear wait detection - only does something if Pi interrupts are used
    waitClearDetected();

    // Re-establish wait generation
    // LogWrite("BusAccess", LOG_DEBUG, "controlRelease");
    waitEnablementUpdate();

    // Check if any bus action is pending
    busActionCheck();

    // Check if we need to assert any new bus requests
    busActionHandleStart();

    // No longer request bus & set all control lines high (inactive)
    uint32_t setMask = BR_BUSRQ_BAR_MASK | BR_WR_BAR_MASK | BR_RD_BAR_MASK | BR_IORQ_BAR_MASK | BR_MREQ_BAR_MASK | BR_WAIT_BAR_MASK;
    if (_hwVersionNumber != 17)
        setMask = setMask | BR_V20_M1_BAR_MASK;
    WR32(ARM_GPIO_GPSET0, setMask);

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
    waitForBusAck(false);

    // Bus no longer under BusRaider control
    _busIsUnderControl = false;
}

// Request bus, wait until available and take control
BR_RETURN_TYPE BusAccess::controlRequestAndTake()
{
    // Request
    controlRequest();

    // Check for ack
    if (!waitForBusAck(true))
    {
        // We didn't get the bus
        controlRelease();
        return BR_NO_BUS_ACK;
    }

    // Take control
    controlTake();
    return BR_OK;
}

bool BusAccess::waitForBusAck(bool ack)
{
    // Initially check very frequently so the response is fast
    for (int j = 0; j < 1000; j++)
        if (controlBusReqAcknowledged() == ack)
            break;

    // Fall-back to slower checking which can be timed against target clock speed
    if (controlBusReqAcknowledged() != ack)
    {
        uint32_t maxUsToWait = BusSocketInfo::getUsFromTStates(BR_MAX_WAIT_FOR_BUSACK_T_STATES, clockCurFreqHz());
        if (maxUsToWait <= 0)
            maxUsToWait = 1;
        for (uint32_t j = 0; j < maxUsToWait; j++)
        {
            if (controlBusReqAcknowledged() == ack)
                break;
            microsDelay(1);
        }
    }

    // Check we succeeded
    return controlBusReqAcknowledged() == ack;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Control Bus Functions
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Control bus read
uint32_t BusAccess::controlBusRead()
{
    uint32_t startGetCtrlBusUs = micros();
    int loopCount = 0;
    while (true)
    {
        // Read the control lines
        uint32_t busVals = RD32(ARM_GPIO_GPLEV0);

        // Handle slower M1 signal on V1.7 hardware
        if (_hwVersionNumber == 17)
        {
            // Check if we're in a wait - in which case FF OE will be active
            // So we must set the data bus direction outward even if this causes a temporary
            // conflict on the data bus
            WR32(ARM_GPIO_GPCLR0, BR_DATA_DIR_IN_MASK);

            // Delay for settling of M1
            lowlev_cycleDelay(CYCLES_DELAY_FOR_M1_SETTLING);

            // Read the control lines
            busVals = RD32(ARM_GPIO_GPLEV0);

            // Set data bus driver direction inward - onto PIB
            WR32(ARM_GPIO_GPSET0, BR_DATA_DIR_IN_MASK);
        }

        // Get the appropriate bits for up-line communication
        uint32_t ctrlBusVals = 
                (((busVals & BR_RD_BAR_MASK) == 0) ? BR_CTRL_BUS_RD_MASK : 0) |
                (((busVals & BR_WR_BAR_MASK) == 0) ? BR_CTRL_BUS_WR_MASK : 0) |
                (((busVals & BR_MREQ_BAR_MASK) == 0) ? BR_CTRL_BUS_MREQ_MASK : 0) |
                (((busVals & BR_IORQ_BAR_MASK) == 0) ? BR_CTRL_BUS_IORQ_MASK : 0) |
                (((busVals & BR_WAIT_BAR_MASK) == 0) ? BR_CTRL_BUS_WAIT_MASK : 0) |
                (((busVals & BR_V20_M1_BAR_MASK) == 0) ? BR_CTRL_BUS_M1_MASK : 0) |
                (((busVals & BR_BUSACK_BAR_MASK) == 0) ? BR_CTRL_BUS_BUSACK_MASK : 0);

        // Handle slower M1 signal on V1.7 hardware
        if (_hwVersionNumber == 17)
        {
            // Clear M1 in case set above
            ctrlBusVals = ctrlBusVals & (~BR_CTRL_BUS_M1_MASK);

            // Set M1 from PIB reading
            ctrlBusVals |= (((busVals & BR_V17_M1_PIB_BAR_MASK) == 0) ? BR_CTRL_BUS_M1_MASK : 0);
        }

        // Check if valid (MREQ || IORQ) && (RD || WR)
        bool ctrlValid = ((ctrlBusVals & BR_CTRL_BUS_IORQ_MASK) || (ctrlBusVals & BR_CTRL_BUS_MREQ_MASK)) && ((ctrlBusVals & BR_CTRL_BUS_RD_MASK) || (ctrlBusVals & BR_CTRL_BUS_WR_MASK));
        // Also valid if IORQ && M1 as this is used for interrupt ack
        ctrlValid = ctrlValid || ((ctrlBusVals & BR_CTRL_BUS_IORQ_MASK) && (ctrlBusVals & BR_CTRL_BUS_M1_MASK));
        
        // If ctrl is already valid then continue
        if (ctrlValid)
        {
            // if (((ctrlBusVals & BR_CTRL_BUS_IORQ_MASK) == 0) && ((ctrlBusVals & BR_CTRL_BUS_MREQ_MASK) == 0))
            // {
            //     // TODO
            //     digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
            //     microsDelay(1);
            //     digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
            // }
            return ctrlBusVals;
        }

        // Break out if time-out and enough loops done
        loopCount++;
        if ((isTimeout(micros(), startGetCtrlBusUs, MAX_WAIT_FOR_CTRL_BUS_VALID_US)) && (loopCount > MIN_LOOP_COUNT_FOR_CTRL_BUS_VALID))
        {
            // // TODO
            // digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
            // microsDelay(1);
            // digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
            // microsDelay(1);
            // digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
            // microsDelay(1);
            // digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
            // microsDelay(1);
            // digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
            // microsDelay(1);
            // digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
            // microsDelay(1);
            return ctrlBusVals;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Address Bus Functions
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Set low address value by clearing and counting
void BusAccess::addrLowSet(uint32_t lowAddrByte)
{
    // Clear initially
    muxClearLowAddr();
    // Clock the required value in - requires one more count than
    // expected as the output register is one clock pulse behind the counter
    if (_hwVersionNumber == 17)
    {
        for (uint32_t i = 0; i < (lowAddrByte & 0xff) + 1; i++) {
            WR32(ARM_GPIO_GPSET0, BR_V17_LADDR_CK_MASK);
            lowlev_cycleDelay(CYCLES_DELAY_FOR_LOW_ADDR_SET);
            WR32(ARM_GPIO_GPCLR0, BR_V17_LADDR_CK_MASK);
            lowlev_cycleDelay(CYCLES_DELAY_FOR_LOW_ADDR_SET);
        }
    }
    else
    {
        WR32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
        WR32(ARM_GPIO_GPSET0, BR_MUX_LADDR_CLK << BR_MUX_LOW_BIT_POS);
        for (uint32_t i = 0; i < (lowAddrByte & 0xff) + 1; i++) {
#ifdef V2_PROTO_USING_MUX_EN
            WR32(ARM_GPIO_GPCLR0, BR_MUX_EN_BAR_MASK);
            lowlev_cycleDelay(CYCLES_DELAY_FOR_CLOCK_LOW_ADDR);
            WR32(ARM_GPIO_GPSET0, BR_MUX_EN_BAR_MASK);
            lowlev_cycleDelay(CYCLES_DELAY_FOR_CLOCK_LOW_ADDR);
#else
            // This clears the OE FFbut is used as a safe way
            // to cycle the low address clock
            WR32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
            WR32(ARM_GPIO_GPSET0, BR_MUX_DATA_OE_BAR_LOW << BR_MUX_LOW_BIT_POS);
            lowlev_cycleDelay(CYCLES_DELAY_FOR_CLOCK_LOW_ADDR);
            WR32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
            lowlev_cycleDelay(CYCLES_DELAY_FOR_CLOCK_LOW_ADDR);
#endif
        }
    }
}

// Increment low address value by clocking the counter
void BusAccess::addrLowInc()
{
    if (_hwVersionNumber == 17)
    {
        WR32(ARM_GPIO_GPSET0, BR_V17_LADDR_CK_MASK);
        lowlev_cycleDelay(CYCLES_DELAY_FOR_LOW_ADDR_SET);
        WR32(ARM_GPIO_GPCLR0, BR_V17_LADDR_CK_MASK);
        lowlev_cycleDelay(CYCLES_DELAY_FOR_LOW_ADDR_SET);
    }
    else
    {
#ifdef V2_PROTO_USING_MUX_EN
        // This sets the low address clock low as it is MUX0
        WR32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
        WR32(ARM_GPIO_GPCLR0, BR_MUX_EN_BAR_MASK);
        lowlev_cycleDelay(CYCLES_DELAY_FOR_CLOCK_LOW_ADDR);
        WR32(ARM_GPIO_GPSET0, BR_MUX_EN_BAR_MASK);
#else
        // This clears the OE FF but is used as a safe way
        // to cycle the low address clock
        WR32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
        WR32(ARM_GPIO_GPSET0, BR_MUX_DATA_OE_BAR_LOW << BR_MUX_LOW_BIT_POS);
        lowlev_cycleDelay(CYCLES_DELAY_FOR_CLOCK_LOW_ADDR);
        WR32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
        lowlev_cycleDelay(CYCLES_DELAY_FOR_CLOCK_LOW_ADDR);
#endif
    }
}

// Set the high address value
void BusAccess::addrHighSet(uint32_t highAddrByte)
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
            WR32(ARM_GPIO_GPSET0, 1 << BR_HADDR_CK);
            lowlev_cycleDelay(CYCLES_DELAY_FOR_HIGH_ADDR_SET);
            WR32(ARM_GPIO_GPCLR0, 1 << BR_HADDR_CK);
        }
    }
    else
    {
        // Shift the value into the register
        // Takes one more shift than expected as output reg is one pulse behind shift
        for (uint32_t i = 0; i < 9; i++) {
            // Set or clear serial pin to shift register
            if (highAddrByte & 0x80)
                muxClear();
            else
                // Mux low address clear doubles as high address serial in 
                muxSet(BR_MUX_LADDR_CLR_BAR_LOW);
            // Delay to allow settling
            lowlev_cycleDelay(CYCLES_DELAY_FOR_HIGH_ADDR_SET);
            // Shift the address value for next bit
            highAddrByte = highAddrByte << 1;
            // Clock the bit
            WR32(ARM_GPIO_GPSET0, 1 << BR_HADDR_CK);
            lowlev_cycleDelay(CYCLES_DELAY_FOR_HIGH_ADDR_SET);
            WR32(ARM_GPIO_GPCLR0, 1 << BR_HADDR_CK);
        }
    }

    // Clear multiplexer
    lowlev_cycleDelay(CYCLES_DELAY_FOR_HIGH_ADDR_SET);
    muxClear();
}

// Set the full address
void BusAccess::addrSet(unsigned int addr)
{
    addrHighSet(addr >> 8);
    addrLowSet(addr & 0xff);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Read and Write Host Memory
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Write a single byte to currently set address (or IO port)
// Assumes:
// - control of host bus has been requested and acknowledged
// - address bus is already set and output enabled to host bus
// - PIB is already set to output
void BusAccess::byteWrite(uint32_t data, int iorq)
{
    // Set the data onto the PIB
    pibSetValue(data);
    // Perform the write
    // Clear DIR_IN (so make direction out), enable data output onto data bus and MREQ_BAR active
    WR32(ARM_GPIO_GPCLR0, BR_DATA_DIR_IN_MASK | BR_MUX_CTRL_BIT_MASK | (iorq ? BR_IORQ_BAR_MASK : BR_MREQ_BAR_MASK));
    WR32(ARM_GPIO_GPSET0, BR_MUX_DATA_OE_BAR_LOW << BR_MUX_LOW_BIT_POS);
    // Write the data by setting WR_BAR active
    if (_hwVersionNumber == 17)
    {
        WR32(ARM_GPIO_GPCLR0, BR_WR_BAR_MASK);
    }
    else
    {
#ifdef V2_PROTO_USING_MUX_EN
        WR32(ARM_GPIO_GPCLR0, BR_WR_BAR_MASK | BR_MUX_EN_BAR_MASK);
#else
        WR32(ARM_GPIO_GPCLR0, BR_WR_BAR_MASK);
#endif
    }
    // Target write delay
    lowlev_cycleDelay(CYCLES_DELAY_FOR_WRITE_TO_TARGET);
    // Deactivate and leave data direction set to inwards
    if (_hwVersionNumber == 17)
    {
        WR32(ARM_GPIO_GPSET0, BR_DATA_DIR_IN_MASK | (iorq ? BR_IORQ_BAR_MASK : BR_MREQ_BAR_MASK) | BR_WR_BAR_MASK);
        muxClear();
    }
    else
    {
#ifdef V2_PROTO_USING_MUX_EN
        WR32(ARM_GPIO_GPSET0, BR_DATA_DIR_IN_MASK | BR_MUX_EN_BAR_MASK | (iorq ? BR_IORQ_BAR_MASK : BR_MREQ_BAR_MASK) | BR_WR_BAR_MASK);
#else
        WR32(ARM_GPIO_GPSET0, BR_DATA_DIR_IN_MASK | (iorq ? BR_IORQ_BAR_MASK : BR_MREQ_BAR_MASK) | BR_WR_BAR_MASK);
        WR32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
#endif
    }
}

// Read a single byte from currently set address (or IO port)
// Assumes:
// - control of host bus has been requested and acknowledged
// - address bus is already set and output enabled to host bus
// - PIB is already set to input
uint8_t BusAccess::byteRead(int iorq)
{
    // Enable data output onto PIB, MREQ_BAR and RD_BAR both active
    WR32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK | (iorq ? BR_IORQ_BAR_MASK : BR_MREQ_BAR_MASK) | BR_RD_BAR_MASK);
    WR32(ARM_GPIO_GPSET0, BR_DATA_DIR_IN_MASK | (BR_MUX_DATA_OE_BAR_LOW << BR_MUX_LOW_BIT_POS));
    if (_hwVersionNumber != 17)
    {
#ifdef V2_PROTO_USING_MUX_EN
        WR32(ARM_GPIO_GPCLR0, BR_MUX_EN_BAR_MASK);
#endif
    }
    // Delay to allow data to settle
    lowlev_cycleDelay(CYCLES_DELAY_FOR_READ_FROM_PIB);
    // Get the data
    uint8_t val = pibGetValue();
    // Deactivate leaving data-dir inwards
    if (_hwVersionNumber == 17)
    {
        WR32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
        WR32(ARM_GPIO_GPSET0, (iorq ? BR_IORQ_BAR_MASK : BR_MREQ_BAR_MASK) | BR_RD_BAR_MASK);
    }
    else
    {
#ifdef V2_PROTO_USING_MUX_EN
        WR32(ARM_GPIO_GPSET0, BR_MUX_EN_BAR_MASK | (iorq ? BR_IORQ_BAR_MASK : BR_MREQ_BAR_MASK) | BR_RD_BAR_MASK);
#else
        WR32(ARM_GPIO_GPSET0, (iorq ? BR_IORQ_BAR_MASK : BR_MREQ_BAR_MASK) | BR_RD_BAR_MASK);
        WR32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
#endif
    }
    return val;
}

// Write a consecutive block of memory to host
BR_RETURN_TYPE BusAccess::blockWrite(uint32_t addr, const uint8_t* pData, uint32_t len, bool busRqAndRelease, bool iorq)
{
    // Check if we need to request bus
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

    // Iterate data
    for (uint32_t i = 0; i < len; i++)
    {
        // Write byte
        byteWrite(*pData, iorq);

        // Increment the lower address counter
        addrLowInc();

        // Increment addresses
        pData++;
        addr++;

        // Check if we've rolled over the lowest 8 bits
        if ((addr & 0xff) == 0) {
            // Set the address again
            addrSet(addr);
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

// Read a consecutive block of memory from host
// Assumes:
// - control of host bus has been requested and acknowledged
BR_RETURN_TYPE BusAccess::blockRead(uint32_t addr, uint8_t* pData, uint32_t len, bool busRqAndRelease, bool iorq)
{
    // Check if we need to request bus
    if (busRqAndRelease) {
        // Request bus and take control after ack
        BR_RETURN_TYPE ret = controlRequestAndTake();
        if (ret != BR_OK)
            return ret;
    }

    // Set PIB to input
    pibSetIn();

    // Data direction for data bus drivers inward
    WR32(ARM_GPIO_GPSET0, BR_DATA_DIR_IN_MASK);

    // Set the address to initial value
    addrSet(addr);

    // Calculate bit patterns outside loop
    uint32_t reqLinePlusRead = (iorq ? BR_IORQ_BAR_MASK : BR_MREQ_BAR_MASK) | (1 << BR_RD_BAR);

    // Iterate data
    for (uint32_t i = 0; i < len; i++)
    {

        // Enable data bus driver output - must be done each time round the loop as it is
        // cleared by IORQ or MREQ rising edge
        muxDataBusOutputEnable();

        // IORQ_BAR / MREQ_BAR and RD_BAR both active
        WR32(ARM_GPIO_GPCLR0, reqLinePlusRead);
        
        // Delay to allow data bus to settle
        lowlev_cycleDelay(CYCLES_DELAY_FOR_READ_FROM_PIB);
        
        // Get the data
        *pData = pibGetValue();

        // Deactivate IORQ/MREQ and RD and clock the low address
        WR32(ARM_GPIO_GPSET0, reqLinePlusRead);

        // Inc low address
        addrLowInc();

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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Clock Generator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Setup clock generator
void BusAccess::clockSetup()
{
    _clockGenerator.setOutputPin();
}

void BusAccess::clockSetFreqHz(uint32_t freqHz)
{
    _clockGenerator.setFrequency(freqHz);
}

void BusAccess::clockEnable(bool en)
{
    _clockGenerator.enable(en);
}

uint32_t BusAccess::clockCurFreqHz()
{
    return _clockGenerator.getFreqInHz();
}

uint32_t BusAccess::clockGetMinFreqHz()
{
    return _clockGenerator.getMinFreqHz();
}

uint32_t BusAccess::clockGetMaxFreqHz()
{
    return _clockGenerator.getMaxFreqHz();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Wait helper functions
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusAccess::waitEnablementUpdate()
{
    // Iterate bus sockets to see if any enable Mem/IO wait states
    bool ioWait = false;
    bool memWait = false;
    for (int i = 0; i < _busSocketCount; i++)
    {
        if (_busSockets[i].enabled)
        {
            memWait = memWait || _busSockets[i].waitOnMemory;
            ioWait = ioWait || _busSockets[i].waitOnIO;
        }
    }

    // Store flags
    _waitOnMemory = memWait;
    _waitOnIO = ioWait;

    // LogWrite("BusAccess", LOG_DEBUG, "WAIT ENABLEMENT mreq %d iorq %d", memWait, ioWait);

    // Set PWM generator idle value to enable/disable wait states
    // This is done using the idle states of the PWM
    uint32_t pwmCtrl = RD32(ARM_PWM_CTL);
    pwmCtrl &= ~(ARM_PWM_CTL_SBIT1 | ARM_PWM_CTL_SBIT2);
    if (_waitOnIO)
        pwmCtrl |= ARM_PWM_CTL_SBIT1;
    if (_waitOnMemory)
        pwmCtrl |= ARM_PWM_CTL_SBIT2;
    WR32(ARM_PWM_CTL, pwmCtrl);

    // Debug
    // LogWrite("BusAccess", LOG_DEBUG, "WAIT UPDATE mem %d io %d", _waitOnMemory, _waitOnIO);

#ifdef ISR_TEST
    // Setup edge triggering on falling edge of IORQ
    // Clear any current detected edges
    WR32(ARM_GPIO_GPEDS0, (1 << BR_IORQ_BAR) | (1 << BR_MREQ_BAR));  
    // Set falling edge detect
    if (_waitOnIO)
    {
        WR32(ARM_GPIO_GPFEN0, RD32(ARM_GPIO_GPFEN0) | BR_IORQ_BAR_MASK);
        lowlev_enable_fiq();
    }
    else
    {
        WR32(ARM_GPIO_GPFEN0, RD32(ARM_GPIO_GPFEN0) & (~BR_IORQ_BAR_MASK));
        lowlev_disable_fiq();
    }
    // if (_waitOnMemory)
    //     WR32(ARM_GPIO_GPFEN0, RD32(ARM_GPIO_GPFEN0) | BR_WAIT_BAR_MASK);
    // else
    //     WR32(ARM_GPIO_GPFEN0, RD32(ARM_GPIO_GPFEN0) & (~BR_WAIT_BAR_MASK);
#endif
}

void BusAccess::waitGenerationDisable()
{
    // Debug
    // LogWrite("BusAccess", LOG_DEBUG, "WAIT DISABLE");
    // Set PWM idle state to disable waits
    uint32_t pwmCtrl = RD32(ARM_PWM_CTL);
    pwmCtrl &= ~(ARM_PWM_CTL_SBIT1 | ARM_PWM_CTL_SBIT2);
    WR32(ARM_PWM_CTL, pwmCtrl);
}

void BusAccess::waitSetupMREQAndIORQEnables()
{
    // Use PWM to generate pulses as the behaviour of the processor is non-deterministic in terms
    // of time taken between consecutive instructions and when MREQ/IORQ enable is low wait
    // states are disabled and bus operations can be missed

    // Clock for PWM 
    uint32_t clockSource = ARM_CM_CTL_CLKSRC_PLLD;
    uint32_t freqReqd = 31250000;

    // Disable the clock (without changing SRC and FLIP)
    WR32(ARM_CM_PWMCTL, ARM_CM_PASSWD | clockSource);

    // Wait a little if clock is busy
    int busyCount = 0;
    static const int MAX_BUSY_WAIT_COUNT = 100000;
    uint32_t lastBusy = 0;
    for (int i = 0; i < MAX_BUSY_WAIT_COUNT; i++)
    {
        if ((RD32(ARM_CM_PWMCTL) & ARM_CM_CTL_BUSY) == 0)
            break;
        microsDelay(1);
        busyCount++;
        lastBusy = RD32(ARM_CM_PWMCTL);
    }
    uint32_t afterKill = 0;
    if (busyCount == MAX_BUSY_WAIT_COUNT)
    {
        WR32(ARM_CM_PWMCTL, ARM_CM_PASSWD | ARM_CM_CTL_KILL | clockSource);
        microsDelay(1);
        afterKill = RD32(ARM_CM_PWMCTL);
    }

    // Set output pins
    // MREQ will use PWM channel 2 and IORQ PWM channel 1
    pinMode(BR_MREQ_WAIT_EN, PINMODE_ALT0);
    pinMode(BR_IORQ_WAIT_EN, PINMODE_ALT0);

    // Clear status on PWM generator
    WR32(ARM_PWM_STA, 0xffffffff);

    // Set the divisor for PWM clock
    uint32_t divisor = ARM_CM_CTL_PLLD_FREQ / freqReqd;
    if (divisor > 4095)
        divisor = 4095;
    WR32(ARM_CM_PWMDIV, ARM_CM_PASSWD | divisor << 12);
    microsDelay(1);

    // Enable PWM clock
    WR32(ARM_CM_PWMCTL, ARM_CM_PASSWD | ARM_CM_CTL_ENAB | clockSource);
    microsDelay(1);

    // Clear wait state enables initially - leaving LOW disables wait state generation
    // Enable PWM two channels (one for IORQ and one for MREQ)
    // FIFO is cleared so pins for IORQ and MREQ enable will be low
    WR32(ARM_PWM_CTL, ARM_PWM_CTL_CLRF1 | ARM_PWM_CTL_USEF2 | ARM_PWM_CTL_MODE2 | ARM_PWM_CTL_PWEN2 |
                                          ARM_PWM_CTL_USEF1 | ARM_PWM_CTL_MODE1 | ARM_PWM_CTL_PWEN1);
    microsDelay(1);

    // Debug
    LogWrite("BusAccess", LOG_DEBUG, "PWM div %d, busyCount %d, lastBusy %08x afterKill %08x", divisor, busyCount, lastBusy, afterKill);
}

void BusAccess::waitResetFlipFlops(bool forceClear)
{
    // Since the FIFO is shared the data output to MREQ/IORQ enable pins will be interleaved so we need to write data for both
    if ((RD32(ARM_PWM_STA) & 1) == 0)
    {
        // Write to FIFO
        uint32_t busVals = RD32(ARM_GPIO_GPLEV0);
        bool ioWaitClear = (forceClear || ((busVals & BR_IORQ_BAR_MASK) == 0)) && _waitOnIO;
        WR32(ARM_PWM_FIF1, ioWaitClear ? 0x00ffffff : 0);  // IORQ sequence
        bool memWaitClear = (forceClear || ((busVals & BR_MREQ_BAR_MASK) == 0)) && _waitOnMemory;
        WR32(ARM_PWM_FIF1, memWaitClear ? 0x00ffffff : 0);  // MREQ sequence
    }

    // Clear flag
    _waitAsserted = false;
}

void BusAccess::waitClearDetected()
{
    // TODO maybe no longer needed as service or timer used instead of edge detection???
    // Clear currently detected edge
    // WR32(ARM_GPIO_GPEDS0, BR_ANY_EDGE_MASK);
}

void BusAccess::waitSuspendBusDetailOneCycle()
{
    if (_hwVersionNumber == 17)
        _waitSuspendBusDetailOneCycle = true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Utility Functions
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Set a pin to be an output and set initial value for that pin
void BusAccess::setPinOut(int pinNumber, bool val)
{
    digitalWrite(pinNumber, val);
    pinMode(pinNumber, OUTPUT);
    digitalWrite(pinNumber, val);
}

void BusAccess::isrAssert(int code)
{
    if (code < ISR_ASSERT_NUM_CODES)
        _isrAssertCounts[code]++;
}

int  BusAccess::isrAssertGetCount(int code)
{
    if (code < ISR_ASSERT_NUM_CODES)
        return _isrAssertCounts[code];
    return 0;
}

void BusAccess::isrValue(int code, int val)
{
    if (code < ISR_ASSERT_NUM_CODES)
        _isrAssertCounts[code] = val;
}

void BusAccess::isrPeak(int code, int val)
{
    if (code < ISR_ASSERT_NUM_CODES)
        if (_isrAssertCounts[code] < val)
            _isrAssertCounts[code] = val;
}

void BusAccess::setSignal(BR_BUS_ACTION busAction, bool assertSignal)
{
    switch (busAction)
    {
        case BR_BUS_ACTION_RESET: 
            assertSignal ? muxSet(BR_MUX_RESET_Z80_BAR_LOW) : muxClear();
            // LogWrite("BA", LOG_DEBUG, "RESET"); 
            // ISR_VALUE(ISR_ASSERT_CODE_DEBUG_E, assertSignal);
            break;
        case BR_BUS_ACTION_NMI: 
            assertSignal ? muxSet(BR_MUX_NMI_BAR_LOW) : muxClear(); 
            // ISR_VALUE(ISR_ASSERT_CODE_DEBUG_F, assertSignal);
            break;
        case BR_BUS_ACTION_IRQ: 
            assertSignal ? muxSet(BR_MUX_IRQ_BAR_LOW) : muxClear(); 
            // LogWrite("BA", LOG_DEBUG, "IRQ"); 
            // ISR_VALUE(ISR_ASSERT_CODE_DEBUG_I, assertSignal);
            break;
        case BR_BUS_ACTION_BUSRQ: 
            // ISR_VALUE(ISR_ASSERT_CODE_DEBUG_B, assertSignal);
            assertSignal ? controlRequest() : controlRelease(); 
            break;
        default: break;
    }
}

void BusAccess::pagingPageIn()
{
    for (int i = 0; i < _busSocketCount; i++)
    {
        if (_busSockets[i].enabled)
            _busSockets[i].busActionCallback(BR_BUS_ACTION_PAGE_IN_FOR_INJECT, BR_BUS_ACTION_GENERAL);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// External API control
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusAccess::rawBusControlEnable(bool en)
{
    busAccessReset();
    _busServiceEnabled = !en;
}

void BusAccess::rawBusControlClearWait()
{
    waitResetFlipFlops();
    // Handle release after a read
    waitHandleReadRelease();
}

void BusAccess::rawBusControlWaitDisable()
{
    waitGenerationDisable();
}

void BusAccess::rawBusControlClockEnable(bool en)
{
    if(!en)
    {
        clockEnable(false);
        pinMode(BR_CLOCK_PIN, OUTPUT);
    }
    else
    {
        clockSetup();
        clockSetFreqHz(1000000);
        clockEnable(true);
    }
}

bool BusAccess::rawBusControlTakeBus()
{
    return (controlRequestAndTake() == BR_OK);
}

void BusAccess::rawBusControlReleaseBus()
{
    controlRelease();
}

void BusAccess::rawBusControlSetAddress(uint32_t addr)
{
    addrSet(addr);
}

void BusAccess::rawBusControlSetData(uint32_t data)
{
    // Set the data onto the PIB
    pibSetOut();
    pibSetValue(data);
    // Perform the write
    // Clear DIR_IN (so make direction out), enable data output onto data bus and MREQ_BAR active
    WR32(ARM_GPIO_GPCLR0, BR_DATA_DIR_IN_MASK | BR_MUX_CTRL_BIT_MASK);
    WR32(ARM_GPIO_GPSET0, BR_MUX_DATA_OE_BAR_LOW << BR_MUX_LOW_BIT_POS);
    if (_hwVersionNumber == 17)
    {
        lowlev_cycleDelay(CYCLES_DELAY_FOR_OUT_FF_SET);
        WR32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
    }
    else
    {
#ifdef V2_PROTO_USING_MUX_EN
        WR32(ARM_GPIO_GPCLR0, BR_MUX_EN_BAR_MASK);
        lowlev_cycleDelay(CYCLES_DELAY_FOR_OUT_FF_SET);
        WR32(ARM_GPIO_GPSET0, BR_MUX_EN_BAR_MASK);
        WR32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);       
#else
        lowlev_cycleDelay(CYCLES_DELAY_FOR_OUT_FF_SET);
        WR32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);       
#endif
    }
}

uint32_t BusAccess::rawBusControlReadRaw()
{
    return RD32(ARM_GPIO_GPLEV0);
}

void BusAccess::rawBusControlSetPin(uint32_t pinNumber, bool level)
{
    digitalWrite(pinNumber, level);
}

bool BusAccess::rawBusControlGetPin(uint32_t pinNumber)
{
    return digitalRead(pinNumber);
}

uint32_t BusAccess::rawBusControlReadPIB()
{
    pibSetIn();
    digitalWrite(BR_DATA_DIR_IN, 1);
    return (RD32(ARM_GPIO_GPLEV0) >> BR_DATA_BUS) & 0xff;
}

void BusAccess::rawBusControlWritePIB(uint32_t val)
{
    // Set the data onto the PIB
    pibSetOut();
    pibSetValue(val);
}

void BusAccess::rawBusControlMuxSet(uint32_t val)
{
    muxSet(val);
}

void BusAccess::rawBusControlMuxClear()
{
    muxClear();
}
