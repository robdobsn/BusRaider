// Bus Raider Machine ZXSpectrum
// Rob Dobson 2018

#include "McZXSpectrum.h"
#include "usb_hid_keys.h"
#include "wgfx.h"
#include "busraider.h"
#include "rdutils.h"
#include "target_memory_map.h"

static const char* LogPrefix = "ZXSpectrum";

McDescriptorTable McZXSpectrum::_descriptorTable = {
    // Machine name
    "ZX Spectrum",
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
    .clockFrequencyHz = 3500000
};

// Enable machine
void McZXSpectrum::enable()
{
    LogWrite(LogPrefix, LOG_DEBUG, "Enabling");
    br_set_bus_access_callback(memoryRequestCallback);
}

// Disable machine
void McZXSpectrum::disable()
{
    LogWrite(LogPrefix, LOG_DEBUG, "Disabling");
    br_remove_bus_access_callback();
}

void McZXSpectrum::handleExecAddr(uint32_t execAddr)
{
    // Handle the execution address
    uint8_t jumpCmd[3] = { 0xc3, uint8_t(execAddr & 0xff), uint8_t((execAddr >> 8) & 0xff) };
    targetDataBlockStore(0, jumpCmd, 3);
    LogWrite(LogPrefix, LOG_DEBUG, "Added JMP %04x at 0000", execAddr);
}

// Handle display refresh (called at a rate indicated by the machine's descriptor table)
void McZXSpectrum::displayRefresh()
{
    // Read memory at the location of the memory mapped screen
    unsigned char pScrnBuffer[ZXSPECTRUM_DISP_RAM_SIZE];
    br_read_block(ZXSPECTRUM_DISP_RAM_ADDR, pScrnBuffer, ZXSPECTRUM_DISP_RAM_SIZE, 1, 0);

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
}

// Handle a key press
void McZXSpectrum::keyHandler([[maybe_unused]] unsigned char ucModifiers, [[maybe_unused]] const unsigned char rawKeys[6])
{
}

// Handle a file
void McZXSpectrum::fileHander(const char* pFileInfo, const uint8_t* pFileData, int fileLen)
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
    LogWrite(LogPrefix, LOG_DEBUG, "Processing binary file, baseAddr %04x len %d", baseAddr, fileLen);
    targetDataBlockStore(baseAddr, pFileData, fileLen);
}

// Handle a request for memory or IO - or possibly something like in interrupt vector in Z80
uint32_t McZXSpectrum::memoryRequestCallback([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, [[maybe_unused]] uint32_t flags)
{
    // Check for read
    if (flags & BR_CTRL_BUS_RD)
    {
        return 0;
    }

    // Not read
    return 0;
}

