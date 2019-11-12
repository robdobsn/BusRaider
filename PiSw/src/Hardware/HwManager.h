// Bus Raider Hardware Manager
// Rob Dobson 2019

#pragma once

#include <string.h> 
#include "HwBase.h"
#include "../System/logging.h"
#include "../TargetBus/BusAccess.h"
#include "../CommandInterface/CommandHandler.h"

// #define DEBUG_IO_ACCESS 1

#ifdef DEBUG_IO_ACCESS
class DebugIOPortAccess
{
public:
    int port;
    bool type;
    int val;
};
#endif

class HwManager
{
public:
    // Init
    static void init();

    // Service
    static void service();

    // Manage hardware
    static void add(HwBase* pHw);

    // Memory emulation mode 
    static void setMemoryEmulationMode(bool val);
    static bool getMemoryEmulationMode()
    {
        return _memoryEmulationMode;
    }

    // Memory paging enable 
    static void setMemoryPagingEnable(bool val);
    static bool getMemoryPagingEnable()
    {
        return _memoryPagingEnable;
    }

    // Opcode inject enable 
    static void setOpcodeInjectEnable(bool val);
    static bool getOpcodeInjectEnable()
    {
        return _opcodeInjectEnable;
    }

    // Page out RAM/ROM for opcode injection
    static void pageOutForInjection(bool pageOut);

    // Mirror memory mode - read/write to hardware enacts on mirror
    static void setMirrorMode(bool val);
    static void mirrorClone();

    // Block access to hardware
    static uint32_t getMaxAddress();
    static BR_RETURN_TYPE blockWrite(uint32_t addr, const uint8_t* pBuf, uint32_t len, 
                bool busRqAndRelease, bool iorq, bool forceMirrorAccess);
    static BR_RETURN_TYPE blockRead(uint32_t addr, uint8_t* pBuf, uint32_t len, 
                bool busRqAndRelease, bool iorq, bool forceMirrorAccess);

    // Tracer interface to hardware
    static void tracerClone();
    static void tracerHandleAccess(uint32_t addr, uint32_t data, 
            uint32_t flags, uint32_t& retVal);

    // Enable/Disable
    static bool enableHw(const char* hwName, bool enable);
    static void disableAll();

    // Get mirror memory for address
    static uint8_t* getMirrorMemForAddr(uint32_t addr);

    // Setup from Json
    static void setupFromJson(const char* jsonKey, const char* hwJson);

private:

    // Hardware slots
    static const int MAX_HARDWARE = 10;
    static const int MAX_HARDWARE_NAME_LEN = 100;
    static HwBase* _pHw[MAX_HARDWARE];
    static int _numHardware;

    // Bus socket we're attached to
    static int _busSocketId;
    static BusSocketInfo _busSocketInfo;

    // Comms socket we're attached to and setup info
    static int _commsSocketId;
    static CommsSocketInfo _commsSocketInfo;

    // Memory emulation mode
    static bool _memoryEmulationMode;

    // Mirror memory mode
    static bool _mirrorMode;

    // Paging mode
    static bool _memoryPagingEnable;

    // Opcode injection mode
    static bool _opcodeInjectEnable;

    // Default hardware list (to add if no hardware specified)
    static const char* _pDefaultHardwareList;

#ifdef DEBUG_IO_ACCESS
    // Debug collection of IO accesses
    static const int DEBUG_MAX_IO_PORT_ACCESSES = 500;
    static DebugIOPortAccess _debugIOPortBuf[DEBUG_MAX_IO_PORT_ACCESSES];
    static RingBufferPosn _debugIOPortPosn;
#endif

    // Handle messages (telling us to start/stop)
    static bool handleRxMsg(const char* pCmdJson, const uint8_t* pParams, int paramsLen,
                    char* pRespJson, int maxRespLen);
                    
    // Reset complete callback
    static void busActionCompleteStatic(BR_BUS_ACTION actionType, BR_BUS_ACTION_REASON reason);

    // Wait interrupt handler
    static void handleWaitInterruptStatic(uint32_t addr, uint32_t data, 
            uint32_t flags, uint32_t& retVal);
};
