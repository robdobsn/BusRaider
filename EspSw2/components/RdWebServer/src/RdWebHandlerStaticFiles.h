/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RdWebServer
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef ESP8266

#include "RdWebHandler.h"
#include <Logger.h>

class RdWebRequest;
class RdWebRequestHeader;

class RdWebHandlerStaticFiles : public RdWebHandler
{
public:
    RdWebHandlerStaticFiles(const char* pBaseURI, const char* pBaseFolder, 
            const char* pCacheControl, const char* pDefaultPath);
    virtual ~RdWebHandlerStaticFiles();
    virtual const char* getName() override;
    virtual RdWebResponder* getNewResponder(const RdWebRequestHeader& requestHeader, 
                const RdWebRequestParams& params, const RdWebServerSettings& webServerSettings) override final;
    virtual bool isFileHandler() override final
    {
        return true;
    }
private:
    // URI
    String _baseURI;

    // Folder
    String _baseFolder;
    bool _isBaseReallyAFolder;

    // Default path (for response to /)
    String _defaultPath;

    // Cache
    String _cacheControl;
    String _lastModifiedTimeStr;

    // GZip
    bool _gzipFirst;
    bool _gzipStats;

    // Helpers
    bool urlFileExists(const RdWebRequestHeader& header, String& filePath);
    String getFilePath(const RdWebRequestHeader& header, bool defaultPath);

};

#endif
