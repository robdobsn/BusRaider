/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FileSystem 
// Handles SPIFFS and SD card file access
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

static const char* MODULE_PREFIX = "FileSystem";

FileSystem fileSystem;

// #define DEBUG_FILE_UPLOAD 1

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FileSystem::FileSystem()
{
    // Vars    
    _enableSPIFFS = false;
    _spiffsIsOk = false;
    _enableSD = false;
    _sdIsOk = false;
    _cacheFileList = false;
    _cachedFileListValid = false;
    _defaultToSDIfAvailable = true;
    _pSDCard = NULL;
    _fileSysMutex = xSemaphoreCreateMutex();
    _maxCacheBytes = 5000;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileSystem::setup(bool enableSPIFFS, bool spiffsFormatIfCorrupt, bool enableSD, 
        int sdMOSIPin, int sdMISOPin, int sdCLKPin, int sdCSPin, bool defaultToSDIfAvailable,
        bool cacheFileList, int maxCacheBytes)
{
    // Init
    _spiffsIsOk = false;
    _sdIsOk = false;
    _cacheFileList = cacheFileList;
    _cachedFileListValid = false;
    _defaultToSDIfAvailable = defaultToSDIfAvailable;
    _maxCacheBytes = maxCacheBytes;

    // See if SPIFFS enabled
    _enableSPIFFS = enableSPIFFS;

    // Init SPIFFS if required
    if (_enableSPIFFS)
    {
        // Using ESP32 native SPIFFS support rather than arduino as potential bugs encountered in some
        // arduino functions
        esp_vfs_spiffs_conf_t conf = {
            .base_path = "/spiffs",
            .partition_label = NULL,
            .max_files = 5,
            .format_if_mount_failed = spiffsFormatIfCorrupt
        };        
        // Use settings defined above to initialize and mount SPIFFS filesystem.
        // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
        esp_err_t ret = esp_vfs_spiffs_register(&conf);
        if (ret != ESP_OK)
        {
            if (ret == ESP_FAIL)
                LOG_W(MODULE_PREFIX, "setup failed mount/format SPIFFS");
            else if (ret == ESP_ERR_NOT_FOUND)
                LOG_W(MODULE_PREFIX, "setup failed to find SPIFFS partition");
            else
                LOG_W(MODULE_PREFIX, "setup failed to init SPIFFS (error %s)", esp_err_to_name(ret));
        }
        else
        {
            // Get SPIFFS info
            size_t total = 0, used = 0;
            esp_err_t ret = esp_spiffs_info(NULL, &total, &used);
            if (ret != ESP_OK)
                LOG_W(MODULE_PREFIX, "setup failed to get SPIFFS info (error %s)", esp_err_to_name(ret));
            else
                LOG_I(MODULE_PREFIX, "setup SPIFFS partition size total %d, used %d", total, used);

            // SPIFFS ok
            _spiffsIsOk = true;
        }
    }
    else
    {
        LOG_I(MODULE_PREFIX, "SPIFFS disabled");
    }

    // See if SD enabled
    _enableSD = enableSD;

    // Init SD if enabled
    if (_enableSD)
    {
        // Check valid
        if (sdMOSIPin == -1 || sdMISOPin == -1 || sdCLKPin == -1 || sdCSPin == -1)
        {
            LOG_W(MODULE_PREFIX, "setup SD pins invalid");
        }
        else
        {
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
            esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sd", &host, &slot_config, &mount_config, &pCard);
            if (ret != ESP_OK) {
                if (ret == ESP_FAIL)
                    LOG_W(MODULE_PREFIX, "setup failed mount SD");
                else
                    LOG_I(MODULE_PREFIX, "setup failed to init SD (error %s)", esp_err_to_name(ret));
            }
            else
            {
                _pSDCard = pCard;
                LOG_I(MODULE_PREFIX, "setup SD ok");
                // SD ok
                _sdIsOk = true;
            }

            // DEBUG SD print SD card info
            // sdmmc_card_print_info(stdout, pCard);
        }
    }
    else
    {
        LOG_I(MODULE_PREFIX, "SD disabled");
    }
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
    
    // TODO - check WDT maybe enabled
    // Reformat - need to disable Watchdog timer while formatting
    // Watchdog is not enabled on core 1 in Arduino according to this
    // https://www.bountychannel.com/issues/44690700-watchdog-with-system-reset
    _cachedFileListValid = false;
    // disableCore0WDT();
    esp_err_t ret = esp_spiffs_format(NULL);
    // enableCore0WDT();
    Utils::setJsonBoolResult("reformat", respStr, ret == ESP_OK);
    LOG_W(MODULE_PREFIX, "Reformat SPIFFS result %s", (ret == ESP_OK ? "OK" : "FAIL"));
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get path of root of default file system
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String FileSystem::getDefaultFSRoot()
{
    // Default file system root path
    if (_sdIsOk && ((!_spiffsIsOk) || (!_defaultToSDIfAvailable)))
        return "sd";
    return "spiffs";
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
    xSemaphoreTake(_fileSysMutex, portMAX_DELAY);

    // Check file exists
    struct stat st;
    String rootFilename = getFilePath(nameOfFS, filename);

    if (stat(rootFilename.c_str(), &st) != 0)
    {
        xSemaphoreGive(_fileSysMutex);
        LOG_D(MODULE_PREFIX, "getFileInfo %s cannot stat", rootFilename.c_str());
        return false;
    }
    if (!S_ISREG(st.st_mode))
    {
        xSemaphoreGive(_fileSysMutex);
        LOG_D(MODULE_PREFIX, "getFileInfo %s is a folder", rootFilename.c_str());
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
    // Standard request name to add to response JSON
    String reqStr = req;

    // Check file system supported
    String nameOfFS;
    if (!checkFileSystem(fileSystemStr, nameOfFS))
    {
        LOG_W(MODULE_PREFIX, "getFilesJSON unknownFS %s", fileSystemStr.c_str());
        Utils::setJsonErrorResult(reqStr.c_str(), respStr, "unknownfs");
        return false;
    }

    // Check if cached version can be used
    if ((_cachedFileListValid) && (_cachedFileListResponse.length() != 0))
    {
        respStr = _cachedFileListResponse;
        return true;
        
    }
    // Take mutex
    if (xSemaphoreTake(_fileSysMutex, 0) != pdTRUE)
    {
        Utils::setJsonErrorResult(reqStr.c_str(), respStr, "fsbusy");
        return false;
    }

    // Get size of file systems
    String baseFolderForFS;
    double fsSizeBytes = 0, fsUsedBytes = 0;
    if (nameOfFS == "spiffs")
    {
        uint32_t sizeBytes = 0, usedBytes = 0;
        esp_err_t ret = esp_spiffs_info(NULL, &sizeBytes, &usedBytes);
        if (ret != ESP_OK)
        {
            xSemaphoreGive(_fileSysMutex);
            LOG_W(MODULE_PREFIX, "getFilesJSON Failed to get SPIFFS info (error %s)", esp_err_to_name(ret));
            Utils::setJsonErrorResult(reqStr.c_str(), respStr, "spiffsInfo");
            return false;
        }
        // FS settings
        fsSizeBytes = sizeBytes;
        fsUsedBytes = usedBytes;
        nameOfFS = "spiffs";
        baseFolderForFS = "/spiffs";
    }
    else if (nameOfFS == "sd")
    {
        // Get size info
        sdmmc_card_t* pCard = (sdmmc_card_t*)_pSDCard;
        if (pCard)
        {
            fsSizeBytes = ((double) pCard->csd.capacity) * pCard->csd.sector_size;
        	FATFS* fsinfo;
            DWORD fre_clust;
            if(f_getfree("0:",&fre_clust,&fsinfo) == 0)
            {
                fsUsedBytes = ((double)(fsinfo->csize))*((fsinfo->n_fatent - 2) - (fsinfo->free_clst))
            #if _MAX_SS != 512
                    *(fsinfo->ssize);
            #else
                    *512;
            #endif
            }
        }
        // Set FS info
        nameOfFS = "sd";
        baseFolderForFS = "/sd";
    }

    // Check file system is valid
    if (fsSizeBytes == 0)
    {
        LOG_W(MODULE_PREFIX, "getFilesJSON No valid file system");
        Utils::setJsonErrorResult(reqStr.c_str(), respStr, "nofs");
        return false;
    }

    // Open directory
    String rootFolder = (folderStr.startsWith("/") ? baseFolderForFS + folderStr : (baseFolderForFS + "/" + folderStr));
    DIR* dir = opendir(rootFolder.c_str());
    if (!dir)
    {
        xSemaphoreGive(_fileSysMutex);
        LOG_W(MODULE_PREFIX, "getFilesJSON Failed to open base folder %s", rootFolder.c_str());
        Utils::setJsonErrorResult(reqStr.c_str(), respStr, "nofolder");
        return false;
    }

    // Start response JSON
    respStr = R"({"req":")" + reqStr + R"(","rslt":"ok","fsName":")" + nameOfFS + R"(","fsBase":")" + baseFolderForFS + 
                R"(","diskSize":)" + String(fsSizeBytes) + R"(,"diskUsed":)" + fsUsedBytes +
                R"(,"folder":")" + String(rootFolder) + R"(","files":[)";
    bool firstFile = true;

    // Read directory entries
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
            respStr += ",";
        firstFile = false;
        respStr += R"({"name":")";
        respStr += ent->d_name;
        respStr += R"(","size":)";
        respStr += String(fileSize);
        respStr += "}";
    }

    // Finished with file list
    closedir(dir);
    xSemaphoreGive(_fileSysMutex);

    // Complete string and replenish cache
    respStr += "]}";
    if (_cacheFileList)
    {
        _cachedFileListResponse = respStr;
        _cachedFileListValid = true;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get file contents
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String FileSystem::getFileContents(const String& fileSystemStr, const String& filename, 
                int maxLen, bool cacheIfPossible)
{
    // Check file system supported
    String nameOfFS;
    if (!checkFileSystem(fileSystemStr, nameOfFS))
    {
        LOG_D(MODULE_PREFIX, "getContents %s invalid file system %s", filename.c_str(), fileSystemStr.c_str());
        return "";
    }

    // Filename

    String rootFilename = getFilePath(nameOfFS, filename);

    // Check if cached
    for (CachedFileEntry& cachedFile : _cachedFiles)
    {
        if (cachedFile._fileName.equals(rootFilename))
        {
            cachedFile._lastUsedMs = millis();
            return cachedFile._fileContents;
        }
    }

    // Take mutex
    xSemaphoreTake(_fileSysMutex, portMAX_DELAY);

    // Get file info - to check length
    struct stat st;
    if (stat(rootFilename.c_str(), &st) != 0)
    {
        xSemaphoreGive(_fileSysMutex);
        LOG_D(MODULE_PREFIX, "getContents %s cannot stat", rootFilename.c_str());
        return "";
    }
    if (!S_ISREG(st.st_mode))
    {
        xSemaphoreGive(_fileSysMutex);
        LOG_D(MODULE_PREFIX, "getContents %s is a folder", rootFilename.c_str());
        return "";
    }

    // Check valid
    if (maxLen <= 0)
    {
        maxLen = heap_caps_get_free_size(MALLOC_CAP_8BIT) / 3;
    }
    if (st.st_size >= maxLen-1)
    {
        xSemaphoreGive(_fileSysMutex);
        LOG_D(MODULE_PREFIX, "getContents %s free heap %d size %d too big to read", rootFilename.c_str(), maxLen, (int)st.st_size);
        return "";
    }
    int fileSize = st.st_size;

    // Open file
    FILE* pFile = fopen(rootFilename.c_str(), "rb");
    if (!pFile)
    {
        xSemaphoreGive(_fileSysMutex);
        LOG_D(MODULE_PREFIX, "getContents failed to open file to read %s", rootFilename.c_str());
        return "";
    }

    // Buffer
    uint8_t* pBuf = new uint8_t[fileSize+1];
    if (!pBuf)
    {
        fclose(pFile);
        xSemaphoreGive(_fileSysMutex);
        LOG_D(MODULE_PREFIX, "getContents failed to allocate %d", fileSize);
        return "";
    }

    // Read
    size_t bytesRead = fread((char*)pBuf, 1, fileSize, pFile);
    fclose(pFile);
    xSemaphoreGive(_fileSysMutex);
    pBuf[bytesRead] = 0;
    String readData = (char*)pBuf;
    delete [] pBuf;

    // Decide whether to cache
    if (cacheIfPossible && (readData.length() < _maxCacheBytes))
    {
        // Get current cache size
        int spaceNeeded = readData.length();
        int numCachedFiles = _cachedFiles.size();
        for (int i = 0; i < numCachedFiles; i++)
        {
            if (cachedFilesGetBytes() + spaceNeeded <= _maxCacheBytes)
                break;
            cachedFilesDropOldest();
        }
        // Check ok and store in cache
        if (cachedFilesGetBytes() + spaceNeeded < _maxCacheBytes)
            cachedFilesStoreNew(rootFilename, readData);
    }
    return readData;
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
    xSemaphoreTake(_fileSysMutex, portMAX_DELAY);

    // Open file for writing
    String rootFilename = getFilePath(nameOfFS, filename);
    FILE* pFile = fopen(rootFilename.c_str(), "wb");
    if (!pFile)
    {
        xSemaphoreGive(_fileSysMutex);
        LOG_D(MODULE_PREFIX, "setContents failed to open file to write %s", rootFilename.c_str());
        return "";
    }

    // Write
    size_t bytesWritten = fwrite((uint8_t*)(fileContents.c_str()), 1, fileContents.length(), pFile);
    fclose(pFile);

    // Clean up
    _cachedFileListValid = false;
    xSemaphoreGive(_fileSysMutex);
    return bytesWritten == fileContents.length();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Upload
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileSystem::uploadAPIBlocksComplete()
{
    // This may or may not be called depending on the interface used
    // HTTP file upload will call it
    // Other file upload paths don't - but it isn't needed as the uploadAPIBlockHandler as a finalBlock flag
}

void FileSystem::uploadAPIBlockHandler(const char* fileSystem, const String& req, const String& filename, 
                    int fileLength, size_t filePos, const uint8_t *data, size_t len, bool finalBlock)
{
#ifdef DEBUG_FILE_UPLOAD
    LOG_I(MODULE_PREFIX, "uploadAPIBlockHandler fileSys %s, filename %s, total %d, pos %d, len %d, final %d", 
                fileSystem, filename.c_str(), fileLength, filePos, len, finalBlock);
#endif

    // Check file system supported
    String nameOfFS;
    if (!checkFileSystem(String(fileSystem), nameOfFS))
    {
        LOG_W(MODULE_PREFIX, "uploadBlock file system %s not supported", fileSystem);
        return;
    }

    // Take mutex
    xSemaphoreTake(_fileSysMutex, portMAX_DELAY);
    String tempFileName = "/__tmp__";
    String tmpRootFilename = getFilePath(nameOfFS, tempFileName);
    if (data && (len > 0))
    {
        FILE* pFile = NULL;

        // Check if we should overwrite or append
        if (filePos > 0)
            pFile = fopen(tmpRootFilename.c_str(), "ab");
        else
            pFile = fopen(tmpRootFilename.c_str(), "wb");
        if (!pFile)
        {
            xSemaphoreGive(_fileSysMutex);
            LOG_W(MODULE_PREFIX, "uploadBlock failed to open file to write %s", tmpRootFilename.c_str());
            return;
        }

        // Write file block to temporary file
        size_t bytesWritten = fwrite(data, 1, len, pFile);
        fclose(pFile);
        if (bytesWritten != len)
        {
            LOG_W(MODULE_PREFIX, "uploadBlock write failed %s (written %d != len %d)", tmpRootFilename.c_str(), bytesWritten, len);
        }
        else
        {
#ifdef DEBUG_FILE_UPLOAD
            LOG_I(MODULE_PREFIX, "uploadBlock write ok %s (written %d == len %d)", tmpRootFilename.c_str(), bytesWritten, len);
#endif
        }
    }
    else if (!finalBlock)
    {
        LOG_W(MODULE_PREFIX, "uploadBlock Non-final block with null ptr or len %d == 0", len);
    }

    // Rename if last block
    if (finalBlock)
    {
        // Check if destination file exists before renaming
        struct stat st;
        String rootFilename = getFilePath(nameOfFS, filename);
        if (stat(rootFilename.c_str(), &st) == 0) 
        {
            // Remove in case filename already exists
            unlink(rootFilename.c_str());
        }

        // Rename
        if (rename(tmpRootFilename.c_str(), rootFilename.c_str()) != 0)
        {
            LOG_W(MODULE_PREFIX, "uploadBlock failed rename %s to %s", tmpRootFilename.c_str(), rootFilename.c_str());
        }
        else
        {
            // Cached file list now invalid
            _cachedFileListValid = false;
#ifdef DEBUG_FILE_UPLOAD
            LOG_I(MODULE_PREFIX, "uploadBlock finalBlock rename OK fileSys %s, filename %s, total %d, pos %d, len %d", 
                nameOfFS.c_str(), filename.c_str(), fileLength, filePos, len);
#endif
        }

        // Restore semaphore
        xSemaphoreGive(_fileSysMutex);
        
        // Debug - check file present
        if (!exists(getFilePath(nameOfFS, filename).c_str()))
        {
            LOG_W(MODULE_PREFIX, "uploadBlock file does not exist %s", getFilePath(nameOfFS, filename).c_str());
        }
        if (exists(tmpRootFilename.c_str()))
        {
            LOG_W(MODULE_PREFIX, "uploadBlock temp file exists %s", tmpRootFilename.c_str());
        }
    }
    else
    {
        // Restore semaphore
        xSemaphoreGive(_fileSysMutex);
    }
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
    xSemaphoreTake(_fileSysMutex, portMAX_DELAY);

    // Remove file
    struct stat st;
    String rootFilename = getFilePath(nameOfFS, filename);
    if (stat(rootFilename.c_str(), &st) == 0) 
    {
        unlink(rootFilename.c_str());
    }

    _cachedFileListValid = false;
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

String FileSystem::getFileExtension(String& fileName)
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

bool FileSystem::checkFileSystem(const String& fileSystemStr, String& fsName)
{
    // Check file system
    fsName = fileSystemStr;
    fsName.trim();
    fsName.toLowerCase();

    // Use default file system if not specified
    if (fsName.length() == 0)
        fsName = getDefaultFSRoot();

    // Handle access
    if (fsName == "spiffs")
    {
        if (!_spiffsIsOk)
            return false;
        return true;
    }
    if (fsName == "sd")
    {
        if (!_sdIsOk)
            return false;
        return true;
    }

    // Unknown FS
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get file path
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String FileSystem::getFilePath(const String& nameOfFS, const String& filename)
{
    // Check if filename already contains file system
    if ((filename.indexOf("spiffs/") >= 0) || (filename.indexOf("sd/") >= 0))
        return (filename.startsWith("/") ? filename : ("/" + filename));
    return (filename.startsWith("/") ? "/" + nameOfFS + filename : ("/" + nameOfFS + "/" + filename));
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get file path with fs check
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool FileSystem::getFileFullPath(const String& filename, String& fileFullPath)
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

bool FileSystem::exists(const char* path)
{
    // Take mutex
    xSemaphoreTake(_fileSysMutex, portMAX_DELAY);
    struct stat buffer;   
    bool rslt = (stat(path, &buffer) == 0);
    xSemaphoreGive(_fileSysMutex);
    return rslt;
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
    xSemaphoreTake(_fileSysMutex, portMAX_DELAY);

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
    xSemaphoreTake(_fileSysMutex, portMAX_DELAY);

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
// Cached file helpers
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t FileSystem::cachedFilesGetBytes()
{
    uint32_t totalBytes = 0;
    for (CachedFileEntry& cachedFile : _cachedFiles)
        totalBytes += cachedFile._fileContents.length();
    return totalBytes;
}

void FileSystem::cachedFilesDropOldest()
{
    // Find oldest
    std::list<CachedFileEntry>::iterator oldestIt = _cachedFiles.begin();
    uint32_t oldestMs = 0;
    for (std::list<CachedFileEntry>::iterator it = _cachedFiles.begin(); it != _cachedFiles.end(); it++)
    {
        uint32_t sinceUsedMs = Utils::timeElapsed(millis(), it->_lastUsedMs);
        if (oldestMs < sinceUsedMs)
        {
            oldestMs = sinceUsedMs;
            oldestIt = it;
        }
    }

    // Remove oldest
    if (oldestIt != _cachedFiles.end())
        _cachedFiles.erase(oldestIt);
}

void FileSystem::cachedFilesStoreNew(const String& fileName, const String& fileContents)
{
    // Append to list
    CachedFileEntry ent(fileName, fileContents);
    _cachedFiles.push_back(ent);
}

