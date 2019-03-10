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

// Module name
static const char FromTargetDebug[] = "TargetDebug";

// Debugger
TargetDebug __targetDebug;

// Memory buffer
uint8_t TargetDebug::_targetMemBuffer[MAX_TARGET_MEMORY_LEN];

// Breakpoints
bool TargetDebug::_breakpointsEnabled = true;
int TargetDebug::_breakpointNumEnabled = 0;
Breakpoint TargetDebug::_breakpoints[MAX_BREAKPOINTS];
int TargetDebug::_breakpointIdxsToCheck[MAX_BREAKPOINTS];
bool TargetDebug::_breakpointHitFlag = false;
int TargetDebug::_breakpointHitIndex = 0;
bool TargetDebug::_stepOverEnabled = false;
uint32_t TargetDebug::_stepOverPCValue = 0;
bool TargetDebug::_stepOverHit = false;

// Callback to send debug frame
SendRemoteDebugProtocolMsgType* TargetDebug::_pSendRemoteDebugProtocolMsgCallback = NULL;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TargetDebug::TargetDebug()
{
    _pTargetMachine = NULL;
    _debugInitalized = false;
    _debugInCPUStep = false;
    _busControlRequestedForMemGrab = false;
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
    _emulatedRAMReadPagingActive = false;
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
// Memory access
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint8_t TargetDebug::getMemoryByte(uint32_t addr)
{
    if (addr >= MAX_TARGET_MEM_ADDR)
        return 0;
    return _targetMemBuffer[addr];
}

uint16_t TargetDebug::getMemoryWord(uint32_t addr)
{
    if (addr >= MAX_TARGET_MEM_ADDR)
        return 0;
    return _targetMemBuffer[(addr+1) % MAX_TARGET_MEMORY_LEN] * 256 + _targetMemBuffer[addr];
}

void TargetDebug::clearMemory()
{
    memset(_targetMemBuffer, 0, MAX_TARGET_MEMORY_LEN);
}

bool TargetDebug::blockWrite(uint32_t addr, const uint8_t* pBuf, uint32_t len)
{
    int copyLen = len;
    if (addr >= MAX_TARGET_MEMORY_LEN)
        return false;
    if (addr + copyLen > MAX_TARGET_MEMORY_LEN)
        copyLen = MAX_TARGET_MEMORY_LEN - addr;
    memcpy(_targetMemBuffer + addr, pBuf, copyLen);
    return true;
}

bool TargetDebug::blockRead(uint32_t addr, uint8_t* pBuf, uint32_t len)
{
    if (!((_debugInCPUStep) || (_pTargetMachine && _pTargetMachine->getDescriptorTable(0)->emulatedRAM &&
            (addr >= _pTargetMachine->getDescriptorTable(0)->emulatedRAMStart) && 
            (addr < _pTargetMachine->getDescriptorTable(0)->emulatedRAMStart + _pTargetMachine->getDescriptorTable(0)->emulatedRAMLen))))
    {
        // LogWrite(FromTargetDebug, LOG_VERBOSE, "blockRead: Not in single step or emulated RAM %04x %d", addr, len);
        return false;
    }
    int copyLen = len;
    if (addr >= MAX_TARGET_MEMORY_LEN)
    {
        LogWrite(FromTargetDebug, LOG_DEBUG, "blockRead: Too long %04x %d", addr, len);
        return false;
    }
    if (addr + copyLen > MAX_TARGET_MEMORY_LEN)
        copyLen = MAX_TARGET_MEMORY_LEN - addr;
    memcpy(pBuf, _targetMemBuffer + addr, copyLen);
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Injected instruction sequence handling for register set/get
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetDebug::startGetRegisterSequence()
{
    // Start sequence of getting registers
    _registerMode = REGISTER_MODE_GET;
    _registerPrevInstrComplete = false;
    _registerQueryWriteIndex = 0;
    _registerModeStep = 0;
}

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
// Grab all physical memory and release bus
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetDebug::grabMemoryAndReleaseBusRq()
{
    // Check if the bus in under BusRaider control
    if (BusAccess::isUnderControl() && _pTargetMachine)
    {
        // Get the current wait enablement and disable MREQ waits
        bool curMonitorIORQ = false;
        bool curMonitorMREQ = false;
        BusAccess::waitGet(curMonitorIORQ, curMonitorMREQ);
        BusAccess::waitOnInstruction(false);

        uint32_t emuRAM = _pTargetMachine->getDescriptorTable(0)->emulatedRAM;
        uint32_t emuRAMStart = _pTargetMachine->getDescriptorTable(0)->emulatedRAMStart;
        uint32_t emuRAMLen = _pTargetMachine->getDescriptorTable(0)->emulatedRAMLen;
        // Check if emulating memory
        if (emuRAM && (emuRAMLen > 0))
        {
            if (emuRAMStart > 0)
            {
                // Read memory before emulated section
                BusAccess::blockRead(0, _targetMemBuffer, emuRAMLen, false, false);
            }
            if (emuRAMStart+emuRAMLen < MAX_TARGET_MEMORY_LEN)
            {
                // Read memory after emulated section
                BusAccess::blockRead(0, _targetMemBuffer+emuRAMStart+emuRAMLen, 
                            MAX_TARGET_MEMORY_LEN-(emuRAMStart+emuRAMLen), false, false);
            }
        }
        else
        {
            // Read all of memory
            BusAccess::blockRead(0, _targetMemBuffer, 0x10000, false, false);
        }

        // Restore wait enablement
        BusAccess::waitOnInstruction(curMonitorMREQ);
        
        // Release control of bus
        BusAccess::controlRelease(false);

    }
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
    // Check for memory grab required
    if (_busControlRequestedForMemGrab)
    {
        // No longer request
        _busControlRequestedForMemGrab = false;

        // Release paging
        digitalWrite(BR_PAGING_RAM_PIN, 0);

        // Request bus so we can grab memory data before entering a wait state
        if (BusAccess::controlRequestAndTake() == BR_OK)
            grabMemoryAndReleaseBusRq();
    }

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
        disasmZ80(_targetMemBuffer, 0, _stepOverPCValue, respBuf, INTEL, false, true);
        strlcat(respBuf, "\ncommand@cpu-step> ", MAX_DISASSEMBLY_LINE_LEN);
        if (_pSendRemoteDebugProtocolMsgCallback)
            (*_pSendRemoteDebugProtocolMsgCallback)(respBuf, "0");
        _stepOverHit = false;
    }

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Debugger command handler - based on ZEsarUX / Z80 Debugger for VS Code
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool TargetDebug::debuggerCommand(char* pCommand, char* pResponse, int maxResponseLen)
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
        if (_pTargetMachine)
            strlcat(pResponse, _pTargetMachine->getDescriptorTable(0)->machineName, maxResponseLen);
        else
            strlcat(pResponse, "Unknown Machine", maxResponseLen);
    }
    else if (commandMatch(cmdStr, "set-debug-settings"))
    {
        strlcat(pResponse, "", maxResponseLen);
        _debugInitalized = true;
    }
    else if (commandMatch(cmdStr, "hard-reset-cpu"))
    {
        LogWrite(FromTargetDebug, LOG_VERBOSE, "Reset machine");
        if (_pTargetMachine)
            _pTargetMachine->reset();
        strlcat(pResponse, "", maxResponseLen);
    }
    else if (commandMatch(cmdStr, "enter-cpu-step"))
    {
        LogWrite(FromTargetDebug, LOG_VERBOSE, "enter-cpu-step");
        BR_RETURN_TYPE retc = BusAccess::pause();
        if (retc == BR_OK)
        {
            BusAccess::waitOnInstruction(true);
            BusAccess::clockSetFreqHz(BR_TARGET_DEBUG_CLOCK_HZ);
            _lastInstructionWasPrefixed = false;
        }
        else
        {
            LogWrite(FromTargetDebug, LOG_DEBUG, "pause failed in enter-cpu-step (%s)", BusAccess::retcString(retc));
        }
        strlcat(pResponse, "", maxResponseLen);
    }
    else if (commandMatch(cmdStr, "exit-cpu-step"))
    {
        LogWrite(FromTargetDebug, LOG_VERBOSE, "exit-cpu-step");
        BusAccess::waitRestoreDefaults();
        BR_RETURN_TYPE retc = BusAccess::pauseRelease();
        _debugInCPUStep = false;
        if (retc != BR_OK)
            LogWrite(FromTargetDebug, LOG_DEBUG, "pauseRelease failed in exit-cpu-step (%s)", BusAccess::retcString(retc));
        strlcat(pResponse, "", maxResponseLen);
    }
    else if (commandMatch(cmdStr, "quit"))
    {
        // LogWrite(FromTargetDebug, LOG_VERBOSE, "quit");
        BusAccess::waitRestoreDefaults();
        BR_RETURN_TYPE retc = BusAccess::pauseRelease();
        _debugInCPUStep = false;
        if ((retc != BR_OK) && (retc != BR_ALREADY_DONE))
            LogWrite(FromTargetDebug, LOG_DEBUG, "pauseRelease failed in quit (%s)", BusAccess::retcString(retc));
        _debugInitalized = false;
        strlcat(pResponse, "", maxResponseLen);
    }
    else if (commandMatch(cmdStr, "smartload"))
    {
        LogWrite(FromTargetDebug, LOG_VERBOSE, "smartload %s", argStr);
        McManager::handleTargetProgram(true, true, true);
        strlcat(pResponse, "", maxResponseLen);
    }
    else if (commandMatch(cmdStr, "clear-membreakpoints"))
    {
        strlcat(pResponse, "", maxResponseLen);
    }
    else if (commandMatch(cmdStr, "enable-breakpoint"))
    {
        LogWrite(FromTargetDebug, LOG_VERBOSE, "enable breakpoint %s", argStr);
        int breakpointIdx = strtol(argStr, NULL, 10) - 1;
        enableBreakpoint(breakpointIdx, true);
        strlcat(pResponse, "", maxResponseLen);
    }
    else if (commandMatch(cmdStr, "enable-breakpoints"))
    {
        _breakpointsEnabled = true;
        strlcat(pResponse, "", maxResponseLen);
    }
    else if (commandMatch(cmdStr, "disable-breakpoint"))
    {
        LogWrite(FromTargetDebug, LOG_VERBOSE, "disable breakpoint %s", argStr);
        int breakpointIdx = strtol(argStr, NULL, 10) - 1;
        enableBreakpoint(breakpointIdx, false);
        strlcat(pResponse, "", maxResponseLen);
    }
    else if (commandMatch(cmdStr, "disable-breakpoints"))
    {
        _breakpointsEnabled = false;
        strlcat(pResponse, "", maxResponseLen);
    }
    else if (commandMatch(cmdStr, "set-breakpoint"))
    {
        LogWrite(FromTargetDebug, LOG_VERBOSE, "set breakpoint %s %s", argStr, argStr2);
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
        disasmZ80(_targetMemBuffer, 0, addr, pResponse, INTEL, false, true);
        // LogWrite(FromTargetDebug, LOG_VERBOSE, "disassemble %s %s %s %d %s", argStr, argStr2, argRest, addr, pResponse);
    }
    else if (commandMatch(cmdStr, "get-registers"))
    {
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
        if (startAddr >= 0 && startAddr <= MAX_TARGET_MEM_ADDR)
        {
            for (int i = 0; i < leng; i++)
            {
                char chBuf[10];
                ee_sprintf(chBuf, "%02x", getMemoryByte(startAddr+i));
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
                ee_sprintf(chBuf, "%04xH ", getMemoryWord(startAddr + i*2));;
                strlcat(pResponse, chBuf, maxResponseLen);
            }
        }
    }
    else if (commandMatch(cmdStr, "run"))
    {
        // LogWrite(FromTargetDebug, LOG_VERBOSE, "run");
        BR_RETURN_TYPE retc = BusAccess::pauseRelease();
        _debugInCPUStep = false;
        if (retc != BR_OK)
            LogWrite(FromTargetDebug, LOG_DEBUG, "pauseRelease failed in run (%s)", BusAccess::retcString(retc));
        strlcat(pResponse, "Running until a breakpoint, key press or data sent, menu opening or other event\n", 
                    maxResponseLen);
        return true;
    }
    else if (commandMatch(cmdStr, "cpu-step"))
    {
        // LogWrite(FromTargetDebug, LOG_VERBOSE, "cpu-step");
        disasmZ80(_targetMemBuffer, 0, _z80Registers.PC, pResponse, INTEL, false, true);
        BR_RETURN_TYPE retc = BusAccess::pauseStep();
        if (retc == BR_OK)
        {
            // Start sequence of getting registers
            startGetRegisterSequence();
            // LogWrite(FromTargetDebug, LOG_VERBOSE, "cpu-step done");
            _debugInCPUStep = true;

        }
        else
        {
            LogWrite(FromTargetDebug, LOG_DEBUG, "cpu-step failed (%s)", BusAccess::retcString(retc));
        }
    }
    else if (commandMatch(cmdStr, "cpu-step-over"))
    {
        // Get address to run to by disassembling code at current location
        int instrLen = disasmZ80(_targetMemBuffer, 0, _z80Registers.PC, pResponse, INTEL, false, true);
        _stepOverPCValue = _z80Registers.PC + instrLen;
        // LogWrite(FromTargetDebug, LOG_VERBOSE, "cpu-step-over PCnow %04x StepToPC %04x", _z80Registers.PC, _stepOverPCValue);
        _stepOverEnabled = true;
        _stepOverHit = false;
        // Release execution
        BR_RETURN_TYPE retc = BusAccess::pauseRelease();
        _debugInCPUStep = false;
        if (retc != BR_OK)
            LogWrite(FromTargetDebug, LOG_DEBUG, "cpu-step-over pauseRelease failed (%s)", BusAccess::retcString(retc));
        strlcpy(pResponse, "\n", maxResponseLen);
        return true;
    }
    else if (commandMatch(cmdStr, ""))
    {
        // LogWrite(FromTargetDebug, LOG_VERBOSE, "blank (step) %s", _debugInitalized ? "" : "not in debug mode");
        if (_debugInitalized)
        {
            // Blank is used for pause
            BR_RETURN_TYPE retc = BusAccess::pause();
            if (retc == BR_OK)
            {
                // Start sequence of getting registers
                startGetRegisterSequence();
                // LogWrite(FromTargetDebug, LOG_VERBOSE, "now paused SEND-BLANK");
                _debugInCPUStep = true;
            }
            else
            {
                LogWrite(FromTargetDebug, LOG_DEBUG, "pause failed in SEND-BLANK command (%s)", BusAccess::retcString(retc));
            }
        }

        strlcat(pResponse, "", maxResponseLen);
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
    strlcat(pResponse, (BusAccess::pauseIsPaused() ? "\ncommand@cpu-step> " : "\ncommand> "), maxResponseLen);

    // LogWrite(FromTargetDebug, LOG_VERBOSE, "resp %s", pResponse);

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle register setting
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetDebug::store16BitVal(uint8_t arry[], int offset, uint16_t val)
{
    arry[offset] = val & 0xff;
    arry[offset+1] = (val >> 8) & 0xff;
}

int TargetDebug::getInstructionsToSetRegs(Z80Registers& regs, uint8_t* pCodeBuffer, uint32_t codeMaxlen)
{
    // Instructions to set register values
    static uint8_t regSetInstructions[] = 
    {
        0x00,                       // nop - in case previous instruction was prefixed
        0xdd, 0x21, 0x00, 0x00,     // ld ix, xxxx
        0xfd, 0x21, 0x00, 0x00,     // ld iy, xxxx
        0x21, 0x00, 0x00,           // ld hl, xxxx
        0x11, 0x00, 0x00,           // ld de, xxxx
        0x01, 0x00, 0x00,           // ld bc, xxxx
        0xd9,                       // exx
        0x21, 0x00, 0x00,           // ld hl, xxxx
        0x11, 0x00, 0x00,           // ld de, xxxx
        0x01, 0x00, 0x00,           // ld bc, xxxx
        0xf1, 0x00, 0x00,           // pop af + two bytes that are read as if from stack
        0x08,                       // ex af,af'
        0xf1, 0x00, 0x00,           // pop af + two bytes that are read as if from stack
        0x31, 0x00, 0x00,           // ld sp, xxxx
        0x3e, 0x00,                 // ld a, xx
        0xed, 0x47,                 // ld i, a
        0x3e, 0x00,                 // ld a, xx
        0xed, 0x4f,                 // ld r, a
        0x3e, 0x00,                 // ld a, xx
        0xed, 0x46,                 // im 0
        0xfb,                       // ei
        0xc3, 0x00, 0x00            // jp xxxx
    };
    const int RegisterIXUpdatePos = 3;
    const int RegisterIYUpdatePos = 7;
    const int RegisterHLDASHUpdatePos = 10;
    const int RegisterDEDASHUpdatePos = 13;
    const int RegisterBCDASHUpdatePos = 16;
    const int RegisterHLUpdatePos = 20;
    const int RegisterDEUpdatePos = 23;
    const int RegisterBCUpdatePos = 26;
    const int RegisterAFDASHUpdatePos = 29;
    const int RegisterAFUpdatePos = 33;
    const int RegisterSPUpdatePos = 36;
    const int RegisterIUpdatePos = 39;
    const int RegisterRUpdatePos = 43;
    const int RegisterAUpdatePos = 47;
    const int RegisterIMUpdatePos = 49;
    const int RegisterINTENUpdatePos = 50;
    const int RegisterPCUpdatePos = 52;

    // Fill in the register values
    store16BitVal(regSetInstructions, RegisterIXUpdatePos, regs.IX);
    store16BitVal(regSetInstructions, RegisterIYUpdatePos, regs.IY);
    store16BitVal(regSetInstructions, RegisterHLDASHUpdatePos, regs.HLDASH);
    store16BitVal(regSetInstructions, RegisterDEDASHUpdatePos, regs.DEDASH);
    store16BitVal(regSetInstructions, RegisterBCDASHUpdatePos, regs.BCDASH);
    store16BitVal(regSetInstructions, RegisterSPUpdatePos, regs.SP);
    store16BitVal(regSetInstructions, RegisterHLUpdatePos, regs.HL);
    store16BitVal(regSetInstructions, RegisterDEUpdatePos, regs.DE);
    store16BitVal(regSetInstructions, RegisterBCUpdatePos, regs.BC);
    store16BitVal(regSetInstructions, RegisterAFDASHUpdatePos, regs.AFDASH);
    store16BitVal(regSetInstructions, RegisterAFUpdatePos, regs.AF);
    regSetInstructions[RegisterIUpdatePos] = regs.I;
    regSetInstructions[RegisterRUpdatePos] = (regs.R + 256 - 5) % 256;
    regSetInstructions[RegisterAUpdatePos] = regs.AF >> 8;
    regSetInstructions[RegisterIMUpdatePos] = (regs.INTMODE == 0) ? 0x46 : ((regs.INTMODE == 1) ? 0x56 : 0x5e);
    regSetInstructions[RegisterINTENUpdatePos] = (regs.INTENABLED == 0) ? 0xf3 : 0xfb;
    store16BitVal(regSetInstructions, RegisterPCUpdatePos, regs.PC);

    if (codeMaxlen >= sizeof(regSetInstructions))
    {
        memcpy(pCodeBuffer, regSetInstructions, codeMaxlen);
        return sizeof(regSetInstructions);
    }
    return 0;
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
        _registerSetCodeLen = getInstructionsToSetRegs(_z80Registers, _registerSetBuffer, MAX_REGISTER_SET_CODE_LEN);
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

uint32_t TargetDebug::handleInterrupt([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, 
            [[maybe_unused]] uint32_t flags, [[maybe_unused]] uint32_t retVal,
            [[maybe_unused]] McDescriptorTable& descriptorTable)
{
    // Only handle MREQs
    if ((flags & BR_CTRL_BUS_MREQ_MASK) == 0)
        return retVal;

    // Check if RAM/ROM paged out from last MREQ
    if (_emulatedRAMReadPagingActive)
    {
        // Release paging on physical RAM/ROM
        digitalWrite(BR_PAGING_RAM_PIN, 0);
        _emulatedRAMReadPagingActive = false;
    }

    // Check if RAM is emulated
    if (descriptorTable.emulatedRAM && (addr >= descriptorTable.emulatedRAMStart) && 
            (addr < descriptorTable.emulatedRAMStart + descriptorTable.emulatedRAMLen))
    {
        // Check for read or write to emulated RAM / ROM
        if (flags & BR_CTRL_BUS_RD_MASK)
        {
            retVal = _targetMemBuffer[addr];
            // Page-out physical RAM/ROM
            digitalWrite(BR_PAGING_RAM_PIN, 1);
            _emulatedRAMReadPagingActive = true;

        }
        else if (flags & BR_CTRL_BUS_WR_MASK)
        {
            _targetMemBuffer[addr] = data;
        }
    }

    // Handle M1 cycles
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

    // Check for the end of paging mode - un-page RAM
    if (_registerMode == REGISTER_MODE_UNPAGE)
    {
        // Clear the paging of RAM
        _registerMode = REGISTER_MODE_NONE;
        digitalWrite(BR_PAGING_RAM_PIN, 0);

        // Restore wait-state generation if required
        if (_instrWaitRestoreNeeded)
        {
            BusAccess::waitOnInstruction(_instrWaitCurMode);
            BusAccess::clockSetFreqHz(_instrClockCurFreqHz);            
            _instrWaitRestoreNeeded = false;
        }

        // Handle M1 cycle
        _registerPrevInstrComplete = false;
        if (flags & BR_CTRL_BUS_M1_MASK)
        {
            // Always need to hold at this point
            retVal |= BR_MEM_ACCESS_HOLD;
        }
        else
        {
            // After regsiter set/get there should always be an M1 cycle
            ISR_ASSERT(ISR_ASSERT_CODE_UNPAGE_NOT_M1);
        }
    }

    // Breakpoint handling: enabled and M1 cycle and not in BUSRQ mode and not injecting register get/set
    if ((_breakpointsEnabled && (_breakpointNumEnabled > 0)) && 
            (_registerMode == REGISTER_MODE_NONE) &&
            (!_debugInCPUStep) &&
            (flags & BR_CTRL_BUS_M1_MASK) && 
            (!BusAccess::isUnderControl()))
    {
        // Check breakpoints
        for (int i = 0; i < _breakpointNumEnabled; i++)
        {
            int bpIdx = _breakpointIdxsToCheck[i];
            if (_breakpoints[bpIdx].pcValue == addr)
            {
                BR_RETURN_TYPE retc = BusAccess::pause();
                if (retc == BR_OK)
                    startGetRegisterSequence();
                _z80Registers.PC = addr;
                _registerPrevInstrComplete = true;
                _debugInCPUStep = true;
                _breakpointHitFlag = true;
                _breakpointHitIndex = bpIdx;
                break;
            }
        }
    }

    // Step-over handling: enabled and M1 cycle and not in BUSRQ mode and not injecting register get/set
    if (_stepOverEnabled && 
            (_registerMode == REGISTER_MODE_NONE) &&
            (!_debugInCPUStep) &&
            (flags & BR_CTRL_BUS_M1_MASK) && 
            (!BusAccess::isUnderControl()))
    {
        // Check stepover
        if (_stepOverPCValue == addr)
        {
            BR_RETURN_TYPE retc = BusAccess::pause();
            if (retc == BR_OK)
                startGetRegisterSequence();
            _z80Registers.PC = addr;
            _registerPrevInstrComplete = true;
            _debugInCPUStep = true;
            _stepOverHit = true;
            _stepOverEnabled = false;
        }
    }

    // Handle register query and set modes
    if ((_registerMode == REGISTER_MODE_GET) || (_registerMode == REGISTER_MODE_SET))
    {
        // See if we are waiting for the end of the previous instruction
        if (_registerPrevInstrComplete || ((!_lastInstructionWasPrefixed) && (flags & BR_CTRL_BUS_M1_MASK)))
        {
            //TODO
                // for (int i = 0; i < 5; i++)
                // {
                //     digitalWrite(8,1);
                //     microsDelay(1);
                //     digitalWrite(8,0);
                //     microsDelay(1);
                // }
            // If we've just completed the previous instruction then the current address is the PC
            if ((!_registerPrevInstrComplete) && (_registerMode == REGISTER_MODE_GET))
            {
                //     digitalWrite(BR_PUSH_ADDR_BAR, 1);
                // int addrMask = 0x8000;
                // digitalWrite(8,1);
                // microsDelay(1);
                // for (int i = 0; i < 16; i++)
                // {
                //     digitalWrite(8,addr & addrMask);
                //     microsDelay(1);
                //     addrMask = addrMask >> 1;
                // }
                // digitalWrite(8,0);
                // microsDelay(1);
                _z80Registers.PC = addr;
            }

            // Previous instruction now done
            _registerPrevInstrComplete = true;

            // Page-out RAM/ROM while injecting
            digitalWrite(BR_PAGING_RAM_PIN, 1);

            // Handle the injection mode
            if (_registerMode == REGISTER_MODE_GET)
            {
                // We're now inserting instructions or getting results back
                handleRegisterGet(addr, data, flags, retVal);
            }
            else
            {
                // We're now inserting instructions to set registers
                handleRegisterSet(retVal);
                // Page-out RAM/ROM
                digitalWrite(BR_PAGING_RAM_PIN, 1);
            }

            // If register get/set is finished after this cycle then request the bus (if not fully emulating RAM)
            if ((_registerMode == REGISTER_MODE_UNPAGE) && ((!descriptorTable.emulatedRAM) || 
                    (descriptorTable.emulatedRAMStart != 0) || (descriptorTable.emulatedRAMLen != 0x10000)))
            {
                BusAccess::controlRequest();
                _busControlRequestedForMemGrab = true;
            }
        }
        else
        {
            // Check if this is a prefix instruction - if so we need to avoid breaking at the wrong time
            _lastInstructionWasPrefixed &= isPrefixInstruction(((retVal & BR_MEM_ACCESS_RSLT_NOT_DECODED) ? data : retVal) & 0xff);
        }
    }

    return retVal;
}

