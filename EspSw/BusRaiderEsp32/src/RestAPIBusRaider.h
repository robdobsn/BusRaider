// REST API for System, WiFi, etc
// Rob Dobson 2012-2018

#pragma once
#include <Arduino.h>
#include "RestAPIEndpoints.h"
#include "RestAPISystem.h"
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
    RestAPISystem& _restAPISystem;
    static const int TIME_TO_WAIT_BEFORE_RESTART_MS = 1000;
    uint32_t _restartPendingStartMs;
    bool _restartPending;

    // Config for machines
    ConfigNVS _machineConfig;

public:
    RestAPIBusRaider(CommandSerial &commandSerial, MachineInterface &machineInterface, 
            FileManager& fileManager, RestAPISystem& restAPISystem);

    void service();
    void apiTargetCommand(String &reqStr, String &respStr);
    void apiTargetCommandPost(String &reqStr, String &respStr);
    void apiTargetCommandPostContent(const String &reqStr, uint8_t *pData, size_t len, size_t index, size_t total);

#ifdef SUPPORT_WEB_TERMINAL_REST
    void apiPostCharsDone(String &reqStr, String &respStr);
    void apiPostCharsBody(uint8_t *data, size_t len, size_t index, size_t total);
#endif

    void apiUploadToFileManComplete(String &reqStr, String &respStr);
    void apiUploadToFileManPart(String& req, String& filename, size_t contentLen, size_t index, 
                    uint8_t *data, size_t len, bool finalBlock);

    void apiUploadPiSwComplete(String &reqStr, String &respStr);
    void apiUploadPiSwPart(String& req, String& filename, size_t contentLen, size_t index, 
                    uint8_t *data, size_t len, bool finalBlock);

    void apiUploadAppendComplete(String &reqStr, String &respStr);
    void apiUploadAppendPart(String& req, String& filename, size_t contentLen, size_t index, 
                    uint8_t *data, size_t len, bool finalBlock);

    void apiUploadAndRunComplete(String &reqStr, String &respStr);
    void apiUploadAndRunPart(String& req, String& filename, size_t contentLen, size_t index, 
                    uint8_t *data, size_t len, bool finalBlock);

    void apiSendFileToTargetBuffer(const String &reqStr, String &respStr);
    void apiAppendFileToTargetBuffer(const String &reqStr, String &respStr);

    void runFileOnTarget(const String &reqStr, String &respStr);
    void apiQueryStatus(const String &reqStr, String &respStr);
    void apiQueryCurMc(const String &reqStr, String &respStr);
    void apiSetMcJson(const String &reqStr, String &respStr);
    void apiSetMcJsonContent(const String &reqStr, uint8_t *pData, size_t len, size_t index, size_t total);
    void apiQueryESPHealth(const String &reqStr, String &respStr);
    void apiESPFirmwarePart(String& req, String& filename, size_t contentLen, size_t index, 
                    uint8_t *data, size_t len, bool finalBlock);
    void apiESPFirmwareUpdateDone(String &reqStr, String &respStr);

    // Setup endpoints
    void setup(RestAPIEndpoints &endpoints);

};
