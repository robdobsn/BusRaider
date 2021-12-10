/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// StreamManager 
//
// Rob Dobson 2018-2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <StreamManager.h>
#include <RestAPIEndpointManager.h>
#include <Utils.h>
#include <Logger.h>
#include <ArduinoTime.h>

static const char* MODULE_PREFIX = "StreamManager";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

StreamManager::StreamManager(const char *pModuleName, ConfigBase& defaultConfig, ConfigBase* pGlobalConfig, ConfigBase* pMutableConfig) 
        : SysModBase(pModuleName, defaultConfig, pGlobalConfig, pMutableConfig)
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void StreamManager::setup()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void StreamManager::service()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// REST API Endpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void StreamManager::addRestAPIEndpoints(RestAPIEndpointManager& endpointManager)
{
    endpointManager.addEndpoint("teststream", 
                    RestAPIEndpointDef::ENDPOINT_CALLBACK, 
                    RestAPIEndpointDef::ENDPOINT_POST,
                    std::bind(&StreamManager::apiTestStreamComplete, this, 
                            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                    "TestStream", 
                    "application/json", 
                    nullptr,
                    RestAPIEndpointDef::ENDPOINT_CACHE_NEVER,
                    nullptr, 
                    nullptr,
                    std::bind(&StreamManager::apiTestStreamBlock, this, 
                            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                    std::bind(&StreamManager::apiTestStreamIsReady, this, std::placeholders::_1));
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Stream test - completed
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void StreamManager::apiTestStreamComplete(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo)
{
    LOG_I(MODULE_PREFIX, "apiTestStreamComplete %s", reqStr.c_str());
    Utils::setJsonBoolResult(reqStr.c_str(), respStr, true);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Stream test - chunk
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool StreamManager::apiTestStreamBlock(const String& req, FileStreamBlock& fileStreamBlock, 
                const APISourceInfo& sourceInfo)
{
    LOG_I(MODULE_PREFIX, "apiTestStreamBlock %s len %d channelID %d", 
                    req.c_str(), fileStreamBlock.blockLen, sourceInfo.channelID);
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Stream test - isReady
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool StreamManager::apiTestStreamIsReady(const APISourceInfo& sourceInfo)
{
    if (!Utils::isTimeout(millis(), _lastReqMs, 2000))
        return false;
    _lastReqMs = millis();
    LOG_I(MODULE_PREFIX, "apiTestStreamIsReady channelID %d", sourceInfo.channelID);
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Upload file to file system - part of file (from HTTP POST file)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool StreamManager::fileStreamDataBlock(FileStreamBlock& fileStreamBlock)
{
// #ifdef DEBUG_FILE_MANAGER_UPLOAD
    LOG_I(MODULE_PREFIX, "fileStreamDataBlock fileName %s fileLen %d filePos %d blockLen %d isFinal %d", 
            fileStreamBlock.filename ? fileStreamBlock.filename : "<null>", 
            fileStreamBlock.fileLen, fileStreamBlock.filePos,
            fileStreamBlock.blockLen, fileStreamBlock.finalBlock);
// #endif

    // // Handle block
    // return _fileTransferHelper.handleRxBlock(fileStreamBlock);
    return true;
}
