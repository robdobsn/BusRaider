// Bus Raider
// Rob Dobson 2018-2019

#include "BusAccess.h"
#include "PiWiring.h"
#include "lowlev.h"
#include "lowlib.h"
#include <circle/bcm2835.h>
#include "TargetClockGenerator.h"

// Module name
static const char MODULE_PREFIX[] = "BusAccCtrl";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Bus Sockets
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Add a bus socket
int BusAccess::busSocketAdd(bool enabled, BusAccessCBFnType* busAccessCallback,
            BusActionCBFnType* busActionCallback, 
            bool waitOnMemory, bool waitOnIO,
            bool resetPending, uint32_t resetDurationTStates,
            bool nmiPending, uint32_t nmiDurationTStates,
            bool irqPending, uint32_t irqDurationTStates,
            bool busMasterRequest, BR_BUS_ACTION_REASON busMasterReason,
            bool holdInWaitReq, void* pSourceObject)
{
    // Check if all used
    if (_busSocketCount >= MAX_BUS_SOCKETS)
        return -1;

    // Add in available space
    _busSockets[_busSocketCount].set(
            enabled, busAccessCallback, busActionCallback, 
            waitOnMemory, waitOnIO,
            resetPending, resetDurationTStates,
            nmiPending, nmiDurationTStates,
            irqPending, irqDurationTStates,
            busMasterRequest, busMasterReason,
            holdInWaitReq, pSourceObject);
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
    // Set data bus to input and ensure the WR, RD, MREQ, IORQ lines are high
    write32(ARM_GPIO_GPSET0, BR_DATA_DIR_IN_MASK | BR_IORQ_BAR_MASK | BR_MREQ_BAR_MASK | BR_WR_BAR_MASK | BR_RD_BAR_MASK | (_hwVersionNumber == 17 ? BR_V20_M1_BAR_MASK : 0));
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
    waitEnablementUpdate();

    // Check if any bus action is pending
    busActionCheck();

    // Check if we need to assert any new bus requests
    busActionHandleStart();

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
        uint32_t maxUsToWait = 500000; //BusSocketInfo::getUsFromTStates(BR_MAX_WAIT_FOR_BUSACK_T_STATES, clockCurFreqHz());
        if (maxUsToWait <= 0)
            maxUsToWait = 1;
        for (uint32_t j = 0; j < maxUsToWait; j++)
        {
            if (controlBusReqAcknowledged() == ack)
                break;
            microsDelay(1);
        }
    }

    // LogWrite(MODULE_PREFIX, LOG_DEBUG, "waitForBusAck controlBusReqAcknowledged %d ack %d GPIO %d", 
    //         controlBusReqAcknowledged(), ack, read32(ARM_GPIO_GPLEV0));

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
        uint32_t busVals = read32(ARM_GPIO_GPLEV0);

        // Handle slower M1 signal on V1.7 hardware
        if (_hwVersionNumber == 17)
        {
            // Check if we're in a wait - in which case FF OE will be active
            // So we must set the data bus direction outward even if this causes a temporary
            // conflict on the data bus
            write32(ARM_GPIO_GPCLR0, BR_DATA_DIR_IN_MASK);

            // Delay for settling of M1
            lowlev_cycleDelay(CYCLES_DELAY_FOR_M1_SETTLING);

            // Read the control lines
            busVals = read32(ARM_GPIO_GPLEV0);

            // Set data bus driver direction inward - onto PIB
            write32(ARM_GPIO_GPSET0, BR_DATA_DIR_IN_MASK);
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
// Address & Data Bus Functions
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusAccess::addrAndDataBusRead(uint32_t& addr, uint32_t& dataBusVals)
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

    // Or in the high address
    addr = (pibGetValue() & 0xff) << 8;

    // Enable the low address onto the PIB
    muxSet(BR_MUX_LADDR_OE_BAR);

    // Delay to allow data to settle
    lowlev_cycleDelay(CYCLES_DELAY_FOR_READ_FROM_PIB);

    // Get low address value
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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Read and Write Host Memory
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Write a single byte to currently set address (or IO port)
// Assumes:
// - control of host bus has been requested and acknowledged
// - address bus is already set and output enabled to host bus
// - PIB is already set to output
void BusAccess::byteWrite(uint32_t data, BlockAccessType accessType)
{
    // Set the data onto the PIB
    pibSetValue(data);
    // Perform the write
    // Clear DIR_IN (so make direction out), enable data output onto data bus and MREQ_BAR active
    write32(ARM_GPIO_GPCLR0, BR_DATA_DIR_IN_MASK | BR_MUX_CTRL_BIT_MASK | ((accessType == ACCESS_IO) ? BR_IORQ_BAR_MASK : BR_MREQ_BAR_MASK));
    write32(ARM_GPIO_GPSET0, BR_MUX_DATA_OE_BAR_LOW << BR_MUX_LOW_BIT_POS);
    // Write the data by setting WR_BAR active
    if (_hwVersionNumber == 17)
    {
        write32(ARM_GPIO_GPCLR0, BR_WR_BAR_MASK);
    }
    else
    {
        write32(ARM_GPIO_GPCLR0, BR_WR_BAR_MASK | BR_MUX_EN_BAR_MASK);
    }
    // Target write delay
    lowlev_cycleDelay(CYCLES_DELAY_FOR_WRITE_TO_TARGET);
    // Deactivate and leave data direction set to inwards
    if (_hwVersionNumber == 17)
    {
        write32(ARM_GPIO_GPSET0, BR_DATA_DIR_IN_MASK | ((accessType == ACCESS_IO) ? BR_IORQ_BAR_MASK : BR_MREQ_BAR_MASK) | BR_WR_BAR_MASK);
        muxClear();
    }
    else
    {
        write32(ARM_GPIO_GPSET0, BR_DATA_DIR_IN_MASK | BR_MUX_EN_BAR_MASK | ((accessType == ACCESS_IO) ? BR_IORQ_BAR_MASK : BR_MREQ_BAR_MASK) | BR_WR_BAR_MASK);
    }
}

// Read a single byte from currently set address (or IO port)
// Assumes:
// - control of host bus has been requested and acknowledged
// - address bus is already set and output enabled to host bus
// - PIB is already set to input
uint8_t BusAccess::byteRead(BlockAccessType accessType)
{
    // Enable data output onto PIB, MREQ_BAR and RD_BAR both active
    write32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK | ((accessType == ACCESS_IO) ? BR_IORQ_BAR_MASK : BR_MREQ_BAR_MASK) | BR_RD_BAR_MASK);
    write32(ARM_GPIO_GPSET0, BR_DATA_DIR_IN_MASK | (BR_MUX_DATA_OE_BAR_LOW << BR_MUX_LOW_BIT_POS));
    if (_hwVersionNumber != 17)
    {
        write32(ARM_GPIO_GPCLR0, BR_MUX_EN_BAR_MASK);
    }
    // Delay to allow data to settle
    lowlev_cycleDelay(CYCLES_DELAY_FOR_READ_FROM_PIB);
    // Get the data
    uint8_t val = pibGetValue();
    // Deactivate leaving data-dir inwards
    if (_hwVersionNumber == 17)
    {
        write32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
        write32(ARM_GPIO_GPSET0, ((accessType == ACCESS_IO) ? BR_IORQ_BAR_MASK : BR_MREQ_BAR_MASK) | BR_RD_BAR_MASK);
    }
    else
    {
        write32(ARM_GPIO_GPSET0, BR_MUX_EN_BAR_MASK | ((accessType == ACCESS_IO) ? BR_IORQ_BAR_MASK : BR_MREQ_BAR_MASK) | BR_RD_BAR_MASK);
    }
    return val;
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
            // LogWrite(MODULE_PREFIX, LOG_DEBUG, "RESET"); 
            // ISR_VALUE(ISR_ASSERT_CODE_DEBUG_E, assertSignal);
            break;
        case BR_BUS_ACTION_NMI: 
            assertSignal ? muxSet(BR_MUX_NMI_BAR_LOW) : muxClear(); 
            // ISR_VALUE(ISR_ASSERT_CODE_DEBUG_F, assertSignal);
            break;
        case BR_BUS_ACTION_IRQ: 
            assertSignal ? muxSet(BR_MUX_IRQ_BAR_LOW) : muxClear(); 
            // LogWrite(MODULE_PREFIX, LOG_DEBUG, "IRQ"); 
            // ISR_VALUE(ISR_ASSERT_CODE_DEBUG_I, assertSignal);
            break;
        case BR_BUS_ACTION_BUSRQ: 
            // ISR_VALUE(ISR_ASSERT_CODE_DEBUG_B, assertSignal);
            assertSignal ? controlRequest() : controlRelease(); 
            break;
        default: break;
    }
}

void BusAccess::busAccessCallbackPageIn()
{
    // LogWrite(MODULE_PREFIX, LOG_DEBUG, "targetPageForInjection FALSE");

    for (int i = 0; i < _busSocketCount; i++)
    {
        if (_busSockets[i].enabled)
            _busSockets[i].busActionCallback(_busSockets[i].pSourceObject,
                         BR_BUS_ACTION_PAGE_IN_FOR_INJECT, BR_BUS_ACTION_GENERAL);
    }
}

// Paging pin on the bus
void BusAccess::busPagePinSetActive(bool active)
{
    // LogWrite(MODULE_PREFIX, LOG_DEBUG, "pagePin = %d", (_hwVersionNumber == 17) ? active : !active);
    digitalWrite(BR_PAGING_RAM_PIN, (_hwVersionNumber == 17) ? active : !active);
}

void BusAccess::formatCtrlBus(uint32_t ctrlBus, char* msgBuf, int maxMsgBufLen)
{
    // Check valid
    if (maxMsgBufLen < 20)
        return;
    // Format
    snprintf(msgBuf, maxMsgBufLen, "%c%c%c%c%c", 
                (ctrlBus & BR_CTRL_BUS_MREQ_MASK) ? 'M' : '.',
                (ctrlBus & BR_CTRL_BUS_IORQ_MASK) ? 'I' : '.',
                (ctrlBus & BR_CTRL_BUS_RD_MASK) ? 'R' : '.',
                (ctrlBus & BR_CTRL_BUS_WR_MASK) ? 'W' : '.',
                (ctrlBus & BR_CTRL_BUS_M1_MASK) ? '1' : '.');
}

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // Auto bus test - assumes ONLY the BusRaider is on the bus
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// BR_RETURN_TYPE BusAccess::testBusAuto()
// {
//     // Check that BUSACK is low
//     bool busrqAckBar = digitalRead(BR_BUSACK_BAR);
//     if (busrqAckBar != 0)
//         return BR_NO_BUS_ACK;

//     // Set control lines
//     digitalWrite(BR_WR_BAR, 1);
//     digitalWrite(BR_RD_BAR, 1);
//     digitalWrite(BR_MREQ_BAR, 1);
//     digitalWrite(BR_IORQ_BAR, 1);
//     digitalWrite(BR_WAIT_BAR_PIN, 1);
//     digitalWrite(BR_BUSRQ_BAR, 1);
//     digitalWrite(BR_PAGING_RAM_PIN, 1);
//     muxClear();

//     // Step through patterns to test


// }
