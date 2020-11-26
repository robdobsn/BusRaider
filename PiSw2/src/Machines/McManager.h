// Bus Raider Machine Manager
// Rob Dobson 2018

#pragma once

#include <string.h> 
#include "McBase.h"
#include "logging.h"
#include "DisplayBase.h"
#include "BusControl.h"
#include "CommandHandler.h"

class DisplayBase;

class McManager
{
public:
    // Constuction
    McManager(DisplayBase* pDisplay, CommandHandler& commandHandler, BusControl& busControl);

    // Init
    void init();

    // Service
    void service();

    // Manage machines
    void add(McBase* pMachine);
    bool setupMachine(const char* mcJson);
    bool setMachineByName(const char* mcName);

    // Machine access
    int getNumMachines();
    McBase* getMachine();
    const char* getMachineJSON();
    int getMachineClock();
    const char* getMachineName();
    int getDisplayRefreshRate();
    const char* getMachineForFileType(const char* fileType);
    
    // Handle a key press
    void keyHandler(unsigned char ucModifiers, const unsigned char rawKeys[6]);

    // Display updates
    void displayRefresh();

    // Heartbeat
    void machineHeartbeat();

    // Handle communication with machine
    void hostSerialAddRxCharsToBuffer(const uint8_t* pRxChars, uint32_t rxLen);
    uint32_t hostSerialNumChAvailable();
    uint32_t hostSerialReadChars(uint8_t* pBuf, uint32_t bufMaxLen);
    void sendKeyStrToTargetStatic(const char* pKeyStr);

    // Handle target files
    static bool targetFileHandlerStatic(void* pObject, const char* rxFileInfo, const uint8_t* pData, unsigned dataLen);
    bool targetFileHandler(const char* rxFileInfo, const uint8_t* pData, unsigned dataLen);

    uint32_t getClockFreqInHz()
    {
        return _busControl.clock().getFreqInHz();
    }

    // IRQ on target
    void targetIrq(int durationTStates = -1)
    {
        // Generate a maskable interrupt
        _busControl.sock().reqIRQ(_busSocketId, durationTStates);
    }

private:
    // Singleton
    static McManager* _pMcManager;

    // Display
    DisplayBase* _pDisplay;

    // CommandHandler
    CommandHandler& _commandHandler;

    // Bus control
    BusControl& _busControl;

    // Bus socket we're attached to and setup info
    int _busSocketId;

    // Comms socket we're attached to and setup info
    int _commsSocketId;

    // Handle messages (telling us to start/stop)
    static bool handleRxMsgStatic(void* pObject, const char* pCmdJson, const uint8_t* pParams, unsigned paramsLen,
                    char* pRespJson, unsigned maxRespLen);
    bool handleRxMsg(const char* pCmdJson, const uint8_t* pParams, unsigned paramsLen,
                    char* pRespJson, unsigned maxRespLen);

    // Bus action active callback
    static void busActionActiveStatic(void* pObject, BR_BUS_ACTION actionType, 
            BR_BUS_ACTION_REASON reason, BR_RETURN_TYPE rslt);
    void busActionActive(BR_BUS_ACTION actionType, BR_BUS_ACTION_REASON reason, 
            BR_RETURN_TYPE rslt);

    // Wait interrupt handler
    static void handleWaitInterruptStatic(void* pObject, uint32_t addr, uint32_t data, 
            uint32_t flags, uint32_t& retVal);
            
    // Characters received from the host
    static const uint32_t MAX_RX_HOST_CHARS = 10000;
    uint8_t _rxHostCharsBuffer[MAX_RX_HOST_CHARS+1];
    uint32_t _rxHostCharsBufferLen;

    // Machines
    static const int MAX_MACHINES = 10;
    static const int MAX_MACHINE_NAME_LEN = 100;
    McBase* _pMachines[MAX_MACHINES];
    int _numMachines;
    McBase* _pCurMachine;

    // Pending actions
    bool _busActionPendingDisplayRefresh;

    // Display refresh
    const int REFRESH_RATE_WINDOW_SIZE_MS = 1000;
    uint32_t _refreshCount;
    int _refreshRate;
    uint32_t _refreshLastUpdateMs;
    uint32_t _refreshLastCountResetMs;

    // Screen mirroring
    const int SCREEN_MIRROR_REFRESH_MS = 100;
    bool _screenMirrorOut;
    uint32_t _screenMirrorLastMs;
    const uint32_t SCREEN_MIRROR_FULL_REFRESH_COUNT = 500;
    uint32_t _screenMirrorCount;
};
