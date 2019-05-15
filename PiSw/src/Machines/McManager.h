// Bus Raider Machine Manager
// Rob Dobson 2018

#pragma once

#include <string.h> 
#include "McBase.h"
#include "../System/logging.h"
#include "../System/DisplayBase.h"
#include "../TargetBus/BusAccess.h"
#include "../CommandInterface/CommandHandler.h"

class McManager
{
public:
    // Init
    static void init(DisplayBase* pDisplay);

    // Service
    static void service();

    // Manage machines
    static void add(McBase* pMachine);
    static bool setupMachine(const char* mcJson);
    static bool setMachineByName(const char* mcName);

    // Machine access
    static int getNumMachines();
    static McBase* getMachine();
    static const char* getMachineJSON();
    static int getMachineClock();
    static const char* getMachineName();
    static int getDisplayRefreshRate();
    static const char* getMachineForFileType(const char* fileType);

    // Display updates
    static void displayRefresh();

    // Current machine descriptor table
    static McDescriptorTable* getDescriptorTable();

    // Handle communication with machine
    static void handleRxCharFromTarget(const uint8_t* pRxChars, int rxLen);
    static int getNumCharsReceivedFromHost();
    static int getCharsReceivedFromHost(uint8_t* pBuf, int bufMaxLen);
    static void sendKeyCodeToTarget(int asciiCode);

    // Target programming
    static void handleTargetProgram(bool execAfterProgramming);

    // Target control
    static void targetReset();

    // Handle target files
    static bool handleTargetFile(const char* rxFileInfo, const uint8_t* pData, int dataLen);

    static void targetIrq(int durationUs = -1);

private:
    static McDescriptorTable defaultDescriptorTable;

    // Bus socket we're attached to and setup info
    static int _busSocketId;
    static BusSocketInfo _busSocketInfo;

    // Comms socket we're attached to and setup info
    static int _commsSocketId;
    static CommsSocketInfo _commsSocketInfo;

    // Handle messages (telling us to start/stop)
    static bool handleRxMsg(const char* pCmdJson, const uint8_t* pParams, int paramsLen,
                    char* pRespJson, int maxRespLen);

    // Exec program on target
    static void targetExec();
    
    // Bus action complete callback
    static void busActionCompleteStatic(BR_BUS_ACTION actionType, BR_BUS_ACTION_REASON reason);

    // Wait interrupt handler
    static void handleWaitInterruptStatic(uint32_t addr, uint32_t data, 
            uint32_t flags, uint32_t& retVal);
            
    // Characters received from the host
    static const int MAX_RX_HOST_CHARS = 2000;
    static uint8_t _rxHostCharsBuffer[MAX_RX_HOST_CHARS+1];
    static int _rxHostCharsBufferLen;

    // Display
    static DisplayBase* _pDisplay;

    // Machines
    static const int MAX_MACHINES = 10;
    static const int MAX_MACHINE_NAME_LEN = 100;
    static McBase* _pMachines[MAX_MACHINES];
    static int _numMachines;
    static char _currentMachineName[MAX_MACHINE_NAME_LEN];
    static McBase* _pCurMachine;

    // Pending actions
    static bool _busActionPendingProgramTarget;
    static bool _busActionPendingExecAfterProgram;
    static bool _busActionPendingDisplayRefresh;
    static bool _busActionCodeWrittenAtResetVector;

    // Display refresh
    #define REFRESH_RATE_WINDOW_SIZE_MS 1000
    static uint32_t _refreshCount;
    static int _refreshRate;
    static uint32_t _refreshLastUpdateUs;
    static uint32_t _refreshLastCountResetUs;

};
