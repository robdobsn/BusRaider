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
#ifdef USE_WEBSOCKET_TERMINAL
    _pWebSocket = NULL;
#endif
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

    // TODO
    // _rdpValStatCount = 0;
    // _rdpLastOutMs = 0;
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
            Log.trace("%sportNum %d, baudRate %d, rxPin, txPin\n",
                        MODULE_PREFIX, _targetSerialPortNum, _targetSerialBaudRate, 16, 17);
        }
        else if (_targetSerialPortNum == 2)
        {
            _pTargetSerial->begin(_targetSerialBaudRate, SERIAL_8N1, 26, 25, false);
            Log.trace("%sportNum %d, baudRate %d, rxPin, txPin\n",
                        MODULE_PREFIX, _targetSerialPortNum, _targetSerialBaudRate, 26, 25);
        }
        else
        {
            _pTargetSerial->begin(_targetSerialBaudRate);
            Log.trace("%sportNum %d, baudRate %d, rxPin, txPin\n",
                        MODULE_PREFIX, _targetSerialPortNum, _targetSerialBaudRate, 3, 1);
        }
        _pTargetSerial->setRxBufferSize(4096);
    }

    // Get demo button
    String demoPinStr = csConfig.getString("demoPin", "");
    int demoPin = -1;
    if (demoPinStr.length() != 0)
        demoPin = ConfigPinMap::getPinFromName(demoPinStr.c_str());
    if (demoPin != -1)
        _debounceButton.setup(demoPin, 0,
                    std::bind(&MachineInterface::handleDemoModeButtonPress, this, std::placeholders::_1));

    // Add web socket handler
#ifdef USE_WEBSOCKET_TERMINAL
    if (_pWebServer)
    {
        String wsPath = csConfig.getString("wsPath", "");
        if (wsPath.length() > 0)
        {
            // Get pointer to AsyncWebServer
            AsyncWebServer *pAsyncServer = _pWebServer->_pServer;
            if (pAsyncServer)
            {
                // Delete any existing
                // if (_pWebSocket)
                // {
                //     pAsyncServer->removeHandler(_pWebSocket);
                //     _pWebSocket->closeAll();
                //     delete _pWebSocket;
                // }
                // _pWebSocket = NULL;

                // Add
                _pWebSocket = new AsyncWebSocket(wsPath);
                _pWebSocket->onEvent(std::bind(&MachineInterface::wsEventHandler, this,
                                                std::placeholders::_1, std::placeholders::_2,
                                                std::placeholders::_3, std::placeholders::_4,
                                                std::placeholders::_5, std::placeholders::_6));
                pAsyncServer->addHandler(_pWebSocket);
            }
        }
    }
#endif        
}

void MachineInterface::service()
{
    // TODO
    // if (Utils::isTimeout(millis(), _rdpLastOutMs, 2000))
    // {
    //     Log.trace("_rdpValStatCount %d\n", _rdpValStatCount);
    //     _rdpLastOutMs = millis();
    // }

    // Check cached status is ok
    if (_cachedStatusJSON.length() <= 2)
    {
        if ((_cachedStatusRequestMs == 0) || (Utils::isTimeout(millis(), _cachedStatusRequestMs, TIME_BETWEEN_STATUS_REQS_MS)))
        {
            // Request status
            if (_pCommandSerial)
                _pCommandSerial->sendTargetCommand("getStatus");
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
            Log.verbose("McIF RxChars %s\n", charsReceived.c_str());
        
            // Send to Pi
            if (_pCommandSerial)
                _pCommandSerial->sendTargetData("RxHost", 
                        (const uint8_t*)charsReceived.c_str(), charsReceived.length(), 0);

            // Send to Telnet
            if (_pTelnetServer)
                _pTelnetServer->sendChars(charsReceived.c_str(), charsReceived.length());

#ifdef USE_WEBSOCKET_TERMINAL
            // Send to websocket
            if (_pWebSocket)
            {
                String keyJSON = "{\"dataType\":\"key\",\"val\":\"" +
                                Utils::escapeJSON(charsReceived) + "\"}";
                _pWebSocket->printfAll("%s", keyJSON.c_str());
            }
#endif
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

            // Log.notice("WOULD UPLOAD %s\n", preloadName.c_str());
            
            // Send
            _pCommandSerial->startUploadFromFileSystem("spiffs", "", preloadName, "");
            break;
        }
        case DEMO_STATE_LOAD:
        {
            // Check not busy
            if (_pCommandSerial->uploadInProgress())
                break;

            // Log.notice("WOULD UPLOAD %s\n", _demoFileToRun.c_str());

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
            Log.trace("%szesarux <- %s payloadLen %d payload ¬¬%s¬¬\n", 
                        MODULE_PREFIX, pRxStr, payloadLen,
                        payloadStr.c_str());
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

#ifdef USE_WEBSOCKET_TERMINAL
// int convertEscKey(String& keyStr)
// {
//     // Handle escape keys
//     int escPos = 0;
//     String escStr;
//     escStr.reserve(5);
//     for (int i = 0; i < keyStr.length(); i++)
//     {
//         int ch = keyStr[i];
//         if (ch == '\\')
//         {
//             // check for simple escapes
//             if (escPos != 1)
//             {
//                 Log.trace("ESC char %x\n", (char)'\\');
//                 return '\\';
//             }
//             // Now in an escape sequence
//                 Log.trace("In esc %d\n", escPos);
//             escPos = 1;
//             continue;
//         }
//         else if ((escPos == 1) && ((ch == 'U' || ch == 'u')) && keyStr.length() > 2)
//         {
//             // Escape is backslash U encoding
//             escPos = 1;
//                 Log.trace("In esc u %d\n", escPos);
//             continue;
//         }
//         else if (escPos == 1)
//         {
//             // Escape is standard encoding
//             switch (ch)
//             {
//                 case 'r':
//                     return 0x0d;
//                 case 'n':
//                     return 0x0a;
//                 case '0':
//                     return 0;
//                 case 't':
//                     return 0x09;
//                 case 'b':
//                     return 0x08;
//             }
//             return -1;
//         }
//         else if (((escPos > 0) && (escPos < 5)) && isxdigit(ch))
//         {
//             if (escPos == 1)
//                 escStr = "";
//             escStr += (char)ch;
//             escPos++;
//                 Log.trace("In esc %d %s %c\n", escPos, escStr, ch);
//             if (escPos == 5)
//             {
//                 int newCh = strtol(escStr.c_str(), NULL, 16);
//                 Log.trace("ESC U esc char %x\n", (char)newCh);
//                 return newCh;
//             }
//         }
//         else if (escPos != 0)
//         {
//             // Something went wrong
//                 Log.trace("Something wrong esc %d ch %d\n", escPos, ch);
//             return -1;
//         }
//         else
//         {
//                 Log.trace("ESC std char %x\n", (char)ch);
//                 return ch;
//         }
//     }
//     return -1;
// }


void wsEventHandler(AsyncWebSocket *server,
                    AsyncWebSocketClient *client,
                    AwsEventType type,
                    void *arg, uint8_t *data,
                    size_t len)
{
    // Handle event types
    if (type == WS_EVT_CONNECT)
    {
        //client connected
        Log.trace("McIF ws connect %s id %d\n", server->url(), client->id());
    }
    else if (type == WS_EVT_DISCONNECT)
    {
        // Client disconnected
        Log.trace("McIF ws disconnect\n");
    }
    else if (type == WS_EVT_ERROR)
    {
        // Error was received from the other end
        Log.trace("McIF ws error rx %d\n", *((uint16_t *)arg));
        // Log.trace("McIF ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t *)arg), (char *)data);
    }
    else if (type == WS_EVT_PONG)
    {
        Log.trace("McIF pong rx\n");
        // Pong message was received (in response to a ping request maybe)
        // Log.trace("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len) ? (char *)data : "");
    }
    else if (type == WS_EVT_DATA)
    {
        // Check serial to host is open
        if (!_pTargetSerial)
            return;

        // Extract the data
        static const int MAX_WS_RX_LEN = 200;
        char rxData[MAX_WS_RX_LEN + 1];
        int maxLen = len < MAX_WS_RX_LEN ? len : MAX_WS_RX_LEN;
        strncpy(rxData, (const char *)data, maxLen);
        rxData[maxLen] = 0;

        // Debug
        Log.trace("WebSocket received len %d = %s\n", len, rxData);

        // Check type of data
        String dataType = RdJson::getString("dataType", "", rxData);
        if (dataType.equalsIgnoreCase("key"))
        {
            // Get key
            String keyVal = RdJson::getString("val", "", rxData);
            Log.trace("WebSocket data is key %s\n", keyVal.c_str());

            // Check for modifier keys
            int ascKey = -1;
            // bool isShift = false;
            bool isCtrl = false;
            // bool isAlt = false;
            // if (keyVal.startsWith("SHIFT-"))
            // {
            //     isShift = true;
            //     keyVal = keyVal.substring(6);
            // }
            if (keyVal.startsWith("CTRL-"))
            {
                isCtrl = true;
                keyVal = keyVal.substring(5);
            }
            // if (keyVal.startsWith("ALT-"))
            // {
            //     isAlt = true;
            //     keyVal = keyVal.substring(4);
            // }

            // Check special keys
            if (keyVal.equalsIgnoreCase("enter"))
                ascKey = 0x0d;
            else if (keyVal.equalsIgnoreCase("tab"))
                ascKey = 0x09;
            else if (keyVal.equalsIgnoreCase("spacebar"))
                ascKey = ' ';
            else if (keyVal.equalsIgnoreCase("backspace"))
                ascKey = 0x08;
            else if (keyVal.equalsIgnoreCase("clear"))
                ascKey = 0x0c;
            else if (keyVal.equalsIgnoreCase("doublequotes"))
                ascKey = '"';
            else if (keyVal.equalsIgnoreCase("singlequotes"))
                ascKey = '\'';
            else if (keyVal.length() == 1)
                ascKey = keyVal[0];

            // Handle CTRL
            if (isCtrl)
            {
                ascKey = ascKey & 0x1f;
            }

            // Handle new code
            if (ascKey == -1)
            {
                Log.trace("McIF Ignoring key %s\n", keyVal.c_str());
            }
            else
            {
                // Send key to host
                _pTargetSerial->write((char)ascKey);
                Log.trace("McIF to host char %x\n", (char)ascKey);
            }
        }
    }
}
#endif

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
