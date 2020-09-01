// Bus Raider Machine RobsZ80
// Rob Dobson 2018

#include <stdlib.h>
#include <string.h>
#include "McRobsZ80.h"
#include "usb_hid_keys.h"
#include "../System/rdutils.h"
#include "../System/logging.h"
#include "../TargetBus/BusAccess.h"
#include "../TargetBus/TargetProgrammer.h"
#include "../Machines/McManager.h"
#include "../Hardware/HwManager.h"
#include "../Fonts/SystemFont.h"

static const char* MODULE_PREFIX = "RobsZ80";

McVariantTable McRobsZ80::_defaultDescriptorTables[] = {
    {
        // Machine name
        "Rob's Z80",
        // Processor
        McVariantTable::PROCESSOR_Z80,
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
        .displayMemoryMapped = true,
        // Clock
        .clockFrequencyHz = 12000000,
        // Interrupt rate per second
        .irqRate = 0,
        // Bus monitor
        .monitorIORQ = false,
        .monitorMREQ = false,
        .setRegistersCodeAddr = 0
    }
};

McRobsZ80::McRobsZ80(McManager& mcManager) : 
        McBase(mcManager, 
            _defaultDescriptorTables, 
            sizeof(_defaultDescriptorTables)/sizeof(_defaultDescriptorTables[0]))
{
    _screenBufferValid = false;
}

// Enable machine
void McRobsZ80::enable()
{
    _screenBufferValid = false;
}

// Disable machine
void McRobsZ80::disable()
{
}

// Handle display refresh (called at a rate indicated by the machine's descriptor table)
void McRobsZ80::displayRefreshFromMirrorHw()
{
    // Read mirror memory at the location of the memory mapped screen
    uint8_t pScrnBuffer[ROBSZ80_DISP_RAM_SIZE];
    if (getHwManager().blockRead(ROBSZ80_DISP_RAM_ADDR, pScrnBuffer, ROBSZ80_DISP_RAM_SIZE, false, false, true) == BR_OK)
        updateDisplayFromBuffer(pScrnBuffer, ROBSZ80_DISP_RAM_SIZE);
}

void McRobsZ80::updateDisplayFromBuffer(uint8_t* pScrnBuffer, uint32_t bufLen)
{
    DisplayBase* pDisplay = getDisplay();
    if (!pDisplay || (bufLen < ROBSZ80_DISP_RAM_SIZE))
        return;

    // Write to the display on the Pi Zero
    int bytesPerRow = getDescriptorTable().displayPixelsX/8;
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
                pDisplay->setPixel(x, y, (pScrnBuffer[bufIdx] & pixMask) ? 1 : 0, DISPLAY_FX_DEFAULT);
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
bool McRobsZ80::fileHandler(const char* pFileInfo, const uint8_t* pFileData, int fileLen)
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
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "Processing binary file, baseAddr %04x len %d", baseAddr, fileLen);
    getTargetProgrammer().addMemoryBlock(baseAddr, pFileData, fileLen);
    return true;
}

// Handle a request for memory or IO - or possibly something like in interrupt vector in Z80
void McRobsZ80::busAccessCallback([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, 
        [[maybe_unused]] uint32_t flags, [[maybe_unused]] uint32_t& retVal)
{
}

// Bus action complete callback
void McRobsZ80::busActionCompleteCallback(BR_BUS_ACTION actionType)
{
    // Check for BUSRQ
    if (actionType == BR_BUS_ACTION_BUSRQ)
    {
        // Read memory at the location of the memory mapped screen
        uint8_t pScrnBuffer[ROBSZ80_DISP_RAM_SIZE];
        if (getHwManager().blockRead(ROBSZ80_DISP_RAM_ADDR, pScrnBuffer, ROBSZ80_DISP_RAM_SIZE, false, false, false) == BR_OK)
            updateDisplayFromBuffer(pScrnBuffer, ROBSZ80_DISP_RAM_SIZE);
    }
}