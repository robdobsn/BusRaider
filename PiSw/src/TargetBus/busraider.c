// Bus Raider
// Rob Dobson 2018

#include "busraider.h"
#include "piwiring.h"
#include "../System/irq.h"
#include "../System/ee_printf.h"
#include "../System/lowlev.h"
#include "../System/lowlib.h"
#include "../System/bare_metal_pi_zero.h"

// Uncomment the following to use bitwise access to busses and pins on Pi
// #define USE_BITWISE_DATA_BUS_ACCESS 1
// #define USE_BITWISE_CTRL_BUS_ACCESS 1

// Uncomment the following line to use SPI0 CE0 of the Pi as a debug pin
#define USE_PI_SPI0_CE0_AS_DEBUG_PIN 1

// Comment out this line to disable WAIT state generation altogether
// #define BR_ENABLE_WAIT_AND_FIQ 1

// Instrument FIQ
// #define INSTRUMENT_BUSRAIDER_FIQ 1

// Callback on bus access
static br_bus_access_callback_fntype* __br_pBusAccessCallback = NULL;

// Current wait state mask for restoring wait enablement after change
static uint32_t __br_wait_state_en_mask = 0;

// Wait interrupt enablement cache (so it can be restored after disable)
static bool __br_wait_interrupt_enabled = false;

// Currently paused - i.e. wait is active
volatile bool __br_pause_is_paused = false;

// // Bus values while single stepping
uint32_t __br_pause_addr = 0;
uint32_t __br_pause_data = 0;
uint32_t __br_pause_flags = 0;

static const int MAX_WAIT_FOR_CTRL_LINES_US = 10000;

// Period target write control bus line is asserted during a write
// #define CYCLES_DELAY_FOR_WRITE_TO_TARGET 2

// Period target read control bus line is asserted during a read
//#define CYCLES_DELAY_FOR_READ_FROM_TARGET 2

// Delay in machine cycles for mux set
#define CYCLES_DELAY_FOR_MUX_SET 10

// Delay in machine cycles for setting the pulse width when clearing/incrementing the address counter/shift-reg
#define CYCLES_DELAY_FOR_CLEAR_LOW_ADDR 10
#define CYCLES_DELAY_FOR_LOW_ADDR_SET 10
#define CYCLES_DELAY_FOR_HIGH_ADDR_SET 10

// Set a pin to be an output and set initial value for that pin
void br_set_pin_out(int pin, int val)
{
    digitalWrite(pin, val);
    pinMode(pin, OUTPUT);
    digitalWrite(pin, val);
}

void br_mux_set_pins_output()
{
    br_set_pin_out(BR_MUX_0, 0);
    br_set_pin_out(BR_MUX_1, 0);
    br_set_pin_out(BR_MUX_2, 0);
}

void br_mux_clear()
{
    // Clear to a safe setting - sets HADDR_SER low
    WR32(GPCLR0, BR_MUX_CTRL_BIT_MASK);
}

void br_mux_set(int muxVal)
{
    // Clear first as this is a safe setting - sets HADDR_SER low
    WR32(GPCLR0, BR_MUX_CTRL_BIT_MASK);
    // Now set bits required
    WR32(GPSET0, muxVal << BR_MUX_LOW_BIT_POS);
#ifdef CYCLES_DELAY_FOR_MUX_SET
    lowlev_cycleDelay(CYCLES_DELAY_FOR_MUX_SET);
#endif    
}

// Initialise the bus raider
void br_init()
{
    // Disable FIQ - used for wait state handling
    br_disable_wait_interrupt();
    // Pins not set here as outputs will be inputs (inc PIB used for data bus)
    // Setup the multiplexer
    br_mux_set_pins_output();
    // Bus Request
    br_set_pin_out(BR_BUSRQ_BAR, 1);
    // Clear wait states initially - leaving LOW disables wait state generation
    br_set_pin_out(BR_MREQ_WAIT_EN, 0);
    br_set_pin_out(BR_IORQ_WAIT_EN, 0);
    __br_wait_state_en_mask = 0;
    // Address push
    br_set_pin_out(BR_PUSH_ADDR_BAR, 1);
    // High address clock
    br_set_pin_out(BR_HADDR_CK, 0);
    // Low address clock
    br_set_pin_out(BR_LADDR_CK, 0);
    // Data bus direction
    br_set_pin_out(BR_DATA_DIR_IN, 1);
    // Remember wait interrupts currently disabled
    __br_wait_interrupt_enabled = false;
    // Debug
#ifdef USE_PI_SPI0_CE0_AS_DEBUG_PIN
    pinMode(BR_DEBUG_PI_SPI0_CE0, OUTPUT);
#endif
}

// Hold host in reset state - call br_reset_host() to clear reset
void br_reset_hold_host()
{
    // Disable wait interrupts
    br_disable_wait_interrupt();

    // Reset by taking reset_bar low
    br_mux_set(BR_MUX_RESET_Z80_BAR_LOW);
}

// Reset the host
void br_reset_host()
{
    // Disable wait interrupts
    br_disable_wait_interrupt();

    // Reset by taking reset_bar low and then high
    br_mux_set(BR_MUX_RESET_Z80_BAR_LOW);

    // Hold the reset line low
    microsDelay(100);

    // Clear WAIT flip-flop
    WR32(GPCLR0, BR_MREQ_WAIT_EN_MASK | BR_IORQ_WAIT_EN_MASK);

    // // Re-enable the wait state generation
    WR32(GPSET0, __br_wait_state_en_mask);

    // Clear wait interrupt conditions and enable
    br_clear_wait_interrupt();
    if (__br_wait_interrupt_enabled)
        br_enable_wait_interrupt();

    // Remove the reset condition
    br_mux_clear();

    // Clear detected edge on any pin
    WR32(GPEDS0, 0xffffffff);
}

// Non-maskable interrupt the host
void br_nmi_host()
{
    // NMI by taking nmi_bar line low and high
    br_mux_set(BR_MUX_NMI_BAR_LOW);
    microsDelay(10);
    br_mux_clear();
}

// Maskable interrupt the host
void br_irq_host()
{
    // IRQ by taking irq_bar line low and high
    br_mux_set(BR_MUX_IRQ_BAR_LOW);
    microsDelay(10);
    br_mux_clear();
}

// Request access to the bus
void br_request_bus()
{
    // Set the PIB to input
    br_set_pib_input();
    // Set data bus to input
    WR32(GPSET0, 1 << BR_DATA_DIR_IN);
    // Request the bus
    digitalWrite(BR_BUSRQ_BAR, 0);
}

// Check if bus request has been acknowledged
int br_bus_acknowledged()
{
    return (RD32(GPLEV0) & (1 << BR_BUSACK_BAR)) == 0;
}

// Take control of bus
void br_take_control()
{
    // Disable interrupts
    br_disable_wait_interrupt();

    // Control bus
    br_set_pin_out(BR_WR_BAR, 1);
    br_set_pin_out(BR_RD_BAR, 1);
    br_set_pin_out(BR_MREQ_BAR, 1);
    br_set_pin_out(BR_IORQ_BAR, 1);

    // Address bus enabled
    digitalWrite(BR_PUSH_ADDR_BAR, 0);
}

// Release control of bus
void br_release_control(bool resetTargetOnRelease)
{
    // Control bus
    pinMode(BR_WR_BAR, INPUT);
    pinMode(BR_RD_BAR, INPUT);
    pinMode(BR_MREQ_BAR, INPUT);
    pinMode(BR_IORQ_BAR, INPUT);

    // Address bus not enabled
    digitalWrite(BR_PUSH_ADDR_BAR, 1);
    
    // Re-enable interrupts if required
    if (__br_wait_interrupt_enabled)
        br_enable_wait_interrupt();
    
    // Check for reset
    if (resetTargetOnRelease)
    {
        // Reset by taking reset_bar low and then high
        br_mux_set(BR_MUX_RESET_Z80_BAR_LOW);
        // No longer request bus
        digitalWrite(BR_BUSRQ_BAR, 1);
        microsDelay(10);
        br_mux_clear();
    }
    else
    {
        // No longer request bus
        digitalWrite(BR_BUSRQ_BAR, 1);
    }
}

// Request bus, wait until available and take control
BR_RETURN_TYPE br_req_and_take_bus()
{
    // Request
    br_request_bus();

    // Check for ack
    for (int i = 0; i < 10000; i++) {
        if (br_bus_acknowledged()) {
            break;
        }
    }
    if (!br_bus_acknowledged()) {
        br_release_control(false);
        return BR_NO_BUS_ACK;
    }

    // Take control
    br_take_control();
    return BR_OK;
}

// Set low address value by clearing and counting
// Assumptions:
// - some other code will set push-addr to enable onto the address bus
void br_set_low_addr(uint32_t lowAddrByte)
{
#ifdef CYCLES_DELAY_FOR_CLEAR_LOW_ADDR
    lowlev_cycleDelay(CYCLES_DELAY_FOR_CLEAR_LOW_ADDR);
#endif
    // Clear initially
    br_mux_set(BR_MUX_LADDR_CLR_BAR_LOW);
    // Delay a few cycles
#ifdef CYCLES_DELAY_FOR_CLEAR_LOW_ADDR
    lowlev_cycleDelay(CYCLES_DELAY_FOR_CLEAR_LOW_ADDR);
#endif
    br_mux_clear();
    // Clock the required value in - requires one more count than
    // expected as the output register is one clock pulse behind the counter
    for (uint32_t i = 0; i < (lowAddrByte & 0xff) + 1; i++) {
        WR32(GPSET0, 1 << BR_LADDR_CK);
#ifdef CYCLES_DELAY_FOR_LOW_ADDR_SET
        lowlev_cycleDelay(CYCLES_DELAY_FOR_LOW_ADDR_SET);
#endif
        WR32(GPCLR0, 1 << BR_LADDR_CK);
#ifdef CYCLES_DELAY_FOR_LOW_ADDR_SET
        lowlev_cycleDelay(CYCLES_DELAY_FOR_LOW_ADDR_SET);
#endif
    }
}

// Increment low address value by clocking the counter
// Assumptions:
// - some other code will set push-addr to enable onto the address bus
void br_inc_low_addr()
{
    WR32(GPSET0, 1 << BR_LADDR_CK);
#ifdef CYCLES_DELAY_FOR_LOW_ADDR_SET
        lowlev_cycleDelay(CYCLES_DELAY_FOR_LOW_ADDR_SET);
#endif
    WR32(GPCLR0, 1 << BR_LADDR_CK);
#ifdef CYCLES_DELAY_FOR_LOW_ADDR_SET
        lowlev_cycleDelay(CYCLES_DELAY_FOR_LOW_ADDR_SET);
#endif
}

// Set the high address value
// Assumptions:
// - some other code will set push-addr to enable onto the address bus
void br_set_high_addr(uint32_t highAddrByte)
{
    // Shift the value into the register
    // Takes one more shift than expected as output reg is one pulse behind shift
    for (uint32_t i = 0; i < 9; i++) {
        // Set or clear serial pin to shift register
        if (highAddrByte & 0x80)
            br_mux_set(BR_MUX_HADDR_SER_HIGH);
        else
            br_mux_set(BR_MUX_HADDR_SER_LOW);
        // Delay to allow settling
#ifdef CYCLES_DELAY_FOR_HIGH_ADDR_SET
        lowlev_cycleDelay(CYCLES_DELAY_FOR_HIGH_ADDR_SET);
#endif
        // Shift the address value for next bit
        highAddrByte = highAddrByte << 1;
        // Clock the bit
        WR32(GPSET0, 1 << BR_HADDR_CK);
#ifdef CYCLES_DELAY_FOR_HIGH_ADDR_SET
        lowlev_cycleDelay(CYCLES_DELAY_FOR_HIGH_ADDR_SET);
#endif
        WR32(GPCLR0, 1 << BR_HADDR_CK);
    }
#ifdef CYCLES_DELAY_FOR_HIGH_ADDR_SET
    lowlev_cycleDelay(CYCLES_DELAY_FOR_HIGH_ADDR_SET);
#endif
    // Clear multiplexer
    br_mux_clear();
}

// Set the full address
// Assumptions:
// - some other code will set push-addr to enable onto the address bus
void br_set_full_addr(unsigned int addr)
{
    br_set_high_addr(addr >> 8);
    br_set_low_addr(addr & 0xff);
}

// Set the PIB (pins used for data bus access) to outputs (from Pi)
void br_set_pib_output()
{
#ifdef USE_BITWISE_DATA_BUS_ACCESS
    for (uint32_t i = 0; i < 8; i++) {
        pinMode(BR_DATA_BUS + i, OUTPUT);
    }
#else
    WR32(BR_PIB_GPF_REG, (RD32(BR_PIB_GPF_REG) & BR_PIB_GPF_MASK) | BR_PIB_GPF_OUTPUT);
#endif
}

// Set the PIB (pins used for data bus access) to inputs (to Pi)
void br_set_pib_input()
{
#ifdef USE_BITWISE_DATA_BUS_ACCESS
    for (uint32_t i = 0; i < 8; i++) {
        pinMode(BR_DATA_BUS + i, INPUT);
    }
#else
    WR32(BR_PIB_GPF_REG, (RD32(BR_PIB_GPF_REG) & BR_PIB_GPF_MASK) | BR_PIB_GPF_INPUT);
#endif
}

// Set a value onto the PIB (pins used for data bus access)
void br_set_pib_value(uint8_t val)
{
#ifdef USE_BITWISE_DATA_BUS_ACCESS
    // uint32_t va0 = RD32(GPLEV0);
    // uint32_t va1 = (RD32(GPLEV0) & BR_PIB_MASK) | (((uint32_t)val) << BR_DATA_BUS);
    for (uint32_t i = 0; i < 8; i++) {
        digitalWrite(BR_DATA_BUS + i, val & 0x01);
        val = val >> 1;
    }
#else
    uint32_t setBits = ((uint32_t)val) << BR_DATA_BUS;
    uint32_t clrBits = (~(((uint32_t)val) << BR_DATA_BUS)) & (~BR_PIB_MASK);
    WR32(GPSET0, setBits);
    WR32(GPCLR0, clrBits);
#endif
}

// Get a value from the PIB (pins used for data bus access)
uint8_t br_get_pib_value()
{
#ifdef USE_BITWISE_DATA_BUS_ACCESS
    uint8_t val = 0;
    for (uint32_t i = 0; i < 8; i++) {
        int bitVal = digitalRead(BR_DATA_BUS + i);
        // ee_printf("%d ", bitVal);
        val = val >> 1;
        val = val | ((bitVal != 0) ? 0x80 : 0);
    }
    // ee_printf("\n");
    return val;
#else
    return (RD32(GPLEV0) >> BR_DATA_BUS) & 0xff;
#endif
}

// Write a single byte to currently set address (or IO port)
// Assumes:
// - control of host bus has been requested and acknowledged
// - address bus is already set and output enabled to host bus
// - PIB is already set to output
void br_write_byte(uint32_t byte, int iorq)
{
    // Set the data onto the PIB
    br_set_pib_value(byte);
    // Perform the write
#ifdef USE_BITWISE_CTRL_BUS_ACCESS
    digitalWrite(BR_DATA_DIR_IN, 0);
    br_mux_set(BR_MUX_DATA_OE_BAR_LOW);
    digitalWrite(iorq ? BR_IORQ_BAR : BR_MREQ_BAR, 0);
    digitalWrite(BR_WR_BAR, 0);
    digitalWrite(BR_WR_BAR, 1);
    digitalWrite(iorq ? BR_IORQ_BAR : BR_MREQ_BAR, 1);
    br_mux_clear();
    digitalWrite(BR_DATA_DIR_IN, 1);
#else
    // Clear DIR_IN (so make direction out), enable data output onto data bus and MREQ_BAR active
    WR32(GPCLR0, (1 << BR_DATA_DIR_IN) | BR_MUX_CTRL_BIT_MASK | (1 << (iorq ? BR_IORQ_BAR : BR_MREQ_BAR)));
    WR32(GPSET0, BR_MUX_DATA_OE_BAR_LOW << BR_MUX_LOW_BIT_POS);
    // Write the data by setting WR_BAR active
    WR32(GPCLR0, (1 << BR_WR_BAR));
#ifdef CYCLES_DELAY_FOR_WRITE_TO_TARGET
    // Target write delay
    lowlev_cycleDelay(CYCLES_DELAY_FOR_WRITE_TO_TARGET);
#endif
    // Deactivate and leave data direction set to inwards
    WR32(GPSET0, (1 << BR_DATA_DIR_IN) | (1 << (iorq ? BR_IORQ_BAR : BR_MREQ_BAR)) | (1 << BR_WR_BAR));
    br_mux_clear();
#endif
}

// Read a single byte from currently set address (or IO port)
// Assumes:
// - control of host bus has been requested and acknowledged
// - address bus is already set and output enabled to host bus
// - PIB is already set to input
// - data direction on data bus driver is set to input (default)
uint8_t br_read_byte(int iorq)
{
    // Read the byte
#ifdef USE_BITWISE_CTRL_BUS_ACCESS
    digitalWrite(BR_DATA_DIR_IN, 1);
    br_mux_set(BR_MUX_DATA_OE_BAR_LOW);
    digitalWrite((iorq ? BR_IORQ_BAR : BR_MREQ_BAR), 0);
    digitalWrite(BR_RD_BAR, 0);
    uint8_t val = br_get_pib_value();
    digitalWrite(BR_RD_BAR, 1);
    digitalWrite((iorq ? BR_IORQ_BAR : BR_MREQ_BAR), 1);
    br_mux_clear();
#else
    // enable data output onto PIB (data-dir must be inwards already), MREQ_BAR and RD_BAR both active
    WR32(GPCLR0, BR_MUX_CTRL_BIT_MASK | (1 << (iorq ? BR_IORQ_BAR : BR_MREQ_BAR)) | (1 << BR_RD_BAR));
    WR32(GPSET0, BR_MUX_DATA_OE_BAR_LOW << BR_MUX_LOW_BIT_POS);
    // Delay to allow data to settle
#ifdef CYCLES_DELAY_FOR_READ_FROM_TARGET
    lowlev_cycleDelay(CYCLES_DELAY_FOR_READ_FROM_TARGET);
#endif
    // Get the data
    uint8_t val = (RD32(GPLEV0) >> BR_DATA_BUS) & 0xff;
    // Deactivate leaving data-dir inwards
    WR32(GPSET0, (1 << (iorq ? BR_IORQ_BAR : BR_MREQ_BAR)) | (1 << BR_RD_BAR));
    // Clear the MUX to a safe setting - sets HADDR_SER low
    WR32(GPCLR0, BR_MUX_CTRL_BIT_MASK);
#endif
    return val;
}

// Write a consecutive block of memory to host
BR_RETURN_TYPE br_write_block(uint32_t addr, uint8_t* pData, uint32_t len, int busRqAndRelease, int iorq)
{
    // Check if we need to request bus
    if (busRqAndRelease) {
        // Request bus and take control after ack
        BR_RETURN_TYPE ret = br_req_and_take_bus();
        if (ret != BR_OK)
            return ret;
    }

    // Set PIB to input
    br_set_pib_input();

    // Set the address to initial value
    br_set_full_addr(addr);

    // Set the PIB to output
    br_set_pib_output();

    // Iterate data
    for (uint32_t i = 0; i < len; i++) {
        // Write byte
        br_write_byte(*pData, iorq);

        // Increment the lower address counter
        br_inc_low_addr();

        // Increment addresses
        pData++;
        addr++;

        // Check if we've rolled over the lowest 8 bits
        if ((addr & 0xff) == 0) {
            // Set the address again
            br_set_full_addr(addr);
        }
    }

    // Set the PIB back to INPUT
    br_set_pib_input();

    // Check if we need to release bus
    if (busRqAndRelease) {
        // release bus
        br_release_control(false);
    }
    return BR_OK;
}

// Read a consecutive block of memory from host
// Assumes:
// - control of host bus has been requested and acknowledged
// - data direction on data bus driver is set to input (default)
BR_RETURN_TYPE br_read_block(uint32_t addr, uint8_t* pData, uint32_t len, int busRqAndRelease, int iorq)
{
    // Check if we need to request bus
    if (busRqAndRelease) {
        // Request bus and take control after ack
        BR_RETURN_TYPE ret = br_req_and_take_bus();
        if (ret != BR_OK)
            return ret;
    }

    // Set PIB to input
    br_set_pib_input();

#ifdef USE_PI_SPI0_CE0_AS_DEBUG_PIN
    digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
#endif
    microsDelay(20);

    // Set the address to initial value
    br_set_full_addr(addr);

    microsDelay(20);
#ifdef USE_PI_SPI0_CE0_AS_DEBUG_PIN
    digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
#endif

    // Iterate data
    for (uint32_t i = 0; i < len; i++) {
        // Read byte
        *pData = br_read_byte(iorq);

        // Increment the lower address counter
        br_inc_low_addr();

        // Increment addresses
        pData++;
        addr++;

        // Check if we've rolled over the lowest 8 bits
        if ((addr & 0xff) == 0) {
            // Set the address again
            br_set_full_addr(addr);
        }
    }

    // Check if we need to release bus
    if (busRqAndRelease) {
        // release bus
        br_release_control(false);
    }
    return BR_OK;
}

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
    WR32(GPCLR0, BR_MREQ_WAIT_EN_MASK | BR_IORQ_WAIT_EN_MASK);

    // Check if any wait-states to be generated
    if (__br_wait_state_en_mask == 0)
    {
        __br_wait_interrupt_enabled = false;
        return;
    }

#ifdef BR_ENABLE_WAIT_AND_FIQ

    // Set wait-state generation
    WR32(GPSET0, __br_wait_state_en_mask);

    // Set vector for WAIT state interrupt
    irq_set_wait_state_handler(br_wait_state_isr);

    // Setup edge triggering on falling edge of IORQ
    // Clear any current detected edges
    WR32(GPEDS0, (1 << BR_IORQ_BAR) | (1 << BR_MREQ_BAR));  
    // Set falling edge detect
    if (enWaitOnIORQ)
        WR32(GPFEN0, RD32(GPFEN0) | (1 << BR_IORQ_BAR));
    else
        WR32(GPFEN0, RD32(GPFEN0) & (~(1 << BR_IORQ_BAR)));
    if (enWaitOnMREQ)
        WR32(GPFEN0, RD32(GPFEN0) | (1 << BR_WAIT_BAR));
    else
        WR32(GPFEN0, RD32(GPFEN0) & (~(1 << BR_WAIT_BAR)));

    // Enable FIQ interrupts on GPIO[3] which is any GPIO pin
    WR32(IRQ_FIQ_CONTROL, (1 << 7) | 52);

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
        busVals = RD32(GPLEV0);

        // Specifically need to wait if MREQ is low but both RD and WR are high (wait for WR to go low in this case)
        ctrlValid = !(((busVals & (1 << BR_MREQ_BAR)) == 0) && (((busVals & (1 << BR_RD_BAR)) != 0) && ((busVals & (1 << BR_WR_BAR)) != 0)));
        if (ctrlValid)
            break;

        // Count loops
        avoidLockupCtr++;

    // TODO - delay??

    }

    // Check if we have valid control lines
    if ((!ctrlValid) || ((busVals & (1 << BR_WAIT_BAR)) != 0))
    {

            // TODO - delay??

        // digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);

        // Spurious ISR?
        WR32(GPCLR0, BR_MREQ_WAIT_EN_MASK | BR_IORQ_WAIT_EN_MASK);

    // TODO - delay??

        // Re-enable the wait state generation
        WR32(GPSET0, __br_wait_state_en_mask);
        // Clear detected edge on any pin
        WR32(GPEDS0, 0xffffffff);

                // digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);

        return;
    }

    // TODO - delay??


    // Read the low address
    br_set_pib_input();
    WR32(GPSET0, 1 << BR_DATA_DIR_IN);
    br_mux_set(BR_MUX_LADDR_OE_BAR);

    // TODO - delay??

    uint32_t addr = br_get_pib_value() & 0xff;

    // Read the high address
    br_mux_set(BR_MUX_HADDR_OE_BAR);

    addr |= (br_get_pib_value() & 0xff) << 8;
 
    // Clear the mux to deactivate output enables
    br_mux_clear();

    // Get the appropriate bits for up-line communication
    uint32_t ctrlBusVals = 
        (((busVals & (1 << BR_RD_BAR)) == 0) ? BR_CTRL_BUS_RD_MASK : 0) |
        (((busVals & (1 << BR_WR_BAR)) == 0) ? BR_CTRL_BUS_WR_MASK : 0) |
        (((busVals & (1 << BR_MREQ_BAR)) == 0) ? BR_CTRL_BUS_MREQ_MASK : 0) |
        (((busVals & (1 << BR_IORQ_BAR)) == 0) ? BR_CTRL_BUS_IORQ_MASK : 0) |
        (((busVals & (1 << BR_WAIT_BAR)) == 0) ? BR_CTRL_BUS_WAIT_MASK : 0) |
        (((RD32(GPLEV0) & (1 << BR_M1_PIB_BAR)) == 0) ? BR_CTRL_BUS_M1_MASK : 0);

    // Read the data bus if the target machine is writing
    uint32_t dataBusVals = 0;
    bool isWriting = (busVals & (1 << BR_WR_BAR)) == 0;
    if (isWriting)
    {
        digitalWrite(BR_DATA_DIR_IN, 1);
        br_mux_set(BR_MUX_DATA_OE_BAR_LOW);
        dataBusVals = br_get_pib_value() & 0xff;
        br_mux_clear();
    }

    // Send this to anything listening
    uint32_t retVal = BR_MEM_ACCESS_RSLT_NOT_DECODED;
    if (__br_pBusAccessCallback)
        retVal = __br_pBusAccessCallback(addr, dataBusVals, ctrlBusVals);

    // If not writing and result is valid then put the returned data onto the bus
    if (!isWriting && ((retVal & BR_MEM_ACCESS_RSLT_NOT_DECODED) == 0))
    {
        digitalWrite(BR_DATA_DIR_IN, 0);
        // In HW version 1.7 onwards there is a flip-flop to handle data OE
        // so prime this flip-flop here
        br_mux_set(BR_MUX_DATA_OE_BAR_LOW);
        br_set_pib_output();
        br_set_pib_value(retVal & 0xff);
        br_mux_clear();
    }

    // digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);

    // Check if pause requested
    __br_pause_is_paused = ((retVal & BR_MEM_ACCESS_RSLT_REQ_PAUSE) != 0);
    if (!__br_pause_is_paused)
    {
        // TODO - DEBUG - MUST BE REMOVED
        // WR32(GPSET0, 1 << BR_HADDR_CK);
        // WR32(GPSET0, 1 << BR_HADDR_CK);
        // WR32(GPCLR0, 1 << BR_HADDR_CK);

        // No pause requested - clear the WAIT state so execution can continue
        // The order of the following lines is important
        // The repeated line delays the flip-flop reset pulse enough
        // to make it reliable - without this extra delay clearing the flip-flop isn't
        // reliable as the pulse can be too short (Raspberry Pi outputs are weedy)
        // Clear the WAIT state flip-flop
        WR32(GPCLR0, BR_MREQ_WAIT_EN_MASK | BR_IORQ_WAIT_EN_MASK);
        WR32(GPCLR0, BR_MREQ_WAIT_EN_MASK | BR_IORQ_WAIT_EN_MASK);
        WR32(GPCLR0, BR_MREQ_WAIT_EN_MASK | BR_IORQ_WAIT_EN_MASK);
        // Delay for a short time

    // TODO - delay??

        // // Re-enable the wait state generation
        WR32(GPSET0, __br_wait_state_en_mask);

        // // Clear detected edge on any pin
        WR32(GPEDS0, 0xffffffff);
        
        // TODO - DEBUG - MUST BE REMOVED
        // WR32(GPSET0, 1 << BR_HADDR_CK);
        // WR32(GPSET0, 1 << BR_HADDR_CK);
        // WR32(GPCLR0, 1 << BR_HADDR_CK);
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
    WR32(GPEDS0, 0xffffffff);
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
    WR32(GPCLR0, BR_MREQ_WAIT_EN_MASK | BR_IORQ_WAIT_EN_MASK);

    // Delay for a short time
    microsDelay(2);

        // TODO - delay??


    // Re-enable the wait state generation
    WR32(GPSET0, __br_wait_state_en_mask);

    // Clear detected edge on any pin
    WR32(GPEDS0, 0xffffffff);

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
