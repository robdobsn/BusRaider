// Bus Raider
// Rob Dobson 2019

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include "../System/ee_printf.h"

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
        ee_sprintf(tmpStr, " AF'=%04x BC'=%04x HL'=%04x DE'=%04x I=%02x R=%02x  F=-------- F'=-------- MEMPTR=%04x IM%d INTEN%02x VPS: %d",
                AFDASH, BCDASH, HLDASH, DEDASH, I, R, MEMPTR, INTMODE, INTENABLED, VPS );
        strlcat(pResponse, tmpStr, maxLen);
    }
};