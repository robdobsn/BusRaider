// Bus Raider
// Rob Dobson 2019

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include "../System/ee_sprintf.h"

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
        char tmpStr[100];
        ee_sprintf(tmpStr, "PC=%04x SP=%04x BC=%04x AF=%04x HL=%04x DE=%04x IX=%04x IY=%04x",
                PC, SP, BC, AF, HL, DE, IX, IY);
        strlcpy(pResponse, tmpStr, maxLen);
        ee_sprintf(tmpStr, " AF'=%04x BC'=%04x HL'=%04x DE'=%04x I=%02x R=%02x",
                AFDASH, BCDASH, HLDASH, DEDASH, I, R);
        strlcat(pResponse, tmpStr, maxLen);
        ee_sprintf(tmpStr, "  F=%c%c-%c-%c%c%c F'=%c%c--%c-%c%c%c MEMPTR=%04x IM%d INTEN%02x VPS: %d",
                (AF & 0x80) ? 'S' : '-',
                (AF & 0x40) ? 'Z' : '-',
                (AF & 0x10) ? 'H' : '-',
                (AF & 0x04) ? 'P' : '-',
                (AF & 0x02) ? 'N' : '-',
                (AF & 0x01) ? 'C' : '-',
                (AFDASH & 0x80) ? 'S' : '-',
                (AFDASH & 0x40) ? 'Z' : '-',
                (AFDASH & 0x10) ? 'H' : '-',
                (AFDASH & 0x04) ? 'P' : '-',
                (AFDASH & 0x02) ? 'N' : '-',
                (AFDASH & 0x01) ? 'C' : '-',
                MEMPTR, INTMODE, INTENABLED, VPS );
        strlcat(pResponse, tmpStr, maxLen);
    }
};
