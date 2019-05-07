// Bus Raider
// Rob Dobson 2019

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include "../CommandInterface/CommandHandler.h"

// Validator
class ZEsarUXInterface
{
public:
    static const int MAX_MEM_BLOCK_READ_WRITE = 1024;

    ZEsarUXInterface();
    void init();

    // Service
    void service();

private:

    // Singleton instance
    static ZEsarUXInterface* _pThisInstance;

    // Comms socket we're attached to and setup info
    static int _commsSocketId;
    static CommsSocketInfo _commsSocketInfo;

    // Handle messages (telling us to start/stop)
    static bool handleRxMsg(const char* pCmdJson, const uint8_t* pParams, int paramsLen,
                    char* pRespJson, int maxRespLen);

    void handleMessage(const char* pJsonCmd, const char* pZesaruxMsg, uint32_t zesaruxIndex);
    bool handleLine(char* pCmd, char* pResponse, int maxResponseLen);
    bool commandMatch(const char* s1, const char* s2); 

    static const int ZEsarUX_CMD_MAX_LEN = 1000;
};

