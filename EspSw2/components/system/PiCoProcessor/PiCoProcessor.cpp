/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// PiCoProcessor
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Logger.h>
#include <PiCoProcessor.h>
#include <Utils.h>
#include <RestAPIEndpointManager.h>
#include "ProtocolEndpointManager.h"
#include <NetworkSystem.h>
#include <ESPUtils.h>
#include <driver/uart.h>
#include <driver/gpio.h>

static const char *MODULE_PREFIX = "PiCoProcessor";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

PiCoProcessor::PiCoProcessor(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, 
        ConfigBase *pMutableConfig, const char* systemVersion)
    : SysModBase(pModuleName, defaultConfig, pGlobalConfig, pMutableConfig)
{
    // Config variables
    _isEnabled = false;
    _uartNum = 0;
    _baudRate = 912600;
    _txPin = 0;
    _rxPin = 0;
    _rxBufSize = 1024;
    _txBufSize = 1024;
    _hdlcMaxLen = 1024;
    _isInitialised = false;
    _pHDLC = NULL;
    _cachedPiStatusJSON = "{}";
    _cachedPiStatusRequestMs = 0;
    _pRestAPIEndpointManager = NULL;
    _systemVersion = systemVersion;

    // Assume hardware version until detected
    _hwVersion = ESP_HW_VERSION_DEFAULT;

    // EndpointID
    _protocolEndpointID = ProtocolEndpointManager::UNDEFINED_ID;
}

PiCoProcessor::~PiCoProcessor()
{
    if (_isInitialised)
        uart_driver_delete((uart_port_t)_uartNum);
    // Clean up HDLC
    if (_pHDLC)
        delete _pHDLC;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PiCoProcessor::setup()
{
    // Apply config
    applySetup();
}

void PiCoProcessor::applySetup()
{
    // Clear previous config if we've been here before
    if (_isInitialised)
        uart_driver_delete((uart_port_t)_uartNum);
    _isInitialised = false;

    // Enable
    _isEnabled = configGetLong("enable", 0) != 0;

    // Port
    _uartNum = configGetLong("uartNum", 80);

    // Baud
    _baudRate = configGetLong("baudRate", 912600);

    // Protocol
    _protocol = configGetString("protocol", "");

    // Pins
    _rxPin = configGetLong("rxPin", -1);
    _txPin = configGetLong("txPin", -1);

    // Buffers
    _rxBufSize = configGetLong("rxBufSize", 1024);
    _txBufSize = configGetLong("txBufSize", 1024);
    _hdlcMaxLen = configGetLong("hdlcMaxLen", 1024);

    LOG_I(MODULE_PREFIX, "setup enabled %s uartNum %d baudRate %d txPin %d rxPin %d rxBufSize %d txBufSize %d hdlcMaxLen %d protocol %s", 
                    _isEnabled ? "YES" : "NO", _uartNum, _baudRate, _txPin, _rxPin, _rxBufSize, _txBufSize, _hdlcMaxLen, _protocol.c_str());

    if (_isEnabled && (_rxPin != -1) && (_txPin != -1))
    {

        // Configure UART. Note that REF_TICK is used so that the baud rate remains
        // correct while APB frequency is changing in light sleep mode
        const uart_config_t uart_config = {
                .baud_rate = _baudRate,
                .data_bits = UART_DATA_8_BITS,
                .parity = UART_PARITY_DISABLE,
                .stop_bits = UART_STOP_BITS_1,
                .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
                .rx_flow_ctrl_thresh = 10,
                .use_ref_tick = false,
        };
        esp_err_t err = uart_param_config((uart_port_t)_uartNum, &uart_config);
        if (err != ESP_OK)
        {
            LOG_E(MODULE_PREFIX, "Failed to initialize uart, err %d", err);
            return;
        }

        // Setup pins
        err = uart_set_pin((uart_port_t)_uartNum, _txPin, _rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
        if (err != ESP_OK)
        {
            LOG_E(MODULE_PREFIX, "Failed to set uart pins, err %d", err);
            return;
        }

        // Delay before UART change
        vTaskDelay(1);

        // Install UART driver for interrupt-driven reads and writes
        err = uart_driver_install((uart_port_t)_uartNum, _rxBufSize, _txBufSize, 0, NULL, 0);
        if (err != ESP_OK)
        {
            LOG_E(MODULE_PREFIX, "Failed to install uart driver, err %d", err);
            return;
        }

        // Clean up existing HDLC
        if (_pHDLC)
            delete _pHDLC;
        
        // New HDLC
        _pHDLC = new MiniHDLC(            
            std::bind(&PiCoProcessor::hdlcFrameTxCB, this, 
                std::placeholders::_1, std::placeholders::_2),
            std::bind(&PiCoProcessor::hdlcFrameRxCB, this, 
                std::placeholders::_1, std::placeholders::_2),
            _hdlcMaxLen, _hdlcMaxLen);
            
        // Init ok
        _isInitialised = true;
    }

    // Detect hardware version
    detectHardwareVersion();    
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PiCoProcessor::service()
{
    if (!_isInitialised)
        return;

    // Check cached status is ok
    if (_cachedPiStatusJSON.length() <= 2)
    {
        if ((_cachedPiStatusRequestMs == 0) || 
            (Utils::isTimeout(millis(), _cachedPiStatusRequestMs, TIME_BETWEEN_PI_STATUS_REQS_MS)))
        {
            // Request status
            const char getStatusCmd[] = "{\"cmdName\":\"getStatus\",\"reqStr\":\"\"}\0";
            sendToPi((const uint8_t*)getStatusCmd, sizeof(getStatusCmd));
            LOG_I(MODULE_PREFIX, "Query Pi Status");
            _cachedPiStatusRequestMs = millis();
        }
    }

    // Check anything available
    size_t numCharsAvailable = 0;
    esp_err_t err = uart_get_buffered_data_len((uart_port_t)_uartNum, &numCharsAvailable);
    if ((err == ESP_OK) && (numCharsAvailable > 0))
    {
        // Get data
        static const int MAX_BYTES_PER_CALL = 100;
        uint8_t buf[MAX_BYTES_PER_CALL];
        uint32_t bytesToGet = numCharsAvailable;
        if (bytesToGet > MAX_BYTES_PER_CALL)
            bytesToGet = MAX_BYTES_PER_CALL;
        uint32_t bytesRead = uart_read_bytes((uart_port_t)_uartNum, buf, bytesToGet, 1);
        if (bytesRead != 0)
        {
            LOG_I(MODULE_PREFIX, "service charsAvail %d ch %02x", numCharsAvailable, buf[0]);
            if (_pHDLC)
                _pHDLC->handleBuffer(buf, bytesRead);
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Endpoint Manager
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PiCoProcessor::addRestAPIEndpoints(RestAPIEndpointManager &endpointManager)
{
    endpointManager.addEndpoint("queryESPHealth", 
                        RestAPIEndpointDef::ENDPOINT_CALLBACK, 
                        RestAPIEndpointDef::ENDPOINT_GET, 
                        std::bind(&PiCoProcessor::apiQueryESPHealth, this,
                                std::placeholders::_1, std::placeholders::_2),
                        "Query ESP Health");    
    _pRestAPIEndpointManager = &endpointManager;
}

void PiCoProcessor::addProtocolEndpoints(ProtocolEndpointManager &endpointManager)
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send response to co-processor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PiCoProcessor::sendResponseToPi(String& reqStr, String& msgJson)
{
    LOG_I(MODULE_PREFIX, "req %s ... response Msg %s", reqStr.c_str(), msgJson.c_str());

    String frame;
    if (msgJson.startsWith("{"))
    {
        frame = "{\"cmdName\":\"" + reqStr + "Resp\",\"msg\":" + msgJson + "}\0";
    }
    else
    {
        frame = "{\"cmdName\":\"" + reqStr + "Resp\"," + msgJson + "}\0";
    }
    sendToPi((const uint8_t*)frame.c_str(), frame.length());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send to co-processor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PiCoProcessor::sendToPi(const uint8_t *pFrame, int frameLen)
{
    LOG_I(MODULE_PREFIX, "sendToPi frameLen %d", frameLen);
    _pHDLC->sendFrame(pFrame, frameLen);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PiCoProcessor::hdlcFrameRxCB(const uint8_t* pFrame, int frameLen)
{
    // Debug
    LOG_I(MODULE_PREFIX, "Frame received len %d", frameLen);

    // Extract frame type
    if (frameLen < 0)
        return;

    // Buffer is null terminated
    const char* pRxStr = (const char*)pFrame;

    // Log.trace("%s<- %s\n", MODULE_PREFIX, pRxStr);

    // Get command
    String cmdName = RdJson::getString("cmdName", "", pRxStr);
    if (cmdName.equalsIgnoreCase("getStatusResp"))
    {
        // Cache the status frame
        _cachedPiStatusJSON = pRxStr;
        LOG_I(MODULE_PREFIX, "Received status update %s", _cachedPiStatusJSON.c_str());
    }
    else if (cmdName.equalsIgnoreCase("keyCode"))
    {
        // Send key to target
        int asciiCode = RdJson::getLong("key", 0, pRxStr);

        // TODO 2020
        // if (_pTargetSerial && (asciiCode != 0))
        //     _pTargetSerial->write((char)asciiCode);
        LOG_I(MODULE_PREFIX, "sent target char %x", (char)asciiCode);
    }
    else if (cmdName.equalsIgnoreCase("apiReq"))
    {
        String requestStr = RdJson::getString("req", "", pRxStr);
        if (_pRestAPIEndpointManager && requestStr.length() != 0)
        {
            LOG_I(MODULE_PREFIX, "hdlcFrameRxCB apiReq");
            String respStr;
            _pRestAPIEndpointManager->handleApiRequest(requestStr.c_str(), respStr);
            sendResponseToPi(requestStr, respStr);
        }
    }
    else if ((cmdName.equalsIgnoreCase("rdp")) || (cmdName.equalsIgnoreCase("zesarux")))
    {
        // Payload is after a string terminator
        int headerJsonEndPos = strlen(pRxStr);
        int payloadStartPos = 0;
        int payloadLen = 0;
        if (headerJsonEndPos+1 < frameLen)
        {
            payloadStartPos = headerJsonEndPos+1;
            payloadLen = frameLen - headerJsonEndPos - 1;
        }
        uint32_t dataLen = RdJson::getLong("dataLen", payloadLen, pRxStr);

        // TODO 2020
        LOG_I(MODULE_PREFIX, "RDP %d", dataLen);

        // // TODO
        // // String inStr = (const char*)(pFrame+payloadStartPos);
        // // if (inStr.indexOf("validatorStatus") > 0)
        // //     _rdpValStatCount++;
        // String payloadStr = (((const char*)pFrame)+payloadStartPos);
        // payloadStr.replace("\n", "\\n");
        // if ((cmdName.equalsIgnoreCase("rdp")))
        // {
        //     // Log.trace("%srdp <- %s payloadLen %d payload ¬¬%s¬¬\n", 
        //     //             MODULE_PREFIX, pRxStr, payloadLen,
        //     //             payloadStr.c_str());
        //     _miniHDLCForRDPTCP.sendFrame(pFrame+payloadStartPos, dataLen);
        // }
        // else
        // {
        //     // Log.trace("%szesarux <- %s payloadLen %d payload ¬¬%s¬¬\n", 
        //     //             MODULE_PREFIX, pRxStr, payloadLen,
        //     //             payloadStr.c_str());
        //     _pZEsarUXTCPServer->sendChars(pFrame+payloadStartPos, dataLen);
        // }
    }
    else if (cmdName.equalsIgnoreCase("log"))
    {
        // Extract msg
        String logMsg = RdJson::getString("msg", "", pRxStr);
        String msgSrc = RdJson::getString("src", "", pRxStr);
        String msgLev = RdJson::getString("lev", "", pRxStr);
        LOG_I(msgSrc.c_str(), "%s: %s\n", msgLev.c_str(), logMsg.c_str());
    }
    else if (cmdName.equalsIgnoreCase("mirrorScreen"))
    {
        LOG_I(MODULE_PREFIX, "Mirror screen len %d buf[52]... %x %x %x %x", 
                frameLen, pFrame[52], pFrame[53], pFrame[54], pFrame[55]);

        // TODO 2020
        // _pWebServer->webSocketSend(pFrame, frameLen);
    }
    else if ((cmdName.endsWith("Resp")))
    {
        // TODO 2020
        // _cmdResponseNew = true;
        // _cmdResponseBuf = pRxStr;
        LOG_I(MODULE_PREFIX, "RespMessageReceived %s\n", pRxStr);
    }
}

void PiCoProcessor::hdlcFrameTxCB(const uint8_t* pFrame, int frameLen)
{
    // Debug
    Utils::logHexBuf(pFrame, frameLen, MODULE_PREFIX, "Frame to send");
    // Send the message
    int bytesSent = uart_write_bytes((uart_port_t)_uartNum, 
                        (const char*)pFrame, frameLen);
    if (bytesSent != frameLen)
    {
        LOG_W(MODULE_PREFIX, "hdlcFrameTxCB len %d only wrote %d bytes",
                frameLen, bytesSent);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// WiFi status code
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const char* PiCoProcessor::getWifiStatusStr()
{
    if (networkSystem.isWiFiStaConnectedWithIP())
        return "C";
    return "I";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// API requests
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PiCoProcessor::apiQueryESPHealth(const String &reqStr, String &respStr)
{
    LOG_I(MODULE_PREFIX, "queryESPHealth %s", reqStr.c_str());
    char healthStr[500];
    snprintf(healthStr, sizeof(healthStr), R"("espHealth":{"wifiIP":"%s","wifiConn":"%s","ssid":"%s","MAC":"%s","RSSI":"%d","espV":"%s","espHWV":"%d"})", 
                networkSystem.getWiFiIPV4AddrStr().c_str(), 
                getWifiStatusStr(),
                networkSystem.getSSID().c_str(), 
                getSystemMACAddressStr(ESP_MAC_WIFI_STA, ":").c_str(), 
                0, 
                _systemVersion.c_str(), 
                _hwVersion);
    respStr = healthStr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Detect hardware version
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PiCoProcessor::detectHardwareVersion()
{
    // Test pins for each version
    int testInPins[] = { HW_VERSION_DETECT_V22_IN_PIN, HW_VERSION_DETECT_V20_IN_PIN };
    int testHwVersions[] { 22, 20 };

    // V1.7 has no pins connected
    _hwVersion = 17;

    for (int i = 0; i < 100; i++)

    // Hardware version detection
    gpio_pad_select_gpio((gpio_num_t)HW_VERSION_DETECT_OUT_PIN);
    gpio_set_direction((gpio_num_t)HW_VERSION_DETECT_OUT_PIN, GPIO_MODE_OUTPUT);
    for (uint32_t i = 0; i < sizeof(testInPins)/sizeof(testInPins[0]); i++)
    {
        gpio_set_level((gpio_num_t)HW_VERSION_DETECT_OUT_PIN, 0);
        gpio_pad_select_gpio((gpio_num_t)testInPins[i]);
        gpio_set_direction((gpio_num_t)testInPins[i], GPIO_MODE_INPUT);
        esp_err_t esperr = gpio_pullup_en((gpio_num_t)testInPins[i]);
        delay(1);
        bool hwIn0Value = gpio_get_level((gpio_num_t)testInPins[i]) != 0;
        gpio_set_level((gpio_num_t)HW_VERSION_DETECT_OUT_PIN, 1);
        bool hwIn1Value = gpio_get_level((gpio_num_t)testInPins[i]) != 0;
        gpio_pullup_dis((gpio_num_t)testInPins[i]);
        // Check if IN and OUT pins tied together - if setting out to 0
        // makes in 0 then they are (as input is pulled-up otherwise)
        if ((hwIn0Value == 0) && (hwIn1Value == 1))
        {
            _hwVersion = testHwVersions[i];
            LOG_I(MODULE_PREFIX, "HW%d version detect wrote 0 got %d wrote 1 got %d so hwVersion = %d", 
                    testHwVersions[i], hwIn0Value, hwIn1Value, _hwVersion);
            break;
        }
        else
        {
            LOG_I(MODULE_PREFIX, "HW%d version detect wrote 0 got %d wrote 1 got %d - so NOT that version - esperr %d", 
                    testHwVersions[i], hwIn0Value, hwIn1Value, esperr);
        }
    }

    // Tidy up
    gpio_set_direction((gpio_num_t)HW_VERSION_DETECT_OUT_PIN, GPIO_MODE_INPUT);
}
