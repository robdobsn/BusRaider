// Bus Raider Machine Base Class
// Rob Dobson 2018

#pragma once
#include "wgfxfont.h"
#include <stdint.h>

static const int MC_WINDOW_NUMBER = 0;

class McDescriptorTable
{
  public:
    // Name
    const char* machineName;
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

    // Handle single-step for the machine - if false returned then the bus raider will issue a bus step
    virtual bool step()
    {
        return false;
    }

    // Handle a key press
    virtual void keyHandler(unsigned char ucModifiers, const unsigned char rawKeys[6]) = 0;

    // Handle a file
    virtual void fileHandler(const char* pFileInfo, const uint8_t* pFileData, int fileLen) = 0;

};
