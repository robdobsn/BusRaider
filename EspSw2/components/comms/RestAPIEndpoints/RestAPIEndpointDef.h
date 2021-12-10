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
#include "FileStreamBlock.h"
#include "APISourceInfo.h"

// Callback function for any endpoint
typedef std::function<void(String &reqStr, String &respStr, const APISourceInfo& sourceInfo)> RestAPIFunction;
typedef std::function<void(String &reqStr, const uint8_t *pData, size_t len, size_t index, 
            size_t total, const APISourceInfo& sourceInfo)> RestAPIFnBody;
typedef std::function<void(String &reqStr, FileStreamBlock& fileStreamBlock, const APISourceInfo& sourceInfo)> RestAPIFnChunk;
typedef std::function<bool(const APISourceInfo& sourceInfo)> RestAPIFnIsReady;

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
                       RestAPIFunction callbackMain,
                       const char *pDescription,
                       const char *pContentType,
                       const char *pContentEncoding,
                       EndpointCache_t cacheControl,
                       const char *pExtraHeaders,
                       RestAPIFnBody callbackBody,
                       RestAPIFnChunk callbackChunk,
                       RestAPIFnIsReady callbackIsReady
                       )
    {
        _endpointStr = pStr;
        _endpointType = endpointType;
        _endpointMethod = endpointMethod;
        _callbackMain = callbackMain;
        _callbackBody = callbackBody;
        _callbackChunk = callbackChunk;
        _callbackIsReady = callbackIsReady;
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
    RestAPIFunction _callbackMain;
    RestAPIFnBody _callbackBody;
    RestAPIFnChunk _callbackChunk;
    RestAPIFnIsReady _callbackIsReady;
    EndpointCache_t _cacheControl;
    String _extraHeaders;

    void callbackMain(String &req, String &resp, const APISourceInfo& sourceInfo)
    {
        if (_callbackMain)
            _callbackMain(req, resp, sourceInfo);
    }

    void callbackBody(String&req, const uint8_t *pData, size_t len, size_t bufferPos, 
                        size_t total, const APISourceInfo& sourceInfo)
    {
        if (_callbackBody)
            _callbackBody(req, pData, len, bufferPos, total, sourceInfo);
    }

    void callbackChunk(String&req, FileStreamBlock& fileStreamBlock, const APISourceInfo& sourceInfo)
    {
        if (_callbackChunk)
            _callbackChunk(req, fileStreamBlock, sourceInfo);
    }

    bool callbackIsReady(const APISourceInfo& sourceInfo)
    {
        if (_callbackIsReady)
            return _callbackIsReady(sourceInfo);
        return true;
    }
};
