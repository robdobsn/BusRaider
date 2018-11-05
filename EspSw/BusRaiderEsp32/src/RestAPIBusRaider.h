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

    void service();
    void apiTargetCommand(String &reqStr, String &respStr);

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
    void apiQueryESPHealth(const String &reqStr, String &respStr);
    void apiESPFirmwarePart(String& req, String& filename, size_t contentLen, size_t index, 
                    uint8_t *data, size_t len, bool finalBlock);
    void apiESPFirmwareUpdateDone(String &reqStr, String &respStr);
    void setup(RestAPIEndpoints &endpoints);

};
