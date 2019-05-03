// Bus Raider Hardware Manager
// Rob Dobson 2018

#include "HwManager.h"
#include <string.h>
#include "../TargetBus/TargetState.h"
#include "../TargetBus/BusAccess.h"
#include "../TargetBus/TargetTracker.h"
#include "../System/rdutils.h"
#include "../System/ee_sprintf.h"
#include "../System/piwiring.h"
#include "Hw64KRAM.h"
#include "Hw512KRamRom.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Variables
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Module name
static const char FromHwManager[] = "HwManager";

// Sockets
int HwManager::_busSocketId = -1;
BusSocketInfo HwManager::_busSocketInfo = 
{
    true,
    HwManager::handleWaitInterruptStatic,
    HwManager::busActionCompleteStatic,
    false,
    false,
    BR_BUS_ACTION_NONE,
    1,
    false,
    BR_BUS_ACTION_DISPLAY,
    false
};

int HwManager::_commsSocketId = -1;
// Main comms socket - to wire up command handler
CommsSocketInfo HwManager::_commsSocketInfo =
{
    true,
    HwManager::handleRxMsg,
    NULL,
    NULL
};

// Memory emulation flag
bool HwManager::_memoryEmulationMode = false;

// Memory paging mode
bool HwManager::_memoryPagingEnable = false;

// Opcode injection mode
bool HwManager::_opcodeInjectEnable = false;

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

// Default hardware list - to use if no hardware specified
const char* HwManager::_pDefaultHardwareList = "[{\"name\":\"64KRAM\",\"enable\":1}]";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Init
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void HwManager::init()
{
    // Connect to the bus socket
    if (_busSocketId < 0)
        _busSocketId = BusAccess::busSocketAdd(_busSocketInfo);

    // Connect to the comms socket
    if (_commsSocketId < 0)
        _commsSocketId = CommandHandler::commsSocketAdd(_commsSocketInfo);

    // Add hardware - HwBase constructor adds to HwManager
    new Hw64KRam();
    new Hw512KRamRom();
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
// Hardware config
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Set memory emulation mode
void HwManager::setMemoryEmulationMode(bool val)
{
    // Debug
    LogWrite(FromHwManager, LOG_DEBUG, "setMemoryEmulationMode %s", val ? "Y" : "N");

    // Iterate hardware
    for (int i = 0; i < _numHardware; i++)
    {
        if (_pHw[i] && _pHw[i]->isEnabled())
            _pHw[i]->setMemoryEmulationMode(val);
    }

    // Set
    _memoryEmulationMode = val;
}

// Page out RAM/ROM for opcode injection
void HwManager::pageOutForInjection(bool pageOut)
{
    // LogWrite(FromHwManager, LOG_DEBUG, "pageOutForInjection %s numHw %d", pageOut ? "Y" : "N", _numHardware);

    // Iterate hardware
    for (int i = 0; i < _numHardware; i++)
    {
        if (_pHw[i] && _pHw[i]->isEnabled())
            _pHw[i]->pageOutForInjection(pageOut);
    }
}

// Set memory paging enable
void HwManager::setMemoryPagingEnable(bool val)
{
    // Debug
    // LogWrite(FromHwManager, LOG_DEBUG, "setMemoryPagingEnable %s", val ? "Y" : "N");

    // Iterate hardware
    for (int i = 0; i < _numHardware; i++)
    {
        if (_pHw[i] && _pHw[i]->isEnabled())
            _pHw[i]->setMemoryPagingEnable(val);
    }

    // Set
    _memoryPagingEnable = val;
}

// Set opcode inject enable
void HwManager::setOpcodeInjectEnable(bool val)
{
    // Debug
    LogWrite(FromHwManager, LOG_DEBUG, "setOpcodeInjectEnable %s", val ? "Y" : "N");

    // Set
    _opcodeInjectEnable = val;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Access to memory / io blocks
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BR_RETURN_TYPE HwManager::blockWrite(uint32_t addr, const uint8_t* pBuf, uint32_t len, bool busRqAndRelease, bool iorq, bool forceMirrorAccess)
{
    BR_RETURN_TYPE retVal = BR_OK;
    // Check if bus access is available
    if (TargetTracker::busAccessAvailable() && !forceMirrorAccess)
        return BusAccess::blockWrite(addr, pBuf, len, busRqAndRelease, iorq);

    // Iterate hardware
    for (int i = 0; i < _numHardware; i++)
    {
        if (_pHw[i] && _pHw[i]->isEnabled())
            _pHw[i]->blockWrite(addr, pBuf, len, busRqAndRelease, iorq);
    }
    return retVal;
}

BR_RETURN_TYPE HwManager::blockRead(uint32_t addr, uint8_t* pBuf, uint32_t len, bool busRqAndRelease, bool iorq, bool forceMirrorAccess)
{
    BR_RETURN_TYPE retVal = BR_OK;
    // Check if bus access is available
    if (TargetTracker::busAccessAvailable() && !forceMirrorAccess)
        return BusAccess::blockRead(addr, pBuf, len, busRqAndRelease, iorq);

    // Iterate hardware
    for (int i = 0; i < _numHardware; i++)
    {
        if (_pHw[i] && _pHw[i]->isEnabled())
            _pHw[i]->blockRead(addr, pBuf, len, busRqAndRelease, iorq);
    }
    return retVal;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Validator Interface
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// The Validator interface is allows a whole extra version of the hardware to be accessed for purposes
// of validation of the hardware against an emulated processor (this is not the same as memory emulation)

void HwManager::validatorClone()
{
    // Iterate hardware
    for (int i = 0; i < _numHardware; i++)
    {
        if (_pHw[i] && _pHw[i]->isEnabled())
            _pHw[i]->validatorClone();
    }
}

void HwManager::validatorHandleAccess(uint32_t addr, uint32_t data, 
        uint32_t flags, uint32_t& retVal)
{
    // Iterate hardware
    for (int i = 0; i < _numHardware; i++)
    {
        if (_pHw[i] && _pHw[i]->isEnabled())
        {
            _pHw[i]->validatorHandleAccess(addr, data, flags, retVal);
        }
    }

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Received message handler
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool HwManager::handleRxMsg(const char* pCmdJson, [[maybe_unused]]const uint8_t* pParams, [[maybe_unused]]int paramsLen,
                [[maybe_unused]]char* pRespJson, [[maybe_unused]]int maxRespLen)
{
    // Get the command string from JSON
    static const int MAX_CMD_NAME_STR = 50;
    char cmdName[MAX_CMD_NAME_STR+1];
    if (!jsonGetValueForKey("cmdName", pCmdJson, cmdName, MAX_CMD_NAME_STR))
        return false;

    // Check for memory/IO read
    if (strcasecmp(cmdName, "hwEnable") == 0)
    {
        // Get clock rate required
        static const int MAX_CMD_PARAM_STR = 100;
        char hwName[MAX_CMD_PARAM_STR+1];
        if (!jsonGetValueForKey("hwName", pCmdJson, hwName, MAX_CMD_PARAM_STR))
            return false;

        // Get enable param
        char enableVal[MAX_CMD_PARAM_STR+1];
        if (!jsonGetValueForKey("enable", pCmdJson, enableVal, MAX_CMD_PARAM_STR))
            return false;
        bool enable = strtoul(enableVal, NULL, 10) != 0;

        // Enable hardware
        bool foundOk = enableHw(hwName, enable);

        // Ok
        ee_sprintf(pRespJson, "\"err\":\"%s\"", foundOk ? "ok" : "notFound");
        return true;
    }
    else if (strcasecmp(cmdName, "hwList") == 0)
    {
        // Response string
        strlcpy(pRespJson, "\"err\":\"ok\",\"hwList\":[", maxRespLen);

        // Get info on each hw item
        bool commaNeeded = false;
        for (int i = 0; i < _numHardware; i++)
        {
            if (!_pHw[i])
                continue;
            static const int MAX_HW_INFO_LEN = 200;
            char hwInfoStr[MAX_HW_INFO_LEN];
            ee_sprintf(hwInfoStr, "{\"name\":\"%s\",\"enabled\":%d}", _pHw[i]->name(), _pHw[i]->isEnabled());
            if (commaNeeded)
                strlcat(pRespJson, ",", maxRespLen);
            strlcat(pRespJson, hwInfoStr, maxRespLen);
            commaNeeded = true;
        }

        // Close string
        strlcat(pRespJson, "]", maxRespLen);
        return true;
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Callbacks/Hooks
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void HwManager::busActionCompleteStatic(BR_BUS_ACTION actionType, BR_BUS_ACTION_REASON reason)
{
    // LogWrite(FromHwManager, LOG_DEBUG, "busActionComplete %d numHw %d en %s reason %d", actionType, _numHardware,
    //         (_numHardware > 0) ? (_pHw[0]->isEnabled() ? "Y" : "N") : "X", reason);
 
    // Iterate hardware
    for (int i = 0; i < _numHardware; i++)
        if (_pHw[i] && _pHw[i]->isEnabled())
            _pHw[i]->handleBusActionComplete(actionType, reason);
}

void HwManager::handleWaitInterruptStatic(uint32_t addr, uint32_t data, 
        uint32_t flags, uint32_t& retVal)
{
    // Iterate hardware
    for (int i = 0; i < _numHardware; i++)
        if (_pHw[i] && _pHw[i]->isEnabled())
            _pHw[i]->handleMemOrIOReq(addr, data, flags, retVal);

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
}

// Enable
bool HwManager::enableHw(const char* hwName, bool enable)
{
    // Find hardware and enable
    for (int i = 0; i < _numHardware; i++)
    {
        if (!_pHw[i])
            continue;
        if (strcasecmp(_pHw[i]->name(), hwName) == 0)
        {
            _pHw[i]->enable(enable);
            return true;
        }
    }
    return false;
}

// Disable
void HwManager::disableAll()
{

}

// Setup from Json
void HwManager::setupFromJson(const char* jsonKey, const char* hwJson)
{
    // Get hardware content list if present
    static const int MAX_JSON_LIST_STR = 5000;
    char hwList[MAX_JSON_LIST_STR];
    bool hwListValid = jsonGetValueForKey(jsonKey, hwJson, hwList, MAX_JSON_LIST_STR);

    // Check if hardware list is present, if not set default
    const char* pJsonHwListToUse = hwList;
    if ((!hwListValid) || (strlen(hwList) == 0) || (strcasecmp(hwList, "[]") == 0))
        pJsonHwListToUse = _pDefaultHardwareList;

    int hwListLen = jsonGetArrayLen(pJsonHwListToUse);
    LogWrite(FromHwManager, LOG_DEBUG, "Hardware list len %d", hwListLen);

    // Iterate through hardware
    for (int hwIdx = 0; hwIdx < hwListLen; hwIdx++)
    {
        // Get hardware def
        static const int HW_DEF_MAXLEN = 1000;
        char hwDefJson[HW_DEF_MAXLEN];
        bool valid = jsonGetArrayElem(hwIdx, pJsonHwListToUse, hwDefJson, HW_DEF_MAXLEN);
        // LogWrite(FromHwManager, LOG_DEBUG, "Hardware item valid %d elem %s", valid, hwDefJson);
        if (!valid)
            continue;

        // Get name
        static const int HW_NAME_MAXLEN = 100;
        char hwName[HW_NAME_MAXLEN];
        if (!jsonGetValueForKey("name", hwDefJson, hwName, HW_NAME_MAXLEN))
            continue;
        LogWrite(FromHwManager, LOG_DEBUG, "Hardware %d Name %s", hwIdx, hwName);

        // Get enable
        static const int HW_ENABLE_MAXLEN = 100;
        char hwEnable[HW_ENABLE_MAXLEN];
        strcpy(hwEnable, "1");
        if (!jsonGetValueForKey("enable", hwDefJson, hwEnable, HW_ENABLE_MAXLEN))
            strcpy(hwEnable, "1");
        bool en = false;
        if (strcasecmp(hwEnable, "y") == 0)
            en = true;
        else
            en = strtol(hwEnable, NULL, 10) != 0;
        
        // Enable/Disable
        enableHw(hwName, en);

        LogWrite(FromHwManager, LOG_DEBUG, "Hardware %d Name %s Enable %d", hwIdx, hwName, en);

        // Configure each piece of hardware with this information
        // only the named one will actually use the configuration
        for (int i = 0; i < _numHardware; i++)
        {
            if (!_pHw[i])
                continue;
            _pHw[i]->configure(hwName, hwDefJson);
        }
    }    

}
