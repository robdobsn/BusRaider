/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FileSystemChunker
//
// Rob Dobson 2018-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <WString.h>

class FileSystemChunker
{
public:

    // Constructor
    FileSystemChunker();

    // Destructor
    ~FileSystemChunker();

    // Start access to a file in chunks
    // Returns false on failure
    bool start(const String& filePath, uint32_t chunkMaxLen, bool readByLine);

    // Get next chunk of file
    // Returns false on failure
    bool next(uint8_t* pBuf, uint32_t bufLen, uint32_t& readLen, bool& finalChunk);

    // End file access
    void end();

    // Get file length
    uint32_t getFileLen()
    {
        return _fileLen;
    }

    // Is busy
    bool isBusy()
    {
        return _isBusy;
    }

    // Get file name
    String& getFileName()
    {
        return _filePath;
    }

private:

    // File name
    String _filePath;

    // File length
    uint32_t _fileLen;

    // Current position
    uint32_t _curPos;

    // Chunk max length
    uint32_t _chunkMaxLen;

    // Read by line
    bool _readByLine;

    // Busy
    bool _isBusy;
};
