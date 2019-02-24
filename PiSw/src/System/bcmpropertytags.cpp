//
// bcmpropertytags.cpp
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
#include "bcmpropertytags.h"
// #include <circle/util.h>
// #include "synchronize.h"
#include "memorymap.h"

// #include <circle/bcm2835.h>
// #include <circle/memory.h>
// #include "macros.h"
// #include "assert.h"
#include <string.h>

struct TPropertyBuffer
{
	uint32_t	nBufferSize;			// bytes
	uint32_t	nCode;
	#define CODE_REQUEST		0x00000000
	#define CODE_RESPONSE_SUCCESS	0x80000000
	#define CODE_RESPONSE_FAILURE	0x80000001
	uint8_t	Tags[0];
	// end tag follows
}
PACKED;

CBcmPropertyTags::CBcmPropertyTags (void)
:	m_MailBox (BCM_MAILBOX_PROP_OUT)
{
}

CBcmPropertyTags::~CBcmPropertyTags (void)
{
}

bool CBcmPropertyTags::GetTag (uint32_t nTagId, void *pTag, unsigned nTagSize, unsigned nRequestParmSize)
{
	// assert (pTag != 0);
	// assert (nTagSize >= sizeof (TPropertyTagSimple));

	TPropertyTag *pHeader = (TPropertyTag *) pTag;
	pHeader->nTagId = nTagId;
	pHeader->nValueBufSize = nTagSize - sizeof (TPropertyTag);
	pHeader->nValueLength = nRequestParmSize & ~VALUE_LENGTH_RESPONSE;

	if (!GetTags (pTag, nTagSize))
	{
		return false;
	}

	if (!(pHeader->nValueLength & VALUE_LENGTH_RESPONSE))
	{
		return false;
	}

	pHeader->nValueLength &= ~VALUE_LENGTH_RESPONSE;
	if (pHeader->nValueLength == 0)
	{
		return false;
	}

	return true;
}

#define COHERENT_SLOT_PROP_MAILBOX	0
#define COHERENT_SLOT_GPIO_VIRTBUF	1
#define COHERENT_SLOT_TOUCHBUF		2

uint32_t CBcmPropertyTags::GetCoherentPage (unsigned nSlot)
{
	uint32_t nPageAddress = (uint32_t) MEM_COHERENT_REGION;
	nPageAddress += nSlot * PAGE_SIZE;
	return nPageAddress;
}

bool CBcmPropertyTags::GetTags (void *pTags, unsigned nTagsSize)
{
	// assert (pTags != 0);
	// assert (nTagsSize >= sizeof (TPropertyTagSimple));
	unsigned nBufferSize = sizeof (TPropertyBuffer) + nTagsSize + sizeof (uint32_t);
	// assert ((nBufferSize & 3) == 0);

	TPropertyBuffer *pBuffer =
		(TPropertyBuffer *) GetCoherentPage (COHERENT_SLOT_PROP_MAILBOX);

	pBuffer->nBufferSize = nBufferSize;
	pBuffer->nCode = CODE_REQUEST;
	memcpy (pBuffer->Tags, pTags, nTagsSize);

	uint32_t *pEndTag = (uint32_t *) (pBuffer->Tags + nTagsSize);
	*pEndTag = PROPTAG_END;

	lowlev_dsb();

	uint32_t nBufferAddress = BUS_ADDRESS ((unsigned int) pBuffer);
	if (m_MailBox.WriteRead (nBufferAddress) != nBufferAddress)
	{
		return false;
	}

	lowlev_dmb();

	if (pBuffer->nCode != CODE_RESPONSE_SUCCESS)
	{
		return false;
	}

	memcpy (pTags, pBuffer->Tags, nTagsSize);

	return true;
}
