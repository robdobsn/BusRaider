// Bus Raider Machine ZXSpectrum
// Rob Dobson 2018

#include "McZXSpectrum.h"
#include "usb_hid_keys.h"
#include "../System/wgfx.h"
#include "../TargetBus/BusAccess.h"
#include "../TargetBus/TargetState.h"
#include "../Utils/rdutils.h"
#include <stdlib.h>
#include "../FileFormats/McZXSpectrumTZXFormat.h"
#include "../FileFormats/McZXSpectrumZ80Format.h"
#include "../Debugger/TargetDebug.h"
#include "../Machines/McManager.h"

const char* McZXSpectrum::_logPrefix = "ZXSpectrum";

unsigned char McZXSpectrum::_curKeyModifiers = 0;
unsigned char McZXSpectrum::_curKeys[MAX_KEYS];

McDescriptorTable McZXSpectrum::_descriptorTable = {
    // Machine name
    "ZX Spectrum",
    McDescriptorTable::PROCESSOR_Z80,
    // Required display refresh rate
    .displayRefreshRatePerSec = 30,
    .displayPixelsX = 256,
    .displayPixelsY = 192,
    .displayCellX = 8,
    .displayCellY = 8,
    .pixelScaleX = 4,
    .pixelScaleY = 3,
    .pFont = &__systemFont,
    .displayForeground = WGFX_WHITE,
    .displayBackground = WGFX_BLACK,
    // Clock
    .clockFrequencyHz = 3500000,
    // Bus monitor
    .monitorIORQ = true,
    .monitorMREQ = false,
    .emulatedRAMStart = 0,
    .emulatedRAMLen = 0,
    .setRegistersByInjection = false,
    .setRegistersCodeAddr = ZXSPECTRUM_DISP_RAM_ADDR
};

// Enable machine
void McZXSpectrum::enable()
{
    _screenBufferValid = false;
    LogWrite(_logPrefix, LOG_DEBUG, "Enabling");
    BusAccess::accessCallbackAdd(memoryRequestCallback);
    // Bus raider enable wait states on IORQ
    BusAccess::waitEnable(_descriptorTable.monitorIORQ, _descriptorTable.monitorMREQ);
}

// Disable machine
void McZXSpectrum::disable()
{
    LogWrite(_logPrefix, LOG_DEBUG, "Disabling");
    BusAccess::waitEnable(false, false);
    BusAccess::accessCallbackRemove();
}

void McZXSpectrum::handleRegisters(Z80Registers& regs)
{
    // Handle the registers
    TargetState::setTargetRegisters(regs);

    // 

    // Handle the execution address
    // LogWrite(_logPrefix, LOG_DEBUG, "Doing nothing with registers %04x\n", regs.PC);


    // TODO - maybe store register set program in screen memory ??
    // uint32_t execAddr = regs.PC;
    // uint8_t jumpCmd[3] = { 0xc3, uint8_t(execAddr & 0xff), uint8_t((execAddr >> 8) & 0xff) };
    // TargetState::addMemoryBlock(0, jumpCmd, 3);
    // LogWrite(_logPrefix, LOG_DEBUG, "Added JMP %04x at 0000", execAddr);

}

// Handle display refresh (called at a rate indicated by the machine's descriptor table)
void McZXSpectrum::displayRefresh()
{
    // Read memory at the location of the memory mapped screen
    unsigned char pScrnBuffer[ZXSPECTRUM_DISP_RAM_SIZE];
    McManager::blockRead(ZXSPECTRUM_DISP_RAM_ADDR, pScrnBuffer, ZXSPECTRUM_DISP_RAM_SIZE, 1, 0);

    // Check for colour data change - refresh everything if changed
    for (uint32_t colrIdx = ZXSPECTRUM_PIXEL_RAM_SIZE; colrIdx < ZXSPECTRUM_DISP_RAM_SIZE; colrIdx++)
    {
        if (pScrnBuffer[colrIdx] != _screenBuffer[colrIdx])
        {
            _screenBufferValid = false;
           break;
        }
    }

    // Write to the display on the Pi Zero
    int bytesPerRow = _descriptorTable.displayPixelsX/8;
    int colrCellsPerRow = _descriptorTable.displayPixelsX/_descriptorTable.displayCellX;
    for (uint32_t bufIdx = 0; bufIdx < ZXSPECTRUM_PIXEL_RAM_SIZE; bufIdx++)
    {
        if (!_screenBufferValid || (_screenBuffer[bufIdx] != pScrnBuffer[bufIdx]))
        {
            // Save new version of screen buffer
            _screenBuffer[bufIdx] = pScrnBuffer[bufIdx];

            // Get colour info for this location
            int col = bufIdx % colrCellsPerRow;
            int row = (bufIdx / colrCellsPerRow);
            row = (row & 0x07) | ((row & 0xc0) >> 3);
            int colrAddr = ZXSPECTRUM_PIXEL_RAM_SIZE + (row*colrCellsPerRow) + col;

            // Bits 0..2 are INK colour, 3..5 are PAPER colour, 6 = brightness, 7 = flash
            int cellColourData = pScrnBuffer[colrAddr];

            // Set the pixels in this byte
            int pixMask = 0x80;
            for (int i = 0; i < 8; i++)
            {
                int x = ((bufIdx % bytesPerRow) * 8) + i;
                int y = bufIdx / bytesPerRow;
                // Munge y value for weird spectrum addressing
                y = ((y & 0x38) >> 3) | ((y & 0x07) << 3) | (y & 0xc0);
                // Get pixel colour
                bool pixVal = pScrnBuffer[bufIdx] & pixMask;
                int pixColour = ((cellColourData & 0x38) >> 3) | ((cellColourData & 0x80) >> 4);
                if (pixVal)
                    pixColour = (cellColourData & 0x07) | ((cellColourData & 0x80) >> 4); 
                wgfxSetColourPixel(MC_WINDOW_NUMBER, x, y, pixColour);
                // Bump the pixel mask
                pixMask = pixMask >> 1;
            }
        }
    }

    // Save colour data for later checks
    for (uint32_t colrIdx = ZXSPECTRUM_PIXEL_RAM_SIZE; colrIdx < ZXSPECTRUM_DISP_RAM_SIZE; colrIdx++)
        _screenBuffer[colrIdx] = pScrnBuffer[colrIdx];
    _screenBufferValid = true;

    // Generate a maskable interrupt to trigger Spectrum's keyboard ISR
    BusAccess::targetIRQ();
}

// Handle a key press
void McZXSpectrum::keyHandler([[maybe_unused]] unsigned char ucModifiers, [[maybe_unused]] const unsigned char rawKeys[6])
{
    _curKeyModifiers = ucModifiers;
    for (int i = 0; (i < MAX_KEYS) && (i < 6); i++)
        _curKeys[i] = rawKeys[i];
}

// Handle a file
void McZXSpectrum::fileHandler(const char* pFileInfo, const uint8_t* pFileData, int fileLen)
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

    // See if it is TZX format
    if (strcasecmp(pFileType, ".tzx") == 0)
    {
        // TZX
        McZXSpectrumTZXFormat formatHandler;
        LogWrite(_logPrefix, LOG_DEBUG, "Processing TZX file len %d", fileLen);
        formatHandler.proc(TargetState::addMemoryBlock, handleRegisters, pFileData, fileLen);
    }
    else if (strcasecmp(pFileType, ".z80") == 0)
    {
        // .Z80
        McZXSpectrumZ80Format formatHandler;
        LogWrite(_logPrefix, LOG_DEBUG, "Processing Z80 file len %d", fileLen);
        // Handle registers and injecting RET
        formatHandler.proc(TargetState::addMemoryBlock, handleRegisters, pFileData, fileLen);
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

uint32_t McZXSpectrum::getKeyPressed(const int* keyCodes, int keyCodesLen)
{
    uint32_t retVal = 0xff;
    for (int i = 0; i < MAX_KEYS; i++)
    {
        int bitMask = 0x01;
        for (int j = 0; j < keyCodesLen; j++)
        {
            if (_curKeys[i] == keyCodes[j])
                retVal &= ~bitMask;
            bitMask = bitMask << 1;
        }
    }
    return retVal;
}

// Handle a request for memory or IO - or possibly something like in interrupt vector in Z80
uint32_t McZXSpectrum::memoryRequestCallback([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, [[maybe_unused]] uint32_t flags)
{
    uint32_t retVal = BR_MEM_ACCESS_RSLT_NOT_DECODED;
    
    // Callback to debugger
    TargetDebug* pDebug = TargetDebug::get();
    if (pDebug)
        retVal = pDebug->handleInterrupt(addr, data, flags, retVal, _descriptorTable);
    if ((retVal & BR_MEM_ACCESS_INSTR_INJECT) != 0)
        return retVal;

    // Check for read from IO
    if ((flags & BR_CTRL_BUS_RD_MASK) && (flags & BR_CTRL_BUS_IORQ_MASK))
    {

        // Note that in the following I've used the KEY_HANJA as a placeholder
        // as I think it is a key that won't normally occur

        // Check for special codes in the key buffer
        bool specialKeyBackspace = false;
        for (int i = 0; i < MAX_KEYS; i++)
        {
            if (_curKeys[i] == KEY_BACKSPACE)
                specialKeyBackspace = true;
        }
        // Check if address is keyboard
        if (addr == 0xfefe)
        {
            static const int keys[] = {KEY_HANJA, KEY_Z, KEY_X, KEY_C, KEY_V};
            uint32_t keysPressed = getKeyPressed(keys, sizeof(keys)/sizeof(int));
            if (specialKeyBackspace || ((_curKeyModifiers & KEY_MOD_LSHIFT) != 0) ||
                             (_curKeyModifiers & KEY_MOD_RSHIFT) != 0)
                keysPressed &= 0xfe;
            retVal = keysPressed;
        }
        else if (addr == 0xfdfe)
        {
            static const int keys[] = {KEY_A, KEY_S, KEY_D, KEY_F, KEY_G};
            retVal = getKeyPressed(keys, sizeof(keys)/sizeof(int));
        }
        else if (addr == 0xfbfe)
        {
            static const int keys[] = {KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T};
            retVal = getKeyPressed(keys, sizeof(keys)/sizeof(int));
        }
        else if (addr == 0xf7fe)
        {
            static const int keys[] = {KEY_1, KEY_2, KEY_3, KEY_4, KEY_5};
            retVal = getKeyPressed(keys, sizeof(keys)/sizeof(int));
        }
        else if (addr == 0xeffe)
        {
            static const int keys[] = {KEY_0, KEY_9, KEY_8, KEY_7, KEY_6};
            uint32_t keysPressed = getKeyPressed(keys, sizeof(keys)/sizeof(int));
            if (specialKeyBackspace)
                keysPressed &= 0xfe;
            retVal = keysPressed;
        }
        else if (addr == 0xdffe)
        {
            static const int keys[] = {KEY_P, KEY_O, KEY_I, KEY_U, KEY_Y};
            retVal = getKeyPressed(keys, sizeof(keys)/sizeof(int));
        }
        else if (addr == 0xbffe)
        {
            static const int keys[] = {KEY_ENTER, KEY_L, KEY_K, KEY_J, KEY_H};
            retVal = getKeyPressed(keys, sizeof(keys)/sizeof(int));
        }
        else if (addr == 0x7ffe)
        {
            static const int keys[] = {KEY_SPACE, KEY_HANJA, KEY_M, KEY_N, KEY_B};
            uint32_t keysPressed = getKeyPressed(keys, sizeof(keys)/sizeof(int));
            if (((_curKeyModifiers & KEY_MOD_LCTRL) != 0) || (_curKeyModifiers & KEY_MOD_RCTRL) != 0)
                keysPressed &= 0xfd;
            retVal = keysPressed;
        }
    }

    return retVal;
}

