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
#include <RICRESTMsg.h>
#include <NetworkSystem.h>
#include <ESPUtils.h>
#include <driver/uart.h>
#include <driver/gpio.h>

static const char *MODULE_PREFIX = "PiCoProcessor";

// Debug
// #define DEBUG_PI_UPLOAD_END
// #define DEBUG_PI_SW_UPLOAD
// #define DEBUG_PI_UPLOAD_FROM_FS
// #define DEBUG_PI_QUERY_STATUS
// #define DEBUG_PI_SERIAL_GET
// #define DEBUG_PI_QUERY_ESP_HEALTH
// #define DEBUG_PI_UPLOAD_COMMON_BLOCK
// #define DEBUG_PI_UPLOAD_COMMON_BLOCK_DETAIL
// #define DEBUG_PI_RX_API_REQ
// #define DEBUG_PI_RX_STATUS_UPDATE
// #define DEBUG_PI_RX_FRAME
// #define DEBUG_PI_SEND_FRAME
// #define DEBUG_PI_SEND_TARGET_CMD
// #define DEBUG_PI_SEND_RESP_TO_PI
// #define DEBUG_PI_SEND_FILE_BLOCK
// #define DEBUG_PI_TX_FRAME_TO_PI
// #define DEBUG_RICREST_CMD_FRAMES
#define DEBUG_RDP_MSG_FROM_PI
#define DEBUG_PI_UPLOAD_ACKS
// #define DEBUG_PI_UPLOAD_ACKS_DETAIL

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
    _cmdResponseNew = false;

    // Upload vars
    _uploadFromFSInProgress = false;
    _uploadFromAPIInProgress = false;
    _uploadStartMs = 0;
    _uploadLastBlockMs = 0;
    _uploadBlockCount = 0;
    _uploadFilePos = 0;
    _fileCRC = 0;
    _uploadBytesSent = 0;
    _uploadStartAck = false;
    _uploadBlockRxIndex = 0;
    _uploadEndAck = false;
    _uploadEndNotAck = false;

    // Stats
    _statsLastReportMs = 0;
    _statsRxCh = 0;
    _statsTxCh = 0;
    _statsRxFr = 0;
    _statsTxFr = 0;

    // RDP
    _rdpCommandIndex = 0;
    _rdpChannelId = 0;

    // Assume hardware version until detected
    _hwVersion = ESP_HW_VERSION_DEFAULT;

    // EndpointID
    _protocolEndpointID = ProtocolEndpointManager::UNDEFINED_ID;

    // Endpoint manager
    _pEndpointManager = NULL;
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
    {
        LOG_I(MODULE_PREFIX, "applySetup tidying up");
        uart_driver_delete((uart_port_t)_uartNum);
    }
    _isInitialised = false;

    // Enable
    _isEnabled = configGetLong("enable", 0) != 0;

    // Port
    _uartNum = configGetLong("uartNum", 2);

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
            std::bind(&PiCoProcessor::hdlcFrameTxToPiCB, this, 
                std::placeholders::_1, std::placeholders::_2),
            std::bind(&PiCoProcessor::hdlcFrameRxFromPiCB, this, 
                std::placeholders::_1, std::placeholders::_2),
            0x7E, 0x7D,
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
            const char getStatusCmd[] = "{\"cmdName\":\"getStatus\",\"reqStr\":\"\"}";
            sendMsgStrToPi(getStatusCmd);
#ifdef DEBUG_PI_QUERY_STATUS
            LOG_I(MODULE_PREFIX, "Query Pi Status");
#endif
            _cachedPiStatusRequestMs = millis();
        }
    }

    // Pump the serial interface
    serialInterfacePump();

    // Stats report
    if (Utils::isTimeout(millis(), _statsLastReportMs, STATS_REPORT_TIME_MS))
    {
        MiniHDLCStats* pStats = NULL;
        char hdlcStats[150];
        strcpy(hdlcStats, "HDLC not setup");
        if (_pHDLC)
        {
            pStats = _pHDLC->getStats();
            snprintf(hdlcStats, sizeof(hdlcStats), "HDLC CRCErrors %d FrameTooLongs %d AllocFails %d", 
                pStats->_frameCRCErrCount, pStats->_frameTooLongCount, pStats->_rxBufAllocFail);
        }
        LOG_I(MODULE_PREFIX, "service CommsStats RxCh %d TxCh %d RxFr %d TxFr %d %s", 
                _statsRxCh, _statsTxCh, _statsRxFr, _statsTxFr, hdlcStats); 
        _statsLastReportMs = millis();
    }

    // Check if there's a file system upload in progress
    if (_uploadFromFSInProgress)
    {
        // See if ready to handle the next chunk
        if (Utils::isTimeout(millis(), _uploadLastBlockMs, DEFAULT_BETWEEN_BLOCKS_MS))
        {
            // Get next chunk of data
            uint32_t readLen = 0;
            bool finalChunk = false;
            _uploadBlockBuffer.resize(UPLOAD_BLOCK_SIZE_BYTES);

            // Save the data
            if (_uploadBlockBuffer.size() >= UPLOAD_BLOCK_SIZE_BYTES)
            {
                if (_chunker.next(_uploadBlockBuffer.data(), UPLOAD_BLOCK_SIZE_BYTES, readLen, finalChunk))
                {
                    // File block info
                    uint32_t fileLen = _chunker.getFileLen();
                    FileBlockInfo fileBlockInfo(_chunker.getFileName().c_str(), 
                                fileLen, _uploadFilePos, 
                                _uploadBlockBuffer.data(), readLen, finalChunk,
                                0, false,
                                fileLen, true);
                    // Handle the block
                    uploadCommonBlockHandler(_uploadFileType.c_str(), _uploadFromFSRequest, fileBlockInfo); 
                    _uploadFilePos += readLen;
                }
                else
                {
                    // Tidy up if finished
                    if (!finalChunk)
                        LOG_W(MODULE_PREFIX, "service upload returned false but isn't final chunk");
                    _uploadFromFSInProgress = false;
#ifdef DEBUG_PI_UPLOAD_FROM_FS
                    LOG_W(MODULE_PREFIX, "service uploadFromFS done lastBlockMs %lu betweenBlocksMs %u chunkLen %u finalChunk %d", 
                            _uploadLastBlockMs, DEFAULT_BETWEEN_BLOCKS_MS, readLen, finalChunk);
#endif
                    _uploadBlockBuffer.clear();
                }
            }
        }
    }

    // Check for timeouts on any upload
    if (uploadInProgress())
    {
        // Check for timeouts
        uint32_t curMs = millis();
        if (Utils::isTimeout(curMs+1, _uploadLastBlockMs, MAX_BETWEEN_BLOCKS_MS))
        {
            _uploadFromFSInProgress = false;
            _uploadFromAPIInProgress = false;
            LOG_W(MODULE_PREFIX, "Upload block timed out millis %d lastBlockMs %ld", 
                        curMs, _uploadLastBlockMs);
        }
        if (Utils::isTimeout(curMs+1, _uploadStartMs, MAX_UPLOAD_MS))
        {
            _uploadFromFSInProgress = false;
            _uploadFromAPIInProgress = false;
            LOG_W(MODULE_PREFIX, "Upload timed out");
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
    endpointManager.addEndpoint("uploadpisw", 
                        RestAPIEndpointDef::ENDPOINT_CALLBACK, 
                        RestAPIEndpointDef::ENDPOINT_POST,
                        std::bind(&PiCoProcessor::apiUploadPiSwComplete, this, 
                                std::placeholders::_1, std::placeholders::_2),
                        "Upload Pi Software", 
                        "application/json", 
                        NULL,
                        RestAPIEndpointDef::ENDPOINT_CACHE_NEVER,
                        NULL, 
                        NULL,
                        std::bind(&PiCoProcessor::apiUploadPiSwPart, this, 
                                std::placeholders::_1, std::placeholders::_2));
    endpointManager.addEndpoint("querystatus", 
                        RestAPIEndpointDef::ENDPOINT_CALLBACK, 
                        RestAPIEndpointDef::ENDPOINT_GET, 
                        std::bind(&PiCoProcessor::apiQueryPiStatus, this,
                                std::placeholders::_1, std::placeholders::_2),
                        "Query Pi Status");
    endpointManager.addEndpoint("querycurmc",
                        RestAPIEndpointDef::ENDPOINT_CALLBACK, 
                        RestAPIEndpointDef::ENDPOINT_GET, 
                        std::bind(&PiCoProcessor::apiQueryCurMc, this,
                                std::placeholders::_1, std::placeholders::_2),
                        "Query machine");                        
    endpointManager.addEndpoint("setmcjson", 
                        RestAPIEndpointDef::ENDPOINT_CALLBACK, 
                        RestAPIEndpointDef::ENDPOINT_POST,
                        std::bind(&PiCoProcessor::apiSetMcJson, this, 
                                std::placeholders::_1, std::placeholders::_2),
                        "Set Machine JSON", 
                        "application/json", 
                        NULL, 
                        RestAPIEndpointDef::ENDPOINT_CACHE_NEVER,
                        NULL,
                        std::bind(&PiCoProcessor::apiSetMcJsonContent, this, 
                                std::placeholders::_1, std::placeholders::_2, 
                                std::placeholders::_3, std::placeholders::_4,
                                std::placeholders::_5),
                        NULL);
    endpointManager.addEndpoint("targetcmd", 
                        RestAPIEndpointDef::ENDPOINT_CALLBACK, 
                        RestAPIEndpointDef::ENDPOINT_GET, 
                        std::bind(&PiCoProcessor::apiTargetCommand, this,
                                std::placeholders::_1, std::placeholders::_2),
                        "Target command (GET)");
    endpointManager.addEndpoint("targetcmd", 
                        RestAPIEndpointDef::ENDPOINT_CALLBACK, 
                        RestAPIEndpointDef::ENDPOINT_POST,
                        std::bind(&PiCoProcessor::apiTargetCommandPost, this, 
                                std::placeholders::_1, std::placeholders::_2),
                        "Target command (POST)", 
                        "application/json", 
                        NULL, 
                        RestAPIEndpointDef::ENDPOINT_CACHE_NEVER,
                        NULL,
                        std::bind(&PiCoProcessor::apiTargetCommandPostContent, this, 
                                std::placeholders::_1, std::placeholders::_2, 
                                std::placeholders::_3, std::placeholders::_4,
                                std::placeholders::_5),
                        NULL);
    endpointManager.addEndpoint("sendfiletotargetbuffer", 
                        RestAPIEndpointDef::ENDPOINT_CALLBACK, 
                        RestAPIEndpointDef::ENDPOINT_GET, 
                        std::bind(&PiCoProcessor::apiSendFileToTargetBuffer, this,
                                std::placeholders::_1, std::placeholders::_2),
                        "Send file to target buffer - from file system to target buffer");                            
    endpointManager.addEndpoint("appendfiletotargetbuffer", 
                        RestAPIEndpointDef::ENDPOINT_CALLBACK, 
                        RestAPIEndpointDef::ENDPOINT_GET, 
                        std::bind(&PiCoProcessor::apiAppendFileToTargetBuffer, this,
                                std::placeholders::_1, std::placeholders::_2),
                        "Append file to target buffer - from file system to target buffer");                            
    endpointManager.addEndpoint("runfileontarget", 
                        RestAPIEndpointDef::ENDPOINT_CALLBACK, 
                        RestAPIEndpointDef::ENDPOINT_GET, 
                        std::bind(&PiCoProcessor::apiRunFileOnTarget, this,
                                std::placeholders::_1, std::placeholders::_2),
                        "Run file on target - /fileSystem/filename - from file system");      
    // endpointManager.addEndpoint("uploadappend", 
    //                     RestAPIEndpointDef::ENDPOINT_CALLBACK, 
    //                     RestAPIEndpointDef::ENDPOINT_POST,
    //                     std::bind(&PiCoProcessor::apiUploadAppendComplete, this, 
    //                             std::placeholders::_1, std::placeholders::_2),
    //                     "Upload file", 
    //                     "application/json", 
    //                     NULL, 
    //                     RestAPIEndpointDef::ENDPOINT_CACHE_NEVER,
    //                     NULL,
    //                     NULL,
    //                     std::bind(&PiCoProcessor::apiUploadAppendPart, this, 
    //                             std::placeholders::_1, std::placeholders::_2, 
    //                             std::placeholders::_3, std::placeholders::_4,
    //                             std::placeholders::_5, std::placeholders::_6,
    //                             std::placeholders::_7));   
    // endpointManager.addEndpoint("uploadtofileman", 
    //                     RestAPIEndpointDef::ENDPOINT_CALLBACK, 
    //                     RestAPIEndpointDef::ENDPOINT_POST,
    //                     std::bind(&PiCoProcessor::apiUploadToFileManComplete, this, 
    //                             std::placeholders::_1, std::placeholders::_2),
    //                     "Upload file", 
    //                     "application/json", 
    //                     NULL, 
    //                     RestAPIEndpointDef::ENDPOINT_CACHE_NEVER,
    //                     NULL,
    //                     NULL,
    //                     std::bind(&PiCoProcessor::apiUploadToFileManPart, this, 
    //                             std::placeholders::_1, std::placeholders::_2, 
    //                             std::placeholders::_3, std::placeholders::_4,
    //                             std::placeholders::_5, std::placeholders::_6,
    //                             std::placeholders::_7));                                    
    // endpointManager.addEndpoint("uploadandrun", 
    //                     RestAPIEndpointDef::ENDPOINT_CALLBACK, 
    //                     RestAPIEndpointDef::ENDPOINT_POST,
    //                     std::bind(&PiCoProcessor::apiUploadAndRunComplete, this, 
    //                             std::placeholders::_1, std::placeholders::_2),
    //                     "Upload and run file", 
    //                     "application/json", 
    //                     NULL, 
    //                     RestAPIEndpointDef::ENDPOINT_CACHE_NEVER,
    //                     NULL,
    //                     NULL,
    //                     std::bind(&PiCoProcessor::apiUploadAndRunPart, this, 
    //                             std::placeholders::_1, std::placeholders::_2, 
    //                             std::placeholders::_3, std::placeholders::_4,
    //                             std::placeholders::_5, std::placeholders::_6,
    //                             std::placeholders::_7));                          
    // endpointManager.addEndpoint("sendKey", 
    //                     RestAPIEndpointDef::ENDPOINT_CALLBACK, 
    //                     RestAPIEndpointDef::ENDPOINT_GET, 
    //                     std::bind(&PiCoProcessor::apiSendKey, this,
    //                             std::placeholders::_1, std::placeholders::_2),
    //                     "Send key from terminal window");    

    // Stash endoint manager
    _pRestAPIEndpointManager = &endpointManager;
}

void PiCoProcessor::addProtocolEndpoints(ProtocolEndpointManager &endpointManager)
{
    // Record endpointManager
	_pEndpointManager = &endpointManager;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// API request - Health
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PiCoProcessor::apiQueryESPHealth(const String &reqStr, String &respStr)
{
#ifdef DEBUG_PI_QUERY_ESP_HEALTH
    LOG_I(MODULE_PREFIX, "queryESPHealth %s", reqStr.c_str());
#endif
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
// API request - Upload pi-sw - completed
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PiCoProcessor::apiUploadPiSwComplete(const String &reqStr, String &respStr)
{
#ifdef DEBUG_PI_SW_UPLOAD
    LOG_I(MODULE_PREFIX, "apiUploadPiSwComplete %s", reqStr.c_str());
#endif
    Utils::setJsonBoolResult(reqStr.c_str(), respStr, true);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// API request - Upload pi-sw - part of file (from HTTP POST file)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PiCoProcessor::apiUploadPiSwPart(const String& req, FileBlockInfo& fileBlockInfo)
{
#ifdef DEBUG_PI_SW_UPLOAD
    LOG_I(MODULE_PREFIX, "apiUploadPiSwPart %d, %d, %d, %d", contentLen, index, len, finalBlock);
#endif
    uploadAPIBlockHandler("firmware", req, fileBlockInfo);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// API request - Query Pi Status
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PiCoProcessor::apiQueryPiStatus(const String &reqStr, String &respStr)
{
    respStr = _cachedPiStatusJSON.c_str();
    // LOG_I(MODULE_PREFIX, "apiQueryStatus %s resp %s", reqStr.c_str(), respStr.c_str());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// API request - Query current machine
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PiCoProcessor::apiQueryCurMc(const String &reqStr, String &respStr)
{
    configGetConfig().getConfigString(respStr);
    LOG_I(MODULE_PREFIX, "apiQueryCurMc %s returning %s", reqStr.c_str(), respStr.c_str());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// API request - Set Machine
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PiCoProcessor::apiSetMcJson(const String &reqStr, String &respStr)
{
    LOG_I(MODULE_PREFIX, "apiSetMcJson %s", reqStr.c_str());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// API request - Set Machine Content
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PiCoProcessor::apiSetMcJsonContent(const String &reqStr, const uint8_t *pData, size_t len, size_t index, size_t total)
{
    // Extract JSON
    static const int MAX_JSON_DATA_LEN = 1000;
    char jsonData[MAX_JSON_DATA_LEN];
    size_t toCopy = len+1;
    if (len > MAX_JSON_DATA_LEN)
        toCopy = MAX_JSON_DATA_LEN;
    strlcpy(jsonData, (const char*) pData, toCopy);
    // Debug
    LOG_I(MODULE_PREFIX, "apiSetMcJsonContent %s json %s",  
            reqStr.c_str(), jsonData);
    // Store in non-volatile so we can pick back up with same machine
    SysModBase::configSaveData(String(jsonData));
    // Send to the Pi
    sendTargetData("setmcjson", pData, len, 0);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// API request - Target command
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PiCoProcessor::apiTargetCommand(const String &reqStr, String &respStr)
{
    // Get command
    String targetCmd = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1);
    // Log.trace("%sapiTargetCommand %s\n", MODULE_PREFIX, targetCmd.c_str());
    sendTargetCommand(targetCmd, reqStr, respStr, true);
}

void PiCoProcessor::apiTargetCommandPost(const String &reqStr, String &respStr)
{
}

void PiCoProcessor::apiTargetCommandPostContent(const String &reqStr, const uint8_t *pData, size_t len, size_t index, size_t total)
{
    // Get command
    String targetCmd = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1);
    // Extract JSON
    static const int MAX_JSON_DATA_LEN = 1000;
    char jsonData[MAX_JSON_DATA_LEN];
    size_t toCopy = len+1;
    if (len > MAX_JSON_DATA_LEN)
        toCopy = MAX_JSON_DATA_LEN;
    strlcpy(jsonData, (const char*) pData, toCopy);
    // Debug
    // Log.trace("%sapiTargetCommandPostContent %s json %s\n", MODULE_PREFIX, 
    //         reqStr.c_str(), jsonData);
    // Send to the Pi
    sendTargetData(targetCmd.c_str(), pData, len, 0);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// API target file handling
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PiCoProcessor::apiSendFileToTargetBuffer(const String &reqStr, String &respStr)
{
    // Clear target first
    sendTargetCommand("ClearTarget", reqStr, respStr, true);
    // File system
    String fileSystemStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1);
    // Filename
    String filename = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 2);
    LOG_I(MODULE_PREFIX, "apiSendFileToBuf filename %s", filename.c_str());
    startUploadFromFileSystem(fileSystemStr, reqStr, filename, "");
}

void PiCoProcessor::apiAppendFileToTargetBuffer(const String &reqStr, String &respStr)
{
    // File system
    String fileSystemStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1);
    // Filename        
    String filename = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 2);
    LOG_I(MODULE_PREFIX, "apiAppendFileToBuf filename %s", filename.c_str());
    startUploadFromFileSystem(fileSystemStr, reqStr, filename, "");
}

void PiCoProcessor::apiRunFileOnTarget(const String &reqStr, String &respStr)
{
    // Clear target first
    sendTargetCommand("ClearTarget", reqStr, respStr, false);
    // File system
    String fileSystemStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1);
    // Filename        
    String filename = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 2);
    LOG_I(MODULE_PREFIX, "runFileOnTarget filename %s", filename.c_str());
    startUploadFromFileSystem(fileSystemStr, "", filename, "ProgramAndReset");
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
// Detect hardware version
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PiCoProcessor::detectHardwareVersion()
{
    // Test pins for each version
    int testInPins[] = { HW_VERSION_DETECT_V22_IN_PIN, HW_VERSION_DETECT_V20_IN_PIN };
    int testHwVersions[] { 22, 20 };

    // V1.7 has no pins connected
    _hwVersion = 17;

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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// File send elements
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PiCoProcessor::sendFileStartRecord(const char* fileType, const String& req, const String& filename, int fileLength)
{
    String reqParams = Utils::getJSONFromHTTPQueryStr(req.c_str(), true);
    char msgStr[600];
    snprintf(msgStr, sizeof(msgStr), R"({"cmdName":"ufStart","fileType":"%s","fileName":"%s","fileLen":%d%s%s})",
                    fileType, 
                    filename.c_str(),
                    fileLength,
                    reqParams.length() > 0 ? "," : "",
                    reqParams.length() > 0 ? reqParams.c_str() : "");
    sendMsgStrToPi(msgStr);
    vTaskDelay(50);
}

void PiCoProcessor::sendFileBlock(size_t index, const uint8_t *pData, size_t len)
{
    std::vector<uint8_t> msgData;
    char msgHeader[100];
    snprintf(msgHeader, sizeof(msgHeader),
            R"({"cmdName":"ufBlock","index":%d,"len":%d})", 
            index, len);
    unsigned headerLen = strlen(msgHeader);
    unsigned msgStrPlusPayloadLen = headerLen + 1 + len;
    msgData.resize(msgStrPlusPayloadLen);
    if (msgData.size() >= msgStrPlusPayloadLen)
    {
        LOG_I(MODULE_PREFIX, "sendFileBlock blockLen %d headerLenExclTerm %d totalLen %d msgHeader %s msgDataSize %d", 
                len, headerLen, msgStrPlusPayloadLen, msgHeader, msgData.size());         
        memcpy(msgData.data(), msgHeader, headerLen + 1);
        memcpy(msgData.data() + headerLen + 1, pData, len);
        sendMsgAndPayloadToPi(msgData.data(), msgStrPlusPayloadLen);
    }
#ifdef DEBUG_PI_SEND_FILE_BLOCK
    LOG_I(MODULE_PREFIX, "sendFileBlock blockLen %d headerLenExclTerm %d totalLen %d msgHeader %s", 
                len, headerLen, msgStrPlusPayloadLen, msgHeader); 
#endif
}

void PiCoProcessor::sendFileEndRecord(int blockCount, const char* pAdditionalJsonNameValues)
{
    char msgStr[300];
    snprintf(msgStr, sizeof(msgStr), R"({"cmdName":"ufEnd","blockCount":%d,"crc":"0x%04x"%s%s})",
                    blockCount, _fileCRC,
                    pAdditionalJsonNameValues ? "," : "",
                    pAdditionalJsonNameValues ? pAdditionalJsonNameValues : "");
    sendMsgStrToPi(msgStr);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send response to co-processor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PiCoProcessor::sendResponseToPi(const String& reqStr, String& msgJson)
{
#ifdef DEBUG_PI_SEND_RESP_TO_PI
    LOG_I(MODULE_PREFIX, "req %s ... response Msg %s", reqStr.c_str(), msgJson.c_str());
#endif

    bool hasBraces = msgJson.startsWith("{");
    char msgStr[300];
    snprintf(msgStr, sizeof(msgStr), R"({"cmdName":"%sResp","msg":%s%s%s})",
                    reqStr.c_str(),
                    hasBraces ? "" : "{",
                    msgJson.c_str(),
                    hasBraces ? "" : "}");
    sendMsgStrToPi(msgStr);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send command to target
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool PiCoProcessor::sendTargetCommand(const String& targetCmd, const String& reqStr, String& respStr, bool waitForResponse)
{
#ifdef DEBUG_PI_SEND_TARGET_CMD
    LOG_I(MODULE_PREFIX, "sendTargetCommand Msg %s", targetCmd.c_str());
#endif

    // Clear response flag
    _cmdResponseNew = false;

    // Form command
    char msgStr[300];
    snprintf(msgStr, sizeof(msgStr), R"({"cmdName":"%s","reqStr":"%s"})",
                    targetCmd.c_str(),
                    reqStr.c_str());
    sendMsgStrToPi(msgStr);

    // Return if no response required
    if (!waitForResponse)
    {
        Utils::setJsonBoolResult(reqStr.c_str(), respStr, true);
        return true;
    }

    // Wait for a response (or time-out)
    uint32_t timeStarted = millis();
    while (!Utils::isTimeout(millis(), timeStarted, MAX_WAIT_FOR_CMD_RESPONSE_MS))
    {
        service();
        if (_cmdResponseNew)
        {
            respStr = _cmdResponseBuf;
            return true;
        }
    }
    Utils::setJsonBoolResult(reqStr.c_str(), respStr, false);
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send data to target
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PiCoProcessor::sendTargetData(const String& cmdName, const uint8_t* pData, int len, int index)
{
    std::vector<uint8_t> msgData;
    char msgHeader[100];
    snprintf(msgHeader, sizeof(msgHeader),
            R"({"cmdName":"%s","msgIdx":"%d","dataLen":"%d"})", 
            cmdName.c_str(), index, len);
    unsigned headerLen = strlen(msgHeader);
    unsigned msgStrPlusPayloadLen = headerLen + 1 + len;
    msgData.resize(msgStrPlusPayloadLen);
    if (msgData.size() >= msgStrPlusPayloadLen)
    {
        memcpy(msgData.data(), msgHeader, headerLen + 1);
        memcpy(msgData.data() + headerLen + 1, pData, len);
        sendMsgAndPayloadToPi(msgData.data(), headerLen + len + 1);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send to co-processor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PiCoProcessor::sendMsgStrToPi(const char *pMsgStr)
{
    // Include string terminator
    sendMsgAndPayloadToPi((const uint8_t*) pMsgStr, strlen(pMsgStr)+1);
}

void PiCoProcessor::sendMsgAndPayloadToPi(const uint8_t *pFrame, int frameLen)
{
#ifdef DEBUG_PI_SEND_FRAME
    LOG_I(MODULE_PREFIX, "sendToPi frameLen %d", frameLen);
#endif
    _pHDLC->sendFrame(pFrame, frameLen);
    _statsTxFr++;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HDLC frame received from Pi
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PiCoProcessor::hdlcFrameRxFromPiCB(const uint8_t* pFrame, int frameLen)
{
#ifdef DEBUG_PI_RX_FRAME
    // Debug
    LOG_I(MODULE_PREFIX, "Frame received len %d", frameLen);
#endif

    // Check valid
    if (frameLen < 0)
        return;
    _statsRxFr++;

    // Buffer is null terminated
    const char* pRxStr = (const char*)pFrame;

    // Log.trace("%s<- %s\n", MODULE_PREFIX, pRxStr);

    // Get command
    String cmdName = RdJson::getString("cmdName", "", pRxStr);

    // Check for file transfer related messages
    if ((cmdName.equalsIgnoreCase("ufStartAck")))
    {
        _uploadStartAck = true;
#ifdef DEBUG_PI_UPLOAD_ACKS
        LOG_I(MODULE_PREFIX, "ufStartAck %s", pRxStr);
#endif
    }
    else if ((cmdName.equalsIgnoreCase("ufBlockAck")))
    {
        _uploadBlockRxIndex = RdJson::getLong("index", 0, pRxStr);
#ifdef DEBUG_PI_UPLOAD_ACKS_DETAIL
        LOG_I(MODULE_PREFIX, "ufBlockAck %s", pRxStr);
#endif
    }
    else if ((cmdName.equalsIgnoreCase("ufEndAck")))
    {
        _uploadEndAck = true;
#ifdef DEBUG_PI_UPLOAD_ACKS
        LOG_I(MODULE_PREFIX, "ufEndAck %s", pRxStr);
#endif
    }
    else if ((cmdName.equalsIgnoreCase("ufEndNotAck")))
    {
        _uploadEndNotAck = true;
#ifdef DEBUG_PI_UPLOAD_ACKS
        LOG_I(MODULE_PREFIX, "ufEndNotAck %s", pRxStr);
#endif
    }

    // Check if transferring files
    if (_uploadFromAPIInProgress || _uploadFromFSInProgress)
        return;

    // Check other messages
    if (cmdName.equalsIgnoreCase("getStatusResp"))
    {
        // Cache the status frame
        _cachedPiStatusJSON = pRxStr;
#ifdef DEBUG_PI_RX_STATUS_UPDATE
        LOG_I(MODULE_PREFIX, "Received status update %s", _cachedPiStatusJSON.c_str());
#endif
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
#ifdef DEBUG_PI_RX_API_REQ
            LOG_I(MODULE_PREFIX, "hdlcFrameRxFromPiCB apiReq");
#endif
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
        uint32_t msgIdx = RdJson::getLong("msgIdx", 0, pRxStr);

        // String inStr = (const char*)(pFrame+payloadStartPos);
        // if (inStr.indexOf("validatorStatus") > 0)
        //     _rdpValStatCount++;
        if ((cmdName.equalsIgnoreCase("rdp")))
        {
#ifdef DEBUG_RDP_MSG_FROM_PI
            uint32_t dataLen = RdJson::getLong("dataLen", payloadLen, pRxStr);
            String payloadStr;
            Utils::strFromBuffer(pFrame+payloadStartPos, payloadLen, payloadStr, true);
            // String payloadStr = (((const char*)pFrame)+payloadStartPos);
            payloadStr.replace("\n", "\\n");
            LOG_I(MODULE_PREFIX, "rdp <- %s payloadLen %d dataLen %d payload ¬¬%s¬¬", 
                        pRxStr, payloadLen, dataLen,
                        payloadStr.c_str());
#endif

            // Send to websocket
            if (payloadLen > 0)
            {
                // Send the response back
                RICRESTMsg ricRESTRespMsg;
                ProtocolEndpointMsg endpointMsg;
                ricRESTRespMsg.encode(pFrame + payloadStartPos, payloadLen, endpointMsg, 
                            RICRESTMsg::RICREST_REST_ELEM_CMDRESPJSON);
                endpointMsg.setAsResponse(_rdpChannelId, MSG_PROTOCOL_RICREST, msgIdx, MSG_DIRECTION_RESPONSE);

                // Send message on the appropriate channel
                if (_pEndpointManager)
                    _pEndpointManager->handleOutboundMessage(endpointMsg);
            // _miniHDLCForRDPTCP.sendFrame(pFrame+payloadStartPos, dataLen);
            }
        }
        else
        {
            // Log.trace("%szesarux <- %s payloadLen %d payload ¬¬%s¬¬\n", 
            //             MODULE_PREFIX, pRxStr, payloadLen,
            //             payloadStr.c_str());
            // _pZEsarUXTCPServer->sendChars(pFrame+payloadStartPos, dataLen);
        }
    }
    else if (cmdName.equalsIgnoreCase("log"))
    {
        // Extract msg
        String logMsg = RdJson::getString("msg", "", pRxStr);
        String msgSrc = RdJson::getString("src", "", pRxStr);
        String msgLev = RdJson::getString("lev", "", pRxStr);
        LOG_I(msgSrc.c_str(), "%s: %s", msgLev.c_str(), logMsg.c_str());
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
        // Responses
        _cmdResponseNew = true;
        _cmdResponseBuf = pRxStr;
        LOG_I(MODULE_PREFIX, "RespMessageReceived %s\n", pRxStr);
    }
}

void PiCoProcessor::hdlcFrameTxToPiCB(const uint8_t* pFrame, int frameLen)
{
#ifdef DEBUG_PI_TX_FRAME_TO_PI
    // Debug
    Utils::logHexBuf(pFrame, frameLen, MODULE_PREFIX, "Frame to send");
#endif
    // Send the message
    int bytesSent = uart_write_bytes((uart_port_t)_uartNum, 
                        (const char*)pFrame, frameLen);
    if (bytesSent != frameLen)
    {
        LOG_W(MODULE_PREFIX, "hdlcFrameTxToPiCB len %d only wrote %d bytes",
                frameLen, bytesSent);
    }
    _statsTxCh += bytesSent;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle upload block
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PiCoProcessor::uploadAPIBlockHandler(const char* fileType, const String& req, FileBlockInfo& fileBlockInfo)
{
    // Check there isn't an upload in progress from FS
    if (_uploadFromFSInProgress)
    {
        LOG_W(MODULE_PREFIX, "uploadAPIBlockHandler upload already in progress");
        return;
    }

    // Check upload from API already in progress
    if (!_uploadFromAPIInProgress)
    {
        if (fileBlockInfo.filePos != 0)
            return;
        // Upload now in progress
        _uploadLastBlockMs = millis();
        _uploadFromAPIInProgress = true;
        _uploadBlockCount = 0;
        _uploadFilePos = 0;
        _uploadStartMs = millis();
        LOG_I(MODULE_PREFIX, "uploadAPIBlockHandler starting new fileType %s filename %s fileLenValid %d fileLen %d contentLen %d pos %d, blockLen %d",
                fileType, fileBlockInfo.filename,
                fileBlockInfo.fileLenValid, fileBlockInfo.fileLen, fileBlockInfo.contentLen,
                fileBlockInfo.filePos, fileBlockInfo.blockLen);
    }
    
    // Commmon handler
    if (!uploadCommonBlockHandler(fileType, req, fileBlockInfo))
    {
        _uploadFromAPIInProgress = false;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Common upload block handler
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool PiCoProcessor::uploadCommonBlockHandler(const char* fileType, const String& req, FileBlockInfo& fileBlockInfo)
{
#ifdef DEBUG_PI_UPLOAD_COMMON_BLOCK_DETAIL
    LOG_I(MODULE_PREFIX, "uploadCommonBlockHandler filePos %d blkCnt %d blkLen %d isFinal %d rxIdx %d bytesSent %d", 
                fileBlockInfo.filePos, _uploadBlockCount, fileBlockInfo.blockLen, fileBlockInfo.finalBlock,
                _uploadBlockRxIndex, _uploadBytesSent);
#endif

    // For timeouts        
    _uploadLastBlockMs = millis();

    // Check if first block in an upload
    if (_uploadBlockCount == 0)
    {
        // CRC calculation
        _fileCRC = MiniHDLC::crcInitCCITT();
        _uploadFileType = fileType;

        // Debug
        LOG_I(MODULE_PREFIX, "uploadCommonBlockHandler new upload millis %ld filetype %s fileName %s fileLenValid %d fileLen %d contentLen %d blockLen %d final %d isFS %s isAPI %s", 
                _uploadLastBlockMs, fileType, fileBlockInfo.filename, 
                fileBlockInfo.fileLenValid, fileBlockInfo.fileLen, fileBlockInfo.contentLen,
                fileBlockInfo.blockLen, fileBlockInfo.finalBlock,
                (_uploadFromFSInProgress ? "yes" : "no"), 
                (_uploadFromAPIInProgress ? "yes" : "no"));

        // Init acks
        _uploadStartAck = false;
        _uploadBlockRxIndex = 0;
        _uploadEndAck = false;
        _uploadEndNotAck = false;
        _uploadBytesSent = 0;

        // Send file start
        for (int retryCount = 0; retryCount < UPLOAD_MAX_RESENDS_BEFORE_FAIL; retryCount++)
        {
            sendFileStartRecord(fileType, req, fileBlockInfo.filename, 
                        fileBlockInfo.fileLenValid ? fileBlockInfo.fileLen : fileBlockInfo.contentLen);
            if (waitForStartAck())
                break;
            LOG_I(MODULE_PREFIX, "uploadCommonBlockHandler retry %d upload start", retryCount+1);
        }
        if (!waitForStartAck())
            return false;
    }

    // Send the block
    for (int retryCount = 0; retryCount < UPLOAD_MAX_RESENDS_BEFORE_FAIL; retryCount++)
    {
        sendFileBlock(fileBlockInfo.filePos, fileBlockInfo.pBlock, fileBlockInfo.blockLen);
        if (waitForBlockAck(fileBlockInfo.filePos))
            break;
        LOG_I(MODULE_PREFIX, "uploadCommonBlockHandler retry %d upload block %d", 
                    retryCount+1, _uploadBlockCount);
    }
    if (!waitForBlockAck(fileBlockInfo.filePos))
        return false;
    _uploadBlockCount++;

    // Update CRC
    _fileCRC = MiniHDLC::crcUpdateCCITT(_fileCRC, fileBlockInfo.pBlock, fileBlockInfo.blockLen);
    _uploadBytesSent += fileBlockInfo.blockLen;

    // Check if that was the final block
    if (fileBlockInfo.finalBlock)
    {
        for (int retryCount = 0; retryCount < UPLOAD_MAX_RESENDS_BEFORE_FAIL; retryCount++)
        {
            sendFileEndRecord(_uploadBlockCount, NULL);
            if (waitForEndAck())
                break;
            LOG_I(MODULE_PREFIX, "uploadCommonBlockHandler retry %d upload end", retryCount+1);
        }
        if (!waitForEndAck())
            return false;

#ifdef DEBUG_PI_UPLOAD_END
        LOG_I(MODULE_PREFIX, "uploadCommonBlockHandler file end sent CRC of whole file %04x", _fileCRC);
#endif
        if (_uploadTargetCommandWhenComplete.length() != 0)
        {
            String respStr;
            sendTargetCommand(_uploadTargetCommandWhenComplete, "", respStr, false);
            LOG_I(MODULE_PREFIX, "uploadCommonBlockHandler post-upload target command sent %s",
                    _uploadTargetCommandWhenComplete.c_str());
        }
        else
        {
            LOG_I(MODULE_PREFIX, "uploadCommonBlockHandler post-upload no target command requested");
        }
        _uploadTargetCommandWhenComplete = "";
        _uploadFromFSInProgress = false;
        _uploadFromAPIInProgress = false;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Upload a file from the file system
// Request is in the format of HTTP query parameters (e.g. "?baseAddr=1234")
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool PiCoProcessor::startUploadFromFileSystem(const String& fileSystemName, 
                const String& uploadRequest, const String& filename,
                const char* pTargetCmdWhenDone)
{
    // Check no upload is already happening
    if (uploadInProgress())
    {
        LOG_W(MODULE_PREFIX, "startUploadFromFileSystem upload already in progress");
        return false;
    }

    // Start a chunked file session
    if (!_chunker.start(filename, UPLOAD_BLOCK_SIZE_BYTES, false))
    {
        LOG_I(MODULE_PREFIX, "startUploadFromFileSystem failed to start %s", filename.c_str());
        return false;
    }

    // Upload now in progress
    LOG_I(MODULE_PREFIX, "startUploadFromFileSystem %s", filename.c_str());
    _uploadFromFSInProgress = true;
    _uploadFileType = "target";
    _uploadFromFSRequest = uploadRequest;
    _uploadBlockCount = 0;
    _uploadFilePos = 0;
    _uploadStartMs = millis();
    _uploadLastBlockMs = millis();
    if (pTargetCmdWhenDone)
        _uploadTargetCommandWhenComplete = pTargetCmdWhenDone;
    else
        _uploadTargetCommandWhenComplete = "";
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Process RICRESTMsg CmdFrame - coming from websocket
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool PiCoProcessor::procRICRESTCmdFrame(const String& cmdName, const RICRESTMsg& ricRESTReqMsg, 
                        String& respMsg, uint32_t channelID)
{
    // Handle command frames
    if (cmdName.equalsIgnoreCase("comtest"))
    {
        ConfigBase cmdFrame = ricRESTReqMsg.getPayloadJson();
        unsigned msgIdx = cmdFrame.getLong("msgIdx", 0);
        // Response
        char extraJson[100];
        snprintf(extraJson, sizeof(extraJson), "\"cmdName\":\"%sResp\",\"msgIdx\":%d", cmdName.c_str(), msgIdx);
        Utils::setJsonResult(ricRESTReqMsg.getReq().c_str(), respMsg, true, NULL, extraJson);

#ifdef DEBUG_RICREST_CMD_FRAMES
        LOG_I(MODULE_PREFIX, "processRICRESTCmdFrame %s => Resp %s", cmdName.c_str(), respMsg.c_str());
#endif
        return true;
    }

    // Send on to Pi as a combined message
    std::vector<uint8_t> combinedMsg;
    unsigned jsonLen = ricRESTReqMsg.getPayloadJson().length();
    unsigned combinedLen = jsonLen + 1 + ricRESTReqMsg.getBinLen();
    combinedMsg.resize(combinedLen);
    if (combinedMsg.size() >= combinedLen)
    {
        memcpy(combinedMsg.data(), ricRESTReqMsg.getPayloadJson().c_str(), jsonLen + 1);
        memcpy(combinedMsg.data() + jsonLen + 1, ricRESTReqMsg.getBinBuf(), ricRESTReqMsg.getBinLen());
        sendTargetData("rdp", combinedMsg.data(), combinedLen, _rdpCommandIndex++);
        _rdpChannelId  = channelID;
        return true;
    }

#ifdef DEBUG_RICREST_CMD_FRAMES
    LOG_I(MODULE_PREFIX, "processRICRESTCmdFrame UNKNOWN %s", cmdName.c_str());
#endif

    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Wait for acknowledgement
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool PiCoProcessor::waitForStartAck()
{
    uint32_t waitStartMs = millis();
    while (!Utils::isTimeout(millis(), waitStartMs, UPLOAD_MAX_WAIT_FOR_ACK_BEFORE_RESEND_MS))
    {
        serialInterfacePump();
        if (_uploadStartAck)
            return true;
    }
    return false;
}

bool PiCoProcessor::waitForBlockAck(size_t index)
{
    uint32_t waitStartMs = millis();
    while (!Utils::isTimeout(millis(), waitStartMs, UPLOAD_MAX_WAIT_FOR_ACK_BEFORE_RESEND_MS))
    {
        serialInterfacePump();
        if (_uploadBlockRxIndex == index)
            return true;
    }
    return false;
}

bool PiCoProcessor::waitForEndAck()
{
    uint32_t waitStartMs = millis();
    while (!Utils::isTimeout(millis(), waitStartMs, UPLOAD_MAX_WAIT_FOR_ACK_BEFORE_RESEND_MS))
    {
        serialInterfacePump();
        if (_uploadEndAck)
            return true;
        else if (_uploadEndNotAck)
        {
            _uploadEndNotAck = false;
            return false;
        }
    }
    return false;
}

void PiCoProcessor::serialInterfacePump()
{
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
#ifdef DEBUG_PI_SERIAL_GET
            LOG_I(MODULE_PREFIX, "service charsAvail %d ch %02x", numCharsAvailable, buf[0]);
#endif
            if (_pHDLC)
                _pHDLC->handleBuffer(buf, bytesRead);
        }
        _statsRxCh += bytesRead;
    }
}
