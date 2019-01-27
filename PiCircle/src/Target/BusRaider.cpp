// Bus Raider
// Rob Dobson 2018

#include "busraider.h"
#include <circle/interrupt.h>
#include <circle/timer.h>
#include <circle/bcm2835.h>
#include <circle/memio.h>
#include "../System/lowlev.h"

// Uncomment the following line to use SPI0 CE0 of the Pi as a debug pin
#define USE_PI_SPI0_CE0_AS_DEBUG_PIN 1

// Comment out this line to disable WAIT state generation altogether
#define BR_ENABLE_WAIT_AND_FIQ 1

// Instrument FIQ
// #define INSTRUMENT_BUSRAIDER_FIQ 1

// Callback on bus access
BusRaiderCBFnType* BusRaider::_pBusAccessCallback = NULL;

// Time to wait for control lines to be valid
static const int MAX_WAIT_FOR_CTRL_LINES_US = 10000;

// Period target write control bus line is asserted during a write
#define CYCLES_DELAY_FOR_WRITE_TO_TARGET 500

// Period target read control bus line is asserted during a read from the PIB (any bus element)
#define CYCLES_DELAY_FOR_READ_FROM_PIB 500

// Delay in machine cycles for setting the pulse width when clearing/incrementing the address counter/shift-reg
#define CYCLES_DELAY_FOR_CLEAR_LOW_ADDR 500
#define CYCLES_DELAY_FOR_LOW_ADDR_SET 500
#define CYCLES_DELAY_FOR_HIGH_ADDR_SET 500
#define CYCLES_DELAY_FOR_WAIT_CLEAR 500

BusRaider::BusRaider()
{
    // Clear
    _waitStateEnMask = 0;
    _waitIntEnabled = false;
    _pauseCurAddr = 0;
    _pauseCurData = 0;
    _pauseCurControlBus = 0;
}

BusRaider::~BusRaider()
{

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Initialisation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Initialise the bus raider
void BusRaider::init()
{
    // Disable FIQ - used for wait state handling
    waitIntDisable();

    // Setup the multiplexer
    setPinOut(_pinMux[0], BR_MUX_0, 0);
    setPinOut(_pinMux[1], BR_MUX_1, 0);
    setPinOut(_pinMux[2], BR_MUX_2, 0);

    // Bus Request
    setPinOut(_pinBusReq, BR_BUSRQ_BAR, 1);
    setPinIn(_pinBusAck, BR_BUSACK_BAR);

    // Setup the PIB
    setPinIn(_pinPIB[0], BR_DATA_BUS);
    setPinIn(_pinPIB[1], BR_DATA_BUS+1);
    setPinIn(_pinPIB[2], BR_DATA_BUS+2);
    setPinIn(_pinPIB[3], BR_DATA_BUS+3);
    setPinIn(_pinPIB[4], BR_DATA_BUS+4);
    setPinIn(_pinPIB[5], BR_DATA_BUS+5);
    setPinIn(_pinPIB[6], BR_DATA_BUS+6);
    setPinIn(_pinPIB[7], BR_DATA_BUS+7);

    // Setup control bus
    setPinIn(_pinMREQ, BR_MREQ_BAR);
    setPinIn(_pinIORQ, BR_IORQ_BAR);
    setPinIn(_pinRD, BR_RD_BAR);
    setPinIn(_pinWR, BR_WR_BAR);

    // Clear wait states initially - leaving LOW disables wait state generation
    setPinOut(_pinWaitMREQ, BR_MREQ_WAIT_EN, 0);
    setPinOut(_pinWaitIORQ, BR_IORQ_WAIT_EN, 0);
    _waitStateEnMask = 0;

    // Address push
    setPinOut(_pinAddrPush, BR_PUSH_ADDR_BAR, 1);

    // High address clock
    setPinOut(_pinAddrHighClock, BR_HADDR_CK, 0);

    // Low address clock
    setPinOut(_pinAddrLowClock, BR_LADDR_CK, 0);

    // Data bus direction
    setPinOut(_pinDataDirIn, BR_DATA_DIR_IN, 1);

    // Remember wait interrupts currently disabled
    _waitIntEnabled = false;

    // Debug
#ifdef USE_PI_SPI0_CE0_AS_DEBUG_PIN
    setPinOut(_pinDebugCEO, BR_DEBUG_PI_SPI0_CE0, 0);
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Host Actions (Reset/NMI/IRQ)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Reset the host
void BusRaider::hostReset()
{
    // Disable wait interrupts
    waitIntDisable();

    // Reset by taking reset_bar low and then high
    muxSet(BR_MUX_RESET_Z80_BAR_LOW);

    // Hold the reset line low
    CTimer::SimpleusDelay(100);

    // Clear WAIT flip-flop
    write32(ARM_GPIO_GPCLR0, BR_MREQ_WAIT_EN_MASK | BR_IORQ_WAIT_EN_MASK);

    // // Re-enable the wait state generation
    write32(ARM_GPIO_GPSET0, _waitStateEnMask);

    // Clear wait interrupt conditions and enable
    waitIntClear();
    if (_waitIntEnabled)
        waitIntEnable();

    // Remove the reset condition
    muxClear();
}

// Hold host in reset state - call br_reset_host() to clear reset
void BusRaider::hostResetHold()
{
    // Disable wait interrupts
    waitIntDisable();

    // Reset by taking reset_bar low
    muxSet(BR_MUX_RESET_Z80_BAR_LOW);
}

// Non-maskable interrupt the host
void BusRaider::hostNMI()
{
    // NMI by taking nmi_bar line low and high
    muxSet(BR_MUX_NMI_BAR_LOW);
    CTimer::SimpleusDelay(10);
    muxClear();
}

// Maskable interrupt the host
void BusRaider::hostIRQ()
{
    // IRQ by taking irq_bar line low and high
    muxSet(BR_MUX_IRQ_BAR_LOW);
    CTimer::SimpleusDelay(10);
    muxClear();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Bus Request / Acknowledge
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Request access to the bus
void BusRaider::controlRequestBus()
{
    // Set the PIB to input
    pibSetIn();
    // Set data bus to input
    pinRawWrite(BR_DATA_DIR_IN, 1);
    // Request the bus
    pinRawWrite(BR_BUSRQ_BAR, 0);
}

// Check if bus request has been acknowledged
int BusRaider::controlBusAcknowledged()
{
    return pinRawRead(BR_BUSACK_BAR) == 0;
}

// Take control of bus
void BusRaider::controlTake()
{
    // Disable interrupts
    waitIntDisable();

    // Control bus
    pinRawMode(BR_MREQ_BAR, false, 1);
    pinRawMode(BR_IORQ_BAR, false, 1);
    pinRawMode(BR_RD_BAR, false, 1);
    pinRawMode(BR_WR_BAR, false, 1);

    // Address bus enabled
    pinRawWrite(BR_PUSH_ADDR_BAR, 0);
}

// Release control of bus
void BusRaider::controlRelease(bool resetTargetOnRelease)
{
    // Control bus
    pinRawMode(BR_MREQ_BAR, true, 0);
    pinRawMode(BR_IORQ_BAR, true, 0);
    pinRawMode(BR_RD_BAR, true, 0);
    pinRawMode(BR_WR_BAR, true, 0);

    // Address bus not enabled
    pinRawWrite(BR_PUSH_ADDR_BAR, 1);
    
    // Re-enable interrupts if required
    if (_waitIntEnabled)
        waitIntEnable();
    
    // Check for reset
    if (resetTargetOnRelease)
    {
        // Reset by taking reset_bar low and then high
        muxSet(BR_MUX_RESET_Z80_BAR_LOW);
        
        // No longer request bus
        pinRawWrite(BR_BUSRQ_BAR, 1);
        CTimer::SimpleusDelay(10);
        muxClear();
    }
    else
    {
        // No longer request bus
        pinRawWrite(BR_BUSRQ_BAR, 1);
    }
}

// Request bus, wait until available and take control
BR_RETURN_TYPE BusRaider::controlReqAndTake()
{
    // Request
    controlRequestBus();

    // Check for ack
    for (int i = 0; i < MAX_WAIT_FOR_ACK_US; i++) 
    {
        if (controlBusAcknowledged()) 
            break;
        CTimer::SimpleusDelay(1);
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
void BusRaider::addrLowSet(uint32_t lowAddrByte)
{
    // Clear initially
    lowlevCycleDelay(CYCLES_DELAY_FOR_CLEAR_LOW_ADDR);
    muxSet(BR_MUX_LADDR_CLR_BAR_LOW);
    // Delay a few cycles
    lowlevCycleDelay(CYCLES_DELAY_FOR_CLEAR_LOW_ADDR);
    muxClear();
    // Clock the required value in - requires one more count than
    // expected as the output register is one clock pulse behind the counter
    for (uint32_t i = 0; i < (lowAddrByte & 0xff) + 1; i++) {
        write32(ARM_GPIO_GPSET0, 1 << BR_LADDR_CK);
        lowlevCycleDelay(CYCLES_DELAY_FOR_LOW_ADDR_SET);
        write32(ARM_GPIO_GPCLR0, 1 << BR_LADDR_CK);
        lowlevCycleDelay(CYCLES_DELAY_FOR_LOW_ADDR_SET);
    }
}

// Increment low address value by clocking the counter
// Assumptions:
// - some other code will set push-addr to enable onto the address bus
void BusRaider::addrLowInc()
{
    write32(ARM_GPIO_GPSET0, 1 << BR_LADDR_CK);
    lowlevCycleDelay(CYCLES_DELAY_FOR_LOW_ADDR_SET);
    write32(ARM_GPIO_GPCLR0, 1 << BR_LADDR_CK);
    lowlevCycleDelay(CYCLES_DELAY_FOR_LOW_ADDR_SET);
}

// Set the high address value
// Assumptions:
// - some other code will set push-addr to enable onto the address bus
void BusRaider::addrHighSet(uint32_t highAddrByte)
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
        lowlevCycleDelay(CYCLES_DELAY_FOR_HIGH_ADDR_SET);
        // Shift the address value for next bit
        highAddrByte = highAddrByte << 1;
        // Clock the bit
        write32(ARM_GPIO_GPSET0, 1 << BR_HADDR_CK);
        lowlevCycleDelay(CYCLES_DELAY_FOR_HIGH_ADDR_SET);
        write32(ARM_GPIO_GPCLR0, 1 << BR_HADDR_CK);
    }
    // Clear multiplexer
    lowlevCycleDelay(CYCLES_DELAY_FOR_HIGH_ADDR_SET);
    muxClear();
}

// Set the full address
// Assumptions:
// - some other code will set push-addr to enable onto the address bus
void BusRaider::addrSet(unsigned int addr)
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
void BusRaider::byteWrite(uint32_t byte, int iorq)
{
    // Set the data onto the PIB
    pibSetValue(byte);
    // Perform the write
    // Clear DIR_IN (so make direction out), enable data output onto data bus and MREQ_BAR active
    write32(ARM_GPIO_GPCLR0, (1 << BR_DATA_DIR_IN) | BR_MUX_CTRL_BIT_MASK | (1 << (iorq ? BR_IORQ_BAR : BR_MREQ_BAR)));
    write32(ARM_GPIO_GPSET0, BR_MUX_DATA_OE_BAR_LOW << BR_MUX_LOW_BIT_POS);
    // Write the data by setting WR_BAR active
    write32(ARM_GPIO_GPCLR0, (1 << BR_WR_BAR));
#ifdef CYCLES_DELAY_FOR_WRITE_TO_TARGET
    // Target write delay
    lowlevCycleDelay(CYCLES_DELAY_FOR_WRITE_TO_TARGET);
#endif
    // Deactivate and leave data direction set to inwards
    write32(ARM_GPIO_GPSET0, (1 << BR_DATA_DIR_IN) | (1 << (iorq ? BR_IORQ_BAR : BR_MREQ_BAR)) | (1 << BR_WR_BAR));
    muxClear();
}

// Read a single byte from currently set address (or IO port)
// Assumes:
// - control of host bus has been requested and acknowledged
// - address bus is already set and output enabled to host bus
// - PIB is already set to input
// - data direction on data bus driver is set to input (default)
uint8_t BusRaider::byteRead(int iorq)
{
    // Enable data output onto PIB (data-dir must be inwards already), MREQ_BAR and RD_BAR both active
    write32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK | (1 << (iorq ? BR_IORQ_BAR : BR_MREQ_BAR)) | (1 << BR_RD_BAR));
    write32(ARM_GPIO_GPSET0, BR_MUX_DATA_OE_BAR_LOW << BR_MUX_LOW_BIT_POS);
    // Delay to allow data to settle
    lowlevCycleDelay(CYCLES_DELAY_FOR_READ_FROM_PIB);
    // Get the data
    uint8_t val = (read32(ARM_GPIO_GPLEV0) >> BR_DATA_BUS) & 0xff;
    // Deactivate leaving data-dir inwards
    write32(ARM_GPIO_GPSET0, (1 << (iorq ? BR_IORQ_BAR : BR_MREQ_BAR)) | (1 << BR_RD_BAR));
    // Clear the MUX to a safe setting - sets HADDR_SER low
    write32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
    return val;
}

// Write a consecutive block of memory to host
BR_RETURN_TYPE BusRaider::blockWrite(uint32_t addr, const uint8_t* pData, uint32_t len, int busRqAndRelease, int iorq)
{
	// for (int i = 0; i < 10; i++)
	// {
	// 	pinRawWrite(8, 1);
	// 	CTimer::SimpleusDelay(1);
	// 	pinRawWrite(8, 0);
	// 	CTimer::SimpleusDelay(1);
	// }


    // Check if we need to request bus
    if (busRqAndRelease) {
        // Request bus and take control after ack
        BR_RETURN_TYPE ret = controlReqAndTake();
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

        // pinRawWrite(BR_DEBUG_PI_SPI0_CE0, 1);
        // pinRawWrite(BR_DEBUG_PI_SPI0_CE0, 1);
        // pinRawWrite(BR_DEBUG_PI_SPI0_CE0, 0);
        // pinRawWrite(BR_DEBUG_PI_SPI0_CE0, 0);

        // for (int i = 0; i < 10; i++)
        // {
        //     pinRawWrite(8, 1);
        //     CTimer::SimpleusDelay(1);
        //     pinRawWrite(8, 0);
        //     CTimer::SimpleusDelay(1);
        // }

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
BR_RETURN_TYPE BusRaider::blockRead(uint32_t addr, uint8_t* pData, uint32_t len, int busRqAndRelease, int iorq)
{
    // Check if we need to request bus
    if (busRqAndRelease) {
        // Request bus and take control after ack
        BR_RETURN_TYPE ret = controlReqAndTake();
        if (ret != BR_OK)
            return ret;
    }

    // Set PIB to input
    pibSetIn();

    // Set the address to initial value
    addrSet(addr);

    // Enable data onto to PIB
    write32(ARM_GPIO_GPSET0, BR_MUX_DATA_OE_BAR_LOW << BR_MUX_LOW_BIT_POS);

    // Iterate data
    for (uint32_t i = 0; i < len; i++) {

        // IORQ_BAR / MREQ_BAR and RD_BAR both active
        write32(ARM_GPIO_GPCLR0, (1 << (iorq ? BR_IORQ_BAR : BR_MREQ_BAR)) | (1 << BR_RD_BAR));
        
        // Delay to allow data bus to settle
        lowlevCycleDelay(CYCLES_DELAY_FOR_READ_FROM_PIB);
        
        // Get the data
        *pData = (read32(ARM_GPIO_GPLEV0) >> BR_DATA_BUS) & 0xff;

        // Deactivate IORQ/MREQ and RD and clock the low address
        write32(ARM_GPIO_GPSET0, (1 << (iorq ? BR_IORQ_BAR : BR_MREQ_BAR)) | (1 << BR_RD_BAR) | (1 << BR_LADDR_CK));

        // Low address clock pulse period
        lowlevCycleDelay(CYCLES_DELAY_FOR_LOW_ADDR_SET);

        // Clock pulse high again
        write32(ARM_GPIO_GPCLR0, 1 << BR_LADDR_CK);

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
// Enable interrupts
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Utility Functions
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusRaider::setPinOut(CGPIOPin& gpioPin, int pinNumber, bool val)
{
    pinRawMode(pinNumber, false, val);
    // gpioPin.AssignPin(pinNumber);
    // gpioPin.SetMode(TGPIOMode::GPIOModeOutput);
    // gpioPin.Write(val);
}

void BusRaider::setPinIn(CGPIOPin& gpioPin, int pinNumber)
{
    pinRawMode(pinNumber, true, 0);
    // gpioPin.AssignPin(pinNumber);
    // gpioPin.SetMode(TGPIOMode::GPIOModeInput);
}

// Set the MUX
void BusRaider::muxSet(int muxVal)
{
    // Clear first as this is a safe setting - sets HADDR_SER low
    write32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
    // Now set bits required
    write32(ARM_GPIO_GPSET0, muxVal << BR_MUX_LOW_BIT_POS);
}

// Clear the MUX
void BusRaider::muxClear()
{
    // Clear to a safe setting - sets HADDR_SER low
    write32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
}

// Clear wait interrupt
void BusRaider::waitIntClear()
{
    // Clear detected edge on any pin
    write32(ARM_GPIO_GPEDS0, 0xffffffff);
}

// Interrupt disable
void BusRaider::waitIntDisable()
{
    CInterruptSystem::DisableFIQ();
}

// Interrupt enable
void BusRaider::waitIntEnable()
{
    CInterruptSystem::EnableFIQ(0);
}

// Set the PIB (pins used for data bus access) to outputs (from Pi)
void BusRaider::pibSetOut()
{
    write32(BR_PIB_GPF_REG, (read32(BR_PIB_GPF_REG) & BR_PIB_GPF_MASK) | BR_PIB_GPF_OUTPUT);
}

// Set the PIB (pins used for data bus access) to inputs (to Pi)
void BusRaider::pibSetIn()
{
    write32(BR_PIB_GPF_REG, (read32(BR_PIB_GPF_REG) & BR_PIB_GPF_MASK) | BR_PIB_GPF_INPUT);
}

// Set a value onto the PIB (pins used for data bus access)
void BusRaider::pibSetValue(uint8_t val)
{
    uint32_t setBits = ((uint32_t)val) << BR_DATA_BUS;
    uint32_t clrBits = (~(((uint32_t)val) << BR_DATA_BUS)) & (~BR_PIB_MASK);
    write32(ARM_GPIO_GPSET0, setBits);
    write32(ARM_GPIO_GPCLR0, clrBits);
}

// Get a value from the PIB (pins used for data bus access)
uint8_t BusRaider::pibGetValue()
{
    return (read32(ARM_GPIO_GPLEV0) >> BR_DATA_BUS) & 0xff;
}

void BusRaider::pinRawMode(int pinNumber, bool inputMode, bool val)
{
    if (!inputMode)
        pinRawWrite(pinNumber, val);
    uint32_t gpfSelReg = ARM_GPIO_GPFSEL0 + (pinNumber / 10) * 4;
    uint8_t bitPos = ((pinNumber - ((pinNumber / 10) * 10)) % 10) * 3;
    uint32_t regVal = read32(gpfSelReg);
    regVal &= ~(7 << bitPos);
    uint8_t modeVal = inputMode ? 0 : 1;
    regVal |= (modeVal & 0x0f) << bitPos;
    write32(gpfSelReg, regVal);
    if (!inputMode)
        pinRawWrite(pinNumber, val);
}

void BusRaider::pinRawWrite(int pinNumber, bool val)
{
    if (val) {
        if (pinNumber < 32)
            write32(ARM_GPIO_GPSET0, 1 << pinNumber);
        else
            write32(ARM_GPIO_GPSET0+0x04, 1 << (pinNumber - 32));
    } else {
        if (pinNumber < 32)
            write32(ARM_GPIO_GPCLR0, 1 << pinNumber);
        else
            write32(ARM_GPIO_GPCLR0+0x04, 1 << (pinNumber - 32));
    }
}

bool BusRaider::pinRawRead(int pinNumber)
{
    if (pinNumber < 32)
        return ((read32(ARM_GPIO_GPLEV0) >> pinNumber) & 0x01) != 0;
    return ((read32(ARM_GPIO_GPLEV0+0x04) >> (pinNumber - 32)) & 0x01) != 0;
}







#ifdef OLD_CODE



// Clear all IO space
void br_clear_all_io()
{
    // Fill IO "memory" with 0xff
    uint8_t tmpBuf[0x100];
    for (int kk = 0; kk < 0x100; kk++)
        tmpBuf[kk] = 0xff;
    br_write_block(0, tmpBuf, 0x100, 1, 1);  
}

// Set the bus access interrupt callback
void br_set_bus_access_callback(br_bus_access_callback_fntype* pBusAccessCallback)
{
    __br_pBusAccessCallback = pBusAccessCallback;
}

void br_remove_bus_access_callback()
{
    __br_pBusAccessCallback = NULL;
}

#ifdef INSTRUMENT_BUSRAIDER_FIQ

#define MAX_IO_PORT_VALS 1000
volatile uint32_t iorqPortAccess[MAX_IO_PORT_VALS];
volatile int iorqIsNotActive = 0;
volatile int iorqCount = 0;
int loopCtr = 0;

#endif

// Enable wait-states and FIQ
void br_enable_mem_and_io_access(bool enWaitOnIORQ, bool enWaitOnMREQ)
{
    // Disable interrupts
    br_disable_wait_interrupt();

#ifdef INSTRUMENT_BUSRAIDER_FIQ
    for (int i = 0; i < MAX_IO_PORT_VALS; i++)
    {
        iorqPortAccess[i] = 0;
    }
#endif

    // Enable wait state generation
    __br_wait_state_en_mask = 0;
    if (enWaitOnIORQ)
        __br_wait_state_en_mask = __br_wait_state_en_mask | BR_IORQ_WAIT_EN_MASK;
    if (enWaitOnMREQ)
        __br_wait_state_en_mask = __br_wait_state_en_mask | BR_MREQ_WAIT_EN_MASK;
    write32(ARM_GPIO_GPCLR0, BR_MREQ_WAIT_EN_MASK | BR_IORQ_WAIT_EN_MASK);

    // Check if any wait-states to be generated
    if (__br_wait_state_en_mask == 0)
    {
        __br_wait_interrupt_enabled = false;
        return;
    }

#ifdef BR_ENABLE_WAIT_AND_FIQ

    // Set wait-state generation
    write32(ARM_GPIO_GPSET0, __br_wait_state_en_mask);

    // Set vector for WAIT state interrupt
    irq_set_wait_state_handler(br_wait_state_isr);

    // Setup edge triggering on falling edge of IORQ
    // Clear any current detected edges
    write32(ARM_GPIO_GPEDS0, (1 << BR_IORQ_BAR) | (1 << BR_MREQ_BAR));  
    // Set falling edge detect
    if (enWaitOnIORQ)
        write32(GPFEN0, read32(GPFEN0) | (1 << BR_IORQ_BAR));
    else
        write32(GPFEN0, read32(GPFEN0) & (~(1 << BR_IORQ_BAR)));
    if (enWaitOnMREQ)
        write32(GPFEN0, read32(GPFEN0) | (1 << BR_WAIT_BAR));
    else
        write32(GPFEN0, read32(GPFEN0) & (~(1 << BR_WAIT_BAR)));

    // Enable FIQ interrupts on GPIO[3] which is any GPIO pin
    write32(IRQ_FIQ_CONTROL, (1 << 7) | 52);

    // Enable Fast Interrupts
    __br_wait_interrupt_enabled = true;
    br_enable_wait_interrupt();

#endif  
}

// Main Interrupt Service Routing for MREQ/IORQ based interrupts
// The Bus Raider hardware creates WAIT states while this IRQ is running
// These have to be cleared on exit and the interrupt source register also cleared
void br_wait_state_isr(void* pData)
{
    // pData unused
    pData = pData;

    // Loop until control lines are valid
    // In a write cycle WR is asserted after MREQ so we need to wait until WR changes before
    // we can determine what kind of operation is in progress 
    bool ctrlValid = false;
    int avoidLockupCtr = 0;
    uint32_t busVals = 0;
    while(avoidLockupCtr < MAX_WAIT_FOR_CTRL_LINES_US)
    {
        // Read the control lines
        busVals = read32(ARM_GPIO_GPLEV0);

        // Specifically need to wait if MREQ is low but both RD and WR are high (wait for WR to go low in this case)
        ctrlValid = !(((busVals & (1 << BR_MREQ_BAR)) == 0) && (((busVals & (1 << BR_RD_BAR)) != 0) && ((busVals & (1 << BR_WR_BAR)) != 0)));
        if (ctrlValid)
            break;

        // Count loops
        avoidLockupCtr++;
    }

    // Check if we have valid control lines
    if ((!ctrlValid) || ((busVals & (1 << BR_WAIT_BAR)) != 0))
    {

#ifdef USE_PI_SPI0_CE0_AS_DEBUG_PIN
        digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
#endif

        // Spurious ISR?
        write32(ARM_GPIO_GPCLR0, BR_MREQ_WAIT_EN_MASK | BR_IORQ_WAIT_EN_MASK);

        // Pulse needs to be wide enough to clear flip-flop reliably
        lowlevCycleDelay(CYCLES_DELAY_FOR_WAIT_CLEAR);

        // Re-enable the wait state generation
        write32(ARM_GPIO_GPSET0, __br_wait_state_en_mask);

        // Clear detected edge on any pin
        write32(ARM_GPIO_GPEDS0, 0xffffffff);

#ifdef USE_PI_SPI0_CE0_AS_DEBUG_PIN
        digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
#endif
        return;
    }

    // Enable the low address onto the PIB
    pibSetIn();
    write32(ARM_GPIO_GPSET0, 1 << BR_DATA_DIR_IN);
    muxSet(BR_MUX_LADDR_OE_BAR);

    // Delay to allow data to settle
    lowlevCycleDelay(CYCLES_DELAY_FOR_READ_FROM_PIB);

    // Get low address value
    uint32_t addr = br_get_pib_value() & 0xff;

    // Read the high address
    muxSet(BR_MUX_HADDR_OE_BAR);

    // Delay to allow data to settle
    lowlevCycleDelay(CYCLES_DELAY_FOR_READ_FROM_PIB);

    // Or in the high address
    addr |= (br_get_pib_value() & 0xff) << 8;
 
    // Clear the mux to deactivate output enables
    muxClear();

    // Delay to allow data to settle
    lowlevCycleDelay(CYCLES_DELAY_FOR_READ_FROM_PIB);

    // Get the appropriate bits for up-line communication
    uint32_t ctrlBusVals = 
        (((busVals & (1 << BR_RD_BAR)) == 0) ? BR_CTRL_BUS_RD_MASK : 0) |
        (((busVals & (1 << BR_WR_BAR)) == 0) ? BR_CTRL_BUS_WR_MASK : 0) |
        (((busVals & (1 << BR_MREQ_BAR)) == 0) ? BR_CTRL_BUS_MREQ_MASK : 0) |
        (((busVals & (1 << BR_IORQ_BAR)) == 0) ? BR_CTRL_BUS_IORQ_MASK : 0) |
        (((busVals & (1 << BR_WAIT_BAR)) == 0) ? BR_CTRL_BUS_WAIT_MASK : 0) |
        (((read32(ARM_GPIO_GPLEV0) & (1 << BR_M1_PIB_BAR)) == 0) ? BR_CTRL_BUS_M1_MASK : 0);

    // Read the data bus if the target machine is writing
    uint32_t dataBusVals = 0;
    bool isWriting = (busVals & (1 << BR_WR_BAR)) == 0;
    if (isWriting)
    {
        // Enable data bus onto PIB
        digitalWrite(BR_DATA_DIR_IN, 1);
        muxSet(BR_MUX_DATA_OE_BAR_LOW);

        // Delay to allow data to settle
        lowlevCycleDelay(CYCLES_DELAY_FOR_READ_FROM_PIB);

        // Read the data bus values
        dataBusVals = br_get_pib_value() & 0xff;

        // Clear Mux
        muxClear();
    }

    // Send this to anything listening
    uint32_t retVal = BR_MEM_ACCESS_RSLT_NOT_DECODED;
    if (__br_pBusAccessCallback)
        retVal = __br_pBusAccessCallback(addr, dataBusVals, ctrlBusVals);

    // If not writing and result is valid then put the returned data onto the bus
    if (!isWriting && ((retVal & BR_MEM_ACCESS_RSLT_NOT_DECODED) == 0))
    {
        // Now driving data onto the target data bus
        digitalWrite(BR_DATA_DIR_IN, 0);
        // A flip-flop to handles data OE during the IORQ/MREQ cycle and 
        // deactivates at the end of that cycle - so prime this flip-flop here
        muxSet(BR_MUX_DATA_OE_BAR_LOW);
        pibSetOut();
        pibSetValue(retVal & 0xff);
        muxClear();
    }

    // digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);

    // Check if pause requested
    __br_pause_is_paused = ((retVal & BR_MEM_ACCESS_RSLT_REQ_PAUSE) != 0);
    if (!__br_pause_is_paused)
    {
        // TODO - DEBUG - MUST BE REMOVED
        // write32(ARM_GPIO_GPSET0, 1 << BR_HADDR_CK);
        // write32(ARM_GPIO_GPSET0, 1 << BR_HADDR_CK);
        // write32(ARM_GPIO_GPCLR0, 1 << BR_HADDR_CK);

        // No pause requested - clear the WAIT state so execution can continue
        // Clear the WAIT state flip-flop
        write32(ARM_GPIO_GPCLR0, BR_MREQ_WAIT_EN_MASK | BR_IORQ_WAIT_EN_MASK);

        // Pulse needs to be wide enough to clear flip-flop reliably
        lowlevCycleDelay(CYCLES_DELAY_FOR_WAIT_CLEAR);

        // Re-enable the wait state generation as defined by the global mask
        write32(ARM_GPIO_GPSET0, __br_wait_state_en_mask);

        // // Clear detected edge on any pin
        write32(ARM_GPIO_GPEDS0, 0xffffffff);
        
        // TODO - DEBUG - MUST BE REMOVED
        // write32(ARM_GPIO_GPSET0, 1 << BR_HADDR_CK);
        // write32(ARM_GPIO_GPSET0, 1 << BR_HADDR_CK);
        // write32(ARM_GPIO_GPCLR0, 1 << BR_HADDR_CK);
    }
    else
    {
        // Store the current address, data and ctrl line state
        __br_pause_addr = addr;
        __br_pause_data = isWriting ? dataBusVals : retVal;
        __br_pause_flags = ctrlBusVals;
    }
}

void br_clear_wait_interrupt()
{
    // Clear detected edge on any pin
    write32(ARM_GPIO_GPEDS0, 0xffffffff);
}

void br_disable_wait_interrupt()
{
    // Disable Fast Interrupts
    lowlev_disable_fiq();
}

void br_enable_wait_interrupt()
{
    // Clear interrupts first
    br_clear_wait_interrupt();

    // Enable Fast Interrupts
    lowlev_enable_fiq();
}

void br_pause_get_current(uint32_t* pAddr, uint32_t* pData, uint32_t* pFlags)
{
    *pAddr = __br_pause_addr;
    *pData = __br_pause_data;
    *pFlags = __br_pause_flags;
}

bool br_pause_is_paused()
{
    return __br_pause_is_paused;
}

bool br_pause_release()
{
    // Check if paused
    if (!__br_pause_is_paused)
        return false;

    // Release pause
    __br_pause_is_paused = false;

    // Clear the WAIT state flip-flop
    write32(ARM_GPIO_GPCLR0, BR_MREQ_WAIT_EN_MASK | BR_IORQ_WAIT_EN_MASK);

    // Delay for a short time
    CTimer::SimpleusDelay(2);

        // TODO - delay??


    // Re-enable the wait state generation
    write32(ARM_GPIO_GPSET0, __br_wait_state_en_mask);

    // Clear detected edge on any pin
    write32(ARM_GPIO_GPEDS0, 0xffffffff);

    return true;
}

void br_service()
{
#ifdef INSTRUMENT_BUSRAIDER_FIQ
    loopCtr++;
    if (loopCtr > 500000)
    {
        bool anyOut = false;
        for (int i = 0; i < MAX_IO_PORT_VALS; i++)
        {
            if (iorqPortAccess[i] == 0)
                break;

            int accVal = (iorqPortAccess[i] >> 16) & 0x03;
            anyOut = true;
            ee_printf("port %x %c %c count %d; ",
                    iorqPortAccess[i] & 0xffff,
                    ((accVal & 0x01) != 0) ? 'R' : ' ',
                    ((accVal & 0x02) != 0) ? 'W' : ' ',
                    iorqPortAccess[i] >> 18);
        }
        if (iorqIsNotActive > 0)
        {
            if (anyOut)
                ee_printf("\n");
            ee_printf("Not actv %d, ", iorqIsNotActive);
        }
        ee_printf("IORQ_WAIT_EN %d count %d\n",digitalRead(BR_IORQ_WAIT_EN), iorqCount);
        loopCtr = 0;
    }
#endif
}

#endif
