// Config EEPROM Storage
// Rob Dobson 2017

#pragma once

#include "ConfigBase.h"
#include <EEPROM.h>

class ConfigEEPROM : public ConfigBase
{
private:
    // Base location to store dataStr in EEPROM
    int _eepromBaseLocation;

    // Max length of config data
    int _configMaxDataLen;

    // Write state
    bool _isWriting;
    int _curWriteOffset;
    int _numBytesToWrite;
    String _stringToWrite;

    // Dirty flag - when set indicates a write is needed
    bool _dirtyFlag;

public:
    ConfigEEPROM(int configBase, int configMaxLen)
    {
        _eepromBaseLocation = configBase;
        _configMaxDataLen = configMaxLen;
        _isWriting = false;
        _curWriteOffset = 0;
        _numBytesToWrite = 0;
        _stringToWrite = "";
        _dirtyFlag = false;
    }

    ~ConfigEEPROM()
    {
    }

    bool isDirty()
    {
        return _dirtyFlag;
    }

    void setDirty()
    {
        _dirtyFlag = true;
        Log.notice("setDirty");
    }

    // Clear
    void clear()
    {

        // Write empty convig
        write("");

        // Set the config str to empty
        ConfigBase::clear();

        // Clear the config string
        setConfigData("");
    }    

    // Read a configuration string from EEPROM
    String read()
    {
        // Check EEPROM has been initialised - if not just start with a null string
        if (EEPROM.read(_eepromBaseLocation) == 0xff)
        {
            Log.notice("ConfigEEPROM::read uninitialised");
            return "";
        }

        // Find out how long the string is - don't allow > _configMaxDataLen
        int dataStrLen = _configMaxDataLen - 1;
        for (int chIdx = 0; chIdx < _configMaxDataLen; chIdx++)
        {
            int ch = EEPROM.read(_eepromBaseLocation + chIdx);
            if (ch == 0)
            {
                dataStrLen = chIdx;
                break;
            }
        }

        // Get string from EEPROM
        String dataStr;
        dataStr.reserve(dataStrLen);

        // Fill string from EEPROM location
        for (int chIdx = 0; chIdx < dataStrLen; chIdx++)
        {
            char ch = EEPROM.read(_eepromBaseLocation + chIdx);
            dataStr.concat(ch);
        }
        Log.notice("ConfigEEPROM::read %d chars", dataStrLen);
        _dirtyFlag = false;
        return dataStr;
    }


    // Write configuration string to EEPROM
    bool write(const char* pDataStr)
    {
        Log.notice("ConfigEEPROM:write writing %d bytes", strlen(pDataStr));

        // Copy data to write (in case it changes in the background)
        _stringToWrite = pDataStr;
        _curWriteOffset = 0;
        _dirtyFlag = false;

        // Get length of string
        int dataStrLen = _stringToWrite.length();
        if (dataStrLen >= _configMaxDataLen)
            dataStrLen = _configMaxDataLen - 1;

        // Record details to write
        _numBytesToWrite = dataStrLen;
        _isWriting = true;
        return true;
    }

    // Service the writing process
    void service()
    {
        // Only if writing
        if (!_isWriting)
            return;
        // Write next char
        if (_curWriteOffset < _numBytesToWrite)
        {
            EEPROM.write(_eepromBaseLocation + _curWriteOffset, _stringToWrite.charAt(_curWriteOffset));
            // Log.trace("Writing %c", _stringToWrite.charAt(_curWriteOffset));
        }
        else
        {
            EEPROM.write(_eepromBaseLocation + _curWriteOffset, 0);
            // Log.trace("Writing 0 terminator");
        }
        // Bump
        _curWriteOffset += 1;
        if (_curWriteOffset > _numBytesToWrite)
        {
            // Clean up
            _isWriting = false;
            _stringToWrite = "";
            Log.notice("ConfigEEPROM: service write complete");
        }
    }

    // Initialise
    bool setup()
    {
        // Setup base class
        ConfigBase::setup();

        // Read config
        String configData = read();

        // Set config string
        setConfigData(configData.c_str());
        Log.trace("ConfigEEPROM: read len: %d\n", configData.length());
        Log.trace("ConfigEEPROM: read: %s\n", configData.c_str());

        // Ok
        return true;
    }

    // Write configuration string
    bool writeConfig()
    {
        const char *pConfigData = getConfigData();
        String truncatedStr = "";

        // Get length of string
        int dataStrLen = 0;
        if (pConfigData != NULL)
        {
            dataStrLen = strlen(pConfigData);
        }
        if (dataStrLen >= _configMaxDataLen)
        {
            dataStrLen = _configMaxDataLen - 1;
            truncatedStr = _dataStrJSON.substring(0, dataStrLen);
            pConfigData = truncatedStr.c_str();
        }
        Log.trace("ConfigEEPROM: Writing len: %d\n", dataStrLen);

        // Set config string
        write(pConfigData);

        // Ok
        return true;
    }

};
