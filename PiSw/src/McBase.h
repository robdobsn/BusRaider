// Bus Raider Machine Base Class
// Rob Dobson 2018

#pragma once
#include "globaldefs.h"
#include "wgfxfont.h"

class McDescriptorTable
{
  public:
    // Display
    int displayRefreshRatePerSec;
    int displayPixelsX;
    int displayPixelsY;
    int displayCellX;
    int displayCellY;
    WgfxFont* pFont;
    int displayForeground;
    int displayBackground;
};

class McBase
{
  public:

    McBase();

    // Get descriptor table for the machine
    virtual McDescriptorTable* getDescriptorTable(int subType) = 0;

    // Handle display refresh (called at a rate indicated by the machine's descriptor table)
    virtual void displayRefresh() = 0;

    // Handle a key press
    virtual void keyHandler(unsigned char ucModifiers, const unsigned char rawKeys[6]) = 0;

    // Handle a file
    virtual void fileHander(const char* pFileInfo, const uint8_t* pFileData, int fileLen) = 0;

    // Handle a request for memory or IO - or possibly something like in interrupt vector in Z80
    virtual void memoryRequest(uint32_t addr, uint32_t data, uint32_t flags) = 0;

};
