// Bus Raider Machine DebugZ80
// Rob Dobson 2018

#include "McDebugZ80.h"
#include "usb_hid_keys.h"
#include "wgfx.h"
#include "busraider.h"
#include "rdutils.h"
#include "target_memory_map.h"

const char* McDebugZ80::_logPrefix = "DebugZ80";

uint8_t McDebugZ80::_systemRAM[DEBUGZ80_RAM_SIZE];

volatile bool McDebugZ80::_scrnBufDirtyFlag = true;

McDebugZ80::McDebugZ80() : McBase()
{
}

McDescriptorTable McDebugZ80::_descriptorTable = {
    // Machine name
    "Debug Z80",
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
    .clockFrequencyHz = 100000
};


// Enable machine
void McDebugZ80::enable()
{
    LogWrite(_logPrefix, LOG_DEBUG, "Enabling");
    br_set_bus_access_callback(memoryRequestCallback);
    // Bus raider enable wait states on MREQ and IORQ
    br_enable_mem_and_io_access(true, true, false, false, false);
}

// Disable machine
void McDebugZ80::disable()
{
    LogWrite(_logPrefix, LOG_DEBUG, "Disabling");
    // Bus raider disable wait states
    br_enable_mem_and_io_access(false, false, false, false, false);
    br_remove_bus_access_callback();
}

void McDebugZ80::handleExecAddr(uint32_t execAddr)
{
    // Handle the execution address
    uint8_t jumpCmd[3] = { 0xc3, uint8_t(execAddr & 0xff), uint8_t((execAddr >> 8) & 0xff) };
    targetDataBlockStore(0, jumpCmd, 3);
    LogWrite(_logPrefix, LOG_DEBUG, "Added JMP %04x at 0000", execAddr);
}

static int debugVals[10] = { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1 };
static int lastDebug[10] = { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1 };

// Handle display refresh (called at a rate indicated by the machine's descriptor table)
void McDebugZ80::displayRefresh()
{
    // Write to the display on the Pi Zero
    if (_scrnBufDirtyFlag)
    {
        _scrnBufDirtyFlag = false;
        int bytesPerRow = _descriptorTable.displayPixelsX/8;
        for (uint32_t bufIdx = 0; bufIdx < DEBUGZ80_DISP_RAM_SIZE; bufIdx++)
        {
            if (_pScrnBufCopy[bufIdx] != _systemRAM[bufIdx + DEBUGZ80_DISP_RAM_ADDR])
            {
                _pScrnBufCopy[bufIdx] = _systemRAM[bufIdx + DEBUGZ80_DISP_RAM_ADDR];
                // Set the pixels in this byte
                int pixMask = 0x80;
                for (int i = 0; i < 8; i++)
                {
                    int x = ((bufIdx % bytesPerRow) * 8) + i;
                    int y = bufIdx / bytesPerRow;
                    wgfxSetMonoPixel(MC_WINDOW_NUMBER, x, y, (_pScrnBufCopy[bufIdx] & pixMask) ? 1 : 0);
                    pixMask = pixMask >> 1;
                }
            }
        }
    }

    for (unsigned int i = 0; i < sizeof(debugVals)/sizeof(debugVals[0]); i++)
    {   
        if ((lastDebug[i] != debugVals[i]) && (debugVals[i] != -1))
        {
            LogWrite(_logPrefix, LOG_WARNING, "debug %d == %02x", i, debugVals[i]);
            lastDebug[i] = debugVals[i];
        }
    }
    // LogWrite(_logPrefix, LOG_WARNING, "Disp: %02x %02x %02x %02x", _systemRAM[0xc000],_systemRAM[0xc001],_systemRAM[0xc002],_systemRAM[0xc003]);
}

// Handle a key press
void McDebugZ80::keyHandler([[maybe_unused]] unsigned char ucModifiers, [[maybe_unused]] const unsigned char rawKeys[6])
{
}

// Handle a file
void McDebugZ80::fileHandler(const char* pFileInfo, const uint8_t* pFileData, int fileLen)
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
    targetDataBlockStore(baseAddr, pFileData, fileLen);
}

// Handle reset for the machine - if false returned then the bus raider will issue a hardware reset
bool McDebugZ80::reset()
{
    static uint8_t program[] = { 0x3e, 0xf0, 0x21, 0x00, 0xc0, 0x11, 0x01, 0xc0, 0x77, 0x01, 0x00, 0x40, 0xed, 0xb0, 0x3c, 0xfe,
                    0x7e, 0xc2, 0x02, 0x00, 0xc3, 0x00, 0x00 };
    // static uint8_t program[] = { 0xc3, 0x00, 0x00 };
    memset(_systemRAM, 0, DEBUGZ80_RAM_SIZE);
    memcpy(_systemRAM, program, sizeof(program));
    memset(_pScrnBufCopy, 0, DEBUGZ80_DISP_RAM_SIZE);
    
    LogWrite(_logPrefix, LOG_WARNING, "RESETTING");
    br_reset_host();
    LogWrite(_logPrefix, LOG_WARNING, "RESETDEBUGVARS");
    for (unsigned int i = 0; i < sizeof(debugVals)/sizeof(debugVals[0]); i++)
    {   
        lastDebug[i] = -1;
        debugVals[i] = -1;
    }
    LogWrite(_logPrefix, LOG_WARNING, "RESET");

    return true;
}

// Handle a request for memory or IO - or possibly something like an interrupt vector in Z80
uint32_t McDebugZ80::memoryRequestCallback([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, [[maybe_unused]] uint32_t flags)
{
    // Check for read
    if (flags & (1 << BR_CTRL_BUS_RD))
    {
        if (flags & (1 << BR_CTRL_BUS_MREQ))
        {
            if (addr < 3)
            {
                debugVals[addr+1] = _systemRAM[addr];
            }
            else
            {
                if (debugVals[0] == -1)
                    debugVals[0] = addr; 
            }

            return _systemRAM[addr];
        }
        // // Decode port
        // if (addr == 0x13)  // Joystick
        // {
        //     // Indicate no buttons are pressed
        //     return 0xff;
        // }

        // Other IO ports are not decoded
        return BR_MEM_ACCESS_RSLT_NOT_DECODED;
    }
    else if (flags & (1 << BR_CTRL_BUS_WR))
    {
        if (flags & (1 << BR_CTRL_BUS_MREQ))
        {
            if ((addr >= DEBUGZ80_DISP_RAM_ADDR) && (addr < DEBUGZ80_DISP_RAM_ADDR+DEBUGZ80_DISP_RAM_SIZE))
            {
                _scrnBufDirtyFlag = true;
                _systemRAM[addr] = data;
                if (addr == 0xc000)
                {
                    debugVals[5] = addr >> 8;
                    debugVals[6] = addr & 0xff;
                    debugVals[4] = data;
                }
            }
        }
    }

    // Not decoded
    return BR_MEM_ACCESS_RSLT_NOT_DECODED;
}

