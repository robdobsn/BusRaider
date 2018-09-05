// Bus Raider Machine Manager
// Rob Dobson 2018

#pragma once

#include "globaldefs.h"
#include "McBase.h"
#include "TargetClockGenerator.h"
#include "ee_printf.h"

class McManager
{
  private:
    static McDescriptorTable defaultDescriptorTable;
  public:

    static const int MAX_MACHINES = 10;
    static McBase* _pMachines[MAX_MACHINES];
    static int _numMachines;
    static int _curMachineIdx;
    static TargetClockGenerator _clockGen;

    static void add(McBase* pMachine);
    static bool setMachineIdx(int mcIdx);
    static bool setMachineByName(const char* mcName);

    static int getNumMachines()
    {
        return _numMachines;
    }

    static McBase* getMachine()
    {
        if ((_curMachineIdx < 0) || (_curMachineIdx >= _numMachines))
            return NULL;
        return _pMachines[_curMachineIdx];
    }

    static McDescriptorTable* getDescriptorTable(int subType)
    {
        McBase* pMc = getMachine();
        if (pMc)
            return pMc->getDescriptorTable(subType);
        return &defaultDescriptorTable;
    }

};

