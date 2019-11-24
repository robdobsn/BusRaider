// Bus Raider
// Rob Dobson 2019

#include "KeyConversion.h"

const char* KeyConversion::_keyboardTypeStrs[] = 
{
    "US", "UK", "FR", "DE", "IT", "ES"
};

uint32_t KeyConversion::getNumTypes()
{
    return sizeof(_keyboardTypeStrs)/sizeof(_keyboardTypeStrs[0]);
}

uint16_t KeyConversion::_hidCodeConversion[][PHY_MAX_CODE+1][K_ALTSHIFTTAB+1] =
{
    {
#include "../../uspi/lib/keymap_us.h"
    },
    {
#include "../../uspi/lib/keymap_uk.h"
    },
    {
#include "../../uspi/lib/keymap_fr.h"
    },
    {
#include "../../uspi/lib/keymap_de.h"
    },
    {
#include "../../uspi/lib/keymap_it.h"
    },
    {
#include "../../uspi/lib/keymap_es.h"
    }
};
