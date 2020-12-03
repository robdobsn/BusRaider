// Bus Raider
// Rob Dobson 2019

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include "../TargetBus/TargetCPU.h"

class CommandHandler;
class BusControl;
class HwManager;
class McManager;

class ControlAPI
{
public:
    static const int MAX_MEM_BLOCK_READ_WRITE = 1024;

    // ControlAPI(CommandHandler& commandHandler, HwManager& hwManager, BusControl& busControl);
    ControlAPI(CommandHandler &commandHandler, BusControl &busControl, McManager &mcManager);
    void init();

    // Service
    void service();

    // Bus access to data
    BR_RETURN_TYPE blockAccessSync(uint32_t addr, uint8_t *pData, uint32_t len, bool iorq,
                                   bool read, bool write);

private:
    // Singleton instance
    static ControlAPI *_pThisInstance;

    // Command handler
    CommandHandler &_commandHandler;

    // // HwManager
    // HwManager& _hwManager;

    // Bus access
    BusControl &_busControl;

    // Machine manager
    McManager &_mcManager;

//#define USE_TARGET_TRACKER
#ifdef USE_TARGET_TRACKER
    // Target tracker
    TargetController &_targetController;
#endif

    // Bus socket we're attached to and setup info
    int _busSocketId;

    // Comms socket we're attached to and setup info
    int _commsSocketId;

    // Handle messages (telling us to start/stop)
    static bool handleRxMsgStatic(void *pObject, const char *pCmdJson, const uint8_t *pParams, unsigned paramsLen,
                                  char *pRespJson, unsigned maxRespLen);
    bool handleRxMsg(const char *pCmdJson, const uint8_t *pParams, unsigned paramsLen,
                     char *pRespJson, unsigned maxRespLen);

    // Bus action active callback
    static void busActionActiveStatic(void *pObject, BR_BUS_ACTION actionType,
                                      BR_BUS_ACTION_REASON reason, BR_RETURN_TYPE rslt);
    void busActionActive(BR_BUS_ACTION actionType, BR_BUS_ACTION_REASON reason,
                         BR_RETURN_TYPE rslt);

    // Wait interrupt handler
    static void handleWaitInterruptStatic(void *pObject, uint32_t addr, uint32_t data,
                                          uint32_t flags, uint32_t &retVal);
    void handleWaitInterrupt(uint32_t addr, uint32_t data,
                             uint32_t flags, uint32_t &retVal);

    // Helpers
    bool busLineHandler(const char *pCmdJson);
    bool muxLineHandler(const char *pCmdJson);
    void busLinesRead(char *pRespJson, int maxRespLen);
    bool getArg(const char *argName, int argNum, const char *pCmdJson,
                uint32_t &value, char *pOutStr = NULL, uint32_t maxOutStrLen = 0,
                bool forceDecimal = false);

    // Message actions
    bool apiProgClear(char *pRespJson, unsigned maxRespLen);
    bool apiProgAddMemBlock(const char *pCmdJson,
                            const uint8_t *pParams, unsigned paramsLen,
                            char *pRespJson, unsigned maxRespLen);
    bool apiProgWrite(const char *pCmdJson,
                      const uint8_t *pParams, unsigned paramsLen,
                      char *pRespJson, unsigned maxRespLen);
    bool apiProgWriteAndRun(char *pRespJson, unsigned maxRespLen);
    bool apiTargetReset(char *pRespJson, unsigned maxRespLen);
    bool apiFileToTarget(const char *pCmdJson,
                         const uint8_t *pParams, unsigned paramsLen,
                         char *pRespJson, unsigned maxRespLen);
    bool apiSetMcJson(const char *pCmdJson,
                      const uint8_t *pParams, unsigned paramsLen,
                      char *pRespJson, unsigned maxRespLen);
    bool apiSendKeyToTarget(const char *pCmdJson,
                            const uint8_t *pParams, unsigned paramsLen,
                            char *pRespJson, unsigned maxRespLen);
    bool apiReadFromTarget(const char *pCmdJson,
                           const uint8_t *pParams, unsigned paramsLen,
                           char *pRespJson, unsigned maxRespLen);
    bool apiWriteToTarget(const char *pCmdJson,
                          const uint8_t *pParams, unsigned paramsLen,
                          char *pRespJson, unsigned maxRespLen);
    bool apiWriteReadTarget(const char *pCmdJson,
                            const uint8_t *pParams, unsigned paramsLen,
                            char *pRespJson, unsigned maxRespLen);
    bool apiDebuggerBreak(const char *pCmdJson,
                          const uint8_t *pParams, unsigned paramsLen,
                          char *pRespJson, unsigned maxRespLen);
    bool apiDebuggerContinue(const char *pCmdJson,
                             const uint8_t *pParams, unsigned paramsLen,
                             char *pRespJson, unsigned maxRespLen);
    bool apiDebuggerStepIn(const char *pCmdJson,
                           const uint8_t *pParams, unsigned paramsLen,
                           char *pRespJson, unsigned maxRespLen);
    bool apiDebuggerGetRegs(const char *pCmdJson,
                            const uint8_t *pParams, unsigned paramsLen,
                            char *pRespJson, unsigned maxRespLen);
    bool apiDebuggerStatus(const char *pCmdJson,
                           const uint8_t *pParams, unsigned paramsLen,
                           char *pRespJson, unsigned maxRespLen);

    // // Synchronous bus access
    // uint8_t _memAccessDataBuf[MAX_MEM_BLOCK_READ_WRITE];
    // bool _memAccessPending;
    // bool _memAccessWrite;
    // uint32_t _memAccessDataLen;
    // uint32_t _memAccessAddr;
    // bool _memAccessIo;
    // uint32_t _memAccessRdWrErrCount;
    // static const int MAX_RDWR_ERR_STR_LEN = 200;
    // char _memAccessRdWrErrStr[MAX_RDWR_ERR_STR_LEN];
    // bool _memAccessRdWrTest;

    // // Comms
    // bool _stepCompletionPending;
    // bool _targetTrackerResetPending;
};
