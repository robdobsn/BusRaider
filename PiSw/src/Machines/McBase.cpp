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
    TargetDebug::get()->handleDebuggerCommand(pCommand, pResponse, maxResponseLen);
    return false;
}
