/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Log Manager
// Handles logging to different destinations
//
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <LogManager.h>
#include <SysManager.h>
#include <LoggerCore.h>
#include <LoggerPapertrail.h>

// Log prefix
static const char *MODULE_PREFIX = "LogMan";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

LogManager::LogManager(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, 
            ConfigBase *pMutableConfig)
    : SysModBase(pModuleName, defaultConfig, pGlobalConfig, pMutableConfig)
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void LogManager::setup()
{
    // Clear existing loggers
    loggerCore.clearLoggers();

    // Get config
    std::vector<String> logDests;
    configGetArrayElems("logDests", logDests);
    for (const ConfigBase logDestConfig : logDests)
    {
        // LOG_I(MODULE_PREFIX, "logDest %s", logDestConfig.getConfigString().c_str());
        // Get type
        bool isEnabled = logDestConfig.getLong("enable", true) != 0;
        if (!isEnabled)
            continue;
        String logDestType = logDestConfig.getString("type", "");
        if (logDestType.equalsIgnoreCase("Papertrail"))
        {
            // Construct papertrail logger
            LoggerPapertrail *pLogger = new LoggerPapertrail(logDestConfig);
            loggerCore.addLogger(pLogger);
            // LOG_I(MODULE_PREFIX, "Added Papertrail logger");
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void LogManager::service()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get status JSON
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String LogManager::getStatusJSON()
{
    return "{}";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Debug
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String LogManager::getDebugJSON()
{
    return "{}";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Endpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void LogManager::addRestAPIEndpoints(RestAPIEndpointManager &endpointManager)
{
}

void LogManager::addProtocolEndpoints(ProtocolEndpointManager& endpointManager)
{
}