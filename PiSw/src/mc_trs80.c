// Bus Raider TRS80 Support
// Rob Dobson 2018

#include "mc_trs80.h"
#include "nmalloc.h"
#include "ee_printf.h"

static uint8_t* __trs80ScreenBuffer = NULL;

static void trs80_init()
{
	// Allocate storage for display
	McGenericDescriptor* pMcDescr = mc_generic_get();
	__trs80ScreenBuffer = nmalloc_malloc(pMcDescr->displayBufSize);
}

static void trs80_deinit()
{
	// Allocate storage for display
	if (__trs80ScreenBuffer)
		nmalloc_free((void*)__trs80ScreenBuffer);	
}

static const char FromTRS80[] = "trs80";

static void trs80_keyHandler(unsigned char ucModifiers, const unsigned char rawKeys[6])
{
	LogWrite(FromTRS80, 4, "Key %02x %02x", ucModifiers, rawKeys[0]);
}

static void trs80_displayHandler(uint8_t* pDispBuf, int dispBufLen)
{
	LogWrite(FromTRS80, 4, "Disp %d %d", pDispBuf, dispBufLen);
}

static McGenericDescriptor trs80_descr =
{
	// Initialisation function
	.pInit = trs80_init,
	.pDeInit = trs80_deinit,
	// Display
	.bMemoryMapped = 1,
	.displayStartAddr = 0x3c00,
	.displayBufSize = 0x0400,
	.displayRefreshRatePerSec = 25,
	// Keyboard
	.pKeyHandler = trs80_keyHandler,
	.pDispHandler = trs80_displayHandler
};

McGenericDescriptor* trs80_get_descriptor(int subType)
{
	subType = subType;
	return &trs80_descr;
}

