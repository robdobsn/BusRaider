// Bus Raider
// Rob Dobson 2018-2019
// Portions copyright Circle 2014-2018  R. Stange <rsta2@o2online.de>

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// BCM2835 Fields

#if RASPPI == 1
#define ARM_IO_BASE		0x20000000
#else
#define ARM_IO_BASE		0x3F000000
#endif
#define ARM_IO_END		(ARM_IO_BASE + 0xFFFFFF)

#define GPU_IO_BASE		0x7E000000

#define GPU_CACHED_BASE		0x40000000
#define GPU_UNCACHED_BASE	0xC0000000

#if RASPPI == 1
	#ifdef GPU_L2_CACHE_ENABLED
		#define GPU_MEM_BASE	GPU_CACHED_BASE
	#else
		#define GPU_MEM_BASE	GPU_UNCACHED_BASE
	#endif
#else
	#define GPU_MEM_BASE	GPU_UNCACHED_BASE
#endif

// Convert ARM address to GPU bus address (does also work for aliases)
#define BUS_ADDRESS(addr)	(((addr) & ~0xC0000000) | GPU_MEM_BASE)

//
// General Purpose I/O
//
#define ARM_GPIO_BASE		(ARM_IO_BASE + 0x200000)

//bits 2-0 of GPFSEL0: set to 000 to make Pin 0 an output. 001 is an input. Other combinations represent alternate functions
//bits 3-5 are for pin 1.
//...
//bits 27-29 are for pin 9.
//GPFSEL1 repeats, but bits 2-0 are Pin 10, 27-29 are pin 19.
//...
#define ARM_GPIO_GPFSEL0 	(ARM_GPIO_BASE+0x00) //gpio function select. There are 6 of these (32 bit registers)
#define ARM_GPIO_GPFSEL1 	(ARM_GPIO_BASE+0x04)
#define ARM_GPIO_GPFSEL2 	(ARM_GPIO_BASE+0x08)
#define ARM_GPIO_GPFSEL3 	(ARM_GPIO_BASE+0x0c)
#define ARM_GPIO_GPFSEL4 	(ARM_GPIO_BASE+0x10)
#define ARM_GPIO_GPFSEL5 	(ARM_GPIO_BASE+0x14)

//writing a '1' to bit N of GPSET0 makes that pin HIGH.
//writing a '0' has no effect.
//GPSET0[0-31] maps to pins 0-31
//GPSET1[0-21] maps to pins 32-53
#define ARM_GPIO_GPSET0 	(ARM_GPIO_BASE+0x1C) //GPIO Pin Output Set. There are 2 of these (32 bit registers)
#define ARM_GPIO_GPSET1 	(ARM_GPIO_BASE+0x20)
//GPCLR acts the same way as GPSET, but clears the pin instead.
#define ARM_GPIO_GPCLR0 	(ARM_GPIO_BASE+0x28) //GPIO Pin Output Clear. There are 2 of these (32 bits each)
#define ARM_GPIO_GPCLR1 	(ARM_GPIO_BASE+0x2C)
// GPLEVn level of GPIO bit
#define ARM_GPIO_GPLEV0 	(ARM_GPIO_BASE+0x34) //GPIO Pin Level. There are 2 of these (32 bits each)
#define ARM_GPIO_GPLEV1 	(ARM_GPIO_BASE+0x38) //GPIO Pin Level. Second (32 bits)

// Edge and level detection registers
#define ARM_GPIO_GPEDS0 	(ARM_GPIO_BASE+0x40)
#define ARM_GPIO_GPEDS1 	(ARM_GPIO_BASE+0x44)
#define ARM_GPIO_GPREN0 	(ARM_GPIO_BASE+0x4c) // GPIO Rising Edge Detect Enable 0
#define ARM_GPIO_GPREN1 	(ARM_GPIO_BASE+0x50) // GPIO Rising Edge Detect Enable 1
#define ARM_GPIO_GPFEN0 	(ARM_GPIO_BASE+0x58) // GPIO Falling Edge Detect Enable 0
#define ARM_GPIO_GPFEN1 	(ARM_GPIO_BASE+0x5c) // GPIO Falling Edge Detect Enable 1
#define ARM_GPIO_GPHEN0 	(ARM_GPIO_BASE+0x64) // GPIO High Detect Enable 0
#define ARM_GPIO_GPHEN1 	(ARM_GPIO_BASE+0x68) // GPIO High Detect Enable 1
#define ARM_GPIO_GPLEN0 	(ARM_GPIO_BASE+0x70) // GPIO Low Detect Enable 0
#define ARM_GPIO_GPLEN1 	(ARM_GPIO_BASE+0x74) // GPIO Low Detect Enable 1
#define ARM_GPIO_GPAREN0 	(ARM_GPIO_BASE+0x7c) // GPIO Async Rising Edge Detect 0
#define ARM_GPIO_GPAREN1 	(ARM_GPIO_BASE+0x80) // GPIO Async Rising Edge Detect 1
#define ARM_GPIO_GPAFEN0 	(ARM_GPIO_BASE+0x88) // GPIO Async Falling Edge Detect 0
#define ARM_GPIO_GPAFEN1    (ARM_GPIO_BASE+0x8c) // GPIO Async Falling Edge Detect 1

// Pull-up/pull-down control registers
#define ARM_GPIO_GPPUD      (ARM_GPIO_BASE+0x94)
#define ARM_GPIO_GPPUDCLK0  (ARM_GPIO_BASE+0x98)
#define ARM_GPIO_GPPUDCLK1  (ARM_GPIO_BASE+0x9C)

// MiniUART
#define ARM_AUX_MU_CLOCK    250000000

#define ARM_AUX_BASE		(ARM_IO_BASE + 0x215000)

#define ARM_AUX_ENABLE     	(ARM_AUX_BASE+0x04)
#define ARM_AUX_MU_IO_REG   (ARM_AUX_BASE+0x40)
#define ARM_AUX_MU_IER_REG  (ARM_AUX_BASE+0x44)
#define ARM_AUX_MU_IIR_REG  (ARM_AUX_BASE+0x48)
#define ARM_AUX_MU_LCR_REG  (ARM_AUX_BASE+0x4C)
#define ARM_AUX_MU_MCR_REG  (ARM_AUX_BASE+0x50)
#define ARM_AUX_MU_LSR_REG  (ARM_AUX_BASE+0x54)
#define ARM_AUX_MU_MSR_REG  (ARM_AUX_BASE+0x58)
#define ARM_AUX_MU_SCRATCH  (ARM_AUX_BASE+0x5C)
#define ARM_AUX_MU_CNTL_REG (ARM_AUX_BASE+0x60)
#define ARM_AUX_MU_STAT_REG (ARM_AUX_BASE+0x64)
#define ARM_AUX_MU_BAUD_REG (ARM_AUX_BASE+0x68)

// UART0 = PL011
#define ARM_UART_CLOCK      48000000

#define ARM_UART0_BASE		(ARM_IO_BASE + 0x201000)

#define ARM_UART0_DR		(ARM_UART0_BASE + 0x00)
#define ARM_UART0_FR     	(ARM_UART0_BASE + 0x18)
#define ARM_UART0_IBRD   	(ARM_UART0_BASE + 0x24)
#define ARM_UART0_FBRD   	(ARM_UART0_BASE + 0x28)
#define ARM_UART0_LCRH   	(ARM_UART0_BASE + 0x2C)
#define ARM_UART0_CR     	(ARM_UART0_BASE + 0x30)
#define ARM_UART0_IFLS   	(ARM_UART0_BASE + 0x34)
#define ARM_UART0_IMSC   	(ARM_UART0_BASE + 0x38)
#define ARM_UART0_RIS    	(ARM_UART0_BASE + 0x3C)
#define ARM_UART0_MIS    	(ARM_UART0_BASE + 0x40)
#define ARM_UART0_ICR    	(ARM_UART0_BASE + 0x44)

// Definitions from Raspberry PI Remote Serial Protocol.
//     Copyright 2012 Jamie Iles, jamie@jamieiles.com.
//     Licensed under GPLv2
#define UART0_DR_OE_MASK		(1 << 11)
#define UART0_DR_BE_MASK		(1 << 10)
#define UART0_DR_PE_MASK		(1 << 9)
#define UART0_DR_FE_MASK		(1 << 8)

#define UART0_FR_TXFE_MASK		(1 << 7)
#define UART0_FR_RXFF_MASK		(1 << 6)
#define UART0_FR_TXFF_MASK		(1 << 5)
#define UART0_FR_RXFE_MASK		(1 << 4)
#define UART0_FR_BUSY_MASK		(1 << 3)

#define UART0_LCRH_SPS_MASK		(1 << 7)
#define UART0_LCRH_WLEN8_MASK		(3 << 5)
#define UART0_LCRH_WLEN7_MASK		(2 << 5)
#define UART0_LCRH_WLEN6_MASK		(1 << 5)
#define UART0_LCRH_WLEN5_MASK		(0 << 5)
#define UART0_LCRH_FEN_MASK		(1 << 4)
#define UART0_LCRH_STP2_MASK		(1 << 3)
#define UART0_LCRH_EPS_MASK		(1 << 2)
#define UART0_LCRH_PEN_MASK		(1 << 1)
#define UART0_LCRH_BRK_MASK		(1 << 0)

#define UART0_CR_CTSEN_MASK		(1 << 15)
#define UART0_CR_RTSEN_MASK		(1 << 14)
#define UART0_CR_OUT2_MASK		(1 << 13)
#define UART0_CR_OUT1_MASK		(1 << 12)
#define UART0_CR_RTS_MASK		(1 << 11)
#define UART0_CR_DTR_MASK		(1 << 10)
#define UART0_CR_RXE_MASK		(1 << 9)
#define UART0_CR_TXE_MASK		(1 << 8)
#define UART0_CR_LBE_MASK		(1 << 7)
#define UART0_CR_UART_EN_MASK		(1 << 0)

#define UART0_IFLS_RXIFSEL_SHIFT	3
#define UART0_IFLS_RXIFSEL_MASK	(7 << IFLS_RXIFSEL_SHIFT)
#define UART0_IFLS_TXIFSEL_SHIFT	0
#define UART0_IFLS_TXIFSEL_MASK	(7 << IFLS_TXIFSEL_SHIFT)
#define UART0_IFLS_IFSEL_1_8		0
#define UART0_IFLS_IFSEL_1_4		1
#define UART0_IFLS_IFSEL_1_2		2
#define UART0_IFLS_IFSEL_3_4		3
#define UART0_IFLS_IFSEL_7_8		4

#define UART0_INT_OE			(1 << 10)
#define UART0_INT_BE			(1 << 9)
#define UART0_INT_PE			(1 << 8)
#define UART0_INT_FE			(1 << 7)
#define UART0_INT_RT			(1 << 6)
#define UART0_INT_TX			(1 << 5)
#define UART0_INT_RX			(1 << 4)
#define UART0_INT_DSRM		(1 << 3)
#define UART0_INT_DCDM		(1 << 2)
#define UART0_INT_CTSM		(1 << 1)

// Interrupts
#define ARM_IC_BASE		(ARM_IO_BASE + 0xB000)

#define ARM_IC_IRQ_BASIC_PENDING  (ARM_IC_BASE + 0x200)
#define ARM_IC_IRQ_PENDING_1	  (ARM_IC_BASE + 0x204)
#define ARM_IC_IRQ_PENDING_2	  (ARM_IC_BASE + 0x208)
#define ARM_IC_FIQ_CONTROL	  	  (ARM_IC_BASE + 0x20C)
#define ARM_IC_ENABLE_IRQS_1	  (ARM_IC_BASE + 0x210)
#define ARM_IC_ENABLE_IRQS_2	  (ARM_IC_BASE + 0x214)
#define ARM_IC_ENABLE_BASIC_IRQS  (ARM_IC_BASE + 0x218)
#define ARM_IC_DISABLE_IRQS_1	  (ARM_IC_BASE + 0x21C)
#define ARM_IC_DISABLE_IRQS_2	  (ARM_IC_BASE + 0x220)
#define ARM_IC_DISABLE_BASIC_IRQS (ARM_IC_BASE + 0x224)

//
// ARM Timer
//
#define ARM_TIMER_BASE		(ARM_IO_BASE + 0xB000)

#define ARM_TIMER_LOAD		(ARM_TIMER_BASE + 0x400)
#define ARM_TIMER_VALUE		(ARM_TIMER_BASE + 0x404)
#define ARM_TIMER_CTRL		(ARM_TIMER_BASE + 0x408)
#define ARM_TIMER_IRQCLR	(ARM_TIMER_BASE + 0x40C)
#define ARM_TIMER_RAWIRQ	(ARM_TIMER_BASE + 0x410)
#define ARM_TIMER_MASKIRQ	(ARM_TIMER_BASE + 0x414)
#define ARM_TIMER_RELOAD	(ARM_TIMER_BASE + 0x418)
#define ARM_TIMER_PREDIV	(ARM_TIMER_BASE + 0x41C)
#define ARM_TIMER_CNTR		(ARM_TIMER_BASE + 0x420)

//
// GPU Timer
//

#define ARM_SYSTIMER_RATE 1000000

#define ARM_SYSTIMER_CS (ARM_IO_BASE+0x3000)
#define ARM_SYSTIMER_CLO (ARM_IO_BASE+0x3004)
#define ARM_SYSTIMER_CHI (ARM_IO_BASE+0x3008)
#define ARM_SYSTIMER_C0 (ARM_IO_BASE+0x300C)
#define ARM_SYSTIMER_C1 (ARM_IO_BASE+0x3010)
#define ARM_SYSTIMER_C2 (ARM_IO_BASE+0x3014)
#define ARM_SYSTIMER_C3 (ARM_IO_BASE+0x3018)

//
// Mailbox
//
#define MAILBOX_BASE		(ARM_IO_BASE + 0xB880)

#define MAILBOX0_READ  		(MAILBOX_BASE + 0x00)
#define MAILBOX0_STATUS 	(MAILBOX_BASE + 0x18)
	#define MAILBOX_STATUS_EMPTY	0x40000000
#define MAILBOX1_WRITE		(MAILBOX_BASE + 0x20)
#define MAILBOX1_STATUS 	(MAILBOX_BASE + 0x38)
	#define MAILBOX_STATUS_FULL	0x80000000

#define MAILBOX_CHANNEL_PM	0			// power management
#define MAILBOX_CHANNEL_FB 	1			// frame buffer
#define BCM_MAILBOX_PROP_OUT	8			// property tags (ARM to VC)

//
// Clock Manager
//
#define ARM_CM_BASE		    (ARM_IO_BASE + 0x101000)
#define ARM_CM_GP0CTL		(ARM_CM_BASE + 0x70)
#define ARM_CM_GP0DIV		(ARM_CM_BASE + 0x74)
#define ARM_CM_PASSWD 		(0x5A << 24)

// Interrupts
#define ARM_IRQS_PER_REG	32
#define ARM_NUM_IRQ_LINES	(ARM_IRQS_PER_REG * 2 + 8)

#define ARM_IRQ1_BASE		0
#define ARM_IRQ2_BASE		(ARM_IRQ1_BASE + ARM_IRQS_PER_REG)
#define ARM_IRQBASIC_BASE	(ARM_IRQ2_BASE + ARM_IRQS_PER_REG)

#define ARM_IRQ_TIMER0		(ARM_IRQ1_BASE + 0)
#define ARM_IRQ_TIMER1		(ARM_IRQ1_BASE + 1)
#define ARM_IRQ_TIMER2		(ARM_IRQ1_BASE + 2)
#define ARM_IRQ_TIMER3		(ARM_IRQ1_BASE + 3)
#define ARM_IRQ_CODEC0		(ARM_IRQ1_BASE + 4)
#define ARM_IRQ_CODEC1		(ARM_IRQ1_BASE + 5)
#define ARM_IRQ_CODEC2		(ARM_IRQ1_BASE + 6)
#define ARM_IRQ_JPEG		(ARM_IRQ1_BASE + 7)
#define ARM_IRQ_ISP		(ARM_IRQ1_BASE + 8)
#define ARM_IRQ_USB		(ARM_IRQ1_BASE + 9)
#define ARM_IRQ_3D		(ARM_IRQ1_BASE + 10)
#define ARM_IRQ_TRANSPOSER	(ARM_IRQ1_BASE + 11)
#define ARM_IRQ_MULTICORESYNC0	(ARM_IRQ1_BASE + 12)
#define ARM_IRQ_MULTICORESYNC1	(ARM_IRQ1_BASE + 13)
#define ARM_IRQ_MULTICORESYNC2	(ARM_IRQ1_BASE + 14)
#define ARM_IRQ_MULTICORESYNC3	(ARM_IRQ1_BASE + 15)
#define ARM_IRQ_DMA0		(ARM_IRQ1_BASE + 16)
#define ARM_IRQ_DMA1		(ARM_IRQ1_BASE + 17)
#define ARM_IRQ_DMA2		(ARM_IRQ1_BASE + 18)
#define ARM_IRQ_DMA3		(ARM_IRQ1_BASE + 19)
#define ARM_IRQ_DMA4		(ARM_IRQ1_BASE + 20)
#define ARM_IRQ_DMA5		(ARM_IRQ1_BASE + 21)
#define ARM_IRQ_DMA6		(ARM_IRQ1_BASE + 22)
#define ARM_IRQ_DMA7		(ARM_IRQ1_BASE + 23)
#define ARM_IRQ_DMA8		(ARM_IRQ1_BASE + 24)
#define ARM_IRQ_DMA9		(ARM_IRQ1_BASE + 25)
#define ARM_IRQ_DMA10		(ARM_IRQ1_BASE + 26)
#define ARM_IRQ_DMA11		(ARM_IRQ1_BASE + 27)
#define ARM_IRQ_DMA12		(ARM_IRQ1_BASE + 28)
#define ARM_IRQ_AUX		(ARM_IRQ1_BASE + 29)
#define ARM_IRQ_ARM		(ARM_IRQ1_BASE + 30)
#define ARM_IRQ_VPUDMA		(ARM_IRQ1_BASE + 31)

#define ARM_IRQ_HOSTPORT	(ARM_IRQ2_BASE + 0)
#define ARM_IRQ_VIDEOSCALER	(ARM_IRQ2_BASE + 1)
#define ARM_IRQ_CCP2TX		(ARM_IRQ2_BASE + 2)
#define ARM_IRQ_SDC		(ARM_IRQ2_BASE + 3)
#define ARM_IRQ_DSI0		(ARM_IRQ2_BASE + 4)
#define ARM_IRQ_AVE		(ARM_IRQ2_BASE + 5)
#define ARM_IRQ_CAM0		(ARM_IRQ2_BASE + 6)
#define ARM_IRQ_CAM1		(ARM_IRQ2_BASE + 7)
#define ARM_IRQ_HDMI0		(ARM_IRQ2_BASE + 8)
#define ARM_IRQ_HDMI1		(ARM_IRQ2_BASE + 9)
#define ARM_IRQ_PIXELVALVE1	(ARM_IRQ2_BASE + 10)
#define ARM_IRQ_I2CSPISLV	(ARM_IRQ2_BASE + 11)
#define ARM_IRQ_DSI1		(ARM_IRQ2_BASE + 12)
#define ARM_IRQ_PWA0		(ARM_IRQ2_BASE + 13)
#define ARM_IRQ_PWA1		(ARM_IRQ2_BASE + 14)
#define ARM_IRQ_CPR		(ARM_IRQ2_BASE + 15)
#define ARM_IRQ_SMI		(ARM_IRQ2_BASE + 16)
#define ARM_IRQ_GPIO0		(ARM_IRQ2_BASE + 17)
#define ARM_IRQ_GPIO1		(ARM_IRQ2_BASE + 18)
#define ARM_IRQ_GPIO2		(ARM_IRQ2_BASE + 19)
#define ARM_IRQ_GPIO3		(ARM_IRQ2_BASE + 20)
#define ARM_IRQ_I2C		(ARM_IRQ2_BASE + 21)
#define ARM_IRQ_SPI		(ARM_IRQ2_BASE + 22)
#define ARM_IRQ_I2SPCM		(ARM_IRQ2_BASE + 23)
#define ARM_IRQ_SDIO		(ARM_IRQ2_BASE + 24)
#define ARM_IRQ_UART		(ARM_IRQ2_BASE + 25)
#define ARM_IRQ_SLIMBUS		(ARM_IRQ2_BASE + 26)
#define ARM_IRQ_VEC		(ARM_IRQ2_BASE + 27)
#define ARM_IRQ_CPG		(ARM_IRQ2_BASE + 28)
#define ARM_IRQ_RNG		(ARM_IRQ2_BASE + 29)
#define ARM_IRQ_ARASANSDIO	(ARM_IRQ2_BASE + 30)
#define ARM_IRQ_AVSPMON		(ARM_IRQ2_BASE + 31)

#define ARM_IRQ_ARM_TIMER	(ARM_IRQBASIC_BASE + 0)
#define ARM_IRQ_ARM_MAILBOX	(ARM_IRQBASIC_BASE + 1)
#define ARM_IRQ_ARM_DOORBELL_0	(ARM_IRQBASIC_BASE + 2)
#define ARM_IRQ_ARM_DOORBELL_1	(ARM_IRQBASIC_BASE + 3)
#define ARM_IRQ_VPU0_HALTED	(ARM_IRQBASIC_BASE + 4)
#define ARM_IRQ_VPU1_HALTED	(ARM_IRQBASIC_BASE + 5)
#define ARM_IRQ_ILLEGAL_TYPE0	(ARM_IRQBASIC_BASE + 6)
#define ARM_IRQ_ILLEGAL_TYPE1	(ARM_IRQBASIC_BASE + 7)

#define IRQ_LINES		(ARM_IRQS_PER_REG * 2 + 8)

// FIQs
#define ARM_FIQ_TIMER0		0
#define ARM_FIQ_TIMER1		1
#define ARM_FIQ_TIMER2		2
#define ARM_FIQ_TIMER3		3
#define ARM_FIQ_CODEC0		4
#define ARM_FIQ_CODEC1		5
#define ARM_FIQ_CODEC2		6
#define ARM_FIQ_JPEG		7
#define ARM_FIQ_ISP		8
#define ARM_FIQ_USB		9
#define ARM_FIQ_3D		10
#define ARM_FIQ_TRANSPOSER	11
#define ARM_FIQ_MULTICORESYNC0	12
#define ARM_FIQ_MULTICORESYNC1	13
#define ARM_FIQ_MULTICORESYNC2	14
#define ARM_FIQ_MULTICORESYNC3	15
#define ARM_FIQ_DMA0		16
#define ARM_FIQ_DMA1		17
#define ARM_FIQ_DMA2		18
#define ARM_FIQ_DMA3		19
#define ARM_FIQ_DMA4		20
#define ARM_FIQ_DMA5		21
#define ARM_FIQ_DMA6		22
#define ARM_FIQ_DMA7		23
#define ARM_FIQ_DMA8		24
#define ARM_FIQ_DMA9		25
#define ARM_FIQ_DMA10		26
#define ARM_FIQ_DMA11		27
#define ARM_FIQ_DMA12		28
#define ARM_FIQ_AUX		29
#define ARM_FIQ_ARM		30
#define ARM_FIQ_VPUDMA		31
#define ARM_FIQ_HOSTPORT	32
#define ARM_FIQ_VIDEOSCALER	33
#define ARM_FIQ_CCP2TX		34
#define ARM_FIQ_SDC		35
#define ARM_FIQ_DSI0		36
#define ARM_FIQ_AVE		37
#define ARM_FIQ_CAM0		38
#define ARM_FIQ_CAM1		39
#define ARM_FIQ_HDMI0		40
#define ARM_FIQ_HDMI1		41
#define ARM_FIQ_PIXELVALVE1	42
#define ARM_FIQ_I2CSPISLV	43
#define ARM_FIQ_DSI1		44
#define ARM_FIQ_PWA0		45
#define ARM_FIQ_PWA1		46
#define ARM_FIQ_CPR		47
#define ARM_FIQ_SMI		48
#define ARM_FIQ_GPIO0		49
#define ARM_FIQ_GPIO1		50
#define ARM_FIQ_GPIO2		51
#define ARM_FIQ_GPIO3		52
#define ARM_FIQ_I2C		53
#define ARM_FIQ_SPI		54
#define ARM_FIQ_I2SPCM		55
#define ARM_FIQ_SDIO		56
#define ARM_FIQ_UART		57
#define ARM_FIQ_SLIMBUS		58
#define ARM_FIQ_VEC		59
#define ARM_FIQ_CPG		60
#define ARM_FIQ_RNG		61
#define ARM_FIQ_ARASANSDIO	62
#define ARM_FIQ_AVSPMON		63
#define ARM_FIQ_ARM_TIMER	64
#define ARM_FIQ_ARM_MAILBOX	65
#define ARM_FIQ_ARM_DOORBELL_0	66
#define ARM_FIQ_ARM_DOORBELL_1	67
#define ARM_FIQ_VPU0_HALTED	68
#define ARM_FIQ_VPU1_HALTED	69
#define ARM_FIQ_ILLEGAL_TYPE0	70
#define ARM_FIQ_ILLEGAL_TYPE1	71

#define ARM_MAX_FIQ		71

#ifdef __cplusplus
}
#endif