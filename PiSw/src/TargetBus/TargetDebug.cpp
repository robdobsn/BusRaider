// Bus Raider
// Rob Dobson 2019

#include "TargetDebug.h"
#include <string.h>
#include <stdlib.h>
#include "../System/ee_printf.h"
#include "../TargetBus/BusAccess.h"
#include "../System/piwiring.h"
#include "../System/lowlib.h"
#include "../System/rdutils.h"
#include "../Machines/McManager.h"
#include "../Disassembler/src/mdZ80.h"
#include "../TargetBus/RAMEmulator.h"
#include "../TargetBus/TargetCPUZ80.h"
#include "../Hardware/HwManager.h"

// Module name
static const char FromTargetDebug[] = "TargetDebug";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Callbacks and singletons
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Debugger
TargetDebug __targetDebug;

// Callback to send debug frame
SendRemoteDebugProtocolMsgType* TargetDebug::_pSendRemoteDebugProtocolMsgCallback = NULL;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Breakpoint and step-over
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool TargetDebug::_breakpointsEnabled = true;
int TargetDebug::_breakpointNumEnabled = 0;
Breakpoint TargetDebug::_breakpoints[MAX_BREAKPOINTS];
int TargetDebug::_breakpointIdxsToCheck[MAX_BREAKPOINTS];
bool TargetDebug::_breakpointHitFlag = false;
int TargetDebug::_breakpointHitIndex = 0;
bool TargetDebug::_stepOverEnabled = false;
uint32_t TargetDebug::_stepOverPCValue = 0;
bool TargetDebug::_stepOverHit = false;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TargetDebug::TargetDebug()
{
    _pTargetMachine = NULL;
    _debugMode = DEBUG_MODE_NONE;
    _targetStateAcqMode = TARGET_STATE_ACQ_NONE;
    _instrWaitRestoreNeeded = false;
    _instrWaitCurMode = false;
    _instrClockCurFreqHz = BR_TARGET_DEBUG_CLOCK_HZ;
    _registerMode = REGISTER_MODE_NONE;
    _registerPrevInstrComplete = false;
    _registerQueryWriteIndex = 0;
    _registerModeStep = 0;
    for (int i = 0; i < MAX_BREAKPOINTS; i++)
        _breakpoints[i].enabled = false;
    _breakpointNumEnabled = 0;
    _breakpointsEnabled = true;
    _breakpointHitFlag = false;
    _stepOverEnabled = false;
    _stepOverHit = false;
    _breakpointHitIndex = 0;
    _registerSetCodeLen = 0;
    _lastInstructionWasPrefixed = false;
    _thisInstructionIsPrefixed = false;
}

// Get singleton
TargetDebug* TargetDebug::get()
{
    return &__targetDebug;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetDebug::setup(McBase* pTargetMachine)
{
    _pTargetMachine = pTargetMachine;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Execution control
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetDebug::pause()
{
    targetStateAcqStart(true);
}

void TargetDebug::step([[maybe_unused]]bool stepOver)
{

}

void TargetDebug::release()
{

}

bool TargetDebug::isPaused()
{
    return _debugMode == DEBUG_MODE_STEP_INTO;
}

// Inform target debug that target programming is started
void TargetDebug::targetProgrammingStarted()
{

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Target state acquisition - state machine
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetDebug::targetStateAcqClear()
{
    _targetStateAcqMode = TARGET_STATE_ACQ_NONE;
    // _debugInCPUStep = false;
    _debugMode = DEBUG_MODE_NONE;
}

void TargetDebug::targetStateAcqStart([[maybe_unused]]bool leaveInPause)
{
    // Start the state machine to machine state

    // Check if bus access required at start
    // if (HwManager::pagingRequiresBusAccess() && !RAMEmulator::isActive())
    //     _targetStateAcqMode = TARGET_STATE_ACQ_PRE_INJECT;
    // else
    //     _targetStateAcqMode = TARGET_STATE_ACQ_INJECT;

    //             // Pause execution
    //     pause();
    //     BusAccess::waitOnInstruction(true);
    //     BusAccess::clockSetFreqHz(BR_TARGET_DEBUG_CLOCK_HZ);
    //     _lastInstructionWasPrefixed = false;


    //     _debugInCPUStep = true;
    //     _debugMode = ????

    // // BLANK
    //             pause();

    //         // Start sequence of getting registers
    //         startGetRegisterSequence();

    //         // LogWrite(FromTargetDebug, LOG_VERBOSE, "now paused SEND-BLANK");
    //         _debugInCPUStep = true;



}

void TargetDebug::targetStateAcqRelease(DEBUG_MODE debugMode)
{

    // BusAccess::waitRestoreDefaults();
    // BR_RETURN_TYPE retc = McManager::targetRelease();
    // _debugInCPUStep = false;
    // if (retc != BR_OK)
    //     LogWrite(FromTargetDebug, LOG_DEBUG, "pauseRelease failed in exit-cpu-step (%s)", BusAccess::retcString(retc));


    //     // STEP
    //     BR_RETURN_TYPE retc = McManager::targetStep();
    //     if (retc == BR_OK)
    //     {
    //         // Start sequence of getting registers
    //         startGetRegisterSequence();
    //         // LogWrite(FromTargetDebug, LOG_VERBOSE, "cpu-step done");
    //         _debugInCPUStep = true;

    //     }
    //     else
    //     {
    //         LogWrite(FromTargetDebug, LOG_DEBUG, "cpu-step failed (%s)", BusAccess::retcString(retc));
    //     }


    //     // STEPOVER
    //             _stepOverEnabled = true;
    //     _stepOverHit = false;
    //     // Release execution
    //     BR_RETURN_TYPE retc = McManager::targetRelease();
    //     _debugInCPUStep = false;
    //     if (retc != BR_OK)
    //         LogWrite(FromTargetDebug, LOG_DEBUG, "cpu-step-over pauseRelease failed (%s)", BusAccess::retcString(retc));


    _debugMode = debugMode;
}

void TargetDebug::targetStateAcqService()
{
    // Check for memory grab required
    // if (_busControlRequestedForMemGrab)
    // {
    //     // No longer request
    //     _busControlRequestedForMemGrab = false;

    //     // Release paging
    //     HwManager::pageOutForInjection(false);

    //     // Request bus so we can grab memory data before entering a wait state
    //     if (BusAccess::controlRequestAndTake() == BR_OK)
    //         BusAccess::grabMemoryAndReleaseBusRq(RAMEmulator::getMemBuffer(), RAMEmulator::getMemBufferLen());
    // }
}

uint32_t TargetDebug::targetStateAcqWaitISR([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, 
            [[maybe_unused]] uint32_t flags, [[maybe_unused]] uint32_t retVal)
{
    // switch(_targetStateAcqMode)
    // {
    //     case TARGET_STATE_ACQ_NONE:
    //     {
    //         break;
    //     }
    //     case TARGET_STATE_ACQ_PRE_INJECT:
    //     {
    //         break;
    //     }
    // }


//     if (mode == START GETTING REGS)
//     {
//             // Start sequence of getting registers
//         _registerMode = REGISTER_MODE_GET;
//         _registerPrevInstrComplete = false;
//         _registerQueryWriteIndex = 0;
//         _registerModeStep = 0;
//     }

// #else
//     // Check for the end of paging mode - un-page RAM
//     if (_registerMode == REGISTER_MODE_UNPAGE)
//     {
//         // Clear the paging of RAM
//         HwManager::pageOutForInjection(false);
//         _registerMode = REGISTER_MODE_NONE;

//         // Restore wait-state generation if required
//         if (_instrWaitRestoreNeeded)
//         {
//             BusAccess::waitOnInstruction(_instrWaitCurMode);
//             BusAccess::clockSetFreqHz(_instrClockCurFreqHz);            
//             _instrWaitRestoreNeeded = false;
//         }

//         // Handle M1 cycle
//         _registerPrevInstrComplete = false;
//         if (flags & BR_CTRL_BUS_M1_MASK)
//         {
//             // Always need to hold at this point
//             retVal |= BR_MEM_ACCESS_HOLD;
//         }
//         else
//         {
//             // After regsiter set/get there should always be an M1 cycle
//             ISR_ASSERT(ISR_ASSERT_CODE_UNPAGE_NOT_M1);
//         }
//     }

//     // Handle register query and set modes
//     if ((_registerMode == REGISTER_MODE_GET) || (_registerMode == REGISTER_MODE_SET))
//     {
//         // See if we are waiting for the end of the previous instruction
//         if (_registerPrevInstrComplete || ((!_lastInstructionWasPrefixed) && (flags & BR_CTRL_BUS_M1_MASK)))
//         {
//             //TODO
//                 // for (int i = 0; i < 5; i++)
//                 // {
//                 //     digitalWrite(8,1);
//                 //     microsDelay(1);
//                 //     digitalWrite(8,0);
//                 //     microsDelay(1);
//                 // }
//             // If we've just completed the previous instruction then the current address is the PC
//             if ((!_registerPrevInstrComplete) && (_registerMode == REGISTER_MODE_GET))
//             {
//                 //     digitalWrite(BR_PUSH_ADDR_BAR, 1);
//                 // int addrMask = 0x8000;
//                 // digitalWrite(8,1);
//                 // microsDelay(1);
//                 // for (int i = 0; i < 16; i++)
//                 // {
//                 //     digitalWrite(8,addr & addrMask);
//                 //     microsDelay(1);
//                 //     addrMask = addrMask >> 1;
//                 // }
//                 // digitalWrite(8,0);
//                 // microsDelay(1);
//                 _z80Registers.PC = addr;
//             }

//             // Previous instruction now done
//             _registerPrevInstrComplete = true;

//             // Page-out RAM/ROM while injecting
//             HwManager::pageOutForInjection(true);

//             // Handle the injection mode
//             if (_registerMode == REGISTER_MODE_GET)
//             {
//                 // We're now inserting instructions or getting results back
//                 handleRegisterGet(addr, data, flags, retVal);
//             }
//             else
//             {
//                 // We're now inserting instructions to set registers
//                 handleRegisterSet(retVal);
//             }

//             // If register get/set is finished after this cycle then request the bus (if not fully emulating RAM)
//             if ((_registerMode == REGISTER_MODE_UNPAGE) && (!RAMEmulator::isActive()))
//             {
//                 BusAccess::controlRequest();
//                 _busControlRequestedForMemGrab = true;
//             }
//         }
//         else
//         {
//             // Check if this is a prefix instruction - if so we need to avoid breaking at the wrong time
//             _lastInstructionWasPrefixed &= isPrefixInstruction(((retVal & BR_MEM_ACCESS_RSLT_NOT_DECODED) ? data : retVal) & 0xff);
//         }
//     }

    return retVal;
}

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // Grab all physical memory and release bus
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// void TargetDebug::grabMemoryAndReleaseBusRq(uint8_t* pBuf, uint32_t bufLen)
// {
//     // Check if the bus in under BusRaider control
//     if (BusAccess::isUnderControl())
//     {
//         // Get the current wait enablement and disable MREQ waits
//         bool curMonitorIORQ = false;
//         bool curMonitorMREQ = false;
//         BusAccess::waitGet(curMonitorIORQ, curMonitorMREQ);
//         BusAccess::waitOnInstruction(false);

//         // Read all of memory
//         BusAccess::blockRead(0, pBuf, bufLen, false, false);

//         // Restore wait enablement
//         BusAccess::waitOnInstruction(curMonitorMREQ);
        
//         // Release control of bus
//         BusAccess::controlRelease(false, false);
//     }
// }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Breakpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetDebug::enableBreakpoint(int idx, bool enabled)
{
    if ((idx < 0) || (idx >= MAX_BREAKPOINTS))
        return;
    _breakpoints[idx].enabled = enabled;
    _breakpointNumEnabled = 0;
    int numBreakpointsEnabled = 0;
    for (int i = 0; i < MAX_BREAKPOINTS; i++)
        if (_breakpoints[i].enabled)
            _breakpointIdxsToCheck[numBreakpointsEnabled++] = i;
    _breakpointNumEnabled = numBreakpointsEnabled;
}

void TargetDebug::setBreakpointMessage(int idx, const char* hitMessage)
{
    if ((idx < 0) || (idx >= MAX_BREAKPOINTS))
        return;
    if (hitMessage != NULL)
        strlcpy(_breakpoints[idx].hitMessage, hitMessage, Breakpoint::MAX_HIT_MSG_LEN);
}

void TargetDebug::setBreakpointPCAddr(int idx, uint32_t pcVal)
{
    if ((idx < 0) || (idx >= MAX_BREAKPOINTS))
        return;
    _breakpoints[idx].pcValue = pcVal;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Injected instruction sequence handling for register set
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetDebug::startSetRegisterSequence(Z80Registers* pRegs)
{
    // Set regs
    if (pRegs)
        _z80Registers = *pRegs;

    bool curMonitorIORQ = false;
    BusAccess::waitGet(curMonitorIORQ, _instrWaitCurMode);
    _instrClockCurFreqHz = BusAccess::clockCurFreqHz();
    LogWrite(FromTargetDebug, LOG_VERBOSE, "startSetRegisterSequence curMonitorInstr %s",
                _instrWaitCurMode ? "Y" : "N");

    // Put machine into wait-state mode for MREQ
    if (!_instrWaitCurMode)
        _lastInstructionWasPrefixed = false;
    _instrWaitRestoreNeeded = true;
    BusAccess::waitOnInstruction(true);
    BusAccess::clockSetFreqHz(BR_TARGET_DEBUG_CLOCK_HZ);

    // Start sequence of setting registers
    _registerMode = REGISTER_MODE_SET;
    _registerPrevInstrComplete = false;
    _registerModeStep = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Utility functions
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool TargetDebug::commandMatch(const char* s1, const char* s2)
{
    const char* p1 = s1;
    const char* p2 = s2;
    // Skip whitespace at start of received string
    while (*p1 == ' ')
        p1++;
    // Check match from start of received string
    while(*p1)
    {
        if (*p2 == 0)
        {
            while (rdisspace(*p1))
                p1++;
            return *p1 == 0;
        }
        if (*p1 == 0)
        {
            // LogWrite(FromTargetDebug, LOG_VERBOSE, "Compare <%s> <%s> FALSE s1 shorter", s1, s2);
            return false;
        }
        if (rdtolower(*p1++) != rdtolower(*p2++))
        {
            // LogWrite(FromTargetDebug, LOG_VERBOSE, "Compare <%s> <%s> FALSE no match at %d", s1, s2, p1 - s1);
            return false;
        }
    }
    // LogWrite(FromTargetDebug, LOG_VERBOSE, "Compare <%s> <%s> %s ", s1, s2, *p2 == 0 ? "TRUE" : "FALSE");
    return (*p2 == 0);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service - to handle breakpoint hits
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetDebug::service()
{
    // Service the target state acquisition state-machine
    targetStateAcqService();

    // Check for breakpoint hit
    if (_breakpointHitFlag)
    {
        LogWrite(FromTargetDebug, LOG_VERBOSE, "breakpoint hit %d debug", _breakpointHitIndex);
        if (_pSendRemoteDebugProtocolMsgCallback)
            (*_pSendRemoteDebugProtocolMsgCallback)("\ncommand@cpu-step> ", "0");
        _breakpointHitFlag = false;
    }

    // Check for step-over hit
    if (_stepOverHit)
    {
        static const int MAX_DISASSEMBLY_LINE_LEN = 200;
        char respBuf[MAX_DISASSEMBLY_LINE_LEN];
        LogWrite(FromTargetDebug, LOG_VERBOSE, "step-over hit");
        disasmZ80(RAMEmulator::getMemBuffer(), 0, _stepOverPCValue, respBuf, INTEL, false, true);
        strlcat(respBuf, "\ncommand@cpu-step> ", MAX_DISASSEMBLY_LINE_LEN);
        if (_pSendRemoteDebugProtocolMsgCallback)
            (*_pSendRemoteDebugProtocolMsgCallback)(respBuf, "0");
        _stepOverHit = false;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Debugger command handler - based on ZEsarUX / Z80 Debugger for VS Code
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool TargetDebug::handleDebuggerCommand(char* pCommand, char* pResponse, int maxResponseLen)
{
    // Empty response initially
    pResponse[0] = 0;

    // Break into lines
    char* pCmdCur = pCommand;
    char* pCmdNext = pCmdCur;
    while (*pCmdCur)
    {
        // Find end of command string
        while(*pCmdNext)
        {
            if (*pCmdNext == '\n')
            {
                // Replace newline with terminator
                *pCmdNext = 0;
                // Move to next command (or null)
                pCmdNext++;
                break;
            }
            pCmdNext++;
        }

        // Process the command
        procDebuggerLine(pCmdCur, pResponse, maxResponseLen);

        // Next command
        pCmdCur = pCmdNext;
    }
    return true;
}

bool TargetDebug::procDebuggerLine(char* pCmd, char* pResponse, int maxResponseLen)
{
    // Split
    char* cmdStr = strtok(pCmd, " ");
    // // Trim command string
    // int j = 0;
    // for (size_t i = 0; i < strlen(cmdStr); i++)
    // {
    //     if (!rdisspace(cmdStr[i]))
    //     {
    //         cmdStr[j++] = cmdStr[i];
    //     }
    // }
    // cmdStr[j] = 0;
    // McManager::logDebugMessage(pCmd);

    char* argStr = strtok(NULL, " ");
    char* argStr2 = strtok(NULL, " ");
    char* argRest = strtok(NULL, "");

    if (commandMatch(cmdStr, "about"))
    {
        strlcat(pResponse, "BusRaider RCP", maxResponseLen);
    }
    else if (commandMatch(cmdStr, "get-version"))
    {
        strlcat(pResponse, "7.2-SN", maxResponseLen);
    }
    else if (commandMatch(cmdStr, "get-current-machine"))
    {
        strlcat(pResponse, McManager::getDescriptorTable()->machineName, maxResponseLen);
    }
    else if (commandMatch(cmdStr, "set-debug-settings"))
    {
    }
    else if (commandMatch(cmdStr, "hard-reset-cpu"))
    {
        LogWrite(FromTargetDebug, LOG_VERBOSE, "Reset machine");
        McManager::targetReset(false, false);
        targetStateAcqClear();
    }
    else if (commandMatch(cmdStr, "enter-cpu-step"))
    {
        LogWrite(FromTargetDebug, LOG_VERBOSE, "enter-cpu-step");

        // Acquire target state and hold
        targetStateAcqStart(true);
    }
    else if (commandMatch(cmdStr, "exit-cpu-step"))
    {
        LogWrite(FromTargetDebug, LOG_VERBOSE, "exit-cpu-step");

        // Release target
        targetStateAcqRelease(DEBUG_MODE_NONE);
    }
    else if (commandMatch(cmdStr, "quit"))
    {
        // LogWrite(FromTargetDebug, LOG_VERBOSE, "quit");
        BusAccess::waitRestoreDefaults();

        // Release target
        targetStateAcqRelease(DEBUG_MODE_NONE);
    }
    else if (commandMatch(cmdStr, "smartload"))
    {
        LogWrite(FromTargetDebug, LOG_VERBOSE, "smartload %s", argStr);

        // Program the target
        McManager::handleTargetProgram(true, false, true, true);
    }
    else if (commandMatch(cmdStr, "clear-membreakpoints"))
    {
    }
    else if (commandMatch(cmdStr, "enable-breakpoint"))
    {
        LogWrite(FromTargetDebug, LOG_VERBOSE, "enable breakpoint %s", argStr);

        // Get breakpoint to enable
        int breakpointIdx = strtol(argStr, NULL, 10) - 1;
        enableBreakpoint(breakpointIdx, true);
    }
    else if (commandMatch(cmdStr, "enable-breakpoints"))
    {
        // Enable
        _breakpointsEnabled = true;
    }
    else if (commandMatch(cmdStr, "disable-breakpoint"))
    {
        LogWrite(FromTargetDebug, LOG_VERBOSE, "disable breakpoint %s", argStr);

        // Breakpoint number
        int breakpointIdx = strtol(argStr, NULL, 10) - 1;
        enableBreakpoint(breakpointIdx, false);
    }
    else if (commandMatch(cmdStr, "disable-breakpoints"))
    {
        _breakpointsEnabled = false;
        strlcat(pResponse, "", maxResponseLen);
    }
    else if (commandMatch(cmdStr, "set-breakpoint"))
    {
        LogWrite(FromTargetDebug, LOG_VERBOSE, "set breakpoint %s %s", argStr, argStr2);

        // Set breakpoint
        int breakpointIdx = strtol(argStr, NULL, 10) - 1;
        if ((argStr2[0] != 'P') || (argStr2[1] != 'C') || (argStr2[2] != '='))
        {
            LogWrite(FromTargetDebug, LOG_DEBUG, "breakpoint format must be PC= argstr2 %02x %02x %02x", 
                        argStr2[0], argStr2[1], argStr2[2]);
        }
        else
        {
            int addr = strtol(argStr2+3, NULL, 16);
            setBreakpointPCAddr(breakpointIdx, addr);
        }        
    }
    else if (commandMatch(cmdStr, "set-breakpointaction"))
    {
        LogWrite(FromTargetDebug, LOG_VERBOSE, "set breakpoint action %s %s %s", argStr, argStr2, argRest);

        // Set action on breakpoint
        int breakpointIdx = strtol(argStr, NULL, 10) - 1;
        if (!commandMatch(argStr2, "prints"))
        {
            LogWrite(FromTargetDebug, LOG_DEBUG, "breakpoint doesn't have message");
        }
        else
        {        
            setBreakpointMessage(breakpointIdx, argRest);
        }
    }
    else if (commandMatch(cmdStr, "disassemble"))
    {
        // Disassemble at current location
        int addr = strtol(argStr, NULL, 10);
        disasmZ80(RAMEmulator::getMemBuffer(), 0, addr, pResponse, INTEL, false, true);
        // LogWrite(FromTargetDebug, LOG_VERBOSE, "disassemble %s %s %s %d %s", argStr, argStr2, argRest, addr, pResponse);
    }
    else if (commandMatch(cmdStr, "get-registers"))
    {
        // Format registers to return
        _z80Registers.format(pResponse, maxResponseLen);
    }
    else if (commandMatch(cmdStr, "read-memory"))
    {
        // LogWrite(FromTargetDebug, LOG_VERBOSE, "read mem %s %s", argStr, argStr2);
        int memAddn = 0;
        if (strlen(argStr) > 5)
        {
            char memAddnStr[10];
            memset(memAddnStr, 0, 10);
            for (int i = 0; i < 10; i++)
            {
                memAddnStr[i] = argStr[5+i];
                if (memAddnStr[i] == 0)
                    break;
            }
            memAddn = strtol(memAddnStr, NULL, 10);
            argStr[5] = 0;
        }
        int startAddr = strtol(argStr, NULL, 10) + memAddn;
        int leng = strtol(argStr2, NULL, 10);
        if (startAddr >= 0 && startAddr <= RAMEmulator::getMemBufferLen())
        {
            for (int i = 0; i < leng; i++)
            {
                char chBuf[10];
                ee_sprintf(chBuf, "%02x", RAMEmulator::getMemoryByte(startAddr+i));
                strlcat(pResponse, chBuf, maxResponseLen);
            }
        }
    }
    else if (commandMatch(cmdStr, "get-tstates-partial"))
    {
        strlcat(pResponse, "Unknown command get-tstates-partial", maxResponseLen);
    }    
    else if (commandMatch(cmdStr, "check-extensions"))
    {
        strlcat(pResponse, "Unknown command check-extensions", maxResponseLen);
    }
    else if (commandMatch(cmdStr, "reset-tstates-partial"))
    {
        strlcat(pResponse, "Unknown command reset-tstates-partial", maxResponseLen);
    }
    else if (commandMatch(cmdStr, "get-cpu-frequency"))
    {
        strlcat(pResponse, "Unknown command get-cpu-frequency", maxResponseLen);
    }
    else if (commandMatch(cmdStr, "get-stack-backtrace"))
    {
        int startAddr = _z80Registers.SP;
        int numFrames = strtol(argStr, NULL, 10);
        if ((numFrames > 0) && (numFrames < MAX_MEM_DUMP_LEN / 2))
        {
            for (int i = 0; i < numFrames; i++)
            {
                char chBuf[20];
                ee_sprintf(chBuf, "%04xH ", RAMEmulator::getMemoryWord(startAddr + i*2));;
                strlcat(pResponse, chBuf, maxResponseLen);
            }
        }
    }
    else if (commandMatch(cmdStr, "run"))
    {
        // LogWrite(FromTargetDebug, LOG_VERBOSE, "run");

        // Release target to run (still in debug mode as breakpoint maybe hit)
        targetStateAcqRelease(DEBUG_MODE_RUN);

        // Reply without sending a prompt
        strlcat(pResponse, "Running until a breakpoint, key press or data sent, menu opening or other event\n", 
                    maxResponseLen);
        return true;
    }
    else if (commandMatch(cmdStr, "cpu-step"))
    {
        // LogWrite(FromTargetDebug, LOG_VERBOSE, "cpu-step");

        // Disassemble to generate response
        disasmZ80(RAMEmulator::getMemBuffer(), 0, _z80Registers.PC, pResponse, INTEL, false, true);

        // Release target for a single instruction
        targetStateAcqRelease(DEBUG_MODE_STEP_INTO);
    }
    else if (commandMatch(cmdStr, "cpu-step-over"))
    {
        // Get address to run to by disassembling code at current location
        int instrLen = disasmZ80(RAMEmulator::getMemBuffer(), 0, _z80Registers.PC, pResponse, INTEL, false, true);
        _stepOverPCValue = _z80Registers.PC + instrLen;
        // LogWrite(FromTargetDebug, LOG_VERBOSE, "cpu-step-over PCnow %04x StepToPC %04x", _z80Registers.PC, _stepOverPCValue);

        // Release target for step over
        targetStateAcqRelease(DEBUG_MODE_STEP_OVER);

        // Return immediately (no prompt)
        return true;
    }
    else if (commandMatch(cmdStr, ""))
    {
        // LogWrite(FromTargetDebug, LOG_VERBOSE, "blank (step) %s", _debugInitalized ? "" : "not in debug mode");
        if (_debugMode != DEBUG_MODE_NONE)
        {
            // Blank is used for pause
            targetStateAcqStart(true);
        }
    }
    else
    {
        LogWrite(FromTargetDebug, LOG_DEBUG, "unknown command %s", cmdStr);
        // for (unsigned int i = 0; i < strlen(pCommand); i++)
        // {
        //     char chBuf[10];
        //     ee_sprintf(chBuf, "%02x ", pCommand[i]);
        //     strlcat(pResponse, chBuf, maxResponseLen);
        // }
        strlcat(pResponse, "Unknown command", maxResponseLen);
    }
    
    // Complete and add command request
    strlcat(pResponse, (McManager::targetIsPaused() ? "\ncommand@cpu-step> " : "\ncommand> "), maxResponseLen);

    // LogWrite(FromTargetDebug, LOG_VERBOSE, "resp %s", pResponse);

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Interrupt extension for debugger - handle register GET
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetDebug::handleRegisterGet(uint32_t addr, uint32_t data, uint32_t flags, uint32_t& retVal)
{
    // Instructions to get register values
    static uint8_t regQueryInstructions[] = 
    {
                        // loop:
        0xF5,           //     push af
        0xED, 0x5F,     //     ld a,r - note that this instruction puts IFF2 into PF flag
        0x77,           //     ld (hl),a
        0xED, 0x57,     //     ld a,i
        0x77,           //     ld (hl),a
        0x33,           //     inc sp
        0x33,           //     inc sp
        0x12,           //     ld (de),a
        0x02,           //     ld (bc),a
        0xD9,           //     exx
        0x77,           //     ld (hl),a
        0x12,           //     ld (de),a
        0x02,           //     ld (bc),a
        0xD9,           //     exx
        0x08,           //     ex af,af'
        0xF5,           //     push af
        0x33,           //     inc sp
        0x33,           //     inc sp
        0x08,           //     ex af,af'
        0xDD, 0xE5,     //     push ix        
        0x33,           //     inc sp
        0x33,           //     inc sp    
        0xFD, 0xE5,     //     push iy - no inc sp as we pop af lower down which restores sp to where it was    
        0x3E, 0x00,     //     ld a, 0 - note that the value 0 gets changed in the code below  
        0xED, 0x4F,     //     ld r, a
        0xF1, 0x00, 0x00,    //     pop af + two bytes that are read as if from stack 
                        //     - note that the values 0, 0 get changed in the code below  
        0x18, 0xDE      //     jr loop
    };
    const int RegisterRUpdatePos = 28;
    const int RegisterAFUpdatePos = 32;

    // Check if writing
    if (flags & BR_CTRL_BUS_WR_MASK)
    {
        retVal = BR_MEM_ACCESS_INSTR_INJECT;
        // Store the result in registers
        switch(_registerModeStep)
        {
            case 1:  // push af
            {
                if (_registerQueryWriteIndex == 0)
                {
                    _z80Registers.SP = addr+1;
                    _z80Registers.AF = data << 8;
                    regQueryInstructions[RegisterAFUpdatePos+1] = data;                     
                    _registerQueryWriteIndex++;
                }
                else
                {
                    _z80Registers.AF = (_z80Registers.AF & 0xff00) | (data & 0xff);
                    regQueryInstructions[RegisterAFUpdatePos] = data;                     
                }
                break;
            }
            case 4: // ld (hl), a (where a has the contents of r)
            {
                _z80Registers.HL = addr;
                // R value compensates for the push af and ld r,a instructions
                _z80Registers.R = data - 3;
                // Value stored back to R compensates for ld a,NN and jr loop instructions
                regQueryInstructions[RegisterRUpdatePos] = _z80Registers.R - 2;                 
                break;
            }
            case 7: // ld (hl), a (where a has the contents of i)
            {
                _z80Registers.I = data;                        
                break;
            }
            case 10: // ld (de), a
            {
                _z80Registers.DE = addr;                        
                break;
            }
            case 11: // ld (bc), a
            {
                _z80Registers.BC = addr;                        
                break;
            }
            case 13: // ld (hl), a (after EXX so hl')
            {
                _z80Registers.HLDASH = addr;                        
                break;
            }
            case 14: // ld (de), a (after EXX so de')
            {
                _z80Registers.DEDASH = addr;                        
                break;
            }
            case 15: // ld (bc), a (after EXX so bc')
            {
                _z80Registers.BCDASH = addr;                        
                break;
            }
            case 18:  // push af (actually af')
            {
                if (_registerQueryWriteIndex == 0)
                {
                    _z80Registers.AFDASH = (_z80Registers.AFDASH & 0xff) | (data << 8);
                    _registerQueryWriteIndex++;
                }
                else
                {
                    _z80Registers.AFDASH = (_z80Registers.AFDASH & 0xff00) | (data & 0xff);
                }
                break;
            }
            case 22:  // push ix
            {
                if (_registerQueryWriteIndex == 0)
                {
                    _z80Registers.IX = (_z80Registers.IX & 0xff) | (data << 8);
                    _registerQueryWriteIndex++;
                }
                else
                {
                    _z80Registers.IX = (_z80Registers.IX & 0xff00) | (data & 0xff);
                }
                break;
            }
            case 26:  // push ix
            {
                if (_registerQueryWriteIndex == 0)
                {
                    _z80Registers.IY = (_z80Registers.IY & 0xff) | (data << 8);
                    _registerQueryWriteIndex++;
                }
                else
                {
                    _z80Registers.IY = (_z80Registers.IY & 0xff00) | (data & 0xff);
                }
                break;
            }
        }
    }
    else
    {
        // Send instructions to query registers
        retVal = regQueryInstructions[_registerModeStep++] | BR_MEM_ACCESS_INSTR_INJECT;
        _registerQueryWriteIndex = 0;
    }

    // Check if complete
    if (_registerModeStep >= sizeof(regQueryInstructions))
        _registerMode = REGISTER_MODE_UNPAGE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Interrupt extension for debugger - handle register SET
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetDebug::handleRegisterSet(uint32_t& retVal)
{
    // Fill in the register values
    if (_registerModeStep == 0)
    {
        _registerSetCodeLen = TargetCPUZ80::getInjectToSetRegs(_z80Registers, _registerSetBuffer, MAX_REGISTER_SET_CODE_LEN);
        if (_registerSetCodeLen == 0)
        {
            _registerMode = REGISTER_MODE_NONE;
            return;
        }
    }

    // Return the instruction / data to inject
    retVal = _registerSetBuffer[_registerModeStep++] | BR_MEM_ACCESS_INSTR_INJECT;

    // Check if complete
    if (_registerModeStep >= _registerSetCodeLen)
        _registerMode = REGISTER_MODE_UNPAGE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Test is an instruction is a prefix
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool TargetDebug::isPrefixInstruction(uint32_t instr)
{
    switch(instr)
    {
        case 0xdd:
        case 0xed:
        case 0xfd:
        case 0xcb:
            return true;
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Interrupt extension for debugger
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t TargetDebug::handleWaitInterrupt([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, 
            [[maybe_unused]] uint32_t flags, [[maybe_unused]] uint32_t retVal)
{
    // Only handle MREQs
    if ((flags & BR_CTRL_BUS_MREQ_MASK) == 0)
        return retVal;

    // Track M1 cycles to ensure we know when we're at the start of a new instruction
    // and not in the middle of a prefixed instruction sequence
    if (flags & BR_CTRL_BUS_M1_MASK)
    {
        // Record state from previous instruction
        _lastInstructionWasPrefixed = _thisInstructionIsPrefixed;
        // Check if this is a prefixed instruction
        _thisInstructionIsPrefixed = isPrefixInstruction(((retVal & BR_MEM_ACCESS_RSLT_NOT_DECODED) ? data : retVal) & 0xff);
    }
    else
    {
        // Can't be in the middle of a prefixed instruction if this isn't an M1 cycle
        _thisInstructionIsPrefixed = false;
        _lastInstructionWasPrefixed = false;
    }

    // // Breakpoint handling: enabled and M1 cycle and not in BUSRQ mode and not injecting register get/set
    // if ((_breakpointsEnabled && (_breakpointNumEnabled > 0)) && 
    //         (_registerMode == REGISTER_MODE_NONE) &&
    //         (!_debugInCPUStep) &&
    //         (flags & BR_CTRL_BUS_M1_MASK) && 
    //         (!BusAccess::isUnderControl()))
    // {
    //     // Check breakpoints
    //     for (int i = 0; i < _breakpointNumEnabled; i++)
    //     {
    //         int bpIdx = _breakpointIdxsToCheck[i];
    //         if (_breakpoints[bpIdx].pcValue == addr)
    //         {
    //             BR_RETURN_TYPE retc = McManager::targetPause();
    //             if (retc == BR_OK)
    //                 startGetRegisterSequence();
    //             _z80Registers.PC = addr;
    //             _registerPrevInstrComplete = true;
    //             _debugInCPUStep = true;
    //             _breakpointHitFlag = true;
    //             _breakpointHitIndex = bpIdx;
    //             break;
    //         }
    //     }
    // }

    // // Step-over handling: enabled and M1 cycle and not in BUSRQ mode and not injecting register get/set
    // if (_stepOverEnabled && 
    //         (_registerMode == REGISTER_MODE_NONE) &&
    //         (!_debugInCPUStep) &&
    //         (flags & BR_CTRL_BUS_M1_MASK) && 
    //         (!BusAccess::isUnderControl()))
    // {
    //     // Check stepover
    //     if (_stepOverPCValue == addr)
    //     {
    //         BR_RETURN_TYPE retc = McManager::targetPause();
    //         if (retc == BR_OK)
    //             startGetRegisterSequence();
    //         _z80Registers.PC = addr;
    //         _registerPrevInstrComplete = true;
    //         _debugInCPUStep = true;
    //         _stepOverHit = true;
    //         _stepOverEnabled = false;
    //     }
    // }

    // Handle targetStateAcquisition
    return targetStateAcqWaitISR(addr, data, flags, retVal);
}

