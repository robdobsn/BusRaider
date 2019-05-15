// Bus Raider
// Rob Dobson 2019

#include "BusRaiderAPI.h"
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
static const char FromBusRaiderAPI[] = "BusRaiderAPI";

// Sockets
int BusRaiderAPI::_busSocketId = -1;
int BusRaiderAPI::_commsSocketId = -1;

// Main comms socket - to wire up command handler
CommsSocketInfo BusRaiderAPI::_commsSocketInfo =
{
    true,
    BusRaiderAPI::handleRxMsg,
    NULL,
    NULL
};

// Bus socket
BusSocketInfo BusRaiderAPI::_busSocketInfo = 
{
    true,
    NULL,
    NULL,
    false,
    false,
    BR_BUS_ACTION_NONE,
    1,
    false,
    BR_BUS_ACTION_DISPLAY,
    false
};

// This instance
BusRaiderAPI* BusRaiderAPI::_pThisInstance = NULL;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Constructor
BusRaiderAPI::BusRaiderAPI()
{
    _pThisInstance = this;
}

void BusRaiderAPI::init()
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

bool BusRaiderAPI::handleRxMsg(const char* pCmdJson, [[maybe_unused]]const uint8_t* pParams, [[maybe_unused]]int paramsLen,
                [[maybe_unused]]char* pRespJson, [[maybe_unused]]int maxRespLen)
{
    LogWrite(FromBusRaiderAPI, LOG_DEBUG, "handleRxMsg");
    
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
        uint8_t dataBuf[MAX_MEM_BLOCK_READ_WRITE];
        BusAccess::blockRead(addr, dataBuf, dataLen, true, isIo);
        // Format data to send
        static const int MAX_MEM_READ_RESP = MAX_MEM_BLOCK_READ_WRITE*2+100; 
        char jsonResp[MAX_MEM_READ_RESP];
        ee_sprintf(jsonResp, "\"err\":\"ok\",\"len\":%d,\"data\":\"", dataLen);
        int pos = strlen(jsonResp);
        for (int i = 0; i < dataLen; i++)
        {
            ee_sprintf(jsonResp+pos, "%02x", dataBuf[i]);
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
        BusAccess::blockWrite(addr, pParams, dataLen, true, isIo);
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
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
        // Get bus status
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
        LogWrite(FromBusRaiderAPI, LOG_DEBUG, "Target Reset");
        // Get bus status
        BusAccess::targetReqReset(_busSocketId);
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "clockSetHz") == 0)
    {
        // Get clock rate required
        static const int MAX_CMD_PARAM_STR = 50;
        char paramVal[MAX_CMD_PARAM_STR+1];
        if (!jsonGetValueForKey("clockHz", pCmdJson, paramVal, MAX_CMD_PARAM_STR))
            return false;
        uint32_t clockRateHz = strtoul(paramVal, NULL, 10);

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
            BusAccess::targetReqReset(_busSocketId);
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "targetTrackerOff") == 0)
    {
        // Turn target tracker on
        TargetTracker::enable(false);
        LogWrite(FromBusRaiderAPI, LOG_DEBUG, "TargettrackerOff");
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "stepInto") == 0)
    {
        // Turn target tracker on
        TargetTracker::stepInto();
        // LogWrite(FromBusRaiderAPI, LOG_DEBUG, "TargettrackerStep");
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "stepRun") == 0)
    {
        // Turn target tracker on
        TargetTracker::stepRun();
        LogWrite(FromBusRaiderAPI, LOG_DEBUG, "TargettrackerRun");
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
        // LogWrite(FromBusRaiderAPI, LOG_DEBUG, "Wait cycle set to %d", strtoul(paramVal, NULL, 10));
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }

    return false;
}

bool BusRaiderAPI::getArgsRdAndWr(const char* pCmdJson, uint32_t& addr, int& dataLen, bool& isIo)
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

void BusRaiderAPI::busActionCompleteStatic([[maybe_unused]]BR_BUS_ACTION actionType, [[maybe_unused]] BR_BUS_ACTION_REASON reason)
{
}

void BusRaiderAPI::handleWaitInterruptStatic(uint32_t addr, uint32_t data, 
        uint32_t flags, uint32_t& retVal)
{
    if (_pThisInstance)
        _pThisInstance->handleWaitInterrupt(addr, data, flags, retVal);
}

void BusRaiderAPI::handleWaitInterrupt([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, 
        [[maybe_unused]] uint32_t flags, [[maybe_unused]] uint32_t& retVal)
{
}

void BusRaiderAPI::service()
{
}
