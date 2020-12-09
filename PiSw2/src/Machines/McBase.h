// Bus Raider Machine Base Class
// Rob Dobson 2018

#pragma once
#include <stdint.h>
#include "DisplayBase.h"
#include "TargetCPU.h"
#include "McVariantTable.h"
#include <circle/util.h>

static const int MC_WINDOW_NUMBER = 0;

struct WgfxFont;
class BusControl;
class McManager;
class TargetProgrammer;

class McBase
{
public:

    McBase(McManager& mcManager, BusControl& busControl, const McVariantTable* pVariantTables, uint32_t numVariants);

    // Check if name is a valid one for this machine
    virtual bool isCalled(const char* mcName, uint32_t& machineVariant);

    // Check if machine can process a file type
    virtual bool canProcFileType(const char* fileType);

    // Get current machine name
    virtual const char* getMachineName();

    // Get active machine name or comma separated list of all supported machine names
    virtual void getMachineNames(char* mcNamesCommaSep, uint32_t maxLen);

    // Setup machine from JSON
    virtual bool setupMachine(const char* mcName, const char* mcJson);

    // Get machine descriptor table
    virtual const McVariantTable& getDescriptorTable();

    // Enable machine
    virtual void enableMachine()
    {
    };

    // Disable machine
    virtual void disableMachine()
    {
    };

    // Service
    virtual void service()
    {
    }

    // Setup display
    virtual void setupDisplay(DisplayBase* pDisplay);

    // Heartbeat
    virtual void machineHeartbeat();

    // Handle display refresh (called at a rate indicated by the machine's descriptor table)
    virtual void refreshDisplay() = 0;

    // Handle reset for the machine - if false returned then the bus raider will issue a hardware reset
    virtual bool reset( bool restoreWaitDefaults,  bool holdInReset);

    // Handle a key press
    virtual void keyHandler(unsigned char ucModifiers, const unsigned char rawKeys[6]) = 0;

    // Handle a file
    virtual bool fileHandler(const char* pFileInfo, const uint8_t* pFileData, 
                        int fileLen, TargetProgrammer& targetProgrammer) = 0;

    // Handle a request for memory or IO - or possibly something like in interrupt vector in Z80
    virtual void busAccessCallback(uint32_t addr, uint32_t data, uint32_t flags, uint32_t& retVal) = 0;

    // Bus action active callback
    virtual void busActionActiveCallback(BR_BUS_ACTION actionType, BR_BUS_ACTION_REASON reason, 
                        BR_RETURN_TYPE rslt) = 0;

    // Mirror change buffer max length
    static const int MAX_MIRROR_CHANGE_BUF_LEN = 5000;

    // Get changes made since last mirror display update
    virtual uint32_t getMirrorChanges( uint8_t* pMirrorChangeBuf, 
                 uint32_t mirrorChangeMaxLen,  bool forceGetAll)
    {
        return 0;
    }

    // Check state of monitoring MREQ and IORQ
    virtual bool isMonitorIORQEnabled()
    {
        return _machineDescriptor.monitorIORQ;
    }
    virtual bool isMonitorMREQEnabled()
    {
        return _machineDescriptor.monitorMREQ;
    }

    // Set state of monitoring MREQ / IORQ
    virtual void setMonitorIORQEnabled(bool en)
    {
        _machineDescriptor.monitorIORQ = en;
    }
    virtual void setMonitorMREQEnabled(bool en)
    {
        _machineDescriptor.monitorMREQ = en;
    }

    // Display refresh rate
    virtual uint32_t getDisplayRefreshRatePerSec()
    {
        return _machineDescriptor.displayRefreshRatePerSec;
    }

    // Get address for setRegistersCode
    virtual uint32_t getSetRegistersCodeAddr()
    {
        return _machineDescriptor.setRegistersCodeAddr;
    }

    // Check if display is memory mapped
    virtual bool isDisplayMemoryMapped()
    {
        return _machineDescriptor.displayMemoryMapped;
    }

    // Get display
    virtual DisplayBase* getDisplay()
    {
        return _pDisplay;
    }

    virtual void informClockFreqHz(uint32_t clockFreqHz)
    {
    }

protected:
    // Machine manager
    McManager& _mcManager;

    // Bus control
    BusControl& _busControl;

private:
    // Machine descriptor
    McVariantTable _machineDescriptor;

    // Machine variant tables
    static const uint32_t MAX_VARIANTS_FOR_MACHINE = 10;
    const McVariantTable* _pVariantTables[MAX_VARIANTS_FOR_MACHINE];
    uint32_t _variantTableCount;
    uint32_t _activeVariantIdx;

    // Default variant table
    static McVariantTable _defaultVariantTable;

    // Display
    DisplayBase* _pDisplay;
};
