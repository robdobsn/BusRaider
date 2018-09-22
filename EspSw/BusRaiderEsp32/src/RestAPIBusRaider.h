// REST API for System, WiFi, etc
// Rob Dobson 2012-2018

#pragma once
#include <Arduino.h>
#include "RestAPIEndpoints.h"
#include "CommandSerial.h"
#include "MachineInterface.h"

// #define SUPPORT_WEB_TERMINAL_REST 1

class RestAPIBusRaider
{
  private:
    CommandSerial& _commandSerial;
    MachineInterface& _machineInterface;
    static const int TIME_TO_WAIT_BEFORE_RESTART_MS = 1000;
    uint32_t _restartPendingStartMs;
    bool _restartPending;

  public:
    RestAPIBusRaider(CommandSerial &commandSerial, MachineInterface &machineInterface) :
             _commandSerial(commandSerial), _machineInterface(machineInterface)
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

    void apiFileComplete(String &reqStr, String &respStr)
    {
        Log.trace("RestAPIBusRaider: apiFileComplete %s\n", reqStr.c_str());
        Utils::setJsonBoolResult(respStr, true);
    }

    void apiFileUpload(String filename, size_t contentLen, size_t index, 
                    uint8_t *data, size_t len, bool final)
    {
        Log.notice("apiUp %d, %d, %d, %d\n", contentLen, index, len, final);
        if (contentLen > 0)
            commandSerial.fileUploadPart(filename, contentLen, index, data, len, final);
    }

    void apiQueryStatus(String &reqStr, String &respStr)
    {
        Log.trace("RestAPIBusRaider: apiQueryStatus %s\n", reqStr.c_str());
        respStr = machineInterface.getStatus();
    }
    
    void apiESPFirmwarePart(String filename, size_t contentLen, size_t index, 
                    uint8_t *data, size_t len, bool final)
    {
        Log.notice("apiESPFirmwarePart %d, %d, %d, %d\n", contentLen, index, len, final);
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
        if (final)
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
        endpoints.addEndpoint("upload", 
                            RestAPIEndpointDef::ENDPOINT_CALLBACK, 
                            RestAPIEndpointDef::ENDPOINT_POST,
                            std::bind(&RestAPIBusRaider::apiFileComplete, this, 
                                    std::placeholders::_1, std::placeholders::_2),
                            "Upload file", "application/json", 
                            NULL, 
                            true, 
                            NULL,
                            NULL,
                            std::bind(&RestAPIBusRaider::apiFileUpload, this, 
                                    std::placeholders::_1, std::placeholders::_2, 
                                    std::placeholders::_3, std::placeholders::_4,
                                    std::placeholders::_5, std::placeholders::_6));                                    
        endpoints.addEndpoint("querystatus", 
                            RestAPIEndpointDef::ENDPOINT_CALLBACK, 
                            RestAPIEndpointDef::ENDPOINT_GET, 
                            std::bind(&RestAPIBusRaider::apiQueryStatus, this,
                                    std::placeholders::_1, std::placeholders::_2),
                            "Query status");
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
