
#pragma once

#include "logging.h"
#include "PiWiring.h"
#include "lowlib.h"
#include <circle/bcm2835.h>
#include <circle/memio.h>

class TargetClockGenerator
{
  public:
    TargetClockGenerator();

    bool setup(int pin = -1);

    uint32_t getMinFreqHz()
    {
        return 1;
    }

    uint32_t getMaxFreqHz()
    {
        return 25000000;
    }

    void setFreqHz(uint32_t newFreq);

    void enable(bool en);

    uint32_t getFreqInHz()
    {
        return _freqReqd;
    }

private:
    uint32_t _freqReqd;
    bool _enabled;
    int _outputPin;
    int _altMode;
};