/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Serial Console
// Provides serial terminal access to REST API and diagnostics
//
// Rob Dobson 2018-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Logger.h>
#include "SerialConsole.h"
#include <RestAPIEndpointManager.h>
#include "ProtocolEndpointManager.h"
#include <ConfigBase.h>
#include <driver/uart.h>

// Log prefix
static const char *MODULE_PREFIX = "SerialConsole";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SerialConsole::SerialConsole(const char* pModuleName, ConfigBase& defaultConfig, ConfigBase* pGlobalConfig, ConfigBase* pMutableConfig)
        : SysModBase(pModuleName, defaultConfig, pGlobalConfig, pMutableConfig)
{
    _curLine.reserve(MAX_REGULAR_LINE_LEN);
    _prevChar = -1;
    _cmdRxState = CommandRx_idle;
    _isEnabled = 0;
    _crlfOnTx = 1;
    _baudRate = 115200;
    _uartNum = 0;
    _isInitialised = false;

    // EndpointID
    _protocolEndpointID = ProtocolEndpointManager::UNDEFINED_ID;    
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SerialConsole::setup()
{
    // Get params
    _isEnabled = configGetLong("enable", 0) != 0;
    _crlfOnTx = configGetLong("crlfOnTx", 1);
    _uartNum = configGetLong("uartNum", 0);
    _baudRate = configGetLong("baudRate", 115200);

    // Protocol
    _protocol = configGetString("protocol", "RICSerial");

    // Delay before UART change
    vTaskDelay(1);

    // Configure UART. Note that REF_TICK is used so that the baud rate remains
    // correct while APB frequency is changing in light sleep mode
    const uart_config_t uart_config = {
            .baud_rate = _baudRate,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .rx_flow_ctrl_thresh = 100,
            .use_ref_tick = true,
    };
    esp_err_t err = uart_param_config((uart_port_t)_uartNum, &uart_config);
    if (err != ESP_OK)
    {
        LOG_E(MODULE_PREFIX, "Failed to initialize uart, err %d", err);
        return;
    }

    // Delay after UART change
    vTaskDelay(1);

    // Debug
    LOG_I(MODULE_PREFIX, "setup enabled %s uartNum %d baudRate %d crlfOnTx %s", 
                _isEnabled ? "YES" : "NO", _uartNum, _baudRate, _crlfOnTx ? "YES" : "NO");

    // Install UART driver for interrupt-driven reads and writes
    err = uart_driver_install((uart_port_t)_uartNum, 256, 1024, 0, NULL, 0);
    if (err != ESP_OK)
    {
        LOG_E(MODULE_PREFIX, "Failed to install uart driver, err %d", err);
        return;
    }
    _isInitialised = true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Endpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SerialConsole::addRestAPIEndpoints(RestAPIEndpointManager& endpoints)
{
}

void SerialConsole::addProtocolEndpoints(ProtocolEndpointManager &endpointManager)
{
    // Register as a channel of protocol messages
    _protocolEndpointID = endpointManager.registerChannel(_protocol.c_str(),
            std::bind(&SerialConsole::sendMsg, this, std::placeholders::_1),
            modName(),
            std::bind(&SerialConsole::readyToSend, this));
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get from terminal
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int SerialConsole::getChar()
{
    if (_isEnabled)
    {
        // Check anything available
        size_t numCharsAvailable = 0;
        esp_err_t err = uart_get_buffered_data_len((uart_port_t)_uartNum, &numCharsAvailable);
        if ((err == ESP_OK) && (numCharsAvailable > 0))
        {
            // Get char
            uint8_t charRead;
            if (uart_read_bytes((uart_port_t)_uartNum, &charRead, 1, 1) > 0)
                return charRead;
        }
    }
    return -1;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Put to terminal
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SerialConsole::putStr(const char* pStr)
{
    if (_isEnabled)
    {
        uart_write_bytes((uart_port_t)_uartNum, pStr, strlen(pStr));
    }
}

void SerialConsole::putStr(const String& str)
{
    if (_isEnabled)
    {
        uart_write_bytes((uart_port_t)_uartNum, str.c_str(), str.length());
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get the state of the reception of Commands 
// Maybe:
//   idle = 'i' = no command entry in progress,
//   newChar = XOFF = new command char received since last call - pause transmission
//   waiting = 'w' = command incomplete but no new char since last call
//   complete = XON = command completed - resume transmission
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SerialConsole::CommandRxState SerialConsole::getXonXoff()
{
    char curSt = _cmdRxState;
    if (_cmdRxState == CommandRx_complete)
    {
        // Serial.printf("<COMPLETE>");
        _cmdRxState = CommandRx_idle;
    }
    else if (_cmdRxState == CommandRx_newChar)
    {
        // Serial.printf("<NEWCH>");
        _cmdRxState = CommandRx_waiting;
    }
    return curSt;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SerialConsole::service()
{
    // Check for char
    int ch = getChar();
    if (ch == -1)
        return;

    // Check for MSB set
    if (ch >= 128)
    {
        // LOG_I(MODULE_PREFIX, "MSB set on rx %02x", ch);
        int decodedByte = _protocolOverAscii.decodeByte(ch);
        if (decodedByte != -1)
        {
            uint8_t rxBuf[1] = {(uint8_t)decodedByte};
            if (getProtocolEndpointManager())
                getProtocolEndpointManager()->handleInboundMessage(_protocolEndpointID, rxBuf, 1);
            // LOG_I(MODULE_PREFIX, "byte rx %02x", rxBuf[0]);
        }
        return;
    }

    // Check for line end
    if ((ch == '\r') || (ch == '\n'))
    {
        // Check for terminal sending a CRLF sequence
        if (_prevChar == '\r')
        {
            _prevChar = ' ';
            return;
        }
        _prevChar = ch;

        // Check if empty line - show menu
        if (_curLine.length() <= 0)
        {
            // Show endpoints
            showEndpoints();
            return;
        }

        putStr(_crlfOnTx ? "\r\n" : "\n");
        // Check for immediate instructions
        LOG_D(MODULE_PREFIX, "CommsSerial: ->cmdInterp cmdStr %s", _curLine.c_str());
        String retStr;
        if (getRestAPIEndpoints())
            getRestAPIEndpoints()->handleApiRequest(_curLine.c_str(), retStr);
        // Display response
        putStr(retStr);
        putStr(_crlfOnTx ? "\r\n" : "\n");

        // Reset line
        _curLine = "";
        _cmdRxState = CommandRx_complete;
        return;
    }

    // Store previous char for CRLF checks
    _prevChar = ch;

    // Check line not too long
    if (_curLine.length() >= ABS_MAX_LINE_LEN)
    {
        _curLine = "";
        _cmdRxState = CommandRx_idle;
        return;
    }

    // Check for backspace
    if (ch == 0x08)
    {
        if (_curLine.length() > 0)
        {
            _curLine.remove(_curLine.length() - 1);
            char tmpStr[4] = { (char)ch, ' ', (char)ch, '\0'};
            putStr(tmpStr);
        }
        return;
    }
    else if (ch == '?')
    {
        // Check if empty line - show menu
        if (_curLine.length() <= 0)
        {
            // Show endpoints
            showEndpoints();
            return;
        }
    }

    // Output for user to see
    if (_curLine.length() == 0)
        putStr(_crlfOnTx ? "\r\n" : "\n");
    char tmpStr[2] = {(char)ch, '\0'};
    putStr(tmpStr);

    // Add char to line
    _curLine.concat((char)ch);

    // Set state to show we're busy getting a command
    _cmdRxState = CommandRx_newChar;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// showEndpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SerialConsole::showEndpoints()
{
    if (!getRestAPIEndpoints())
        return;
    for (int i = 0; i < getRestAPIEndpoints()->getNumEndpoints(); i++)
    {
        RestAPIEndpointDef* pEndpoint = getRestAPIEndpoints()->getNthEndpoint(i);
        if (!pEndpoint)
            continue;
        putStr(String(" ") + pEndpoint->_endpointStr + String(": ") +  pEndpoint->_description + (_crlfOnTx ? "\r\n" : "\n"));
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send message
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool SerialConsole::sendMsg(ProtocolEndpointMsg& msg)
{
    // Debug
    // LOG_I(MODULE_PREFIX, "sendMsg channelID %d, direction %s msgNum %d, len %d",
    //         msg.getChannelID(), msg.getDirectionAsString(msg.getDirection()), msg.getMsgNumber(), msg.getBufLen());

    // Check valid
    if (!_isInitialised)
        return false;

    // Encode
    uint32_t encodedFrameMax = msg.getBufLen() * 2 > PROTOCOL_OVER_ASCII_MSG_MAX_LEN ? msg.getBufLen() * 2 : PROTOCOL_OVER_ASCII_MSG_MAX_LEN;
    uint8_t encodedFrame[encodedFrameMax];
    uint32_t encodedFrameLen = _protocolOverAscii.encodeFrame(msg.getBuf(), msg.getBufLen(), encodedFrame, encodedFrameMax);

    // Send the message
    int bytesSent = uart_write_bytes((uart_port_t)_uartNum, (const char*)encodedFrame, encodedFrameLen);
    if (bytesSent != encodedFrameLen)
    {
        LOG_W(MODULE_PREFIX, "sendMsg channelID %d, direction %s msgNum %d, len %d only wrote %d bytes",
                msg.getChannelID(), msg.getDirectionAsString(msg.getDirection()), msg.getMsgNumber(), encodedFrameLen, bytesSent);

        return false;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Ready to send indicator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool SerialConsole::readyToSend()
{
    // TODO - handle ready to send
    return true;
}
