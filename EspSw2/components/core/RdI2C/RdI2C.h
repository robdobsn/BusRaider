/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RdI2C
// I2C implemented using direct ESP32 register access
//
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <Utils.h>
#include <stdint.h>
#include <soc/i2c_struct.h>
#include <soc/i2c_reg.h>
#include "esp_attr.h"
#include "esp_intr_alloc.h"
#include "WString.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// #define DEBUG_RICI2C_ISR
// #define DEBUG_RICI2C_ISR_ALL_SOURCES
// #define DEBUG_RICI2C_ISR_ON_FAIL

class RdI2C
{
public:
    RdI2C();
    virtual ~RdI2C();

    // Init/de-init
    bool init(uint8_t i2cPort, uint16_t pinSDA, uint16_t pinSCL, uint32_t busFrequency, 
                uint32_t busFilteringLevel = DEFAULT_BUS_FILTER_LEVEL);

    // Busy
    bool isBusy();

    // Access result code
    enum AccessResultCode
    {
        ACCESS_RESULT_PENDING,
        ACCESS_RESULT_OK,
        ACCESS_RESULT_HW_TIME_OUT,
        ACCESS_RESULT_ACK_ERROR,
        ACCESS_RESULT_ARB_LOST,
        ACCESS_RESULT_SW_TIME_OUT,
        ACCESS_RESULT_INVALID,
        ACCESS_RESULT_NOT_READY,
        ACCESS_RESULT_INCOMPLETE
    };

    // Access the bus
    AccessResultCode access(uint16_t address, uint8_t* pWriteBuf, uint32_t numToWrite,
                    uint8_t* pReadBuf, uint32_t numToRead, uint32_t& numRead);

    // Debugging
    class I2CStats
    {
    public:
        I2CStats()
        {
            clear();
        }
        void IRAM_ATTR clear()
        {
            isrCount = 0;
            startCount = 0;
            ackErrCount = 0;
            engineTimeOutCount = 0;
            softwareTimeOutCount = 0;
            transCompleteCount = 0;
            masterTransCompleteCount = 0;
            arbitrationLostCount = 0;
            txFifoEmptyCount = 0;
            txSendEmptyCount = 0;
            incompleteTransaction = 0;
        }
        void IRAM_ATTR update(uint32_t rawFlags)
        {
            isrCount++;
            if (rawFlags & I2C_TRANS_START_INT_ST)
                startCount++;
            if (rawFlags & I2C_ACK_ERR_INT_ST)
                ackErrCount++;
            if (rawFlags & I2C_TIME_OUT_INT_ST)
                engineTimeOutCount++;
            if (rawFlags & I2C_TRANS_COMPLETE_INT_ST)
                transCompleteCount++;
            if (rawFlags & I2C_ARBITRATION_LOST_INT_ST)
                arbitrationLostCount++;
            if (rawFlags & I2C_MASTER_TRAN_COMP_INT_ST)
                masterTransCompleteCount++;
            if (rawFlags & I2C_TXFIFO_EMPTY_INT_ST)
                txFifoEmptyCount++;
            if (rawFlags & I2C_TX_SEND_EMPTY_INT_ST)
                txSendEmptyCount++;
        }
        void IRAM_ATTR recordSoftwareTimeout()
        {
            softwareTimeOutCount++;
        }
        void IRAM_ATTR recordIncompleteTransaction()
        {
            incompleteTransaction++;
        }
        String debugStr()
        {
            char outStr[200];
            snprintf(outStr, sizeof(outStr), "ISRs %d Starts %d AckErrs %d EngTimO %d TransComps %d ArbLost %d MastTransComp %d SwTimO %d TxFIFOmt %d TxSendMT %d incomplete %d", 
                            isrCount, startCount, ackErrCount, engineTimeOutCount, transCompleteCount,
                            arbitrationLostCount,  masterTransCompleteCount, softwareTimeOutCount, 
                            txFifoEmptyCount, txSendEmptyCount, incompleteTransaction);
            return outStr;
        }
        uint32_t isrCount;
        uint32_t startCount;
        uint32_t ackErrCount;
        uint32_t engineTimeOutCount;
        uint32_t transCompleteCount;
        uint32_t arbitrationLostCount;
        uint32_t softwareTimeOutCount;
        uint32_t masterTransCompleteCount;
        uint32_t txFifoEmptyCount;
        uint32_t txSendEmptyCount;
        uint32_t incompleteTransaction;
    };

    // Get stats
    I2CStats getStats()
    {
        return _i2cStats;
    }

    // Consts
    static const uint32_t DEFAULT_BUS_FILTER_LEVEL = 7;

    // Debug access result
    static String getAccessResultStr(AccessResultCode accessResultCode)
    {
        switch(accessResultCode)
        {
        case ACCESS_RESULT_PENDING: return "pending";
        case ACCESS_RESULT_OK: return "ok";
        case ACCESS_RESULT_HW_TIME_OUT: return "hwTimeOut";
        case ACCESS_RESULT_ACK_ERROR: return "ackError";
        case ACCESS_RESULT_ARB_LOST: return "arbLost";
        case ACCESS_RESULT_SW_TIME_OUT: return "swTimeOut";
        case ACCESS_RESULT_INVALID: return "invalid";
        case ACCESS_RESULT_NOT_READY: return "notReady";
        case ACCESS_RESULT_INCOMPLETE: return "incomplete";
        default: return "unknown";
        }
    }

private:
    // Settings
    uint8_t _i2cPort;
    int16_t _pinSDA;
    int16_t _pinSCL;
    uint32_t _busFrequency;
    uint32_t _busFilteringLevel;

    // Init flag
    bool _isInitialised;

    // Address bytes to add to FIFO when required
    uint8_t _startAddrPlusRW;
    bool _startAddrPlusRWRequired;
    uint8_t _restartAddrPlusRW;
    bool _restartAddrPlusRWRequired;

    // Read buffer
    volatile uint32_t _readBufPos;
    volatile uint8_t* _readBufStartPtr;
    volatile uint32_t _readBufMaxLen;

    // Write buffer
    volatile uint32_t _writeBufPos;
    volatile const uint8_t* _writeBufStartPtr;
    volatile uint32_t _writeBufLen;

    // Access result code
    volatile AccessResultCode _accessResultCode;

    // Interrupt handle, clear and enable flags
    intr_handle_t _i2cISRHandle;
    uint32_t _interruptClearFlags;
    uint32_t _interruptEnFlags;

    // FIFO size
    static const uint32_t I2C_ENGINE_FIFO_SIZE = sizeof(I2C0.ram_data)/sizeof(I2C0.ram_data[0]);

    // Command max send/receive size
    static const uint32_t I2C_ENGINE_CMD_MAX_TX_BYTES = 255;
    static const uint32_t I2C_ENGINE_CMD_MAX_RX_BYTES = 255;

    // Command queue siz
    static const uint32_t I2C_ENGINE_CMD_QUEUE_SIZE = sizeof(I2C0.command)/sizeof(I2C0.command[0]);

    // Interrupts flags
    static const uint32_t INTERRUPT_BASE_ENABLES = 
                    I2C_ACK_ERR_INT_ENA | 
                    I2C_TIME_OUT_INT_ENA | 
                    I2C_TRANS_COMPLETE_INT_ENA |
                    I2C_ARBITRATION_LOST_INT_ENA | 
                    I2C_TXFIFO_EMPTY_INT_ENA | 
                    I2C_RXFIFO_FULL_INT_ENA 
#ifdef DEBUG_RICI2C_ISR_ALL_SOURCES
                    | I2C_TRANS_START_INT_ENA 
                    | I2C_END_DETECT_INT_ENA |
                    I2C_MASTER_TRAN_COMP_INT_ENA | 
                    I2C_RXFIFO_OVF_INT_ENA
#endif
                    ;

    // Interrupt and FIFO consts
    static const uint32_t INTERRUPT_BASE_CLEARS = INTERRUPT_BASE_ENABLES;
    static const uint32_t TX_FIFO_NEARING_EMPTY_INT = I2C_TXFIFO_EMPTY_INT_ENA;
    static const uint32_t RX_FIFO_NEARING_FULL_INT = I2C_RXFIFO_FULL_INT_ENA;

    // State
    static const uint32_t SCL_STATUS_START = 1;
    static const uint32_t MAIN_STATUS_SEND_ACK = 5;
    static const uint32_t MAIN_STATUS_WAIT_ACK = 6;

    // Access type
    enum I2CAccessType
    {
        ACCESS_POLL,
        ACCESS_READ_ONLY,
        ACCESS_WRITE_ONLY,
        ACCESS_WRITE_RESTART_READ,        
    };

    // Helpers
    bool ensureI2CReady();
    void prepareI2CAccess();
    void deinit();
    void reinitI2CModule();
    bool setBusFrequency(uint32_t busFreq);
    uint32_t getApbFrequency();
    void setI2CCommand(uint32_t cmdIdx, uint8_t op_code, uint8_t byte_num, bool ack_val, bool ack_exp, bool ack_en);
    bool initInterrupts();
    void initBusFiltering();
    bool checkI2CLinesOk();
    static void IRAM_ATTR i2cISRStatic(void* arg);
    void IRAM_ATTR i2cISR();
    uint32_t IRAM_ATTR fillTxFifo();
    uint32_t IRAM_ATTR emptyRxFifo();

    // Debugging
    static String debugMainStatusStr(const char* prefix, uint32_t statusFlags);
    static String debugFIFOStatusStr(const char* prefix, uint32_t statusFlags);
    static String debugINTFlagStr(const char* prefix, uint32_t statusFlags);
    void debugShowStatus(const char* prefix, uint32_t addr);

    // I2C stats
    I2CStats _i2cStats;

    // Debug I2C ISR
#if defined(DEBUG_RICI2C_ISR) || defined(DEBUG_RICI2C_ISR_ON_FAIL)
    class DebugI2CISRElem
    {
    public:
        DebugI2CISRElem()
        {
            clear();
        }
        void IRAM_ATTR clear()
        {
            _micros = 0;
            _msg[0] = 0;
            _intStatus = 0;
        }
        void IRAM_ATTR set(const char* msg, uint32_t intStatus, 
                        uint32_t mainStatus, uint32_t txFifoStatus)
        {
            _micros = micros();
            _intStatus = intStatus;
            _mainStatus = mainStatus;
            _txFifoStatus = txFifoStatus;
            strlcpy(_msg, msg, sizeof(_msg));
        }
        String toStr()
        {
            char outStr[300];
            snprintf(outStr, sizeof(outStr), "%lld %s INT (%08x) %s MAIN (%08x) %s FIFO (%08x) %s", 
                        _micros, _msg,
                        _intStatus, debugINTFlagStr("", _intStatus).c_str(),
                        _mainStatus, debugMainStatusStr("", _mainStatus).c_str(), 
                        _txFifoStatus, debugFIFOStatusStr("", _txFifoStatus).c_str()
            );
            return outStr;
        }
        uint64_t _micros;
        char _msg[20];
        uint32_t _intStatus;
        uint32_t _mainStatus;
        uint32_t _txFifoStatus;
    };
    class DebugI2CISR
    {
    public:
        DebugI2CISR()
        {
            clear();
        }
        void IRAM_ATTR clear()
        {
            _i2cISRDebugCount = 0;
        }
        static const uint32_t DEBUG_RICI2C_ISR_MAX = 100;
        DebugI2CISRElem _i2cISRDebugElems[DEBUG_RICI2C_ISR_MAX];
        uint32_t _i2cISRDebugCount;
        void IRAM_ATTR debugISRAdd(const char* msg, uint32_t intStatus, 
                        uint32_t mainStatus, uint32_t txFifoStatus)
        {
            if (_i2cISRDebugCount >= DEBUG_RICI2C_ISR_MAX)
                return;
            _i2cISRDebugElems[_i2cISRDebugCount].set(msg, intStatus, mainStatus, txFifoStatus);
            _i2cISRDebugCount++;
        }
        uint32_t getCount()
        {
            return _i2cISRDebugCount;
        }
        DebugI2CISRElem& getElem(uint32_t i)
        {
            return _i2cISRDebugElems[i];
        }
    };
    DebugI2CISR _debugI2CISR;
#endif
};
