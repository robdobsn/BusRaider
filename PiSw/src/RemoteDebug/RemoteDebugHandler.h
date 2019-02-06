#pragma once

#include "../Utils/rdutils.h"
#include <string.h>
#include <stdlib.h>
#include "../System/ee_printf.h"

// Module name
static const char FromRemoteDebug[] = "RemoteDebugHandler";

class RemoteDebugHandler
{
private:
    static const int MAX_CMD_STR_LEN = 200;
    bool _singleStepMode;

public:
    RemoteDebugHandler()
    {
        _singleStepMode = false;
    }

    bool isSingleStepMode()
    {
        return _singleStepMode;
    }

    void handleCommand(const char* pCmdJson, const uint8_t* pParams, int paramsLen, char* pResponseJson, int maxResponseLen)
    {
        // Extract command
        #define MAX_CMD_PARAM_STR 30
        char indexValStr[MAX_CMD_PARAM_STR+1];
        if (!jsonGetValueForKey("index", pCmdJson, indexValStr, MAX_CMD_PARAM_STR))
        {
            LogWrite(FromRemoteDebug, LOG_DEBUG, "NO INDEX VAL");
            return;
        }
        char lenValStr[MAX_CMD_PARAM_STR+1];
        if (!jsonGetValueForKey("len", pCmdJson, lenValStr, MAX_CMD_PARAM_STR))
        {
            LogWrite(FromRemoteDebug, LOG_DEBUG, "NO len VAL");
            return;
        }
        int lenVal = strtol(lenValStr, NULL, 10);
        if ((lenVal < 0) || (lenVal > MAX_CMD_STR_LEN))
        {
            LogWrite(FromRemoteDebug, LOG_DEBUG, "too long val %d", lenVal);
            return;
        }
        if (lenVal > paramsLen)
            lenVal = paramsLen;
        char fullStr[MAX_CMD_STR_LEN+1];
        memcpy(fullStr, pParams, lenVal);
        fullStr[lenVal] = 0;

        // Split
        char* cmdStr = strtok(fullStr, " ");
        char* argStr = strtok(NULL, " ");
        char* argStr2 = strtok(NULL, " ");

        // Check the specific command
        strlcpy(pResponseJson, "\"index\":\"", maxResponseLen);
        strlcat(pResponseJson, indexValStr, maxResponseLen);
        strlcat(pResponseJson, "\",\"content\":\"", maxResponseLen);
        
        // Add content
        if ((strcasecmp(cmdStr, "about") == 0))
        {
            strlcat(pResponseJson, "BusRaider RCP", maxResponseLen);
        }
        else if (strcasecmp(cmdStr, "get-version") == 0)
        {
            strlcat(pResponseJson, "7.2-SN", maxResponseLen);
        }
        else if (strcasecmp(cmdStr, "get-current-machine") == 0)
        {
            strlcat(pResponseJson, "Spectrum 48K", maxResponseLen);
        }
        else if (strcasecmp(cmdStr, "set-debug-settings") == 0)
        {
            strlcat(pResponseJson, "", maxResponseLen);
        }
        else if (strcasecmp(cmdStr, "hard-reset-cpu") == 0)
        {
            strlcat(pResponseJson, "", maxResponseLen);
        }
        else if (strcasecmp(cmdStr, "enter-cpu-step") == 0)
        {
            _singleStepMode = true;
            strlcat(pResponseJson, "", maxResponseLen);
        }
        else if (strcasecmp(cmdStr, "smartload") == 0)
        {
            LogWrite(FromRemoteDebug, LOG_DEBUG, "smartload %s", argStr);
            strlcat(pResponseJson, "", maxResponseLen);
        }
        else if (strcasecmp(cmdStr, "clear-membreakpoints") == 0)
        {
            strlcat(pResponseJson, "", maxResponseLen);
        }
        else if (strcasecmp(cmdStr, "enable-breakpoints") == 0)
        {
            strlcat(pResponseJson, "", maxResponseLen);
        }
        else if (strcasecmp(cmdStr, "disable-breakpoint") == 0)
        {
            LogWrite(FromRemoteDebug, LOG_DEBUG, "disable breakpoint %s", argStr);
            strlcat(pResponseJson, "", maxResponseLen);
        }
        else if (strcasecmp(cmdStr, "get-registers") == 0)
        {
            strlcat(pResponseJson, "PC=6062 SP=fdfc BC=2302 AF=f482 HL=6319 DE=4208 IX=663c IY=5c3a AF'=0044 BC'=050e HL'=2758 DE'=0047 I=3f R=33  F=S-----N- F'=-Z---P-- MEMPTR=15e6 IM1 IFF12 VPS: 0 ", maxResponseLen);
        }
        else if (strcasecmp(cmdStr, "read-memory") == 0)
        {
            LogWrite(FromRemoteDebug, LOG_DEBUG, "read mem %s %s", argStr, argStr2);
            strlcat(pResponseJson, "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000", maxResponseLen);
        }
        else if (strcasecmp(cmdStr, "reset-tstates-partial") == 0)
        {
            strlcat(pResponseJson, "Unknown command", maxResponseLen);
        }
        else
        {
            strlcat(pResponseJson, "Unknown command", maxResponseLen);
        }

        // Complete and add command request
        strlcat(pResponseJson, isSingleStepMode() ? "\ncommand@cpu-step> \"" : "\ncommand> \"", maxResponseLen);

        LogWrite(FromRemoteDebug, LOG_DEBUG, "respJson %s", pResponseJson);
    }
};


