#include "irq.h"
#include "lowlev.h"

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
    lowlev_enable_irq();
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
    lowlev_enable_irq();
}

////////////////////////////////////////////////////////////////////////////////////////////
// Common interrupt service routine
////////////////////////////////////////////////////////////////////////////////////////////

void __attribute__((interrupt("IRQ"))) irq_handler_(void)
{
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

////////////////////////////////////////////////////////////////////////////////////////////
// WAIT_STATE FIQ interrupt service routine
////////////////////////////////////////////////////////////////////////////////////////////

static IntHandler* __irqHandlerWaitStateFn = 0;

void irq_set_wait_state_handler(IntHandler* pHandler)
{
    __irqHandlerWaitStateFn = pHandler;
}

void c_firq_handler()
{
    if (__irqHandlerWaitStateFn)
        __irqHandlerWaitStateFn(0);
}
