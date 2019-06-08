// Bus Raider
// Rob Dobson 2019

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include "../TargetBus/BusAccess.h"
#include "../CommandInterface/CommandHandler.h"

class BusController
{
public:
    static const int MAX_MEM_BLOCK_READ_WRITE = 1024;

    BusController();
    void init();

    // Service
    void service();

    // Bus access to data
    static BR_RETURN_TYPE blockAccessSync(uint32_t addr, uint8_t* pData, uint32_t len, bool iorq, bool write);

private:

    // Singleton instance
    static BusController* _pThisInstance;

    // Bus socket we're attached to and setup info
    static int _busSocketId;
    static BusSocketInfo _busSocketInfo;

    // Comms socket we're attached to and setup info
    static int _commsSocketId;
    static CommsSocketInfo _commsSocketInfo;

    // Handle messages (telling us to start/stop)
    static bool handleRxMsg(const char* pCmdJson, const uint8_t* pParams, int paramsLen,
                    char* pRespJson, int maxRespLen);

    // Bus action complete callback
    static void busActionCompleteStatic(BR_BUS_ACTION actionType, BR_BUS_ACTION_REASON reason);

    // Wait interrupt handler
    static void handleWaitInterruptStatic(uint32_t addr, uint32_t data, 
            uint32_t flags, uint32_t& retVal);
    void handleWaitInterrupt(uint32_t addr, uint32_t data, 
            uint32_t flags, uint32_t& retVal);

    // Helpers
    static bool busLineHandler(const char* pCmdJson);
    static bool muxLineHandler(const char* pCmdJson);
    static void busLinesRead(char* pRespJson, [[maybe_unused]]int maxRespLen);
    static bool getArg(const char* argName, int argNum, const char* pCmdJson, 
                uint32_t& value, char* pOutStr = NULL, uint32_t maxOutStrLen = 0,
                bool forceDecimal = false);

    // Synchronous bus access
    static uint8_t _memAccessDataBuf[MAX_MEM_BLOCK_READ_WRITE];
    static bool _memAccessPending;
    static bool _memAccessWrite;
    static uint32_t _memAccessDataLen;
    static uint32_t _memAccessAddr;
    static bool _memAccessIo;
    static uint32_t _memAccessRdWrErrCount;
    static const int MAX_RDWR_ERR_STR_LEN = 200;
    static char _memAccessRdWrErrStr[MAX_RDWR_ERR_STR_LEN];
    static bool _memAccessRdWrTest;
};
