/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FileManager 
//
// Rob Dobson 2018-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <FileManager.h>
#include <FileSystem.h>
#include <ConfigPinMap.h>
#include <RestAPIEndpointManager.h>
#include <Logger.h>

static const char* MODULE_PREFIX = "FileManager";

// #define DEBUG_FILE_MANAGER_FILE_LIST
// #define DEBUG_FILE_MANAGER_FILE_LIST_DETAIL
// #define DEBUG_FILE_MANAGER_UPLOAD

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FileManager::FileManager(const char *pModuleName, ConfigBase& defaultConfig, ConfigBase* pGlobalConfig, ConfigBase* pMutableConfig) 
        : SysModBase(pModuleName, defaultConfig, pGlobalConfig, pMutableConfig)
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileManager::setup()
{
    applySetup();
}

void FileManager::applySetup()
{
    // Config settings
    bool enableSPIFFS = (configGetLong("SPIFFSEnabled", 0) != 0);
    bool spiffsFormatIfCorrupt = (configGetLong("SPIFFSFormatIfCorrupt", 0) != 0);
    bool enableSD = (configGetLong("SDEnabled", 0) != 0);
    bool defaultToSDIfAvailable = (configGetLong("DefaultSD", 0) != 0);
    bool cacheFileList = (configGetLong("CacheFileList", 0) != 0);

    // SD pins
    String pinName = configGetString("SDMOSI", "");
    int sdMOSIPin = ConfigPinMap::getPinFromName(pinName.c_str());
    pinName = configGetString("SDMISO", "");
    int sdMISOPin = ConfigPinMap::getPinFromName(pinName.c_str());
    pinName = configGetString("SDCLK", "");
    int sdCLKPin = ConfigPinMap::getPinFromName(pinName.c_str());
    pinName = configGetString("SDCS", "");
    int sdCSPin = ConfigPinMap::getPinFromName(pinName.c_str());

    // Cache max
    int maxCacheBytes = configGetLong("maxCacheBytes", 5000);

    // Setup file system
    fileSystem.setup(enableSPIFFS, spiffsFormatIfCorrupt, enableSD, sdMOSIPin, sdMISOPin, sdCLKPin, sdCSPin, 
                defaultToSDIfAvailable, cacheFileList, maxCacheBytes);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileManager::service()
{
    // Nothing to do
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// REST API Endpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileManager::addRestAPIEndpoints(RestAPIEndpointManager& endpointManager)
{
    endpointManager.addEndpoint("reformatfs", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                    std::bind(&FileManager::apiReformatFS, this, std::placeholders::_1, std::placeholders::_2), 
                    "Reformat file system e.g. /spiffs");
    endpointManager.addEndpoint("filelist", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                    std::bind(&FileManager::apiFileList, this, std::placeholders::_1, std::placeholders::_2), 
                    "List files in folder e.g. /spiffs/folder ... ~ for / in folder");
    endpointManager.addEndpoint("fileread", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                    std::bind(&FileManager::apiFileRead, this, std::placeholders::_1, std::placeholders::_2), 
                    "Read file ... name", "text/plain");
    endpointManager.addEndpoint("filedelete", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                    std::bind(&FileManager::apiDeleteFile, this, std::placeholders::_1, std::placeholders::_2), 
                    "Delete file e.g. /spiffs/filename ... ~ for / in filename");
    endpointManager.addEndpoint("fileupload", 
                    RestAPIEndpointDef::ENDPOINT_CALLBACK, 
                    RestAPIEndpointDef::ENDPOINT_POST,
                    std::bind(&FileManager::apiUploadToFileManComplete, this, 
                            std::placeholders::_1, std::placeholders::_2),
                    "Upload file", 
                    "application/json", 
                    NULL,
                    RestAPIEndpointDef::ENDPOINT_CACHE_NEVER,
                    NULL, 
                    NULL,
                    std::bind(&FileManager::apiUploadToFileManPart, this, 
                            std::placeholders::_1, std::placeholders::_2));
}

// Format file system
void FileManager::apiReformatFS(const String &reqStr, String& respStr)
{
    // File system
    String fileSystemStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1);
    fileSystem.reformat(fileSystemStr, respStr);
}

// List files on a file system
// Uses FileManager.h
// In the reqStr the first part of the path is the file system name (e.g. sd or spiffs, can be blank to default)
// The second part of the path is the folder - note that / must be replaced with ~ in folder
void FileManager::apiFileList(const String &reqStr, String& respStr)
{
    // File system
    String fileSystemStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1);
    // Folder
    String folderStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 2);
    String extraPath = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 3);
    if (extraPath.length() > 0)
        folderStr += "/" + extraPath;

#ifdef DEBUG_FILE_MANAGER_FILE_LIST
    LOG_I(MODULE_PREFIX, "apiFileList req %s fileSysStr %s folderStr %s", reqStr.c_str(), fileSystemStr.c_str(), folderStr.c_str());
#endif

    folderStr.replace("~", "/");
    if (folderStr.length() == 0)
        folderStr = "/";
    fileSystem.getFilesJSON(reqStr.c_str(), fileSystemStr, folderStr, respStr);

#ifdef DEBUG_FILE_MANAGER_FILE_LIST_DETAIL
    LOG_W(MODULE_PREFIX, "apiFileList respStr %s", respStr.c_str());
#endif
}

// Read file contents
// Uses FileManager.h
// In the reqStr the first part of the path is the file system name (e.g. sd or spiffs)
// The second part of the path is the folder and filename - note that / must be replaced with ~ in folder
void FileManager::apiFileRead(const String &reqStr, String& respStr)
{
    // File system
    String fileSystemStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1);
    // Filename
    String fileNameStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 2);
    String extraPath = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 3);
    if (extraPath.length() > 0)
        fileNameStr += "/" + extraPath;
    fileNameStr.replace("~", "/");
    respStr = fileSystem.getFileContents(fileSystemStr, fileNameStr);
}

// Delete file on the file system
// Uses FileManager.h
// In the reqStr the first part of the path is the file system name (e.g. sd or spiffs)
// The second part of the path is the filename - note that / must be replaced with ~ in filename
void FileManager::apiDeleteFile(const String &reqStr, String& respStr)
{
    // File system
    String fileSystemStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1);
    // Filename
    String filenameStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 2);
    String extraPath = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 3);
    if (extraPath.length() > 0)
        filenameStr += "/" + extraPath;
    bool rslt = false;
    filenameStr.replace("~", "/");
    if (filenameStr.length() != 0)
        rslt = fileSystem.deleteFile(fileSystemStr, filenameStr);
    Utils::setJsonBoolResult(reqStr.c_str(), respStr, rslt);
    LOG_I(MODULE_PREFIX, "deleteFile reqStr %s fs %s, filename %s rslt %s", 
                        reqStr.c_str(), fileSystemStr.c_str(), filenameStr.c_str(),
                        rslt ? "ok" : "fail");
}

// Upload file to file system - completed
void FileManager::apiUploadToFileManComplete(const String &reqStr, String &respStr)
{
#ifdef DEBUG_FILE_MANAGER_UPLOAD
    LOG_I(MODULE_PREFIX, "apiUploadToFileManComplete %s", reqStr.c_str());
#endif
    fileSystem.uploadAPIBlocksComplete();
    Utils::setJsonBoolResult(reqStr.c_str(), respStr, true);
    }

// Upload file to file system - part of file (from HTTP POST file)
void FileManager::apiUploadToFileManPart(const String& req, FileBlockInfo& fileBlockInfo)
{
#ifdef DEBUG_FILE_MANAGER_UPLOAD
    LOG_I(MODULE_PREFIX, "apiUpToFileMan fileLen %d filePos %d blockLen %d isFinal %d", 
            fileBlockInfo.fileLen, fileBlockInfo.filePos,
            fileBlockInfo.blockLen, fileBlockInfo.finalBlock);
#endif
    fileSystem.uploadAPIBlockHandler("", req, fileBlockInfo);
}
