// Bus Raider
// Rob Dobson 2018-2019

#include "BusAccess.h"
#include "../System/piwiring.h"
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
    WR32(ARM_GPIO_GPSET0, 1 << BR_DATA_DIR_IN);
    // Request the bus
    digitalWrite(BR_BUSRQ_BAR, 0);
}

// Check if bus request has been acknowledged
int BusAccess::controlBusAcknowledged()
{
    return (RD32(ARM_GPIO_GPLEV0) & BR_BUSACK_BAR_MASK) == 0;
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

    // Address bus enabled
    digitalWrite(BR_PUSH_ADDR_BAR, 0);
}

// Release control of bus
void BusAccess::controlRelease()
{
    // Prime flip-flop that skips refresh cycles
    // So that the very first MREQ cycle after a BUSRQ/BUSACK causes a WAIT to be generated
    // (if enabled)

    // Set M1 high (inactive) - M1 is on bit 0 of PIB with resistor
    pibSetOut();
    pibSetValue(1 << BR_M1_PIB_DATA_LINE);

    // Set data direction out so we can set the M1 value
    WR32(ARM_GPIO_GPCLR0, 1 << BR_DATA_DIR_IN);

    // Pulse MREQ to prime the FF
    lowlev_cycleDelay(CYCLES_DELAY_FOR_MREQ_FF_RESET);
    digitalWrite(BR_MREQ_BAR, 0);
    lowlev_cycleDelay(CYCLES_DELAY_FOR_MREQ_FF_RESET);
    digitalWrite(BR_MREQ_BAR, 1);

    // Go back to data inwards
    WR32(ARM_GPIO_GPSET0, 1 << BR_DATA_DIR_IN);
    pibSetIn();

    // Clear the mux to deactivate output enables
    muxClear();

    // Address bus not enabled
    digitalWrite(BR_PUSH_ADDR_BAR, 1);

    // Clear wait detected in case we created some MREQ cycles that
    // triggered wait
    uint32_t busVals = RD32(ARM_GPIO_GPLEV0);
    if ((busVals & BR_WAIT_BAR_MASK) == 0)
        waitResetFlipFlops();
    waitClearDetected();

    // Re-establish wait generation
    // LogWrite("BusAccess", LOG_DEBUG, "controlRelease");
    waitEnablementUpdate();

    // Check if any bus action is pending
    busActionCheck();

    // Check if we need to assert any new bus requests
    busActionHandleStart();

    // No longer request bus
    digitalWrite(BR_BUSRQ_BAR, 1);

    // Control bus release
    pinMode(BR_WR_BAR, INPUT);
    pinMode(BR_RD_BAR, INPUT);
    pinMode(BR_MREQ_BAR, INPUT);
    pinMode(BR_IORQ_BAR, INPUT);

    // Bus no longer under BusRaider control
    _busIsUnderControl = false;    
}

// Request bus, wait until available and take control
BR_RETURN_TYPE BusAccess::controlRequestAndTake()
{
    // Request
    controlRequest();

    // Check for ack
    for (int i = 0; i < BusSocketInfo::MAX_WAIT_FOR_BUSACK_US; i++)
    {
        if (controlBusAcknowledged())
            break;
        microsDelay(1);
    }
    
    // Check we really have the bus
    if (!controlBusAcknowledged()) 
    {
        controlRelease();
        return BR_NO_BUS_ACK;
    }

    // Take control
    controlTake();
    return BR_OK;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Address Bus Functions
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Set low address value by clearing and counting
// Assumptions:
// - some other code will set push-addr to enable onto the address bus
void BusAccess::addrLowSet(uint32_t lowAddrByte)
{
    // Clear initially
    lowlev_cycleDelay(CYCLES_DELAY_FOR_CLEAR_LOW_ADDR);
    muxSet(BR_MUX_LADDR_CLR_BAR_LOW);
    // Delay a few cycles
    lowlev_cycleDelay(CYCLES_DELAY_FOR_CLEAR_LOW_ADDR);
    muxClear();
    // Clock the required value in - requires one more count than
    // expected as the output register is one clock pulse behind the counter
    for (uint32_t i = 0; i < (lowAddrByte & 0xff) + 1; i++) {
        WR32(ARM_GPIO_GPSET0, 1 << BR_LADDR_CK);
        lowlev_cycleDelay(CYCLES_DELAY_FOR_LOW_ADDR_SET);
        WR32(ARM_GPIO_GPCLR0, 1 << BR_LADDR_CK);
        lowlev_cycleDelay(CYCLES_DELAY_FOR_LOW_ADDR_SET);
    }
}

// Increment low address value by clocking the counter
// Assumptions:
// - some other code will set push-addr to enable onto the address bus
void BusAccess::addrLowInc()
{
    WR32(ARM_GPIO_GPSET0, 1 << BR_LADDR_CK);
    lowlev_cycleDelay(CYCLES_DELAY_FOR_LOW_ADDR_SET);
    WR32(ARM_GPIO_GPCLR0, 1 << BR_LADDR_CK);
    lowlev_cycleDelay(CYCLES_DELAY_FOR_LOW_ADDR_SET);
}

// Set the high address value
// Assumptions:
// - some other code will set push-addr to enable onto the address bus
void BusAccess::addrHighSet(uint32_t highAddrByte)
{
    // Shift the value into the register
    // Takes one more shift than expected as output reg is one pulse behind shift
    for (uint32_t i = 0; i < 9; i++) {
        // Set or clear serial pin to shift register
        if (highAddrByte & 0x80)
            muxSet(BR_MUX_HADDR_SER_HIGH);
        else
            muxSet(BR_MUX_HADDR_SER_LOW);
        // Delay to allow settling
        lowlev_cycleDelay(CYCLES_DELAY_FOR_HIGH_ADDR_SET);
        // Shift the address value for next bit
        highAddrByte = highAddrByte << 1;
        // Clock the bit
        WR32(ARM_GPIO_GPSET0, 1 << BR_HADDR_CK);
        lowlev_cycleDelay(CYCLES_DELAY_FOR_HIGH_ADDR_SET);
        WR32(ARM_GPIO_GPCLR0, 1 << BR_HADDR_CK);
    }
    // Clear multiplexer
    lowlev_cycleDelay(CYCLES_DELAY_FOR_HIGH_ADDR_SET);
    muxClear();
}

// Set the full address
// Assumptions:
// - some other code will set push-addr to enable onto the address bus
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
void BusAccess::byteWrite(uint32_t byte, int iorq)
{
    // Set the data onto the PIB
    pibSetValue(byte);
    // Perform the write
    // Clear DIR_IN (so make direction out), enable data output onto data bus and MREQ_BAR active
    WR32(ARM_GPIO_GPCLR0, (1 << BR_DATA_DIR_IN) | BR_MUX_CTRL_BIT_MASK | (1 << (iorq ? BR_IORQ_BAR : BR_MREQ_BAR)));
    WR32(ARM_GPIO_GPSET0, BR_MUX_DATA_OE_BAR_LOW << BR_MUX_LOW_BIT_POS);
    // Write the data by setting WR_BAR active
    WR32(ARM_GPIO_GPCLR0, (1 << BR_WR_BAR));
    // Target write delay
    lowlev_cycleDelay(CYCLES_DELAY_FOR_WRITE_TO_TARGET);
    // Deactivate and leave data direction set to inwards
    WR32(ARM_GPIO_GPSET0, (1 << BR_DATA_DIR_IN) | (1 << (iorq ? BR_IORQ_BAR : BR_MREQ_BAR)) | (1 << BR_WR_BAR));
    muxClear();
}

// Read a single byte from currently set address (or IO port)
// Assumes:
// - control of host bus has been requested and acknowledged
// - address bus is already set and output enabled to host bus
// - PIB is already set to input
// - data direction on data bus driver is set to input (default)
uint8_t BusAccess::byteRead(int iorq)
{
    // Enable data output onto PIB (data-dir must be inwards already), MREQ_BAR and RD_BAR both active
    WR32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK | (1 << (iorq ? BR_IORQ_BAR : BR_MREQ_BAR)) | (1 << BR_RD_BAR));
    WR32(ARM_GPIO_GPSET0, BR_MUX_DATA_OE_BAR_LOW << BR_MUX_LOW_BIT_POS);
    // Delay to allow data to settle
    lowlev_cycleDelay(CYCLES_DELAY_FOR_READ_FROM_PIB);
    // Get the data
    uint8_t val = (RD32(ARM_GPIO_GPLEV0) >> BR_DATA_BUS) & 0xff;
    // Deactivate leaving data-dir inwards
    WR32(ARM_GPIO_GPSET0, (1 << (iorq ? BR_IORQ_BAR : BR_MREQ_BAR)) | (1 << BR_RD_BAR));
    // Clear the MUX to a safe setting - sets HADDR_SER low
    WR32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
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
    for (uint32_t i = 0; i < len; i++) {
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
// - data direction on data bus driver is set to input (default)
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

    // Set the address to initial value
    addrSet(addr);

    // Enable data onto to PIB
    WR32(ARM_GPIO_GPSET0, BR_MUX_DATA_OE_BAR_LOW << BR_MUX_LOW_BIT_POS);

    // Calculate bit patterns outside loop
    uint32_t reqLinePlusReadPlusCk = (1 << (iorq ? BR_IORQ_BAR : BR_MREQ_BAR)) | (1 << BR_RD_BAR) | (1 << BR_LADDR_CK);

    // Iterate data
    for (uint32_t i = 0; i < len; i++) {

        // IORQ_BAR / MREQ_BAR and RD_BAR both active
        WR32(ARM_GPIO_GPCLR0, reqLinePlusReadPlusCk);
        
        // Delay to allow data bus to settle
        lowlev_cycleDelay(CYCLES_DELAY_FOR_READ_FROM_PIB);
        
        // Get the data
        *pData = (RD32(ARM_GPIO_GPLEV0) >> BR_DATA_BUS) & 0xff;

        // Deactivate IORQ/MREQ and RD and clock the low address
        WR32(ARM_GPIO_GPSET0, reqLinePlusReadPlusCk);

        // Increment addresses
        pData++;
        addr++;

        // Check if we've rolled over the lowest 8 bits
        if ((addr & 0xff) == 0) {

            // Clock low again
            WR32(ARM_GPIO_GPCLR0, 1 << BR_LADDR_CK);

           // Set the address again
            addrSet(addr);
        }
    }

    // Clock low again
    WR32(ARM_GPIO_GPCLR0, 1 << BR_LADDR_CK);

    // Data no longer enabled onto PIB
    muxClear();

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

void BusAccess::waitResetFlipFlops()
{
    // Since the FIFO is shared the data output to MREQ/IORQ enable pins will be interleaved so we need to write data for both
    if ((RD32(ARM_PWM_STA) & 1) == 0)
    {
        // Write to FIFO
        WR32(ARM_PWM_FIF1, 0x00ffffff);
        WR32(ARM_PWM_FIF1, 0x00ffffff);
    }

    // Clear flag
    _waitAsserted = false;

    // Handle release after a read
    if (_targetReadInProgress)
        waitHandleReadRelease();
}

void BusAccess::waitClearDetected()
{
    // TODO maybe no longer needed as service or timer used instead of edge detection???
    // Clear currently detected edge
    // WR32(ARM_GPIO_GPEDS0, BR_ANY_EDGE_MASK);
}

void BusAccess::waitSuspendBusDetailOneCycle()
{
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
        case BR_BUS_ACTION_RESET: assertSignal ? muxSet(BR_MUX_RESET_Z80_BAR_LOW) : muxClear();
                    // LogWrite("BA", LOG_DEBUG, "RESET"); 
                    break;
        case BR_BUS_ACTION_NMI: assertSignal ? muxSet(BR_MUX_NMI_BAR_LOW) : muxClear(); break;
        case BR_BUS_ACTION_IRQ: assertSignal ? muxSet(BR_MUX_IRQ_BAR_LOW) : muxClear(); 
                    // LogWrite("BA", LOG_DEBUG, "IRQ"); 
                    break;
        case BR_BUS_ACTION_BUSRQ: assertSignal ? digitalWrite(BR_BUSRQ_BAR, 0) : digitalWrite(BR_BUSRQ_BAR, 1); break;
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
