//
// serial.cpp
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2014-2020  R. Stange <rsta2@o2online.de>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#include "UartMaxiSerialDevice.h"
#include <circle/devicenameservice.h>
#include <circle/bcm2835.h>
#include <circle/memio.h>
#include <circle/machineinfo.h>
#include <circle/synchronize.h>
#include <circle/logger.h>
#include <circle/alloc.h>
#include <circle/util.h>
#include <assert.h>

// TODO remove
uint32_t testVal = 0;
uint32_t testVal2 = 0;
uint32_t testVal3 = 0;
uint32_t isrRegCheck1 = 0;

static const char FromUartMaxi[] = "UartMaxi";

#ifndef USE_RPI_STUB_AT

#define GPIOS			2
#define GPIO_TXD		0
#define GPIO_RXD		1

#define VALUES			2
#define VALUE_PIN		0
#define VALUE_ALT		1

// Registers
#define ARM_UART_DR		(m_nBaseAddress + 0x00)
#define ARM_UART_FR     	(m_nBaseAddress + 0x18)
#define ARM_UART_IBRD   	(m_nBaseAddress + 0x24)
#define ARM_UART_FBRD   	(m_nBaseAddress + 0x28)
#define ARM_UART_LCRH   	(m_nBaseAddress + 0x2C)
#define ARM_UART_CR     	(m_nBaseAddress + 0x30)
#define ARM_UART_IFLS   	(m_nBaseAddress + 0x34)
#define ARM_UART_IMSC   	(m_nBaseAddress + 0x38)
#define ARM_UART_RIS    	(m_nBaseAddress + 0x3C)
#define ARM_UART_MIS    	(m_nBaseAddress + 0x40)
#define ARM_UART_ICR    	(m_nBaseAddress + 0x44)

// Definitions from Raspberry PI Remote Serial Protocol.
//     Copyright 2012 Jamie Iles, jamie@jamieiles.com.
//     Licensed under GPLv2
#define DR_OE_MASK		(1 << 11)
#define DR_BE_MASK		(1 << 10)
#define DR_PE_MASK		(1 << 9)
#define DR_FE_MASK		(1 << 8)

#define FR_TXFE_MASK		(1 << 7)
#define FR_RXFF_MASK		(1 << 6)
#define FR_TXFF_MASK		(1 << 5)
#define FR_RXFE_MASK		(1 << 4)
#define FR_BUSY_MASK		(1 << 3)

#define LCRH_SPS_MASK		(1 << 7)
#define LCRH_WLEN8_MASK		(3 << 5)
#define LCRH_WLEN7_MASK		(2 << 5)
#define LCRH_WLEN6_MASK		(1 << 5)
#define LCRH_WLEN5_MASK		(0 << 5)
#define LCRH_FEN_MASK		(1 << 4)
#define LCRH_STP2_MASK		(1 << 3)
#define LCRH_EPS_MASK		(1 << 2)
#define LCRH_PEN_MASK		(1 << 1)
#define LCRH_BRK_MASK		(1 << 0)

#define CR_CTSEN_MASK		(1 << 15)
#define CR_RTSEN_MASK		(1 << 14)
#define CR_OUT2_MASK		(1 << 13)
#define CR_OUT1_MASK		(1 << 12)
#define CR_RTS_MASK		(1 << 11)
#define CR_DTR_MASK		(1 << 10)
#define CR_RXE_MASK		(1 << 9)
#define CR_TXE_MASK		(1 << 8)
#define CR_LBE_MASK		(1 << 7)
#define CR_UART_EN_MASK		(1 << 0)

#define IFLS_RXIFSEL_SHIFT	3
#define IFLS_RXIFSEL_MASK	(7 << IFLS_RXIFSEL_SHIFT)
#define IFLS_TXIFSEL_SHIFT	0
#define IFLS_TXIFSEL_MASK	(7 << IFLS_TXIFSEL_SHIFT)
	#define IFLS_IFSEL_1_8		0
	#define IFLS_IFSEL_1_4		1
	#define IFLS_IFSEL_1_2		2
	#define IFLS_IFSEL_3_4		3
	#define IFLS_IFSEL_7_8		4

#define INT_OE			(1 << 10)
#define INT_BE			(1 << 9)
#define INT_PE			(1 << 8)
#define INT_FE			(1 << 7)
#define INT_RT			(1 << 6)
#define INT_TX			(1 << 5)
#define INT_RX			(1 << 4)
#define INT_DSRM		(1 << 3)
#define INT_DCDM		(1 << 2)
#define INT_CTSM		(1 << 1)

static uintptr s_BaseAddress[SERIAL_DEVICES] =
{
	ARM_IO_BASE + 0x201000,
#if RASPPI >= 4
	0,
	ARM_IO_BASE + 0x201400,
	ARM_IO_BASE + 0x201600,
	ARM_IO_BASE + 0x201800,
	ARM_IO_BASE + 0x201A00
#endif
};

#define NONE	{10000, 10000}

static unsigned s_GPIOConfig[SERIAL_DEVICES][GPIOS][VALUES] =
{
	// TXD      RXD
#if SERIAL_GPIO_SELECT == 14
	{{14,  0}, {15,  0}},
#elif SERIAL_GPIO_SELECT == 32
	{{32,  3}, {33,  3}},
#elif SERIAL_GPIO_SELECT == 36
	{{36,  2}, {37,  2}},
#else
	#error SERIAL_GPIO_SELECT must be 14, 32 or 36!
#endif
#if RASPPI >= 4
	{  NONE,     NONE  }, // unused
	{{ 0,  4}, { 1,  4}},
	{{ 4,  4}, { 5,  4}},
	{{ 8,  4}, { 9,  4}},
	{{12,  4}, {13,  4}}
#endif
};

#define ALT_FUNC(device, gpio)	((TGPIOMode) (  s_GPIOConfig[device][gpio][VALUE_ALT] \
					      + GPIOModeAlternateFunction0))

unsigned CUartMaxiSerialDevice::s_nInterruptUseCount = 0;
CInterruptSystem *CUartMaxiSerialDevice::s_pInterruptSystem = 0;
boolean CUartMaxiSerialDevice::s_bUseFIQ = FALSE;
volatile u32 CUartMaxiSerialDevice::s_nInterruptDeviceMask = 0;
CUartMaxiSerialDevice *CUartMaxiSerialDevice::s_pThis[SERIAL_DEVICES] = {0};

CUartMaxiSerialDevice::CUartMaxiSerialDevice (CInterruptSystem *pInterruptSystem, boolean bUseFIQ, unsigned nDevice)
:	m_pInterruptSystem (pInterruptSystem),
	m_bUseFIQ (bUseFIQ),
	m_nDevice (nDevice),
	m_nBaseAddress (0),
	m_bValid (FALSE),
	m_pRxBuffer(0),
	m_rxBufferPosn(0), 
	m_bRxBufRequiresDeletion(false),
	m_nRxStatus (UART_ERROR_NONE),
	m_pTxBuffer(0),
	m_txBufferPosn(0),
	m_bTxBufRequiresDeletion(false),
	m_nOptions (SERIAL_OPTION_ONLCR),
	m_pMagic (0),
	m_SpinLock (bUseFIQ ? FIQ_LEVEL : IRQ_LEVEL)
#ifdef REALTIME
	, m_LineSpinLock (TASK_LEVEL)
#endif
{
	if (   m_nDevice >= SERIAL_DEVICES
	    || s_GPIOConfig[nDevice][0][VALUE_PIN] >= GPIO_PINS)
	{
		return;
	}

	assert (s_pThis[m_nDevice] == 0);
	s_pThis[m_nDevice] = this;

	m_nBaseAddress = s_BaseAddress[nDevice];
	assert (m_nBaseAddress != 0);

#if SERIAL_GPIO_SELECT == 14
	if (nDevice == 0)
	{
		// to be sure there is no collision with the Bluetooth controller
		m_GPIO32.AssignPin (32);
		m_GPIO32.SetMode (GPIOModeInput);

		m_GPIO33.AssignPin (33);
		m_GPIO33.SetMode (GPIOModeInput);
	}
#endif

	m_TxDPin.AssignPin (s_GPIOConfig[nDevice][GPIO_TXD][VALUE_PIN]);
	m_TxDPin.SetMode (ALT_FUNC (nDevice, GPIO_TXD));

	m_RxDPin.AssignPin (s_GPIOConfig[nDevice][GPIO_RXD][VALUE_PIN]);
	m_RxDPin.SetMode (ALT_FUNC (nDevice, GPIO_RXD));
	m_RxDPin.SetPullMode (GPIOPullModeUp);

	m_bValid = TRUE;
}

CUartMaxiSerialDevice::~CUartMaxiSerialDevice (void)
{
	if (!m_bValid)
	{
		return;
	}

	// remove device from interrupt handling
	s_nInterruptDeviceMask &= ~(1 << m_nDevice);
	DataSyncBarrier ();

	PeripheralEntry ();
	write32 (ARM_UART_IMSC, 0);
	write32 (ARM_UART_CR, 0);
	PeripheralExit ();

	// disconnect interrupt, if this is the last device, which uses interrupts
	if (   m_pInterruptSystem != 0
	    && --s_nInterruptUseCount == 0)
	{
		assert (s_pInterruptSystem != 0);
		if (!s_bUseFIQ)
		{
			s_pInterruptSystem->DisconnectIRQ (ARM_IRQ_UART);
		}
		else
		{
			s_pInterruptSystem->DisconnectFIQ ();
		}

		s_pInterruptSystem = 0;
		s_bUseFIQ = FALSE;
	}

	m_TxDPin.SetMode (GPIOModeInput);
	m_RxDPin.SetMode (GPIOModeInput);

    // Remove memory use
    if (m_pRxBuffer && m_bRxBufRequiresDeletion)
	{
        delete [] m_pRxBuffer;
	}
    if (m_pTxBuffer && m_bTxBufRequiresDeletion)
	{
        delete [] m_pTxBuffer;
	}

	s_pThis[m_nDevice] = 0;
	m_bValid = FALSE;
}

boolean CUartMaxiSerialDevice::Initialize (unsigned nBaudrate, unsigned nRxBufSize, unsigned nTxBufSize)
{
	if (!m_bValid)
	{
		return FALSE;
	}

	SetClockRate (CLOCK_ID_UART, 32000000, true);
	unsigned nClockRate = CMachineInfo::Get ()->GetClockRate (CLOCK_ID_UART);
	assert (nClockRate > 0);

	assert (300 <= nBaudrate && nBaudrate <= 4000000);
	unsigned nBaud16 = nBaudrate * 16;
	unsigned nIntDiv = nClockRate / nBaud16;
	assert (1 <= nIntDiv && nIntDiv <= 0xFFFF);
	unsigned nFractDiv2 = (nClockRate % nBaud16) * 8 / nBaudrate;
	unsigned nFractDiv = nFractDiv2 / 2 + nFractDiv2 % 2;
	assert (nFractDiv <= 0x3F);

	// CLogger::Get ()->Write (FromUartMaxi, LogDebug, "nClockRate %d nIntDiv %d nFractDiv %d nFractDiv2 %d", 
	// 			nClockRate, nIntDiv, nFractDiv, nFractDiv2);
	
	// Allocate buffers
	AllocateBuffer(m_pRxBuffer, m_bRxBufRequiresDeletion, nRxBufSize, m_rxBufferPosn, m_rxFallbackBuffer, FALLBACK_BUFFER_SIZE);
	AllocateBuffer(m_pTxBuffer, m_bTxBufRequiresDeletion, nTxBufSize, m_txBufferPosn, m_txFallbackBuffer, FALLBACK_BUFFER_SIZE);

	// Interrupt control
	if (m_pInterruptSystem != 0)		// check interrupts valid
	{
		if (s_nInterruptUseCount > 0)
		{
			// if there is already an interrupt enabled device,
			// interrupt configuration must be the same
			if (   m_pInterruptSystem != s_pInterruptSystem
			    || m_bUseFIQ != s_bUseFIQ)
			{
				s_pThis[m_nDevice] = 0;
				m_bValid = FALSE;

				return FALSE;
			}
		}
		else
		{
			// if we are the first interrupt enabled device,
			// connect interrupt with our configuration
			s_pInterruptSystem = m_pInterruptSystem;
			s_bUseFIQ = m_bUseFIQ;

			if (!s_bUseFIQ)
			{
				s_pInterruptSystem->ConnectIRQ (ARM_IRQ_UART, InterruptStub, 0);
				testVal3++;
				// CLogger::Get ()->Write (FromUartMaxi, LogDebug, "UART IRQ connected");
			}
			else
			{
				s_pInterruptSystem->ConnectFIQ (ARM_FIQ_UART, InterruptStub, 0);
			}
		}

		assert (s_nInterruptUseCount < SERIAL_DEVICES);
		s_nInterruptUseCount++;
	}

	PeripheralEntry ();

	write32 (ARM_UART_IMSC, 0);
	write32 (ARM_UART_ICR,  0x7FF);
	write32 (ARM_UART_IBRD, nIntDiv);
	write32 (ARM_UART_FBRD, nFractDiv);

	if (m_pInterruptSystem != 0)
	{
		// Interrupts at 7/8 full
		write32 (ARM_UART_IFLS,   IFLS_IFSEL_1_4 << IFLS_TXIFSEL_SHIFT
					| IFLS_IFSEL_1_2 << IFLS_RXIFSEL_SHIFT);
		write32 (ARM_UART_LCRH, LCRH_WLEN8_MASK | LCRH_FEN_MASK);		// 8N1
		write32 (ARM_UART_IMSC, INT_RX | INT_RT | INT_OE);

		// add device to interrupt handling
		s_nInterruptDeviceMask |= 1 << m_nDevice;
		DataSyncBarrier ();
	}
	else
	{
		write32 (ARM_UART_LCRH, LCRH_WLEN8_MASK);	// 8N1
	}

	write32 (ARM_UART_CR, CR_UART_EN_MASK | CR_TXE_MASK | CR_RXE_MASK);

	// TODO remove
	isrRegCheck1 = read32(ARM_IC_ENABLE_IRQS_2);

	PeripheralExit ();

	CDeviceNameService::Get ()->AddDevice ("ttyS1", this, FALSE);

	return TRUE;
}

bool CUartMaxiSerialDevice::SetClockRate(unsigned nClockId, unsigned nRate, bool bSkipTurbo)
{
	CBcmPropertyTags Tags;
	TPropertyTagSetClockRate TagSetClockRate;
	TagSetClockRate.nClockId = nClockId;
	TagSetClockRate.nRate = nRate;
	TagSetClockRate.nSkipSettingTurbo = bSkipTurbo ? SKIP_SETTING_TURBO : 0;
	return Tags.GetTag (PROPTAG_SET_CLOCK_RATE, &TagSetClockRate, sizeof TagSetClockRate, 12);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Clear
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CUartMaxiSerialDevice::Clear()
{
    m_rxBufferPosn.clear();
    m_uartStats.Clear();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Write
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

unsigned CUartMaxiSerialDevice::AvailableForWrite()
{
    return m_txBufferPosn.size() - m_txBufferPosn.count();
}

int CUartMaxiSerialDevice::WriteBase(unsigned int c)
{
    if (!m_pTxBuffer)
        return 0;
    bool canPut = m_txBufferPosn.canPut();
    if (!canPut)
    {
        m_uartStats.m_txBufferFullCount++;
        return 0;
    }
    m_pTxBuffer[m_txBufferPosn._putPos] = c;
    m_txBufferPosn.hasPut();
    return 1;
}

int CUartMaxiSerialDevice::Write(unsigned int ch)
{
    // Add char to buffer
    int retc = WriteBase(ch);

    // Ensure ISR picks up
    TxPumpPrime();
    return retc;
}

int CUartMaxiSerialDevice::Write(const u8* pBuffer, size_t nCount)
{
    // Send
	int numWritten = 0;
    for (size_t i = 0; i < nCount; i++)
        numWritten += WriteBase(pBuffer[i]);
    // Ensure ISR picks up
    TxPumpPrime();
	return numWritten;
}

int CUartMaxiSerialDevice::WriteStr(const char* pStr)
{
    // Send
    return Write((const u8*)pStr, strlen(pStr));
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TX Priming
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CUartMaxiSerialDevice::TxPumpPrime()
{
	
    EnterCritical();
    // Handle situation where tx has been idle / FIFO not full
    while(m_txBufferPosn.canGet() && m_pTxBuffer)
    {
        PeripheralEntry();
        // Check TX FIFO  
        if ((read32(ARM_UART0_FR) & FR_TXFF_MASK) == 0)
        {
            // Fifo is not full - add data from buffer to FIFO
            uint8_t val = m_pTxBuffer[m_txBufferPosn.posToGet()];
            m_txBufferPosn.hasGot();
            write32(ARM_UART0_DR, val);
        }
        else
        {
            // FIFO is full - enable interrupts
            write32(ARM_UART0_IMSC, read32(ARM_UART0_IMSC) | INT_TX);
            break;
        }
        PeripheralExit();
    }
    LeaveCritical();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Read
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int CUartMaxiSerialDevice::Read()
{
    if ((!m_pRxBuffer) || (!m_rxBufferPosn.canGet()))
        return -1;
    int ch = m_pRxBuffer[m_rxBufferPosn._getPos];
    m_rxBufferPosn.hasGot();
    return ch;
}

unsigned CUartMaxiSerialDevice::Read(u8 *pBuffer, size_t nCount)
{
	// TODO remove
	testVal2 = read32 (ARM_UART0_MIS);

    if (!m_pRxBuffer)
	{
        return 0;
	}

	size_t nRx = 0;
	while(m_rxBufferPosn.canGet() && (nRx < nCount))
	{
		pBuffer[nRx] = m_pRxBuffer[m_rxBufferPosn._getPos];
		m_rxBufferPosn.hasGot();
		nRx++;
	}
	return nRx;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CanGet/Peek/Available
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CUartMaxiSerialDevice::CanGet()
{
    return m_rxBufferPosn.canGet();
}

int CUartMaxiSerialDevice::Peek()
{
    if ((!m_pRxBuffer) || (!m_rxBufferPosn.canGet()))
        return -1;
    return m_pRxBuffer[m_rxBufferPosn._getPos];
}

unsigned CUartMaxiSerialDevice::AvailableForRead()
{
    return m_rxBufferPosn.count();
}

unsigned CUartMaxiSerialDevice::GetOptions (void) const
{
	return m_nOptions;
}

void CUartMaxiSerialDevice::SetOptions (unsigned nOptions)
{
	m_nOptions = nOptions;
}

void CUartMaxiSerialDevice::RegisterMagicReceivedHandler (const char *pMagic, TMagicReceivedHandler *pHandler)
{
	assert (m_pInterruptSystem != 0);
	assert (m_pMagic == 0);

	assert (pMagic != 0);
	assert (*pMagic != '\0');
	assert (pHandler != 0);

	m_pMagicReceivedHandler = pHandler;

	m_pMagicPtr = pMagic;
	m_pMagic = pMagic;		// enables the scanner
}

void CUartMaxiSerialDevice::InterruptHandler()
{
	boolean bMagicReceived = FALSE;

    PeripheralEntry();

    // Debug
#ifdef USE_PI_SPI0_CE0_AS_DEBUG_PIN
    digitalWrite(DEBUG_PI_SPI0_CE0, 1);
#endif

	// acknowledge pending interrupts
	write32(ARM_UART0_ICR, read32(ARM_UART0_MIS));

    // Handle received chars
	while (!(read32(ARM_UART0_FR) & FR_RXFE_MASK))
	{
        // Get rx data and flags
		uint32_t nDR = read32(ARM_UART0_DR);
		if (nDR & DR_BE_MASK)
		{
            // Check for Serial BREAK  
            m_uartStats.m_rxBreakCount++;
			if (m_nRxStatus == UART_ERROR_NONE)
				m_nRxStatus = UART_ERROR_BREAK;
		}
		else if (nDR & DR_OE_MASK)
		{
            // Check for overrun error
            m_uartStats.m_rxOverrunErrCount++;
			if (m_nRxStatus == UART_ERROR_NONE)
				m_nRxStatus = UART_ERROR_OVERRUN;
		}
		else if (nDR & DR_FE_MASK)
		{
            // Check for framing error
            m_uartStats.m_rxFramingErrCount++;
			if (m_nRxStatus == UART_ERROR_NONE)
				m_nRxStatus = UART_ERROR_FRAMING;
		}

        // Store in rx buffer
    	if (m_rxBufferPosn.canPut() && m_pRxBuffer)
		{
			u8 rxCh = nDR & 0xFF;
			m_pRxBuffer[m_rxBufferPosn.posToPut()] = rxCh;
            m_rxBufferPosn.hasPut();

			if (m_pMagic != 0)
			{
				if ((char)rxCh == *m_pMagicPtr)
				{
					if (*++m_pMagicPtr == '\0')
					{
						bMagicReceived = TRUE;
					}
				}
				else
				{
					m_pMagicPtr = m_pMagic;
				}
				// Check for framing error
				m_uartStats.m_rxFramingErrCount++;
				if (m_nRxStatus == UART_ERROR_NONE)
					m_nRxStatus = UART_ERROR_FRAMING;
			}			
		}
		else
		{
            // Record buffer full error
            m_uartStats.m_rxBufferFullCount++;
			if (m_nRxStatus == UART_ERROR_NONE)
				m_nRxStatus = UART_ERROR_FULL;
		}
	}

    // Check the tx fifo 
	while (!(read32(ARM_UART0_FR) & FR_TXFF_MASK))
	{
        // FIFO is not full
		if (m_txBufferPosn.canGet() && m_pTxBuffer)
		{
            // Tx buffer has some chars to tx so add one to the FIFO
			write32(ARM_UART0_DR, m_pTxBuffer[m_txBufferPosn.posToGet()]);
			m_txBufferPosn.hasGot();
		}
		else
		{
            // Nothing more in the buffer to tx so disable the TX ISR
			write32(ARM_UART0_IMSC, read32(ARM_UART0_IMSC) & ~INT_TX);
			break;
		}
	}

    // Debug
#ifdef USE_PI_SPI0_CE0_AS_DEBUG_PIN
    digitalWrite(DEBUG_PI_SPI0_CE0, 0);
#endif

	if (bMagicReceived)
	{
		(*m_pMagicReceivedHandler) ();
	}

    PeripheralExit();
}

void CUartMaxiSerialDevice::InterruptStub (void *pParam)
{
	testVal++;
	DataMemBarrier ();
	u32 nDeviceMask = s_nInterruptDeviceMask;

#if RASPPI >= 4
	for (unsigned i = 0; nDeviceMask != 0 && i < SERIAL_DEVICES; nDeviceMask &= ~(1 << i), i++)
	{
		if (nDeviceMask & (1 << i))
		{
			assert (s_pThis[i] != 0);
			s_pThis[i]->InterruptHandler ();
		}
	}
#else
	// only device 0 is supported here
	if (nDeviceMask & 1)
	{
		assert (s_pThis[0] != 0);
		s_pThis[0]->InterruptHandler ();
	}
#endif
}

void CUartMaxiSerialDevice::AllocateBuffer(u8*& pBuffer, bool& bufRequiresDeletion, unsigned reqBufSize, 
			RingBufferPosn& bufPosn, u8* pFallbackBuffer, unsigned fallbackBufferSize)
{
	// Check if already allocated and the correct size
	if (pBuffer && (bufPosn.size() == reqBufSize))
		return;

	// Check if deletion needed
	if (bufRequiresDeletion && pBuffer)
	{
		delete [] pBuffer;
		pBuffer = 0;
		bufRequiresDeletion = false;
		bufPosn.init(0);
	}

	// Check for zero-length - which indicates the fallback buffer should be used
	if (reqBufSize == 0)
	{
		pBuffer = pFallbackBuffer;
		bufRequiresDeletion = false;

		// Ring buffer position
		bufPosn.init(fallbackBufferSize);
		return;
    }

	// Handle buffer allocation
	pBuffer = new uint8_t[reqBufSize];
	if (pBuffer)
	{
		// Dynamically allocated so requires deletion
		bufRequiresDeletion = true;

		// Ring buffer position
		bufPosn.init(reqBufSize);
	}
}

#else	// #ifndef USE_RPI_STUB_AT

boolean CUartMaxiSerialDevice::Initialize (unsigned nBaudrate)
{
	CDeviceNameService::Get ()->AddDevice ("ttyS1", this, FALSE);

	return TRUE;
}

int CUartMaxiSerialDevice::Write (const void *pBuffer, unsigned nCount)
{
	asm volatile
	(
		"push {r0-r2}\n"
		"mov r0, %0\n"
		"mov r1, %1\n"
		"bkpt #0x7FFB\n"	// send message to GDB client
		"pop {r0-r2}\n"

		: : "r" (pBuffer), "r" (nCount)
	);

	return nCount;
}

#endif
