// Bus Raider Machine DebugZ80
// Rob Dobson 2018

#include "McDebugZ80.h"
#include "usb_hid_keys.h"
#include "../System/wgfx.h"
#include "../TargetBus/BusAccess.h"
#include "../TargetBus/target_memory_map.h"
#include "../Utils/rdutils.h"
#include "../Debugger/TargetDebug.h"
#include <stdlib.h>

const char* McDebugZ80::_logPrefix = "DebugZ80";

uint8_t McDebugZ80::_systemRAM[DEBUGZ80_RAM_SIZE];

volatile bool McDebugZ80::_scrnBufDirtyFlag = true;

extern WgfxFont __TRS80Level3Font;

static const int NUM_DEBUG_VALS = 200;
class DebugAccessRec
{
public:
    uint32_t addr;
    uint32_t expectedAddr;
    uint32_t dataFromZ80;
    uint32_t expectedData;
    uint32_t dataToZ80;
    uint32_t flags;
    uint32_t expectedFlags;
    uint32_t stepCount;
};
static volatile DebugAccessRec debugAccessInfo[NUM_DEBUG_VALS];
static volatile int debugAccessCount = 0;
static volatile int debugAccessLastCount = 0;

int32_t volatile debugCurBCCount = 0xffff;
uint32_t volatile debugCurChVal = '0';
uint32_t volatile debugCurAddr = 0;
uint32_t volatile debugStepCount = 0;

McDebugZ80::McDebugZ80() : McBase()
{
}

McDescriptorTable McDebugZ80::_descriptorTable = {
    // Machine name
    "Debug Z80",
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
    .clockFrequencyHz = 200000,
    // Bus monitor
    .monitorIORQ = true,
    .monitorMREQ = true
};

// Enable machine
void McDebugZ80::enable()
{
    LogWrite(_logPrefix, LOG_DEBUG, "Enabling");
    BusAccess::accessCallbackAdd(memoryRequestCallback);
    // Bus raider enable wait states on MREQ and IORQ
    BusAccess::waitEnable(_descriptorTable.monitorIORQ, _descriptorTable.monitorMREQ);
}

// Disable machine
void McDebugZ80::disable()
{
    LogWrite(_logPrefix, LOG_DEBUG, "Disabling");
    // Bus raider disable wait states
    BusAccess::waitEnable(false, false);
    BusAccess::accessCallbackRemove();
}

void McDebugZ80::handleExecAddr(uint32_t execAddr)
{
    // Handle the execution address
    uint8_t jumpCmd[3] = { 0xc3, uint8_t(execAddr & 0xff), uint8_t((execAddr >> 8) & 0xff) };
    targetDataBlockStore(0, jumpCmd, 3);
    LogWrite(_logPrefix, LOG_DEBUG, "Added JMP %04x at 0000", execAddr);
}

// Handle display refresh (called at a rate indicated by the machine's descriptor table)
void McDebugZ80::displayRefresh()
{
    // Write to the display on the Pi Zero
    if (_scrnBufDirtyFlag)
    {
        _scrnBufDirtyFlag = false;
        // Write to the display on the Pi Zero
        int cols = _descriptorTable.displayPixelsX / _descriptorTable.displayCellX;
        int rows = _descriptorTable.displayPixelsY / _descriptorTable.displayCellY;
        for (int k = 0; k < rows; k++) 
        {
            for (int i = 0; i < cols; i++)
            {
                int cellIdx = k * cols + i;
                if (_pScrnBufCopy[cellIdx] != _systemRAM[cellIdx + DEBUGZ80_DISP_RAM_ADDR])
                {
                    _pScrnBufCopy[cellIdx] = _systemRAM[cellIdx + DEBUGZ80_DISP_RAM_ADDR];
                    wgfx_putc(MC_WINDOW_NUMBER, i, k, _pScrnBufCopy[cellIdx]);
                }
            }
        }
    }
    
    // Debug values
    if (debugAccessLastCount != debugAccessCount)
    {
        for (int i = debugAccessLastCount; i < debugAccessCount; i++)
        {
            int flags = debugAccessInfo[i].flags;
            int expFlags = debugAccessInfo[i].expectedFlags;
            LogWrite(_logPrefix, LOG_WARNING, "%2d %07d got %04x %02x %c%c%c%c%c%c exp %04x %02x %c%c%c%c%c%c ToZ80 %02x",
                        i, 
                        debugAccessInfo[i].stepCount,
                        debugAccessInfo[i].addr, 
                        debugAccessInfo[i].dataFromZ80, 
                        flags & 0x01 ? 'R': ' ', flags & 0x02 ? 'W': ' ', flags & 0x04 ? 'M': ' ',
                        flags & 0x08 ? 'I': ' ', flags & 0x10 ? '1': ' ', flags & 0x20 ? 'T': ' ',
                        debugAccessInfo[i].expectedAddr, 
                        debugAccessInfo[i].expectedData == 0xffff ? 0 : debugAccessInfo[i].expectedData, 
                        expFlags & 0x01 ? 'R': ' ', expFlags & 0x02 ? 'W': ' ', expFlags & 0x04 ? 'M': ' ',
                        expFlags & 0x08 ? 'I': ' ', expFlags & 0x10 ? '1': ' ', expFlags & 0x20 ? 'T': ' ',
                        debugAccessInfo[i].dataToZ80);

        }
        debugAccessLastCount = debugAccessCount;
    }
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
    // static uint8_t program[] = { 0x3e, 0xf0, 0x21, 0x00, 0xc0, 0x11, 0x01, 0xc0, 0x77, 0x01, 0x00, 0x40, 0xed, 0xb0, 0x3c, 0xfe,
    //                 0x7e, 0xc2, 0x02, 0x00, 0xc3, 0x00, 0x00 };
    // static uint8_t program[] = { 0xc3, 0x00, 0x00 };
    static uint8_t program[] = { 0x1e, 0x30, 0x21, 0x00, 0xc0, 0x73, 0x01, 0xff, 0x1f, 0x0b, 0x78, 0xb1, 0xc2, 0x09, 0x00, 
                    0x1c, 0x7b, 0xfe, 0x3a, 0xc2, 0x02, 0x00, 0xc3, 0x00, 0x00 };
    memset(_systemRAM, 0, DEBUGZ80_RAM_SIZE);
    memcpy(_systemRAM, program, sizeof(program));
    memset(_pScrnBufCopy, 0xff, DEBUGZ80_DISP_RAM_SIZE);

    static uint8_t testChars[] = "Hello world";
    memcpy(&_systemRAM[DEBUGZ80_DISP_RAM_ADDR], testChars, sizeof(testChars));
    _scrnBufDirtyFlag = true;
    // Debug
    BusAccess::waitIntDisable();
    pinMode(BR_DEBUG_PI_SPI0_CE0, OUTPUT);
    debugAccessCount = 0;
    debugAccessLastCount = 0;
    debugCurBCCount = 0;
    debugCurChVal = '0';
    debugCurAddr = 0;
    debugStepCount = 0;
    // Actual reset
    LogWrite(_logPrefix, LOG_WARNING, "RESET");
    BusAccess::targetReset();

    return true;
}

// Handle a request for memory or IO - or possibly something like an interrupt vector in Z80
uint32_t McDebugZ80::memoryRequestCallback([[maybe_unused]] uint32_t addr, 
        [[maybe_unused]] uint32_t data, [[maybe_unused]] uint32_t flags)
{
    // Check for read
    uint32_t retVal = BR_MEM_ACCESS_RSLT_NOT_DECODED;
    if (flags & BR_CTRL_BUS_RD_MASK)
    {
        if (flags & BR_CTRL_BUS_MREQ_MASK)
        {
            retVal = _systemRAM[addr];
        }
    }
    else if (flags & BR_CTRL_BUS_WR_MASK)
    {
        if (flags & BR_CTRL_BUS_MREQ_MASK)
        {
            if ((addr >= DEBUGZ80_DISP_RAM_ADDR) && (addr < DEBUGZ80_DISP_RAM_ADDR+DEBUGZ80_DISP_RAM_SIZE))
            {
                _scrnBufDirtyFlag = true;
                _systemRAM[addr] = data;
                retVal = 0;
            }
        }
    }

    // Mirror Z80 expected behaviour
    uint32_t expCtrl = BR_CTRL_BUS_RD_MASK | BR_CTRL_BUS_MREQ_MASK | BR_CTRL_BUS_WAIT_MASK;
    uint32_t nextAddr = debugCurAddr;
    uint32_t expData = 0xffff;
    switch(debugCurAddr)
    {
        case 0: nextAddr++; expCtrl |= BR_CTRL_BUS_M1_MASK; debugCurChVal = '0'; break;
        case 1: nextAddr++; break;
        case 2: nextAddr++; expCtrl |= BR_CTRL_BUS_M1_MASK; break;
        case 3: nextAddr++; break;
        case 4: nextAddr++; break;
        case 5: nextAddr = 0xc000; expCtrl |= BR_CTRL_BUS_M1_MASK; break;        
        case 6: nextAddr++; expCtrl |= BR_CTRL_BUS_M1_MASK; debugCurBCCount = 0x1fff; break;
        case 7: nextAddr++; break;
        case 8: nextAddr++; break;
        case 9: nextAddr++; expCtrl |= BR_CTRL_BUS_M1_MASK; debugCurBCCount--; break;
        case 10: nextAddr++; expCtrl |= BR_CTRL_BUS_M1_MASK; break;
        case 11: nextAddr++; expCtrl |= BR_CTRL_BUS_M1_MASK; break;
        case 12: nextAddr++; expCtrl |= BR_CTRL_BUS_M1_MASK; break;
        case 13: nextAddr++; break;
        case 14: nextAddr++; if (debugCurBCCount != 0) nextAddr = 9; break;
        case 15: nextAddr++; expCtrl |= BR_CTRL_BUS_M1_MASK; break;
        case 16: nextAddr++; expCtrl |= BR_CTRL_BUS_M1_MASK; debugCurChVal++; break;
        case 17: nextAddr++; expCtrl |= BR_CTRL_BUS_M1_MASK; break;
        case 18: nextAddr++; break;
        case 19: nextAddr++; expCtrl |= BR_CTRL_BUS_M1_MASK; break;
        case 20: nextAddr++; break;
        case 21: nextAddr++; if (debugCurChVal <= '9') nextAddr = 2; break;
        case 22: nextAddr++; expCtrl |= BR_CTRL_BUS_M1_MASK; break;
        case 23: nextAddr++; break;
        case 24: nextAddr = 0; break;
        case 0xc000: nextAddr = 6; expCtrl = BR_CTRL_BUS_WR_MASK | BR_CTRL_BUS_MREQ_MASK | BR_CTRL_BUS_WAIT_MASK; expData = debugCurChVal; break;
    }

    if ((addr != debugCurAddr) || (expCtrl != flags) || ((expData != 0xffff) && (expData != data)))
    {
        // Debug signal
        digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
        // Only first N errors
        if (debugAccessCount < 20)
        {
            debugAccessInfo[debugAccessCount].addr = addr;
            debugAccessInfo[debugAccessCount].expectedAddr = debugCurAddr;
            debugAccessInfo[debugAccessCount].dataFromZ80 = data;
            debugAccessInfo[debugAccessCount].expectedData = expData;
            debugAccessInfo[debugAccessCount].dataToZ80 = retVal;
            debugAccessInfo[debugAccessCount].flags = flags;
            debugAccessInfo[debugAccessCount].expectedFlags = expCtrl;
            debugAccessInfo[debugAccessCount].stepCount = debugStepCount;
            debugAccessCount++;
        }
        // Since there was an error we don't know what the next address should be
        // So guess that it is the next address
        nextAddr = addr + 1;
        // Debug signal
        digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
    }
    debugCurAddr = nextAddr;
    debugStepCount++;

    // Callback to debugger
    TargetDebug* pDebug = TargetDebug::get();
    if (pDebug)
        retVal = pDebug->handleInterrupt(addr, data, flags, retVal);

    return retVal;
}

