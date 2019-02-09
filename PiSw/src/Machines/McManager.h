// Bus Raider Machine Manager
// Rob Dobson 2018

#pragma once

#include "McBase.h"
#include "../TargetBus/TargetClockGenerator.h"
#include "../Debugger/TargetDebug.h"
#include "../System/ee_printf.h"
#include "../CommandInterface/CommandHandler.h"
#include <string.h> 

class McManager
{
private:
    static McDescriptorTable defaultDescriptorTable;

private:
   
    // Characters received from the host
    static const int MAX_RX_HOST_CHARS = 2000;
    static uint8_t _rxHostCharsBuffer[MAX_RX_HOST_CHARS+1];
    static int _rxHostCharsBufferLen;

    // Command handler
    static CommandHandler* _pCommandHandler;

public:

    static const int MAX_MACHINES = 10;
    static const int MAX_MACHINE_NAME_LEN = 100;
    static McBase* _pMachines[MAX_MACHINES];
    static int _numMachines;
    static int _curMachineIdx;
    static TargetClockGenerator _clockGen;

    static void add(McBase* pMachine);
    static bool setMachineIdx(int mcIdx);
    static bool setMachineByName(const char* mcName);

    static int getNumMachines()
    {
        return _numMachines;
    }

    static McBase* getMachine()
    {
        if ((_curMachineIdx < 0) || (_curMachineIdx >= _numMachines))
        {
            if (_numMachines > 0)
                return _pMachines[0];
            return NULL;
        }
        return _pMachines[_curMachineIdx];
    }

    static McDescriptorTable* getDescriptorTable(int subType)
    {
        McBase* pMc = getMachine();
        if (pMc)
            return pMc->getDescriptorTable(subType);
        return &defaultDescriptorTable;
    }

    static const char* getMachineJSON()
    {
        static const int MAX_MC_JSON_LEN = MAX_MACHINES * (MAX_MACHINE_NAME_LEN + 10) + 20;
        static char mcString[MAX_MC_JSON_LEN+1];

        // Machine list
        strlcpy(mcString, "\"machineList\":[", MAX_MC_JSON_LEN);
        for (int i = 0; i < getNumMachines(); i++)
        {
            if (i != 0)
                strlcpy(mcString+strlen(mcString),",", MAX_MC_JSON_LEN);
            strlcpy(mcString+strlen(mcString),"\"", MAX_MC_JSON_LEN);
            strlcpy(mcString+strlen(mcString), _pMachines[i]->getDescriptorTable(0)->machineName, MAX_MC_JSON_LEN);
            strlcpy(mcString+strlen(mcString),"\"", MAX_MC_JSON_LEN);
        }
        strlcpy(mcString+strlen(mcString),"]", MAX_MC_JSON_LEN);

        // Current machine
        strlcpy(mcString+strlen(mcString),",\"machineCur\":", MAX_MC_JSON_LEN);
        strlcpy(mcString+strlen(mcString), "\"", MAX_MC_JSON_LEN);
        if ((_curMachineIdx >= 0) && (_curMachineIdx < _numMachines))
            strlcpy(mcString+strlen(mcString), getDescriptorTable(_curMachineIdx)->machineName, MAX_MC_JSON_LEN);
        strlcpy(mcString+strlen(mcString), "\"", MAX_MC_JSON_LEN);

        // Ret
        return mcString;
    }

    static void handleRxCharFromHost(const uint8_t* pRxChars, int rxLen)
    {
        // Check for overflow
        if (rxLen + _rxHostCharsBufferLen >= MAX_RX_HOST_CHARS)
        {
            // Discard old data as probably not being used
            _rxHostCharsBufferLen = 0;
        }

        // Check there is space
        if (rxLen + _rxHostCharsBufferLen >= MAX_RX_HOST_CHARS)
            return;

        // Add to buffer
        memcpy(_rxHostCharsBuffer+_rxHostCharsBufferLen, pRxChars, rxLen);
        _rxHostCharsBufferLen += rxLen;
        *(_rxHostCharsBuffer+_rxHostCharsBufferLen) = 0;
    }

    static int getNumCharsReceivedFromHost()
    {
        return _rxHostCharsBufferLen;
    }

    static int getCharsReceivedFromHost(uint8_t* pBuf, int bufMaxLen)
    {
        if ((!pBuf) || (bufMaxLen < _rxHostCharsBufferLen))
            return 0;
        memcpy(pBuf, _rxHostCharsBuffer, _rxHostCharsBufferLen);
        int retVal = _rxHostCharsBufferLen;
        _rxHostCharsBufferLen = 0;
        return retVal;
    }

    static void sendKeyCodeToTarget(int asciiCode)
    {
        if (_pCommandHandler)
            _pCommandHandler->sendKeyCodeToTarget(asciiCode);
    }

    static void sendDebugMessage(const char* pStr)
    {
        LogWrite("McManager", LOG_DEBUG, "debugMsg %s cmdH %d\n", pStr, _pCommandHandler); 
        if (_pCommandHandler)
            _pCommandHandler->sendDebugMessage(pStr);
    }

    static void init(CommandHandler* pCommandHandler)
    {
        // Add a callback for received characters from target
        pCommandHandler->setRxFromTargetCallback(handleRxCharFromHost);
        _pCommandHandler = pCommandHandler;
        
        // Let the target debugger know how to communicate
        TargetDebug::get()->setSendDebugMessageCallback(sendDebugMessage);
    }

    static bool debuggerCommand(const char* pCommand, char* pResponse, int maxResponseLen)
    {
        McBase* pMc = getMachine();
        // LogWrite("McManager", LOG_DEBUG, "debuggerCommand %s %d %d\n", pCommand, 
        //         maxResponseLen, pMc);
        if (pMc)
            return pMc->debuggerCommand(pCommand, pResponse, maxResponseLen);
        return false;
    }
};
