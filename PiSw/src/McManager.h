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
    static const int MAX_MACHINE_NAME_LEN = 100;
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
        {
            if (_numMachines > 0)
                return _pMachines[0];
            return NULL;
        }
        return _pMachines[_curMachineIdx];
    }

    static McDescriptorTable* getDescriptorTable(int subType)
    {
        McBase* pMc = getMachine();
        if (pMc)
            return pMc->getDescriptorTable(subType);
        return &defaultDescriptorTable;
    }

    static const char* getMachineJSON()
    {
        static const int MAX_MC_JSON_LEN = MAX_MACHINES * (MAX_MACHINE_NAME_LEN + 10) + 20;
        static char mcString[MAX_MC_JSON_LEN+1];

        // Machine list
        strncpy(mcString, "\"machineList\":[", MAX_MC_JSON_LEN);
        for (int i = 0; i < getNumMachines(); i++)
        {
            if (i != 0)
                strncpy(mcString+strlen(mcString),",", MAX_MC_JSON_LEN);
            strncpy(mcString+strlen(mcString),"\"", MAX_MC_JSON_LEN);
            strncpy(mcString+strlen(mcString), _pMachines[i]->getDescriptorTable(0)->machineName, MAX_MC_JSON_LEN);
            strncpy(mcString+strlen(mcString),"\"", MAX_MC_JSON_LEN);
        }
        strncpy(mcString+strlen(mcString),"]", MAX_MC_JSON_LEN);

        // Current machine
        strncpy(mcString+strlen(mcString),",\"machineCur\":", MAX_MC_JSON_LEN);
        strncpy(mcString+strlen(mcString), "\"", MAX_MC_JSON_LEN);
        if ((_curMachineIdx >= 0) && (_curMachineIdx < _numMachines))
            strncpy(mcString+strlen(mcString), getDescriptorTable(_curMachineIdx)->machineName, MAX_MC_JSON_LEN);
        strncpy(mcString+strlen(mcString), "\"", MAX_MC_JSON_LEN);

        // Ret
        return mcString;
    }

};

