// Bus Raider Machine Manager
// Rob Dobson 2018

#include "McManager.h"
#include <string.h>
#include "lowlib.h"
#include "rdutils.h"
#include "PiWiring.h"
#include "TargetCPUZ80.h"
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

McManager::McManager(DisplayBase* pDisplay, CommandHandler& commandHandler, BusControl& busAccess) :
            _pDisplay(pDisplay),
            _commandHandler(commandHandler),
            _busAccess(busAccess)
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
    _refreshLastUpdateMs = 0;
    _refreshLastCountResetMs = 0;
    _refreshRate = 0;
    _screenMirrorOut = false;
    _screenMirrorCount = 0;
    _screenMirrorLastMs = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Init
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void McManager::init()
{
    // Connect to the bus socket
    if (_busSocketId < 0)
        _busSocketId = _busAccess.sock().add(
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
    // TODO 2020
    new McTerminal(*this, _busAccess);
    new McTRS80(*this, _busAccess);
    // new McRobsZ80(*this);
    // new McZXSpectrum(*this);

    // Refresh init
    _refreshCount = 0;
    _refreshLastUpdateMs = 0;
    
    // Screen mirroring
    _screenMirrorOut = true;
    _screenMirrorLastMs = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void McManager::service()
{
    // Refresh display
    displayRefresh();

    // Check for screen mirroring
    if (_screenMirrorOut && _pCurMachine)
    {
        if (isTimeout(millis(), _screenMirrorLastMs, SCREEN_MIRROR_REFRESH_MS))
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
            _screenMirrorLastMs = millis();
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
    uint32_t actualHz = _busAccess.clock().getFreqInHz();
    snprintf(mcString+strlen(mcString), MAX_MC_JSON_LEN, 
                ",\"clockHz\":\"%d\"", (int)actualHz);

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
    bool nameValid = jsonGetValueForKey("name", mcJson, mcName, MAX_MC_NAME_LEN);
    if (!nameValid || (strlen(mcName) == 0))
    {
        // Select first machine
        if (_numMachines > 0)
            strlcpy(mcName, _pMachines[0]->getMachineName(), sizeof(mcName));
    }

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

    // Warn bus control
    _busAccess.machineChangeInit();

    // Remove step tracer
    // TODO
    // getStepTracer().stopAll(true);

    // Setup the machine
    pMc->setupMachine(mcName, mcJson);

    // Setup display
    pMc->setupDisplay(_pDisplay);

    // Setup the bus socket
    _busAccess.sock().setup(_busSocketId, 
                pMc->isMonitorMREQEnabled(), pMc->isMonitorIORQEnabled());

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
    // else
    // {
    //     LogWrite(MODULE_PREFIX, LOG_DEBUG, "no load cmd found");
    // }

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

    unsigned long reqUpdateMs = 1000 / _pCurMachine->getDisplayRefreshRatePerSec();
    // Drop rate to one tenth if TargetTracker is running
    // TODO 2020
    // if (getTargetTracker().isTrackingActive())
    //     reqUpdateUs = 10 * reqUpdateUs;
    if (isTimeout(millis(), _refreshLastUpdateMs, reqUpdateMs)) 
    {
        // Update timings
        _refreshLastUpdateMs = millis();
        _refreshCount++;

        // LogWrite(MODULE_PREFIX, LOG_NOTICE, "displayRefresh isMemMapped %d waitIsHeld %d isEmulatingMem %d isTrackingActive %d",
        //     _pCurMachine->isDisplayMemoryMapped(), _busAccess.waitIsHeld(), _hwManager.isEmulatingMemory(), _busAccess.isTrackingActive());

        // TODO 2020
        // Determine whether display is memory mapped
        if (_pCurMachine->isDisplayMemoryMapped()) // && _busAccess.busRqNeededForMemAccess())
        {
            // TODO 2020
            // if (getTargetTracker().busAccessAvailable())
            // {
                // Asynch display refresh - start bus access request here
                _busAccess.sock().reqBus(_busSocketId, BR_BUS_ACTION_DISPLAY);
                _busActionPendingDisplayRefresh = true;
            // }
            // else if (getTargetTracker().isTrackingActive())
            // {
            //     // Refresh from mirror hardware
            //     _busActionPendingDisplayRefresh = true;
            //     _pCurMachine->refreshDisplay();
            // }
            // else
            // {
                // Synchronous display update (from local memory copy)
                // _pCurMachine->refreshDisplay();
            // }
        }
        else
        {
            _pCurMachine->refreshDisplay();
        }

        // Heartbeat
        machineHeartbeat();
    }

    // Service machine
    _pCurMachine->service();

    // Check for reset of rate
    if (isTimeout(millis(), _refreshLastCountResetMs, REFRESH_RATE_WINDOW_SIZE_MS))
    {
        _refreshRate = _refreshCount * 1000 / REFRESH_RATE_WINDOW_SIZE_MS;
        _refreshCount = 0;
        _refreshLastCountResetMs = millis();
    }
}

void McManager::machineHeartbeat()
{
    if (!_busAccess.ctrl().allowHeartbeat())
        return;
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
        _busAccess.prog().clear();
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "ProgramTarget") == 0)
    {
        _busAccess.ctrl().programmingStart(false);
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if ((strcasecmp(cmdName, "ProgramAndReset") == 0) ||
            (strcasecmp(cmdName, "ProgramAndExec") == 0))
    {
        _busAccess.ctrl().programmingStart(true);
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "ResetTarget") == 0)
    {
        // LogWrite(MODULE_PREFIX, LOG_VERBOSE, "ResetTarget");
        _busAccess.sock().reqReset(_busSocketId);
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if ((strcasecmp(cmdName, "FileTarget") == 0) || ((strcasecmp(cmdName, "SRECTarget") == 0)))
    {
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "File to Target, len %d, json %s", paramsLen, pCmdJson);
        McBase* pMc = _pMcManager->getMachine();
        if (pMc)
            pMc->fileHandler(pCmdJson, pParams, paramsLen, _busAccess.prog());
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
// Target file handling
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool McManager::targetFileHandlerStatic(void* pObject, const char* rxFileInfo, const uint8_t* pData, unsigned dataLen)
{
    if (!pObject)
        return false;
    return ((McManager*)pObject)->targetFileHandler(rxFileInfo, pData, dataLen);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Target file handling
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool McManager::targetFileHandler(const char* rxFileInfo, const uint8_t* pData, unsigned dataLen)
{
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "targetFileHandler");
    McBase* pMc = getMachine();
    if (pMc)
        return pMc->fileHandler(rxFileInfo, pData, dataLen, _busAccess.prog());
    return false;
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
    // LogWrite(MODULE_PREFIX, LOG_DEBUG,"bus action complete type %d reason %d dispPending %d", 
    //         actionType, reason, _busActionPendingDisplayRefresh);

    // We don't care what the reason for the BUSRQ is we will use it for what we need
    if (actionType == BR_BUS_ACTION_BUSRQ)
    {
        // Display refresh pending?
        if (_busActionPendingDisplayRefresh)
        {
            // Call the machine to handle
            if (_pCurMachine)
                _pCurMachine->refreshDisplay();
            _busActionPendingDisplayRefresh = false;    
        }
    }
}

