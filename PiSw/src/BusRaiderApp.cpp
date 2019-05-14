// Bus Raider
// Rob Dobson 2019

#include "BusRaiderApp.h"
#include "System/OTAUpdate.h"
#include "System/Display.h"
#include "System/UartMaxi.h"
#include "System/lowlib.h"
#include "System/rdutils.h"
#include "System/ee_sprintf.h"
#include "Machines/McManager.h"
#include "Machines/usb_hid_keys.h"
#include "Machines/McTerminal.h"
typedef unsigned char		u8;
#include "../uspi/include/uspi.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Vars
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Log string
const char* FromBusRaiderApp = "BusRaiderApp";

BusRaiderApp* BusRaiderApp::_pApp = NULL;

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
    _commandHandler.setPutToSerialCallback(putToSerial);
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
        // LogWrite(FromMain, LOG_DEBUG, "Checking for keyboards...");

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
            strlcat(statusStr, "              ", MAX_STATUS_STR_LEN);
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
        strlcat(statusStr, "                       ", MAX_STATUS_STR_LEN);
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
        if (McManager::targetIsPaused())
            strlcat(statusStr, "Paused      ", MAX_STATUS_STR_LEN);
        else
            strlcat(statusStr, "Free Running", MAX_STATUS_STR_LEN);
        if (McManager::targetBusUnderPiControl())
            strlcat(statusStr, "PiControl   ", MAX_STATUS_STR_LEN);
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

        // Ready for next time
        _statusUpdateStartUs = micros();
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Callbacks
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Function to send to uart from command handler
void BusRaiderApp::putToSerial(const uint8_t* pBuf, int len)
{
    for (int i = 0; i < len; i++)
        _pApp->_uart.write(pBuf[i]);
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

void BusRaiderApp::usbKeypressHandlerStatic(unsigned char ucModifiers, const unsigned char rawKeys[6])
{
    _pApp->usbKeypressHandler(ucModifiers, rawKeys);
}

void BusRaiderApp::usbKeypressHandler(unsigned char ucModifiers, const unsigned char rawKeys[6])
{
    // Check for immediate mode
    if (rawKeys[0] == KEY_F2)
    {
        if (!_immediateMode)
        {
            LogPrintf("Entering immediate mode, e.g. w/ssid/password/hostname<enter> to setup WiFi ...\n");
        }
        _immediateMode = true;
        return;
    }
    if (_immediateMode)
    {
        if (_immediateModeLineLen < IMM_MODE_LINE_MAXLEN)
        {
            int asciiCode = McTerminal::convertRawToAscii(ucModifiers, rawKeys);
            if (asciiCode == 0)
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
                    CommandHandler::sendAPIReq(_immediateModeLine);
                    LogPrintf("Sent request to ESP32: %s\n", _immediateModeLine);
                }
                _immediateModeLineLen = 0;
            }
            else if ((asciiCode >= 32) && (asciiCode < 127))
            {
                _immediateModeLine[_immediateModeLineLen++] = asciiCode;
            }
            _display.consolePut(asciiCode);
            // LogPrintf("%x ", asciiCode);
        }
        return;
    }

    // Send to the target machine to process
    // ee_printf("KEY mod %02x raw %02x %02x %02x\n", ucModifiers, rawKeys[0], rawKeys[1], rawKeys[2]);
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
    if (!jsonGetValueForKey("espHealth", pCmdJson, espHealthJson, MAX_ESP_HEALTH_STR))
        return;
    if (!jsonGetValueForKey("wifiIP", espHealthJson, _esp32IPAddress, MAX_IP_ADDR_STR))
        return;
    _esp32IPAddressValid = (strcmp(_esp32IPAddress, "0.0.0.0") != 0);
    if (!jsonGetValueForKey("wifiConn", espHealthJson, _esp32WifiConnStr, MAX_WIFI_CONN_STR))
        return;
    if (!jsonGetValueForKey("ssid", espHealthJson, _esp32WifiSSID, MAX_WIFI_SSID_STR))
        return;
    if (!jsonGetValueForKey("espV", espHealthJson, _esp32ESP32Version, MAX_ESP_VERSION_STR))
        return;
    // LogWrite(FromBusRaiderApp, LOG_DEBUG, "Ip Address %s", _esp32IPAddress);
}

