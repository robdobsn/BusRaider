// Bus Raider Machine Manager
// Rob Dobson 2018

#include "McManager.h"
#include "../System/wgfx.h"
#include <string.h>
#include "../TargetBus/TargetState.h"
#include "../TargetBus/BusAccess.h"

// Module name
static const char FromMcManager[] = "McManager";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Machines
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Machine list
McBase* McManager::_pMachines[McManager::MAX_MACHINES];
int McManager::_numMachines = 0;
int McManager::_curMachineIdx = -1;
CommandHandler* McManager::_pCommandHandler = NULL;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Default descriptor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
    .monitorMREQ = false,
    .emulatedRAMStart = 0,
    .emulatedRAMLen = 0
};

uint8_t McManager::_rxHostCharsBuffer[MAX_RX_HOST_CHARS+1];
int McManager::_rxHostCharsBufferLen = 0;

TargetClockGenerator McManager::_clockGen;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Init
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void McManager::init(CommandHandler* pCommandHandler)
{
    // Add a callback for received characters from target
    pCommandHandler->setRxFromTargetCallback(handleRxCharFromTarget);
    _pCommandHandler = pCommandHandler;
    
    // Let the target debugger know how to communicate
    TargetDebug::get()->setSendDebugMessageCallback(sendDebugMessage);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Machine access
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int McManager::getNumMachines()
{
    return _numMachines;
}

McBase* McManager::getMachine()
{
    if ((_curMachineIdx < 0) || (_curMachineIdx >= _numMachines))
    {
        if (_numMachines > 0)
            return _pMachines[0];
        return NULL;
    }
    return _pMachines[_curMachineIdx];
}

McDescriptorTable* McManager::getDescriptorTable(int subType)
{
    McBase* pMc = getMachine();
    if (pMc)
        return pMc->getDescriptorTable(subType);
    return &defaultDescriptorTable;
}

const char* McManager::getMachineJSON()
{
    static const int MAX_MC_JSON_LEN = MAX_MACHINES * (MAX_MACHINE_NAME_LEN + 10) + 20;
    static char mcString[MAX_MC_JSON_LEN+1];

    // Machine list
    strlcpy(mcString, "\"machineList\":[", MAX_MC_JSON_LEN);
    for (int i = 0; i < getNumMachines(); i++)
    {
        if (i != 0)
            strlcpy(mcString+strlen(mcString),",", MAX_MC_JSON_LEN);
        strlcpy(mcString+strlen(mcString),"\"", MAX_MC_JSON_LEN);
        strlcpy(mcString+strlen(mcString), _pMachines[i]->getDescriptorTable(0)->machineName, MAX_MC_JSON_LEN);
        strlcpy(mcString+strlen(mcString),"\"", MAX_MC_JSON_LEN);
    }
    strlcpy(mcString+strlen(mcString),"]", MAX_MC_JSON_LEN);

    // Current machine
    strlcpy(mcString+strlen(mcString),",\"machineCur\":", MAX_MC_JSON_LEN);
    strlcpy(mcString+strlen(mcString), "\"", MAX_MC_JSON_LEN);
    if ((_curMachineIdx >= 0) && (_curMachineIdx < _numMachines))
        strlcpy(mcString+strlen(mcString), getDescriptorTable(_curMachineIdx)->machineName, MAX_MC_JSON_LEN);
    strlcpy(mcString+strlen(mcString), "\"", MAX_MC_JSON_LEN);

    // Ret
    return mcString;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Manage Machine List
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
        LogWrite(FromMcManager, LOG_DEBUG, "Started machine %s\n", 
                    pMachine->getDescriptorTable(0)->machineName);

    }
    else
    {
        LogWrite(FromMcManager, LOG_DEBUG, "Failed to start machine idx %d\n", mcIdx);
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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Communication with machine
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void McManager::handleRxCharFromTarget(const uint8_t* pRxChars, int rxLen)
{
    // Check for overflow
    if (rxLen + _rxHostCharsBufferLen >= MAX_RX_HOST_CHARS)
    {
        // Discard old data as probably not being used
        _rxHostCharsBufferLen = 0;
    }

    // Check there is space
    if (rxLen + _rxHostCharsBufferLen >= MAX_RX_HOST_CHARS)
        return;

    // Add to buffer
    memcpy(_rxHostCharsBuffer+_rxHostCharsBufferLen, pRxChars, rxLen);
    _rxHostCharsBufferLen += rxLen;
    *(_rxHostCharsBuffer+_rxHostCharsBufferLen) = 0;
}

int McManager::getNumCharsReceivedFromHost()
{
    return _rxHostCharsBufferLen;
}

int McManager::getCharsReceivedFromHost(uint8_t* pBuf, int bufMaxLen)
{
    if ((!pBuf) || (bufMaxLen < _rxHostCharsBufferLen))
        return 0;
    memcpy(pBuf, _rxHostCharsBuffer, _rxHostCharsBufferLen);
    int retVal = _rxHostCharsBufferLen;
    _rxHostCharsBufferLen = 0;
    return retVal;
}

void McManager::sendKeyCodeToTarget(int asciiCode)
{
    if (_pCommandHandler)
        _pCommandHandler->sendKeyCodeToTarget(asciiCode);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Debug messaging
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void McManager::sendDebugMessage(const char* pStr)
{
    LogWrite(FromMcManager, LOG_DEBUG, "debugMsg %s cmdH %d\n", pStr, _pCommandHandler); 
    if (_pCommandHandler)
        _pCommandHandler->sendDebugMessage(pStr);
}

bool McManager::debuggerCommand(const char* pCommand, char* pResponse, int maxResponseLen)
{
    McBase* pMc = getMachine();
    // LogWrite(FromMcManager, LOG_DEBUG, "debuggerCommand %s %d %d\n", pCommand, 
    //         maxResponseLen, pMc);
    if (pMc)
        return pMc->debuggerCommand(pCommand, pResponse, maxResponseLen);
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Memory access
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool McManager::blockWrite(uint32_t addr, const uint8_t* pBuf, uint32_t len, bool busRqAndRelease, bool iorq)
{
    // Machine info
    McBase* pMachine = getMachine();
    if (!pMachine)
        return false;
    uint32_t emuRAMStart = pMachine->getDescriptorTable(0)->emulatedRAMStart;
    uint32_t emuRAMLen = pMachine->getDescriptorTable(0)->emulatedRAMLen;
    // Block cannot cross boundary between emulated and real RAM
    if (!iorq && (emuRAMStart <= addr) && (emuRAMStart+emuRAMLen >= addr + len))
    {
        TargetDebug* pDebug = TargetDebug::get();
        if (pDebug)
            return pDebug->blockWrite(addr, pBuf, len);
    }
    else
    {
        return BusAccess::blockWrite(addr, pBuf, len, busRqAndRelease, iorq) == BR_OK;
    }
    return false;
}

bool McManager::blockRead(uint32_t addr, uint8_t* pBuf, uint32_t len, bool busRqAndRelease, bool iorq)
{
    // Machine info
    McBase* pMachine = getMachine();
    if (!pMachine)
        return false;
    uint32_t emuRAMStart = pMachine->getDescriptorTable(0)->emulatedRAMStart;
    uint32_t emuRAMLen = pMachine->getDescriptorTable(0)->emulatedRAMLen;
    // Block cannot cross boundary between emulated and real RAM
    if (!iorq && (emuRAMStart <= addr) && (emuRAMStart+emuRAMLen >= addr + len))
    {
        TargetDebug* pDebug = TargetDebug::get();
        if (pDebug)
            return pDebug->blockRead(addr, pBuf, len);
    }
    else
    {
        return BusAccess::blockRead(addr, pBuf, len, busRqAndRelease, iorq) == BR_OK;
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Target programming
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Handle programming of target machine
void McManager::targetReset()
{
    bool resetDone = false;
    McBase* pMc = McManager::getMachine();
    if (pMc)
        resetDone = pMc->reset();
    if (!resetDone)
        BusAccess::targetReset();
}

void McManager::targetClearAllIO()
{
    BusAccess::clearAllIO();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Target programming
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Handle programming of target machine
void McManager::handleTargetProgram(const char* cmdName)
{
    if (TargetState::numMemoryBlocks() == 0) 
    {
        // Nothing new to write
        LogWrite(FromMcManager, LOG_DEBUG, "ProgramTarget - nothing to write");
    } 
    else 
    {
        // Handle programming in one BUSRQ/BUSACK pass
        if (BusAccess::controlRequestAndTake() != BR_OK)
        {
            LogWrite(FromMcManager, LOG_DEBUG, "ProgramTarget - failed to capture bus");   
            return;
        }
        // Write the blocks
        for (int i = 0; i < TargetState::numMemoryBlocks(); i++) {
            TargetState::TargetMemoryBlock* pBlock = TargetState::getMemoryBlock(i);
            LogWrite(FromMcManager, LOG_DEBUG,"ProgramTarget start %08x len %d", pBlock->start, pBlock->len);
            McManager::blockWrite(pBlock->start, TargetState::getMemoryImagePtr() + pBlock->start, pBlock->len, false, false);
        }
        // Written
        LogWrite(FromMcManager, LOG_DEBUG, "ProgramTarget - written %d blocks", TargetState::numMemoryBlocks());

        // Check for reset too
        if (strcasecmp(cmdName, "ProgramAndReset") == 0)
        {
            LogWrite(FromMcManager, LOG_DEBUG, "Resetting target");
            if (TargetState::areRegistersValid())
            {
                Z80Registers regs;
                TargetState::getTargetRegsAndInvalidate(regs);
                TargetDebug* pDebug = TargetDebug::get();
                if (pDebug)
                    pDebug->startSetRegisterSequence(&regs);
            }
            BusAccess::controlRelease(true);
        }
        else
        {
            BusAccess::controlRelease(false);
        }
    }
    // Clear target state
    TargetState::clear();
}

