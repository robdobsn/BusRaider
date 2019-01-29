//
// kernel.cpp
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2014  R. Stange <rsta2@o2online.de>
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
#include <circle/debug.h>
#include <circle/timer.h>
#include <assert.h>
#include <string.h>

static const char FromKernel[] = "kernel";

CKernel::CKernel (void)
:	m_Screen (m_Options.GetWidth (), m_Options.GetHeight ()),
	m_Serial (&m_Interrupt),
	m_Timer (&m_Interrupt),
	m_Logger (m_Options.GetLogLevel (), &m_Timer)
{
	// m_ActLED.Blink (5);	// show we are alive
}

CKernel::~CKernel (void)
{
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
		CDevice *pTarget = m_DeviceNameService.GetDevice (m_Options.GetLogDevice (), FALSE);
		if (pTarget == 0)
		{
			pTarget = &m_Screen;
		}

		bOK = m_Logger.Initialize (pTarget);
	}

	if (bOK)
	{
		bOK = m_Timer.Initialize ();
	}

	return bOK;
}

TShutdownMode CKernel::Run (void)
{
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
			}
			if (m_Timer.GetTicks() > curTicks + 500)
				break;
		// }
		}
			// const char* sss = "Compile time: " __DATE__ " " __TIME__ "\n";
			// m_Serial.Write(sss, strlen(sss));
			
			// Receive chars

		CString sss;
		sss.Format("Loop %d (of %d) Recived bytes %d (total %d) Time in ticks %d ~~~", i+1, maxLoops, rxLoop, rxTotal, m_Timer.GetTicks());
		// const char* sss = "Compile time: " __DATE__ " " __TIME__ "\n";
		m_Serial.Write((const char*)sss, sss.GetLength());
		// m_Logger.Write (FromKernel, LogNotice, "Compile time: " __DATE__ " " __TIME__);
	}

	m_Serial.Write("Restarting\n", 11);
	m_Serial.Flush();

// 	// show the character set on screen
// 	for (char chChar = ' '; chChar <= '~'; chChar++)
// 	{
// 		if (chChar % 8 == 0)
// 		{
// 			m_Screen.Write ("\n", 1);
// 		}

// 		CString Message;
// 		Message.Format ("%02X: \'%c\' ", (unsigned) chChar, chChar);
		
// 		m_Screen.Write ((const char *) Message, Message.GetLength ());
// 	}
// 	m_Screen.Write ("\n", 1);

// #ifndef NDEBUG
// 	// some debugging features
// 	m_Logger.Write (FromKernel, LogDebug, "Dumping the start of the ATAGS");
// 	debug_hexdump ((void *) 0x100, 128, FromKernel);

// 	m_Logger.Write (FromKernel, LogNotice, "The following assertion will fail");
// 	// assert (1 == 2);
// #endif
	// CTimer::SimpleMsDelay(1000);
	// }
	return ShutdownReboot;
}
