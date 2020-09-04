// Bus Raider Machine Manager
// Rob Dobson 2018

#include "McManager.h"
#include <string.h>
#include "lowlib.h"
#include "rdutils.h"
#include "PiWiring.h"
#include "TargetProgrammer.h"
#include "BusAccess.h"
#include "TargetCPUZ80.h"
#include "HwManager.h"
#include "McTRS80.h"
#include "McRobsZ80.h"
#include "McZXSpectrum.h"
#include "McTerminal.h"
#include <circle/util.h>

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Variables
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Module name
static const char MODULE_PREFIX[] = "McManager";

// Singleton
McManager* McManager::_pMcManager = NULL;

McManager::McManager(DisplayBase* pDisplay, CommandHandler& commandHandler, HwManager& hwManager, 
                BusAccess& busAccess, TargetProgrammer& targetProgrammer) :
            _pDisplay(pDisplay),
            _commandHandler(commandHandler), 
            _hwManager(hwManager),
            _busAccess(busAccess),
            _targetProgrammer(targetProgrammer)
{
    // Sockets
    _busSocketId = -1;
    _commsSocketId = -1;
   
    // McManager
    _pMcManager = this;
    _numMachines = 0;
    _pCurMachine = NULL;
    _rxHostCharsBufferLen = 0;
    _refreshCount = 0;
    _refreshLastUpdateUs = 0;
    _refreshLastCountResetUs = 0;
    _refreshRate = 0;
    _screenMirrorOut = false;
    _screenMirrorCount = 0;
    _screenMirrorLastUs = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Init
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void McManager::init()
{
    // Connect to the bus socket
    if (_busSocketId < 0)
        _busSocketId = _busAccess.busSocketAdd(
            true,
            handleWaitInterruptStatic,
            busActionCompleteStatic,
            false,
            false,
            // Reset
            false,
            0,
            // NMI
            false,
            0,
            // IRQ
            false,
            0,
            false,
            BR_BUS_ACTION_DISPLAY,
            false,
            this
        );

    // Connect to the comms socket
    if (_commsSocketId < 0)
        _commsSocketId = _commandHandler.commsSocketAdd(this, true, handleRxMsgStatic, 
                            NULL, targetFileHandlerStatic);

    // LogWrite(MODULE_PREFIX, LogDebug, "COMMS SOCKET INIT %d", _commsSocketId);

    // Add machines - McBase does the actual add
    new McTerminal(*this);
    new McTRS80(*this);
    new McRobsZ80(*this);
    new McZXSpectrum(*this);

    // Refresh init
    _refreshCount = 0;
    _refreshLastUpdateUs = 0;
    
    // Screen mirroring
    _screenMirrorOut = true;
    _screenMirrorLastUs = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void McManager::service()
{
    // Check for screen mirroring
    if (_screenMirrorOut && _pCurMachine)
    {
        if (isTimeout(micros(), _screenMirrorLastUs, SCREEN_MIRROR_REFRESH_US))
        {
            // See if time to force a full refresh
            bool forceGetAll = false;
            if (_screenMirrorCount++ > SCREEN_MIRROR_FULL_REFRESH_COUNT)
            {
                forceGetAll = true;
                _screenMirrorCount = 0;
            }
            // Check for changes
            uint8_t mirrorChanges[McBase::MAX_MIRROR_CHANGE_BUF_LEN];
            uint32_t mirrorChangeLen = _pCurMachine->getMirrorChanges(mirrorChanges, McBase::MAX_MIRROR_CHANGE_BUF_LEN, forceGetAll);
            // LogWrite(MODULE_PREFIX, LOG_DEBUG, "Change len %d", mirrorChangeLen);
            if (mirrorChangeLen > 0)
                _commandHandler.sendWithJSON("mirrorScreen", "", 0, mirrorChanges, mirrorChangeLen);
            _screenMirrorLastUs = micros();
        }
    }
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
    if (getMachine() == NULL)
        return "Unknown";
    return getMachine()->getMachineName();
}

// const char* McManager::getMachineForFileType(const char* fileType)
// {
//     for (int i = 0; i < _numMachines; i++)
//     {
//         if (_pMachines[i])
//         {
//             if (_pMachines[i]->canProcFileType(fileType))
//             {
//                 return _pMachines[i]->getMachineName();
//             }
//         }
//     }
//     return NULL;
// }

// McVariantTable* McManager::getDescriptorTable()
// {
//     McVariantTable* pDescriptorTable = NULL;
//     if (_pCurMachine)
//         pDescriptorTable = _pCurMachine->getDescriptorTable();
//     if (pDescriptorTable)
//         return pDescriptorTable;
//     return &defaultDescriptorTable;
// }

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
    strlcat(mcString, getMachineName(), MAX_MC_JSON_LEN);
    strlcat(mcString, "\"", MAX_MC_JSON_LEN);

    // Clock info
    uint32_t actualHz = getBusAccess().clockCurFreqHz();
    snprintf(mcString+strlen(mcString), MAX_MC_JSON_LEN, ",\"clockHz\":\"%d\"", (int)actualHz);

    // Ret
    return mcString;
}

int McManager::getMachineClock()
{
    return getBusAccess().clockCurFreqHz();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Manage Machine List
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void McManager::targetIrq(int durationTStates)
{
    // Generate a maskable interrupt
    getBusAccess().targetReqIRQ(_busSocketId, durationTStates);
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
    static const int MAX_MC_JSON_LEN = 1000;
    char mcJson[MAX_MC_JSON_LEN];
    snprintf(mcJson, MAX_MC_JSON_LEN, "\"name\":\"%s\"", mcName);
    return setupMachine(mcJson);
}

bool McManager::setupMachine(const char* mcJson)
{
    // Extract machine name
    static const int MAX_MC_NAME_LEN = 100;
    char mcName [MAX_MC_NAME_LEN];
    if (!jsonGetValueForKey("name", mcJson, mcName, MAX_MC_NAME_LEN))
        return false;

    // Ask each machine if it is them
    McBase* pMc = NULL;
    for (int i = 0; i < _numMachines; i++)
    {
        if (_pMachines[i])
        {
            uint32_t variantIdx = 0;
            if (_pMachines[i]->isCalled(mcName, variantIdx))
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

    // Remove step tracer
    // TODO
    // getStepTracer().stopAll(true);

    // Remove wait generation
    getBusAccess().waitOnIO(_busSocketId, false);
    getBusAccess().waitOnMemory(_busSocketId, false);

    // Setup the machine
    pMc->setupMachine(mcName, mcJson);

    // Setup display
    pMc->setupDisplay(_pDisplay);

    // Enable wait generation as required
    getBusAccess().waitOnIO(_busSocketId, pMc->isMonitorIORQEnabled());
    getBusAccess().waitOnMemory(_busSocketId, pMc->isMonitorMREQEnabled());

    // See if any files to load
    static const int MAX_FILE_NAME_LEN = 100;
    char loadName [MAX_FILE_NAME_LEN];
    if (jsonGetValueForKey("load", mcJson, loadName, MAX_FILE_NAME_LEN))
    {
        // Request the file from the ESP32
        static const int MAX_API_REQ_LEN = 200;
        char apiReqStr[MAX_API_REQ_LEN];
        strlcpy(apiReqStr, "runfileontarget//", MAX_API_REQ_LEN);
        strlcat(apiReqStr, loadName, MAX_API_REQ_LEN);
        // Send command to ESP32
        _commandHandler.sendAPIReq(apiReqStr);
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "runFileOnTarget %s", apiReqStr);
    }
    else
    {
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "no load cmd found");
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Display refresh
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void McManager::displayRefresh()
{
    // Check valid
    if (!_pCurMachine)
        return;

    unsigned long reqUpdateUs = 1000000 / _pCurMachine->getDisplayRefreshRatePerSec();
    // Drop rate to one tenth if TargetTracker is running
    // TODO 2020
    // if (getTargetTracker().isTrackingActive())
    //     reqUpdateUs = 10 * reqUpdateUs;
    if (isTimeout(micros(), _refreshLastUpdateUs, reqUpdateUs)) 
    {
        // Update timings
        _refreshLastUpdateUs = micros();
        _refreshCount++;

        // Determine whether display is memory mapped
        if (_pCurMachine->isDisplayMemoryMapped())
        {
            // TODO 2020
            // if (getTargetTracker().busAccessAvailable())
            // {
            //     // Asynch display refresh - start bus access request here
            //     getBusAccess().targetReqBus(_busSocketId, BR_BUS_ACTION_DISPLAY);
            //     _busActionPendingDisplayRefresh = true;
            // }
            // else if (getTargetTracker().isTrackingActive())
            // {
            //     // Refresh from mirror hardware
            //     _busActionPendingDisplayRefresh = true;
            //     _pCurMachine->displayRefreshFromMirrorHw();
            // }
            // else
            // {
                // Synchronous display update (from local memory copy)
                _pCurMachine->displayRefreshFromMirrorHw();
            // }
        }
        else
        {
            _pCurMachine->displayRefreshFromMirrorHw();
        }

        // Heartbeat
        // TODO 2020
        // if (!getTargetTracker().isTrackingActive())
        //     machineHeartbeat();
    }

    // Service machine
    _pCurMachine->service();

    // Check for reset of rate
    if (isTimeout(micros(), _refreshLastCountResetUs, REFRESH_RATE_WINDOW_SIZE_MS * 1000))
    {
        _refreshRate = _refreshCount * 1000 / REFRESH_RATE_WINDOW_SIZE_MS;
        _refreshCount = 0;
        _refreshLastCountResetUs = micros();
    }
}

void McManager::machineHeartbeat()
{
    if (_pCurMachine)
        _pCurMachine->machineHeartbeat();
}

int McManager::getDisplayRefreshRate()
{
    return _refreshRate;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Communication with machine
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void McManager::hostSerialAddRxCharsToBuffer(const uint8_t* pRxChars, uint32_t rxLen)
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
    memcopyfast(_rxHostCharsBuffer+_rxHostCharsBufferLen, pRxChars, rxLen);
    _rxHostCharsBufferLen += rxLen;

    // DEBUG
    // char buf[1000];
    // buf[0] = 0;
    // int toWr = rxLen;
    // if (rxLen > 250)
    //     toWr = 250;
    // for (int i = 0; i < toWr; i++)
    //     ee_sprintf(buf+strlen(buf), "%02x ", pRxChars[i]);
    // LogWrite(MODULE_PREFIX, LOG_DEBUG, "FromHostSer %s", buf);
}

uint32_t McManager::hostSerialNumChAvailable()
{
    return _rxHostCharsBufferLen;
}

uint32_t McManager::hostSerialReadChars(uint8_t* pBuf, uint32_t bufMaxLen)
{
    // Check for maximum chars available
    uint32_t charsToCopy = bufMaxLen;
    if (charsToCopy > _rxHostCharsBufferLen)
        charsToCopy = _rxHostCharsBufferLen;

    // Check valid
    if ((!pBuf) || (charsToCopy == 0))
        return 0;

    // Copy the chars
    memcopyfast(pBuf, _rxHostCharsBuffer, charsToCopy);

    // Move buffer down if required
    if (charsToCopy > _rxHostCharsBufferLen)
    {
        memmove(_rxHostCharsBuffer, _rxHostCharsBuffer+charsToCopy, _rxHostCharsBufferLen-charsToCopy);
        _rxHostCharsBufferLen -= charsToCopy;
    }
    else
    {
        _rxHostCharsBufferLen = 0;
    }
    return charsToCopy;
}

void McManager::sendKeyStrToTargetStatic(const char* pKeyStr)
{
    _commandHandler.sendKeyStrToTargetStatic(pKeyStr);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Target control
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// // Handle control of target machine
void McManager::targetReset()
{
    // Reset target
    getBusAccess().targetReqReset(_busSocketId);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Target programming
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Handle programming of target machine
void McManager::targetProgrammingStart(bool execAfterProgramming)
{
    // Check there is something to write
    if (getTargetProgrammer().numMemoryBlocks() == 0) 
    {
        // Nothing new to write
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "ProgramTarget - nothing to write");
    } 
    else 
    {
        // BUSRQ is used even if memory is emulated because it holds the processor while changes are made
        // Give the BusAccess some service first to ensure WAIT handling is complete before requesting the bus
        for (int i = 0; i < 3; i++)
            getBusAccess().service();

        // Request target bus
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "targetProgStart targetReqBus");
        getBusAccess().targetReqBus(_busSocketId, BR_BUS_ACTION_PROGRAMMING);
        _busActionPendingProgramTarget = true;
        _busActionPendingExecAfterProgram = execAfterProgramming;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Target file handling
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool McManager::targetFileHandlerStatic(void* pObject, const char* rxFileInfo, const uint8_t* pData, unsigned dataLen)
{
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "targetFileHandler");
    if (!pObject)
        return false;
    McManager* pMcManager = (McManager*)pObject;
    McBase* pMc = pMcManager->getMachine();
    if (pMc)
        return pMc->fileHandler(rxFileInfo, pData, dataLen);
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Received message handler
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool McManager::handleRxMsgStatic(void* pObject, const char* pCmdJson, 
                const uint8_t* pParams, unsigned paramsLen,
                char* pRespJson, unsigned maxRespLen)
{
    if (!pObject)
        return false;
    return ((McManager*)pObject)->handleRxMsg(pCmdJson, pParams, paramsLen, pRespJson, maxRespLen);

}

bool McManager::handleRxMsg(const char* pCmdJson, const uint8_t* pParams, unsigned paramsLen,
                char* pRespJson, unsigned maxRespLen)
{
    // LogWrite(MODULE_PREFIX, LOG_DEBUG, "handleRxMsg %s", pCmdJson);
    #define MAX_CMD_NAME_STR 200
    char cmdName[MAX_CMD_NAME_STR+1];
    if (!jsonGetValueForKey("cmdName", pCmdJson, cmdName, MAX_CMD_NAME_STR))
        return false;
    pRespJson[0] = 0;

    if (strcasecmp(cmdName, "ClearTarget") == 0)
    {
        // LogWrite(MODULE_PREFIX, LOG_VERBOSE, "ClearTarget");
        getTargetProgrammer().clear();
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "ProgramTarget") == 0)
    {
        _pMcManager->targetProgrammingStart(false);

        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if ((strcasecmp(cmdName, "ProgramAndReset") == 0) ||
            (strcasecmp(cmdName, "ProgramAndExec") == 0))
    {
        _pMcManager->targetProgrammingStart(true);
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "ResetTarget") == 0)
    {
        // LogWrite(MODULE_PREFIX, LOG_VERBOSE, "ResetTarget");
        _pMcManager->targetReset();
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if ((strcasecmp(cmdName, "FileTarget") == 0) || ((strcasecmp(cmdName, "SRECTarget") == 0)))
    {
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "File to Target, len %d, json %s", paramsLen, pCmdJson);
        McBase* pMc = _pMcManager->getMachine();
        if (pMc)
            pMc->fileHandler(pCmdJson, pParams, paramsLen);
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    // else if (strncasecmp(cmdName, "SetMachine", strlen("SetMachine")) == 0)
    // {
    //     // Get machine name
    //     const char* pMcName = strstr(cmdName,"=");
    //     if (pMcName)
    //     {
    //         // Move to first char of actual name
    //         pMcName++;
    //         setMachineByName(pMcName);
    //         LogWrite(MODULE_PREFIX, LOG_VERBOSE, "Set Machine to %s", pMcName);
    //     }
    //     strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
    //     return true;
    // }
    else if (strcasecmp(cmdName, "SetMcJson") == 0)
    {
        // Get mcJson
        static char mcJson[_commandHandler.MAX_MC_SET_JSON_LEN];
        size_t toCopy = paramsLen+1;
        if (toCopy > _commandHandler.MAX_MC_SET_JSON_LEN)
            toCopy = _commandHandler.MAX_MC_SET_JSON_LEN;
        strlcpy(mcJson, (const char*)pParams, toCopy);
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "SetMachine %s", mcJson);
        bool setupOk = setupMachine(mcJson); 
        if (setupOk)
            strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        else
            strlcpy(pRespJson, "\"err\":\"fail\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "RxHost") == 0)
    {
        // LogWrite(MODULE_PREFIX, LOG_VERBOSE, "RxFromHost, len %d", dataLen);
        hostSerialAddRxCharsToBuffer(pParams, paramsLen);
        return true;
    }
    else if (strcasecmp(cmdName, "sendKey") == 0)
    {
        char* pEnd = NULL;
        uint8_t usbKeyCodes[_commandHandler.NUM_USB_KEYS_PASSED];
        memset(usbKeyCodes, 0, sizeof(usbKeyCodes));
        unsigned asciiCode = strtoul((const char*)pParams, &pEnd, 10);
        usbKeyCodes[0] = strtoul(pEnd, &pEnd, 10);
        unsigned usbModCode = strtoul(pEnd, &pEnd, 10);
        asciiCode = asciiCode;
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "SendKey, %s ascii 0x%02x usbKey 0x%02x usbMod 0x%02x", 
        //             pParams, asciiCode, usbKeyCodes[0], usbModCode);
        if (_pCurMachine)
            _pCurMachine->keyHandler(usbModCode, usbKeyCodes);
        return true;
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Start target program - or reset
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void McManager::targetExec()
{
    // TODO 2020
    // LogWrite(MODULE_PREFIX, LOG_DEBUG, "Starting target code, debugActive %c", 
    //                 getTargetTracker().isTrackingActive() ? 'Y' : 'N');
    bool performHardReset = true;
    // TODO 2020
    // if (getTargetProgrammer().areRegistersValid())
    // {
    //     // Check how to set registers
    //     if (getHwManager().getOpcodeInjectEnable() || getTargetTracker().isTrackingActive())
    //     {
    //         // Use the TargetTracker module to inject instructions to set registers
    //         Z80Registers regs;
    //         getTargetProgrammer().getTargetRegs(regs);
    //         getTargetTracker().startSetRegisterSequence(&regs);
    //         performHardReset = false;
    //     }
    //     else
    //     {
    //         // If the code doesn't start at 0 or a start location has been supplied,
    //         // generate a code snippet to set registers and run
    //         if (!_busActionCodeWrittenAtResetVector || getTargetProgrammer().areRegistersValid())
    //         {
    //             uint8_t regSetCode[MAX_REGISTER_SET_CODE_LEN];
    //             Z80Registers regs;
    //             getTargetProgrammer().getTargetRegs(regs);
    //             static const int REGISTERS_STR_MAX_LEN = 500;
    //             char regsStr[REGISTERS_STR_MAX_LEN];
    //             regs.format(regsStr, REGISTERS_STR_MAX_LEN);
    //             LogWrite(MODULE_PREFIX, LOG_DEBUG, "Regs: %s", regsStr);
    //             uint32_t codeDestAddr = 0;
    //             if (_pCurMachine)
    //                 _pCurMachine->getSetRegistersCodeAddr();
    //             int codeLen = TargetCPUZ80::getSnippetToSetRegs(codeDestAddr, regs, regSetCode, MAX_REGISTER_SET_CODE_LEN);
    //             if (codeLen != 0)
    //             {
    //                 // Reg setting code
    //                 LogWrite(MODULE_PREFIX, LOG_DEBUG,"Set regs snippet at %04x len %d", codeDestAddr, codeLen);
    //                 getHwManager().blockWrite(codeDestAddr, regSetCode, codeLen, false, false, false);
                    
    //                 // Reset vector
    //                 uint8_t jumpCmd[3] = { 0xc3, uint8_t(codeDestAddr & 0xff), uint8_t((codeDestAddr >> 8) & 0xff) };
    //                 getHwManager().blockWrite(Z80_PROGRAM_RESET_VECTOR, jumpCmd, 3, false, false, false);

    //                 // TODO 2020
    //                 // LogDumpMemory(MODULE_PREFIX, LOG_DEBUG, regSetCode, regSetCode + codeLen);
    //             }
    //         }
    //     }
    // }

    // See if we need to do a hard reset
    if (performHardReset)
    {
        // Request reset target
        getBusAccess().targetReqReset(_busSocketId);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Bus action callback
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void McManager::busActionCompleteStatic(void* pObject, BR_BUS_ACTION actionType,  BR_BUS_ACTION_REASON reason)
{
    if (!pObject)
        return;
    ((McManager*)pObject)->busActionComplete(actionType, reason);
}

void McManager::busActionComplete(BR_BUS_ACTION actionType,  BR_BUS_ACTION_REASON reason)
{
    // LogWrite(MODULE_PREFIX, LOG_DEBUG,"bus action complete type %d reason %d programPending %d dispPending %d", 
    //         actionType, reason, _busActionPendingProgramTarget, _busActionPendingDisplayRefresh);
    // We don't care what the reason for the BUSRQ is we will use it for what we need
    if (actionType == BR_BUS_ACTION_BUSRQ)
    {
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "busActionCompleteStatic BUSRQ");

        // Program target pending?
        if (_busActionPendingProgramTarget)
        {
            // LogWrite(MODULE_PREFIX, LOG_DEBUG, "busActionCompleteStatic pendingProgramTarget numBlocks %d",
            //                 getTargetProgrammer().numMemoryBlocks());

            // Write the blocks
            _busActionCodeWrittenAtResetVector = false;
            for (int i = 0; i < getTargetProgrammer().numMemoryBlocks(); i++) {
                TargetProgrammer::TargetMemoryBlock* pBlock = getTargetProgrammer().getMemoryBlock(i);
                BR_RETURN_TYPE brResult = getHwManager().blockWrite(pBlock->start, 
                            getTargetProgrammer().getMemoryImagePtr() + pBlock->start, pBlock->len, false, false, false);
                LogWrite(MODULE_PREFIX, LOG_DEBUG,"ProgramTarget done %08x len %d result %d micros %u", pBlock->start, pBlock->len, brResult, micros());
                if (pBlock->start == Z80_PROGRAM_RESET_VECTOR)
                    _busActionCodeWrittenAtResetVector = true;
            }

            // // Debug
            // uint8_t testBlock[0x100];
            // uint32_t baseAddr = 0x6000;
            // uint32_t blockLen = 0x100;
            // BusAccess::blockRead(baseAddr, testBlock, blockLen, false, false);
            // char buf2[100];
            // buf2[0] = 0;
            // uint32_t lineStart = 0;
            // for (uint32_t i = 0; i < blockLen; i++)
            // {
            //     char buf1[10];
            //     ee_sprintf(buf1, "%02x ", testBlock[i]);
            //     strlcat(buf2, buf1, 100);
            //     if (i % 0x10 == 0x0f)
            //     {
            //         LogWrite(MODULE_PREFIX, LOG_DEBUG, "%04x %s", baseAddr+lineStart, buf2);
            //         buf2[0] = 0;
            //         lineStart = i+1;
            //     }
            // }

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

void McManager::handleWaitInterruptStatic(void* pObject, uint32_t addr, uint32_t data, 
        uint32_t flags, uint32_t& retVal)
{
    if (!pObject)
        return;
    McBase* pMc = ((McManager*)pObject)->_pCurMachine;
    if (!pMc)
        return;
    pMc->busAccessCallback(addr, data, flags, retVal);
}