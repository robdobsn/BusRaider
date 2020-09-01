//
/// \file uartMaxi.h
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
#ifndef _circle_uart_maxi_h
#define _circle_uart_maxi_h

#include <circle/device.h>
#include <circle/interrupt.h>
#include <circle/gpiopin.h>
#include <circle/spinlock.h>
#include <circle/sysconfig.h>
#include <circle/types.h>
#include "RingBufferPosn.h"
#include <inttypes.h>

/// \class CUartMaxiSerialDevice
/// \brief Driver for PL011 UART
///
/// \details GPIO pin mapping (chip numbers)
/// nDevice | TXD    | RXD    | Support
/// :-----: | :----: | :----: | :------
/// 0       | GPIO14 | GPIO15 | All boards
/// ^       | GPIO32 | GPIO33 | Compute Modules
/// ^       | GPIO36 | GPIO37 | Compute Modules
/// 1       |        |        | None (AUX)
/// 2       | GPIO0  | GPIO1  | Raspberry Pi 4 only
/// 3       | GPIO4  | GPIO5  | Raspberry Pi 4 only
/// 4       | GPIO8  | GPIO9  | Raspberry Pi 4 only
/// 5       | GPIO12 | GPIO13 | Raspberry Pi 4 only
/// GPIO32/33 and GPIO36/37 can be selected with system option SERIAL_GPIO_SELECT.\n
/// GPIO0/1 are normally reserved for ID EEPROM.\n
/// Handshake lines CTS and RTS are not supported.

#if RASPPI < 4
	#define SERIAL_DEVICES		1
#else
	#define SERIAL_DEVICES		6
#endif

#define SERIAL_BUF_SIZE		2048			// must be a power of 2
#define SERIAL_BUF_MASK		(SERIAL_BUF_SIZE-1)

// serial options
#define SERIAL_OPTION_ONLCR	(1 << 0)	///< Translate NL to NL+CR on output (default)

// returned from Read/Write as negative value
#define SERIAL_ERROR_BREAK	1
#define SERIAL_ERROR_OVERRUN	2
#define SERIAL_ERROR_FRAMING	3

class CUartMaxiSerialDevice : public CDevice
{
public:
    enum UART_ERROR_CODES
    {
        UART_ERROR_NONE,
        UART_ERROR_BREAK,
        UART_ERROR_OVERRUN,
        UART_ERROR_FRAMING,
        UART_ERROR_FULL
    };

#ifndef USE_RPI_STUB_AT
	/// \param pInterruptSystem Pointer to interrupt system object (or 0 for polling driver)
	/// \param bUseFIQ Use FIQ instead of IRQ
	/// \param nDevice Device number (see: GPIO pin mapping)
	CUartMaxiSerialDevice (CInterruptSystem *pInterruptSystem, boolean bUseFIQ = FALSE,
		       unsigned nDevice = 0);

	~CUartMaxiSerialDevice (void);
#endif

	/// \param nBaudrate Baud rate in bits per second
	/// \return Operation successful?
	boolean Initialize (unsigned nBaudrate = 115200, unsigned nRxBufSize = 0, unsigned nTxBufSize = 0);

	// Output

	/// \param pBuffer Pointer to data to be sent
	/// \param nCount Number of bytes to be sent
	/// \return Number of bytes successfully sent (< 0 on error)
	int Write (const u8 *pBuffer, size_t nCount);

	// Write single char
	int Write (unsigned int ch);

	// Write string
	int WriteStr(const char* pStr);

	// Clear
	void Clear();

	/// \return Number of bytes buffer space available for Write()
	/// \note Does only work with interrupt driver.
	unsigned AvailableForWrite (void);

    // Input

	// Read byte
    int Read();

	/// \param pBuffer Pointer to buffer for received data
	/// \param nCount Maximum number of bytes to be received
	/// \return Number of bytes received (0 no data available, < 0 on error)
    unsigned Read(u8 *pBuffer, size_t nCount);

	// One or more bytes available
    bool CanGet();

	/// \return Next received byte which will be returned by Read() (-1 if no data available)
	/// \note Does only work with interrupt driver.
    int Peek();

	/// \return Number of bytes already received available for Read()
	/// \note Does only work with interrupt driver.
	unsigned AvailableForRead (void);

	// Get status
    UART_ERROR_CODES GetStatus()
    {
        UART_ERROR_CODES tmpCode = m_nRxStatus;
        m_nRxStatus = UART_ERROR_NONE;
        return tmpCode;
    }

	// Stats
	class UARTStats
	{
	public:
		UARTStats()
		{
			Clear();
		}
	    void Clear()
		{
			m_txBufferFullCount = 0;
			m_rxFramingErrCount = 0;
			m_rxOverrunErrCount = 0;
			m_rxBreakCount = 0;
			m_rxBufferFullCount = 0;
			m_isrCount = 0;
		}
		unsigned m_isrCount;
		unsigned m_rxFramingErrCount;
		unsigned m_rxOverrunErrCount;
		unsigned m_rxBreakCount;
		unsigned m_rxBufferFullCount;
	    unsigned m_txBufferFullCount;
	};
	const UARTStats& GetStats()
	{
		return m_uartStats;
	}

#ifndef USE_RPI_STUB_AT
	/// \return Serial options mask (see serial options)
	unsigned GetOptions (void) const;
	/// \param nOptions Serial options mask (see serial options)
	void SetOptions (unsigned nOptions);

	typedef void TMagicReceivedHandler (void);
	/// \param pMagic String for which is searched in the received data\n
	/// (must remain valid after return from this method)
	/// \param pHandler Handler which is called, when the magic string is found
	/// \note Does only work with interrupt driver.
	void RegisterMagicReceivedHandler (const char *pMagic, TMagicReceivedHandler *pHandler);

// protected:
// 	/// \brief Waits until all written bytes have been sent out
// 	void Flush (void);

private:
    int WriteBase(unsigned int c);
    void TxPumpPrime();
	void InterruptHandler (void);
	static void InterruptStub (void *pParam);

	bool SetClockRate(unsigned nClockId, unsigned nRate, bool bSkipTurbo);

	void AllocateBuffer(u8*& pBuffer, bool& bufRequiresDeletion, unsigned reqBufSize, 
			RingBufferPosn& bufPosn, u8* pFallbackBuffer, unsigned fallbackBufferSize);

private:
	CInterruptSystem *m_pInterruptSystem;
	boolean m_bUseFIQ;
	unsigned m_nDevice;
	uintptr  m_nBaseAddress;
	boolean  m_bValid;

#if SERIAL_GPIO_SELECT == 14
	CGPIOPin m_GPIO32;
	CGPIOPin m_GPIO33;
#endif
	CGPIOPin m_TxDPin;
	CGPIOPin m_RxDPin;

    // Rx Buffer & status
    uint8_t *m_pRxBuffer;
    RingBufferPosn m_rxBufferPosn;
	bool m_bRxBufRequiresDeletion;

	// Rx status
    UART_ERROR_CODES m_nRxStatus;

    // Tx Buffer
    uint8_t *m_pTxBuffer;
    RingBufferPosn m_txBufferPosn;
	bool m_bTxBufRequiresDeletion;

	// Stats
	UARTStats m_uartStats;

    // Fallback buffers
    static const int FALLBACK_BUFFER_SIZE = 10000;
    uint8_t m_rxFallbackBuffer[FALLBACK_BUFFER_SIZE];
    uint8_t m_txFallbackBuffer[FALLBACK_BUFFER_SIZE];

	unsigned m_nOptions;

	const char *m_pMagic;
	const char *m_pMagicPtr;
	TMagicReceivedHandler *m_pMagicReceivedHandler;

	CSpinLock m_SpinLock;
	CSpinLock m_LineSpinLock;

	static unsigned s_nInterruptUseCount;
	static CInterruptSystem *s_pInterruptSystem;
	static boolean s_bUseFIQ;
	static volatile u32 s_nInterruptDeviceMask;
	static CUartMaxiSerialDevice *s_pThis[SERIAL_DEVICES];

#endif
};

#endif
