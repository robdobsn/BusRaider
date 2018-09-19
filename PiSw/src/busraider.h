// Bus Raider
// Rob Dobson 2018

#pragma once

#include "bare_metal_pi_zero.h"
#include "globaldefs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t br_bus_access_callback_fntype(uint32_t addr, uint32_t data, uint32_t flags);

// Return codes
typedef enum {
    BR_OK = 0,
    BR_ERR = 1,
    BR_NO_BUS_ACK = 2,
} BR_RETURN_TYPE;

#define BR_MEM_ACCESS_RSLT_NOT_DECODED 0x8000

// Control bus bits used to pass to machines, etc
#define BR_CTRL_BUS_RD 0
#define BR_CTRL_BUS_WR 1
#define BR_CTRL_BUS_MREQ 2
#define BR_CTRL_BUS_IORQ 3
#define BR_CTRL_BUS_M1 4

// Multiplexer
// Wiring is a little off as A0, A1, A2 on 74HC138 are Pi 11, 9, 10 respectively
// Pi GPIO0    74HC138      CTRL Value
// 11 10 09    A2 A1 A0     
//  0  0  0     0  0  0     PI_HADDR_SER
//  0  0  1     0  1  0     PI_DATA_OE_BAR
//  0  1  0     1  0  0     PI_IRQ_BAR
//  0  1  1     1  1  0     PI_LADDR_OE_BAR
//  1  0  0     0  0  1     PI_LADDR_CLR_BAR
//  1  0  1     0  1  1     PI_RESET_Z80_BAR
//  1  1  0     1  0  1     PI_NMI_BAR
//  1  1  1     1  1  1     PI_HADDR_OE_BAR
#define BR_MUX_LOW_BIT_POS 9
#define BR_MUX_CTRL_BIT_MASK (0x07 << BR_MUX_LOW_BIT_POS)
#define BR_MUX_HADDR_SER_LOW 0x00
#define BR_MUX_LADDR_CLR_BAR_LOW 0x04
#define BR_MUX_DATA_OE_BAR_LOW 0x01
#define BR_MUX_RESET_Z80_BAR_LOW 0x05
#define BR_MUX_IRQ_BAR_LOW 0x02
#define BR_MUX_NMI_BAR_LOW 0x06
#define BR_MUX_LADDR_OE_BAR 0x03
#define BR_MUX_HADDR_OE_BAR 0x07
/*
// The following is used when HADDR_SER needs to be HIGH - the default is low
// It actually sets MUX to DATA_OE_BAR - this is ok as PIB is either in our out
// at this stage so data will be pulled into or out of the 
#define BR_MUX_HADDR_SER_HIGH BR_MUX_DATA_OE_BAR_LOW 
*/

// This is used when HADDR_SER needs to be HIGH - default is LOW based on mux input 0
#define BR_MUX_HADDR_SER_HIGH BR_MUX_LADDR_CLR_BAR_LOW

// Pi pins used for control of host bus
#define BR_BUSRQ_BAR 19 // SPI1 MISO
#define BR_BUSACK_BAR 2 // SDA
#define BR_MUX_0 11 // SPI0 SCLK
#define BR_MUX_1 9 // SPI0 MISO
#define BR_MUX_2 10 // SPI0 MOSI
#define BR_IORQ_WAIT_EN 12 // GPIO12
#define BR_MREQ_WAIT_EN 13 // GPIO13
#define BR_WR_BAR 17 // SPI1 CE1
#define BR_RD_BAR 18 // SPI1 CE0
#define BR_MREQ_BAR 0 // ID_SD
#define BR_IORQ_BAR 1 // ID_SC
#define BR_DATA_BUS 20 // GPIO20..27
#define BR_PUSH_ADDR_BAR 3 // SCL
#define BR_HADDR_CK 7 // SPI0 CE1
#define BR_LADDR_CK 16 // SPI1 CE2
#define BR_DATA_DIR_IN 6
#define BR_M1_BAR 5 // GPIO5
#define BR_CLOCK_PIN 4 // GPIO4

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
extern void br_release_control(bool resetTargetOnRelease);
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
extern void br_write_byte(uint32_t byte, int iorq);
extern uint8_t br_read_byte(int iorq);
// Read and write blocks
extern BR_RETURN_TYPE br_write_block(uint32_t addr, uint8_t* pData, uint32_t len, int busRqAndRelease, int iorq);
extern BR_RETURN_TYPE br_read_block(uint32_t addr, uint8_t* pData, uint32_t len, int busRqAndRelease, int iorq);
// Enable WAIT
extern void br_enable_mem_and_io_access(bool enWaitOnIORQ, bool enWaitOnMREQ);
extern void br_wait_state_isr(void* pData);

// Clear IO
extern void br_clear_all_io();

// Service
extern void br_service();

// Set and remove callbacks on bus access
extern void br_set_bus_access_callback(br_bus_access_callback_fntype* pBusAccessCallback);
extern void br_remove_bus_access_callback();

#ifdef __cplusplus
}
#endif
