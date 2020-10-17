// Bus Raider Machine Serial Terminal
// Rob Dobson 2018

#include <stdlib.h>
#include "McTerminal.h"
#include "usb_hid_keys.h"
#include "rdutils.h"
#include "lowlib.h"
#include "BusControl.h"
#include "TargetProgrammer.h"
#include "TermH19.h"
#include "TermAnsi.h"

static const char* MODULE_PREFIX = "McTerm";

extern WgfxFont __p12x16Font;
KeyConversion McTerminal::_keyConversion;

McVariantTable McTerminal::_machineDescriptorTables[] = {
    {
        // Machine name
        "Serial Terminal ANSI",
        // Processor
        McVariantTable::PROCESSOR_Z80,
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
    },
    {
        // Machine name
        "Serial Terminal H19",
        // Processor
        McVariantTable::PROCESSOR_Z80,
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

McTerminal::McTerminal(McManager& mcManager, BusControl& busAccess) : 
    McBase(mcManager, busAccess, _machineDescriptorTables, 
            sizeof(_machineDescriptorTables)/sizeof(_machineDescriptorTables[0])),
    _sendToTargetBufPos(MAX_SEND_TO_TARGET_CHARS)
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

    // Emulation of uarts
    _emulate6850 = true;
    _emulationInterruptOnRx = false;
    _emulation6850NeedsReset = true;
    _emulation6850NotSetup = false;
}

// Enable machine
void McTerminal::enableMachine()
{
    // // Check for change in terminal type
    // if (_pTerminalEmulation)
    //     delete _pTerminalEmulation;
    // _pTerminalEmulation = NULL;

    // // Emulation
    // if (strstr(getMachineName(), "H19") == NULL)
    //     _pTerminalEmulation = new TermAnsi();
    // else
    //     _pTerminalEmulation = new TermH19();
    // if (_pTerminalEmulation)
    //     _pTerminalEmulation->init(
    //                 getDescriptorTable().displayPixelsX/getDescriptorTable().displayCellX, 
    //                 getDescriptorTable().displayPixelsY/getDescriptorTable().displayCellY
    //                 );

    // // Invalidate screen caches
    // invalidateScreenCaches(false);
}

// Disable machine
void McTerminal::disableMachine()
{
}

// Setup machine from JSON
bool McTerminal::setupMachine(const char* mcName, const char* mcJson)
{
    // Setup via base class implementation
    bool rslt = McBase::setupMachine(mcName, mcJson);

    // Check for variations
    _emulate6850 = true;
    _emulationInterruptOnRx = false;
    setMonitorIORQEnabled(true);
    static const int MAX_UART_EMULATION_STR = 100;
    char emulUartStr[MAX_UART_EMULATION_STR];
    bool emulUartValid = jsonGetValueForKey("emulate6850", mcJson, emulUartStr, MAX_UART_EMULATION_STR);
    if (emulUartValid)
    {
        _emulate6850 = (strtoul(emulUartStr, NULL, 10) != 0);
        if (!_emulate6850)
            setMonitorIORQEnabled(false);
    }

    // Keyboard type
    static const int KEYBOARD_TYPE_STR_MAX = 100;
    char keyboardTypeStr[KEYBOARD_TYPE_STR_MAX];
    bool keyboardTypeValid = jsonGetValueForKey("keyboardType", mcJson, keyboardTypeStr, KEYBOARD_TYPE_STR_MAX);
    if (keyboardTypeValid)
    {
        _keyConversion.setKeyboardTypeStr(keyboardTypeStr);
    }
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "setupMachine emulate6850 %s keyboardType %s",
             _emulate6850 ? "Y" : "N",
             _keyConversion.getKeyboardTypeStr());
    return rslt;
}

// Handle display refresh (called at a rate indicated by the machine's descriptor table)
void McTerminal::refreshDisplay()
{
    DisplayBase* pDisplay = getDisplay();
    if (!pDisplay)
        return;

    // Use this to get keys from host and display
    // bool screenChanged = false;
    int numCharsAvailable = _mcManager.hostSerialNumChAvailable();
    if (numCharsAvailable != 0)
    {
        // Get chars
        static const int MAX_CHARS_AT_A_TIME = 1000;
        uint8_t charBuf[MAX_CHARS_AT_A_TIME];
        if (numCharsAvailable > MAX_CHARS_AT_A_TIME)
            numCharsAvailable = MAX_CHARS_AT_A_TIME;
        // screenChanged = true;
        uint32_t gotChars = _mcManager.hostSerialReadChars(charBuf, numCharsAvailable);
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "Got Rx Chars act len = %d\n", gotChars);
        
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
                pDisplay->write(_cursorInfo._col, _cursorInfo._row, _cursorInfo._replacedChar);
            for (uint32_t k = 0; k < _pTerminalEmulation->_rows; k++) 
            {
                for (uint32_t i = 0; i < _pTerminalEmulation->_cols; i++)
                {
                    uint32_t cellIdx = k * _pTerminalEmulation->_cols + i;
                    TermChar& termChar = _pTerminalEmulation->getTermChar(cellIdx);
                    if (!_screenCache[cellIdx].equals(termChar))
                    {
                        if (termChar._attribs & TermChar::TERM_ATTR_REVERSE)
                        {
                            pDisplay->foreground((DISPLAY_FX_COLOUR) termChar._backColour);
                            pDisplay->background((DISPLAY_FX_COLOUR) termChar._foreColour);
                        }
                        else
                        {
                            pDisplay->foreground((DISPLAY_FX_COLOUR) termChar._foreColour);
                            pDisplay->background((DISPLAY_FX_COLOUR) termChar._backColour);
                        }
                        pDisplay->write(i, k, termChar._charCode);
                        _screenCache[cellIdx] = termChar;
                    }
                }
            }
            if (_cursorIsShown)
            {
                // Stash location
                _cursorInfo = _pTerminalEmulation->_cursor;
                int cellIdx = _cursorInfo._row * _pTerminalEmulation->_cols + _cursorInfo._col;
                _cursorInfo._replacedChar = _pTerminalEmulation->getTermChar(cellIdx)._charCode;
                // Show cursor
                pDisplay->write(_cursorInfo._col, _cursorInfo._row, _cursorInfo._cursorChar);
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
                    _cursorInfo._replacedChar = _pTerminalEmulation->getTermChar(cellIdx)._charCode;
                    // Show cursor
                    pDisplay->write(_cursorInfo._col, _cursorInfo._row, _cursorInfo._cursorChar);
                }
                else
                {
                    // Replace cursor
                    pDisplay->write(_cursorInfo._col, _cursorInfo._row, _cursorInfo._replacedChar);
                }
                _cursorIsShown = !_cursorIsShown;
                _cursorBlinkLastUs = micros();
            }
        }
        else
        {
            if (_cursorIsShown)
                pDisplay->write(_cursorInfo._col, _cursorInfo._row, _cursorInfo._replacedChar);
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
            TermChar& termChar = _pTerminalEmulation->getTermChar(cellIdx);
            if (!_screenMirrorCache[cellIdx].equals(termChar))
            {
                if (curPos + sizeof(changeElemPacked) > mirrorChangeMaxLen)
                {
                    bufFull = true;
                    break;
                }
                changeElemPacked.col = i;
                changeElemPacked.row = k;
                changeElemPacked.ch = termChar._charCode;
                changeElemPacked.fore = termChar._foreColour;
                changeElemPacked.back = termChar._backColour;
                changeElemPacked.attr = termChar._attribs;
                memcopyfast(pMirrorChangeBuf+curPos, &changeElemPacked, sizeof(changeElemPacked));
                curPos += sizeof(changeElemPacked);
                _screenMirrorCache[cellIdx] = termChar;
            }
        }
    }
    // LogWrite(MODULE_PREFIX, LOG_DEBUG, "BufMax %d Packing %d rows %d cols %d packedElemSize %d mirrorChangeStart %d curPos %d buf[0] %x buf[1] %x buf[2] %x buf[3] %x", 
    //             mirrorChangeMaxLen, sizeof(_screenMirrorCache), _pTerminalEmulation->_rows, _pTerminalEmulation->_cols, 
    //             sizeof(changeElemPacked), mirrorChangeStart, curPos, pMirrorChangeBuf[0], pMirrorChangeBuf[1], pMirrorChangeBuf[2], pMirrorChangeBuf[3]);

    return (mirrorChangeStart == curPos) ? 0 : curPos;
}

const char* McTerminal::convertRawToKeyString(unsigned char ucModifiers, const unsigned char rawKeys[6])
{
    static const char* emptyKeyStr = "";
    static char singleKeyStr[2];
    strcpy(singleKeyStr, " ");
    int rawKey = rawKeys[0];
    if ((rawKey >= 0x80) && (rawKeys[1] != 0))
        rawKey = rawKeys[1];
    if ((rawKey == KEY_NONE) || (rawKey == KEY_ERR_OVF))
        return emptyKeyStr;

    // Lookup
    uint32_t convertedKeyCode = _keyConversion.lookupKey(ucModifiers, rawKey);
    if ((convertedKeyCode >= KeySpace) && (convertedKeyCode < KeyMaxCode))
    {
        const char* pKeyStrToReturn = _keyConversion.s_KeyStrings[convertedKeyCode-KeySpace];
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "KeyStr %s CODE %02x %02x RAWKEY %02x %02x %02x %02x %02x %02x Mod %02x",
        //             pKeyStrToReturn, pKeyStrToReturn[0], pKeyStrToReturn[1], rawKeys[0], rawKeys[1], rawKeys[2], rawKeys[3], rawKeys[4], rawKeys[5], ucModifiers);
        return pKeyStrToReturn;
    }
    else
    {
        bool ctrlPressed = (ucModifiers & KEY_MOD_LCTRL) || (ucModifiers & KEY_MOD_RCTRL);
        if (ctrlPressed)
            singleKeyStr[0] = convertedKeyCode & 0x1f;
        else
            singleKeyStr[0] = convertedKeyCode;
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "KeyStr %s CODE %02x RAWKEY %02x %02x %02x %02x %02x %02x Mod %02x",
        //             singleKeyStr, singleKeyStr[0], rawKeys[0], rawKeys[1], rawKeys[2], rawKeys[3], rawKeys[4], rawKeys[5], ucModifiers);

        return singleKeyStr;
    }
}

// Handle a key press
void McTerminal::keyHandler( unsigned char ucModifiers,  const unsigned char rawKeys[6])
{
    const char* pKeyStr = convertRawToKeyString(ucModifiers, rawKeys);

    // LogWrite(MODULE_PREFIX, LOG_DEBUG, "ASCII %02x RAWKEY %02x %02x %02x %02x %02x %02x Mod %02x",
    //                 asciiCode, rawKeys[0], rawKeys[1], rawKeys[2], rawKeys[3], rawKeys[4], rawKeys[5], ucModifiers);

    // Send chars to host
    if (strlen(pKeyStr) == 0)
        return;

    // Send to host
    if (_emulate6850)
    {
        for (uint32_t i = 0; i < strlen(pKeyStr); i++)
        {
            if (_sendToTargetBufPos.canPut())
            {
                _sendToTargetBuf[_sendToTargetBufPos.posToPut()] = pKeyStr[i];
                _sendToTargetBufPos.hasPut();

                // Interrupt if enabled
                if (_emulationInterruptOnRx)
                    _mcManager.targetIrq();
            }
        }
    }
    else
    {
        _mcManager.sendKeyStrToTargetStatic(pKeyStr);
    }
}

// Handle a file
bool McTerminal::fileHandler(const char* pFileInfo, const uint8_t* pFileData, int fileLen,
                TargetProgrammer& targetProgrammer)
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
        baseAddr = strtoul(baseAddrStr, NULL, 16);
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "Processing binary file, baseAddr %04x len %d", baseAddr, fileLen);
    targetProgrammer.addMemoryBlock(baseAddr, pFileData, fileLen);
    return true;
}

// Handle a request for memory or IO - or possibly something like in interrupt vector in Z80
void McTerminal::busAccessCallback( uint32_t addr,  uint32_t data, 
             uint32_t flags,  uint32_t& retVal)
{

    // static int debugCount = 0;

    // Check for uart emulation
    if (_emulate6850)
    {
        // See if IORQ at appropriate address
        if ((flags & BR_CTRL_BUS_IORQ_MASK) && (!(flags & BR_CTRL_BUS_M1_MASK)) && ((addr & 0xc0) == 0x80))
        {
            if (flags & BR_CTRL_BUS_RD_MASK)
            {
                if ((addr & 0x01) == 0)
                {
                    // Read status
                    // Set the transmit data empty high to indicate UART can transmit
                    retVal = 0x02;

                    // Check if anything to send to target
                    if (_sendToTargetBufPos.canGet())
                    {
                        retVal |= 0x01;
                    }

                    // Check if reset needed - in which case return overrun and framing errors
                    if (_emulation6850NeedsReset)
                        retVal |= 0x30;
                    else if (_emulation6850NotSetup)
                        retVal = 0;
        
                    // LogWrite(MODULE_PREFIX, LOG_DEBUG, "IORQ READ %04x returning %02x", addr, retVal);
                }
                else
                {
                    // Read received data
                    retVal = 0;
                    if (_sendToTargetBufPos.canGet())
                    {
                        retVal = _sendToTargetBuf[_sendToTargetBufPos.posToGet()];
                        _sendToTargetBufPos.hasGot();

                        // Interrupt if chars still available
                        if ((_emulationInterruptOnRx) && _sendToTargetBufPos.canGet())
                            _mcManager.targetIrq();
                    }
                    _emulation6850NeedsReset = false;
                }
            }
            else if (flags & BR_CTRL_BUS_WR_MASK)
            {
                if ((addr & 0x01) == 0)
                {
                    // Write control
                    // Check interrupt on rx char available
                    _emulationInterruptOnRx = ((data & 0x80) != 0);
                    // Reset
                    if ((data & 0x03) == 0x03)
                    {
                        _emulation6850NeedsReset = false;
                        _emulation6850NotSetup = true;
                    }
                    else if (_emulation6850NotSetup)
                    {
                        _emulation6850NotSetup = false;
                    }
                }
                else
                {
                    // Write data - send to terminal window
                    if (_pTerminalEmulation)
                        _pTerminalEmulation->putChar(data);
                }
            }
            // if (debugCount < 50)
            // {
            // LogWrite(MODULE_PREFIX, LOG_DEBUG, "IORQ %s %04x data %02x retVal %02x irq %d",
            //         (flags & BR_CTRL_BUS_RD_MASK) ? "RD" : ((flags & BR_CTRL_BUS_WR_MASK) ? "WR" : "??"),
            //         addr, 
            //         data,
            //         retVal,
            //         _emulationInterruptOnRx);
            //     debugCount++;
            // }
        }
    }
}

// Bus action complete callback
void McTerminal::busActionCompleteCallback( BR_BUS_ACTION actionType)
{
    if (actionType == BR_BUS_ACTION_RESET)
    {
        _emulation6850NeedsReset = true;
        _emulation6850NotSetup = false;
    }
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