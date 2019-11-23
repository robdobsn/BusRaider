// Bus Raider
// Rob Dobson 2019

#include "BusRaiderApp.h"
#include "System/OTAUpdate.h"
#include "System/Display.h"
#include "System/UartMaxi.h"
#include "System/lowlib.h"
#include "System/rdutils.h"
#include "System/timer.h"
#include "System/ee_sprintf.h"
#include "System/PiWiring.h"
#include "Machines/McManager.h"
#include "Machines/usb_hid_keys.h"
#include "Machines/McTerminal.h"
#include "TargetBus/TargetTracker.h"
typedef unsigned char		u8;
#include "../uspi/include/uspi.h"
#include "System/memoryTests.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Vars
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Log string
const char* FromBusRaiderApp = "BusRaiderApp";

BusRaiderApp* BusRaiderApp::_pApp = NULL;

BusRaiderApp::KeyInfo BusRaiderApp::_keyInfoBuffer[MAX_USB_KEYS_BUFFERED];
RingBufferPosn BusRaiderApp::_keyInfoBufferPos(BusRaiderApp::MAX_USB_KEYS_BUFFERED);
bool BusRaiderApp::_inKeyboardRoutine = false;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Comms socket
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Main comms socket - to wire up OTA updates
CommsSocketInfo BusRaiderApp::_commsSocket =
{
    true,
    BusRaiderApp::handleRxMsg,
    OTAUpdate::performUpdate,
    NULL
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Init
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BusRaiderApp::BusRaiderApp(Display& display, UartMaxi& uart) :
    _display(display), _uart(uart)
{
    _pApp = this;
    clear();
}

void BusRaiderApp::init()
{
    clear();

    // Command Handler
    _commandHandler.setPutToSerialCallback(serialPutStr, serialTxAvailable);
    _commandHandler.commsSocketAdd(_commsSocket);

}

void BusRaiderApp::clear()
{
    // General vars
    _statusUpdateStartUs = 0;
    _immediateMode = false;
    _immediateModeLineLen = 0;
    _esp32StatusUpdateStartUs = 0;
    _piStatusCached[0] = 0;
    _lastActivityTickerState = false;

    // Esp32 status
    _esp32IPAddress[0] = 0;
    _esp32WifiConnStr[0] = 0;
    _esp32WifiSSID[0] = 0;
    _esp32ESP32Version[0] = 0;
    _esp32IPAddressValid = false;
    _esp32StatusLastRxUs = 0;
    _esp32LastMachineCmd[0] = 0;
    _esp32LastMachineValid = false;
    _esp32LastMachineReqUs = 0;
}
void BusRaiderApp::initUSB()
{
    // USB
    if (USPiInitialize()) 
    {
        // LogWrite(FromBusRaiderApp, LOG_DEBUG, "Checking for keyboards...");

        if (USPiKeyboardAvailable()) 
        {
            USPiKeyboardRegisterKeyStatusHandlerRaw(usbKeypressHandlerStatic);
            _display.statusPut(Display::STATUS_FIELD_KEYBOARD, Display::STATUS_HILITE, "Keyboard OK, F2 for Settings");
        } 
        else 
        {
            _display.statusPut(Display::STATUS_FIELD_KEYBOARD, Display::STATUS_FAIL, "Keyboard Not Found");
        }
    } else 
    {
        _display.statusPut(Display::STATUS_FIELD_KEYBOARD, Display::STATUS_NORMAL, "USB Init Fail - No Keyboard");
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusRaiderApp::service()
{
    // Handle status update to ESP32
    if (isTimeout(micros(), _esp32StatusUpdateStartUs, PI_TO_ESP32_STATUS_UPDATE_RATE_MS * 1000)) 
    {
        // Send status update
        if (!_commandHandler.isFileTransferInProgress())
        {
            // Get Pi status
            char respBuffer[MAX_PI_STATUS_MSG_LEN];
            getPiStatus(respBuffer, MAX_PI_STATUS_MSG_LEN);

            // Check if changed
            if (strcmp(respBuffer, _piStatusCached) != 0)
            {
                strlcpy(_piStatusCached, respBuffer, MAX_PI_STATUS_MSG_LEN);
                // Send update
                _commandHandler.sendWithJSON("getStatusResp", respBuffer);
            }

            // Time of last check
            _esp32StatusUpdateStartUs = micros();
        }
    }

    // Handle status update from ESP32
    if (isTimeout(micros(), _esp32StatusLastRxUs, ESP32_TO_PI_STATUS_UPDATE_MAX_MS * 1000))
    {
        // Request status update
        if (!_commandHandler.isFileTransferInProgress())
        {
            _commandHandler.sendAPIReq("queryESPHealth");
            // LogWrite(FromBusRaiderApp, LOG_DEBUG, "Req ESP32 status as not received recently");
        }
        _esp32StatusLastRxUs = micros();
    }

    // Handle initial get of ESP32 last machine info
    if (!_esp32LastMachineValid)
    {
        if (isTimeout(micros(), _esp32LastMachineReqUs, ESP32_LAST_MC_REQ_MS * 1000))
        {
            _commandHandler.sendAPIReq("querycurmc");
            _esp32LastMachineReqUs = micros();
        }
    }

    // Pump the characters from the UART to Command Handler
    serviceGetFromSerial();

    // Display status update
    statusDisplayUpdate();

    // Service commmand handler
    _commandHandler.service();

    // Service keyboard
    if (!_inKeyboardRoutine)
    {
        // LogWrite(FromBusRaiderApp, LOG_DEBUG, "Keys in buffer %d", _keyInfoBufferPos.count());
        if (_keyInfoBufferPos.canGet())
        {
            KeyInfo* pKeyInfo = &_keyInfoBuffer[_keyInfoBufferPos.posToGet()];
            _keyInfoBufferPos.hasGot();
            // LogWrite(FromBusRaiderApp, LOG_DEBUG, "Keyattop %02x %02x %02x", pKeyInfo->rawKeys[0], pKeyInfo->rawKeys[1], pKeyInfo->rawKeys[2]);
            _inKeyboardRoutine = true;
            if (_pApp)
                _pApp->usbKeypressHandler(pKeyInfo->modifiers, pKeyInfo->rawKeys);
            _inKeyboardRoutine = false;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Received message handler
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool BusRaiderApp::handleRxMsg(const char* pCmdJson, [[maybe_unused]]const uint8_t* pParams, [[maybe_unused]]int paramsLen,
                char* pRespJson, int maxRespLen)
{
    // LogWrite(FromBusRaiderApp, LOG_DEBUG, "rxMsg %s", pCmdJson);
    #define MAX_CMD_NAME_STR 200
    char cmdName[MAX_CMD_NAME_STR+1];
    if (!jsonGetValueForKey("cmdName", pCmdJson, cmdName, MAX_CMD_NAME_STR))
        return false;

    pRespJson[0] = 0;

    if (strcasecmp(cmdName, "getStatus") == 0)
    {
        // Return the status of the Pi app
        _pApp->getPiStatus(pRespJson, maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "queryESPHealthResp") == 0)
    {
        // Store ESP32 status info
        _pApp->storeESP32StatusInfo(pCmdJson);
        return true;
    }
    else if (strcasecmp(cmdName, "queryCurMcResp") == 0)
    {
        // Set machine
        LogWrite(FromBusRaiderApp, LOG_DEBUG, "queryCurMcResp %s", pCmdJson);
        McManager::setupMachine(pCmdJson);
        // Got ok - no need to re-request
        _pApp->_esp32LastMachineValid = true;
        return true;
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Status Display
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static inline int locmin ( int a, int b ) { return a < b ? a : b; }
void BusRaiderApp::statusDisplayUpdate()
{
    // Update status
    if (isTimeout(micros(), _statusUpdateStartUs, STATUS_UPDATE_TIME_MS * 1000)) 
    {
        // Ready for next time
        _statusUpdateStartUs = micros();

        // ESP32 info
        const int MAX_STATUS_STR_LEN = 50;
        char statusStr[MAX_STATUS_STR_LEN+1];
        strlcpy(statusStr, "ESP32 Version: ", MAX_STATUS_STR_LEN);
        if (strlen(_esp32ESP32Version) == 0)
        {
            strlcat(statusStr, "Not Connected!        ", MAX_STATUS_STR_LEN);
            _display.statusPut(Display::STATUS_FIELD_ESP_VERSION, Display::STATUS_FAIL, statusStr);
        }
        else
        {
            strlcat(statusStr, _esp32ESP32Version, MAX_STATUS_STR_LEN);
            strlcat(statusStr, BusAccess::getHwVersion() == 17 ? " (HW V1.7)" : " (HW V2.0)", MAX_STATUS_STR_LEN);
            strlcat(statusStr, "        ", MAX_STATUS_STR_LEN);
            _display.statusPut(Display::STATUS_FIELD_ESP_VERSION, Display::STATUS_NORMAL, statusStr);
        }

        // WiFi status
        statusStr[0] = 0;
        switch(*_esp32WifiConnStr)
        {
            case 'C': 
                strlcpy(statusStr, "WiFi IP: ", MAX_STATUS_STR_LEN); 
                if (_esp32IPAddressValid)
                    strlcat(statusStr, _esp32IPAddress, MAX_STATUS_STR_LEN);
                strlcat(statusStr, "        ", MAX_STATUS_STR_LEN);
                _display.statusPut(Display::STATUS_FIELD_IP_ADDR, Display::STATUS_NORMAL, statusStr);
                break;
            default: 
                strlcpy(statusStr, "WiFi not connected     ", MAX_STATUS_STR_LEN);
                _display.statusPut(Display::STATUS_FIELD_IP_ADDR, Display::STATUS_FAIL, statusStr);
                break;
        }

        // Machine name
        strlcpy(statusStr, "M/C: ", MAX_STATUS_STR_LEN);
        strlcat(statusStr, McManager::getMachineName(), MAX_STATUS_STR_LEN);
        strlcat(statusStr, "                       ", locmin(MAX_STATUS_STR_LEN, 40));
        _display.statusPut(Display::STATUS_FIELD_CUR_MACHINE, Display::STATUS_NORMAL, statusStr);

        // Speed
        int clockSpeed = McManager::getMachineClock();
        int mhz = clockSpeed/1000000;
        int khz = (clockSpeed - mhz*1000000)/1000;
        ee_sprintf(statusStr, "Clock: %d.%s%s%dMHz",
                    mhz,
                    khz <= 99 ? "0" : "",
                    khz <= 9 ? "0" : "",
                    khz);
        strlcat(statusStr, "    ", MAX_STATUS_STR_LEN);
        _display.statusPut(Display::STATUS_FIELD_MACHINES, Display::STATUS_NORMAL, statusStr);

        // BusAccess status
        statusStr[0] = 0;
        strlcpy(statusStr, "Bus: ", MAX_STATUS_STR_LEN);
        if (TargetTracker::isTrackingActive())
            strlcat(statusStr, "Debug       ", MAX_STATUS_STR_LEN);
        else if (BusAccess::waitIsHeld())
            strlcat(statusStr, "Paused      ", MAX_STATUS_STR_LEN);
        else
            strlcat(statusStr, "Free Running", MAX_STATUS_STR_LEN);
        if (BusAccess::isUnderControl())
            strlcat(statusStr, " & PiControl   ", MAX_STATUS_STR_LEN);
        _display.statusPut(Display::STATUS_FIELD_BUS_ACCESS, Display::STATUS_NORMAL, statusStr);

        // Refresh rate
        int refreshRate = McManager::getDisplayRefreshRate();
        const int MAX_REFRESH_STR_LEN = 40;
        const char* refreshText = "Refresh: ";
        char refreshStr[MAX_REFRESH_STR_LEN+1];
        strlcpy(refreshStr, _lastActivityTickerState ? "| " : "- ", MAX_REFRESH_STR_LEN);
        _lastActivityTickerState = !_lastActivityTickerState;
        strlcat(refreshStr, refreshText, MAX_REFRESH_STR_LEN);
        uint8_t rateStr[20];
        rditoa(refreshRate, rateStr, MAX_REFRESH_STR_LEN, 10);
        strlcat(refreshStr, (char*)rateStr, MAX_REFRESH_STR_LEN);
        strlcat(refreshStr, "fps     ", MAX_REFRESH_STR_LEN);
        _display.statusPut(Display::STATUS_FIELD_REFRESH_RATE, Display::STATUS_NORMAL, refreshStr);

        // Get ISR debug info
        strcpy(statusStr, "");
        int rxFramingErrCount = 0;
        int rxOverrunErrCount = 0;
        int rxBreakCount = 0;
        int rxBufferFullCount = 0;
        int txBufferFullCount = 0;
        _uart.getStatusCounts(txBufferFullCount, rxFramingErrCount, rxOverrunErrCount, rxBreakCount, rxBufferFullCount);
        if (txBufferFullCount > 0)
        {
            ee_sprintf(refreshStr, "TxFull %u,", txBufferFullCount);
            strlcat(statusStr, refreshStr, MAX_STATUS_STR_LEN);
        }
        if (rxFramingErrCount > 0)
        {
            ee_sprintf(refreshStr, "RxFr %u,", rxFramingErrCount);
            strlcat(statusStr, refreshStr, MAX_STATUS_STR_LEN);
        }
        if (rxOverrunErrCount > 0)
        {
            ee_sprintf(refreshStr, "RxOv %u,", rxOverrunErrCount);
            strlcat(statusStr, refreshStr, MAX_STATUS_STR_LEN);
        }
        // if (rxBreakCount > 0)
        // {
        //     ee_sprintf(refreshStr, "RxBk %u,", rxBreakCount);
        //     strlcat(statusStr, refreshStr, MAX_STATUS_STR_LEN);
        // }
        if (rxBufferFullCount > 0)
        {
            ee_sprintf(refreshStr, "RxFull %u,", rxBufferFullCount);
            strlcat(statusStr, refreshStr, MAX_STATUS_STR_LEN);
        }
        for (int i = 0; i < ISR_ASSERT_NUM_CODES; i++)
        {
            int cnt = BusAccess::isrAssertGetCount(i);
            if (cnt > 0)
            {
                ee_sprintf(refreshStr, "[%c]=%d,", i+'A'-1, cnt);
                strlcat(statusStr, refreshStr, MAX_STATUS_STR_LEN);
            }
        }
        strlcat(statusStr, "                                    ", MAX_STATUS_STR_LEN);
    
        _display.statusPut(Display::STATUS_FIELD_ASSERTS, Display::STATUS_FAIL, statusStr);

        // Get file receive status
        uint32_t fileLen = 0, filePos = 0;
        if (!_commandHandler.getFileReceiveStatus(fileLen, filePos))
            fileLen = 0;
        int progressBarLen = 0;
        const int MAX_FILE_PROGRESS_BAR_LEN = 40;
        if (fileLen > 0)
            progressBarLen = filePos*MAX_FILE_PROGRESS_BAR_LEN/fileLen;
        if (progressBarLen > 0)
        {
            const int MAX_FILE_STATUS_STR_LEN = MAX_FILE_PROGRESS_BAR_LEN + 20;
            char fileStatusStr[MAX_FILE_STATUS_STR_LEN+1];
            static const char* uploadStr = "Upload ["; 
            strlcpy(fileStatusStr, uploadStr, MAX_FILE_STATUS_STR_LEN);
            int stCurPos = strlen(fileStatusStr);
            for (int i = 0; i < MAX_FILE_PROGRESS_BAR_LEN; i++)
                fileStatusStr[i+stCurPos] = (i < progressBarLen) ? '#' : ' ';
            fileStatusStr[stCurPos+MAX_FILE_PROGRESS_BAR_LEN] = ']';
            fileStatusStr[stCurPos+MAX_FILE_PROGRESS_BAR_LEN+1] = 0;
            _display.statusPut(Display::STATUS_FIELD_FILE_STATUS, Display::STATUS_FAIL, fileStatusStr);
        }
        else
        {
            _display.statusPut(Display::STATUS_FIELD_FILE_STATUS, Display::STATUS_FAIL, "                                                  ");
        }

        // LogWrite(FromBusRaiderApp, LOG_DEBUG, "filelen %d filepos %d progbarlen %d dispStr %s", 
        //             fileLen, filePos, progressBarLen, fileStatusStr);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Callbacks
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Function to send to uart from command handler
void BusRaiderApp::serialPutStr(const uint8_t* pBuf, int len)
{
    for (int i = 0; i < len; i++)
        _pApp->_uart.write(pBuf[i]);
}

uint32_t BusRaiderApp::serialTxAvailable()
{
    return _pApp->_uart.txAvailable();
}

void BusRaiderApp::serviceGetFromSerial()
{
    // Handle serial communication
    for (int rxCtr = 0; rxCtr < 10000; rxCtr++) {
        if (!_pApp->_uart.poll())
            break;
        // Handle char
        int ch = _pApp->_uart.read();
        uint8_t buf[2];
        buf[0] = ch;
        CommandHandler::handleHDLCReceivedChars(buf, 1);
    }    
}

void BusRaiderApp::usbKeypressHandlerStatic(unsigned char ucModifiers, const unsigned char rawKeys[CommandHandler::NUM_USB_KEYS_PASSED])
{
    // Place in ring buffer
    if (_keyInfoBufferPos.canPut())
    {
        KeyInfo* pKeyInfo = (&_keyInfoBuffer[_keyInfoBufferPos.posToPut()]);
        for (int i = 0; i < CommandHandler::NUM_USB_KEYS_PASSED; i++)
            pKeyInfo->rawKeys[i] = rawKeys[i];
        pKeyInfo->modifiers = ucModifiers;
        _keyInfoBufferPos.hasPut();
        // ISR_ASSERT(ISR_ASSERT_CODE_DEBUG_C);
    }
}

void BusRaiderApp::usbKeypressHandler(unsigned char ucModifiers, const unsigned char rawKeys[CommandHandler::NUM_USB_KEYS_PASSED])
{
    // Check for immediate mode
    if (rawKeys[0] == KEY_F2)
    {
        if (!_immediateMode)
        {
            _display.consolePut("Immediate mode\n");
            _display.consolePut("* WiFi setup enter      ... w/ssid/password/hostname <enter>\n");
            _display.consolePut("* BusRaider board test  ... t <enter>\n");
            _display.consolePut("* Memory test           ... m <enter>\n");

            // _display.consolePut("To self-test (CPU) BUSRQ type ... q<enter>\n");
            // _display.consolePut("To show raw bus values type ... r<enter>\n");
            // _display.consolePut("To test the bus only (NO CPU OR memory) type ... b<enter>\n");
        }
        _immediateMode = true;
        return;
    }
    if (_immediateMode)
    {
        if (_immediateModeLineLen < IMM_MODE_LINE_MAXLEN)
        {
            int asciiCode = McTerminal::convertRawToAscii(ucModifiers, rawKeys);
            if (asciiCode < 0)
                return;
            if (asciiCode == 0x08)
            {
                if (_immediateModeLineLen > 0)
                    _immediateModeLineLen--;
                _display.consolePut(asciiCode);
                _display.consolePut(' ');
            }
            else if (asciiCode == 0x0d)
            {
                _immediateMode = false;
                _immediateModeLine[_immediateModeLineLen] = 0;
                if (_immediateModeLineLen > 0)
                {
                    // Check for test commands
                    if (rdtolower(_immediateModeLine[0]) == 't')
                    {
                        // Run detailed bus test
                        testSelf_detailedBus();
                    }
                    else if (rdtolower(_immediateModeLine[0]) == 'm')
                    {
                        // Run memory test
                        testSelf_memory();
                    }
                    else if ((rdtolower(_immediateModeLine[0]) == 'r') || (rdtolower(_immediateModeLine[0]) == 's'))
                    {
                        // Run self-tests
                        testSelf_readSetBus(rdtolower(_immediateModeLine[0]) == 'r');
                    }
                    else
                    {
                        // Send other commands to ESP32
                        CommandHandler::sendAPIReq(_immediateModeLine);
                        _display.consolePut("\nSent request to ESP32:");
                        _display.consolePut(_immediateModeLine);
                        _display.consolePut("\n");
                    }
                }
                _immediateModeLineLen = 0;
                _display.consolePut("\nNormal mode\n");
                _immediateMode = false;
            }
            else if ((asciiCode >= 32) && (asciiCode < 127))
            {
                _immediateModeLine[_immediateModeLineLen++] = asciiCode;
            }
            _display.consolePut(asciiCode);
        }
        return;
    }

    // Send to the target machine to process
    // LogWrite(FromBusRaiderApp, LOG_DEBUG, "KEY mod %02x raw %02x %02x %02x\n", ucModifiers, rawKeys[0], rawKeys[1], rawKeys[2]);
    McBase* pMc = McManager::getMachine();
    if (pMc)
        pMc->keyHandler(ucModifiers, rawKeys);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get Pi status
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusRaiderApp::getPiStatus(char* pRespJson, int maxRespLen)
{
    const char* mcJSON = McManager::getMachineJSON();
    if (pRespJson)
        strlcpy(pRespJson, mcJSON, maxRespLen);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Address callback
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusRaiderApp::storeESP32StatusInfo(const char* pCmdJson)
{
    _esp32StatusLastRxUs = micros();
    microsDelay(1000);
    // Get espHealth field
    char espHealthJson[MAX_ESP_HEALTH_STR];
    jsonGetValueForKey("espHealth", pCmdJson, espHealthJson, MAX_ESP_HEALTH_STR);
    jsonGetValueForKey("wifiIP", espHealthJson, _esp32IPAddress, MAX_IP_ADDR_STR);
    _esp32IPAddressValid = (strcmp(_esp32IPAddress, "0.0.0.0") != 0);
    jsonGetValueForKey("wifiConn", espHealthJson, _esp32WifiConnStr, MAX_WIFI_CONN_STR);
    jsonGetValueForKey("ssid", espHealthJson, _esp32WifiSSID, MAX_WIFI_SSID_STR);
    jsonGetValueForKey("espV", espHealthJson, _esp32ESP32Version, MAX_ESP_VERSION_STR);
    char espHwVersStr[MAX_ESP_VERSION_STR];
    espHwVersStr[0] = '\0';
    int espHwVersion = BusAccess::HW_VERSION_DEFAULT;
    jsonGetValueForKey("espHWV", espHealthJson, espHwVersStr, MAX_ESP_VERSION_STR);
    if (strlen(espHwVersStr) != 0)
        espHwVersion = atoi(espHwVersStr);
    BusAccess::setHwVersion(espHwVersion);
    // LogWrite(FromBusRaiderApp, LOG_DEBUG, "Ip Address %s", _esp32IPAddress);
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
        // Service the comms channels and display updates
        service();
        // Timer polling
        timer_poll();
        // Service bus access
        BusAccess::service();

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
    uint32_t startUpdateTimeMs = millis();

    int issueCount = 0;
    while (1)
    {
        // Service the comms channels and display updates
        service();
        // Timer polling
        timer_poll();
        // Service bus access
        BusAccess::service();

        // Check state
        switch(testState)
        {
            case TEST_STATE_CONFIRM:
            {
                if (_keyInfoBufferPos.canGet())
                {
                    KeyInfo* pKeyInfo = &_keyInfoBuffer[_keyInfoBufferPos.posToGet()];
                    _keyInfoBufferPos.hasGot();
                    // LogWrite(FromBusRaiderApp, LOG_DEBUG, "Keyin %02x %02x %02x", pKeyInfo->rawKeys[0], pKeyInfo->rawKeys[1], pKeyInfo->rawKeys[2]);
                    int asciiCode = McTerminal::convertRawToAscii(pKeyInfo->modifiers, pKeyInfo->rawKeys);
                    char keyStr[2];
                    keyStr[0] = asciiCode;
                    keyStr[1] = 0;
                    if (rdisalpha(asciiCode) || rdisdigit(asciiCode) || rdisspace(asciiCode))
                        _display.consolePut(keyStr);
                    if (rdtolower(asciiCode) == 'y')
                    {
                        _display.consolePut("\n");
                        testState = TEST_STATE_SET_MC;
                    }
                }
                if (isTimeout(millis(), startUpdateTimeMs, 20000))
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
                startUpdateTimeMs = millis();
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
                if (isTimeout(millis(), startUpdateTimeMs, 60000))
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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Self test - read/set bus
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusRaiderApp::testSelf_detailedBus()
{
    _display.consoleForeground(DISPLAY_FX_BLUE);
    _display.consolePut("\nDetailed bus test\n");
    _display.consoleForeground(DISPLAY_FX_WHITE);
    _display.consolePut("\nThis test requires a male-male jumper wire but\n");
    _display.consolePut("but don't plug anything into the bus until asked to do so\n");
    _display.consolePut("Confirm that ONLY BusRaider is installed (y/n) ...");

    enum testState_type
    {
        TEST_STATE_CONFIRM,
        TEST_STATE_SET_MC,
        TEST_STATE_TEST_SEQUENCE,
        TEST_STATE_TEST_WAIT_FOR_KEY,
        TEST_STATE_PREPPED,
        TEST_STATE_TEST_BUS_PIN,
        TEST_STATE_TEST_PIN_TO_PIN,
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
        { "MREQ", TEST_OPT_PIN_TO_PIN, "Place the same jumper: BUSACK to %s", BR_BUSACK_BAR, OUTPUT, BR_MREQ_BAR, true, "FAILED: %s not working check U1", "%s looks OK" },
        { "WR", TEST_OPT_PIN_TO_PIN, "Place the same jumper: BUSACK to %s", BR_BUSACK_BAR, OUTPUT, BR_WR_BAR, true, "FAILED: %s not working check U1", "%s looks OK" },
        { "RD", TEST_OPT_PIN_TO_PIN, "Place the same jumper: BUSACK to %s", BR_BUSACK_BAR, OUTPUT, BR_RD_BAR, true, "FAILED: %s not working check U1", "%s looks OK" },
        { "IORQ", TEST_OPT_PIN_TO_PIN, "Place the same jumper: BUSACK to %s", BR_BUSACK_BAR, OUTPUT, BR_IORQ_BAR, true, "FAILED: %s not working check U1", "%s looks OK" },
        { "ADDR", TEST_OPT_ADDR, "Next test address bus, remove jumper", BR_BUSACK_BAR, -1, 0, true, "", "Address bus is OK" },
    };

    uint32_t startUpdateTimeMs = millis();

    int issueCount = 0;
    uint32_t testSeqIdx = 0;
    while (1)
    {
        // Service the comms channels and display updates
        service();
        // Timer polling
        timer_poll();
        // Service bus access
        BusAccess::service();

        // Check for keyboard keys
        int asciiCode = 0;
        if (_keyInfoBufferPos.canGet())
        {
            KeyInfo* pKeyInfo = &_keyInfoBuffer[_keyInfoBufferPos.posToGet()];
            _keyInfoBufferPos.hasGot();
            // LogWrite(FromBusRaiderApp, LOG_DEBUG, "Key %c", pKeyInfo->rawKeys[0]);
            asciiCode = McTerminal::convertRawToAscii(pKeyInfo->modifiers, pKeyInfo->rawKeys);
            char keyStr[2];
            keyStr[0] = asciiCode;
            keyStr[1] = 0;
            if (rdisalpha(asciiCode) || rdisdigit(asciiCode) || rdisspace(asciiCode))
                _display.consolePut(keyStr);
        }

        // Test for quit
        if ((rdtolower(asciiCode) == 'q') || (asciiCode == 27))
            testState = TEST_STATE_DONE;

        // Test for time-out
        if (isTimeout(millis(), startUpdateTimeMs, 60000))
            {
                testState = TEST_STATE_DONE;
                _display.consolePut("\nTest timed-out, repeat if required\n");
            }

        // Check state
        testSequence_type* pTestRec = &testSequence[testSeqIdx];
        switch(testState)
        {
            case TEST_STATE_CONFIRM:
            {
                if (rdtolower(asciiCode) == 'y')
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
                startUpdateTimeMs = millis();
                break;
            }
            case TEST_STATE_TEST_SEQUENCE:
            {
                if (strlen(pTestRec->msgBeforeTest) != 0)
                {
                    char outMsg[200];
                    ee_sprintf(outMsg, pTestRec->msgBeforeTest, pTestRec->testPinName);
                    _display.consolePut(outMsg);
                    _display.consolePut(", then press SPACE BAR");
                    startUpdateTimeMs = millis();
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
                if (asciiCode == ' ')
                {
                    _display.consolePut("\n");
                    startUpdateTimeMs = millis();
                    testState = TEST_STATE_PREPPED;
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
            case TEST_STATE_TEST_ADDR:
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
                    ee_sprintf(testStr, "Awr %04x Ard %04x Ard2 %02x data %02x ctrl %s\n", addrTestMask, addr, vv, data, ctrlStr);
                    LogWrite(FromBusRaiderApp, LOG_DEBUG, "%s", testStr);

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
                    ee_sprintf(testStr, "Awr %04x Ard %04x Ard2 %02x data %02x ctrl %s\n", inverseTestMask, addr, vv, data, ctrlStr);
                    LogWrite(FromBusRaiderApp, LOG_DEBUG, "%s", testStr);

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
                if ((addrStuckHighMask | addrStuckLowMask | addrInteractionMask) == 0)
                {
                    testState = TEST_STATE_TEST_OK;
                }
                else
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
                        ee_sprintf(tmpStr, "%d", i);
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
                    testState = TEST_STATE_TEST_FAILED;
                }
                digitalWrite(BR_BUSRQ_BAR, 1);
                break;
            }
            case TEST_STATE_TEST_FAILED:
            {
                char outMsg[200];
                _display.consoleForeground(DISPLAY_FX_RED);
                ee_sprintf(outMsg, pTestRec->msgIfFailed, pTestRec->testPinName);
                _display.consolePut(outMsg);
                _display.consolePut("\n");
                _display.consoleForeground(DISPLAY_FX_WHITE);
                issueCount++;
                if (!pTestRec->continueOnFail)
                {
                    _display.consoleForeground(DISPLAY_FX_RED);
                    _display.consolePut("This issue needs to be resolved before testing again\n");
                    _display.consoleForeground(DISPLAY_FX_WHITE);
                    testState = TEST_STATE_DONE;
                }
                testState = TEST_STATE_NEXT;
                break;
            }
            case TEST_STATE_TEST_OK:
            {
                char outMsg[200];
                if (strlen(pTestRec->msgIfSucceeded) > 0)
                {
                    _display.consoleForeground(DISPLAY_FX_GREEN);
                    ee_sprintf(outMsg, pTestRec->msgIfSucceeded, pTestRec->testPinName);
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
// Self test - memory test
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusRaiderApp::testSelf_memory()
{
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
    uint32_t startUpdateTimeMs = millis();
    int issueCount = 0;
    uint32_t testSeqIdx = 0;
    while (1)
    {
        // Service the comms channels and display updates
        service();
        // Timer polling
        timer_poll();
        // Service bus access
        BusAccess::service();

        // Check for keyboard keys
        int asciiCode = 0;
        if (_keyInfoBufferPos.canGet())
        {
            KeyInfo* pKeyInfo = &_keyInfoBuffer[_keyInfoBufferPos.posToGet()];
            _keyInfoBufferPos.hasGot();
            // LogWrite(FromBusRaiderApp, LOG_DEBUG, "Key %c", pKeyInfo->rawKeys[0]);
            asciiCode = McTerminal::convertRawToAscii(pKeyInfo->modifiers, pKeyInfo->rawKeys);
            char keyStr[2];
            keyStr[0] = asciiCode;
            keyStr[1] = 0;
            if (rdisalpha(asciiCode) || rdisdigit(asciiCode) || rdisspace(asciiCode))
                _display.consolePut(keyStr);
        }

        // Test for quit
        if ((rdtolower(asciiCode) == 'q') || (asciiCode == 27))
            testState = TEST_STATE_DONE;

        // Test for time-out
        if (isTimeout(millis(), startUpdateTimeMs, 60000))
            {
                testState = TEST_STATE_DONE;
                _display.consolePut("\nTest timed-out, repeat if required\n");
            }

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
                if (rdtolower(asciiCode) == 'y')
                {
                    _display.consolePut("\n");
                    startUpdateTimeMs = millis();
                    testState = TEST_STATE_PERFORM_RESET;
                } else if (rdtolower(asciiCode) == 'n')
                {
                    _display.consolePut("\n");
                    startUpdateTimeMs = millis();
                    testState = TEST_STATE_CHECK_BUSRQ;
                }
                break;
            }
            case TEST_STATE_PERFORM_RESET:
            {
                // Reset the machine
                BusAccess::rawBusControlMuxSet(BR_MUX_RESET_Z80_BAR_LOW);
                microsDelay(100000);
                BusAccess::rawBusControlMuxClear();
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
                if (rdtolower(asciiCode) == 'y')
                {
                    _display.consolePut("... performing test\n");
                    startUpdateTimeMs = millis();
                    testState = TEST_STATE_PERFORM_BLOCK_TEST;
                } else if (rdtolower(asciiCode) == 'n')
                {
                    _display.consolePut("... skipping test\n");
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

                uint32_t rslt = memTestDevice(blockStart, blockLen);
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
                    rslt = memTestAddressBus(blockStart, blockLen);
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

                    rslt = memTestDataBus(blockStart);
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
