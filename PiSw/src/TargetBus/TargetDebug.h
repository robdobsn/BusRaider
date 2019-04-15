// Bus Raider
// Rob Dobson 2019

#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include "../Machines/McBase.h"
#include "../System/logging.h"
#include "../TargetBus/BusAccess.h"
#include "../TargetBus/TargetRegisters.h"

typedef void SendRemoteDebugProtocolMsgType(const char* pStr, const char* rdpMessageIdStr);

class Breakpoint
{
public:
    static const int MAX_HIT_MSG_LEN = 100;
    bool enabled;
    char hitMessage[MAX_HIT_MSG_LEN];
    uint32_t pcValue;

    Breakpoint()
    {
        enabled = false;
        hitMessage[0] = 0;
        pcValue = 0;
    }
};

class TargetDebug
{
public:
    TargetDebug();

    // Service, Setup etc
    static TargetDebug* get();
    void service();
    void setup(McBase* pTargetMachine);

    // Execution control
    void pause();
    void step(bool stepOver);
    void release();
    bool isPaused();

    // Handle a debugger command
    bool handleDebuggerCommand(char* pCommand, char* pResponse, int maxResponseLen);

    // Handle wait-state interrupt
    uint32_t handleWaitInterrupt([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, 
            [[maybe_unused]] uint32_t flags, [[maybe_unused]] uint32_t retVal);

    // Comms callback
    void setSendRemoteDebugProtocolMsgCallback(SendRemoteDebugProtocolMsgType* pSendRemoteDebugProtocolMsgCallback)
    {
        _pSendRemoteDebugProtocolMsgCallback = pSendRemoteDebugProtocolMsgCallback;
    }

    // Register injection and code snippet generation
    void startSetRegisterSequence(Z80Registers* pRegs = NULL);

    // Inform target debug that target programming is started
    void targetProgrammingStarted();

private:

    // Debug Mode
    // Keeps track of the current mode the debugger is in
    enum DEBUG_MODE {
        DEBUG_MODE_NONE,
        DEBUG_MODE_STEP_INTO,
        DEBUG_MODE_STEP_OVER,
        DEBUG_MODE_RUN
    };
    DEBUG_MODE _debugMode;

    // Target state acquisition
    // This is a state-machine which controls:
    // - the sequence of opcode injection to get registers
    // - paging of hardware that requires bus access (e.g. 512K RAM/ROM)
    // - memory grab (into RAMEmulation) for debug access to memory
    void targetStateAcqClear();
    void targetStateAcqStart(bool leaveInPause);
    void targetStateAcqRelease(DEBUG_MODE debugMode);
    void targetStateAcqService();
    uint32_t targetStateAcqWaitISR([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, 
            [[maybe_unused]] uint32_t flags, [[maybe_unused]] uint32_t retVal);
    
    enum TARGET_STATE_ACQ_STATE {
        TARGET_STATE_ACQ_NONE,
        TARGET_STATE_ACQ_PRE_INJECT,
        TARGET_STATE_ACQ_INJECT,
        TARGET_STATE_ACQ_REGISTER_GRAB,
        TARGET_STATE_ACQ_POST_INJECT
    };
    TARGET_STATE_ACQ_STATE _targetStateAcqMode;

    bool commandMatch(const char* s1, const char* s2);
    void enableBreakpoint(int idx, bool enabled);
    void setBreakpointMessage(int idx, const char* hitMessage);
    void setBreakpointPCAddr(int idx, uint32_t pcVal);
    void startGetRegisterSequence();
    void handleRegisterGet(uint32_t addr, uint32_t data, uint32_t flags, uint32_t& retVal);
    void handleRegisterSet(uint32_t& retVal);
    bool isPrefixInstruction(uint32_t instr);
    bool procDebuggerLine(char* pCmd, char* pResponse, int maxResponseLen);

    // Machine being debugged
    McBase* _pTargetMachine;

    // Callback to send debug frames
    static SendRemoteDebugProtocolMsgType* _pSendRemoteDebugProtocolMsgCallback;

    // Registers
    Z80Registers _z80Registers;

    // Debug mode initialized indicator
    bool _debugInitalized;

    // // Debug in single-step mode
    // bool _debugInCPUStep;

    // Flags to help deal with prefixed instructions when single-stepping
    bool _thisInstructionIsPrefixed;
    bool _lastInstructionWasPrefixed;

    // // Flag indicating bus has been requested for full-memory read
    // bool _busControlRequestedForMemGrab;

    // Memory access and bus control
    // void grabMemoryAndReleaseBusRq(uint8_t* pBuf, uint32_t bufLen);

    // Current MREQ monitor mode when starting register set
    bool _instrWaitRestoreNeeded;
    bool _instrWaitCurMode;
    uint32_t _instrClockCurFreqHz;

    // Register mode
    enum REGISTER_MODE {
        REGISTER_MODE_NONE,
        REGISTER_MODE_GET,
        REGISTER_MODE_SET,
        REGISTER_MODE_UNPAGE,
    };
    REGISTER_MODE _registerMode;

    bool _registerPrevInstrComplete;
    int _registerQueryWriteIndex;
    unsigned int _registerModeStep;
    uint8_t _registerSetBuffer[MAX_REGISTER_SET_CODE_LEN];
    uint32_t _registerSetCodeLen;

    // Limit on data sent back
    static const int MAX_MEM_DUMP_LEN = 1024;

    // Breakpoints
    static const int MAX_BREAKPOINTS = 100;
    static bool _breakpointsEnabled;
    static int _breakpointNumEnabled;
    static int _breakpointIdxsToCheck[MAX_BREAKPOINTS];
    static Breakpoint _breakpoints[MAX_BREAKPOINTS];
    static bool _breakpointHitFlag;
    static int _breakpointHitIndex;

    // Step-over address
    static bool _stepOverEnabled;
    static uint32_t _stepOverPCValue;
    static bool _stepOverHit;

};

extern TargetDebug __targetDebug;
