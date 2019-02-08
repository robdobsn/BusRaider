// Bus Raider Machine Manager
// Rob Dobson 2018

#include "McManager.h"
#include "../System/wgfx.h"
#include <string.h>

McBase* McManager::_pMachines[McManager::MAX_MACHINES];
int McManager::_numMachines = 0;
int McManager::_curMachineIdx = -1;
CommandHandler* McManager::_pCommandHandler = NULL;

McDescriptorTable McManager::defaultDescriptorTable = {
    // Machine name
    "Default",
    // Processor
    McDescriptorTable::PROCESSOR_Z80,
    // Required display refresh rate
    .displayRefreshRatePerSec = 30,
    .displayPixelsX = 8 * 80,
    .displayPixelsY = 16 * 25,
    .displayCellX = 8,
    .displayCellY = 16,
    .pixelScaleX = 2,
    .pixelScaleY = 1,
    .pFont = &__systemFont,
    .displayForeground = WGFX_WHITE,
    .displayBackground = WGFX_BLACK,
    // Clock
    .clockFrequencyHz = 1000000,
    // Bus monitor
    .monitorIORQ = false,
    .monitorMREQ = false
};

uint8_t McManager::_rxHostCharsBuffer[MAX_RX_HOST_CHARS+1];
int McManager::_rxHostCharsBufferLen = 0;

TargetClockGenerator McManager::_clockGen;

void McManager::add(McBase* pMachine)
{
    if (_numMachines >= MAX_MACHINES)
        return;
    _pMachines[_numMachines++] = pMachine;
}

bool McManager::setMachineIdx(int mcIdx)
{
    // Check valid
    if (mcIdx < 0 || mcIdx >= _numMachines)
        return false;

    // Check if no change
    if (_curMachineIdx == mcIdx)
        return false;
    
    // Disable current machine
    if (getMachine())
        getMachine()->disable();

    // Set the new machine
    _curMachineIdx = mcIdx;

    // Enable machine
    if (getMachine())
    {
        // Machine
        McBase* pMachine = getMachine();

        // Setup clock
        uint32_t clockFreqHz = pMachine->getDescriptorTable(0)->clockFrequencyHz;
        if (clockFreqHz != 0)
        {
            _clockGen.setOutputPin();
            _clockGen.setFrequency(clockFreqHz);
            _clockGen.enable(true);
        }
        else
        {
            _clockGen.enable(false);
        }

        // Start
        pMachine->enable();

        // Started machine
        LogWrite("McManager", LOG_DEBUG, "Started machine %s\n", 
                    pMachine->getDescriptorTable(0)->machineName);

    }
    else
    {
        LogWrite("McManager", LOG_DEBUG, "Failed to start machine idx %d\n", mcIdx);
    }
    return true;
}

bool McManager::setMachineByName(const char* mcName)
{
    // Find machine
    for (int i = 0; i < _numMachines; i++)
    {
        if (strncasecmp(mcName, _pMachines[i]->getDescriptorTable(0)->machineName, MAX_MACHINE_NAME_LEN) == 0)
        {
            return setMachineIdx(i);
        }
    }
    return false;
}
