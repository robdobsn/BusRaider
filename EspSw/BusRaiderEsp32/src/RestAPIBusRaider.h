// REST API for System, WiFi, etc
// Rob Dobson 2012-2018

#pragma once
#include <Arduino.h>
#include "RestAPIEndpoints.h"
#include "CommandSerial.h"
#include "MachineInterface.h"
#include "FileManager.h"

// #define SUPPORT_WEB_TERMINAL_REST 1

class RestAPIBusRaider
{
  private:
    CommandSerial& _commandSerial;
    MachineInterface& _machineInterface;
    FileManager& _fileManager;    
    static const int TIME_TO_WAIT_BEFORE_RESTART_MS = 1000;
    uint32_t _restartPendingStartMs;
    bool _restartPending;

  public:
    RestAPIBusRaider(CommandSerial &commandSerial, MachineInterface &machineInterface, FileManager& fileManager) :
             _commandSerial(commandSerial), _machineInterface(machineInterface), _fileManager(fileManager)
    {
        _restartPending = false;
        _restartPendingStartMs = 0;
    }

    void service()
    {
        if (_restartPending && Utils::isTimeout(millis(), _restartPendingStartMs, TIME_TO_WAIT_BEFORE_RESTART_MS))
        {
            ESP.restart();
        }
    }

    void apiTargetCommand(String &reqStr, String &respStr)
    {
        bool rslt = true;
        // Get command
        String targetCmd = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 1);
        Log.trace("RestAPIBusRaider: Command %s\n", targetCmd.c_str());
        _commandSerial.sendTargetCommand(targetCmd);
        Utils::setJsonBoolResult(respStr, rslt);
    }

#ifdef SUPPORT_WEB_TERMINAL_REST
    void apiPostCharsDone(String &reqStr, String &respStr)
    {
        bool rslt = true;
        Utils::setJsonBoolResult(respStr, rslt);
    }

    void apiPostCharsBody(uint8_t *data, size_t len, size_t index, size_t total)
    {
        // Log.trace("Rest %s %d %d %d\n", ss, len, index, total);
        // machineInterface.sendSerialRxDataToHost(data, len);
    }
#endif

    void apiUploadToFileManComplete(String &reqStr, String &respStr)
    {
        Log.trace("RestAPIBusRaider: apiUploadToFileManComplete %s\n", reqStr.c_str());
        Utils::setJsonBoolResult(respStr, true);
    }

    void apiUploadToFileManPart(String filename, size_t contentLen, size_t index, 
                    uint8_t *data, size_t len, bool finalBlock)
    {
        Log.verbose("RestAPIBusRaider: apiUpToFileMan %d, %d, %d, %d\n", contentLen, index, len, finalBlock);
        if (contentLen > 0)
            _fileManager.uploadAPIBlockHandler("SPIFFS", filename, contentLen, index, data, len, finalBlock);
    }

    void apiUploadPiSwComplete(String &reqStr, String &respStr)
    {
        Log.trace("RestAPIBusRaider: apiUploadPiSwComplete %s\n", reqStr.c_str());
        Utils::setJsonBoolResult(respStr, true);
    }

    void apiUploadPiSwPart(String filename, size_t contentLen, size_t index, 
                    uint8_t *data, size_t len, bool finalBlock)
    {
        Log.verbose("RestAPIBusRaider: apiUploadPiSwPart %d, %d, %d, %d\n", contentLen, index, len, finalBlock);
        if (contentLen > 0)
            _commandSerial.uploadAPIBlockHandler("firmware", filename, contentLen, index, data, len, finalBlock);
    }

    void apiUploadAndRunComplete(String &reqStr, String &respStr)
    {
        Log.trace("RestAPIBusRaider: apiUploadAndRunComplete %s\n", reqStr.c_str());
        _commandSerial.sendTargetCommand("ProgramAndReset");
        Utils::setJsonBoolResult(respStr, true);
    }

    void apiUploadAndRunPart(String filename, size_t contentLen, size_t index, 
                    uint8_t *data, size_t len, bool finalBlock)
    {
        Log.verbose("apiUp&Run %d, %d, %d, %d\n", contentLen, index, len, finalBlock);
        if (contentLen > 0)
            _commandSerial.uploadAPIBlockHandler("target", filename, contentLen, index, data, len, finalBlock);
    }

    void apiSendFileToTargetBuffer(const String &reqStr, String &respStr)
    {
        // Clear target first
        _commandSerial.sendTargetCommand("ClearTarget");
        // File system
        String fileSystemStr = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 1);
        // Filename        
        String filename = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 2);
        Log.verbose("apiSendFileToBuf filename %s\n", filename.c_str());
        _commandSerial.startUploadFromFileSystem(fileSystemStr, filename);
    }

    void apiAppendFileToTargetBuffer(const String &reqStr, String &respStr)
    {
        // Clear target first
        _commandSerial.sendTargetCommand("ClearTarget");
        // File system
        String fileSystemStr = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 1);
        // Filename        
        String filename = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 2);
        Log.verbose("apiAppendFileToBuf filename %s\n", filename.c_str());
        _commandSerial.startUploadFromFileSystem(fileSystemStr, filename);
    }

    void runFileOnTarget(const String &reqStr, String &respStr)
    {
        // Clear target first
        _commandSerial.sendTargetCommand("ClearTarget");
        // File system
        String fileSystemStr = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 1);
        // Filename        
        String filename = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 2);
        Log.verbose("runFileOnTarget filename %s\n", filename.c_str());
        _commandSerial.startUploadFromFileSystem(fileSystemStr, filename, "ProgramAndReset");
    }

    void apiQueryStatus(const String &reqStr, String &respStr)
    {
        Log.verbose("RestAPIBusRaider: apiQueryStatus %s\n", reqStr.c_str());
        respStr = machineInterface.getStatus();
    }
    
    void apiQueryESPHealth(const String &reqStr, String &respStr)
    {
        Log.verbose("RestAPIBusRaider: queryESPHealth %s\n", reqStr.c_str());
        String healthStr;
        RestAPISystem::reportHealth(0, NULL, &healthStr);
        respStr = "\"espHealth\":{" + healthStr + "}";
    }

    void apiESPFirmwarePart(String filename, size_t contentLen, size_t index, 
                    uint8_t *data, size_t len, bool finalBlock)
    {
        Log.trace("apiESPFirmwarePart %d, %d, %d, %d\n", contentLen, index, len, finalBlock);
        // Check if first part
        if (index == 0)
        {
            // Abort any existing update process
            Update.abort();

            // Check the update can be started
            bool enoughSpace = Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH);
            if (!enoughSpace)
            {
                Log.notice("OTAUpdate: Not enough space for firmware\n");
                return;
            }
        }
        // Check if in progress
        if (Update.isRunning())
        {
            bool bytesWritten = Update.write(data, len);
            if (bytesWritten != len)
            {
                Log.warning("apiESPFirmwarePart failed to write correct amount %d != %d\n",
                            bytesWritten, len);
            }
        }
        // Check if final block
        if (finalBlock)
        {
            if (Update.isFinished())
            {
                Log.warning("apiESPFirmwarePart finished - rebooting ...\n");
                Update.end();
                _restartPendingStartMs = millis();
                _restartPending = true;
            }
            else
            {
                Log.warning("apiESPFirmwarePart final block but not finished - abort!\n");
                Update.abort();
            }
        }
    }

    void apiESPFirmwareUpdateDone(String &reqStr, String &respStr)
    {
        Log.trace("RestAPIBusRaider: apiESPFirmwareUpdate %s\n", reqStr.c_str());
        Utils::setJsonBoolResult(respStr, true);
    }
    
    void setup(RestAPIEndpoints &endpoints)
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
                                    std::placeholders::_5, std::placeholders::_6));                                    
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
                                    std::placeholders::_5, std::placeholders::_6));                                    
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
                                    std::placeholders::_5, std::placeholders::_6));                                    
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
                                    std::placeholders::_5, std::placeholders::_6));
    }

};
