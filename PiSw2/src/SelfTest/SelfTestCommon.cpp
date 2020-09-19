// Bus Raider
// Rob Dobson 2019

#include "BusRaiderApp.h"
#include "System/Display.h"
#include "System/ee_sprintf.h"
#include "System/PiWiring.h"
#include "System/lowlib.h"
#include "System/rdutils.h"
#include "TargetBus/BusAccess.h"
#include "System/timer.h"
#include "Machines/McTerminal.h"
#include "System/memoryTests.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Self test - common looping code
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BusRaiderApp::SelfTestRetType BusRaiderApp::testSelf_commonLoop()
{
    // Service the comms channels and display updates
    service();
    // Timer polling
    timer_poll();
    // Service bus access
    BusAccess::service();

    // Check for keyboard keys
    const char* pKeyStr = "\0";
    _testSelf_curKeyAscii = -1;
    if (_keyInfoBufferPos.canGet())
    {
        KeyInfo* pKeyInfo = &_keyInfoBuffer[_keyInfoBufferPos.posToGet()];
        _keyInfoBufferPos.hasGot();
        // LogWrite(FromBusRaiderTests, LOG_DEBUG, "Key %c", pKeyInfo->rawKeys[0]);
        pKeyStr = McTerminal::convertRawToKeyString(pKeyInfo->modifiers, pKeyInfo->rawKeys);
        if (rdisalpha(pKeyStr[0]) || rdisdigit(pKeyStr[0]) || rdisspace(pKeyStr[0]))
            _display.consolePut(pKeyStr);
        _testSelf_curKeyAscii = pKeyStr[0];
    }

    // Test for quit
    if ((rdtolower(_testSelf_curKeyAscii) == 'q') || (_testSelf_curKeyAscii == 27))
        return TEST_SELF_RET_QUIT;

    // Test for time-out
    if (isTimeout(millis(), _testSelf_startUpdateTimeMs, 60000))
    {
        _display.consolePut("\nTest timed-out, repeat if required\n");
        return TEST_SELF_RET_TIMEOUT;
    }

    // Return ok
    return TEST_SELF_RET_OK;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Self test
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusRaiderApp::testSelf_busrq()
{
    _display.consolePut("\nSelf-testing\n");
    _display.consolePut("Make sure CPU and RAM are plugged in ...\n");

    enum testState_type
    {
        TEST_STATE_SET_MC,
        TEST_STATE_CHECK_BUSRQ,
        TEST_STATE_DONE
    } testState = TEST_STATE_SET_MC;

    // Loop here until tests done
    while (1)
    {
        // Loop to service keyboard and bus access
        if (testSelf_commonLoop() != TEST_SELF_RET_OK)
            testState = TEST_STATE_DONE;

        // Check state
        int issueCount = 0;
        switch(testState)
        {
            case TEST_STATE_SET_MC:
            {
                bool mcSet = McManager::setMachineByName("Serial Terminal ANSI");
                if (!mcSet)
                {
                    _display.consoleForeground(DISPLAY_FX_RED);
                    _display.consolePut("FAILED to set machine Serial Terminal ANSI - maybe a name change?\n");
                    _display.consoleForeground(DISPLAY_FX_WHITE);
                    issueCount++;
                }
                testState = TEST_STATE_CHECK_BUSRQ;
                break;
            }
            case TEST_STATE_CHECK_BUSRQ:
            {
                // Check if BUSRQ works
                BR_RETURN_TYPE busAckedRetc = BusAccess::controlRequestAndTake();
                if (busAckedRetc != BR_OK)
                {
                    _display.consoleForeground(DISPLAY_FX_RED);
                    _display.consolePut("FAILED to request Z80 bus: ");
                    _display.consolePut(BusAccess::retcString(busAckedRetc));
                    _display.consolePut("\nCheck CLOCK jumper installed on BusRaider OR external clock\n");
                    _display.consolePut("Use bustest command to test the bus slowly\n");
                    _display.consolePut("Or if you have a scope/logic analyzer check:\n");
                    _display.consolePut("- CLOCK line on the backplane - should be oscillating\n");
                    _display.consolePut("- BUSRQ should pulse low during this test and BUSACK should follow\n");
                    _display.consoleForeground(DISPLAY_FX_WHITE);
                    issueCount++;
                }
                testState = TEST_STATE_DONE;
                break;
            }
            case TEST_STATE_DONE:
            {
                BusAccess::controlRelease();
                if (issueCount == 0)
                {
                    _display.consoleForeground(DISPLAY_FX_GREEN);
                    _display.consolePut("Tests complete no issues found");
                    _display.consoleForeground(DISPLAY_FX_WHITE);
                }
                else
                {
                    _display.consoleForeground(DISPLAY_FX_RED);
                    char issueText[100];
                    ee_sprintf(issueText, "Tests complete %d issue%s found",
                                    issueCount, issueCount > 1 ? "s" : "");
                    _display.consolePut(issueText);
                    _display.consoleForeground(DISPLAY_FX_WHITE);
                }
                return;
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Self test - read/set bus
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusRaiderApp::testSelf_readSetBus(bool readMode)
{
    _display.consolePut("\nSelf-testing show bus values\n");
    _display.consolePut("Confirm ONLY BusRaider installed (y/n) ...");

    enum testState_type
    {
        TEST_STATE_CONFIRM,
        TEST_STATE_SET_MC,
        TEST_STATE_READ_BUS,
        TEST_STATE_SET_BUS,
        TEST_STATE_DONE
    } testState = TEST_STATE_CONFIRM;

    uint32_t lastUpdateTimeMs = 0;
    _testSelf_startUpdateTimeMs = millis();

    int issueCount = 0;
    while (1)
    {
        // Loop to service keyboard and bus access
        if (testSelf_commonLoop() != TEST_SELF_RET_OK)
            testState = TEST_STATE_DONE;
            
        // Check state
        switch(testState)
        {
            case TEST_STATE_CONFIRM:
            {
                if (_keyInfoBufferPos.canGet())
                {
                    KeyInfo* pKeyInfo = &_keyInfoBuffer[_keyInfoBufferPos.posToGet()];
                    _keyInfoBufferPos.hasGot();
                    // LogWrite(FromBusRaiderTests, LOG_DEBUG, "Keyin %02x %02x %02x", pKeyInfo->rawKeys[0], pKeyInfo->rawKeys[1], pKeyInfo->rawKeys[2]);
                    const char* pKeyStr = McTerminal::convertRawToKeyString(pKeyInfo->modifiers, pKeyInfo->rawKeys);
                    if (rdisalpha(pKeyStr[0]) || rdisdigit(pKeyStr[0]) || rdisspace(pKeyStr[0]))
                        _display.consolePut(pKeyStr);
                    if (rdtolower(pKeyStr[0]) == 'y')
                    {
                        _display.consolePut("\n");
                        testState = TEST_STATE_SET_MC;
                    }
                }
                if (isTimeout(millis(), _testSelf_startUpdateTimeMs, 20000))
                {
                    testState = TEST_STATE_DONE;
                    _display.consolePut("\nTimed-out, repeat if required\n");
                }
                break;
            }
            case TEST_STATE_SET_MC:
            {
                bool mcSet = McManager::setMachineByName("Serial Terminal ANSI");
                if (!mcSet)
                {
                    _display.consoleForeground(DISPLAY_FX_RED);
                    _display.consolePut("FAILED to set machine Serial Terminal ANSI - maybe a name change?\n");
                    _display.consoleForeground(DISPLAY_FX_WHITE);
                    issueCount++;
                }
                testState = readMode ? TEST_STATE_READ_BUS : TEST_STATE_SET_BUS;
                _testSelf_startUpdateTimeMs = millis();
                break;
            }
            case TEST_STATE_READ_BUS:
            {
                if (isTimeout(millis(), lastUpdateTimeMs, 1000))
                {
                    uint32_t rawVals = BusAccess::rawBusControlReadRaw();
                    uint32_t addr = 0, data = 0, ctrl = 0;
                    WR32(ARM_GPIO_GPCLR0, BR_DATA_DIR_IN_MASK);
                    BusAccess::rawBusControlReadAll(ctrl, addr, data);
                    char busInfo[100];
                    char ctrlBusStr[20];
                    BusAccess::formatCtrlBus(ctrl, ctrlBusStr, 20);
                    ee_sprintf(busInfo, "BUS Addr: %04x Data: %02x, Ctrl: %s%c%c (%08x)\n", 
                                addr, data, 
                                ctrlBusStr,
                                (rawVals & BR_BUSACK_BAR_MASK) ? '.' : 'K',
                                (rawVals & BR_WAIT_BAR_MASK) ? '.' : 'T',
                                rawVals);
                    _display.consolePut(busInfo);
                    lastUpdateTimeMs = millis();
                    // Toggle MREQ to ensure bus control is cleared
                    BusAccess::rawBusControlSetPin(BR_CTRL_BUS_MREQ_BITNUM, 0);
                    microsDelay(1);
                    BusAccess::rawBusControlSetPin(BR_CTRL_BUS_MREQ_BITNUM, 1);
                }
                if (isTimeout(millis(), _testSelf_startUpdateTimeMs, 60000))
                {
                    testState = TEST_STATE_DONE;
                    _display.consolePut("\nTest finished, repeat if required\n");
                }
                break;
            }
            case TEST_STATE_SET_BUS:
            {
                testState = TEST_STATE_DONE;
                break;                
            }
            case TEST_STATE_DONE:
            {
                BusAccess::controlRelease();
                return;
            }
        }
    }
}

