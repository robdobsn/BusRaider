//
// bcmmailbox.cpp
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2014-2018  R. Stange <rsta2@o2online.de>
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
#include "bcmmailbox.h"
// #include "memio.h"
// #include "synchronize.h"
// #include <timer.h>
#include "../System/lowlib.h"
// #include "assert.h"

// CSpinLock CBcmMailBox::s_SpinLock (TASK_LEVEL);

CBcmMailBox::CBcmMailBox (unsigned nChannel)
:	m_nChannel (nChannel)
{
}

CBcmMailBox::~CBcmMailBox (void)
{
}

uint32_t CBcmMailBox::WriteRead (uint32_t nData)
{
	PeripheralEntry ();

	// s_SpinLock.Acquire ();

	Flush ();

	Write (nData);

	uint32_t nResult = Read ();

	// s_SpinLock.Release ();

	PeripheralExit ();

	return nResult;
}

void CBcmMailBox::Flush (void)
{
	while (!(RD32 (MAILBOX0_STATUS) & MAILBOX_STATUS_EMPTY))
	{
		RD32 (MAILBOX0_READ);

		microsDelay(20000);
	}
}

uint32_t CBcmMailBox::Read (void)
{
	uint32_t nResult;
	
	do
	{
		while (RD32 (MAILBOX0_STATUS) & MAILBOX_STATUS_EMPTY)
		{
			// do nothing
		}
		
		nResult = RD32 (MAILBOX0_READ);
	}
	while ((nResult & 0xF) != m_nChannel);		// channel number is in the lower 4 bits

	return nResult & ~0xF;
}

void CBcmMailBox::Write (uint32_t nData)
{
	while (RD32 (MAILBOX1_STATUS) & MAILBOX_STATUS_FULL)
	{
		// do nothing
	}

	// assert ((nData & 0xF) == 0);
	WR32 (MAILBOX1_WRITE, m_nChannel | nData);	// channel number is in the lower 4 bits
}