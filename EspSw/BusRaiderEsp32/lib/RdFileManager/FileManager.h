// FileManager 
// Rob Dobson 2018

#pragma once
#include "Arduino.h"
#include "FS.h"
#include "SPIFFS.h"

class FileManager
{
private:
    bool _enableSPIFFS;
    bool _defaultToSPIFFS;

    // Chunked file access
    static const int CHUNKED_BUF_MAXLEN = 1000;
    uint8_t _chunkedFileBuffer[CHUNKED_BUF_MAXLEN];
    int _chunkedFileInProgress;
    int _chunkedFilePos;
    String _chunkedFilename;
    int _chunkedFileLen;
    bool _chunkOnLineEndings;

public:
    FileManager()
    {
        _enableSPIFFS = false;
        _defaultToSPIFFS = true;
        _chunkedFileLen = 0;
        _chunkedFilePos = 0;
        _chunkedFileInProgress = false;
    }

    void setup(ConfigBase& config)
    {
        // Get config
        ConfigBase fsConfig(config.getString("fileManager", "").c_str());
        Log.notice("FileManager: config %s\n", fsConfig.getConfigData());

        // See if SPIFFS enabled
        _enableSPIFFS = fsConfig.getLong("spiffsEnabled", 0) != 0;
        bool spiffsFormatIfCorrupt = fsConfig.getLong("spiffsFormatIfCorrupt", 0) != 0;

        // Init if required
        if (_enableSPIFFS)
        {
            if (!SPIFFS.begin(spiffsFormatIfCorrupt))
                Log.warning("File system SPIFFS failed to initialise\n");
        }
    }
    
    bool getFilesJSON(const String& fileSystemStr, const String& folderStr, String& respStr)
    {
        // Check file system supported
        if ((fileSystemStr.length() > 0) && (fileSystemStr != "SPIFFS"))
        {
            respStr = "{\"rslt\":\"fail\",\"error\":\"unknownfs\",\"files\":[]}";
            return false;
        }

        // Only SPIFFS currently
        fs::FS fs = SPIFFS;
        size_t spiffsSize = SPIFFS.totalBytes();
        size_t spiffsUsed = SPIFFS.usedBytes();

        // Open folder
        File base = fs.open(folderStr.c_str());
        if (!base)
        {
            respStr = "{\"rslt\":\"fail\",\"error\":\"nofolder\",\"files\":[]}";
            return false;
        }
        if (!base.isDirectory())
        {
            respStr = "{\"rslt\":\"fail\",\"error\":\"notfolder\",\"files\":[]}";
            return false;
        }

        // Iterate over files
        File file = base.openNextFile();
        respStr = "{\"rslt\":\"ok\",\"diskSize\":" + String(spiffsSize) + ",\"diskUsed\":" + 
                    spiffsUsed + ",\"folder\":\"" + String(folderStr) + "\",\"files\":[";
        bool firstFile = true;
        while (file)
        {
            if (!file.isDirectory())
            {
                if (!firstFile)
                    respStr += ",";
                firstFile = false;
                respStr += "{\"name\":\"";
                respStr += file.name();
                respStr += "\",\"size\":";
                respStr += String(file.size());
                respStr += "}";
            }

            // Next
            file = base.openNextFile();
        }
        respStr += "]}";
        return true;
    }

    String getFileContents(const char* fileSystem, const String& filename, int maxLen)
    {
        // Only SPIFFS supported
        if (strcmp(fileSystem, "SPIFFS") != 0)
            return "";

        // Open file
        File file = SPIFFS.open(filename, FILE_READ);
        if (!file)
        {
            Log.trace("FileManager: failed to open file %s\n", filename);
            return "";
        }

        // Buffer
        uint8_t* pBuf = new uint8_t[maxLen+1];
        if (!pBuf)
        {
            Log.trace("FileManager: failed to allocate %d\n", maxLen);
            return "";
        }

        // Read
        size_t bytesRead = file.read(pBuf, maxLen);
        file.close();
        pBuf[bytesRead] = 0;
        String readData = (char*)pBuf;
        delete [] pBuf;
        return readData;
    }

    void uploadAPIBlockHandler(const char* fileSystem, const String& filename, int fileLength, size_t index, uint8_t *data, size_t len, bool finalBlock)
    {
        Log.trace("FileManager: uploadAPIBlockHandler fileSys %s, filename %s, total %d, idx %d, len %d, final %d\n", 
                    fileSystem, filename.c_str(), fileLength, index, len, finalBlock);
        if (strcmp(fileSystem, "SPIFFS") != 0)
            return;

        // Access type
        const char* accessType = FILE_WRITE;
        if (index > 0)
        {
            accessType = FILE_APPEND;
        }
        // Write file block
        File file = SPIFFS.open("/__tmp__", accessType);
        if (!file)
        {
            Log.trace("FileManager: failed to open __tmp__ file\n");
            return;
        }
        if (!file.write(data, len))
        {
            Log.trace("FileManager: failed write to __tmp__ file\n");
        }

        // Rename if last block
        if (finalBlock)
        {
            // Remove in case filename already exists
            SPIFFS.remove("/" + filename);
            // Rename
            if (!SPIFFS.rename("/__tmp__", "/" + filename))
            {
                Log.trace("FileManager: failed rename __tmp__ to %s\n", filename.c_str());
            }
        }
    }

    bool deleteFile(const String& fileSystemStr, const String& filename)
    {
        // Check file system supported
        if ((fileSystemStr.length() > 0) && (fileSystemStr != "SPIFFS"))
        {
            return false;
        }
        if (!SPIFFS.remove("/" + filename))
        {
            Log.notice("FileManager: failed to remove file %s\n", filename.c_str());
            return false; 
        }               
        return true;
    }

    bool chunkedFileStart(const String& fileSystemStr, const String& filename, bool readByLine)
    {
        // Check file system supported
        if ((fileSystemStr.length() > 0) && (fileSystemStr != "SPIFFS"))
        {
            return false;
        }

        // Check file valid
        File file = SPIFFS.open("/" + filename, FILE_READ);
        if (!file)
        {
            Log.trace("FileManager: chunked failed to open to %s file\n", filename.c_str());
            return false;
        }
        _chunkedFileLen = file.size();
        file.close();
        
        // Setup access
        _chunkedFilename = filename;
        _chunkedFileInProgress = true;
        _chunkedFilePos = 0;
        _chunkOnLineEndings = readByLine;
        return true;
    }

    uint8_t* chunkFileNext(String& filename, int& fileLen, int& chunkPos, int& chunkLen, bool& finalChunk)
    {
        // Check valid
        chunkLen = 0;
        if (!_chunkedFileInProgress)
            return NULL;

        // Return details
        filename = _chunkedFilename;
        fileLen = _chunkedFileLen;
        chunkPos = _chunkedFilePos;

        // Open file and seek
        File file = SPIFFS.open("/" + _chunkedFilename, FILE_READ);
        if (!file)
        {
            Log.trace("FileManager: chunkNext failed open %s\n", _chunkedFilename.c_str());
            return NULL;
        }
        if (!file.seek(_chunkedFilePos))
        {
            Log.trace("FileManager: chunkNext failed seek %d\n", _chunkedFilePos);
            file.close();
            return NULL;
        }

        // Fill the buffer with file data
        chunkLen = file.read(_chunkedFileBuffer, CHUNKED_BUF_MAXLEN);

        // Bump position and check if this was the final block
        _chunkedFilePos = _chunkedFilePos + CHUNKED_BUF_MAXLEN;
        if ((file.position() < _chunkedFilePos) || (chunkLen < CHUNKED_BUF_MAXLEN))
        {
            finalChunk = true;
            _chunkedFileInProgress = false;
        }

        // Close
        file.close();
        return _chunkedFileBuffer;
    }
};
