// Bus Raider
// Rob Dobson 2019

#include "BusRaiderApp.h"
#include "Display.h"
#include "PiWiring.h"
#include "lowlib.h"
#include "rdutils.h"
#include "BusAccess.h"
#include "timer.h"
#include "McTerminal.h"
#include "memoryTests.h"

// Log string
static const char* FromBusRaiderTests = "BusRaiderTests";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Self test - detailed bus
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusRaiderApp::testSelf_detailedBus()
{
    _display.consoleForeground(DISPLAY_FX_BLUE);
    _display.consolePut("\nDetailed bus test\n");
    _display.consoleForeground(DISPLAY_FX_WHITE);
    _display.consolePut("\nThis test requires a male-male jumper wire but\n");
    _display.consolePut("but don't plug anything into the bus until asked to do so\n");
    _display.consolePut("Install ONLY BusRaider (and logic analyzer etc) ...\n");
    _display.consolePut("Ready to start test? (y/n) ... ");

    enum testState_type
    {
        TEST_STATE_CONFIRM,
        TEST_STATE_SET_MC,
        TEST_STATE_TEST_SEQUENCE,
        TEST_STATE_TEST_WAIT_FOR_KEY,
        TEST_STATE_PREPPED,
        TEST_STATE_TEST_BUS_PIN,
        TEST_STATE_TEST_PIN_TO_PIN,
        TEST_STATE_TEST_PIN_TO_MUX,
        TEST_STATE_TEST_ADDR,
        TEST_STATE_TEST_FAILED,
        TEST_STATE_TEST_OK,
        TEST_STATE_NEXT,
        TEST_STATE_DONE
    } testState = TEST_STATE_CONFIRM;

    enum testOption_type
    {
        TEST_OPT_PIN_FIXED,
        TEST_OPT_PIN_TO_PIN,
        TEST_OPT_PIN_TO_MUX,
        TEST_OPT_ADDR
    };

    struct testSequence_type
    {
        const char* testPinName;
        testOption_type testOption;
        const char* msgBeforeTest;
        int pinToTest;
        int pinMode;
        int levelToCheckFor;
        bool continueOnFail;
        const char* msgIfFailed;
        const char* msgIfSucceeded;
    } testSequence[] =
    {
        // { "BUSACK", TEST_OPT_PIN_FIXED, "", BR_BUSACK_BAR, OUTPUT, 0, false, "FAILED: BUSACK reads LOW - check Q4/Q5 for shorts", "" },
        { "BUSACK", TEST_OPT_PIN_FIXED, "", BR_BUSACK_BAR, INPUT_PULLUP, 1, false, "FAILED: BUSACK reads LOW - check Q4/Q5 for shorts", "" },
        { "BUSACK", TEST_OPT_PIN_FIXED, "Place a jumper: BUSACK to GND", BR_BUSACK_BAR, -1, 0, false, "FAILED: BUSACK reads HIGH - check Q4/Q5 for shorts", "BUSACK looks OK" },
        { "RESET", TEST_OPT_PIN_TO_MUX, "Place the same jumper: BUSACK to %s", BR_BUSACK_BAR, INPUT_PULLUP, BR_MUX_RESET_Z80_BAR_LOW, true, "FAILED: %s not working check U5 & Q1", "%s looks OK" },
        { "INT", TEST_OPT_PIN_TO_MUX, "Place the same jumper: BUSACK to %s", BR_BUSACK_BAR, INPUT_PULLUP, BR_MUX_IRQ_BAR_LOW, true, "FAILED: %s not working check U5 & Q2", "%s looks OK" },
        { "NMI", TEST_OPT_PIN_TO_MUX, "Place the same jumper: BUSACK to %s", BR_BUSACK_BAR, INPUT_PULLUP, BR_MUX_NMI_BAR_LOW, true, "FAILED: %s not working check U5 & Q3 & NMI JUMPER", "%s looks OK" },
        { "MREQ", TEST_OPT_PIN_TO_PIN, "Place the same jumper: BUSACK to %s", BR_BUSACK_BAR, OUTPUT, BR_MREQ_BAR, true, "FAILED: %s not working check U1", "%s looks OK" },
        { "WR", TEST_OPT_PIN_TO_PIN, "Place the same jumper: BUSACK to %s", BR_BUSACK_BAR, OUTPUT, BR_WR_BAR, true, "FAILED: %s not working check U1", "%s looks OK" },
        { "RD", TEST_OPT_PIN_TO_PIN, "Place the same jumper: BUSACK to %s", BR_BUSACK_BAR, OUTPUT, BR_RD_BAR, true, "FAILED: %s not working check U1", "%s looks OK" },
        { "IORQ", TEST_OPT_PIN_TO_PIN, "Place the same jumper: BUSACK to %s", BR_BUSACK_BAR, OUTPUT, BR_IORQ_BAR, true, "FAILED: %s not working check U1", "%s looks OK" },
        { "ADDR", TEST_OPT_ADDR, "Next test address bus, remove jumper", BR_BUSACK_BAR, -1, 0, true, "", "Address bus is OK" },
    };

    _testSelf_startUpdateTimeMs = millis();

    int issueCount = 0;
    uint32_t testSeqIdx = 0;
    while (1)
    {
        // Loop to service keyboard and bus access
        if (testSelf_commonLoop() != TEST_SELF_RET_OK)
            testState = TEST_STATE_DONE;

        // Check state
        testSequence_type* pTestRec = &testSequence[testSeqIdx];
        switch(testState)
        {
            case TEST_STATE_CONFIRM:
            {
                if (rdtolower(_testSelf_curKeyAscii) == 'y')
                {
                    _display.consolePut("\n");
                    testState = TEST_STATE_SET_MC;
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
                testState = TEST_STATE_TEST_SEQUENCE;
                testSeqIdx = 0;
                _testSelf_startUpdateTimeMs = millis();
                break;
            }
            case TEST_STATE_TEST_SEQUENCE:
            {
                if (strlen(pTestRec->msgBeforeTest) != 0)
                {
                    char outMsg[200];
                    snprintf(outMsg, sizeof(outMsg), pTestRec->msgBeforeTest, pTestRec->testPinName);
                    _display.consolePut(outMsg);
                    _display.consolePut(", then press SPACE BAR");
                    _testSelf_startUpdateTimeMs = millis();
                    testState = TEST_STATE_TEST_WAIT_FOR_KEY;
                }
                else
                {
                    testState = TEST_STATE_PREPPED;
                }
                break;
            }
            case TEST_STATE_TEST_WAIT_FOR_KEY:
            {
                if (_testSelf_curKeyAscii == ' ')
                {
                    _display.consolePut("\n");
                    _testSelf_startUpdateTimeMs = millis();
                    testState = TEST_STATE_PREPPED;
                }
                else if (rdtolower(_testSelf_curKeyAscii) == 's')
                {
                    _display.consolePut("\nSKIPPED\n");
                    _testSelf_startUpdateTimeMs = millis();
                    testState = TEST_STATE_NEXT;
                }
                break;
            }
            case TEST_STATE_PREPPED:
            {
                // Handle bus test option
                switch (pTestRec->testOption)
                {
                    case TEST_OPT_PIN_FIXED:
                        testState = TEST_STATE_TEST_BUS_PIN;
                        break;
                    case TEST_OPT_PIN_TO_PIN:
                        testState = TEST_STATE_TEST_PIN_TO_PIN;
                        break;
                    case TEST_OPT_PIN_TO_MUX:
                        testState = TEST_STATE_TEST_PIN_TO_MUX;
                        break;
                    case TEST_OPT_ADDR:
                        testState = TEST_STATE_TEST_ADDR;
                        break;
                    default:
                        testState = TEST_STATE_DONE;
                        break;
                }
                break;
            }
            case TEST_STATE_TEST_BUS_PIN:
            {
                // Set pinMode
                if (pTestRec->pinMode >= 0)
                {
                    pinMode(pTestRec->pinToTest, pTestRec->pinMode);
                    if (pTestRec->pinMode == OUTPUT)
                        digitalWrite(pTestRec->pinToTest, pTestRec->levelToCheckFor);
                }

                // Check pin
                testState = TEST_STATE_TEST_OK;
                if (pTestRec->pinMode != OUTPUT)
                {
                    bool pinLevel = digitalRead(pTestRec->pinToTest);
                    if (pinLevel != pTestRec->levelToCheckFor)
                        testState = TEST_STATE_TEST_FAILED;
                }
                break;
            }
            case TEST_STATE_TEST_PIN_TO_PIN:
            {
                // Set pinModes
                if (pTestRec->pinMode >= 0)
                    pinMode(pTestRec->pinToTest, pTestRec->pinMode);

                // Check pin can go low
                testState = TEST_STATE_TEST_OK;
                pinMode(pTestRec->levelToCheckFor, INPUT_PULLUP);
                digitalWrite(pTestRec->pinToTest, 0);
                microsDelay(500000);
                if (digitalRead(pTestRec->levelToCheckFor) != 0)
                    testState = TEST_STATE_TEST_FAILED;

                // Check pin can go high
                pinMode(pTestRec->levelToCheckFor, INPUT_PULLDOWN);
                digitalWrite(pTestRec->pinToTest, 1);
                microsDelay(500000);
                if (digitalRead(pTestRec->levelToCheckFor) == 0)
                    testState = TEST_STATE_TEST_FAILED;

                // Restore
                pinMode(pTestRec->levelToCheckFor, INPUT);
                break;
            }
            case TEST_STATE_TEST_PIN_TO_MUX:
            {
                // Set pinMode used for reading
                if (pTestRec->pinMode >= 0)
                {
                    pinMode(pTestRec->pinToTest, pTestRec->pinMode);
                }

                // Clear mux
                BusAccess::rawBusControlMuxClear();

                // Check pin is high due to input pullup
                microsDelay(100);
                testState = TEST_STATE_TEST_OK;
                bool pinLevel = digitalRead(pTestRec->pinToTest);
                if (pinLevel == 0)
                {
                    testState = TEST_STATE_TEST_FAILED;
                }

                // Set mux
                BusAccess::rawBusControlMuxSet(pTestRec->levelToCheckFor);

                // Check pin is low due to mux signal
                microsDelay(100);
                pinLevel = digitalRead(pTestRec->pinToTest);
                if (pinLevel != 0)
                {
                    testState = TEST_STATE_TEST_FAILED;
                }

                // Clear mux
                BusAccess::rawBusControlMuxClear();
                // Restore
                pinMode(pTestRec->pinToTest, INPUT);
                break;
            }
            case TEST_STATE_TEST_ADDR:
            {
                if (testSelf_detailedBus_addr())
                    testState = TEST_STATE_TEST_OK;
                else
                    testState = TEST_STATE_TEST_FAILED;
                break;
            }
            case TEST_STATE_TEST_FAILED:
            {
                char outMsg[200];
                _display.consoleForeground(DISPLAY_FX_RED);
                snprintf(outMsg, sizeof(outMsg), pTestRec->msgIfFailed, pTestRec->testPinName);
                _display.consolePut(outMsg);
                _display.consolePut("\n");
                _display.consoleForeground(DISPLAY_FX_WHITE);
                issueCount++;
                if (pTestRec->testOption == TEST_STATE_TEST_PIN_TO_MUX)
                {
                    snprintf(outMsg, sizeof(outMsg), "Pin %s should oscillate at 2Hz for 5 minutes", pTestRec->testPinName);
                    _display.consolePut(outMsg);
                    _display.consolePut("\n");
                    for (int i = 0; i < 600)
                    {
                        if (i % 2 == 0)
                        {
                            BusAccess::rawBusControlMuxSet(pTestRec->levelToCheckFor);
                        }
                        else
                        {
                            BusAccess::rawBusControlMuxClear();
                        }
                        delayMicroseconds(500000);
                    }
                    break;
                }
                if (!pTestRec->continueOnFail)
                {
                    _display.consoleForeground(DISPLAY_FX_RED);
                    _display.consolePut("This issue needs to be resolved before testing again\n");
                    _display.consoleForeground(DISPLAY_FX_WHITE);
                    testState = TEST_STATE_DONE;
                }
                else
                {
                    testState = TEST_STATE_NEXT;
                }
                break;
            }
            case TEST_STATE_TEST_OK:
            {
                char outMsg[200];
                if (strlen(pTestRec->msgIfSucceeded) > 0)
                {
                    _display.consoleForeground(DISPLAY_FX_GREEN);
                    snprintf(outMsg, sizeof(outMsg), pTestRec->msgIfSucceeded, pTestRec->testPinName);
                    _display.consolePut(outMsg);
                    _display.consolePut("\n");
                    _display.consoleForeground(DISPLAY_FX_WHITE);
                }
                testState = TEST_STATE_NEXT;
                break;
            }
            case TEST_STATE_NEXT:
            {
                // Next test
                testSeqIdx++;
                if (testSeqIdx >= sizeof(testSequence)/sizeof(testSequence[0]))
                    testState = TEST_STATE_DONE;
                else
                    testState = TEST_STATE_TEST_SEQUENCE;
                break;
            }
            case TEST_STATE_DONE:
            {
                pinMode(BR_BUSRQ_BAR, OUTPUT);
                digitalWrite(BR_BUSRQ_BAR, 1);
                pinMode(BR_BUSACK_BAR, INPUT);
                pinMode(BR_WR_BAR, INPUT);
                pinMode(BR_RD_BAR, INPUT);
                pinMode(BR_MREQ_BAR, INPUT);
                pinMode(BR_IORQ_BAR, INPUT);
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
                    snprintf(issueText, sizeof(issueText), "Tests complete %d issue%s found",
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
// 
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool BusRaiderApp::testSelf_detailedBus_addr()
{
    // pull BUSRQ and BUSACK low
    pinMode(BR_BUSACK_BAR, OUTPUT);
    digitalWrite(BR_BUSACK_BAR, 0);
    digitalWrite(BR_BUSRQ_BAR, 0);
    digitalWrite(BR_DATA_DIR_IN, 0);
    uint32_t addrTestMask = 1;
    uint32_t addrStuckHighMask = 0;
    uint32_t addrStuckLowMask = 0;
    uint32_t addrInteractionMask = 0;
    for (int i = 0; i < 16; i++)
    {
        BusAccess::rawBusControlSetAddress(addrTestMask);
        microsDelay(200000);

        // Data direction in before getting state of address bus
        digitalWrite(BR_DATA_DIR_IN, 0);

        // Debug
        uint32_t vv = 0;
        // BusAccess::rawBusControlMuxSet(BR_MUX_LADDR_OE_BAR);
        // // vv = BusAccess::rawBusControlReadPIB();
        // WR32(BR_PIB_GPF_REG, (RD32(BR_PIB_GPF_REG) & BR_PIB_GPF_MASK) | BR_PIB_GPF_INPUT);
        // vv = (RD32(ARM_GPIO_GPLEV0) >> BR_DATA_BUS) & 0xff;

        // Get the address and data bus
        uint32_t addr = 0, data = 0, ctrl = 0;
        BusAccess::rawBusControlReadAll(ctrl, addr, data);

        // Debug
        char ctrlStr[100];
        BusAccess::formatCtrlBus(ctrl, ctrlStr, 20);
        char testStr[100];
        snprintf(testStr, sizeof(testStr), "Awr %04x Ard %04x Ard2 %02x data %02x ctrl %s\n", addrTestMask, addr, vv, data, ctrlStr);
        LogWrite(FromBusRaiderTests, LOG_DEBUG, "%s", testStr);

        // Check correct
        if (addrTestMask != addr)
        {
            if ((addrTestMask & addr) == 0)
                addrStuckLowMask |= addrTestMask;
            else
                addrInteractionMask |= addrTestMask;
        }

        // Now set inverse of every address bit
        uint32_t inverseTestMask = (~addrTestMask & 0xffff);
        BusAccess::rawBusControlSetAddress(inverseTestMask);

        // Data direction in before getting state of address bus
        digitalWrite(BR_DATA_DIR_IN, 0);

        // Read address
        BusAccess::rawBusControlReadAll(ctrl, addr, data);

        // Debug
        BusAccess::formatCtrlBus(ctrl, ctrlStr, 20);
        snprintf(testStr, sizeof(testStr), "Awr %04x Ard %04x Ard2 %02x data %02x ctrl %s\n", inverseTestMask, addr, vv, data, ctrlStr);
        LogWrite(FromBusRaiderTests, LOG_DEBUG, "%s", testStr);

        // Check correct
        if (inverseTestMask != addr)
        {
            if ((inverseTestMask | addr) == 1)
                addrStuckHighMask |= addrTestMask;
            else
                addrInteractionMask |= addrTestMask;
        }
        // if ((~addrTestMask) != addr)
        // {
        //     addrFailMask |= addrTestMask;
        //     addrLevelMask |= (addr & addrTestMask);
        // }
        addrTestMask = addrTestMask << 1;
    }

    // Result
    bool rsltOk = (addrStuckHighMask | addrStuckLowMask | addrInteractionMask) == 0;
    if (!rsltOk)
    {
        char stuckHighStr[100];
        stuckHighStr[0] = 0;
        char stuckLowStr[100];
        stuckLowStr[0] = 0;
        char interactStr[100];
        interactStr[0] = 0;
        uint32_t addrTestMask = 0x8000;                
        for (int i = 0; i < 16; i++)
        {
            char tmpStr[10];
            snprintf(tmpStr, sizeof(tmpStr), "%d", i);
            if (addrStuckHighMask & addrTestMask)
            {
                if (strlen(stuckHighStr) > 0)
                    strlcat(stuckHighStr, ", ", 100);
                strlcat(stuckHighStr, tmpStr, 100);
            }
            else if (addrStuckLowMask & addrTestMask)
            {
                if (strlen(stuckLowStr) > 0)
                    strlcat(stuckLowStr, ", ", 100);
                strlcat(stuckLowStr, tmpStr, 100);
            }
            else
            {
                if (strlen(interactStr) > 0)
                    strlcat(interactStr, ", ", 100);
                strlcat(interactStr, tmpStr, 100);
            }
            addrTestMask = addrTestMask >> 1;
        }
        if (strlen(stuckHighStr) > 0)
        {
            _display.consolePut("FAILED Address bus lines stuck HIGH: ");
            _display.consolePut(stuckHighStr);
        }
        if (strlen(stuckLowStr) > 0)
        {
            _display.consolePut("FAILED Address bus lines stuck LOW: ");
            _display.consolePut(stuckLowStr);
        }
        if (strlen(interactStr) > 0)
        {
            _display.consolePut("FAILED Address bus lines bridged: ");
            _display.consolePut(interactStr);
        }
        _display.consolePut("\n");
    }
    digitalWrite(BR_BUSRQ_BAR, 1);
    return rsltOk;
}