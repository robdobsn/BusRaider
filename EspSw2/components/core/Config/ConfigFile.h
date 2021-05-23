/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ConfigFile
// Configuration persisted to a file
//
// Rob Dobson 2016-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ConfigBase.h>
#include <FileManager.h>

class ConfigFile : public ConfigBase
{
private:

    // File manager
    FileManager& _fileManager;

    // Config filesystem
    String _fileSystem;

    // Config filename
    String _filename;

public:
    ConfigFile(FileManager& fileManager, const char *fileSystem, const char* filename, int configMaxlen) :
            _fileManager(fileManager)
    {
        _fileSystem = fileSystem;
        _filename = filename;
        _configMaxDataLen = configMaxlen;
    }

    virtual ~ConfigFile()
    {
    }

    // Get max length
    virtual int getMaxLen() const override final;

    // Clear
    virtual void clear() override final;

    // Initialise
    virtual bool setup() override final;

    // Write configuration string
    virtual bool writeConfig(const String& configJSONStr) override final;
};
