// Bus Raider
// Rob Dobson 2019

#include "StepValidator.h"
#include "../Machines/McManager.h"
#include "../System/piwiring.h"
#include "../System/lowlib.h"
#include "../TargetBus/TargetState.h"
#include "libz80/z80.h"

// Module name
static const char FromStepValidator[] = "StepValidator";

StepValidatorCycle StepValidator::_stepCycles[MAX_STEP_CYCLES_FOR_INSTR];
int StepValidator::_stepCycleCount = 0;
int StepValidator::_stepCyclePos = 0;

StepValidator* StepValidator::_pThisInstance = NULL;

StepValidator::StepValidator() : 
        _emulatedMemory(STD_TARGET_MEMORY_LEN),
        _exceptionsPosn(NUM_DEBUG_VALS)
{
    _debugCurChVal = '0';
    _debugStepCount = 0;
    _excpCount = 0;
    _serviceCount = 0;
    _isActive = false;
    _stepCycleCount = 0;
    _stepCyclePos = 0;

    // Set Z80 CPU callbacks
    _pThisInstance = this;
    _cpu_z80.ioRead = io_read;
	_cpu_z80.ioWrite = io_write;
	_cpu_z80.memRead = mem_read;
	_cpu_z80.memWrite = mem_write;
}

void StepValidator::start()
{
    LogWrite(FromStepValidator, LOG_DEBUG, "start");

    // Setup test
    testCase1();

    // Deactivate wait interrupts so we don't get a false start
    _isActive = false;
    BusAccess::waitRestoreDefaults();
    BusAccess::waitIntDisable();

    // Reset the emulated CPU
    Z80RESET(&_cpu_z80);

    // Fill memory system of emulated machine
    _emulatedMemory.blockWrite(0, TargetState::getMemoryImagePtr(), TargetState::getMemoryImageSize());

    // Clear test case variables
    _excpCount = 0;
    _debugStepCount = 0;
    _stepCycleCount = 0;
    _stepCyclePos = 0;
    _isActive = true;

    // Program the real CPU and reset
    McManager::handleTargetProgram(true, true, false, false);

}

void StepValidator::stop()
{
    LogWrite(FromStepValidator, LOG_DEBUG, "stop");
    BusAccess::waitRestoreDefaults();
    _isActive = false;
}

uint32_t StepValidator::handleWaitInterrupt([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, 
        [[maybe_unused]] uint32_t flags, [[maybe_unused]] uint32_t retVal)
{

    if (!_isActive)
        return retVal;

    // Debug signal
    digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
    microsDelay(1);
    digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
    // microsDelay(2);
    // int addrMask = 0x8000;
    // for (int i = 0; i < 16; i++)
    // {
    //     digitalWrite(BR_DEBUG_PI_SPI0_CE0, (addr & addrMask));
    //     microsDelay(1);
    //     addrMask = addrMask >> 1;
    // }
    // digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);

    // // Get Z80 expected behaviour
    uint32_t expCtrl = 0; // BR_CTRL_BUS_RD_MASK | BR_CTRL_BUS_M1_MASK | BR_CTRL_BUS_MREQ_MASK | BR_CTRL_BUS_WAIT_MASK;
    uint32_t expAddr = 0; // _cpu_z80.PC;
    uint32_t expData = 0; // _emulatedMemory.getMemoryByte(expAddr);
    if (_stepCyclePos == _stepCycleCount)
    {
        _stepCyclePos = 0;
        _stepCycleCount = 0;
        Z80Execute(&_cpu_z80);
    }
    expCtrl = _stepCycles[_stepCyclePos].flags;
    expAddr = _stepCycles[_stepCyclePos].addr;
    expData = _stepCycles[_stepCyclePos].data;
    _stepCyclePos++;

    // switch(_debugCurAddr)
    // {
    //     case 0: nextAddr++; expCtrl |= BR_CTRL_BUS_M1_MASK; _debugCurChVal = '0'; break;
    //     case 1: nextAddr++; break;
    //     case 2: nextAddr++; expCtrl |= BR_CTRL_BUS_M1_MASK; break;
    //     case 3: nextAddr++; break;
    //     case 4: nextAddr++; break;
    //     case 5: nextAddr = 0x3c00; expCtrl |= BR_CTRL_BUS_M1_MASK; break;        
    //     case 6: nextAddr++; expCtrl |= BR_CTRL_BUS_M1_MASK; _debugCurBCCount = 0x1fff; break;
    //     case 7: nextAddr++; break;
    //     case 8: nextAddr++; break;
    //     case 9: nextAddr++; expCtrl |= BR_CTRL_BUS_M1_MASK; _debugCurBCCount--; break;
    //     case 10: nextAddr++; expCtrl |= BR_CTRL_BUS_M1_MASK; break;
    //     case 11: nextAddr++; expCtrl |= BR_CTRL_BUS_M1_MASK; break;
    //     case 12: nextAddr++; expCtrl |= BR_CTRL_BUS_M1_MASK; break;
    //     case 13: nextAddr++; break;
    //     case 14: nextAddr++; if (_debugCurBCCount != 0) nextAddr = 9; break;
    //     case 15: nextAddr++; expCtrl |= BR_CTRL_BUS_M1_MASK; break;
    //     case 16: nextAddr++; expCtrl |= BR_CTRL_BUS_M1_MASK; _debugCurChVal++; break;
    //     case 17: nextAddr++; expCtrl |= BR_CTRL_BUS_M1_MASK; break;
    //     case 18: nextAddr++; break;
    //     case 19: nextAddr++; expCtrl |= BR_CTRL_BUS_M1_MASK; break;
    //     case 20: nextAddr++; break;
    //     case 21: nextAddr++; if (_debugCurChVal <= '9') nextAddr = 2; break;
    //     case 22: nextAddr++; expCtrl |= BR_CTRL_BUS_M1_MASK; break;
    //     case 23: nextAddr++; break;
    //     case 24: nextAddr = 0; break;
    //     case 0x3c00: nextAddr = 6; expCtrl = BR_CTRL_BUS_WR_MASK | BR_CTRL_BUS_MREQ_MASK | BR_CTRL_BUS_WAIT_MASK; 
    //             expData = _debugCurChVal; break;
    // }

    if ((addr != expAddr) || (expCtrl != (flags & ~BR_CTRL_BUS_WAIT_MASK)) || (expData != data))
    {
        if ((_excpCount < 10) && _exceptionsPosn.canPut())
        {
            int pos = _exceptionsPosn.posToPut();
            _exceptions[pos].addr = addr;
            _exceptions[pos].expectedAddr = expAddr;
            _exceptions[pos].dataFromZ80 = data;
            _exceptions[pos].expectedData = expData;
            _exceptions[pos].dataToZ80 = retVal;
            _exceptions[pos].flags = flags;
            _exceptions[pos].expectedFlags = expCtrl;
            _exceptions[pos].stepCount = _debugStepCount;
            _exceptionsPosn.hasPut();
                ISR_ASSERT(ISR_ASSERT_CODE_DEBUG_A);
        }
        _excpCount++;

        // Since there was an error we don't know what the next address should be
        // So guess that it is the next address
        // nextAddr = addr + 1;
    }
    // _debugCurAddr = nextAddr;
    _debugStepCount++;

    return retVal;
}

void StepValidator::service()
{
    _serviceCount++;
    if (_serviceCount < 100)
        return;
    _serviceCount = 0;

    // Handle debug 
    char debugJson[500];
    debugJson[0] = 0;
    if (_exceptionsPosn.canGet())
    {
        int pos = _exceptionsPosn.posToGet();

        int flags = _exceptions[pos].flags;
        int expFlags = _exceptions[pos].expectedFlags;
        ee_sprintf(debugJson+strlen(debugJson), "%07d got %04x %02x %c%c%c%c%c%c exp %04x %02x %c%c%c%c%c%c ToZ80 %02x",
                    _exceptions[pos].stepCount,
                    _exceptions[pos].addr, 
                    _exceptions[pos].dataFromZ80, 
                    flags & 0x01 ? 'R': ' ', flags & 0x02 ? 'W': ' ', flags & 0x04 ? 'M': ' ',
                    flags & 0x08 ? 'I': ' ', flags & 0x10 ? '1': ' ', flags & 0x20 ? 'T': ' ',
                    _exceptions[pos].expectedAddr, 
                    _exceptions[pos].expectedData == 0xffff ? 0 : _exceptions[pos].expectedData, 
                    expFlags & 0x01 ? 'R': ' ', expFlags & 0x02 ? 'W': ' ', expFlags & 0x04 ? 'M': ' ',
                    expFlags & 0x08 ? 'I': ' ', expFlags & 0x10 ? '1': ' ', expFlags & 0x20 ? 'T': ' ',
                    _exceptions[pos].dataToZ80);
        McManager::logDebugMessage(debugJson);

        _exceptionsPosn.hasGot();
    }
}

void StepValidator::testCase1()
{
    static const char* testStr = "  <- Step Tester starting .....";

    // Clear target state
    TargetState::clear();

    // Write to target state
    TargetState::addMemoryBlock(0x3c00, (uint8_t*)testStr, strlen(testStr));

    static uint8_t program[] = {
                                // org 0

                                // loopstart:
        0x1e, 0x30,             // 	ld e,0x30
                                // loophere:
        0x21, 0x00, 0x3c,       // 	ld hl,0x3c00
        0x73,                   // 	ld (hl),e
        0x01, 0xff, 0x1f,       // 	ld bc,0x0001
                                // loopin:
        0x0b,                   // 	dec bc
        0x78,                   // 	ld a,b
        0xb1,                   // 	or c
        0xc2, 0x09, 0x00,       // 	jp nz, loopin
        0x1c,                   // 	inc e
        0x7b,                   // 	ld a, e
        0xfe, 0x3a,             // 	cp a,0x3a
        0xc2, 0x02, 0x00,       // 	jp nz,loophere
        0xc3, 0x00, 0x00        // 	jp loopstart
    };

    TargetState::addMemoryBlock(0, program, sizeof(program));

}

// Memory and IO functions
byte StepValidator::mem_read([[maybe_unused]] int param, [[maybe_unused]] ushort address)
{
    if (!_pThisInstance)
        return 0;
    byte dataVal = _pThisInstance->_emulatedMemory.getMemoryByte(address);
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
    _pThisInstance->_emulatedMemory.putMemoryByte(address, data);
}

byte StepValidator::io_read([[maybe_unused]] int param, [[maybe_unused]] ushort address)
{
    if (!_pThisInstance)
        return 0;
    // TODO
    return 0;
}

void StepValidator::io_write([[maybe_unused]] int param, [[maybe_unused]] ushort address, [[maybe_unused]] byte data)
{
    // TODO
}
