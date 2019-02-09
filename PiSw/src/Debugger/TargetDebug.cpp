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

// Get target
TargetDebug* TargetDebug::get()
{
    return &__targetDebug;
}

TargetDebug::TargetDebug()
{
    _registerQueryMode = false;
    _registerQueryStep = 0;
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

void TargetDebug::blockWrite(uint32_t addr, uint8_t* pBuf, uint32_t len)
{
    int copyLen = len;
    if (addr >= MAX_TARGET_MEMORY_LEN)
        return;
    if (addr + copyLen > MAX_TARGET_MEMORY_LEN)
        copyLen = MAX_TARGET_MEMORY_LEN - addr;
    memcpy(_targetMemBuffer + addr, pBuf, copyLen);
}

void TargetDebug::grabMemoryAndReleaseBusRq(McBase* pMachine, bool singleStep)
{
    // Start sequence of getting registers
    if (singleStep)
    {
        //TODO
        // _registerQueryMode = true;
        _registerQueryStep = 0;
    }

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
        LogWrite(FromTargetDebug, LOG_DEBUG, "enable breakpoint %s", argStr);
        strlcat(pResponse, "", maxResponseLen);
    }
    else if (matches(cmdStr, "enable-breakpoints", MAX_CMD_STR_LEN))
    {
        strlcat(pResponse, "", maxResponseLen);
    }
    else if (matches(cmdStr, "disable-breakpoint", MAX_CMD_STR_LEN))
    {
        LogWrite(FromTargetDebug, LOG_DEBUG, "disable breakpoint %s", argStr);
        strlcat(pResponse, "", maxResponseLen);
    }
    else if (matches(cmdStr, "disable-breakpoints", MAX_CMD_STR_LEN))
    {
        strlcat(pResponse, "", maxResponseLen);
    }
    else if (matches(cmdStr, "get-registers", MAX_CMD_STR_LEN))
    {
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
            LogWrite(FromTargetDebug, LOG_DEBUG, "cpu-step done");
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
            LogWrite(FromTargetDebug, LOG_DEBUG, "now paused SEND-BLANK");
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

uint32_t TargetDebug::handleInterrupt([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, 
            [[maybe_unused]] uint32_t flags, [[maybe_unused]] uint32_t retVal,
            [[maybe_unused]] McDescriptorTable& descriptorTable)
{
    // Only handle MREQs
    if ((flags & BR_CTRL_BUS_MREQ_MASK) == 0)
        return retVal;

    // Instructions to get register values
    static uint8_t regQueryInstructions[] = 
    {
        0x18, 0xfe
    };

    // Check if we're in register query mode
    if (_registerQueryMode)
    {
        // Send instructions to query registers
        retVal = regQueryInstructions[_registerQueryStep++] | BR_MEM_ACCESS_INSTR_INJECT;

        // Check if complete
        if (_registerQueryStep >= sizeof(regQueryInstructions))
            _registerQueryMode = false;
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

