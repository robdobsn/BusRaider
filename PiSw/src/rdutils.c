// Bus Raider
// Rob Dobson 2018

#include "rdutils.h"
#include "globaldefs.h"

int timer_isTimeout(unsigned long curTime, unsigned long lastTime, unsigned long maxDuration)
{
    if (curTime >= lastTime) {
        return (curTime > lastTime + maxDuration);
    }
    return (ULONG_MAX - (lastTime - curTime) > maxDuration);
}
