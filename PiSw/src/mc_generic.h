// Bus Raider Machine Type
// Rob Dobson 2018

#pragma once
#include "globaldefs.h"
#include "wgfxfont.h"

typedef void TMcInitFunction();
typedef void TMcDeInitFunction();
typedef void TMcKeyHandlerRaw(unsigned char ucModifiers, const unsigned char rawKeys[6]);
typedef void TMcDispHandler();

typedef struct McGenericDescriptor
{
	// Initialisation/Deinit
	TMcInitFunction* pInit;
	TMcDeInitFunction* pDeInit;
	// Display
	int displayRefreshRatePerSec;
	int displayPixelsX;
	int displayPixelsY;
	int displayCellX;
	int displayCellY;
	WgfxFont* pFont;
	// Keyboard
	TMcKeyHandlerRaw* pKeyHandler;
	TMcDispHandler* pDispHandler;
} McGenericDescriptor;

extern void mc_generic_set(const char* mcName);
extern void mc_generic_restore();
extern McGenericDescriptor* mc_generic_get_descriptor();

extern void mc_generic_handle_key(unsigned char ucModifiers, const unsigned char rawKeys[6]);
extern void mc_generic_handle_disp();
