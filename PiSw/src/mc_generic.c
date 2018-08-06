// Bus Raider Machine Type
// Rob Dobson 2018

#include "mc_trs80.h"
#include "utils.h"

static McGenericDescriptor* __pMCtype = NULL;

void mc_generic_set(const char* mcName)
{
    // Restore if needed - deallocates memory etc
    mc_generic_restore();

    // Check for TRS80 Model 1
    if (strcmp((char*)mcName, "TRS80Model1") == 0) {
        __pMCtype = trs80_get_descriptor(0);
    } else {
        __pMCtype = NULL;
    }

    // Check if valid
    if (__pMCtype) {
        // Init - allocates memory etc
        if (__pMCtype->pInit)
            __pMCtype->pInit();
    }
}

void mc_generic_restore()
{
    // Check if valid
    if (__pMCtype) {
        if (__pMCtype->pDeInit)
            __pMCtype->pDeInit();
    }
}

McGenericDescriptor* mc_generic_get_descriptor()
{
    return __pMCtype;
}

void mc_generic_handle_key(unsigned char ucModifiers, const unsigned char rawKeys[6])
{
    // Check if valid
    if (__pMCtype) {
        // Handle key
        if (__pMCtype->pKeyHandler)
            __pMCtype->pKeyHandler(ucModifiers, rawKeys);
    }
}

void mc_generic_handle_disp(uint8_t* pDispBuf, int dispBufLen)
{
    // Check if valid
    if (__pMCtype) {
        // Handle display
        if (__pMCtype->pDispHandler)
            __pMCtype->pDispHandler(pDispBuf, dispBufLen);
    }
}
