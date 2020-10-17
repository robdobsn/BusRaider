// Bus Raider
// Rob Dobson 2019

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "TargetCPU.h"
#include "BusSocketInfo.h"
#include "TargetRegisters.h"
#include "TargetBreakpoints.h"
#include "TargetProgrammer.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Defs
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class BusControl;

class TargetController
{
public:

    TargetController(BusControl& busAccess);

    // Init
    void init();
    void service();
    void enable(bool en, bool waitHold);
    void clear()
    {
        _targetProgrammer.clear();
    }

    // Step
    void stepInto();
    void stepRun();
    void stepOver();
    void stepTo(uint32_t toAddr);
    // static void stepPause();

    // Regs
    Z80Registers& getRegs()
    {
        return _z80Registers;
    };
    void getRegsFormatted(char* pBuf, int len);
    
    // Get mode
    bool isPaused(); 
    bool isTrackingActive();

    // Target programming
    TargetProgrammer& targetProgrammer()
    {
        return _targetProgrammer;
    }
    void programmingStart(bool execAfterProgramming);

    // Complete the process of programming the target
    void completeTargetProgram();

    // Reset / exec target
    void targetReset();
    void targetExec();

    // Register injection and code snippet generation
    void startSetRegisterSequence(Z80Registers* pRegs = NULL);
    // void startGetRegisterSequence();
    static const int MAX_CODE_SNIPPET_LEN = 100;
    int getInstructionsToSetRegs(Z80Registers& regs, uint8_t* pCodeBuffer, uint32_t codeMaxlen);

    // Disassembly
    static const int MAX_Z80_DISASSEMBLY_LINE_LEN = 300;

    // Step mode
    enum STEP_MODE_TYPE
    {
        STEP_MODE_STEP_PAUSED,
        STEP_MODE_STEP_INTO,
        STEP_MODE_STEP_OVER,
        STEP_MODE_RUN
    };

    // Step modes
    STEP_MODE_TYPE getStepMode()
    {
        return _stepMode;
    }

    bool isStepPaused()
    {
        return (_stepMode == STEP_MODE_STEP_PAUSED);
    }

    // Breakpoints
    void enableBreakpoints(bool en)
    {
        _breakpoints.enableBreakpoints(en);
    }
    void enableBreakpoint(int idx, bool enabled)
    {
        _breakpoints.enableBreakpoint(idx, enabled);
    }
    void setBreakpointMessage(int idx, const char* hitMessage)
    {
        _breakpoints.setBreakpointMessage(idx, hitMessage);
    }
    void setBreakpointPCAddr(int idx, uint32_t pcVal)
    {
        _breakpoints.setBreakpointPCAddr(idx, pcVal);
    }
    void setFastBreakpoint(uint32_t addr, bool en)
    {
        _breakpoints.setFastBreakpoint(addr, en);
    }
    void clearFastBreakpoints()
    {
        _breakpoints.clearFastBreakpoints();
    }

    bool allowHeartbeat()
    {
        // TODO 2020
        return true;
    }

private:
    // Can't turn off mid-injection so store flag to indicate disable pending
    bool _disablePending;

    // Target reset pending
    bool _targetResetPending;

    // Page out for injection active
    bool _pageOutForInjectionActive;

    // Pending actions
    bool _busActionPendingProgramTarget;
    bool _busActionPendingExecAfterProgram;
    bool _busActionCodeWrittenAtResetVector;
    
    // Step mode
    STEP_MODE_TYPE _stepMode;

    // Step over
    uint32_t _stepOverPCValue;

    // Code snippet
    uint32_t _snippetLen;
    uint32_t _snippetPos;
    uint8_t _snippetBuf[MAX_CODE_SNIPPET_LEN];
    uint32_t _snippetWriteIdx;

    // Injection type
    bool _setRegs;

    // Mirror memory requirements
    bool _postInjectMemoryMirror;

    // Request grab display
    bool _requestDisplayWhileStepping;

    // Registers
    Z80Registers _z80Registers;

    // Bus control
    BusControl& _busAccess;

    // Target Programmer
    TargetProgrammer _targetProgrammer;
    
    // Bus socket we're attached to and setup info
    int _busSocketId;
    BusSocketInfo _busSocketInfo;

    // Register get/set
    enum OPCODE_INJECT_PROGRESS
    {
        OPCODE_INJECT_GENERAL,
        OPCODE_INJECT_GRAB_MEMORY,
        OPCODE_INJECT_DONE
    };
    OPCODE_INJECT_PROGRESS handleRegisterGet(uint32_t addr, uint32_t data, uint32_t flags, uint32_t& retVal);
    OPCODE_INJECT_PROGRESS handleRegisterSet(uint32_t& retVal);

    void handleStepOverBkpts(uint32_t addr, uint32_t data, uint32_t flags, uint32_t& retVal);
    void handleTrackerIdle(uint32_t addr, uint32_t data, uint32_t flags, uint32_t& retVal);
    void handleInjection(uint32_t addr, uint32_t data, uint32_t flags, uint32_t& retVal);

    // Utils
    bool isPrefixInstruction(uint32_t instr);
    bool trackPrefixedInstructions(uint32_t flags, uint32_t data, uint32_t retVal);
    void store16BitVal(uint8_t arry[], int offset, uint16_t val);
    bool handlePendingDisable();

    // Bus action complete callback
    static void busActionCompleteStatic(void* pObject, BR_BUS_ACTION actionType, BR_BUS_ACTION_REASON reason);
    void busActionComplete(BR_BUS_ACTION actionType, BR_BUS_ACTION_REASON reason);

    // Wait interrupt handler
    static void handleWaitInterruptStatic(void* pObject, uint32_t addr, uint32_t data, 
            uint32_t flags, uint32_t& retVal);
    void handleWaitInterrupt(uint32_t addr, uint32_t data, 
            uint32_t flags, uint32_t& retVal);

    // Prefix tracking - element 0 is oldest
    bool _prefixTracker[2];

    // Target state acquisition
    enum TARGET_STATE_ACQ {
        TARGET_STATE_ACQ_NONE,
        TARGET_STATE_ACQ_INJECT_IF_NEW_INSTR,
        TARGET_STATE_ACQ_INJECTING,
        TARGET_STATE_ACQ_POST_INJECT
    };
    TARGET_STATE_ACQ _targetStateAcqMode;

    // Breakpoints
    TargetBreakpoints _breakpoints;

    // Machine heartbeat cycle counter
    uint32_t _machineHeartbeatCounter;

    // Debug
    static const int MAX_BYTES_IN_INSTR = 10;
    uint8_t _debugInstrBytes[MAX_BYTES_IN_INSTR];
    uint32_t _debugInstrBytePos;
};
