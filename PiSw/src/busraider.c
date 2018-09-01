// Bus Raider
// Rob Dobson 2018

#include "busraider.h"
#include "piwiring.h"
#include "timer.h"
#include "irq.h"
#include "utils.h"

// Following pins are jumpered - to allow SPI bus usage if not needed
// Uncomment the following lines to enable these pins
// #define BR_ENABLE_NMI 1
// #define BR_ENABLE_IRQ 1
#define BR_ENABLE_WAIT_STATES 1
// Use Low address read
#define BR_ENABLE_LADDR_READ 1

// Uncomment the following to use bitwise access to busses and pins on Pi
// #define USE_BITWISE_BUS_ACCESS 1

// Callback on bus access
static br_bus_access_callback_fntype* __br_pBusAccessCallback = NULL;

// Set a pin to be an output and set initial value for that pin
void br_set_pin_out(int pin, int val)
{
    digitalWrite(pin, val);
    pinMode(pin, OUTPUT);
    digitalWrite(pin, val);
}

// Initialise the bus raider
void br_init()
{
    // Pins not set here as outputs will be inputs (inc PIB used for data bus)
    // Bus Request
    br_set_pin_out(BR_BUSRQ, 0);
    // RESET Z80
    br_set_pin_out(BR_RESET, 0);
// NMI
#ifdef BR_ENABLE_NMI
    br_set_pin_out(BR_NMI, 0);
#endif
// IRQ
#ifdef BR_ENABLE_IRQ
    br_set_pin_out(BR_IRQ, 0);
#endif
// WAIT
#ifdef BR_ENABLE_WAIT_STATES
    // Clear WAIT initially - leaving LOW disables WAIT generation
    br_set_pin_out(BR_WAIT, 0);
#endif
// Low address read
#ifdef BR_ENABLE_LADDR_READ
    br_set_pin_out(BR_LADDR_OE_BAR, 1);
#endif
    // Address push
    br_set_pin_out(BR_PUSH_ADDR_BAR, 1);
    // High address clock
    br_set_pin_out(BR_HADDR_CK, 0);
    // High address serial
    br_set_pin_out(BR_HADDR_SER, 0);
    // Low address clock
    br_set_pin_out(BR_LADDR_CK, 0);
    // Low address clear
    br_set_pin_out(BR_LADDR_CLR_BAR, 0);
    // Data bus direction
    br_set_pin_out(BR_DATA_DIR_IN, 1);
    // Data bus output enable
    br_set_pin_out(BR_DATA_OE_BAR, 1);
}

// Reset the host
void br_reset_host()
{
    // Reset by taking reset line high and low (it is inverted compared to host line)
    digitalWrite(BR_RESET, 1);
    delayMicroseconds(1000);
    digitalWrite(BR_RESET, 0);
}

// Non-maskable interrupt the host
void br_nmi_host()
{
// NMI by taking nmi line high and low (it is inverted compared to host line)
#ifdef BR_ENABLE_NMI
    digitalWrite(BR_NMI, 1);
    digitalWrite(BR_NMI, 0);
#endif
}

// Maskable interrupt the host
void br_irq_host()
{
// IRQ by taking irq line high and low (it is inverted compared to host line)
#ifdef BR_ENABLE_IRQ
    digitalWrite(BR_IRQ, 1);
    digitalWrite(BR_IRQ, 0);
#endif
}

// Request access to the bus
void br_request_bus()
{
    digitalWrite(BR_BUSRQ, 1);
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
    // Data bus not enabled
    digitalWrite(BR_DATA_OE_BAR, 1);
    // Address bus enabled
    digitalWrite(BR_PUSH_ADDR_BAR, 0);
}

// Release control of bus
void br_release_control()
{
    // Control bus
    pinMode(BR_WR_BAR, INPUT);
    pinMode(BR_RD_BAR, INPUT);
    pinMode(BR_MREQ_BAR, INPUT);
    pinMode(BR_IORQ_BAR, INPUT);
    // Data bus not enabled
    digitalWrite(BR_DATA_OE_BAR, 1);
    // Address bus not enabled
    digitalWrite(BR_PUSH_ADDR_BAR, 1);
    // No longer request bus
    digitalWrite(BR_BUSRQ, 0);
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
        br_release_control();
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
    W32(GPCLR0, 1 << BR_LADDR_CLR_BAR);
    W32(GPSET0, 1 << BR_LADDR_CLR_BAR);
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
            W32(GPSET0, 1 << BR_HADDR_SER);
        else
            W32(GPCLR0, 1 << BR_HADDR_SER);
        // Shift the address value for next bit
        highAddrByte = highAddrByte << 1;
        // Clock the bit
        W32(GPSET0, 1 << BR_HADDR_CK);
        W32(GPCLR0, 1 << BR_HADDR_CK);
    }
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
#ifdef USE_BITWISE_BUS_ACCESS
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
#ifdef USE_BITWISE_BUS_ACCESS
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
#ifdef USE_BITWISE_BUS_ACCESS
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
#ifdef USE_BITWISE_BUS_ACCESS
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
#ifdef USE_BITWISE_BUS_ACCESS
    digitalWrite(BR_DATA_DIR_IN, 0);
    digitalWrite(BR_DATA_OE_BAR, 0);
    digitalWrite(BR_MREQ_BAR, 0);
    digitalWrite(BR_WR_BAR, 0);
    digitalWrite(BR_WR_BAR, 1);
    digitalWrite((iorq ? BR_IORQ_BAR : BR_MREQ_BAR), 1);
    digitalWrite(BR_DATA_OE_BAR, 1);
    digitalWrite(BR_DATA_DIR_IN, 1);
#else
    // Clear DIR_IN (so make direction out), enable data output onto data bus and MREQ_BAR active
    W32(GPCLR0, (1 << BR_DATA_DIR_IN) | (1 << BR_DATA_OE_BAR) | (1 << (iorq ? BR_IORQ_BAR : BR_MREQ_BAR)));
    // Write the data by setting WR_BAR active
    W32(GPCLR0, (1 << BR_WR_BAR));
    // Deactivate and leave data direction set to inwards
    W32(GPSET0, (1 << BR_DATA_DIR_IN) | (1 << BR_DATA_OE_BAR) | (1 << (iorq ? BR_IORQ_BAR : BR_MREQ_BAR)) | (1 << BR_WR_BAR));
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
#ifdef USE_BITWISE_BUS_ACCESS
    digitalWrite(BR_DATA_DIR_IN, 1);
    digitalWrite(BR_DATA_OE_BAR, 0);
    digitalWrite((iorq ? BR_IORQ_BAR : BR_MREQ_BAR), 0);
    digitalWrite(BR_RD_BAR, 0);
    uint8_t val = br_get_pib_value();
    digitalWrite(BR_RD_BAR, 1);
    digitalWrite((iorq ? BR_IORQ_BAR : BR_MREQ_BAR), 1);
    digitalWrite(BR_DATA_OE_BAR, 1);
#else
    // enable data output onto PIB (data-dir must be inwards already), MREQ_BAR and RD_BAR both active
    W32(GPCLR0, (1 << BR_DATA_OE_BAR) | (1 << (iorq ? BR_IORQ_BAR : BR_MREQ_BAR)) | (1 << BR_RD_BAR));
    // Get the data
    uint8_t val = br_get_pib_value();
    // Deactivate leaving data-dir inwards
    W32(GPSET0, (1 << BR_DATA_OE_BAR) | (1 << (iorq ? BR_IORQ_BAR : BR_MREQ_BAR)) | (1 << BR_RD_BAR));
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
            // Set the high address again
            br_set_high_addr(addr >> 8);
        }
    }

    // Set the PIB back to INPUT
    br_set_pib_input();

    // Check if we need to release bus
    if (busRqAndRelease) {
        // release bus
        br_release_control();
    }
    return BR_OK;
}

// Read a consecutive block of memory from host
// Assumes:
// - control of host bus has been requested and acknowledged
// - PIB is already set to input
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
            // Set the high address again
            br_set_high_addr(addr >> 8);
        }
    }

    // Check if we need to release bus
    if (busRqAndRelease) {
        // release bus
        br_release_control();
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

// #define TEST_BUSRAIDER_FIQ 1

#ifdef TEST_BUSRAIDER_FIQ

volatile int iorqPortsRead[256];
volatile int iorqPortsWritten[256];
volatile int iorqPortsOther[256];
volatile int iorqIsNotActive = 0;

// Enable wait-states
void br_enable_wait_states()
{

    for (int i = 0; i < 256; i++)
    {
        iorqPortsRead[i] = 0;
        iorqPortsWritten[i] = 0;
        iorqPortsOther[i] = 0;
    }

#ifdef BR_ENABLE_WAIT_STATES
    // Clear WAIT to stop any wait happening
    digitalWrite(BR_WAIT, 0);

    // Set vector for WAIT state interrupt
    irq_set_wait_state_handler(br_wait_state_isr);

    // Setup edge triggering on falling edge of MREQ
    W32(GPEDS0, 1 << BR_IORQ_BAR);  // Clear any current detected edge
    W32(GPFEN0, 1 << BR_IORQ_BAR);  // Set falling edge detect

    W32(IRQ_FIQ_CONTROL, (1 << 7) | 52);
    enable_fiq();

    // Allow WAIT generation
    digitalWrite(BR_WAIT, 1);
#endif
}

void br_wait_state_isr(void* pData)
{
    pData = pData;
    
    // Read the low address
    digitalWrite(BR_LADDR_OE_BAR, 0);
    uint8_t lowAddr = br_get_pib_value() & 0xff;
    digitalWrite(BR_LADDR_OE_BAR, 1);

    // Read the control lines
    uint32_t busVals = R32(GPLEV0);
    if ((busVals & (1 << BR_RD_BAR)) == 0)
    {
        iorqPortsRead[lowAddr]++;
    }
    else if ((busVals & (1 << BR_WR_BAR)) == 0)
    {
        iorqPortsWritten[lowAddr]++;
    }
    else
    {
        iorqPortsOther[lowAddr]++;
    }
    if (busVals & (iorqIsNotActive != 0))
        iorqIsNotActive++;

    // Clear the WAIT state
    W32(GPCLR0, 1 << BR_WAIT);
    W32(GPSET0, 1 << BR_WAIT);
    // Clear detected edge on any pin
    W32(GPEDS0, 0xffffffff);  
}

#include "ee_printf.h"

int loopCtr = 0;
void br_service()
{
    loopCtr++;
    if (loopCtr > 100000)
    {
        for (int i = 0; i < 256; i++)
        {
            if (iorqPortsRead[i] > 0 || iorqPortsWritten[i] > 0 || iorqPortsOther[i] > 0)
                ee_printf("%d r %d w %d o %d\n", i, iorqPortsRead[i], iorqPortsWritten[i], iorqPortsOther[i]);
        }
        if (iorqIsNotActive > 0)
            ee_printf("Not actv %d", iorqIsNotActive);
        ee_printf("\n");
        loopCtr = 0;
    }
}

#else

// Enable wait-states
void br_enable_wait_states()
{
#ifdef BR_ENABLE_WAIT_STATES
    // Clear WAIT to stop any wait happening
    digitalWrite(BR_WAIT, 0);

    // Set vector for WAIT state interrupt
    irq_set_wait_state_handler(br_wait_state_isr);

    // Setup edge triggering on falling edge of IORQ
    W32(GPEDS0, 1 << BR_IORQ_BAR);  // Clear any current detected edge
    W32(GPFEN0, 1 << BR_IORQ_BAR);  // Set falling edge detect

    // Enable FIQ interrupts on GPIO[3] which is any GPIO pin
    W32(IRQ_FIQ_CONTROL, (1 << 7) | 52);

    // Enable Fast Interrupts
    enable_fiq();

    // Allow WAIT state generation
    digitalWrite(BR_WAIT, 1);
#endif
}

void br_wait_state_isr(void* pData)
{
    pData = pData;

    // Read the low address
    digitalWrite(BR_LADDR_OE_BAR, 0);
    uint32_t lowAddr = br_get_pib_value() & 0xff;
    digitalWrite(BR_LADDR_OE_BAR, 1);

    // Read the control lines
    uint32_t busVals = R32(GPLEV0);

    // Set the appropriate bits for up-line communication
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
        digitalWrite(BR_DATA_OE_BAR, 0);
        dataBusVals = br_get_pib_value() & 0xff;
        digitalWrite(BR_DATA_OE_BAR, 0);
    }

    // Send this to anything listening
    uint32_t retVal = 0;
    if (__br_pBusAccessCallback)
        retVal = __br_pBusAccessCallback(lowAddr, dataBusVals, ctrlBusVals) & 0xff;

    // If not writing then put the returned data onto the bus
    if (!isWriting)
    {
        digitalWrite(BR_DATA_DIR_IN, 0);
        digitalWrite(BR_DATA_OE_BAR, 0);
        br_set_pib_output();
        br_set_pib_value(retVal);
    }

    // Clear the WAIT state
    W32(GPCLR0, 1 << BR_WAIT);
    W32(GPSET0, 1 << BR_WAIT);
    // Clear detected edge on any pin
    W32(GPEDS0, 0xffffffff);  

    // If not writing then we need to wait until the request is complete
    if (!isWriting)
    {
        // Wait until the request has completed (both IORQ_BAR and MREQ_BAR high)
        uint32_t ctrlBusMask = ((1 << BR_CTRL_BUS_MREQ) | (1 << BR_CTRL_BUS_IORQ));
        int hangAvoidCount = 0;
        while ((R32(GPLEV0) & ctrlBusMask) != ctrlBusMask)
        {
            // Stop waiting if this takes more than 1000 loops
            // (which will be way longer than the 1uS or less it should take)
            hangAvoidCount++;
            if (hangAvoidCount > 1000)
                break;
        }
        // Put the bus back to how it should be left
        br_set_pib_input();
        digitalWrite(BR_DATA_DIR_IN, 1);
    }

}

void br_service()
{
}

#endif