// Bus Raider
// Rob Dobson 2018

#include "uartMaxi.h"
#include "bare_metal_pi_zero.h"
#include "../System/ee_printf.h"
#include "irq.h"
#include "nmalloc.h"
#include "piwiring.h"
#include "lowlib.h"
#include <stddef.h>
#include <string.h>

// Statics
static const char FromUartMaxi[] = "UartMaxi";

UartMaxi::UartMaxi() :
    _rxBufferPosn(0), _txBufferPosn(0)
{
    _pRxBuffer = NULL;
    _pTxBuffer = NULL;
    _nRxStatus = UART_ERROR_NONE;
}

UartMaxi::~UartMaxi()
{
    // Disable interrupt mask
    WR32(ARM_UART0_IMSC, 0);
    WR32(ARM_UART0_CR, 0);

    // Remove IRQ
    irq_set_PL011MaxiUart_handler(NULL, 0);

    // Remove memory use
    if (_pRxBuffer)
        nmalloc_free((void**)&_pRxBuffer);
    if (_pTxBuffer)
        nmalloc_free((void**)&_pTxBuffer);
}

bool UartMaxi::setup(unsigned int baudRate, int rxBufSize, int txBufSize)
{
    // Allocate for buffers
    _pRxBuffer = (uint8_t*)nmalloc_malloc(rxBufSize);
    if (!_pRxBuffer)
    {
        LogWrite(FromUartMaxi, LOG_DEBUG, "Cannot allocate for Rx buffer size %d\n", rxBufSize);
        return false;
    }
    _pTxBuffer = (uint8_t*)nmalloc_malloc(txBufSize);
    if (!_pTxBuffer)
    {
        LogWrite(FromUartMaxi, LOG_DEBUG, "Cannot allocate for Tx buffer size %d\n", txBufSize);
        return false;
    }
    _rxBufferPosn.init(rxBufSize);
    _txBufferPosn.init(txBufSize);

    // Rx/Tx pins
    pinMode(14, PINMODE_ALT0);
    pinMode(15, PINMODE_ALT0);

    // Baud rate calculation
    uint32_t FUARTCLK = AUX_MU_CLOCK;
	uint32_t baudRateX16 = baudRate * 16;
    uint32_t intBaudDivisor = FUARTCLK / baudRateX16;
    uint32_t fracBaudDivisor = uint32_t((FUARTCLK % baudRateX16) * 64 + 0.5);

    // Connect interrupt
    irq_set_PL011MaxiUart_handler(isrStatic, 0);

    // Set uart registers
	WR32(ARM_UART0_IMSC, 0);
	WR32(ARM_UART0_ICR,  0x7FF);
	WR32(ARM_UART0_IBRD, intBaudDivisor);
	WR32(ARM_UART0_FBRD, fracBaudDivisor);

    // Wire up interrupts & select 8N1
    WR32(ARM_UART0_IFLS,   UART0_IFLS_IFSEL_1_4 << UART0_IFLS_TXIFSEL_SHIFT
                    | UART0_IFLS_IFSEL_1_4 << UART0_IFLS_RXIFSEL_SHIFT);
    WR32(ARM_UART0_LCRH, UART0_LCRH_WLEN8_MASK | UART0_LCRH_FEN_MASK);
    WR32(ARM_UART0_IMSC, UART0_INT_RX | UART0_INT_RT | UART0_INT_OE);    

    // Enable
	WR32(ARM_UART0_CR, UART0_CR_UART_EN_MASK | UART0_CR_TXE_MASK | UART0_CR_RXE_MASK);
    return true;
}

void UartMaxi::clear()
{
    _rxBufferPosn.clear();
}

int UartMaxi::writeBase(unsigned int c)
{
    if (!_txBufferPosn.canPut())
        return 0;
    _pTxBuffer[_txBufferPosn._putPos] = c;
    _txBufferPosn.hasPut();
    return 1;
}

void UartMaxi::txPumpPrime()
{
    // Handle situation where tx has been idle / FIFO not full
    while(_txBufferPosn.canGet())
    {
        if (!(RD32(ARM_UART0_FR) & UART0_FR_TXFF_MASK))
        {
            WR32(ARM_UART0_DR, _pTxBuffer[_txBufferPosn.posToGet()]);
            _txBufferPosn.hasGot();
        }
        else
        {
            WR32(ARM_UART0_IMSC, RD32(ARM_UART0_IMSC) | UART0_INT_TX);
            break;
        }        
    }
}

int UartMaxi::write(unsigned int c)
{
    // Add char to buffer
    int retc = writeBase(c);

    // Ensure ISR picks up
    txPumpPrime();
    return retc;
}

void UartMaxi::write(const char* data, unsigned int size)
{
    // Send
    for (size_t i = 0; i < size; i++)
        writeBase(data[i]);
    // Ensure ISR picks up
    txPumpPrime();
}

void UartMaxi::writeStr(const char* data)
{
    // Send
    write(data, strlen(data));
}


int UartMaxi::read()
{
    if (!_rxBufferPosn.canGet())
        return -1;
    int ch = _pRxBuffer[_rxBufferPosn._getPos];
    _rxBufferPosn.hasGot();
    return ch;
}

int UartMaxi::peek()
{
    if (!_rxBufferPosn.canGet())
        return -1;
    return _pRxBuffer[_rxBufferPosn._getPos];
}

int UartMaxi::available()
{
    return _rxBufferPosn.count();
}

void UartMaxi::isrStatic(void* pParam)
{
	UartMaxi *pStatic = (UartMaxi*) pParam;
	pStatic->isr();
}

void UartMaxi::isr()
{
	// acknowledge pending interrupts
	WR32(ARM_UART0_ICR, RD32(ARM_UART0_MIS));

	while (!(RD32(ARM_UART0_FR) & UART0_FR_RXFE_MASK))
	{
		uint32_t nDR = RD32(ARM_UART0_DR);
		if (nDR & UART0_DR_BE_MASK)
		{
			if (_nRxStatus == UART_ERROR_NONE)
				_nRxStatus = UART_ERROR_BREAK;
		}
		else if (nDR & UART0_DR_OE_MASK)
		{
			if (_nRxStatus == UART_ERROR_NONE)
				_nRxStatus = UART_ERROR_OVERRUN;
		}
		else if (nDR & UART0_DR_FE_MASK)
		{
			if (_nRxStatus == UART_ERROR_NONE)
				_nRxStatus = UART_ERROR_FRAMING;
		}

    	if (_rxBufferPosn.canPut())
		{
			_pRxBuffer[_rxBufferPosn.posToPut()] = nDR & 0xFF;
            _rxBufferPosn.hasPut();
		}
		else
		{
			if (_nRxStatus == UART_ERROR_NONE)
				_nRxStatus = UART_ERROR_FULL;
		}
	}

	while (!(RD32(ARM_UART0_FR) & UART0_FR_TXFF_MASK))
	{
		if (_txBufferPosn.canGet())
		{
			WR32(ARM_UART0_DR, _pTxBuffer[_txBufferPosn.posToGet()]);
			_txBufferPosn.hasGot();
		}
		else
		{
			WR32(ARM_UART0_IMSC, RD32(ARM_UART0_IMSC) & ~UART0_INT_TX);
			break;
		}
	}
}