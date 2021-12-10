/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// StatePublisher
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <WString.h>
#include <list>
#include <vector>
#include <APISourceInfo.h>
#include <SysModBase.h>

class ConfigBase;
class RobotController;
class ProtocolEndpointManager;
class RICRESTMsg;

// #define DEBUG_STATEPUB_OUTPUT_PUBLISH_STATS 1

class StatePublisher : public SysModBase
{
public:
    // Constructor
    StatePublisher(const char* pModuleName, ConfigBase& defaultConfig, ConfigBase* pGlobalConfig, ConfigBase* pMutableConfig);
    ~StatePublisher();

    // Subscription API
    void apiSubscription(String &reqStr, String& respStr, const APISourceInfo& sourceInfo);

    // Debug
    virtual String getDebugJSON() override final;

    // Receive msg generator callback function
    virtual void receiveMsgGenCB(const char* msgGenID, SysMod_publishMsgGenFn msgGenCB, SysMod_stateDetectCB stateDetectCB) override final;

protected:
    // Setup
    virtual void setup() override final;

    // Service - called frequently
    virtual void service() override final;

    // Add endpoints
    virtual void addRestAPIEndpoints(RestAPIEndpointManager& endpointManager) override final;
    
    // Add protocol endpoints
    virtual void addProtocolEndpoints(ProtocolEndpointManager &endpointManager) override final;

private:
    enum TriggerType_t
    {
        TRIGGER_AT_TIME_INTERVALS
    };

    static const int32_t PUBLISHING_HANDLE_UNDEFINED = -1;
    static const uint32_t REDUCED_PUB_RATE_WHEN_BUSY_MS = 1000;

    class InterfaceRateRec
    {
    public:
        InterfaceRateRec()
        {
            _rateHz = 1.0;
            _betweenPubsMs = 0;
            _lastPublishMs = 0;
            _channelID = PUBLISHING_HANDLE_UNDEFINED;
        }
        void setRateHz(double rateHz)
        {
            _rateHz = rateHz;
            if (_rateHz == 0)
                _betweenPubsMs = 0;
            else
                _betweenPubsMs = 1000/rateHz;
            // Fix between pubs ms as isTimout check has an average of 1ms more than requested
            if (_betweenPubsMs > 9)
                _betweenPubsMs--;
        }
        String _interface;
        String _protocol;
        double _rateHz;
        uint32_t _betweenPubsMs;
        uint32_t _lastPublishMs;
        int32_t _channelID;
    };

    // Publication records
    class PubRec
    {
    public:
        PubRec()
        {
            _trigger = TRIGGER_AT_TIME_INTERVALS;
            _msgGenFn = nullptr;
            _stateDetectFn = nullptr;
        }
        String _name;
        TriggerType_t _trigger;
        String _msgIDStr;
        SysMod_publishMsgGenFn _msgGenFn;
        SysMod_stateDetectCB _stateDetectFn;
        std::list<InterfaceRateRec> _interfaceRates;

        // This is used by _stateDetectFn callback for state change
        // detection information - the state change is detected by
        // the comparing the returned value from the callback with
        // the previous hash value
        std::vector<uint8_t> _stateHash;
    };
    std::list<PubRec> _publicationRecs;

#ifdef DEBUG_STATEPUB_OUTPUT_PUBLISH_STATS
    // Stats
    uint64_t _recentWorstTimeUs;
    uint32_t _worstTimeSetMs;
#endif

    // Endpoint manager
    ProtocolEndpointManager* _pProtocolEndpointManager;

    // Helpers
    void cleanUp();
    bool publishData(PubRec& pubRec, InterfaceRateRec& rateRec);
};
