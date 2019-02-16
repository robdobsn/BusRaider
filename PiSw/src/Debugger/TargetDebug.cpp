// Bus Raider
// Rob Dobson 2019

#include "TargetDebug.h"
#include "../Utils/rdutils.h"
#include <string.h>
#include <stdlib.h>
#include "../System/ee_printf.h"
#include "../TargetBus/BusAccess.h"
#include "../TargetBus/piwiring.h"
#include "../System/lowlib.h"

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
    _registerMode = REGISTER_MODE_NONE;
    _registerModeGotM1 = false;
    _registerQueryWriteIndex = 0;
    _registerModeStep = 0;
    for (int i = 0; i < MAX_BREAKPOINTS; i++)
        _breakpoints[i].enabled = false;
    _breakpointNumEnabled = 0;
    _breakpointsEnabled = true;
    _breakpointHitFlag = false;
    _breakpointHitIndex = 0;
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
    _registerModeGotM1 = false;
    _registerQueryWriteIndex = 0;
    _registerModeStep = 0;
}

void TargetDebug::startSetRegisterSequence(Z80Registers* pRegs)
{
    // Set regs
    if (pRegs)
        _z80Registers = *pRegs;

    LogWrite(FromTargetDebug, LOG_DEBUG, "startSetRegisterSequence regs %d", pRegs);

    // Start sequence of setting registers
    _registerMode = REGISTER_MODE_SET;
    _registerModeGotM1 = false;
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
        uint32_t emuRAMStart = pMachine->getDescriptorTable(0)->emulatedRAMStart;
        uint32_t emuRAMLen = pMachine->getDescriptorTable(0)->emulatedRAMLen;
        // Check if emulating memory
        if (emuRAMLen > 0)
        {
            if (emuRAMStart > 0)
            {
                // Read memory before emulated section
                BusAccess::blockRead(0, _targetMemBuffer, emuRAMLen, false, false);
            }
            if (emuRAMStart+emuRAMLen < MAX_TARGET_MEMORY_LEN)
            {
                // Read memory after emulated section
                BusAccess::blockRead(0, _targetMemBuffer+emuRAMStart+emuRAMLen, MAX_TARGET_MEMORY_LEN-(emuRAMStart+emuRAMLen), false, false);
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
        LogWrite(FromTargetDebug, LOG_DEBUG, "breakpoint hit %d debug cb %d", _breakpointHitIndex, _pSendDebugMessageCallback);
        if (_pSendDebugMessageCallback)
            (*_pSendDebugMessageCallback)("\ncommand@cpu-step> ");
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
    }
    else if (matches(cmdStr, "hard-reset-cpu", MAX_CMD_STR_LEN))
    {
        pMachine->reset();
        strlcat(pResponse, "", maxResponseLen);
    }
    else if (matches(cmdStr, "enter-cpu-step", MAX_CMD_STR_LEN))
    {
        BusAccess::waitEnable(pMachine->getDescriptorTable(0)->monitorIORQ, true);
        strlcat(pResponse, "", maxResponseLen);
    }
    else if (matches(cmdStr, "exit-cpu-step", MAX_CMD_STR_LEN))
    {
        BusAccess::waitEnable(pMachine->getDescriptorTable(0)->monitorIORQ, pMachine->getDescriptorTable(0)->monitorMREQ);
        BusAccess::pauseRelease();
        strlcat(pResponse, "", maxResponseLen);
    }
    else if (matches(cmdStr, "quit", MAX_CMD_STR_LEN))
    {
        BusAccess::waitEnable(pMachine->getDescriptorTable(0)->monitorIORQ, pMachine->getDescriptorTable(0)->monitorMREQ);
        BusAccess::pauseRelease();
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
            LogWrite(FromTargetDebug, LOG_DEBUG, "breakpoint doesn't start PC= argstr2 %02x %02x %02x", argStr2[0], argStr2[1], argStr2[2]);
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
        strlcat(pResponse, "Unknown command", maxResponseLen);
    }    
    else if (matches(cmdStr, "reset-tstates-partial", MAX_CMD_STR_LEN))
    {
        strlcat(pResponse, "Unknown command", maxResponseLen);
    }
    else if (matches(cmdStr, "get-cpu-frequency", MAX_CMD_STR_LEN))
    {
        strlcat(pResponse, "Unknown command", maxResponseLen);
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
        BusAccess::pauseRelease();
        strlcat(pResponse, "Running until a breakpoint, key press or data sent, menu opening or other event\n", maxResponseLen);
        return true;
    }
    else if (matches(cmdStr, "cpu-step", MAX_CMD_STR_LEN))
    {
        if (BusAccess::pauseStep())
        {
            grabMemoryAndReleaseBusRq(pMachine, true);
            // LogWrite(FromTargetDebug, LOG_DEBUG, "cpu-step done");
        }
        else
        {
            LogWrite(FromTargetDebug, LOG_DEBUG, "cpu-step failed");
        }
        strlcat(pResponse, "", maxResponseLen);
    }
    else if (matches(cmdStr, "\n", MAX_CMD_STR_LEN))
    {
        // Blank is used for pause
        if (BusAccess::pause())
        {
            grabMemoryAndReleaseBusRq(pMachine, false);
            // LogWrite(FromTargetDebug, LOG_DEBUG, "now paused SEND-BLANK");
        }
        else
        {
            LogWrite(FromTargetDebug, LOG_DEBUG, "pause failed SEND-BLANK");
        }        

        strlcat(pResponse, "", maxResponseLen);
    }
    else
    {
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
        0xF1, 0x00, 0x00,    //     pop af + two bytes that are read is if from stack - note that the values 0, 0 get changed in the code below  
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
        _registerMode = REGISTER_MODE_NONE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Interrupt extension for debugger - handle register SET
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetDebug::handleRegisterSet(uint32_t& retVal)
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
    if (_registerModeStep == 0)
    {
        *((uint16_t*)(regSetInstructions+RegisterIXUpdatePos)) = _z80Registers.IX;
        *((uint16_t*)(regSetInstructions+RegisterIYUpdatePos)) = _z80Registers.IY;
        *((uint16_t*)(regSetInstructions+RegisterHLDASHUpdatePos)) = _z80Registers.HLDASH;
        *((uint16_t*)(regSetInstructions+RegisterDEDASHUpdatePos)) = _z80Registers.DEDASH;
        *((uint16_t*)(regSetInstructions+RegisterBCDASHUpdatePos)) = _z80Registers.BCDASH;
        *((uint16_t*)(regSetInstructions+RegisterSPUpdatePos)) = _z80Registers.SP;
        *((uint16_t*)(regSetInstructions+RegisterHLUpdatePos)) = _z80Registers.HL;
        *((uint16_t*)(regSetInstructions+RegisterDEUpdatePos)) = _z80Registers.DE;
        *((uint16_t*)(regSetInstructions+RegisterBCUpdatePos)) = _z80Registers.BC;
        *((uint16_t*)(regSetInstructions+RegisterAFDASHUpdatePos)) = _z80Registers.AFDASH;
        *((uint16_t*)(regSetInstructions+RegisterAFUpdatePos)) = _z80Registers.AF;
        regSetInstructions[RegisterIUpdatePos] = _z80Registers.I;
        regSetInstructions[RegisterRUpdatePos] = _z80Registers.R;
        regSetInstructions[RegisterAUpdatePos] = _z80Registers.AF >> 8;
        regSetInstructions[RegisterIMUpdatePos] = (_z80Registers.INTMODE == 0) ? 0x46 : ((_z80Registers.INTMODE == 1) ? 0x56 : 0x5e);
        regSetInstructions[RegisterINTENUpdatePos] = (_z80Registers.INTENABLED == 0) ? 0xf3 : 0xfb;
        *((uint16_t*)(regSetInstructions+RegisterPCUpdatePos)) = _z80Registers.PC;
    }

    // Return the instruction / data in inject
    retVal = regSetInstructions[_registerModeStep++] | BR_MEM_ACCESS_INSTR_INJECT;

    // Check if complete
    if (_registerModeStep >= sizeof(regSetInstructions))
        _registerMode = REGISTER_MODE_NONE;
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

    // See if breakpoints enabled and M1 cycle
    if ((_registerMode == REGISTER_MODE_NONE) && _breakpointsEnabled && (_breakpointNumEnabled > 0) && (flags & BR_CTRL_BUS_M1_MASK))
    {
        // Check breakpoints
        for (int i = 0; i < _breakpointNumEnabled; i++)
        {
            int bpIdx = _breakpointIdxsToCheck[i];
            if (_breakpoints[bpIdx].pcValue == addr)
            {
                if (BusAccess::pause())
                    startGetRegisterSequence();
                _breakpointHitFlag = true;
                _breakpointHitIndex = bpIdx;
                break;
            }
        }
    }

    // Check if we're in register query mode
    if ((_registerMode == REGISTER_MODE_GET) && (_registerModeGotM1 || (flags & BR_CTRL_BUS_M1_MASK)))
    {
        // We're now inserting instructions or getting results back
        _registerModeGotM1 = true;
        handleRegisterGet(addr, data, flags, retVal);
    }
    else if ((_registerMode == REGISTER_MODE_SET) && (_registerModeGotM1 || (flags & BR_CTRL_BUS_M1_MASK)))
    {
        // We're now inserting instructions to set registers
        _registerModeGotM1 = true;
        handleRegisterSet(retVal);
    }
    else if ((addr >= descriptorTable.emulatedRAMStart) && 
            (addr < descriptorTable.emulatedRAMStart + descriptorTable.emulatedRAMLen))
    {
        // Check for read
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

    return retVal;
}

