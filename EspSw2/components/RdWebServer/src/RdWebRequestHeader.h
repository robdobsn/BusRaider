/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RdWebServer
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>
#include <WString.h>
#include <RdJson.h>
#include "RdWebInterface.h"

class RdWebRequestHeaderExtract
{
public:
    RdWebRequestHeaderExtract()
    {
        clear();
    }

    void clear()
    {
        method = WEB_METHOD_NONE;
        host.clear();
        contentType.clear();
        multipartBoundary.clear();
        authorization.clear();
        isMultipart = false;
        isDigest = false;
        contentLength = 0;
    }

    // Request method
    RdWebServerMethod method;

    // Host - from Host header
    String host;

    // Content type
    String contentType;

    // Multipart boundary
    String multipartBoundary;

    // Is multipart message
    bool isMultipart;

    // Content length
    uint32_t contentLength;

    // Authorization
    String authorization;
    bool isDigest;
};

// Web request header info
class RdWebRequestHeader
{
public:
    RdWebRequestHeader()
    {
        clear();
    }

    void clear()
    {
        gotFirstLine = false;
        isComplete = false;
        URIAndParams.clear();
        URL.clear();
        params.clear();
        versStr.clear();
        nameValues.clear();
        nameValues.reserve(MAX_WEB_HEADERS/2);
        isContinue = false;
        reqConnType = REQ_CONN_TYPE_HTTP;
        extract.clear();
    }

    // Got first line (which contains request)
    bool gotFirstLine;

    // Is complete
    bool isComplete;

    // URI (inc params)
    String URIAndParams;

    // URL (excluding params)
    String URL;

    // Params
    String params;

    // Version
    String versStr;

    // Header name/value pairs
    static const uint32_t MAX_WEB_HEADERS = 20;
    std::vector<RdJson::NameValuePair> nameValues;

    // Header extract
    RdWebRequestHeaderExtract extract;

    // Continue required
    bool isContinue;

    // Requested connection type
    RdWebReqConnectionType reqConnType;

    // WebSocket info
    String webSocketKey;
    String webSocketVersion;

};
