// Bus Raider Machine Base Class
// Rob Dobson 2018

#include "McBase.h"
#include "McManager.h"
#include "../System/rdutils.h"
#include "../Hardware/HwManager.h"
#include <stdlib.h>

McBase::McBase(McDescriptorTable* pDefaultTables, int numTables)
{
    // Copy descriptor table info
    _pDefaultDescriptorTables = pDefaultTables;
    _defaultDescriptorTablesLen = numTables;
    _activeDescriptorTable = pDefaultTables[0];
    _pDisplay = NULL;
    _activeSubType = 0;

    // Add to machine manager
    McManager::add(this);
}

// Get descriptor table for the machine (-1 for current subType)
McDescriptorTable* McBase::getDescriptorTable()
{
    return &_activeDescriptorTable;
}

// Handle reset for the machine - if false returned then the bus raider will issue a hardware reset
bool McBase::reset([[maybe_unused]] bool restoreWaitDefaults, [[maybe_unused]] bool holdInReset)
{
    return false;
}

// Check if name is a valid one for this machine
bool McBase::isCalled(const char* mcName)
{
    for (int i = 0; i < _defaultDescriptorTablesLen; i++)
        // Check against supported names
        if (strcasecmp(_pDefaultDescriptorTables[i].machineName, mcName) == 0)
            return true;
    return false;
}

// Check if machine can process a file type
bool McBase::canProcFileType([[maybe_unused]] const char* fileType)
{
    return false;
}

// Get current machine name
const char* McBase::getMachineName()
{
    return _activeDescriptorTable.machineName;
}

// Get comma separated list of machine names
void McBase::getMachineNames(char* mcNameStr, int maxLen)
{
    mcNameStr[0] = 0;
    bool firstItem = true;
    for (int j = 0; j < _defaultDescriptorTablesLen; j++)
    {
        if (!firstItem)
            strlcat(mcNameStr,",", maxLen);
        firstItem = false;
        strlcat(mcNameStr,"\"", maxLen);
        strlcat(mcNameStr, _pDefaultDescriptorTables[j].machineName, maxLen);
        strlcat(mcNameStr,"\"", maxLen);
    }
}

// Setup machine from JSON
bool McBase::setupMachine(const char* mcName, const char* mcJson)
{
    // Disable machine first
    LogWrite("McBase", LOG_DEBUG, "Disabling %s", mcName);
    disable();

    // Get machine sub type
    int mcSubType = -1;
    for (int i = 0; i < _defaultDescriptorTablesLen; i++)
    {
        // Check against supported names
        if (strcasecmp(_pDefaultDescriptorTables[i].machineName, mcName) == 0)
        {
            mcSubType = i;
            break;
        }
    }
    if (mcSubType < 0)
    {
        LogWrite("McBase", LOG_DEBUG, "Subtype invalid %d", mcSubType);
        return false;
    }

    // Copy descriptor
    _activeDescriptorTable = _pDefaultDescriptorTables[mcSubType];
    _activeSubType = mcSubType;

    // Disable initially
    HwManager::disableAll();

    // Setup hardware
    HwManager::setupFromJson("hw", mcJson);

    // Setup clock
    uint32_t clockFreqHz = _activeDescriptorTable.clockFrequencyHz;

    // Check if clock specified in json
    static const int MAX_CLOCK_SET_STR = 100;
    char clockSpeedStr[MAX_CLOCK_SET_STR];
    bool clockValid = jsonGetValueForKey("clockHz", mcJson, clockSpeedStr, MAX_CLOCK_SET_STR);
    if (clockValid)
    {
        uint32_t clockHz = strtol(clockSpeedStr, NULL, 10);
        if ((clockHz >= BusAccess::clockGetMinFreqHz()) && (clockHz <= BusAccess::clockGetMaxFreqHz()))
            clockFreqHz = clockHz;
    }
    if ((clockFreqHz >= BusAccess::clockGetMinFreqHz()) && (clockFreqHz <= BusAccess::clockGetMaxFreqHz()))
    {
        BusAccess::clockSetup();
        BusAccess::clockSetFreqHz(clockFreqHz);
        BusAccess::clockEnable(true);
    }
    else
    {
        BusAccess::clockEnable(false);
    }

    // Enable machine
    enable();
    LogWrite("McBase", LOG_DEBUG, "Enabling %s", mcName);
    return true;
}

// Setup display
void McBase::setupDisplay(DisplayBase* pDisplay)
{
    _pDisplay = pDisplay;
    LogWrite("McBase", LOG_DEBUG, "setupDisplay ResX %d ResY %d",
                _activeDescriptorTable.displayPixelsX, _activeDescriptorTable.displayPixelsY);
    if (!pDisplay)
        return;
    // Layout display for machine
    pDisplay->targetLayout(
        _activeDescriptorTable.displayPixelsX, _activeDescriptorTable.displayPixelsY,
        _activeDescriptorTable.displayCellX, _activeDescriptorTable.displayCellY,
        _activeDescriptorTable.pixelScaleX, _activeDescriptorTable.pixelScaleY,
        _activeDescriptorTable.pFont, 
        _activeDescriptorTable.displayForeground, _activeDescriptorTable.displayBackground);
}

// Machine heartbeat
void McBase::machineHeartbeat()
{
}
