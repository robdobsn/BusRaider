// Bus Raider
// Rob Dobson 2019

#include "ControlAPI.h"
#include "PiWiring.h"
#include "lowlib.h"
#include "logging.h"
#include "rdutils.h"
#include "BusControl.h"
#include "CommandHandler.h"
#include "McManager.h"
#include "DebugHelper.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Variables
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Module name
static const char MODULE_PREFIX[] = "ControlAPI";

// This instance
ControlAPI* ControlAPI::_pThisInstance = NULL;

// Debug
// #define DEBUG_RX_FRAME
// #define DEBUG_COMMS_SOCKET
// #define DEBUG_API_BLOCK_ACCESS_SYNC
// #define DEBUG_BUS_ACTION_COMPLETE
// #define DEBUG_API_DEBUGGER
// #define DEBUG_API_PROGRAMMER
// #define DEBUG_API_DETAIL

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Constructor
ControlAPI::ControlAPI(CommandHandler& commandHandler, BusControl& busControl, McManager& mcManager) :
        _commandHandler(commandHandler), _busControl(busControl), _mcManager(mcManager)
{
    _pThisInstance = this;
    // Sockets
    _busSocketId = -1;
    _commsSocketId = -1;
}

void ControlAPI::init()
{
    // Connect to the bus socket
    if (_busSocketId < 0)
        _busSocketId = _busControl.sock().addSocket( 
            true,
            ControlAPI::handleWaitInterruptStatic,
            ControlAPI::busReqAckedStatic,
            this);

    // Connect to the comms socket
    if (_commsSocketId < 0)
    {
        _commsSocketId = _commandHandler.commsSocketAdd(this, true, ControlAPI::handleRxMsgStatic, NULL, NULL);
#ifdef DEBUG_COMMS_SOCKET
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "init commsSocketId %d", _commsSocketId);
#endif
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Received message handler
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ControlAPI::handleRxMsgStatic(void* pObject, const char* pCmdJson, 
                const uint8_t* pParams, unsigned paramsLen,
                char* pRespJson, unsigned maxRespLen)
{
#ifdef DEBUG_RX_FRAME_STATIC
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "handleRxMsgStatic pObject %08x %s", (unsigned)pObject, pCmdJson);
#endif
    if (!pObject)
        return false;
    return ((ControlAPI*)pObject)->handleRxMsg(pCmdJson, pParams, paramsLen, pRespJson, maxRespLen);
}

bool ControlAPI::handleRxMsg(const char* pCmdJson, 
                const uint8_t* pParams, unsigned paramsLen,
                char* pRespJson, unsigned maxRespLen)
{

    // Get the command string from JSON
    static const int MAX_CMD_NAME_STR = 50;
    char cmdName[MAX_CMD_NAME_STR+1];
    if (!jsonGetValueForKey("cmdName", pCmdJson, cmdName, MAX_CMD_NAME_STR))
    {
#ifdef DEBUG_RX_FRAME
        LogWrite(MODULE_PREFIX, LOG_WARNING, "handleRxMsg ERROR NO CMD NAME json %s", pCmdJson);
#endif

        return false;
    }

#ifdef DEBUG_RX_FRAME
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "handleRxMsg cmdName %s json %s", cmdName, pCmdJson);
#endif

    // Default ok response
    strlcpy(pRespJson, R"("rslt":"ok")", maxRespLen);

    // Check cmdName
    if ((strcasecmp(cmdName, "imagerClear") == 0) || 
        (strcasecmp(cmdName, "progClear") == 0) || 
        (strcasecmp(cmdName, "ClearTarget") == 0))
    {
        // Clear the programmer used to program the target
        return apiImagerClear(pRespJson, maxRespLen);
    }
    if ((strcasecmp(cmdName, "imagerAdd") == 0) ||
        (strcasecmp(cmdName, "progAdd") == 0))
    {
        // Add a memory block to the programmer used to program the target
        return apiImagerAddMemBlock(pCmdJson, pParams, paramsLen, pRespJson, maxRespLen);
    }
    if ((strcasecmp(cmdName, "imagerWrite") == 0) || 
        (strcasecmp(cmdName, "progWrite") == 0) || 
        (strcasecmp(cmdName, "ProgramTarget") == 0))
    {
        // Write the programmer contents to the target
        return apiImagerWrite(pCmdJson, pParams, paramsLen, pRespJson, maxRespLen);
    }
    else if ((strcasecmp(cmdName, "imagerWriteAndExec") == 0) || 
            (strcasecmp(cmdName, "progWriteAndExec") == 0) || 
            (strcasecmp(cmdName, "ProgramAndReset") == 0) ||
            (strcasecmp(cmdName, "ProgramAndExec") == 0))
    {
        // Write the programmer contents to the target and then start the program on the target
        return apiImagerWriteAndExec(pRespJson, maxRespLen);
    }
    else if ((strcasecmp(cmdName, "targetReset") == 0) || (strcasecmp(cmdName, "ResetTarget") == 0))
    {
        // Reset the target processor
        return apiTargetReset(pRespJson, maxRespLen);
    }
    else if ((strcasecmp(cmdName, "fileSend") == 0) || 
             (strcasecmp(cmdName, "FileTarget") == 0) || 
             (strcasecmp(cmdName, "SRECTarget") == 0))
    {
        // Send file to file-system
        return apiFileToTarget(pCmdJson, pParams, paramsLen, pRespJson, maxRespLen);
    }
    else if (strcasecmp(cmdName, "SetMcJson") == 0)
    {
        // Set a machine to emulate
        return apiSetMcJson(pCmdJson, pParams, paramsLen, pRespJson, maxRespLen);
    }
    else if (strcasecmp(cmdName, "RxHost") == 0)
    {
#ifdef DEBUG_API_DETAIL
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "RxFromHost, len %d", paramsLen);
#endif
        // TODO 2020 add back in
        // hostSerialAddRxCharsToBuffer(pParams, paramsLen);
        return true;
    }
    else if (strcasecmp(cmdName, "sendKey") == 0)
    {
        // Send a key to the target
        return apiSendKeyToTarget(pCmdJson, pParams, paramsLen, pRespJson, maxRespLen);
    } 
    else if (strcasecmp(cmdName, "Rd") == 0)
    {
        // Read from target memory
        return apiReadFromTarget(pCmdJson, pParams, paramsLen, pRespJson, maxRespLen);
    }
    else if (strcasecmp(cmdName, "Wr") == 0)
    {
        // Write to target memory
        return apiWriteToTarget(pCmdJson, pParams, paramsLen, pRespJson, maxRespLen);
    }
    else if (strcasecmp(cmdName, "testWrRd") == 0)
    {
        // Write to target memory and then read back in a single BUSRQ
        return apiWriteReadTarget(pCmdJson, pParams, paramsLen, pRespJson, maxRespLen);
    }
    else if (strcasecmp(cmdName, "debugBreak") == 0)
    {
        // Break the target program execution and hold the processor
        return apiDebuggerBreak(pCmdJson, pParams, paramsLen, pRespJson, maxRespLen);
    }
    else if (strcasecmp(cmdName, "debugContinue") == 0)
    {
        // Continue execution
        return apiDebuggerContinue(pCmdJson, pParams, paramsLen, pRespJson, maxRespLen);
    }
    else if (strcasecmp(cmdName, "debugStepIn") == 0)
    {
        // Step the processor in
        return apiDebuggerStepIn(pCmdJson, pParams, paramsLen, pRespJson, maxRespLen);
    }
    else if (strcasecmp(cmdName, "debugStatus") == 0)
    {
        // Get debug status
        return apiDebuggerStatus(pCmdJson, pParams, paramsLen, pRespJson, maxRespLen);
    }
    else if (strcasecmp(cmdName, "debugRegsFormatted") == 0)
    {
        // Get registers
        return apiDebuggerRegsFormatted(pCmdJson, pParams, paramsLen, pRespJson, maxRespLen);
    }
    else if (strcasecmp(cmdName, "debugRegsJSON") == 0)
    {
        // Get registers
        return apiDebuggerRegsJSON(pCmdJson, pParams, paramsLen, pRespJson, maxRespLen);
    }
    else if (strcasecmp(cmdName, "rawBusControlOn") == 0)
    {
        // busControl.rawBusControlEnable(true);
        // busLinesRead(pRespJson, maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "rawBusControlOff") == 0)
    {
        // busControl.rawBusControlEnable(false);
        // busLinesRead(pRespJson, maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "rawBusWaitClear") == 0)
    {
        // busControl.rawBusControlClearWait();
        // busLinesRead(pRespJson, maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "rawBusWaitDisable") == 0)
    {
        // busControl.rawBusControlWaitDisable();
        return true;
    }
    else if (strcasecmp(cmdName, "rawBusClockEnable") == 0)
    {
        // busControl.rawBusControlClockEnable(true);
        // busLinesRead(pRespJson, maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "rawBusClockDisable") == 0)
    {
        // busControl.rawBusControlClockEnable(false);
        // busLinesRead(pRespJson, maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "rawBusTake") == 0)
    {
        // busControl.rawBusControlTakeBus();
        // busLinesRead(pRespJson, maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "rawBusRelease") == 0)
    {
        // busControl.rawBusControlReleaseBus();
        // busLinesRead(pRespJson, maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "rawBusSetAddress") == 0)
    {
#ifdef DEBUG_API_DETAIL
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "rawBusAddress %s", pCmdJson);
#endif
        // // Get address
        // uint32_t addr = 0;
        // if (!getArg("addr", 1, pCmdJson, addr))
        // {
        //     strlcpy(pRespJson, "\"rslt\":\"InvArgs\"", maxRespLen);
        //     return true;
        // }
        // busControl.rawBusControlSetAddress(addr);
        // // LogWrite(MODULE_PREFIX, LOG_DEBUG, "rawBusAddress addr %04x", addr);
        // busLinesRead(pRespJson, maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "rawBusGetData") == 0)
    {
        // busControl.rawBusControlReadPIB();
        // busLinesRead(pRespJson, maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "rawBusSetData") == 0)
    {
        // // Get data
        // uint32_t data = 0;
        // if (!getArg("data", 1, pCmdJson, data))
        // {
        //     strlcpy(pRespJson, "\"rslt\":\"InvArgs\"", maxRespLen);
        //     return true;
        // }
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "SET data %02x", data);
        // busControl.rawBusControlSetData(data);
        // busLinesRead(pRespJson, maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "rawBusWritePIB") == 0)
    {
        // // Get data
        // uint32_t data = 0;
        // if (!getArg("data", 1, pCmdJson, data))
        // {
        //     strlcpy(pRespJson, "\"rslt\":\"InvArgs\"", maxRespLen);
        //     return true;
        // }
        // busControl.rawBusControlWritePIB(data);
        // busLinesRead(pRespJson, maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "rawBusSetPin") == 0)
    {
        // // Get pin and value
        // uint32_t pin = 0;
        // if (!getArg("pin", 1, pCmdJson, pin, NULL, 0, true))
        // {
        //     strlcpy(pRespJson, "\"rslt\":\"InvArgs\"", maxRespLen);
        //     return true;
        // }
        // uint32_t value = 0;
        // if (!getArg("value", 2, pCmdJson, value))
        // {
        //     strlcpy(pRespJson, "\"rslt\":\"InvArgs\"", maxRespLen);
        //     return true;
        // }
        // busControl.rawBusControlSetPin(pin, value);
        // busLinesRead(pRespJson, maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "rawBusGetPin") == 0)
    {
        // // Get pin and value
        // uint32_t pin = 0;
        // if (!getArg("pin", 1, pCmdJson, pin, NULL, 0, true))
        // {
        //     strlcpy(pRespJson, "\"rslt\":\"InvArgs\"", maxRespLen);
        //     return true;
        // }
        // busControl.rawBusControlGetPin(pin);
        // busLinesRead(pRespJson, maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "rawBusGetLines") == 0)
    {
        // busLinesRead(pRespJson, maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "rawBusSetLine") == 0)
    {
        // // Handle it
        // busLineHandler(pCmdJson);
        // busLinesRead(pRespJson, maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "rawBusControlMuxSet") == 0)
    {
        // // Handle it
        // muxLineHandler(pCmdJson);
        // busLinesRead(pRespJson, maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "rawBusMuxClear") == 0)
    {
        // busControl.rawBusControlMuxClear();
        // busLinesRead(pRespJson, maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "busStatus") == 0)
    {
        // // Get bus status
        // BusAccessStats statusInfo;
        // busControl.getStatus(statusInfo);
        // strlcpy(pRespJson, statusInfo.getJson(), maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "busStatusClear") == 0)
    {
        // // Clear bus status
        // busControl.clearStatus();
        return true;
    }
    else if (strcasecmp(cmdName, "busInit") == 0)
    {
        // // Reinit bus
        // busControl.busAccessReinit();
        return true;
    }
    else if (strcasecmp(cmdName, "targetReset") == 0)
    {
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "Target Reset");
        // // Get bus status
        // busControl.targetReqReset(_busSocketId);
        return true;
    }
    else if (strcasecmp(cmdName, "targetBusReq") == 0)
    {
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "Target Bus Req");
        // // Get bus status
        // busControl.targetReqBus(_busSocketId, BR_BUS_REQ_REASON_GENERAL);
        return true;
    }
    else if (strcasecmp(cmdName, "targetBusRel") == 0)
    {
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "Target Bus Req");
        // // Get bus status
        // busControl.targetReqBus(_busSocketId, BR_BUS_REQ_REASON_GENERAL);
        return true;
    }
    else if (strcasecmp(cmdName, "clockHzGet") == 0)
    {
        // Get clock
        uint32_t actualHz = _mcManager.getClockFreqInHz();
        snprintf(pRespJson, maxRespLen, R"("rslt":"ok","clockHz":"%d")", (unsigned)actualHz);
#ifdef DEBUG_API_DETAIL
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "clockHzGet %s", pRespJson);
#endif
        return true;
    }
    else if (strcasecmp(cmdName, "clockHzSet") == 0)
    {
#ifdef DEBUG_API_DETAIL
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "clockHzSet cmd %s params %s %02x %02x %02x %02x", pCmdJson, pParams, pParams[0], pParams[1], pParams[2], pParams[3]);
#endif
        // Get clock rate required (may be in cmdJson or paramsJson)
        static const int MAX_CMD_PARAM_STR = 50;
        char paramVal[MAX_CMD_PARAM_STR+1];
        if (!jsonGetValueForKey("clockHz", pCmdJson, paramVal, MAX_CMD_PARAM_STR))
            if (!jsonGetValueForKey("clockHz", (const char*)pParams, paramVal, MAX_CMD_PARAM_STR))
            {
                LogWrite(MODULE_PREFIX, LOG_WARNING, "clockHzSet clockHz param not found");
                return false;
            }
        uint32_t clockRateHz = strtoul(paramVal, NULL, 10);
#ifdef DEBUG_API_DETAIL
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "clockHzSet %d", clockRateHz);
#endif
        // Set clock
        _mcManager.setClockFreqInHz(clockRateHz);

        // Get clock
        uint32_t actualHz = _mcManager.getClockFreqInHz();
        snprintf(pRespJson, maxRespLen, R"("rslt":"ok","clockHz":"%d")", (unsigned)actualHz);
        return true;
    }
    else if (strcasecmp(cmdName, "waitHoldOn") == 0)
    {
        // // Hold
        // busControl.waitHold(_busSocketId, true);
        return true;
    }
    else if (strcasecmp(cmdName, "waitHoldOff") == 0)
    {
        // // Hold
        // busControl.waitHold(_busSocketId, false);
        return true;
    }
    else if (strcasecmp(cmdName, "waitRelease") == 0)
    {
        // // Release
        // busControl.waitRelease();
        return true;
    }
    else if (strcasecmp(cmdName, "waitMemoryOn") == 0)
    {
        // // Wait on memory
        // busControl.waitOnMemory(_busSocketId, true);
        return true;
    }
    else if (strcasecmp(cmdName, "waitMemoryOff") == 0)
    {
        // // Wait on memory
        // busControl.waitOnMemory(_busSocketId, false);
        return true;
    }
    else if (strcasecmp(cmdName, "waitIOOn") == 0)
    {
        // // Wait on IO
        // busControl.waitOnIO(_busSocketId, true);
        return true;
    }
    else if (strcasecmp(cmdName, "waitIOOff") == 0)
    {
        // // Wait on IO
        // busControl.waitOnIO(_busSocketId, false);
        return true;
    }
    else if (strcasecmp(cmdName, "targetTrackerOn") == 0)
    {
#ifdef USE_TARGET_TRACKER
        // Get clock rate required
        static const int MAX_CMD_PARAM_STR = 50;
        char paramVal[MAX_CMD_PARAM_STR+1];
        bool reset = true;
        if (jsonGetValueForKey("reset", pCmdJson, paramVal, MAX_CMD_PARAM_STR))
        {
            reset = strtol(paramVal, NULL, 10) != 0;
        }
        // Turn target tracker on
        targetTracker.enable(true, false);
        if (reset)
        {
            targetTracker.targetReset();
            _targetTrackerResetPending = true;
        }
        strlcpy(pRespJson, R"("rslt":"ok")", maxRespLen);
#endif
        return true;
    }
    else if (strcasecmp(cmdName, "targetTrackerOff") == 0)
    {
#ifdef USE_TARGET_TRACKER
        // Turn target tracker on
        targetTracker.enable(false, false);
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "TargettrackerOff");
        strlcpy(pRespJson, R"("rslt":"ok")", maxRespLen);
#endif
        return true;
    }
    else if (strcasecmp(cmdName, "stepInto") == 0)
    {
#ifdef USE_TARGET_TRACKER
        // Turn target tracker on
        targetTracker.stepInto();
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "TargettrackerStep %s", pCmdJson);
        strlcpy(pRespJson, R"("rslt":"ok")", maxRespLen);
        _stepCompletionPending = true;
#endif
        return true;
    }
    else if (strcasecmp(cmdName, "stepRun") == 0)
    {
        // Turn target tracker on
#ifdef USE_TARGET_TRACKER
        targetTracker.stepRun();
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "TargettrackerRun");
        strlcpy(pRespJson, R"("rslt":"ok")", maxRespLen);
#endif
        return true;
    }
    else if (strcasecmp(cmdName, "waitCycleUs") == 0)
    {
        // // Get params
        // static const int MAX_CMD_PARAM_STR = 50;
        // char paramVal[MAX_CMD_PARAM_STR+1];
        // if (!jsonGetValueForKey("cycleUs", pCmdJson, paramVal, MAX_CMD_PARAM_STR))
        //     return false;
        // // Wait cycles
        // busControl.waitSetCycleUs(strtoul(paramVal, NULL, 10));
        // // LogWrite(MODULE_PREFIX, LOG_DEBUG, "Wait cycle set to %d", strtoul(paramVal, NULL, 10));
        // strlcpy(pRespJson, "\"rslt\":\"ok\"", maxRespLen);
        return true;
    }

    // Unknown command
    strlcpy(pRespJson, R"("rslt":"unknownCmd")", maxRespLen);

    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers and handlers
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ControlAPI::getArg(const char* argName, int argNum, const char* pCmdJson, uint32_t& value, 
            char* pOutStr, uint32_t maxOutStrLen, bool forceDecimal)
{
    // Get from request string if possible
    static const int MAX_CMD_REQ_STR = 200;
    char reqStr[MAX_CMD_REQ_STR+1];
    if (jsonGetValueForKey("reqStr", pCmdJson, reqStr, MAX_CMD_REQ_STR))
    {
        const char* pStr = reqStr;
        if (*pStr == '/')
            pStr++;
        // See if there are enough slashes
        for (int i = 0; i < argNum+1; i++)
        {
            pStr = strstr(pStr, "/");
            if (!pStr)
                break;
            pStr += 1;
        }
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "argName %s, argNum %d, reqStr %s, strPtr %u slashPtr %u slashStr %s", 
        //             argName, argNum, reqStr, reqStr, pStr, pStr ? pStr : "NULL");
        if (pStr)
        {
            // Extract hex number from here
            if (strlen(pStr) > 0)
            {
                if (pOutStr)
                    strlcpy(pOutStr, pStr, maxOutStrLen);
                if (forceDecimal)
                    value = strtoul(pStr, NULL, 10);
                else
                    value = strtoul(pStr, NULL, 16);
                return true;
            }
        }
    }
    // Get from params
    static const int MAX_CMD_PARAM_STR = 50;
    char paramVal[MAX_CMD_PARAM_STR+1];
    char argNameDec[MAX_CMD_PARAM_STR+1];
    strlcpy(argNameDec, argName, MAX_CMD_PARAM_STR);
    strlcat(argNameDec, "Dec", MAX_CMD_PARAM_STR);
    if (jsonGetValueForKey(argName, pCmdJson, paramVal, MAX_CMD_PARAM_STR))
    {
        if (pOutStr)
            strlcpy(pOutStr, paramVal, maxOutStrLen);
        if (forceDecimal)
            value = strtoul(paramVal, NULL, 10);
        else
            value = strtoul(paramVal, NULL, 16);
        return true;
    }
    if (jsonGetValueForKey(argNameDec, pCmdJson, paramVal, MAX_CMD_PARAM_STR))
    {
        if (pOutStr)
            strlcpy(pOutStr, paramVal, maxOutStrLen);
        value = strtoul(paramVal, NULL, 10);
        return true;
    }
    return false;
}

bool ControlAPI::busLineHandler(const char* pCmdJson)
{
    // static const int MAX_CMD_PARAM_STR = 50;
    // uint32_t lineNum_DoNotUse;
    // char lineName[MAX_CMD_PARAM_STR];
    // if (!getArg("line", 1, pCmdJson, lineNum_DoNotUse, lineName, MAX_CMD_PARAM_STR))
    // {
    //     return false;
    // }
    // char* pStrEnd = strstr(lineName, "/");
    // if (pStrEnd)
    //     *pStrEnd = 0;
    // uint32_t busValue = 0;
    // if (!getArg("value", 2, pCmdJson, busValue))
    // {
    //     return false;
    // }
    // LogWrite(MODULE_PREFIX, LOG_DEBUG, "bussetline %s %d", lineName, busValue);
    // if (strcasecmp(lineName, "WR") == 0)
    // {
    //     busControl.rawBusControlSetPin(BR_WR_BAR, busValue);
    // }
    // else if (strcasecmp(lineName, "RD") == 0)
    // {
    //     busControl.rawBusControlSetPin(BR_RD_BAR, busValue);
    // }
    // else if (strcasecmp(lineName, "MREQ") == 0)
    // {
    //     busControl.rawBusControlSetPin(BR_MREQ_BAR, busValue);
    // }
    // else if (strcasecmp(lineName, "IORQ") == 0)
    // {
    //     busControl.rawBusControlSetPin(BR_IORQ_BAR, busValue);
    // }
    // else if (strcasecmp(lineName, "PAGE") == 0)
    // {
    //     busControl.rawBusControlSetPin(BR_IORQ_BAR, busValue);
    // }
    // else if (strcasecmp(lineName, "DATA_DIR_IN") == 0)
    // {
    //     busControl.rawBusControlSetPin(BR_DATA_DIR_IN, busValue);
    // }
    // else if (strcasecmp(lineName, "IORQ_WAIT_EN") == 0)
    // {
    //     busControl.rawBusControlSetPin(BR_IORQ_WAIT_EN, busValue);
    // }
    // else if (strcasecmp(lineName, "MREQ_WAIT_EN") == 0)
    // {
    //     busControl.rawBusControlSetPin(BR_MREQ_WAIT_EN, busValue);
    // }
    // else if (strcasecmp(lineName, "HADDR_CK") == 0)
    // {
    //     busControl.rawBusControlSetPin(BR_HADDR_CK, busValue);
    // }
    // else if (strcasecmp(lineName, "PUSH_ADDR") == 0)
    // {
    //     if (busControl.getHwVersion() == 17)
    //         busControl.rawBusControlSetPin(BR_V17_PUSH_ADDR_BAR, busValue);
    // }
    // else if (strcasecmp(lineName, "CLOCK") == 0)
    // {
    //     busControl.rawBusControlSetPin(BR_CLOCK_PIN, busValue);
    // }
    // else if (strcasecmp(lineName, "M1") == 0)
    // {
    //     busControl.rawBusControlSetPin(BR_V20_M1_BAR, busValue);
    // }
    return true;
}

bool ControlAPI::muxLineHandler(const char* pCmdJson)
{
    // static const int MAX_CMD_PARAM_STR = 50;
    // uint32_t lineNum_DoNotUse;
    // char lineName[MAX_CMD_PARAM_STR];
    // if (!getArg("line", 1, pCmdJson, lineNum_DoNotUse, lineName, MAX_CMD_PARAM_STR))
    // {
    //     return false;
    // }
    // char* pStrEnd = strstr(lineName, "/");
    // if (pStrEnd)
    //     *pStrEnd = 0;
    // LogWrite(MODULE_PREFIX, LOG_DEBUG, "muxSet %s", lineName);
    // if (strcasecmp(lineName, "LADDR_CLK") == 0)
    // {
    //     busControl.rawBusControlMuxSet(BR_MUX_LADDR_CLK);
    // }
    // else if (strcasecmp(lineName, "LADDR_CLR_BAR") == 0)
    // {
    //     busControl.rawBusControlMuxSet(BR_MUX_LADDR_CLR_BAR_LOW);
    // }
    // else if (strcasecmp(lineName, "DATA_OE_BAR") == 0)
    // {
    //     busControl.rawBusControlMuxSet(BR_MUX_DATA_OE_BAR_LOW);
    // }
    // else if (strcasecmp(lineName, "RESET") == 0)
    // {
    //     busControl.rawBusControlMuxSet(BR_MUX_RESET_Z80_BAR_LOW);
    // }
    // else if (strcasecmp(lineName, "IRQ") == 0)
    // {
    //     busControl.rawBusControlMuxSet(BR_MUX_IRQ_BAR_LOW);
    // }
    // else if (strcasecmp(lineName, "NMI") == 0)
    // {
    //     busControl.rawBusControlMuxSet(BR_MUX_NMI_BAR_LOW);
    // }
    // else if (strcasecmp(lineName, "LADDR_OE_BAR") == 0)
    // {
    //     busControl.rawBusControlMuxSet(BR_MUX_LADDR_OE_BAR);
    // }
    // else if (strcasecmp(lineName, "HADDR_OE_BAR") == 0)
    // {
    //     busControl.rawBusControlMuxSet(BR_MUX_HADDR_OE_BAR);
    // }
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Callbacks/Hooks
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ControlAPI::busReqAckedStatic(void* pObject, BR_BUS_REQ_REASON reason, BR_RETURN_TYPE rslt)
{
    if (!pObject)
        return;
    ((ControlAPI*)pObject)->busReqAcked(reason, rslt);
}

void ControlAPI::busReqAcked(BR_BUS_REQ_REASON reason, BR_RETURN_TYPE rslt)
{
// #ifdef DEBUG_BUS_ACTION_COMPLETE
//     LogWrite(MODULE_PREFIX, LOG_DEBUG, "busReqAcked type %d reason %d rslt %d", 
//                 actionType, reason, rslt);
// #endif

//     HwManager& hwManager = _hwManager;
//     if ((actionType == BR_BUS_ACTION_BUSRQ) && _memAccessPending)
//     {
//         if (rslt == BR_OK)
//         {
//             if (!_memAccessWrite)
//             {
//                 // Read
//                 hwManager.blockRead(_memAccessAddr, _memAccessDataBuf, 
//                             _memAccessDataLen, false, _memAccessIo, false);
//             }
//             else
//             {
//                 // Write
//                 hwManager.blockWrite(_memAccessAddr, _memAccessDataBuf, 
//                             _memAccessDataLen, false, _memAccessIo, false);
//             }
//         }
//         _memAccessPending = false;
//     }
}

void ControlAPI::handleWaitInterruptStatic(void* pObject, uint32_t addr, uint32_t data, 
        uint32_t flags, uint32_t& retVal)
{
    if (!pObject)
        return;
    ((ControlAPI*)pObject)->handleWaitInterrupt(addr, data, flags, retVal);
}

void ControlAPI::handleWaitInterrupt( uint32_t addr,  uint32_t data, 
         uint32_t flags,  uint32_t& retVal)
{
}

void ControlAPI::service()
{
#ifdef USE_TARGET_TRACKER
    // Check for targettracker enable message response
    TargetController& targetTracker = _mcManager.getTargetTracker();
    if (_targetTrackerResetPending && targetTracker.isStepPaused())
    {
        _commandHandler.sendUnnumberedMsg("targetTrackerOnDone", "\"rslt\":\"ok\"");
        _targetTrackerResetPending = false;
    }
    // Check for step completion message response
    if (_stepCompletionPending && targetTracker.isStepPaused())
    {
        _commandHandler.sendUnnumberedMsg("stepIntoDone", "\"rslt\":\"ok\"");
        _stepCompletionPending = false;
    }
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// API
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ControlAPI::apiImagerClear(char* pRespJson, unsigned maxRespLen)
{
    // LogWrite(MODULE_PREFIX, LOG_VERBOSE, "imagerClear");
    _busControl.imager().clear();
    return true;
}

bool ControlAPI::apiImagerAddMemBlock(const char* pCmdJson, 
            const uint8_t* pParams, unsigned paramsLen,
            char* pRespJson, unsigned maxRespLen)
{
    // Get params
    uint32_t addr = 0;
    if (!getArg("addr", 1, pCmdJson, addr))
    {
        strlcpy(pRespJson, R"("rslt":"InvArgAddr")", maxRespLen);
        return true;
    }
#ifdef DEBUG_API_PROGRAMMER
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "imagerAdd addr 0x%04x len %d", addr, paramsLen);
#endif
    _busControl.imager().addMemoryBlock(addr, pParams, paramsLen);
    return true;
}

bool ControlAPI::apiImagerWrite(const char* pCmdJson, 
            const uint8_t* pParams, unsigned paramsLen,
            char* pRespJson, unsigned maxRespLen)
{
    // Get params
    uint32_t doExec = false;
    uint32_t doDebug = false;
    getArg("exec", 1, pCmdJson, doExec);
    getArg("debug", 2, pCmdJson, doDebug);
#ifdef DEBUG_API_PROGRAMMER
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "apiImagerWrite exec %d debug %d", doExec, doDebug);
#endif
    _busControl.ctrl().programmingStart(doExec, doDebug);
    return true;
}

bool ControlAPI::apiImagerWriteAndExec(char* pRespJson, unsigned maxRespLen)
{
    _busControl.ctrl().programmingStart(true, false);
    return true;
}

bool ControlAPI::apiTargetReset(char* pRespJson, unsigned maxRespLen)
{
#ifdef DEBUG_API_DETAIL
    LogWrite(MODULE_PREFIX, LOG_VERBOSE, "ResetTarget");
#endif
    _busControl.ctrl().targetReset();
    return true;
}

bool ControlAPI::apiFileToTarget(const char* pCmdJson, 
            const uint8_t* pParams, unsigned paramsLen,
            char* pRespJson, unsigned maxRespLen)
{
#ifdef DEBUG_API_DETAIL
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "apiFileToTarget len %d json %s", paramsLen, pCmdJson);
#endif
    bool rslt = _mcManager.targetFileHandler(pCmdJson, pParams, paramsLen);
    if (!rslt)
        strlcpy(pRespJson, R"("rslt":"fail")", maxRespLen);
    return true;
}

bool ControlAPI::apiSetMcJson(const char* pCmdJson, 
            const uint8_t* pParams, unsigned paramsLen,
            char* pRespJson, unsigned maxRespLen)
{
    // Extract mcJson
    static char mcJson[_commandHandler.MAX_MC_SET_JSON_LEN];
    size_t toCopy = paramsLen+1;
    if (toCopy > _commandHandler.MAX_MC_SET_JSON_LEN)
        toCopy = _commandHandler.MAX_MC_SET_JSON_LEN;
    strlcpy(mcJson, (const char*)pParams, toCopy);
#ifdef DEBUG_API_DETAIL
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "apiSetMcJson %s", mcJson);
#endif
    bool setupOk = _mcManager.setupMachine(mcJson); 
    if (!setupOk)
        strlcpy(pRespJson, R"("rslt":"fail")", maxRespLen);
    return true;
}

bool ControlAPI::apiSendKeyToTarget(const char* pCmdJson, 
            const uint8_t* pParams, unsigned paramsLen,
            char* pRespJson, unsigned maxRespLen)
{
    char* pEnd = NULL;
    uint8_t usbKeyCodes[_commandHandler.NUM_USB_KEYS_PASSED];
    memset(usbKeyCodes, 0, sizeof(usbKeyCodes));
    unsigned asciiCode = strtoul((const char*)pParams, &pEnd, 10);
    usbKeyCodes[0] = strtoul(pEnd, &pEnd, 10);
    unsigned usbModCode = strtoul(pEnd, &pEnd, 10);
    asciiCode = asciiCode;
#ifdef DEBUG_API_DETAIL
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "SendKey, %s ascii 0x%02x usbKey 0x%02x usbMod 0x%02x", 
                pParams, asciiCode, usbKeyCodes[0], usbModCode);
#endif
    _mcManager.keyHandler(usbModCode, usbKeyCodes);
    return true;
}

bool ControlAPI::apiReadFromTarget(const char* pCmdJson, 
            const uint8_t* pParams, unsigned paramsLen,
            char* pRespJson, unsigned maxRespLen)
{
#ifdef DEBUG_API_DETAIL
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "apiReadFromTarget %s", pCmdJson);
#endif

    // Get params
    uint32_t addr = 0;
    uint32_t dataLen = 0;
    uint32_t isIo = false;
    if (!getArg("addr", 1, pCmdJson, addr))
    {
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "apiReadFromTarget %s addr not present", pCmdJson);
        strlcpy(pRespJson, R"("rslt":"InvArgAddr")", maxRespLen);
        return true;
    }
    if (!getArg("len", 2, pCmdJson, dataLen))
    {
        strlcpy(pRespJson, R"("rslt":"InvArgLen")", maxRespLen);
        return true;
    }
    getArg("isIo", 3, pCmdJson, isIo);

    // Get data using bus access
    if ((dataLen <= 0) || (dataLen > MAX_MEM_BLOCK_READ_WRITE))
    {
        strlcpy(pRespJson, R"("rslt":"LenTooLong")", maxRespLen);
        return true;
    }
    // Synchronous memory read
    uint8_t pData[MAX_MEM_BLOCK_READ_WRITE];
    BR_RETURN_TYPE rslt = _busControl.blockAccessSync(addr, pData, dataLen, isIo, true, false);
    if (rslt != BR_OK)
    {
        snprintf(pRespJson, maxRespLen, R"("rslt":"%s")", _busControl.retcString(rslt));
        return true;
    }

    // Format data to reply with
    static const int MAX_MEM_READ_RESP = MAX_MEM_BLOCK_READ_WRITE*2+100; 
    char jsonResp[MAX_MEM_READ_RESP];
    snprintf(jsonResp, sizeof(jsonResp), R"("rslt":"ok","len":%d,"addr":"0x%04x","isIo":%d,"data":")", 
                (int)dataLen, (unsigned int)addr, isIo);
    int pos = strlen(jsonResp);
    for (uint32_t i = 0; i < dataLen; i++)
    {
        snprintf(jsonResp+pos, sizeof(jsonResp), "%02x", pData[i]);
        pos+=2;
    }
    strlcat(jsonResp, R"(")", MAX_MEM_READ_RESP);
    strlcpy(pRespJson, jsonResp, maxRespLen);

#ifdef DEBUG_API_DETAIL
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "apiReadFromTarget result ok, lastByte 0x%c%c", 
                jsonResp[strlen(jsonResp)-3], jsonResp[strlen(jsonResp)-2]);
#endif
    return true;
}

bool ControlAPI::apiWriteToTarget(const char* pCmdJson, 
            const uint8_t* pParams, unsigned paramsLen,
            char* pRespJson, unsigned maxRespLen)
{
#ifdef DEBUG_API_DETAIL
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "apiWriteToTarget %s", pCmdJson);
#endif 

    // Get params
    uint32_t addr = 0;
    uint32_t dataLen = 0;
    uint32_t isIo = false;
    if (!getArg("addr", 1, pCmdJson, addr))
    {
        strlcpy(pRespJson, R"("rslt":"InvArgAddr")", maxRespLen);
        return true;
    }
    if (!getArg("len", 2, pCmdJson, dataLen))
    {
        strlcpy(pRespJson, R"("rslt":"InvArgData")", maxRespLen);
        return true;
    }
    getArg("isIo", 3, pCmdJson, isIo);
    // Get data using bus access
    if ((dataLen <= 0) || (dataLen > MAX_MEM_BLOCK_READ_WRITE) || (dataLen > (uint32_t)paramsLen))
    {
        strlcpy(pRespJson, R"("rslt":"LenTooLong")", maxRespLen);
        return true;
    }
    // Synchronous memory write
    BR_RETURN_TYPE rslt = _busControl.blockAccessSync(addr, const_cast<uint8_t*>(pParams), 
                    dataLen, isIo, true, false);
    if (rslt != BR_OK)
    {
        LogWrite(MODULE_PREFIX, LOG_WARNING, "apiWriteToTarget failed %s", _busControl.retcString(rslt));
        snprintf(pRespJson, maxRespLen, R"("rslt":"%s")", _busControl.retcString(rslt));
        return true;
    }
    strlcpy(pRespJson, R"("rslt":"ok")", maxRespLen);
    return true;
}

bool ControlAPI::apiWriteReadTarget(const char* pCmdJson, 
            const uint8_t* pParams, unsigned paramsLen,
            char* pRespJson, unsigned maxRespLen)
{
#ifdef DEBUG_API_DETAIL
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "apiWriteReadTarget %s", pCmdJson);
#endif        
        // Get params
        uint32_t addr = 0;
        uint32_t dataLen = 0;
        uint32_t isIo = false;
        if (!getArg("addr", 1, pCmdJson, addr))
        {
            strlcpy(pRespJson, R"("rslt":"InvArgAddr")", maxRespLen);
            return true;
        }
        if (!getArg("len", 2, pCmdJson, dataLen))
        {
            strlcpy(pRespJson, R"("rslt":"InvArgData")", maxRespLen);
            return true;
        }
        getArg("isIo", 3, pCmdJson, isIo);
        // Get data using bus access
        if ((dataLen <= 0) || (dataLen > MAX_MEM_BLOCK_READ_WRITE) || (dataLen > (uint32_t)paramsLen))
        {
            strlcpy(pRespJson, R"("rslt":"LenTooLong")", maxRespLen);
            return true;
        }
        // Synchronous memory write and then read
        uint8_t wrAndRdData[dataLen];
        memcpy(wrAndRdData, pParams, dataLen);
        BR_RETURN_TYPE rslt = _busControl.blockAccessSync(addr, wrAndRdData, dataLen, isIo, true, true);
        if (rslt != BR_OK)
        {
            snprintf(pRespJson, maxRespLen, R"("rslt":"%s")", _busControl.retcString(rslt));
            return true;
        }
        // Format data to reply with
        static const int MAX_MEM_READ_RESP = MAX_MEM_BLOCK_READ_WRITE*2+100; 
        char jsonResp[MAX_MEM_READ_RESP];
        snprintf(jsonResp, sizeof(jsonResp), R"("rslt":"ok","len":%d,"addr":"0x%04x","isIo":%d,"data":")", 
                    (int)dataLen, (unsigned int)addr, isIo);
        int pos = strlen(jsonResp);
        for (uint32_t i = 0; i < dataLen; i++)
        {
            snprintf(jsonResp+pos, sizeof(jsonResp), "%02x", wrAndRdData[i]);
            pos+=2;
        }
        strlcat(jsonResp, R"(")", MAX_MEM_READ_RESP);
        strlcpy(pRespJson, jsonResp, maxRespLen);

#ifdef DEBUG_API_DETAIL
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "apiWriteReadTarget result ok, lastByte 0x%c%c", 
                jsonResp[strlen(jsonResp)-3], jsonResp[strlen(jsonResp)-2]);
#endif
        //         if (_pThisInstance->_memAccessReadPending)
        // {
        //     _pThisInstance->_memAccessRdWrPending = false;
        //     for (int i = 0; i < 1000; i++)
        //     {
        //         uint8_t writeBuf[MAX_MEM_BLOCK_READ_WRITE];
        //         for (int j = 0; j < MAX_MEM_BLOCK_READ_WRITE; j++)
        //             writeBuf[j] = rand() & 0xff;
        //         busControl.blockWrite(0x3c00, writeBuf, MAX_MEM_BLOCK_READ_WRITE, false, false);

        //         uint8_t readBuf[MAX_MEM_BLOCK_READ_WRITE];
        //         busControl.blockRead(0x3c00, readBuf, MAX_MEM_BLOCK_READ_WRITE, false, false);

        //         if (memcmp(writeBuf, readBuf, MAX_MEM_BLOCK_READ_WRITE) != 0)
        //         {
        //             if (_pThisInstance->_memAccessRdWrErrCount == 0)
        //             {
        //                 int errPos = 0;
        //                 for (int k = 0; k < MAX_MEM_BLOCK_READ_WRITE; k++)
        //                 {
        //                     if (writeBuf[k] != readBuf[k])
        //                     {
        //                         errPos = k;
        //                         break;
        //                     }
        //                 }
        //                 // digitalWrite(8, 1);
        //                 // microsDelay(100);
        //                 // digitalWrite(8, 0);
        //                 ee_sprintf(_pThisInstance->_memAccessRdWrErrStr, "POS %d WR %02x %02x %02x %02x RD %02x %02x %02x %02x", 
        //                             errPos,
        //                             writeBuf[errPos], writeBuf[errPos+1], writeBuf[errPos+2], writeBuf[errPos+3],
        //                             readBuf[errPos+0], readBuf[errPos+1], readBuf[errPos+2], readBuf[errPos+3]);
        //             }
        //             _pThisInstance->_memAccessRdWrErrCount++;
        //         }
        //     }
        // }

        // // Request the bus
        // _pThisInstance->_memAccessRdWrPending = true;
        // _pThisInstance->_memAccessDataLen = MAX_MEM_BLOCK_READ_WRITE;
        // _pThisInstance->_memAccessAddr = 0xc000;
        // _pThisInstance->_memAccessIo = false;
        // _pThisInstance->_memAccessRdWrErrCount = 0;
        // _pThisInstance->_memAccessRdWrErrStr[0] = 0;
        // busControl.targetReqBus(_busSocketId, BR_BUS_REQ_REASON_GENERAL);
        // // Now enter a loop to wait for the bus action to complete
        // static const uint32_t MAX_WAIT_FOR_BUS_ACCESS_US = 50000;
        // uint32_t busAccessReqStart = micros();
        // while(!isTimeout(micros(), busAccessReqStart, MAX_WAIT_FOR_BUS_ACCESS_US))
        // {
        //     // Finished?
        //     if (!_pThisInstance->_memAccessRdWrPending)
        //                     break;
        //     // Service the bus access
        //     busControl.service();
        //                 }
        // // Check if completed ok
        // if (_pThisInstance->_memAccessRdWrPending)
        // {
        //     _pThisInstance->_memAccessRdWrPending = false;
        //     ee_sprintf(pRespJson, "\"rslt\":\"fail\"");
        //     // digitalWrite(8,1);
        //             // microsDelay(100);
        //     // digitalWrite(8,0);
        //     return true;
        //     }
        // if (maxRespLen > 200)
        // {
        //     ee_sprintf(pRespJson, "\"rslt\":\"%s\",\"errs\":%d,\"errStr\":\"%s\"",
        //             _pThisInstance->_memAccessRdWrErrCount > 0 ? "fail" : "ok", 
        //             _pThisInstance->_memAccessRdWrErrCount, 
        //             _pThisInstance->_memAccessRdWrErrStr);
        //     LogWrite(MODULE_PREFIX, LOG_DEBUG, pRespJson);
        // }
        // strlcpy(pRespJson, "\"rslt\":\"ok\"", maxRespLen);
        return true;
}

bool ControlAPI::apiDebuggerBreak(const char* pCmdJson, 
            const uint8_t* pParams, unsigned paramsLen,
            char* pRespJson, unsigned maxRespLen)
{
#ifdef DEBUG_API_DEBUGGER
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "apiDebuggerBreak %s", pCmdJson);
#endif
    if (!_busControl.ctrl().debuggerBreak())
    {
        strlcpy(pRespJson, R"("rslt":"failedToBreak")", maxRespLen);
    }
    else
    {
        formDebuggerResponse(pCmdJson, pParams, paramsLen, pRespJson, maxRespLen);
    }
    return true;
}

bool ControlAPI::apiDebuggerContinue(const char* pCmdJson, 
            const uint8_t* pParams, unsigned paramsLen,
            char* pRespJson, unsigned maxRespLen)
{
#ifdef DEBUG_API_DEBUGGER
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "apiDebuggerContinue %s", pCmdJson);
#endif
    _busControl.ctrl().debuggerContinue();
    return true;
}

bool ControlAPI::apiDebuggerStepIn(const char* pCmdJson, 
            const uint8_t* pParams, unsigned paramsLen,
            char* pRespJson, unsigned maxRespLen)
{
#ifdef DEBUG_API_DEBUGGER
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "apiDebuggerStepIn %s", pCmdJson);
#endif
    if (!_busControl.ctrl().debuggerStepIn())
    {
        strlcpy(pRespJson, R"("rslt":"failedToStep")", maxRespLen);
    }
    else
    {
        formDebuggerResponse(pCmdJson, pParams, paramsLen, pRespJson, maxRespLen);
    }

    // uint32_t startMs = millis();
    // while(!isTimeout(millis(), startMs, 10))
    // {
    //     for (uint32_t i = 0; i < 10; i++)
    //         _busControl.ctrl().service(false);
    //     if (_busControl.ctrl().isHeldAtWait())
    //         break;
    // }
    return true;
}

bool ControlAPI::apiDebuggerRegsFormatted(const char* pCmdJson, 
            const uint8_t* pParams, unsigned paramsLen,
            char* pRespJson, unsigned maxRespLen)
{
    char regsStr[200];
    _busControl.ctrl().getRegsFormatted(regsStr, sizeof(regsStr));
    strlcpy(pRespJson, R"("rslt":"ok","regs":")", maxRespLen);
    strlcat(pRespJson, regsStr, maxRespLen);
    strlcat(pRespJson, R"(")", maxRespLen);
    return true;
}

bool ControlAPI::apiDebuggerRegsJSON(const char* pCmdJson, 
            const uint8_t* pParams, unsigned paramsLen,
            char* pRespJson, unsigned maxRespLen)
{
    char regsStr[400];
    _busControl.ctrl().getRegsJSON(regsStr, sizeof(regsStr));
    strlcpy(pRespJson, R"("rslt":"ok",)", maxRespLen);
    strlcat(pRespJson, regsStr, maxRespLen);
    return true;
}

bool ControlAPI::apiDebuggerStatus(const char* pCmdJson, 
            const uint8_t* pParams, unsigned paramsLen,
            char* pRespJson, unsigned maxRespLen)
{
    snprintf(pRespJson, maxRespLen, R"("rslt":"ok","debugging":%d,"held":%d)", 
            _busControl.ctrl().isDebugging(),
            _busControl.ctrl().isHeldAtWait());
    return true;
}

bool ControlAPI::formDebuggerResponse(const char* pCmdJson, 
            const uint8_t* pParams, unsigned paramsLen,
            char* pRespJson, unsigned maxRespLen)
{
    uint32_t numLines = 15;
    getArg("numLines", 1, pCmdJson, numLines);
    apiDebuggerRegsJSON(pCmdJson, pParams, paramsLen, pRespJson, maxRespLen);
    char debuggerResp[2000];
    uint32_t numBytesIn1stInstr = 0;
    _busControl.ctrl().disassemble(numLines, debuggerResp, sizeof(debuggerResp), numBytesIn1stInstr);
    char debuggerJson[2000];
    snprintf(debuggerJson, sizeof(debuggerJson), R"(,"disasm":"%s","instrBytes":%d)", 
                debuggerResp, numBytesIn1stInstr);
    strlcat(pRespJson, debuggerJson, maxRespLen);
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "disasm %s", pRespJson);    
    return true;
}
