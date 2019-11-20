// Bus Raider
// Rob Dobson 2019

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "TargetCPU.h"
#include "BusAccess.h"
#include "TargetRegisters.h"
#include "TargetBreakpoints.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Defs
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class TargetTracker
{
public:

    // Init
    static void init();
    static void service();
    static void enable(bool en);

    // Step
    static void stepInto();
    static void stepRun();
    static void stepOver();

    // Regs
    static Z80Registers& getRegs()
    {
        return _z80Registers;
    };
    static void getRegsFormatted(char* pBuf, int len);
    
    // Check if bus can be accessed directly
    static bool busAccessAvailable();

    // Get mode
    static bool isPaused(); 
    static bool isTrackingActive();

    // Complete the process of programming the target
    static void completeTargetProgram();

    // Reset target
    static void targetReset();

    // Register injection and code snippet generation
    static void startSetRegisterSequence(Z80Registers* pRegs = NULL);
    // static void startGetRegisterSequence();
    static const int MAX_CODE_SNIPPET_LEN = 100;
    static int getInstructionsToSetRegs(Z80Registers& regs, uint8_t* pCodeBuffer, uint32_t codeMaxlen);

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
    static STEP_MODE_TYPE getStepMode()
    {
        return _stepMode;
    }

    static bool isStepPaused()
    {
        return (_stepMode == STEP_MODE_STEP_PAUSED);
    }

    // Breakpoints
    static void enableBreakpoints(bool en)
    {
        _breakpoints.enableBreakpoints(en);
    }
    static void enableBreakpoint(int idx, bool enabled)
    {
        _breakpoints.enableBreakpoint(idx, enabled);
    }
    static void setBreakpointMessage(int idx, const char* hitMessage)
    {
        _breakpoints.setBreakpointMessage(idx, hitMessage);
    }
    static void setBreakpointPCAddr(int idx, uint32_t pcVal)
    {
        _breakpoints.setBreakpointPCAddr(idx, pcVal);
    }
    static void setFastBreakpoint(uint32_t addr, bool en)
    {
        _breakpoints.setFastBreakpoint(addr, en);
    }
    static void clearFastBreakpoints()
    {
        _breakpoints.clearFastBreakpoints();
    }

private:

    // Can't turn off mid-injection so store flag to indicate disable pending
    static bool _disablePending;

    // Target reset pending
    static bool _targetResetPending;

    // Page out for injection active
    static bool _pageOutForInjectionActive;

    // Step mode
    static STEP_MODE_TYPE _stepMode;

    // Step over
    static uint32_t _stepOverPCValue;

    // Code snippet
    static uint32_t _snippetLen;
    static uint32_t _snippetPos;
    static uint8_t _snippetBuf[MAX_CODE_SNIPPET_LEN];
    static uint32_t _snippetWriteIdx;

    // Injection type
    static bool _setRegs;

    // Mirror memory requirements
    static bool _postInjectMemoryMirror;

    // Request grab display
    static bool _requestDisplayWhileStepping;

    // Registers
    static Z80Registers _z80Registers;

    // Register get/set
    enum OPCODE_INJECT_PROGRESS
    {
        OPCODE_INJECT_GENERAL,
        OPCODE_INJECT_GRAB_MEMORY,
        OPCODE_INJECT_DONE
    };
    static OPCODE_INJECT_PROGRESS handleRegisterGet(uint32_t addr, uint32_t data, uint32_t flags, uint32_t& retVal);
    static OPCODE_INJECT_PROGRESS handleRegisterSet(uint32_t& retVal);

    static void handleStepOverBkpts(uint32_t addr, uint32_t data, uint32_t flags, uint32_t& retVal);
    static void handleTrackerIdle(uint32_t addr, uint32_t data, uint32_t flags, uint32_t& retVal);
    static void handleInjection(uint32_t addr, uint32_t data, uint32_t flags, uint32_t& retVal);

    // Utils
    static bool isPrefixInstruction(uint32_t instr);
    static bool trackPrefixedInstructions(uint32_t flags, uint32_t data, uint32_t retVal);
    static void store16BitVal(uint8_t arry[], int offset, uint16_t val);
    static bool handlePendingDisable();

    // Bus socket we're attached to and setup info
    static int _busSocketId;
    static BusSocketInfo _busSocketInfo;

    // Bus action complete callback
    static void busActionCompleteStatic(BR_BUS_ACTION actionType, BR_BUS_ACTION_REASON reason);

    // Wait interrupt handler
    static void handleWaitInterruptStatic(uint32_t addr, uint32_t data, 
            uint32_t flags, uint32_t& retVal);

    // Prefix tracking - element 0 is oldest
    static bool _prefixTracker[2];

    // Target state acquisition
    enum TARGET_STATE_ACQ {
        TARGET_STATE_ACQ_NONE,
        TARGET_STATE_ACQ_INJECT_IF_NEW_INSTR,
        TARGET_STATE_ACQ_INJECTING,
        TARGET_STATE_ACQ_POST_INJECT
    };
    static TARGET_STATE_ACQ _targetStateAcqMode;

    // Breakpoints
    static TargetBreakpoints _breakpoints;

    // Machine heartbeat cycle counter
    static uint32_t _machineHeartbeatCounter;

    // Debug
    static const int MAX_BYTES_IN_INSTR = 10;
    static uint8_t _debugInstrBytes[MAX_BYTES_IN_INSTR];
    static uint32_t _debugInstrBytePos;
};
