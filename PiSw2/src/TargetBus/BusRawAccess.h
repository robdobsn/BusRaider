// Bus Raider
// Rob Dobson 2018

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <circle/bcm2835.h>
#include <circle/memio.h>
#include "lowlib.h"
#include "lowlev.h"
#include "TargetCPU.h"
#include "BusSocketManager.h"
#include "BusAccessDefs.h"
#include "TargetClockGenerator.h"
#include "BusAccessStatusInfo.h"
#include "MemoryController.h"
#include "BusRawAccess_Timing.h"

// #define CHECK_ARM_PWM_STA_BEFORE_WAIT_CLEAR

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Bus raw access
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class BusSocketInfo;

class BusRawAccess
{
public:
    BusRawAccess(TargetClockGenerator& targetClockGenerator);

    // Initialization
    void init();

    // Service
    void service();

    // Reset target
    void targetReset(uint32_t ms);

    // Set signal (RESET/IRQ/NMI/BUSRQ)
    void setBusSignal(BR_BUS_ACTION busAction, bool assert);

    // Read and write raw blocks - directly to bus
    BR_RETURN_TYPE rawBlockWrite(uint32_t addr, const uint8_t* pData, uint32_t len, BlockAccessType accessType);
    BR_RETURN_TYPE rawBlockRead(uint32_t addr, uint8_t* pData, uint32_t len, BlockAccessType accessType);

    // Wait hardware config
    BR_RETURN_TYPE waitConfigure(bool waitOnMemory, bool waitOnIO);
    void waitSuspend(bool suspend);

    // Bus request
    BR_RETURN_TYPE busRequestAndTake(uint32_t maxWaitForBUSACKus = 0);
    void busReqRelease();
    void busReqStart();
    void busReqTakeControl();
    bool busReqWaitForAck(bool ack, uint32_t maxWaitForBUSACKus = 0);
    bool rawBUSAKActive()
    {
        return (read32(ARM_GPIO_GPLEV0) & BR_BUSACK_BAR_MASK) == 0;
    }
    
    // State
    bool busReqAcknowledged()
    {
        return _busReqAcknowledged;
    }

    // Control the PIB (bus used to transfer data to/from Pi)
    inline void pibSetOut()
    {
        write32(BR_PIB_GPF_REG, (read32(BR_PIB_GPF_REG) & BR_PIB_GPF_MASK) | BR_PIB_GPF_OUTPUT);
    }

    inline void pibSetIn()
    {
        write32(BR_PIB_GPF_REG, (read32(BR_PIB_GPF_REG) & BR_PIB_GPF_MASK) | BR_PIB_GPF_INPUT);
    }

    inline void pibSetValue(uint8_t val)
    {
        uint32_t setBits = ((uint32_t)val) << BR_DATA_BUS;
        uint32_t clrBits = (~(((uint32_t)val) << BR_DATA_BUS)) & (~BR_PIB_MASK);
        write32(ARM_GPIO_GPSET0, setBits);
        write32(ARM_GPIO_GPCLR0, clrBits);
    }
 
    // Get a value from the PIB (pins used for data/address bus access)
    inline uint8_t pibGetValue()
    {
        return (read32(ARM_GPIO_GPLEV0) >> BR_DATA_BUS) & 0xff;
    }

    // Mux functions
    void muxSet(int muxVal);
    void muxClear();

    // Bus reads
    uint32_t controlBusRead();
    void addrAndDataBusRead(uint32_t& addr, uint32_t& dataBusVals);

    // Version
    static const int HW_VERSION_DEFAULT = 20;
    int getHwVersion(bool getDefault)
    {
        if (getDefault)
            return HW_VERSION_DEFAULT;
        return _hwVersionNumber;
    }
    void setHwVersion(int hwVersion)
    {
        _hwVersionNumber = hwVersion;
    }

    // Clear wait flip-flops
    static inline void waitResetFlipFlops()
    {
        // LogWrite("BusAccess", LOG_DEBUG, "WAIT waitResetFlipFlops");

        // Wait flip-flops are reset using the BCM2835 PWM controller
        // Since the PWM FIFO is shared the data output to MREQ/IORQ enable pins 
        // will be interleaved so we need to write data for both
#ifdef CHECK_ARM_PWM_STA_BEFORE_WAIT_CLEAR
        if ((read32(ARM_PWM_STA) & 1) == 0)
#endif
        {
            // Clear the wait
            write32(ARM_PWM_FIF1, 0x0000ffff);  // IORQ sequence
            write32(ARM_PWM_FIF1, 0x0000ffff);  // MREQ sequence
        }
    }

private:
    // State
    bool _busReqAcknowledged;
    bool _pageIsActive;

    // Hardware version
    // V1.7 ==> 17, V2.0 ==> 20
    int _hwVersionNumber;

    // Target clock generator
    TargetClockGenerator& _targetClockGenerator;

    // Wait system
    bool _rawWaitOnMem;
    bool _rawWaitOnIO;
    void waitSystemInit();
    void waitClearDetected();
    void waitRawSet(bool waitOnMemory, bool waitOnIO);

    // Set a pin to be an output and set initial value for that pin
    void setPinOut(int pinNumber, bool val);

    // Mux functions
    void muxDataBusOutputEnable();
    void muxDataBusOutputStart();
    void muxDataBusOutputFinish();
    void muxClearLowAddr();

    // Address bus
    void addrLowSet(uint32_t lowAddrByte);
    void addrHighSet(uint32_t highAddrByte);
    void addrSet(unsigned int addr);
    void addrLowInc();

};
