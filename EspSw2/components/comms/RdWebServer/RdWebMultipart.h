/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RdWebServer
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>
#include <WString.h>
#include <functional>
#include <vector>

class RdMultipartForm
{
public:
    RdMultipartForm()
    {
        clear();
    }
    void clear()
    {
        _fileName.clear();
        _name.clear();
        _contentType.clear();
        _contentDisp.clear();
        _crc16 = 0;
        _crc16Valid = false;
        _fileLenBytes = 0;
        _fileLenValid = false;

    }
    String _fileName;
    String _name;
    String _contentDisp;
    String _contentType;
    uint32_t _crc16;
    bool _crc16Valid;
    uint32_t _fileLenBytes;
    bool _fileLenValid;
};

enum RdMultipartEvent
{
    RDMULTIPART_EVENT_PART_BEGIN,
    RDMULTIPART_EVENT_PART_END,
    RDMULTIPART_EVENT_HEADER_BEGIN,
    RDMULTIPART_EVENT_HEADER_END,
    RDMULTIPART_EVENT_HEADER_FIELD,
    RDMULTIPART_EVENT_HEADER_VALUE,
    RDMULTIPART_EVENT_ALL_HEADERS_END,
    RDMULTIPART_EVENT_END,
};

typedef std::function<void(RdMultipartEvent event, const uint8_t *pBuf, uint32_t pos)> RdMultipartEventCB;
typedef std::function<void(const uint8_t *pBuf, uint32_t len, RdMultipartForm& formInfo, 
                uint32_t contentPos, bool isFinalPart)> RdMultipartDataCB;
typedef std::function<void(const String& name, const String& val)> RdMultipartHeaderNameValueCB;

class RdWebMultipart
{
public:
    RdMultipartEventCB onEvent;
    RdMultipartDataCB onData;
    RdMultipartHeaderNameValueCB onHeaderNameValue;

    RdWebMultipart();
    RdWebMultipart(const String &boundary);
    ~RdWebMultipart();
    void clear();
    void setBoundary(const String &boundary);
    bool handleData(const uint8_t *pBuf, uint32_t len);
    bool succeeded() const;
    bool hasError() const;
    bool stopped() const;

    static const char* getEventText(RdMultipartEvent event)
    {
        switch(event)
        {
            case RDMULTIPART_EVENT_PART_BEGIN: return("MultipartEventBegin");
            case RDMULTIPART_EVENT_PART_END: return("MultipartEventPartEnd");
            case RDMULTIPART_EVENT_HEADER_BEGIN: return("MultipartEventHeaderBegin");
            case RDMULTIPART_EVENT_HEADER_END: return("MultipartEventHeaderEnd");
            case RDMULTIPART_EVENT_HEADER_FIELD: return("MultipartEventHeaderField");
            case RDMULTIPART_EVENT_HEADER_VALUE: return("MultipartEventHeaderValue");
            case RDMULTIPART_EVENT_ALL_HEADERS_END: return("MultipartEventHeadersEnd");
            case RDMULTIPART_EVENT_END: return("MultipartEventEnd");
        }
        return "UNKNOWN";
    }

private:
    static const uint8_t CR = 13;
    static const uint8_t LF = 10;
    static const uint8_t SPACE = 32;
    static const uint8_t HYPHEN = 45;
    static const uint8_t COLON = 58;
    static const uint32_t INVALID_POS = UINT32_MAX;

    enum State
    {
        RDMULTIPART_ERROR,
        RDMULTIPART_START,
        RDMULTIPART_START_BOUNDARY,
        RDMULTIPART_HEADER_FIELD_START,
        RDMULTIPART_HEADER_FIELD,
        RDMULTIPART_HEADER_VALUE_START,
        RDMULTIPART_HEADER_VALUE,
        RDMULTIPART_HEADER_VALUE_GOT,
        RDMULTIPART_HEADERS_AWAIT_FINAL_LF,
        RDMULTIPART_PART_DATA,
        RDMULTIPART_END
    };

    // Boundary string
    String _boundaryStr;

    // Index of chars in boundaryStr for boyes-moore algorithm
    bool _boundaryCharMap[UINT8_MAX+1];

    // Buffer to handle unmatched boundaries
    std::vector<uint8_t> _boundaryBuf;

    // Parser state
    State _parseState;

    // Content pos
    uint32_t _contentPos;

    // Final part
    bool _isFinalPart;

    // Index in the boundary
    uint32_t _boundaryIdx;

    // Header name and value
    uint32_t _headerFieldStartPos;
    uint32_t _headerValueStartPos;
    String _headerName;

    // Form info
    RdMultipartForm _formInfo;

    // Debug
    uint32_t _debugBytesHandled;

    // Valid TCHARS
    static const uint32_t NUM_ASCII_VALS = 128;
    static const bool IS_VALID_TCHAR[NUM_ASCII_VALS];

    // Helpers
    void clearCallbacks();
    void indexBoundary();
    void stateCallback(RdMultipartEvent event, const uint8_t *pBuf, uint32_t pos);
    void dataCallback(const uint8_t *pBuf, uint32_t pos, uint32_t bufLen);
    uint8_t lower(uint8_t c) const;
    inline bool isBoundaryChar(uint8_t c) const;
    bool isHeaderFieldCharacter(uint8_t c) const;
    bool processHeaderByte(const uint8_t *buffer, uint32_t bufPos, uint32_t len);
    bool processPayload(const uint8_t *buffer, uint32_t bufPos, uint32_t len);
    void headerNameFound(const uint8_t* pBuf, uint32_t pos, uint32_t len);
    void headerValueFound(const uint8_t* pBuf, uint32_t pos, uint32_t len);

};
