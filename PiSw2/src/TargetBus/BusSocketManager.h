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
#include "BusSocketInfo.h"

class BusControl;

class BusSocketManager
{
public:
    BusSocketManager(BusControl& busControl);

    // Bus Sockets - used to hook things like waitInterrupts, busControl, etc
    int add(bool enabled, BusAccessCBFnType* busAccessCallback,
            BusActionCBFnType* busActionCallback, 
            bool waitOnMemory, bool waitOnIO,
            bool resetPending, uint32_t resetDurationTStates,
            bool nmiPending, uint32_t nmiDurationTStates,
            bool irqPending, uint32_t irqDurationTStates,
            bool busMasterRequest, BR_BUS_ACTION_REASON busMasterReason,
            bool holdInWaitReq, void* pSourceObject);
    void enable(uint32_t busSocket, bool enable);
    bool isEnabled(uint32_t busSocket);
    void setup(uint32_t busSocket, bool waitOnMem, bool waitOnIO);

    // Suspend socket activity
    void suspend(bool suspend, bool clearPendingActions);

    // Socket requests
    void reqIRQ(uint32_t busSocket, int durationTStates = -1);
    void reqReset(uint32_t busSocket, int durationTStates = -1);
    void reqNMI(uint32_t busSocket, int durationTStates = -1);
    void reqBus(uint32_t busSocket, BR_BUS_ACTION_REASON busMasterReason);

private:
    // Sockets
    static const int MAX_BUS_SOCKETS = 10;
    BusSocketInfo _busSockets[MAX_BUS_SOCKETS];
    uint32_t _busSocketCount;

    // Bus control
    BusControl& _busControl;

    // Wait config
    bool _waitOnMemory;
    bool _waitOnIO;

    // Suspended
    bool _isSuspended;

    // Socket with pending or in-process action
    // -1 is no action
    int _socketWithAction;

    // Set action from socket manager
    // socketWithAction == -1 if no action pending
    void socketSetAction(bool memWait, bool ioWait, int socketWithAction);

    // Helpers
    void clearPending();
    void updateAfterSocketChange();
    static void cycleActionStaticCB(void* pObject, uint32_t slotIdx, BR_RETURN_TYPE rslt);
    void cycleActionCB(uint32_t slotIdx, BR_RETURN_TYPE rslt);
};