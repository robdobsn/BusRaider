// Bus Raider
// Rob Dobson 2019

#pragma once

#include <stdint.h>
#include <circle/util.h>
#include "usb_hid_keys.h"
#include "logging.h"
#include "lowlib.h"

#define PHY_MAX_CODE	127

#define K_NORMTAB	0
#define K_SHIFTTAB	1
#define K_ALTTAB	2
#define K_ALTSHIFTTAB	3

#define C(chr)		((uint16_t) (uint8_t) (chr))

typedef enum
{
	KeyNone  = 0x00,
	KeySpace = 0x100,
	KeyEscape,
	KeyBackspace,
	KeyTabulator,
	KeyReturn,
	KeyInsert,
	KeyHome,
	KeyPageUp,
	KeyDelete,
	KeyEnd,
	KeyPageDown,
	KeyUp,
	KeyDown,
	KeyLeft,
	KeyRight,
	KeyF1,
	KeyF2,
	KeyF3,
	KeyF4,
	KeyF5,
	KeyF6,
	KeyF7,
	KeyF8,
	KeyF9,
	KeyF10,
	KeyF11,
	KeyF12,
	KeyApplication,
	KeyCapsLock,
	KeyPrintScreen,
	KeyScrollLock,
	KeyPause,
	KeyNumLock,
	KeyKP_Divide,
	KeyKP_Multiply,
	KeyKP_Subtract,
	KeyKP_Add,
	KeyKP_Enter,
	KeyKP_1,
	KeyKP_2,
	KeyKP_3,
	KeyKP_4,
	KeyKP_5,
	KeyKP_6,
	KeyKP_7,
	KeyKP_8,
	KeyKP_9,
	KeyKP_0,
	KeyKP_Center,
	KeyKP_Comma,
	KeyKP_Period,
	KeyMaxCode
}
TSpecialKey;

// typedef enum
// {
// 	ActionSwitchCapsLock = KeyMaxCode,
// 	ActionSwitchNumLock,
// 	ActionSwitchScrollLock,
// 	ActionSelectConsole1,
// 	ActionSelectConsole2,
// 	ActionSelectConsole3,
// 	ActionSelectConsole4,
// 	ActionSelectConsole5,
// 	ActionSelectConsole6,
// 	ActionSelectConsole7,
// 	ActionSelectConsole8,
// 	ActionSelectConsole9,
// 	ActionSelectConsole10,
// 	ActionSelectConsole11,
// 	ActionSelectConsole12,
// 	ActionShutdown,
// 	ActionNone
// }
// TSpecialAction;

enum {
    KEYBOARD_TYPE_SEL_US = 0,
    KEYBOARD_TYPE_SEL_UK,
    KEYBOARD_TYPE_SEL_FR,
    KEYBOARD_TYPE_SEL_DE,
    KEYBOARD_TYPE_SEL_IT,
    KEYBOARD_TYPE_SEL_ES
};
class KeyConversion
{
public:
    KeyConversion()
    {
        _curKeyboardType = KEYBOARD_TYPE_SEL_US;
    }
    // Keyboard translation tables for different countries    
    static uint16_t _hidCodeConversion[][PHY_MAX_CODE+1][K_ALTSHIFTTAB+1];

    uint32_t _curKeyboardType;
    void setKeyboardType(uint32_t keyboardType)
    {
        _curKeyboardType = keyboardType;
    }
    static const char* _keyboardTypeStrs[];

    uint32_t getNumTypes();
    const char* getTypeStr(uint32_t keyboardType)
    {
        return _keyboardTypeStrs[keyboardType];
    }
    const char* getKeyboardTypeStr()
    {
        return _keyboardTypeStrs[_curKeyboardType];
    }
    void setKeyboardTypeStr(const char* keyboardTypeStr)
    {
        for (uint32_t i = 0; i < getNumTypes(); i++)
        {
            if (strcasecmp(keyboardTypeStr, getTypeStr(i)) == 0)
            {
                _curKeyboardType = i;
                break;
            }
        }
    }
    uint16_t lookupKey(unsigned char ucModifiers, const unsigned char rawKey)
    {

        if (rawKey <= PHY_MAX_CODE)
        {
            bool shiftPressed = (ucModifiers & KEY_MOD_LSHIFT) || (ucModifiers & KEY_MOD_RSHIFT);
            bool altPressed = (ucModifiers & KEY_MOD_LALT) || (ucModifiers & KEY_MOD_RALT);
            uint32_t shAltIdx = (shiftPressed ? 1 : 0) + (altPressed ? 2 : 0);

            // LogWrite("KeyConversion", LOG_DEBUG, "RAWKEY %02x Mod %02x type %d shAltIdx %d",
            //                         rawKey, ucModifiers, _curKeyboardType, shAltIdx);

            return _hidCodeConversion[_curKeyboardType][rawKey][shAltIdx];
        }
        return -1;
    }

	static const char *s_KeyStrings[KeyMaxCode-KeySpace];

};

