
#pragma once

#include "globaldefs.h"
#include "McBase.h"

class McManager
{
  public:

    static const int MAX_MACHINES = 10;
    static McBase* _pMachines[MAX_MACHINES];
    static int _numMachines;
    static int _curMachineIdx;

    static void add(McBase* pMachine);

    static void setMachineIdx(int mcIdx)
    {
        _curMachineIdx = mcIdx;
    }

    static McBase* getMachine()
    {
        if (_curMachineIdx >= _numMachines)
            return NULL;
        return _pMachines[_curMachineIdx];
    }

};

