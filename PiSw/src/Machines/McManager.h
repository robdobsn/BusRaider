// Bus Raider Machine Manager
// Rob Dobson 2018

#pragma once

#include "McBase.h"
#include "../TargetBus/TargetClockGenerator.h"
#include "../Debugger/TargetDebug.h"
#include "../System/ee_printf.h"
#include "../CommandInterface/CommandHandler.h"
#include <string.h> 

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

private:

    static const int MAX_MACHINES = 10;
    static const int MAX_MACHINE_NAME_LEN = 100;
    static McBase* _pMachines[MAX_MACHINES];
    static int _numMachines;
    static int _curMachineIdx;
    static TargetClockGenerator _clockGen;

public:
    // Init
    static void init(CommandHandler* pCommandHandler);

    // Manage machines
    static void add(McBase* pMachine);
    static bool setMachineIdx(int mcIdx, bool forceUpdate);
    static bool setMachineByName(const char* mcName);
    static bool setMachineOpts(const char* mcOpts);

    // Machine access
    static int getNumMachines();
    static McBase* getMachine();
    static McDescriptorTable* getDescriptorTable(int subType);
    static const char* getMachineJSON();

    // Handle communication with machine
    static void handleRxCharFromTarget(const uint8_t* pRxChars, int rxLen);
    static int getNumCharsReceivedFromHost();
    static int getCharsReceivedFromHost(uint8_t* pBuf, int bufMaxLen);
    static void sendKeyCodeToTarget(int asciiCode);

    // Debug message handling
    static void sendDebugMessage(const char* pStr);
    static bool debuggerCommand(const char* pCommand, char* pResponse, int maxResponseLen);

    // Target programming
    static void handleTargetProgram(const char* cmdName);

    // Target control
    static void targetReset();
    static void targetClearAllIO();

    // Memory access
    static bool blockWrite(uint32_t addr, const uint8_t* pBuf, uint32_t len, bool busRqAndRelease, bool iorq);
    static bool blockRead(uint32_t addr, uint8_t* pBuf, uint32_t len, bool busRqAndRelease, bool iorq);

};
