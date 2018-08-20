// REST API for System, WiFi, etc
// Rob Dobson 2012-2018

#pragma once
#include <Arduino.h>
#include "RestAPIEndpoints.h"
#include "PiComms.h"

class RestAPIBusRaider
{
  private:
    PiComms& _piComms;

  public:
    RestAPIBusRaider(PiComms &piComms) : _piComms(piComms)
    {
    }

    void service()
    {
    }

    void apiTargetCommand(String &reqStr, String &respStr)
    {
        bool rslt = false;
        // Get command
        String targetCmd = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 1);
        Log.trace("RestAPIBusRaider: Command %s\n", targetCmd.c_str());
        _piComms.sendTargetCommand(targetCmd);
        Utils::setJsonBoolResult(respStr, rslt);
    }

    void setup(RestAPIEndpoints &endpoints)
    {
        endpoints.addEndpoint("targetcmd", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                        std::bind(&RestAPIBusRaider::apiTargetCommand, this, std::placeholders::_1, std::placeholders::_2),
                        "Target command");
    }

};
