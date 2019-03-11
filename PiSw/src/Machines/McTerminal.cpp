// Bus Raider Machine Serial Terminal
// Rob Dobson 2018

#include "McTerminal.h"
#include "usb_hid_keys.h"
#include "../System/wgfx.h"
#include "../TargetBus/BusAccess.h"
#include "../TargetBus/TargetState.h"
#include "../System/rdutils.h"
#include <stdlib.h>
#include "../System/nmalloc.h" //TODO
#include "../System/lowlib.h"
#include "../Hardware/HwManager.h"

const char* McTerminal::_logPrefix = "McTerm";

McDescriptorTable McTerminal::_descriptorTables[] = {
    {
        // Machine name
        "Serial Terminal",
        // Processor
        McDescriptorTable::PROCESSOR_Z80,
        // Required display refresh rate
        .displayRefreshRatePerSec = 30,
        .displayPixelsX = 80*8,
        .displayPixelsY = 30*16,
        .displayCellX = 8,
        .displayCellY = 16,
        .pixelScaleX = 1,
        .pixelScaleY = 1,
        .pFont = &__systemFont,
        .displayForeground = WGFX_WHITE,
        .displayBackground = WGFX_BLACK,
        // Clock
        .clockFrequencyHz = 7000000,
        // Interrupt rate per second
        .irqRate = 0,
        // Bus monitor
        .monitorIORQ = false,
        .monitorMREQ = false,
        .emulatedRAM = false,
        .emulatedRAMStart = 0,
        .emulatedRAMLen = 0x100000,
        .setRegistersByInjection = false,
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
        .displayPixelsY = 30*16,
        .displayCellX = 8,
        .displayCellY = 16,
        .pixelScaleX = 1,
        .pixelScaleY = 1,
        .pFont = &__systemFont,
        .displayForeground = WGFX_WHITE,
        .displayBackground = WGFX_BLACK,
        // Clock
        .clockFrequencyHz = 7000000,
        // Interrupt rate per second
        .irqRate = 0,
        // Bus monitor
        .monitorIORQ = true,
        .monitorMREQ = false,
        .emulatedRAM = false,
        .emulatedRAMStart = 0,
        .emulatedRAMLen = 0x100000,
        .setRegistersByInjection = false,
        .setRegistersCodeAddr = 0
    }
};

int McTerminal::_shiftDigitKeyMap[SHIFT_DIGIT_KEY_MAP_LEN] =
    { '!', '"', '#', '$', '%', '^', '&', '*', '(', ')' };

int McTerminal::_machineSubType = MC_SUB_TYPE_STD;

McTerminal::McTerminal() : McBase()
{
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

// Get number of descriptor tables for machine
int McTerminal::getDescriptorTableCount()
{
    return sizeof(_descriptorTables)/sizeof(_descriptorTables[0]);
}

// Get descriptor table for the machine
McDescriptorTable* McTerminal::getDescriptorTable([[maybe_unused]] int subType)
{
    if ((subType < 0) || (subType >= getDescriptorTableCount()))
        return &_descriptorTables[0];
    return &_descriptorTables[subType];
}

// Enable machine
void McTerminal::enable([[maybe_unused]]int subType)
{
    // Check valid type
    _machineSubType = subType;
    if ((subType < 0) || (subType >= getDescriptorTableCount()))
        _machineSubType = MC_SUB_TYPE_STD;
    // Invalidate screen buffer
    _screenBufferValid = false;
    LogWrite(_logPrefix, LOG_DEBUG, "Enabling %s", _descriptorTables[subType].machineName);
    BusAccess::accessCallbackAdd(memoryRequestCallback);
    // Bus raider enable wait states on IORQ
    BusAccess::waitSetup(_descriptorTables[subType].monitorIORQ, _descriptorTables[subType].monitorMREQ || _descriptorTables[subType].emulatedRAM);
}

// Disable machine
void McTerminal::disable()
{
    LogWrite(_logPrefix, LOG_DEBUG, "Disabling");
    BusAccess::waitSetup(false, false);
    BusAccess::accessCallbackRemove();
}

// Handle display refresh (called at a rate indicated by the machine's descriptor table)
void McTerminal::displayRefresh()
{
    // Use this to get keys from host and display
    bool screenChanged = false;
    int numCharsAvailable = McManager::getNumCharsReceivedFromHost();
    if (numCharsAvailable != 0)
    {
        // Get chars
        uint8_t* pCharBuf = (uint8_t*)nmalloc_malloc(numCharsAvailable+1);
        if (pCharBuf)
        {
            screenChanged = true;
            int gotChars = McManager::getCharsReceivedFromHost(pCharBuf, numCharsAvailable);
            // LogWrite(_logPrefix, LOG_DEBUG, "Got Rx Chars act len = %d\n", gotChars);
            
            // Handle each char
            for (int i = 0; i < gotChars; i++)
            {
                // Send to terminal window
                dispChar(pCharBuf[i]);
            }

            // Release chars
            nmalloc_free((void**)(&pCharBuf));
        }

    }

    // Update the display on the Pi Zero
    if (screenChanged)
    {
        for (int k = 0; k < _termRows; k++) 
        {
            for (int i = 0; i < _termCols; i++)
            {
                int cellIdx = k * _termCols + i;
                if (!_screenBufferValid || (_screenBuffer[cellIdx] != _screenChars[cellIdx]))
                {
                    wgfx_putc(MC_WINDOW_NUMBER, i, k, _screenChars[cellIdx]);
                    _screenBuffer[cellIdx] = _screenChars[cellIdx];
                }
            }
        }
    }
    _screenBufferValid = true;

    // Show the cursor as required
    if (_cursorShow)
    {
        if (isTimeout(micros(), _cursorBlinkLastUs, _cursorBlinkRateMs*1000))
        {
            if (_cursorIsOn)
            {
                int cellIdx = _curPosY * _termCols + _curPosX;
                wgfx_putc(MC_WINDOW_NUMBER, _curPosX, _curPosY, _screenChars[cellIdx]);
            }
            else
            {
                wgfx_putc(MC_WINDOW_NUMBER, _curPosX, _curPosY, _cursorChar);
            }
            _cursorIsOn = !_cursorIsOn;
            _cursorBlinkLastUs = micros();
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
    McManager::sendKeyCodeToTarget(asciiCode);
}

// Handle a file
void McTerminal::fileHandler(const char* pFileInfo, const uint8_t* pFileData, int fileLen)
{
    // Get the file type (extension of file name)
    #define MAX_VALUE_STR 30
    #define MAX_FILE_NAME_STR 100
    char fileName[MAX_FILE_NAME_STR+1];
    if (!jsonGetValueForKey("fileName", pFileInfo, fileName, MAX_FILE_NAME_STR))
        return;

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
}

// Handle a request for memory or IO - or possibly something like in interrupt vector in Z80
uint32_t McTerminal::memoryRequestCallback([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, [[maybe_unused]] uint32_t flags)
{
    // Offer to the hardware manager
    uint32_t retVal = HwManager::handleMemOrIOReq(addr, data, flags);
    
    // Callback to debugger
    TargetDebug* pDebug = TargetDebug::get();
    if (pDebug)
        retVal = pDebug->handleInterrupt(addr, data, flags, retVal, _descriptorTables[_machineSubType]);

    return retVal;
}

void McTerminal::clearScreen()
{
    for (int i = 0; i < _termRows * _termCols; i++)
        _screenChars[i] = ' ';
    _curPosX = 0;
    _curPosY = 0;
}

void McTerminal::dispChar(uint8_t ch)
{
    switch(ch)
    {
        case 0x0d:
        {
            moveAndCheckCurPos(-1,-1,0,1);
            break;
        }
        case 0x0a:
        {
            _curPosX = 0;
            break;
        }
        case 0x08:
        {
            moveAndCheckCurPos(-1,-1,-1,0);
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
            moveAndCheckCurPos(-1, -1, 1, 0);
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

void McTerminal::moveAndCheckCurPos(int absX, int absY, int relX, int relY)
{
    if (_cursorIsOn)
    {
        int cellIdx = _curPosY * _termCols + _curPosX;
        wgfx_putc(MC_WINDOW_NUMBER, _curPosX, _curPosY, _screenChars[cellIdx]);
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
