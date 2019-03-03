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

// Callback to send debug frame
SendDebugMessageType* TargetDebug::_pSendDebugMessageCallback = NULL;

// Get target
TargetDebug* TargetDebug::get()
{
    return &__targetDebug;
}

TargetDebug::TargetDebug()
{
    _debugInitalized = false;
    _debugInCPUStep = false;
    _instrWaitRestoreNeeded = false;
    _instrWaitCurMode = false;
    _registerMode = REGISTER_MODE_NONE;
    _registerPrevInstrComplete = false;
    _registerQueryWriteIndex = 0;
    _registerModeStep = 0;
    for (int i = 0; i < MAX_BREAKPOINTS; i++)
        _breakpoints[i].enabled = false;
    _breakpointNumEnabled = 0;
    _breakpointsEnabled = true;
    _breakpointHitFlag = false;
    _breakpointHitIndex = 0;
    _registerSetCodeLen = 0;
}

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
    if (!_debugInCPUStep)
        return false;
    int copyLen = len;
    if (addr >= MAX_TARGET_MEMORY_LEN)
        return false;
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
    LogWrite(FromTargetDebug, LOG_DEBUG, "startSetRegisterSequence curMonitorInstr %s",
                _instrWaitCurMode ? "Y" : "N");

    // Put machine into wait-state mode for MREQ
    _instrWaitRestoreNeeded = true;
    BusAccess::waitOnInstruction(true);

    // Start sequence of setting registers
    _registerMode = REGISTER_MODE_SET;
    _registerPrevInstrComplete = false;
    _registerModeStep = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Grab all physical memory and release bus
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetDebug::grabMemoryAndReleaseBusRq(McBase* pMachine, [[maybe_unused]] bool singleStep)
{
    // Start sequence of getting registers
    startGetRegisterSequence();

    // Check if the bus in under BusRaider control
    if (BusAccess::isUnderControl())
    {
        uint32_t emuRAM = pMachine->getDescriptorTable(0)->emulatedRAM;
        uint32_t emuRAMStart = pMachine->getDescriptorTable(0)->emulatedRAMStart;
        uint32_t emuRAMLen = pMachine->getDescriptorTable(0)->emulatedRAMLen;
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

        // Release control of bus
        BusAccess::controlRelease(false);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Utility functions
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool TargetDebug::matches(const char* s1, const char* s2, int maxLen)
{
    const char* p1 = s1;
    const char* p2 = s2;
    // Skip whitespace at start of received string
    while (*p1 == ' ')
        p1++;
    // Check match from start of received string
    for (int i = 0; i < maxLen; i++)
    {
        if (*p2 == 0)
        {
            while (rdisspace(*p1))
                p1++;
            return *p1 == 0;
        }
        if (*p1 == 0)
            return false;
        if (rdtolower(*p1++) != rdtolower(*p2++))
            return false;
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service - to handle breakpoint hits
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetDebug::service()
{
    if (_breakpointHitFlag)
    {
        LogWrite(FromTargetDebug, LOG_DEBUG, "breakpoint hit %d debug cb %d", 
                    _breakpointHitIndex, _pSendDebugMessageCallback);
        if (_pSendDebugMessageCallback)
            (*_pSendDebugMessageCallback)("\ncommand@cpu-step> ", "0");
        _breakpointHitFlag = false;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Debugger command handler - based on ZEsarUX / Z80 Debugger for VS Code
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool TargetDebug::debuggerCommand(McBase* pMachine, [[maybe_unused]] const char* pCommand, 
        [[maybe_unused]] char* pResponse, [[maybe_unused]] int maxResponseLen)
{
    static const int MAX_CMD_STR_LEN = 2000;
    char command[MAX_CMD_STR_LEN+1];
    strlcpy(command, pCommand, MAX_CMD_STR_LEN);
    pResponse[0] = 0;

    // Split
    char* cmdStr = strtok(command, " ");
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

    if (matches(cmdStr, "about", MAX_CMD_STR_LEN))
    {
        strlcat(pResponse, "BusRaider RCP", maxResponseLen);
    }
    else if (matches(cmdStr, "get-version", MAX_CMD_STR_LEN))
    {
        strlcat(pResponse, "7.2-SN", maxResponseLen);
    }
    else if (matches(cmdStr, "get-current-machine", MAX_CMD_STR_LEN))
    {
        strlcat(pResponse, pMachine->getDescriptorTable(0)->machineName, maxResponseLen);
    }
    else if (matches(cmdStr, "set-debug-settings", MAX_CMD_STR_LEN))
    {
        strlcat(pResponse, "", maxResponseLen);
        _debugInitalized = true;
    }
    else if (matches(cmdStr, "hard-reset-cpu", MAX_CMD_STR_LEN))
    {
        LogWrite(FromTargetDebug, LOG_DEBUG, "Reset machine");
        pMachine->reset();
        strlcat(pResponse, "", maxResponseLen);
    }
    else if (matches(cmdStr, "enter-cpu-step", MAX_CMD_STR_LEN))
    {
        LogWrite(FromTargetDebug, LOG_DEBUG, "enter-cpu-step");
        BR_RETURN_TYPE retc = BusAccess::pause();
        if (retc == BR_OK)
        {
            BusAccess::waitOnInstruction(true);
            _debugInCPUStep = true;
        }
        else
        {
            LogWrite(FromTargetDebug, LOG_DEBUG, "pause failed in enter-cpu-step (%s)", BusAccess::retcString(retc));
        }
        strlcat(pResponse, "", maxResponseLen);
    }
    else if (matches(cmdStr, "exit-cpu-step", MAX_CMD_STR_LEN))
    {
        LogWrite(FromTargetDebug, LOG_DEBUG, "exit-cpu-step");
        BusAccess::waitRestoreDefaults();
        BR_RETURN_TYPE retc = BusAccess::pauseRelease();
        _debugInCPUStep = false;
        if (retc != BR_OK)
            LogWrite(FromTargetDebug, LOG_DEBUG, "pauseRelease failed in exit-cpu-step (%s)", BusAccess::retcString(retc));
        strlcat(pResponse, "", maxResponseLen);
    }
    else if (matches(cmdStr, "quit", MAX_CMD_STR_LEN))
    {
        LogWrite(FromTargetDebug, LOG_DEBUG, "quit");
        BusAccess::waitRestoreDefaults();
        BR_RETURN_TYPE retc = BusAccess::pauseRelease();
        _debugInCPUStep = false;
        if ((retc != BR_OK) && (retc != BR_ALREADY_DONE))
            LogWrite(FromTargetDebug, LOG_DEBUG, "pauseRelease failed in quit (%s)", BusAccess::retcString(retc));
        _debugInitalized = false;
        strlcat(pResponse, "", maxResponseLen);
    }
    else if (matches(cmdStr, "smartload", MAX_CMD_STR_LEN))
    {
        LogWrite(FromTargetDebug, LOG_DEBUG, "smartload %s", argStr);
        strlcat(pResponse, "", maxResponseLen);
    }
    else if (matches(cmdStr, "clear-membreakpoints", MAX_CMD_STR_LEN))
    {
        strlcat(pResponse, "", maxResponseLen);
    }
    else if (matches(cmdStr, "enable-breakpoint", MAX_CMD_STR_LEN))
    {
        // LogWrite(FromTargetDebug, LOG_DEBUG, "enable breakpoint %s", argStr);
        int breakpointIdx = strtol(argStr, NULL, 10) - 1;
        enableBreakpoint(breakpointIdx, true);
        strlcat(pResponse, "", maxResponseLen);
    }
    else if (matches(cmdStr, "enable-breakpoints", MAX_CMD_STR_LEN))
    {
        _breakpointsEnabled = true;
        strlcat(pResponse, "", maxResponseLen);
    }
    else if (matches(cmdStr, "disable-breakpoint", MAX_CMD_STR_LEN))
    {
        // LogWrite(FromTargetDebug, LOG_DEBUG, "disable breakpoint %s", argStr);
        int breakpointIdx = strtol(argStr, NULL, 10) - 1;
        enableBreakpoint(breakpointIdx, false);
        strlcat(pResponse, "", maxResponseLen);
    }
    else if (matches(cmdStr, "disable-breakpoints", MAX_CMD_STR_LEN))
    {
        _breakpointsEnabled = false;
        strlcat(pResponse, "", maxResponseLen);
    }
    else if (matches(cmdStr, "set-breakpoint", MAX_CMD_STR_LEN))
    {
        LogWrite(FromTargetDebug, LOG_DEBUG, "set breakpoint %s %s", argStr, argStr2);
        int breakpointIdx = strtol(argStr, NULL, 10) - 1;
        if ((argStr2[0] != 'P') || (argStr2[1] != 'C') || (argStr2[2] != '='))
        {
            LogWrite(FromTargetDebug, LOG_DEBUG, "breakpoint doesn't start PC= argstr2 %02x %02x %02x", 
                        argStr2[0], argStr2[1], argStr2[2]);
        }
        else
        {
            int addr = strtol(argStr2+3, NULL, 16);
            setBreakpointPCAddr(breakpointIdx, addr);
        }        
    }
    else if (matches(cmdStr, "set-breakpointaction", MAX_CMD_STR_LEN))
    {
        LogWrite(FromTargetDebug, LOG_DEBUG, "set breakpoint action %s %s %s", argStr, argStr2, argRest);
        int breakpointIdx = strtol(argStr, NULL, 10) - 1;
        if (!matches(argStr2, "prints", MAX_CMD_STR_LEN))
        {
            LogWrite(FromTargetDebug, LOG_DEBUG, "breakpoint doesn't have message");
        }
        else
        {        
            setBreakpointMessage(breakpointIdx, argRest);
        }
    }
    else if (matches(cmdStr, "get-registers", MAX_CMD_STR_LEN))
    {
        uint32_t curAddr = 0;
        uint32_t curData = 0;
        uint32_t curFlags = 0;
        BusAccess::pauseGetCurrent(&curAddr, &curData, &curFlags);
        _z80Registers.PC = curAddr;
        _z80Registers.format1(pResponse, maxResponseLen);
    }
    else if (matches(cmdStr, "read-memory", MAX_CMD_STR_LEN))
    {
        // LogWrite(FromTargetDebug, LOG_DEBUG, "read mem %s %s", argStr, argStr2);
        int startAddr = strtol(argStr, NULL, 10);
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
    else if (matches(cmdStr, "get-tstates-partial", MAX_CMD_STR_LEN))
    {
        strlcat(pResponse, "Unknown command get-tstates-partial", maxResponseLen);
    }    
    else if (matches(cmdStr, "check-extensions", MAX_CMD_STR_LEN))
    {
        strlcat(pResponse, "Unknown command check-extensions", maxResponseLen);
    }    
    else if (matches(cmdStr, "reset-tstates-partial", MAX_CMD_STR_LEN))
    {
        strlcat(pResponse, "Unknown command reset-tstates-partial", maxResponseLen);
    }
    else if (matches(cmdStr, "get-cpu-frequency", MAX_CMD_STR_LEN))
    {
        strlcat(pResponse, "Unknown command get-cpu-frequency", maxResponseLen);
    }
    else if (matches(cmdStr, "get-stack-backtrace", MAX_CMD_STR_LEN))
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
    else if (matches(cmdStr, "run", MAX_CMD_STR_LEN))
    {
        LogWrite(FromTargetDebug, LOG_DEBUG, "run");
        BR_RETURN_TYPE retc = BusAccess::pauseRelease();
        _debugInCPUStep = false;
        if (retc != BR_OK)
            LogWrite(FromTargetDebug, LOG_DEBUG, "pauseRelease failed in run (%s)", BusAccess::retcString(retc));
        strlcat(pResponse, "Running until a breakpoint, key press or data sent, menu opening or other event\n", 
                    maxResponseLen);
        return true;
    }
    else if (matches(cmdStr, "cpu-step", MAX_CMD_STR_LEN))
    {
        LogWrite(FromTargetDebug, LOG_DEBUG, "cpu-step");
        BR_RETURN_TYPE retc = BusAccess::pauseStep();
        if (retc == BR_OK)
        {
            grabMemoryAndReleaseBusRq(pMachine, true);
            // LogWrite(FromTargetDebug, LOG_DEBUG, "cpu-step done");
        }
        else
        {
            LogWrite(FromTargetDebug, LOG_DEBUG, "cpu-step failed (%s)", BusAccess::retcString(retc));
        }
        strlcat(pResponse, "", maxResponseLen);
    }
    else if (matches(cmdStr, "\n", MAX_CMD_STR_LEN))
    {
        LogWrite(FromTargetDebug, LOG_DEBUG, "blank (step) %s", _debugInitalized ? "" : "not in debug mode");
        if (_debugInitalized)
        {
            // Blank is used for pause
            BR_RETURN_TYPE retc = BusAccess::pause();
            if (retc == BR_OK)
            {
                grabMemoryAndReleaseBusRq(pMachine, false);
                // LogWrite(FromTargetDebug, LOG_DEBUG, "now paused SEND-BLANK");
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
        for (unsigned int i = 0; i < strlen(pCommand); i++)
        {
            char chBuf[10];
            ee_sprintf(chBuf, "%02x ", pCommand[i]);
            strlcat(pResponse, chBuf, maxResponseLen);
        }
        strlcat(pResponse, "Unknown command", maxResponseLen);
    }
    

    // Complete and add command request
    strlcat(pResponse, (BusAccess::pauseIsPaused() ? "\ncommand@cpu-step> " : "\ncommand> "), maxResponseLen);

    // LogWrite(FromTargetDebug, LOG_DEBUG, "resp %s", pResponse);

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
        0xdd, 0x21, 0x00, 0x00,     // ld ix, xxxx
        0xfd, 0x21, 0x00, 0x00,     // ld iy, xxxx
        0x21, 0x00, 0x00,           // ld hl, xxxx
        0x11, 0x00, 0x00,           // ld de, xxxx
        0x01, 0x00, 0x00,           // ld bc, xxxx
        0xd9,                       // exx
        0x31, 0x00, 0x00,           // ld sp, xxxx
        0x21, 0x00, 0x00,           // ld hl, xxxx
        0x11, 0x00, 0x00,           // ld de, xxxx
        0x01, 0x00, 0x00,           // ld bc, xxxx
        0xf1, 0x00, 0x00,           // pop af + two bytes that are read as if from stack
        0x08,                       // ex af,af'
        0xf1, 0x00, 0x00,           // pop af + two bytes that are read as if from stack
        0x3e, 0x00,                 // ld a, xx
        0xed, 0x47,                 // ld i, a
        0x3e, 0x00,                 // ld a, xx
        0xed, 0x4f,                 // ld r, a
        0x3e, 0x00,                 // ld a, xx
        0xed, 0x46,                 // im 0
        0xfb,                       // ei
        0xc3, 0x00, 0x00            // jp xxxx
    };
    const int RegisterIXUpdatePos = 2;
    const int RegisterIYUpdatePos = 6;
    const int RegisterHLDASHUpdatePos = 9;
    const int RegisterDEDASHUpdatePos = 12;
    const int RegisterBCDASHUpdatePos = 15;
    const int RegisterSPUpdatePos = 19;
    const int RegisterHLUpdatePos = 22;
    const int RegisterDEUpdatePos = 25;
    const int RegisterBCUpdatePos = 28;
    const int RegisterAFDASHUpdatePos = 31;
    const int RegisterAFUpdatePos = 35;
    const int RegisterIUpdatePos = 38;
    const int RegisterRUpdatePos = 42;
    const int RegisterAUpdatePos = 46;
    const int RegisterIMUpdatePos = 48;
    const int RegisterINTENUpdatePos = 49;
    const int RegisterPCUpdatePos = 51;

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
    regSetInstructions[RegisterRUpdatePos] = (regs.R + 256 - 7) % 256;
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
        0xF1, 0x00, 0x00,    //     pop af + two bytes that are read is if from stack 
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
                    _z80Registers.AF = (_z80Registers.AF & 0xff) | (data << 8);
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

    // Check if RAM is emulated
    if (descriptorTable.emulatedRAM && (addr >= descriptorTable.emulatedRAMStart) && 
            (addr < descriptorTable.emulatedRAMStart + descriptorTable.emulatedRAMLen))
    {
        // Check for read or write to emulated RAM / ROM
        if (flags & BR_CTRL_BUS_RD_MASK)
        {
            if (flags & BR_CTRL_BUS_MREQ_MASK)
            {
                retVal = _targetMemBuffer[addr];
            }
        }
        else if (flags & BR_CTRL_BUS_WR_MASK)
        {
            if (flags & BR_CTRL_BUS_MREQ_MASK)
            {
                _targetMemBuffer[addr] = data;
            }
        }
        
    }

    // Handle M1 cycles
    if (flags & BR_CTRL_BUS_M1_MASK)
    {
        // Record state from previous instruction
        _lastInstructionWasPrefixed = _thisInstructionIsPrefixed;
        // Check if this is a prefixed instruction
        _thisInstructionIsPrefixed = isPrefixInstruction(((retVal & BR_MEM_ACCESS_RSLT_NOT_DECODED) ? data : retVal) & 0xff);
        // if (_lastInstructionWasPrefixed)
        // {
        //     digitalWrite(BR_PAGING_RAM_PIN, 1);
        //     microsDelay(3);
        //     digitalWrite(BR_PAGING_RAM_PIN, 0);
        //     microsDelay(1);
        // }
        // if (_thisInstructionIsPrefixed)
        // {
        //     digitalWrite(BR_PAGING_RAM_PIN, 1);
        //     microsDelay(1);
        //     digitalWrite(BR_PAGING_RAM_PIN, 0);
        //     microsDelay(1);
        // }
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
        if (descriptorTable.ramPaging)
            digitalWrite(BR_PAGING_RAM_PIN, 0);

        // Restore wait-state generation if required
        if (_instrWaitRestoreNeeded)
        {
            BusAccess::waitOnInstruction(_instrWaitCurMode);            
            _instrWaitRestoreNeeded = false;
        }

        // Handle M1 cycle
        _registerPrevInstrComplete = false;
        if (flags & BR_CTRL_BUS_M1_MASK)
        {

            // Always need to hold at this point
            retVal |= BR_MEM_ACCESS_HOLD;

            // // Debug
            // // TODO
            // digitalWrite(BR_PAGING_RAM_PIN, 1);
            // microsDelay(1);
            // if (_lastInstructionWasPrefixed)
            //     microsDelay(10);
            // digitalWrite(BR_PAGING_RAM_PIN, 0);
        }
        else
        {
            // After regsiter set/get there should always be an M1 cycle
            ISR_ASSERT(ISR_ASSERT_CODE_UNPAGE_NOT_M1);
        }

    }

    // See if breakpoints enabled and M1 cycle
    if ((_registerMode == REGISTER_MODE_NONE) && _breakpointsEnabled && (_breakpointNumEnabled > 0) && (flags & BR_CTRL_BUS_M1_MASK))
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
                _breakpointHitFlag = true;
                _breakpointHitIndex = bpIdx;
                break;
            }
        }
    }

    // Handle register query and set modes
    if ((_registerMode == REGISTER_MODE_GET) || (_registerMode == REGISTER_MODE_SET))
    {
        // See if we are waiting for the end of the previous instruction
        if (_registerPrevInstrComplete || ((!_lastInstructionWasPrefixed) && (flags & BR_CTRL_BUS_M1_MASK)))
        {
            if (_registerMode == REGISTER_MODE_GET)
            {
                // We're now inserting instructions or getting results back
                _registerPrevInstrComplete = true;
                handleRegisterGet(addr, data, flags, retVal);
                // Page-out paged RAM/ROM if required
                if (descriptorTable.ramPaging)
                    digitalWrite(BR_PAGING_RAM_PIN, 1);
            }
            else
            {
                // We're now inserting instructions to set registers
                _registerPrevInstrComplete = true;
                handleRegisterSet(retVal);
                // Page-out RAM/ROM if required
                if (descriptorTable.ramPaging)
                    digitalWrite(BR_PAGING_RAM_PIN, 1);
            }

            // // Debug
            // // TODO
            // digitalWrite(BR_PAGING_RAM_PIN, 1);
            // microsDelay(1);
            // if (_registerPrevInstrComplete)
            //     microsDelay(5);
            // digitalWrite(BR_PAGING_RAM_PIN, 0);
            // microsDelay(1);
            // if (_lastInstructionWasPrefixed)
            //     microsDelay(5);
            // digitalWrite(BR_PAGING_RAM_PIN, 1);
            // microsDelay(1);
            // digitalWrite(BR_PAGING_RAM_PIN, 0);

        }
        else
        {
            // Check if this is a prefix instruction - if so we need to avoid breaking at the wrong time
            _lastInstructionWasPrefixed &= isPrefixInstruction(((retVal & BR_MEM_ACCESS_RSLT_NOT_DECODED) ? data : retVal) & 0xff);

            // // // Debug
            // // // TODO
            // digitalWrite(BR_PAGING_RAM_PIN, 1);
            // microsDelay(1);
            // for (int i = 0; i < 5; i++)
            // {
            //     digitalWrite(BR_PAGING_RAM_PIN, 0);
            //     if (_registerPrevInstrComplete)
            //         microsDelay(3);
            //     else
            //         microsDelay(1);
            //     digitalWrite(BR_PAGING_RAM_PIN, 1);
            //     microsDelay(1);
            // }
            // digitalWrite(BR_PAGING_RAM_PIN, 0);

        }
    }

    // if ((_registerMode == REGISTER_MODE_GET) && (_registerModeGotM1 || (flags & BR_CTRL_BUS_M1_MASK)))
    // {
    //     // We're now inserting instructions or getting results back
    //     _registerModeGotM1 = true;
    //     handleRegisterGet(addr, data, flags, retVal);
    //     // Page-out paged RAM/ROM if required
    //     if (descriptorTable.ramPaging)
    //         digitalWrite(BR_PAGING_RAM_PIN, 1);
    // }
    // else if ((_registerMode == REGISTER_MODE_SET) && (_registerModeGotM1 || (flags & BR_CTRL_BUS_M1_MASK)))
    // {
    //     // We're now inserting instructions to set registers
    //     _registerModeGotM1 = true;
    //     handleRegisterSet(retVal);
    //     // Page-out RAM/ROM if required
    //     if (descriptorTable.ramPaging)
    //         digitalWrite(BR_PAGING_RAM_PIN, 1);
    // }

    return retVal;
}

