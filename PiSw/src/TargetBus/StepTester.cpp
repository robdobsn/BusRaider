// Bus Raider
// Rob Dobson 2019

#include "StepTester.h"
#include "../Machines/McManager.h"
#include "../System/piwiring.h"
#include "../System/lowlib.h"
#include "../System/ee_sprintf.h"

StepTester::StepTester() : 
        _exceptionsPosn(NUM_DEBUG_VALS)
{
    _debugCurBCCount = 0xffff;
    _debugCurChVal = '0';
    _debugCurAddr = 0;
    _debugStepCount = 0;
    _excpCount = 0;
    _serviceCount = 0;
    _isActive = false;
}

void StepTester::start()
{
    writeTestCode();
    _isActive = true;
}

void StepTester::stop()
{
    BusAccess::waitRestoreDefaults();

    _isActive = false;
}

void StepTester::writeTestCode()
{
    static const char* testStr = "  <- Step Tester starting ...";

    // Write using McManager
    McManager::blockWrite(0x3c00, (uint8_t*)testStr, strlen(testStr), true, false);

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

    McManager::blockWrite(0, program, sizeof(program), true, false);

    BusAccess::waitSetup(true, true);

    McManager::targetReset(false, false);
    _regs.clear();

}

uint32_t StepTester::handleWaitInterrupt([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, 
        [[maybe_unused]] uint32_t flags, [[maybe_unused]] uint32_t retVal)
{
    if (!_isActive)
        return retVal;

    // Mirror Z80 expected behaviour
    uint32_t expCtrl = BR_CTRL_BUS_RD_MASK | BR_CTRL_BUS_MREQ_MASK | BR_CTRL_BUS_WAIT_MASK;
    uint32_t nextAddr = _debugCurAddr;
    uint32_t expData = 0xffff;
    switch(_debugCurAddr)
    {
        case 0: nextAddr++; expCtrl |= BR_CTRL_BUS_M1_MASK; _debugCurChVal = '0'; break;
        case 1: nextAddr++; break;
        case 2: nextAddr++; expCtrl |= BR_CTRL_BUS_M1_MASK; break;
        case 3: nextAddr++; break;
        case 4: nextAddr++; break;
        case 5: nextAddr = 0x3c00; expCtrl |= BR_CTRL_BUS_M1_MASK; break;        
        case 6: nextAddr++; expCtrl |= BR_CTRL_BUS_M1_MASK; _debugCurBCCount = 0x1fff; break;
        case 7: nextAddr++; break;
        case 8: nextAddr++; break;
        case 9: nextAddr++; expCtrl |= BR_CTRL_BUS_M1_MASK; _debugCurBCCount--; break;
        case 10: nextAddr++; expCtrl |= BR_CTRL_BUS_M1_MASK; break;
        case 11: nextAddr++; expCtrl |= BR_CTRL_BUS_M1_MASK; break;
        case 12: nextAddr++; expCtrl |= BR_CTRL_BUS_M1_MASK; break;
        case 13: nextAddr++; break;
        case 14: nextAddr++; if (_debugCurBCCount != 0) nextAddr = 9; break;
        case 15: nextAddr++; expCtrl |= BR_CTRL_BUS_M1_MASK; break;
        case 16: nextAddr++; expCtrl |= BR_CTRL_BUS_M1_MASK; _debugCurChVal++; break;
        case 17: nextAddr++; expCtrl |= BR_CTRL_BUS_M1_MASK; break;
        case 18: nextAddr++; break;
        case 19: nextAddr++; expCtrl |= BR_CTRL_BUS_M1_MASK; break;
        case 20: nextAddr++; break;
        case 21: nextAddr++; if (_debugCurChVal <= '9') nextAddr = 2; break;
        case 22: nextAddr++; expCtrl |= BR_CTRL_BUS_M1_MASK; break;
        case 23: nextAddr++; break;
        case 24: nextAddr = 0; break;
        case 0x3c00: nextAddr = 6; expCtrl = BR_CTRL_BUS_WR_MASK | BR_CTRL_BUS_MREQ_MASK | BR_CTRL_BUS_WAIT_MASK; 
                expData = _debugCurChVal; break;
    }

    if ((addr != _debugCurAddr) || (expCtrl != flags) || ((expData != 0xffff) && (expData != data)))
    {
        _excpCount++;

        if ((_excpCount < 10) && _exceptionsPosn.canPut())
        {
            int pos = _exceptionsPosn.posToPut();
            _exceptions[pos].addr = addr;
            _exceptions[pos].expectedAddr = _debugCurAddr;
            _exceptions[pos].dataFromZ80 = data;
            _exceptions[pos].expectedData = expData;
            _exceptions[pos].dataToZ80 = retVal;
            _exceptions[pos].flags = flags;
            _exceptions[pos].expectedFlags = expCtrl;
            _exceptions[pos].stepCount = _debugStepCount;
            _exceptionsPosn.hasPut();
        }
        // Since there was an error we don't know what the next address should be
        // So guess that it is the next address
        nextAddr = addr + 1;
        // Debug signal
        // digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
        // microsDelay(2);
        // digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
    }
    _debugCurAddr = nextAddr;
    _debugStepCount++;

    return retVal;
}

void StepTester::service()
{
    if (!_isActive)
        return;

    _serviceCount++;
    if (_serviceCount < 100)
        return;
    _serviceCount = 0;

    if (_excpCount > 20)
        return;
    
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
