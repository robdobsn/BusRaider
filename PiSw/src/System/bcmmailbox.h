//
// bcmmailbox.h
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
#ifndef _circle_bcmmailbox_h
#define _circle_bcmmailbox_h

#include "../System/BCM2835.h"
// #include "spinlock.h"
// #include "types.h"
#include <stdint.h>

class CBcmMailBox
{
public:
	CBcmMailBox (unsigned nChannel);
	~CBcmMailBox (void);

	uint32_t WriteRead (uint32_t nData);

private:
	void Flush (void);

	uint32_t Read (void);
	void Write (uint32_t nData);

private:
	unsigned m_nChannel;

	// static CSpinLock s_SpinLock;
};

#endif
