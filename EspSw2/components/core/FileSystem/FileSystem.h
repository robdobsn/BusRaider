/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FileSystem
// Handles SPIFFS/LittleFS and SD card file access
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
#include "FileStreamBlock.h"
#include "ArduinoTime.h"
#include "SpiramAwareAllocator.h"
#include <string>

class FileSystem
{
public:
    FileSystem();

    // Setup 
    void setup(bool enableSPIFFS, bool enableLittleFS, bool localFsFormatIfCorrupt, bool enableSD, 
        int sdMOSIPin, int sdMISOPin, int sdCLKPin, int sdCSPin, bool defaultToSDIfAvailable,
        bool cacheFileSystemInfo);

    // Service
    void service();

    // Reformat
    void reformat(const String& fileSystemStr, String& respStr);

    // Get path of root of default file system
    String getDefaultFSRoot() const;

    // Get a list of files on the file system as a JSON format string
    // {"rslt":"ok","diskSize":123456,"diskUsed":1234,"folder":"/","files":[{"name":"file1.txt","size":223},{"name":"file2.txt","size":234}]}
    bool getFilesJSON(const char* req, const String& fileSystemStr, const String& folderStr, String& respStr);

    // Get file contents as a string
    // If a non-null pointer is returned then it must be freed by caller
    char* getFileContents(const String& fileSystemStr, const String& filename, int maxLen=0);

    // Set file contents from string
    bool setFileContents(const String& fileSystemStr, const String& filename, String& fileContents);

    // Delete file on file system
    bool deleteFile(const String& fileSystemStr, const String& filename);
    
    // Test file exists and get info
    bool getFileInfo(const String& fileSystemStr, const String& filename, uint32_t& fileLength);

    // Get file name extension
    static String getFileExtension(const String& filename);

    // Read line from file
    char* readLineFromFile(char* pBuf, int maxLen, FILE* pFile);

    // Exists - check if file exists
    bool exists(const char* path) const;

    // Stat (details on a file)
    typedef enum {
        FILE_SYSTEM_STAT_NO_EXIST,
        FILE_SYSTEM_STAT_DIR,
        FILE_SYSTEM_STAT_FILE,
    } FileSystemStatType;

    // Get stat
    FileSystemStatType pathType(const char* filename);

    // Get a file path using default file system if necessary
    bool getFileFullPath(const String& filename, String& fileFullPath) const;

    // Get a section of a file
    bool getFileSection(const String& fileSystemStr, const String& filename, uint32_t sectionStart, uint8_t* pBuf, 
            uint32_t sectionLen, uint32_t& readLen);

    // Get a line from a text file
    bool getFileLine(const String& fileSystemStr, const String& filename, uint32_t startFilePos, uint8_t* pBuf, 
            uint32_t lineMaxLen, uint32_t& fileCurPos);

    // Open file
    FILE* fileOpen(const String& fileSystemStr, const String& filename, bool writeMode, uint32_t seekToPos);

    // Close file
    bool fileClose(FILE* pFile);

    // Read from file
    uint32_t fileRead(FILE* pFile, uint8_t* pBuf, uint32_t readLen);

    // Write to file
    uint32_t fileWrite(FILE* pFile, const uint8_t* pBuf, uint32_t writeLen);

    // Get file position
    uint32_t filePos(FILE* pFile);
    
    // Get temporary file name
    String getTempFileName();
    
private:

    // File system controls
    bool _localFSIsLittleFS;
    bool _defaultToSDIfAvailable;
    bool _sdIsOk;
    bool _cacheFileSystemInfo;

    // SD card
    void* _pSDCard;

    // Cached file info
    class CachedFileInfo
    {
    public:
        CachedFileInfo()
        {
            fileSize = 0;
            isValid = false;
        }
        std::basic_string<char, std::char_traits<char>, SpiramAwareAllocator<char>> fileName;
        uint32_t fileSize;
        bool isValid;
    };
    class CachedFileSystem
    {
    public:
        CachedFileSystem()
        {
            fsSizeBytes = 0;
            fsUsedBytes = 0;
            isSizeInfoValid = false;
            isFileInfoValid = false;
            isFileInfoSetup = false;
            isUsed = false;
        }
        std::basic_string<char, std::char_traits<char>, SpiramAwareAllocator<char>> fsName;
        std::basic_string<char, std::char_traits<char>, SpiramAwareAllocator<char>> fsBase;
        std::list<CachedFileInfo, SpiramAwareAllocator<CachedFileInfo>> cachedRootFileList;
        uint32_t fsSizeBytes;
        uint32_t fsUsedBytes;
        bool isSizeInfoValid;
        bool isFileInfoValid;
        bool isFileInfoSetup;
        bool isUsed;
    };
    CachedFileSystem _sdFsCache;
    CachedFileSystem _localFsCache;

    // Mutex controlling access to file system
    SemaphoreHandle_t _fileSysMutex;

private:
    bool checkFileSystem(const String& fileSystemStr, String& fsName) const;
    String getFilePath(const String& nameOfFS, const String& filename) const;
    void localFileSystemSetup(bool enableSPIFFS, bool enableLittleFS, bool formatIfCorrupt);
    bool localFileSystemSetupLittleFS(bool formatIfCorrupt);
    bool localFileSystemSetupSPIFFS(bool formatIfCorrupt);
    bool sdFileSystemSetup(bool enableSD, int sdMOSIPin, int sdMISOPin, int sdCLKPin, int sdCSPin);
    bool fileInfoCacheToJSON(const char* req, CachedFileSystem& cachedFs, const String& folderStr, String& respStr);
    bool fileInfoGenImmediate(const char* req, CachedFileSystem& cachedFs, const String& folderStr, String& respStr);
    bool fileSysInfoUpdateCache(const char* req, CachedFileSystem& cachedFs, String& respStr);
    void markFileCacheDirty(const String& fsName, const String& filename);
    void fileSystemCacheService(CachedFileSystem& cachedFs);
    String formatJSONFileInfo(const char* req, CachedFileSystem& cachedFs, const String& fileListStr, const String& rootFolder);
    static const uint32_t SERVICE_COUNT_FOR_CACHE_PRIMING = 10;

    // File system name
    static constexpr const char* LOCAL_FILE_SYSTEM_NAME = "local";
    static constexpr const char* LOCAL_FILE_SYSTEM_BASE_PATH = "/local";
    static constexpr const char* LOCAL_FILE_SYSTEM_PATH_ELEMENT = "local/";
    static constexpr const char* LOCAL_FILE_SYSTEM_ALT_NAME = "spiffs";
    static constexpr const char* SD_FILE_SYSTEM_NAME = "sd";
    static constexpr const char* SD_FILE_SYSTEM_BASE_PATH = "/sd";
    static constexpr const char* SD_FILE_SYSTEM_PATH_ELEMENT = "sd/";
    static constexpr const char* LOCAL_FILE_SYSTEM_PARTITION_LABEL = "spiffs";
};

extern FileSystem fileSystem;
