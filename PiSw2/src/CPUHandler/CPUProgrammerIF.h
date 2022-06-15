// Bus Raider
// Rob Dobson 2022

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

class CPUProgrammerIF
{
public:
    virtual bool isProgrammingPending() = 0;
    virtual void programmingHasStarted() = 0;
    virtual void programmingWrite() = 0;
};
