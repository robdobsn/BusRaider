// Bus Raider
// Rob Dobson 2019

#include "BusControlAPI.h"
#include "PiWiring.h"
#include "lowlib.h"
#include "logging.h"
#include "rdutils.h"
#include "TargetTracker.h"
#include "BusAccess.h"
#include "BusAccess.h"
#include "CommandHandler.h"
#include "HwManager.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Variables
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Module name
static const char MODULE_PREFIX[] = "BusControlAPI";

// This instance
BusControlAPI* BusControlAPI::_pThisInstance = NULL;

// Debug
// #define DEBUG_RX_FRAME
// #define DEBUG_COMMS_SOCKET
// #define DEBUG_API_BLOCK_ACCESS_SYNC
// #define DEBUG_BUS_ACTION_COMPLETE

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Constructor
BusControlAPI::BusControlAPI(CommandHandler& commandHandler, HwManager& hwManager, BusAccess& busAccess) :
        _commandHandler(commandHandler), _hwManager(hwManager), _busAccess(busAccess)
{
    _pThisInstance = this;
    // Sockets
    _busSocketId = -1;
    _commsSocketId = -1;
    // Synchronous memory access
    _memAccessPending = false;
    _memAccessWrite = false;
    _memAccessDataLen = 0;
    _memAccessAddr = 0;
    _memAccessIo = false;
    _memAccessRdWrErrCount = 0;
    _memAccessRdWrTest = false;
    _stepCompletionPending = false;
    _targetTrackerResetPending = false;
}

void BusControlAPI::init()
{
    // Connect to the bus socket
    BusAccess& busAccess = _busAccess;
    if (_busSocketId < 0)
        _busSocketId = busAccess.busSocketAdd( 
            true,
            BusControlAPI::handleWaitInterruptStatic,
            BusControlAPI::busActionCompleteStatic,
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
            BR_BUS_ACTION_GENERAL,
            false,
            this);

    // Connect to the comms socket
    if (_commsSocketId < 0)
    {
        _commsSocketId = _commandHandler.commsSocketAdd(this, true, BusControlAPI::handleRxMsgStatic, NULL, NULL);
#ifdef DEBUG_COMMS_SOCKET
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "init commsSocketId %d", _commsSocketId);
#endif
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Received message handler
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool BusControlAPI::handleRxMsgStatic(void* pObject, const char* pCmdJson, 
                const uint8_t* pParams, unsigned paramsLen,
                char* pRespJson, unsigned maxRespLen)
{
#ifdef DEBUG_RX_FRAME_STATIC
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "handleRxMsgStatic pObject %08x %s", (unsigned)pObject, pCmdJson);
#endif
    if (!pObject)
        return false;
    return ((BusControlAPI*)pObject)->handleRxMsg(pCmdJson, pParams, paramsLen, pRespJson, maxRespLen);
}

bool BusControlAPI::handleRxMsg(const char* pCmdJson, 
                const uint8_t* pParams, unsigned paramsLen,
                char* pRespJson, unsigned maxRespLen)
{

#ifdef DEBUG_RX_FRAME
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "handleRxMsg %s", pCmdJson);
#endif

    // Get the command string from JSON
    static const int MAX_CMD_NAME_STR = 50;
    char cmdName[MAX_CMD_NAME_STR+1];
    if (!jsonGetValueForKey("cmdName", pCmdJson, cmdName, MAX_CMD_NAME_STR))
        return false;

    // Check for memory/IO read
    BusAccess& busAccess = _busAccess;
    if (strcasecmp(cmdName, "Rd") == 0)
    {
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "Rd %s %04x", pCmdJson, read32(ARM_GPIO_GPLEV0));

        // Get params
        uint32_t addr = 0;
        uint32_t dataLen = 0;
        bool isIo = false;
        if (!getArg("addr", 1, pCmdJson, addr))
        {
            strlcpy(pRespJson, "\"err\":\"InvArgs\"", maxRespLen);
            return true;
        }
        if (!getArg("len", 2, pCmdJson, dataLen))
        {
            strlcpy(pRespJson, "\"err\":\"InvArgs\"", maxRespLen);
            return true;
        }
        uint32_t val = 0;
        if (!getArg("isIo", 3, pCmdJson, val))
        {
            isIo = val != 0;
            strlcpy(pRespJson, "\"err\":\"InvArgs\"", maxRespLen);
            return true;
        }
        // Get data using bus access
        if ((dataLen <= 0) || (dataLen > MAX_MEM_BLOCK_READ_WRITE))
        {
            strlcpy(pRespJson, "\"err\":\"LenTooLong\"", maxRespLen);
            return true;
        }
        // Synchronous memory read
        uint8_t pData[MAX_MEM_BLOCK_READ_WRITE];
        BR_RETURN_TYPE rslt = BusControlAPI::blockAccessSync(addr, pData, dataLen, isIo, false);
        if (rslt != BR_OK)
        {
            strlcpy(pRespJson, "\"err\":\"fail\"", maxRespLen);
            return true;
        }
        // Format data to send
        static const int MAX_MEM_READ_RESP = MAX_MEM_BLOCK_READ_WRITE*2+100; 
        char jsonResp[MAX_MEM_READ_RESP];
        snprintf(jsonResp, sizeof(jsonResp), "\"err\":\"ok\",\"len\":%d,\"addr\":\"0x%04x\",\"isIo\":%d,\"data\":\"", 
                    (int)dataLen, (unsigned int)addr, isIo);
        int pos = strlen(jsonResp);
        for (uint32_t i = 0; i < dataLen; i++)
        {
            snprintf(jsonResp+pos, sizeof(jsonResp), "%02x", pData[i]);
            pos+=2;
        }
        strlcat(jsonResp, "\"", MAX_MEM_READ_RESP);
        strlcpy(pRespJson, jsonResp, maxRespLen);

        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "Rd result ok, lastByte 0x%c%c", jsonResp[strlen(jsonResp)-3], jsonResp[strlen(jsonResp)-2]);

        return true;
    }
    else if (strcasecmp(cmdName, "Wr") == 0)
    {
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "Wr %s", pCmdJson);
        
        // Get params
        uint32_t addr = 0;
        uint32_t dataLen = 0;
        uint32_t isIo = false;
        if (!getArg("addr", 1, pCmdJson, addr))
        {
            strlcpy(pRespJson, "\"err\":\"InvArgs\"", maxRespLen);
            return true;
        }
        if (!getArg("len", 2, pCmdJson, dataLen))
        {
            strlcpy(pRespJson, "\"err\":\"InvArgs\"", maxRespLen);
            return true;
        }
        if (!getArg("isIo", 3, pCmdJson, isIo))
        {
            strlcpy(pRespJson, "\"err\":\"InvArgs\"", maxRespLen);
            return true;
        }
        // Get data using bus access
        if ((dataLen <= 0) || (dataLen > MAX_MEM_BLOCK_READ_WRITE) || (dataLen > (uint32_t)paramsLen))
        {
            strlcpy(pRespJson, "\"err\":\"LenTooLong\"", maxRespLen);
            return true;
        }
        // Synchronous memory write
        BR_RETURN_TYPE rslt = BusControlAPI::blockAccessSync(addr, const_cast<uint8_t*>(pParams), dataLen, isIo, true);
        if (rslt != BR_OK)
        {
            strlcpy(pRespJson, "\"err\":\"fail\"", maxRespLen);
            return true;
        }
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "testRdWr") == 0)
    {
        //         if (_pThisInstance->_memAccessReadPending)
        // {
        //     _pThisInstance->_memAccessRdWrPending = false;
        //     for (int i = 0; i < 1000; i++)
        //     {
        //         uint8_t writeBuf[MAX_MEM_BLOCK_READ_WRITE];
        //         for (int j = 0; j < MAX_MEM_BLOCK_READ_WRITE; j++)
        //             writeBuf[j] = rand() & 0xff;
        //         busAccess.blockWrite(0x3c00, writeBuf, MAX_MEM_BLOCK_READ_WRITE, false, false);

        //         uint8_t readBuf[MAX_MEM_BLOCK_READ_WRITE];
        //         busAccess.blockRead(0x3c00, readBuf, MAX_MEM_BLOCK_READ_WRITE, false, false);

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
        // busAccess.targetReqBus(_busSocketId, BR_BUS_ACTION_GENERAL);
        // // Now enter a loop to wait for the bus action to complete
        // static const uint32_t MAX_WAIT_FOR_BUS_ACCESS_US = 50000;
        // uint32_t busAccessReqStart = micros();
        // while(!isTimeout(micros(), busAccessReqStart, MAX_WAIT_FOR_BUS_ACCESS_US))
        // {
        //     // Finished?
        //     if (!_pThisInstance->_memAccessRdWrPending)
        //                     break;
        //     // Service the bus access
        //     busAccess.service();
        //                 }
        // // Check if completed ok
        // if (_pThisInstance->_memAccessRdWrPending)
        // {
        //     _pThisInstance->_memAccessRdWrPending = false;
        //     ee_sprintf(pRespJson, "\"err\":\"fail\"");
        //     // digitalWrite(8,1);
        //             // microsDelay(100);
        //     // digitalWrite(8,0);
        //     return true;
        //     }
        // if (maxRespLen > 200)
        // {
        //     ee_sprintf(pRespJson, "\"err\":\"%s\",\"errs\":%d,\"errStr\":\"%s\"",
        //             _pThisInstance->_memAccessRdWrErrCount > 0 ? "fail" : "ok", 
        //             _pThisInstance->_memAccessRdWrErrCount, 
        //             _pThisInstance->_memAccessRdWrErrStr);
        //     LogWrite(MODULE_PREFIX, LOG_DEBUG, pRespJson);
        // }
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "rawBusControlOn") == 0)
    {
        busAccess.rawBusControlEnable(true);
        busLinesRead(pRespJson, maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "rawBusControlOff") == 0)
    {
        busAccess.rawBusControlEnable(false);
        busLinesRead(pRespJson, maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "rawBusWaitClear") == 0)
    {
        busAccess.rawBusControlClearWait();
        busLinesRead(pRespJson, maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "rawBusWaitDisable") == 0)
    {
        busAccess.rawBusControlWaitDisable();
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "rawBusClockEnable") == 0)
    {
        busAccess.rawBusControlClockEnable(true);
        busLinesRead(pRespJson, maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "rawBusClockDisable") == 0)
    {
        busAccess.rawBusControlClockEnable(false);
        busLinesRead(pRespJson, maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "rawBusTake") == 0)
    {
        busAccess.rawBusControlTakeBus();
        busLinesRead(pRespJson, maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "rawBusRelease") == 0)
    {
        busAccess.rawBusControlReleaseBus();
        busLinesRead(pRespJson, maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "rawBusSetAddress") == 0)
    {
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "rawBusAddress %s", pCmdJson);
        // Get address
        uint32_t addr = 0;
        if (!getArg("addr", 1, pCmdJson, addr))
        {
            strlcpy(pRespJson, "\"err\":\"InvArgs\"", maxRespLen);
            return true;
        }
        busAccess.rawBusControlSetAddress(addr);
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "rawBusAddress addr %04x", addr);
        busLinesRead(pRespJson, maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "rawBusGetData") == 0)
    {
        busAccess.rawBusControlReadPIB();
        busLinesRead(pRespJson, maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "rawBusSetData") == 0)
    {
        // Get data
        uint32_t data = 0;
        if (!getArg("data", 1, pCmdJson, data))
        {
            strlcpy(pRespJson, "\"err\":\"InvArgs\"", maxRespLen);
            return true;
        }
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "SET data %02x", data);
        busAccess.rawBusControlSetData(data);
        busLinesRead(pRespJson, maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "rawBusWritePIB") == 0)
    {
        // Get data
        uint32_t data = 0;
        if (!getArg("data", 1, pCmdJson, data))
        {
            strlcpy(pRespJson, "\"err\":\"InvArgs\"", maxRespLen);
            return true;
        }
        busAccess.rawBusControlWritePIB(data);
        busLinesRead(pRespJson, maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "rawBusSetPin") == 0)
    {
        // Get pin and value
        uint32_t pin = 0;
        if (!getArg("pin", 1, pCmdJson, pin, NULL, 0, true))
        {
            strlcpy(pRespJson, "\"err\":\"InvArgs\"", maxRespLen);
            return true;
        }
        uint32_t value = 0;
        if (!getArg("value", 2, pCmdJson, value))
        {
            strlcpy(pRespJson, "\"err\":\"InvArgs\"", maxRespLen);
            return true;
        }
        busAccess.rawBusControlSetPin(pin, value);
        busLinesRead(pRespJson, maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "rawBusGetPin") == 0)
    {
        // Get pin and value
        uint32_t pin = 0;
        if (!getArg("pin", 1, pCmdJson, pin, NULL, 0, true))
        {
            strlcpy(pRespJson, "\"err\":\"InvArgs\"", maxRespLen);
            return true;
        }
        busAccess.rawBusControlGetPin(pin);
        busLinesRead(pRespJson, maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "rawBusGetLines") == 0)
    {
        busLinesRead(pRespJson, maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "rawBusSetLine") == 0)
    {
        // Handle it
        busLineHandler(pCmdJson);
        busLinesRead(pRespJson, maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "rawBusControlMuxSet") == 0)
    {
        // Handle it
        muxLineHandler(pCmdJson);
        busLinesRead(pRespJson, maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "rawBusMuxClear") == 0)
    {
        busAccess.rawBusControlMuxClear();
        busLinesRead(pRespJson, maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "busStatus") == 0)
    {
        // Get bus status
        BusAccessStatusInfo statusInfo;
        busAccess.getStatus(statusInfo);
        strlcpy(pRespJson, statusInfo.getJson(), maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "busStatusClear") == 0)
    {
        // Clear bus status
        busAccess.clearStatus();
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "busInit") == 0)
    {
        // Get bus status
        busAccess.busAccessReset();
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "targetReset") == 0)
    {
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "Target Reset");
        // Get bus status
        busAccess.targetReqReset(_busSocketId);
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "targetBusReq") == 0)
    {
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "Target Bus Req");
        // Get bus status
        busAccess.targetReqBus(_busSocketId, BR_BUS_ACTION_GENERAL);
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "targetBusRel") == 0)
    {
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "Target Bus Req");
        // Get bus status
        busAccess.targetReqBus(_busSocketId, BR_BUS_ACTION_GENERAL);
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "clockHzGet") == 0)
    {
        // Get clock
        uint32_t actualHz = busAccess.clockCurFreqHz();
        snprintf(pRespJson, maxRespLen, "\"err\":\"ok\",\"clockHz\":\"%d\"", (unsigned)actualHz);
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "clockHzGet %s", pRespJson);
        return true;
    }
    else if (strcasecmp(cmdName, "clockHzSet") == 0)
    {
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "clockHzSet cmd %s params %s %02x %02x %02x %02x", pCmdJson, pParams, pParams[0], pParams[1], pParams[2], pParams[3]);
        // Get clock rate required (may be in cmdJson or paramsJson)
        static const int MAX_CMD_PARAM_STR = 50;
        char paramVal[MAX_CMD_PARAM_STR+1];
        if (!jsonGetValueForKey("clockHz", pCmdJson, paramVal, MAX_CMD_PARAM_STR))
            if (!jsonGetValueForKey("clockHz", (const char*)pParams, paramVal, MAX_CMD_PARAM_STR))
                return false;
        uint32_t clockRateHz = strtoul(paramVal, NULL, 10);
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "clockHzSet %d", clockRateHz);

        // Set clock
        busAccess.clockSetFreqHz(clockRateHz);

        // Get clock
        uint32_t actualHz = busAccess.clockCurFreqHz();
        snprintf(pRespJson, maxRespLen, "\"err\":\"ok\",\"clockHz\":\"%d\"", (unsigned)actualHz);
        return true;
    }
    else if (strcasecmp(cmdName, "waitHoldOn") == 0)
    {
        // Hold
        busAccess.waitHold(_busSocketId, true);
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "waitHoldOff") == 0)
    {
        // Hold
        busAccess.waitHold(_busSocketId, false);
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "waitRelease") == 0)
    {
        // Release
        busAccess.waitRelease();
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "waitMemoryOn") == 0)
    {
        // Wait on memory
        busAccess.waitOnMemory(_busSocketId, true);
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "waitMemoryOff") == 0)
    {
        // Wait on memory
        busAccess.waitOnMemory(_busSocketId, false);
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "waitIOOn") == 0)
    {
        // Wait on IO
        busAccess.waitOnIO(_busSocketId, true);
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "waitIOOff") == 0)
    {
        // Wait on IO
        busAccess.waitOnIO(_busSocketId, false);
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
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
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
#endif
        return true;
    }
    else if (strcasecmp(cmdName, "targetTrackerOff") == 0)
    {
#ifdef USE_TARGET_TRACKER
        // Turn target tracker on
        targetTracker.enable(false, false);
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "TargettrackerOff");
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
#endif
        return true;
    }
    else if (strcasecmp(cmdName, "stepInto") == 0)
    {
#ifdef USE_TARGET_TRACKER
        // Turn target tracker on
        targetTracker.stepInto();
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "TargettrackerStep %s", pCmdJson);
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
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
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
#endif
        return true;
    }
    else if (strcasecmp(cmdName, "getRegs") == 0)
    {
#ifdef USE_TARGET_TRACKER
        char regsStr[200];
        targetTracker.getRegsFormatted(regsStr, sizeof(regsStr));
        strlcpy(pRespJson, "\"err\":\"ok\",\"regs\":\"", maxRespLen);
        strlcat(pRespJson, regsStr, maxRespLen);
        strlcat(pRespJson, "\"", maxRespLen);
#endif
        return true;
    }
    else if (strcasecmp(cmdName, "waitCycleUs") == 0)
    {
        // Get params
        static const int MAX_CMD_PARAM_STR = 50;
        char paramVal[MAX_CMD_PARAM_STR+1];
        if (!jsonGetValueForKey("cycleUs", pCmdJson, paramVal, MAX_CMD_PARAM_STR))
            return false;
        // Wait cycles
        busAccess.waitSetCycleUs(strtoul(paramVal, NULL, 10));
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "Wait cycle set to %d", strtoul(paramVal, NULL, 10));
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers and handlers
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool BusControlAPI::getArg(const char* argName, int argNum, const char* pCmdJson, uint32_t& value, 
            char* pOutStr, uint32_t maxOutStrLen, bool forceDecimal)
{
    // Get from request string if possible
    static const int MAX_CMD_REQ_STR = 200;
    char reqStr[MAX_CMD_REQ_STR+1];
    if (jsonGetValueForKey("reqStr", pCmdJson, reqStr, MAX_CMD_REQ_STR))
    {
        const char* pStr = reqStr;
        // See if there are enough slashes
        for (int i = 0; i < argNum+2; i++)
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

bool BusControlAPI::busLineHandler(const char* pCmdJson)
{
    static const int MAX_CMD_PARAM_STR = 50;
    uint32_t lineNum_DoNotUse;
    char lineName[MAX_CMD_PARAM_STR];
    if (!getArg("line", 1, pCmdJson, lineNum_DoNotUse, lineName, MAX_CMD_PARAM_STR))
    {
        return false;
    }
    char* pStrEnd = strstr(lineName, "/");
    if (pStrEnd)
        *pStrEnd = 0;
    uint32_t busValue = 0;
    if (!getArg("value", 2, pCmdJson, busValue))
    {
        return false;
    }
    BusAccess& busAccess = _busAccess;
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "bussetline %s %d", lineName, busValue);
    if (strcasecmp(lineName, "WR") == 0)
    {
        busAccess.rawBusControlSetPin(BR_WR_BAR, busValue);
    }
    else if (strcasecmp(lineName, "RD") == 0)
    {
        busAccess.rawBusControlSetPin(BR_RD_BAR, busValue);
    }
    else if (strcasecmp(lineName, "MREQ") == 0)
    {
        busAccess.rawBusControlSetPin(BR_MREQ_BAR, busValue);
    }
    else if (strcasecmp(lineName, "IORQ") == 0)
    {
        busAccess.rawBusControlSetPin(BR_IORQ_BAR, busValue);
    }
    else if (strcasecmp(lineName, "PAGE") == 0)
    {
        busAccess.rawBusControlSetPin(BR_IORQ_BAR, busValue);
    }
    else if (strcasecmp(lineName, "DATA_DIR_IN") == 0)
    {
        busAccess.rawBusControlSetPin(BR_DATA_DIR_IN, busValue);
    }
    else if (strcasecmp(lineName, "IORQ_WAIT_EN") == 0)
    {
        busAccess.rawBusControlSetPin(BR_IORQ_WAIT_EN, busValue);
    }
    else if (strcasecmp(lineName, "MREQ_WAIT_EN") == 0)
    {
        busAccess.rawBusControlSetPin(BR_MREQ_WAIT_EN, busValue);
    }
    else if (strcasecmp(lineName, "HADDR_CK") == 0)
    {
        busAccess.rawBusControlSetPin(BR_HADDR_CK, busValue);
    }
    else if (strcasecmp(lineName, "PUSH_ADDR") == 0)
    {
        if (busAccess.getHwVersion() == 17)
            busAccess.rawBusControlSetPin(BR_V17_PUSH_ADDR_BAR, busValue);
    }
    else if (strcasecmp(lineName, "CLOCK") == 0)
    {
        busAccess.rawBusControlSetPin(BR_CLOCK_PIN, busValue);
    }
    else if (strcasecmp(lineName, "M1") == 0)
    {
        busAccess.rawBusControlSetPin(BR_V20_M1_BAR, busValue);
    }
    return true;
}

bool BusControlAPI::muxLineHandler(const char* pCmdJson)
{
    static const int MAX_CMD_PARAM_STR = 50;
    uint32_t lineNum_DoNotUse;
    char lineName[MAX_CMD_PARAM_STR];
    if (!getArg("line", 1, pCmdJson, lineNum_DoNotUse, lineName, MAX_CMD_PARAM_STR))
    {
        return false;
    }
    char* pStrEnd = strstr(lineName, "/");
    if (pStrEnd)
        *pStrEnd = 0;
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "muxSet %s", lineName);
    BusAccess& busAccess = _busAccess;
    if (strcasecmp(lineName, "LADDR_CLK") == 0)
    {
        busAccess.rawBusControlMuxSet(BR_MUX_LADDR_CLK);
    }
    else if (strcasecmp(lineName, "LADDR_CLR_BAR") == 0)
    {
        busAccess.rawBusControlMuxSet(BR_MUX_LADDR_CLR_BAR_LOW);
    }
    else if (strcasecmp(lineName, "DATA_OE_BAR") == 0)
    {
        busAccess.rawBusControlMuxSet(BR_MUX_DATA_OE_BAR_LOW);
    }
    else if (strcasecmp(lineName, "RESET") == 0)
    {
        busAccess.rawBusControlMuxSet(BR_MUX_RESET_Z80_BAR_LOW);
    }
    else if (strcasecmp(lineName, "IRQ") == 0)
    {
        busAccess.rawBusControlMuxSet(BR_MUX_IRQ_BAR_LOW);
    }
    else if (strcasecmp(lineName, "NMI") == 0)
    {
        busAccess.rawBusControlMuxSet(BR_MUX_NMI_BAR_LOW);
    }
    else if (strcasecmp(lineName, "LADDR_OE_BAR") == 0)
    {
        busAccess.rawBusControlMuxSet(BR_MUX_LADDR_OE_BAR);
    }
    else if (strcasecmp(lineName, "HADDR_OE_BAR") == 0)
    {
        busAccess.rawBusControlMuxSet(BR_MUX_HADDR_OE_BAR);
    }
    return true;
}

void BusControlAPI::busLinesRead(char* pRespJson, int maxRespLen)
{
    // Read bus lines
    BusAccess& busAccess = _busAccess;
    uint32_t lines = busAccess.rawBusControlReadRaw();
    bool m1Val = lines & BR_V20_M1_BAR_MASK; 
    if (busAccess.getHwVersion() == 17)
        m1Val = (lines & BR_V17_M1_PIB_BAR_MASK);
    snprintf(pRespJson, maxRespLen, "\"err\":\"ok\",\"raw\":\"%02x\",\"pib\":\"%02x\",\"ctrl\":\"%c%c%c%c%c\"",
            (unsigned)busAccess.rawBusControlReadRaw(), 
            (unsigned)((busAccess.rawBusControlReadRaw() >> BR_DATA_BUS) & 0xff),
            (lines & BR_MREQ_BAR_MASK) ? 'M' : '.', 
            (lines & BR_IORQ_BAR_MASK) ? 'I' : '.', 
            (lines & BR_RD_BAR_MASK) ? 'R' : '.', 
            (lines & BR_WR_BAR_MASK) ? 'W' : '.',
            m1Val ? '1' : '.'
            );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Callbacks/Hooks
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusControlAPI::busActionCompleteStatic(void* pObject, BR_BUS_ACTION actionType, BR_BUS_ACTION_REASON reason)
{
    if (!pObject)
        return;
    ((BusControlAPI*)pObject)->busActionComplete(actionType, reason);
}

void BusControlAPI::busActionComplete(BR_BUS_ACTION actionType,  BR_BUS_ACTION_REASON reason)
{
#ifdef DEBUG_BUS_ACTION_COMPLETE
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "busActionComplete type %d reason %d", actionType, reason);
#endif

    HwManager& hwManager = _hwManager;
    if ((actionType == BR_BUS_ACTION_BUSRQ) && _memAccessPending)
    {
        if (!_memAccessWrite)
        {
            // Read
            hwManager.blockRead(_memAccessAddr, _memAccessDataBuf, 
                        _memAccessDataLen, false, _memAccessIo, false);
        }
        else
        {
            // Write
            hwManager.blockWrite(_memAccessAddr, _memAccessDataBuf, 
                        _memAccessDataLen, false, _memAccessIo, false);
        }
        _memAccessPending = false;
    }
}

void BusControlAPI::handleWaitInterruptStatic(void* pObject, uint32_t addr, uint32_t data, 
        uint32_t flags, uint32_t& retVal)
{
    if (!pObject)
        return;
    ((BusControlAPI*)pObject)->handleWaitInterrupt(addr, data, flags, retVal);
}

void BusControlAPI::handleWaitInterrupt( uint32_t addr,  uint32_t data, 
         uint32_t flags,  uint32_t& retVal)
{
}

void BusControlAPI::service()
{
#ifdef USE_TARGET_TRACKER
    // Check for targettracker enable message response
    TargetTracker& targetTracker = _mcManager.getTargetTracker();
    if (_targetTrackerResetPending && targetTracker.isStepPaused())
    {
        _commandHandler.sendUnnumberedMsg("targetTrackerOnDone", "\"err\":\"ok\"");
        _targetTrackerResetPending = false;
    }
    // Check for step completion message response
    if (_stepCompletionPending && targetTracker.isStepPaused())
    {
        _commandHandler.sendUnnumberedMsg("stepIntoDone", "\"err\":\"ok\"");
        _stepCompletionPending = false;
    }
#endif
}

BR_RETURN_TYPE BusControlAPI::blockAccessSync(uint32_t addr, uint8_t* pData, uint32_t len, bool iorq, bool write)
{
    // Request the bus
    _memAccessPending = true;
    _memAccessWrite = write;
    _memAccessDataLen = len;
    if (_memAccessDataLen > MAX_MEM_BLOCK_READ_WRITE)
        _memAccessDataLen = MAX_MEM_BLOCK_READ_WRITE;
    _memAccessAddr = addr;
    _memAccessIo = iorq;
    if (write)
        memcopyfast(_memAccessDataBuf, pData, len);
    BusAccess& busAccess = _busAccess;
    busAccess.targetReqBus(_busSocketId, BR_BUS_ACTION_GENERAL);
    // Now enter a loop to wait for the bus action to complete
    static const uint32_t MAX_WAIT_FOR_BUS_ACCESS_US = 50000;
    uint32_t busAccessReqStart = micros();

#ifdef DEBUG_API_BLOCK_ACCESS_SYNC
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "busControlAPI busRequestMade");
#endif

    while(!isTimeout(micros(), busAccessReqStart, MAX_WAIT_FOR_BUS_ACCESS_US))
    {
        // Finished?
        if (!_memAccessPending)
        {
#ifdef DEBUG_API_BLOCK_ACCESS_SYNC
            LogWrite(MODULE_PREFIX, LOG_DEBUG, "busControlAPI memAccess no longer pending");
#endif
            break;
        }
        // Service the bus access - the actual read/write operation occurs in a
        // callback during this function call
        busAccess.service();
    }

#ifdef DEBUG_API_BLOCK_ACCESS_SYNC
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "busControlAPI busRequestDone");
#endif

    // Check if completed ok
    if (_memAccessPending)
    {
        _memAccessPending = false;
        return BR_NO_BUS_ACK;
    }
    if (!write)
        memcopyfast(pData, _memAccessDataBuf, len);
    return BR_OK;
}