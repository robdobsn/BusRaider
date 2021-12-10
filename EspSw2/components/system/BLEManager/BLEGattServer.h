/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// BLEGattServer
// Handles BLE GATT
//
// RIC V2 Firmware
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <functional>
#include <functional>
#include "host/ble_hs.h"

typedef std::function<void (const char* characteristicName, bool readOp, const uint8_t *payloadbuffer, int payloadlength)> BLEGattServerAccessCBType;

class BLEGattServer
{
public:
    // UUIDs
    static ble_uuid128_t GATT_RICV2_MAIN_SERVICE_UUID;
    static ble_uuid128_t GATT_RICV2_MESSAGE_COMMAND_UUID;
    static ble_uuid128_t GATT_RICV2_MESSAGE_RESPONSE_UUID;

    // Init and deinit
    static int initServer();
    static void deinitServer();

    // Callback
    static void registrationCallback(struct ble_gatt_register_ctxt *ctxt, void *arg);

    // Set callback to call when a service is accessed
    static void setServerAccessCB(BLEGattServerAccessCBType callback)
    {
        _accessCallback = callback;
    }

    // Subscription
    static void handleSubscription(struct ble_gap_event * pEvent, uint16_t connHandle);

    // Send to central (using notification)
    static bool sendToCentral(const uint8_t* pBuf, uint32_t bufLen);

    // Check ready to send
    static bool readyToSend();

private:

    // Max size of packet that can be received from NimBLE
    static const int BLE_MAX_RX_PACKET_SIZE = 512;

    // ?????
    static uint8_t gatt_svr_sec_test_static_val;

    // Handles
    static uint16_t _bleGattMessageResponseHandle;
    static uint16_t _bleGapConnHandle;

    // State of notify (send from peripheral)
    static bool _responseNotifyState;

    static const struct ble_gatt_svc_def servicesList[];

    // Get data that has been written to characteristic (sent by central)
    static int getDataWrittenToCharacteristic(struct os_mbuf *om, uint16_t min_len, uint16_t max_len,
                   void *dst, uint16_t *len);

    // Callback on access to characteristics
    static int commandCharAccess(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt,
                             void *arg);
    static int responseCharAccess(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt,
                             void *arg);

    static BLEGattServerAccessCBType _accessCallback;

    static const uint32_t MIN_TIME_BETWEEN_ERROR_MSGS_MS = 500;
    static uint32_t _lastBLEErrorMsgMs;
    static uint32_t _lastBLEErrorMsgCode;

};
