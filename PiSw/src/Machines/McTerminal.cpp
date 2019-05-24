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
#include "../Fonts/SystemFont.h"

const char* McTerminal::_logPrefix = "McTerm";

McDescriptorTable McTerminal::_defaultDescriptorTables[] = {
    {
        // Machine name
        "Serial Terminal",
        // Processor
        McDescriptorTable::PROCESSOR_Z80,
        // Required display refresh rate
        .displayRefreshRatePerSec = 30,
        .displayPixelsX = 80*8,
        .displayPixelsY = 25*16,
        .displayCellX = 8,
        .displayCellY = 16,
        .pixelScaleX = 1,
        .pixelScaleY = 1,
        .pFont = &__systemFont,
        .displayForeground = DISPLAY_FX_WHITE,
        .displayBackground = DISPLAY_FX_BLACK,
        .displayMemoryMapped = false,
        // Clock
        .clockFrequencyHz = 7000000,
        // Interrupt rate per second
        .irqRate = 0,
        // Bus monitor
        .monitorIORQ = false,
        .monitorMREQ = false,
        .setRegistersCodeAddr = 0
    },
    {
        // Machine name
        "Serial Terminal EmuUART",
        // Processor
        McDescriptorTable::PROCESSOR_Z80,
        // Required display refresh rate
        .displayRefreshRatePerSec = 30,
        .displayPixelsX = 80*8,
        .displayPixelsY = 25*16,
        .displayCellX = 8,
        .displayCellY = 16,
        .pixelScaleX = 1,
        .pixelScaleY = 1,
        .pFont = &__systemFont,
        .displayForeground = DISPLAY_FX_WHITE,
        .displayBackground = DISPLAY_FX_BLACK,
        .displayMemoryMapped = false,
        // Clock
        .clockFrequencyHz = 7000000,
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
    _pTerminalEmulation = new TermH19();
    if (_pTerminalEmulation)
        _pTerminalEmulation->init(80, 25);

    // Copy descriptor table
    _termCols = DEFAULT_TERM_COLS;
    _termRows = DEFAULT_TERM_ROWS;
    _screenBufferValid = false;
    _curPosX = _curPosY = 0;
    _cursorShow = true;
    _cursorBlinkLastUs = 0;
    _cursorBlinkRateMs = 500;
    _cursorIsOn = false;
    _cursorChar = '_';
    clearScreen();
}

// Enable machine
void McTerminal::enable()
{
    // Invalidate screen buffer
    _screenBufferValid = false;
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
            // dispChar(charBuf[i], _pDisplay);
            if (_pTerminalEmulation)
                _pTerminalEmulation->putChar(charBuf[i]);
        }
    }

    // Update the display on the Pi Zero
    // if (screenChanged)
    // {
    //     for (int k = 0; k < _termRows; k++) 
    //     {
    //         for (int i = 0; i < _termCols; i++)
    //         {
    //             int cellIdx = k * _termCols + i;
    //             if (!_screenBufferValid || (_screenBuffer[cellIdx] != _screenChars[cellIdx]))
    //             {
    //                 _pDisplay->write(i, k, _screenChars[cellIdx]);
    //                 _screenBuffer[cellIdx] = _screenChars[cellIdx];
    //             }
    //         }
    //     }
    // }
    // _screenBufferValid = true;
    if (_pTerminalEmulation)
    {
        if (_pTerminalEmulation->hasChanged())
        {
            for (uint32_t k = 0; k < _pTerminalEmulation->_rows; k++) 
            {
                for (uint32_t i = 0; i < _pTerminalEmulation->_cols; i++)
                {
                    uint32_t cellIdx = k * _pTerminalEmulation->_cols + i;
                    _pDisplay->write(i, k, _pTerminalEmulation->_pCharBuffer[cellIdx]._charCode);
                }
            }
        }

        // Show the cursor as required
        if (!_pTerminalEmulation->_cursor._off)
        {
            if (isTimeout(micros(), _cursorBlinkLastUs, _cursorBlinkRateMs*1000))
            {
                if (_cursorIsOn)
                {
                    int cellIdx = _pTerminalEmulation->_cursor._row * _pTerminalEmulation->_cols + _pTerminalEmulation->_cursor._col;
                    _pDisplay->write(_pTerminalEmulation->_cursor._col, _pTerminalEmulation->_cursor._row, _pTerminalEmulation->_pCharBuffer[cellIdx]._charCode);
                }
                else
                {
                    _pDisplay->write(_pTerminalEmulation->_cursor._col, _pTerminalEmulation->_cursor._row, _cursorChar);
                }
                _cursorIsOn = !_cursorIsOn;
                _cursorBlinkLastUs = micros();
            }
        }
    }
}

int McTerminal::convertRawToAscii(unsigned char ucModifiers, const unsigned char rawKeys[6])
{
    int asciiCode = 0;
    int rawKey = rawKeys[0];
    bool shiftPressed = (ucModifiers & KEY_MOD_LSHIFT) || (ucModifiers & KEY_MOD_RSHIFT);
    // bool ctrlPressed = (ucModifiers & KEY_MOD_LCTRL) || (ucModifiers & KEY_MOD_RCTRL);
    if ((rawKey == KEY_NONE) || (rawKey == KEY_ERR_OVF))
        return 0;
    if ((rawKey >= KEY_A) && (rawKey <= KEY_Z)) {
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
        asciiCode = shiftPressed ? '@' : '\'';
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
        asciiCode = 0x1d;        
    } else if (rawKey == KEY_UP) {
        // Handle Up
        asciiCode = 0x11;
    } else if (rawKey == KEY_DOWN) {
        // Handle Down
        asciiCode = 0x12;
    } else if (rawKey == KEY_LEFT) {
        // Handle Left
        asciiCode = 0x13;
    } else if (rawKey == KEY_RIGHT) {
        // Handle Right
        asciiCode = 0x14;
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

    // Send chars to host
    if (asciiCode == 0)
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

void McTerminal::clearScreen()
{
    for (int i = 0; i < _termRows * _termCols; i++)
        _screenChars[i] = ' ';
    _curPosX = 0;
    _curPosY = 0;
}

void McTerminal::dispChar(uint8_t ch, DisplayBase* pDisplay)
{
    switch(ch)
    {
        case 0x0d:
        {
            moveAndCheckCurPos(-1, -1, 0, 1, pDisplay);
            break;
        }
        case 0x0a:
        {
            _curPosX = 0;
            break;
        }
        case 0x08:
        {
            moveAndCheckCurPos(-1, -1, -1, 0, pDisplay);
            _screenChars[_curPosY * _termCols + _curPosX] = ' ';
            break;
        }
        case 0x0c:
        {
            clearScreen();
            break;
        }
        case 0x09:
        {
            break;
        }
        default:
        {
            // Show char at cursor
            _screenChars[_curPosY * _termCols + _curPosX] = ch;

            // Move cursor
            moveAndCheckCurPos(-1, -1, 1, 0, pDisplay);
            break;
        }
    }
}

void McTerminal::vscrollBuffer(int rows)
{
    if (rows >= _termRows)
    {
        clearScreen();
        return;
    }

    // Move chars
    int charsToMove = (_termRows - rows) * _termCols;
    int charsToWipe = rows * _termCols;
    memcpy(_screenChars, _screenChars+charsToWipe, charsToMove);

    // Clear remaining
    for (int i = charsToMove; i < _termRows * _termCols; i++)
        _screenChars[i] = ' ';

    // Set buffer dirty to ensure rewritten
    _screenBufferValid = false;
}

void McTerminal::moveAndCheckCurPos(int absX, int absY, int relX, int relY, DisplayBase* pDisplay)
{
    if (_cursorIsOn)
    {
        int cellIdx = _curPosY * _termCols + _curPosX;
        if (pDisplay)
            pDisplay->write(_curPosX, _curPosY, _screenChars[cellIdx]);
    }
    // LogWrite("Th", LOG_DEBUG, "%d %d %d %d cur %d %d", absX, absY, relX, relY, _curPosX, _curPosY);
    if (absX >= 0)
        _curPosX = absX;
    if (absY >= 0)
        _curPosY = absY;
    if (_curPosX+relX >= 0)
        _curPosX += relX;
    else
        _curPosX = 0;
    _curPosY += relY;
    if (_curPosX >= _termCols)
    {
        _curPosX = 0;
        _curPosY++;
    }
    if (_curPosY >= _termRows)
    {
        vscrollBuffer(1);
        _curPosY -= 1;
    }
    if (_curPosX < 0)
        _curPosX = 0;
    if (_curPosY < 0)
        _curPosY = 0;
    // LogWrite("Th", LOG_DEBUG, "after %d %d %d %d cur %d %d", absX, absY, relX, relY, _curPosX, _curPosY);
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