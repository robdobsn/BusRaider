// Bus Raider
// Rob Dobson 2018-2020

#pragma once

// Following documented here: https://www.scribd.com/doc/127599939/BCM2835-Audio-clocks
#define ARM_CM_PCMCTL		(ARM_CM_BASE + 0x98)
#define ARM_CM_PCMDIV		(ARM_CM_BASE + 0x9C)
#define ARM_CM_PWMCTL		(ARM_CM_BASE + 0xA0)
#define ARM_CM_PWMDIV		(ARM_CM_BASE + 0xA4)

#define ARM_CM_CTL_PLLD_FREQ 500000000
#define ARM_CM_CTL_OSCILLATOR_FREQ 19200000
#define ARM_CM_CTL_MIN_FREQ_FOR_PLLD 250000
#define ARM_CM_CTL_CLKSRC_OSCILLATOR     0x01
#define ARM_CM_CTL_CLKSRC_TEST0     0x02
#define ARM_CM_CTL_CLKSRC_TEST1     0x03
#define ARM_CM_CTL_CLKSRC_PLLA     0x04
#define ARM_CM_CTL_CLKSRC_PLLC     0x05
#define ARM_CM_CTL_CLKSRC_PLLD     0x06
#define ARM_CM_CTL_CLKSRC_HDMIAUX     0x07
#define ARM_CM_CTL_CLK_SET_MASK 0x0f0f
#define ARM_CM_CTL_KILL     (1 << 5)
#define ARM_CM_CTL_ENAB     (1 << 4)
#define ARM_CM_CTL_BUSY     (1 << 7)
