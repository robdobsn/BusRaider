// Bus Raider Machine Base Class
// Rob Dobson 2018

#include "McBase.h"
#include "McManager.h"

McBase::McBase()
{
    McManager::add(this);
}

bool McBase::debuggerCommand(char* pCommand, char* pResponse, int maxResponseLen)
{
    TargetDebug* pDebug = TargetDebug::get();
    // LogWrite("McBase", LOG_DEBUG, "debuggerCommand %s %d %d\n", pCommand, 
    //         maxResponseLen, pDebug);
    if (pDebug)
        return pDebug->debuggerCommand(pCommand, pResponse, maxResponseLen);
    return false;
}
