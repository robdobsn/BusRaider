// Machine Interface
// Rob Dobson 2018

// Provides some wiring for the Bus Raider
// The ESP32 has three serial interfaces:
// - programming/logging
// - Pi
// - Target serial (e.g. Z80 68B50)

// The CommandSerial.h file contains code which communicates with the Pi using and HDLC frame protocol
// when a frame is ready from the Pi it is handled by this MachineInterface
// This MachineInterface also handles the target serial port communication including telnet support

#include "MachineInterface.h"
#include "ConfigPinMap.h"

static const char* MODULE_PREFIX = "MachineInterface: ";

static const char* demoModeControlFileName = "demo.json";
static const int demoModeControlFileMaxlen = 2000;

MachineInterface::MachineInterface() : 
        _miniHDLCForRDPTCP(std::bind(&MachineInterface::hdlcTxCharTCP, this, std::placeholders::_1), 
            std::bind(&MachineInterface::hdlcRxFrameTCP, this, std::placeholders::_1, std::placeholders::_2),
            true, false)
{
    _cachedStatusJSON = "{}";
    _pTargetSerial = NULL;
    _targetSerialPortNum = -1;
    _targetSerialBaudRate = 115200;
    _pWebServer = NULL;
    _pFileManager = NULL;
    _pTelnetServer = NULL;
    _pTCPHDLCServer = NULL;
    _pZEsarUXTCPServer = NULL;
    _pRestAPIEndpoints = NULL;
    _demoState = DEMO_STATE_IDLE;
    _demoPreloadFileIdx = 0;
    _demoProgramIdx = 0;
    _rdpCommandIndex = 0;
    _zesaruxCommandIndex = 0;
    _cachedStatusRequestMs = 0;
    _cmdResponseNew = false;
    // Assume hardware version until detected
    _hwVersion = ESP_HW_VERSION_DEFAULT;
}

// Setup
void MachineInterface::setup(ConfigBase &config, 
            WebServer *pWebServer, 
            CommandSerial* pCommandSerial,
            AsyncTelnetServer* pTelnetServer, 
            RemoteDebugProtocolServer* pZEsarUXTCPServer, 
            RemoteDebugProtocolServer* pTCPHDLCServer, 
            RestAPIEndpoints* pRestAPIEndpoints, 
            FileManager* pFileManager)
{
    // Get config
    ConfigBase csConfig(config.getString("machineIF", "").c_str());
    Log.trace("%sconfig %s\n", MODULE_PREFIX, csConfig.getConfigCStrPtr());

    // Save webserver and command serial
    _pWebServer = pWebServer;
    _pCommandSerial = pCommandSerial;
    _pTelnetServer = pTelnetServer;
    _pZEsarUXTCPServer = pZEsarUXTCPServer;
    _pTCPHDLCServer = pTCPHDLCServer;
    _pRestAPIEndpoints = pRestAPIEndpoints;
    _pFileManager = pFileManager;

    // Hardware version detection
    pinMode(HW_VERSION_DETECT_IN_PIN, INPUT_PULLUP);
    pinMode(HW_VERSION_DETECT_OUT_PIN, OUTPUT);
    digitalWrite(HW_VERSION_DETECT_OUT_PIN, 0);
    bool hwIn0Value = digitalRead(HW_VERSION_DETECT_IN_PIN);
    digitalWrite(HW_VERSION_DETECT_OUT_PIN, 1);
    bool hwIn1Value = digitalRead(HW_VERSION_DETECT_IN_PIN);
    // Check if IN and OUT pins tied together - if setting out to 0
    // makes in 0 then they are (as input is pulled-up otherwise)
    _hwVersion = 17;
    if (hwIn0Value != hwIn1Value)
        _hwVersion = 20;
    Log.trace("%sHW version detect wrote 1 got %d wrote 0 got %d so hwVersion = %d\n", MODULE_PREFIX, 
            hwIn0Value, hwIn1Value, _hwVersion);

    // Tidy up
    pinMode(HW_VERSION_DETECT_IN_PIN, INPUT);
    pinMode(HW_VERSION_DETECT_OUT_PIN, INPUT);

    // Set the telnet callback
    if (_pTelnetServer)
    {
        _pTelnetServer->onData([this](void* cbArg, const char* pData, int numChars) 
        {
            (void)cbArg;
            // Send chars
            const char* pBuf = pData;
            for (int i = 0; i < numChars; i++)
            {
                if (this->_pTargetSerial)
                    this->_pTargetSerial->write(*pBuf++);

            }
        }, this);
    }

    // Callback for TCP-HDLC server
    if (_pTCPHDLCServer)
    {
        _pTCPHDLCServer->onData([this](void* cbArg, const uint8_t* pData, int dataLen) 
        {
            (void)cbArg;

            // Process chars through HDLC
            for (int rxCtr = 0; rxCtr < dataLen; rxCtr++)
                _miniHDLCForRDPTCP.handleChar(pData[rxCtr]);

        }, this);
    }

    // Callback for TCP-HDLC server
    if (_pZEsarUXTCPServer)
    {
        _pZEsarUXTCPServer->onData([this](void* cbArg, const uint8_t* pData, int dataLen) 
        {
            (void)cbArg;

            if (_pCommandSerial)
                _pCommandSerial->sendTargetData("zesarux", pData, dataLen, 
                            _zesaruxCommandIndex++);
        }, this);
    }

    // Set callback on frame received from Pi
    if (_pCommandSerial)
    {
        _pCommandSerial->setCallbackOnRxFrame([this](const uint8_t *frameBuffer, int frameLength) 
        {
            // Log.trace("onDataFromPi len %s %d\n", frameBuffer, frameLength);
            handleFrameRxFromPi(frameBuffer, frameLength);
        });
    }

    // Get target serial port
    _targetSerialPortNum = csConfig.getLong("portNum", -1);
    _targetSerialBaudRate = csConfig.getLong("baudRate", 115200);

    // Debug
    Log.trace("%sportNum %d, baudRate %d\n", MODULE_PREFIX, _targetSerialPortNum, _targetSerialBaudRate);

    // Check target serial port
    if (_targetSerialPortNum == -1)
        return;

    // Setup target serial port
    _pTargetSerial = new HardwareSerial(_targetSerialPortNum);
    if (_pTargetSerial)
    {
        if (_targetSerialPortNum == 1)
        {
            _pTargetSerial->begin(_targetSerialBaudRate, SERIAL_8N1, 16, 17, false);
            Log.trace("%sportNum %d, baudRate %d, rxPin %d, txPin %d\n",
                        MODULE_PREFIX, _targetSerialPortNum, _targetSerialBaudRate, 16, 17);
            // Make rx pin a pullup to avoid noise if link unconnected
            pinMode(16, INPUT_PULLUP);
        }
        else if (_targetSerialPortNum == 2)
        {
            _pTargetSerial->begin(_targetSerialBaudRate, SERIAL_8N1, 26, 25, false);
            Log.trace("%sportNum %d, baudRate %d, rxPin %d, txPin %d\n",
                        MODULE_PREFIX, _targetSerialPortNum, _targetSerialBaudRate, 26, 25);
            // Make rx pin a pullup to avoid noise if link unconnected
            pinMode(26, INPUT_PULLUP);
        }
        else
        {
            _pTargetSerial->begin(_targetSerialBaudRate);
            Log.trace("%sportNum %d, baudRate %d, rxPin %d, txPin %d\n",
                        MODULE_PREFIX, _targetSerialPortNum, _targetSerialBaudRate, 3, 1);
            // Make rx pin a pullup to avoid noise if link unconnected
            pinMode(3, INPUT_PULLUP);
        }
        _pTargetSerial->setRxBufferSize(8192);
    }

    // Get demo button
    String demoPinStr = csConfig.getString("demoPin", "");
    int demoPin = -1;
    if (demoPinStr.length() != 0)
        demoPin = ConfigPinMap::getPinFromName(demoPinStr.c_str());
    if (demoPin != -1)
        _debounceButton.setup(demoPin, 0,
                    std::bind(&MachineInterface::handleDemoModeButtonPress, this, std::placeholders::_1));

    // Add web socket handlers
    String wsPath = csConfig.getString("wsPath", "");
    if (wsPath.length() > 0)
        _pWebServer->webSocketOpen(wsPath);
}

void MachineInterface::service()
{
    // Check cached status is ok
    if (_cachedStatusJSON.length() <= 2)
    {
        if ((_cachedStatusRequestMs == 0) || (Utils::isTimeout(millis(), _cachedStatusRequestMs, TIME_BETWEEN_STATUS_REQS_MS)))
        {
            // Request status
            if (_pCommandSerial)
                _pCommandSerial->sendTargetCommand("getStatus", "");
            // Log.trace("%sQuery status\n", MODULE_PREFIX);
            _cachedStatusRequestMs = millis();
        }
    }

    // Check for serial chars received from target
    if (_pTargetSerial)
    {
        String charsReceived;
        const int MAX_CHARS_TO_PROCESS = 100;
        // See if characters to be processed
        for (int rxCtr = 0; rxCtr < MAX_CHARS_TO_PROCESS; rxCtr++)
        {
            int rxCh = _pTargetSerial->read();
            if (rxCh < 0)
                break;

            // Add char to string
            charsReceived.concat((char)rxCh);

        }

        // Handle any string of chars received from target
        if (charsReceived.length() > 0)
        {
            // Debug
            // Log.trace("McIF RxChars %s\n", charsReceived.c_str());
        
            // Send to Pi
            if (_pCommandSerial)
                _pCommandSerial->sendTargetData("RxHost", 
                        (const uint8_t*)charsReceived.c_str(), charsReceived.length(), 0);

            // Send to Telnet
            if (_pTelnetServer)
                _pTelnetServer->sendChars(charsReceived.c_str(), charsReceived.length());
        }
    }

    // Handle demo button
    _debounceButton.service();

    // Handle demo state machine - for preloading files
    if (!_pCommandSerial)
        return;
    switch(_demoState)
    {
        case DEMO_STATE_PRELOAD:
        {
            // Check not busy
            if (_pCommandSerial->uploadInProgress())
                break;
            
            int numPreloadFiles = 0;
            RdJson::getType(numPreloadFiles, _demoPreloadFilesJson.c_str());
            if (numPreloadFiles < 0)
                numPreloadFiles = 0;
            if (numPreloadFiles > DEMO_PRELOAD_MAX_FILES)
                numPreloadFiles = DEMO_PRELOAD_MAX_FILES;

            // Log.notice("NO UPLOAD IN PROGRESS preloadIdx %d preloadNum %d\n", _demoPreloadFileIdx, numPreloadFiles);

            // Check done
            if (_demoPreloadFileIdx >= numPreloadFiles)
            {
                _demoState = DEMO_STATE_LOAD;
                break;
            }

            // Get file name
            String preloadIdxStr = "[" + String(_demoPreloadFileIdx++) + "]";
            String preloadName = RdJson::getString(preloadIdxStr.c_str(), 
                        "", _demoPreloadFilesJson.c_str());
            if (preloadName.length() == 0)
            {
                _demoState = DEMO_STATE_LOAD;
                break;
            }

            // Log.notice("Start FS upload preload file %s\n", preloadName.c_str());
            
            // Send
            _pCommandSerial->startUploadFromFileSystem("spiffs", "", preloadName, "");
            break;
        }
        case DEMO_STATE_LOAD:
        {
            // Check not busy
            if (_pCommandSerial->uploadInProgress())
                break;

            // Log.notice("Start FS upload demo file %s\n", _demoFileToRun.c_str());

            // Send and run
            _pCommandSerial->startUploadFromFileSystem("spiffs", "", _demoFileToRun, "ProgramAndReset");
            _demoState = DEMO_STATE_IDLE;
            break;
        }
    }
}

// Handle a frame received from the Pi
void MachineInterface::handleFrameRxFromPi(const uint8_t *frameBuffer, int frameLength)
{
    // Extract frame type
    if ((frameLength < 0) || (frameLength > MAX_COMMAND_LEN))
        return;

    // Buffer is null terminated in the HDLC receiver
    const char* pRxStr = (const char*)frameBuffer;

    // Log.trace("%s<- %s\n", MODULE_PREFIX, pRxStr);

    // Get command
    String cmdName = RdJson::getString("cmdName", "", pRxStr);
    if (cmdName.equalsIgnoreCase("getStatusResp"))
    {
        // Cache the status frame
        _cachedStatusJSON = pRxStr;
        // Log.trace("Received status update %s\n", MODULE_PREFIX, _cachedStatusJSON);
    }
    else if (cmdName.equalsIgnoreCase("keyCode"))
    {
        // Send key to target
        int asciiCode = RdJson::getLong("key", 0, pRxStr);
        if (_pTargetSerial && (asciiCode != 0))
            _pTargetSerial->write((char)asciiCode);
        Log.verbose("%ssent target char %x\n", MODULE_PREFIX, (char)asciiCode);
    }
    else if (cmdName.equalsIgnoreCase("apiReq"))
    {
        String requestStr = RdJson::getString("req", "", pRxStr);
        if (_pRestAPIEndpoints && requestStr.length() != 0)
        {
            String respStr;
            _pRestAPIEndpoints->handleApiRequest(requestStr.c_str(), respStr);
            if (_pCommandSerial)
            {
                _pCommandSerial->responseMessage(requestStr, respStr);
                // Log.trace("%sresponding to Pi %s\n", MODULE_PREFIX, respStr.c_str());
            }
        }
    }
    else if ((cmdName.equalsIgnoreCase("rdp")) || (cmdName.equalsIgnoreCase("zesarux")))
    {
        // Payload is after a string terminator
        int headerJsonEndPos = strlen(pRxStr);
        int payloadStartPos = 0;
        int payloadLen = 0;
        if (headerJsonEndPos+1 < frameLength)
        {
            payloadStartPos = headerJsonEndPos+1;
            payloadLen = frameLength - headerJsonEndPos - 1;
        }
        uint32_t dataLen = RdJson::getLong("dataLen", payloadLen, pRxStr);

        // TODO
        // String inStr = (const char*)(frameBuffer+payloadStartPos);
        // if (inStr.indexOf("validatorStatus") > 0)
        //     _rdpValStatCount++;
        String payloadStr = (((const char*)frameBuffer)+payloadStartPos);
        payloadStr.replace("\n", "\\n");
        if ((cmdName.equalsIgnoreCase("rdp")))
        {
            // Log.trace("%srdp <- %s payloadLen %d payload ¬¬%s¬¬\n", 
            //             MODULE_PREFIX, pRxStr, payloadLen,
            //             payloadStr.c_str());
            _miniHDLCForRDPTCP.sendFrame(frameBuffer+payloadStartPos, dataLen);
        }
        else
        {
            // Log.trace("%szesarux <- %s payloadLen %d payload ¬¬%s¬¬\n", 
            //             MODULE_PREFIX, pRxStr, payloadLen,
            //             payloadStr.c_str());
            _pZEsarUXTCPServer->sendChars(frameBuffer+payloadStartPos, dataLen);
        }
    }
    else if (cmdName.equalsIgnoreCase("log"))
    {
        // Extract msg
        String logMsg = RdJson::getString("msg", "", pRxStr);
        String msgSrc = RdJson::getString("src", "", pRxStr);
        String msgLev = RdJson::getString("lev", "", pRxStr);
        Log.trace("%s: %s: %s\n", msgLev.c_str(), msgSrc.c_str(), logMsg.c_str());
    }
    else if (cmdName.equalsIgnoreCase("mirrorScreen"))
    {
        // Log.trace("Mirror screen len %d\n", frameLength);
        _pWebServer->webSocketSend(frameBuffer, frameLength);
    }
    else if ((cmdName.endsWith("Resp")))
    {
        _cmdResponseNew = true;
        _cmdResponseBuf = pRxStr;
    }

}

const char *MachineInterface::getStatus()
{
    return _cachedStatusJSON.c_str();
}

void MachineInterface::handleDemoModeButtonPress(int buttonValue)
{
    // Check pressed (not released)
    if (!buttonValue)
        return;

    // Check demo busy
    if (_demoState != DEMO_STATE_IDLE)
        return;

    // Handle demo mode
    Log.notice("%sEntering demo mode\n", MODULE_PREFIX);

    // Select the next program to download
    if (!_pFileManager)
        return;

    // Get the demo mode control file contents
    String demoJson = _pFileManager->getFileContents("spiffs",
                        String("/") + demoModeControlFileName, demoModeControlFileMaxlen);
    if (demoJson.length() <= 0)
        return;
    Log.notice("Demo JSON %s\n", demoJson.c_str());

    // Get the number of entries in the demo table
    String filesList = RdJson::getString("demo", "[]", demoJson.c_str());
    int fileListLen = 0;
    RdJson::getType(fileListLen, filesList.c_str());
    _demoProgramIdx = _demoProgramIdx % fileListLen;

    // Get the current entry
    String fileInfo = RdJson::getString(("[" + String(_demoProgramIdx) + "]").c_str(),
                    "", filesList.c_str());
    String filenameToUpload = RdJson::getString("file", "", fileInfo.c_str());
    String machineToTarget = RdJson::getString("mc", "", fileInfo.c_str());
    String filesToPreload = RdJson::getString("preload", "[]", fileInfo.c_str());
    int numPreloadFiles = 0;
    RdJson::getType(numPreloadFiles, filesToPreload.c_str());
    if (numPreloadFiles < 0)
        numPreloadFiles = 0;
    if (numPreloadFiles > DEMO_PRELOAD_MAX_FILES)
        numPreloadFiles = DEMO_PRELOAD_MAX_FILES;

    // Switch machine
    if (machineToTarget.length() > 0)
    {
        String apiCmd = "targetcmd/setmachine=" + machineToTarget;
        String respStr;
        if (_pRestAPIEndpoints)
            _pRestAPIEndpoints->handleApiRequest(apiCmd.c_str(), respStr);
    }

    // Clear target buffer
    String apiCmd = "targetcmd/cleartarget";
    String respStr;
    if (_pRestAPIEndpoints)
        _pRestAPIEndpoints->handleApiRequest(apiCmd.c_str(), respStr);

    Log.notice("%sDemo mode machine %s, num preload %d, filename %s\n",
            MODULE_PREFIX, machineToTarget.c_str(), numPreloadFiles, filenameToUpload.c_str());

    // Setup state machine for preload and run
    _demoPreloadFilesJson = filesToPreload;
    _demoFileToRun = filenameToUpload;
    _demoState = DEMO_STATE_PRELOAD;
    _demoPreloadFileIdx = 0;

    // Bump
    _demoProgramIdx++;
}

void MachineInterface::hdlcRxFrameTCP(const uint8_t *framebuffer, int framelength)
{
    // Send it to Pi
    if (_pCommandSerial)
        _pCommandSerial->sendTargetData("rdp",
                framebuffer, framelength, _rdpCommandIndex++);

    // Log.trace("hdlcRxFrameTCP len%d\n", framelength);
}

void MachineInterface::hdlcTxCharTCP(uint8_t ch)
{
    if (_pTCPHDLCServer)
    {
        _pTCPHDLCServer->sendChars(&ch, 1);
    }
}

void MachineInterface::sendKeyToTarget(int keyCode)
{
    // Send key to target
    if (_pTargetSerial)
        _pTargetSerial->write(keyCode);
    // Log.verbose("%ssent target char %x\n", MODULE_PREFIX, (char)asciiCode);
}

bool MachineInterface::sendTargetCommand(const String& cmd, const String& reqStr, String& respStr, bool waitForResponse)
{
    // Send command
    _cmdResponseNew = false;
    _pCommandSerial->sendTargetCommand(cmd, reqStr);

    // Return if no response required
    if (!waitForResponse)
    {
        Utils::setJsonBoolResult(respStr, true);
        return true;
    }

    // Wait for a response (or time-out)
    uint32_t timeStarted = millis();
    while (!Utils::isTimeout(millis(), timeStarted, MAX_WAIT_FOR_CMD_RESPONSE_MS))
    {
        _pCommandSerial->service();
        if (_cmdResponseNew)
        {
            respStr = _cmdResponseBuf;
            return true;
        }
    }
    Utils::setJsonBoolResult(respStr, false);
    return false;
}
