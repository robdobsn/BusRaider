// REST API for System, WiFi, etc
// Rob Dobson 2012-2018

#pragma once
#include <Arduino.h>
#include "RestAPIEndpoints.h"
#include "CommandSerial.h"
#include "MachineInterface.h"

class RestAPIBusRaider
{
  private:
    CommandSerial& _commandSerial;
    MachineInterface& _machineInterface;

  public:
    RestAPIBusRaider(CommandSerial &commandSerial, MachineInterface &machineInterface) :
             _commandSerial(commandSerial), _machineInterface(machineInterface)
    {
    }

    void service()
    {
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

    void apiFileStart(String &reqStr, String &respStr)
    {
        Log.trace("RestAPIBusRaider: apiFileStart %s\n", reqStr.c_str());
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

    void setup(RestAPIEndpoints &endpoints)
    {
        endpoints.addEndpoint("targetcmd", 
                            RestAPIEndpointDef::ENDPOINT_CALLBACK, 
                            RestAPIEndpointDef::ENDPOINT_GET, 
                            std::bind(&RestAPIBusRaider::apiTargetCommand, this,
                                    std::placeholders::_1, std::placeholders::_2),
                            "Target command");
        endpoints.addEndpoint("upload", 
                            RestAPIEndpointDef::ENDPOINT_CALLBACK, 
                            RestAPIEndpointDef::ENDPOINT_POST,
                            std::bind(&RestAPIBusRaider::apiFileStart, this, 
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
    }

};
