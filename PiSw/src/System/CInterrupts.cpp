// Bus Raider
// IRQ handling
// Rob Dobson 2019

#include "CInterrupts.h"
#include "lowlev.h"
#include "lowlib.h"
#include "PiWiring.h"
#include <stddef.h>

ISRFunctionType *CInterrupts::_irqISR[ARM_NUM_IRQ_LINES];
void *CInterrupts::_pParam[ARM_NUM_IRQ_LINES];
ISRFunctionType	*CInterrupts::_fiqISR = NULL;
void *CInterrupts::_pFIQParam = NULL;
int CInterrupts::_fiqIntNumber = 0;

#define ARM_IC_IRQ_REGS		3

#define ARM_IC_IRQ_PENDING(irq)	(  (irq) < ARM_IRQ2_BASE	\
				 ? ARM_IC_IRQ_PENDING_1		\
				 : ((irq) < ARM_IRQBASIC_BASE	\
				   ? ARM_IC_IRQ_PENDING_2	\
				   : ARM_IC_IRQ_BASIC_PENDING))
#define ARM_IC_IRQS_ENABLE(irq)	(  (irq) < ARM_IRQ2_BASE	\
				 ? ARM_IC_ENABLE_IRQS_1		\
				 : ((irq) < ARM_IRQBASIC_BASE	\
				   ? ARM_IC_ENABLE_IRQS_2	\
				   : ARM_IC_ENABLE_BASIC_IRQS))
#define ARM_IC_IRQS_DISABLE(irq) (  (irq) < ARM_IRQ2_BASE	\
				 ? ARM_IC_DISABLE_IRQS_1	\
				 : ((irq) < ARM_IRQBASIC_BASE	\
				   ? ARM_IC_DISABLE_IRQS_2	\
				   : ARM_IC_DISABLE_BASIC_IRQS))
#define ARM_IRQ_MASK(irq)	(1 << ((irq) & (ARM_IRQS_PER_REG-1)))

void CInterrupts::setup()
{
	WR32(ARM_IC_FIQ_CONTROL, 0);
	WR32(ARM_IC_DISABLE_IRQS_1, (uint32_t) -1);
	WR32(ARM_IC_DISABLE_IRQS_2, (uint32_t) -1);
	WR32(ARM_IC_DISABLE_BASIC_IRQS, (uint32_t) -1);
	lowlev_enable_irq();
	lowlev_enable_fiq();
}

void CInterrupts::connectIRQ(unsigned nIRQ, ISRFunctionType *pHandler, void *pParam)
{
    if ((nIRQ >= ARM_NUM_IRQ_LINES) || (!pHandler))
        return;
    _irqISR[nIRQ] = pHandler;
    _pParam[nIRQ] = pParam;
    enableIRQ(nIRQ);
}

void CInterrupts::disconnectIRQ(unsigned nIRQ)
{
    if (nIRQ >= ARM_NUM_IRQ_LINES)
        return;
    disableIRQ(nIRQ);
    _irqISR[nIRQ] = 0;
    _pParam[nIRQ] = 0;
}

void CInterrupts::connectFIQ(unsigned nFIQ, ISRFunctionType *pHandler, void *pParam)
{
    if ((nFIQ >= ARM_NUM_IRQ_LINES) || (!pHandler))
        return;
    _fiqIntNumber = nFIQ;
	_fiqISR = pHandler;
	_pFIQParam = pParam;
	enableFIQ(nFIQ);
}

void CInterrupts::disconnectFIQ()
{
	disableFIQ ();
	_fiqISR = NULL;
	_pFIQParam = NULL;
}

void CInterrupts::enableIRQ(unsigned nIRQ)
{
    if (nIRQ >= ARM_NUM_IRQ_LINES)
        return;
	WR32(ARM_IC_IRQS_ENABLE(nIRQ), ARM_IRQ_MASK(nIRQ));
}

void CInterrupts::disableIRQ(unsigned nIRQ)
{
    if (nIRQ >= ARM_NUM_IRQ_LINES)
        return;
	WR32(ARM_IC_IRQS_DISABLE(nIRQ), ARM_IRQ_MASK(nIRQ));
}

void CInterrupts::enableFIQ(unsigned nFIQ)
{
    if (nFIQ >= ARM_NUM_IRQ_LINES)
        return;
	WR32(ARM_IC_FIQ_CONTROL, nFIQ | 0x80);
}

void CInterrupts::disableFIQ ()
{
	WR32(ARM_IC_FIQ_CONTROL, 0);
}

bool CInterrupts::irqHandler(unsigned nIRQ)
{
    if (nIRQ >= ARM_NUM_IRQ_LINES)
        return false;
    ISRFunctionType *pHandler = _irqISR[nIRQ];
    if (pHandler != 0)
    {
        (*pHandler) (_pParam[nIRQ]);
        return true;
    }
    else
    {
        disableIRQ(nIRQ);
    }
    return false;
}

void CInterrupts::interruptService()
{
	uint32_t pending[ARM_IC_IRQ_REGS];
	pending[0] = RD32(ARM_IC_IRQ_PENDING_1);
	pending[1] = RD32(ARM_IC_IRQ_PENDING_2);
	pending[2] = RD32(ARM_IC_IRQ_BASIC_PENDING) & 0xFF;
	for (unsigned nReg = 0; nReg < ARM_IC_IRQ_REGS; nReg++)
	{
		uint32_t nPending = pending[nReg];
		if (nPending != 0)
		{
			unsigned nIRQ = nReg * ARM_IRQS_PER_REG;
			do
			{
				if ((nPending & 1) && irqHandler(nIRQ))
					return;
				nPending >>= 1;
				nIRQ++;
			}
			while (nPending != 0);
		}
	}
}

void CInterrupts::fiqService()
{
	if (_fiqISR)
		(*_fiqISR)(_pFIQParam);
}

extern "C" void __attribute__((interrupt("IRQ"))) irq_handler_(void)
{
    CInterrupts::interruptService();
}


// extern "C" void __attribute__((interrupt("FIQ"))) c_firq_handler()
extern "C" void c_firq_handler()
{
	CInterrupts::fiqService();
}


// // https://www.raspberrypi.org/documentation/hardware/raspberrypi/bcm2835/BCM2835-ARM-Peripherals.pdf
// // Page 112 onwards 
// // http://xinu.mscs.mu.edu/BCM2835_Interrupt_Controller
// // Contrary to documentation bit 19 in BASIC_IRQ does not seem to correspond to UART

// static rpi_irq_controller_t* pIRQController = (rpi_irq_controller_t*)RPI_INTERRUPT_CONTROLLER_BASE;

// ////////////////////////////////////////////////////////////////////////////////////////////
// // USB interrupt handling
// ////////////////////////////////////////////////////////////////////////////////////////////

// static IntHandler* __irqHandlerUSBFn = 0;
// static void* __irqHandlerUSBData = 0;

// void irq_set_usb_handler(IntHandler* pHandler, void* pData)
// {
//     __irqHandlerUSBFn = pHandler;
//     __irqHandlerUSBData = pData;
//     lowlev_enable_irq();
// }

// ////////////////////////////////////////////////////////////////////////////////////////////
// // AUX (MiniUART) and PL011 MaxiUART interrupt handling
// ////////////////////////////////////////////////////////////////////////////////////////////

// // IntHandler*(_irq_handlers[63]) = { 0 };
// // void*(_irq_handlers_data[63]) = { 0 };

// static IntHandler* __irqHandler_AUXMiniUART_Fn = 0;
// static void* __irqHandler_AUXMiniUART_Data = 0;

// void irq_set_auxMiniUart_handler(IntHandler* pHandler, void* pData)
// {
//     __irqHandler_AUXMiniUART_Fn = pHandler;
//     __irqHandler_AUXMiniUART_Data = pData;
//     lowlev_enable_irq();
// }

// static IntHandler* __irqHandler_PL011MaxiUART_Fn = 0;
// static void* __irqHandler_PL011MaxiUART_Data = 0;

// void irq_set_PL011MaxiUart_handler(IntHandler* pHandler, void* pData)
// {
//     __irqHandler_PL011MaxiUART_Fn = pHandler;
//     __irqHandler_PL011MaxiUART_Data = pData;
//     lowlev_enable_irq();
// }

// ////////////////////////////////////////////////////////////////////////////////////////////
// // Common interrupt service routine
// ////////////////////////////////////////////////////////////////////////////////////////////

// extern "C" void __attribute__((interrupt("IRQ"))) irq_handler_(void)
// {
//     if (pIRQController->IRQ_basic_pending & (1 << 11) && __irqHandlerUSBFn)
//     {
//         // IRQ 9
//         __irqHandlerUSBFn(__irqHandlerUSBData);
//     }

//     // The value here for AUX (MiniUART) interrupt ... 1 << 29 in IRQ_pending_1 found by looking
//     // at which bits set in interrupt register
//     if ((pIRQController->IRQ_pending_1 & (1 << 29)) && __irqHandler_AUXMiniUART_Fn)
//     {
//         __irqHandler_AUXMiniUART_Fn(__irqHandler_AUXMiniUART_Data);
//     }

//    // The value here for PL011 MaxiUART interrupt ... 1 << 25 in IRQ_pending_2 found by looking
//     // at which bits set in interrupt register
//     if ((pIRQController->IRQ_pending_2 & (1 << 25)) && __irqHandler_PL011MaxiUART_Fn)
//     {
//         __irqHandler_PL011MaxiUART_Fn(__irqHandler_PL011MaxiUART_Data);
//                     digitalWrite(47, 1);
//             microsDelay(100000);
//             digitalWrite(47, 0);
//             microsDelay(100000);
//     }
// }

// ////////////////////////////////////////////////////////////////////////////////////////////
// // WAIT_STATE FIQ interrupt service routine
// ////////////////////////////////////////////////////////////////////////////////////////////

// static IntHandler* __irqHandlerWaitStateFn = 0;

// void irq_set_wait_state_handler(IntHandler* pHandler)
// {
//     __irqHandlerWaitStateFn = pHandler;
// }

// extern "C" void c_firq_handler()
// {
//     if (__irqHandlerWaitStateFn)
//         __irqHandlerWaitStateFn(0);
// }
