
#include "McBase.h"
#include "McManager.h"

McBase::McBase()
{
    McManager::add(this);
}
