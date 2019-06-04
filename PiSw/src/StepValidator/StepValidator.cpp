// Bus Raider
// Rob Dobson 2019

#include "StepValidator.h"
#include "../System/piwiring.h"
#include "../System/lowlib.h"
#include "../System/ee_sprintf.h"
#include "../System/logging.h"
#include "../System/rdutils.h"
#include "libz80/z80.h"

// Uncomment the following line to use SPI0 CE0 of the Pi as a debug pin
// #define USE_PI_SPI0_CE0_AS_DEBUG_PIN 1

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Variables
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Module name
static const char FromStepValidator[] = "StepValidator";

// Main comms socket - to wire up command handler
CommsSocketInfo StepValidator::_commsSocketInfo =
{
    true,
    StepValidator::handleRxMsg,
    NULL,
    NULL
};

// Sockets
int StepValidator::_busSocketId = -1;
int StepValidator::_commsSocketId = -1;

// Step validator
StepValidatorCycle StepValidator::_stepCycles[MAX_STEP_CYCLES_FOR_INSTR];
int StepValidator::_stepCycleCount = 0;
int StepValidator::_stepCyclePos = 0;
BusSocketInfo StepValidator::_busSocketInfo = 
{
    false,
    StepValidator::handleWaitInterruptStatic,
    StepValidator::busActionCompleteStatic,
    true,
    true,
    false,
    0,
    false,
    0,
    false,
    0,
    false,
    BR_BUS_ACTION_GENERAL,
    false
};

// This instance
StepValidator* StepValidator::_pThisInstance = NULL;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Constructor
StepValidator::StepValidator() : 
        _exceptionsPosn(NUM_DEBUG_VALS)
{
    // Vars
    _isActive = false;
    _stepCycleCount = 0;
    _stepCyclePos = 0;
    _pThisInstance = this;
    _logging = false;
    _primeFromMemPending = false;
    _serviceCount = 0;

    // Validator memory as required
#ifdef STEP_VAL_WITHOUT_HW_MANAGER
    _validatorMemoryLen = 0;
    _pValidatorMemory = NULL;    
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

void StepValidator::init()
{
    // Allocate validator memory as required
#ifdef STEP_VAL_WITHOUT_HW_MANAGER
    _validatorMemoryLen = STD_TARGET_MEMORY_LEN;
    _pValidatorMemory = new uint8_t[_validatorMemoryLen];
#endif

    // Connect to the comms socket
    if (_commsSocketId < 0)
        _commsSocketId = CommandHandler::commsSocketAdd(_commsSocketInfo);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle CommandInterface message
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool StepValidator::handleRxMsg(const char* pCmdJson, [[maybe_unused]]const uint8_t* pParams, [[maybe_unused]]int paramsLen,
                [[maybe_unused]]char* pRespJson, [[maybe_unused]]int maxRespLen)
{
    // Get the command string from JSON
    #define MAX_CMD_NAME_STR 200
    char cmdName[MAX_CMD_NAME_STR+1];
    if (!jsonGetValueForKey("cmdName", pCmdJson, cmdName, MAX_CMD_NAME_STR))
        return false;

    // Check for validator start
    if (strcasecmp(cmdName, "validatorStart") == 0)
    {
        char loggingStr[MAX_CMD_NAME_STR];
        loggingStr[0] = 0;
        bool logging = false;
        jsonGetValueForKey("logging", pCmdJson, loggingStr, MAX_CMD_NAME_STR);
        if ((strlen(loggingStr) != 0) && (loggingStr[0] == '1'))
            logging = true;
        if (_pThisInstance)
            _pThisInstance->start(logging);
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "validatorStop") == 0)
    {
        if (_pThisInstance)
            _pThisInstance->stop();
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "validatorStatus") == 0)
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
        // LogWrite(FromStepValidator, LOG_DEBUG, "StepValStatus %s", statusStr);
        return true;
    }
    else if (strcasecmp(cmdName, "validatorPrimeFromMem") == 0)
    {
        if (_pThisInstance)
            _pThisInstance->primeFromMem();
        strlcpy(pRespJson, "\"err\":\"ok\"", maxRespLen);
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Start/Stop
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void StepValidator::start(bool logging)
{
    // Debug
    _logging = logging;
    if (_logging)
        LogWrite(FromStepValidator, LOG_DEBUG, "StepValStart");

    // Change CPU clock
    BusAccess::clockSetFreqHz(250000);

    // Connect to the bus socket
    if (_busSocketId < 0)
        _busSocketId = BusAccess::busSocketAdd(_busSocketInfo);
    BusAccess::busSocketEnable(_busSocketId, true);

    // Reset target - becomes active when reset acknowledge signal is received
    BusAccess::targetReqReset(_busSocketId);
}

void StepValidator::stop()
{
    if (_logging)
        LogWrite(FromStepValidator, LOG_DEBUG, "StepValStop");

    // Turn off the bus socket    
    BusAccess::busSocketEnable(_busSocketId, false);
    _isActive = false;
}

void StepValidator::primeFromMem()
{
#ifdef STEP_VAL_WITHOUT_HW_MANAGER
    // Grab all of normal memory
    uint8_t* pBuf = new uint8_t[STD_TARGET_MEMORY_LEN];
    int blockReadResult = BusAccess::blockRead(0, pBuf, STD_TARGET_MEMORY_LEN, false);
    if (blockReadResult != BR_OK)
        LogWrite(FromStepValidator, LOG_DEBUG, "Block read for stepVal failed %d", blockReadResult);
    uint32_t maxLen = (_validatorMemoryLen < STD_TARGET_MEMORY_LEN) ? _validatorMemoryLen : STD_TARGET_MEMORY_LEN;
    if (_pValidatorMemory)
        memcpy(_pValidatorMemory, pBuf, maxLen);
    delete [] pBuf;
#else
    BusAccess::targetReqBus(_busSocketId, BR_BUS_ACTION_MIRROR);
    _primeFromMemPending = true;
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Callbacks/Hooks
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void StepValidator::busActionCompleteStatic([[maybe_unused]]BR_BUS_ACTION actionType, [[maybe_unused]] BR_BUS_ACTION_REASON reason)
{
    if ((actionType == BR_BUS_ACTION_RESET) && _pThisInstance)
    {
        _pThisInstance->resetComplete();
}
    else if ((actionType == BR_BUS_ACTION_BUSRQ) && _pThisInstance)
    {
        if (_pThisInstance->_primeFromMemPending)
        {
            HwManager::validatorClone();
            _pThisInstance->_primeFromMemPending = false;
        }
    }
}

void StepValidator::resetComplete()
{
    // Reset the emulated CPU
    Z80RESET(&_cpu_z80);

    // Clear stats
    _stats.clear();

    // Clear test case variables
    _stepCycleCount = 0;
    _stepCyclePos = 0;
    _isActive = true;

    if (_logging)
        LogWrite(FromStepValidator, LOG_DEBUG, "Reset");
#ifdef USE_PI_SPI0_CE0_AS_DEBUG_PIN
    digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
    microsDelay(10);
    digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
#endif

}

void StepValidator::handleWaitInterruptStatic(uint32_t addr, uint32_t data, 
        uint32_t flags, uint32_t& retVal)
{
    if (_pThisInstance)
        _pThisInstance->handleWaitInterrupt(addr, data, flags, retVal);
}

void StepValidator::handleWaitInterrupt([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, 
        [[maybe_unused]] uint32_t flags, [[maybe_unused]] uint32_t& retVal)
{    
    
    if (!_isActive)
        return;

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

#ifdef USE_PI_SPI0_CE0_AS_DEBUG_PIN
    digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
    microsDelay(1);
    digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
#endif

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

        // Debug signal
#ifdef USE_PI_SPI0_CE0_AS_DEBUG_PIN
        digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
        microsDelay(200);
        digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
#endif
    }

    // Bump instruction count
    _stats.isrCalls++;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void StepValidator::service()
{
    _serviceCount++;
    if (_serviceCount < 100000)
        return;
    _serviceCount = 0;

    // Handle debug 
    char debugMsg[500];
    debugMsg[0] = 0;
    if (_exceptionsPosn.canGet())
    {
        // Check if logging enabled
        if (_logging)
        {
            // Exception
            int pos = _exceptionsPosn.posToGet();
            int flags = _exceptions[pos].flags;
            int expFlags = _exceptions[pos].expectedFlags;
            ee_sprintf(debugMsg+strlen(debugMsg), "%07d got %04x %02x %c%c%c%c%c%c%c%c%c exp %04x %02x %c%c%c%c%c%c%c%c%c ToZ80 %02x",
                        _exceptions[pos].stepCount,
                        _exceptions[pos].addr, 
                        _exceptions[pos].dataFromZ80, 
                        flags & 0x01 ? 'R': ' ', flags & 0x02 ? 'W': ' ', flags & 0x04 ? 'M': ' ',
                        flags & 0x08 ? 'I': ' ', flags & 0x10 ? '1': ' ', flags & 0x20 ? 'T': ' ',
                        flags & 0x40 ? 'X': ' ', flags & 0x80 ? 'Q': ' ', flags & 0x100 ? 'N': ' ',
                        _exceptions[pos].expectedAddr, 
                        _exceptions[pos].expectedData == 0xffff ? 0 : _exceptions[pos].expectedData, 
                        expFlags & 0x01 ? 'R': ' ', expFlags & 0x02 ? 'W': ' ', expFlags & 0x04 ? 'M': ' ',
                        expFlags & 0x08 ? 'I': ' ', expFlags & 0x10 ? '1': ' ', expFlags & 0x20 ? 'T': ' ',
                        expFlags & 0x40 ? 'X': ' ', expFlags & 0x80 ? 'Q': ' ', expFlags & 0x100 ? 'N': ' ',
                        _exceptions[pos].dataToZ80);
            LogWrite(FromStepValidator, LOG_DEBUG, debugMsg);
        }

        // No longer need exception
        _exceptionsPosn.hasGot();
    }
}

void StepValidator::getStatus(char* pRespJson, [[maybe_unused]]int maxRespLen, const char* statusIdxStr)
{
    ee_sprintf(pRespJson, "\"isrCount\":%d,\"errors\":%d,\"msgIdx\":%s", _stats.isrCalls, _stats.errors, statusIdxStr);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Validator mem/IO access
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Memory and IO functions
byte StepValidator::mem_read([[maybe_unused]] int param, [[maybe_unused]] ushort address)
{
    if (!_pThisInstance)
        return 0;
    uint32_t dataVal = 0; 
#ifdef STEP_VAL_WITHOUT_HW_MANAGER
    if (_pThisInstance->_pValidatorMemory && (address < _pThisInstance->_validatorMemoryLen))
        dataVal = _pThisInstance->_pValidatorMemory[address];
#else
    HwManager::validatorHandleAccess(address, 0, BR_CTRL_BUS_MREQ_MASK | BR_CTRL_BUS_RD_MASK, dataVal);
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

void StepValidator::mem_write([[maybe_unused]] int param, [[maybe_unused]] ushort address, [[maybe_unused]] byte data)
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
    if (_pThisInstance->_pValidatorMemory && (address < _pThisInstance->_validatorMemoryLen))
        _pThisInstance->_pValidatorMemory[address] = data;
#else
    uint32_t retVal = 0;
    HwManager::validatorHandleAccess(address, data, BR_CTRL_BUS_MREQ_MASK | BR_CTRL_BUS_WR_MASK, retVal);
#endif
}

byte StepValidator::io_read([[maybe_unused]] int param, [[maybe_unused]] ushort address)
{
    if (!_pThisInstance)
        return 0;
    uint32_t dataVal = 0x80; // TODO - really need to find a way to make this the value returned on the BUS to keep validity
#ifndef STEP_VAL_WITHOUT_HW_MANAGER
    HwManager::validatorHandleAccess(address, 0, BR_CTRL_BUS_IORQ_MASK | BR_CTRL_BUS_RD_MASK, dataVal);
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

void StepValidator::io_write([[maybe_unused]] int param, [[maybe_unused]] ushort address, [[maybe_unused]] byte data)
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
    HwManager::validatorHandleAccess(address, data, BR_CTRL_BUS_IORQ_MASK | BR_CTRL_BUS_WR_MASK, retVal);
#endif
}
