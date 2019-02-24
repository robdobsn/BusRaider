//
// bcmpropertytags.h
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
#ifndef _circle_bcmpropertytags_h
#define _circle_bcmpropertytags_h

#include "bcmmailbox.h"
#include "lowlib.h"
// #include <circle/macros.h>
// #include "types.h"

#define PROPTAG_END			0x00000000

#define PROPTAG_GET_FIRMWARE_REVISION	0x00000001
#define PROPTAG_SET_CURSOR_INFO		0x00008010
#define PROPTAG_SET_CURSOR_STATE	0x00008011
#define PROPTAG_GET_BOARD_MODEL		0x00010001
#define PROPTAG_GET_BOARD_REVISION	0x00010002
#define PROPTAG_GET_MAC_ADDRESS		0x00010003
#define PROPTAG_GET_BOARD_SERIAL	0x00010004
#define PROPTAG_GET_ARM_MEMORY		0x00010005
#define PROPTAG_GET_VC_MEMORY		0x00010006
#define PROPTAG_SET_POWER_STATE		0x00028001
#define PROPTAG_GET_CLOCK_RATE		0x00030002
#define PROPTAG_GET_MAX_CLOCK_RATE	0x00030004
#define PROPTAG_GET_TEMPERATURE		0x00030006
#define PROPTAG_GET_MIN_CLOCK_RATE	0x00030007
#define PROPTAG_GET_TURBO		0x00030009
#define PROPTAG_GET_MAX_TEMPERATURE	0x0003000A
#define PROPTAG_GET_EDID_BLOCK		0x00030020
#define PROPTAG_SET_CLOCK_RATE		0x00038002
#define PROPTAG_SET_TURBO		0x00038009
#define PROPTAG_ALLOCATE_BUFFER		0x00040001
#define PROPTAG_GET_DISPLAY_DIMENSIONS	0x00040003
#define PROPTAG_GET_PITCH		0x00040008
#define PROPTAG_GET_TOUCHBUF		0x0004000F
#define PROPTAG_GET_GPIO_VIRTBUF	0x00040010
#define PROPTAG_SET_PHYS_WIDTH_HEIGHT	0x00048003
#define PROPTAG_SET_VIRT_WIDTH_HEIGHT	0x00048004
#define PROPTAG_SET_DEPTH		0x00048005
#define PROPTAG_SET_VIRTUAL_OFFSET	0x00048009
#define PROPTAG_SET_PALETTE		0x0004800B
#define PROPTAG_WAIT_FOR_VSYNC		0x0004800E
#define PROPTAG_SET_TOUCHBUF		0x0004801F
#define PROPTAG_SET_GPIO_VIRTBUF	0x00048020
#define PROPTAG_GET_COMMAND_LINE	0x00050001

struct TPropertyTag
{
	uint32_t	nTagId;
	uint32_t	nValueBufSize;			// bytes, multiple of 4
	uint32_t	nValueLength;			// bytes
	#define VALUE_LENGTH_RESPONSE	(1 << 31)
	//uint8_t	ValueBuffer[0];			// must be padded to be 4 byte aligned
}
PACKED;

struct TPropertyTagSimple
{
	TPropertyTag	Tag;
	uint32_t		nValue;
}
PACKED;

struct TPropertyTagSetCursorInfo
{
	TPropertyTag	Tag;
	union
	{
		uint32_t	nWidth;			// should be >= 16
		uint32_t	nResponse;
	#define CURSOR_RESPONSE_VALID	0	// response
	}
	PACKED;
	uint32_t		nHeight;		// should be >= 16
	uint32_t		nUnused;
	uint32_t		nPixelPointer;		// physical address, format 32bpp ARGB
	uint32_t		nHotspotX;
	uint32_t		nHotspotY;
}
PACKED;

struct TPropertyTagSetCursorState
{
	TPropertyTag	Tag;
	union
	{
		uint32_t	nEnable;
	#define CURSOR_ENABLE_INVISIBLE		0
	#define CURSOR_ENABLE_VISIBLE		1
		uint32_t	nResponse;
	}
	PACKED;
	uint32_t		nPosX;
	uint32_t		nPosY;
	uint32_t		nFlags;
	#define CURSOR_FLAGS_DISP_COORDS	0
	#define CURSOR_FLAGS_FB_COORDS		1
}
PACKED;

struct TPropertyTagMACAddress
{
	TPropertyTag	Tag;
	uint8_t		Address[6];
	uint8_t		Padding[2];
}
PACKED;

struct TPropertyTagSerial
{
	TPropertyTag	Tag;
	uint32_t		Serial[2];
}
PACKED;

struct TPropertyTagMemory
{
	TPropertyTag	Tag;
	uint32_t		nBaseAddress;
	uint32_t		nSize;			// bytes
}
PACKED;

struct TPropertyTagPowerState
{
	TPropertyTag	Tag;
	uint32_t		nDeviceId;
	#define DEVICE_ID_SD_CARD	0
	#define DEVICE_ID_USB_HCD	3
	uint32_t		nState;
	#define POWER_STATE_OFF		(0 << 0)
	#define POWER_STATE_ON		(1 << 0)
	#define POWER_STATE_WAIT	(1 << 1)
	#define POWER_STATE_NO_DEVICE	(1 << 1)	// in response
}
PACKED;

struct TPropertyTagClockRate
{
	TPropertyTag	Tag;
	uint32_t		nClockId;
	#define CLOCK_ID_EMMC		1
	#define CLOCK_ID_UART		2
	#define CLOCK_ID_ARM		3
	#define CLOCK_ID_CORE		4
	uint32_t		nRate;			// Hz
}
PACKED;

struct TPropertyTemperature
{
	TPropertyTag	Tag;
	uint32_t		nTemperatureId;
	#define TEMPERATURE_ID		0
	uint32_t		nValue;			// degree Celsius * 1000
}
PACKED;
#define TPropertyTagTemperature		TPropertyTemperature

struct TPropertyTagTurbo
{
	TPropertyTag	Tag;
	uint32_t		nTurboId;
	#define TURBO_ID		0
	uint32_t		nLevel;
	#define TURBO_OFF		0
	#define TURBO_ON		1
}
PACKED;

struct TPropertyTagEDIDBlock
{
	TPropertyTag	Tag;
	uint32_t		nBlockNumber;
	#define EDID_FIRST_BLOCK	0
	uint32_t		nStatus;
	#define EDID_STATUS_SUCCESS	0
	uint8_t		Block[128];
}
PACKED;

struct TPropertyTagSetClockRate
{
	TPropertyTag	Tag;
	uint32_t		nClockId;
	uint32_t		nRate;			// Hz
	uint32_t		nSkipSettingTurbo;
	#define SKIP_SETTING_TURBO	1	// when setting ARM clock
}
PACKED;

struct TPropertyTagAllocateBuffer
{
	TPropertyTag	Tag;
	union
	{
		uint32_t	nAlignment;		// in bytes
		uint32_t	nBufferBaseAddress;
	}
	PACKED;
	uint32_t		nBufferSize;
}
PACKED;

struct TPropertyTagDisplayDimensions
{
	TPropertyTag	Tag;
	uint32_t		nWidth;
	uint32_t		nHeight;
}
PACKED;

struct TPropertyTagVirtualOffset
{
	TPropertyTag	Tag;
	uint32_t		nOffsetX;
	uint32_t		nOffsetY;
}
PACKED;

struct TPropertyTagSetPalette
{
	TPropertyTag	Tag;
	union
	{
		uint32_t	nOffset;		// first palette index to set (0-255)
		uint32_t	nResult;
	#define SET_PALETTE_VALID	0
	}
	PACKED;
	uint32_t		nLength;		// number of palette entries to set (1-256)
	uint32_t		Palette[0];		// RGBA values, offset to offset+length-1
}
PACKED;

struct TPropertyTagCommandLine
{
	TPropertyTag	Tag;
	uint8_t		String[2048];
}
PACKED;

class CBcmPropertyTags
{
public:
	CBcmPropertyTags (void);
	~CBcmPropertyTags (void);

	uint32_t GetCoherentPage (unsigned nSlot);

	bool GetTag (uint32_t	  nTagId,			// tag identifier
			void	 *pTag,				// pointer to tag struct
			unsigned  nTagSize,			// size of tag struct
			unsigned  nRequestParmSize = 0);	// number of parameter bytes
	
	bool GetTags (void	 *pTags,			// pointer to tags struct
			 unsigned nTagsSize);			// size of tags struct

private:
	CBcmMailBox m_MailBox;
};

#endif
