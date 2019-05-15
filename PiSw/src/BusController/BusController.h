// Bus Raider
// Rob Dobson 2019

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include "../TargetBus/BusAccess.h"
#include "../CommandInterface/CommandHandler.h"

// Validator
class BusController
{
public:
    static const int MAX_MEM_BLOCK_READ_WRITE = 1024;

    BusController();
    void init();

    // Service
    void service();

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
    static bool getArgsRdAndWr(const char* pCmdJson, uint32_t& addr, int& dataLen, bool& isIo);
};
