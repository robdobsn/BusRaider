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
#include "TargetImager.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Defs
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Test processor is reading from the data bus (inc reading an ISR vector)
// and result is valid then put the returned data onto the bus
#define CTRL_BUS_IS_READ(x) ((((x & BR_RD_BAR_MASK) == 0) && \
                             (((x & BR_MREQ_BAR_MASK) == 0) || ((x & BR_IORQ_BAR_MASK) == 0))) || \
                     (((x & BR_IORQ_BAR_MASK) == 0) && ((x & BR_V20_M1_BAR_MASK) == 0)))

// Test processor is in wait and signals are valid
#define CTRL_BUS_IS_WAIT(x) (((x & BR_WAIT_BAR_MASK) == 0) && ((x & BR_BUSACK_BAR_MASK) != 0) && \
            (((x & BR_RD_BAR_MASK) == 0) || ((x & BR_WR_BAR_MASK) == 0)) && \
            (((x & BR_IORQ_BAR_MASK) == 0) || ((x & BR_MREQ_BAR_MASK) == 0)))

class BusControl;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TargetControl
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class TargetControl
{
public:
    // Constructor
    TargetControl(BusControl& busControl);

    // Init
    void init();

    // Service
    void service(bool dontStartAnyNewBusActions);

    // Clear
    void clear()
    {
        _targetImager.clear();
    }

    // Suspend
    void suspend(bool suspend);

    // Debugger
    bool debuggerBreak();
    void debuggerContinue();
    bool debuggerStepIn();

    // Set callback on bus access
    void registerCallbacks(BusAccessCBFnType* pBusAccessCB, 
                BusReqAckedCBFnType* pBusReqAckedCB, 
                void* pObject)
    {
        _pBusAccessCB = pBusAccessCB;
        _pBusReqAckedCB = pBusReqAckedCB;
        _pBusCBObject = pObject;
    }

    // Target programming
    TargetImager& targetImager()
    {
        return _targetImager;
    }
    void programmingStart(bool execAfterProgramming, bool enterDebugger);

    // Bus request
    bool reqBus(BR_BUS_REQ_REASON busReqReason,
            uint32_t socketIdx);

    // Check if machine heartbeat is allowed
    bool allowHeartbeat()
    {
        // TODO 2020 check if this is sufficient test
        return !isDebugging();
    }

    // Check if debugging
    bool isDebugging()
    {
        return _debuggerState != DEBUGGER_STATE_FREE_RUNNING;
    }

    // Check if held at WAIT
    bool isHeldAtWait()
    {
        return _cycleHeldInWaitState;
    }

    // Regs
    Z80Registers& getRegs()
    {
        return _z80Registers;
    };
    void getRegsFormatted(char* pBuf, int len);
    void getRegsJSON(char* pBuf, int len);

    // Disassemble
    void disassemble(uint32_t numLines, char* pResp, uint32_t respMaxLen, 
            uint32_t& numBytesIn1StInstruction);

    // INT, NMI and RESET requests
    void targetReset();
    void targetINT();
    void targetNMI();

private:
    // Bus control
    BusControl& _busControl;

    // Target Programmer
    TargetImager _targetImager;

    // Suspended
    bool _isSuspended;

    // Programming state
    bool _programmingStartPending;
    bool _programmingDoExec;
    bool _programmingEnterDebugger;
    void programmingClear();
    void programmingWrite();
    void programExec(bool codeAtResetVector);
    static const uint32_t MAX_WAIT_FOR_BUSAK_US = 100000;

    // Cycle request stat
    enum CYCLE_REQ_STATE
    {
        CYCLE_REQ_STATE_NONE,
        CYCLE_REQ_STATE_PENDING,
        CYCLE_REQ_STATE_ASSERTED
    };

    class BusReqInfo
    {
    public:
        BusReqInfo()
        {
            clear();
        }
        void clear()
        {
            _busReqAckedCB = NULL;
            _pObject = NULL;
            _socketIdx = 0;
            _reqState = CYCLE_REQ_STATE_NONE;
            _busReqUs = 0;
            _busReqAssertedUs = 0;
            _busRqReason = BR_BUS_REQ_REASON_GENERAL;
        }
        void pending(BR_BUS_REQ_REASON busReqReason,
            uint32_t socketIdx,
            BusReqAckedCBFnType* completeCB, 
            void* pObject)
        {
            _reqState = CYCLE_REQ_STATE_PENDING;
            _busReqAckedCB = completeCB;
            _pObject = pObject;
            _socketIdx = socketIdx;
            _busReqUs = micros();
            _busReqAssertedUs = 0;
            _busRqReason = busReqReason;
        }
        void asserted()
        {
            _busReqAssertedUs = micros();
            _reqState = CYCLE_REQ_STATE_ASSERTED;
        }
        void complete()
        {
            _reqState = CYCLE_REQ_STATE_NONE;
        }
        BusReqAckedCBFnType* _busReqAckedCB;
        void* _pObject;
        uint32_t _socketIdx;
        CYCLE_REQ_STATE _reqState;
        uint32_t _busReqUs;
        uint32_t _busReqAssertedUs;
        BR_BUS_REQ_REASON _busRqReason;
    };
    BusReqInfo _busReqInfo;

    // RESET, INT, NMI info
    class SignalRequestInfo
    {
    public:
        SignalRequestInfo()
        {
            clear();
        }
        void clear()
        {
            _isPending = false;
        }
        bool _isPending;
    };
    SignalRequestInfo _reqINTInfo;
    SignalRequestInfo _reqNMIInfo;
    SignalRequestInfo _reqRESETInfo;

    // Cycle Request Handling
    bool _cycleHeldInWaitState;
    bool _cycleWaitForReadCompletionRequired;
    void cycleClear();
    void cycleService(bool dontStartAnyNewBusActions);
    void cycleSuspend(bool suspend);
    void cycleCheckWait();
    void cycleSetupForFastWait();
    void cycleFullWaitProcessing(uint32_t rawBusVals);
    bool cycleWaitForReadCompletion();
    void cycleHandleHeldInWait();
    void cycleCheckProgPending();
    void cycleDoProgIfPending();
    void cycleHandleBusReq();

    // Debugger wait handling
    static const uint32_t MAX_WAIT_FOR_DEBUG_HELD_AT_WAIT_MS = 100;
    bool debuggerHandleWaitCycle(uint32_t addr, uint32_t data, uint32_t flags);

    // Memory wait high address watch table
    static const uint32_t MEM_WAIT_HIGH_ADDR_WATCH_LEN = 256;
    uint8_t _memWaitHighAddrWatch[MEM_WAIT_HIGH_ADDR_WATCH_LEN];

    // Callback on bus access
    BusAccessCBFnType* _pBusAccessCB;
    BusReqAckedCBFnType* _pBusReqAckedCB;
    void* _pBusCBObject;

    // Debugger state
    enum DebuggerState
    {
        DEBUGGER_STATE_FREE_RUNNING,
        DEBUGGER_STATE_AT_BREAK,
    };

    // Debug state
    volatile DebuggerState _debuggerState;

    // Step mode
    enum DebuggerStepMode
    {
        DEBUGGER_STEP_MODE_NONE,
        DEBUGGER_STEP_MODE_STEP_INTO
    };

    // Debug step mode
    volatile DebuggerStepMode _debuggerStepMode;

    // Registers
    Z80Registers _z80Registers;

    // void cycleHandleReadRelease();
    // // State
    // bool _waitIsActive;
    // bool _cycleReadInProgress;

// public:
//     void service();
//     void enable(bool en, bool waitHold);
//     void clear()
//     {
//         _targetImager.clear();
//     }

//     // Step
//     void stepInto();
//     void stepRun();
//     void stepOver();
//     void stepTo(uint32_t toAddr);
//     // static void stepPause();
    
//     // Get mode
//     bool isPaused(); 
//     bool isTrackingActive();

//     // Reset / exec target
//     void targetReset();
//     void targetExec();

//     // Register injection and code snippet generation
//     void startSetRegisterSequence(Z80Registers* pRegs = NULL);
//     // void startGetRegisterSequence();
//     static const int MAX_CODE_SNIPPET_LEN = 100;
//     int getInstructionsToSetRegs(Z80Registers& regs, uint8_t* pCodeBuffer, uint32_t codeMaxlen);

//     // Disassembly
//     static const int MAX_Z80_DISASSEMBLY_LINE_LEN = 300;

//     // Step mode
//     enum STEP_MODE_TYPE
//     {
//         STEP_MODE_STEP_PAUSED,
//         STEP_MODE_STEP_INTO,
//         STEP_MODE_STEP_OVER,
//         STEP_MODE_RUN
//     };

//     // Step modes
//     STEP_MODE_TYPE getStepMode()
//     {
//         return _stepMode;
//     }

//     bool isStepPaused()
//     {
//         return (_stepMode == STEP_MODE_STEP_PAUSED);
//     }

//     // Breakpoints
//     void enableBreakpoints(bool en)
//     {
//         _breakpoints.enableBreakpoints(en);
//     }
//     void enableBreakpoint(int idx, bool enabled)
//     {
//         _breakpoints.enableBreakpoint(idx, enabled);
//     }
//     void setBreakpointMessage(int idx, const char* hitMessage)
//     {
//         _breakpoints.setBreakpointMessage(idx, hitMessage);
//     }
//     void setBreakpointPCAddr(int idx, uint32_t pcVal)
//     {
//         _breakpoints.setBreakpointPCAddr(idx, pcVal);
//     }
//     void setFastBreakpoint(uint32_t addr, bool en)
//     {
//         _breakpoints.setFastBreakpoint(addr, en);
//     }
//     void clearFastBreakpoints()
//     {
//         _breakpoints.clearFastBreakpoints();
//     }

//     bool allowHeartbeat()
//     {
//         // TODO 2020
//         return true;
//     }

// private:
//     // Bus control
//     BusControl& _busControl;

//     // Target Programmer
//     TargetImager _targetImager;

//     // Can't turn off mid-injection so store flag to indicate disable pending
//     bool _disablePending;

//     // Target reset pending
//     bool _targetResetPending;

//     // Page out for injection active
//     bool _pageOutForInjectionActive;

//     // Pending actions
//     bool _busActionPendingProgramTarget;
//     bool _busActionPendingExecAfterProgram;
//     bool _busActionCodeWrittenAtResetVector;
    
//     // Step mode
//     STEP_MODE_TYPE _stepMode;

//     // Step over
//     uint32_t _stepOverPCValue;

//     // Code snippet
//     uint32_t _snippetLen;
//     uint32_t _snippetPos;
//     uint8_t _snippetBuf[MAX_CODE_SNIPPET_LEN];
//     uint32_t _snippetWriteIdx;

//     // Injection type
//     bool _setRegs;

//     // Mirror memory requirements
//     bool _postInjectMemoryMirror;

//     // Request grab display
//     bool _requestDisplayWhileStepping;
    
//     // Bus socket we're attached to and setup info
//     int _busSocketId;
//     BusSocketInfo _busSocketInfo;

//     // Register get/set
//     enum OPCODE_INJECT_PROGRESS
//     {
//         OPCODE_INJECT_GENERAL,
//         OPCODE_INJECT_GRAB_MEMORY,
//         OPCODE_INJECT_DONE
//     };
//     OPCODE_INJECT_PROGRESS handleRegisterGet(uint32_t addr, uint32_t data, uint32_t flags, uint32_t& retVal);
//     OPCODE_INJECT_PROGRESS handleRegisterSet(uint32_t& retVal);

//     void handleStepOverBkpts(uint32_t addr, uint32_t data, uint32_t flags, uint32_t& retVal);
//     void handleTrackerIdle(uint32_t addr, uint32_t data, uint32_t flags, uint32_t& retVal);
//     void handleInjection(uint32_t addr, uint32_t data, uint32_t flags, uint32_t& retVal);

//     // Utils
//     bool isPrefixInstruction(uint32_t instr);
//     bool trackPrefixedInstructions(uint32_t flags, uint32_t data, uint32_t retVal);
//     void store16BitVal(uint8_t arry[], int offset, uint16_t val);
//     bool handlePendingDisable();

//     // Bus action active callback
//     static void busReqAckedStatic(void* pObject, BR_BUS_ACTION actionType, BR_BUS_REQ_REASON reason);
//     void busReqAcked(BR_BUS_ACTION actionType, BR_BUS_REQ_REASON reason);

//     // Wait interrupt handler
//     static void handleWaitInterruptStatic(void* pObject, uint32_t addr, uint32_t data, 
//             uint32_t flags, uint32_t& retVal);
//     void handleWaitInterrupt(uint32_t addr, uint32_t data, 
//             uint32_t flags, uint32_t& retVal);

//     // Prefix tracking - element 0 is oldest
//     bool _prefixTracker[2];

//     // Target state acquisition
//     enum TARGET_STATE_ACQ {
//         TARGET_STATE_ACQ_NONE,
//         TARGET_STATE_ACQ_INJECT_IF_NEW_INSTR,
//         TARGET_STATE_ACQ_INJECTING,
//         TARGET_STATE_ACQ_POST_INJECT
//     };
//     TARGET_STATE_ACQ _targetStateAcqMode;

//     // Breakpoints
//     TargetBreakpoints _breakpoints;

//     // Machine heartbeat cycle counter
//     uint32_t _machineHeartbeatCounter;

//     // Debug
//     static const int MAX_BYTES_IN_INSTR = 10;
//     uint8_t _debugInstrBytes[MAX_BYTES_IN_INSTR];
//     uint32_t _debugInstrBytePos;
};
