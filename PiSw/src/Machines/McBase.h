// Bus Raider Machine Base Class
// Rob Dobson 2018

#pragma once
#include <stdint.h>

static const int MC_WINDOW_NUMBER = 0;

struct WgfxFont;

class McDescriptorTable
{
public:
    enum PROCESSOR_TYPE { PROCESSOR_Z80 };

public:
    // Name
    const char* machineName;
    // Processor type
    PROCESSOR_TYPE processorType;
    // Display
    int displayRefreshRatePerSec;
    int displayPixelsX;
    int displayPixelsY;
    int displayCellX;
    int displayCellY;
    int pixelScaleX;
    int pixelScaleY;
    WgfxFont* pFont;
    int displayForeground;
    int displayBackground;
    // Clock
    uint32_t clockFrequencyHz;
    // Bus monitor modes
    bool monitorIORQ;
    bool monitorMREQ;
};

class McBase
{
  public:

    McBase();

    // Enable machine
    virtual void enable() = 0;

    // Disable machine
    virtual void disable() = 0;

    // Get descriptor table for the machine
    virtual McDescriptorTable* getDescriptorTable(int subType) = 0;

    // Handle display refresh (called at a rate indicated by the machine's descriptor table)
    virtual void displayRefresh() = 0;

    // Handle reset for the machine - if false returned then the bus raider will issue a hardware reset
    virtual bool reset()
    {
        return false;
    }

    // Handle a key press
    virtual void keyHandler(unsigned char ucModifiers, const unsigned char rawKeys[6]) = 0;

    // Handle a file
    virtual void fileHandler(const char* pFileInfo, const uint8_t* pFileData, int fileLen) = 0;

    // Handle debugger command
    virtual bool debuggerCommand([[maybe_unused]] const char* pCommand, [[maybe_unused]] char* pResponse, [[maybe_unused]] int maxResponseLen);
};
