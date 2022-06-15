
#include "../BusControl/BusControl.h"
#include "memoryTests.h"

// The following code from
// https://barrgroup.com/Embedded-Systems/How-To/Memory-Test-Suite-C

/**********************************************************************
 *
 * Function:    memTestDataBus()
 *
 * Description: Test the data bus wiring in a memory region by
 *              performing a walking 1's test at a fixed address
 *              within that region.  The address (and hence the
 *              memory region) is selected by the caller.
 *
 * Notes:       
 *
 * Returns:     0 if the test succeeds.  
 *              A non-zero result is the first pattern that failed.
 *
 **********************************************************************/
uint8_t memTestDataBus(BusControl& busControl, uint32_t address)
{
    uint8_t pattern;
    /*
     * Perform a walking 1's test at the given address.
     */
    for (pattern = 1; pattern != 0; pattern <<= 1)
    {
        /*
         * Write the test pattern.
         */
        busControl.mem().blockWrite(address, &pattern, 1, BLOCK_ACCESS_MEM);

        /*
         * Read it back (immediately is okay for this test).
         */
        uint8_t readPattern;
        busControl.mem().blockRead(address, &readPattern, 1, BLOCK_ACCESS_MEM);
        if (readPattern != pattern) 
        {
            return pattern;
        }
    }

    return 0;
}

/**********************************************************************
 *
 * Function:    memTestAddressBus()
 *
 * Description: Test the address bus wiring in a memory region by
 *              performing a walking 1's test on the relevant bits
 *              of the address and checking for aliasing. This test
 *              will find single-bit address failures such as stuck
 *              -high, stuck-low, and shorted pins.  The base address
 *              and size of the region are selected by the caller.
 *
 * Notes:       For best results, the selected base address should
 *              have enough LSB 0's to guarantee single address bit
 *              changes.  For example, to test a 64-Kbyte region, 
 *              select a base address on a 64-Kbyte boundary.  Also, 
 *              select the region size as a power-of-two--if at all 
 *              possible.
 *
 * Returns:     NULL if the test succeeds.  
 *              A non-zero result is the first address at which an
 *              aliasing problem was uncovered.  By examining the
 *              contents of memory, it may be possible to gather
 *              additional information about the problem.
 *
 **********************************************************************/

uint32_t memTestAddressBus(BusControl& busControl, uint32_t baseAddress, unsigned long nBytes)
{
    unsigned long addressMask = (nBytes/sizeof(uint8_t) - 1);
    unsigned long offset;
    unsigned long testOffset;

    uint8_t pattern     = (uint8_t) 0xAAAAAAAA;
    uint8_t antipattern = (uint8_t) 0x55555555;


    /*
     * Write the default pattern at each of the power-of-two offsets.
     */
    for (offset = 1; (offset & addressMask) != 0; offset <<= 1)
    {
        busControl.mem().blockWrite(baseAddress + offset, &pattern, 1, BLOCK_ACCESS_MEM);
    }

    /* 
     * Check for address bits stuck high.
     */
    testOffset = 0;
    busControl.mem().blockWrite(baseAddress + testOffset, &antipattern, 1, BLOCK_ACCESS_MEM);

    for (offset = 1; (offset & addressMask) != 0; offset <<= 1)
    {
        uint8_t readPattern;
        busControl.mem().blockRead(baseAddress + offset, &readPattern, 1, BLOCK_ACCESS_MEM);

        if (readPattern != pattern)
        {
            return (baseAddress + offset);
        }
    }

    busControl.mem().blockWrite(baseAddress + testOffset, &pattern, 1, BLOCK_ACCESS_MEM);

    /*
     * Check for address bits stuck low or shorted.
     */
    for (testOffset = 1; (testOffset & addressMask) != 0; testOffset <<= 1)
    {
        busControl.mem().blockWrite(baseAddress + testOffset, &antipattern, 1, BLOCK_ACCESS_MEM);

        uint8_t readPattern;
        busControl.mem().blockRead(baseAddress, &readPattern, 1, BLOCK_ACCESS_MEM);
		if (readPattern != pattern)
		{
			return (baseAddress + testOffset);
		}

        for (offset = 1; (offset & addressMask) != 0; offset <<= 1)
        {
            uint8_t readPattern;
            busControl.mem().blockRead(baseAddress + offset, &readPattern, 1, BLOCK_ACCESS_MEM);

            if ((readPattern != pattern) && (offset != testOffset))
            {
                return (baseAddress + testOffset);
            }
        }

        busControl.mem().blockWrite(baseAddress + testOffset, &pattern, 1, BLOCK_ACCESS_MEM);
    }

    return 0;

}

/**********************************************************************
 *
 * Function:    memTestDevice()
 *
 * Description: Test the integrity of a physical memory device by
 *              performing an increment/decrement test over the
 *              entire region.  In the process every storage bit 
 *              in the device is tested as a zero and a one.  The
 *              base address and the size of the region are
 *              selected by the caller.
 *
 * Notes:       
 *
 * Returns:     NULL if the test succeeds.
 *
 *              A non-zero result is the first address at which an
 *              incorrect value was read back.  By examining the
 *              contents of memory, it may be possible to gather
 *              additional information about the problem.
 *
 **********************************************************************/
uint32_t memTestDevice(BusControl& busControl, uint32_t baseAddress, unsigned long nBytes)	
{
    unsigned long offset;
    unsigned long nWords = nBytes / sizeof(uint8_t);

    uint8_t pattern;
    uint8_t antipattern;

    uint8_t* pMem = new uint8_t[nBytes];
    if (!pMem)
        return UINT32_MAX;

    /*
     * Fill memory with a known pattern.
     */
    for (pattern = 1, offset = 0; offset < nWords; pattern++, offset++)
    {
        pMem[offset] = pattern;
    }
    busControl.mem().blockWrite(baseAddress, pMem, nBytes, BLOCK_ACCESS_MEM);

    /*
     * Check each location and invert it for the second pass.
     */
    busControl.mem().blockRead(baseAddress, pMem, nBytes, BLOCK_ACCESS_MEM);    
    for (pattern = 1, offset = 0; offset < nWords; pattern++, offset++)
    {
        uint8_t readPattern = pMem[offset];
        if (readPattern != pattern)
        {
            return (baseAddress + offset);
        }

        antipattern = ~pattern;
        pMem[offset] = antipattern;
    }
    busControl.mem().blockWrite(baseAddress, pMem, nBytes, BLOCK_ACCESS_MEM);

    /*
     * Check each location for the inverted pattern and zero it.
     */
    busControl.mem().blockRead(baseAddress, pMem, nBytes, BLOCK_ACCESS_MEM);    
    for (pattern = 1, offset = 0; offset < nWords; pattern++, offset++)
    {
        antipattern = ~pattern;
        uint8_t readPattern = pMem[offset];
        if (readPattern != antipattern)
        {
            return (baseAddress + offset);
        }
    }

    return 0;

}   /* memTestDevice() */

// // Memory test block
// int memTestAll(uint32_t baseAddress, uint32_t numBytes)
// {
//     if ((memTestDataBus(baseAddress) != 0) ||
//         (memTestAddressBus(baseAddress, numBytes) != NULL) ||
//         (memTestDevice(baseAddress, numBytes) != NULL))
//     {
//         return (-1);
//     }
//     else
//     {
//         return (0);
//     }
		
// }   /* memTest() */
