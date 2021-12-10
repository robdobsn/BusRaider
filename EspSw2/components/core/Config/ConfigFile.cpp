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

//#define DEBUG_CONFIG_FILE

ConfigFile::ConfigFile(FileManager& fileManager, const char *fileSystem, const char* filename, int configMaxlen) :
        _fileManager(fileManager)
{
    _fileSystem = fileSystem;
    _filename = filename;
    _configMaxDataLen = configMaxlen;
}

~ConfigFile::ConfigFile()
{
}

// Clear
void ConfigFile::clear()
{
    // Delete the backing file
    _fileManager.deleteFile(_fileSystem, _filename);
    
    // Set the config str to empty
    ConfigBase::clear();

    // Clear the config string
    _setConfigData("");
}

// Initialise
bool ConfigFile::setup()
{
    // Setup base class
    ConfigBase::setup();

    // Get config string
    char* pConfigData = _fileManager.getFileContents(_fileSystem, _filename, _configMaxDataLen);
    if (pConfigData)
    {
        _setConfigData(pConfigData);
        free(pConfigData);
#ifdef DEBUG_CONFIG_FILE
        LOG_I(MODULE_PREFIX, "Config %s read len(%d) %s", _filename.c_str(), configData.length(), configData.c_str());
#endif
    }
    else
    {
        _setConfigData("");
    }

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

#ifdef DEBUG_CONFIG_FILE
    LOG_I(MODULE_PREFIX, "Written config %s len %d%s",
                    _filename.c_str(), _dataStrJSON.length(),
                    (_dataStrJSON.length() >= _configMaxDataLen ? " TRUNCATED" : ""));
#endif

    // Ok
    return true;
}
