/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FileSystemChunker
//
// Rob Dobson 2018-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "FileSystemChunker.h"
#include "FileSystem.h"
#include <Logger.h>

#define DEBUG_FILE_CHUNKER
#define DEBUG_FILE_CHUNKER_CONTENTS

#if defined(DEBUG_FILE_CHUNKER) || defined(DEBUG_FILE_CHUNKER_CONTENTS)
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
    _isBusy = false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Destructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FileSystemChunker::~FileSystemChunker()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Start chunk access
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool FileSystemChunker::start(const String& filePath, uint32_t chunkMaxLen, bool readByLine)
{
    //Check if already busy
    if (_isBusy)
        return false;

    // Get file details
    if (!fileSystem.getFileInfo("", filePath, _fileLen))
        return false;

    // Store
    _chunkMaxLen = chunkMaxLen;
    _readByLine = readByLine;
    _filePath = filePath;
    _isBusy = true;
    _curPos = 0;

#ifdef DEBUG_FILE_CHUNKER
    // Debug
    LOG_I(MODULE_PREFIX, "start filename %s size %d byLine %s", 
            filePath.c_str(), _fileLen, (_readByLine ? "Y" : "N"));
#endif
    return true; 
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Next chunk
// Returns false on failure
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool FileSystemChunker::next(uint8_t* pBuf, uint32_t bufLen, uint32_t& readLen, bool& finalChunk)
{
    // Check valid
    finalChunk = false;
    if (!_isBusy)
        return false;

    // Ensure we don't read beyond buffer
    uint32_t maxToRead = bufLen < _chunkMaxLen ? bufLen : _chunkMaxLen;
    readLen = 0;

    // Check if read by line
    if (_readByLine)
    {
        uint32_t finalFilePos = 0;
        bool readOk = fileSystem.getFileLine("", _filePath, _curPos, pBuf, maxToRead, finalFilePos);
        _curPos = finalFilePos;
        if (!readOk)
        {
            _isBusy = false;
            finalChunk = true;
        }
        else
        {
            readLen = strlen((char*)pBuf);
        }

#ifdef DEBUG_FILE_CHUNKER
        // Debug
        LOG_I(MODULE_PREFIX, "next byLine filename %s readOk %s pos %d read %d busy %s", 
                _filePath.c_str(), readOk ? "YES" : "NO", _curPos, readLen, _isBusy ? "YES" : "NO");
#endif

        return readOk;
    }

    // Must be reading blocks
    bool readOk = fileSystem.getFileSection("", _filePath, _curPos, pBuf, maxToRead, readLen);
    _curPos += readLen;
    if (!readOk)
    {
        _isBusy = false;
    }
    if (_curPos == _fileLen)
    {
        _isBusy = false;
        finalChunk = true; 
    }

#ifdef DEBUG_FILE_CHUNKER
        // Debug
        LOG_I(MODULE_PREFIX, "next binary filename %s readOk %s pos %d read %d busy %s", 
                _filePath.c_str(), readOk ? "YES" : "NO", _curPos, readLen, _isBusy ? "YES" : "NO");
#endif
#ifdef DEBUG_FILE_CHUNKER_CONTENTS
        String debugStr;
        Utils::getHexStrFromBytes(pBuf, bufLen, debugStr);
        LOG_I(MODULE_PREFIX, "CHUNK: %s", debugStr.c_str());
#endif

    return readOk;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// End
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileSystemChunker::end()
{
    _isBusy = false;
}
