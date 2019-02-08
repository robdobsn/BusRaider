// Bus Raider
// Rob Dobson 2019

#include "TargetDebug.h"
#include "../Utils/rdutils.h"
#include <string.h>
#include <stdlib.h>
#include "../System/ee_printf.h"
#include "../TargetBus/BusAccess.h"

// Module name
static const char FromTargetDebug[] = "TargetDebug";

// Debugger
TargetDebug __targetDebug;

// Memory buffer
uint8_t TargetDebug::_targetMemBuffer[MAX_MEM_DUMP_LEN];

// Get target
TargetDebug* TargetDebug::get()
{
    return &__targetDebug;
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
    // Trim command string
    int j = 0;
    for (size_t i = 0; i < strlen(cmdStr); i++)
    {
        if (!rdisspace(cmdStr[i]))
        {
            cmdStr[j++] = cmdStr[i];
        }
    }
    cmdStr[j] = 0;
    char* argStr = strtok(NULL, " ");
    char* argStr2 = strtok(NULL, " ");

    if ((strcasecmp(cmdStr, "about") == 0))
    {
        strlcat(pResponse, "BusRaider RCP", maxResponseLen);
    }
    else if (strcasecmp(cmdStr, "get-version") == 0)
    {
        strlcat(pResponse, "7.2-SN", maxResponseLen);
    }
    else if (strcasecmp(cmdStr, "get-current-machine") == 0)
    {
        strlcat(pResponse, pMachine->getDescriptorTable(0)->machineName, maxResponseLen);
    }
    else if (strcasecmp(cmdStr, "set-debug-settings") == 0)
    {
        strlcat(pResponse, "", maxResponseLen);
    }
    else if (strcasecmp(cmdStr, "hard-reset-cpu") == 0)
    {
        pMachine->reset();
        strlcat(pResponse, "", maxResponseLen);
    }
    else if (strcasecmp(cmdStr, "enter-cpu-step") == 0)
    {
        BusAccess::waitEnable(pMachine->getDescriptorTable(0)->monitorIORQ, true);
        strlcat(pResponse, "", maxResponseLen);
    }
    else if (strcasecmp(cmdStr, "exit-cpu-step") == 0)
    {
        BusAccess::waitEnable(pMachine->getDescriptorTable(0)->monitorIORQ, pMachine->getDescriptorTable(0)->monitorMREQ);
        strlcat(pResponse, "", maxResponseLen);
    }
    else if (strcasecmp(cmdStr, "smartload") == 0)
    {
        LogWrite(FromTargetDebug, LOG_DEBUG, "smartload %s", argStr);
        strlcat(pResponse, "", maxResponseLen);
    }
    else if (strcasecmp(cmdStr, "clear-membreakpoints") == 0)
    {
        strlcat(pResponse, "", maxResponseLen);
    }
    else if (strcasecmp(cmdStr, "enable-breakpoints") == 0)
    {
        strlcat(pResponse, "", maxResponseLen);
    }
    else if (strcasecmp(cmdStr, "disable-breakpoint") == 0)
    {
        LogWrite(FromTargetDebug, LOG_DEBUG, "disable breakpoint %s", argStr);
        strlcat(pResponse, "", maxResponseLen);
    }
    else if (strcasecmp(cmdStr, "get-registers") == 0)
    {
        _z80Registers.format1(pResponse, maxResponseLen);
    }
    else if (strcasecmp(cmdStr, "read-memory") == 0)
    {
        // LogWrite(FromTargetDebug, LOG_DEBUG, "read mem %s %s", argStr, argStr2);
        int startAddr = strtol(argStr, NULL, 10);
        int leng = strtol(argStr2, NULL, 10);
        if ((startAddr >= 0 && startAddr <= MAX_TARGET_MEM_ADDR) && 
                (leng > 0 && leng <= MAX_MEM_DUMP_LEN))
        {
            BusAccess::blockRead(startAddr, _targetMemBuffer, leng, true, false);
            for (int i = 0; i < leng; i++)
            {
                char chBuf[10];
                ee_sprintf(chBuf, "%02x", _targetMemBuffer[i]);
                strlcat(pResponse, chBuf, maxResponseLen);
            }
        }
    }
    else if (strcasecmp(cmdStr, "reset-tstates-partial") == 0)
    {
        strlcat(pResponse, "Unknown command", maxResponseLen);
    }
    else if (strcasecmp(cmdStr, "get-cpu-frequency") == 0)
    {
        strlcat(pResponse, "Unknown command", maxResponseLen);
    }
    else if (strcasecmp(cmdStr, "get-stack-backtrace") == 0)
    {
        int startAddr = _z80Registers.SP;
        int numFrames = strtol(argStr, NULL, 10);
        if ((numFrames > 0) && (numFrames < MAX_MEM_DUMP_LEN / 2))
        {
            BusAccess::blockRead(startAddr, _targetMemBuffer, numFrames*2, true, false);
            for (int i = 0; i < numFrames; i++)
            {
                char chBuf[20];
                ee_sprintf(chBuf, "%04xH ", _targetMemBuffer[i*2+1] * 256 + _targetMemBuffer[i*2]);
                strlcat(pResponse, chBuf, maxResponseLen);
            }
        }
    }
    else if (strcasecmp(cmdStr, "run") == 0)
    {
        BusAccess::pauseRelease();
        strlcat(pResponse, "Running until a breakpoint, key press or data sent, menu opening or other event\n\n", maxResponseLen);
        return true;
    }
    else if (strlen(cmdStr) == 0)
    {
        // Blank is used for pause
        LogWrite(FromTargetDebug, LOG_DEBUG, "now paused %s", cmdStr);
        BusAccess::pause();
        strlcat(pResponse, "", maxResponseLen);
    }
    else
    {
        strlcat(pResponse, "Unknown command", maxResponseLen);
    }
    

    // Complete and add command request
    strlcat(pResponse, (BusAccess::pauseIsPaused() ? "\ncommand@cpu-step> " : "\ncommand> "), maxResponseLen);

    // LogWrite(FromTargetDebug, LOG_DEBUG, "resp %s", pResponse);

    return true;
}

uint32_t TargetDebug::handleInterrupt([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, 
        [[maybe_unused]] uint32_t flags, [[maybe_unused]] uint32_t retVal)
{
    return retVal;
}

