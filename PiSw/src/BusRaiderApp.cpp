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
#include "Machines/McManager.h"
#include "Machines/usb_hid_keys.h"
#include "Machines/McTerminal.h"
#include "TargetBus/TargetTracker.h"
typedef unsigned char		u8;
#include "../uspi/include/uspi.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Vars
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Log string
const char* FromBusRaiderApp = "BusRaiderApp";

BusRaiderApp* BusRaiderApp::_pApp = NULL;

BusRaiderApp::KeyInfo BusRaiderApp::_keyInfoBuffer[MAX_USB_KEYS_BUFFERED];
RingBufferPosn BusRaiderApp::_keyInfoBufferPos(BusRaiderApp::MAX_USB_KEYS_BUFFERED);
bool BusRaiderApp::_inKeyboardRoutine = false;

const uint32_t BusRaiderApp::_autoBaudRates[] = { 2000000, 1000000 };
uint32_t BusRaiderApp::_autoBaudLastESP32CommsMs = 0;

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

    // Auto baud
    _autoBaudLastESP32CommsMs = 0;
    _autoBaudLastCheckMs = 0;
    _autoBaudFailCount = 0;
    _autoBaudCurBaudIdx = 0;

    // Tests
    _testSelf_curKeyAscii = -1;
    _testSelf_startUpdateTimeMs = 0;
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

    // Check for communication with the ESP32 - if this fails for an extended period then try different baud rates
    if (isTimeout(millis(), _autoBaudLastCheckMs, _autoBaudCheckPeriodMs))
    {
        // _display.consolePut("Checking auto baud\n");
        if (isTimeout(millis(), _autoBaudLastESP32CommsMs, _autoBaudMaxTimeBetweenESPCommsMs))
        {
            // _display.consolePut("Not connected\n");
            // Not connected so count failures
            _autoBaudFailCount++;
            if (_autoBaudFailCount > _autoBaudFailCountToChangeBaud)
            {
                _autoBaudFailCount = 0;
                
                // Get next baud
                _autoBaudCurBaudIdx++;
                if (_autoBaudCurBaudIdx >= sizeof(_autoBaudRates)/sizeof(_autoBaudRates[0]))
                    _autoBaudCurBaudIdx = 0;
                
                // Try this rate
                _uart.setup(_autoBaudRates[_autoBaudCurBaudIdx]);
                _uart.clear();
                char outStr[200];
                ee_sprintf(outStr, "Not connected to ESP32: trying baud rate %d\n", _autoBaudRates[_autoBaudCurBaudIdx]);
                _display.consolePut(outStr);
            }
        }
        _autoBaudLastCheckMs = millis();
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Received message handler
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool BusRaiderApp::handleRxMsg(const char* pCmdJson, [[maybe_unused]]const uint8_t* pParams, [[maybe_unused]]int paramsLen,
                char* pRespJson, int maxRespLen)
{
    _autoBaudLastESP32CommsMs = millis();

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
        // LogWrite(FromBusRaiderApp, LOG_DEBUG, "queryCurMcResp %s", pCmdJson);
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
            char tmpStr[10];
            int hwVers = BusAccess::getHwVersion();
            ee_sprintf(tmpStr, " (HW V%d.%d)", hwVers / 10, hwVers %10);
            strlcat(statusStr, tmpStr, MAX_STATUS_STR_LEN);
            strlcat(statusStr, "        ", MAX_STATUS_STR_LEN);
            int dispStatus = Display::STATUS_NORMAL;
            if (isTimeout(millis(), _autoBaudLastESP32CommsMs, _autoBaudMaxTimeBetweenESPCommsMs))
                dispStatus = Display::STATUS_FAIL;
            _display.statusPut(Display::STATUS_FIELD_ESP_VERSION, dispStatus, statusStr);
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
            _display.consolePut("* Bit tests             ... b <enter>\n");

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
            const char* pKeyStr = McTerminal::convertRawToKeyString(ucModifiers, rawKeys);
            // LogWrite(FromBusRaiderApp, LOG_DEBUG, "KEY asc %02x mod %02x raw %02x %02x %02x",
            //             pKeyStr[0], ucModifiers, rawKeys[0], rawKeys[1], rawKeys[2]);
            if (strlen(pKeyStr) == 0)
                return;
            if (pKeyStr[0] == 0x08)
            {
                if (_immediateModeLineLen > 0)
                    _immediateModeLineLen--;
                _display.consolePut(0x08);
                _display.consolePut(' ');
            }
            else if (pKeyStr[0] == 0x0d)
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
                    else if (rdtolower(_immediateModeLine[0]) == 'b')
                    {
                        // Run bus bit test
                        testSelf_busBits();
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
            else if ((pKeyStr[0] >= 32) && (pKeyStr[0] < 127))
            {
                _immediateModeLine[_immediateModeLineLen++] = pKeyStr[0];
                _display.consolePut(pKeyStr[0]);
            }
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

