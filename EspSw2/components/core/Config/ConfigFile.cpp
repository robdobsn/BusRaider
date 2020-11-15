/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ConfigFile
// Configuration persisted to a file
//
// Rob Dobson 2016-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Logger.h>
#include <ConfigFile.h>

static const char* MODULE_PREFIX = "ConfigFile";

// Clear
void ConfigFile::clear()
{
    // Delete the backing file
    _fileManager.deleteFile(_fileSystem, _filename);
    
    // Set the config str to empty
    ConfigBase::clear();

    // Clear the config string
    setConfigData("");
}

// Initialise
bool ConfigFile::setup()
{
    // Setup base class
    ConfigBase::setup();

    // Get config string
    String configData = _fileManager.getFileContents(_fileSystem, _filename, _configMaxDataLen);
    setConfigData(configData.c_str());
    LOG_D(MODULE_PREFIX, "Config %s read len(%d) %s", _filename.c_str(), configData.length(), configData.c_str());

    // Ok
    return true;
}

// Write configuration string
bool ConfigFile::writeConfig(const String& configJSONStr)
{
    // Set config data
    _setConfigData(configJSONStr.c_str());

    // Check the limits on config size
    if (_dataStrJSON.length() >= _configMaxDataLen)
    {
        String truncatedStr = _dataStrJSON.substring(0, _configMaxDataLen-1);
        // Write config file
        _fileManager.setFileContents(_fileSystem, _filename, truncatedStr);
    }
    else
    {
        // Write config file
        _fileManager.setFileContents(_fileSystem, _filename, _dataStrJSON);
    }

    LOG_D(MODULE_PREFIX, "Written config %s len %d%s",
                    _filename.c_str(), _dataStrJSON.length(),
                    (_dataStrJSON.length() >= _configMaxDataLen ? " TRUNCATED" : ""));

    // Ok
    return true;
}
