#pragma once

#include "BCM2835.h"

typedef void ISRFunctionType(void *pParam);

class CInterrupts
{
public:
	static void setup();

	static void connectIRQ(unsigned nIRQ, ISRFunctionType *pHandler, void *pParam);
	static void disconnectIRQ (unsigned nIRQ);

	static void connectFIQ(unsigned nFIQ, ISRFunctionType *pHandler, void *pParam);
	static void disconnectFIQ();

	static void enableIRQ(unsigned nIRQ);
	static void disableIRQ(unsigned nIRQ);

	static void enableFIQ(unsigned nFIQ);
	static void disableFIQ();

	static void interruptService();
	static void fiqService();

private:
    static bool irqHandler(unsigned nIRQ);
	static ISRFunctionType *_irqISR[ARM_NUM_IRQ_LINES];
	static void *_pParam[ARM_NUM_IRQ_LINES];
	static ISRFunctionType	*_fiqISR;
	static void *_pFIQParam;
    static int _fiqIntNumber;
};

// #ifdef __cplusplus
// extern "C" {
// #endif

/* See http://www.valvers.com/open-software/raspberry-pi/step04-bare-metal-programming-in-c-pt4/ */

/** @brief See Section 7.5 of the BCM2835 ARM Peripherals documentation, the base
 *         address of the controller is actually xxxxB000, but there is a 0x200 offset
 *                 to the first addressable register for the interrupt controller, so offset the
 *                         base to the first register */
// #define RPI_INTERRUPT_CONTROLLER_BASE (0x2000B200)

// /** @brief The interrupt controller memory mapped register set */
// typedef struct {
//     volatile unsigned int IRQ_basic_pending;
//     volatile unsigned int IRQ_pending_1;
//     volatile unsigned int IRQ_pending_2;
//     volatile unsigned int FIQ_control;
//     volatile unsigned int Enable_IRQs_1;
//     volatile unsigned int Enable_IRQs_2;
//     volatile unsigned int Enable_Base_IRQs;
//     volatile unsigned int Disable_IRQs_1;
//     volatile unsigned int Disable_IRQs_2;
//     volatile unsigned int Disable_Base_IRQs;
// } rpi_irq_controller_t;

// /** @brief Bits in the Enable_Basic_IRQs register to enable various interrupts.
//  *         See the BCM2835 ARM Peripherals manual, section 7.5 */
// #define RPI_BASIC_ARM_TIMER_IRQ (1 << 0)
// #define RPI_BASIC_ARM_MAILBOX_IRQ (1 << 1)
// #define RPI_BASIC_ARM_DOORBELL_0_IRQ (1 << 2)
// #define RPI_BASIC_ARM_DOORBELL_1_IRQ (1 << 3)
// #define RPI_BASIC_GPU_0_HALTED_IRQ (1 << 4)
// #define RPI_BASIC_GPU_1_HALTED_IRQ (1 << 5)
// #define RPI_BASIC_ACCESS_ERROR_1_IRQ (1 << 6)
// #define RPI_BASIC_ACCESS_ERROR_0_IRQ (1 << 7)

// #define RPI_UART_INTERRUPT_IRQ (1 << 25) /* 25 for IRQ register 2 means IRQ 57 in the table */
// #define RPI_SYSTEM_TIMER_3_IRQ (1 << 3)

// typedef void IntHandler(void* data);

// extern void irq_set_usb_handler(IntHandler* pHandler, void* pData);
// extern void irq_set_auxMiniUart_handler(IntHandler* pHandler, void* pData);
// extern void irq_set_PL011MaxiUart_handler(IntHandler* pHandler, void* pData);
// extern void irq_set_wait_state_handler(IntHandler* pHandler);

// #ifdef __cplusplus
// }
// #endif