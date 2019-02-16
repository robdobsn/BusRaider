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
    uint16_t PC;
    uint16_t SP;
    uint16_t HL;
    uint16_t DE;
    uint16_t BC;
    uint16_t AF;
    uint16_t IX;
    uint16_t IY;
    uint16_t HLDASH;
    uint16_t DEDASH;
    uint16_t BCDASH;
    uint16_t AFDASH;
    uint16_t MEMPTR;
    uint8_t I;
    uint8_t R;
    uint8_t INTMODE;
    uint8_t INTENABLED;
    uint8_t VPS;

public:
    Z80Registers()
    {
        PC = SP = HL = DE = BC = AF = IX = IY = 0;
        HLDASH = DEDASH = BCDASH = AFDASH = MEMPTR = 0;
        I = R = INTMODE = INTENABLED = VPS = 0;
    }
    void format1(char* pResponse, int maxLen)
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