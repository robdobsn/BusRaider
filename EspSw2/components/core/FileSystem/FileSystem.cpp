/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FileSystem 
// Handles SPIFFS/LittleFS and SD card file access
//
// Rob Dobson 2018-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <sys/stat.h>
#include <esp_spiffs.h>
#include <esp_vfs.h>
#include <esp_vfs_fat.h>
#include <esp_err.h>
#include <driver/sdmmc_host.h>
#include <driver/sdmmc_defs.h>
#include <driver/sdspi_host.h>
#include <FileSystem.h>
#include <RdJson.h>
#include <Utils.h>
#include <ConfigPinMap.h>
#include <Logger.h>
#include <SpiramAwareAllocator.h>
#include "esp_littlefs.h"

static const char* MODULE_PREFIX = "FileSystem";

FileSystem fileSystem;

// Warn
#define WARN_ON_FILE_NOT_FOUND
#define WARN_ON_FILE_SYSTEM_ERRORS
#define WARN_ON_INVALID_FILE_SYSTEM
#define WARN_ON_FILE_TOO_BIG
#define WARN_ON_GET_CONTENTS_IS_FOLDER

// Debug
// #define DEBUG_FILE_UPLOAD
// #define DEBUG_GET_FILE_CONTENTS
// #define DEBUG_FILE_EXISTS_PERFORMANCE
// #define DEBUG_FILE_SYSTEM_MOUNT
// #define DEBUG_CACHE_FS_INFO

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FileSystem::FileSystem()
{
    // Vars
    _localFSIsLittleFS = false;
    _cacheFileSystemInfo = false;
    _defaultToSDIfAvailable = true;
    _pSDCard = NULL;
    _fileSysMutex = xSemaphoreCreateMutex();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileSystem::setup(bool enableSPIFFS, bool enableLittleFS, bool localFsFormatIfCorrupt, bool enableSD, 
        int sdMOSIPin, int sdMISOPin, int sdCLKPin, int sdCSPin, bool defaultToSDIfAvailable,
        bool cacheFileSystemInfo)
{
    // Init
    _localFsCache.isUsed = false;
    _localFsCache.fsName.clear();
    _sdFsCache.isUsed = false;
    _cacheFileSystemInfo = cacheFileSystemInfo;
    _defaultToSDIfAvailable = defaultToSDIfAvailable;

    // Setup local file system
    localFileSystemSetup(enableSPIFFS, enableLittleFS, localFsFormatIfCorrupt);

    // Setup SD file system
    sdFileSystemSetup(enableSD, sdMOSIPin, sdMISOPin, sdCLKPin, sdCSPin);

    // Service a few times to setup caches
    for (uint32_t i = 0; i < SERVICE_COUNT_FOR_CACHE_PRIMING; i++)
        service();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileSystem::service()
{
    if (!_cacheFileSystemInfo)
        return;
    fileSystemCacheService(_localFsCache);
    fileSystemCacheService(_sdFsCache);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Reformat
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileSystem::reformat(const String& fileSystemStr, String& respStr)
{
    // Check file system supported
    String nameOfFS;
    if (!checkFileSystem(fileSystemStr, nameOfFS))
    {
        Utils::setJsonErrorResult("reformat", respStr, "invalidfs");
        return;
    }
    
    // Check FS name
    if (!nameOfFS.equalsIgnoreCase(LOCAL_FILE_SYSTEM_NAME))
    {
        LOG_W(MODULE_PREFIX, "Can only format local file system");
        return;
    }

    // TODO - check WDT maybe enabled
    // Reformat - need to disable Watchdog timer while formatting
    // Watchdog is not enabled on core 1 in Arduino according to this
    // https://www.bountychannel.com/issues/44690700-watchdog-with-system-reset
    // disableCore0WDT();
    _localFsCache.isSizeInfoValid = false;
    _localFsCache.isFileInfoValid = false;
    _localFsCache.isFileInfoSetup = false;
    esp_err_t ret = ESP_FAIL;
    if (_localFSIsLittleFS)
        ret = esp_littlefs_format(NULL);
    else
        ret = esp_spiffs_format(NULL);
    // enableCore0WDT();
    Utils::setJsonBoolResult("reformat", respStr, ret == ESP_OK);
    LOG_W(MODULE_PREFIX, "Reformat result %s", (ret == ESP_OK ? "OK" : "FAIL"));
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get path of root of default file system
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String FileSystem::getDefaultFSRoot() const
{
    // Default file system root path
    if (_sdFsCache.isUsed)
    {
        if (_defaultToSDIfAvailable)
            return SD_FILE_SYSTEM_NAME;
        if (_localFsCache.fsName.length() == 0)
            return SD_FILE_SYSTEM_NAME;
    }
    return _localFsCache.fsName.c_str();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get file info
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool FileSystem::getFileInfo(const String& fileSystemStr, const String& filename, uint32_t& fileLength)
{
    String nameOfFS;
    if (!checkFileSystem(fileSystemStr, nameOfFS)) {
        LOG_W(MODULE_PREFIX, "getFileInfo %s invalid file system %s", filename.c_str(), fileSystemStr.c_str());
        return false;
    }

    // Take mutex
    if (xSemaphoreTake(_fileSysMutex, portMAX_DELAY) != pdTRUE)
        return false;

    // Check file exists
    struct stat st;
    String rootFilename = getFilePath(nameOfFS, filename);

    if (stat(rootFilename.c_str(), &st) != 0)
    {
        xSemaphoreGive(_fileSysMutex);
#ifdef WARN_ON_FILE_NOT_FOUND
        LOG_W(MODULE_PREFIX, "getFileInfo %s cannot stat", rootFilename.c_str());
#endif
        return false;
    }
    if (!S_ISREG(st.st_mode))
    {
        xSemaphoreGive(_fileSysMutex);
#ifdef WARN_ON_FILE_SYSTEM_ERRORS
        LOG_W(MODULE_PREFIX, "getFileInfo %s is a folder", rootFilename.c_str());
#endif
        return false;
    }
    fileLength = st.st_size;
    xSemaphoreGive(_fileSysMutex);
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get file list JSON
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool FileSystem::getFilesJSON(const char* req, const String& fileSystemStr, const String& folderStr, String& respStr)
{
    // Check file system supported
    String nameOfFS;
    if (!checkFileSystem(fileSystemStr, nameOfFS))
    {
        LOG_W(MODULE_PREFIX, "getFilesJSON unknownFS %s", fileSystemStr.c_str());
        Utils::setJsonErrorResult(req, respStr, "unknownfs");
        return false;
    }

    // Check if cached information can be used
    CachedFileSystem& cachedFs = nameOfFS.equalsIgnoreCase(LOCAL_FILE_SYSTEM_NAME) ? _localFsCache : _sdFsCache;
    if (_cacheFileSystemInfo && ((folderStr.length() == 0) || (folderStr.equalsIgnoreCase("/"))))
    {
        LOG_I(MODULE_PREFIX, "getFilesJSON using cached info");
        return fileInfoCacheToJSON(req, cachedFs, "/", respStr);
    }

    // Generate info immediately
    return fileInfoGenImmediate(req, cachedFs, folderStr, respStr);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get file contents
// If non-null pointer returned then it must be freed by caller
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

char* FileSystem::getFileContents(const String& fileSystemStr, const String& filename, 
                int maxLen)
{
    // Check file system supported
    String nameOfFS;
    if (!checkFileSystem(fileSystemStr, nameOfFS))
    {
#ifdef WARN_ON_INVALID_FILE_SYSTEM
        LOG_W(MODULE_PREFIX, "getContents %s invalid file system %s", filename.c_str(), fileSystemStr.c_str());
#endif
        return nullptr;
    }

    // Filename
    String rootFilename = getFilePath(nameOfFS, filename);

    // Take mutex
    if (xSemaphoreTake(_fileSysMutex, portMAX_DELAY) != pdTRUE)
        return nullptr;

    // Get file info - to check length
    struct stat st;
    if (stat(rootFilename.c_str(), &st) != 0)
    {
        xSemaphoreGive(_fileSysMutex);
#ifdef WARN_ON_FILE_NOT_FOUND
        LOG_W(MODULE_PREFIX, "getContents %s cannot stat", rootFilename.c_str());
#endif
        return nullptr;
    }
    if (!S_ISREG(st.st_mode))
    {
        xSemaphoreGive(_fileSysMutex);
#ifdef WARN_ON_GET_CONTENTS_IS_FOLDER
        LOG_I(MODULE_PREFIX, "getContents %s is a folder", rootFilename.c_str());
#endif
        return nullptr;
    }

    // Check valid
    if (maxLen <= 0)
    {
        maxLen = SpiramAwareAllocator<char>::max_allocatable() / 3;
    }
    if (st.st_size >= maxLen-1)
    {
        xSemaphoreGive(_fileSysMutex);
#ifdef WARN_ON_FILE_TOO_BIG
        LOG_W(MODULE_PREFIX, "getContents %s free heap %d size %d too big to read", rootFilename.c_str(), maxLen, (int)st.st_size);
#endif
        return nullptr;
    }
    int fileSize = st.st_size;

    // Open file
    FILE* pFile = fopen(rootFilename.c_str(), "rb");
    if (!pFile)
    {
        xSemaphoreGive(_fileSysMutex);
#ifdef WARN_ON_FILE_NOT_FOUND
        LOG_W(MODULE_PREFIX, "getContents failed to open file to read %s", rootFilename.c_str());
#endif
        return nullptr;
    }

#ifdef DEBUG_GET_FILE_CONTENTS
    uint32_t intMemPreAlloc = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#endif

    // Buffer
    SpiramAwareAllocator<char> allocator;
    char* pBuf = allocator.allocate(fileSize+1);
    if (!pBuf)
    {
        fclose(pFile);
        xSemaphoreGive(_fileSysMutex);
#ifdef WARN_ON_FILE_TOO_BIG
        LOG_W(MODULE_PREFIX, "getContents failed to allocate %d", fileSize);
#endif
        return nullptr;
    }

    // Read
    size_t bytesRead = fread(pBuf, 1, fileSize, pFile);
    fclose(pFile);
    xSemaphoreGive(_fileSysMutex);
    pBuf[bytesRead] = 0;

#ifdef DEBUG_GET_FILE_CONTENTS
    LOG_I(MODULE_PREFIX, "getContents preReadIntHeap %d postReadIntHeap %d fileSize %d filename %s", intMemPreAlloc, 
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                fileSize, rootFilename.c_str());
#endif

    // Return buffer - must be freed by caller
    return pBuf;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Set file contents
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool FileSystem::setFileContents(const String& fileSystemStr, const String& filename, String& fileContents)
{
    // Check file system supported
    String nameOfFS;
    if (!checkFileSystem(fileSystemStr, nameOfFS))
    {
        return false;
    }

    // Take mutex
    if (xSemaphoreTake(_fileSysMutex, portMAX_DELAY) != pdTRUE)
        return false;

    // Open file for writing
    String rootFilename = getFilePath(nameOfFS, filename);
    FILE* pFile = fopen(rootFilename.c_str(), "wb");
    if (!pFile)
    {
        xSemaphoreGive(_fileSysMutex);
#ifdef WARN_ON_FILE_SYSTEM_ERRORS
        LOG_W(MODULE_PREFIX, "setContents failed to open file to write %s", rootFilename.c_str());
#endif
        return "";
    }

    // Write
    size_t bytesWritten = fwrite((uint8_t*)(fileContents.c_str()), 1, fileContents.length(), pFile);
    fclose(pFile);

    // Clean up
#ifdef DEBUG_CACHE_FS_INFO
    LOG_I(MODULE_PREFIX, "setFileContents cache invalid");
#endif
    markFileCacheDirty(nameOfFS, filename);
    xSemaphoreGive(_fileSysMutex);
    return bytesWritten == fileContents.length();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Delete
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool FileSystem::deleteFile(const String& fileSystemStr, const String& filename)
{
    // Check file system supported
    String nameOfFS;
    if (!checkFileSystem(fileSystemStr, nameOfFS))
    {
        return false;
    }
    
    // Take mutex
    if (xSemaphoreTake(_fileSysMutex, portMAX_DELAY) != pdTRUE)
        return false;

    // Remove file
    struct stat st;
    String rootFilename = getFilePath(nameOfFS, filename);
    if (stat(rootFilename.c_str(), &st) == 0) 
    {
        unlink(rootFilename.c_str());
    }

#ifdef DEBUG_CACHE_FS_INFO
    LOG_I(MODULE_PREFIX, "deleteFile cache invalid");
#endif
    markFileCacheDirty(nameOfFS, filename);
    xSemaphoreGive(_fileSysMutex);   
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Read line
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

char* FileSystem::readLineFromFile(char* pBuf, int maxLen, FILE* pFile)
{
    // Iterate over chars
    pBuf[0] = 0;
    char* pCurPtr = pBuf;
    int curLen = 0;
    while (true)
    {
        if (curLen >= maxLen-1)
            break;
        int ch = fgetc(pFile);
        if (ch == EOF)
        {
            if (curLen != 0)
                break;
            return NULL;
        }
        if (ch == '\n')
            break;
        if (ch == '\r')
            continue;
        *pCurPtr++ = ch;
        *pCurPtr = 0;
        curLen++;
    }
    return pBuf;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get file name extension
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String FileSystem::getFileExtension(const String& fileName)
{
    String extn;
    // Find last .
    int dotPos = fileName.lastIndexOf('.');
    if (dotPos < 0)
        return extn;
    // Return substring
    return fileName.substring(dotPos+1);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get file system and check ok
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool FileSystem::checkFileSystem(const String& fileSystemStr, String& fsName) const
{
    // Check file system
    fsName = fileSystemStr;
    fsName.trim();
    fsName.toLowerCase();

    // Check alternate name
    if (fsName.equalsIgnoreCase(LOCAL_FILE_SYSTEM_ALT_NAME))
        fsName = LOCAL_FILE_SYSTEM_NAME;

    // Use default file system if not specified
    if (fsName.length() == 0)
        fsName = getDefaultFSRoot();

    // Handle local file system
    if (fsName == LOCAL_FILE_SYSTEM_NAME)
        return _localFsCache.fsName.length() > 0;

    // SD file system
    if (fsName == SD_FILE_SYSTEM_NAME)
    {
        if (!_sdFsCache.isUsed)
            return false;
        return true;
    }

    // Unknown FS
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get file path
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String FileSystem::getFilePath(const String& nameOfFS, const String& filename) const
{
    // Check if filename already contains file system
    if ((filename.indexOf(LOCAL_FILE_SYSTEM_PATH_ELEMENT) >= 0) || (filename.indexOf(SD_FILE_SYSTEM_PATH_ELEMENT) >= 0))
        return (filename.startsWith("/") ? filename : ("/" + filename));
    return (filename.startsWith("/") ? "/" + nameOfFS + filename : ("/" + nameOfFS + "/" + filename));
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get file path with fs check
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool FileSystem::getFileFullPath(const String& filename, String& fileFullPath) const
{
    // Extract filename elements
    String modFileName = filename;
    String fsName;
    modFileName.trim();
    int firstSlashPos = modFileName.indexOf('/');
    if (firstSlashPos > 0)
    {
        fsName = modFileName.substring(0, firstSlashPos);
        modFileName = modFileName.substring(firstSlashPos+1);
    }

    // Check the file system is valid
    String nameOfFS;
    if (!checkFileSystem(fsName, nameOfFS)) {
        LOG_W(MODULE_PREFIX, "getFileFullPath %s invalid file system %s", 
                        filename.c_str(), fsName.c_str());
        fileFullPath = filename;
        return false;
    }

    // Form the full path
    fileFullPath = getFilePath(nameOfFS, modFileName);
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Exists - check if file exists
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool FileSystem::exists(const char* path) const
{
    // Take mutex
#ifdef DEBUG_FILE_EXISTS_PERFORMANCE
    uint64_t st1 = micros();
#endif
    if (xSemaphoreTake(_fileSysMutex, portMAX_DELAY) != pdTRUE)
        return false;
#ifdef DEBUG_FILE_EXISTS_PERFORMANCE
    uint64_t st2 = micros();
#endif
    struct stat buffer;   
    bool rslt = (stat(path, &buffer) == 0);
#ifdef DEBUG_FILE_EXISTS_PERFORMANCE
    uint64_t st3 = micros();
#endif
    xSemaphoreGive(_fileSysMutex);
#ifdef DEBUG_FILE_EXISTS_PERFORMANCE
    uint64_t st4 = micros();
    LOG_I(MODULE_PREFIX, "exists 1:%lld 2:%lld 3:%lld", st2-st1, st3-st2, st4-st3);
#endif
    return rslt;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Path type - folder/file/non-existent
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FileSystem::FileSystemStatType FileSystem::pathType(const char* filename)
{
    // Take mutex
    if (xSemaphoreTake(_fileSysMutex, portMAX_DELAY) != pdTRUE)
        return FILE_SYSTEM_STAT_NO_EXIST;
    struct stat buffer;
    bool rslt = stat(filename, &buffer);
    xSemaphoreGive(_fileSysMutex);
    if (rslt != 0)
        return FILE_SYSTEM_STAT_NO_EXIST;
    if (S_ISREG(buffer.st_mode))
        return FILE_SYSTEM_STAT_FILE;
    return FILE_SYSTEM_STAT_DIR;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get a section of a file
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool FileSystem::getFileSection(const String& fileSystemStr, const String& filename, uint32_t sectionStart, uint8_t* pBuf, 
            uint32_t sectionLen, uint32_t& readLen)
{
    // Check file system supported
    String nameOfFS;
    if (!checkFileSystem(fileSystemStr, nameOfFS))
    {
        LOG_W(MODULE_PREFIX, "getFileSection %s invalid file system %s", filename.c_str(), fileSystemStr.c_str());
        return false;
    }

    // Take mutex
    if (xSemaphoreTake(_fileSysMutex, portMAX_DELAY) != pdTRUE)
        return false;

    // Open file
    String rootFilename = getFilePath(nameOfFS, filename);
    FILE* pFile = fopen(rootFilename.c_str(), "rb");
    if (!pFile)
    {
        xSemaphoreGive(_fileSysMutex);
        LOG_W(MODULE_PREFIX, "getFileSection failed to open file to read %s", rootFilename.c_str());
        return false;
    }

    // Move to appropriate place in file
    fseek(pFile, sectionStart, SEEK_SET);

    // Read
    readLen = fread((char*)pBuf, 1, sectionLen, pFile);
    fclose(pFile);
    xSemaphoreGive(_fileSysMutex);
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get a line from a text file
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool FileSystem::getFileLine(const String& fileSystemStr, const String& filename, uint32_t startFilePos, uint8_t* pBuf, 
            uint32_t lineMaxLen, uint32_t& fileCurPos)
{
    // Check file system supported
    String nameOfFS;
    if (!checkFileSystem(fileSystemStr, nameOfFS))
    {
        LOG_W(MODULE_PREFIX, "getFileLine %s invalid file system %s", filename.c_str(), fileSystemStr.c_str());
        return false;
    }

    // Take mutex
    if (xSemaphoreTake(_fileSysMutex, portMAX_DELAY) != pdTRUE)
        return false;

    // Open file for text reading
    String rootFilename = getFilePath(nameOfFS, filename);
    FILE* pFile = fopen(rootFilename.c_str(), "r");
    if (!pFile)
    {
        xSemaphoreGive(_fileSysMutex);
        LOG_W(MODULE_PREFIX, "getFileLine failed to open file to read %s", rootFilename.c_str());
        return false;
    }

    // Move to appropriate place in file
    fseek(pFile, startFilePos, SEEK_SET);

    // Read line
    char* pReadLine = readLineFromFile((char*)pBuf, lineMaxLen-1, pFile);

    // Record final
    fileCurPos = ftell(pFile);

    // Close
    fclose(pFile);
    xSemaphoreGive(_fileSysMutex);

    // Ok if we got something
    return pReadLine != NULL;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Open file
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FILE* FileSystem::fileOpen(const String& fileSystemStr, const String& filename, 
                    bool writeMode, uint32_t seekToPos)
{
    // Check file system supported
    String nameOfFS;
    if (!checkFileSystem(fileSystemStr, nameOfFS))
    {
        LOG_W(MODULE_PREFIX, "fileOpen %s invalid file system %s", filename.c_str(), fileSystemStr.c_str());
        return nullptr;
    }

    // Take mutex
    if (xSemaphoreTake(_fileSysMutex, portMAX_DELAY) != pdTRUE)
        return nullptr;

    // Open file
    String rootFilename = getFilePath(nameOfFS, filename);
    FILE* pFile = fopen(rootFilename.c_str(), writeMode ? "wb" : "rb");
    if (pFile && (seekToPos != 0))
        fseek(pFile, seekToPos, SEEK_SET);

    // Check writing (invalidates cache)
    if (writeMode)
    {
        markFileCacheDirty(nameOfFS, filename);
    }

    // Release mutex
    xSemaphoreGive(_fileSysMutex);

    // Check result
    if (!pFile)
    {
        LOG_W(MODULE_PREFIX, "getFileSection failed to open file to read %s", rootFilename.c_str());
        return nullptr;
    }

    if (writeMode)
    {
#ifdef DEBUG_CACHE_FS_INFO
        LOG_I(MODULE_PREFIX, "fileOpen cache invalid");
#endif
    }

    // Return file 
    return pFile;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Close file
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool FileSystem::fileClose(FILE* pFile)
{
    // Take mutex
    if (xSemaphoreTake(_fileSysMutex, portMAX_DELAY) != pdTRUE)
        return false;

    // Close file
    fclose(pFile);

    // Release mutex
    xSemaphoreGive(_fileSysMutex);
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Read from file
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t FileSystem::fileRead(FILE* pFile, uint8_t* pBuf, uint32_t readLen)
{
    // Ensure valid
    if (!pFile)
    {
        LOG_W(MODULE_PREFIX, "fileRead filePtr null");
        return 0;
    }

    // Take mutex
    if (xSemaphoreTake(_fileSysMutex, portMAX_DELAY) != pdTRUE)
        return 0;

    // Read
    uint32_t lenRead = fread((char*)pBuf, 1, readLen, pFile);

    // Release mutex
    xSemaphoreGive(_fileSysMutex);
    return lenRead;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Write to file
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t FileSystem::fileWrite(FILE* pFile, const uint8_t* pBuf, uint32_t writeLen)
{
    // Ensure valid
    if (!pFile)
    {
        LOG_W(MODULE_PREFIX, "fileWrite filePtr null");
        return 0;
    }

    // Take mutex
    if (xSemaphoreTake(_fileSysMutex, portMAX_DELAY) != pdTRUE)
        return 0;

    // Read
    uint32_t lenWritten = fwrite((char*)pBuf, 1, writeLen, pFile);

    // Release mutex
    xSemaphoreGive(_fileSysMutex);
    return lenWritten;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get temporary file name
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String getTempFileName()
{
    return "__temp__";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get file position
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t FileSystem::filePos(FILE* pFile)
{
    // Ensure valid
    if (!pFile)
    {
        LOG_W(MODULE_PREFIX, "filePos filePtr null");
        return 0;
    }

    // Take mutex
    if (xSemaphoreTake(_fileSysMutex, portMAX_DELAY) != pdTRUE)
        return 0;

    // Read
    uint32_t filePosition = ftell(pFile);

    // Release mutex
    xSemaphoreGive(_fileSysMutex);
    return filePosition;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup local file system
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileSystem::localFileSystemSetup(bool enableSPIFFS, bool enableLittleFS, bool formatIfCorrupt)
{
    // Check enabled
    if (!enableSPIFFS && !enableLittleFS)
    {
        LOG_I(MODULE_PREFIX, "localFileSystemSetup local file system disabled");
        return;
    }

    // Check if SPIFFS if enabled
    if (enableSPIFFS)
    {
        // Format SPIFFS file system if required - this will fail if the partition is not
        // in SPIFFS format and formatting will only be done if enabled and LittleFS is not
        // enabled
        if (localFileSystemSetupSPIFFS(!enableLittleFS && formatIfCorrupt))
            return;
    }

    // Init LittleFS if enabled
    if (enableLittleFS)
    {
        // Init LittleFS file system (format if required)
        if (localFileSystemSetupLittleFS(formatIfCorrupt))
            return;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup local file system
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool FileSystem::localFileSystemSetupLittleFS(bool formatIfCorrupt)
{
    // Using ESP IDF virtual file system
    esp_vfs_littlefs_conf_t conf = {
        .base_path = LOCAL_FILE_SYSTEM_BASE_PATH,
        .partition_label = LOCAL_FILE_SYSTEM_PARTITION_LABEL,
        .format_if_mount_failed = formatIfCorrupt,
        .dont_mount = false,
    };        
    // Use settings defined above to initialize and mount LittleFS filesystem.
    // Note: esp_vfs_littlefs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK)
    {
#ifdef DEBUG_FILE_SYSTEM_MOUNT
        if (ret == ESP_FAIL) 
        {
            LOG_I(MODULE_PREFIX, "setup failed mount/format LittleFS");
        } 
        else if (ret == ESP_ERR_NOT_FOUND) 
        {
            LOG_I(MODULE_PREFIX, "setup failed to find LittleFS partition");
        } 
        else 
        {
            LOG_I(MODULE_PREFIX, "setup failed to init LittleFS (error %s)", esp_err_to_name(ret));
        }
#endif
        return false;
    }

    // Get file system info
    size_t total = 0, used = 0;
    ret = esp_littlefs_info(LOCAL_FILE_SYSTEM_PARTITION_LABEL, &total, &used);
    if (ret != ESP_OK) 
    {
        LOG_W(MODULE_PREFIX, "setup failed to get LittleFS info (error %s)", esp_err_to_name(ret));
        return false;
    } 

    // Done ok
    LOG_I(MODULE_PREFIX, "setup LittleFS partition size total %d, used %d", total, used);
    
    // Local file system is ok
    _localFSIsLittleFS = true;
    _localFsCache.isUsed = true;
    _localFsCache.isFileInfoValid = false;
    _localFsCache.isSizeInfoValid = false;
    _localFsCache.isFileInfoSetup = false;
    _localFsCache.fsName = LOCAL_FILE_SYSTEM_NAME;
    _localFsCache.fsBase = LOCAL_FILE_SYSTEM_BASE_PATH;
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup local file system
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool FileSystem::localFileSystemSetupSPIFFS(bool formatIfCorrupt)
{
    // Using ESP IDF virtual file system
    esp_vfs_spiffs_conf_t conf = {
        .base_path = LOCAL_FILE_SYSTEM_BASE_PATH,
        .partition_label = LOCAL_FILE_SYSTEM_PARTITION_LABEL,
        .max_files = 5,
        .format_if_mount_failed = formatIfCorrupt
    };        
    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK)
    {
#ifdef DEBUG_FILE_SYSTEM_MOUNT
        if (ret == ESP_FAIL) 
        {
            LOG_I(MODULE_PREFIX, "setup failed mount/format SPIFFS");
        } 
        else if (ret == ESP_ERR_NOT_FOUND) 
        {
            LOG_I(MODULE_PREFIX, "setup failed to find SPIFFS partition");
        } 
        else 
        {
            LOG_I(MODULE_PREFIX, "setup failed to init SPIFFS (error %s)", esp_err_to_name(ret));
        }
#endif
        return false;
    }

    // Get file system info
    LOG_I(MODULE_PREFIX, "setup SPIFFS initialised ok");
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(LOCAL_FILE_SYSTEM_PARTITION_LABEL, &total, &used);
    if (ret != ESP_OK)
    {
        LOG_W(MODULE_PREFIX, "setup failed to get SPIFFS info (error %s)", esp_err_to_name(ret));
        return false;
    } 

    // Done ok
    LOG_I(MODULE_PREFIX, "setup SPIFFS partition size total %d, used %d", total, used);

    // Local file system is ok
    _localFSIsLittleFS = false;
    _localFsCache.isUsed = true;
    _localFsCache.isFileInfoValid = false;
    _localFsCache.isSizeInfoValid = false;
    _localFsCache.isFileInfoSetup = false;
    _localFsCache.fsName = LOCAL_FILE_SYSTEM_NAME;
    _localFsCache.fsBase = LOCAL_FILE_SYSTEM_BASE_PATH;
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup SD file system
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool FileSystem::sdFileSystemSetup(bool enableSD, int sdMOSIPin, int sdMISOPin, int sdCLKPin, int sdCSPin)
{
    if (!enableSD)
    {
        LOG_I(MODULE_PREFIX, "sdFileSystemSetup SD disabled");
        return false;
    }

    // Check valid
    if (sdMOSIPin == -1 || sdMISOPin == -1 || sdCLKPin == -1 || sdCSPin == -1)
    {
        LOG_W(MODULE_PREFIX, "sdFileSystemSetup SD pins invalid");
        return false;
    }

    // Setup SD
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    sdspi_slot_config_t slot_config = SDSPI_SLOT_CONFIG_DEFAULT();
    slot_config.gpio_miso = (gpio_num_t)sdMISOPin;
    slot_config.gpio_mosi = (gpio_num_t)sdMOSIPin;
    slot_config.gpio_sck  = (gpio_num_t)sdCLKPin;
    slot_config.gpio_cs   = (gpio_num_t)sdCSPin;
    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and formatted
    // in case when mounting fails.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5
    };
#pragma GCC diagnostic pop

    sdmmc_card_t* pCard;
    esp_err_t ret = esp_vfs_fat_sdmmc_mount(SD_FILE_SYSTEM_BASE_PATH, &host, &slot_config, &mount_config, &pCard);
    if (ret != ESP_OK) 
    {
        if (ret == ESP_FAIL) 
        {
            LOG_W(MODULE_PREFIX, "sdFileSystemSetup failed mount SD");
        } 
        else 
        {
            LOG_I(MODULE_PREFIX, "sdFileSystemSetup failed to init SD (error %s)", esp_err_to_name(ret));
        }
        return false;
    }

    _pSDCard = pCard;
    LOG_I(MODULE_PREFIX, "sdFileSystemSetup ok");

    // SD ok
    _sdFsCache.isUsed = true;
    _sdFsCache.isFileInfoValid = false;
    _sdFsCache.isSizeInfoValid = false;
    _sdFsCache.isFileInfoSetup = false;
    _sdFsCache.fsName = SD_FILE_SYSTEM_NAME;
    _sdFsCache.fsBase = SD_FILE_SYSTEM_BASE_PATH;

    // DEBUG SD print SD card info
    // sdmmc_card_print_info(stdout, pCard);

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Convert cached file system info to JSON
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool FileSystem::fileInfoCacheToJSON(const char* req, CachedFileSystem& cachedFs, const String& folderStr, String& respStr)
{
    // Check valid
    if (cachedFs.isSizeInfoValid && cachedFs.isFileInfoValid)
    {
        LOG_I(MODULE_PREFIX, "fileInfoCacheToJSON cached info valid");

        uint32_t debugStartMs = millis();
        String fileListStr;
        bool firstFile = true;
        uint32_t fileCount = 0;
        for (CachedFileInfo& cachedFileInfo : cachedFs.cachedRootFileList)
        {
            fileListStr += String(firstFile ? R"({"name":")" : R"(,{"name":")") + cachedFileInfo.fileName.c_str() + R"(","size":)" + String(cachedFileInfo.fileSize) + "}";
            firstFile = false;
            fileCount++;
        }
        // Format response
        respStr = formatJSONFileInfo(req, cachedFs, fileListStr, folderStr);
        uint32_t elapsedMs = millis() - debugStartMs;
        LOG_I(MODULE_PREFIX, "fileInfoCacheToJSON elapsed %dms fileCount %d", elapsedMs, fileCount);
        return true;
    }

    // Info invalid - need to wait
    LOG_I(MODULE_PREFIX, "fileInfoCacheToJSON cached info invalid - need to wait");
    Utils::setJsonErrorResult(req, respStr, "fsinfodirty");
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get JSON file info immediately
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool FileSystem::fileInfoGenImmediate(const char* req, CachedFileSystem& cachedFs, const String& folderStr, String& respStr)
{
    // Check if file-system info is valid
    if (!cachedFs.isUsed)
    {
        Utils::setJsonErrorResult(req, respStr, "fsinvalid");
        return false;
    }
    if (!cachedFs.isSizeInfoValid)
    {
        if (!fileSysInfoUpdateCache(req, cachedFs, respStr))
            return false;
    }

    // Take mutex
    if (xSemaphoreTake(_fileSysMutex, 0) != pdTRUE)
    {
        Utils::setJsonErrorResult(req, respStr, "fsbusy");
        return false;
    }

    // Debug
    uint32_t debugStartMs = millis();

    // Check file system is valid
    if (cachedFs.fsSizeBytes == 0)
    {
        xSemaphoreGive(_fileSysMutex);
        LOG_W(MODULE_PREFIX, "getFilesJSON No valid file system");
        Utils::setJsonErrorResult(req, respStr, "nofs");
        return false;
    }

    // Open directory
    String rootFolder = cachedFs.fsBase.c_str();
    if (!folderStr.startsWith("/"))
        rootFolder += "/";
    rootFolder += folderStr;
    DIR* dir = opendir(rootFolder.c_str());
    if (!dir)
    {
        xSemaphoreGive(_fileSysMutex);
        LOG_W(MODULE_PREFIX, "getFilesJSON Failed to open base folder %s", rootFolder.c_str());
        Utils::setJsonErrorResult(req, respStr, "nofolder");
        return false;
    }

    // Read directory entries
    bool firstFile = true;
    String fileListStr;
    struct dirent* ent = NULL;
    while ((ent = readdir(dir)) != NULL)
    {
        // Check for unwanted files
        String fName = ent->d_name;
        if (fName.equalsIgnoreCase("System Volume Information"))
            continue;
        if (fName.equalsIgnoreCase("thumbs.db"))
            continue;

        // Get file info including size
        size_t fileSize = 0;
        struct stat st;
        String filePath = (rootFolder.endsWith("/") ? rootFolder + fName : rootFolder + "/" + fName);
        if (stat(filePath.c_str(), &st) == 0) 
        {
            fileSize = st.st_size;
        }

        // Form the JSON list
        if (!firstFile)
            fileListStr += ",";
        firstFile = false;
        fileListStr += R"({"name":")";
        fileListStr += ent->d_name;
        fileListStr += R"(","size":)";
        fileListStr += String(fileSize);
        fileListStr += "}";
    }

    // Finished with file list
    closedir(dir);
    xSemaphoreGive(_fileSysMutex);

    // Format response
    respStr = formatJSONFileInfo(req, cachedFs, fileListStr, rootFolder);

    // Debug
    uint32_t debugGetFilesMs = millis() - debugStartMs;
    LOG_I(MODULE_PREFIX, "getFilesJSON timing fileList %dms", debugGetFilesMs);

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Update File system info cache
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool FileSystem::fileSysInfoUpdateCache(const char* req, CachedFileSystem& cachedFs, String& respStr)
{
    // Take mutex
    if (xSemaphoreTake(_fileSysMutex, 0) != pdTRUE)
    {
        Utils::setJsonErrorResult(req, respStr, "fsbusy");
        return false;
    }

    // Get size of file systems
    uint32_t debugStartMs = millis();
    String fsName = cachedFs.fsName.c_str();
    if (fsName == LOCAL_FILE_SYSTEM_NAME)
    {
        uint32_t sizeBytes = 0, usedBytes = 0;
        esp_err_t ret = ESP_FAIL;
        if (_localFSIsLittleFS)
            ret = esp_littlefs_info(LOCAL_FILE_SYSTEM_PARTITION_LABEL, &sizeBytes, &usedBytes);
        else
            ret = esp_spiffs_info(LOCAL_FILE_SYSTEM_PARTITION_LABEL, &sizeBytes, &usedBytes);
        if (ret != ESP_OK)
        {
            xSemaphoreGive(_fileSysMutex);
            LOG_W(MODULE_PREFIX, "fileSysInfoUpdateCache failed to get file system info (error %s)", esp_err_to_name(ret));
            Utils::setJsonErrorResult(req, respStr, "fsInfo");
            return false;
        }
        // FS settings
        cachedFs.fsSizeBytes = sizeBytes;
        cachedFs.fsUsedBytes = usedBytes;
        cachedFs.isSizeInfoValid = true;
    }
    else if (fsName == SD_FILE_SYSTEM_NAME)
    {
        // Get size info
        sdmmc_card_t* pCard = (sdmmc_card_t*)_pSDCard;
        if (pCard)
        {
            cachedFs.fsSizeBytes = ((double) pCard->csd.capacity) * pCard->csd.sector_size;
        	FATFS* fsinfo;
            DWORD fre_clust;
            if(f_getfree("0:",&fre_clust,&fsinfo) == 0)
            {
                cachedFs.fsUsedBytes = ((double)(fsinfo->csize))*((fsinfo->n_fatent - 2) - (fsinfo->free_clst))
            #if _MAX_SS != 512
                    *(fsinfo->ssize);
            #else
                    *512;
            #endif
            }
            cachedFs.isSizeInfoValid = true;
        }
    }
    xSemaphoreGive(_fileSysMutex);
    uint32_t debugGetFsInfoMs = millis() - debugStartMs;
    LOG_I(MODULE_PREFIX, "fileSysInfoUpdateCache timing fsInfo %dms", debugGetFsInfoMs);
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Mark file cache dirty
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileSystem::markFileCacheDirty(const String& fsName, const String& filename)
{
    // Check caching enabled
    if (!_cacheFileSystemInfo)
        return;

    // Get file system info
    CachedFileSystem& cachedFs = fsName.equalsIgnoreCase(LOCAL_FILE_SYSTEM_NAME) ? _localFsCache : _sdFsCache;

    // Check valid
    if (!cachedFs.isFileInfoSetup)
        return;

    // Find the file in the cached file list
    bool fileFound = false;
    for (CachedFileInfo& cachedFileInfo : cachedFs.cachedRootFileList)
    {
        // Check name
        if (cachedFileInfo.fileName.compare(filename.c_str()) == 0)
        {
            // Invalidate specific file
            cachedFileInfo.isValid = false;
            fileFound = true;
            break;
        }
    }

    // Check for file not found
    if (!fileFound)
    {
        CachedFileInfo newFileInfo;
        newFileInfo.fileName = filename.c_str();
        newFileInfo.fileSize = 0;
        newFileInfo.isValid = false;
        cachedFs.cachedRootFileList.push_back(newFileInfo);
    }
    cachedFs.isFileInfoValid = false;
    cachedFs.isSizeInfoValid = false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Mark file cache dirty
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileSystem::fileSystemCacheService(CachedFileSystem& cachedFs)
{
    // Check if file-system info is invalid
    if (!cachedFs.isUsed)
        return;

    // Update fsInfo if required        
    String respStr;
    if (!cachedFs.isSizeInfoValid)
    {
        fileSysInfoUpdateCache("", cachedFs, respStr);
        return;
    }

    // Update file info if required
    if (!cachedFs.isFileInfoSetup)
    {
        uint32_t debugStartMs = millis();

        // Take mutex
        if (xSemaphoreTake(_fileSysMutex, 0) != pdTRUE)
            return;

        // Clear file list
        cachedFs.cachedRootFileList.clear();

        // Iterate file info
        String rootFolder = (cachedFs.fsBase + "/").c_str();
        DIR* dir = opendir(rootFolder.c_str());
        if (!dir)
        {
            xSemaphoreGive(_fileSysMutex);
            return;
        }
        // Read directory entries
        struct dirent* ent = NULL;
        uint32_t fileCount = 0;
        while ((ent = readdir(dir)) != NULL)
        {
            // Check for unwanted files
            String fName = ent->d_name;
            if (fName.equalsIgnoreCase("System Volume Information"))
                continue;
            if (fName.equalsIgnoreCase("thumbs.db"))
                continue;

            // Get file info including size
            size_t fileSize = 0;
            struct stat st;
            String filePath = rootFolder + fName;
            if (stat(filePath.c_str(), &st) == 0) 
            {
                fileSize = st.st_size;
            }

            // Add file to list
            CachedFileInfo fileInfo;
            fileInfo.fileName = fName.c_str();
            fileInfo.fileSize = fileSize;
            fileInfo.isValid = true;
            cachedFs.cachedRootFileList.push_back(fileInfo);
            fileCount++;
        }

        // Finished with file list
        closedir(dir);
        cachedFs.isFileInfoSetup = true;
        cachedFs.isFileInfoValid = true;
        xSemaphoreGive(_fileSysMutex);
        uint32_t debugCachedFsSetupMs = millis() - debugStartMs;
        LOG_I(MODULE_PREFIX, "fileSystemCacheService fs %s files %d took %dms", 
                    cachedFs.fsName.c_str(), fileCount, debugCachedFsSetupMs);
        return;
    }

    // Check file info
    if (cachedFs.isFileInfoSetup && !cachedFs.isFileInfoValid)
    {
        // Update info for specific file(s)
        // Take mutex
        if (xSemaphoreTake(_fileSysMutex, 0) != pdTRUE)
            return;
        String rootFolder = (cachedFs.fsBase + "/").c_str();
        bool allValid = true;
        for (auto it = cachedFs.cachedRootFileList.begin(); it != cachedFs.cachedRootFileList.end(); ++it)
        {
            // Check name
            if (!(it->isValid))
            {
                // Get file info including size
                struct stat st;
                String filePath = rootFolder + it->fileName.c_str();
                if (stat(filePath.c_str(), &st) == 0) 
                {
                    it->fileSize = st.st_size;
                    LOG_I(MODULE_PREFIX, "fileSystemCacheService updated %s size %ld", 
                            it->fileName.c_str(), st.st_size);
                }
                else
                {
                    // Remove from list
                    cachedFs.cachedRootFileList.erase(it);
                    LOG_I(MODULE_PREFIX, "fileSystemCacheService deleted %s", 
                            it->fileName.c_str());
                    allValid = false;
                    break;
                }
                // File info now valid
                it->isValid = true;
                break;
            }
        }
        LOG_I(MODULE_PREFIX, "fileSystemCacheService fileInfo %s", allValid ? "valid" : "invalid");
        cachedFs.isFileInfoValid = allValid;
        xSemaphoreGive(_fileSysMutex);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Format JSON file info
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String FileSystem::formatJSONFileInfo(const char* req, CachedFileSystem& cachedFs, 
            const String& fileListStr, const String& rootFolder)
{
    // Start response JSON
    return R"({"req":")" + String(req) + R"(","rslt":"ok","fsName":")" + String(cachedFs.fsName.c_str()) + 
                R"(","fsBase":")" + String(cachedFs.fsBase.c_str()) + 
                R"(","diskSize":)" + String(cachedFs.fsSizeBytes) + R"(,"diskUsed":)" + String(cachedFs.fsUsedBytes) +
                R"(,"folder":")" + rootFolder + R"(","files":[)" + fileListStr + "]}";
}
