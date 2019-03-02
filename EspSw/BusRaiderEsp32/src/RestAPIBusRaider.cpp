// REST API for System, WiFi, etc
// Rob Dobson 2012-2018

#include "ArduinoLog.h"
#include "Update.h"
#include "RestAPIBusRaider.h"
#include "MachineInterface.h"
#include "RestAPIEndpoints.h"
#include "RestAPISystem.h"

static const char* MODULE_PREFIX = "RestAPIBusRaider: ";

RestAPIBusRaider::RestAPIBusRaider(CommandSerial &commandSerial, MachineInterface &machineInterface, 
            FileManager& fileManager, RestAPISystem& restAPISystem) :
            _commandSerial(commandSerial), _machineInterface(machineInterface),
            _fileManager(fileManager), _restAPISystem(restAPISystem)
{
    _restartPending = false;
    _restartPendingStartMs = 0;
}

void RestAPIBusRaider::service()
{
    if (_restartPending && Utils::isTimeout(millis(), _restartPendingStartMs, TIME_TO_WAIT_BEFORE_RESTART_MS))
    {
        ESP.restart();
    }
}

void RestAPIBusRaider::apiTargetCommand(String &reqStr, String &respStr)
{
    bool rslt = true;
    // Get command
    String targetCmd = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 1);
    Log.trace("%sCommand %s\n", MODULE_PREFIX, targetCmd.c_str());
    _commandSerial.sendTargetCommand(targetCmd);
    Utils::setJsonBoolResult(respStr, rslt);
}

#ifdef SUPPORT_WEB_TERMINAL_REST
void RestAPIBusRaider::apiPostCharsDone(String &reqStr, String &respStr)
{
    bool rslt = true;
    Utils::setJsonBoolResult(respStr, rslt);
}

void RestAPIBusRaider::apiPostCharsBody(uint8_t *data, size_t len, size_t index, size_t total)
{
    // Log.trace("Rest %s %d %d %d\n", ss, len, index, total);
    // machineInterface.sendSerialRxDataToHost(data, len);
}
#endif

void RestAPIBusRaider::apiUploadToFileManComplete(String &reqStr, String &respStr)
{
    Log.trace("%sapiUploadToFileManComplete %s\n", MODULE_PREFIX, reqStr.c_str());
    Utils::setJsonBoolResult(respStr, true);
}

void RestAPIBusRaider::apiUploadToFileManPart(String& req, String& filename, size_t contentLen, size_t index, 
                uint8_t *data, size_t len, bool finalBlock)
{
    Log.verbose("%sapiUpToFileMan %d, %d, %d, %d\n", MODULE_PREFIX, contentLen, index, len, finalBlock);
    if (contentLen > 0)
        _fileManager.uploadAPIBlockHandler("SPIFFS", req, filename, contentLen, index, data, len, finalBlock);
}

void RestAPIBusRaider::apiUploadPiSwComplete(String &reqStr, String &respStr)
{
    Log.trace("%sapiUploadPiSwComplete %s\n", MODULE_PREFIX, reqStr.c_str());
    Utils::setJsonBoolResult(respStr, true);
}

void RestAPIBusRaider::apiUploadPiSwPart(String& req, String& filename, size_t contentLen, size_t index, 
                uint8_t *data, size_t len, bool finalBlock)
{
    Log.verbose("%sapiUploadPiSwPart %d, %d, %d, %d\n", MODULE_PREFIX, contentLen, index, len, finalBlock);
    if (contentLen > 0)
        _commandSerial.uploadAPIBlockHandler("firmware", req, filename, contentLen, index, data, len, finalBlock);
}

void RestAPIBusRaider::apiUploadAppendComplete(String &reqStr, String &respStr)
{
    Log.trace("%sapiUploadAppendComplete %s\n", MODULE_PREFIX, reqStr.c_str());
    Utils::setJsonBoolResult(respStr, true);
}

void RestAPIBusRaider::apiUploadAppendPart(String& req, String& filename, size_t contentLen, size_t index, 
                uint8_t *data, size_t len, bool finalBlock)
{
    Log.verbose("%sapiUp&Append %d, %d, %d, %d\n", MODULE_PREFIX, contentLen, index, len, finalBlock);
    if (contentLen > 0)
        _commandSerial.uploadAPIBlockHandler("target", req, filename, contentLen, index, data, len, finalBlock);
}

void RestAPIBusRaider::apiUploadAndRunComplete(String &reqStr, String &respStr)
{
    Log.trace("%sapiUploadAndRunComplete %s\n", MODULE_PREFIX, reqStr.c_str());
    _commandSerial.sendTargetCommand("ProgramAndReset");
    Utils::setJsonBoolResult(respStr, true);
}

void RestAPIBusRaider::apiUploadAndRunPart(String& req, String& filename, size_t contentLen, size_t index, 
                uint8_t *data, size_t len, bool finalBlock)
{
    Log.verbose("%sapiUp&Run %d, %d, %d, %d\n", MODULE_PREFIX, contentLen, index, len, finalBlock);
    if (contentLen > 0)
        _commandSerial.uploadAPIBlockHandler("target", req, filename, contentLen, index, data, len, finalBlock);
}

void RestAPIBusRaider::apiSendFileToTargetBuffer(const String &reqStr, String &respStr)
{
    // Clear target first
    _commandSerial.sendTargetCommand("ClearTarget");
    // File system
    String fileSystemStr = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 1);
    // Filename        
    String filename = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 2);
    Log.verbose("%sapiSendFileToBuf filename %s\n", MODULE_PREFIX, filename.c_str());
    _commandSerial.startUploadFromFileSystem(fileSystemStr, reqStr, filename, "");
}

void RestAPIBusRaider::apiAppendFileToTargetBuffer(const String &reqStr, String &respStr)
{
    // File system
    String fileSystemStr = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 1);
    // Filename        
    String filename = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 2);
    Log.verbose("%sapiAppendFileToBuf filename %s\n", MODULE_PREFIX, filename.c_str());
    _commandSerial.startUploadFromFileSystem(fileSystemStr, reqStr, filename, "");
}

void RestAPIBusRaider::runFileOnTarget(const String &reqStr, String &respStr)
{
    // Clear target first
    _commandSerial.sendTargetCommand("ClearTarget");
    // File system
    String fileSystemStr = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 1);
    // Filename        
    String filename = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 2);
    Log.verbose("%srunFileOnTarget filename %s\n", MODULE_PREFIX, filename.c_str());
    _commandSerial.startUploadFromFileSystem(fileSystemStr, "", filename, "ProgramAndReset");
}

void RestAPIBusRaider::apiQueryStatus(const String &reqStr, String &respStr)
{
    Log.verbose("%sapiQueryStatus %s\n", MODULE_PREFIX, reqStr.c_str());
    respStr = _machineInterface.getStatus();
}

void RestAPIBusRaider::apiQueryESPHealth(const String &reqStr, String &respStr)
{
    Log.verbose("%squeryESPHealth %s\n", MODULE_PREFIX, reqStr.c_str());
    String healthStr;
    _restAPISystem.reportHealth(0, NULL, &healthStr);
    respStr = "\"espHealth\":{" + healthStr + "}";
}

void RestAPIBusRaider::apiDebugger(const String &reqStr, String &respStr)
{
    Log.verbose("%sdebugger %s\n", MODULE_PREFIX, reqStr.c_str());
    // Send target command to Pi
    _commandSerial.sendTargetData("rdp", (const uint8_t*)reqStr.c_str(), 
                reqStr.length(), 0);
    Utils::setJsonBoolResult(respStr, true);
}

void RestAPIBusRaider::apiESPFirmwarePart(String& req, String& filename, size_t contentLen, size_t index, 
                uint8_t *data, size_t len, bool finalBlock)
{
    Log.trace("%sapiESPFirmwarePart %d, %d, %d, %d\n", MODULE_PREFIX, contentLen, index, len, finalBlock);
    // Check if first part
    if (index == 0)
    {
        // Abort any existing update process
        Update.abort();

        // Check the update can be started
        bool enoughSpace = Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH);
        if (!enoughSpace)
        {
            Log.notice("%sNot enough space for firmware\n", MODULE_PREFIX);
            return;
        }
    }
    // Check if in progress
    if (Update.isRunning())
    {
        bool bytesWritten = Update.write(data, len);
        if (bytesWritten != len)
        {
            Log.warning("%sapiESPFirmwarePart failed to write correct amount %d != %d\n", MODULE_PREFIX,
                        bytesWritten, len);
        }
    }
    // Check if final block
    if (finalBlock)
    {
        if (Update.isFinished())
        {
            Log.warning("%sapiESPFirmwarePart finished - rebooting ...\n", MODULE_PREFIX);
            Update.end();
            _restartPendingStartMs = millis();
            _restartPending = true;
        }
        else
        {
            Log.warning("%sapiESPFirmwarePart final block but not finished - abort!\n", MODULE_PREFIX);
            Update.abort();
        }
    }
}

void RestAPIBusRaider::apiESPFirmwareUpdateDone(String &reqStr, String &respStr)
{
    Log.trace("%sapiESPFirmwareUpdate %s\n", MODULE_PREFIX, reqStr.c_str());
    Utils::setJsonBoolResult(respStr, true);
}

void RestAPIBusRaider::setup(RestAPIEndpoints &endpoints)
{
    endpoints.addEndpoint("targetcmd", 
                        RestAPIEndpointDef::ENDPOINT_CALLBACK, 
                        RestAPIEndpointDef::ENDPOINT_GET, 
                        std::bind(&RestAPIBusRaider::apiTargetCommand, this,
                                std::placeholders::_1, std::placeholders::_2),
                        "Target command");
    endpoints.addEndpoint("sendfiletotargetbuffer", 
                        RestAPIEndpointDef::ENDPOINT_CALLBACK, 
                        RestAPIEndpointDef::ENDPOINT_GET, 
                        std::bind(&RestAPIBusRaider::apiSendFileToTargetBuffer, this,
                                std::placeholders::_1, std::placeholders::_2),
                        "Send file to target buffer - from file system to target buffer");                            
    endpoints.addEndpoint("appendfiletotargetbuffer", 
                        RestAPIEndpointDef::ENDPOINT_CALLBACK, 
                        RestAPIEndpointDef::ENDPOINT_GET, 
                        std::bind(&RestAPIBusRaider::apiAppendFileToTargetBuffer, this,
                                std::placeholders::_1, std::placeholders::_2),
                        "Append file to target buffer - from file system to target buffer");                            
    endpoints.addEndpoint("runfileontarget", 
                        RestAPIEndpointDef::ENDPOINT_CALLBACK, 
                        RestAPIEndpointDef::ENDPOINT_GET, 
                        std::bind(&RestAPIBusRaider::runFileOnTarget, this,
                                std::placeholders::_1, std::placeholders::_2),
                        "Run file on target - /fileSystem/filename - from file system");                            
#ifdef SUPPORT_WEB_TERMINAL_REST
    endpoints.addEndpoint("postchars", 
                        RestAPIEndpointDef::ENDPOINT_CALLBACK, 
                        RestAPIEndpointDef::ENDPOINT_POST, 
                        std::bind(&RestAPIBusRaider::apiPostCharsDone, this,
                                std::placeholders::_1, std::placeholders::_2),
                        "PostChars to host machine", "application/json", 
                        NULL, 
                        true, 
                        NULL,
                        std::bind(&RestAPIBusRaider::apiPostCharsBody, this, 
                                std::placeholders::_1, std::placeholders::_2, 
                                std::placeholders::_3, std::placeholders::_4));
#endif
    endpoints.addEndpoint("uploadpisw", 
                        RestAPIEndpointDef::ENDPOINT_CALLBACK, 
                        RestAPIEndpointDef::ENDPOINT_POST,
                        std::bind(&RestAPIBusRaider::apiUploadPiSwComplete, this, 
                                std::placeholders::_1, std::placeholders::_2),
                        "Upload Pi Software", "application/json", 
                        NULL, 
                        true, 
                        NULL,
                        NULL,
                        std::bind(&RestAPIBusRaider::apiUploadPiSwPart, this, 
                                std::placeholders::_1, std::placeholders::_2, 
                                std::placeholders::_3, std::placeholders::_4,
                                std::placeholders::_5, std::placeholders::_6,
                                std::placeholders::_7));
    endpoints.addEndpoint("uploadappend", 
                        RestAPIEndpointDef::ENDPOINT_CALLBACK, 
                        RestAPIEndpointDef::ENDPOINT_POST,
                        std::bind(&RestAPIBusRaider::apiUploadAppendComplete, this, 
                                std::placeholders::_1, std::placeholders::_2),
                        "Upload file", "application/json", 
                        NULL, 
                        true, 
                        NULL,
                        NULL,
                        std::bind(&RestAPIBusRaider::apiUploadAppendPart, this, 
                                std::placeholders::_1, std::placeholders::_2, 
                                std::placeholders::_3, std::placeholders::_4,
                                std::placeholders::_5, std::placeholders::_6,
                                std::placeholders::_7));   
    endpoints.addEndpoint("uploadtofileman", 
                        RestAPIEndpointDef::ENDPOINT_CALLBACK, 
                        RestAPIEndpointDef::ENDPOINT_POST,
                        std::bind(&RestAPIBusRaider::apiUploadToFileManComplete, this, 
                                std::placeholders::_1, std::placeholders::_2),
                        "Upload file", "application/json", 
                        NULL, 
                        true, 
                        NULL,
                        NULL,
                        std::bind(&RestAPIBusRaider::apiUploadToFileManPart, this, 
                                std::placeholders::_1, std::placeholders::_2, 
                                std::placeholders::_3, std::placeholders::_4,
                                std::placeholders::_5, std::placeholders::_6,
                                std::placeholders::_7));                                    
    endpoints.addEndpoint("uploadandrun", 
                        RestAPIEndpointDef::ENDPOINT_CALLBACK, 
                        RestAPIEndpointDef::ENDPOINT_POST,
                        std::bind(&RestAPIBusRaider::apiUploadAndRunComplete, this, 
                                std::placeholders::_1, std::placeholders::_2),
                        "Upload and run file", "application/json", 
                        NULL, 
                        true, 
                        NULL,
                        NULL,
                        std::bind(&RestAPIBusRaider::apiUploadAndRunPart, this, 
                                std::placeholders::_1, std::placeholders::_2, 
                                std::placeholders::_3, std::placeholders::_4,
                                std::placeholders::_5, std::placeholders::_6,
                                std::placeholders::_7));                                    
    endpoints.addEndpoint("querystatus", 
                        RestAPIEndpointDef::ENDPOINT_CALLBACK, 
                        RestAPIEndpointDef::ENDPOINT_GET, 
                        std::bind(&RestAPIBusRaider::apiQueryStatus, this,
                                std::placeholders::_1, std::placeholders::_2),
                        "Query status");
    endpoints.addEndpoint("queryESPHealth", 
                        RestAPIEndpointDef::ENDPOINT_CALLBACK, 
                        RestAPIEndpointDef::ENDPOINT_GET, 
                        std::bind(&RestAPIBusRaider::apiQueryESPHealth, this,
                                std::placeholders::_1, std::placeholders::_2),
                        "Query ESP Health");
    endpoints.addEndpoint("espFirmwareUpdate", 
                        RestAPIEndpointDef::ENDPOINT_CALLBACK, 
                        RestAPIEndpointDef::ENDPOINT_POST,
                        std::bind(&RestAPIBusRaider::apiESPFirmwareUpdateDone, this, 
                                std::placeholders::_1, std::placeholders::_2),
                        "Update ESP32 firmware", "application/json", 
                        NULL, 
                        true, 
                        NULL,
                        NULL,
                        std::bind(&RestAPIBusRaider::apiESPFirmwarePart, this, 
                                std::placeholders::_1, std::placeholders::_2, 
                                std::placeholders::_3, std::placeholders::_4,
                                std::placeholders::_5, std::placeholders::_6,
                                std::placeholders::_7));
}
