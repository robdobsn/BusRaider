// Bus Raider Machine Base Class
// Rob Dobson 2018

#pragma once
#include <stdint.h>
#include "../System/DisplayBase.h"
#include "../TargetBus/TargetCPU.h"

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

class McBase
{
public:

    McBase(McDescriptorTable* pDefaultTables, int numTables);

    // Check if name is a valid one for this machine
    virtual bool isCalled(const char* mcName);

    // Check if machine can process a file type
    virtual bool canProcFileType(const char* fileType);

    // Get current machine name
    virtual const char* getMachineName();

    // Get active machine name or comma separated list of all supported machine names
    virtual void getMachineNames(char* mcNamesCommaSep, int maxLen);

    // Setup machine from JSON
    virtual bool setupMachine(const char* mcName, const char* mcJson);

    // Get descriptor table for the machine (-1 for current subType)
    virtual McDescriptorTable* getDescriptorTable();

    // Enable machine
    virtual void enable() = 0;

    // Disable machine
    virtual void disable() = 0;

    // Service
    virtual void service()
    {
    }

    // Setup display
    virtual void setupDisplay(DisplayBase* pDisplay);

    // Heartbeat
    virtual void machineHeartbeat();

    // Handle display refresh (called at a rate indicated by the machine's descriptor table)
    virtual void displayRefreshFromMirrorHw() = 0;

    // Handle reset for the machine - if false returned then the bus raider will issue a hardware reset
    virtual bool reset([[maybe_unused]] bool restoreWaitDefaults, [[maybe_unused]] bool holdInReset);

    // Handle a key press
    virtual void keyHandler(unsigned char ucModifiers, const unsigned char rawKeys[6]) = 0;

    // Handle a file
    virtual bool fileHandler(const char* pFileInfo, const uint8_t* pFileData, int fileLen) = 0;

    // Handle a request for memory or IO - or possibly something like in interrupt vector in Z80
    virtual void busAccessCallback(uint32_t addr, uint32_t data, uint32_t flags, uint32_t& retVal) = 0;

    // Bus action complete callback
    virtual void busActionCompleteCallback(BR_BUS_ACTION actionType) = 0;

    // Mirror change buffer max length
    static const int MAX_MIRROR_CHANGE_BUF_LEN = 5000;

    // Get changes made since last mirror display update
    virtual uint32_t getMirrorChanges([[maybe_unused]] uint8_t* pMirrorChangeBuf, 
                [[maybe_unused]] uint32_t mirrorChangeMaxLen, [[maybe_unused]] bool forceGetAll)
    {
        return 0;
    }

protected:
    // Descriptor tables
    McDescriptorTable _activeDescriptorTable;
    McDescriptorTable* _pDefaultDescriptorTables;
    int _defaultDescriptorTablesLen;
    int _activeSubType;

    // Display
    DisplayBase* _pDisplay;
};
