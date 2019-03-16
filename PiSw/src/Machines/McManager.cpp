// Bus Raider Machine Manager
// Rob Dobson 2018

#include "McManager.h"
#include "../System/wgfx.h"
#include "../System/Timers.h"
#include <string.h>
#include "../TargetBus/TargetState.h"
#include "../TargetBus/BusAccess.h"
#include "../TargetBus/RAMEmulator.h"
#include "../System/rdutils.h"
#include "../System/piwiring.h"
#include "../TargetBus/TargetCPUZ80.h"
#include "../Hardware/HwManager.h"
#include "../StepValidator/StepValidator.h"

// Module name
static const char FromMcManager[] = "McManager";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Machines
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Machine list
McBase* McManager::_pMachines[McManager::MAX_MACHINES];
int McManager::_numMachines = 0;
int McManager::_curMachineIdx = -1;
int McManager::_curMachineSubType = 0;
McBase* McManager::_pCurMachine = NULL;
CommandHandler* McManager::_pCommandHandler = NULL;
Display* McManager::_pDisplay = NULL;

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
    // Interrupt rate per second
    .irqRate = 0,
    // Bus monitor
    .monitorIORQ = false,
    .monitorMREQ = false,
    .emulatedRAM = false,
    .setRegistersByInjection = false,
    .setRegistersCodeAddr = 0
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Statics
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

StepTester McManager::_stepTester;
StepValidator* McManager::_pStepValidator = NULL;
uint8_t McManager::_rxHostCharsBuffer[MAX_RX_HOST_CHARS+1];
int McManager::_rxHostCharsBufferLen = 0;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Init
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void McManager::init(CommandHandler* pCommandHandler, Display* pDisplay)
{
    // Command handler & display
    _pCommandHandler = pCommandHandler;
    _pDisplay = pDisplay;
    
    // Let the target debugger know how to communicate
    TargetDebug::get()->setSendRemoteDebugProtocolMsgCallback(sendRemoteDebugProtocolMsg);

    // Set callback for bus access
    BusAccess::accessCallbackAdd(busAccessCallback);

    // Create step validator
    if (!_pStepValidator)
        _pStepValidator = new StepValidator();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void McManager::service()
{
    _stepTester.service();
    if (_pStepValidator)
        _pStepValidator->service();
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

McDescriptorTable* McManager::getDescriptorTable()
{
    if (_pCurMachine)
        return _pCurMachine->getDescriptorTable();
    return &defaultDescriptorTable;
}

const char* McManager::getMachineJSON()
{
    static const int MAX_MC_JSON_LEN = MAX_MACHINES * (MAX_MACHINE_NAME_LEN + 10) + 20;
    static char mcString[MAX_MC_JSON_LEN+1];

    // Machine list
    strlcpy(mcString, "\"machineList\":[", MAX_MC_JSON_LEN);
    bool firstItem = true;
    for (int i = 0; i < getNumMachines(); i++)
    {
        // Iterate sub types
        for (int j = 0; j < _pMachines[i]->getDescriptorTableCount(); j++)
        {
            if (!firstItem)
                strlcpy(mcString+strlen(mcString),",", MAX_MC_JSON_LEN);
            firstItem = false;
            strlcpy(mcString+strlen(mcString),"\"", MAX_MC_JSON_LEN);
            strlcpy(mcString+strlen(mcString), _pMachines[i]->getDescriptorTable(j)->machineName, MAX_MC_JSON_LEN);
            strlcpy(mcString+strlen(mcString),"\"", MAX_MC_JSON_LEN);
        }
    }
    strlcpy(mcString+strlen(mcString),"]", MAX_MC_JSON_LEN);

    // Current machine
    strlcpy(mcString+strlen(mcString),",\"machineCur\":", MAX_MC_JSON_LEN);
    strlcpy(mcString+strlen(mcString), "\"", MAX_MC_JSON_LEN);
    strlcpy(mcString+strlen(mcString), getDescriptorTable()->machineName, MAX_MC_JSON_LEN);
    strlcpy(mcString+strlen(mcString), "\"", MAX_MC_JSON_LEN);

    // Ret
    return mcString;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Manage Machine List
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void McManager::targetIrq(int durationUs)
{
    // Generate a maskable interrupt
    BusAccess::targetIRQ(durationUs);
}

void McManager::targetIrqFromTimer([[maybe_unused]] void* pParam)
{   
    targetIrq();
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

bool McManager::setMachineIdx(int mcIdx, int mcSubType, bool forceUpdate)
{
    // Check valid
    if (mcIdx < 0 || mcIdx >= _numMachines)
        return false;

    // Check if no change
    if ((_curMachineIdx == mcIdx) && (_curMachineSubType != mcSubType) && !forceUpdate)
        return false;
    
    // Disable current machine
    BusAccess::waitSetup(false, false);
    if (_pCurMachine)
        _pCurMachine->disable();

    // Set the new machine
    _curMachineIdx = mcIdx;
    _curMachineSubType = mcSubType;
    _pCurMachine = _pMachines[mcIdx];

    // Layout display for machine
    McDescriptorTable* pMcDescr = _pCurMachine->getDescriptorTable(_curMachineSubType);
    int windowBorderWidth = 5;
    if (_pDisplay)
        _pDisplay->targetLayout(-1, 0, 
            pMcDescr->displayPixelsX, pMcDescr->displayPixelsY,
            pMcDescr->displayCellX, pMcDescr->displayCellY,
            pMcDescr->pixelScaleX, pMcDescr->pixelScaleY,
            pMcDescr->pFont, pMcDescr->displayForeground, pMcDescr->displayBackground,
            windowBorderWidth, 8);

    // Enable machine
    if (getMachine())
    {
        // Setup clock
        uint32_t clockFreqHz = pMcDescr->clockFrequencyHz;
        if (clockFreqHz != 0)
        {
            BusAccess::clockSetup();
            BusAccess::clockSetFreqHz(clockFreqHz);
            BusAccess::clockEnable(true);
        }
        else
        {
            BusAccess::clockEnable(false);
        }

        // Setup RAMEmulator and paging
        RAMEmulator::setup(_pCurMachine);
        RAMEmulator::activateEmulation(pMcDescr->emulatedRAM);
        HwManager::pageOutForEmulation(pMcDescr->emulatedRAM);

        // Set target machine in debug
        TargetDebug::get()->setup(_pCurMachine);

        // Debug
        LogWrite(FromMcManager, LOG_DEBUG, "Enabling %s EmuRAM %s RegByInject %s IntIORQ %s IntMREQ %s (%04x)",
                    pMcDescr->emulatedRAM ? "Y" : "N",
                    pMcDescr->setRegistersByInjection ?  "Y" : "N",
                    pMcDescr->monitorIORQ ?  "Y" : "N",
                    pMcDescr->monitorMREQ || pMcDescr->emulatedRAM ?  "Y" : "N",
                    pMcDescr->setRegistersCodeAddr);

        // Bus raider enable wait states
        BusAccess::waitSetup(pMcDescr->monitorIORQ, pMcDescr->monitorMREQ || pMcDescr->emulatedRAM);

        // Start
        _pCurMachine->enable(mcSubType);

        // Heartbeat timer
        if (_pCurMachine->getDescriptorTable(mcSubType)->irqRate != 0)
            Timers::set(1000000 / pMcDescr->irqRate, McManager::targetIrqFromTimer, NULL);

        // Started machine
        LogWrite(FromMcManager, LOG_VERBOSE, "Started machine %s", 
                    pMcDescr->machineName);

    }
    else
    {
        LogWrite(FromMcManager, LOG_DEBUG, "Failed to start machine idx %d", mcIdx);
    }
    return true;
}

bool McManager::setMachineByName(const char* mcName)
{
    // Find machine
    for (int i = 0; i < _numMachines; i++)
    {
        // Iterate sub types
        for (int j = 0; j < _pMachines[i]->getDescriptorTableCount(); j++)
        {
            if (strncasecmp(mcName, _pMachines[i]->getDescriptorTable(j)->machineName, MAX_MACHINE_NAME_LEN) == 0)
            {
                return setMachineIdx(i, j, false);
            }
        }
    }
    return false;
}

bool McManager::setMachineOpts(const char* mcOpts)
{
    // Machine
    if (!_pCurMachine)
        return false;

    // Disable machine
    BusAccess::waitSetup(false, false);
    _pCurMachine->disable();

    // Get options
    McDescriptorTable* pMcDescr = getDescriptorTable();
    pMcDescr->emulatedRAM = (strstr(mcOpts, "EMURAM") != 0);
    pMcDescr->setRegistersByInjection = (strstr(mcOpts, "INJECT") != 0);

    // Debug
    LogWrite(FromMcManager, LOG_DEBUG, "setMachineOpts %s emuRAM %d instrInject %d", mcOpts,
                pMcDescr->emulatedRAM,
                pMcDescr->setRegistersByInjection);

    // Set RAM paging based on whether emulation is active
    RAMEmulator::activateEmulation(pMcDescr->emulatedRAM);
    HwManager::pageOutForEmulation(pMcDescr->emulatedRAM);

    // Bus raider enable wait states
    BusAccess::waitSetup(pMcDescr->monitorIORQ, pMcDescr->monitorMREQ || pMcDescr->emulatedRAM);

    // Enable
    _pCurMachine->enable(_curMachineSubType);
    return true;
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

void McManager::sendRemoteDebugProtocolMsg(const char* pStr, const char* rdpMessageIdStr)
{
    // LogWrite(FromMcManager, LOG_VERBOSE, "debugMsg %s cmdH %d msgId %s", pStr, _pCommandHandler, rdpMessageIdStr); 
    if (_pCommandHandler)
        _pCommandHandler->sendRemoteDebugProtocolMsg(pStr, rdpMessageIdStr);
}

bool McManager::debuggerCommand(char* pCommand, char* pResponse, int maxResponseLen)
{
    McBase* pMc = getMachine();
    // LogWrite(FromMcManager, LOG_VERBOSE, "debuggerCommand %s %d %d\n", pCommand, 
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
    // Check if we are paused or emulating memory - if so bus access cannot write memory
    if ((TargetDebug::get()->isPaused() || RAMEmulator::isActive()) && (!iorq))
        return RAMEmulator::blockWrite(addr, pBuf, len);
    // Grab the bus
    return BusAccess::blockWrite(addr, pBuf, len, busRqAndRelease, iorq) == BR_OK;
}

bool McManager::blockRead(uint32_t addr, uint8_t* pBuf, uint32_t len, bool busRqAndRelease, bool iorq)
{
    // Check if emulating memory or if we are paused - if the latter bus access cannot get memory but stepping process will have
    // grabbed all of physical RAM and placed it in the buffer for debugging
    if ((TargetDebug::get()->isPaused() || RAMEmulator::isActive()) && (!iorq))
        return RAMEmulator::blockRead(addr, pBuf, len);
    // Grab the bus
    return BusAccess::blockRead(addr, pBuf, len, busRqAndRelease, iorq) == BR_OK;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Target control
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Handle control of target machine
void McManager::targetReset(bool restoreWaitDefaults, bool holdInReset)
{
    // Reset attached hardware
    HwManager::reset();

    // Reset target
    bool resetDone = false;
    McBase* pMc = McManager::getMachine();
    if (pMc)
        resetDone = pMc->reset(restoreWaitDefaults, holdInReset);
    if (!resetDone)
        BusAccess::targetReset(restoreWaitDefaults, holdInReset);
}

void McManager::targetClearAllIO()
{
    BusAccess::clearAllIO();
}

void McManager::targetPause()
{
    TargetDebug::get()->pause();
}

void McManager::targetRelease()
{
    TargetDebug::get()->release();
}

void McManager::targetStep(bool stepOver)
{
    TargetDebug::get()->step(stepOver);
}

bool McManager::targetIsPaused()
{
    return TargetDebug::get()->isPaused();
}

bool McManager::targetBusUnderPiControl()
{
    return BusAccess::isUnderControl();
}

uint32_t McManager::busAccessCallback(uint32_t addr, uint32_t data, uint32_t flags)
{
    // Offer to the hardware manager
    uint32_t retVal = BR_MEM_ACCESS_RSLT_NOT_DECODED;

    // Emulated RAM
    retVal = RAMEmulator::handleWaitInterrupt(addr, data, flags, retVal);
    
    // Hardware management   
    retVal = HwManager::handleWaitInterrupt(addr, data, flags, retVal);

    // Callback to debugger
    retVal = TargetDebug::get()->handleWaitInterrupt(addr, data, flags, retVal);

    // Step validator
    retVal = _stepTester.handleWaitInterrupt(addr, data, flags, retVal);
    if (_pStepValidator)
        retVal = _pStepValidator->handleWaitInterrupt(addr, data, flags, retVal);

    // Offer to target machine
    if (_pCurMachine)
        return _pCurMachine->busAccessCallback(addr, data, flags, retVal);

    return BR_MEM_ACCESS_RSLT_NOT_DECODED;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Target programming
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Handle programming of target machine
void McManager::handleTargetProgram(bool resetAfterProgramming, bool addWaitOnInstruction,
            bool holdInPause, bool forceSetRegsByInjection)
{
    // Check there is something to write
    if (TargetState::numMemoryBlocks() == 0) 
    {
        // Nothing new to write
        LogWrite(FromMcManager, LOG_DEBUG, "ProgramTarget - nothing to write");
    } 
    else 
    {
        // Let the TargetDebug module know that we are programming
        TargetDebug::get()->targetProgrammingStarted();

        // BUSRQ is used even if memory is emulated because it holds the processor while changes are made

        // Release bus if paused or controlled
        BusAccess::controlRelease(false, false);

        // Handle programming in one BUSRQ/BUSACK pass
        if (BusAccess::controlRequestAndTake() != BR_OK)
        {
            LogWrite(FromMcManager, LOG_DEBUG, "ProgramTarget - failed to capture bus");   
            return;
        }

        // Write the blocks
        bool blockWrittenAtResetEntryPoint = false;
        for (int i = 0; i < TargetState::numMemoryBlocks(); i++) {
            TargetState::TargetMemoryBlock* pBlock = TargetState::getMemoryBlock(i);
            LogWrite(FromMcManager, LOG_VERBOSE,"ProgramTarget start %08x len %d", pBlock->start, pBlock->len);
            McManager::blockWrite(pBlock->start, TargetState::getMemoryImagePtr() + pBlock->start, pBlock->len, false, false);
            if (pBlock->start == Z80_PROGRAM_RESET_VECTOR)
                blockWrittenAtResetEntryPoint = true;
        }

        // Written
        LogWrite(FromMcManager, LOG_VERBOSE, "ProgramTarget - written %d blocks", TargetState::numMemoryBlocks());

        // Check for hold in pause
        if (holdInPause)
            TargetDebug::get()->pause();

        // Check for reset too
        if (resetAfterProgramming)
        {
            LogWrite(FromMcManager, LOG_DEBUG, "Starting target code");
            bool performHardReset = true;
            if (TargetState::areRegistersValid())
            {
                // Check how to set registers
                if (getDescriptorTable()->setRegistersByInjection || forceSetRegsByInjection)
                {
                    // Use the BusAccess module to inject instructions to set registers
                    // Z80Registers regs;
                    // TargetState::getTargetRegs(regs);
                    // TargetDebug::get()->startSetRegisterSequence(&regs);
                    // performHardReset = false;
                }
                else
                {
                    // If the code doesn't start at 0, generate a code snippet to set registers and run
                    if (!blockWrittenAtResetEntryPoint)
                    {
                        uint8_t regSetCode[MAX_REGISTER_SET_CODE_LEN];
                        Z80Registers regs;
                        TargetState::getTargetRegs(regs);
                        char regsStr[500];
                        regs.format(regsStr, 500);
                        LogWrite(FromMcManager, LOG_DEBUG, "Regs: %s", regsStr);
                        uint32_t codeDestAddr = getDescriptorTable()->setRegistersCodeAddr;
                        int codeLen = TargetCPUZ80::getSnippetToSetRegs(codeDestAddr, regs, regSetCode, MAX_REGISTER_SET_CODE_LEN);
                        if (codeLen != 0)
                        {
                            // Reg setting code
                            LogWrite(FromMcManager, LOG_DEBUG,"Set regs snippet at %04x len %d", codeDestAddr, codeLen);
                            McManager::blockWrite(codeDestAddr, regSetCode, codeLen, false, false);
                            
                            // Reset vector
                            uint8_t jumpCmd[3] = { 0xc3, uint8_t(codeDestAddr & 0xff), uint8_t((codeDestAddr >> 8) & 0xff) };
                            McManager::blockWrite(0, jumpCmd, 3, false, false);
                            ee_dump_mem(regSetCode, regSetCode + codeLen);
                        }
                    }
                }
            }

            // Release bus
            LogWrite(FromMcManager, LOG_DEBUG, "controlRelease hardReset %s", performHardReset ? "Y" : "N");
            BusAccess::controlRelease(performHardReset, addWaitOnInstruction);
        }
        else
        {
            // Release bus
            BusAccess::controlRelease(false, addWaitOnInstruction);
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Target file handling
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void McManager::handleTargetFile(const char* rxFileInfo, const uint8_t* pData, int dataLen)
{
    McBase* pMc = McManager::getMachine();
    if (pMc)
        pMc->fileHandler(rxFileInfo, pData, dataLen);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Command handling
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void McManager::handleCommand(const char* pCmdJson, 
                    [[maybe_unused]] const uint8_t* pParams, [[maybe_unused]] int paramsLen,
                    char* pRespJson, int maxRespLen)
{
    // LogWrite(FromMcManager, LOG_DEBUG, "req %s", pCmdJson);
    #define MAX_CMD_NAME_STR 200
    char cmdName[MAX_CMD_NAME_STR+1];
    if (!jsonGetValueForKey("cmdName", pCmdJson, cmdName, MAX_CMD_NAME_STR))
        return;
    pRespJson[0] = 0;

    if (strcasecmp(cmdName, "getStatus") == 0)
    {
        const char* mcJSON = getMachineJSON();
        if (pRespJson)
            strlcat(pRespJson, mcJSON, maxRespLen);
    }
    else if (strcasecmp(cmdName, "ClearTarget") == 0)
    {
        // LogWrite(FromMcManager, LOG_VERBOSE, "ClearTarget");
        TargetState::clear();
    }
    else if (strcasecmp(cmdName, "ProgramTarget") == 0)
    {
        McManager::handleTargetProgram(false, false, false, false);
    }
    else if (strcasecmp(cmdName, "ProgramAndReset") == 0)
    {
        McManager::handleTargetProgram(true, false, false, false);
    }
    else if (strcasecmp(cmdName, "ResetTarget") == 0)
    {
        // LogWrite(FromMcManager, LOG_VERBOSE, "ResetTarget");
        McManager::targetReset(true, false);
    }
    else if (strcasecmp(cmdName, "PauseTarget") == 0)
    {
        // LogWrite(FromMcManager, LOG_VERBOSE, "PauseTarget");
        McManager::targetPause();
    }
    else if (strcasecmp(cmdName, "ReleaseTarget") == 0)
    {
        // LogWrite(FromMcManager, LOG_VERBOSE, "ReleaseTarget");
        McManager::targetRelease();
    }
    else if (strcasecmp(cmdName, "IOClrTarget") == 0)
    {
        LogWrite(FromMcManager, LOG_VERBOSE, "IO Clear Target");
        McManager::targetClearAllIO();
    }
    else if ((strcasecmp(cmdName, "FileTarget") == 0) || ((strcasecmp(cmdName, "SRECTarget") == 0)))
    {
        // LogWrite(FromMcManager, LOG_VERBOSE, "File to Target, len %d", paramsLen);
        McBase* pMc = McManager::getMachine();
        if (pMc)
            pMc->fileHandler(pCmdJson, pParams, paramsLen);
    }
    else if (strcasecmp(cmdName, "SRECTarget") == 0)
    {
        // LogWrite(FromMcManager, LOG_VERBOSE, "SREC to Target, len %d", paramsLen);
        McBase* pMc = McManager::getMachine();
        if (pMc)
            pMc->fileHandler(pCmdJson, pParams, paramsLen);

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
    }
    else if (strncasecmp(cmdName, "mcoptions", strlen("mcoptions")) == 0)
    {
        // Get options
        const char* pOptions = strstr(cmdName,"=");
        if (pOptions)
        {
            // Move to first char of actual parameter
            pOptions++;
            LogWrite(FromMcManager, LOG_VERBOSE, "Set Machine options to %s", pOptions);
            setMachineOpts(pOptions);
        }
    }
    else if (strcasecmp(cmdName, "RxHost") == 0)
    {
        // LogWrite(FromMcManager, LOG_VERBOSE, "RxFromHost, len %d", dataLen);
        handleRxCharFromTarget(pParams, paramsLen);
    }
    else if (strcasecmp(cmdName, "rdp") == 0)
    {
        // Get message index value
        static const int MAX_RDP_MSG_ID_LEN = 20;
        char rdpMessageIdStr[MAX_RDP_MSG_ID_LEN+1];
        strcpy(rdpMessageIdStr, "0");
        if (!jsonGetValueForKey("index", pCmdJson, rdpMessageIdStr, MAX_RDP_MSG_ID_LEN))
            LogWrite(FromMcManager, LOG_DEBUG, "RDP message no index value found");
        // Send to remote debug handler
        static const int MAX_CMD_STR_LEN = 200;
        static char commandStr[MAX_CMD_STR_LEN+1];
        if (paramsLen > MAX_CMD_STR_LEN)
            paramsLen = MAX_CMD_STR_LEN;
        memcpy(commandStr, pParams, paramsLen);
        commandStr[paramsLen] = 0;
        static const int MAX_RESPONSE_MSG_LEN = 2000;
        static char responseMessage[MAX_RESPONSE_MSG_LEN+1];
        responseMessage[0] = 0;
        McManager::debuggerCommand(commandStr, responseMessage, MAX_RESPONSE_MSG_LEN);
        // Send response back
        sendRemoteDebugProtocolMsg(responseMessage, rdpMessageIdStr);
    }
    else if (strcasecmp(cmdName, "steptesterstart") == 0)
    {
        // Step validation
        if (_pStepValidator)
            _pStepValidator->start();
    }
    else if (strcasecmp(cmdName, "steptesterstop") == 0)
    {
        // Step validation
        if (_pStepValidator)
            _pStepValidator->stop();
    }
    else
    {
        LogWrite(FromMcManager, LOG_DEBUG, "Unknown command %s", cmdName);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Log
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void McManager::logDebugMessage(const char* pStr)
{
    if (_pCommandHandler)
        _pCommandHandler->logDebugMessage(pStr);
}

