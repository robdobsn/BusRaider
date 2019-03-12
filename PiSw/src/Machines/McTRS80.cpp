// Bus Raider Machine TRS80
// Rob Dobson 2018

#include "McTRS80.h"
#include "usb_hid_keys.h"
#include "../System/wgfx.h"
#include "../TargetBus/BusAccess.h"
#include "../TargetBus/TargetState.h"
#include "../System/rdutils.h"
#include <stdlib.h>
#include "../FileFormats/McTRS80CmdFormat.h"
#include "../TargetBus/TargetDebug.h"
#include "../Machines/McManager.h"

const char* McTRS80::_logPrefix = "TRS80";

extern WgfxFont __TRS80Level3Font;

McDescriptorTable McTRS80::_descriptorTable = {
    // Machine name
    "TRS80",
    // Processor
    McDescriptorTable::PROCESSOR_Z80,
    // Required display refresh rate
    .displayRefreshRatePerSec = 30,
    .displayPixelsX = 8 * 64,
    .displayPixelsY = 24 * 16,
    .displayCellX = 8,
    .displayCellY = 24,
    .pixelScaleX = 2,
    .pixelScaleY = 1,
    .pFont = &__TRS80Level3Font,
    .displayForeground = WGFX_GREEN,
    .displayBackground = WGFX_BLACK,
    // Clock
    .clockFrequencyHz = 1770000,
    // Interrupt rate per second
    .irqRate = 0,
    // Bus monitor
    .monitorIORQ = true,
    .monitorMREQ = false,
    .emulatedRAM = false,
    .emulatedRAMStart = 0,
    .emulatedRAMLen = 0x100000,
    .setRegistersByInjection = false,
    .setRegistersCodeAddr = TRS80_DISP_RAM_ADDR
};

// Enable machine
void McTRS80::enable([[maybe_unused]]int subType)
{
    // Invalidate screen buffer
    _screenBufferValid = false;
    _keyBufferDirty = false;

    LogWrite(_logPrefix, LOG_DEBUG, "Enabling TRS80 EmuRAM %s (%04x-%04x) RegByInject %s IntIORQ %s IntMREQ %s (%04x)",
                _descriptorTable.emulatedRAM ? "Y" : "N",
                _descriptorTable.emulatedRAMStart, _descriptorTable.emulatedRAMStart + _descriptorTable.emulatedRAMLen,
                _descriptorTable.setRegistersByInjection ?  "Y" : "N",
                _descriptorTable.monitorIORQ ?  "Y" : "N",
                _descriptorTable.monitorMREQ || _descriptorTable.emulatedRAM ?  "Y" : "N",
                _descriptorTable.setRegistersCodeAddr);
    BusAccess::accessCallbackAdd(memoryRequestCallback);
    // Bus raider enable wait states
    BusAccess::waitSetup(_descriptorTable.monitorIORQ, _descriptorTable.monitorMREQ || _descriptorTable.emulatedRAM);
}

// Disable machine
void McTRS80::disable()
{
    LogWrite(_logPrefix, LOG_DEBUG, "Disabling TRS80");
    // Bus raider disable wait states
    BusAccess::waitSetup(false, false);
    BusAccess::accessCallbackRemove();
}

// void McTRS80::handleRegisters(Z80Registers& regs)
// {
//     // Handle the execution address
//     uint32_t execAddr = regs.PC;
//     uint8_t jumpCmd[3] = { 0xc3, uint8_t(execAddr & 0xff), uint8_t((execAddr >> 8) & 0xff) };
//     TargetState::addMemoryBlock(0, jumpCmd, 3);
//     LogWrite(_logPrefix, LOG_DEBUG, "Added JMP %04x at 0000", execAddr);
// }

// Handle display refresh (called at a rate indicated by the machine's descriptor table)
void McTRS80::displayRefresh()
{
    // Read memory of RC2014 at the location of the TRS80 memory mapped screen
    unsigned char pScrnBuffer[TRS80_DISP_RAM_SIZE];
    bool dataValid = McManager::blockRead(TRS80_DISP_RAM_ADDR, pScrnBuffer, TRS80_DISP_RAM_SIZE, 1, 0);
    if (!dataValid)
        return;

    // Write to the display on the Pi Zero
    int cols = _descriptorTable.displayPixelsX / _descriptorTable.displayCellX;
    int rows = _descriptorTable.displayPixelsY / _descriptorTable.displayCellY;
    for (int k = 0; k < rows; k++) 
    {
        for (int i = 0; i < cols; i++)
        {
            int cellIdx = k * cols + i;
            if (!_screenBufferValid || (_screenBuffer[cellIdx] != pScrnBuffer[cellIdx]))
            {
                wgfx_putc(MC_WINDOW_NUMBER, i, k, pScrnBuffer[cellIdx]);
                _screenBuffer[cellIdx] = pScrnBuffer[cellIdx];
            }
        }
    }
    _screenBufferValid = true;

    // Check for key presses and send to the TRS80 if necessary
    if (_keyBufferDirty)
    {
        McManager::blockWrite(TRS80_KEYBOARD_ADDR, _keyBuffer, TRS80_KEYBOARD_RAM_SIZE, 1, 0);
        _keyBufferDirty = false;
    }
}

// Handle a key press
void McTRS80::keyHandler(unsigned char ucModifiers, const unsigned char rawKeys[6])
{
    // LogWrite(FromTRS80, 4, "Key %02x %02x", ucModifiers, rawKeys[0]);

    // TRS80 keyboard is mapped as follows
    // Addr Bit 0       1       2       3       4       5       6       7
    // 3801     @       A       B       C       D       E       F       G
    // 3802     H       I       J       K       L       M       N       O
    // 3804     P       Q       R       S       T       U       V       W
    // 3808     X       Y       Z
    // 3810     0       1       2       3       4       5       6       7
    // 3820     8       9       *       +       <       =       >       ?
    // 3840     Enter   Clear   Break   Up      Down    Left    Right   Space
    // 3880     Shift   *****                   Control

    static const int TRS80_KEY_BYTES = 8;
    uint8_t keybdBytes[TRS80_KEY_BYTES];
    for (int i = 0; i < TRS80_KEY_BYTES; i++)
        keybdBytes[i] = 0;

    // Go through key codes
    int suppressShift = 0;
    int suppressCtrl = 0;
    for (int keyIdx = 0; keyIdx < 6; keyIdx++) {
        // Key
        unsigned char rawKey = rawKeys[keyIdx];

        // Handle key
        if ((rawKey >= KEY_A) && (rawKey <= KEY_Z)) {
            // Handle A..Z
            int bitIdx = ((rawKey - KEY_A) + 1) % 8;
            keybdBytes[(((rawKey - KEY_A) + 1) / 8)] |= (1 << bitIdx);
        } else if ((rawKey == KEY_2) && ((ucModifiers & KEY_MOD_LSHIFT) != 0)) {
            // Handle @
            keybdBytes[0] |= 1;
            suppressShift = 1;
        } else if ((rawKey == KEY_6) && ((ucModifiers & KEY_MOD_LSHIFT) != 0)) {
            // Handle ^
            suppressShift = 1;
        } else if ((rawKey == KEY_7) && ((ucModifiers & KEY_MOD_LSHIFT) != 0)) {
            // Handle &
            keybdBytes[4] |= 0x40;
        } else if ((rawKey == KEY_8) && (ucModifiers & KEY_MOD_LSHIFT)) {
            // Handle *
            keybdBytes[5] |= 4;
        } else if ((rawKey == KEY_9) && (ucModifiers & KEY_MOD_LSHIFT)) {
            // Handle (
            keybdBytes[5] |= 1;
            keybdBytes[7] |= 0x01;
        } else if ((rawKey == KEY_0) && (ucModifiers & KEY_MOD_LSHIFT)) {
            // Handle )
            keybdBytes[5] |= 2;
            keybdBytes[7] |= 0x01;
        } else if ((rawKey >= KEY_1) && (rawKey <= KEY_9)) {
            // Handle 1..9
            int bitIdx = ((rawKey - KEY_1) + 1) % 8;
            keybdBytes[(((rawKey - KEY_1) + 1) / 8) + 4] |= (1 << bitIdx);
        } else if (rawKey == KEY_0) {
            // Handle 0
            keybdBytes[4] |= 1;
        } else if ((rawKey == KEY_SEMICOLON) && ((ucModifiers & KEY_MOD_LSHIFT) == 0)) {
            // Handle ;
            keybdBytes[5] |= 8;
            suppressShift = 1;
        } else if ((rawKey == KEY_SEMICOLON) && (ucModifiers & KEY_MOD_LSHIFT)) {
            // Handle :
            keybdBytes[5] |= 4;
            suppressShift = 1;
        } else if ((rawKey == KEY_APOSTROPHE) && (ucModifiers & KEY_MOD_LSHIFT)) {
            // Handle "
            keybdBytes[4] |= 4;
            keybdBytes[7] |= 0x01;
        } else if (rawKey == KEY_COMMA) {
            // Handle <
            keybdBytes[5] |= 0x10;
        } else if (rawKey == KEY_DOT) {
            // Handle >
            keybdBytes[5] |= 0x40;
        } else if ((rawKey == KEY_EQUAL) && ((ucModifiers & KEY_MOD_LSHIFT) == 0)) {
            // Handle =
            keybdBytes[5] |= 0x20;
            keybdBytes[7] |= 0x01;
        } else if ((rawKey == KEY_EQUAL) && ((ucModifiers & KEY_MOD_LSHIFT) != 0)) {
            // Handle +
            keybdBytes[5] |= 0x8;
            keybdBytes[7] |= 0x01;
        } else if ((rawKey == KEY_MINUS) && ((ucModifiers & KEY_MOD_LSHIFT) == 0)) {
            // Handle -
            keybdBytes[5] |= 0x20;
            suppressShift = 1;
        } else if (rawKey == KEY_SLASH) {
            // Handle ?
            keybdBytes[5] |= 0x80;
        } else if (rawKey == KEY_ENTER) {
            // Handle Enter
            keybdBytes[6] |= 0x01;
        } else if (rawKey == KEY_BACKSPACE) {
            // Treat as LEFT
            keybdBytes[6] |= 0x20;
        } else if (rawKey == KEY_ESC) {
            // Handle Break
            keybdBytes[6] |= 0x04;
        } else if (rawKey == KEY_UP) {
            // Handle Up
            keybdBytes[6] |= 0x08;
        } else if (rawKey == KEY_DOWN) {
            // Handle Down
            keybdBytes[6] |= 0x10;
        } else if (rawKey == KEY_LEFT) {
            // Handle Left
            keybdBytes[6] |= 0x20;
        } else if (rawKey == KEY_RIGHT) {
            // Handle Right
            keybdBytes[6] |= 0x40;
        } else if (rawKey == KEY_SPACE) {
            // Handle Space
            keybdBytes[6] |= 0x80;
        } else if (rawKey == KEY_F1) {
            // Handle CLEAR
            keybdBytes[6] |= 0x02;
        } else if (rawKey == KEY_LEFTSHIFT) {
            // Handle Left Shift
            keybdBytes[7] |= 0x01;
        } else if (rawKey == KEY_RIGHTSHIFT) {
            // Handle Left Shift
            keybdBytes[7] |= 0x02;
        } else if ((rawKey == KEY_LEFTCTRL) || (rawKey == KEY_RIGHTCTRL)) {
            // Handle <
            keybdBytes[7] |= 0x10;
        }
    }

    // Suppress shift keys if needed
    if (suppressShift) {
        keybdBytes[7] &= 0xfc;
    }
    if (suppressCtrl) {
        keybdBytes[7] &= 0xef;
    }

    // Build RAM map
    uint8_t kbdMap[TRS80_KEYBOARD_RAM_SIZE];
    for (uint32_t i = 0; i < TRS80_KEYBOARD_RAM_SIZE; i++) {
        // Clear initially
        kbdMap[i] = 0;
        // Set all locations that would be set in real TRS80 due to
        // matrix operation of keyboard on address lines
        for (int j = 0; j < TRS80_KEY_BYTES; j++) {
            if (i & (1 << j))
                kbdMap[i] |= keybdBytes[j];
        }
        // Check for changes
        if (kbdMap[i] != _keyBuffer[i])
        {
            _keyBuffer[i] = kbdMap[i];
            _keyBufferDirty = true;
        }
    }

    // DEBUG
    // for (int i = 0; i < 16; i++)
    // {
    //     uart_printf("%02x..", i*16);
    //     for (int j = 0; j < 16; j++)
    //     {
    //         uart_printf("%02x ", kbdMap[i*16+j]);
    //     }
    //     uart_printf("\n");
    // }
}

// Handle a file
void McTRS80::fileHandler(const char* pFileInfo, const uint8_t* pFileData, int fileLen)
{
    // Get the file type (extension of file name)
    #define MAX_VALUE_STR 30
    #define MAX_FILE_NAME_STR 100
    char fileName[MAX_FILE_NAME_STR+1];
    if (!jsonGetValueForKey("fileName", pFileInfo, fileName, MAX_FILE_NAME_STR))
        return;
    // Check type of file (assume extension is delimited by .)
    const char* pFileType = strstr(fileName, ".");
    const char* pEmpty = "";
    if (pFileType == NULL)
        pFileType = pEmpty;
    if (strcasecmp(pFileType, ".cmd") == 0)
    {
        // TRS80 command file
        McTRS80CmdFormat cmdFormat;
        LogWrite(_logPrefix, LOG_DEBUG, "Processing TRS80 CMD file len %d", fileLen);
        cmdFormat.proc(TargetState::addMemoryBlock, TargetState::setTargetRegisters, pFileData, fileLen);
    }
    else
    {
        // Treat everything else as a binary file
        uint16_t baseAddr = 0;
        char baseAddrStr[MAX_VALUE_STR+1];
        if (jsonGetValueForKey("baseAddr", pFileInfo, baseAddrStr, MAX_VALUE_STR))
            baseAddr = strtol(baseAddrStr, NULL, 16);
        LogWrite(_logPrefix, LOG_DEBUG, "Processing binary file, baseAddr %04x len %d", baseAddr, fileLen);
        TargetState::addMemoryBlock(baseAddr, pFileData, fileLen);
    }
}

// Handle a request for memory or IO - or possibly something like in interrupt vector in Z80
uint32_t McTRS80::memoryRequestCallback([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, [[maybe_unused]] uint32_t flags)
{
    uint32_t retVal = BR_MEM_ACCESS_RSLT_NOT_DECODED;

    // Check for read from IO
    if ((flags & BR_CTRL_BUS_RD_MASK) && (flags & BR_CTRL_BUS_IORQ_MASK))
    {
        // Decode port
        if (addr == 0x13)  // Joystick
        {
            // Indicate no buttons are pressed
            retVal = 0xff;
        }
    }
    
    // Callback to debugger
    TargetDebug* pDebug = TargetDebug::get();
    if (pDebug)
        retVal = pDebug->handleInterrupt(addr, data, flags, retVal, _descriptorTable);

    return retVal;
}
