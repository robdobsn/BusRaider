/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// BLEManager
// Handles BLE connectivity and data
//
// RIC Firmware
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Logger.h>
#include "BLEManager.h"
#include "BLEGattServer.h"
#include "RestAPIEndpointManager.h"
#include "ProtocolEndpointManager.h"
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include <Utils.h>
#include <ESPUtils.h>
#include <SysManager.h>

// #define DEBUG_BLE_RX_PAYLOAD 1
// #define DEBUG_BLE_TX_MSG 1
// #define DEBUG_BLE_TX_MSG_SPLIT 1
// #define DEBUG_BLE_TX_MSG_DETAIL 1
// #define DEBUG_BLE_PUBLISH 1

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Statics, etc
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Log prefix
static const char *MODULE_PREFIX = "BLEManager";

// NOTE: this is the main service in BLEGattServer
ble_uuid128_t BLEManager::BLE_RICV2_ADVERTISING_UUID = BLEGattServer::GATT_RICV2_MAIN_SERVICE_UUID;

// Singleton BLEManager
BLEManager *BLEManager::_pBLEManager = NULL;

// Address type
uint8_t BLEManager::own_addr_type = 0;

// Is connected
bool BLEManager::_isConnected = false;
uint16_t BLEManager::_bleGapConnHandle = 0;

#ifdef USE_TIMED_ADVERTISING_CHECK
// Advertising check ms
bool BLEManager::_advertisingCheckRequired = false;
uint32_t BLEManager::_advertisingCheckMs = millis();
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BLEManager::BLEManager(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig)
    : SysModBase(pModuleName, defaultConfig, pGlobalConfig, pMutableConfig)
{
    // BLE interface
    _enableBLE = false;
    _BLEDeviceInitialised = false;

    // EndpointID
    _protocolEndpointID = ProtocolEndpointManager::UNDEFINED_ID;

    // Singleton
    _pBLEManager = this;

    // Stats
    _rxTotalCount = 0;
    _txTotalCount = 0;

    // Outbound msg timing
    _lastOutboundMsgMs = 0;
}

BLEManager::~BLEManager()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BLEManager::setup()
{
    applySetup();
}

void BLEManager::applySetup()
{
    // See if BLE enabled
    _enableBLE = (configGetLong("enable", 0) != 0);
    LOG_D(MODULE_PREFIX, "applySetup BLE enabled");

    // Setup if enabled
    if (_enableBLE)
    {
        // Get max packet length
        _maxPacketLength = configGetLong("maxPktLen", MAX_BLE_PACKET_LEN_DEFAULT);

        // Init Nimble
        if (!_BLEDeviceInitialised)
        {
            esp_err_t err = esp_nimble_hci_and_controller_init();
            if (err != ESP_OK)
            {
                LOG_E(MODULE_PREFIX, "applySetup nimble init failed err=%d", err);
                return;
            }
            nimble_port_init();
        }
        _BLEDeviceInitialised = true;

        // onReset callback
        ble_hs_cfg.reset_cb = [](int reason) {
            LOG_D(MODULE_PREFIX, "onReset() reason=%d", reason);
        };

        // onSync callback
        ble_hs_cfg.sync_cb = []() {
            if (_pBLEManager)
                _pBLEManager->onSync();
        };

        ble_hs_cfg.gatts_register_cb = BLEGattServer::registrationCallback;
        ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

        // Not really explained here
        // https://microchipdeveloper.com/wireless:ble-gap-security
        ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_KEYBOARD_DISP;
        ble_hs_cfg.sm_sc = 0;

        int rc = BLEGattServer::initServer();
        assert(rc == 0);

        // Set the advertising name
        rc = ble_svc_gap_device_name_set(getAdvertisingName().c_str());
        assert(rc == 0);

        // Set the callback
        BLEGattServer::setServerAccessCB(gattAccessCallbackStatic);

        // Start the host task
        nimble_port_freertos_init(bleHostTask);

        // Currently disconnected
        setIsConnected(false);

        // Debug
        LOG_I(MODULE_PREFIX, "applySetup maxPktLen %d", _maxPacketLength);
    }
    else if (_BLEDeviceInitialised)
    {
        // Deinit GATTServer
        BLEGattServer::deinitServer();

        // Remove callbacks etc
        ble_hs_cfg.store_status_cb = NULL;
        ble_hs_cfg.gatts_register_cb  = NULL;
        ble_hs_cfg.sync_cb = NULL;
        ble_hs_cfg.reset_cb = NULL;

        // Deinitialize
        nimble_port_deinit();
        esp_err_t err = esp_nimble_hci_and_controller_deinit();
        if (err != ESP_OK)
        {
            LOG_W(MODULE_PREFIX, "applySetup deinit failed");
        }
        _BLEDeviceInitialised = false;

        // Debug
        LOG_I(MODULE_PREFIX, "applySetup de-init ok");
    }

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BLEManager::service()
{
    // Check we are initialised
    if (!_BLEDeviceInitialised)
        return;

#ifdef USE_TIMED_ADVERTISING_CHECK
    // Handle advertising check
    if ((!_isConnected) && (_advertisingCheckRequired))
    {
        if (Utils::isTimeout(millis(), _advertisingCheckMs, ADVERTISING_CHECK_MS))
        {
            // Check advertising
            if (!ble_gap_adv_active())
            {
                // Debug
                LOG_W(MODULE_PREFIX, "service not conn or adv so start advertising");

                // Start advertising
                startAdvertising();
            }
            else
            {
                LOG_I(MODULE_PREFIX, "service BLE advertising check ok");
                _advertisingCheckRequired = false;
            }
            _advertisingCheckMs = millis();
        }
    }
#endif

    // Handle outbound message queue
    if (_bleOutboundQueue.count() > 0)
    {
        if (Utils::isTimeout(millis(), _lastOutboundMsgMs, BLE_MIN_TIME_BETWEEN_OUTBOUND_MSGS_MS))
        {
            ProtocolRawMsg bleOutMsg;
            if (_bleOutboundQueue.get(bleOutMsg))
                BLEGattServer::sendToCentral(bleOutMsg.getBuf(), bleOutMsg.getBufLen());
            _lastOutboundMsgMs = millis();
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Endpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BLEManager::addRestAPIEndpoints(RestAPIEndpointManager &endpointManager)
{
}

void BLEManager::addProtocolEndpoints(ProtocolEndpointManager& endpointManager)
{
    // Register as a channel of protocol messages
    _protocolEndpointID = endpointManager.registerChannel("RICSerial", 
            std::bind(&BLEManager::sendBLEMsg, this, std::placeholders::_1),
            "BLE",
            std::bind(&BLEManager::readyToSend, this));
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get status JSON
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
String BLEManager::getStatusJSON()
{
    int8_t rssiOut = 0;
    int bleGapConnRSSI = ble_gap_conn_rssi(_bleGapConnHandle, &rssiOut);
    char statusStr[200];
    snprintf(statusStr, sizeof(statusStr), 
                "{\"rslt\":\"ok\",\"isConn\":%d,\"isAdv\":%d,\"advName\":\"%s\",\"rssi\":%d,\"BLEMAC\":\"%s\"}", 
                _isConnected,
                ble_gap_adv_active(),
                ble_svc_gap_device_name(),
                bleGapConnRSSI == 0 ? rssiOut : 0,
                getSystemMACAddressStr(ESP_MAC_BT, ":").c_str()
                );
    return statusStr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Debug
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String BLEManager::getDebugStr()
{
    bool advertisingActive = ble_gap_adv_active();
    bool discoveryActive = ble_gap_disc_active();
    bool gapConnActive = ble_gap_conn_active();
    int8_t rssiOut = 0;
    char tmpRssi[20];
    strlcpy(tmpRssi, "", sizeof(tmpRssi));
    if (_isConnected)
    {
        int bleGapConnRSSI = ble_gap_conn_rssi(_bleGapConnHandle, &rssiOut);
        if (bleGapConnRSSI == 0)
            snprintf(tmpRssi, sizeof(tmpRssi), "%ddB", rssiOut);
    }
    // Also ble_gap_conn_desc can be obtained
    String advertisingName = " ";
    advertisingName += ble_svc_gap_device_name();
    char tmpBuf[200];
    snprintf(tmpBuf, sizeof(tmpBuf)-1, "BLE%s%s%s%s%s Rx%d Tx%d",
        _isConnected ? " Conn" : (advertisingActive ? "Adv" : ""),
        gapConnActive ? " Actv" : "",
        advertisingActive ? advertisingName.c_str() : "",
        discoveryActive ? " Discov" : "",
        tmpRssi,
        _rxTotalCount,
        _txTotalCount);

    return tmpBuf;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// onSync callback
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BLEManager::onSync()
{
    // Validate
    int rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

    // Figure out address to use while advertising (no privacy for now)
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0)
    {
        LOG_W(MODULE_PREFIX, "onSync() error determining address type; rc=%d", rc);
        return;
    }

    // Debug showing addr
    uint8_t addrVal[6] = {0};
    rc = ble_hs_id_copy_addr(own_addr_type, addrVal, NULL);
    LOG_I(MODULE_PREFIX, "onSync() Device Address: %x:%x:%x:%x:%x:%x",
              addrVal[5], addrVal[4], addrVal[3], addrVal[2], addrVal[1], addrVal[0]);

    // Start advertising
    startAdvertising();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// startAdvertising
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Enables advertising with the following parameters:
 *     o General discoverable mode.
 *     o Undirected connectable mode.
 */
void BLEManager::startAdvertising()
{
    // Check if already advertising
    if (ble_gap_adv_active())
    {
        LOG_I(MODULE_PREFIX, "startAdv - already adv");
        return;
    }

    LOG_I(MODULE_PREFIX, "startAdv Entry");
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;

    /**
     *  Set the advertisement data included in our advertisements:
     *     o Flags (indicates advertisement type and other general info).
     *     o Advertising tx power.
     *     o Device name.
     *     o 128 bit UUID
     */

    memset(&fields, 0, sizeof fields);

    /* Advertise two flags:
     *     o Discoverability in forthcoming advertisement (general)
     *     o BLE-only (BR/EDR unsupported).
     */
    fields.flags = BLE_HS_ADV_F_DISC_GEN |
                   BLE_HS_ADV_F_BREDR_UNSUP;

    /* Indicate that the TX power level field should be included; have the
     * stack fill this value automatically.  This is done by assigning the
     * special value BLE_HS_ADV_TX_PWR_LVL_AUTO.
     */
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    fields.uuids128 = &BLE_RICV2_ADVERTISING_UUID;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0)
    {
        LOG_E(MODULE_PREFIX, "error setting adv fields; rc=%d", rc);
        return;
    }

    // Clear fields
    memset(&fields, 0, sizeof fields);

    // Set the advertising name
    if (ble_svc_gap_device_name_set(getAdvertisingName().c_str()) != 0)
    {
        LOG_E(MODULE_PREFIX, "error setting adv name rc=%d", rc);
    }

    const char *name = ble_svc_gap_device_name();
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    rc = ble_gap_adv_rsp_set_fields(&fields);
    if (rc != 0)
    {
        LOG_E(MODULE_PREFIX, "error setting adv rsp fields; rc=%d", rc);
        return;
    }

    /* Begin advertising. */
    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, nimbleGapEvent, NULL);
    if (rc != 0)
    {
        LOG_E(MODULE_PREFIX, "error enabling adv; rc=%d", rc);
        return;
    }

    LOG_I(MODULE_PREFIX, "startAdv Complete");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BLEManager::print_addr(const uint8_t *addr)
{
    const uint8_t *u8p;

    u8p = addr;
    LOG_I(MODULE_PREFIX, "%02x:%02x:%02x:%02x:%02x:%02x",
                u8p[5], u8p[4], u8p[3], u8p[2], u8p[1], u8p[0]);
}
/**
 * Logs information about a connection to the console.
 */
void BLEManager::logConnectionInfo(struct ble_gap_conn_desc *desc)
{
    LOG_I(MODULE_PREFIX, "handle=%d our_ota_addr_type=%d our_ota_addr=",
                desc->conn_handle, desc->our_ota_addr.type);
    print_addr(desc->our_ota_addr.val);
    LOG_I(MODULE_PREFIX, " our_id_addr_type=%d our_id_addr=",
                desc->our_id_addr.type);
    print_addr(desc->our_id_addr.val);
    LOG_I(MODULE_PREFIX, " peer_ota_addr_type=%d peer_ota_addr=",
                desc->peer_ota_addr.type);
    print_addr(desc->peer_ota_addr.val);
    LOG_I(MODULE_PREFIX, " peer_id_addr_type=%d peer_id_addr=",
                desc->peer_id_addr.type);
    print_addr(desc->peer_id_addr.val);
    LOG_I(MODULE_PREFIX, " conn_itvl=%d conn_latency=%d supervision_timeout=%d "
                      "encrypted=%d authenticated=%d bonded=%d",
                desc->conn_itvl, desc->conn_latency,
                desc->supervision_timeout,
                desc->sec_state.encrypted,
                desc->sec_state.authenticated,
                desc->sec_state.bonded);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GAP event
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * The nimble host executes this callback when a GAP event occurs.  The
 * application associates a GAP event callback with each connection that forms.
 * The same callback is used for all connections.
 *
 * @param event                 The type of event being signalled.
 * @param ctxt                  Various information pertaining to the event.
 * @param arg                   Application-specified argument; unused
 *
 * @return                      0 if the application successfully handled the
 *                                  event; nonzero on failure.  The semantics
 *                                  of the return code is specific to the
 *                                  particular GAP event being signalled.
 */
int BLEManager::nimbleGapEvent(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;

    switch (event->type)
    {
        case BLE_GAP_EVENT_CONNECT:
        {
            // A new connection was established or a connection attempt failed
            LOG_W(MODULE_PREFIX, "GAPEvent connection %s (%d) ",
                        event->connect.status == 0 ? "established" : "failed",
                        event->connect.status);
            if (event->connect.status == 0)
            {
                setIsConnected(true, event->connect.conn_handle);
                int rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
                assert(rc == 0);
                logConnectionInfo(&desc);
            }
            else
            {
                setIsConnected(false);
                // // Connection failed; resume advertising
                LOG_W(MODULE_PREFIX, "GAPEvent connection failed - request resume advertising");
                startAdvertising();

            }
            return 0;
        }
        case BLE_GAP_EVENT_DISCONNECT:
        {
            LOG_W(MODULE_PREFIX, "GAPEvent disconnect; reason=%d ", event->disconnect.reason);
            setIsConnected(false);
            logConnectionInfo(&event->disconnect.conn);

            // Connection terminated; resume advertising
            // LOG_W(MODULE_PREFIX, "GAPEvent disconnect - requesting startAdvertising");
            // startAdvertising();
            return 0;
        }
        case BLE_GAP_EVENT_CONN_UPDATE:
        { // The central has updated the connection parameters
            LOG_W(MODULE_PREFIX, "GAPEvent connection updated; status=%d ",
                        event->conn_update.status);
            int rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
            assert(rc == 0);
            logConnectionInfo(&desc);
            return 0;
        }
        case BLE_GAP_EVENT_CONN_UPDATE_REQ:
        {
            LOG_W(MODULE_PREFIX, "GAPEvent connection update request");
            return 0;
        }
        case BLE_GAP_EVENT_L2CAP_UPDATE_REQ:
        {
            LOG_W(MODULE_PREFIX, "GAPEvent L2CAP update request");
            return 0;
        }
        case BLE_GAP_EVENT_TERM_FAILURE:
        {
            LOG_W(MODULE_PREFIX, "GAPEvent term failure");
            return 0;
        }
        case BLE_GAP_EVENT_DISC:
        {
            LOG_W(MODULE_PREFIX, "GAPEvent DISC");
            return 0;
        }
        case BLE_GAP_EVENT_DISC_COMPLETE:
        {
            LOG_W(MODULE_PREFIX, "GAPEvent DISC COMPLETE");
            return 0;
        }
        case BLE_GAP_EVENT_ADV_COMPLETE:
        {
            LOG_W(MODULE_PREFIX, "GAPEvent advertise complete; reason=%d",
                        event->adv_complete.reason);
            startAdvertising();
            return 0;
        }
        case BLE_GAP_EVENT_ENC_CHANGE:
        {
            // Encryption has been enabled or disabled for this connection
            LOG_W(MODULE_PREFIX, "GAPEvent encryption change; status=%d ",
                        event->enc_change.status);
            int rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
            assert(rc == 0);
            logConnectionInfo(&desc);
            return 0;
        }
        case BLE_GAP_EVENT_PASSKEY_ACTION:
        {
            LOG_W(MODULE_PREFIX, "GAPEvent PASSKEY action");
            return 0;
        }
        case BLE_GAP_EVENT_NOTIFY_RX:
        {
            LOG_W(MODULE_PREFIX, "GAPEvent notify RX");
            return 0;
        }
        case BLE_GAP_EVENT_NOTIFY_TX:
        {
            _pBLEManager->_txTotalCount++;
            // LOG_I(MODULE_PREFIX, "GAPEvent notify TX");
            // Every transmission from GAP - could be used for stats if other stats not available
            return 0;
        }
        case BLE_GAP_EVENT_SUBSCRIBE:
        {
            LOG_W(MODULE_PREFIX, "GAPEvent subscribe conn_handle=%d attr_handle=%d "
                            "reason=%d prevn=%d curn=%d previ=%d curi=%d",
                        event->subscribe.conn_handle,
                        event->subscribe.attr_handle,
                        event->subscribe.reason,
                        event->subscribe.prev_notify,
                        event->subscribe.cur_notify,
                        event->subscribe.prev_indicate,
                        event->subscribe.cur_indicate);

            // Handle subscription to GATT attr
            BLEGattServer::handleSubscription(event, _bleGapConnHandle);
            return 0;
        }
        case BLE_GAP_EVENT_MTU:
        {
            LOG_W(MODULE_PREFIX, "mtu update event; conn_handle=%d cid=%d mtu=%d",
                        event->mtu.conn_handle,
                        event->mtu.channel_id,
                        event->mtu.value);
            return 0;
        }
        case BLE_GAP_EVENT_IDENTITY_RESOLVED:
        {
            LOG_W(MODULE_PREFIX, "GAPEvent identity resolved");
            return 0;
        }
        case BLE_GAP_EVENT_REPEAT_PAIRING:
        { 
            LOG_W(MODULE_PREFIX, "GAPEvent Repeat Pairing");

            /* We already have a bond with the peer, but it is attempting to
            * establish a new secure link.  This app sacrifices security for
            * convenience: just throw away the old bond and accept the new link.
            */

            // Delete the old bond
            int rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
            assert(rc == 0);
            ble_store_util_delete_peer(&desc.peer_id_addr);

            // Return BLE_GAP_REPEAT_PAIRING_RETRY to indicate that the host should
            // continue with the pairing operation.
            return BLE_GAP_REPEAT_PAIRING_RETRY;
        }
        case BLE_GAP_EVENT_PHY_UPDATE_COMPLETE:
        {
            LOG_W(MODULE_PREFIX, "GAPEvent PHY update complete");
            return 0;
        }
        case BLE_GAP_EVENT_EXT_DISC:
        {
            LOG_W(MODULE_PREFIX, "GAPEvent EXT DISC");
            return 0;
        }
    }

    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BLE task - runs until nimble_port_stop
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BLEManager::bleHostTask(void *param)
{
    // This function will return only when nimble_port_stop() is executed
    LOG_I(MODULE_PREFIX, "BLE Host Task Started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Callback for GATT access
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BLEManager::gattAccessCallbackStatic(const char* characteristicName, bool readOp, const uint8_t *payloadbuffer, int payloadlength)
{
    // LOG_D(MODULE_PREFIX, "GattAccessCallback %s %s len %d\n", characteristicName, readOp ? "read" : "write", payloadlength);
    if (_pBLEManager)
        _pBLEManager->gattAccessCallback(characteristicName, readOp, payloadbuffer, payloadlength);
}

void BLEManager::gattAccessCallback(const char* characteristicName, bool readOp, const uint8_t *payloadbuffer, int payloadlength)
{
    _pBLEManager->_rxTotalCount++;
    if (!readOp)
    {
        // Send the message to the ProtocolEndpointManager if this is a write to the characteristic
        if (getProtocolEndpointManager())
            getProtocolEndpointManager()->handleInboundMessage(_protocolEndpointID, payloadbuffer, payloadlength);

#ifdef DEBUG_BLE_RX_PAYLOAD
        // Debug
        uint32_t sz = payloadlength;
        const uint8_t* pVals = payloadbuffer;
        char outBuf[400];
        strcpy(outBuf, "");
        char tmpBuf[10];
        for (int i = 0; i < sz; i++)
        {
            sprintf(tmpBuf, "%02x ", pVals[i]);
            strlcat(outBuf, tmpBuf, sizeof(outBuf));
        }
        LOG_I(MODULE_PREFIX, "gatt rx payloadLen %d payload %s", sz, outBuf);
#endif
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send message over BLE
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool BLEManager::sendBLEMsg(ProtocolEndpointMsg& msg)
{
#ifdef DEBUG_BLE_TX_MSG
#ifndef DEBUG_BLE_PUBLISH
    if (msg.getDirection() != MSG_DIRECTION_PUBLISH) 
    {
#endif
        LOG_I(MODULE_PREFIX, "sendBLEMsg channelID %d, direction %s msgNum %d, len %d msg %s",
            msg.getChannelID(), msg.getDirectionAsString(msg.getDirection()), msg.getMsgNumber(), msg.getBufLen(), msg.getBuf());
#ifndef DEBUG_BLE_PUBLISH
    }
#endif
#endif

    // Check if publish message and only add if outbound queue less than half full
    if ((msg.getDirection() == MSG_DIRECTION_PUBLISH) && (_bleOutboundQueue.count() >= _bleOutboundQueue.maxLen()/2))
        return false;

    // Split into multiple messages if required
    const uint8_t* pMsgPtr = msg.getBuf();
    uint32_t remainingLen = msg.getBufLen();
    for (int msgIdx = 0; msgIdx < BLE_OUTBOUND_MSG_QUEUE_SIZE; msgIdx++)
    {
        uint32_t msgLen = _maxPacketLength;
        if (msgLen > remainingLen)
            msgLen = remainingLen;

        // Send to the queue
        ProtocolRawMsg bleOutMsg(pMsgPtr, msgLen);
        _bleOutboundQueue.put(bleOutMsg);

#ifdef DEBUG_BLE_TX_MSG_SPLIT
        if (msg.getBufLen() > _maxPacketLength)
            LOG_W(MODULE_PREFIX, "sendBLEMsg msgIdx %d partLen %d totalLen %d rsltOk %d", msgIdx, msgLen, msg.getBufLen(), rsltOk);
#endif
#ifdef DEBUG_BLE_TX_MSG_DETAIL
        if (msg.getBufLen() > _maxPacketLength)
        {
            String hexStr;
            Utils::getHexStrFromBytes(msg.getBuf(), msg.getBufLen(), hexStr);
            LOG_W(MODULE_PREFIX, "sendBLEMsg msgIdx %d partLen %d %s", msgIdx, msgLen, hexStr.c_str());
        }
#endif
        // Calculate remaining
        remainingLen -= msgLen;
        pMsgPtr += msgLen;
        if (remainingLen == 0)
            break;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Ready to send indicator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool BLEManager::readyToSend()
{
    // Check if BLE connected
    return BLEGattServer::readyToSend();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Set connection state
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BLEManager::setIsConnected(bool isConnected, uint16_t connHandle)
{
#ifdef USE_TIMED_ADVERTISING_CHECK
    // Reset timer for advertising check
    _advertisingCheckRequired = !isConnected;
    _advertisingCheckMs = millis();
#endif

    // Connected state change
    _isConnected = isConnected;
    _bleGapConnHandle = connHandle;
    
    // Inform hooks of status change
    if (_pBLEManager)
        _pBLEManager->executeStatusChangeCBs();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get advertising name
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String BLEManager::getAdvertisingName()
{
    // Check singleton ok
    if (!_pBLEManager)
        return "";

    // Name
    String adName = _pBLEManager->configGetString("adName", _pBLEManager->getSystemName().c_str());
    if (adName.length() == 0)
        adName = _pBLEManager->getFriendlyName();
    if (adName.length() == 0)
        adName = _pBLEManager->getSystemName();
    return adName;
}
