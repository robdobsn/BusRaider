// Machine Interface
// Rob Dobson 2018

#pragma once

class MachineInterface
{
  private:
    String _cachedStatusJSON;

    // Serial
    HardwareSerial *_pSerial;

    // Serial port details
    int _serialPortNum;
    int _baudRate;

    // Web server
    WebServer *_pWebServer;

    // Web socket for communication of keystrokes
    AsyncWebSocket *_pWebSocket;

  public:
    MachineInterface()
    {
        _cachedStatusJSON = "{}";
        _pSerial = NULL;
        _serialPortNum = -1;
        _baudRate = 115200;
        _pWebServer = NULL;
        _pWebSocket = NULL;
    }

    void setup(ConfigBase &config, WebServer *pWebServer)
    {
        // Get config
        ConfigBase csConfig(config.getString("machineIF", "").c_str());
        Log.trace("MachineInterface: config %s\n", csConfig.getConfigData());

        // Save webserver
        _pWebServer = pWebServer;

        // Add web socket handler
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

        //             if (_pWebServer->_pServer) {
        //   _pWebServer->_pServer->on("/testws", HTTP_GET, [](AsyncWebServerRequest *request) {
        //       const char* resp = "<head><script>function test() {"
        //            "var exampleSocket = new WebSocket(\"ws://192.168.86.44/ws\", "
        //            "\"protocolOne\"); exampleSocket.onopen = function () {"
        //          "exampleSocket.send(\"Here's some text that the server is urgently awaiting!\"); }; }"
        //           "</script></head><body onload=\"test()\">Hello</body>";
        //       request->send(200, "text/html", resp);
        //   }); }

        // Get serial port
        _serialPortNum = csConfig.getLong("portNum", -1);
        _baudRate = csConfig.getLong("baudRate", 115200);

        // Debug
        Log.trace("MachineInterface: portNum %d, baudRate %d\n", _serialPortNum, _baudRate);

        // Setup port
        if (_serialPortNum == -1)
            return;

        // Setup serial port
        _pSerial = new HardwareSerial(_serialPortNum);
        if (_pSerial)
        {
            if (_serialPortNum == 1)
            {
                _pSerial->begin(_baudRate, SERIAL_8N1, 16, 17, false);
                Log.trace("MachineInterface: portNum %d, baudRate %d, rxPin, txPin\n",
                          _serialPortNum, _baudRate, 16, 17);
            }
            else if (_serialPortNum == 2)
            {
                _pSerial->begin(_baudRate, SERIAL_8N1, 26, 25, false);
                Log.trace("MachineInterface: portNum %d, baudRate %d, rxPin, txPin\n",
                          _serialPortNum, _baudRate, 26, 25);
            }
            else
            {
                _pSerial->begin(_baudRate);
                Log.trace("MachineInterface: portNum %d, baudRate %d, rxPin, txPin\n",
                          _serialPortNum, _baudRate, 3, 1);
            }
        }
    }

    const char *getStatus()
    {
        return _cachedStatusJSON.c_str();
    }

    void service()
    {
        // Check for serial chars received
        if (_pSerial)
        {
            String charsReceived;
            const int MAX_CHARS_TO_PROCESS = 100;
            // See if characters to be processed
            for (int rxCtr = 0; rxCtr < MAX_CHARS_TO_PROCESS; rxCtr++)
            {
                int rxCh = _pSerial->read();
                if (rxCh < 0)
                    break;

                // Add char to string
                charsReceived += (char)rxCh;

                // Debug
                Serial.printf("%c", rxCh);
            }

            // Send any string of chars received to web socket
            if (charsReceived.length() > 0)
            {
                String keyJSON = "{\"dataType\":\"key\",\"val\":\"" +
                                 Utils::escapeJSON(charsReceived) + "\"}";
                if (_pWebSocket)
                {
                    _pWebSocket->printfAll("%s", keyJSON.c_str());
                }
            }
        }
    }

    void handleRxFrame(const uint8_t *framebuffer, int framelength)
    {
        // Extract frame type
        if ((framelength < 0) || (framelength > 1500))
            return;

        // Ensure string is terminated
        ((char *)framebuffer)[framelength] = 0;

        // Get command
        String cmdName = RdJson::getString("cmdName", "", (const char *)framebuffer);
        if (cmdName.equalsIgnoreCase("statusUpdate"))
        {
            // Store the status frame
            _cachedStatusJSON = (const char *)framebuffer;
        }
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
            if (!_pSerial)
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
                    _pSerial->write((char)ascKey);
                    Log.trace("McIF to host char %x\n", (char)ascKey);
                }
            }
        }
    }
};
