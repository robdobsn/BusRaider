/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FileSystemChunker
//
// Rob Dobson 2018-2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "FileSystemChunker.h"
#include "FileSystem.h"
#include <Logger.h>

// #define DEBUG_FILE_CHUNKER
// #define DEBUG_FILE_CHUNKER_PERFORMANCE
// #define DEBUG_FILE_CHUNKER_CONTENTS
#define DEBUG_FILE_CHUNKER_READ_THRESH_MS 50

#if defined(DEBUG_FILE_CHUNKER) || defined(DEBUG_FILE_CHUNKER_CONTENTS) || defined(DEBUG_FILE_CHUNKER_PERFORMANCE) || defined(DEBUG_FILE_CHUNKER_READ_THRESH_MS)
static const char* MODULE_PREFIX = "FileSystemChunker";
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FileSystemChunker::FileSystemChunker()
{
    _curPos = 0;
    _chunkMaxLen = 0;
    _fileLen = 0;
    _readByLine = false;
    _isActive = false;
    _pFile = nullptr;
    _writing = false;
    _keepOpen = false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Destructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FileSystemChunker::~FileSystemChunker()
{
    if (_pFile)
        fileSystem.fileClose(_pFile);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Start chunk access
// chunkMaxLen may be 0 if writing
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool FileSystemChunker::start(const String& filePath, uint32_t chunkMaxLen, bool readByLine, bool writing, bool keepOpen)
{
    //Check if already busy
    if (_isActive)
        return false;

    // Get file details
    if (!writing && !fileSystem.getFileInfo("", filePath, _fileLen))
    {
        LOG_W(MODULE_PREFIX, "start cannot getFileInfo %s", filePath);
        return false;
    }

    // Store
    _chunkMaxLen = chunkMaxLen;
    _readByLine = readByLine;
    _filePath = filePath;
    _writing = writing;
    _keepOpen = keepOpen;
    _isActive = true;
    _curPos = 0;

#ifdef DEBUG_FILE_CHUNKER
    // Debug
    LOG_I(MODULE_PREFIX, "start filename %s size %d byLine %s", 
            filePath.c_str(), _fileLen, (_readByLine ? "Y" : "N"));
#endif
    return true; 
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Next chunk read
// Returns false on failure
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool FileSystemChunker::nextRead(uint8_t* pBuf, uint32_t bufLen, uint32_t& handledBytes, bool& finalChunk)
{
    // Check valid
    finalChunk = false;
    if (!_isActive)
        return false;

    // Ensure we don't read beyond buffer
    uint32_t maxToRead = bufLen < _chunkMaxLen ? bufLen : _chunkMaxLen;
    handledBytes = 0;

    // Check if keep open
    if (_keepOpen)
        return nextReadKeepOpen(pBuf, bufLen, handledBytes, finalChunk, maxToRead);

    // Check if read by line
    if (_readByLine)
    {
        uint32_t finalFilePos = 0;
        bool readOk = fileSystem.getFileLine("", _filePath, _curPos, pBuf, maxToRead, finalFilePos);
        _curPos = finalFilePos;
        if (!readOk)
        {
            _isActive = false;
            finalChunk = true;
        }
        else
        {
            handledBytes = strlen((char*)pBuf);
        }

#ifdef DEBUG_FILE_CHUNKER
        // Debug
        LOG_I(MODULE_PREFIX, "next byLine filename %s readOk %s pos %d read %d busy %s", 
                _filePath.c_str(), readOk ? "YES" : "NO", _curPos, handledBytes, _isActive ? "YES" : "NO");
#endif

        return readOk;
    }

    // Must be reading blocks
    bool readOk = fileSystem.getFileSection("", _filePath, _curPos, pBuf, maxToRead, handledBytes);
    _curPos += handledBytes;
    if (!readOk)
    {
        _isActive = false;
    }
    if (_curPos == _fileLen)
    {
        _isActive = false;
        finalChunk = true; 
    }

#ifdef DEBUG_FILE_CHUNKER
        // Debug
        LOG_I(MODULE_PREFIX, "next binary filename %s readOk %s pos %d read %d busy %s", 
                _filePath.c_str(), readOk ? "YES" : "NO", _curPos, handledBytes, _isActive ? "YES" : "NO");
#endif
#ifdef DEBUG_FILE_CHUNKER_CONTENTS
        String debugStr;
        Utils::getHexStrFromBytes(pBuf, bufLen, debugStr);
        LOG_I(MODULE_PREFIX, "CHUNK: %s", debugStr.c_str());
#endif

    return readOk;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Next chunk read
// Returns false on failure
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool FileSystemChunker::nextReadKeepOpen(uint8_t* pBuf, uint32_t bufLen, uint32_t& handledBytes, 
                        bool& finalChunk, uint32_t numToRead)
{
#ifdef DEBUG_FILE_CHUNKER_READ_THRESH_MS
    uint32_t debugChunkerReadStartMs = millis();
    uint32_t debugStartMs = millis();
    uint32_t debugFileOpenTimeMs = 0;
    uint32_t debugReadTimeMs = 0;
    uint32_t debugCloseTimeMs = 0;
#endif

    // Check if file is open already
    if (!_pFile)
    {
        _pFile = fileSystem.fileOpen("", _filePath, _writing, _curPos);
#ifdef DEBUG_FILE_CHUNKER_READ_THRESH_MS
        debugFileOpenTimeMs = millis() - debugStartMs;
        debugStartMs = millis();
#endif
    }

    // Read if valid
    if (_pFile)
    {
        // Read
        handledBytes = fileSystem.fileRead(_pFile, pBuf, bufLen);

#ifdef DEBUG_FILE_CHUNKER_READ_THRESH_MS
        debugReadTimeMs = millis() - debugStartMs;
        debugStartMs = millis();
#endif

        if (handledBytes != bufLen)
        {
            // Close on final chunk
            finalChunk = true;
            fileSystem.fileClose(_pFile);
            _pFile = nullptr;
            _isActive = false;
        }
    }

#ifdef DEBUG_FILE_CHUNKER_READ_THRESH_MS
    if (millis() - debugChunkerReadStartMs > DEBUG_FILE_CHUNKER_READ_THRESH_MS)
    {
        debugCloseTimeMs = millis() - debugStartMs;
        LOG_I(MODULE_PREFIX, "nextReadKeepOpen fileOpen %dms read %dms close %dms filename %s readBytes %d busy %s", 
                debugFileOpenTimeMs, debugReadTimeMs, debugCloseTimeMs,
                _filePath.c_str(), handledBytes, _isActive ? "YES" : "NO");
    }
#endif
#ifdef DEBUG_FILE_CHUNKER
    // Debug
    LOG_I(MODULE_PREFIX, "nextReadKeepOpen filename %s readBytes %d busy %s", 
            _filePath.c_str(), handledBytes, _isActive ? "YES" : "NO");
#endif
#ifdef DEBUG_FILE_CHUNKER_CONTENTS
    String debugStr;
    Utils::getHexStrFromBytes(pBuf, handledBytes, debugStr);
    LOG_I(MODULE_PREFIX, "nextReadKeepOpen: %s", debugStr.c_str());
#endif

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Next chunk write
// Returns false on failure
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool FileSystemChunker::nextWrite(const uint8_t* pBuf, uint32_t bufLen, uint32_t& handledBytes, bool& finalChunk)
{
#ifdef DEBUG_FILE_CHUNKER_PERFORMANCE
    uint32_t startMs = millis();
#endif

    // Check valid
    handledBytes = 0;
    if (!_isActive)
        return false;

    // Check if file is open already
    if (!_pFile)
        _pFile = fileSystem.fileOpen("", _filePath, _writing, _curPos);

    // Write if valid
    bool writeOk = false;
    if (_pFile)
    {
        // Write
        handledBytes = fileSystem.fileWrite(_pFile, pBuf, bufLen);
        writeOk = handledBytes == bufLen;

        // Check if we should keep open
        if (!_keepOpen || finalChunk)
        {
            fileSystem.fileClose(_pFile);
            _pFile = nullptr;
        }
    }

#ifdef DEBUG_FILE_CHUNKER
        // Debug
        LOG_I(MODULE_PREFIX, "nextWrite filename %s writeOk %s written %d busy %s", 
                _filePath.c_str(), writeOk ? "YES" : "NO", handledBytes, _isActive ? "YES" : "NO");
#endif
#ifdef DEBUG_FILE_CHUNKER_CONTENTS
        String debugStr;
        Utils::getHexStrFromBytes(pBuf, bufLen, debugStr);
        LOG_I(MODULE_PREFIX, "nextWrite: %s", debugStr.c_str());
#endif

#ifdef DEBUG_FILE_CHUNKER_PERFORMANCE
    LOG_I(MODULE_PREFIX, "nextWrite elapsed ms %ld", millis() - startMs);
#endif

    return writeOk;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// End
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileSystemChunker::end()
{
    _isActive = false;
    relax();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Relax (closes file if open)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileSystemChunker::relax()
{
    // Check if open
    if (_pFile)
    {
        fileSystem.fileClose(_pFile);
        _pFile = nullptr;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get file position
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t FileSystemChunker::getFilePos() const
{
    if (_keepOpen)
    {
        if (_pFile)
            return fileSystem.filePos(_pFile);
        return 0;
    }
    return _curPos;
}
