// Bus Raider Hardware Manager
// Rob Dobson 2018

#include "HwManager.h"
#include "BusControl.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Variables
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Module name
static const char MODULE_PREFIX[] = "HwManager";

// HwManager* HwManager::_pThisInstance = NULL;

// Debug
#define DEBUG_COMMS_SOCKET

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

// Default hardware list - to use if no hardware specified
const char* HwManager::_pDefaultHardwareList = 
        "[{\"name\":\"RAMROM\",\"enable\":1,\"pageOut\":\"busPAGE\",\"bankHw\":\"LINEAR\",\"memSizeK\":1024}]";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

HwManager::HwManager(BusControl& busControl) :
    _busControl(busControl)
{
    // _pThisInstance = this;
    // Sockets
    // _busSocketId = -1;
    // _commsSocketId = -1;
    // _isEmulatingMemory = false;
    // _memoryPagingEnable = false;
    // _opcodeInjectEnable = false;
    // _mirrorMode = false;
    // _numHardware = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get code snippet to setup hardware
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t HwManager::getSnippetToSetupHw(uint32_t codeLocation, uint8_t* pCodeBuffer, uint32_t codeMaxlen)
{
    // Instructions to setup hardware
    static uint8_t hwSetupInstructions[] = 
    {
        0x3e, 0x80,                 // ld a, 0x80 - this sets the IO regs of a Z180 to be based at 0x80
        0xed, 0x39, 0x3f,           // out (c), a - Z180 IO Control Register (ICR)
        0x3e, 0x80,                 // ld a, 0x80 - this sets the clock divider to divide by 1
        0xed, 0x39, 0x9f,           // out (c), a - Z180 CPU Control Register (CCR) - note rebased
    };
    if (codeMaxlen >= sizeof(hwSetupInstructions))
    {
        memcopyfast(pCodeBuffer, hwSetupInstructions, sizeof(hwSetupInstructions));
        return sizeof(hwSetupInstructions);
    }
    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Init
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// void HwManager::init()
// {
//     // Connect to the bus socket
//     if (_busSocketId < 0)
//         _busSocketId = _busControl.busSocketAdd(
//             true,
//             HwManager::handleWaitInterruptStatic,
//             HwManager::busActionActiveStatic,
//             false,
//             false,
//             // Reset
//             false,
//             0,
//             // NMI
//             false,
//             0,
//             // IRQ
//             false,
//             0,
//             false,
//             BR_BUS_ACTION_GENERAL,
//             false,
//             this
//         );

//     // Connect to the comms socket
//     if (_commsSocketId < 0)
//     {
//         _commsSocketId = _commandHandler.commsSocketAdd(this, true, handleRxMsgStatic, NULL, NULL);
// #ifdef DEBUG_COMMS_SOCKET
//         LogWrite(MODULE_PREFIX, LOG_DEBUG, "init commsSocketId %d", _commsSocketId);
// #endif
//     }
//     // Add hardware - HwBase constructor adds to HwManager
//     // TODO 2020
//     //    new HwRAMROM(*this, _busControl);
// }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// void HwManager::service()
// {
//     #ifdef DEBUG_IO_ACCESS
//     // Handle debug 
//     char debugJson[500];
//     debugJson[0] = 0;
//     for (int i = 0; i < 20; i++)
//     {
//         if (_debugIOPortPosn.canGet())
//         {
//             int pos = _debugIOPortPosn.posToGet();
//             ee_sprintf(debugJson+strlen(debugJson), "%s %04x=%02x,",
//                 ((_debugIOPortBuf[pos].type == 0) ? "RD" : ((_debugIOPortBuf[pos].type == 1) ? "WR" : "INTACK")),
//                 _debugIOPortBuf[pos].port,
//                 _debugIOPortBuf[pos].val);
//             _debugIOPortPosn.hasGot();
//         }
//     }
//     if (strlen(debugJson) != 0)
//         CommandHandler::logDebugMessage(debugJson);
// #endif
// }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Manage Machine List
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// void HwManager::addHardwareElementStatic(HwBase* pHw)
// {
//     if (_pThisInstance->_numHardware >= MAX_HARDWARE)
//         return;
//     _pThisInstance->_pHw[_pThisInstance->_numHardware++] = pHw;
// }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Hardware config
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Set memory emulation mode
// void HwManager::setMemoryEmulationMode(bool val)
// {
//     // Debug
//     LogWrite(MODULE_PREFIX, LOG_DEBUG, "setMemoryEmulationMode %s", val ? "Y" : "N");

//     // Iterate hardware
//     for (int i = 0; i < _numHardware; i++)
//     {
//         if (_pHw[i] && _pHw[i]->isEnabled())
//             _pHw[i]->setMemoryEmulationMode(val);
//     }

//     // Set
//     _isEmulatingMemory = val;
// }

// // Page out RAM/ROM for opcode injection
// void HwManager::pageOutForInjection(bool pageOut)
// {
//     // LogWrite(MODULE_PREFIX, LOG_DEBUG, "pageOutForInjection %s numHw %d", pageOut ? "Y" : "N", _numHardware);

//     // Iterate hardware
//     for (int i = 0; i < _numHardware; i++)
//     {
//         if (_pHw[i] && _pHw[i]->isEnabled())
//             _pHw[i]->pageOutForInjection(pageOut);
//     }
// }

// // Set memory paging enable
// void HwManager::setMemoryPagingEnable(bool val)
// {
//     // Debug
//     // LogWrite(MODULE_PREFIX, LOG_DEBUG, "setMemoryPagingEnable %s", val ? "Y" : "N");

//     // Iterate hardware
//     for (int i = 0; i < _numHardware; i++)
//     {
//         if (_pHw[i] && _pHw[i]->isEnabled())
//             _pHw[i]->setMemoryPagingEnable(val);
//     }

//     // Set
//     _memoryPagingEnable = val;
// }

// // Set opcode inject enable
// void HwManager::setOpcodeInjectEnable(bool val)
// {
//     // Debug
//     LogWrite(MODULE_PREFIX, LOG_DEBUG, "setOpcodeInjectEnable %s", val ? "Y" : "N");

//     // Set
//     _opcodeInjectEnable = val;
// }

// // 
// void HwManager::setMirrorMode(bool val)
// {
//     // Iterate hardware
//     for (int i = 0; i < _numHardware; i++)
//     {
//         if (_pHw[i] && _pHw[i]->isEnabled())
//             _pHw[i]->setMirrorMode(val);
//     }

//     // Set
//     _mirrorMode = val;
// }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Access to memory / io blocks
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// uint32_t HwManager::getMaxAddress()
// {
//     // Iterate hardware
//     uint32_t maxAddress = STD_TARGET_MEMORY_LEN-1;
//     for (int i = 0; i < _numHardware; i++)
//     {
//         if (_pHw[i] && _pHw[i]->isEnabled())
//         {
//             uint32_t maxAddrForHw = _pHw[i]->getMaxAddress();
//             if (maxAddress < maxAddrForHw)
//                 maxAddress = maxAddrForHw; 
//         }
//     }
//     return maxAddress;
// }

// BR_RETURN_TYPE HwManager::blockWrite(uint32_t addr, const uint8_t* pBuf, uint32_t len, 
//                 bool busRqAndRelease, bool iorq, bool forceMirrorAccess)
// {
//     // LogWrite(MODULE_PREFIX, LOG_DEBUG, "blockWrite");

//     // Check if bus access is available
//     forceMirrorAccess = forceMirrorAccess || (!busAccessAvailable());

//     // Iterate hardware
//     BR_RETURN_TYPE retVal = BR_OK;
//     for (int i = 0; i < _numHardware; i++)
//     {
//         if (_pHw[i] && _pHw[i]->isEnabled())
//         {
//             BR_RETURN_TYPE newRet = _pHw[i]->blockWrite(addr, pBuf, len, busRqAndRelease, iorq, forceMirrorAccess);
//             retVal = (newRet == BR_OK || newRet == BR_NOT_HANDLED) ? retVal : newRet;
//         }
//     }
//     return retVal;
// }

// BR_RETURN_TYPE HwManager::blockRead(uint32_t addr, uint8_t* pBuf, uint32_t len, bool busRqAndRelease, bool iorq, bool forceMirrorAccess)
// {
//     // Check if bus access is available
//     forceMirrorAccess = forceMirrorAccess || (!busAccessAvailable());

//     // LogWrite(MODULE_PREFIX, LOG_DEBUG, "blockRead forceMirrorAccess %d", forceMirrorAccess);

//     // Iterate hardware
//     BR_RETURN_TYPE retVal = BR_OK;
//     for (int i = 0; i < _numHardware; i++)
//     {
//         if (_pHw[i] && _pHw[i]->isEnabled())
//         {
//             BR_RETURN_TYPE newRet = _pHw[i]->blockRead(addr, pBuf, len, busRqAndRelease, iorq, forceMirrorAccess);
//             retVal = (newRet == BR_OK || newRet == BR_NOT_HANDLED) ? retVal : newRet;
//             // LogWrite(MODULE_PREFIX, LOG_DEBUG, "blkrd %d %d", i, retVal);
//         }
//     }
//     return retVal;
// }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Mirror memory
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// // Get mirror memory for address
// uint8_t* HwManager::getMirrorMemForAddr(uint32_t addr)
// {
//     // Iterate hardware
//     uint8_t* pMirrorMemPtr = NULL;
//     for (int i = 0; i < _numHardware; i++)
//     {
//         if (_pHw[i] && _pHw[i]->isEnabled())
//         {
//             pMirrorMemPtr = _pHw[i]->getMirrorMemForAddr(addr);
//             if (pMirrorMemPtr != NULL)
//                 break;
//         }
//     }
//     return pMirrorMemPtr;
// }

// void HwManager::mirrorClone()
// {
//     // Iterate hardware
//     for (int i = 0; i < _numHardware; i++)
//     {
//         if (_pHw[i] && _pHw[i]->isEnabled())
//             _pHw[i]->mirrorClone();
//     }
// }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Tracer Interface
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// The Tracer interface allows a whole extra version of the hardware to be accessed for purposes
// of tracing of the hardware - possibly against an emulated processor (this is not the same as memory emulation)

// void HwManager::tracerClone()
// {
//     // Iterate hardware
//     for (int i = 0; i < _numHardware; i++)
//     {
//         if (_pHw[i] && _pHw[i]->isEnabled())
//             _pHw[i]->tracerClone();
//     }
// }

// void HwManager::tracerHandleAccess(uint32_t addr, uint32_t data, 
//         uint32_t flags, uint32_t& retVal)
// {
//     // Iterate hardware
//     for (int i = 0; i < _numHardware; i++)
//     {
//         if (_pHw[i] && _pHw[i]->isEnabled())
//         {
//             _pHw[i]->tracerHandleAccess(addr, data, flags, retVal);
//         }
//     }

// }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Received message handler
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// bool HwManager::handleRxMsgStatic(void* pObject, const char* pCmdJson, 
//                 const uint8_t* pParams, unsigned paramsLen,
//                 char* pRespJson, unsigned maxRespLen)
// {
//     if (!pObject)
//         return false;
//     return ((HwManager*)pObject)->handleRxMsg(pCmdJson, pParams, paramsLen, pRespJson, maxRespLen);
// }

// bool HwManager::handleRxMsg(const char* pCmdJson, const uint8_t* pParams, unsigned paramsLen,
//                 char* pRespJson, unsigned maxRespLen)
// {
//     // Get the command string from JSON
//     static const int MAX_CMD_NAME_STR = 50;
//     char cmdName[MAX_CMD_NAME_STR+1];
//     if (!jsonGetValueForKey("cmdName", pCmdJson, cmdName, MAX_CMD_NAME_STR))
//         return false;

//     // Check for memory/IO read
//     if (strcasecmp(cmdName, "hwEnable") == 0)
//     {
//         // Get clock rate required
//         static const int MAX_CMD_PARAM_STR = 100;
//         char hwName[MAX_CMD_PARAM_STR+1];
//         if (!jsonGetValueForKey("hwName", pCmdJson, hwName, MAX_CMD_PARAM_STR))
//             return false;

//         // Get enable param
//         char enableVal[MAX_CMD_PARAM_STR+1];
//         if (!jsonGetValueForKey("enable", pCmdJson, enableVal, MAX_CMD_PARAM_STR))
//             return false;
//         bool enable = strtoul(enableVal, NULL, 10) != 0;

//         // Enable hardware and configure
//         bool foundOk = enableHw(hwName, enable);
//         configureHw(hwName, pCmdJson);

//         // Ok
//         snprintf(pRespJson, maxRespLen, "\"err\":\"%s\"", foundOk ? "ok" : "notFound");
//         return true;
//     }
//     else if (strcasecmp(cmdName, "hwList") == 0)
//     {
//         // Response string
//         strlcpy(pRespJson, "\"err\":\"ok\",\"hwList\":[", maxRespLen);

//         // Get info on each hw item
//         bool commaNeeded = false;
//         for (int i = 0; i < _numHardware; i++)
//         {
//             if (!_pHw[i])
//                 continue;
//             static const int MAX_HW_INFO_LEN = 1000;
//             char hwInfoStr[MAX_HW_INFO_LEN];
//             snprintf(hwInfoStr, sizeof(hwInfoStr), "{\"name\":\"%s\",\"enabled\":%d}", _pHw[i]->name(), _pHw[i]->isEnabled());
//             if (commaNeeded)
//                 strlcat(pRespJson, ",", maxRespLen);
//             strlcat(pRespJson, hwInfoStr, maxRespLen);
//             commaNeeded = true;
//         }

//         // Close string
//         strlcat(pRespJson, "]", maxRespLen);
//         return true;
//     }
//     return false;
// }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Callbacks/Hooks
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// void HwManager::busActionActiveStatic(void* pObject, BR_BUS_ACTION actionType, 
//         BR_BUS_ACTION_REASON reason, BR_RETURN_TYPE rslt)
// {
//     if (!pObject)
//         return;
//     ((HwManager*)pObject)->busActionActive(actionType, reason, rslt);
// }

// void HwManager::busActionActive(BR_BUS_ACTION actionType, BR_BUS_ACTION_REASON reason, 
//         BR_RETURN_TYPE rslt)
// {
//     // LogWrite(MODULE_PREFIX, LOG_DEBUG, "busActionActive %d numHw %d en %s reason %d rslt %d", 
//     //          actionType, _numHardware, rslt,
//     //         (_numHardware > 0) ? (_pHw[0]->isEnabled() ? "Y" : "N") : "X", reason);
 
//     // Iterate hardware
//     for (int i = 0; i < _numHardware; i++)
//         if (_pHw[i] && _pHw[i]->isEnabled())
//             _pHw[i]->handleBusActionActive(actionType, reason, rslt);
// }

// void HwManager::handleWaitInterruptStatic(void* pObject, uint32_t addr, uint32_t data, 
//         uint32_t flags, uint32_t& retVal)
// {
//     if (!pObject)
//         return;
//     ((HwManager*)pObject)->handleWaitInterrupt(addr, data, flags, retVal);
// }

// void HwManager::handleWaitInterrupt(uint32_t addr, uint32_t data, 
//         uint32_t flags, uint32_t& retVal)
// {
//     // Iterate hardware
//     for (int i = 0; i < _numHardware; i++)
//         if (_pHw[i] && _pHw[i]->isEnabled())
//             _pHw[i]->handleMemOrIOReq(addr, data, flags, retVal);

//     // Debug logging - check for IO and RD or WRITE
//     if (flags & BR_CTRL_BUS_IORQ_MASK)
//     {
// #ifdef DEBUG_IO_ACCESS
//         // Decode port
//         if (_debugIOPortPosn.canPut())
//         {
//             int pos = _debugIOPortPosn.posToPut();
//             _debugIOPortBuf[pos].port = addr;
//             _debugIOPortBuf[pos].type = (flags & BR_CTRL_BUS_RD_MASK) ? 0 : ((flags & BR_CTRL_BUS_WR_MASK) ? 1 : 2);
//             _debugIOPortBuf[pos].val = data;
//             _debugIOPortPosn.hasPut();
//         }
// #endif
//     }
// }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Hardware enable/disable
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Enable
// bool HwManager::enableHw(const char* hwName, bool enable)
// {
//     // Find hardware and enable
//     for (int i = 0; i < _numHardware; i++)
//     {
//         if (!_pHw[i])
//             continue;
//         if (strcasecmp(_pHw[i]->name(), hwName) == 0)
//         {
//             _pHw[i]->enable(enable);
//             return true;
//         }
//     }
//     return false;
// }

// // Disable
// void HwManager::disableAll()
// {
//     for (int i = 0; i < _numHardware; i++)
//     {
//         if (!_pHw[i])
//             continue;
//         _pHw[i]->enable(false);
//     }
// }

// // Configure
// void HwManager::configureHw(const char* hwName, const char* hwDefJson)
// {
//     // Find hardware and enable
//     for (int i = 0; i < _numHardware; i++)
//     {
//         if (!_pHw[i])
//             continue;
//         if (strcasecmp(_pHw[i]->name(), hwName) == 0)
//         {
//             _pHw[i]->configure(hwDefJson);
//             break;
//         }
//     }
// }

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // Setup from JSON
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// // Setup from Json
// void HwManager::setupFromJson(const char* jsonKey, const char* hwJson)
// {
//     // Get hardware content list if present
//     static const int MAX_JSON_LIST_STR = 5000;
//     char hwList[MAX_JSON_LIST_STR];
//     bool hwListValid = jsonGetValueForKey(jsonKey, hwJson, hwList, MAX_JSON_LIST_STR);

//     // Check if hardware list is present, if not set default
//     const char* pJsonHwListToUse = hwList;
//     if ((!hwListValid) || (strlen(hwList) == 0) || (strcasecmp(hwList, "[]") == 0))
//         pJsonHwListToUse = _pDefaultHardwareList;

//     int hwListLen = jsonGetArrayLen(pJsonHwListToUse);
//     LogWrite(MODULE_PREFIX, LOG_DEBUG, "Hardware list len %d", hwListLen);

//     // Iterate through hardware
//     for (int hwIdx = 0; hwIdx < hwListLen; hwIdx++)
//     {
//         // Get hardware def
//         static const int HW_DEF_MAXLEN = 1000;
//         char hwDefJson[HW_DEF_MAXLEN];
//         bool valid = jsonGetArrayElem(hwIdx, pJsonHwListToUse, hwDefJson, HW_DEF_MAXLEN);
//         LogWrite(MODULE_PREFIX, LOG_DEBUG, "Hardware item valid %d elem %s", valid, hwDefJson);
//         if (!valid)
//             continue;

//         // Get name
//         static const int HW_NAME_MAXLEN = 100;
//         char hwName[HW_NAME_MAXLEN];
//         if (!jsonGetValueForKey("name", hwDefJson, hwName, HW_NAME_MAXLEN))
//             continue;
//         LogWrite(MODULE_PREFIX, LOG_DEBUG, "Hardware %d Name %s", hwIdx, hwName);

//         // Get enable
//         static const int HW_ENABLE_MAXLEN = 100;
//         char hwEnable[HW_ENABLE_MAXLEN];
//         strcpy(hwEnable, "1");
//         if (!jsonGetValueForKey("enable", hwDefJson, hwEnable, HW_ENABLE_MAXLEN))
//             strcpy(hwEnable, "1");
//         bool en = false;
//         if (strcasecmp(hwEnable, "y") == 0)
//             en = true;
//         else
//             en = strtoul(hwEnable, NULL, 10) != 0;
        
//         // Enable/Disable
//         enableHw(hwName, en);

//         LogWrite(MODULE_PREFIX, LOG_DEBUG, "Hardware %d Name %s Enable %d", hwIdx, hwName, en);

//         // Configure hardware
//         configureHw(hwName, hwDefJson);
//     }    
// }

// // Check if bus can be accessed directly
// bool HwManager::busAccessAvailable()
// {
//     return !_busControl.waitIsHeld() && !isEmulatingMemory() && 
//                     // !_busControl.busSocketIsEnabled(_busSocketId) &&
//                     !_busControl.isTrackingActive();
// }
