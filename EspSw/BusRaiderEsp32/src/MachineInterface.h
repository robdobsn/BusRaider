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

#pragma once

#include "WebServer.h"
#include "CommandSerial.h"
#include "AsyncTelnetServer.h"
#include "RestAPIEndpoints.h"

// #define USE_WEBSOCKET_TERMINAL 1

class MachineInterface
{
private:
    // Cache for machine status so that it can be quickly returned
    // when needed
    String _cachedStatusJSON;

    // Serial
    HardwareSerial* _pTargetSerial;

    // Serial port details
    int _targetSerialPortNum;
    int _targetSerialBaudRate;

    // Web server ptr (copy)
    WebServer* _pWebServer;

    // Command serial ptr (copy)
    CommandSerial* _pCommandSerial;

    // Web socket for communication of keystrokes
#ifdef USE_WEBSOCKET_TERMINAL
    AsyncWebSocket* _pWebSocket;
#endif

    // Telnet server
    AsyncTelnetServer* _pTelnetServer;

    // REST API
    RestAPIEndpoints* _pRestAPIEndpoints;

    // File manager
    FileManager* _pFileManager;

    // Demo handling
    int _demoPin;
    uint32_t _demoPinDebounceTime;
    int _demoPinDebounceVal;
    static const int DEMO_PIN_DEBOUNCE_MS = 50;
    int _demoProgramIdx;

public:
    MachineInterface()
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
        _pRestAPIEndpoints = NULL;
        _demoPin = -1;
        _demoPinDebounceTime = 0;
        _demoPinDebounceVal = 0;
        _demoProgramIdx = 0;
    }

    // Setup
    void setup(ConfigBase &config, WebServer *pWebServer, CommandSerial* pCommandSerial,
                AsyncTelnetServer* pTelnetServer, RestAPIEndpoints* pRestAPIEndpoints,
                FileManager* pFileManager)
    {
        // Get config
        ConfigBase csConfig(config.getString("machineIF", "").c_str());
        Log.trace("MachineInterface: config %s\n", csConfig.getConfigData());

        // Save webserver and command serial
        _pWebServer = pWebServer;
        _pCommandSerial = pCommandSerial;
        _pTelnetServer = pTelnetServer;
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
        Log.trace("MachineInterface: portNum %d, baudRate %d\n", _targetSerialPortNum, _targetSerialBaudRate);

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
                Log.trace("MachineInterface: portNum %d, baudRate %d, rxPin, txPin\n",
                          _targetSerialPortNum, _targetSerialBaudRate, 16, 17);
            }
            else if (_targetSerialPortNum == 2)
            {
                _pTargetSerial->begin(_targetSerialBaudRate, SERIAL_8N1, 26, 25, false);
                Log.trace("MachineInterface: portNum %d, baudRate %d, rxPin, txPin\n",
                          _targetSerialPortNum, _targetSerialBaudRate, 26, 25);
            }
            else
            {
                _pTargetSerial->begin(_targetSerialBaudRate);
                Log.trace("MachineInterface: portNum %d, baudRate %d, rxPin, txPin\n",
                          _targetSerialPortNum, _targetSerialBaudRate, 3, 1);
            }
        }

        // Get demo button
        String demoPinStr = csConfig.getString("demoPin", "");
        _demoPin = -1;
        _demoPinDebounceTime = millis();
        _demoPinDebounceVal = 0;
        if (demoPinStr.length() != 0)
            _demoPin = ConfigPinMap::getPinFromName(demoPinStr.c_str());
        if (_demoPin != -1)
        {
            pinMode(_demoPin, INPUT);
            _demoPinDebounceVal = digitalRead(_demoPin);
        }

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

    void service()
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
        if (_demoPin != -1)
        {
            // Check for change of value
            if (digitalRead(_demoPin) != _demoPinDebounceVal)
            {
                if (Utils::isTimeout(millis(), _demoPinDebounceTime, DEMO_PIN_DEBOUNCE_MS))
                {
                    if (_demoPinDebounceVal != 0)
                    {
                        // Handle pin press
                        Log.notice("MachineInterface: demo pin pressed\n");

                        // Select the next program to download
                        if (_pFileManager)
                        {
                            String filesJson;
                            if (_pFileManager->getFilesJSON("SPIFFS", "/", filesJson))
                            {
                                // Get the index of file to upload
                                String filesList = RdJson::getString("files", "[]", filesJson.c_str());
                                int fileListLen = 0;
                                RdJson::getType(fileListLen, filesList.c_str());
                                _demoProgramIdx = _demoProgramIdx % fileListLen;

                                // Get filename
                                String fileinfoToUpload = RdJson::getString(("[" + String(_demoProgramIdx) + "]").c_str(),
                                                "", filesList.c_str());
                                String filenameToUpload = RdJson::getString("name", "", fileinfoToUpload.c_str());
                                // Handle upload
                                Log.notice("MachineInterface: demo %d %s\n", _demoProgramIdx,
                                                filenameToUpload.c_str());
                                if (filenameToUpload.length() > 0)
                                {
                                    String apiCmd = "runfileontarget/SPIFFS/" + filenameToUpload;
                                    String respStr;
                                    if (_pRestAPIEndpoints)
                                        _pRestAPIEndpoints->handleApiRequest(apiCmd.c_str(), respStr);
                                }
                                _demoProgramIdx++;
                            }
                        }
                    }
                }
            }
            else
            {
                _demoPinDebounceTime = millis();
            }
            
        }
    }

    // Handle a frame received from the Pi
    void handleFrameRxFromPi(const uint8_t *framebuffer, int framelength)
    {
        // Extract frame type
        if ((framelength < 0) || (framelength > 1500))
            return;

        // Ensure string is terminated
        char* pStr = new char[framelength+1];
        memcpy(pStr, framebuffer, framelength);
        pStr[framelength] = 0;

        // Get command
        String cmdName = RdJson::getString("cmdName", "", pStr);
        if (cmdName.equalsIgnoreCase("statusUpdate"))
        {
            // Cache the status frame
            _cachedStatusJSON = (const char *)pStr;
        }
        else if (cmdName.equalsIgnoreCase("keyCode"))
        {
            // Send key to target
            int asciiCode = RdJson::getLong("key", 0, pStr);
            if (_pTargetSerial && (asciiCode != 0))
                _pTargetSerial->write((char)asciiCode);
            Log.trace("McIF sent target char %x\n", (char)asciiCode);
        }
        else if (cmdName.equalsIgnoreCase("apiReq"))
        {
            String requestStr = RdJson::getString("req", "", pStr);
            if (_pRestAPIEndpoints && requestStr.length() != 0)
            {
                String respStr;
                _pRestAPIEndpoints->handleApiRequest(requestStr.c_str(), respStr);
                if (_pCommandSerial)
                    _pCommandSerial->responseMessage(respStr);
            }
        }

        // Tidy up
        delete [] pStr;
    }

    const char *getStatus()
    {
        return _cachedStatusJSON.c_str();
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
};
