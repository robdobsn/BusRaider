// Bus Raider
// Rob Dobson 2018

#pragma once

#include "BusAccessDefs.h"
#include "TargetCPU.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Callback types
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Callback types
typedef void BusAccessCBFnType(void* pObject, uint32_t addr, uint32_t data, uint32_t flags, uint32_t& curRetVal);
// typedef void BusActionCBFnType(void* pObject, BR_BUS_ACTION actionType, BR_BUS_REQ_REASON reason, BR_RETURN_TYPE rslt);
typedef void BusReqAckedCBFnType(void* pObject, BR_BUS_REQ_REASON reason, BR_RETURN_TYPE rslt);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Bus Socket Info - this is used to plug-in to the BusAccess layer
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class BusSocketInfo
{
public:
    void setSocketInfo(bool enabled, 
            BusAccessCBFnType* busAccessCallback,
            BusReqAckedCBFnType* busReqAckedCallback,
            void* pSourceObject)

            // // BusActionCBFnType* busActionCallback, 
            // bool waitOnMemory, bool waitOnIO,
            // bool resetPending, uint32_t resetDurationMs,
            // bool nmiPending, uint32_t nmiDurationTStates,
            // bool irqPending, uint32_t irqDurationTStates,
            // bool busMasterRequest, BR_BUS_REQ_REASON busMasterReason,
            // bool holdInWaitReq, )
    {
        // Socket enablement
        this->enabled = enabled;

        // Callbacks
        this->busAccessCallback = busAccessCallback;
        this->busReqAckedCallback = busReqAckedCallback;
        this->pSourceObject = pSourceObject;

        // Flags
        this->waitOnMemory = false;
        this->waitOnIO = false;

        // Bus actions and duration
        // this->resetPending = false;
        // this->resetDurationMs = 0;
        // this->nmiPending = false;
        // this->nmiDurationTStates = 0;
        // this->irqPending = false;
        // this->irqDurationTStates = 0;

        // Bus master request and reason
        // this->busMasterRequest = false;
        // this->busMasterReason = BR_BUS_REQ_REASON_GENERAL;

        // Bus hold in wait
        // this->holdInWaitReq = false;
    }

    // Socket enablement
    bool enabled;

    // Callbacks
    BusAccessCBFnType* busAccessCallback;
    BusReqAckedCBFnType* busReqAckedCallback;
    void* pSourceObject;

    // Flags
    bool waitOnMemory;
    bool waitOnIO;

    // Bus actions and duration
    // volatile bool resetPending;
    // volatile uint32_t resetDurationMs;
    // volatile bool nmiPending;
    // volatile uint32_t nmiDurationTStates;
    // volatile bool irqPending;
    // volatile uint32_t irqDurationTStates;

    // Bus master request and reason
    // volatile bool busMasterRequest;
    // volatile BR_BUS_REQ_REASON busMasterReason;

    // Bus hold in wait
    // volatile bool holdInWaitReq;

    // // Get type of bus action
    // BR_BUS_ACTION getType()
    // {
    //     if (busMasterRequest)
    //         return BR_BUS_ACTION_BUSRQ;
    //     if (resetPending)
    //         return BR_BUS_ACTION_RESET;
    //     if (nmiPending)
    //         return BR_BUS_ACTION_NMI;
    //     if (irqPending)
    //         return BR_BUS_ACTION_IRQ;
    //     return BR_BUS_ACTION_NONE;
    // }

    // void clearDown(BR_BUS_ACTION type)
    // {
    //     if (type == BR_BUS_ACTION_BUSRQ)
    //         busMasterRequest = false;
    //     else if (type == BR_BUS_ACTION_RESET)
    //         resetPending = false;
    //     else if (type == BR_BUS_ACTION_NMI)
    //         nmiPending = false;
    //     else if (type == BR_BUS_ACTION_IRQ)
    //         irqPending = false;
    // }

    // void clearPending()
    // {
    //     busMasterRequest = false;
    //     resetPending = false;
    //     nmiPending = false;
    //     irqPending = false;
    // }

    // // Time calc
    // static uint32_t getUsFromTStates(uint32_t tStates, uint32_t clockFreqHz, uint32_t defaultTStates = 1)
    // {
    //     uint32_t tS = tStates >= 1 ? tStates : defaultTStates;
    //     uint32_t uS = 1000000 * tS / clockFreqHz;
    //     if (uS <= 0)
    //         uS = 1;
    //     return uS;
    // }

    // uint32_t getAssertUs(uint32_t clockFreqHz)
    // {
    //     BR_BUS_ACTION type = getType();
    //     if (type == BR_BUS_ACTION_BUSRQ)
    //         return getUsFromTStates(BR_MAX_WAIT_FOR_BUSACK_T_STATES, clockFreqHz);
    //     else if (type == BR_BUS_ACTION_RESET)
    //         return resetDurationMs * 1000;
    //     else if (type == BR_BUS_ACTION_NMI)
    //         return getUsFromTStates(nmiDurationTStates, clockFreqHz, BR_NMI_PULSE_T_STATES);
    //     else if (type == BR_BUS_ACTION_IRQ)
    //         return getUsFromTStates(irqDurationTStates, clockFreqHz, BR_IRQ_PULSE_T_STATES);
    //     return 0;
    // }
};
