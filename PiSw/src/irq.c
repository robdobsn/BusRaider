#include "irq.h"
#include "ee_printf.h"
#include "utils.h"

// https://www.raspberrypi.org/documentation/hardware/raspberrypi/bcm2835/BCM2835-ARM-Peripherals.pdf
// Page 112 onwards 
// http://xinu.mscs.mu.edu/BCM2835_Interrupt_Controller
// Contrary to documentation bit 19 in BASIC_IRQ does not seem to correspond to UART

static rpi_irq_controller_t* pIRQController = (rpi_irq_controller_t*)RPI_INTERRUPT_CONTROLLER_BASE;

////////////////////////////////////////////////////////////////////////////////////////////
// USB interrupt handling
////////////////////////////////////////////////////////////////////////////////////////////

static IntHandler* __irqHandlerUSBFn = 0;
static void* __irqHandlerUSBData = 0;

void irq_set_usb_handler(IntHandler* pHandler, void* pData)
{
    __irqHandlerUSBFn = pHandler;
    __irqHandlerUSBData = pData;
    enable_irq();
}

////////////////////////////////////////////////////////////////////////////////////////////
// UART interrupt handling
////////////////////////////////////////////////////////////////////////////////////////////

// IntHandler*(_irq_handlers[63]) = { 0 };
// void*(_irq_handlers_data[63]) = { 0 };

static IntHandler* __irqHandlerUARTFn = 0;
static void* __irqHandlerUARTData = 0;

void irq_set_uart_handler(IntHandler* pHandler, void* pData)
{
    __irqHandlerUARTFn = pHandler;
    __irqHandlerUARTData = pData;
    enable_irq();
}

////////////////////////////////////////////////////////////////////////////////////////////
// Common interrupt service routine
////////////////////////////////////////////////////////////////////////////////////////////

void __attribute__((interrupt("IRQ"))) irq_handler_(void)
{

    edgeCountGlobal++;
    globalAllBasicBits |= pIRQController->IRQ_pending_1;
    if (edgeCountGlobal % 1000 == 0)
        globalAllBasicBits = 0;

    if (pIRQController->IRQ_basic_pending & (1 << 11) && __irqHandlerUSBFn)
    {
        // IRQ 9
        __irqHandlerUSBFn(__irqHandlerUSBData);
    }

    // The value here for UART interrupt ... 1 << 29 in IRQ_pending_1 found by looking
    // at which bits set in interrupt register
    // and does not correspond to any documentation I can find
    if ((pIRQController->IRQ_pending_1 & (1 << 29)) && __irqHandlerUARTFn)
    {
        __irqHandlerUARTFn(__irqHandlerUARTData);
    }
}

volatile int edgeCountGlobal = 0;
volatile unsigned int globalAllBasicBits = 0;

void irq_set_iorq_handler()
{
    // Setup edge triggering on falling edge of pin
    #define BR_RD_BAR 18
    W32(GPEDS0, 1 << BR_RD_BAR);  // Clear any current detected edge
    W32(GPFEN0, 1 << BR_RD_BAR);  // Set falling edge detect
   W32(IRQ_FIQ_CONTROL, (1 << 7) | 52);
   enable_fiq();
}
// void __attribute__((interrupt("FIQ"))) fiq_handler_(void)
// {
// 
    //             register volatile char *gpsed0 asm ("r8");
    // register volatile char *dat asm ("r9");
    // gpsed0 = GPEDS0;
    // dat = *gpsed0;
    // if (dat != 0)
    // {
    //     *gpsed0 = 0xffffffff;

    // }
// }
