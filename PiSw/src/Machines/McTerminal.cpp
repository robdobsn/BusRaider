// Bus Raider Machine Serial Terminal
// Rob Dobson 2018

#include <stdlib.h>
#include "McTerminal.h"
#include "usb_hid_keys.h"
#include "../System/rdutils.h"
#include "../System/nmalloc.h"
#include "../System/lowlib.h"
#include "../TargetBus/BusAccess.h"
#include "../TargetBus/TargetState.h"
#include "../Hardware/HwManager.h"
#include "../TerminalEmulation/TermH19.h"
#include "../TerminalEmulation/TermAnsi.h"

const char* McTerminal::_logPrefix = "McTerm";

extern WgfxFont __p12x16Font;

McDescriptorTable McTerminal::_defaultDescriptorTables[] = {
    {
        // Machine name
        "Serial Terminal ANSI",
        // Processor
        McDescriptorTable::PROCESSOR_Z80,
        // Required display refresh rate
        .displayRefreshRatePerSec = 30,
        .displayPixelsX = 80*12,
        .displayPixelsY = 25*16,
        .displayCellX = 12,
        .displayCellY = 16,
        .pixelScaleX = 1,
        .pixelScaleY = 2,
        .pFont = &__p12x16Font,
        .displayForeground = DISPLAY_FX_WHITE,
        .displayBackground = DISPLAY_FX_BLACK,
        .displayMemoryMapped = false,
        // Clock
        .clockFrequencyHz = 7373000,
        // Interrupt rate per second
        .irqRate = 0,
        // Bus monitor
        .monitorIORQ = false,
        .monitorMREQ = false,
        .setRegistersCodeAddr = 0
    },
    {
        // Machine name
        "Serial Terminal H19",
        // Processor
        McDescriptorTable::PROCESSOR_Z80,
        // Required display refresh rate
        .displayRefreshRatePerSec = 30,
        .displayPixelsX = 80*12,
        .displayPixelsY = 25*16,
        .displayCellX = 12,
        .displayCellY = 16,
        .pixelScaleX = 1,
        .pixelScaleY = 2,
        .pFont = &__p12x16Font,
        .displayForeground = DISPLAY_FX_WHITE,
        .displayBackground = DISPLAY_FX_BLACK,
        .displayMemoryMapped = false,
        // Clock
        .clockFrequencyHz = 7373000,
        // Interrupt rate per second
        .irqRate = 0,
        // Bus monitor
        .monitorIORQ = true,
        .monitorMREQ = false,
        .setRegistersCodeAddr = 0
    }
};

int McTerminal::_shiftDigitKeyMap[SHIFT_DIGIT_KEY_MAP_LEN] =
    { '!', '"', '#', '$', '%', '^', '&', '*', '(', ')' };

McTerminal::McTerminal() : McBase(_defaultDescriptorTables, sizeof(_defaultDescriptorTables)/sizeof(_defaultDescriptorTables[0]))
{
    // Emulation
    _pTerminalEmulation = new TermAnsi();
    if (_pTerminalEmulation)
        _pTerminalEmulation->init(80, 25);

    // Init
    invalidateScreenCaches(false);
    _cursorBlinkLastUs = 0;
    _cursorBlinkRateMs = 500;
    _cursorIsShown = false;
}

// Enable machine
void McTerminal::enable()
{
    // Check for change in terminal type
    if (_pTerminalEmulation)
        delete _pTerminalEmulation;
    _pTerminalEmulation = NULL;

    // Emulation
    if (_activeSubType == 0)
        _pTerminalEmulation = new TermAnsi();
    else
        _pTerminalEmulation = new TermH19();
    if (_pTerminalEmulation)
        _pTerminalEmulation->init(_activeDescriptorTable.displayPixelsX/_activeDescriptorTable.displayCellX, 
                    _activeDescriptorTable.displayPixelsY/_activeDescriptorTable.displayCellY);

    // Invalidate screen caches
    invalidateScreenCaches(false);
}

// Disable machine
void McTerminal::disable()
{
}

// Handle display refresh (called at a rate indicated by the machine's descriptor table)
void McTerminal::displayRefreshFromMirrorHw()
{
    if (!_pDisplay)
        return;

    // Use this to get keys from host and display
    // bool screenChanged = false;
    int numCharsAvailable = McManager::hostSerialNumChAvailable();
    if (numCharsAvailable != 0)
    {
        // Get chars
        static const int MAX_CHARS_AT_A_TIME = 1000;
        uint8_t charBuf[MAX_CHARS_AT_A_TIME];
        if (numCharsAvailable > MAX_CHARS_AT_A_TIME)
            numCharsAvailable = MAX_CHARS_AT_A_TIME;
        // screenChanged = true;
        uint32_t gotChars = McManager::hostSerialReadChars(charBuf, numCharsAvailable);
        // LogWrite(_logPrefix, LOG_DEBUG, "Got Rx Chars act len = %d\n", gotChars);
        
        // Handle each char
        for (uint32_t i = 0; i < gotChars; i++)
        {
            // Send to terminal window
            if (_pTerminalEmulation)
                _pTerminalEmulation->putChar(charBuf[i]);
        }
    }

    // _screenBufferValid = true;
    if (_pTerminalEmulation)
    {
        if (_pTerminalEmulation->hasChanged())
        {
            if (_cursorIsShown)
                _pDisplay->write(_cursorInfo._col, _cursorInfo._row, _cursorInfo._replacedChar);
            for (uint32_t k = 0; k < _pTerminalEmulation->_rows; k++) 
            {
                for (uint32_t i = 0; i < _pTerminalEmulation->_cols; i++)
                {
                    uint32_t cellIdx = k * _pTerminalEmulation->_cols + i;
                    if (!_screenCache[cellIdx].equals(_pTerminalEmulation->_pCharBuffer[cellIdx]))
                    {
                        _pDisplay->write(i, k, _pTerminalEmulation->_pCharBuffer[cellIdx]._charCode);
                        _screenCache[cellIdx] = _pTerminalEmulation->_pCharBuffer[cellIdx];
                    }
                }
            }
            if (_cursorIsShown)
            {
                // Stash location
                _cursorInfo = _pTerminalEmulation->_cursor;
                int cellIdx = _cursorInfo._row * _pTerminalEmulation->_cols + _cursorInfo._col;
                _cursorInfo._replacedChar = _pTerminalEmulation->_pCharBuffer[cellIdx]._charCode;
                // Show cursor
                _pDisplay->write(_cursorInfo._col, _cursorInfo._row, _cursorInfo._cursorChar);
            }
        }
        _pTerminalEmulation->_cursor._updated = false;

        // Show the cursor as required
        if (!_pTerminalEmulation->_cursor._off)
        {
            if (isTimeout(micros(), _cursorBlinkLastUs, _cursorBlinkRateMs*1000))
            {
                if (!_cursorIsShown)
                {
                    // Stash location
                    _cursorInfo = _pTerminalEmulation->_cursor;
                    int cellIdx = _cursorInfo._row * _pTerminalEmulation->_cols + _cursorInfo._col;
                    _cursorInfo._replacedChar = _pTerminalEmulation->_pCharBuffer[cellIdx]._charCode;
                    // Show cursor
                    _pDisplay->write(_cursorInfo._col, _cursorInfo._row, _cursorInfo._cursorChar);
                }
                else
                {
                    // Replace cursor
                    _pDisplay->write(_cursorInfo._col, _cursorInfo._row, _cursorInfo._replacedChar);
                }
                _cursorIsShown = !_cursorIsShown;
                _cursorBlinkLastUs = micros();
            }
        }
        else
        {
            if (_cursorIsShown)
                _pDisplay->write(_cursorInfo._col, _cursorInfo._row, _cursorInfo._replacedChar);
            _cursorIsShown = false;
        }
    }
}

uint32_t McTerminal::getMirrorChanges(uint8_t* pMirrorChangeBuf, uint32_t mirrorChangeMaxLen, bool forceGetAll)
{
    // Go through mirror cache and find changes
    uint32_t curPos = 0;

    // If time to force get of all screen info then invalidate the cache
    if (forceGetAll)
        invalidateScreenCaches(true);

    // Add the screen dimensions to the buffer first
    if (!pMirrorChangeBuf || (mirrorChangeMaxLen < 10))
        return 0;
    pMirrorChangeBuf[curPos++] = _pTerminalEmulation->_cols;
    pMirrorChangeBuf[curPos++] = _pTerminalEmulation->_rows;
    uint32_t mirrorChangeStart = curPos;

    // Add the packed character changes
    #pragma pack(push, 1)
    struct {
        uint8_t col;
        uint8_t row;
        uint8_t ch;
        uint8_t fore;
        uint8_t back;
        uint8_t attr;
    } changeElemPacked;
    #pragma pack(pop)
    bool bufFull = false;
    for (uint32_t k = 0; k < _pTerminalEmulation->_rows; k++) 
    {
        if (bufFull)
            break;
        for (uint32_t i = 0; i < _pTerminalEmulation->_cols; i++)
        {
            uint32_t cellIdx = k * _pTerminalEmulation->_cols + i;
            if (!_screenMirrorCache[cellIdx].equals(_pTerminalEmulation->_pCharBuffer[cellIdx]))
            {
                if (curPos + sizeof(changeElemPacked) > mirrorChangeMaxLen)
                {
                    bufFull = true;
                    break;
                }
                changeElemPacked.col = i;
                changeElemPacked.row = k;
                changeElemPacked.ch = _pTerminalEmulation->_pCharBuffer[cellIdx]._charCode;
                changeElemPacked.fore = _pTerminalEmulation->_pCharBuffer[cellIdx]._foreColour;
                changeElemPacked.back = _pTerminalEmulation->_pCharBuffer[cellIdx]._backColour;
                changeElemPacked.attr = _pTerminalEmulation->_pCharBuffer[cellIdx]._attribs;
                memcpy(pMirrorChangeBuf+curPos, &changeElemPacked, sizeof(changeElemPacked));
                curPos += sizeof(changeElemPacked);
                _screenMirrorCache[cellIdx] = _pTerminalEmulation->_pCharBuffer[cellIdx];
            }
        }
    }
    // LogWrite(_logPrefix, LOG_DEBUG, "Packing %d", sizeof(_screenMirrorCache));

    return (mirrorChangeStart == curPos) ? 0 : curPos;
}

int McTerminal::convertRawToAscii(unsigned char ucModifiers, const unsigned char rawKeys[6])
{
    int asciiCode = -1;
    int rawKey = rawKeys[0];
    if ((rawKey >= 0x80) && (rawKeys[1] != 0))
        rawKey = rawKeys[1];
    bool shiftPressed = (ucModifiers & KEY_MOD_LSHIFT) || (ucModifiers & KEY_MOD_RSHIFT);
    bool ctrlPressed = (ucModifiers & KEY_MOD_LCTRL) || (ucModifiers & KEY_MOD_RCTRL);
    if ((rawKey == KEY_NONE) || (rawKey == KEY_ERR_OVF))
        return -1;
    if ((rawKey >= KEY_A) && (rawKey <= KEY_Z)) {
        if (ctrlPressed)
            return (rawKey-KEY_A+1);
        // Handle A-Z
        asciiCode = (rawKey-KEY_A) + (shiftPressed ? 'A' : 'a');
    } else if ((rawKey >= KEY_1) && (rawKey <= KEY_9)) {
        // Handle 1..9
        if (!shiftPressed)
            asciiCode = (rawKey-KEY_1) + '1';
        else
            asciiCode = _shiftDigitKeyMap[rawKey-KEY_1];
    } else if (rawKey == KEY_0) {
        // Handle 0 )
        asciiCode = shiftPressed ? _shiftDigitKeyMap[9] : '0';
    } else if (rawKey == KEY_SEMICOLON) {
        // Handle ; :
        asciiCode = shiftPressed ? ':' : ';';
    } else if (rawKey == KEY_APOSTROPHE) {
        // Handle ' @
        if (ctrlPressed)
            return (0);
        asciiCode = shiftPressed ? '@' : '\'';
    } else if (rawKey == KEY_LEFTBRACE) {
        // Handle ' [
        if (ctrlPressed)
            return (0x1b);
        asciiCode = shiftPressed ? '{' : '[';
    } else if (rawKey == KEY_BACKSLASH) {
        // Handle backslash
        if (ctrlPressed)
            return (0x1c);
        asciiCode = shiftPressed ? '|' : '\\';
    } else if (rawKey == KEY_RIGHTBRACE) {
        // Handle ' ]
        if (ctrlPressed)
            return (0x1d);
        asciiCode = shiftPressed ? '}' : ']';
    } else if (rawKey == KEY_COMMA) {
        // Handle , <
        asciiCode = shiftPressed ? '<' : ',';
    } else if (rawKey == KEY_DOT) {
        // Handle . >
        asciiCode = shiftPressed ? '>' : '.';
    } else if (rawKey == KEY_EQUAL) {
        // Handle = +
        asciiCode = shiftPressed ? '+' : '=';        
    } else if (rawKey == KEY_MINUS) {
        // Handle - _
        if (ctrlPressed)
            return (0x1f);
        asciiCode = shiftPressed ? '_' : '-';        
    } else if (rawKey == KEY_SLASH) {
        // Handle / ?
        asciiCode = shiftPressed ? '?' : '/';        
    } else if (rawKey == KEY_ENTER) {
        // Handle Enter
        asciiCode = 0x0d;
    } else if (rawKey == KEY_BACKSPACE) {
        // Handle backspace
        asciiCode = 0x08;
    } else if (rawKey == KEY_ESC) {
        // Handle ESC
        asciiCode = 0x1b;        
    } else if (rawKey == KEY_TAB) {
        // Handle TAB
        asciiCode = 0x09;
    } else if (rawKey == KEY_DELETE) {
        // Handle DELETE
        asciiCode = 0x7F;
    } else if (rawKey == KEY_UP) {
        // Handle Up = Ctrl-E
        asciiCode = 0x05;
    } else if (rawKey == KEY_DOWN) {
        // Handle Down = Ctrl-X
        asciiCode = 0x18;
    } else if (rawKey == KEY_LEFT) {
        // Handle Left = Ctrl - S
        asciiCode = 0x13;
    } else if (rawKey == KEY_RIGHT) {
        // Handle Right = Ctrl -D
        asciiCode = 0x04;
    } else if (rawKey == KEY_SPACE) {
        // Handle Space
        asciiCode = 0x20;
    }
    return asciiCode;
}

// Handle a key press
void McTerminal::keyHandler([[maybe_unused]] unsigned char ucModifiers, [[maybe_unused]] const unsigned char rawKeys[6])
{
    int asciiCode = convertRawToAscii(ucModifiers, rawKeys);

    // LogWrite(_logPrefix, LOG_DEBUG, "ASCII %02x RAWKEY %02x %02x %02x %02x %02x %02x Mod %02x",
    //                 asciiCode, rawKeys[0], rawKeys[1], rawKeys[2], rawKeys[3], rawKeys[4], rawKeys[5], ucModifiers);

    // Send chars to host
    if (asciiCode < 0)
        return;

    // Send to host
    McManager::sendKeyCodeToTargetStatic(asciiCode);
}

// Handle a file
bool McTerminal::fileHandler(const char* pFileInfo, const uint8_t* pFileData, int fileLen)
{
    // Get the file type (extension of file name)
    #define MAX_VALUE_STR 30
    #define MAX_FILE_NAME_STR 100
    char fileName[MAX_FILE_NAME_STR+1];
    if (!jsonGetValueForKey("fileName", pFileInfo, fileName, MAX_FILE_NAME_STR))
        return false;

    // Get type of file (assume extension is delimited by .)
    const char* pFileType = strstr(fileName, ".");
    const char* pEmpty = "";
    if (pFileType == NULL)
        pFileType = pEmpty;

    // Treat everything as a binary file
    uint16_t baseAddr = 0;
    char baseAddrStr[MAX_VALUE_STR+1];
    if (jsonGetValueForKey("baseAddr", pFileInfo, baseAddrStr, MAX_VALUE_STR))
        baseAddr = strtol(baseAddrStr, NULL, 16);
    LogWrite(_logPrefix, LOG_DEBUG, "Processing binary file, baseAddr %04x len %d", baseAddr, fileLen);
    TargetState::addMemoryBlock(baseAddr, pFileData, fileLen);
    return true;
}

// Handle a request for memory or IO - or possibly something like in interrupt vector in Z80
void McTerminal::busAccessCallback([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, 
            [[maybe_unused]] uint32_t flags, [[maybe_unused]] uint32_t& retVal)
{
}

// Bus action complete callback
void McTerminal::busActionCompleteCallback([[maybe_unused]] BR_BUS_ACTION actionType)
{
}

void McTerminal::invalidateScreenCaches(bool mirrorOnly)
{
    if (mirrorOnly)
    {
        for (uint32_t i = 0; i < sizeof(_screenCache)/sizeof(_screenCache[0]); i++)
            _screenCache[i]._attribs = TermChar::TERM_ATTR_INVALID_CODE;    
    }
    for (uint32_t i = 0; i < sizeof(_screenMirrorCache)/sizeof(_screenMirrorCache[0]); i++)
        _screenMirrorCache[i]._attribs = TermChar::TERM_ATTR_INVALID_CODE;    
}