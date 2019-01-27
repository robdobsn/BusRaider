// Bus Raider
// Rob Dobson 2018
// Command handler

#pragma once
#include <functional>

typedef std::function<void(const char* machineName)> ChangeMachineFunction;

class CommandHandler
{
public:
    CommandHandler(ChangeMachineFunction changeMcFunction);

    ~CommandHandler();

private:
    ChangeMachineFunction _pChangeMcFunction;

};
