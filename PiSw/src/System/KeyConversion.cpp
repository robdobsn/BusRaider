// Bus Raider
// Rob Dobson 2019

#include "KeyConversion.h"

const char* KeyConversion::_keyboardTypeStrs[] = 
{
    "US", "UK", "FR", "DE", "IT", "ES"
};

uint32_t KeyConversion::getNumTypes()
{
    return sizeof(_keyboardTypeStrs)/sizeof(_keyboardTypeStrs[0]);
}

// order must match TSpecialKey beginning at KeySpace
const char *KeyConversion::s_KeyStrings[KeyMaxCode-KeySpace] =
{
	" ",			// KeySpace
	"\x1b",			// KeyEscape
	"\x08",			// KeyBackspace
	"\t",			// KeyTabulator
	"\x0d",			// KeyReturn
	"\x1b[2~",		// KeyInsert
	"\x1b[1~",		// KeyHome
	"\x1b[5~",		// KeyPageUp
	"\x7f",		    // KeyDelete
	"\x1b[4~",		// KeyEnd
	"\x1b[6~",		// KeyPageDown
	"\x1b[A",		// KeyUp
	"\x1b[B",		// KeyDown
	"\x1b[D",		// KeyLeft
	"\x1b[C",		// KeyRight
	"\x1b[[A",		// KeyF1
	"\x1b[[B",		// KeyF2
	"\x1b[[C",		// KeyF3
	"\x1b[[D",		// KeyF4
	"\x1b[[E",		// KeyF5
	"\x1b[17~",		// KeyF6
	"\x1b[18~",		// KeyF7
	"\x1b[19~",		// KeyF8
	"\x1b[20~",		// KeyF9
	0,			// KeyF10
	0,			// KeyF11
	0,			// KeyF12
	0,			// KeyApplication
	0,			// KeyCapsLock
	0,			// KeyPrintScreen
	0,			// KeyScrollLock
	0,			// KeyPause
	0,			// KeyNumLock
	"/",			// KeyKP_Divide
	"*",			// KeyKP_Multiply
	"-",			// KeyKP_Subtract
	"+",			// KeyKP_Add
	"\n",			// KeyKP_Enter
	"1",			// KeyKP_1
	"2",			// KeyKP_2
	"3",			// KeyKP_3
	"4",			// KeyKP_4
	"5",			// KeyKP_5
	"6",			// KeyKP_6
	"7",			// KeyKP_7
	"8",			// KeyKP_8
	"9",			// KeyKP_9
	"0",			// KeyKP_0
	"\x1b[G",		// KeyKP_Center
	",",			// KeyKP_Comma
	"."			// KeyKP_Period
};

uint16_t KeyConversion::_hidCodeConversion[][PHY_MAX_CODE+1][K_ALTSHIFTTAB+1] =
{
    {
#include "../../uspi/lib/keymap_us.h"
    },
    {
#include "../../uspi/lib/keymap_uk.h"
    },
    {
#include "../../uspi/lib/keymap_fr.h"
    },
    {
#include "../../uspi/lib/keymap_de.h"
    },
    {
#include "../../uspi/lib/keymap_it.h"
    },
    {
#include "../../uspi/lib/keymap_es.h"
    }
};
