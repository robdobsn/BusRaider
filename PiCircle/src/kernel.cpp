//
// kernel.cpp
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2016  R. Stange <rsta2@o2online.de>
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

#include "kernel.h"
#include <circle/string.h>
#include <assert.h>
#include <string.h>
#include <circle/debug.h>
#include <circle/timer.h>

static const char FromKernel[] = "kernel";

CKernel *CKernel::s_pThis = 0;

CKernel::CKernel (void)
:	m_Screen (m_Options.GetWidth (), m_Options.GetHeight ()),
	m_Serial (&m_Interrupt),
	m_Timer (&m_Interrupt),
	m_Logger (m_Options.GetLogLevel (), &m_Timer),
#ifdef _INCLUDE_USB_
	m_DWHCI (&m_Interrupt, &m_Timer),
#endif
	m_ShutdownMode (ShutdownNone)
{
#ifdef _INCLUDE_USB_
    m_pKeyboard = NULL;
#endif
	s_pThis = this;
}

CKernel::~CKernel (void)
{
	s_pThis = 0;
}

boolean CKernel::Initialize (void)
{
	boolean bOK = TRUE;

	if (bOK)
	{
		bOK = m_Interrupt.Initialize ();
	}

	if (bOK)
	{
		bOK = m_Screen.Initialize ();
	}
	
	if (bOK)
	{
		bOK = m_Serial.Initialize (115200);
	}
	
	if (bOK)
	{
		CDevice *pTarget = 0;  m_DeviceNameService.GetDevice (m_Options.GetLogDevice (), FALSE);
		if (pTarget == 0)
		{
			pTarget = &m_Serial;
		}

		bOK = m_Logger.Initialize (pTarget);
	}
	
	if (bOK)
	{
		bOK = m_Timer.Initialize ();
	}

#ifdef _INCLUDE_USB_
	if (bOK)
	{
		bOK = m_DWHCI.Initialize ();
	}
#endif

	return bOK;
}

TShutdownMode CKernel::Run (void)
{
#ifdef _INCLUDE_USB_
	m_pKeyboard = (CUSBKeyboardDevice *) m_DeviceNameService.GetDevice ("ukbd1", FALSE);
	if (m_pKeyboard == 0)
	{
		m_Logger.Write (FromKernel, LogError, "Keyboard not found");
	}
    else
    {
#if 1	// set to 0 to test raw mode
        m_pKeyboard->RegisterShutdownHandler (ShutdownHandler);
        m_pKeyboard->RegisterKeyPressedHandler (KeyPressedHandler);
#else
    	m_pKeyboard->RegisterKeyStatusHandlerRaw (KeyStatusHandlerRaw);
#endif
    }
#endif

	int rxTotal = 0;
	int rxLoop = 0;
	int maxLoops = 3;
	for (int i = 0; i < maxLoops; i++)
	{
		// m_Serial.Write("Listening\n", 10);
		// int curTicks = m_Timer.GetTicks();
		rxLoop = 0;
		// while (m_Timer.GetTicks() < curTicks + 5000)
		// {
		unsigned int curTicks = m_Timer.GetTicks();
		while(1)
		{
			unsigned char buf[1000];
			int numRead = m_Serial.Read(buf, sizeof buf);
			if (numRead > 0)
			{
				rxTotal += numRead;
				rxLoop += numRead;
                _busRaider.handleSerialRxBuffer(buf, numRead);
				// _miniHDLC.handleBuffer(buf, numRead);
			}
			if (m_Timer.GetTicks() > curTicks + 1000)
				break;
		// }
		}
			// const char* sss = "Compile time: " __DATE__ " " __TIME__ "\n";
			// m_Serial.Write(sss, strlen(sss));
			
			// Receive chars

		CString sss;
		m_Logger.Write(FromKernel, LogNotice, "Hello there");
		sss.Format("Loop %d (of %d) Received bytes %d (total %d) Time in ticks %d ~~~\n", i+1, maxLoops, rxLoop, rxTotal, m_Timer.GetTicks());
		// const char* sss = "Compile time: " __DATE__ " " __TIME__ "\n";
		m_Serial.Write((const char*)sss, sss.GetLength());
		// m_Logger.Write (FromKernel, LogNotice, "Compile time: " __DATE__ " " __TIME__);
	}

	m_Serial.Write("Restarting\n", 11);
	// m_Serial.Flush();
    CTimer::SimpleMsDelay(1000);

	return ShutdownReboot;
}

#ifdef _INCLUDE_USB_

void CKernel::KeyPressedHandler (const char *pString)
{
	assert (s_pThis != 0);
	s_pThis->m_Serial.Write (pString, strlen (pString));
}

void CKernel::ShutdownHandler (void)
{
	assert (s_pThis != 0);
	s_pThis->m_ShutdownMode = ShutdownReboot;
}

void CKernel::KeyStatusHandlerRaw (unsigned char ucModifiers, const unsigned char RawKeys[6])
{
	assert (s_pThis != 0);

	CString Message;
	Message.Format ("Key status (modifiers %02X)", (unsigned) ucModifiers);

	for (unsigned i = 0; i < 6; i++)
	{
		if (RawKeys[i] != 0)
		{
			CString KeyCode;
			KeyCode.Format (" %02X", (unsigned) RawKeys[i]);

			Message.Append (KeyCode);
		}
	}

	s_pThis->m_Logger.Write (FromKernel, LogNotice, Message);
}

#endif