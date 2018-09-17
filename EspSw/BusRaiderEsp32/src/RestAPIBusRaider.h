// REST API for System, WiFi, etc
// Rob Dobson 2012-2018

#pragma once
#include <Arduino.h>
#include "RestAPIEndpoints.h"
#include "CommandSerial.h"

class RestAPIBusRaider
{
  private:
    CommandSerial& _commandSerial;

  public:
    RestAPIBusRaider(CommandSerial &commandSerial) : _commandSerial(commandSerial)
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
        // _commandSerial.sendTargetCommand(targetCmd);
        Utils::setJsonBoolResult(respStr, rslt);
    }

    void apiFileStart(String &reqStr, String &respStr)
    {
        // Log.trace("RestAPIBusRaider: apiFileStart %s\n", reqStr.c_str());
        Log.notice("apiFileStart\n");
        Utils::setJsonBoolResult(respStr, true);
    }

    void apiFileUpload(String filename, size_t contentLen, size_t index, 
                    uint8_t *data, size_t len, bool final)
    {
        Log.notice("apiUp %d, %d, %d, %d\n", contentLen, index, len, final);
        
        // if (request && (request->contentLength() > 0))
        //     commandSerial.fileUploadPart(filename, request->contentLength(), index, data, len, final);
        // String uploadResponseStr;
        // Utils::setJsonBoolResult(uploadResponseStr, true);
        // request->send(200, "application/json", _fileUploadResponseJson.c_str()
        // Log.trace("RestAPIBusRaider: apiFileUpload %s index %s len %d final %d\n", 
        //             filename.c_str(), index, len, final);
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
    }

};
