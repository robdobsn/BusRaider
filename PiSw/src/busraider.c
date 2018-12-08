// Bus Raider
// Rob Dobson 2018

#include "busraider.h"
#include "piwiring.h"
#include "timer.h"
#include "irq.h"
#include "utils.h"
#include "ee_printf.h"

// Uncomment the following to use bitwise access to busses and pins on Pi
// #define USE_BITWISE_DATA_BUS_ACCESS 1
// #define USE_BITWISE_CTRL_BUS_ACCESS 1

// Uncomment the following line to use SPI0 CE0 of the Pi as a debug pin
#define USE_PI_SPI0_CE0_AS_DEBUG_PIN 1

// Comment out this line to disable WAIT state generation altogether
#define BR_ENABLE_WAIT_AND_FIQ 1

// Hardware versions
// #define HARDWARE_VERSION_1_6 1

// Instrument FIQ
// #define INSTRUMENT_BUSRAIDER_FIQ 1

// Callback on bus access
static br_bus_access_callback_fntype* __br_pBusAccessCallback = NULL;

// Current wait state mask for restoring wait enablement after change
static uint32_t __br_wait_state_en_mask = 0;

// Wait interrupt enablement cache (so it can be restored after disable)
static bool __br_wait_interrupt_enabled = false;

// Single stepping modes
bool __br_single_step_any = false;
bool __br_single_step_io = false;
bool __br_single_step_instructions = false;
bool __br_single_step_mem_rdwr = false;

// Single step is in progress - i.e. wait is active
volatile bool __br_single_step_in_progress = false;

// Bus values while single stepping
uint32_t __br_single_step_addr = 0;
uint32_t __br_single_step_data = 0;
uint32_t __br_single_step_flags = 0;

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
    W32(GPCLR0, BR_MUX_CTRL_BIT_MASK);
}

void br_mux_set(int muxVal)
{
    // Clear first as this is a safe setting - sets HADDR_SER low
    W32(GPCLR0, BR_MUX_CTRL_BIT_MASK);
    // Now set bits required
    W32(GPSET0, muxVal << BR_MUX_LOW_BIT_POS);
}

// Initialise the bus raider
void br_init()
{
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

#ifdef USE_PI_SPI0_CE0_AS_DEBUG_PIN
    br_set_pin_out(BR_DEBUG_PI_SPI0_CE0, 0);
#endif

    // No wait interrupts until enabled
    __br_wait_interrupt_enabled = false;
}

// Reset the host
void br_reset_host()
{
    // Disable wait interrupts
    br_disable_wait_interrupt();

    // Reset by taking reset_bar low and then high
    br_mux_set(BR_MUX_RESET_Z80_BAR_LOW);

    // Delay for a short time
    uint32_t timeNow = micros();
    while (!timer_isTimeout(micros(), timeNow, 100)) {
        // Do nothing
    }

    // Clear wait interrupt conditions and enable
    br_clear_wait_interrupt();
    if (__br_wait_interrupt_enabled)
        br_enable_wait_interrupt();

    // Remove the reset condition
    br_mux_clear();
}

// Non-maskable interrupt the host
void br_nmi_host()
{
    // NMI by taking nmi_bar line low and high
    br_mux_set(BR_MUX_NMI_BAR_LOW);
    delayMicroseconds(10);
    br_mux_clear();
}

// Maskable interrupt the host
void br_irq_host()
{
    // IRQ by taking irq_bar line low and high
    br_mux_set(BR_MUX_IRQ_BAR_LOW);
    delayMicroseconds(10);
    br_mux_clear();
}

// Request access to the bus
void br_request_bus()
{
    digitalWrite(BR_BUSRQ_BAR, 0);
}

// Check if bus request has been acknowledged
int br_bus_acknowledged()
{
    return (R32(GPLEV0) & (1 << BR_BUSACK_BAR)) == 0;
}

// Take control of bus
void br_take_control()
{
    // control bus
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
    // Check for reset
    if (resetTargetOnRelease)
    {
        // Reset by taking reset_bar low and then high
        br_mux_set(BR_MUX_RESET_Z80_BAR_LOW);
        // No longer request bus
        digitalWrite(BR_BUSRQ_BAR, 1);
        delayMicroseconds(1000);
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
    // Clear initially
    br_mux_set(BR_MUX_LADDR_CLR_BAR_LOW);
    br_mux_clear();
    // Clock the required value in - requires one more count than
    // expected as the output register is one clock pulse behind the counter
    for (uint32_t i = 0; i < (lowAddrByte & 0xff) + 1; i++) {
        W32(GPSET0, 1 << BR_LADDR_CK);
        W32(GPCLR0, 1 << BR_LADDR_CK);
    }
}

// Increment low address value by clocking the counter
// Assumptions:
// - some other code will set push-addr to enable onto the address bus
void br_inc_low_addr()
{
    W32(GPSET0, 1 << BR_LADDR_CK);
    W32(GPCLR0, 1 << BR_LADDR_CK);
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
        // Shift the address value for next bit
        highAddrByte = highAddrByte << 1;
        // Clock the bit
        W32(GPSET0, 1 << BR_HADDR_CK);
        W32(GPCLR0, 1 << BR_HADDR_CK);
    }
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
    W32(BR_PIB_GPF_REG, (R32(BR_PIB_GPF_REG) & BR_PIB_GPF_MASK) | BR_PIB_GPF_OUTPUT);
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
    W32(BR_PIB_GPF_REG, (R32(BR_PIB_GPF_REG) & BR_PIB_GPF_MASK) | BR_PIB_GPF_INPUT);
#endif
}

// Set a value onto the PIB (pins used for data bus access)
void br_set_pib_value(uint8_t val)
{
#ifdef USE_BITWISE_DATA_BUS_ACCESS
    // uint32_t va0 = R32(GPLEV0);
    // uint32_t va1 = (R32(GPLEV0) & BR_PIB_MASK) | (((uint32_t)val) << BR_DATA_BUS);
    for (uint32_t i = 0; i < 8; i++) {
        digitalWrite(BR_DATA_BUS + i, val & 0x01);
        val = val >> 1;
    }
#else
    uint32_t setBits = ((uint32_t)val) << BR_DATA_BUS;
    uint32_t clrBits = (~(((uint32_t)val) << BR_DATA_BUS)) & (~BR_PIB_MASK);
    W32(GPSET0, setBits);
    W32(GPCLR0, clrBits);
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
    uint32_t busVals = R32(GPLEV0);
    return (busVals >> BR_DATA_BUS) & 0xff;
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
#ifndef HARDWARE_VERSION_1_6
    br_mux_clear();
#endif
    digitalWrite(iorq ? BR_IORQ_BAR : BR_MREQ_BAR, 0);
    digitalWrite(BR_WR_BAR, 0);
    digitalWrite(BR_WR_BAR, 1);
    digitalWrite(iorq ? BR_IORQ_BAR : BR_MREQ_BAR, 1);
#ifdef HARDWARE_VERSION_1_6
    br_mux_clear();
#endif
    digitalWrite(BR_DATA_DIR_IN, 1);
#else
    // Clear DIR_IN (so make direction out), enable data output onto data bus and MREQ_BAR active
    W32(GPCLR0, (1 << BR_DATA_DIR_IN) | BR_MUX_CTRL_BIT_MASK | (1 << (iorq ? BR_IORQ_BAR : BR_MREQ_BAR)));
    W32(GPSET0, BR_MUX_DATA_OE_BAR_LOW << BR_MUX_LOW_BIT_POS);
#ifndef HARDWARE_VERSION_1_6
    W32(GPCLR0, BR_MUX_CTRL_BIT_MASK);
#endif
    // Write the data by setting WR_BAR active
    W32(GPCLR0, (1 << BR_WR_BAR));
    // Deactivate and leave data direction set to inwards
    W32(GPSET0, (1 << BR_DATA_DIR_IN) | (1 << (iorq ? BR_IORQ_BAR : BR_MREQ_BAR)) | (1 << BR_WR_BAR));
    // Clear the MUX
#ifdef HARDWARE_VERSION_1_6
    W32(GPCLR0, BR_MUX_CTRL_BIT_MASK);
#endif
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
#ifndef HARDWARE_VERSION_1_6
    br_mux_clear();
#endif
    digitalWrite((iorq ? BR_IORQ_BAR : BR_MREQ_BAR), 0);
    digitalWrite(BR_RD_BAR, 0);
    uint8_t val = br_get_pib_value();
    digitalWrite(BR_RD_BAR, 1);
    digitalWrite((iorq ? BR_IORQ_BAR : BR_MREQ_BAR), 1);
#ifdef HARDWARE_VERSION_1_6
    br_mux_clear();
#endif
#else
    // enable data output onto PIB (data-dir must be inwards already), MREQ_BAR and RD_BAR both active
    W32(GPCLR0, BR_MUX_CTRL_BIT_MASK | (1 << (iorq ? BR_IORQ_BAR : BR_MREQ_BAR)) | (1 << BR_RD_BAR));
    W32(GPSET0, BR_MUX_DATA_OE_BAR_LOW << BR_MUX_LOW_BIT_POS);
#ifndef HARDWARE_VERSION_1_6
    W32(GPCLR0, BR_MUX_CTRL_BIT_MASK);
#endif
    // Get the data
    uint8_t val = br_get_pib_value();
    // Deactivate leaving data-dir inwards
    W32(GPSET0, (1 << (iorq ? BR_IORQ_BAR : BR_MREQ_BAR)) | (1 << BR_RD_BAR));
    // Clear the MUX
#ifdef HARDWARE_VERSION_1_6
    W32(GPCLR0, BR_MUX_CTRL_BIT_MASK);
#endif
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

    // Set the PIB to output
    br_set_pib_output();

    // Set the address to initial value
    br_set_full_addr(addr);

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

    // Set the address to initial value
    br_set_full_addr(addr);

    // Set PIB to input
    br_set_pib_input();

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
void br_enable_mem_and_io_access(bool enWaitOnIORQ, bool enWaitOnMREQ,
           bool singleStepIO, bool singleStepInstructions, bool singleStepMemRDWR)
{
    // Disable interrupts
    disable_fiq();

    // Store single step flags
    __br_single_step_io = singleStepIO;
    __br_single_step_instructions = singleStepInstructions;
    __br_single_step_mem_rdwr = singleStepMemRDWR;
    __br_single_step_any = __br_single_step_io | __br_single_step_instructions | __br_single_step_mem_rdwr;

#ifdef INSTRUMENT_BUSRAIDER_FIQ
    for (int i = 0; i < MAX_IO_PORT_VALS; i++)
    {
        iorqPortAccess[i] = 0;
    }
#endif

    // Enable wait state generation
    __br_wait_state_en_mask = 0;
    if (enWaitOnIORQ)
        __br_wait_state_en_mask = __br_wait_state_en_mask | (1 << BR_IORQ_WAIT_EN);
    if (enWaitOnMREQ)
        __br_wait_state_en_mask = __br_wait_state_en_mask | (1 << BR_MREQ_WAIT_EN);
    W32(GPCLR0, (1 << BR_MREQ_WAIT_EN) | (1 << BR_IORQ_WAIT_EN));

    // Check if any wait-states to be generated
    if (__br_wait_state_en_mask == 0)
    {
        __br_wait_interrupt_enabled = false;
        return;
    }

#ifdef BR_ENABLE_WAIT_AND_FIQ

    // Set wait-state generation
    W32(GPSET0, __br_wait_state_en_mask);

    // Set vector for WAIT state interrupt
    irq_set_wait_state_handler(br_wait_state_isr);

    // Setup edge triggering on falling edge of IORQ
    // Clear any current detected edges
    W32(GPEDS0, (1 << BR_IORQ_BAR) | (1 << BR_MREQ_BAR));  
    // Set falling edge detect
    if (enWaitOnIORQ)
        W32(GPFEN0, R32(GPFEN0) | (1 << BR_IORQ_BAR));
    else
        W32(GPFEN0, R32(GPFEN0) & (~(1 << BR_IORQ_BAR)));
    if (enWaitOnMREQ)
        W32(GPFEN0, R32(GPFEN0) | (1 << BR_MREQ_BAR));
    else
        W32(GPFEN0, R32(GPFEN0) & (~(1 << BR_MREQ_BAR)));

    // Enable FIQ interrupts on GPIO[3] which is any GPIO pin
    W32(IRQ_FIQ_CONTROL, (1 << 7) | 52);

    // Enable Fast Interrupts
    __br_wait_interrupt_enabled = true;
    enable_fiq();

#endif  
}

void br_wait_state_isr(void* pData)
{
    // pData maybe unused
    (void) pData;

    #ifdef USE_PI_SPI0_CE0_AS_DEBUG_PIN
        W32(GPSET0, 1 << BR_DEBUG_PI_SPI0_CE0);
    #endif

    // // Clear detected edge on any pin
    // W32(GPEDS0, 0xffffffff);

    // // Clear the WAIT state flip-flop
    // W32(GPCLR0, (1 << BR_MREQ_WAIT_EN) | (1 << BR_IORQ_WAIT_EN));

    // // Re-enable the wait state generation
    // W32(GPSET0, __br_wait_state_en_mask);

    // #ifdef USE_PI_SPI0_CE0_AS_DEBUG_PIN
    //     W32(GPCLR0, 1 << BR_DEBUG_PI_SPI0_CE0);
    // #endif

    // return;

    // delayMicroseconds(5);

    // Read the low address
    br_set_pib_input();
    br_mux_set(BR_MUX_LADDR_OE_BAR);
    uint32_t addr = br_get_pib_value() & 0xff;
    // Read the high address
    br_mux_set(BR_MUX_HADDR_OE_BAR);
    addr |= (br_get_pib_value() & 0xff) << 8;
    // Clear the mux to deactivate output enables
    br_mux_clear();

    // Read the control lines
    uint32_t busVals = R32(GPLEV0);

    // There should be a read or write active or there should be M1 and IORQ active 
    // to signal interrupt ack - if not wait some time until there is
    bool isRefreshCycle = false;
    for (int i = 0; i < 100; i++)
    {
        // (MREQ || IORQ) && (RD || WR)
        if ( ( (((busVals & (1 << BR_MREQ_BAR)) == 0) || ((busVals & (1 << BR_IORQ_BAR)) == 0)) &&
                (((busVals & (1 << BR_RD_BAR)) == 0) || ((busVals & (1 << BR_WR_BAR)) == 0))  ) )
            break;
        // IORQ && M1
        if ((((busVals & (1 << BR_IORQ_BAR)) == 0) || ((busVals & (1 << BR_M1_BAR)) == 0)))
            break;
        // Refresh
        if (((busVals & (1 << BR_MREQ_BAR)) != 0) && ((busVals & (1 << BR_IORQ_BAR)) != 0))
        {
            isRefreshCycle = true;
            break;
        }
        // Re-read
        busVals = R32(GPLEV0);
    }

    // Exit if this is a refresh cycle
    if (isRefreshCycle)
    {
        // Clear detected edge on any pin
        W32(GPEDS0, 0xffffffff);

        // Clear the WAIT state flip-flop
        W32(GPCLR0, (1 << BR_MREQ_WAIT_EN) | (1 << BR_IORQ_WAIT_EN));

        // Re-enable the wait state generation
        W32(GPSET0, __br_wait_state_en_mask);
    }
    else
    {
        // Get the appropriate bits for up-line communication
        uint32_t ctrlBusVals = 
            (((busVals & (1 << BR_RD_BAR)) == 0) ? (1 << BR_CTRL_BUS_RD) : 0) |
            (((busVals & (1 << BR_WR_BAR)) == 0) ? (1 << BR_CTRL_BUS_WR) : 0) |
            (((busVals & (1 << BR_MREQ_BAR)) == 0) ? (1 << BR_CTRL_BUS_MREQ) : 0) |
            (((busVals & (1 << BR_IORQ_BAR)) == 0) ? (1 << BR_CTRL_BUS_IORQ) : 0) |
            (((busVals & (1 << BR_M1_BAR)) == 0) ? (1 << BR_CTRL_BUS_M1) : 0);

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
        if (!isWriting && (retVal != BR_MEM_ACCESS_RSLT_NOT_DECODED))
        {
            digitalWrite(BR_DATA_DIR_IN, 0);
            br_mux_set(BR_MUX_DATA_OE_BAR_LOW);
#ifndef HARDWARE_VERSION_1_6
            // In HW version 1.7 onwards there is a flip-flop to handle data OE
            br_mux_clear();
#endif
            br_set_pib_output();
            br_set_pib_value(retVal & 0xff);
        }

    #ifdef INSTRUMENT_BUSRAIDER_FIQ
        // Form key from the control lines
        uint32_t accVal = 0x00;
        if ((busVals & (1 << BR_RD_BAR)) == 0)
            accVal |= 0x01;
        if ((busVals & (1 << BR_WR_BAR)) == 0)
            accVal |= 0x02;
        if (accVal == 0)
            accVal = 0x03;

        // Form key
        uint32_t keyVal = (accVal << 16) | addr; 
        int firstUnused = -1;
        for (int i = 0; i < MAX_IO_PORT_VALS; i++)
        {
            // Check for key
            if ((iorqPortAccess[i] & 0x3ffff) == keyVal)
            {
                // Inc count if we can
                uint32_t count = iorqPortAccess[i] >> 18;
                if (count != 0x3fff)
                    count++;
                iorqPortAccess[i] = (iorqPortAccess[i] & 0x3ffff) | (count << 18);
                break;
            }
            else if (iorqPortAccess[i] == 0)
            {
                firstUnused = i;
                break;
            }
        }
        if (firstUnused != -1)
        {
            iorqPortAccess[firstUnused] = keyVal | (1 << 18);
        }

        if (busVals & (iorqIsNotActive != 0))
            iorqIsNotActive++;
    #endif

        // Check if single-stepping
        if (!__br_single_step_any || 
            (!(__br_single_step_io && ((busVals & (1 << BR_IORQ_BAR)) == 0) && ((busVals & (1 << BR_M1_BAR)) != 0))) ||
            (!(__br_single_step_instructions && ((busVals & (1 << BR_MREQ_BAR)) == 0) && ((busVals & (1 << BR_M1_BAR)) == 0))) ||
            (!(__br_single_step_mem_rdwr && ((busVals & (1 << BR_MREQ_BAR)) == 0) && ((busVals & (1 << BR_M1_BAR)) != 0))))
        {
            // Clear detected edge on any pin
            W32(GPEDS0, 0xffffffff);

            // Clear the WAIT state flip-flop
            W32(GPCLR0, (1 << BR_MREQ_WAIT_EN) | (1 << BR_IORQ_WAIT_EN));

            // Re-enable the wait state generation
            W32(GPSET0, __br_wait_state_en_mask);

#ifdef HARDWARE_VERSION_1_6            
            // Read the control lines
            if (!isWriting && (retVal != BR_MEM_ACCESS_RSLT_NOT_DECODED))
            {
                // Wait until the request has completed (both IORQ_BAR and MREQ_BAR high)
                uint32_t ctrlBusMask = ((1 << BR_MREQ_BAR) | (1 << BR_IORQ_BAR));
                int hangAvoidCount = 0;
                while ((R32(GPLEV0) & ctrlBusMask) != ctrlBusMask)
                {
                    // Stop waiting if this takes more than 1000 loops
                    // (which will be way longer than the 1uS or less it should take)
                    hangAvoidCount++;
                    if (hangAvoidCount > 1000)
                    {
                        break;
                    }
                }
            }

            // Clear the mux and set data direction in again
            W32(GPCLR0, BR_MUX_CTRL_BIT_MASK);
            W32(GPSET0, 1 << BR_DATA_DIR_IN);
            br_set_pib_input();
#endif
        }
        else
        {
            __br_single_step_in_progress = true;
            __br_single_step_addr = addr;
            __br_single_step_data = isWriting ? retVal : dataBusVals;
            __br_single_step_flags = ctrlBusVals;
        }
    }

    #ifdef USE_PI_SPI0_CE0_AS_DEBUG_PIN
        W32(GPCLR0, 1 << BR_DEBUG_PI_SPI0_CE0);
    #endif
}

void br_clear_wait_interrupt()
{
    // Clear detected edge on any pin
    W32(GPEDS0, 0xffffffff);
}

void br_disable_wait_interrupt()
{
    // Disable Fast Interrupts
    disable_fiq();
}

void br_enable_wait_interrupt()
{
    // Enable Fast Interrupts
    enable_fiq();
}

void br_single_step_get_current(uint32_t* pAddr, uint32_t* pData, uint32_t* pFlags)
{
    *pAddr = __br_single_step_addr;
    *pData = __br_single_step_data;
    *pFlags = __br_single_step_flags;
}

bool br_get_single_step_stopped()
{
    return __br_single_step_in_progress;
}

bool br_single_step_next()
{
    // Check if single stepping
    if (!__br_single_step_in_progress)
        return false;

    // Release wait for one step
    __br_single_step_in_progress = false;

#ifdef HARDWARE_VERSION_1_6            

    // Any read or IRQ vector placed on the data bus happens in here 
    // So need to make sure it is long enough
    // but not too long!
    // The code below that checks for the IORQ & MREQ edges is too slow
    // on an 8MHz Z80 but could be used on slower machines?

    // If not writing and result from memory access request 
    // then we need to wait until the request is complete

    // Read the control lines
    uint32_t busVals = R32(GPLEV0);
    bool isWriting = (busVals & (1 << BR_WR_BAR)) == 0;
    if (!isWriting && (__br_single_step_data != BR_MEM_ACCESS_RSLT_NOT_DECODED))
    {
        // Wait until the request has completed (both IORQ_BAR and MREQ_BAR high)
        uint32_t ctrlBusMask = ((1 << BR_MREQ_BAR) | (1 << BR_IORQ_BAR));
        int hangAvoidCount = 0;
        while ((R32(GPLEV0) & ctrlBusMask) != ctrlBusMask)
        {
            // Stop waiting if this takes more than 1000 loops
            // (which will be way longer than the 1uS or less it should take)
            hangAvoidCount++;
            if (hangAvoidCount > 1000)
            {
                break;
            }
        }
    }
    
    // Clear the mux and set data direction in again
    W32(GPCLR0, BR_MUX_CTRL_BIT_MASK);
    W32(GPSET0, 1 << BR_DATA_DIR_IN);
    br_set_pib_input();
#endif

    // Clear detected edge on any pin
    W32(GPEDS0, 0xffffffff);

    // Clear the WAIT state flip-flop
    W32(GPCLR0, (1 << BR_MREQ_WAIT_EN) | (1 << BR_IORQ_WAIT_EN));

    // Re-enable the wait state generation
    W32(GPSET0, __br_wait_state_en_mask);

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
