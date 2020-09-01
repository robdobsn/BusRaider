// Bus Raider
// Rob Dobson 2018

#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Defs
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
#define BR_MUX_LADDR_CLK 0x00
#define BR_MUX_LADDR_CLR_BAR_LOW 0x04
#define BR_MUX_DATA_OE_BAR_LOW 0x01
#define BR_MUX_RESET_Z80_BAR_LOW 0x05
#define BR_MUX_IRQ_BAR_LOW 0x02
#define BR_MUX_NMI_BAR_LOW 0x06
#define BR_MUX_LADDR_OE_BAR 0x03
#define BR_MUX_HADDR_OE_BAR 0x07

// Pi pins used for control of host bus
#define BR_BUSRQ_BAR 19 // SPI1 MISO
#define BR_BUSACK_BAR 2 // SDA
#define BR_MUX_0 11 // SPI0 SCLK
#define BR_MUX_1 9 // SPI0 MISO
#define BR_MUX_2 10 // SPI0 MOSI
#define BR_MUX_EN_BAR 16 // SPI1 CE2
#define BR_IORQ_WAIT_EN 12 // GPIO12
#define BR_MREQ_WAIT_EN 13 // GPIO13
#define BR_WR_BAR 17 // SPI1 CE1
#define BR_RD_BAR 18 // SPI1 CE0
#define BR_MREQ_BAR 0 // ID_SD
#define BR_IORQ_BAR 1 // ID_SC
#define BR_DATA_BUS 20 // GPIO20..27
#define BR_HADDR_CK 7 // SPI0 CE1
#define BR_DATA_DIR_IN 6
#define BR_WAIT_BAR_PIN 5 // GPIO5
#define BR_CLOCK_PIN 4 // GPIO4
#define BR_PAGING_RAM_PIN 8 // CPI CE0

// V1.7 hardware pins
#define BR_V17_LADDR_CK 16 // SPI1 CE2
#define BR_V17_LADDR_CK_MASK (1 << BR_V17_LADDR_CK)
#define BR_V17_M1_PIB_BAR 20 // Piggy backing on the PIB (with resistor to avoid conflict)
#define BR_V17_PUSH_ADDR_BAR 3 // SCL
#define BR_V17_M1_PIB_BAR_MASK (1 << BR_V17_M1_PIB_BAR)
#define BR_V17_M1_PIB_DATA_LINE 0
#define BR_V17_MUX_HADDR_SER_LOW 0x00
#define BR_V17_MUX_HADDR_SER_HIGH BR_MUX_LADDR_CLR_BAR_LOW 

// V2.0 hardware pins
#define BR_V20_M1_BAR 3 // SCL
#define BR_V20_M1_BAR_MASK (1 << BR_V20_M1_BAR)

// Masks for above
#define BR_BUSRQ_BAR_MASK (1 << BR_BUSRQ_BAR)
#define BR_MUX_EN_BAR_MASK (1 << BR_MUX_EN_BAR)
#define BR_IORQ_WAIT_EN_MASK (1 << BR_IORQ_WAIT_EN)
#define BR_MREQ_WAIT_EN_MASK (1 << BR_MREQ_WAIT_EN)
#define BR_MREQ_BAR_MASK (1 << BR_MREQ_BAR)
#define BR_IORQ_BAR_MASK (1 << BR_IORQ_BAR)
#define BR_WR_BAR_MASK (1 << BR_WR_BAR)
#define BR_RD_BAR_MASK (1 << BR_RD_BAR)
#define BR_BUSACK_BAR_MASK (1 << BR_BUSACK_BAR)
#define BR_WAIT_BAR_MASK (1 << BR_WAIT_BAR_PIN)
#define BR_DATA_DIR_IN_MASK (1 << BR_DATA_DIR_IN)
#define BR_ANY_EDGE_MASK 0xffffffff

// Pi debug pin
#define BR_DEBUG_PI_SPI0_CE0 8 // SPI0 CE0
#define BR_DEBUG_PI_SPI0_CE0_MASK (1 << BR_DEBUG_PI_SPI0_CE0) // SPI0 CE0

// Direct access to Pi PIB (used for data transfer to/from host data bus)
#define BR_PIB_MASK (~((uint32_t)0xff << BR_DATA_BUS))
#define BR_PIB_GPF_REG ARM_GPIO_GPFSEL2
#define BR_PIB_GPF_MASK 0xff000000
#define BR_PIB_GPF_INPUT 0x00000000
#define BR_PIB_GPF_OUTPUT 0x00249249

// Debug code for ISR
#define ISR_ASSERT(code) _mcManager.getBusAccess().isrAssert(code)
#define ISR_VALUE(code, val) _mcManager.getBusAccess().isrValue(code, val)
#define ISR_PEAK(code, val) _mcManager.getBusAccess().isrPeak(code, val)
#define ISR_ASSERT_CODE_NONE 0
#define ISR_ASSERT_CODE_DEBUG_A 1
#define ISR_ASSERT_CODE_DEBUG_B 2
#define ISR_ASSERT_CODE_DEBUG_C 3
#define ISR_ASSERT_CODE_DEBUG_D 4
#define ISR_ASSERT_CODE_DEBUG_E 5
#define ISR_ASSERT_CODE_DEBUG_F 6
#define ISR_ASSERT_CODE_DEBUG_G 7
#define ISR_ASSERT_CODE_DEBUG_H 8
#define ISR_ASSERT_CODE_DEBUG_I 9
#define ISR_ASSERT_CODE_DEBUG_J 10
#define ISR_ASSERT_CODE_DEBUG_K 11
#define ISR_ASSERT_NUM_CODES 12

// Timing of a reset, NMI, IRQ and wait for BUSACK
#define BR_RESET_PULSE_T_STATES 100
#define BR_NMI_PULSE_T_STATES 32
#define BR_IRQ_PULSE_T_STATES 32
#define BR_MAX_WAIT_FOR_BUSACK_T_STATES 1000

// Max time bound in service function
#define BR_MAX_TIME_IN_SERVICE_LOOP_US 10000
#define BR_MAX_TIME_IN_READ_LOOP_US 10000

// Clock frequency for debug
#define BR_TARGET_DEBUG_CLOCK_HZ 500000