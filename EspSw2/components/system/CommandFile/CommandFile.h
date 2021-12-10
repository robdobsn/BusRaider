/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CommandFile
//
// Rob Dobson and Declan Shafi 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ConfigBase.h>
#include <RestAPIEndpointManager.h>
#include <SysModBase.h>

class CommandFile : public SysModBase
{
public:
    // Constructor/destructor
    CommandFile(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig);
    virtual ~CommandFile();

protected:
    // Setup
    virtual void setup() override final;

    // Service - called frequently
    virtual void service() override final;

    // Add endpoints
    virtual void addRestAPIEndpoints(RestAPIEndpointManager &endpointManager) override final;

private:
    // Helpers
    void applySetup();
    void apiFileRun(const String &reqStr, String& respStr, const APISourceInfo& sourceInfo);

    //API Processing
    bool handleAPIFile(String& fileName);
    bool processAPILine();
    bool exec();

    //State control
    enum State
    {
        API_STATE_PROCESSING,
        API_STATE_SLEEPING,
        API_STATE_IDLE
    };

    State _curState;

    //Commands
    String _APICode;
    int _curLine;
    int _curPosition;
    int _repsLeft = 0;
    String _curCommand;

    static const int MAX_API_FILE_LENGTH = 5000;

    RestAPIEndpointManager* _pRestAPIEndpointManager;

    //Sleep
    long _stateTimerMilis;
    int _sleepTimeMilis;
};

