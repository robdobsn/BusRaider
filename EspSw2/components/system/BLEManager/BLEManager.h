/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// BLEManager
// Handles BLE connectivity and data
//
// RIC Firmware
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ConfigBase.h>
#include <SysModBase.h>
#include "host/ble_uuid.h"
#include <RingBufferRTOS.h>
#include <ProtocolRawMsg.h>

#define USE_TIMED_ADVERTISING_CHECK 1

class ProtocolEndpointMsg;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BLEManager
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class BLEManager : public SysModBase
{
public:
    BLEManager(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig);
    virtual ~BLEManager();

protected:
    // Setup
    virtual void setup() override final;

    // Service - called frequently
    virtual void service() override final;

    // Add endpoints
    virtual void addRestAPIEndpoints(RestAPIEndpointManager& endpointManager) override final;

    // Add protocol endpoints
    virtual void addProtocolEndpoints(ProtocolEndpointManager& endpointManager) override final;

    // Get status JSON
    virtual String getStatusJSON() override final;

    // Get debug string
    virtual String getDebugStr() override final;

private:
    // Singleton BLEManager
    static BLEManager* _pBLEManager;

    // BLE enabled
    bool _enableBLE;

    // BLE device initialised
    bool _BLEDeviceInitialised;

    // Addr type
    static uint8_t own_addr_type;

    // BLE advertising service UUIDs
    static ble_uuid128_t BLE_RICV2_ADVERTISING_UUID;

    // EndpointID used to identify this message channel to the ProtocolEndpointManager object
    uint32_t _protocolEndpointID;

    // Status
    static bool _isConnected;
    static uint16_t _bleGapConnHandle;

    // Max packet length - seems to be OS dependent (iOS seems to truncate at 182?)
    static const uint32_t MAX_BLE_PACKET_LEN_DEFAULT = 180;
    uint32_t _maxPacketLength;
    
    // Outbound message queue (
    static const int BLE_OUTBOUND_MSG_QUEUE_SIZE = 30;
    RingBufferRTOS<ProtocolRawMsg, BLE_OUTBOUND_MSG_QUEUE_SIZE> _bleOutboundQueue;

    // Min time between adjacent outbound messages
    static const uint32_t BLE_MIN_TIME_BETWEEN_OUTBOUND_MSGS_MS = 25;
    uint32_t _lastOutboundMsgMs;

    // Stats
    uint32_t _rxTotalCount;
    uint32_t _txTotalCount;

#ifdef USE_TIMED_ADVERTISING_CHECK
    // Advertising check timeout
    static bool _advertisingCheckRequired;
    static uint32_t _advertisingCheckMs;
    static const uint32_t ADVERTISING_CHECK_MS = 3000;
#endif

    // Helpers
    void applySetup();
    void onSync();
    static void startAdvertising();
    static int nimbleGapEvent(struct ble_gap_event *event, void *arg);
    static void bleHostTask(void *param);
    static void logConnectionInfo(struct ble_gap_conn_desc *desc);
    static void print_addr(const uint8_t *addr);
    static void gattAccessCallbackStatic(const char* characteristicName, bool readOp, const uint8_t *payloadbuffer, int payloadlength);
    void gattAccessCallback(const char* characteristicName, bool readOp, const uint8_t *payloadbuffer, int payloadlength);
    bool sendBLEMsg(ProtocolEndpointMsg& msg);
    bool readyToSend();
    static void setIsConnected(bool isConnected, uint16_t connHandle = 0);
    static String getAdvertisingName();

};
