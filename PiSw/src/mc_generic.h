// Bus Raider Machine Type
// Rob Dobson 2018

#pragma once
#include "globaldefs.h"

typedef void TMcInitFunction();
typedef void TMcDeInitFunction();
typedef void TMcKeyHandlerRaw(unsigned char ucModifiers, const unsigned char rawKeys[6]);
typedef void TMcDispHandler(uint8_t* pDispBuf, int dispBufLen);

typedef struct McGenericDescriptor
{
	// Initialisation/Deinit
	TMcInitFunction* pInit;
	TMcDeInitFunction* pDeInit;
	// Display
	int bMemoryMapped;
	int displayStartAddr;
	int displayBufSize;
	int displayRefreshRatePerSec;
	// Keyboard
	TMcKeyHandlerRaw* pKeyHandler;
	TMcDispHandler* pDispHandler;
} McGenericDescriptor;

extern void mc_generic_set(const char* mcName);
extern void mc_generic_restore();
extern McGenericDescriptor* mc_generic_get();

extern void mc_generic_handle_key(unsigned char ucModifiers, const unsigned char rawKeys[6]);
extern void mc_generic_handle_disp(uint8_t* pDispBuf, int dispBufLen);
