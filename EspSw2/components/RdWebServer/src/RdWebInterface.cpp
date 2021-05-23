/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RdWebServer
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "RdWebInterface.h"

// Web Methods
const char* RdWebInterface::getHTTPMethodStr(RdWebServerMethod method)
{
    switch(method)
    {
        case WEB_METHOD_NONE: return "NONE";
        case WEB_METHOD_GET: return "GET";
        case WEB_METHOD_POST: return "POST";
        case WEB_METHOD_DELETE: return "DELETE";
        case WEB_METHOD_PUT: return "PUT";
        case WEB_METHOD_PATCH: return "PATCH";
        case WEB_METHOD_HEAD: return "HEAD";
        case WEB_METHOD_OPTIONS: return "OPTIONS";
    }
    return "NONE";
}

// Get conn type string
const char* RdWebInterface::getReqConnTypeStr(RdWebReqConnectionType reqConnType)
{
    switch(reqConnType)
    {
        default:
        case REQ_CONN_TYPE_NONE: return "NONE";
        case REQ_CONN_TYPE_HTTP: return "HTTP";
        case REQ_CONN_TYPE_WEBSOCKET: return "WEBSOCKET";
        case REQ_CONN_TYPE_EVENT: return "DELETE"; 
    }
    return "NONE";
}

// ESP-IDF error codes
const char* RdWebInterface::espIdfErrToStr(err_t err)
{
    switch(err)
    {
        case ERR_OK: return "OK";
        case ERR_MEM: return "Out of Mem";
        case ERR_BUF: return "Buffer error";
        case ERR_TIMEOUT: return "Timeout";
        case ERR_RTE: return "Routing problem";
        case ERR_INPROGRESS: return "Op in progress";
        case ERR_VAL: return "Illegal value";
        case ERR_WOULDBLOCK: return "Op would block";
        case ERR_USE: return "Addr in Use";
        case ERR_ALREADY: return "Already connecting";
        case ERR_ISCONN: return "Already connected";
        case ERR_CONN: return "Write error";
        case ERR_IF: return "NETIF error";
        case ERR_ABRT: return "Conn aborted";
        case ERR_RST: return "Conn reset";
        case ERR_CLSD: return "Conn closed";
        case ERR_ARG: return "Illegal arg";
    }
    return "UNKNOWN";
}

// HTTP status codes
const char* RdWebInterface::getHTTPStatusStr(RdHttpStatusCode status)
{
    switch(status)
    {
        case HTTP_STATUS_CONTINUE: return "Continue";
        case HTTP_STATUS_SWITCHING_PROTOCOLS: return "Switching Protocols";
        case HTTP_STATUS_OK: return "OK";
        case HTTP_STATUS_NOCONTENT: return "No Content";
        case HTTP_STATUS_BADREQUEST: return "Bad Request";
        case HTTP_STATUS_FORBIDDEN: return "Forbidden";
        case HTTP_STATUS_NOTFOUND: return "Not Found";
        case HTTP_STATUS_METHODNOTALLOWED: return "Method Not Allowed";
        case HTTP_STATUS_REQUESTTIMEOUT: return "Request Time-out";
        case HTTP_STATUS_LENGTHREQUIRED: return "Length Required";
        case HTTP_STATUS_PAYLOADTOOLARGE: return "Request Entity Too Large";
        case HTTP_STATUS_URITOOLONG: return "Request-URI Too Large";
        case HTTP_STATUS_UNSUPPORTEDMEDIATYPE: return "Unsupported Media Type";
        case HTTP_STATUS_NOTIMPLEMENTED: return "Not Implemented";
        case HTTP_STATUS_SERVICEUNAVAILABLE: return "Service Unavailable";
        default: return "See W3 ORG";
    }
    return "";
}
