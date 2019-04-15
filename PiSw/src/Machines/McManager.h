// Bus Raider Machine Manager
// Rob Dobson 2018

#pragma once

#include "McBase.h"
#include "../TargetBus/TargetDebug.h"
#include "../System/logging.h"
#include "../CommandInterface/CommandHandler.h"
#include "../System/Display.h"
#include <string.h> 
#include "../TargetBus/StepTester.h"

class StepValidator;

class McManager
{
private:
    static McDescriptorTable defaultDescriptorTable;

private:
   
    // Characters received from the host
    static const int MAX_RX_HOST_CHARS = 2000;
    static uint8_t _rxHostCharsBuffer[MAX_RX_HOST_CHARS+1];
    static int _rxHostCharsBufferLen;

    // Command handler
    static CommandHandler* _pCommandHandler;

    // Display
    static Display* _pDisplay;

private:

    static const int MAX_MACHINES = 10;
    static const int MAX_MACHINE_NAME_LEN = 100;
    static McBase* _pMachines[MAX_MACHINES];
    static int _numMachines;
    static int _curMachineIdx;
    static int _curMachineSubType;
    static McBase* _pCurMachine;

public:
    // Init
    static void init(CommandHandler* pCommandHandler, Display* pDisplay);

    // Service
    static void service();

    // Manage machines
    static void add(McBase* pMachine);
    static bool setMachineIdx(int mcIdx, int mcSubType, bool forceUpdate);
    static bool setMachineByName(const char* mcName);
    static bool setMachineOpts(const char* mcOpts);

    // Machine access
    static int getNumMachines();
    static McBase* getMachine();
    static const char* getMachineJSON();
    static int getMachineClock();

    // Display updates
    static void displayRefresh();

    // Current machine descriptor table
    static McDescriptorTable* getDescriptorTable();

    // Handle communication with machine
    static void handleRxCharFromTarget(const uint8_t* pRxChars, int rxLen);
    static int getNumCharsReceivedFromHost();
    static int getCharsReceivedFromHost(uint8_t* pBuf, int bufMaxLen);
    static void sendKeyCodeToTarget(int asciiCode);

    // Debug message handling
    static void sendRemoteDebugProtocolMsg(const char* pStr, const char* rdpMessageIdStr);
    static bool debuggerCommand(char* pCommand, char* pResponse, int maxResponseLen);

    // Target programming
    static void handleTargetProgram(bool resetAfterProgramming, bool addWaitOnInstruction,
                bool holdInPause, bool forceSetRegsByInjection);

    // Target control
    static void targetReset(bool restoreWaitDefaults, bool holdInReset);
    static void targetPause();
    static void targetRelease();
    static void targetClearAllIO();
    static void targetStep(bool stepOver);
    static bool targetIsPaused();
    static bool targetBusUnderPiControl();

    // Memory access
    static bool blockWrite(uint32_t addr, const uint8_t* pBuf, uint32_t len, bool busRqAndRelease, bool iorq);
    static bool blockRead(uint32_t addr, uint8_t* pBuf, uint32_t len, bool busRqAndRelease, bool iorq);

    // Handle target files
    static void handleTargetFile(const char* rxFileInfo, const uint8_t* pData, int dataLen);

    // Handle commands
    static void handleCommand(const char* pCmdJson, 
                    const uint8_t* pParams, int paramsLen,
                    char* pRespJson, int maxRespLen);

    static void targetIrq(int durationUs = -1);

    // Debug
    static void logDebugMessage(const char* pStr);
    static void logDebugJson(const char* pStr);

private:
    static void targetIrqFromTimer(void* pParam);

    // Handle wait interrupt
    static void busAccessCallback(uint32_t addr, uint32_t data, uint32_t flags, uint32_t& retVal);

    // Step-tester
    static StepTester _stepTester;
    static StepValidator* _pStepValidator;
};
