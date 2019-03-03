// Bus Raider
// Rob Dobson 2018

#include "BusAccess.h"
#include "../System/piwiring.h"
#include "../System/CInterrupts.h"
#include "../System/ee_printf.h"
#include "../System/lowlev.h"
#include "../System/lowlib.h"
#include "../System/BCM2835.h"

// Uncomment the following line to use SPI0 CE0 of the Pi as a debug pin
// #define USE_PI_SPI0_CE0_AS_DEBUG_PIN 1

// Comment out this line to disable WAIT state generation altogether
#define BR_ENABLE_WAIT_AND_FIQ 1

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Variables
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Callback on bus access
BusAccessCBFnType* BusAccess::_pBusAccessCallback = NULL;

// Current wait state mask for restoring wait enablement after change
uint32_t BusAccess::_waitStateEnMask = 0;
uint32_t BusAccess::_waitStateEnMaskDefault = 0;

// Wait interrupt enablement cache (so it can be restored after disable)
bool BusAccess::_waitIntEnabled = false;

// Currently paused - i.e. wait is active
volatile bool BusAccess::_pauseIsPaused = false;

// Bus under BusRaider control
volatile bool BusAccess::_busIsUnderControl = false;

// Request bus when single stepping
bool BusAccess::_requestBusWhenSingleStepping = false;

// // Bus values while single stepping
uint32_t BusAccess::_pauseCurAddr = 0;
uint32_t BusAccess::_pauseCurData = 0;
uint32_t BusAccess::_pauseCurControlBus = 0;

// Debug
int BusAccess::_isrAssertCounts[ISR_ASSERT_NUM_CODES];

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Initialisation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Initialise the bus raider
void BusAccess::init()
{
    // Disable FIQ - used for wait state handling
    waitIntDisable();
    
    // Pins that are not setup here as outputs will be inputs
    // This includes the "bus" used for address-low/high and data (referred to as the PIB)

    // Setup the multiplexer
    setPinOut(BR_MUX_0, 0);
    setPinOut(BR_MUX_1, 0);
    setPinOut(BR_MUX_2, 0);
    
    // Bus Request
    setPinOut(BR_BUSRQ_BAR, 1);
    
    // Clear wait states initially - leaving LOW disables wait state generation
    setPinOut(BR_MREQ_WAIT_EN, 0);
    setPinOut(BR_IORQ_WAIT_EN, 0);
    _waitStateEnMask = 0;
    _waitStateEnMaskDefault = 0;
    
    // Address push
    setPinOut(BR_PUSH_ADDR_BAR, 1);
    
    // High address clock
    setPinOut(BR_HADDR_CK, 0);
    
    // Low address clock
    setPinOut(BR_LADDR_CK, 0);
    
    // Data bus direction
    setPinOut(BR_DATA_DIR_IN, 1);

    // Paging
    setPinOut(BR_PAGING_RAM_PIN, 0);
    
    // Remember wait interrupts currently disabled
    _waitIntEnabled = false;
    
    // Debug
#ifdef USE_PI_SPI0_CE0_AS_DEBUG_PIN
    setPinOut(BR_DEBUG_PI_SPI0_CE0, 0);
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Host Actions (Reset/NMI/IRQ)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Reset the host
void BusAccess::targetReset()
{
    // Disable wait interrupts
    waitIntDisable();

    // Restore wait settings
    waitRestoreDefaults();

    // Reset by taking reset_bar low and then high
    muxSet(BR_MUX_RESET_Z80_BAR_LOW);

    // Hold the reset line low
    microsDelay(100);

    // Clear WAIT flip-flop
    clearWaitFF();

    // Clear wait interrupt conditions and enable
    waitIntClear();
    if (_waitIntEnabled)
        waitIntEnable();

    // Remove the reset condition
    muxClear();

    // Clear wait detected
    clearWaitDetected();
}

// Hold host in reset state - call targetReset() to clear reset
void BusAccess::targetResetHold()
{
    // Disable wait interrupts
    waitIntDisable();

    // Reset by taking reset_bar low
    muxSet(BR_MUX_RESET_Z80_BAR_LOW);
}

// Non-maskable interrupt the host
void BusAccess::targetNMI(int durationUs)
{
    // NMI by taking nmi_bar line low and high
    muxSet(BR_MUX_NMI_BAR_LOW);
    microsDelay((durationUs <= 0) ? 10 : durationUs);
    muxClear();
}

// Maskable interrupt the host
void BusAccess::targetIRQ(int durationUs)
{
    // IRQ by taking irq_bar line low and high
    muxSet(BR_MUX_IRQ_BAR_LOW);
    microsDelay(((durationUs <= 0) || (durationUs > 1000)) ? 50 : durationUs);
    muxClear();
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
    return (RD32(ARM_GPIO_GPLEV0) & (1 << BR_BUSACK_BAR)) == 0;
}

// Take control of bus
void BusAccess::controlTake()
{
    // Disable interrupts
    waitIntDisable();

    // Bus is under BusRaider control
    _busIsUnderControl = true;

    // Control bus
    setPinOut(BR_WR_BAR, 1);
    setPinOut(BR_RD_BAR, 1);
    setPinOut(BR_MREQ_BAR, 1);
    setPinOut(BR_IORQ_BAR, 1);

    // Address bus enabled
    digitalWrite(BR_PUSH_ADDR_BAR, 0);
}

// Release control of bus
void BusAccess::controlRelease(bool resetTargetOnRelease)
{
    // #ifdef USE_PI_SPI0_CE0_AS_DEBUG_PIN
    //     digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
    // #endif

    // Prime flip-flop that skips refresh cycles
    // So that the very first MREQ cycle after a BUSRQ/BUSACK causes a WAIT to be generated
    // (if enabled)
    pibSetOut();
    pibSetValue(1 << BR_M1_PIB_DATA_LINE);

    // Set data direction out so we can set the M1 value
    WR32(ARM_GPIO_GPCLR0, 1 << BR_DATA_DIR_IN);

//TODO optimize

    // Pulse MREQ to prime the FF
    microsDelay(2);
    // lowlev_cycleDelay(CYCLES_DELAY_FOR_MREQ_FF_RESET);
    digitalWrite(BR_MREQ_BAR, 0);
    microsDelay(2);
    // lowlev_cycleDelay(CYCLES_DELAY_FOR_MREQ_FF_RESET);
    digitalWrite(BR_MREQ_BAR, 1);

    // Go back to data inwards
    WR32(ARM_GPIO_GPSET0, 1 << BR_DATA_DIR_IN);
    pibSetIn();
    microsDelay(2);
    // lowlev_cycleDelay(CYCLES_DELAY_FOR_DATA_DIRN);

    // Clear the mux to deactivate output enables
    muxClear();

    // Clear wait detected
    clearWaitFF();
    clearWaitDetected();

    // Control bus
    pinMode(BR_WR_BAR, INPUT);
    pinMode(BR_RD_BAR, INPUT);
    pinMode(BR_MREQ_BAR, INPUT);
    pinMode(BR_IORQ_BAR, INPUT);

    // Address bus not enabled
    digitalWrite(BR_PUSH_ADDR_BAR, 1);
    
    // Re-enable interrupts if required
    if (_waitIntEnabled)
        waitIntEnable();
    
    // Check for reset
    if (resetTargetOnRelease)
    {
        // Reset by taking reset_bar low and then high
        muxSet(BR_MUX_RESET_Z80_BAR_LOW);

        // No longer request bus
        digitalWrite(BR_BUSRQ_BAR, 1);
        microsDelay(10);
        muxClear();
    }
    else
    {
        // No longer request bus
        digitalWrite(BR_BUSRQ_BAR, 1);
    }

    // Bus no longer under BusRaider control
    _busIsUnderControl = false;        


    // #ifdef USE_PI_SPI0_CE0_AS_DEBUG_PIN
    //     digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
    // #endif
}

// Request bus, wait until available and take control
BR_RETURN_TYPE BusAccess::controlRequestAndTake()
{
    // Request
    controlRequest();

    // Check for ack
    for (int i = 0; i < MAX_WAIT_FOR_ACK_US; i++)
    {
        if (controlBusAcknowledged())
            break;
        microsDelay(1);
    }
    if (!controlBusAcknowledged()) 
    {
        controlRelease(false);
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
#ifdef CYCLES_DELAY_FOR_WRITE_TO_TARGET
    // Target write delay
    lowlev_cycleDelay(CYCLES_DELAY_FOR_WRITE_TO_TARGET);
#endif
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
BR_RETURN_TYPE BusAccess::blockWrite(uint32_t addr, const uint8_t* pData, uint32_t len, int busRqAndRelease, int iorq)
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
        controlRelease(false);
    }
    return BR_OK;
}

// Read a consecutive block of memory from host
// Assumes:
// - control of host bus has been requested and acknowledged
// - data direction on data bus driver is set to input (default)
BR_RETURN_TYPE BusAccess::blockRead(uint32_t addr, uint8_t* pData, uint32_t len, int busRqAndRelease, int iorq)
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

    // Iterate data
    for (uint32_t i = 0; i < len; i++) {

        // IORQ_BAR / MREQ_BAR and RD_BAR both active
        WR32(ARM_GPIO_GPCLR0, (1 << (iorq ? BR_IORQ_BAR : BR_MREQ_BAR)) | (1 << BR_RD_BAR));
        
        // Delay to allow data bus to settle
        lowlev_cycleDelay(CYCLES_DELAY_FOR_READ_FROM_PIB);
        
        // Get the data
        *pData = (RD32(ARM_GPIO_GPLEV0) >> BR_DATA_BUS) & 0xff;

        // Deactivate IORQ/MREQ and RD and clock the low address
        WR32(ARM_GPIO_GPSET0, (1 << (iorq ? BR_IORQ_BAR : BR_MREQ_BAR)) | (1 << BR_RD_BAR) | (1 << BR_LADDR_CK));

        // Low address clock pulse period
        lowlev_cycleDelay(CYCLES_DELAY_FOR_LOW_ADDR_SET);

        // Clock pulse high again
        WR32(ARM_GPIO_GPCLR0, 1 << BR_LADDR_CK);

        // Increment addresses
        pData++;
        addr++;

        // Check if we've rolled over the lowest 8 bits
        if ((addr & 0xff) == 0) {
            // Set the address again
            addrSet(addr);
        }
    }

    // Data no longer enabled onto PIB
    muxClear();

    // Check if we need to release bus
    if (busRqAndRelease) {
        // release bus
        controlRelease(false);
    }
    return BR_OK;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Utility Functions
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Clear all IO space
void BusAccess::clearAllIO()
{
    // Fill IO "memory" with 0xff
    uint8_t tmpBuf[0x100];
    for (int kk = 0; kk < 0x100; kk++)
        tmpBuf[kk] = 0xff;
    blockWrite(0, tmpBuf, 0x100, 1, 1);  
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Callbacks on Bus Access
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Set the bus access interrupt callback
void BusAccess::accessCallbackAdd(BusAccessCBFnType* pBusAccessCallback)
{
    _pBusAccessCallback = pBusAccessCallback;
}

void BusAccess::accessCallbackRemove()
{
    _pBusAccessCallback = NULL;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Wait State Enable
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusAccess::waitGet(bool& monitorIORQ, bool& monitorMREQ)
{
    monitorIORQ = (_waitStateEnMask & BR_IORQ_WAIT_EN_MASK) != 0;
    monitorMREQ = (_waitStateEnMask & BR_MREQ_WAIT_EN_MASK) != 0;
}

void BusAccess::waitOnInstruction(bool waitOnInstruction)
{
    // Setup wait on instruction
    if (waitOnInstruction)
    {
        WR32(ARM_GPIO_GPSET0, BR_MREQ_WAIT_EN_MASK);
        _waitStateEnMask = _waitStateEnMask | BR_MREQ_WAIT_EN_MASK;
        WR32(ARM_GPIO_GPFEN0, RD32(ARM_GPIO_GPFEN0) | (1 << BR_WAIT_BAR));
    }
    else
    {
        WR32(ARM_GPIO_GPCLR0, BR_MREQ_WAIT_EN_MASK);
        _waitStateEnMask = _waitStateEnMask & (~BR_MREQ_WAIT_EN_MASK);
        WR32(ARM_GPIO_GPFEN0, RD32(ARM_GPIO_GPFEN0) & (~(1 << BR_WAIT_BAR)));
    }
}

// Restore wait state generation to default
void BusAccess::waitRestoreDefaults()
{
    _waitStateEnMask = _waitStateEnMaskDefault;
    WR32(ARM_GPIO_GPCLR0, BR_MREQ_WAIT_EN_MASK | BR_IORQ_WAIT_EN_MASK);
    WR32(ARM_GPIO_GPSET0, _waitStateEnMask);
    if ((_waitStateEnMask & BR_MREQ_WAIT_EN_MASK) != 0)
        WR32(ARM_GPIO_GPFEN0, RD32(ARM_GPIO_GPFEN0) | (1 << BR_WAIT_BAR));
    else
        WR32(ARM_GPIO_GPFEN0, RD32(ARM_GPIO_GPFEN0) & (~(1 << BR_WAIT_BAR)));
}

// Setup wait-states and FIQ
void BusAccess::waitSetup(bool enWaitOnIORQ, bool enWaitOnMREQ)
{
    // Disable interrupts
    waitIntDisable();

    // Enable wait state generation
    _waitStateEnMask = 0;
    if (enWaitOnIORQ)
        _waitStateEnMask = _waitStateEnMask | BR_IORQ_WAIT_EN_MASK;
    if (enWaitOnMREQ)
        _waitStateEnMask = _waitStateEnMask | BR_MREQ_WAIT_EN_MASK;
    _waitStateEnMaskDefault = _waitStateEnMask;
    WR32(ARM_GPIO_GPCLR0, BR_MREQ_WAIT_EN_MASK | BR_IORQ_WAIT_EN_MASK);

#ifdef BR_ENABLE_WAIT_AND_FIQ

    // Set wait-state generation
    WR32(ARM_GPIO_GPSET0, _waitStateEnMask);

    // Set vector for WAIT state interrupt
    CInterrupts::connectFIQ(0, waitStateISR, 0);

    // Setup edge triggering on falling edge of IORQ and/or MREQ
    // Clear any current detected edges
    WR32(ARM_GPIO_GPEDS0, (1 << BR_IORQ_BAR) | (1 << BR_MREQ_BAR));
    
    // Set falling edge detect
    if (enWaitOnIORQ)
        WR32(ARM_GPIO_GPFEN0, RD32(ARM_GPIO_GPFEN0) | (1 << BR_IORQ_BAR));
    else
        WR32(ARM_GPIO_GPFEN0, RD32(ARM_GPIO_GPFEN0) & (~(1 << BR_IORQ_BAR)));
    if (enWaitOnMREQ)
        WR32(ARM_GPIO_GPFEN0, RD32(ARM_GPIO_GPFEN0) | (1 << BR_WAIT_BAR));
    else
        WR32(ARM_GPIO_GPFEN0, RD32(ARM_GPIO_GPFEN0) & (~(1 << BR_WAIT_BAR)));

    // Enable FIQ interrupts on GPIO[3] which is any GPIO pin
    WR32(ARM_IC_FIQ_CONTROL, (1 << 7) | 52);

    // Enable Fast Interrupts
    _waitIntEnabled = true;
    waitIntEnable();

#endif  
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Wait State Interrupt Service Routine
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Main Interrupt Service Routing for MREQ/IORQ based interrupts
// The Bus Raider hardware creates WAIT states while this IRQ is running
// These have to be cleared on exit and the interrupt source register also cleared
void BusAccess::waitStateISR(void* pData)
{
#ifdef USE_PI_SPI0_CE0_AS_DEBUG_PIN
        digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
#endif

    // pData unused
    pData = pData;

    // Set PIB to input
    pibSetIn();

    // Set data direction out so we can read the M1 value
    WR32(ARM_GPIO_GPCLR0, 1 << BR_DATA_DIR_IN);

    // Clear the mux to deactivate output enables
    muxClear();

    // Delay to allow M1 to settle
    //TODO
    lowlev_cycleDelay(CYCLES_DELAY_FOR_M1_SETTLING);

    // Loop until control lines are valid
    // In a write cycle WR is asserted after MREQ so we need to wait until WR changes before
    // we can determine what kind of operation is in progress 
    bool ctrlValid = false;
    int avoidLockupCtr = 0;
    uint32_t busVals = 0;
    while(avoidLockupCtr < MAX_WAIT_FOR_CTRL_LINES_COUNT)
    {
        // Read the control lines
        busVals = RD32(ARM_GPIO_GPLEV0);

        // Specifically need to wait if MREQ is low but both RD and WR are high (wait for WR to go low in this case)
        ctrlValid = !(((busVals & (1 << BR_MREQ_BAR)) == 0) && (((busVals & (1 << BR_RD_BAR)) != 0) && ((busVals & (1 << BR_WR_BAR)) != 0)));
        if (ctrlValid)
            break;

        // Count loops
        avoidLockupCtr++;
    }

    // Enable the low address onto the PIB
    muxSet(BR_MUX_LADDR_OE_BAR);

    // Delay to allow data to settle
    lowlev_cycleDelay(CYCLES_DELAY_FOR_READ_FROM_PIB);

    // Get low address value
    uint32_t addr = pibGetValue() & 0xff;

    // Read the high address
    muxSet(BR_MUX_HADDR_OE_BAR);

    // Delay to allow data to settle
    lowlev_cycleDelay(CYCLES_DELAY_FOR_READ_FROM_PIB);

    // Or in the high address
    addr |= (pibGetValue() & 0xff) << 8;
 
    // Clear the mux to deactivate output enables
    muxClear();

    // Delay to allow data to settle
    lowlev_cycleDelay(CYCLES_DELAY_FOR_READ_FROM_PIB);

    // Get the appropriate bits for up-line communication
    uint32_t ctrlBusVals = 
        (((busVals & (1 << BR_RD_BAR)) == 0) ? BR_CTRL_BUS_RD_MASK : 0) |
        (((busVals & (1 << BR_WR_BAR)) == 0) ? BR_CTRL_BUS_WR_MASK : 0) |
        (((busVals & (1 << BR_MREQ_BAR)) == 0) ? BR_CTRL_BUS_MREQ_MASK : 0) |
        (((busVals & (1 << BR_IORQ_BAR)) == 0) ? BR_CTRL_BUS_IORQ_MASK : 0) |
        (((busVals & (1 << BR_WAIT_BAR)) == 0) ? BR_CTRL_BUS_WAIT_MASK : 0) |
        (((busVals & (1 << BR_M1_PIB_BAR)) == 0) ? BR_CTRL_BUS_M1_MASK : 0);

    // Read the data bus
    // If the target machine is writing then this will be the data it wants to write
    // If reading then the memory/IO system may have placed its data onto the data bus 

    // Enable data bus onto PIB
    digitalWrite(BR_DATA_DIR_IN, 1);
    muxSet(BR_MUX_DATA_OE_BAR_LOW);

    // Delay to allow data to settle
    lowlev_cycleDelay(CYCLES_DELAY_FOR_READ_FROM_PIB);

    // Read the data bus values
    uint32_t dataBusVals = pibGetValue() & 0xff;

    // Clear Mux
    muxClear();

    // Send this to anything listening
    uint32_t retVal = BR_MEM_ACCESS_RSLT_NOT_DECODED;
    if (_pBusAccessCallback)
        retVal = _pBusAccessCallback(addr, dataBusVals, ctrlBusVals);

    // If Z80 is reading from the data bus (inc reading an ISR vector)
    // and result is valid then put the returned data onto the bus
    bool isWriting = (busVals & (1 << BR_WR_BAR)) == 0;
    if (!isWriting && ((retVal & BR_MEM_ACCESS_RSLT_NOT_DECODED) == 0))
    {
        // Now driving data onto the target data bus
        digitalWrite(BR_DATA_DIR_IN, 0);
        // A flip-flop to handles data OE during the IORQ/MREQ cycle and 
        // deactivates at the end of that cycle - so prime this flip-flop here
        muxSet(BR_MUX_DATA_OE_BAR_LOW);
        pibSetOut();
        pibSetValue(retVal & 0xff);
        lowlev_cycleDelay(CYCLES_DELAY_FOR_WAIT_CLEAR);
        muxClear();
        lowlev_cycleDelay(CYCLES_DELAY_FOR_TARGET_READ);
    }

#ifdef USE_PI_SPI0_CE0_AS_DEBUG_PIN
    // digitalWrite(BR_DEBUG_PI_SPI0_CE0, m1Asserted);
#endif

    // Check if we should hold the target processor at this point
    // if (_pauseIsPaused && m1Asserted && !instructionInjection)
    if ((retVal & BR_MEM_ACCESS_HOLD) != 0)
    {
        // Store the current address, data and ctrl line state
        _pauseCurAddr = addr;
        _pauseCurData = (isWriting || (retVal & BR_MEM_ACCESS_RSLT_NOT_DECODED)) ? dataBusVals : retVal;
        _pauseCurControlBus = ctrlBusVals;

        // Clear wait detected
        clearWaitDetected();
    }
    else
    {
        // No pause requested - clear the WAIT state so execution can continue
#ifdef USE_PI_SPI0_CE0_AS_DEBUG_PIN
        digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
#endif
        // Clear wait detected
        clearWaitDetected();

        // Clear the WAIT state flip-flop
        clearWaitFF();
#ifdef USE_PI_SPI0_CE0_AS_DEBUG_PIN
        digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
#endif
    }

#ifdef USE_PI_SPI0_CE0_AS_DEBUG_PIN
        digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
#endif

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Clear/Enable/Disable Wait Interrupts
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusAccess::waitIntClear()
{
    // Clear detected edge on any pin
    WR32(ARM_GPIO_GPEDS0, 0xffffffff);
}

void BusAccess::waitIntDisable()
{
    // Disable Fast Interrupts
    lowlev_disable_fiq();
}

void BusAccess::waitIntEnable()
{
    // Clear interrupts first
    waitIntClear();

    // Enable Fast Interrupts
    lowlev_enable_fiq();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Pause & Single Step Handling
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BR_RETURN_TYPE BusAccess::pause()
{
    if (_pauseIsPaused)
        return BR_ALREADY_DONE;

    // Request bus so we can grab memory data before entering a wait state
    if (_requestBusWhenSingleStepping)
        if (controlRequestAndTake() != BR_OK)
            return BR_NO_BUS_ACK;

    _pauseIsPaused = true;

    return BR_OK;
}

void BusAccess::pauseGetCurrent(uint32_t* pAddr, uint32_t* pData, uint32_t* pFlags)
{
    *pAddr = _pauseCurAddr;
    *pData = _pauseCurData;
    *pFlags = _pauseCurControlBus;
}

bool BusAccess::pauseIsPaused()
{
    return _pauseIsPaused;
}

BR_RETURN_TYPE BusAccess::pauseStep()
{
    // Check if paused
    if (!_pauseIsPaused)
        return BR_ALREADY_DONE;

    // Disable interrupts
    lowlev_disable_fiq();

    // Request if required
    if (_requestBusWhenSingleStepping)
        controlRequest();

    // Timing critical in here
    lowlev_disable_irq();

    // Clear the WAIT state flip-flop
    clearWaitFF();

    // Clear wait detected
    clearWaitDetected();

    // Enable interrupts
    lowlev_enable_irq();
    lowlev_enable_fiq();

    // Request bus so we can grab memory data before entering a wait state
    if (_requestBusWhenSingleStepping)
        if (controlRequestAndTake() != BR_OK)
            return BR_NO_BUS_ACK;

    return BR_OK;
}

BR_RETURN_TYPE BusAccess::pauseRelease()
{
    // Check if paused
    if (!_pauseIsPaused)
        return BR_ALREADY_DONE;

    // Disable interrupts
    lowlev_disable_irq();
    lowlev_disable_fiq();

    // Release pause
    _pauseIsPaused = false;

    // Clear the WAIT state flip-flop
    clearWaitFF();

    // Clear wait detected
    clearWaitDetected();

    // Enable interrupts
    lowlev_enable_irq();
    lowlev_enable_fiq();

    return BR_OK;
}

void BusAccess::pauseRequestBusOnStep(bool requestBus)
{
    _requestBusWhenSingleStepping = requestBus;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusAccess::service()
{
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

void BusAccess::muxClear()
{
    // Clear to a safe setting - sets HADDR_SER low
    WR32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
}

void BusAccess::muxSet(int muxVal)
{
    // Clear first as this is a safe setting - sets HADDR_SER low
    WR32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
    // Now set bits required
    WR32(ARM_GPIO_GPSET0, muxVal << BR_MUX_LOW_BIT_POS);
}

// Set the PIB (pins used for data bus access) to outputs (from Pi)
void BusAccess::pibSetOut()
{
    WR32(BR_PIB_GPF_REG, (RD32(BR_PIB_GPF_REG) & BR_PIB_GPF_MASK) | BR_PIB_GPF_OUTPUT);
}

// Set the PIB (pins used for data bus access) to inputs (to Pi)
void BusAccess::pibSetIn()
{
    WR32(BR_PIB_GPF_REG, (RD32(BR_PIB_GPF_REG) & BR_PIB_GPF_MASK) | BR_PIB_GPF_INPUT);
}

// Set a value onto the PIB (pins used for data bus access)
void BusAccess::pibSetValue(uint8_t val)
{
    uint32_t setBits = ((uint32_t)val) << BR_DATA_BUS;
    uint32_t clrBits = (~(((uint32_t)val) << BR_DATA_BUS)) & (~BR_PIB_MASK);
    WR32(ARM_GPIO_GPSET0, setBits);
    WR32(ARM_GPIO_GPCLR0, clrBits);
}

// Get a value from the PIB (pins used for data bus access)
uint8_t BusAccess::pibGetValue()
{
    return (RD32(ARM_GPIO_GPLEV0) >> BR_DATA_BUS) & 0xff;
}

void BusAccess::clearWaitFF()
{
    // Clear the WAIT state flip-flop
    WR32(ARM_GPIO_GPCLR0, BR_MREQ_WAIT_EN_MASK | BR_IORQ_WAIT_EN_MASK);

    // Pulse needs to be wide enough to clear flip-flop reliably
    lowlev_cycleDelay(CYCLES_DELAY_FOR_WAIT_CLEAR);

    // Re-enable the wait state generation as defined by the global mask
    WR32(ARM_GPIO_GPSET0, _waitStateEnMask);
}

void BusAccess::clearWaitDetected()
{
    // Clear detected edge on any pin
    WR32(ARM_GPIO_GPEDS0, 0xffffffff);
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
