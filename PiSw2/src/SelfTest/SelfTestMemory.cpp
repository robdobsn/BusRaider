// Bus Raider
// Rob Dobson 2019

#include "BusRaiderApp.h"
#include "Display.h"
#include "PiWiring.h"
#include "lowlib.h"
#include "rdutils.h"
#include "BusAccess.h"
#include "memoryTests.h"
#include "KeyConversion.h"
#include "SelfTest.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Self test - memory test
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void selfTestMemory(BusRaiderApp* pBusRaiderApp, Display& display, BusAccess& busAccess)
{
    display.consoleForeground(DISPLAY_FX_BLUE);
    display.consolePut("\nMemory test\n");
    display.consoleForeground(DISPLAY_FX_WHITE);
    display.consolePut("CPU and RAM cards should be plugged into the bus\n");
    display.consolePut("CPU must have a working CLOCK signal\n");

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
    uint32_t startUpdateTimeMs = millis();
    int issueCount = 0;
    uint32_t testSeqIdx = 0;
    while (1)
    {
        // Loop to service keyboard and bus access
        pBusRaiderApp->selfTestHelperService();

        // Check state
        switch(testState)
        {
            case TEST_STATE_SET_MC:
            {
                // bool mcSet = McManager::setMachineByName("Serial Terminal ANSI");
                // if (!mcSet)
                // {
                //     display.consoleForeground(DISPLAY_FX_RED);
                //     display.consolePut("FAILED to set machine Serial Terminal ANSI - maybe a name change?\n");
                //     display.consoleForeground(DISPLAY_FX_WHITE);
                //     issueCount++;
                // }
                display.consolePut("Do you want to reset the Z80 first (may be needed)? (y/n)");
                testState = TEST_STATE_ASK_IF_RESET;
                break;
            }
            case TEST_STATE_ASK_IF_RESET:
            {
                // Get key
                int key = pBusRaiderApp->selfTestKeyboardGet();
                // LogWrite("MEMTEST", LogNotice, "GET KEY");

                // 
                if (rdtolower(key) == 'y')
                {
                    display.consolePut("\n");
                    startUpdateTimeMs = millis();
                    testState = TEST_STATE_PERFORM_RESET;
                } else if (rdtolower(key) == 'n')
                {
                    display.consolePut("\n");
                    startUpdateTimeMs = millis();
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
                    display.consoleForeground(DISPLAY_FX_RED);
                    display.consolePut("FAILED to request Z80 bus: ");
                    display.consolePut(busAccess.retcString(busAckedRetc));
                    display.consolePut("\nCheck: CLOCK jumper installed on BusRaider OR external clock\n");
                    display.consolePut("Use the test command to test the bus\n");
                    display.consolePut("Or if you have a scope/logic analyzer check:\n");
                    display.consolePut("- CLOCK line on the backplane - should be oscillating\n");
                    display.consolePut("- BUSRQ should pulse low during this test and BUSACK should follow\n");
                    display.consoleForeground(DISPLAY_FX_WHITE);
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
                snprintf(testStr, sizeof(testStr), "*** NOTE: Do not test FLASH or ROM blocks ***\nTest RAM %04x - %04x (length %d)? (y/n) ", 
                                testSequence[testSeqIdx].blockStart,
                                testSequence[testSeqIdx].blockStart + testSequence[testSeqIdx].blockLen-1,
                                testSequence[testSeqIdx].blockLen);
                display.consolePut(testStr);
                testState = TEST_STATE_AWAIT_KEY;
                break;
            }
            case TEST_STATE_AWAIT_KEY:
            {
                // Get key
                int key = pBusRaiderApp->selfTestKeyboardGet();
                if (rdtolower(key) == 'y')
                {
                    display.consolePut("... performing test\n");
                    startUpdateTimeMs = millis();
                    testState = TEST_STATE_PERFORM_BLOCK_TEST;
                } else if (rdtolower(key) == 'n')
                {
                    display.consolePut("... skipping test\n");
                    startUpdateTimeMs = millis();
                    testState = TEST_STATE_DONE_BLOCK_TEST;
                }
                break;
            }
            case TEST_STATE_PERFORM_BLOCK_TEST:
            {
                testState = TEST_STATE_DONE_BLOCK_TEST;

                uint32_t blockStart = testSequence[testSeqIdx].blockStart;
                uint32_t blockLen = testSequence[testSeqIdx].blockLen;

                uint32_t rslt = memTestDevice(busAccess, blockStart, blockLen);
                bool blockTestFailed = false;
                if (rslt != 0)
                {
                    display.consoleForeground(DISPLAY_FX_RED);
                    char outStr[100];
                    snprintf(outStr, sizeof(outStr), "FAILED Memory test at address %04x - check memory card\n", rslt);
                    display.consolePut(outStr);
                    display.consoleForeground(DISPLAY_FX_WHITE);
                    issueCount++;
                    blockTestFailed = true;
                }

                if (!blockTestFailed)
                {
                    rslt = memTestAddressBus(busAccess, blockStart, blockLen);
                    if (rslt != 0)
                    {
                        display.consoleForeground(DISPLAY_FX_RED);
                        char outStr[100];
                        snprintf(outStr, sizeof(outStr), "FAILED Address bus test at address %04x - check for shorts\n", rslt);
                        display.consolePut(outStr);
                        display.consoleForeground(DISPLAY_FX_WHITE);
                        issueCount++;
                        blockTestFailed = true;
                    }

                    rslt = memTestDataBus(busAccess, blockStart);
                    if (rslt != 0)
                    {
                        display.consoleForeground(DISPLAY_FX_RED);
                        char outStr[100];
                        snprintf(outStr, sizeof(outStr), "FAILED Data bus test on data %02x - check for shorts\n", rslt);
                        display.consolePut(outStr);
                        display.consoleForeground(DISPLAY_FX_WHITE);
                        issueCount++;
                        blockTestFailed = true;
                    }
                }
                if (!blockTestFailed)
                {
                    display.consoleForeground(DISPLAY_FX_GREEN);
                    display.consolePut("Test Passed\n");
                    display.consoleForeground(DISPLAY_FX_WHITE);
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
                    display.consoleForeground(DISPLAY_FX_GREEN);
                    display.consolePut("Tests complete no issues found");
                    display.consoleForeground(DISPLAY_FX_WHITE);
                }
                else
                {
                    display.consoleForeground(DISPLAY_FX_RED);
                    char issueText[100];
                    snprintf(issueText, sizeof(issueText), "Tests complete %d issue%s found",
                                    issueCount, issueCount > 1 ? "s" : "");
                    display.consolePut(issueText);
                    display.consoleForeground(DISPLAY_FX_WHITE);
                }
                return;
            }
        }
    }

}

