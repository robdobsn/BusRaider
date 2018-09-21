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

  public:

    MachineInterface()
    {
        _cachedStatusJSON = "{}";
        _pSerial = NULL;
        _serialPortNum = -1;
        _baudRate = 115200;
    }

    void setup(ConfigBase& config)
    {
        // Get config
        ConfigBase csConfig(config.getString("machineIF", "").c_str());
        Log.trace("MachineInterface: config %s\n", csConfig.getConfigData());

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

    const char* getStatus()
    {
        return _cachedStatusJSON.c_str();
    }

    void service()
    {
        // Check for serial chars received
        if (_pSerial)
        {
            // See if characters to be processed
            for (int rxCtr = 0; rxCtr < 100; rxCtr++)
            {
                int rxCh = _pSerial->read();
                if (rxCh < 0)
                    break;

                // Handle char
                Serial.printf("%c", rxCh);
            }        
        }
    }
    
    void handleRxFrame(const uint8_t *framebuffer, int framelength)
    {
        // Extract frame type
        if ((framelength < 0) || (framelength > 1500))
            return;

        // Ensure string is terminated
        ((char*)framebuffer)[framelength] = 0;

        // Get command
        String cmdName = RdJson::getString("cmdName", "", (const char*)framebuffer);
        if (cmdName.equalsIgnoreCase("statusUpdate"))
        {
            // Store the status frame
            _cachedStatusJSON = (const char*)framebuffer;            
        }
    }
};
