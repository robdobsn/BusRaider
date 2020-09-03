#pragma once

#include "stdint.h"

class McVariantTable
{
public:
    enum PROCESSOR_TYPE { PROCESSOR_Z80 };

    // Name
    static const int MAX_MACHINE_NAME_LEN = 100;
    char machineName[MAX_MACHINE_NAME_LEN];
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
    bool displayMemoryMapped;
    // Clock
    uint32_t clockFrequencyHz;
    // Interrupt rate per second
    uint32_t irqRate;
    // Bus monitor modes
    bool monitorIORQ;
    bool monitorMREQ;
    uint32_t setRegistersCodeAddr;
};
