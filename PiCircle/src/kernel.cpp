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
#include <circle/debug.h>
#include <assert.h>

#include "testTiming.h"

static const char FromKernel[] = "kernel";

CKernel::CKernel (void)
:	m_Memory (TRUE),
	m_Screen (m_Options.GetWidth (), m_Options.GetHeight ()),
	m_Logger (m_Options.GetLogLevel ())
{
	m_ActLED.Blink (5);	// show we are alive
}

CKernel::~CKernel (void)
{
}

boolean CKernel::Initialize (void)
{
	boolean bOK = TRUE;

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
	
	return bOK;
}

TShutdownMode CKernel::Run (void)
{
	m_Logger.Write (FromKernel, LogNotice, "Compile time: " __DATE__ " " __TIME__);


	testTiming(10);

	return ShutdownReboot;
}


#ifdef OLD_BOLD

#include "kernel.h"
#include <circle/gpiopin.h>
#include <circle/timer.h>
#include <circle/machineinfo.h>
#include <string.h>

#include "TargetBus/busraider.h"

static const char FromKernel[] = "kernel";

void DoChangeMachine(const char* mcName)
{

}

CKernel::CKernel (void)
:	m_Memory (TRUE),
	m_Screen (1366,768),
	m_Logger (4)
// :
// 	m_commandHandler(DoChangeMachine)
{
	m_ActLED.Blink (5);	// show we are alive
}

CKernel::~CKernel (void)
{
}

boolean CKernel::Initialize (void)
{
	boolean bOK = TRUE;

	if (bOK)
	{
		bOK = m_Screen.Initialize ();
	}
	
	if (bOK)
	{
		bOK = m_Serial.Initialize (115200);
	}
	
	m_Screen.Write("Here\n",5);

	CMachineInfo* pMCInfo = CMachineInfo::Get();
	if (pMCInfo)
	{
		// const char* pMC = pMCInfo->GetMachineName();
		m_Screen.Write("Here2\n",5);
		// m_Screen.Write(pMC, strlen(pMC));
		int model = pMCInfo->GetMachineModel();
		char pb[] = "A\n";
		pb[0] = 'A' + model;
		m_Screen.Write(pb,3);
	}

	//m_Logger.Initialize (&m_Screen);

	// if (bOK)
	// {
	// 	CDevice *pTarget = &m_Serial;
	// 	bool bbb = m_Logger.Initialize (pTarget);
	// 	if (!bbb)
	// 		m_Screen.Write("Fail\n",5);
	// }
	
	return bOK;
}

TShutdownMode CKernel::Run (void)
{
	m_Screen.Write ("hello\n", 6);

	// m_Logger.Write (FromKernel, LogNotice, "Compile time: " __DATE__ " " __TIME__);

    // Bus raider setup
    // _busRaider.init();

	// static const uint8_t program[] = { 0x21, 0x00, 0x3c, 0x11, 0x01, 0x3C, 0x36, 0x22, 0x01, 0x00, 0x44, 0xED, 0xB0, 0xC3, 0x0D, 0x00 };

	// pinRawMode(8, false, 0);
	// for (int i = 0; i < 10; i++)
	// {
	// 	pinRawWrite(8, 1);
	// 	CTimer::SimpleusDelay(1);
	// 	pinRawWrite(8, 0);
	// 	CTimer::SimpleusDelay(1);
	// }

	// CGPIOPin AudioLeft (GPIOPinAudioLeft, GPIOModeOutput);
	// CGPIOPin AudioRight (GPIOPinAudioRight, GPIOModeOutput);

	// // _busRaider.blockWrite(0, program, sizeof(program), true, false);
	// // _busRaider.hostReset();

	// // flash the Act LED 10 times and click on audio (3.5mm headphone jack)
	// for (unsigned i = 1; i <= 10; i++)
	// {
	// 	m_ActLED.On ();
	// 	AudioLeft.Invert ();
	// 	AudioRight.Invert ();
	// 	CTimer::SimpleMsDelay (200);

	// 	m_ActLED.Off ();
	// 	CTimer::SimpleMsDelay (500);

	// 	m_Screen.Write ("hello\n", 6);

	// // 	_busRaider.controlReqAndTake();
	// // 	_busRaider.controlRelease(0);

	// }

	CTimer::SimpleMsDelay(5000);

	return ShutdownReboot;
}

#endif
