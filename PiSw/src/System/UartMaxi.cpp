// Bus Raider
// Rob Dobson 2018

#include "UartMaxi.h"
#include "../System/logging.h"
#include "BCM2835.h"
#include "CInterrupts.h"
#include "MachineInfo.h"
#include "nmalloc.h"
#include "PiWiring.h"
#include "lowlib.h"
#include <stddef.h>
#include <string.h>


// TODO
#include "../TargetBus/BusAccess.h"

// Statics
static const char FromUartMaxi[] = "UartMaxi";

// Uncomment the following line to use SPI0 CE0 of the Pi as a debug pin
// #define USE_PI_SPI0_CE0_AS_DEBUG_PIN 1
#ifdef USE_PI_SPI0_CE0_AS_DEBUG_PIN
#define DEBUG_PI_SPI0_CE0 8
#include "piwiring.h"
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construct/Destruct
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

UartMaxi::UartMaxi() :
    _rxBufferPosn(0), _txBufferPosn(0)
{
    _pRxBuffer = NULL;
    _pTxBuffer = NULL;
    _nRxStatus = UART_ERROR_NONE;
    resetStatusCounts();
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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool UartMaxi::setup(unsigned int baudRate, int rxBufSize, int txBufSize)
{
        // Baud rate calculation
    uint32_t FUARTCLK_MAX = CMachineInfo::Get ()->GetMaxClockRate (CLOCK_ID_UART);
    CMachineInfo::Get ()->SetClockRate (CLOCK_ID_UART, 16000000, true);
    uint32_t FUARTCLK = CMachineInfo::Get ()->GetClockRate (CLOCK_ID_UART);
	uint32_t baudRateX16 = baudRate * 16;
    uint32_t intBaudDivisor = FUARTCLK / baudRateX16;
    if ((intBaudDivisor < 1) || (intBaudDivisor > 0xffff))
    {
        LogWrite(FromUartMaxi, LOG_VERBOSE, "Divisor issue %d, FUARTCLK %d, FUARTCLK_MAX %d, baudRate*16 %d",
                     intBaudDivisor, FUARTCLK, FUARTCLK_MAX, baudRateX16);
        return false;
    }
    unsigned fracBaudDivisor2 = (FUARTCLK % baudRateX16) * 8 / baudRate;
	unsigned fracBaudDivisor = fracBaudDivisor2 / 2 + fracBaudDivisor2 % 2;
    if (fracBaudDivisor > 0x3f)
    {
        LogWrite(FromUartMaxi, LOG_VERBOSE, "Frac issue %d", fracBaudDivisor);
        return false;
    }
    LogWrite(FromUartMaxi, LOG_VERBOSE, "Baud rate %d (%d:%d)\n",
                        baudRate, intBaudDivisor, fracBaudDivisor);

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

    // Debug
#ifdef USE_PI_SPI0_CE0_AS_DEBUG_PIN
    pinMode(DEBUG_PI_SPI0_CE0, OUTPUT);
    digitalWrite(DEBUG_PI_SPI0_CE0, 0);
#endif

    // Clear status
    resetStatusCounts();
    _nRxStatus = UART_ERROR_NONE;
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Clear
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UartMaxi::clear()
{
    _rxBufferPosn.clear();
    resetStatusCounts();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Write
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int UartMaxi::writeBase(unsigned int c)
{
    bool canPut = _txBufferPosn.canPut();
    if ((!_pTxBuffer) || (!canPut))
    {
        _txBufferFullCount++;
        return 0;
    }
    _pTxBuffer[_txBufferPosn._putPos] = c;
    _txBufferPosn.hasPut();
    return 1;
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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TX Priming
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UartMaxi::txPumpPrime()
{
    lowlev_disable_irq();
    // Handle situation where tx has been idle / FIFO not full
    while(_txBufferPosn.canGet() && _pTxBuffer)
    {
        PeripheralEntry();
        // Check TX FIFO  
        if ((RD32(ARM_UART0_FR) & UART0_FR_TXFF_MASK) == 0)
        {
            // Fifo is not full - add data from buffer to FIFO
            uint8_t val = _pTxBuffer[_txBufferPosn.posToGet()];
            _txBufferPosn.hasGot();
            WR32(ARM_UART0_DR, val);
        }
        else
        {
            // FIFO is full - enable interrupts
            WR32(ARM_UART0_IMSC, RD32(ARM_UART0_IMSC) | UART0_INT_TX);
            break;
        }
        PeripheralExit();
    }
    lowlev_enable_irq();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Read
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int UartMaxi::read()
{
    if ((!_pRxBuffer) || (!_rxBufferPosn.canGet()))
        return -1;
    int ch = _pRxBuffer[_rxBufferPosn._getPos];
    _rxBufferPosn.hasGot();
    return ch;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Poll/Peek/Available
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ISR
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UartMaxi::isrStatic(void* pParam)
{
	UartMaxi *pStatic = (UartMaxi*) pParam;
	pStatic->isr();
}

void UartMaxi::isr()
{
    PeripheralEntry();

    // Debug
#ifdef USE_PI_SPI0_CE0_AS_DEBUG_PIN
    digitalWrite(DEBUG_PI_SPI0_CE0, 1);
#endif

	// acknowledge pending interrupts
	WR32(ARM_UART0_ICR, RD32(ARM_UART0_MIS));

    // Handle received chars
	while (!(RD32(ARM_UART0_FR) & UART0_FR_RXFE_MASK))
	{
        // Get rx data and flags
		uint32_t nDR = RD32(ARM_UART0_DR);
		if (nDR & UART0_DR_BE_MASK)
		{
            // Check for Serial BREAK  
            _rxBreakCount++;
			if (_nRxStatus == UART_ERROR_NONE)
				_nRxStatus = UART_ERROR_BREAK;
		}
		else if (nDR & UART0_DR_OE_MASK)
		{
            // Check for overrun error
            _rxOverrunErrCount++;
			if (_nRxStatus == UART_ERROR_NONE)
				_nRxStatus = UART_ERROR_OVERRUN;
		}
		else if (nDR & UART0_DR_FE_MASK)
		{
            // Check for framing error
            _rxFramingErrCount++;
			if (_nRxStatus == UART_ERROR_NONE)
				_nRxStatus = UART_ERROR_FRAMING;
		}

        // Store in rx buffer
    	if (_rxBufferPosn.canPut() && _pRxBuffer)
		{
			_pRxBuffer[_rxBufferPosn.posToPut()] = nDR & 0xFF;
            _rxBufferPosn.hasPut();
		}
		else
		{
            // Record buffer full error
            _rxBufferFullCount++;
			if (_nRxStatus == UART_ERROR_NONE)
				_nRxStatus = UART_ERROR_FULL;
		}
	}

    // Check the tx fifo 
	while (!(RD32(ARM_UART0_FR) & UART0_FR_TXFF_MASK))
	{
        // FIFO is not full
		if (_txBufferPosn.canGet() && _pTxBuffer)
		{
            // Tx buffer has some chars to tx so add one to the FIFO
			WR32(ARM_UART0_DR, _pTxBuffer[_txBufferPosn.posToGet()]);
			_txBufferPosn.hasGot();
		}
		else
		{
            // Nothing more in the buffer to tx so disable the TX ISR
			WR32(ARM_UART0_IMSC, RD32(ARM_UART0_IMSC) & ~UART0_INT_TX);
			break;
		}
	}

    // Debug
#ifdef USE_PI_SPI0_CE0_AS_DEBUG_PIN
    digitalWrite(DEBUG_PI_SPI0_CE0, 0);
#endif

    PeripheralExit();

}