// Bus Raider
// Rob Dobson 2018

#pragma once

#include "bare_metal_pi_zero.h"
#include "globaldefs.h"

// Return codes
typedef enum {
    BR_OK = 0,
    BR_ERR = 1,
    BR_NO_BUS_ACK = 2,
} BR_RETURN_TYPE;

// Pi pins used for control of host bus
#define BR_BUSRQ 19
#define BR_BUSACK_BAR 2 // SDA
#define BR_RESET 4
#define BR_NMI 8
#define BR_IRQ 10
#define BR_WAIT 9
#define BR_WR_BAR 17
#define BR_RD_BAR 18
#define BR_MREQ_BAR 0 // ID_SD
#define BR_IORQ_BAR 1 // ID_SC
#define BR_DATA_BUS 20 // GPIO20..27
#define BR_PUSH_ADDR_BAR 3
#define BR_HADDR_CK 7 // CE1
#define BR_HADDR_SER 5
#define BR_LADDR_CK 16
#define BR_LADDR_CLR_BAR 13
#define BR_DATA_DIR_IN 6
#define BR_DATA_OE_BAR 12

// Direct access to Pi PIB (used for data transfer to/from host data bus)
#define BR_PIB_MASK (~((uint32_t)0xff << BR_DATA_BUS))
#define BR_PIB_GPF_REG GPFSEL2
#define BR_PIB_GPF_MASK 0xff000000
#define BR_PIB_GPF_INPUT 0x00000000
#define BR_PIB_GPF_OUTPUT 0x00249249

// Initialise the bus raider
extern void br_init();
// Reset host
extern void br_reset_host();
// NMI host
extern void br_nmi_host();
// IRQ host
extern void br_irq_host();
// Request access to the bus
extern void br_request_bus();
// Check if bus request has been acknowledged
extern int br_bus_acknowledged();
// Take control of bus
extern void br_take_control();
// Release control of bus
extern void br_release_control();
// Request and take bus
extern BR_RETURN_TYPE br_req_and_take_bus();
// Set address
extern void br_set_low_addr(uint32_t lowAddrByte);
extern void br_inc_low_addr();
extern void br_set_high_addr(uint32_t highAddrByte);
extern void br_set_full_addr(unsigned int addr);
// Control the PIB (bus used to transfer data to/from Pi)
extern void br_set_pib_output();
extern void br_set_pib_input();
extern void br_set_pib_value(uint8_t val);
extern uint8_t br_get_pib_value();
// Read and write bytes
extern void br_write_byte(uint32_t byte);
extern uint8_t br_read_byte();
// Read and write blocks
extern BR_RETURN_TYPE br_write_block(uint32_t addr, uint8_t* pData, uint32_t len, int busRqAndRelease);
extern BR_RETURN_TYPE br_read_block(uint32_t addr, uint8_t* pData, uint32_t len, int busRqAndRelease);

