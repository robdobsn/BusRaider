// Bus Raider Machine RobsZ80
// Rob Dobson 2018

#include "McRobsZ80.h"
#include "usb_hid_keys.h"
#include "wgfx.h"
#include "busraider.h"
#include "rdutils.h"
#include "target_memory_map.h"

static const char* LogPrefix = "RobsZ80";

McDescriptorTable McRobsZ80::_descriptorTable = {
    // Machine name
    "Rob's Z80",
    // Required display refresh rate
    .displayRefreshRatePerSec = 30,
    .displayPixelsX = 512,
    .displayPixelsY = 256,
    .displayCellX = 8,
    .displayCellY = 16,
    .pixelScaleX = 2,
    .pixelScaleY = 2,
    .pFont = &__systemFont,
    .displayForeground = WGFX_WHITE,
    .displayBackground = WGFX_BLACK,
    // Clock
    .clockFrequencyHz = 7000000
};

// Enable machine
void McRobsZ80::enable()
{
    LogWrite(LogPrefix, LOG_DEBUG, "Enabling");
    br_set_bus_access_callback(memoryRequestCallback);
}

// Disable machine
void McRobsZ80::disable()
{
    LogWrite(LogPrefix, LOG_DEBUG, "Disabling");
    br_remove_bus_access_callback();
}

void McRobsZ80::handleExecAddr(uint32_t execAddr)
{
    // Handle the execution address
    uint8_t jumpCmd[3] = { 0xc3, uint8_t(execAddr & 0xff), uint8_t((execAddr >> 8) & 0xff) };
    targetDataBlockStore(0, jumpCmd, 3);
    LogWrite(LogPrefix, LOG_DEBUG, "Added JMP %04x at 0000", execAddr);
}

// Handle display refresh (called at a rate indicated by the machine's descriptor table)
void McRobsZ80::displayRefresh()
{
    // Read memory at the location of the memory mapped screen
    unsigned char pScrnBuffer[ROBSZ80_DISP_RAM_SIZE];
    br_read_block(ROBSZ80_DISP_RAM_ADDR, pScrnBuffer, ROBSZ80_DISP_RAM_SIZE, 1, 0);

    // Write to the display on the Pi Zero
    for (uint32_t bufIdx = 0; bufIdx < ROBSZ80_DISP_RAM_SIZE; bufIdx++)
    {
        if (!_screenBufferValid || (_screenBuffer[bufIdx] != pScrnBuffer[bufIdx]))
        {
            int bytesPerRow = _descriptorTable.displayPixelsX/8;
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
void McRobsZ80::fileHander(const char* pFileInfo, const uint8_t* pFileData, int fileLen)
{
    // Get the file type (extension of file name)
    #define MAX_VALUE_STR 30
    #define MAX_TRS80_FILE_NAME_STR 100
    char fileName[MAX_TRS80_FILE_NAME_STR+1];
    if (!jsonGetValueForKey("fileName", pFileInfo, fileName, MAX_TRS80_FILE_NAME_STR))
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
    LogWrite(LogPrefix, LOG_DEBUG, "Processing binary file, baseAddr %04x len %d", baseAddr, fileLen);
    targetDataBlockStore(baseAddr, pFileData, fileLen);
}

// Handle a request for memory or IO - or possibly something like in interrupt vector in Z80
uint32_t McRobsZ80::memoryRequestCallback([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, [[maybe_unused]] uint32_t flags)
{
    // Check for read
    if (flags & BR_CTRL_BUS_RD)
    {
        return 0;
    }

    // Not read
    return 0;
}

