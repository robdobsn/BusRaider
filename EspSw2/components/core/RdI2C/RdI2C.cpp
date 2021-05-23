/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RdI2C
// I2C implemented using direct ESP32 register access
//
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "RdI2C.h"
#include "Logger.h"
#include "Utils.h"
#include "ArduinoTime.h"
#include <driver/gpio.h>
#include "soc/dport_reg.h"
#include "soc/rtc.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Consts
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static const char* MODULE_PREFIX = "RdI2C";

// #define DEBUG_ACCESS
// #define DEBUG_TIMING
// #define DEBUG_TIMEOUT_CALCS
// #define DEBUG_I2C_COMMANDS
// #define WARN_ON_BUS_IS_BUSY

// #define WARN_ON_BUS_CANNOT_BE_RESET

static const uint8_t ESP32_I2C_CMD_RSTART = 0;
static const uint8_t ESP32_I2C_CMD_WRITE = 1;
static const uint8_t ESP32_I2C_CMD_READ = 2;
static const uint8_t ESP32_I2C_CMD_STOP = 3;
static const uint8_t ESP32_I2C_CMD_END = 4;

#define I2C_DEVICE (_i2cPort == 0 ? I2C0 : I2C1)

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor / Destructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RdI2C::RdI2C()
{
    _i2cPort = 0;
    _pinSDA = -1;
    _pinSCL = -1;
    _busFrequency = 100000;
    _busFilteringLevel = DEFAULT_BUS_FILTER_LEVEL;
    _isInitialised = false;
    _readBufPos = 0;
    _readBufMaxLen = 0;
    _readBufStartPtr = nullptr;
    _writeBufPos = 0;
    _writeBufLen = 0;
    _writeBufStartPtr = nullptr;
    _startAddrPlusRWRequired = false;
    _restartAddrPlusRWRequired = false;
    _accessResultCode = ACCESS_RESULT_PENDING;
    _interruptClearFlags = 0;
    _interruptEnFlags = 0;

    // Interrupt handle
    _i2cISRHandle = nullptr;
}

RdI2C::~RdI2C()
{
    // De-init
    deinit();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Initialisation function
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdI2C::init(uint8_t i2cPort, uint16_t pinSDA, uint16_t pinSCL, uint32_t busFrequency,
                uint32_t busFilteringLevel)
{
    // de-init first in case already initialised
    deinit();

    // Settings
    _i2cPort = i2cPort;
    _pinSDA = pinSDA;
    _pinSCL = pinSCL;
    _busFrequency = busFrequency;
    _busFilteringLevel = busFilteringLevel;

    // Setup interrupts on the required port
    initInterrupts();

    // Use re-init sequence as we have to do this on a lockup too
    reinitI2CModule();

    // Attach SDA and SCL
    if (_pinSDA >= 0)
    {
        uint32_t sdaIdx = _i2cPort == 0 ? I2CEXT0_SDA_OUT_IDX : I2CEXT1_SDA_OUT_IDX;
        gpio_set_level((gpio_num_t)_pinSDA, 1);
        gpio_set_direction((gpio_num_t)_pinSDA, GPIO_MODE_INPUT_OUTPUT_OD);
        gpio_pullup_en((gpio_num_t)_pinSDA);
        gpio_matrix_out((gpio_num_t)_pinSDA, sdaIdx, false, false);
        gpio_matrix_in((gpio_num_t)_pinSDA, sdaIdx, false);
    }
    if (_pinSCL >= 0)
    {
        uint32_t sclIdx = _i2cPort == 0 ? I2CEXT0_SCL_OUT_IDX : I2CEXT1_SCL_OUT_IDX;
        gpio_set_level((gpio_num_t)_pinSCL, 1);
        gpio_set_direction((gpio_num_t)_pinSCL, GPIO_MODE_INPUT_OUTPUT_OD);
        gpio_pullup_en((gpio_num_t)_pinSCL);
        gpio_matrix_out((gpio_num_t)_pinSCL, sclIdx, false, false);
        gpio_matrix_in((gpio_num_t)_pinSCL, sclIdx, false);
    }

    // Enable bus filtering if required
    initBusFiltering();

    // Now inititalized
    _isInitialised = true;
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Test if bus is busy
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdI2C::isBusy()
{
    // Check if hardware is ready
    if (!_isInitialised)
        return true;
    bool isBusy = I2C_DEVICE.status_reg.bus_busy;
#ifdef DEBUG_ACCESS
    LOG_I(MODULE_PREFIX, "isBusy %s", isBusy ? "BUSY" : "IDLE");
#endif
    return isBusy;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Access the I2C bus
// Note:
// - a zero length read and zero length write sends address with R/W flag indicating write to test if a node ACKs
// - a write of non-zero length alone does what it says and can be of arbitrary length
// - a read on non-zero length also can be of arbitrary length
// - a write of non-zero length and read of non-zero length is allowed - write occurs first and can only be of max 14 bytes
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RdI2C::AccessResultCode RdI2C::access(uint16_t address, uint8_t* pWriteBuf, uint32_t numToWrite,
                    uint8_t* pReadBuf, uint32_t numToRead, uint32_t& numRead)
{
    // Check valid
    if ((numToWrite > 0) && !pWriteBuf)
        return ACCESS_RESULT_INVALID;
    if ((numToRead > 0) && !pReadBuf)
        return ACCESS_RESULT_INVALID;

#if defined(DEBUG_RICI2C_ISR) || defined(DEBUG_RICI2C_ISR_ON_FAIL)
    _debugI2CISR.clear();
#endif

    // Ensure the engine is ready
    if (!ensureI2CReady())
        return ACCESS_RESULT_NOT_READY;

    // Check what operation is required
    I2CAccessType i2cOpType = ACCESS_POLL;
    if ((numToRead > 0) && (numToWrite > 0))
        i2cOpType = ACCESS_WRITE_RESTART_READ;
    else if (numToRead > 0)
        i2cOpType = ACCESS_READ_ONLY;
    else if (numToWrite > 0)
        i2cOpType = ACCESS_WRITE_ONLY;

    // The ESP32 I2C engine can accommodate up to 14 write/read commands
    // each of which can request up to 255 bytes of writing/reading
    // If reading is involved then an RSTART command and second address is needed
    // as well as a final command reading one byte which is NACKed
    // Check max lengths to write/read (including the RSTART command if needed) 
    // don't result in exceeding these 14 commands
    uint32_t writeCommands = 0;
    writeCommands = (numToWrite + 1 + I2C_ENGINE_CMD_MAX_TX_BYTES - 1) / I2C_ENGINE_CMD_MAX_TX_BYTES;
    uint32_t readCommands = 0;
    if ((i2cOpType == ACCESS_READ_ONLY) || (i2cOpType == ACCESS_WRITE_RESTART_READ))
        readCommands = 1 + ((numToRead + I2C_ENGINE_CMD_MAX_RX_BYTES - 1) / I2C_ENGINE_CMD_MAX_RX_BYTES);
    uint32_t rstartAndSecondAddrCommands = (i2cOpType == ACCESS_READ_ONLY ? 1 :
                                    (i2cOpType == ACCESS_WRITE_RESTART_READ) ? 2 : 0);
    if (writeCommands + readCommands + rstartAndSecondAddrCommands > (I2C_ENGINE_CMD_QUEUE_SIZE - 2))
        return ACCESS_RESULT_INVALID;

    // Prepare I2C engine for access
    prepareI2CAccess();

    // Command index
    uint32_t cmdIdx = 0;

    // Start condition
    setI2CCommand(cmdIdx++, ESP32_I2C_CMD_RSTART, 0, false, false, false);

    // Set the address write command(s) - needed in all cases
    // enable ACK processing and check we received an ACK
    // an I2C_NACK_INT will be generated if there is a NACK
    // Add one as the address needs to be written
    uint32_t writeAmountLeft = numToWrite + 1;
    for (uint32_t writeCmdIdx = 0; writeCmdIdx < writeCommands; writeCmdIdx++)
    {
        uint32_t writeAmount = (writeAmountLeft > I2C_ENGINE_CMD_MAX_TX_BYTES) ? I2C_ENGINE_CMD_MAX_TX_BYTES : writeAmountLeft;
        setI2CCommand(cmdIdx++, ESP32_I2C_CMD_WRITE, writeAmount, false, false, true);
        if (writeAmountLeft > 0)
            writeAmountLeft -= writeAmount;
#ifdef DEBUG_I2C_COMMANDS
        LOG_I(MODULE_PREFIX, "access cmdIdx %d writeAmount %d writeAmountLeft %d", 
                        MODULE_PREFIX, cmdIdx-1, writeAmount, writeAmountLeft);
#endif
    }

    // Handle adding a re-start command which is needed only for ACCESS_WRITE_RESTART_READ
    if (i2cOpType == ACCESS_WRITE_RESTART_READ)
        setI2CCommand(cmdIdx++, ESP32_I2C_CMD_RSTART, 0, false, false, false);

    // Handle adding an address+READ which is needed in ACCESS_WRITE_RESTART_READ
    if (i2cOpType == ACCESS_WRITE_RESTART_READ)
        setI2CCommand(cmdIdx++, ESP32_I2C_CMD_WRITE, 1, false, false, true);

    // Handle adding of read commands
    uint32_t readAmountLeft = numToRead;
    for (uint32_t readCmdIdx = 0; readCmdIdx < readCommands; readCmdIdx++)
    {
        // Calculate the amount to read in this command - if it is the penultimate command then
        // reduce the count by 1 to ensure only 1 byte is read in the last command (which is NACKed)
        uint32_t readAmount = (readAmountLeft > I2C_ENGINE_CMD_MAX_RX_BYTES) ? I2C_ENGINE_CMD_MAX_RX_BYTES : readAmountLeft;
        if (readCmdIdx == readCommands-2)
            readAmount -= 1;
        // An ACK should be sent after each byte received
        setI2CCommand(cmdIdx++, ESP32_I2C_CMD_READ, readAmount, readCmdIdx == readCommands-1, false, false);
        if (readAmountLeft > 0)
            readAmountLeft -= readAmount;
#ifdef DEBUG_I2C_COMMANDS
        LOG_I(MODULE_PREFIX, "access cmdIdx %d readAmount %d readAmountLeft %d", cmdIdx-1, readAmount, readAmountLeft);
#endif
    }

    // Stop condition
    setI2CCommand(cmdIdx++, ESP32_I2C_CMD_STOP, 0, false, false, false);

    // Store the read and write buffer pointers and lengths
    _readBufStartPtr = pReadBuf;
    _readBufMaxLen = numToRead;
    _readBufPos = 0;
    _writeBufStartPtr = pWriteBuf;
    _writeBufPos = 0;
    _writeBufLen = numToWrite;

    // Fill the Tx FIFO with as much write data as possible (which may be 0 if we are not writing anything)
    _startAddrPlusRW = (address << 1) | (i2cOpType != ACCESS_READ_ONLY ? 0 : 1);
    _startAddrPlusRWRequired = true;
    _restartAddrPlusRW = (address << 1) | 1;
    _restartAddrPlusRWRequired = i2cOpType == ACCESS_WRITE_RESTART_READ;

    // Fill the Tx FIFO and ensure that FIFO interrupts are disabled if they are not required
    uint32_t interruptsToDisable = fillTxFifo();
    _interruptEnFlags = INTERRUPT_BASE_ENABLES & ~interruptsToDisable;

#ifdef DEBUG_I2C_COMMANDS
    LOG_I(MODULE_PREFIX, "access numWriteCmds %d cmdIdx %d numReadCmds %d numRStarts+2ndAddr %d restartReqd %d\n",
                    writeCommands, cmdIdx, readCommands, rstartAndSecondAddrCommands, _restartAddrPlusRWRequired);
#endif

    // Debug
#ifdef DEBUG_ACCESS
    debugShowStatus("access before: ", address);
#endif

    // Calculate minimum time for entire transaction based on number of bits written/read
    uint32_t totalBytesTxAndRx = (numToRead + 1 + numToWrite + 1);
    uint32_t totalBitsTxAndRx = totalBytesTxAndRx * 10;
    uint32_t minTotalUs = (totalBitsTxAndRx * 1000) / (_busFrequency / 1000);

    // Add overhead for starting/restarting/ending transmission and any clock stretching, etc
    static const uint32_t CLOCK_STRETCH_MAX_PER_BYTE_US = 250;
    uint32_t I2C_START_RESTART_END_OVERHEAD_US = 500 + totalBytesTxAndRx * CLOCK_STRETCH_MAX_PER_BYTE_US;
    uint64_t maxExpectedUs = minTotalUs + I2C_START_RESTART_END_OVERHEAD_US;

#ifdef DEBUG_TIMEOUT_CALCS
    LOG_I(MODULE_PREFIX, "access totalBytesTxAndRx %d totalBitsTxAndRx %d minTotalUs %d overhead %d maxExpectedUs %lld",
                totalBytesTxAndRx, totalBitsTxAndRx, minTotalUs, I2C_START_RESTART_END_OVERHEAD_US, maxExpectedUs);
#endif

    // Clear interrupts and enable
    I2C_DEVICE.int_clr.val = _interruptClearFlags;
    I2C_DEVICE.int_ena.val = _interruptEnFlags;

    // Indicate that a result is pending
    _accessResultCode = ACCESS_RESULT_PENDING;

    // Debug
#ifdef DEBUG_RICI2C_ACCESS
    debugShowStatus("access ready: ", address);
#endif

    // Start communicating
    I2C_DEVICE.ctr.trans_start = 1;

    // Wait for a result
    uint64_t startUs = micros();
    while ((_accessResultCode == ACCESS_RESULT_PENDING) &&
            !Utils::isTimeout((uint64_t)micros(), startUs, maxExpectedUs))
    {
        vTaskDelay(0);
    }

    // Check for software time-out
    if (_accessResultCode == ACCESS_RESULT_PENDING)
    {
        _accessResultCode = ACCESS_RESULT_SW_TIME_OUT;
        _i2cStats.recordSoftwareTimeout();
    }

    // Check all of the I2C commands to ensure everything was marked done
    if (_accessResultCode == ACCESS_RESULT_OK)
    {
        for (uint32_t i = 0; i < cmdIdx; i++)
        {
            if (!I2C_DEVICE.command[i].done)
            {
#ifdef DEBUG_RICI2C_ACCESS
                LOG_I(MODULE_PREFIX, "access incomplete addr %02x writeLen %d readLen %d cmdIdx %d not done", 
                            address, numToWrite, numToRead, i);
#endif
                _accessResultCode = ACCESS_RESULT_INCOMPLETE;
                _i2cStats.recordIncompleteTransaction();
                break;
            }
        }
    }

    // Debug
#ifdef DEBUG_TIMING
    uint64_t nowUs = micros();
    LOG_I(MODULE_PREFIX, "access timing now %lld elapsedUs %lld maxExpectedUs %lld startUs %lld accessResult %s linesHeld %d", 
                nowUs, nowUs-startUs, maxExpectedUs, startUs, 
                getAccessResultStr(_accessResultCode).c_str(), checkI2CLinesOk());
#endif

#if defined(DEBUG_RICI2C_ISR) || defined(DEBUG_RICI2C_ISR_ON_FAIL)
#ifdef DEBUG_RICI2C_ISR_ON_FAIL
    if (_accessResultCode != ACCESS_RESULT_OK) 
#endif
    {
        uint32_t numDebugElems = _debugI2CISR.getCount();
        debugShowStatus("access after: ", address);
        LOG_I(MODULE_PREFIX, "access rslt %s ISR calls ...", getAccessResultStr(_accessResultCode).c_str());
        for (uint32_t i = 0; i < numDebugElems; i++)
            LOG_I(MODULE_PREFIX, "... %s", _debugI2CISR.getElem(i).toStr().c_str());
    }
#endif

    // Empty Rx FIFO to extract any read data
    emptyRxFifo();
    numRead = _readBufPos;

    // Clear the read and write buffer pointers defensively - in case of spurious ISRs after this point
    _readBufStartPtr = nullptr;
    _writeBufStartPtr = nullptr;

    // Debug
#ifdef DEBUG_ACCESS
    LOG_I(MODULE_PREFIX, "access addr %02x %s", address, getAccessResultStr(_accessResultCode).c_str());
    debugShowStatus("access after: ", address);
#endif


    return _accessResultCode;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check that the I2C module is ready and reset it if not
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdI2C::ensureI2CReady()
{
    // Check if busy
    if (isBusy())
    {
#ifdef WARN_ON_BUS_IS_BUSY
        LOG_W(MODULE_PREFIX, "ensureI2CReady bus is busy ... resetting\n");
#endif

        // Should not be busy - so reinit I2C
        reinitI2CModule();

        // Wait a moment
        delayMicroseconds(50);

        // Check if now not busy
        if (isBusy())
        {
            // Something more seriously wrong
#ifdef WARN_ON_BUS_CANNOT_BE_RESET
            LOG_W(MODULE_PREFIX, "ensureI2CReady bus still busy ... FAILED ACCESS lines held %d\n", checkI2CLinesOk());
#endif
            return false;
        }
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Prepare for I2C accsss
// This sets the FIFO thresholds, timeouts, etc
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdI2C::prepareI2CAccess()
{
    // Ensure the state machine is stopped
    I2C_DEVICE.ctr.trans_start = 0;
    
    // Max timeout (0xfffff == 13ms)
    I2C_DEVICE.timeout.tout = 0xfffff;

    // Lock fifos
    I2C_DEVICE.fifo_conf.tx_fifo_rst = 1;
    I2C_DEVICE.fifo_conf.rx_fifo_rst = 1;

    // Set the fifo thresholds to reasonable values given 100KHz bus speed and 180MHz processor clock
    // the threshold must allow for interrupt latency
    I2C_DEVICE.fifo_conf.rx_fifo_full_thrhd = 26;
    I2C_DEVICE.fifo_conf.tx_fifo_empty_thrhd = 6;

    // Release fifos
    I2C_DEVICE.fifo_conf.tx_fifo_rst = 0;
    I2C_DEVICE.fifo_conf.rx_fifo_rst = 0;

    // Disable interrupts and clear
    I2C_DEVICE.int_ena.val = 0;
    I2C_DEVICE.int_clr.val = UINT32_MAX;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Re-init the I2C hardware
// Used initially and when a bus lockup is detected
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdI2C::reinitI2CModule()
{
    // Clear interrupts
    I2C_DEVICE.int_ena.val = 0;
    I2C_DEVICE.int_clr.val = UINT32_MAX;

    // Reset hardware - set reset pin, then set clock en, then clear reset pin
    DPORT_SET_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, _i2cPort == 0 ? DPORT_I2C_EXT0_RST : DPORT_I2C_EXT1_RST);
    DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, _i2cPort == 0 ? DPORT_I2C_EXT0_CLK_EN : DPORT_I2C_EXT1_CLK_EN);
    DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, _i2cPort == 0 ? DPORT_I2C_EXT0_RST : DPORT_I2C_EXT1_RST);

    // Clear hardware control register ...
    // Set master mode, normal sda and scl output, enable the clock
    I2C_DEVICE.ctr.val = 0;
    I2C_DEVICE.ctr.ms_mode = 1;
    I2C_DEVICE.ctr.sda_force_out = 1 ;
    I2C_DEVICE.ctr.scl_force_out = 1 ;
    I2C_DEVICE.ctr.clk_en = 1;

    // Timout for receiving data in APB clock cycles (max value is = 2^20-1 == 1048575)
    I2C_DEVICE.timeout.tout = 400000;

    // Clear FIFO config
    I2C_DEVICE.fifo_conf.val = 0;

    // Set bus frequency
    setBusFrequency(_busFrequency);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Set I2C bus frequency
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdI2C::setBusFrequency(uint32_t busFreq)
{
    static const uint32_t MIN_RATIO_CPU_TICKS_TO_I2C_BUS_TICKS = 100;
    static const uint32_t MAX_BUS_CLOCK_PERIOD = 4095;

    // Check valid
    uint32_t apbFrequency = getApbFrequency(); 
    uint32_t period = (apbFrequency / busFreq) / 2;
    uint32_t minBusFreq = apbFrequency / 8192;
    uint32_t maxBusFreq = apbFrequency / MIN_RATIO_CPU_TICKS_TO_I2C_BUS_TICKS;
    if ((minBusFreq > busFreq) || (maxBusFreq < busFreq))
    {
        LOG_W(MODULE_PREFIX, "setFrequency outOfBounds requested %d min %d max %d modifying...", busFreq, minBusFreq, maxBusFreq);
    }

    // Modify to be valid
    if (period < (MIN_RATIO_CPU_TICKS_TO_I2C_BUS_TICKS/2))
    {
        period = (MIN_RATIO_CPU_TICKS_TO_I2C_BUS_TICKS/2);
        busFreq = apbFrequency / (period * 2);
        LOG_W(MODULE_PREFIX, "setFrequency reduced %d", busFreq);
    } 
    else if (period > MAX_BUS_CLOCK_PERIOD)
    {
        period = MAX_BUS_CLOCK_PERIOD;
        busFreq = apbFrequency / (period * 2);
        LOG_W(MODULE_PREFIX, "setFrequency increased %d", busFreq);
    }

    // Calculate half and quarter periods
    uint32_t halfPeriod = period / 2;
    uint32_t quarterPeriod = period / 4;

    // Set the low-level and high-level width of SCL
    I2C_DEVICE.scl_low_period.period = period;
    I2C_DEVICE.scl_high_period.period = period;

    // Set the start-marker timing between SDA going low and SCL going low
    I2C_DEVICE.scl_start_hold.time = halfPeriod;

    // Set the restart-marker timing between SCL going high and SDA going low
    I2C_DEVICE.scl_rstart_setup.time = halfPeriod;

    // Set timing of stop-marker 
    I2C_DEVICE.scl_stop_hold.time = halfPeriod;
    I2C_DEVICE.scl_stop_setup.time = halfPeriod;

    // Set the time period to hold data after SCL goes low and sampling SDA after SCL going high
    // These are actually not used in master mode
    I2C_DEVICE.sda_hold.time = quarterPeriod;
    I2C_DEVICE.sda_sample.time = quarterPeriod;

    return true;
}

// Get APB frequency (used for I2C)
uint32_t RdI2C::getApbFrequency()
{
    rtc_cpu_freq_config_t conf;
    rtc_clk_cpu_freq_get_config(&conf);
    if(conf.freq_mhz >= 80)
        return 80000000;
    return (conf.source_freq_mhz * 1000000U) / conf.div;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Set a command into the ESP32 I2C command registers
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdI2C::setI2CCommand(uint32_t cmdIdx, uint8_t op_code, uint8_t byte_num, bool ack_val, bool ack_exp, bool ack_en)
{
    // Check valid
    if (cmdIdx >= I2C_ENGINE_CMD_QUEUE_SIZE)
        return;

    // Clear and then set all values
    I2C_DEVICE.command[cmdIdx].val = 0;
    I2C_DEVICE.command[cmdIdx].op_code = op_code;
    I2C_DEVICE.command[cmdIdx].byte_num = byte_num;
    I2C_DEVICE.command[cmdIdx].ack_val = ack_val;
    I2C_DEVICE.command[cmdIdx].ack_exp = ack_exp;
    I2C_DEVICE.command[cmdIdx].ack_en = ack_en;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Initialise interrupts
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdI2C::initInterrupts()
{
    // ISR flags allowing for calling if cache disabled, low/medium priority, shared interrupts
    uint32_t isrFlags = ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LOWMED | ESP_INTR_FLAG_SHARED;

    // Enable flags
    _interruptEnFlags = INTERRUPT_BASE_ENABLES;

    // Clear flags
    _interruptClearFlags = INTERRUPT_BASE_CLEARS;

    // Clear any pending interrupt
    I2C_DEVICE.int_clr.val = _interruptClearFlags;

    // Create ISR using the mask to define which interrupts to test for
    esp_err_t retc = ESP_OK;
    if (_i2cPort == 0)
        esp_intr_alloc_intrstatus(ETS_I2C_EXT0_INTR_SOURCE, isrFlags, (uint32_t)&(I2C_DEVICE.int_status.val), 
                        _interruptEnFlags, i2cISRStatic, this, &_i2cISRHandle);
    else
        esp_intr_alloc_intrstatus(ETS_I2C_EXT1_INTR_SOURCE, isrFlags, (uint32_t)&(I2C_DEVICE.int_status.val), 
                        _interruptEnFlags, i2cISRStatic, this, &_i2cISRHandle);

    // Warn if problems
    if (retc != ESP_OK)
    {
        LOG_W(MODULE_PREFIX, "initInterrupts failed retc %d\n", retc);
    }
    return retc == ESP_OK;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Initialise bus filtering if enabled
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdI2C::initBusFiltering()
{
    I2C_DEVICE.scl_filter_cfg.thres = _busFilteringLevel;
    I2C_DEVICE.sda_filter_cfg.thres = _busFilteringLevel;
    I2C_DEVICE.scl_filter_cfg.en = _busFilteringLevel != 0;
    I2C_DEVICE.sda_filter_cfg.en = _busFilteringLevel != 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check I2C lines are ok - assumes bus is idle so lines should be high
// Checks both I2C lines (SCL and SDA) for pin-held-low problems
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdI2C::checkI2CLinesOk()
{
    // Check if SDA or SCL are held low
    bool sdaHeld = gpio_get_level((gpio_num_t)_pinSDA) == 0;
    bool sclHeld = gpio_get_level((gpio_num_t)_pinSCL) == 0;
    return sdaHeld || sclHeld;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Interrupt handler
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void IRAM_ATTR RdI2C::i2cISRStatic(void* arg)
{
    // Get object
    RdI2C* pArg = (RdI2C*)arg;
    if (pArg)
        pArg->i2cISR();
}

void IRAM_ATTR RdI2C::i2cISR()
{
    uint32_t intStatus = I2C_DEVICE.int_status.val;
#if defined(DEBUG_RICI2C_ISR) || defined(DEBUG_RICI2C_ISR_ON_FAIL)
    _debugI2CISR.debugISRAdd("", intStatus, I2C_DEVICE.status_reg.val, I2C_DEVICE.fifo_st.val);
#endif

    // Update
    _i2cStats.update(intStatus);

    // Track results & interrupts to disable that are no longer needed
    AccessResultCode rsltCode = ACCESS_RESULT_PENDING;
    uint32_t interruptsToDisable = 0;

    // Check which interrupt has occurred
    if (intStatus & I2C_TIME_OUT_INT_ST)
    {
        rsltCode = ACCESS_RESULT_HW_TIME_OUT;
    }
    else if (intStatus & I2C_ACK_ERR_INT_ST)
    {
        rsltCode = ACCESS_RESULT_ACK_ERROR;
    }
    else if (intStatus & I2C_ARBITRATION_LOST_INT_ST)
    {
        rsltCode = ACCESS_RESULT_ARB_LOST;
    }
    else if (intStatus & I2C_TRANS_COMPLETE_INT_ST)
    {
        // Set flag indicating successful completion
        rsltCode = ACCESS_RESULT_OK;
    }
    if (rsltCode != ACCESS_RESULT_PENDING)
    {
        // Disable and clear interrupts
        I2C_DEVICE.int_ena.val = 0;
        I2C_DEVICE.int_clr.val = _interruptClearFlags;

        // Set flag indicating successful completion
        if (_accessResultCode == ACCESS_RESULT_PENDING)
        _accessResultCode = rsltCode;
        return;
    }

    // Check for Tx FIFO needing to be refilled
    if (intStatus & I2C_TXFIFO_EMPTY_INT_ST)
    {
        // Refill the FIFO if possible
        interruptsToDisable |= fillTxFifo();
    }

    // Check for Rx FIFO needing to be emptied
    if (intStatus & I2C_RXFIFO_FULL_INT_ST)
    {
        // Empty the FIFO
        interruptsToDisable |= emptyRxFifo();
    }

    // Disable and clear interrupts
    I2C_DEVICE.int_ena.val = 0;
    I2C_DEVICE.int_clr.val = _interruptClearFlags;

    // Remove enables on interrupts no longer wanted
    _interruptEnFlags &= ~interruptsToDisable;

    // Restore interrupt enables
    I2C_DEVICE.int_ena.val = _interruptEnFlags;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Fill Tx FIFO and return bit mask of interrupts to disable
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t IRAM_ATTR RdI2C::fillTxFifo()
{
    // Check if we need to add the start address+RW to the Tx FIFO
    if (_startAddrPlusRWRequired)
    {
        I2C_DEVICE.fifo_data.val = _startAddrPlusRW;
        _startAddrPlusRWRequired = false;
    }

    // Check write buffer valid
    if (!_writeBufStartPtr)
        return TX_FIFO_NEARING_EMPTY_INT;

    // Fill fifo with data to write
    uint32_t remainingBytes = _writeBufLen - _writeBufPos;
    uint32_t toSend = I2C_ENGINE_FIFO_SIZE - I2C_DEVICE.status_reg.tx_fifo_cnt;
    if (toSend > remainingBytes)
        toSend = remainingBytes;
    for (uint32_t i = 0; i < toSend; i++)
        I2C_DEVICE.fifo_data.val = _writeBufStartPtr[_writeBufPos++];

    // If we have finished all data to write AND
    // if we have a restart address to send AND
    // there is space in the FIFO then add the restart address
    if ((_writeBufPos == _writeBufLen) && _restartAddrPlusRWRequired &&
                (I2C_DEVICE.status_reg.tx_fifo_cnt < I2C_ENGINE_FIFO_SIZE))
    {
        I2C_DEVICE.fifo_data.val = _restartAddrPlusRW;
        _restartAddrPlusRWRequired = false;
    }

    // Return flag indicating if Tx FIFO nearing empty interrupt should be disabled
    return ((_writeBufPos < _writeBufLen) || _restartAddrPlusRWRequired) ? 0 : TX_FIFO_NEARING_EMPTY_INT;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Empty Rx FIFO and return bit mask of interrupts to disable
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t IRAM_ATTR RdI2C::emptyRxFifo()
{
    // Check valid
    if (!_readBufStartPtr)
        return RX_FIFO_NEARING_FULL_INT;
        
    // Empty received data from the Rx FIFO
    uint32_t toGet = I2C_DEVICE.status_reg.rx_fifo_cnt;
    for (uint32_t i = 0; i < toGet; i++)
    {
        if (_readBufPos >= _readBufMaxLen)
            break;
        _readBufStartPtr[_readBufPos++] = I2C_DEVICE.fifo_data.val;
    }
    return (_readBufPos >= _readBufMaxLen) ? RX_FIFO_NEARING_FULL_INT : 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// De-initialise the I2C hardware
// Frees pins and interrupts
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdI2C::deinit()
{
    // Check for ISR
    if (_i2cISRHandle)
        esp_intr_free(_i2cISRHandle);
    _i2cISRHandle = nullptr;

    // Check initialised and release pins if so
    if (_isInitialised)
    {
        if (_pinSDA >= 0)
            gpio_reset_pin((gpio_num_t)_pinSDA);
        if (_pinSCL >= 0)
            gpio_reset_pin((gpio_num_t)_pinSCL);
    }
    _isInitialised = false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Debugging functions used to format debug data
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String RdI2C::debugMainStatusStr(const char* prefix, uint32_t statusFlags)
{
    const char* SCLStates[] = {
        "Idle", "Start", "NegEdge", "Low", "PosEdge", "High", "Stop", "Unknown"
    };
    const char* MainStates[] = {
        "Idle", "AddrShift", "ACKAddr", "RxData", "RxData", "SendACK", "WaitACK", "Unknown"
    };
    const char* FlagNames[] = {
        "AckValue",
        "SlaveRw",
        "TimeOut",
        "ArbLost",
        "BusBusy",
        "SlaveAddr",
        "ByteTrans"
    };
    char outStr[150];
    snprintf(outStr, sizeof(outStr), "%sSCLState %s MainState %s TxFifoCount %d RxFifoCount %d",
                prefix,
                SCLStates[(statusFlags >> I2C_SCL_STATE_LAST_S) & I2C_SCL_STATE_LAST_V],
                MainStates[(statusFlags >> I2C_SCL_MAIN_STATE_LAST_S) & I2C_SCL_MAIN_STATE_LAST_V],
                (statusFlags >> I2C_TXFIFO_CNT_S) & I2C_TXFIFO_CNT_V,
                (statusFlags >> I2C_RXFIFO_CNT_S) & I2C_RXFIFO_CNT_V
    );
    static const uint32_t numFlags = sizeof(FlagNames)/sizeof(FlagNames[0]);
    uint32_t maskBit = 1u << (numFlags-1);
    for (uint32_t i = 0; i < numFlags; i++)
    {
        if (statusFlags & maskBit)
        {
            strlcat(outStr, " ", sizeof(outStr));
            uint32_t bitIdx = numFlags-1-i;
                strlcat(outStr, FlagNames[bitIdx], sizeof(outStr));
        }
        maskBit = maskBit >> 1;
    }
    return outStr;
}

String RdI2C::debugFIFOStatusStr(const char* prefix, uint32_t statusFlags)
{
    char outStr[60];
    snprintf(outStr, sizeof(outStr), "%sTxFIFOStart %d TxFIFOEnd %d RxFIFOStart %d RxFIFOEnd %d",
                prefix,
                (statusFlags >> 10) & 0x1f,
                (statusFlags >> 15) & 0x1f,
                (statusFlags >> 0) & 0x1f,
                (statusFlags >> 5) & 0x1f
    );
    return outStr;
}

String RdI2C::debugINTFlagStr(const char* prefix, uint32_t statusFlags)
{
    const char* intStrs[] = {
        "RxFIFOFull",
        "TxFIFOEmpty",
        "RxFIFOOvf",
        "EndDetect",
        "SlaveTransComp",
        "ArbLost",
        "MasterTransComp",
        "TransComplete",
        "TimeOut",
        "TransStart",
        "ACKError",
        "SendEmpty",
        "RxFull",            
    };
    char outStr[200];
    snprintf(outStr, sizeof(outStr), "%s", prefix);
    uint32_t maskBit = 1u << 31;
    bool sepNeeded = false;
    for (uint32_t i = 0; i < 32; i++)
    {
        if (statusFlags & maskBit)
        {
            if (sepNeeded)
                strlcat(outStr, " ", sizeof(outStr));
            sepNeeded = true;
            uint32_t bitIdx = 31-i;
            if (bitIdx > (sizeof(intStrs)/sizeof(intStrs[0])))
                snprintf(outStr+strlen(outStr), sizeof(outStr)-strlen(outStr), "UnknownBit%d", bitIdx);
            else
                strlcat(outStr, intStrs[bitIdx], sizeof(outStr));
        }
        maskBit = maskBit >> 1;
    }
    return outStr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Debugging function used to log I2C status information
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdI2C::debugShowStatus(const char* prefix, uint32_t addr)
{
    LOG_I(MODULE_PREFIX, "%s addr %02x INTRAW (%08x) %s MAIN (%08x) %s FIFO (%08x) %s Stats %s ___", 
        prefix, addr,
        I2C_DEVICE.int_raw.val, debugINTFlagStr("", I2C_DEVICE.int_raw.val).c_str(),
        I2C_DEVICE.status_reg.val, debugMainStatusStr("", I2C_DEVICE.status_reg.val).c_str(), 
        I2C_DEVICE.fifo_st.val, debugFIFOStatusStr("", I2C_DEVICE.fifo_st.val).c_str(),
        _i2cStats.debugStr().c_str()
    );
    String cmdRegsStr;
    Utils::getHexStrFromUint32(const_cast<uint32_t*>(&(I2C_DEVICE.command[0].val)), 
                I2C_ENGINE_CMD_QUEUE_SIZE, cmdRegsStr, " ");
    LOG_I(MODULE_PREFIX, "___Cmds %s", cmdRegsStr.c_str());
    String memStr;
    Utils::getHexStrFromUint32(const_cast<uint32_t*>(&(I2C_DEVICE.ram_data[0])), 
                I2C_ENGINE_FIFO_SIZE, memStr, " ");
    LOG_I(MODULE_PREFIX, "___Mem %s", memStr.c_str());
}