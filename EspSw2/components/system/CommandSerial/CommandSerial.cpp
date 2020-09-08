/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CommandSerial
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Logger.h>
#include <CommandSerial.h>
#include <Utils.h>
#include <RestAPIEndpointManager.h>
#include "ProtocolEndpointManager.h"
#include <driver/uart.h>

static const char *MODULE_PREFIX = "CommandSerial";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CommandSerial::CommandSerial(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig)
    : SysModBase(pModuleName, defaultConfig, pGlobalConfig, pMutableConfig)
{
    // Config variables
    _isEnabled = false;
    _uartNum = 0;
    _baudRate = 912600;
    _txPin = 0;
    _rxPin = 0;
    _rxBufSize = 1024;
    _txBufSize = 1024;
    _hdlcRxMaxLen = 1024;
    _isInitialised = false;

    // EndpointID
    _protocolEndpointID = ProtocolEndpointManager::UNDEFINED_ID;
}

CommandSerial::~CommandSerial()
{
    if (_isInitialised)
        uart_driver_delete((uart_port_t)_uartNum);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandSerial::setup()
{
    // Apply config
    applySetup();
}

void CommandSerial::applySetup()
{
    // Clear previous config if we've been here before
    if (_isInitialised)
        uart_driver_delete((uart_port_t)_uartNum);
    _isInitialised = false;

    // Enable
    _isEnabled = configGetLong("enable", 0) != 0;

    // Port
    _uartNum = configGetLong("uartNum", 80);

    // Baud
    _baudRate = configGetLong("baudRate", 912600);

    // Protocol
    _protocol = configGetString("protocol", "");

    // Pins
    _rxPin = configGetLong("rxPin", -1);
    _txPin = configGetLong("txPin", -1);

    // Buffers
    _rxBufSize = configGetLong("rxBufSize", 1024);
    _txBufSize = configGetLong("txBufSize", 1024);
    _hdlcRxMaxLen = configGetLong("hdlcRxMaxLen", 1024);

    LOG_I(MODULE_PREFIX, "setup enabled %s uartNum %d baudRate %d txPin %d rxPin %d rxBufSize %d txBufSize %d hdlcMaxLen %d protocol %s", 
                    _isEnabled ? "YES" : "NO", _uartNum, _baudRate, _txPin, _rxPin, _rxBufSize, _txBufSize, _hdlcRxMaxLen, _protocol.c_str());

    if (_isEnabled && (_rxPin != -1) && (_txPin != -1))
    {

        // Configure UART. Note that REF_TICK is used so that the baud rate remains
        // correct while APB frequency is changing in light sleep mode
        const uart_config_t uart_config = {
                .baud_rate = _baudRate,
                .data_bits = UART_DATA_8_BITS,
                .parity = UART_PARITY_DISABLE,
                .stop_bits = UART_STOP_BITS_1,
                .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
                .rx_flow_ctrl_thresh = 10,
                .use_ref_tick = false,
        };
        esp_err_t err = uart_param_config((uart_port_t)_uartNum, &uart_config);
        if (err != ESP_OK)
        {
            LOG_E(MODULE_PREFIX, "Failed to initialize uart, err %d", err);
            return;
        }

        // Setup pins
        err = uart_set_pin((uart_port_t)_uartNum, _txPin, _rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
        if (err != ESP_OK)
        {
            LOG_E(MODULE_PREFIX, "Failed to set uart pins, err %d", err);
            return;
        }

        // Delay before UART change
        // TODO - what does this achieve?
        vTaskDelay(1);

        // Install UART driver for interrupt-driven reads and writes
        err = uart_driver_install((uart_port_t)_uartNum, _rxBufSize, _txBufSize, 0, NULL, 0);
        if (err != ESP_OK)
        {
            LOG_E(MODULE_PREFIX, "Failed to install uart driver, err %d", err);
            return;
        }

        _isInitialised = true;
    }

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandSerial::service()
{
    if (!_isInitialised)
        return;

    // Check anything available
    size_t numCharsAvailable = 0;
    esp_err_t err = uart_get_buffered_data_len((uart_port_t)_uartNum, &numCharsAvailable);
    if ((err == ESP_OK) && (numCharsAvailable > 0))
    {
        // Get data
        // TODO - fixed size buffer - review
        static const int MAX_BYTES_PER_CALL = 100;
        uint8_t buf[MAX_BYTES_PER_CALL];
        uint32_t bytesToGet = numCharsAvailable;
        if (bytesToGet > MAX_BYTES_PER_CALL)
            bytesToGet = MAX_BYTES_PER_CALL;
        uint32_t bytesRead = uart_read_bytes((uart_port_t)_uartNum, buf, bytesToGet, 1);
        if (bytesRead != 0)
        {
            // LOG_D(MODULE_PREFIX, "service charsAvail %d ch %02x", numCharsAvailable, buf[0]);
            // Send to protocol handler
            if (getProtocolEndpointManager())
                getProtocolEndpointManager()->handleInboundMessage(_protocolEndpointID, buf, bytesRead);
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Endpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandSerial::addRestAPIEndpoints(RestAPIEndpointManager &endpointManager)
{
}

void CommandSerial::addProtocolEndpoints(ProtocolEndpointManager &endpointManager)
{
    // Register as a channel of protocol messages
    _protocolEndpointID = endpointManager.registerChannel(_protocol.c_str(),
            std::bind(&CommandSerial::sendMsg, this, std::placeholders::_1),
            modName(),
            std::bind(&CommandSerial::readyToSend, this));
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send message
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CommandSerial::sendMsg(ProtocolEndpointMsg& msg)
{
    // Debug
    // LOG_I(MODULE_PREFIX, "sendMsg channelID %d, direction %s msgNum %d, len %d",
    //         msg.getChannelID(), msg.getDirectionAsString(msg.getDirection()), msg.getMsgNumber(), msg.getBufLen());

    if (!_isInitialised)
        return false;

    // Send the message
    int bytesSent = uart_write_bytes((uart_port_t)_uartNum, (const char*)msg.getBuf(), msg.getBufLen());
    if (bytesSent != msg.getBufLen())
    {
        LOG_W(MODULE_PREFIX, "sendMsg channelID %d, direction %s msgNum %d, len %d only wrote %d bytes",
                msg.getChannelID(), msg.getDirectionAsString(msg.getDirection()), msg.getMsgNumber(), msg.getBufLen(), bytesSent);

        return false;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Ready to send indicator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CommandSerial::readyToSend()
{
    // TODO - handle ready to send
    return true;
}
