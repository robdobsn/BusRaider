// Bus Raider
// Rob Dobson 2019-2022

#pragma once

#include "lowlib.h"
#include <circle/util.h>

#define Z80_PROGRAM_RESET_VECTOR 0

class Z80Registers
{
public:
    int PC;
    int SP;
    int HL;
    int DE;
    int BC;
    int AF;
    int IX;
    int IY;
    int HLDASH;
    int DEDASH;
    int BCDASH;
    int AFDASH;
    int MEMPTR;
    int I;
    int R;
    int INTMODE;
    int INTENABLED;
    int VPS;

public:
    Z80Registers()
    {
        clear();
    }
    void clear()
    {
        PC = SP = HL = DE = BC = AF = IX = IY = 0;
        HLDASH = DEDASH = BCDASH = AFDASH = MEMPTR = 0;
        I = R = INTMODE = INTENABLED = VPS = 0;
    }
    void format(char* pResponse, int maxLen)
    {
        char tmpStr[200];
        snprintf(tmpStr, sizeof(tmpStr), "PC=%04x SP=%04x BC=%04x AF=%04x HL=%04x DE=%04x IX=%04x IY=%04x",
                PC, SP, BC, AF, HL, DE, IX, IY);
        strlcpy(pResponse, tmpStr, maxLen);
        snprintf(tmpStr, sizeof(tmpStr), " AF'=%04x BC'=%04x HL'=%04x DE'=%04x I=%02x R=%02x",
                AFDASH, BCDASH, HLDASH, DEDASH, I, R);
        strlcat(pResponse, tmpStr, maxLen);
        snprintf(tmpStr, sizeof(tmpStr), "  F=%c%c-%c-%c%c%c",
                (AF & 0x80) ? 'S' : '-',
                (AF & 0x40) ? 'Z' : '-',
                (AF & 0x10) ? 'H' : '-',
                (AF & 0x04) ? 'P' : '-',
                (AF & 0x02) ? 'N' : '-',
                (AF & 0x01) ? 'C' : '-');
        strlcat(pResponse, tmpStr, maxLen);
        snprintf(tmpStr, sizeof(tmpStr), " F'=%c%c--%c-%c%c%c",
                (AFDASH & 0x80) ? 'S' : '-',
                (AFDASH & 0x40) ? 'Z' : '-',
                (AFDASH & 0x10) ? 'H' : '-',
                (AFDASH & 0x04) ? 'P' : '-',
                (AFDASH & 0x02) ? 'N' : '-',
                (AFDASH & 0x01) ? 'C' : '-');
        strlcat(pResponse, tmpStr, maxLen);
        snprintf(tmpStr, sizeof(tmpStr), " MEMPTR=%04x IM%d IFF%c%c VPS: %d",
                MEMPTR, INTMODE, 
                INTENABLED ? '1' : ' ', 
                INTENABLED ? '2' : ' ', 
                VPS );
        strlcat(pResponse, tmpStr, maxLen);
    }
    void json(char* pResponse, int maxLen)
    {
        char tmpStr[200];
        snprintf(tmpStr, sizeof(tmpStr), 
                R"("PC":"0x%04x","SP":"0x%04x","BC":"0x%04x","AF":"0x%04x","HL":"0x%04x","DE":"0x%04x","IX":"0x%04x","IY":"0x%04x",)",
                PC, SP, BC, AF, HL, DE, IX, IY);
        strlcpy(pResponse, tmpStr, maxLen);
        snprintf(tmpStr, sizeof(tmpStr), 
                R"("AFDASH":"0x%04x","BCDASH":"0x%04x","HLDASH":"0x%04x","DEDASH":"0x%04x","I":"0x%02x","R":"0x%02x",)",
                AFDASH, BCDASH, HLDASH, DEDASH, I, R);
        strlcat(pResponse, tmpStr, maxLen);
        snprintf(tmpStr, sizeof(tmpStr), R"("IM":%d,"IFF":%d,"VPS":%d)",
                INTMODE, 
                INTENABLED,
                VPS );
        strlcat(pResponse, tmpStr, maxLen);
    }
};
