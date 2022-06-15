// Bus Raider
// Rob Dobson 2019

#include "StepTracer.h"
#include "PiWiring.h"
#include "lowlib.h"
#include "string.h"
#include "logging.h"
#include "rdutils.h"
#include "libz80/z80.h"
#include "BusControl.h"
#include "MemoryInterface.h"

// Uncomment the following line to use SPI0 CE0 of the Pi as a debug pin
// #define USE_PI_SPI0_CE0_AS_DEBUG_PIN 1

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Variables
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Module name
static const char MODULE_PREFIX[] = "StepTracer";

// Step tracer
StepTracerCycle StepTracer::_stepCycles[MAX_STEP_CYCLES_FOR_INSTR];
int StepTracer::_stepCycleCount = 0;
int StepTracer::_stepCyclePos = 0;

// This instance
StepTracer* StepTracer::_pThisInstance = NULL;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Constructor
StepTracer::StepTracer(CommandHandler& commandHandler, BusControl& busControl) :
        _commandHandler(commandHandler), _busControl(busControl),
        _exceptionsPosn(NUM_DEBUG_VALS), _tracesPosn(NUM_TRACE_VALS)
{
    // Vars
    _isActive = false;
    _stepCycleCount = 0;
    _stepCyclePos = 0;
    _pThisInstance = this;
    _logging = false;
    _recordAll = false;
    _compareToEmulated = false;
    _primeFromMemPending = false;
    _serviceCount = 0;
    _recordIsHoldingTarget = false;
    _busSocketId = -1;
    _commsSocketId = -1;

    // Tracer memory as required
#ifdef STEP_VAL_WITHOUT_HW_MANAGER
    _tracerMemoryLen = 0;
    _pTracerMemory = NULL;    
#endif

    // Set Z80 CPU callbacks
    _cpu_z80.ioRead = io_read;
	_cpu_z80.ioWrite = io_write;
	_cpu_z80.memRead = mem_read;
	_cpu_z80.memWrite = mem_write;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Utils
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void StepTracer::init()
{
    // Allocate tracer memory as required
#ifdef STEP_VAL_WITHOUT_HW_MANAGER
    _tracerMemoryLen = STD_TARGET_MEMORY_LEN;
    _pTracerMemory = new uint8_t[_tracerMemoryLen];
#endif

    // Connect to the bus socket
    if (_busSocketId < 0)
        _busSocketId = _busControl.socketAdd( 
            true,
            StepTracer::handleWaitInterruptStatic,
            StepTracer::busReqAckedStatic,
            this);

    // Connect to the comms socket
    if (_commsSocketId < 0)
    {
        _commsSocketId = _commandHandler.commsSocketAdd(
            this, 
            true, 
            StepTracer::handleRxMsgStatic, 
            NULL, 
            NULL);
#ifdef DEBUG_COMMS_SOCKET
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "init commsSocketId %d", _commsSocketId);
#endif
    }

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle CommandInterface message
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool StepTracer::handleRxMsgStatic(void* pObject, const char* pCmdJson, 
                    const uint8_t* pParams, unsigned paramsLen,
                    char* pRespJson, unsigned maxRespLen)
{
    if (!pObject)
        return false;
    return ((StepTracer*)pObject)->handleRxMsg(pCmdJson, pParams, paramsLen, 
                pRespJson, maxRespLen);
}

bool StepTracer::handleRxMsg(const char* pCmdJson, 
                    const uint8_t* pParams, unsigned paramsLen,
                    char* pRespJson, unsigned maxRespLen)
{
    // Get the command string from JSON
    #define MAX_CMD_NAME_STR 200
    char cmdName[MAX_CMD_NAME_STR+1];
    if (!jsonGetValueForKey("cmdName", pCmdJson, cmdName, MAX_CMD_NAME_STR))
        return false;

    // Check for tracer start
    if (strcasecmp(cmdName, "tracerStart") == 0)
    {
        char argStr[MAX_CMD_NAME_STR];
        argStr[0] = 0;
        bool logging = false;
        if (jsonGetValueForKey("logging", pCmdJson, argStr, MAX_CMD_NAME_STR))
            if ((strlen(argStr) != 0) && (argStr[0] != '0'))
                logging = true;
        bool recordAll = false;
        if (jsonGetValueForKey("record", pCmdJson, argStr, MAX_CMD_NAME_STR))
            if ((strlen(argStr) != 0) && (argStr[0] != '0'))
                recordAll = true;
        bool compareToEmulated = false;
        if (jsonGetValueForKey("compare", pCmdJson, argStr, MAX_CMD_NAME_STR))
            if ((strlen(argStr) != 0) && (argStr[0] != '0'))
                compareToEmulated = true;

        // Start tracing
        if (_pThisInstance)
            _pThisInstance->start(logging, recordAll, compareToEmulated, true);
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "tracerStop") == 0)
    {
        char argStr[MAX_CMD_NAME_STR];
        argStr[0] = 0;
        bool logging = false;
        if (jsonGetValueForKey("logging", pCmdJson, argStr, MAX_CMD_NAME_STR))
            if ((strlen(argStr) != 0) && (argStr[0] != '0'))
                logging = true;
        if (_pThisInstance)
            _pThisInstance->stop(logging);
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "tracerStatus") == 0)
    {
        static const int MAX_STATUS_INDEX_LEN = 20;
        char statusIdxStr[MAX_STATUS_INDEX_LEN];
        statusIdxStr[0] = 0;
        jsonGetValueForKey("msgIdx", pCmdJson, statusIdxStr, MAX_STATUS_INDEX_LEN);
        static const int MAX_STATUS_RESP_LEN = 1000;
        char statusStr[MAX_STATUS_RESP_LEN];
        strcpy(statusStr, "");
        if (_pThisInstance)
            _pThisInstance->getStatus(statusStr, MAX_STATUS_RESP_LEN, statusIdxStr);
        strlcpy(pRespJson, statusStr, maxRespLen);
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "TracerStatus %s", statusStr);
        return true;
    }
    else if (strcasecmp(cmdName, "tracerGetLong") == 0)
    {
        if (_pThisInstance)
            _pThisInstance->getTraceLong(pRespJson, maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "tracerGetBin") == 0)
    {
        if (_pThisInstance)
            _pThisInstance->getTraceBin();
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Start/Stop
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void StepTracer::start(bool logging, bool recordAll, bool compareToEmulated, bool primeFromMem)
{
    // Debug
    _logging = logging;
    _recordAll = recordAll;
    _compareToEmulated = compareToEmulated;
    if (_logging)
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "TracerStart logging %d record %d compare %d",
                    _logging, _recordAll, _compareToEmulated);

    // Connect to the bus socket
    _busControl.socketEnable(_busSocketId, true);

    // Prime from memory if required
    if (primeFromMem)
    {
#ifdef STEP_VAL_WITHOUT_HW_MANAGER
        // Grab all of normal memory
        uint8_t* pBuf = new uint8_t[STD_TARGET_MEMORY_LEN];
        int blockReadResult = BusAccess::blockRead(0, pBuf, STD_TARGET_MEMORY_LEN, true, false);
        if (blockReadResult != BR_OK)
            LogWrite(MODULE_PREFIX, LOG_DEBUG, "Block read for tracer failed %d", blockReadResult);
        uint32_t maxLen = (_tracerMemoryLen < STD_TARGET_MEMORY_LEN) ? _tracerMemoryLen : STD_TARGET_MEMORY_LEN;
        if (_pTracerMemory)
            memcopyfast(_pTracerMemory, pBuf, maxLen);
        delete [] pBuf;
#else
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "Tracer prime from memory");
        _busControl.socketRequestBus(_busSocketId, BR_BUS_REQ_REASON_MIRROR);
        _primeFromMemPending = true;
#endif
    }
}

void StepTracer::stopAll(bool logging)
{
    if (_pThisInstance)
        _pThisInstance->stop(logging);
}

void StepTracer::stop(bool logging)
{
    _logging = logging;
    if (_logging)
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "TracerStop");

    // TODO 2022
    // // Remove wait on memory and IO
    // BusAccess::waitOnMemory(_busSocketId, false);
    // BusAccess::waitOnIO(_busSocketId, false);

    // // Clear wait
    // BusAccess::waitHold(_busSocketId, false);

    // // Clear bus hold
    // BusAccess::waitRelease();

    // Turn off the bus socket but don't set _busSocketId to -1
    // or all sockets will be used up
    if (_busSocketId >= 0)
        _busControl.socketEnable(_busSocketId, false);
    _isActive = false;

    // Clear log buffers
    _tracesPosn.clear();
    _exceptionsPosn.clear();

    // Clear stats
    _stats.clear();

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Callbacks/Hooks
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void StepTracer::busReqAckedStatic(void* pObject, BR_BUS_REQ_REASON reason, BR_RETURN_TYPE rslt)
{
    if (!pObject)
        return;
    ((StepTracer*)pObject)->busReqAcked(reason, rslt);
}

void StepTracer::busReqAcked(BR_BUS_REQ_REASON reason, BR_RETURN_TYPE rslt)
{
    // if ((actionType == BR_BUS_ACTION_RESET) && _pThisInstance)
    // {
    //     _pThisInstance->resetComplete();
    // }
    // else if ((actionType == BR_BUS_ACTION_BUSRQ) && _pThisInstance)
    // {
    if (_pThisInstance->_primeFromMemPending)
    {
        // Clone the system's memory
        HwManager::tracerClone();
        _pThisInstance->_primeFromMemPending = false;
        // Add wait on memory and IO
        BusAccess::waitOnMemory(_busSocketId, true);
        BusAccess::waitOnIO(_busSocketId, true);
        // Clear wait
        BusAccess::waitHold(_busSocketId, false);
        // Clear bus hold
        BusAccess::waitRelease();
        // Reset target - becomes active when reset acknowledge signal is received
        BusAccess::targetReqReset(_busSocketId);
    }
    // }
}

void StepTracer::resetComplete()
{
    // Stop
    _isActive = false;

    // Clear log buffers
    _tracesPosn.clear();
    _exceptionsPosn.clear();

    // Reset the emulated CPU
    Z80RESET(&_cpu_z80);

    // Clear stats
    _stats.clear();

    // Clear test case variables
    _stepCycleCount = 0;
    _stepCyclePos = 0;
    _isActive = true;

    #ifdef USE_PI_SPI0_CE0_AS_DEBUG_PIN
        digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
        microsDelay(20);
        digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
    #endif
    
    if (_logging)
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "Reset");
#ifdef USE_PI_SPI0_CE0_AS_DEBUG_PIN
    digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
    microsDelay(1);
    digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
#endif

}

void StepTracer::handleWaitInterruptStatic(void* pObject, uint32_t addr, uint32_t data, 
        uint32_t flags, uint32_t& retVal)
{
    if (_pThisInstance)
        _pThisInstance->handleWaitInterrupt(addr, data, flags, retVal);
}

void StepTracer::handleWaitInterrupt([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, 
        [[maybe_unused]] uint32_t flags, [[maybe_unused]] uint32_t& retVal)
{    
    if (!_isActive)
        return;
    
    // TODO
    // Debug signal
    #ifdef USE_PI_SPI0_CE0_AS_DEBUG_PIN
        digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
        microsDelay(1);
        digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
    #endif


    // Handle comparison with an emulated processor
    if (_compareToEmulated)
    {
        // Get Z80 expected behaviour from emulated CPU
        uint32_t expCtrl = 0;
        uint32_t expAddr = 0;
        uint32_t expData = 0;
        if (_stepCyclePos == _stepCycleCount)
        {
            _stepCyclePos = 0;
            _stepCycleCount = 0;
            Z80Execute(&_cpu_z80);
            _stats.instructionCount++;
        }
        expCtrl = _stepCycles[_stepCyclePos].flags;
        expAddr = _stepCycles[_stepCyclePos].addr;
        expData = _stepCycles[_stepCyclePos].data;
        _stepCyclePos++;

        // Check against what we got from the real system
        if ((addr != expAddr) || (expCtrl != (flags & ~BR_CTRL_BUS_WAIT_MASK)) || (expData != data))
        {
            if (_exceptionsPosn.canPut())
            {
                int pos = _exceptionsPosn.posToPut();
                _exceptions[pos].addr = addr;
                _exceptions[pos].expectedAddr = expAddr;
                _exceptions[pos].dataFromZ80 = data;
                _exceptions[pos].expectedData = expData;
                _exceptions[pos].dataToZ80 = retVal;
                _exceptions[pos].flags = flags;
                _exceptions[pos].expectedFlags = expCtrl;
                _exceptions[pos].stepCount = _stats.isrCalls;
                _exceptionsPosn.hasPut();
            }
            _stats.errors++;

    #ifdef USE_PI_SPI0_CE0_AS_DEBUG_PIN
            for (int i = 0; i < _stepCycleCount + 3; i++)
            {
                digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
                microsDelay(1);
                digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
                microsDelay(1);
            }
    #endif

            // Debug signal
    #ifdef USE_PI_SPI0_CE0_AS_DEBUG_PIN
            digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
            microsDelay(100);
            digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
    #endif
        }
    }

    // Handle tracing of all activity
    if (_recordAll)
    {
        // Make sure there is enought space in the buffer for the entire instruction
        if (_tracesPosn.canPut())
        {
            // Put the value
            int pos = _tracesPosn.posToPut();
            _traces[pos].addr = addr;
            _traces[pos].busData = data;
            _traces[pos].returnedData = retVal;
            _traces[pos].flags = flags;
            _traces[pos].traceCount = _stats.isrCalls;
            _tracesPosn.hasPut();

            // Check if we need to hold - ensure there's space for all accesses that might follow
            // noting that a wait on a PUSH (for instance) will involve at least two further writes
            // before the wait takes place
            if (_tracesPosn.size() - _tracesPosn.count() < MIN_SPACES_IN_TRACES)
            {
                // Hold the bus
                _recordIsHoldingTarget = true;
                _busControl.waitHold(_busSocketId, true);
            }
        }
    }

    // Bump instruction count
    _stats.isrCalls++;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void StepTracer::service()
{
    _serviceCount++;
    if (_serviceCount < 10000)
        return;
    _serviceCount = 0;

    // Handle logging and tracing
    static const int DEBUG_MSG_MAX = 1000;
    char debugMsg[DEBUG_MSG_MAX];
    debugMsg[0] = 0;

    // Logging
    if (_exceptionsPosn.canGet())
    {
        // Check if logging enabled
        if (_logging)
        {
            // Exception
            uint32_t pos = _exceptionsPosn.posToGet();
            uint32_t flags = _exceptions[pos].flags;
            uint32_t expFlags = _exceptions[pos].expectedFlags;
            snprintf(debugMsg, sizeof(debugMsg), "%07u got %04x %02x",
                        _exceptions[pos].stepCount,
                        _exceptions[pos].addr, 
                        _exceptions[pos].dataFromZ80);
            char tmpStr[100];
            snprintf(tmpStr, sizeof(tmpStr), " %c%c%c%c%c%c%c%c%c",
                        flags & 0x01 ? 'R': ' ', flags & 0x02 ? 'W': ' ', flags & 0x04 ? 'M': ' ',
                        flags & 0x08 ? 'I': ' ', flags & 0x10 ? '1': ' ', flags & 0x20 ? 'T': ' ',
                        flags & 0x40 ? 'X': ' ', flags & 0x80 ? 'Q': ' ', flags & 0x100 ? 'N': ' ');
            strlcpy(debugMsg, tmpStr, DEBUG_MSG_MAX);
            snprintf(tmpStr, sizeof(tmpStr),"exp %04x %02x",
                        _exceptions[pos].expectedAddr, 
                        _exceptions[pos].expectedData == 0xffff ? 0 : _exceptions[pos].expectedData);
            strlcpy(debugMsg, tmpStr, DEBUG_MSG_MAX);
            snprintf(tmpStr, sizeof(tmpStr), "%c%c%c%c%c%c%c%c%c ToZ80 %02x",
                        expFlags & 0x01 ? 'R': ' ', expFlags & 0x02 ? 'W': ' ', expFlags & 0x04 ? 'M': ' ',
                        expFlags & 0x08 ? 'I': ' ', expFlags & 0x10 ? '1': ' ', expFlags & 0x20 ? 'T': ' ',
                        expFlags & 0x40 ? 'X': ' ', expFlags & 0x80 ? 'Q': ' ', expFlags & 0x100 ? 'N': ' ',
                        _exceptions[pos].dataToZ80);
            strlcpy(debugMsg, tmpStr, DEBUG_MSG_MAX);
            LogWrite(MODULE_PREFIX, LOG_DEBUG, debugMsg);
        }

        // No longer need exception
        _exceptionsPosn.hasGot();
    }
}

void StepTracer::getStatus(char* pRespJson, [[maybe_unused]]int maxRespLen, const char* statusIdxStr)
{
    snprintf(pRespJson, sizeof(pRespJson), "\"isrCount\":%u,\"errors\":%d,\"msgIdx\":%s", _stats.isrCalls, _stats.errors, statusIdxStr);
}

void StepTracer::getTraceLong(char* pRespJson, int maxRespLen)
{
    if (!_tracesPosn.canGet())
    {
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return;
    }
    // Trace
    uint32_t pos = _tracesPosn.posToGet();

    uint32_t flags = _traces[pos].flags;
    snprintf(pRespJson, sizeof(pRespJson), "\"err\":\"ok\",\"trace\":{\"step\":%u,\"addr\":\"%04x\",\"data\":\"%02x\",\"flags\":\"%c%c%c%c%c%c%c%c%c\"}",
                _traces[pos].traceCount,
                _traces[pos].addr, 
                ((flags & 0x02) || (_traces[pos].returnedData & 0x80000000)) ? _traces[pos].busData : _traces[pos].returnedData, 
                flags & 0x01 ? 'R': '.', flags & 0x02 ? 'W': '.', flags & 0x04 ? 'M': '.',
                flags & 0x08 ? 'I': '.', flags & 0x10 ? '1': '.', flags & 0x20 ? 'T': '.',
                flags & 0x40 ? 'X': '.', flags & 0x80 ? 'Q': '.', flags & 0x100 ? 'N': '.');

    // Move ring buffer on
    _tracesPosn.hasGot();

    // No longer hold
    if (_recordIsHoldingTarget && (_tracesPosn.size() - _tracesPosn.count() > MIN_SPACES_IN_TRACES))
    {
        _recordIsHoldingTarget = false;
        BusAccess::waitHold(_busSocketId, false);
        BusAccess::waitRelease();
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get execution trace as binary data
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void StepTracer::getTraceBin()
{
    // Check if we would be able to transmit without issues
    uint32_t txAvailable = CommandHandler::getTxAvailable();
    if (txAvailable < MIN_TX_AVAILABLE_FOR_BIN_FRAME)
    {
        // TODO
        ISR_ASSERT(ISR_ASSERT_CODE_DEBUG_H);
        return;
    }
    else
    {
        ISR_ASSERT(ISR_ASSERT_CODE_DEBUG_I);
    }

    if (!_tracesPosn.canGet())
        return;

    // Current buffer position
    uint32_t pos = _tracesPosn.posToGet();
    uint32_t initialTraceCount = _traces[pos].traceCount;

    // Read while available or until full
    TraceBinElemFormat binElems[MAX_TRACE_MSG_BUF_ELEMS_MAX * sizeof(TraceBinElemFormat)];
    uint32_t count = 0;
    for (int i = 0; i < MAX_TRACE_MSG_BUF_ELEMS_MAX; i++)
    {
        if (!_tracesPosn.canGet())
            break;
        binElems[i].addr = _traces[pos].addr;
        binElems[i].busData = _traces[pos].busData;
        binElems[i].retData = _traces[pos].returnedData & 0xff;
        binElems[i].flags = (_traces[pos].flags & 0x3f) | ((_traces[pos].returnedData & BR_MEM_ACCESS_RSLT_NOT_DECODED) ? 0 : 0x80);
        count++;

        // Move ring buffer on
        _tracesPosn.hasGot();
        pos = _tracesPosn.posToGet();
    }

    // Form JSON message
    static const int JSON_RESP_MAX_LEN = 10000;
    char jsonFrame[JSON_RESP_MAX_LEN];
    strlcpy(jsonFrame, "{\"cmdName\":\"", JSON_RESP_MAX_LEN);
    strlcat(jsonFrame, "tracerGetBinData", JSON_RESP_MAX_LEN);
    strlcat(jsonFrame, "\"", JSON_RESP_MAX_LEN);

    char tmpStr[50];
    // sprintf(tmpStr, ",\"txAvail\":%u", txAvailable);
    // strlcat(jsonFrame, tmpStr, JSON_RESP_MAX_LEN);

    snprintf(tmpStr, sizeof(tmpStr), ",\"traceCount\":%u", initialTraceCount);
    strlcat(jsonFrame, tmpStr, JSON_RESP_MAX_LEN);

    // Data len
    uint32_t binDataLen = count*sizeof(TraceBinElemFormat);
    snprintf(tmpStr, sizeof(tmpStr), ",\"dataLen\":%u}", binDataLen);
    strlcat(jsonFrame, tmpStr, JSON_RESP_MAX_LEN);

    // Copy binary to end of buffer
    memcopyfast(jsonFrame+strlen(jsonFrame)+1, (uint8_t*)binElems, binDataLen);

    CommandHandler::sendWithJSON("rdp", "", 0, (const uint8_t*)jsonFrame, strlen(jsonFrame)+1+binDataLen);

    // TODO
    txAvailable = CommandHandler::getTxAvailable();
    ISR_VALUE(ISR_ASSERT_CODE_DEBUG_J, txAvailable);

    // No longer hold
    if (_recordIsHoldingTarget && (_tracesPosn.size() - _tracesPosn.count() > MIN_SPACES_IN_TRACES))
    {
        _recordIsHoldingTarget = false;
        BusAccess::waitHold(_busSocketId, false);
        BusAccess::waitRelease();
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Tracer mem/IO access
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Memory and IO functions
byte StepTracer::mem_read([[maybe_unused]] int param, [[maybe_unused]] ushort address)
{
    if (!_pThisInstance)
        return 0;
    uint32_t dataVal = 0; 
#ifdef STEP_VAL_WITHOUT_HW_MANAGER
    if (_pThisInstance->_pTracerMemory && (address < _pThisInstance->_tracerMemoryLen))
        dataVal = _pThisInstance->_pTracerMemory[address];
#else
    HwManager::tracerHandleAccess(address, 0, BR_CTRL_BUS_MREQ_MASK | BR_CTRL_BUS_RD_MASK, dataVal);
#endif
    if (_stepCycleCount < MAX_STEP_CYCLES_FOR_INSTR)
    {
        _stepCycles[_stepCycleCount].addr = address;
        _stepCycles[_stepCycleCount].data = dataVal;
        _stepCycles[_stepCycleCount].flags = (_pThisInstance->_cpu_z80.M1 ? BR_CTRL_BUS_M1_MASK : 0) |
                        BR_CTRL_BUS_RD_MASK | BR_CTRL_BUS_MREQ_MASK;
        _stepCycleCount++;
    }
    return dataVal;
}

void StepTracer::mem_write([[maybe_unused]] int param, [[maybe_unused]] ushort address, [[maybe_unused]] byte data)
{
    if (!_pThisInstance)
        return;
    if (_stepCycleCount < MAX_STEP_CYCLES_FOR_INSTR)
    {
        _stepCycles[_stepCycleCount].addr = address;
        _stepCycles[_stepCycleCount].data = data;
        _stepCycles[_stepCycleCount].flags = BR_CTRL_BUS_WR_MASK | BR_CTRL_BUS_MREQ_MASK;
        _stepCycleCount++;
    }
#ifdef STEP_VAL_WITHOUT_HW_MANAGER
    if (_pThisInstance->_pTracerMemory && (address < _pThisInstance->_tracerMemoryLen))
        _pThisInstance->_pTracerMemory[address] = data;
#else
    uint32_t retVal = 0;
    HwManager::tracerHandleAccess(address, data, BR_CTRL_BUS_MREQ_MASK | BR_CTRL_BUS_WR_MASK, retVal);
#endif
}

byte StepTracer::io_read([[maybe_unused]] int param, [[maybe_unused]] ushort address)
{
    if (!_pThisInstance)
        return 0;
    uint32_t dataVal = 0x80; // TODO - really need to find a way to make this the value returned on the BUS to keep validity
#ifndef STEP_VAL_WITHOUT_HW_MANAGER
    HwManager::tracerHandleAccess(address, 0, BR_CTRL_BUS_IORQ_MASK | BR_CTRL_BUS_RD_MASK, dataVal);
#endif
    if (_stepCycleCount < MAX_STEP_CYCLES_FOR_INSTR)
    {
        _stepCycles[_stepCycleCount].addr = address;
        _stepCycles[_stepCycleCount].data = dataVal;
        _stepCycles[_stepCycleCount].flags = BR_CTRL_BUS_RD_MASK | BR_CTRL_BUS_IORQ_MASK;
        _stepCycleCount++;
    }
    return dataVal;
}

void StepTracer::io_write([[maybe_unused]] int param, [[maybe_unused]] ushort address, [[maybe_unused]] byte data)
{
    if (!_pThisInstance)
        return;
    if (_stepCycleCount < MAX_STEP_CYCLES_FOR_INSTR)
    {
        _stepCycles[_stepCycleCount].addr = address;
        _stepCycles[_stepCycleCount].data = data;
        _stepCycles[_stepCycleCount].flags = BR_CTRL_BUS_WR_MASK | BR_CTRL_BUS_IORQ_MASK;
        _stepCycleCount++;
    }
#ifndef STEP_VAL_WITHOUT_HW_MANAGER
    uint32_t retVal = 0;
    HwManager::tracerHandleAccess(address, data, BR_CTRL_BUS_IORQ_MASK | BR_CTRL_BUS_WR_MASK, retVal);
#endif
}
