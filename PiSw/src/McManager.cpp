// Bus Raider Machine Manager
// Rob Dobson 2018

#include "McManager.h"
#include "wgfx.h"

McBase* McManager::_pMachines[McManager::MAX_MACHINES];
int McManager::_numMachines = 0;
int McManager::_curMachineIdx = -1;
McDescriptorTable McManager::defaultDescriptorTable = {
    // Required display refresh rate
    .displayRefreshRatePerSec = 30,
    .displayPixelsX = 8 * 80,
    .displayPixelsY = 16 * 25,
    .displayCellX = 8,
    .displayCellY = 16,
    .pFont = &__systemFont,
    .displayForeground = WGFX_WHITE,
    .displayBackground = WGFX_BLACK
};

void McManager::add(McBase* pMachine)
{
    if (_numMachines >= MAX_MACHINES)
        return;
    _pMachines[_numMachines++] = pMachine;
}
