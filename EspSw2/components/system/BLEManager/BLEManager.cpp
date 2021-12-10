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

// Use a separate thread to send over BLE
#define USE_SEPARATE_THREAD_FOR_BLE_SENDING

// Warn
#define WARN_ON_BLE_INIT_FAILED
#define WARN_ON_BLE_ADVERTISING_START_FAILURE
#define WARN_ON_ONSYNC_ADDR_ERROR

// Debug
// #define DEBUG_ONSYNC_ADDR
// #define DEBUG_BLE_ADVERTISING
// #define DEBUG_BLE_ON_RESET
// #define DEBUG_BLE_RX_PAYLOAD
// #define DEBUG_BLE_TX_MSG
// #define DEBUG_BLE_TX_MSG_SPLIT
// #define DEBUG_BLE_TX_MSG_DETAIL
// #define DEBUG_BLE_PUBLISH
// #define DEBUG_BLE_SETUP
// #define DEBUG_LOG_CONNECT
// #define DEBUG_LOG_CONNECT_DETAIL
// #define DEBUG_LOG_DISCONNECT_DETAIL
// #define DEBUG_LOG_CONN_UPDATE_DETAIL
// #define DEBUG_LOG_CONN_UPDATE
// #define DEBUG_LOG_GAP_EVENT
// #define DEBUG_GAP_EVENT_DISC
// #define DEBUG_BLE_ENC_CHANGE
// #define DEBUG_BLE_ENC_CHANGE_DETAIL
// #define DEBUG_BLE_EVENT_NOTIFY_RX
// #define DEBUG_BLE_EVENT_NOTIFY_TX
// #define DEBUG_BLE_EVENT_SUBSCRIBE
// #define DEBUG_BLE_EVENT_MTU
// #define DEBUG_BLE_TASK_STARTED

// The following should only be uncommented if the cost of getting RSSI is acceptable
// it seems to take 3.5ms in ESP IDF 4.0.1
// #define INCLUDE_RSSI_IN_GET_STATUS_JSON

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Statics, etc
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Log prefix
static const char *MODULE_PREFIX = "BLEMan";

// NOTE: this is the main service in BLEGattServer
ble_uuid128_t BLEManager::BLE_RICV2_ADVERTISING_UUID = BLEGattServer::GATT_RICV2_MAIN_SERVICE_UUID;

// Singleton BLEManager
BLEManager *BLEManager::_pBLEManager = NULL;

// Address type
uint8_t BLEManager::own_addr_type = 0;

// Is connected
bool BLEManager::_isConnected = false;
uint16_t BLEManager::_bleGapConnHandle = 0;

// Advertising name
String BLEManager::_configuredAdvertisingName;

#ifdef USE_TIMED_ADVERTISING_CHECK
// Advertising check ms
bool BLEManager::_advertisingCheckRequired = false;
uint32_t BLEManager::_advertisingCheckMs = millis();
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BLEManager::BLEManager(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, 
                ConfigBase *pMutableConfig, const char* defaultAdvName)
    : SysModBase(pModuleName, defaultConfig, pGlobalConfig, pMutableConfig),
      _bleOutboundQueue(BLE_OUTBOUND_MSG_QUEUE_SIZE)
{
    // BLE interface
    _enableBLE = false;
    _BLEDeviceInitialised = false;
    _defaultAdvName = defaultAdvName;

    // EndpointID
    _protocolEndpointID = ProtocolEndpointManager::UNDEFINED_ID;

    // Singleton
    _pBLEManager = this;

    // Stats
    _rxTotalCount = 0;
    _txTotalCount = 0;

    // Outbound msg timing
    _lastOutboundMsgMs = 0;

    // Outbound queue task
    _outboundMsgTaskHandle = nullptr;
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
#ifdef DEBUG_BLE_SETUP
    LOG_I(MODULE_PREFIX, "applySetup BLE enabled %c", _enableBLE ? 'Y' : 'N');
#endif

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
#ifdef WARN_ON_BLE_INIT_FAILED
                LOG_E(MODULE_PREFIX, "applySetup nimble init failed err=%d", err);
#endif
                return;
            }

            // Log level for NimBLE module is set in here so if we want to override it
            // we have to do so after this call
            nimble_port_init();
        }
        _BLEDeviceInitialised = true;

        // Get NimBLE log level 
        String nimLogLev = configGetString("nimLogLev", "");
        setModuleLogLevel("NimBLE", nimLogLev);

        // onReset callback
        ble_hs_cfg.reset_cb = [](int reason) {
#ifdef DEBUG_BLE_ON_RESET            
            LOG_I(MODULE_PREFIX, "onReset() reason=%d", reason);
#endif
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
        _configuredAdvertisingName = getAdvertisingName();
        rc = ble_svc_gap_device_name_set(_configuredAdvertisingName.c_str());
        assert(rc == 0);

        // Set the callback
        BLEGattServer::setServerAccessCB(gattAccessCallbackStatic);

        // Start the host task
        nimble_port_freertos_init(bleHostTask);

        // Currently disconnected
        setIsConnected(false);

        // Check if a thread should be started for sending
#ifdef USE_SEPARATE_THREAD_FOR_BLE_SENDING
        UBaseType_t taskCore = configGetLong("taskCore", DEFAULT_TASK_CORE);
        BaseType_t taskPriority = configGetLong("taskPriority", DEFAULT_TASK_PRIORITY);
        int taskStackSize = configGetLong("taskStack", DEFAULT_TASK_SIZE_BYTES);

        // Start the worker task
        BaseType_t retc = pdPASS;
        if (_outboundMsgTaskHandle == nullptr)
        {
            retc = xTaskCreatePinnedToCore(
                        outboundMsgTaskStatic,
                        "BLEOutQ",                              // task name
                        taskStackSize,                          // stack size of task
                        this,                                   // parameter passed to task on execute
                        taskPriority,                           // priority
                        (TaskHandle_t*)&_outboundMsgTaskHandle, // task handle
                        taskCore);                              // pin task to core N
        }

        // Debug
        LOG_I(MODULE_PREFIX, "setup maxPktLen %d task %s(%d) core %d priority %d stack %d outQSlots %d minMsBetweenSends %d",
                    _maxPacketLength,
                    (retc == pdPASS) ? "OK" : "FAILED", retc, 
                    taskCore, taskPriority, taskStackSize,
                    BLE_OUTBOUND_MSG_QUEUE_SIZE,
                    BLE_MIN_TIME_BETWEEN_OUTBOUND_MSGS_MS);
        LOG_I(MODULE_PREFIX, "applySetup maxPktLen %d", _maxPacketLength);
#else
        // Debug
        LOG_I(MODULE_PREFIX, "setup maxPktLen %d using service loop outQSlots %d minMsBetweenSends %d",
                    _maxPacketLength,
                    BLE_OUTBOUND_MSG_QUEUE_SIZE,
                    BLE_MIN_TIME_BETWEEN_OUTBOUND_MSGS_MS);
        LOG_I(MODULE_PREFIX, "applySetup maxPktLen %d", _maxPacketLength);
#endif
    }
    else if (_BLEDeviceInitialised)
    {
#ifdef USE_SEPARATE_THREAD_FOR_BLE_SENDING
        // Shutdown task
        xTaskNotifyGive(_outboundMsgTaskHandle);
#endif

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
#ifdef WARN_ON_BLE_INIT_FAILED
            LOG_W(MODULE_PREFIX, "applySetup deinit failed");
#endif
        }
        _BLEDeviceInitialised = false;

        // Debug
#ifdef DEBUG_BLE_SETUP
        LOG_I(MODULE_PREFIX, "applySetup de-init ok");
#endif
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
#ifdef WARN_ON_BLE_ADVERTISING
                LOG_W(MODULE_PREFIX, "service not conn or adv so start advertising");
#endif

                // Start advertising
                bool startAdvOk = startAdvertising();

                // Debug
#ifdef WARN_ON_BLE_ADVERTISING_START_FAILURE
                if (!startAdvOk)
                {
                    LOG_W(MODULE_PREFIX, "service started advertising FAILED");
                    delay(50);
                }
#endif
#ifdef DEBUG_BLE_ADVERTISING
                if (startAdvOk)
                {
                    LOG_W(MODULE_PREFIX, "service started advertising ok");
                }
#endif
            }
            else
            {
#ifdef DEBUG_BLE_ADVERTISING
                LOG_I(MODULE_PREFIX, "service BLE advertising check ok");
#endif
                _advertisingCheckRequired = false;
            }
            _advertisingCheckMs = millis();
        }
    }
#endif

#ifndef USE_SEPARATE_THREAD_FOR_BLE_SENDING
    // Send if not using alternate thread
    handleSendFromOutboundQueue();
#endif
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
            std::bind(&BLEManager::readyToSend, this, std::placeholders::_1),
            BLE_INBOUND_BLOCK_MAX_DEFAULT,
            BLE_INBOUND_QUEUE_MAX_DEFAULT,
            BLE_OUTBOUND_BLOCK_MAX_DEFAULT,
            BLE_OUTBOUND_QUEUE_MAX_DEFAULT);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get status JSON
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
String BLEManager::getStatusJSON()
{
    if (!_BLEDeviceInitialised)
        return R"({"rslt":"failDisabled"})";

#ifdef INCLUDE_RSSI_IN_GET_STATUS_JSON
    char bleRSSIStr[50];
    int8_t rssiOut = 0;
    int bleGapConnRSSI = ble_gap_conn_rssi(_bleGapConnHandle, &rssiOut);
    snprintf(bleRSSIStr, sizeof(bleRSSIStr), R"(,"rssi":%d")", bleGapConnRSSI == 0 ? rssiOut : 0);
#endif

    char statusStr[200];
    snprintf(statusStr, sizeof(statusStr), 
                R"({"rslt":"ok","isConn":%d,"isAdv":%d,"advName":"%s",%s"BLEMAC":"%s"})",
                _isConnected,
                ble_gap_adv_active(),
                ble_svc_gap_device_name(),
#ifdef INCLUDE_RSSI_IN_GET_STATUS_JSON
                bleRSSIStr,
#else
                "",
#endif    
                getSystemMACAddressStr(ESP_MAC_BT, ":").c_str()
                );

    return statusStr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Debug
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String BLEManager::getDebugJSON()
{
    if (!_BLEDeviceInitialised)
        return R"({"s":"none"})";

    bool advertisingActive = ble_gap_adv_active();
    bool discoveryActive = ble_gap_disc_active();
    bool gapConnActive = ble_gap_conn_active();

    // Also ble_gap_conn_desc can be obtained
    String advertisingInfo;
    if (advertisingActive)
        advertisingInfo = R"(,"adv":")" + String(ble_svc_gap_device_name()) + R"(")";
    char tmpBuf[200];
    snprintf(tmpBuf, sizeof(tmpBuf)-1, R"({"s":"%s"%s,"rx":%d,"tx":%d})",
        _isConnected ? (gapConnActive ? "actv" : "conn") : (advertisingActive ? "adv" : (discoveryActive ? "disco" : "none")),
        advertisingInfo.c_str(),
        _rxTotalCount,
        _txTotalCount);
    return tmpBuf;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// onSync callback
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BLEManager::onSync()
{
    if (!_BLEDeviceInitialised)
        return;

    // Validate
    int rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

    // Figure out address to use while advertising (no privacy for now)
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0)
    {
#ifdef WARN_ON_ONSYNC_ADDR_ERROR
        LOG_W(MODULE_PREFIX, "onSync() error determining address type; rc=%d", rc);
#endif
        return;
    }

    // Debug showing addr
    uint8_t addrVal[6] = {0};
    rc = ble_hs_id_copy_addr(own_addr_type, addrVal, NULL);
#ifdef DEBUG_ONSYNC_ADDR
    LOG_I(MODULE_PREFIX, "onSync() Device Address: %x:%x:%x:%x:%x:%x",
              addrVal[5], addrVal[4], addrVal[3], addrVal[2], addrVal[1], addrVal[0]);
#endif

    // Start advertising
    if (!startAdvertising())
    {
#ifdef WARN_ON_BLE_ADVERTISING_START_FAILURE
        LOG_W(MODULE_PREFIX, "onSync started advertising FAILED");
        delay(50);
#endif
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// startAdvertising
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Enables advertising with the following parameters:
 *     o General discoverable mode.
 *     o Undirected connectable mode.
 */
bool BLEManager::startAdvertising()
{
    // Check if already advertising
    if (ble_gap_adv_active())
    {
#ifdef DEBUG_BLE_ADVERTISING
        LOG_I(MODULE_PREFIX, "startAdvertising - already advertising");
#endif
        return true;
    }

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
        return false;
    }

    // Clear fields
    memset(&fields, 0, sizeof fields);

    // Set the advertising name
    if (ble_svc_gap_device_name_set(_configuredAdvertisingName.c_str()) != 0)
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
        return false;
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
        return false;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Log information about a connection to the console.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BLEManager::logConnectionInfo(struct ble_gap_conn_desc *desc)
{
    LOG_I(MODULE_PREFIX, "handle=%d our_ota_addr_type=%d our_ota_addr=%02x:%02x:%02x:%02x:%02x:%02x",
                desc->conn_handle, desc->our_ota_addr.type, 
                desc->our_ota_addr.val[0],
				desc->our_ota_addr.val[1],
				desc->our_ota_addr.val[2],
				desc->our_ota_addr.val[3],
				desc->our_ota_addr.val[4],
				desc->our_ota_addr.val[5]);
    LOG_I(MODULE_PREFIX, " our_id_addr_type=%d our_id_addr=%02x:%02x:%02x:%02x:%02x:%02x",
                desc->our_id_addr.type, 
                desc->our_id_addr.val[0],
				desc->our_id_addr.val[1],
				desc->our_id_addr.val[2],
				desc->our_id_addr.val[3],
				desc->our_id_addr.val[4],
				desc->our_id_addr.val[5]);
    LOG_I(MODULE_PREFIX, " peer_ota_addr_type=%d peer_ota_addr=%02x:%02x:%02x:%02x:%02x:%02x",
                desc->peer_ota_addr.type, 
                desc->peer_ota_addr.val[0],
				desc->peer_ota_addr.val[1],
				desc->peer_ota_addr.val[2],
				desc->peer_ota_addr.val[3],
				desc->peer_ota_addr.val[4],
				desc->peer_ota_addr.val[5]);
    LOG_I(MODULE_PREFIX, " peer_id_addr_type=%d peer_id_addr=%02x:%02x:%02x:%02x:%02x:%02x",
                desc->peer_id_addr.type, 
                desc->peer_id_addr.val[0],
				desc->peer_id_addr.val[1],
				desc->peer_id_addr.val[2],
				desc->peer_id_addr.val[3],
				desc->peer_id_addr.val[4],
				desc->peer_id_addr.val[5]);
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
#ifdef DEBUG_LOG_CONNECT
            LOG_I(MODULE_PREFIX, "GAPEvent connection %s (%d) ",
                        event->connect.status == 0 ? "established" : "failed",
                        event->connect.status);
#endif
            if (event->connect.status == 0)
            {
                setIsConnected(true, event->connect.conn_handle);
                int rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
                assert(rc == 0);
#ifdef DEBUG_LOG_CONNECT_DETAIL
                logConnectionInfo(&desc);
#endif
            }
            else
            {
                setIsConnected(false);
                // Connection failed; resume advertising
                if (!startAdvertising())
                {
#ifdef WARN_ON_BLE_ADVERTISING_START_FAILURE
                    LOG_W(MODULE_PREFIX, "nimbleGapEvent start advertising FAILED");
                    delay(50);
#endif
                }
                else
                {
                    LOG_I(MODULE_PREFIX, "nimbleGapEvent resumed advertising after connection failure");
                }
            }
            return 0;
        }
        case BLE_GAP_EVENT_DISCONNECT:
        {
            LOG_W(MODULE_PREFIX, "nimbleGapEvent disconnect; reason=%d ", event->disconnect.reason);
            setIsConnected(false);
#ifdef DEBUG_LOG_DISCONNECT_DETAIL
            logConnectionInfo(&event->disconnect.conn);
#endif

            // Connection terminated; resume advertising
            // LOG_W(MODULE_PREFIX, "GAPEvent disconnect - requesting startAdvertising");
            // startAdvertising();
            return 0;
        }
        case BLE_GAP_EVENT_CONN_UPDATE:
        { 
            // The central has updated the connection parameters
#ifdef DEBUG_LOG_CONN_UPDATE
            LOG_I(MODULE_PREFIX, "GAPEvent connection updated; status=%d ",
                        event->conn_update.status);
#endif
            int rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
            assert(rc == 0);
#ifdef DEBUG_LOG_CONN_UPDATE_DETAIL
            logConnectionInfo(&desc);
#endif
            return 0;
        }
        case BLE_GAP_EVENT_CONN_UPDATE_REQ:
        {
#ifdef DEBUG_LOG_CONN_UPDATE
            LOG_I(MODULE_PREFIX, "GAPEvent connection update request");
#endif
            return 0;
        }
        case BLE_GAP_EVENT_L2CAP_UPDATE_REQ:
        {
#ifdef DEBUG_LOG_CONN_UPDATE
            LOG_I(MODULE_PREFIX, "GAPEvent L2CAP update request");
#endif
            return 0;
        }
        case BLE_GAP_EVENT_TERM_FAILURE:
        {
#ifdef DEBUG_LOG_GAP_EVENT
            LOG_I(MODULE_PREFIX, "GAPEvent term failure");
#endif
            return 0;
        }
        case BLE_GAP_EVENT_DISC:
        {
#ifdef DEBUG_GAP_EVENT_DISC
            LOG_I(MODULE_PREFIX, "GAPEvent DISC");
#endif
            return 0;
        }
        case BLE_GAP_EVENT_DISC_COMPLETE:
        {
#ifdef DEBUG_GAP_EVENT_DISC
            LOG_I(MODULE_PREFIX, "GAPEvent DISC COMPLETE");
#endif
            return 0;
        }
        case BLE_GAP_EVENT_ADV_COMPLETE:
        {
#ifdef DEBUG_BLE_ADVERTISING
            LOG_I(MODULE_PREFIX, "GAPEvent advertise complete; reason=%d",
                        event->adv_complete.reason);
#endif
            // Connection failed; resume advertising
            if (!startAdvertising())
            {
#ifdef WARN_ON_BLE_ADVERTISING_START_FAILURE
                    LOG_W(MODULE_PREFIX, "nimbleGapEvent started advertising FAILED");
                    delay(50);
#endif
            }
            return 0;
        }
        case BLE_GAP_EVENT_ENC_CHANGE:
        {
            // Encryption has been enabled or disabled for this connection
#ifdef DEBUG_BLE_ENC_CHANGE
            LOG_I(MODULE_PREFIX, "GAPEvent encryption change; status=%d ",
                        event->enc_change.status);
#endif
            int rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
            assert(rc == 0);
#ifdef DEBUG_BLE_ENC_CHANGE_DETAIL
            logConnectionInfo(&desc);
#endif
            return 0;
        }
        case BLE_GAP_EVENT_PASSKEY_ACTION:
        {
#ifdef DEBUG_BLE_ENC_CHANGE_DETAIL
            LOG_I(MODULE_PREFIX, "GAPEvent PASSKEY action");
#endif
            return 0;
        }
        case BLE_GAP_EVENT_NOTIFY_RX:
        {
#ifdef DEBUG_BLE_EVENT_NOTIFY_RX
            LOG_I(MODULE_PREFIX, "GAPEvent notify RX");
#endif
            return 0;
        }
        case BLE_GAP_EVENT_NOTIFY_TX:
        {
            _pBLEManager->_txTotalCount++;
#ifdef DEBUG_BLE_EVENT_NOTIFY_TX
            LOG_I(MODULE_PREFIX, "GAPEvent notify TX");
#endif
            // Every transmission from GAP - could be used for stats if other stats not available
            return 0;
        }
        case BLE_GAP_EVENT_SUBSCRIBE:
        {
#ifdef DEBUG_BLE_EVENT_SUBSCRIBE
            LOG_I(MODULE_PREFIX, "GAPEvent subscribe conn_handle=%d attr_handle=%d "
                            "reason=%d prevn=%d curn=%d previ=%d curi=%d",
                        event->subscribe.conn_handle,
                        event->subscribe.attr_handle,
                        event->subscribe.reason,
                        event->subscribe.prev_notify,
                        event->subscribe.cur_notify,
                        event->subscribe.prev_indicate,
                        event->subscribe.cur_indicate);
#endif
            // Handle subscription to GATT attr
            BLEGattServer::handleSubscription(event, _bleGapConnHandle);
            return 0;
        }
        case BLE_GAP_EVENT_MTU:
        {
#ifdef DEBUG_BLE_EVENT_MTU
            LOG_I(MODULE_PREFIX, "mtu update event; conn_handle=%d cid=%d mtu=%d",
                        event->mtu.conn_handle,
                        event->mtu.channel_id,
                        event->mtu.value);
#endif
            return 0;
        }
        case BLE_GAP_EVENT_IDENTITY_RESOLVED:
        {
#ifdef DEBUG_LOG_GAP_EVENT
            LOG_I(MODULE_PREFIX, "GAPEvent identity resolved");
#endif
            return 0;
        }
        case BLE_GAP_EVENT_REPEAT_PAIRING:
        { 
#ifdef DEBUG_LOG_GAP_EVENT
            LOG_I(MODULE_PREFIX, "GAPEvent Repeat Pairing");
#endif
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
#ifdef DEBUG_LOG_GAP_EVENT
            LOG_I(MODULE_PREFIX, "GAPEvent PHY update complete");
#endif
            return 0;
        }
        case BLE_GAP_EVENT_EXT_DISC:
        {
#ifdef DEBUG_GAP_EVENT_DISC
            LOG_I(MODULE_PREFIX, "GAPEvent EXT DISC");
#endif
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
#ifdef DEBUG_BLE_TASK_STARTED
    LOG_I(MODULE_PREFIX, "BLE Host Task Started");
#endif
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
    if (!_BLEDeviceInitialised)
        return false;

#ifdef DEBUG_BLE_TX_MSG
#ifndef DEBUG_BLE_PUBLISH
    if (msg.getMsgTypeCode() != MSG_TYPE_PUBLISH) 
    {
#endif
        LOG_I(MODULE_PREFIX, "sendBLEMsg channelID %d, msgType %s msgNum %d, len %d msg %s",
            msg.getChannelID(), msg.getMsgTypeAsString(msg.getMsgTypeCode()), msg.getMsgNumber(), msg.getBufLen(), msg.getBuf());
#ifndef DEBUG_BLE_PUBLISH
    }
#endif
#endif

    // Check if publish message and only add if outbound queue less than half full
    if ((msg.getMsgTypeCode() == MSG_TYPE_PUBLISH) && (_bleOutboundQueue.count() >= _bleOutboundQueue.maxLen()/2))
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
        if (msg.getBufLen() > _maxPacketLength) {
            LOG_W(MODULE_PREFIX, "sendBLEMsg msgIdx %d partLen %d totalLen %d rsltOk %d", msgIdx, msgLen, msg.getBufLen(), rsltOk);
        }
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

bool BLEManager::readyToSend(uint32_t channelID)
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
        _pBLEManager->executeStatusChangeCBs(isConnected);
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
    String adName = _pBLEManager->configGetString("adName", "");
    if (adName.length() == 0)
        adName = _pBLEManager->getFriendlyName();
    if ((adName.length() == 0) && (_pBLEManager))
        adName = _pBLEManager->_defaultAdvName;
    if (adName.length() == 0)
        adName = _pBLEManager->getSystemName();
    return adName;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle sending from outbound queue
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BLEManager::handleSendFromOutboundQueue()
{
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
// Task worker for outbound messages
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BLEManager::outboundMsgTaskStatic(void* pvParameters)
{
    // Get the object that requested the task
    BLEManager* pObjPtr = (BLEManager*)pvParameters;
    if (pObjPtr)
        pObjPtr->outboundMsgTask();
}

void BLEManager::outboundMsgTask()
{
    // Run the task until asked to stop
    while (ulTaskNotifyTake(pdTRUE, 0) == 0)
    {        
        // Handle queue
        handleSendFromOutboundQueue();

        // Yield
        vTaskDelay(1);
    }

    // Task has exited
    _outboundMsgTaskHandle = nullptr;
    vTaskDelete(NULL);
}
