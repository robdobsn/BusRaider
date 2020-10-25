/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RestAPIEndpointDef
// Endpoint definition for REST API implementations
//
// Rob Dobson 2012-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <functional>
#include <WString.h>
#include "FileBlockInfo.h"

// Callback function for any endpoint
typedef std::function<void(String &reqStr, String &respStr)> RestAPIFunction;
typedef std::function<void(String &reqStr, const uint8_t *pData, size_t len, size_t index, size_t total)> RestAPIFnBody;
typedef std::function<void(String &reqStr, FileBlockInfo& fileBlockInfo)> RestAPIFnUpload;

// Definition of an endpoint
class RestAPIEndpointDef
{
public:
    enum EndpointType
    {
        ENDPOINT_NONE = 0,
        ENDPOINT_CALLBACK = 1
    };

    enum EndpointMethod
    {
        ENDPOINT_GET = 0,
        ENDPOINT_POST = 1,
        ENDPOINT_PUT = 2,
        ENDPOINT_DELETE = 3
    };

    enum EndpointCache_t
    {
        ENDPOINT_CACHE_NEVER,
        ENDPOINT_CACHE_ALWAYS
    };

    RestAPIEndpointDef(const char *pStr, EndpointType endpointType,
                       EndpointMethod endpointMethod,
                       RestAPIFunction callback,
                       const char *pDescription,
                       const char *pContentType,
                       const char *pContentEncoding,
                       EndpointCache_t cacheControl,
                       const char *pExtraHeaders,
                       RestAPIFnBody callbackBody,
                       RestAPIFnUpload callbackUpload
                       )
    {
        _endpointStr = pStr;
        _endpointType = endpointType;
        _endpointMethod = endpointMethod;
        _callback = callback;
        _callbackBody = callbackBody;
        _callbackUpload = callbackUpload;
        _description = pDescription;
        if (pContentType)
            _contentType = pContentType;
        if (pContentEncoding)
            _contentEncoding = pContentEncoding;
        _cacheControl = cacheControl;
        if (pExtraHeaders)
            _extraHeaders = pExtraHeaders;
    };

    String _endpointStr;
    EndpointType _endpointType;
    EndpointMethod _endpointMethod;
    String _description;
    String _contentType;
    String _contentEncoding;
    RestAPIFunction _callback;
    RestAPIFnBody _callbackBody;
    RestAPIFnUpload _callbackUpload;
    EndpointCache_t _cacheControl;
    String _extraHeaders;

    void callback(String &req, String &resp)
    {
        if (_callback)
            _callback(req, resp);
    }

    void callbackBody(String&req, const uint8_t *pData, size_t len, size_t bufferPos, size_t total)
    {
        if (_callbackBody)
            _callbackBody(req, pData, len, bufferPos, total);
    }

    void callbackUpload(String&req, FileBlockInfo& fileBlockInfo)
    {
        if (_callbackUpload)
            _callbackUpload(req, fileBlockInfo);
    }
};
