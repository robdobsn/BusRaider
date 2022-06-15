// Bus Raider
// Rob Dobson 2019

#include "BusRaiderApp.h"
#include "Display.h"
#include "PiWiring.h"
#include "lowlib.h"
#include "rdutils.h"
#include "BusAccess.h"
#include "timer.h"
#include "Machines/McTerminal.h"
#include "memoryTests.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Self test - bits test
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusRaiderApp::testSelf_busBits()
{
    _display.consoleForeground(DISPLAY_FX_BLUE);
    _display.consolePut("\nBits test\n");
    _display.consoleForeground(DISPLAY_FX_WHITE);
    _display.consolePut("This test works with a logic analyzer, meter or a monitor\n");
    _display.consolePut("board like the Z80 Bus Monitor Board by Jack (search on Tindie)\n");
    _display.consolePut("Make sure no other cards are plugged into the bus\n");
    _display.consolePut("Ready to start test? (y/n) ...");

    enum testState_type
    {
        TEST_STATE_SET_MC,
        TEST_STATE_ASK_IF_READY,
        TEST_STATE_SET_BITS,
        TEST_STATE_AWAIT_KEY,
        TEST_STATE_DONE
    } testState = TEST_STATE_SET_MC;

    enum testBus_type {
        TEST_BUS_ADDR,
        TEST_BUS_DATA
    };

    struct testSequence_type
    {
        testBus_type testBitType;
        int modifier;
        const char* msg;
        uint32_t val;
    } testSequence[] =
    {
        { TEST_BUS_ADDR, 0, "ADDR %04x", 0x0 },
        { TEST_BUS_ADDR, 0, "ADDR %04x", 0x1 },
        { TEST_BUS_ADDR, 0, "ADDR %04x", 0x2 },
        { TEST_BUS_ADDR, 0, "ADDR %04x", 0x4 },
        { TEST_BUS_ADDR, 0, "ADDR %04x", 0x8 },
        { TEST_BUS_ADDR, 0, "ADDR %04x", 0x10 },
        { TEST_BUS_ADDR, 0, "ADDR %04x", 0x20 },
        { TEST_BUS_ADDR, 0, "ADDR %04x", 0x40 },
        { TEST_BUS_ADDR, 0, "ADDR %04x", 0x80 },
        { TEST_BUS_ADDR, 0, "ADDR %04x", 0x100 },
        { TEST_BUS_ADDR, 0, "ADDR %04x", 0x200 },
        { TEST_BUS_ADDR, 0, "ADDR %04x", 0x400 },
        { TEST_BUS_ADDR, 0, "ADDR %04x", 0x800 },
        { TEST_BUS_ADDR, 0, "ADDR %04x", 0x1000 },
        { TEST_BUS_ADDR, 0, "ADDR %04x", 0x2000 },
        { TEST_BUS_ADDR, 0, "ADDR %04x", 0x4000 },
        { TEST_BUS_ADDR, 0, "ADDR %04x", 0x8000 },
        { TEST_BUS_ADDR, 0, "ADDR %04x", 0x5555 },
        { TEST_BUS_ADDR, 0, "ADDR %04x", 0xAAAA },
        { TEST_BUS_DATA, 0, "DATA %02x", 0x0 },
        { TEST_BUS_DATA, 0, "DATA %02x", 0x1 },
        { TEST_BUS_DATA, 0, "DATA %02x", 0x2 },
        { TEST_BUS_DATA, 0, "DATA %02x", 0x4 },
        { TEST_BUS_DATA, 0, "DATA %02x", 0x8 },
        { TEST_BUS_DATA, 0, "DATA %02x", 0x10 },
        { TEST_BUS_DATA, 0, "DATA %02x", 0x20 },
        { TEST_BUS_DATA, 0, "DATA %02x", 0x40 },
        { TEST_BUS_DATA, 0, "DATA %02x", 0x80 },
        { TEST_BUS_DATA, 0, "DATA %02x", 0x55 },
        { TEST_BUS_DATA, 0, "DATA %02x", 0xAA },
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
                bool mcSet = McManager::setMachineByName("Serial Terminal ANSI");
                if (!mcSet)
                {
                    _display.consoleForeground(DISPLAY_FX_RED);
                    _display.consolePut("FAILED to set machine Serial Terminal ANSI - maybe a name change?\n");
                    _display.consoleForeground(DISPLAY_FX_WHITE);
                    issueCount++;
                }
                testState = TEST_STATE_ASK_IF_READY;
                break;
            }
            case TEST_STATE_ASK_IF_READY:
            {
                // 
                if (rdtolower(_testSelf_curKeyAscii) == 'y')
                {
                    _display.consolePut("\n");
                    _testSelf_startUpdateTimeMs = millis();
                    testState = TEST_STATE_SET_BITS;

                    // Set bus outputs
                    pinMode(BR_BUSACK_BAR, OUTPUT);
                    digitalWrite(BR_BUSACK_BAR, 0);
                    digitalWrite(BR_BUSRQ_BAR, 0);
                    BusAccess::rawBusControlMuxSet(BR_MUX_DATA_OE_BAR_LOW);
                    microsDelay(1);
                    BusAccess::rawBusControlMuxClear();

                } else if (rdtolower(_testSelf_curKeyAscii) == 'n')
                {
                    _display.consolePut("\n");
                    _testSelf_startUpdateTimeMs = millis();
                    testState = TEST_STATE_DONE;
                }
                break;
            }
            case TEST_STATE_SET_BITS:
            {
                if (testSeqIdx >= sizeof(testSequence)/sizeof(testSequence[0]))
                {
                    testState = TEST_STATE_DONE;
                }
                else
                {
                    switch(testSequence[testSeqIdx].testBitType)
                    {
                        case TEST_BUS_ADDR:
                        {
                            BusAccess::rawBusControlSetAddress(testSequence[testSeqIdx].val);
                            break;
                        }
                        case TEST_BUS_DATA:
                        {
                            BusAccess::rawBusControlSetData(testSequence[testSeqIdx].val);
                            break;
                        }
                    }

                    // Next test
                    char testStr[100];
                    snprintf(testStr, sizeof(testStr), testSequence[testSeqIdx].msg, testSequence[testSeqIdx].val);
                    _display.consolePut("Test set: ");
                    _display.consolePut(testStr);
                    _display.consolePut(", SPACE or Q(uit) ...");
                    testSeqIdx++;
                    testState = TEST_STATE_AWAIT_KEY;
                }
                break;
            }
            case TEST_STATE_AWAIT_KEY:
            {
                // Key test
                if (rdtolower(_testSelf_curKeyAscii) == ' ')
                {
                    _display.consolePut("\n");
                    _testSelf_startUpdateTimeMs = millis();
                    testState = TEST_STATE_SET_BITS;
                }
                break;
            }
            case TEST_STATE_DONE:
            {
                // Clear bus outputs
                pinMode(BR_BUSACK_BAR, INPUT);
                digitalWrite(BR_BUSRQ_BAR, 1);
                BusAccess::rawBusControlMuxClear();
                _display.consolePut("Tests complete");
                return;
            }
        }
    }
}

