#pragma once

#ifdef __cplusplus
extern "C"{
#endif

#define HIGH 0x1
#define LOW  0x0

#define INPUT 0x0
#define OUTPUT 0x1
#define INPUT_PULLUP 0x2
#define INPUT_PULLDOWN 0x3
#define PINMODE_ALT0 0x4
#define PINMODE_ALT1 0x5
#define PINMODE_ALT2 0x6
#define PINMODE_ALT3 0x7
#define PINMODE_ALT4 0x8
#define PINMODE_ALT5 0x9

#ifndef bool
#define bool int
#endif

typedef	signed char int8_t;
typedef	unsigned char uint8_t;
typedef	short int16_t;
typedef	unsigned short uint16_t;
typedef	int	int32_t;
typedef	unsigned int uint32_t;
typedef	long long int64_t;
typedef	unsigned long long uint64_t;

// BCM2835 Fields

#define GPIO_BASE 0x20200000

//bits 2-0 of GPFSEL0: set to 000 to make Pin 0 an output. 001 is an input. Other combinations represent alternate functions
//bits 3-5 are for pin 1.
//...
//bits 27-29 are for pin 9.
//GPFSEL1 repeats, but bits 2-0 are Pin 10, 27-29 are pin 19.
//...
#define GPFSEL0   0x20200000 //gpio function select. There are 6 of these (32 bit registers)
#define GPFSEL1   0x20200004
#define GPFSEL2   0x20200008
#define GPFSEL3   0x2020000c
#define GPFSEL4   0x20200010
#define GPFSEL5   0x20200014

//writing a '1' to bit N of GPSET0 makes that pin HIGH.
//writing a '0' has no effect.
//GPSET0[0-31] maps to pins 0-31
//GPSET1[0-21] maps to pins 32-53
#define GPSET0    0x2020001C //GPIO Pin Output Set. There are 2 of these (32 bit registers)
#define GPSET1    0x20200020
//GPCLR acts the same way as GPSET, but clears the pin instead.
#define GPCLR0    0x20200028 //GPIO Pin Output Clear. There are 2 of these (32 bits each)
#define GPCLR1    0x2020002C
// GPLEVn level of GPIO bit
#define GPLEV0    0x20200034 //GPIO Pin Level. There are 2 of these (32 bits each)
#define GPLEV1    0x20200038 //GPIO Pin Level. Second (32 bits)

// Edge and level detection registers
#define GPEDS0    0x20200040
#define GPEDS1    0x20200044
#define GPREN0  0x2020004c // GPIO Rising Edge Detect Enable 0
#define GPREN1  0x20200050 // GPIO Rising Edge Detect Enable 1
#define GPFEN0  0x20200058 // GPIO Falling Edge Detect Enable 0
#define GPFEN1  0x2020005c // GPIO Falling Edge Detect Enable 1
#define GPHEN0  0x20200064 // GPIO High Detect Enable 0
#define GPHEN1  0x20200068 // GPIO High Detect Enable 1
#define GPLEN0  0x20200070 // GPIO Low Detect Enable 0
#define GPLEN1  0x20200074 // GPIO Low Detect Enable 1
#define GPAREN0 0x2020007c // GPIO Async Rising Edge Detect 0
#define GPAREN1 0x20200080 // GPIO Async Rising Edge Detect 1
#define GPAFEN0 0x20200088 // GPIO Async Falling Edge Detect 0
#define GPAFEN1 0x2020008c // GPIO Async Falling Edge Detect 1

// Pull-up/pull-down control registers
#define GPPUD       0x20200094
#define GPPUDCLK0   0x20200098
#define GPPUDCLK1   0x2020009C

#define AUX_ENABLES     0x20215004
#define AUX_MU_IO_REG   0x20215040
#define AUX_MU_IER_REG  0x20215044
#define AUX_MU_IIR_REG  0x20215048
#define AUX_MU_LCR_REG  0x2021504C
#define AUX_MU_MCR_REG  0x20215050
#define AUX_MU_LSR_REG  0x20215054
#define AUX_MU_MSR_REG  0x20215058
#define AUX_MU_SCRATCH  0x2021505C
#define AUX_MU_CNTL_REG 0x20215060
#define AUX_MU_STAT_REG 0x20215064
#define AUX_MU_BAUD_REG 0x20215068

#define IRQ_BASIC 0x2000B200
#define IRQ_PEND1 0x2000B204
#define IRQ_PEND2 0x2000B208
#define IRQ_FIQ_CONTROL 0x2000B210
#define IRQ_ENABLE1 0x2000B210
#define IRQ_ENABLE2 0x2000B214
#define IRQ_ENABLE_BASIC 0x2000B218
#define IRQ_DISABLE1 0x2000B21C
#define IRQ_DISABLE2 0x2000B220
#define IRQ_DISABLE_BASIC 0x2000B224

#define SYSTIMERCLO 0x20003004

#ifdef __cplusplus
}
#endif