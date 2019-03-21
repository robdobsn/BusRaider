// Bus Raider Machine RobsZ80
// Rob Dobson 2018

#include "McRobsZ80.h"
#include "usb_hid_keys.h"
#include "../System/wgfx.h"
#include "../TargetBus/BusAccess.h"
#include "../TargetBus/TargetState.h"
#include "../System/rdutils.h"
#include "../System/ee_printf.h"
#include "../TargetBus/TargetDebug.h"
#include "../Machines/McManager.h"
#include <stdlib.h>
#include <string.h>

const char* McRobsZ80::_logPrefix = "RobsZ80";

McDescriptorTable McRobsZ80::_descriptorTable = {
    // Machine name
    "Rob's Z80",
    // Processor
    McDescriptorTable::PROCESSOR_Z80,
    // Required display refresh rate
    .displayRefreshRatePerSec = 30,
    .displayPixelsX = 512,
    .displayPixelsY = 256,
    .displayCellX = 8,
    .displayCellY = 16,
    .pixelScaleX = 2,
    .pixelScaleY = 2,
    .pFont = &__systemFont,
    .displayForeground = DISPLAY_FX_WHITE,
    .displayBackground = DISPLAY_FX_BLACK,
    // Clock
    .clockFrequencyHz = 12000000,
    // Interrupt rate per second
    .irqRate = 0,
    // Bus monitor
    .monitorIORQ = false,
    .monitorMREQ = false,
    .emulatedRAM = false,
    .setRegistersByInjection = false,
    .setRegistersCodeAddr = 0
};

// Enable machine
void McRobsZ80::enable([[maybe_unused]]int subType)
{
    _screenBufferValid = false;
    LogWrite(_logPrefix, LOG_DEBUG, "Enabling");
}

// Disable machine
void McRobsZ80::disable()
{
    LogWrite(_logPrefix, LOG_DEBUG, "Disabling");
}

// Handle display refresh (called at a rate indicated by the machine's descriptor table)
void McRobsZ80::displayRefresh()
{
    // Read memory at the location of the memory mapped screen
    unsigned char pScrnBuffer[ROBSZ80_DISP_RAM_SIZE];
    bool dataValid = McManager::blockRead(ROBSZ80_DISP_RAM_ADDR, pScrnBuffer, ROBSZ80_DISP_RAM_SIZE, 1, 0);
    if (!dataValid)
        return;

    // Write to the display on the Pi Zero
    int bytesPerRow = _descriptorTable.displayPixelsX/8;
    for (uint32_t bufIdx = 0; bufIdx < ROBSZ80_DISP_RAM_SIZE; bufIdx++)
    {
        if (!_screenBufferValid || (_screenBuffer[bufIdx] != pScrnBuffer[bufIdx]))
        {
            _screenBuffer[bufIdx] = pScrnBuffer[bufIdx];
            // Set the pixels in this byte
            int pixMask = 0x80;
            for (int i = 0; i < 8; i++)
            {
                int x = ((bufIdx % bytesPerRow) * 8) + i;
                int y = bufIdx / bytesPerRow;
                wgfxSetMonoPixel(MC_WINDOW_NUMBER, x, y, (pScrnBuffer[bufIdx] & pixMask) ? 1 : 0);
                pixMask = pixMask >> 1;
            }
        }
    }
    _screenBufferValid = true;
}

// Handle a key press
void McRobsZ80::keyHandler([[maybe_unused]] unsigned char ucModifiers, [[maybe_unused]] const unsigned char rawKeys[6])
{
}

// Handle a file
void McRobsZ80::fileHandler(const char* pFileInfo, const uint8_t* pFileData, int fileLen)
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
uint32_t McRobsZ80::busAccessCallback([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, 
        [[maybe_unused]] uint32_t flags, [[maybe_unused]] uint32_t retVal)
{
    // Not decoded
    return retVal;
}

