/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RdWebServer
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef ESP8266

#include "RdWebHandlerStaticFiles.h"
#include "RdWebConnection.h"
#include "RdWebResponder.h"
#include "RdWebResponderFile.h"
#include <Logger.h>
#include <FileSystem.h>

// #define DEBUG_STATIC_FILE_HANDLER 1

#ifdef DEBUG_STATIC_FILE_HANDLER
static const char* MODULE_PREFIX = "RdWebHStatFile";
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RdWebHandlerStaticFiles::RdWebHandlerStaticFiles(const char* pBaseURI, 
                const char* pBaseFolder, const char* pCacheControl,
                const char* pDefaultPath)
{
    // Default path (for response to /)
    _defaultPath = pDefaultPath;

    // Handle URI and base folder
    if (pBaseURI)
        _baseURI = pBaseURI;
    if (pBaseFolder)
        _baseFolder = pBaseFolder;
    if (pCacheControl)
        _cacheControl = pCacheControl;

    // Ensure paths and URLs have a leading /
    if (_baseURI.length() == 0 || _baseURI[0] != '/')
        _baseURI = "/" + _baseURI;
    if (_baseFolder.length() == 0 || _baseFolder[0] != '/')
        _baseFolder = "/" + _baseFolder;

    // Check if baseFolder is actually a folder
    _isBaseReallyAFolder = _baseFolder.endsWith("/");

    // Remove trailing /
    if (_baseURI.endsWith("/"))
        _baseURI.remove(_baseURI.length()-1); 
    if (_baseFolder.endsWith("/"))
        _baseFolder.remove(_baseFolder.length()-1); 
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Destructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RdWebHandlerStaticFiles::~RdWebHandlerStaticFiles()
{        
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// getName of the handler
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const char* RdWebHandlerStaticFiles::getName()
{
    return "HandlerStaticFiles";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get a responder if we can handle this request
// NOTE: this returns a new object or NULL
// NOTE: if a new object is returned the caller is responsible for deleting it when appropriate
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RdWebResponder* RdWebHandlerStaticFiles::getNewResponder(const RdWebRequestHeader& requestHeader, 
            const RdWebRequestParams& params, const RdWebServerSettings& webServerSettings)
{
    // Debug
#ifdef DEBUG_STATIC_FILE_HANDLER
    LOG_I(MODULE_PREFIX, "getNewResponder reqURL %s baseURI %s", requestHeader.URL.c_str(), _baseURI.c_str());    
#endif

    // Must be a GET
    if (requestHeader.extract.method != WEB_METHOD_GET)
        return NULL;

    // Check the URL is valid
    if (!requestHeader.URL.startsWith(_baseURI))
        return NULL;

    // Check that the connection type is HTTP
    if (requestHeader.reqConnType != REQ_CONN_TYPE_HTTP)
        return NULL;

    // Check file exists
    String filePath;
    if (!urlFileExists(requestHeader, filePath))
        return NULL;

    // Looks like we can handle this so create a new responder object
    RdWebResponder* pResponder = new RdWebResponderFile(filePath, this, params);

    // Debug
#ifdef DEBUG_STATIC_FILE_HANDLER
    LOG_W(MODULE_PREFIX, "canHandle constructed new responder %lx uri %s", (unsigned long)pResponder, requestHeader.URL.c_str());
#endif

    // Return new responder - caller must clean up by deleting object when no longer needed
    return pResponder;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check url file exists
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebHandlerStaticFiles::urlFileExists(const RdWebRequestHeader& header, String& filePath)
{
    // Debug
#ifdef DEBUG_STATIC_FILE_HANDLER
    LOG_I(MODULE_PREFIX, "urlFileExists URL %s", header.URL.c_str());
#endif

    // Check if the path is just root
    if (header.URL.equals("/"))
        filePath = getFilePath(header, true);
    else
        filePath = getFilePath(header, false);

    // Check exists
    bool fileExists = fileSystem.exists(filePath.c_str());

    // Debug
#ifdef DEBUG_STATIC_FILE_HANDLER
    LOG_I(MODULE_PREFIX, "urlFileExists path %s %s", filePath.c_str(), fileExists ? "found ok" : "NOT FOUND");
#endif
    return fileExists;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get file path
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String RdWebHandlerStaticFiles::getFilePath(const RdWebRequestHeader& header, bool defaultPath)
{
    // Remove the base path from the URL
    String filePath;
    if (!defaultPath)
        filePath = header.URL.substring(_baseURI.length());
    else
        filePath = "/" + _defaultPath;

    // Add on the file path
    return _baseFolder + filePath;
}

#endif
