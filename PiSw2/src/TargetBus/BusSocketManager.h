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
    void init();

    // Bus Sockets - used to hook things like waitInterrupts, busControl, etc
    int addSocket(bool enabled, BusAccessCBFnType* busAccessCallback,
            BusReqAckedCBFnType* busReqAckedCallback, 
            void* pSourceObject);

    // Enablement
    void enable(uint32_t busSocket, bool enable);
    bool isEnabled(uint32_t busSocket);

    // Suspend socket activity
    void suspend(bool suspend, bool clearPendingActions);

    // Socket requests
    void reqReset(uint32_t busSocket);
    void reqINT(uint32_t busSocket);
    void reqNMI(uint32_t busSocket);
    void reqBus(uint32_t busSocket, BR_BUS_REQ_REASON busReqReason);

private:
    // Sockets
    static const int MAX_BUS_SOCKETS = 10;
    BusSocketInfo _busSockets[MAX_BUS_SOCKETS];
    uint32_t _busSocketCount;

    // Bus control
    BusControl& _busControl;

    // Suspended
    bool _isSuspended;

    // Callbacks
    static void busReqAckedStatic(void* pObject, BR_BUS_REQ_REASON reason, 
                        BR_RETURN_TYPE rslt);
    void busReqAcked(BR_BUS_REQ_REASON reason, BR_RETURN_TYPE rslt);

    static void busAccessCallbackStatic(void* pObject, uint32_t addr, 
                        uint32_t data, uint32_t flags, uint32_t& curRetVal);
    void busAccessCallback(uint32_t addr, uint32_t data, 
                        uint32_t flags, uint32_t& curRetVal);
};