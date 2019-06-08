// Bus Raider
// Rob Dobson 2019

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include "../CommandInterface/CommandHandler.h"

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
    bool handleLine(char* pCmd, char* pResponse, int maxResponseLen, uint32_t zesaruxMsgIndex);
    bool commandMatch(const char* s1, const char* s2); 
    void addPromptMsg(char* pResponse, int maxResponseLen);
    void mungeDisassembly(char* pText);

    static const int ZEsarUX_CMD_MAX_LEN = 1000;
    static const int ZEsarUX_RESP_MAX_LEN = 1000;

    // Smartload state
    uint32_t _smartloadStartUs;
    bool _smartloadInProgress;
    uint32_t _smartloadMsgIdx;
    bool _smartloadStartDetected;
    static const int MAX_SMART_LOAD_TIME_US = 4000000;
    static const int MAX_TIME_BEFORE_START_DETECT_US = 1000000;

    // Reset pending
    bool _resetPending;
    uint32_t _resetPendingTimeUs;
    static const int MAX_TIME_RESET_PENDING_US = 100000;

    // Event pending
    bool _stepCompletionPending;
};

