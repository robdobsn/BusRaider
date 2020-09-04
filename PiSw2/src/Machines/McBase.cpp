// Bus Raider Machine Base Class
// Rob Dobson 2018

#include "McBase.h"
#include "McManager.h"
#include "rdutils.h"
#include "HwManager.h"
#include "SystemFont.h"
#include <stdlib.h>

static const char MODULE_PREFIX[] = "McBase";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Default variant table
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

McVariantTable McBase::_defaultVariantTable = {
    // Machine name
    "Default",
    // Processor
    McVariantTable::PROCESSOR_Z80,
    // Required display refresh rate
    .displayRefreshRatePerSec = 30,
    .displayPixelsX = 8 * 80,
    .displayPixelsY = 16 * 25,
    .displayCellX = 8,
    .displayCellY = 16,
    .pixelScaleX = 2,
    .pixelScaleY = 1,
    .pFont = &__systemFont,
    .displayForeground = DISPLAY_FX_WHITE,
    .displayBackground = DISPLAY_FX_BLACK,
    .displayMemoryMapped = false,
    // Clock
    .clockFrequencyHz = 1000000,
    // Interrupt rate per second
    .irqRate = 0,
    // Bus monitor
    .monitorIORQ = false,
    .monitorMREQ = false,
    .setRegistersCodeAddr = 0
};

McBase::McBase(McManager& mcManager, const McVariantTable* pVariantTables, uint32_t numVariants) :
        _mcManager(mcManager)
{
    // Clear machine variant table info
    for (uint32_t i = 0; i < numVariants && i < MAX_VARIANTS_FOR_MACHINE; i++)
        _pVariantTables[i] = &pVariantTables[i];
    _variantTableCount = numVariants;
    _pDisplay = NULL;
    _activeVariantIdx = 0;
    if (numVariants > 0)
        _machineDescriptor = *pVariantTables;
    else
        _machineDescriptor = _defaultVariantTable;

    // Add to machine manager
    _mcManager.add(this);
}

// Get machine descriptor table for the machine
const McVariantTable& McBase::getDescriptorTable()
{
    return _machineDescriptor;
}

// Get current machine name
const char* McBase::getMachineName()
{
    return _machineDescriptor.machineName;
}

// Check if name is a valid one for this machine
bool McBase::isCalled(const char* mcName, uint32_t& variantIdx)
{
    for (uint32_t i = 0; i < _variantTableCount; i++)
    {
        // Check against supported names
        if (strcasecmp(_pVariantTables[i]->machineName, mcName) == 0)
        {
            variantIdx = i;
            return true;
        }
    }
    return false;
}

// Get comma separated list of machine names
void McBase::getMachineNames(char* mcNameStr, uint32_t maxLen)
{
    mcNameStr[0] = 0;
    bool firstItem = true;
    for (uint32_t j = 0; j < _variantTableCount; j++)
    {
        if (!firstItem)
            strlcat(mcNameStr,",", maxLen);
        firstItem = false;
        strlcat(mcNameStr,"\"", maxLen);
        strlcat(mcNameStr, _pVariantTables[j]->machineName, maxLen);
        strlcat(mcNameStr,"\"", maxLen);
    }
}

// Setup machine from JSON
bool McBase::setupMachine(const char* mcName, const char* mcJson)
{
    // Check if name is valid
    uint32_t variantIdx = 0;
    bool isValid = isCalled(mcName, variantIdx);
    if (!isValid)
    {
        LogWrite(MODULE_PREFIX, LOG_WARNING, "setupMachine name %s not supported", mcName);
        return false;
    }

    // Store new variant idx and table
    _activeVariantIdx = variantIdx;
    _machineDescriptor = *_pVariantTables[variantIdx];

    // Disable machine first
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "Disabling %s", getMachineName());
    disableMachine();
    
    // Disable hardware initially
    getHwManager().disableAll();

    // Setup hardware
    getHwManager().setupFromJson("hw", mcJson);

    // Setup clock
    uint32_t clockFreqHz = _machineDescriptor.clockFrequencyHz;

    // Check if clock specified in json
    static const int MAX_CLOCK_SET_STR = 100;
    char clockSpeedStr[MAX_CLOCK_SET_STR];
    BusAccess& busAccess = _mcManager.getBusAccess();
    bool clockValid = jsonGetValueForKey("clockHz", mcJson, clockSpeedStr, MAX_CLOCK_SET_STR);
    if (clockValid)
    {
        uint32_t clockHz = strtoul(clockSpeedStr, NULL, 10);
        if ((clockHz >= busAccess.clockGetMinFreqHz()) && 
                        (clockHz <= busAccess.clockGetMaxFreqHz()))
            clockFreqHz = clockHz;
    }
    if ((clockFreqHz >= busAccess.clockGetMinFreqHz()) && 
                        (clockFreqHz <= busAccess.clockGetMaxFreqHz()))
    {
        busAccess.clockSetup();
        busAccess.clockSetFreqHz(clockFreqHz);
        busAccess.clockEnable(true);
    }
    else
    {
        busAccess.clockEnable(false);
    }

    // Enable machine
    enableMachine();
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "Enabling %s", getMachineName());
    return true;
}

// Setup display
void McBase::setupDisplay(DisplayBase* pDisplay)
{
    _pDisplay = pDisplay;
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "setupDisplay ResX %d ResY %d",
                _machineDescriptor.displayPixelsX, _machineDescriptor.displayPixelsY);
    if (!pDisplay)
        return;
    // Layout display for machine
    pDisplay->targetLayout(
        _machineDescriptor.displayPixelsX, _machineDescriptor.displayPixelsY,
        _machineDescriptor.displayCellX, _machineDescriptor.displayCellY,
        _machineDescriptor.pixelScaleX, _machineDescriptor.pixelScaleY,
        _machineDescriptor.pFont, 
        _machineDescriptor.displayForeground, _machineDescriptor.displayBackground);
}

// Machine heartbeat
void McBase::machineHeartbeat()
{
}

// Handle reset for the machine - if false returned then the bus raider will issue a hardware reset
bool McBase::reset( bool restoreWaitDefaults,  bool holdInReset)
{
    return false;
}

// Check if machine can process a file type
bool McBase::canProcFileType( const char* fileType)
{
    return false;
}

// Get HwManager
HwManager& McBase::getHwManager()
{
    return _mcManager.getHwManager();
}

// Get Target Programmer
TargetProgrammer& McBase::getTargetProgrammer()
{
    return _mcManager.getTargetProgrammer();
}

