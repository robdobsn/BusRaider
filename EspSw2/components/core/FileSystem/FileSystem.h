/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FileSystem
// Handles SPIFFS and SD card file access
//
// Rob Dobson 2018-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <WString.h>
#include <list>
#include <Utils.h>

class FileSystem
{
public:
    FileSystem();

    // Setup 
    void setup(bool enableSPIFFS, bool spiffsFormatIfCorrupt, bool enableSD, 
        int sdMOSIPin, int sdMISOPin, int sdCLKPin, int sdCSPin, bool defaultToSDIfAvailable,
        bool cacheFileList, int maxCacheBytes);

    // Reformat
    void reformat(const String& fileSystemStr, String& respStr);

    // Get path of root of default file system
    String getDefaultFSRoot();

    // Get a list of files on the file system as a JSON format string
    // {"rslt":"ok","diskSize":123456,"diskUsed":1234,"folder":"/","files":[{"name":"file1.txt","size":223},{"name":"file2.txt","size":234}]}
    bool getFilesJSON(const char* req, const String& fileSystemStr, const String& folderStr, String& respStr);

    // Get/Set file contents as a string
    String getFileContents(const String& fileSystemStr, const String& filename, int maxLen=0, bool cacheIfPossible=false);
    bool setFileContents(const String& fileSystemStr, const String& filename, String& fileContents);

    // Handle a file upload block - same API as ESPAsyncWebServer file handler
    void uploadAPIBlockHandler(const char* fileSystem, const String& req, const String& filename, 
                int fileLength, size_t filePos, const uint8_t *data, size_t len, bool finalBlock);
    void uploadAPIBlocksComplete();

    // Delete file on file system
    bool deleteFile(const String& fileSystemStr, const String& filename);
    
    // Test file exists and get info
    bool getFileInfo(const String& fileSystemStr, const String& filename, uint32_t& fileLength);

    // Get file name extension
    static String getFileExtension(String& filename);

    // Read line from file
    char* readLineFromFile(char* pBuf, int maxLen, FILE* pFile);

    // Exists - check if file exists
    bool exists(const char* path);

    // Stat (details on a file)
    typedef enum {
        FILE_SYSTEM_STAT_NO_EXIST,
        FILE_SYSTEM_STAT_DIR,
        FILE_SYSTEM_STAT_FILE,
    } FileSystemStatType;

    // Get stat
    FileSystemStatType pathType(const char* filename);

    // Get a file path using default file system if necessary
    bool getFileFullPath(const String& filename, String& fileFullPath);

    // Get a section of a file
    bool getFileSection(const String& fileSystemStr, const String& filename, uint32_t sectionStart, uint8_t* pBuf, 
            uint32_t sectionLen, uint32_t& readLen);

    // Get a line from a text file
    bool getFileLine(const String& fileSystemStr, const String& filename, uint32_t startFilePos, uint8_t* pBuf, 
            uint32_t lineMaxLen, uint32_t& fileCurPos);

private:

    // File system controls
    bool _enableSPIFFS;
    bool _spiffsIsOk;
    bool _enableSD;
    bool _defaultToSDIfAvailable;
    bool _sdIsOk;
    bool _cachedFileListValid;
    bool _cacheFileList;

    // SD card
    void* _pSDCard;

    // Cached file list response
    String _cachedFileListResponse;

    // Mutex controlling access to file system
    SemaphoreHandle_t _fileSysMutex;

    // Cached file entry
    class CachedFileEntry
    {
    public:
        CachedFileEntry()
        {
            _lastUsedMs = 0;
        }
        CachedFileEntry(const String& name, const String& fileContents)
        {
            _fileName = name;
            _fileContents = fileContents;
            _lastUsedMs = millis();
        }
        String _fileName;
        String _fileContents;
        uint32_t _lastUsedMs;
    };
    std::list<CachedFileEntry> _cachedFiles;
    uint32_t _maxCacheBytes;

private:
    bool checkFileSystem(const String& fileSystemStr, String& fsName);
    String getFilePath(const String& nameOfFS, const String& filename);
    uint32_t cachedFilesGetBytes();
    void cachedFilesDropOldest();
    void cachedFilesStoreNew(const String& fileName, const String& fileContents);
};

extern FileSystem fileSystem;
