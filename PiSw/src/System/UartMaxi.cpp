// Bus Raider
// Rob Dobson 2018

#include "UartMaxi.h"
#include "BCM2835.h"
#include "CInterrupts.h"
#include "MachineInfo.h"
#include "nmalloc.h"
#include "PiWiring.h"
#include "LowLib.h"
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
    // Disable interrupt mask and UART
    PeripheralEntry();
    WR32(ARM_UART0_IMSC, 0);
    WR32(ARM_UART0_CR, 0);
    PeripheralExit();

    // Remove IRQ
    CInterrupts::disconnectIRQ(ARM_IRQ_UART);

    // Remove memory use
    if (_pRxBuffer)
        nmalloc_free((void**)&_pRxBuffer);
    if (_pTxBuffer)
        nmalloc_free((void**)&_pTxBuffer);
}

bool UartMaxi::setup(unsigned int baudRate, int rxBufSize, int txBufSize)
{
        // Baud rate calculation
    // uint32_t FUARTCLK = ARM_UART_CLOCK;
    uint32_t FUARTCLK = CMachineInfo::Get ()->GetClockRate (CLOCK_ID_UART);
	uint32_t baudRateX16 = baudRate * 16;
    uint32_t intBaudDivisor = FUARTCLK / baudRateX16;
    if ((intBaudDivisor < 1) || (intBaudDivisor > 0xffff))
        return false;
    unsigned fracBaudDivisor2 = (FUARTCLK % baudRateX16) * 8 / baudRate;
	unsigned fracBaudDivisor = fracBaudDivisor2 / 2 + fracBaudDivisor2 % 2;
    if (fracBaudDivisor > 0x3f)
        return false;
    // LogWrite(FromUartMaxi, LOG_DEBUG, "Baud settings %d:%d\n", intBaudDivisor, fracBaudDivisor);

    // Deallocate buffers if required
    if (_pRxBuffer)
        nmalloc_free((void**)&_pRxBuffer);
    _pRxBuffer = NULL;
    if (_pTxBuffer)
        nmalloc_free((void**)&_pTxBuffer);
    _pTxBuffer = NULL;

    // Allocate for buffers
    _pRxBuffer = (uint8_t*)nmalloc_malloc(rxBufSize);
    if (!_pRxBuffer)
    {
        // LogWrite(FromUartMaxi, LOG_DEBUG, "Cannot allocate for Rx buffer size %d\n", rxBufSize);
        return false;
    }
    _pTxBuffer = (uint8_t*)nmalloc_malloc(txBufSize);
    if (!_pTxBuffer)
    {
        // LogWrite(FromUartMaxi, LOG_DEBUG, "Cannot allocate for Tx buffer size %d\n", txBufSize);
        return false;
    }
    _rxBufferPosn.init(rxBufSize);
    _txBufferPosn.init(txBufSize);

    // Rx/Tx pins
    pinMode(15, PINMODE_ALT0);
    pinMode(14, PINMODE_ALT0);

    // uint32_t ra = RD32(ARM_GPIO_GPFSEL1);
    // ra &= ~(7 << 12); //gpio14
    // ra |= 4 << 12; //alt0
    // ra &= ~(7 << 15); //gpio15
    // ra |= 4 << 15; //alt0
    // WR32(ARM_GPIO_GPFSEL1, ra);

    // Connect interrupt
    CInterrupts::connectIRQ(ARM_IRQ_UART, isrStatic, this);

    // Set uart registers
    PeripheralEntry();
	WR32(ARM_UART0_IMSC, 0);
	WR32(ARM_UART0_ICR,  0x7FF);
	WR32(ARM_UART0_IBRD, intBaudDivisor);
	WR32(ARM_UART0_FBRD, fracBaudDivisor);

    // Wire up interrupts & select 8N1
    WR32(ARM_UART0_IFLS, UART0_IFLS_IFSEL_1_4 << UART0_IFLS_TXIFSEL_SHIFT
                    | UART0_IFLS_IFSEL_1_2 << UART0_IFLS_RXIFSEL_SHIFT);
    WR32(ARM_UART0_LCRH, UART0_LCRH_WLEN8_MASK | UART0_LCRH_FEN_MASK);
    WR32(ARM_UART0_IMSC, UART0_INT_RX | UART0_INT_RT | UART0_INT_OE);    

    // Enable
	WR32(ARM_UART0_CR, UART0_CR_UART_EN_MASK | UART0_CR_TXE_MASK | UART0_CR_RXE_MASK);

    PeripheralExit();

    return true;
}

void UartMaxi::clear()
{
    _rxBufferPosn.clear();
}

int UartMaxi::writeBase(unsigned int c)
{
    if ((!_pTxBuffer) || (!_txBufferPosn.canPut()))
        return 0;

    _pTxBuffer[_txBufferPosn._putPos] = c;
    _txBufferPosn.hasPut();
    return 1;
}

void UartMaxi::txPumpPrime()
{
    // Handle situation where tx has been idle / FIFO not full
    while(_txBufferPosn.canGet() && _pTxBuffer)
    {
        PeripheralEntry();
        if ((RD32(ARM_UART0_FR) & UART0_FR_TXFF_MASK) == 0)
        {
            WR32(ARM_UART0_DR, _pTxBuffer[_txBufferPosn.posToGet()]);
            _txBufferPosn.hasGot();
        }
        else
        {
            WR32(ARM_UART0_IMSC, RD32(ARM_UART0_IMSC) | UART0_INT_TX);
            break;
        }
        PeripheralExit();
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
    if ((!_pRxBuffer) || (!_rxBufferPosn.canGet()))
        return -1;
    int ch = _pRxBuffer[_rxBufferPosn._getPos];
    _rxBufferPosn.hasGot();
    return ch;
}

bool UartMaxi::poll()
{
    return _rxBufferPosn.canGet();
}

int UartMaxi::peek()
{
    if ((!_pRxBuffer) || (!_rxBufferPosn.canGet()))
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
    PeripheralEntry();

    // digitalWrite(8, 1);

	// acknowledge pending interrupts
	WR32(ARM_UART0_ICR, RD32(ARM_UART0_MIS));

	while (!(RD32(ARM_UART0_FR) & UART0_FR_RXFE_MASK))
	{
        // digitalWrite(8, 0);
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

    	if (_rxBufferPosn.canPut() && _pRxBuffer)
		{
			_pRxBuffer[_rxBufferPosn.posToPut()] = nDR & 0xFF;
            _rxBufferPosn.hasPut();
		}
		else
		{
			if (_nRxStatus == UART_ERROR_NONE)
				_nRxStatus = UART_ERROR_FULL;
		}
        // digitalWrite(8, 1);
	}

	while (!(RD32(ARM_UART0_FR) & UART0_FR_TXFF_MASK))
	{
		if (_txBufferPosn.canGet() && _pTxBuffer)
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

    // digitalWrite(8, 0);

    PeripheralExit();

}