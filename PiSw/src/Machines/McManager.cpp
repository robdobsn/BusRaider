// Bus Raider Machine Manager
// Rob Dobson 2018

#include "McManager.h"
#include <string.h>
#include "../System/lowlib.h"
#include "../System/rdutils.h"
#include "../System/piwiring.h"
#include "../TargetBus/TargetState.h"
#include "../TargetBus/BusAccess.h"
#include "../TargetBus/TargetCPUZ80.h"
#include "../TargetBus/TargetTracker.h"
#include "../Hardware/HwManager.h"
#include "../StepValidator/StepValidator.h"
#include "../Fonts/SystemFont.h"
#include "../Machines/McTRS80.h"
#include "../Machines/McRobsZ80.h"
#include "../Machines/McZXSpectrum.h"
#include "../Machines/McTerminal.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Variables
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Module name
static const char FromMcManager[] = "McManager";

// Sockets
int McManager::_busSocketId = -1;
int McManager::_commsSocketId = -1;

// Main comms socket - to wire up command handler
CommsSocketInfo McManager::_commsSocketInfo =
{
    true,
    McManager::handleRxMsg,
    NULL,
    McManager::handleTargetFile
};

// Step validator
BusSocketInfo McManager::_busSocketInfo = 
{
    true,
    handleWaitInterruptStatic,
    McManager::busActionCompleteStatic,
    false,
    false,
    BR_BUS_ACTION_NONE,
    1,
    false,
    BR_BUS_ACTION_DISPLAY,
    false
};

// Pending actions
bool McManager::_busActionPendingProgramTarget = false;
bool McManager::_busActionPendingExecAfterProgram = false;
bool McManager::_busActionCodeWrittenAtResetVector = false;
bool McManager::_busActionPendingDisplayRefresh = false;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Machines
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Machine list
McBase* McManager::_pMachines[McManager::MAX_MACHINES];
int McManager::_numMachines = 0;
char McManager::_currentMachineName[MAX_MACHINE_NAME_LEN] = "Default";
McBase* McManager::_pCurMachine = NULL;
DisplayBase* McManager::_pDisplay = NULL;

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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Statics
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint8_t McManager::_rxHostCharsBuffer[MAX_RX_HOST_CHARS+1];
int McManager::_rxHostCharsBufferLen = 0;
uint32_t McManager::_refreshCount = 0;
uint32_t McManager::_refreshLastUpdateUs = 0;
uint32_t McManager::_refreshLastCountResetUs = 0;
int McManager::_refreshRate = 0;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Init
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void McManager::init(DisplayBase* pDisplay)
{
    // Display
    _pDisplay = pDisplay;
    
    // Connect to the bus socket
    if (_busSocketId < 0)
        _busSocketId = BusAccess::busSocketAdd(_busSocketInfo);

    // Connect to the comms socket
    if (_commsSocketId < 0)
        _commsSocketId = CommandHandler::commsSocketAdd(_commsSocketInfo);

    // Add machines - McBase does the actual add
    new McTerminal();
    new McTRS80();
    new McRobsZ80();
    new McZXSpectrum();

    // Refresh init
    _refreshCount = 0;
    _refreshLastUpdateUs = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void McManager::service()
{
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
    return _pCurMachine;
}

const char* McManager::getMachineName()
{
    return _currentMachineName;
}

McDescriptorTable* McManager::getDescriptorTable()
{
    if (_pCurMachine)
        return _pCurMachine->getDescriptorTable();
    return &defaultDescriptorTable;
}

const char* McManager::getMachineJSON()
{
    // Format output
    static const int MAX_MC_JSON_LEN = MAX_MACHINES * (MAX_MACHINE_NAME_LEN + 10) + 20;
    static char mcString[MAX_MC_JSON_LEN+1];
    strlcpy(mcString, "\"machineList\":[", MAX_MC_JSON_LEN);

    // Machine list
    static const int MAX_MC_NAMES_LEN = 300;
    char mcNamesCommaSep[MAX_MC_NAMES_LEN];
    bool firstItem = true;
    for (int i = 0; i < getNumMachines(); i++)
    {
        // Get names of machines (comma separated)
        _pMachines[i]->getMachineNames(mcNamesCommaSep, MAX_MC_NAMES_LEN);
        if (!firstItem)
            strlcat(mcString,",", MAX_MC_JSON_LEN);
        firstItem = false;
        strlcat(mcString, mcNamesCommaSep, MAX_MC_JSON_LEN);
    }

    // Close array
    strlcat(mcString,"]", MAX_MC_JSON_LEN);

    // Current machine
    strlcat(mcString,",\"machineCur\":", MAX_MC_JSON_LEN);
    strlcat(mcString, "\"", MAX_MC_JSON_LEN);
    strlcat(mcString, _currentMachineName, MAX_MC_JSON_LEN);
    strlcat(mcString, "\"", MAX_MC_JSON_LEN);

    // Ret
    return mcString;
}

int McManager::getMachineClock()
{
    return BusAccess::clockCurFreqHz();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Manage Machine List
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void McManager::targetIrq(int durationUs)
{
    // Generate a maskable interrupt
    BusAccess::targetReqIRQ(_busSocketId, durationUs);
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

bool McManager::setMachineByName(const char* mcName)
{
    // Setup json
    static const int MAX_MC_JSON_LEN = 200;
    char mcJson[MAX_MC_JSON_LEN];
    ee_sprintf(mcJson, "\"name\":\"%s\"", mcName);
    return McManager::setupMachine(mcJson);
}

bool McManager::setupMachine(const char* mcJson)
{
    // Extract machine name
    static const int MAX_MC_NAME_LEN = 200;
    char mcName [MAX_MC_NAME_LEN];
    if (!jsonGetValueForKey("name", mcJson, mcName, MAX_MC_NAME_LEN))
        return false;

    // Ask each machine if it is them
    McBase* pMc = NULL;
    for (int i = 0; i < _numMachines; i++)
    {
        if (_pMachines[i])
        {
            if (_pMachines[i]->isCalled(mcName))
            {
                pMc = _pMachines[i];
                break;
            }
        }
    }
    if (!pMc)
        return false;

    // Set cur machine
    _pCurMachine = pMc;

    // Remove wait generation
    BusAccess::waitOnIO(_busSocketId, false);
    BusAccess::waitOnMemory(_busSocketId, false);

    // Disable current machine
    _pCurMachine->disable();

    // Store new name
    strlcpy(_currentMachineName, mcName, MAX_MACHINE_NAME_LEN);

    // Setup display
    pMc->setupDisplay(_pDisplay);

    // Setup the machine
    pMc->setupMachine(mcName, mcJson);

    // Enable
    _pCurMachine->enable();

    // Enable wait generation as required
    BusAccess::waitOnIO(_busSocketId, _pCurMachine->getDescriptorTable()->monitorIORQ);
    BusAccess::waitOnMemory(_busSocketId, _pCurMachine->getDescriptorTable()->monitorMREQ);

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Display refresh
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void McManager::displayRefresh()
{
    unsigned long reqUpdateUs = 1000000 / getDescriptorTable()->displayRefreshRatePerSec;
    if (isTimeout(micros(), _refreshLastUpdateUs, reqUpdateUs)) 
    {
        // Update timings
        _refreshLastUpdateUs = micros();
        _refreshCount++;

        // Determine whether display is memory mapped
        bool isMemoryMapped = getDescriptorTable()->displayMemoryMapped;
        bool useDirectBusAccess = isMemoryMapped && TargetTracker::busAccessAvailable();
        if (useDirectBusAccess)
        {
            // Asynch display refresh - start bus access request here
            BusAccess::targetReqBus(_busSocketId, BR_BUS_ACTION_DISPLAY);
            _busActionPendingDisplayRefresh = true;
        }
        else
        {
            // Synchronous display update (from local memory copy)
            if (_pCurMachine)
                _pCurMachine->displayRefreshFromMirrorHw();
        }
    }

    // Check for reset of rate
    if (isTimeout(micros(), _refreshLastCountResetUs, REFRESH_RATE_WINDOW_SIZE_MS * 1000))
    {
        _refreshRate = _refreshCount * 1000 / REFRESH_RATE_WINDOW_SIZE_MS;
        _refreshCount = 0;
        _refreshLastCountResetUs = micros();
    } 
}

int McManager::getDisplayRefreshRate()
{
    return _refreshRate;
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
    CommandHandler::sendKeyCodeToTarget(asciiCode);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Target control
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// // Handle control of target machine
void McManager::targetReset()
{
    // Reset target
    BusAccess::targetReqReset(_busSocketId);
}

bool McManager::targetIsPaused()
{
    return BusAccess::waitIsHeld();
}

bool McManager::targetBusUnderPiControl()
{
    return BusAccess::isUnderControl();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Target programming
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Handle programming of target machine
void McManager::handleTargetProgram(bool execAfterProgramming)
{
    // Check there is something to write
    if (TargetState::numMemoryBlocks() == 0) 
    {
        // Nothing new to write
        LogWrite(FromMcManager, LOG_DEBUG, "ProgramTarget - nothing to write");
    } 
    else 
    {
        // BUSRQ is used even if memory is emulated because it holds the processor while changes are made

        // Request target bus
        BusAccess::targetReqBus(_busSocketId, BR_BUS_ACTION_PROGRAMMING);
        _busActionPendingProgramTarget = true;
        _busActionPendingExecAfterProgram = execAfterProgramming;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Target file handling
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool McManager::handleTargetFile(const char* rxFileInfo, const uint8_t* pData, int dataLen)
{
    LogWrite(FromMcManager, LOG_DEBUG, "handleTargetFile");
    McBase* pMc = McManager::getMachine();
    if (pMc)
        return pMc->fileHandler(rxFileInfo, pData, dataLen);
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Received message handler
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool McManager::handleRxMsg(const char* pCmdJson, [[maybe_unused]]const uint8_t* pParams, [[maybe_unused]]int paramsLen,
                [[maybe_unused]]char* pRespJson, [[maybe_unused]]int maxRespLen)
{
    // LogWrite(FromMcManager, LOG_DEBUG, "req %s", pCmdJson);
    #define MAX_CMD_NAME_STR 200
    char cmdName[MAX_CMD_NAME_STR+1];
    if (!jsonGetValueForKey("cmdName", pCmdJson, cmdName, MAX_CMD_NAME_STR))
        return false;
    pRespJson[0] = 0;

    if (strcasecmp(cmdName, "ClearTarget") == 0)
    {
        // LogWrite(FromMcManager, LOG_VERBOSE, "ClearTarget");
        TargetState::clear();
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "ProgramTarget") == 0)
    {
        McManager::handleTargetProgram(false);
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if ((strcasecmp(cmdName, "ProgramAndReset") == 0) ||
            (strcasecmp(cmdName, "ProgramAndExec") == 0))
    {
        McManager::handleTargetProgram(true);
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "ResetTarget") == 0)
    {
        // LogWrite(FromMcManager, LOG_VERBOSE, "ResetTarget");
        McManager::targetReset();
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if ((strcasecmp(cmdName, "FileTarget") == 0) || ((strcasecmp(cmdName, "SRECTarget") == 0)))
    {
        LogWrite(FromMcManager, LOG_DEBUG, "File to Target, len %d", paramsLen);
        McBase* pMc = McManager::getMachine();
        if (pMc)
            pMc->fileHandler(pCmdJson, pParams, paramsLen);
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strncasecmp(cmdName, "SetMachine", strlen("SetMachine")) == 0)
    {
        // Get machine name
        const char* pMcName = strstr(cmdName,"=");
        if (pMcName)
        {
            // Move to first char of actual name
            pMcName++;
            setMachineByName(pMcName);
            LogWrite(FromMcManager, LOG_VERBOSE, "Set Machine to %s", pMcName);
        }
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "SetMcJson") == 0)
    {
        // Get options
        char mcJson[CommandHandler::MAX_MC_SET_JSON_LEN];
        if (!jsonGetValueForKey("mcJson", pCmdJson, mcJson, CommandHandler::MAX_MC_SET_JSON_LEN))
            return false;
        LogWrite(FromMcManager, LOG_VERBOSE, "Set Machine options to %s", mcJson);
        bool setupOk = setupMachine(mcJson);
        if (setupOk)
            strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        else
            strlcpy(pRespJson, "\"err\":\"fail\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "RxHost") == 0)
    {
        // LogWrite(FromMcManager, LOG_VERBOSE, "RxFromHost, len %d", dataLen);
        handleRxCharFromTarget(pParams, paramsLen);
        return true;
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Start target program - or reset
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void McManager::targetExec()
{
    LogWrite(FromMcManager, LOG_DEBUG, "Starting target code");
    bool performHardReset = true;
    if (TargetState::areRegistersValid())
    {
        // Check how to set registers
        if (HwManager::getOpcodeInjectEnable())
        {
            // TODO
            // Use the BusAccess module to inject instructions to set registers
            // Z80Registers regs;
            // TargetState::getTargetRegs(regs);
            // TargetDebug::get()->startSetRegisterSequence(&regs);
            // performHardReset = false;
            
                // Request reset opcode injection
            // BusAccess::targetReqOpcodeInject(_busSocketId);
        }
        else
        {
            // If the code doesn't start at 0, generate a code snippet to set registers and run
            if (!_busActionCodeWrittenAtResetVector)
            {
                uint8_t regSetCode[MAX_REGISTER_SET_CODE_LEN];
                Z80Registers regs;
                TargetState::getTargetRegs(regs);
                static const int REGISTERS_STR_MAX_LEN = 500;
                char regsStr[REGISTERS_STR_MAX_LEN];
                regs.format(regsStr, REGISTERS_STR_MAX_LEN);
                LogWrite(FromMcManager, LOG_DEBUG, "Regs: %s", regsStr);
                uint32_t codeDestAddr = getDescriptorTable()->setRegistersCodeAddr;
                int codeLen = TargetCPUZ80::getSnippetToSetRegs(codeDestAddr, regs, regSetCode, MAX_REGISTER_SET_CODE_LEN);
                if (codeLen != 0)
                {
                    // Reg setting code
                    LogWrite(FromMcManager, LOG_DEBUG,"Set regs snippet at %04x len %d", codeDestAddr, codeLen);
                    HwManager::blockWrite(codeDestAddr, regSetCode, codeLen, false, false, false);
                    
                    // Reset vector
                    uint8_t jumpCmd[3] = { 0xc3, uint8_t(codeDestAddr & 0xff), uint8_t((codeDestAddr >> 8) & 0xff) };
                    HwManager::blockWrite(Z80_PROGRAM_RESET_VECTOR, jumpCmd, 3, false, false, false);
                    LogDumpMemory(regSetCode, regSetCode + codeLen);
                }
            }
        }
    }

    // See if we need to do a hard reset
    if (performHardReset)
    {
        // Request reset target
        BusAccess::targetReqReset(_busSocketId);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Bus action callback
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void McManager::busActionCompleteStatic(BR_BUS_ACTION actionType, [[maybe_unused]] BR_BUS_ACTION_REASON reason)
{
    // We don't care what the reason for the BUSRQ is we will use it for what we need
    if (actionType == BR_BUS_ACTION_BUSRQ)
    {
        // Program target pending?
        if (_busActionPendingProgramTarget)
        {
            // Write the blocks
            _busActionCodeWrittenAtResetVector = false;
            for (int i = 0; i < TargetState::numMemoryBlocks(); i++) {
                TargetState::TargetMemoryBlock* pBlock = TargetState::getMemoryBlock(i);
                BR_RETURN_TYPE brResult = HwManager::blockWrite(pBlock->start, TargetState::getMemoryImagePtr() + pBlock->start, pBlock->len, false, false, false);
                LogWrite(FromMcManager, LOG_DEBUG,"ProgramTarget done %08x len %d result %d micros %d", pBlock->start, pBlock->len, brResult, micros());
                if (pBlock->start == Z80_PROGRAM_RESET_VECTOR)
                    _busActionCodeWrittenAtResetVector = true;
            }

            // Check for exec
            if (_busActionPendingExecAfterProgram)
                targetExec();

            // No longer pending
            _busActionPendingProgramTarget = false;
        }
        
        // Display refresh pending?
        if (_busActionPendingDisplayRefresh)
        {
            // Call the machine to handle
            if (_pCurMachine)
                _pCurMachine->busActionCompleteCallback(actionType);
            _busActionPendingDisplayRefresh = false;    
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle wait interrupt
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void McManager::handleWaitInterruptStatic(uint32_t addr, uint32_t data, 
        uint32_t flags, uint32_t& retVal)
{
    if (_pCurMachine)
        _pCurMachine->busAccessCallback(addr, data, flags, retVal);
}