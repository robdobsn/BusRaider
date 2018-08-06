// Bus Raider
// Rob Dobson 2018

#include "busraider.h"
#include "piwiring.h"
#include "timer.h"

// Following pins are jumpered - to allow SPI bus usage if not needed
// Uncomment the following lines to enable these pins
// #define BR_ENABLE_NMI 1
// #define BR_ENABLE_IRQ 1
// #define BR_ENABLE_WAIT 1

// Uncomment the following to use bitwise access to busses and pins on Pi
// #define USE_BITWISE_BUS_ACCESS 1

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
#ifdef BR_ENABLE_WAIT
    br_set_pin_out(BR_WAIT, 0);
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
    for (int i = 0; i < 100000; i++) {
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
    for (uint32_t i = 0; i < lowAddrByte + 1; i++) {
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
    br_set_low_addr(addr);
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

// Write a single byte to currently set address
// Assumes:
// - control of host bus has been requested and acknowledged
// - address bus is already set and output enabled to host bus
// - PIB is already set to output
void br_write_byte(uint32_t byte)
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
    digitalWrite(BR_MREQ_BAR, 1);
    digitalWrite(BR_DATA_OE_BAR, 1);
    digitalWrite(BR_DATA_DIR_IN, 1);
#else
    // Clear DIR_IN (so make direction out), enable data output onto data bus and MREQ_BAR active
    W32(GPCLR0, (1 << BR_DATA_DIR_IN) | (1 << BR_DATA_OE_BAR) | (1 << BR_MREQ_BAR));
    // Write the data by setting WR_BAR active
    W32(GPCLR0, (1 << BR_WR_BAR));
    // Deactivate and leave data direction set to inwards
    W32(GPSET0, (1 << BR_DATA_DIR_IN) | (1 << BR_DATA_OE_BAR) | (1 << BR_MREQ_BAR) | (1 << BR_WR_BAR));
#endif
}

// Read a single byte from currently set address
// Assumes:
// - control of host bus has been requested and acknowledged
// - address bus is already set and output enabled to host bus
// - PIB is already set to input
// - data direction on data bus driver is set to input (default)
uint8_t br_read_byte()
{
    // Read the byte
#ifdef USE_BITWISE_BUS_ACCESS
    digitalWrite(BR_DATA_DIR_IN, 1);
    digitalWrite(BR_DATA_OE_BAR, 0);
    digitalWrite(BR_MREQ_BAR, 0);
    digitalWrite(BR_RD_BAR, 0);
    uint8_t val = br_get_pib_value();
    digitalWrite(BR_RD_BAR, 1);
    digitalWrite(BR_MREQ_BAR, 1);
    digitalWrite(BR_DATA_OE_BAR, 1);
#else
    // enable data output onto PIB (data-dir must be inwards already), MREQ_BAR and RD_BAR both active
    W32(GPCLR0, (1 << BR_DATA_OE_BAR) | (1 << BR_MREQ_BAR) | (1 << BR_RD_BAR));
    // Get the data
    uint8_t val = br_get_pib_value();
    // Deactivate leaving data-dir inwards
    W32(GPSET0, (1 << BR_DATA_OE_BAR) | (1 << BR_MREQ_BAR) | (1 << BR_RD_BAR));
#endif
    return val;
}

// Write a consecutive block of memory to host
BR_RETURN_TYPE br_write_block(uint32_t addr, uint8_t* pData, uint32_t len, int busRqAndRelease)
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
        br_write_byte(*pData);

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
BR_RETURN_TYPE br_read_block(uint32_t addr, uint8_t* pData, uint32_t len, int busRqAndRelease)
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
        *pData = br_read_byte();

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
