// Bus Raider
// Rob Dobson 2018-2020

#include "BusSocketManager.h"
#include "BusControl.h"
#include "DebugHelper.h"

// #define DEBUG_BUS_REQ_CALLBACK
// #define DEBUG_BUS_REQ_CALLBACK_IORQ
// #define DEBUG_WAIT_ENABLES

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
    _isSuspended = false;
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
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Add a bus socket
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int BusSocketManager::addSocket(bool enabled, BusAccessCBFnType* busAccessCallback,
            BusReqAckedCBFnType* busReqAckedCallback, 
            void* pSourceObject)
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
    uint32_t tmpCount = _busSocketCount++;
    return tmpCount;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Enablement
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusSocketManager::enable(uint32_t busSocket, bool enable)
{
    // Check validity
    if (busSocket >= _busSocketCount)
        return;

    // Enable/disable
    _busSockets[busSocket].enabled = enable;
}

bool BusSocketManager::isEnabled(uint32_t busSocket)
{
    // Check validity
    if (busSocket >= _busSocketCount)
        return false;
    return _busSockets[busSocket].enabled;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Socket Functions
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Reset the host
void BusSocketManager::reqReset(uint32_t busSocket)
{
    _busControl.ctrl().targetReset();
}

// Maskable interrupt the host
void BusSocketManager::reqINT(uint32_t busSocket)
{
    _busControl.ctrl().targetINT();
}

// Non-maskable interrupt the host
void BusSocketManager::reqNMI(uint32_t busSocket)
{
    _busControl.ctrl().targetNMI();
}

// Bus request
void BusSocketManager::reqBus(uint32_t busSocketIdx, BR_BUS_REQ_REASON busReqReason)
{
    _busControl.ctrl().reqBus(busReqReason, busSocketIdx);
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
#ifdef DEBUG_BUS_REQ_CALLBACK
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "busReqAcked reason %d rslt %d", reason, rslt); 
#endif

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
}

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

#ifdef DEBUG_BUS_REQ_CALLBACK_IORQ
            if (flags & BR_CTRL_BUS_IORQ_MASK)
                LogWrite(MODULE_PREFIX, LOG_DEBUG, "busAccessCallback IORQ %s from %04x %02x",
                        (flags & BR_CTRL_BUS_RD_MASK) ? "RD" : ((flags & BR_CTRL_BUS_WR_MASK) ? "WR" : "??"),
                        addr, 
                        (flags & BR_CTRL_BUS_WR_MASK) ? data : curRetVal);
#endif
        }
    }
}
