// Bus Raider Machine Base Class
// Rob Dobson 2018

#include "McBase.h"
#include "McManager.h"

McBase::McBase()
{
    McManager::add(this);
}
