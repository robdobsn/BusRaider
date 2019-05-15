// Bus Raider
// Rob Dobson 2019

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "TargetCPU.h"
#include "../TargetBus/BusAccess.h"
#include "../TargetBus/TargetRegisters.h"

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

    // Status
    static void getRegsFormatted(char* pBuf, int len);
    
    // Check if bus can be accessed directly
    static bool busAccessAvailable();

    // Get mode
    static bool isPaused(); 

    // Register injection and code snippet generation
    static void startSetRegisterSequence(Z80Registers* pRegs = NULL);
    // static void startGetRegisterSequence();
    static const int MAX_CODE_SNIPPET_LEN = 100;
    static int getInstructionsToSetRegs(Z80Registers& regs, uint8_t* pCodeBuffer, uint32_t codeMaxlen);

private:

    // Can't turn off mid-injection so store flag to indicate disable pending
    static bool _disablePending;

    // Page out for injection active
    static bool _pageOutForInjectionActive;

    // Step mode
    enum STEP_MODE_TYPE
    {
        STEP_MODE_STEP_INTO,
        STEP_MODE_STEP_RUN
    };
    static STEP_MODE_TYPE _stepMode;

    // Code snippet
    static uint32_t _snippetLen;
    static uint32_t _snippetPos;
    static uint8_t _snippetBuf[MAX_CODE_SNIPPET_LEN];
    static uint32_t _snippetWriteIdx;

    // Injection type
    static bool _setRegs;

    // Mirror memory requirements
    static bool _postInjectMemoryMirror;

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

    // Utils
    static bool isPrefixInstruction(uint32_t instr);
    static bool trackPrefixedInstructions(uint32_t flags, uint32_t codeVal);
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
        TARGET_STATE_ACQ_INSTR_FIRST_BYTE_GOT,
        TARGET_STATE_ACQ_INJECTING,
    };
    static TARGET_STATE_ACQ _targetStateAcqMode;

    // Debug
    static const int MAX_BYTES_IN_INSTR = 10;
    static uint8_t _debugInstrBytes[MAX_BYTES_IN_INSTR];
    static uint32_t _debugInstrBytePos;
};
