// Bus Raider Hardware Manager
// Rob Dobson 2019

#pragma once

#include <string.h> 
#include "HwBase.h"

// #define DEBUG_IO_ACCESS 1

class BusControl;

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
    // Constructor
    HwManager(BusControl& busControl);

    // Get code snippet to setup hardware
    uint32_t getSnippetToSetupHw(uint32_t codeLocation, uint8_t* pCodeBuffer, uint32_t codeMaxlen);

//     // Init
//     void init();

//     // Service
//     void service();

//     // Manage hardware
//     static void addHardwareElementStatic(HwBase* pHw);

//     // Memory emulation mode 
//     void setMemoryEmulationMode(bool val);
//     bool isEmulatingMemory()
//     {
//         return _isEmulatingMemory;
//     }

//     // Memory paging enable 
//     void setMemoryPagingEnable(bool val);
//     bool getMemoryPagingEnable()
//     {
//         return _memoryPagingEnable;
//     }

//     // Opcode inject enable 
//     void setOpcodeInjectEnable(bool val);
//     bool getOpcodeInjectEnable()
//     {
//         return _opcodeInjectEnable;
//     }

//     // Page out RAM/ROM for opcode injection
//     void pageOutForInjection(bool pageOut);

//     // Mirror memory mode - read/write to hardware enacts on mirror
//     void setMirrorMode(bool val);
//     void mirrorClone();

//     // Block access to hardware
//     uint32_t getMaxAddress();
//     BR_RETURN_TYPE blockWrite(uint32_t addr, const uint8_t* pBuf, uint32_t len, 
//                 bool busRqAndRelease, bool iorq, bool forceMirrorAccess);
//     BR_RETURN_TYPE blockRead(uint32_t addr, uint8_t* pBuf, uint32_t len, 
//                 bool busRqAndRelease, bool iorq, bool forceMirrorAccess);

//     // Tracer interface to hardware
//     void tracerClone();
//     void tracerHandleAccess(uint32_t addr, uint32_t data, 
//             uint32_t flags, uint32_t& retVal);

//     // Enable/Disable
//     bool enableHw(const char* hwName, bool enable);
//     void disableAll();
//     void configureHw(const char* hwName, const char* hwDefJson);

//     // Get mirror memory for address
//     uint8_t* getMirrorMemForAddr(uint32_t addr);

    // // Setup from Json
    // void setupFromJson(const char* jsonKey, const char* hwJson);

//     // Get bus socket ID
//     int getBusSocketId()
//     {
//         return _busSocketId;
//     }

//     // Check if bus can be accessed directly
//     bool busAccessAvailable();
// private:

//     // Singleton instance
//     static HwManager* _pThisInstance;

//     // Command Handler
//     CommandHandler& _commandHandler;

    // Bus access
    BusControl& _busControl;

    // // Hardware slots
    // static const int MAX_HARDWARE = 10;
    // static const int MAX_HARDWARE_NAME_LEN = 100;
    // HwBase* _pHw[MAX_HARDWARE];
    // int _numHardware;

//     // Bus socket we're attached to
//     int _busSocketId;   

//     // Comms socket we're attached to and setup info
//     int _commsSocketId;

//     // Memory emulation mode
//     bool _isEmulatingMemory;

//     // Mirror memory mode
//     bool _mirrorMode;

//     // Paging mode
//     bool _memoryPagingEnable;

//     // Opcode injection mode
//     bool _opcodeInjectEnable;

    // Default hardware list (to add if no hardware specified)
    static const char* _pDefaultHardwareList;

// #ifdef DEBUG_IO_ACCESS
//     // Debug collection of IO accesses
//     static const int DEBUG_MAX_IO_PORT_ACCESSES = 500;
//     DebugIOPortAccess _debugIOPortBuf[DEBUG_MAX_IO_PORT_ACCESSES];
//     RingBufferPosn _debugIOPortPosn;
// #endif

//     // Handle messages (telling us to start/stop)
//     static bool handleRxMsgStatic(void* pObject, const char* pCmdJson, const uint8_t* pParams, unsigned paramsLen,
//                     char* pRespJson, unsigned maxRespLen);
//     bool handleRxMsg(const char* pCmdJson, const uint8_t* pParams, unsigned paramsLen,
//                     char* pRespJson, unsigned maxRespLen);
                    
//     // Reset complete callback
//     static void busReqAckedStatic(void* pObject, BR_BUS_ACTION actionType, 
//                 BR_BUS_REQ_REASON reason, BR_RETURN_TYPE rslt);
//     void busReqAcked(BR_BUS_ACTION actionType, BR_BUS_REQ_REASON reason, 
//                 BR_RETURN_TYPE rslt);

//     // Wait interrupt handler
//     static void handleWaitInterruptStatic(void* pObject, uint32_t addr, uint32_t data, 
//             uint32_t flags, uint32_t& retVal);
//     void handleWaitInterrupt(uint32_t addr, uint32_t data, 
//             uint32_t flags, uint32_t& retVal);
};
