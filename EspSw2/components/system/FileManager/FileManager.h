/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FileManager 
// Handles SPIFFS and SD card file access
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
#include "FileBlockInfo.h"

class RestAPIEndpointManager;

class FileManager : public SysModBase
{
public:
    FileManager(const char* pModuleName, ConfigBase& defaultConfig, ConfigBase* pGlobalConfig, ConfigBase* pMutableConfig);

protected:
    // Setup
    virtual void setup() override final;

    // Service - called frequently
    virtual void service() override final;

    // Add endpoints
    virtual void addRestAPIEndpoints(RestAPIEndpointManager& endpointManager) override final;

private:
    // Helpers
    void applySetup();

    // Get system version
    void apiGetVersion(const String &reqStr, String& respStr);

    // Format file system
    void apiReformatFS(const String &reqStr, String& respStr);

    // List files on a file system
    // Uses FileSystem.h
    // In the reqStr the first part of the path is the file system name (e.g. sd or spiffs, can be blank to default)
    // The second part of the path is the folder - note that / must be replaced with ~ in folder
    void apiFileList(const String &reqStr, String& respStr);

    // Read file contents
    // Uses FileSystem.h
    // In the reqStr the first part of the path is the file system name (e.g. sd or spiffs)
    // The second part of the path is the folder and filename - note that / must be replaced with ~ in folder
    void apiFileRead(const String &reqStr, String& respStr);

    // Delete file on the file system
    // Uses FileSystem.h
    // In the reqStr the first part of the path is the file system name (e.g. sd or spiffs)
    // The second part of the path is the filename - note that / must be replaced with ~ in filename
    void apiDeleteFile(const String &reqStr, String& respStr);

    // Upload file to file system - completed
    void apiUploadToFileManComplete(const String &reqStr, String &respStr);

    // Upload file to file system - part of file (from HTTP POST file)
    void apiUploadToFileManPart(const String& req, FileBlockInfo& fileBlockInfo);

};
