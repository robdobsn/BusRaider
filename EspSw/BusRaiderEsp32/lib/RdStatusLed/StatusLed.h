// Status LED
// Rob Dobson 2018

#pragma once

#include "Arduino.h"
#include "Utils.h"
#include "ConfigBase.h"
#include "ConfigPinMap.h"

class StatusLed
{
  public:
    StatusLed()
    {
        _isSetup = false;
        _ledPin = -1;
    }
    void setup(ConfigBase& hwConfig, const char* ledName);
    void setCode(int code);
    void service();

  private:
    bool _isSetup;
    int _ledPin;
    int _ledCode;
    int _curCodePos;
    bool _ledIsOn;
    uint32_t _ledLastMs;
    uint32_t _ledOnMs;
    uint32_t _ledLongOffMs;
    uint32_t _ledShortOffMs;
};