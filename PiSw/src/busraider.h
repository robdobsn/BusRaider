// Bus Raider

#ifndef _BUSRAIDER_H_
#define _BUSRAIDER_H_

// Return codes
typedef enum {
BR_OK             = 0,
BR_ERR            = 1,
BR_NO_BUS_ACK     = 2,
} BR_RETURN_TYPE;

// Pi pins used for control of host bus
#define BR_BUSRQ 19
#define BR_BUSACK_BAR 2			// SDA
#define BR_RESET 4
#define BR_NMI 8
#define BR_IRQ 10
#define BR_WAIT 9
#define BR_WR_BAR 17
#define BR_RD_BAR 18
#define BR_MREQ_BAR 0			// ID_SD
#define BR_IORQ_BAR 1			// ID_SC
#define BR_DATA_BUS 20			// GPIO20..27
#define BR_PUSH_ADDR_BAR 3
#define BR_HADDR_CK 7			// CE1
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
// Request access to the bus
extern void br_request_bus();
// Check if bus request has been acknowledged
extern int br_bus_acknowledged();
// Take control of bus
extern void br_take_control();
// Release control of bus
extern void br_release_control();

#endif
