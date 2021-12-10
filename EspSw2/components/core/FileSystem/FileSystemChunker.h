/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FileSystemChunker
//
// Rob Dobson 2018-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <WString.h>
#include <Utils.h>

class FileSystemChunker
{
public:

    // Constructor
    FileSystemChunker();

    // Destructor
    ~FileSystemChunker();

    // Start access to a file in chunks
    // Returns false on failure
    bool start(const String& filePath, uint32_t chunkMaxLen, bool readByLine, bool writing, bool keepOpen);

    // Read or write next chunk of file
    // Returns false on failure
    bool nextRead(uint8_t* pBuf, uint32_t bufLen, uint32_t& handledBytes, bool& finalChunk);

    // Write next chunk of file
    // Returns false on failure
    bool nextWrite(const uint8_t* pBuf, uint32_t bufLen, uint32_t& handledBytes, bool& finalChunk);

    // End file access
    void end();

    // Relax (closes file if open)
    void relax();

    // Get file length
    uint32_t getFileLen()
    {
        return _fileLen;
    }

    // Is active
    bool isActive()
    {
        return _isActive;
    }

    // Get file name
    String& getFileName()
    {
        return _filePath;
    }

    // Get file position
    uint32_t getFilePos() const;

private:

    // File name
    String _filePath;

    // File length
    uint32_t _fileLen;

    // Current position
    uint32_t _curPos;

    // Chunk max length (may be 0 if writing)
    uint32_t _chunkMaxLen;

    // Read by line
    bool _readByLine;

    // Active
    bool _isActive;

    // Writing (and not reading)
    bool _writing;

    // Keep file open
    bool _keepOpen;

    // File ptr (when file is kept open)
    FILE* _pFile;

    // Helpers
    bool nextReadKeepOpen(uint8_t* pBuf, uint32_t bufLen, uint32_t& handledBytes, 
                        bool& finalChunk, uint32_t numToRead);
};
