// Bus Raider Hardware Manager
// Rob Dobson 2018

#include "HwManager.h"
#include <string.h>
#include "../TargetBus/TargetState.h"
#include "../TargetBus/BusAccess.h"
#include "../System/rdutils.h"
#include "../System/ee_sprintf.h"
#include "../System/piwiring.h"

// Module name
static const char FromHwManager[] = "HwManager";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Statics
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Debug IO Port access
#ifdef DEBUG_IO_ACCESS
DebugIOPortAccess HwManager::_debugIOPortBuf[];
RingBufferPosn HwManager::_debugIOPortPosn(DEBUG_MAX_IO_PORT_ACCESSES);
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Hardware
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Hardware list
HwBase* HwManager::_pHw[HwManager::MAX_HARDWARE];
int HwManager::_numHardware = 0;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Init
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void HwManager::init()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void HwManager::service()
{
    #ifdef DEBUG_IO_ACCESS
    // Handle debug 
    char debugJson[500];
    debugJson[0] = 0;
    for (int i = 0; i < 20; i++)
    {
        if (_debugIOPortPosn.canGet())
        {
            int pos = _debugIOPortPosn.posToGet();
            ee_sprintf(debugJson+strlen(debugJson), "%s %04x=%02x,",
                ((_debugIOPortBuf[pos].type == 0) ? "RD" : ((_debugIOPortBuf[pos].type == 1) ? "WR" : "INTACK")),
                _debugIOPortBuf[pos].port,
                _debugIOPortBuf[pos].val);
            _debugIOPortPosn.hasGot();
        }
    }
    if (strlen(debugJson) != 0)
        McManager::logDebugMessage(debugJson);
#endif

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Manage Machine List
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void HwManager::add(HwBase* pHw)
{
    if (_numHardware >= MAX_HARDWARE)
        return;
    _pHw[_numHardware++] = pHw;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Hardware activity
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Reset
void HwManager::reset()
{
    // Iterate hardware
    for (int i = 0; i < _numHardware; i++)
    {
        _pHw[i]->reset();
    }
}

// Page out RAM/ROM due to emulation
void HwManager::pageOutForEmulation(bool pageOut)
{
    // Debug
    LogWrite(FromHwManager, LOG_DEBUG, "pageOutForEmulation %s", pageOut ? "Y" : "N");

    // Iterate hardware
    for (int i = 0; i < _numHardware; i++)
    {
        if (_pHw[i])
            _pHw[i]->pageOutForEmulation(pageOut);
    }
}

// Page out RAM/ROM for opcode injection
void HwManager::pageOutForInjection(bool pageOut)
{
    // Iterate hardware
    for (int i = 0; i < _numHardware; i++)
    {
        if (_pHw[i])
            _pHw[i]->pageOutForInjection(pageOut);
    }
}

// Check if paging requires bus access
bool HwManager::pagingRequiresBusAccess()
{
    // Iterate hardware
    for (int i = 0; i < _numHardware; i++)
    {
        if (pagingRequiresBusAccess())
            return true;
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Interrupt handler for WAIT
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Interrupt handler for MREQ/IORQ WAIT
uint32_t HwManager::handleWaitInterrupt(uint32_t addr, uint32_t data, uint32_t flags, uint32_t retVal)
{
    // Iterate hardware
    for (int i = 0; i < _numHardware; i++)
        retVal = _pHw[i]->handleMemOrIOReq(addr, data, flags, retVal);

    // Debug logging - check for IO and RD or WRITE
    if (flags & BR_CTRL_BUS_IORQ_MASK)
    {
#ifdef DEBUG_IO_ACCESS
        // Decode port
        if (_debugIOPortPosn.canPut())
        {
            int pos = _debugIOPortPosn.posToPut();
            _debugIOPortBuf[pos].port = addr;
            _debugIOPortBuf[pos].type = (flags & BR_CTRL_BUS_RD_MASK) ? 0 : ((flags & BR_CTRL_BUS_WR_MASK) ? 1 : 2);
            _debugIOPortBuf[pos].val = data;
            _debugIOPortPosn.hasPut();
        }
#endif
    }


    return retVal;
}

