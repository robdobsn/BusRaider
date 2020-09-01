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
// Self test - memory test
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusRaiderApp::testSelf_memory()
{
    BusAccess& busAccess = _mcManager.getBusAccess();
    _display.consoleForeground(DISPLAY_FX_BLUE);
    _display.consolePut("\nMemory test\n");
    _display.consoleForeground(DISPLAY_FX_WHITE);
    _display.consolePut("CPU and RAM cards should be plugged into the bus\n");
    _display.consolePut("CPU must have a working CLOCK signal\n");

    enum testState_type
    {
        TEST_STATE_SET_MC,
        TEST_STATE_ASK_IF_RESET,
        TEST_STATE_PERFORM_RESET,
        TEST_STATE_CHECK_BUSRQ,
        TEST_STATE_MEMORY_TEST_NEXT,
        TEST_STATE_AWAIT_KEY,
        TEST_STATE_PERFORM_BLOCK_TEST,
        TEST_STATE_DONE_BLOCK_TEST,
        TEST_STATE_DONE
    } testState = TEST_STATE_SET_MC;

    struct testSequence_type
    {
        uint32_t blockStart;
        uint32_t blockLen;
    } testSequence[] =
    {
        { 0x4000, 0x4000 },
        { 0x8000, 0x4000 },
        { 0xc000, 0x4000 },
        { 0, 0x4000 }
    };

    // Loop here until tests done
    _testSelf_startUpdateTimeMs = millis();
    int issueCount = 0;
    uint32_t testSeqIdx = 0;
    while (1)
    {
        // Loop to service keyboard and bus access
        if (testSelf_commonLoop() != TEST_SELF_RET_OK)
            testState = TEST_STATE_DONE;

        // Check state
        switch(testState)
        {
            case TEST_STATE_SET_MC:
            {
                // bool mcSet = McManager::setMachineByName("Serial Terminal ANSI");
                // if (!mcSet)
                // {
                //     _display.consoleForeground(DISPLAY_FX_RED);
                //     _display.consolePut("FAILED to set machine Serial Terminal ANSI - maybe a name change?\n");
                //     _display.consoleForeground(DISPLAY_FX_WHITE);
                //     issueCount++;
                // }
                _display.consolePut("Do you want to reset the Z80 first (may be needed)? (y/n)");
                testState = TEST_STATE_ASK_IF_RESET;
                break;
            }
            case TEST_STATE_ASK_IF_RESET:
            {
                // 
                if (rdtolower(_testSelf_curKeyAscii) == 'y')
                {
                    _display.consolePut("\n");
                    _testSelf_startUpdateTimeMs = millis();
                    testState = TEST_STATE_PERFORM_RESET;
                } else if (rdtolower(_testSelf_curKeyAscii) == 'n')
                {
                    _display.consolePut("\n");
                    _testSelf_startUpdateTimeMs = millis();
                    testState = TEST_STATE_CHECK_BUSRQ;
                }
                break;
            }
            case TEST_STATE_PERFORM_RESET:
            {
                // Reset the machine
                busAccess.rawBusControlMuxSet(BR_MUX_RESET_Z80_BAR_LOW);
                microsDelay(100000);
                busAccess.rawBusControlMuxClear();
                testState = TEST_STATE_CHECK_BUSRQ;
                break;
            }
            case TEST_STATE_CHECK_BUSRQ:
            {
                // Check if BUSRQ works
                BR_RETURN_TYPE busAckedRetc = busAccess.controlRequestAndTake();
                if (busAckedRetc != BR_OK)
                {
                    _display.consoleForeground(DISPLAY_FX_RED);
                    _display.consolePut("FAILED to request Z80 bus: ");
                    _display.consolePut(busAccess.retcString(busAckedRetc));
                    _display.consolePut("\nCheck: CLOCK jumper installed on BusRaider OR external clock\n");
                    _display.consolePut("Use the test command to test the bus\n");
                    _display.consolePut("Or if you have a scope/logic analyzer check:\n");
                    _display.consolePut("- CLOCK line on the backplane - should be oscillating\n");
                    _display.consolePut("- BUSRQ should pulse low during this test and BUSACK should follow\n");
                    _display.consoleForeground(DISPLAY_FX_WHITE);
                    issueCount++;
                    testState = TEST_STATE_DONE;
                }
                else
                {
                    testState = TEST_STATE_MEMORY_TEST_NEXT;
                }
                break;
            }
            case TEST_STATE_MEMORY_TEST_NEXT:
            {
                char testStr[200];
                ee_sprintf(testStr, "*** NOTE: Do not test FLASH or ROM blocks ***\nTest RAM %04x - %04x (length %d)? (y/n) ", 
                                testSequence[testSeqIdx].blockStart,
                                testSequence[testSeqIdx].blockStart + testSequence[testSeqIdx].blockLen-1,
                                testSequence[testSeqIdx].blockLen);
                _display.consolePut(testStr);
                testState = TEST_STATE_AWAIT_KEY;
                break;
            }
            case TEST_STATE_AWAIT_KEY:
            {
                // 
                if (rdtolower(_testSelf_curKeyAscii) == 'y')
                {
                    _display.consolePut("... performing test\n");
                    _testSelf_startUpdateTimeMs = millis();
                    testState = TEST_STATE_PERFORM_BLOCK_TEST;
                } else if (rdtolower(_testSelf_curKeyAscii) == 'n')
                {
                    _display.consolePut("... skipping test\n");
                    _testSelf_startUpdateTimeMs = millis();
                    testState = TEST_STATE_DONE_BLOCK_TEST;
                }
                break;
            }
            case TEST_STATE_PERFORM_BLOCK_TEST:
            {
                testState = TEST_STATE_DONE_BLOCK_TEST;

                uint32_t blockStart = testSequence[testSeqIdx].blockStart;
                uint32_t blockLen = testSequence[testSeqIdx].blockLen;

                uint32_t rslt = memTestDevice(blockStart, blockLen, busAccess);
                bool blockTestFailed = false;
                if (rslt != 0)
                {
                    _display.consoleForeground(DISPLAY_FX_RED);
                    char outStr[100];
                    ee_sprintf(outStr, "FAILED Memory test at address %04x - check memory card\n", rslt);
                    _display.consolePut(outStr);
                    _display.consoleForeground(DISPLAY_FX_WHITE);
                    issueCount++;
                    blockTestFailed = true;
                }

                if (!blockTestFailed)
                {
                    rslt = memTestAddressBus(blockStart, blockLen, busAccess);
                    if (rslt != 0)
                    {
                        _display.consoleForeground(DISPLAY_FX_RED);
                        char outStr[100];
                        ee_sprintf(outStr, "FAILED Address bus test at address %04x - check for shorts\n", rslt);
                        _display.consolePut(outStr);
                        _display.consoleForeground(DISPLAY_FX_WHITE);
                        issueCount++;
                        blockTestFailed = true;
                    }

                    rslt = memTestDataBus(blockStart, busAccess);
                    if (rslt != 0)
                    {
                        _display.consoleForeground(DISPLAY_FX_RED);
                        char outStr[100];
                        ee_sprintf(outStr, "FAILED Data bus test on data %02x - check for shorts\n", rslt);
                        _display.consolePut(outStr);
                        _display.consoleForeground(DISPLAY_FX_WHITE);
                        issueCount++;
                        blockTestFailed = true;
                    }
                }
                if (!blockTestFailed)
                {
                    _display.consoleForeground(DISPLAY_FX_GREEN);
                    _display.consolePut("Test Passed\n");
                    _display.consoleForeground(DISPLAY_FX_WHITE);
                }
                break;
            }
            case TEST_STATE_DONE_BLOCK_TEST:
            {
                // Next test
                testSeqIdx++;
                if (testSeqIdx >= sizeof(testSequence)/sizeof(testSequence[0]))
                    testState = TEST_STATE_DONE;
                else
                    testState = TEST_STATE_MEMORY_TEST_NEXT;
                break;
            }
            case TEST_STATE_DONE:
            {
                busAccess.controlRelease();
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

