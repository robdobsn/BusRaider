/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// BLEGattServer
// Handles BLE GATT
//
// RIC V2 Firmware
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <string.h>
#include <Logger.h>
#include <Utils.h>
#include <ArduinoTime.h>
#include "BLEGattServer.h"
// #include "host/ble_uuid.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#define WARN_ON_BLE_CHAR_WRITE_FAIL
#define WARN_ON_BLE_CHAR_READ_UNEXPECTED
#define WARN_ON_BLE_CHAR_WRITE_UNEXPECTED
#define WARN_ON_BLE_CHAR_WRITE_TAKING_TOO_LONG
// #define DEBUG_CMD_CHARACTERISTIC
// #define DEBUG_RESP_CHARACTERISTIC
// #define DEBUG_RESP_SUBSCRIPTION
// #define DEBUG_BLE_REG_SERVICES
// #define DEBUG_BLE_REG_CHARACTERISTIC
// #define DEBUG_BLE_REG_DESCRIPTOR

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Statics, etc
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Log prefix
static const char *MODULE_PREFIX = "BLEGattServer";

/**
 * The service consists of the following characteristics:
 *     o motionCommand: used to request motion
 *     o motionStatus: used to get motion status
 */

// aa76677e-9cfd-4626-a510-0d305be57c8d
ble_uuid128_t BLEGattServer::GATT_RICV2_MAIN_SERVICE_UUID =
    {
        .u = {.type = BLE_UUID_TYPE_128},
        .value = {0x8d, 0x7c, 0xe5, 0x5b, 0x30, 0x0d, 0x10, 0xa5,
                  0x26, 0x46, 0xfd, 0x9c, 0x7e, 0x67, 0x76, 0xaa}};

// aa76677e-9cfd-4626-a510-0d305be57c8e
ble_uuid128_t BLEGattServer::GATT_RICV2_MESSAGE_COMMAND_UUID =
    {
        .u = {.type = BLE_UUID_TYPE_128},
        .value = {0x8e, 0x7c, 0xe5, 0x5b, 0x30, 0x0d, 0x10, 0xa5,
                  0x26, 0x46, 0xfd, 0x9c, 0x7e, 0x67, 0x76, 0xaa}};

// aa76677e-9cfd-4626-a510-0d305be57c8f
ble_uuid128_t BLEGattServer::GATT_RICV2_MESSAGE_RESPONSE_UUID =
    {
        .u = {.type = BLE_UUID_TYPE_128},
        .value = {0x8f, 0x7c, 0xe5, 0x5b, 0x30, 0x0d, 0x10, 0xa5,
                  0x26, 0x46, 0xfd, 0x9c, 0x7e, 0x67, 0x76, 0xaa}};

// Statics
uint16_t BLEGattServer::_bleGattMessageResponseHandle = 0;
bool BLEGattServer::_responseNotifyState = false;
uint16_t BLEGattServer::_bleGapConnHandle = 0;
uint32_t BLEGattServer::_lastBLEErrorMsgMs = 0;
uint32_t BLEGattServer::_lastBLEErrorMsgCode = 0;

// List of services
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
const struct ble_gatt_svc_def BLEGattServer::servicesList[] = {
    {
        /*** Service: Security test. */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &GATT_RICV2_MAIN_SERVICE_UUID.u,
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = &GATT_RICV2_MESSAGE_COMMAND_UUID.u,
                .access_cb = commandCharAccess,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP
            },
            {
                .uuid = &GATT_RICV2_MESSAGE_RESPONSE_UUID.u,
                .access_cb = responseCharAccess,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &_bleGattMessageResponseHandle
            },
            {
                0, /* No more characteristics in this service. */
            }},
    },

    {
        0, /* No more services. */
    },
};
#pragma GCC diagnostic pop

// Access callback
BLEGattServerAccessCBType BLEGattServer::_accessCallback = NULL;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get data written (to characteristic) by central
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int BLEGattServer::getDataWrittenToCharacteristic(struct os_mbuf *om, uint16_t min_len, uint16_t max_len,
                                       void *dst, uint16_t *len)
{
    uint16_t om_len;
    int rc;

    om_len = OS_MBUF_PKTLEN(om);
    if (om_len < min_len || om_len > max_len)
    {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    rc = ble_hs_mbuf_to_flat(om, dst, max_len, len);
    if (rc != 0)
    {
        return BLE_ATT_ERR_UNLIKELY;
    }

    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Command Characteristic access callback
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int BLEGattServer::commandCharAccess(uint16_t conn_handle, uint16_t attr_handle,
                                          struct ble_gatt_access_ctxt *ctxt,
                                          void *arg)
{
#ifdef DEBUG_CMD_CHARACTERISTIC
    LOG_W(MODULE_PREFIX, "cmd char access");
#endif

    switch (ctxt->op)
    {
        case BLE_GATT_ACCESS_OP_WRITE_CHR:
        {
            // Get the written data
            uint16_t rxMsgLen;
            uint8_t rxMsg[BLE_MAX_RX_PACKET_SIZE];
            int rc = getDataWrittenToCharacteristic(ctxt->om,
                                        1,
                                        sizeof rxMsg,
                                        rxMsg, 
                                        &rxMsgLen);

            // Debug
            if (rc == 0)
            {
#ifdef DEBUG_CMD_CHARACTERISTIC
                LOG_W(MODULE_PREFIX, "cmdCharCB opWrite rxFromCentral len %d", rxMsgLen);
#endif
            }
            else
            {
#ifdef WARN_ON_BLE_CHAR_WRITE_FAIL
                LOG_W(MODULE_PREFIX, "cmdCharCB opWrite rxFromCentral failed to get mbuf rc=%d", rc);
#endif
            }

            // Callback with data
            if (_accessCallback)
                _accessCallback("cmdmsg", false, rxMsg, rxMsgLen);

            return rc;
        }
        case BLE_GATT_ACCESS_OP_READ_CHR:
        {
            // This is not expected to happen as the central is not expected
            // to read from this characteristic
#ifdef WARN_ON_BLE_CHAR_READ_UNEXPECTED
            LOG_W(MODULE_PREFIX, "cmdCharCB unexpected opRead");
#endif
            return BLE_ATT_ERR_UNLIKELY;
        }
        default:
        {
            return BLE_ATT_ERR_UNLIKELY;
        }
    }
    return BLE_ATT_ERR_UNLIKELY;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Command Characteristic access callback
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int BLEGattServer::responseCharAccess(uint16_t conn_handle, uint16_t attr_handle,
                                          struct ble_gatt_access_ctxt *ctxt,
                                          void *arg)
{
#ifdef DEBUG_RESP_CHARACTERISTIC
    LOG_W(MODULE_PREFIX, "respCharCB");
#endif

    switch (ctxt->op)
    {
    case BLE_GATT_ACCESS_OP_WRITE_CHR:
    {
        // This is not expected to happen as the central does not sent data
        // on this characteristic
#ifdef WARN_ON_BLE_CHAR_WRITE_UNEXPECTED
        LOG_W(MODULE_PREFIX, "respCharCB unexpected opWrite");
#endif
        return 0;
    }
    case BLE_GATT_ACCESS_OP_READ_CHR:
    {
        // This is not expected to happen as the central receives data via
        // a subscription and isn't expected to read from the characteristic directly
#ifdef WARN_ON_BLE_CHAR_READ_UNEXPECTED
        LOG_W(MODULE_PREFIX, "respCharCB unexpected opRead");
#endif
        return 0;
    }
    default:
    {
        return BLE_ATT_ERR_UNLIKELY;
    }
    }
    return BLE_ATT_ERR_UNLIKELY;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Registration callback
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BLEGattServer::registrationCallback(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    switch (ctxt->op)
    {
    case BLE_GATT_REGISTER_OP_SVC:
    {
#ifdef DEBUG_BLE_REG_SERVICES
        char buf[BLE_UUID_STR_LEN];
        LOG_W(MODULE_PREFIX, "registered service %s with handle=%d",
                    ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                    ctxt->svc.handle);
#endif
        break;
    }
    case BLE_GATT_REGISTER_OP_CHR:
    {
#ifdef DEBUG_BLE_REG_CHARACTERISTIC
        char buf[BLE_UUID_STR_LEN];
        LOG_W(MODULE_PREFIX, "registering characteristic %s with "
                           "def_handle=%d val_handle=%d\n",
                    ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                    ctxt->chr.def_handle,
                    ctxt->chr.val_handle);
#endif
        break;
    }
    case BLE_GATT_REGISTER_OP_DSC:
    {
#ifdef DEBUG_BLE_REG_DESCRIPTOR
        char buf[BLE_UUID_STR_LEN];
        LOG_W(MODULE_PREFIX, "registering descriptor %s with handle=%d",
                    ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                    ctxt->dsc.handle);
#endif
        break;
    }
    default:
    {
        break;
    }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send message to central
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int bleCallbackWR(uint16_t conn_handle,
                             const struct ble_gatt_error *error,
                             struct ble_gatt_attr *attr,
                             void *arg)
{
#ifdef DEBUG_RESP_CHARACTERISTIC
    LOG_I(MODULE_PREFIX, "bleCallbackWR ch %d, err %d", conn_handle, error->status);
#endif
    return 0;
}

bool BLEGattServer::sendToCentral(const uint8_t* pBuf, uint32_t bufLen)
{
    // Check if we are in notify state
    if (!BLEGattServer::_responseNotifyState) 
    {
        LOG_W(MODULE_PREFIX, "sendToCentral failed as client has not subscribed");
        return false;
    }

    // Form buffer to send
    struct os_mbuf *om = ble_hs_mbuf_from_flat(pBuf, bufLen);
#ifdef DEBUG_RESP_CHARACTERISTIC
    LOG_I(MODULE_PREFIX, "sendToCentral sending bufLen %d", bufLen);
#endif
#ifdef WARN_ON_BLE_CHAR_WRITE_TAKING_TOO_LONG
    uint64_t nowUS = micros();
#endif
    int rc = ble_gattc_notify_custom(BLEGattServer::_bleGapConnHandle, BLEGattServer::_bleGattMessageResponseHandle, om);
#ifdef WARN_ON_BLE_CHAR_WRITE_TAKING_TOO_LONG
    uint64_t elapsedUs = micros() - nowUS;
    if (elapsedUs > 50000)
    {
        LOG_W(MODULE_PREFIX, "sendToCentral SLOW took %llduS", elapsedUs);
    }
#endif
    if (rc != 0)
    {
        if (Utils::isTimeout(millis(), _lastBLEErrorMsgMs, MIN_TIME_BETWEEN_ERROR_MSGS_MS) || 
                    (_lastBLEErrorMsgCode != rc))
        {
            LOG_W(MODULE_PREFIX, "sendToCentral failed rc = %d", rc);
            _lastBLEErrorMsgCode = rc;
            _lastBLEErrorMsgMs = millis();
        }
        return false;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check ready to send
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool BLEGattServer::readyToSend(bool& noConn)
{
    noConn = !BLEGattServer::_responseNotifyState;
    // LOG_W(MODULE_PREFIX, "readyToSend %d", BLEGattServer::_responseNotifyState);
    return BLEGattServer::_responseNotifyState;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Initialise server
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int BLEGattServer::initServer()
{
    // Initialise GAP and GATT
    ble_svc_gap_init();
    ble_svc_gatt_init();

    // Prepare for services to be added
    int rc = ble_gatts_count_cfg(servicesList);
    if (rc != 0)
        return rc;

    // Add services
    rc = ble_gatts_add_svcs(servicesList);
    if (rc != 0)
        return rc;

    // Register 

#ifdef DEBUG_FOR_ESP32_MINI_BOARDS
    gpio_pad_select_gpio(LED_OUTPUT_TEST);
    gpio_set_direction(LED_OUTPUT_TEST, GPIO_MODE_OUTPUT);
#endif

    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Deinit server
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BLEGattServer::deinitServer()
{
    
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle subscription
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BLEGattServer::handleSubscription(struct ble_gap_event * pEvent, uint16_t connHandle)
{
    if (pEvent->subscribe.attr_handle == _bleGattMessageResponseHandle) {
        _responseNotifyState = pEvent->subscribe.cur_notify != 0;
        // debug_test_nofify_reset();
    } else if (pEvent->subscribe.attr_handle != _bleGattMessageResponseHandle) {
        _responseNotifyState = pEvent->subscribe.cur_notify != 0;
        // debug_test_notify_stop();
    }
#ifdef DEBUG_RESP_SUBSCRIPTION
    LOG_W(MODULE_PREFIX, "handleSubscription notify enabled %s", _responseNotifyState ? "YES" : "NO");
#endif
}
