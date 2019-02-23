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

MachineInterface::MachineInterface()
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
    _pRemoteDebugServer = NULL;
    _pRestAPIEndpoints = NULL;
    _demoState = DEMO_STATE_IDLE;
    _demoPreloadFileIdx = 0;
    _demoProgramIdx = 0;
    _rdpCommandIndex = 0;
}

// Setup
void MachineInterface::setup(ConfigBase &config, WebServer *pWebServer, CommandSerial* pCommandSerial,
            AsyncTelnetServer* pTelnetServer, RemoteDebugProtocolServer* pRemoteDebugProtocolServer, 
            RestAPIEndpoints* pRestAPIEndpoints, FileManager* pFileManager)
{
    // Get config
    ConfigBase csConfig(config.getString("machineIF", "").c_str());
    Log.trace("%sconfig %s\n", MODULE_PREFIX, csConfig.getConfigCStrPtr());

    // Save webserver and command serial
    _pWebServer = pWebServer;
    _pCommandSerial = pCommandSerial;
    _pTelnetServer = pTelnetServer;
    _pRemoteDebugServer = pRemoteDebugProtocolServer;
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

    // Set the remote data callback
    if (_pRemoteDebugServer)
    {
        _pRemoteDebugServer->onData([this](void* cbArg, const char* pData, int numChars) 
        {
            (void)cbArg;

            // Ensure terminated
            if ((numChars < 0) || (numChars > MAX_COMMAND_LEN))
                return;

            // Ensure string is terminated
            char* pStr = new char[numChars+1];
            memcpy(pStr, pData, numChars);
            pStr[numChars] = 0;
            String str = pStr;
            // Clean up
            delete [] pStr;

            // Send target command to Pi
            if (_pCommandSerial)
                _pCommandSerial->sendTargetData("rdp", (const uint8_t*)str.c_str(), 
                        str.length(), _rdpCommandIndex++);

        }, this);
    }

    // Set callback on frame received from Pi
    if (_pCommandSerial)
    {
        _pCommandSerial->setCallbackOnRxFrame([this](const uint8_t *framebuffer, int framelength) {
            handleFrameRxFromPi(framebuffer, framelength);
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
            _pCommandSerial->startUploadFromFileSystem("SPIFFS", "", preloadName, "");
            break;
        }
        case DEMO_STATE_LOAD:
        {
            // Check not busy
            if (_pCommandSerial->uploadInProgress())
                break;

            // Log.notice("WOULD UPLOAD %s\n", _demoFileToRun.c_str());

            // Send and run
            _pCommandSerial->startUploadFromFileSystem("SPIFFS", "", _demoFileToRun, "ProgramAndReset");
            _demoState = DEMO_STATE_IDLE;
            break;
        }
    }
}

// Handle a frame received from the Pi
void MachineInterface::handleFrameRxFromPi(const uint8_t *framebuffer, int framelength)
{
    // Extract frame type
    if ((framelength < 0) || (framelength > MAX_COMMAND_LEN))
        return;

    // Ensure string is terminated
    char* pStr = new char[framelength+1];
    memcpy(pStr, framebuffer, framelength);
    pStr[framelength] = 0;
    String str = pStr;
    // Tidy up
    delete [] pStr;

    Log.trace("%sreceivedFromPi %s\n", MODULE_PREFIX, str.c_str());

    // Get command
    String cmdName = RdJson::getString("cmdName", "", str.c_str());
    if (cmdName.equalsIgnoreCase("statusUpdate"))
    {
        // Cache the status frame
        _cachedStatusJSON = str;
    }
    else if (cmdName.equalsIgnoreCase("keyCode"))
    {
        // Send key to target
        int asciiCode = RdJson::getLong("key", 0, str.c_str());
        if (_pTargetSerial && (asciiCode != 0))
            _pTargetSerial->write((char)asciiCode);
        Log.trace("%ssent target char %x\n", MODULE_PREFIX, (char)asciiCode);
    }
    else if (cmdName.equalsIgnoreCase("apiReq"))
    {
        String requestStr = RdJson::getString("req", "", str.c_str());
        if (_pRestAPIEndpoints && requestStr.length() != 0)
        {
            String respStr;
            _pRestAPIEndpoints->handleApiRequest(requestStr.c_str(), respStr);
            if (_pCommandSerial)
                _pCommandSerial->responseMessage(respStr);
        }
    }
    else if (cmdName.equalsIgnoreCase("rdp"))
    {
        Log.verbose("%srdp response message %s\n", MODULE_PREFIX, str.c_str());
        String contentStr = RdJson::getString("content", "", str.c_str());
        if (_pRemoteDebugServer)
            _pRemoteDebugServer->sendChars(contentStr.c_str(), contentStr.length());
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
    String demoJson = _pFileManager->getFileContents("SPIFFS",
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

    // {
    //     String filesJson;
    //     if (_pFileManager->getFilesJSON("SPIFFS", "/", filesJson))
    //     {
    //         // Get the index of file to upload
    //         String filesList = RdJson::getString("files", "[]", filesJson.c_str());
    //         int fileListLen = 0;
    //         RdJson::getType(fileListLen, filesList.c_str());
    //         _demoProgramIdx = _demoProgramIdx % fileListLen;

    //         // Get filename
    //         String fileinfoToUpload = RdJson::getString(("[" + String(_demoProgramIdx) + "]").c_str(),
    //                         "", filesList.c_str());
    //         String filenameToUpload = RdJson::getString("name", "", fileinfoToUpload.c_str());
    //         // Handle upload
    //         Log.notice("MachineInterface: demo %d %s\n", _demoProgramIdx,
    //                         filenameToUpload.c_str());
    //         if (filenameToUpload.length() > 0)
    //         {
    //             String apiCmd = "runfileontarget/SPIFFS/" + filenameToUpload;
    //             String respStr;
    //             if (_pRestAPIEndpoints)
    //                 _pRestAPIEndpoints->handleApiRequest(apiCmd.c_str(), respStr);
    //         }
    //         _demoProgramIdx++;
    //     }
    // }


}
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

#ifdef USE_WEBSOCKET_TERMINAL
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

