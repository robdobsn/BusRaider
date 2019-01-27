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
#include <circle/gpiopin.h>
#include <circle/timer.h>

#include "TargetBus/busraider.h"


// #include <circle/bcm2835.h>
// #include <circle/memio.h>


// void pinRawWrite(int pinNumber, bool val)
// {
//     if (val) {
//         if (pinNumber < 32)
//             write32(ARM_GPIO_GPSET0, 1 << pinNumber);
//         else
//             write32(ARM_GPIO_GPSET0+0x04, 1 << (pinNumber - 32));
//     } else {
//         if (pinNumber < 32)
//             write32(ARM_GPIO_GPCLR0, 1 << pinNumber);
//         else
//             write32(ARM_GPIO_GPCLR0+0x04, 1 << (pinNumber - 32));
//     }
// }

// void pinRawMode(int pinNumber, bool inputMode, bool val)
// {
//     if (!inputMode)
//         pinRawWrite(pinNumber, val);
//     uint32_t gpfSelReg = ARM_GPIO_GPFSEL0 + (pinNumber / 10) * 4;
//     uint8_t bitPos = ((pinNumber - ((pinNumber / 10) * 10)) % 10) * 3;
//     uint32_t regVal = read32(gpfSelReg);
//     regVal &= ~(7 << bitPos);
//     uint8_t modeVal = inputMode ? 0 : 1;
//     regVal |= (modeVal & 0x0f) << bitPos;
//     write32(gpfSelReg, regVal);
//     if (!inputMode)
//         pinRawWrite(pinNumber, val);
// }

// bool pinRawRead(int pinNumber)
// {
//     if (pinNumber < 32)
//         return ((read32(ARM_GPIO_GPLEV0) >> pinNumber) & 0x01) != 0;
//     return ((read32(ARM_GPIO_GPLEV0+0x04) >> (pinNumber - 32)) & 0x01) != 0;
// }

void DoChangeMachine(const char* mcName)
{

}

CKernel::CKernel (void)
// :
// 	m_commandHandler(DoChangeMachine)
{
}

CKernel::~CKernel (void)
{
}

boolean CKernel::Initialize (void)
{
	return TRUE;
}

TShutdownMode CKernel::Run (void)
{

    // Bus raider setup
    _busRaider.init();

	static const uint8_t program[] = { 0x21, 0x00, 0x3c, 0x11, 0x01, 0x3C, 0x36, 0x22, 0x01, 0x00, 0x44, 0xED, 0xB0, 0xC3, 0x0D, 0x00 };

	// pinRawMode(8, false, 0);
	// for (int i = 0; i < 10; i++)
	// {
	// 	pinRawWrite(8, 1);
	// 	CTimer::SimpleusDelay(1);
	// 	pinRawWrite(8, 0);
	// 	CTimer::SimpleusDelay(1);
	// }

	CGPIOPin AudioLeft (GPIOPinAudioLeft, GPIOModeOutput);
	CGPIOPin AudioRight (GPIOPinAudioRight, GPIOModeOutput);

	_busRaider.blockWrite(0, program, sizeof(program), true, false);
	_busRaider.hostReset();

	// flash the Act LED 10 times and click on audio (3.5mm headphone jack)
	for (unsigned i = 1; i <= sizeof(program); i++)
	{
		m_ActLED.On ();
		AudioLeft.Invert ();
		AudioRight.Invert ();
		CTimer::SimpleMsDelay (200);

		m_ActLED.Off ();
		CTimer::SimpleMsDelay (500);

	// 	_busRaider.controlReqAndTake();
	// 	_busRaider.controlRelease(0);

	}

	return ShutdownReboot;
}
