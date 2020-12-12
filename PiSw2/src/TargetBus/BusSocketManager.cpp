// Bus Raider
// Rob Dobson 2018-2020

#include "BusSocketManager.h"
#include "BusControl.h"
#include "DebugHelper.h"

// Module name
static const char MODULE_PREFIX[] = "BusSocketMgr";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BusSocketManager::BusSocketManager(BusControl& busControl)
    : _busControl(busControl)
{
    // Bus sockets
    _busSocketCount = 0;
    // _socketActionRequested = false;
    _isSuspended = false;

    // Wait settings
    _waitOnIO = false;
    _waitOnMemory = false;

    // Update after socket change
    updateAfterSocketChange();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Init
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusSocketManager::init()
{
    _busControl.ctrl().registerCallbacks(busAccessCallbackStatic, busReqAckedStatic, this);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Suspend socket activity
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusSocketManager::suspend(bool suspend, bool clearPendingActions)
{
    _busControl.bus().waitSuspend(suspend);
    _isSuspended = suspend;
    // if (clearPendingActions)
    //     clearAllPending();
    // updateAfterSocketChange();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Add a bus socket
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int BusSocketManager::addSocket(bool enabled, BusAccessCBFnType* busAccessCallback,
            BusReqAckedCBFnType* busReqAckedCallback, 
            void* pSourceObject)
            // bool waitOnMemory, bool waitOnIO,
            // bool resetPending, uint32_t resetDurationTStates,
            // bool nmiPending, uint32_t nmiDurationTStates,
            // bool irqPending, uint32_t irqDurationTStates,
            // bool busMasterRequest, BR_BUS_REQ_REASON busReqReason,
            // bool holdInWaitReq, 
{
    // Check if all used
    if (_busSocketCount >= MAX_BUS_SOCKETS)
        return -1;

    // Add socket info
    _busSockets[_busSocketCount].setSocketInfo(
            enabled, 
            busAccessCallback, 
            busReqAckedCallback,
            pSourceObject);
            // busActionCallback, 
            // waitOnMemory, waitOnIO,
            // resetPending, resetDurationTStates,
            // nmiPending, nmiDurationTStates,
            // irqPending, irqDurationTStates,
            // busMasterRequest, busReqReason,
            // holdInWaitReq, pSourceObject);
    uint32_t tmpCount = _busSocketCount++;

    // Update wait state generation
    // LogWrite(MODULE_PREFIX, LOG_NOTICE, "busSocketAdd %d", tmpCount);
    // updateAfterSocketChange();

    return tmpCount;
}

void BusSocketManager::enable(uint32_t busSocket, bool enable)
{
    // Check validity
    if (busSocket >= _busSocketCount)
        return;

    // Enable/disable
    _busSockets[busSocket].enabled = enable;

    // Update wait state generation
    // LogWrite(MODULE_PREFIX, LOG_DEBUG, "busSocketEnable");
    updateAfterSocketChange();
}

bool BusSocketManager::isEnabled(uint32_t busSocket)
{
    // Check validity
    if (busSocket >= _busSocketCount)
        return false;
    return _busSockets[busSocket].enabled;
}

void BusSocketManager::setup(uint32_t busSocket, bool waitOnMem, bool waitOnIO)
{
    // Check validity
    if (busSocket >= _busSocketCount)
        return;
    _busSockets[busSocket].waitOnMemory = waitOnMem;
    _busSockets[busSocket].waitOnIO = waitOnIO;
    updateAfterSocketChange();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Socket Functions
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Reset the host
void BusSocketManager::reqReset(uint32_t busSocket)
{
    _busControl.ctrl().targetReset();

    // // Check validity
    // if (busSocket >= _busSocketCount)
    //     return;
    // _busSockets[busSocket].resetDurationMs = (durationMs <= 0) ? BR_RESET_PULSE_MS : durationMs;
    // _busSockets[busSocket].resetPending = true;
    // // LogWrite(MODULE_PREFIX, LOG_DEBUG, "reqReset");
    // updateAfterSocketChange();
}

// Maskable interrupt the host
void BusSocketManager::reqINT(uint32_t busSocket)
{
    _busControl.ctrl().targetINT();
    // // Check validity
    // // LogWrite(MODULE_PREFIX, LOG_DEBUG, "ReqIRQ sock %d t-states %d", busSocket, durationTStates);
    // if (busSocket >= _busSocketCount)
    //     return;
    // // Request IRQ
    // _busSockets[busSocket].irqDurationTStates = (durationTStates <= 0) ? BR_IRQ_PULSE_T_STATES : durationTStates;
    // _busSockets[busSocket].irqPending = true;
    // updateAfterSocketChange();
}

// Non-maskable interrupt the host
void BusSocketManager::reqNMI(uint32_t busSocket)
{
    _busControl.ctrl().targetNMI();

    // // Check validity
    // if (busSocket >= _busSocketCount)
    //     return;
    // // Request NMI
    // _busSockets[busSocket].nmiDurationTStates = (durationTStates <= 0) ? BR_NMI_PULSE_T_STATES : durationTStates;
    // _busSockets[busSocket].nmiPending = true;
    // updateAfterSocketChange();
}

// Bus request
void BusSocketManager::reqBus(uint32_t busSocketIdx, BR_BUS_REQ_REASON busReqReason)
{
    _busControl.ctrl().reqBus(busReqReason, busSocketIdx);

    // // Check validity
    // if (busSocket >= _busSocketCount)
    // {
    //     LogWrite(MODULE_PREFIX, LOG_DEBUG, "reqBus sock %d invalid count = %d", busSocket, _busSocketCount);
    //     return;
    // }
    // // LogWrite(MODULE_PREFIX, LOG_DEBUG, "reqBus reason %d", busReqReason);
    // _busSockets[busSocket].busMasterRequest = true;
    // _busSockets[busSocket].busReqReason = busReqReason;
    // updateAfterSocketChange();

    // Bus request can be handled immediately as the BUSRQ line is not part of the multiplexer (so is not affected by WAIT processing)

    // TODO 2020
    // busActionCheck();
    // busActionHandleStart();
    // LogWrite(MODULE_PREFIX, LOG_DEBUG, "reqBus sock %d enabled %d reason %d", busSocket, _busSockets[busSocket].enabled, busReqReason);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Update after changes made to a socket
void BusSocketManager::updateAfterSocketChange()
{
    // LogWrite(MODULE_PREFIX, LOG_DEBUG, "updateAfterSocketChange suspended %d", _isSuspended); 

    // Check if suspended
    if (_isSuspended)
        return;

    // Iterate bus sockets to see if any enable Mem/IO wait states
    bool ioWait = false;
    bool memWait = false;
    for (uint32_t i = 0; i < _busSocketCount; i++)
    {
        if (_busSockets[i].enabled)
        {
            memWait = memWait || _busSockets[i].waitOnMemory;
            ioWait = ioWait || _busSockets[i].waitOnIO;
        }
    }

    // Check for changes to wait system
    if ((_waitOnMemory != memWait) || (_waitOnMemory != ioWait))
    {
        // Store flags
        _waitOnMemory = memWait;
        _waitOnIO = ioWait;

        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "WAIT ENABLEMENT mreq %d iorq %d", memWait, ioWait);

        // Set raw
        _busControl.bus().waitConfigSocket(_waitOnMemory, _waitOnIO);

        // Debug
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "WAIT UPDATE mem %d io %d", _waitOnMemory, _waitOnIO);
    }

    // // See if any actions requested
    // for (uint32_t i = 0; i < _busSocketCount; i++)
    // {
    //     BR_BUS_ACTION actionType = _busSockets[i].getType();
    //     if (_busSockets[i].enabled && (actionType != BR_BUS_ACTION_NONE))
    //     {
    //         // LogWrite(MODULE_PREFIX, LOG_DEBUG, "updateAfterSocketChange sockWithAction %d actionType %d", i, actionType); 

    //         // Request action
    //         BR_BUS_REQ_REASON reason = actionType == BR_BUS_ACTION_BUSRQ ? _busSockets[i].busReqReason : BR_BUS_REQ_REASON_GENERAL;
    //         if (_busControl.ctrl().cycleReqAction(actionType,
    //                         reason,
    //                         i,
    //                         _busSockets[i].getAssertUs(_busControl.clock().getFreqInHz()),
    //                         cycleActionStaticCB, 
    //                         this))
    //         {
    //             // Action now requested
    //             _socketActionRequested = true;
    //             // LogWrite(MODULE_PREFIX, LOG_NOTICE, "socketSetAction OK type %d reason %d", actionType, reason);
    //         }
    //         // else
    //         // {
    //         //     LogWrite(MODULE_PREFIX, LOG_NOTICE, "socketSetAction FAILED");
    //         // }
    //         break;
    //     }
    // }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Callback on BUSRQ Acknowledged
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusSocketManager::busReqAckedStatic(void* pObject, BR_BUS_REQ_REASON reason, BR_RETURN_TYPE rslt)
{
    if (!pObject)
        return;
    ((BusSocketManager*)pObject)->busReqAcked(reason, rslt);
}

void BusSocketManager::busReqAcked(BR_BUS_REQ_REASON reason, BR_RETURN_TYPE rslt)
{
    // LogWrite(MODULE_PREFIX, LOG_DEBUG, "busReqAcked socketIdx %d rslt %d", socketIdx, rslt); 

    // Validity check
    // if ((socketIdx >= _busSocketCount) || (socketIdx >= MAX_BUS_SOCKETS))
    //     return;

    // Bus action has called back - inform all sockets
    for (uint32_t i = 0; i < _busSocketCount; i++)
    {
        // Check socket enabled
        if (!_busSockets[i].enabled)
            continue;

        // Inform all active sockets of the bus action completion
        if (_busSockets[i].busReqAckedCallback)
            _busSockets[i].busReqAckedCallback(_busSockets[i].pSourceObject, reason, rslt);
    }

    // // Clear this action for all sockets
    // for (uint32_t i = 0; i < _busSocketCount; i++)
    //     _busSockets[i].clearDown(actionType);
    
    // // Reset indicator of socket with action
    // _socketActionRequested = false;

    // Check for other actions
    // updateAfterSocketChange();
}

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // Clear all pending actions
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// void BusSocketManager::clearAllPending()
// {
//     _socketActionRequested = false;
//     for (uint32_t i = 0; i < _busSocketCount; i++)
//     {
//         _busSockets[i].clearPending();
//     }
//     _busControl.ctrl().requestClear();
// }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Callback when a bus access is taking place
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusSocketManager::busAccessCallbackStatic(void* pObject, uint32_t addr, uint32_t data, 
            uint32_t flags, uint32_t& curRetVal)
{
    if (!pObject)
        return;
    ((BusSocketManager*)pObject)->busAccessCallback(addr, data, flags, curRetVal);
}

void BusSocketManager::busAccessCallback(uint32_t addr, uint32_t data, 
            uint32_t flags, uint32_t& curRetVal)
{
    // Send this to all bus sockets
    for (uint32_t sockIdx = 0; sockIdx < _busSocketCount; sockIdx++)
    {
        if (_busSockets[sockIdx].enabled && _busSockets[sockIdx].busAccessCallback)
        {
            _busSockets[sockIdx].busAccessCallback(_busSockets[sockIdx].pSourceObject,
                             addr, data, flags, curRetVal);
            // if (ctrlBusVals & BR_CTRL_BUS_IORQ_MASK)
            //     LogWrite(MODULE_PREFIX, LOG_DEBUG, "%d IORQ %s from %04x %02x", sockIdx,
            //             (ctrlBusVals & BR_CTRL_BUS_RD_MASK) ? "RD" : ((ctrlBusVals & BR_CTRL_BUS_WR_MASK) ? "WR" : "??"),
            //             addr, 
            //             (ctrlBusVals & BR_CTRL_BUS_WR_MASK) ? dataBusVals : retVal);
        }
    }
}
