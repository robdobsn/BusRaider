// Bus Raider
// Rob Dobson 2019

#include "BusController.h"
#include "../System/piwiring.h"
#include "../System/lowlib.h"
#include "../System/ee_sprintf.h"
#include "../System/logging.h"
#include "../System/rdutils.h"
#include "../TargetBus/TargetTracker.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Variables
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Module name
static const char FromBusController[] = "BusController";

// Sockets
int BusController::_busSocketId = -1;
int BusController::_commsSocketId = -1;

// Main comms socket - to wire up command handler
CommsSocketInfo BusController::_commsSocketInfo =
{
    true,
    BusController::handleRxMsg,
    NULL,
    NULL
};

// Bus socket
BusSocketInfo BusController::_busSocketInfo = 
{
    true,
    BusController::handleWaitInterruptStatic,
    BusController::busActionCompleteStatic,
    false,
    false,
    false,
    0,
    false,
    0,
    false,
    0,
    false,
    BR_BUS_ACTION_GENERAL,
    false
};

// This instance
BusController* BusController::_pThisInstance = NULL;

// Memory read/write handler
uint8_t BusController::_memAccessDataBuf[MAX_MEM_BLOCK_READ_WRITE];

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Constructor
BusController::BusController()
{
    _pThisInstance = this;
    _memAccessReadPending = false;
    _memAccessWritePending = false;
    _memAccessDataLen = 0;
    _memAccessAddr = 0;
    _memAccessIo = false;
}

void BusController::init()
{
    // Connect to the bus socket
    if (_busSocketId < 0)
        _busSocketId = BusAccess::busSocketAdd(_busSocketInfo);

    // Connect to the comms socket
    if (_commsSocketId < 0)
        _commsSocketId = CommandHandler::commsSocketAdd(_commsSocketInfo);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Received message handler
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool BusController::handleRxMsg(const char* pCmdJson, [[maybe_unused]]const uint8_t* pParams, [[maybe_unused]]int paramsLen,
                [[maybe_unused]]char* pRespJson, [[maybe_unused]]int maxRespLen)
{
    // Get the command string from JSON
    static const int MAX_CMD_NAME_STR = 50;
    char cmdName[MAX_CMD_NAME_STR+1];
    if (!jsonGetValueForKey("cmdName", pCmdJson, cmdName, MAX_CMD_NAME_STR))
        return false;

    // Check for memory/IO read
    if (strcasecmp(cmdName, "Rd") == 0)
    {
        // Get params
        uint32_t addr = 0;
        int dataLen = 0;
        bool isIo = false;
        if (!getArgsRdAndWr(pCmdJson, addr, dataLen, isIo))
        {
            strlcpy(pRespJson, "\"err\":\"InvArgs\"", maxRespLen);
            return true;
        }
        // Get data using bus access
        if ((dataLen <= 0) || (dataLen >= MAX_MEM_BLOCK_READ_WRITE))
        {
            strlcpy(pRespJson, "\"err\":\"LenTooLong\"", maxRespLen);
            return true;
        }
        // Request the bus
        _pThisInstance->_memAccessReadPending = true;
        _pThisInstance->_memAccessDataLen = dataLen;
        _pThisInstance->_memAccessAddr = addr;
        _pThisInstance->_memAccessIo = isIo;
        BusAccess::targetReqBus(_busSocketId, BR_BUS_ACTION_GENERAL);
        // Now enter a loop to wait for the bus action to complete
        static const uint32_t MAX_WAIT_FOR_BUS_ACCESS_US = 50000;
        uint32_t busAccessReqStart = micros();
        while(!isTimeout(micros(), busAccessReqStart, MAX_WAIT_FOR_BUS_ACCESS_US))
        {
            // Finished?
            if (!_pThisInstance->_memAccessReadPending)
                break;
            // Service the bus access
            BusAccess::service();
        }
        // Check if completed ok
        if (_pThisInstance->_memAccessReadPending)
        {
            _pThisInstance->_memAccessReadPending = false;
            ee_sprintf(pRespJson, "\"err\":\"fail\"");
            digitalWrite(8,1);
            microsDelay(100);
            digitalWrite(8,0);
            return true;
        }
        // Format data to send
        static const int MAX_MEM_READ_RESP = MAX_MEM_BLOCK_READ_WRITE*2+100; 
        char jsonResp[MAX_MEM_READ_RESP];
        ee_sprintf(jsonResp, "\"err\":\"ok\",\"len\":%d,\"data\":\"", dataLen);
        int pos = strlen(jsonResp);
        for (int i = 0; i < dataLen; i++)
        {
            ee_sprintf(jsonResp+pos, "%02x", _memAccessDataBuf[i]);
            pos+=2;
        }
        strlcat(jsonResp, "\"", MAX_MEM_READ_RESP);
        strlcpy(pRespJson, jsonResp, maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "Wr") == 0)
    {
        // Get params
        uint32_t addr = 0;
        int dataLen = 0;
        bool isIo = false;
        if (!getArgsRdAndWr(pCmdJson, addr, dataLen, isIo))
        {
            strlcpy(pRespJson, "\"err\":\"InvArgs\"", maxRespLen);
            return true;
        }
        // Get data using bus access
        if ((dataLen <= 0) || (dataLen >= MAX_MEM_BLOCK_READ_WRITE) || (dataLen > paramsLen))
        {
            strlcpy(pRespJson, "\"err\":\"LenTooLong\"", maxRespLen);
            return true;
        }
        // Set for write
        _pThisInstance->_memAccessWritePending = true;
        _pThisInstance->_memAccessDataLen = dataLen;
        _pThisInstance->_memAccessAddr = addr;
        _pThisInstance->_memAccessIo = isIo;
        memcpy(_memAccessDataBuf, pParams, dataLen);
        BusAccess::targetReqBus(_busSocketId, BR_BUS_ACTION_GENERAL);
        // Now enter a loop to wait for the bus action to complete
        static const uint32_t MAX_WAIT_FOR_BUS_ACCESS_US = 50000;
        uint32_t busAccessReqStart = micros();
        while(!isTimeout(micros(), busAccessReqStart, MAX_WAIT_FOR_BUS_ACCESS_US))
        {
            // Finished?
            if (!_pThisInstance->_memAccessWritePending)
                break;
            // Service the bus access
            BusAccess::service();
        }
        // Check if completed ok
        if (_pThisInstance->_memAccessWritePending)
        {
            _pThisInstance->_memAccessWritePending = false;
            ee_sprintf(pRespJson, "\"err\":\"fail\"");
            return true;
        }
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "testRdWr") == 0)
    {
        int errs = 0;
        char errStr[200];
        errStr[0] = 0;
        for (int i = 0; i < 1000; i++)
        {
            uint8_t writeBuf[MAX_MEM_BLOCK_READ_WRITE];
            for (int j = 0; j < MAX_MEM_BLOCK_READ_WRITE; j++)
                writeBuf[j] = rand() & 0xff;
            BusAccess::blockWrite(0x3c00, writeBuf, MAX_MEM_BLOCK_READ_WRITE, true, false);

            uint8_t readBuf[MAX_MEM_BLOCK_READ_WRITE];
            BusAccess::blockRead(0x3c00, readBuf, MAX_MEM_BLOCK_READ_WRITE, true, false);

            if (memcmp(writeBuf, readBuf, MAX_MEM_BLOCK_READ_WRITE) != 0)
            {
                if (errs == 0)
                {
                    int errPos = 0;
                    for (int k = 0; k < MAX_MEM_BLOCK_READ_WRITE; k++)
                    {
                        if (writeBuf[k] != readBuf[k])
                        {
                            errPos = k;
                            break;
                        }
                    }
                    // digitalWrite(8, 1);
                    // microsDelay(100);
                    // digitalWrite(8, 0);
                    ee_sprintf(errStr, "POS %d WR %02x %02x %02x %02x RD %02x %02x %02x %02x", 
                                errPos,
                                writeBuf[errPos], writeBuf[errPos+1], writeBuf[errPos+2], writeBuf[errPos+3],
                                readBuf[errPos+0], readBuf[errPos+1], readBuf[errPos+2], readBuf[errPos+3]);
                }
                errs++;
            }
        }
        if (maxRespLen > 200)
        {
            ee_sprintf(pRespJson, "\"err\":\"%s\",\"errs\":%d,\"errStr\":\"%s\"", errs > 0 ? "fail" : "ok", errs, errStr);
            LogWrite(FromBusController, LOG_DEBUG, pRespJson);
        }
        return true;
    }
    else if (strcasecmp(cmdName, "busStatus") == 0)
    {
        // Get bus status
        BusAccessStatusInfo statusInfo;
        BusAccess::getStatus(statusInfo);
        strlcpy(pRespJson, statusInfo.getJson(), maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "busStatusClear") == 0)
    {
        // Clear bus status
        BusAccess::clearStatus();
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "busReset") == 0)
    {
        // Get bus status
        BusAccess::busAccessReset();
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "targetReset") == 0)
    {
        LogWrite(FromBusController, LOG_DEBUG, "Target Reset");
        // Get bus status
        BusAccess::targetReqReset(_busSocketId);
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "clockHzGet") == 0)
    {
        // Get clock
        uint32_t actualHz = BusAccess::clockCurFreqHz();
        ee_sprintf(pRespJson, "\"err\":\"ok\",\"clockHz\":\"%d\"", actualHz);
        // LogWrite(FromBusController, LOG_DEBUG, "clockHzGet %s", pRespJson);
        return true;
    }
    else if (strcasecmp(cmdName, "clockHzSet") == 0)
    {
        // LogWrite(FromBusController, LOG_DEBUG, "clockHzSet cmd %s params %s %02x %02x %02x %02x", pCmdJson, pParams, pParams[0], pParams[1], pParams[2], pParams[3]);
        // Get clock rate required (may be in cmdJson or paramsJson)
        static const int MAX_CMD_PARAM_STR = 50;
        char paramVal[MAX_CMD_PARAM_STR+1];
        if (!jsonGetValueForKey("clockHz", pCmdJson, paramVal, MAX_CMD_PARAM_STR))
            if (!jsonGetValueForKey("clockHz", (const char*)pParams, paramVal, MAX_CMD_PARAM_STR))
                return false;
        uint32_t clockRateHz = strtoul(paramVal, NULL, 10);
        // LogWrite(FromBusController, LOG_DEBUG, "clockHzSet %d", clockRateHz);

        // Set clock
        BusAccess::clockSetFreqHz(clockRateHz);

        // Get clock
        uint32_t actualHz = BusAccess::clockCurFreqHz();
        ee_sprintf(pRespJson, "\"err\":\"ok\",\"clockHz\":\"%d\"", actualHz);
        return true;
    }
    else if (strcasecmp(cmdName, "waitHoldOn") == 0)
    {
        // Hold
        BusAccess::waitHold(_busSocketId, true);
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "waitHoldOff") == 0)
    {
        // Hold
        BusAccess::waitHold(_busSocketId, false);
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "waitRelease") == 0)
    {
        // Release
        BusAccess::waitRelease();
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "waitMemoryOn") == 0)
    {
        // Wait on memory
        BusAccess::waitOnMemory(_busSocketId, true);
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "waitMemoryOff") == 0)
    {
        // Wait on memory
        BusAccess::waitOnMemory(_busSocketId, false);
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "waitIOOn") == 0)
    {
        // Wait on IO
        BusAccess::waitOnIO(_busSocketId, true);
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "waitIOOff") == 0)
    {
        // Wait on IO
        BusAccess::waitOnIO(_busSocketId, false);
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "targetTrackerOn") == 0)
    {
        // Get clock rate required
        static const int MAX_CMD_PARAM_STR = 50;
        char paramVal[MAX_CMD_PARAM_STR+1];
        if (!jsonGetValueForKey("reset", pCmdJson, paramVal, MAX_CMD_PARAM_STR))
            return false;
        bool reset = strtol(paramVal, NULL, 10) != 0;
        // Turn target tracker on
        TargetTracker::enable(true);
        if (reset)
            TargetTracker::targetReset();
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "targetTrackerOff") == 0)
    {
        // Turn target tracker on
        TargetTracker::enable(false);
        LogWrite(FromBusController, LOG_DEBUG, "TargettrackerOff");
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "stepInto") == 0)
    {
        // Turn target tracker on
        TargetTracker::stepInto();
        // LogWrite(FromBusController, LOG_DEBUG, "TargettrackerStep");
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "stepRun") == 0)
    {
        // Turn target tracker on
        TargetTracker::stepRun();
        LogWrite(FromBusController, LOG_DEBUG, "TargettrackerRun");
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "getRegs") == 0)
    {
        char regsStr[200];
        TargetTracker::getRegsFormatted(regsStr, sizeof(regsStr));
        strlcpy(pRespJson, "\"err\":\"ok\",\"regs\":\"", maxRespLen);
        strlcat(pRespJson, regsStr, maxRespLen);
        strlcat(pRespJson, "\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "waitCycleUs") == 0)
    {
        // Get params
        static const int MAX_CMD_PARAM_STR = 50;
        char paramVal[MAX_CMD_PARAM_STR+1];
        if (!jsonGetValueForKey("cycleUs", pCmdJson, paramVal, MAX_CMD_PARAM_STR))
            return false;
        // Turn target tracker on
        BusAccess::waitSetCycleUs(strtoul(paramVal, NULL, 10));
        // LogWrite(FromBusController, LOG_DEBUG, "Wait cycle set to %d", strtoul(paramVal, NULL, 10));
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }

    return false;
}

bool BusController::getArgsRdAndWr(const char* pCmdJson, uint32_t& addr, int& dataLen, bool& isIo)
{
    // Get params
    static const int MAX_CMD_PARAM_STR = 50;
    char paramVal[MAX_CMD_PARAM_STR+1];
    if (!jsonGetValueForKey("addr", pCmdJson, paramVal, MAX_CMD_PARAM_STR))
        return false;
    addr = strtoul(paramVal, NULL, 10);
    if (!jsonGetValueForKey("len", pCmdJson, paramVal, MAX_CMD_PARAM_STR))
        return false;
    dataLen = strtoul(paramVal, NULL, 10);
    if (!jsonGetValueForKey("isIo", pCmdJson, paramVal, MAX_CMD_PARAM_STR))
        return false;
    isIo = strtoul(paramVal, NULL, 10) != 0;
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Callbacks/Hooks
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusController::busActionCompleteStatic([[maybe_unused]]BR_BUS_ACTION actionType, [[maybe_unused]] BR_BUS_ACTION_REASON reason)
{
    if ((actionType == BR_BUS_ACTION_BUSRQ) && _pThisInstance)
    {
        if (_pThisInstance->_memAccessReadPending)
        {
            // Read
            BusAccess::blockRead(_pThisInstance->_memAccessAddr, _memAccessDataBuf, 
                        _pThisInstance->_memAccessDataLen, false, _pThisInstance->_memAccessIo);
            _pThisInstance->_memAccessReadPending = false;
        }
        if (_pThisInstance->_memAccessWritePending)
        {
            // Read
            BusAccess::blockWrite(_pThisInstance->_memAccessAddr, _memAccessDataBuf, 
                        _pThisInstance->_memAccessDataLen, false, _pThisInstance->_memAccessIo);
            _pThisInstance->_memAccessWritePending = false;
        }
    }
}

void BusController::handleWaitInterruptStatic(uint32_t addr, uint32_t data, 
        uint32_t flags, uint32_t& retVal)
{
    if (_pThisInstance)
        _pThisInstance->handleWaitInterrupt(addr, data, flags, retVal);
}

void BusController::handleWaitInterrupt([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, 
        [[maybe_unused]] uint32_t flags, [[maybe_unused]] uint32_t& retVal)
{
}

void BusController::service()
{
}
