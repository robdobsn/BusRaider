// Bus Raider
// Rob Dobson 2018-2020

#include "BusSocketManager.h"
#include "BusControl.h"

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
    _socketWithAction = -1;
    _isSuspended = false;

    // Wait settings
    _waitOnIO = false;
    _waitOnMemory = false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Suspend socket activity
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusSocketManager::suspend(bool suspend, bool clearPendingActions)
{
    _busControl.bus().waitSuspend(suspend);
    _isSuspended = suspend;
    if (clearPendingActions)
        clearPending();
    updateAfterSocketChange();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Add a bus socket
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int BusSocketManager::add(bool enabled, BusAccessCBFnType* busAccessCallback,
            BusActionCBFnType* busActionCallback, 
            bool waitOnMemory, bool waitOnIO,
            bool resetPending, uint32_t resetDurationTStates,
            bool nmiPending, uint32_t nmiDurationTStates,
            bool irqPending, uint32_t irqDurationTStates,
            bool busMasterRequest, BR_BUS_ACTION_REASON busMasterReason,
            bool holdInWaitReq, void* pSourceObject)
{
    // Check if all used
    if (_busSocketCount >= MAX_BUS_SOCKETS)
        return -1;

    // Add socket info
    _busSockets[_busSocketCount].set(
            enabled, busAccessCallback, busActionCallback, 
            waitOnMemory, waitOnIO,
            resetPending, resetDurationTStates,
            nmiPending, nmiDurationTStates,
            irqPending, irqDurationTStates,
            busMasterRequest, busMasterReason,
            holdInWaitReq, pSourceObject);
    uint32_t tmpCount = _busSocketCount++;

    // Update wait state generation
    // LogWrite(MODULE_PREFIX, LOG_NOTICE, "busSocketAdd %d", tmpCount);
    updateAfterSocketChange();

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

// Maskable interrupt the host
void BusSocketManager::reqIRQ(uint32_t busSocket, int durationTStates)
{
    // Check validity
    // LogWrite(MODULE_PREFIX, LOG_DEBUG, "ReqIRQ sock %d us %d", busSocket, _busSockets[busSocket].busActionDurationUs);
    if (busSocket >= _busSocketCount)
        return;
    // Request NMI
    _busSockets[busSocket].irqDurationTStates = (durationTStates <= 0) ? BR_IRQ_PULSE_T_STATES : durationTStates;
    _busSockets[busSocket].irqPending = true;
    updateAfterSocketChange();
}

// Reset the host
void BusSocketManager::reqReset(uint32_t busSocket, int durationMs)
{
    // Check validity
    if (busSocket >= _busSocketCount)
        return;
    _busSockets[busSocket].resetDurationMs = (durationMs <= 0) ? BR_RESET_PULSE_MS : durationMs;
    _busSockets[busSocket].resetPending = true;
    // LogWrite(MODULE_PREFIX, LOG_DEBUG, "reqReset");
    updateAfterSocketChange();
}

// Non-maskable interrupt the host
void BusSocketManager::reqNMI(uint32_t busSocket, int durationTStates)
{
    // Check validity
    if (busSocket >= _busSocketCount)
        return;
    // Request NMI
    _busSockets[busSocket].nmiDurationTStates = (durationTStates <= 0) ? BR_NMI_PULSE_T_STATES : durationTStates;
    _busSockets[busSocket].nmiPending = true;
    updateAfterSocketChange();
}

// Bus request
void BusSocketManager::reqBus(uint32_t busSocket, BR_BUS_ACTION_REASON busMasterReason)
{
    // Check validity
    if (busSocket >= _busSocketCount)
    {
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "reqBus sock %d invalid count = %d", busSocket, _busSocketCount);
        return;
    }
    // LogWrite(MODULE_PREFIX, LOG_DEBUG, "reqBus reason %d", busMasterReason);
    _busSockets[busSocket].busMasterRequest = true;
    _busSockets[busSocket].busMasterReason = busMasterReason;
    updateAfterSocketChange();

    // Bus request can be handled immediately as the BUSRQ line is not part of the multiplexer (so is not affected by WAIT processing)

    // TODO 2020
    // busActionCheck();
    // busActionHandleStart();
    // LogWrite(MODULE_PREFIX, LOG_DEBUG, "reqBus sock %d enabled %d reason %d", busSocket, _busSockets[busSocket].enabled, busMasterReason);
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

    // See if any actions requested
    int busSocketWithAction = -1;
    for (uint32_t i = 0; i < _busSocketCount; i++)
    {
        if (_busSockets[i].enabled && (_busSockets[i].getType() != BR_BUS_ACTION_NONE))
        {
            busSocketWithAction = i;
            break;
        }
    }

    // Inform BusControl
    socketSetAction(memWait, ioWait, busSocketWithAction);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Set action from socket
// socketWithAction == -1 if no action pending
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusSocketManager::socketSetAction(bool memWait, bool ioWait, int socketWithAction)
{
    // Check for changes to wait system
    if ((_waitOnMemory != memWait) || (_waitOnMemory != ioWait))
    {
        // Store flags
        _waitOnMemory = memWait;
        _waitOnIO = ioWait;

        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "WAIT ENABLEMENT mreq %d iorq %d", memWait, ioWait);

        // Set raw
        _busControl.bus().waitConfigure(_waitOnMemory, _waitOnIO);

        // Debug
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "WAIT UPDATE mem %d io %d", _waitOnMemory, _waitOnIO);
    }

    // Check for changes to actions
    // LogWrite(MODULE_PREFIX, LOG_NOTICE, "socketSetAction curSockWithAction %d newSockIdx %d",
    //             _socketWithAction, socketWithAction);
    if (_socketWithAction != socketWithAction)
    {
        // Request action
        if (_busControl.ctrl().cycleReqAction(_busSockets[socketWithAction], 
                        _busSockets[socketWithAction].getAssertUs(_busControl.clock().getFreqInHz()),
                        cycleActionStaticCB, this, socketWithAction))
        {
            // Store socket with action
            _socketWithAction = socketWithAction;
            // LogWrite(MODULE_PREFIX, LOG_NOTICE, "socketSetAction OK type %d reason %d",
            //         _busSockets[socketWithAction].getType(),
            //         _busSockets[socketWithAction].busMasterReason);
        }
        else
        {
            LogWrite(MODULE_PREFIX, LOG_NOTICE, "socketSetAction FAILED");
        }
        
    }

#ifdef ISR_TEST
    // Setup edge triggering on falling edge of IORQ
    // Clear any current detected edges
    write32(ARM_GPIO_GPEDS0, (1 << BR_IORQ_BAR) | (1 << BR_MREQ_BAR));  
    // Set falling edge detect
    if (_waitOnIO)
    {
        write32(ARM_GPIO_GPFEN0, read32(ARM_GPIO_GPFEN0) | BR_IORQ_BAR_MASK);
        lowlev_enable_fiq();
    }
    else
    {
        write32(ARM_GPIO_GPFEN0, read32(ARM_GPIO_GPFEN0) & (~BR_IORQ_BAR_MASK));
        lowlev_disable_fiq();
    }
    // if (_waitOnMemory)
    //     write32(ARM_GPIO_GPFEN0, read32(ARM_GPIO_GPFEN0) | BR_WAIT_BAR_MASK);
    // else
    //     write32(ARM_GPIO_GPFEN0, read32(ARM_GPIO_GPFEN0) & (~BR_WAIT_BAR_MASK);
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Callback on action finished
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusSocketManager::cycleActionStaticCB(void* pObject, uint32_t slotIdx, BR_RETURN_TYPE rslt)
{
    if (!pObject)
        return;
    ((BusSocketManager*)pObject)->cycleActionCB(slotIdx, rslt);
}

void BusSocketManager::cycleActionCB(uint32_t slotIdx, BR_RETURN_TYPE rslt)
{
    // Get action type and reason for the active action
    if ((slotIdx >= _busSocketCount) || (slotIdx >= MAX_BUS_SOCKETS))
        return;
    BR_BUS_ACTION busActionType = _busSockets[slotIdx].getType();
    BR_BUS_ACTION_REASON reason = _busSockets[slotIdx].busMasterReason;
    if (busActionType != BR_BUS_ACTION_BUSRQ)
        reason = BR_BUS_ACTION_GENERAL;

    // Bus action has called back - inform all sockets
    for (uint32_t i = 0; i < _busSocketCount; i++)
    {
        // Check socket enabled
        if (!_busSockets[i].enabled)
            continue;

        // Inform all active sockets of the bus action completion
        if (_busSockets[i].busActionCallback)
            _busSockets[i].busActionCallback(_busSockets[i].pSourceObject, 
                        busActionType, reason);
    }

    // Clear this action for all sockets
    for (uint32_t i = 0; i < _busSocketCount; i++)
        _busSockets[i].clearDown(busActionType);
    
    // Reset indicator of socket with action
    _socketWithAction = -1;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Clear all pending actions
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusSocketManager::clearPending()
{
    _socketWithAction = -1;
    for (uint32_t i = 0; i < _busSocketCount; i++)
    {
        _busSockets[i].clearPending();
    }
    _busControl.ctrl().cycleClearAction();
}