// Bus Raider
// Rob Dobson 2018

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <circle/bcm2835.h>
#include <circle/memio.h>
#include "lowlib.h"
#include "lowlev.h"
#include "BusAccess.h"
#include "CPUHandler.h"
#include "BusSocketManager.h"
#include "BusAccessDefs.h"
#include "BusClockGenerator.h"
#include "MemoryInterface.h"
#include "BusRawAccess.h"
#include "HwManager.h"
#include "TargetProgrammer.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Bus control
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class BusControl
{
public:
    BusControl();

    // Initialization
    void init();

    // Machine change
    void machineChangeInit();
    void machineChangeComplete();

    // Service
    void service();

    // Raw access start/end
    void rawAccessStart();
    void rawAccessEnd();

    // // Target control
    // inline CPUHandler& ctrl()
    // {
    //     return _cpuHandler;
    // }    

    // // Bus socket manager
    // inline BusSocketManager& sock()
    // {
    //     return _busSocketManager;
    // }

    // // Target clock generator
    // inline BusClockGenerator& clock()
    // {
    //     return _clockGenerator;
    // }

    // // Memory controller
    // inline MemoryInterface& mem()
    // {
    //     return _memoryInterface;
    // }

    // // Raw bus access
    // inline BusRawAccess& bus()
    // {
    //     return _busRawAccess;
    // }

    // // Hardware
    // inline HwManager& hw()
    // {
    //     return _hwManager;
    // }

    // Check if debugging
    bool isDebugging()
    {
        return _cpuHandler.isDebugging();
    }

    // Get return type string
    const char* retcString(BR_RETURN_TYPE retc)
    {
        switch (retc)
        {
            case BR_OK: return "ok";
            case BR_ERR: return "general error";
            case BR_ALREADY_DONE: return "already paused";
            case BR_NO_BUS_ACK: return "BUSACK not received";
            default: return "unknown error";
        }
    }

    // Bus access to data
    BR_RETURN_TYPE blockAccessSync(uint32_t addr, uint8_t *pData, uint32_t len, bool iorq,
                                   bool read, bool write);

    // Imager functions
    void imagerClear()
    {
        _targetImager.clear();
    }
    void imagerAddMemoryBlock(uint32_t addr, 
            const uint8_t* pParams, unsigned paramsLen)
    {
        _targetImager.addMemoryBlock(addr, pParams, paramsLen);
    }
    void imagerSetRegistersCodeAddr(uint32_t codeAddr)
    {
        _targetImager.setSetRegistersCodeAddr(codeAddr);
    }
    TargetImager& imager()
    {
        return _targetImager;
    }

    // Programmer functions
    void programmingStart(bool execAfterProgramming, bool enterDebugger)
    {
        _targetProgrammer.programmingStart(execAfterProgramming, enterDebugger);
    }

    // Bus sockets
    int socketAdd(bool enabled, CPUCycleCBType* busAccessCallback,
            CPUBusReqCBType* busReqAckedCallback, 
            void* pSourceObject)
    {
        return _busSocketManager.socketAdd(enabled, busAccessCallback, 
                busReqAckedCallback, pSourceObject);
    }
    void socketEnable(uint32_t busSocket, bool enable)
    {
        _busSocketManager.enable(busSocket, enable);
    }
    void socketRequestBus(uint32_t busSocketIdx, BR_BUS_REQ_REASON busReqReason)
    {
        _busSocketManager.reqBus(busSocketIdx, busReqReason);
    }

private:
    // State of bus
    bool _isInitialized;

    // Clock generator
    BusClockGenerator _clockGenerator;

    // CPUHandler
    CPUHandler _cpuHandler;

    // Bus Sockets
    BusSocketManager _busSocketManager;

    // MemoryInterface
    MemoryInterface _memoryInterface;

    // Bus raw access
    BusRawAccess _busRawAccess;

    // Hardware manager
    HwManager _hwManager;

    // Target programmer
    TargetProgrammer _targetProgrammer;

    // Target Imager
    TargetImager _targetImager;
};
