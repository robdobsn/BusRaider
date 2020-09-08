/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RestAPIEndpointManager
// Endpoints for REST API implementations
//
// Rob Dobson 2012-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <functional>
#include <vector>
#include <WString.h>
#include <RdJson.h>
#include "RestAPIEndpointDef.h"

// Collection of endpoints
class RestAPIEndpointManager
{
public:
    RestAPIEndpointManager();

    virtual ~RestAPIEndpointManager();

    // Get number of endpoints
    int getNumEndpoints();

    // Get nth endpoint
    RestAPIEndpointDef *getNthEndpoint(int n);

    // Add an endpoint
    void addEndpoint(const char *pEndpointStr, 
                    RestAPIEndpointDef::EndpointType endpointType,
                    RestAPIEndpointDef::EndpointMethod endpointMethod,
                    RestAPIFunction callback,
                    const char *pDescription,
                    const char *pContentType = NULL,
                    const char *pContentEncoding = NULL,
                    RestAPIEndpointDef::EndpointCache_t pCache = RestAPIEndpointDef::ENDPOINT_CACHE_NEVER,
                    const char *pExtraHeaders = NULL,
                    RestAPIFnBody callbackBody = NULL,
                    RestAPIFnUpload callbackUpload = NULL);

    // Get the endpoint definition corresponding to a requested endpoint
    RestAPIEndpointDef *getEndpoint(const char *pEndpointStr);

    // Handle an API request
    bool handleApiRequest(const char *requestStr, String &retStr);

    // Get matching endpoint def
    RestAPIEndpointDef* getMatchingEndpointDef(const char *requestStr);

    // Form a string from a char buffer with a fixed length
    static void formStringFromCharBuf(String &outStr, const char *pStr, int len);

    // Remove first argument from string
    static String removeFirstArgStr(const char *argStr);

    // Get Nth argument from a string
    static String getNthArgStr(const char *argStr, int argIdx, bool splitOnQuestionMark = true);

    // Get position and length of nth arg
    static const char *getArgPtrAndLen(const char *argStr, int argIdx, int &argLen, bool splitOnQuestionMark = true);

    // Num args from an argStr
    static int getNumArgs(const char *argStr);

    // Convert encoded URL
    static String unencodeHTTPChars(String &inStr);

    static const char *getEndpointTypeStr(RestAPIEndpointDef::EndpointType endpointType);

    static const char *getEndpointMethodStr(RestAPIEndpointDef::EndpointMethod endpointMethod);

    // URL Parser
    static bool getParamsAndNameValues(const char* reqStr, std::vector<String>& params, std::vector<RdJson::NameValuePair>& nameValuePairs);

private:
    // Vector of endpoints
    std::vector<RestAPIEndpointDef> _endpointsList;
};
