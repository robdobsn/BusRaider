/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FileManager 
// Handles SPIFFS/LittleFS and SD card file access
//
// Rob Dobson 2018-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ConfigBase.h>
#include <SysModBase.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <Utils.h>
#include "FileStreamBlock.h"
#include <ProtocolExchange.h>

class RestAPIEndpointManager;
class APISourceInfo;

class FileManager : public SysModBase
{
public:
    FileManager(const char* pModuleName, ConfigBase& defaultConfig, ConfigBase* pGlobalConfig, ConfigBase* pMutableConfig);

    // // Upload file to file system
    // virtual bool fileStreamDataBlock(FileStreamBlock& fileStreamBlock) override final;

    // Set protocol exchange
    void setProtocolExchange(ProtocolExchange& protocolExchange)
    {
        _pProtocolExchange = &protocolExchange;
    }

protected:
    // Setup
    virtual void setup() override final;

    // Service - called frequently
    virtual void service() override final;

    // Add endpoints
    virtual void addRestAPIEndpoints(RestAPIEndpointManager& endpointManager) override final;

    // Get debug string
    virtual String getDebugJSON() override final;

private:

    // Protocol exchange
    ProtocolExchange* _pProtocolExchange;

    // Helpers
    void applySetup();

    // Format file system
    void apiReformatFS(const String &reqStr, String& respStr, const APISourceInfo& sourceInfo);

    // List files on a file system
    // In the reqStr the first part of the path is the file system name (e.g. sd or local, can be blank to default)
    // The second part of the path is the folder - note that / must be replaced with ~ in folder
    void apiFileList(const String &reqStr, String& respStr, const APISourceInfo& sourceInfo);

    // Read file contents
    // In the reqStr the first part of the path is the file system name (e.g. sd or local)
    // The second part of the path is the folder and filename - note that / must be replaced with ~ in folder
    void apiFileRead(const String &reqStr, String& respStr, const APISourceInfo& sourceInfo);

    // Delete file on the file system
    // In the reqStr the first part of the path is the file system name (e.g. sd or local)
    // The second part of the path is the filename - note that / must be replaced with ~ in filename
    void apiDeleteFile(const String &reqStr, String& respStr, const APISourceInfo& sourceInfo);

    // API upload file to file system - completed
    void apiUploadFileComplete(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo);

    // Upload file to file system - part of file (from HTTP POST file)
    bool apiUploadFileBlock(const String& req, FileStreamBlock& fileStreamBlock, const APISourceInfo& sourceInfo);

};
