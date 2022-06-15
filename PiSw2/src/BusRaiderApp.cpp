// Bus Raider
// Rob Dobson 2019

#include "BusRaiderApp.h"
#include "Display.h"
#include "lowlib.h"
#include "rdutils.h"
#include <circle/timer.h>
#include "McManager.h"
#include "usb_hid_keys.h"
#include "McTerminal.h"
#include "SelfTest.h"
#include "DebugHelper.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// System Name and Version
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define SYSTEM_NAME "BusRaider"
#define SYSTEM_VERSION "3.1.16"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Globals
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Program details
const char* PROG_VERSION = SYSTEM_NAME " " SYSTEM_VERSION " (C) Rob Dobson 2018-2020";
const char* PROG_LINKS_1 = "https://robdobson.com/tag/raider";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Vars
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Debug
// #define DEBUG_APP
// #define DEBUG_APP_KEYS
// #define DEBUG_ESP_STATUS
// #define DEBUG_PROGRESS_BAR
// #define DEBUG_RX_MSGS

// Log string
#if defined(DEBUG_APP) || defined(DEBUG_APP_KEYS)
static const char* MODULE_PREFIX = "BusRaiderApp";
#endif

BusRaiderApp* BusRaiderApp::_pApp = NULL;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Init
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BusRaiderApp::BusRaiderApp(Display& display, CUartMaxiSerialDevice& serial) :
    _display(display),
    _commsManager(serial, NULL),
    _controlAPI(_commsManager.getCommandHandler(), _busControl, _mcManager),
    _mcManager(&_display, _commsManager.getCommandHandler(), _busControl),
    _keyInfoBufferPos(MAX_USB_KEYS_BUFFERED)
{
    _pApp = this;
    _inKeyboardRoutine = false;
    clear();
}

void BusRaiderApp::init()
{
    clear();
    _commsManager.setup();
    _busControl.init();
    _controlAPI.init();
    _mcManager.init();
    
    // Add socket for status handling
    _commsManager.getCommandHandler().commsSocketAdd(this, true, 
            BusRaiderApp::handleRxMsgStatic, NULL, NULL);
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
    _esp32StatusLastRxMs = 0;
    _esp32LastMachineCmd[0] = 0;
    _esp32LastMachineValid = false;
    _esp32LastMachineReqUs = 0;
    _selfTestMode = false;
    _selfTestKeyWaiting = -1;
}

void BusRaiderApp::peripheralStatus(bool usbOk, bool keyboardOk)
{
    // USB
    if (usbOk) 
    {
#ifdef DEBUG_APP_KEYS
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "Checking for keyboards...");
#endif
        if (keyboardOk) 
        {
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
    // Service components
    _busControl.service();
    _controlAPI.service();
    _mcManager.service();
    _commsManager.service();

    // Handle status update to ESP32
    CommandHandler& commandHandler = _commsManager.getCommandHandler();
    if (isTimeout(micros(), _esp32StatusUpdateStartUs, PI_TO_ESP32_STATUS_UPDATE_RATE_MS * 1000)) 
    {
        // Send status update
        if (!commandHandler.isFileTransferInProgress())
        {
            // Get Pi status
            char respBuffer[MAX_PI_STATUS_MSG_LEN];
            getPiStatus(respBuffer, MAX_PI_STATUS_MSG_LEN);

            // Check if changed
            if (strcmp(respBuffer, _piStatusCached) != 0)
            {
                strlcpy(_piStatusCached, respBuffer, MAX_PI_STATUS_MSG_LEN);
                // Send update
                commandHandler.sendWithJSON("getStatusResp", respBuffer);
            }

            // Time of last check
            _esp32StatusUpdateStartUs = micros();
        }
    }

    // Handle status update from ESP32
    if (isTimeout(millis(), _esp32StatusLastRxMs, ESP32_TO_PI_STATUS_UPDATE_MAX_MS))
    {
        // Request status update
        if (!commandHandler.isFileTransferInProgress())
        {
            commandHandler.sendAPIReq("queryESPHealth");
#ifdef DEBUG_ESP_STATUS
            LogWrite(MODULE_PREFIX, LOG_DEBUG, "Req ESP32 status as not received recently");
#endif
        }
        _esp32StatusLastRxMs = millis();
    }

    // Handle initial get of ESP32 last machine info
    if (!_esp32LastMachineValid)
    {
        if (isTimeout(micros(), _esp32LastMachineReqUs, ESP32_LAST_MC_REQ_MS * 1000))
        {
            commandHandler.sendAPIReq("querycurmc");
            _esp32LastMachineReqUs = micros();
#ifdef DEBUG_ESP_STATUS
            LogWrite(MODULE_PREFIX, LOG_DEBUG, "Req ESP32 cur machine");
#endif
        }
    }

    // Display status update
    statusDisplayUpdate();

    // Service keyboard
    if (!_inKeyboardRoutine || _selfTestMode)
    {
#ifdef DEBUG_APP_KEYS
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "Keys in buffer %d", _keyInfoBufferPos.count());
#endif
        if (_keyInfoBufferPos.canGet())
        {
            KeyInfo* pKeyInfo = &_keyInfoBuffer[_keyInfoBufferPos.posToGet()];
            _keyInfoBufferPos.hasGot();
#ifdef DEBUG_APP_KEYS
            LogWrite(MODULE_PREFIX, LOG_DEBUG, "KeyAtTop %02x %02x %02x", 
                        pKeyInfo->rawKeys[0], pKeyInfo->rawKeys[1], pKeyInfo->rawKeys[2]);
#endif
            if (!_inKeyboardRoutine)
            {
                _inKeyboardRoutine = true;
                handleUSBKeypress(pKeyInfo->modifiers, pKeyInfo->rawKeys);
                _inKeyboardRoutine = false;
            }
            else
            {
                const char* pKeyStr = McTerminal::convertRawToKeyString(pKeyInfo->modifiers, pKeyInfo->rawKeys);
#ifdef DEBUG_APP_KEYS
                LogWrite(MODULE_PREFIX, LOG_DEBUG, "Key converted to %s", pKeyStr);
#endif
                if (strlen(pKeyStr) != 0)
                    _selfTestKeyWaiting = pKeyStr[0];
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Received message handler
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool BusRaiderApp::handleRxMsgStatic(void* pObject, const char* pCmdJson, 
                const uint8_t* pParams, unsigned paramsLen,
                char* pRespJson, unsigned maxRespLen)
{
    if (!pObject)
        return false;
    return ((BusRaiderApp*)pObject)->handleRxMsg(pCmdJson, pParams, paramsLen, pRespJson, maxRespLen);
}

bool BusRaiderApp::handleRxMsg(const char* pCmdJson, const uint8_t* pParams, 
                unsigned paramsLen, char* pRespJson, unsigned maxRespLen)
{
#ifdef DEBUG_RX_MSGS
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "rxMsg %s", pCmdJson);
#endif
    #define MAX_CMD_NAME_STR 200
    char cmdName[MAX_CMD_NAME_STR+1];
    if (!jsonGetValueForKey("cmdName", pCmdJson, cmdName, MAX_CMD_NAME_STR))
        return false;

    pRespJson[0] = 0;

    if (strcasecmp(cmdName, "getStatus") == 0)
    {
        // Return the status of the Pi app
        getPiStatus(pRespJson, maxRespLen);
        return true;
    }
    else if (strcasecmp(cmdName, "queryESPHealthResp") == 0)
    {
        // Store ESP32 status info
        storeESP32StatusInfo(pCmdJson);
        return true;
    }
    else if (strcasecmp(cmdName, "queryCurMcResp") == 0)
    {
        // Set machine
#ifdef DEBUG_RX_MSGS
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "queryCurMcResp %s", pCmdJson);
#endif
        _mcManager.setupMachine(pCmdJson);
        // Got ok - no need to re-request
        _esp32LastMachineValid = true;
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
    CommandHandler& commandHandler = _commsManager.getCommandHandler();
    // Update status
    if (isTimeout(micros(), _statusUpdateStartUs, STATUS_UPDATE_TIME_MS * 1000)) 
    {
        // Ready for next time
        _statusUpdateStartUs = micros();

        // Pi Info
        _display.statusPut(Display::STATUS_FIELD_PI_VERSION, Display::STATUS_NORMAL, PROG_VERSION);
        _display.statusPut(Display::STATUS_FIELD_LINKS, Display::STATUS_NORMAL, PROG_LINKS_1);

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
            char tmpStr[30];
            int hwVers = _busControl.bus().getHwVersion(false);
            snprintf(tmpStr, sizeof(tmpStr), " (HW V%d.%d)", hwVers / 10, hwVers %10);
            strlcat(statusStr, tmpStr, MAX_STATUS_STR_LEN);
            strlcat(statusStr, "        ", MAX_STATUS_STR_LEN);
            int dispStatus = Display::STATUS_NORMAL;
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
        strlcpy(statusStr, "M/C: ", Display::DISPLAY_STATUS_INDENT);
        strlcat(statusStr, _mcManager.getMachineName(), Display::DISPLAY_STATUS_INDENT);
        for (uint32_t i = strlen(statusStr); i < Display::DISPLAY_STATUS_INDENT; i++)
            statusStr[i] = ' ';
        statusStr[Display::DISPLAY_STATUS_INDENT] = 0;
        _display.statusPut(Display::STATUS_FIELD_CUR_MACHINE, Display::STATUS_NORMAL, statusStr);

        // Speed
        uint32_t clockSpeedHz = _mcManager.getClockFreqInHz();
        uint32_t mhz = clockSpeedHz/1000000;
        uint32_t khz = (clockSpeedHz - mhz*1000000)/1000;
        snprintf(statusStr, MAX_STATUS_STR_LEN, "Clock: %d.%s%s%dMHz",
                    mhz,
                    khz <= 99 ? "0" : "",
                    khz <= 9 ? "0" : "",
                    khz);
        strlcat(statusStr, "    ", MAX_STATUS_STR_LEN);
        _display.statusPut(Display::STATUS_FIELD_MACHINES, Display::STATUS_NORMAL, statusStr);

        // Bus status
        statusStr[0] = 0;
        strlcpy(statusStr, "Bus: ", MAX_STATUS_STR_LEN);
        if (_busControl.ctrl().isDebugging() && _busControl.ctrl().isHeldAtWait())
            strlcat(statusStr, "Debug Break ", MAX_STATUS_STR_LEN);
        else if (_busControl.ctrl().isDebugging())
            strlcat(statusStr, "Debug       ", MAX_STATUS_STR_LEN);
        else
            strlcat(statusStr, "Free Running", MAX_STATUS_STR_LEN);
        if (_busControl.bus().busReqAcknowledged())
            strlcat(statusStr, " & PiControl   ", MAX_STATUS_STR_LEN);
        else
            strlcat(statusStr, "               ", MAX_STATUS_STR_LEN);
        _display.statusPut(Display::STATUS_FIELD_BUS_ACCESS, Display::STATUS_NORMAL, statusStr);

        // Refresh rate
        int refreshRate = _mcManager.getDisplayRefreshRate();
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

        // TODO 2020
        // // Get ISR debug info
        // strcpy(statusStr, "");
        // int rxFramingErrCount = 0;
        // int rxOverrunErrCount = 0;
        // int rxBreakCount = 0;
        // int rxBufferFullCount = 0;
        // int txBufferFullCount = 0;
        // _uart.getStatusCounts(txBufferFullCount, rxFramingErrCount, rxOverrunErrCount, rxBreakCount, rxBufferFullCount);
        // if (txBufferFullCount > 0)
        // {
        //     ee_sprintf(refreshStr, "TxFull %u,", txBufferFullCount);
        //     strlcat(statusStr, refreshStr, MAX_STATUS_STR_LEN);
        // }
        // if (rxFramingErrCount > 0)
        // {
        //     ee_sprintf(refreshStr, "RxFr %u,", rxFramingErrCount);
        //     strlcat(statusStr, refreshStr, MAX_STATUS_STR_LEN);
        // }
        // if (rxOverrunErrCount > 0)
        // {
        //     ee_sprintf(refreshStr, "RxOv %u,", rxOverrunErrCount);
        //     strlcat(statusStr, refreshStr, MAX_STATUS_STR_LEN);
        // }
        // // if (rxBreakCount > 0)
        // // {
        // //     ee_sprintf(refreshStr, "RxBk %u,", rxBreakCount);
        // //     strlcat(statusStr, refreshStr, MAX_STATUS_STR_LEN);
        // // }
        // if (rxBufferFullCount > 0)
        // {
        //     ee_sprintf(refreshStr, "RxFull %u,", rxBufferFullCount);
        //     strlcat(statusStr, refreshStr, MAX_STATUS_STR_LEN);
        // }

        // Debug Vals
        statusStr[0] = 0;
        for (uint32_t i = 0; i < DebugHelper::NUM_DEBUG_VALS; i++)
        {
            int cnt = 0;
            if (__debugHelper.get(i, cnt))
            {
                snprintf(refreshStr, sizeof(refreshStr), "%d=%d,", i, cnt);
                strlcat(statusStr, refreshStr, MAX_STATUS_STR_LEN);
            }
        }
        strlcat(statusStr, "                                    ", MAX_STATUS_STR_LEN);
        _display.statusPut(Display::STATUS_FIELD_ASSERTS, Display::STATUS_NORMAL, statusStr);

        // Get file receive status
        uint32_t fileLen = 0, filePos = 0;
        if (!commandHandler.getFileReceiveStatus(fileLen, filePos))
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

#ifdef DEBUG_PROGRESS_BAR
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "filelen %d filepos %d progbarlen %d dispStr %s", 
                    fileLen, filePos, progressBarLen, fileStatusStr);
#endif
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// USB Keyboard
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusRaiderApp::keyStatusHandlerRaw(unsigned char ucModifiers, 
                    const unsigned char rawKeys[CommandHandler::NUM_USB_KEYS_PASSED])
{
    // Place in ring buffer
    if (_pApp->_keyInfoBufferPos.canPut())
    {
        KeyInfo* pKeyInfo = (&_pApp->_keyInfoBuffer[_pApp->_keyInfoBufferPos.posToPut()]);
        for (int i = 0; i < CommandHandler::NUM_USB_KEYS_PASSED; i++)
            pKeyInfo->rawKeys[i] = rawKeys[i];
        pKeyInfo->modifiers = ucModifiers;
        _pApp->_keyInfoBufferPos.hasPut();
        // ISR_ASSERT(ISR_ASSERT_CODE_DEBUG_C);
    }
}

void BusRaiderApp::handleUSBKeypress(unsigned char ucModifiers, 
                const unsigned char rawKeys[CommandHandler::NUM_USB_KEYS_PASSED])
{
    CommandHandler commandHandler = _commsManager.getCommandHandler();
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
#ifdef DEBUG_APP_KEYS
            LogWrite(MODULE_PREFIX, LOG_DEBUG, "KEY asc %02x mod %02x raw %02x %02x %02x",
                        pKeyStr[0], ucModifiers, rawKeys[0], rawKeys[1], rawKeys[2]);
#endif
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
                        // testSelf_detailedBus();
                    }
                    else if (rdtolower(_immediateModeLine[0]) == 'm')
                    {
                        // Run memory test
                        _selfTestMode = true;
                        selfTestMemory(this, _display, _busControl);
                        _selfTestMode = false;
                    }
                    else if (rdtolower(_immediateModeLine[0]) == 'b')
                    {
                        // Run bus bit test
                        // testSelf_busBits();
                    }
                    else if ((rdtolower(_immediateModeLine[0]) == 'r') || (rdtolower(_immediateModeLine[0]) == 's'))
                    {
                        // Run self-tests
                        // testSelf_readSetBus(rdtolower(_immediateModeLine[0]) == 'r');
                    }
                    else
                    {
                        // Send other commands to ESP32
                        commandHandler.sendAPIReq(_immediateModeLine);
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
#ifdef DEBUG_APP_KEYS
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "KEY mod %02x raw %02x %02x %02x\n", ucModifiers, rawKeys[0], rawKeys[1], rawKeys[2]);
#endif
    _mcManager.keyHandler(ucModifiers, rawKeys);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get Pi status
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusRaiderApp::getPiStatus(char* pRespJson, int maxRespLen)
{
    const char* mcJSON = _mcManager.getMachineJSON();
    if (pRespJson)
        strlcpy(pRespJson, mcJSON, maxRespLen);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Address callback
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusRaiderApp::storeESP32StatusInfo(const char* pCmdJson)
{
    // Store prev ESP32Conn for check below
    bool esp32WasConn = strlen(_esp32ESP32Version) > 0;
    _esp32StatusLastRxMs = millis();
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
    int espHwVersion = _busControl.bus().getHwVersion(true);
    jsonGetValueForKey("espHWV", espHealthJson, espHwVersStr, MAX_ESP_VERSION_STR);
    if (strlen(espHwVersStr) != 0)
        espHwVersion = atoi(espHwVersStr);
    _busControl.bus().setHwVersion(espHwVersion);
#ifdef DEBUG_ESP_STATUS
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "Ip Address %s", _esp32IPAddress);
#endif

    // Check for connection of ESP32 and setup logging to esp32 if so
    if ((strlen(_esp32ESP32Version) > 0) && !esp32WasConn)
    {
        // Setup logging output
        LogSetOutFn(CommandHandler::logDebug);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Self-test helpers
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusRaiderApp::selfTestHelperService()
{
    service();
}

int BusRaiderApp::selfTestKeyboardGet()
{
    int tmpKey = _selfTestKeyWaiting;
    _selfTestKeyWaiting = -1;
    return tmpKey;
}
