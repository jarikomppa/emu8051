/* 8051 emulator core
 * Copyright 2006 Jari Komppa
 * Released under LGPL
 *
 * core.c
 * General emulation functions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "emu8051.h"

int tick(struct em8051 *aCPU)
{
    int v;
    if (aCPU->mTickDelay)
    {
        aCPU->mTickDelay--;
        return 0;
    }
    aCPU->mTickDelay = aCPU->op[aCPU->mCodeMem[aCPU->mPC & (aCPU->mCodeMemSize - 1)]](aCPU);
    //aCPU->mTickDelay = do_op(aCPU);
    // update parity bit
    v = aCPU->mSFR[REG_ACC];
    v ^= v >> 4;
    v &= 0xf;
    v = (0x6996 >> v) & 1;
    aCPU->mSFR[REG_PSW] = (aCPU->mSFR[REG_PSW] & ~PSWMASK_P) | (v * PSWMASK_P);

    return 1;
}

int decode(struct em8051 *aCPU, int aPosition, unsigned char *aBuffer)
{
    return aCPU->dec[aCPU->mCodeMem[aPosition & (aCPU->mCodeMemSize - 1)]](aCPU, aPosition, aBuffer);
}

void disasm_setptrs(struct em8051 *aCPU);
void op_setptrs(struct em8051 *aCPU);

void reset(struct em8051 *aCPU, int aWipe)
{
    // clear memory, set registers to bootup values, etc    
    if (aWipe)
    {
        memset(aCPU->mCodeMem, 0, aCPU->mCodeMemSize);
        memset(aCPU->mExtData, 0, aCPU->mExtDataSize);
        memset(aCPU->mLowerData, 0, 128);
        if (aCPU->mUpperData) 
            memset(aCPU->mUpperData, 0, 128);
    }

    memset(aCPU->mSFR, 0, 128);

    aCPU->mPC = 0;
    aCPU->mTickDelay = 0;
    aCPU->mSFR[REG_SP] = 7;
    aCPU->mSFR[REG_P0] = 0xff;
    aCPU->mSFR[REG_P1] = 0xff;
    aCPU->mSFR[REG_P2] = 0xff;
    aCPU->mSFR[REG_P3] = 0xff;

    // build function pointer lists

    disasm_setptrs(aCPU);
    op_setptrs(aCPU);
}


int readbyte(FILE * f)
{
    char data[3];
    data[0] = fgetc(f);
    data[1] = fgetc(f);
    data[2] = 0;
    return strtol(data, NULL, 16);
}

int load_obj(struct em8051 *aCPU, char *aFilename)
{
    FILE *f;    
    if (aFilename == 0 || aFilename[0] == 0)
        return -1;
    f = fopen(aFilename, "r");
    if (!f) return -1;
    if (fgetc(f) != ':')
        return -2; // unsupported file format
    while (!feof(f))
    {
        int recordlength;
        int address;
        int recordtype;
        int checksum;
        int i;
        recordlength = readbyte(f);
        address = readbyte(f);
        address <<= 8;
        address |= readbyte(f);
        recordtype = readbyte(f);
        if (recordtype == 1)
            return 0; // we're done
        if (recordtype != 0)
            return -3; // unsupported record type
        checksum = recordtype + recordlength + (address & 0xff) + (address >> 8); // final checksum = 1 + not(checksum)
        for (i = 0; i < recordlength; i++)
        {
            int data = readbyte(f);
            checksum += data;
            aCPU->mCodeMem[address + i] = data;
        }
        i = readbyte(f);
        checksum &= 0xff;
        checksum = 256 - checksum;
        if (i != (checksum & 0xff))
            return -4; // checksum failure
        while (fgetc(f) != ':' && !feof(f)) {} // skip newline        
    }
    return -5;
}
