/* 8051 emulator core
 * Copyright 2006 Jari Komppa
 * Released under LGPL
 *
 * opcodes.c
 * 8051 opcode simulation functions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "emu8051.h"

static void add_solve_flags(struct em8051 * aCPU, int value1, int value2)
{
    int carry = ((value1 & 255) + (value2 & 255)) >> 8;
    int auxcarry = ((value1 & 7) + (value2 & 7)) >> 3;
    int overflow = (((value1 & 127) + (value2 & 127)) >> 7)^carry;
    aCPU->mSFR[REG_PSW] = (aCPU->mSFR[REG_PSW] & ~(PSWMASK_C|PSWMASK_AC|PSWMASK_OV)) |
                          (carry << PSW_C) | (auxcarry << PSW_AC) | (overflow << PSW_OV);
}

static void sub_solve_flags(struct em8051 * aCPU, int value1, int value2)
{
    int carry = (((value1 & 255) - (value2 & 255)) >> 8) & 1;
    int auxcarry = (((value1 & 7) - (value2 & 7)) >> 3) & 1;
    int overflow = ((((value1 & 127) - (value2 & 127)) >> 7) & 1)^carry;
    aCPU->mSFR[REG_PSW] = (aCPU->mSFR[REG_PSW] & ~(PSWMASK_C|PSWMASK_AC|PSWMASK_OV)) |
                          (carry << PSW_C) | (auxcarry << PSW_AC) | (overflow << PSW_OV);
}


static int ajmp_offset(struct em8051 *aCPU)
{
    int address = (aCPU->mPC + 2) & 0xf800 |
        aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)] | 
        ((aCPU->mCodeMem[(aCPU->mPC)&(aCPU->mCodeMemSize-1)] & 0xe0) << 3);

    aCPU->mPC = address;

    return 1;
}

static int ljmp_address(struct em8051 *aCPU)
{
    int address = (aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)] << 8) | 
        aCPU->mCodeMem[(aCPU->mPC + 2)&(aCPU->mCodeMemSize-1)];
    aCPU->mPC = address;

    return 1;
}


static int rr_a(struct em8051 *aCPU)
{
    aCPU->mSFR[REG_ACC] = (aCPU->mSFR[REG_ACC] >> 1) | 
                          (aCPU->mSFR[REG_ACC] << 7);
    aCPU->mPC++;
    return 0;
}

static int inc_a(struct em8051 *aCPU)
{
    aCPU->mSFR[REG_ACC]++;
    aCPU->mPC++;
    return 0;
}

static int inc_mem(struct em8051 *aCPU)
{
    int address = aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)];
    if (address > 0x7f)
    {
        aCPU->mSFR[address - 0x80]++;
        if (aCPU->sfrwrite)
            aCPU->sfrwrite(aCPU, address);
    }
    else
    {
        aCPU->mLowerData[address]++;
    }
    aCPU->mPC += 2;
    return 0;
}

static int inc_indir_rx(struct em8051 *aCPU)
{
    int rx = (aCPU->mCodeMem[(aCPU->mPC)&(aCPU->mCodeMemSize-1)] & 1) + 
             8 * ((aCPU->mSFR[REG_PSW] & (PSWMASK_RS0|PSWMASK_RS1))>>PSW_RS0);
    int address = aCPU->mLowerData[rx];
    if (address > 0x7f)
    {
        if (aCPU->mUpperData)
        {
            aCPU->mUpperData[address - 0x80]++;
        }
        else
        {
            aCPU->mSFR[address - 0x80]++;
            if (aCPU->sfrwrite)
                aCPU->sfrwrite(aCPU, address);
        }
    }
    else
    {
        aCPU->mLowerData[address]++;
    }
    aCPU->mPC++;
    return 0;
}

///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////

static int jbc_bitaddr_offset(struct em8051 *aCPU)
{
    printf("JBC   %03Xh, #%d\n",        
        aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)],
        (signed char)aCPU->mCodeMem[(aCPU->mPC + 2)&(aCPU->mCodeMemSize-1)]);    
    return 1;
}

static int acall_offset(struct em8051 *aCPU)
{
    int address = (aCPU->mPC + 2) & 0xf800 |
        aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)] | 
        ((aCPU->mCodeMem[(aCPU->mPC)&(aCPU->mCodeMemSize-1)] & 0xe0) << 3);
    aCPU->mSFR[REG_SP]++;
    aCPU->mLowerData[aCPU->mSFR[REG_SP] & 0x7f] = (aCPU->mPC + 2) & 0xff;
    aCPU->mSFR[REG_SP]++;
    aCPU->mLowerData[aCPU->mSFR[REG_SP] & 0x7f] = (aCPU->mPC + 2) >> 8;
    aCPU->mPC = address;
    if (aCPU->mSFR[REG_SP] >= 0x80)
        if (aCPU->except)
            aCPU->except(aCPU, EXCEPTION_STACK);
    return 1;
}

static int lcall_address(struct em8051 *aCPU)
{
    aCPU->mSFR[REG_SP]++;
    aCPU->mLowerData[aCPU->mSFR[REG_SP] & 0x7f] = (aCPU->mPC + 3) & 0xff;
    aCPU->mSFR[REG_SP]++;
    aCPU->mLowerData[aCPU->mSFR[REG_SP] & 0x7f] = (aCPU->mPC + 3) >> 8;
    aCPU->mPC = (aCPU->mCodeMem[(aCPU->mPC + 1) & (aCPU->mCodeMemSize-1)] << 8) | 
                (aCPU->mCodeMem[(aCPU->mPC + 2) & (aCPU->mCodeMemSize-1)] << 0);
    if (aCPU->mSFR[REG_SP] >= 0x80)
        if (aCPU->except)
            aCPU->except(aCPU, EXCEPTION_STACK);
    return 1;
}

static int rrc_a(struct em8051 *aCPU)
{
    int c = (aCPU->mSFR[REG_PSW] & PSWMASK_C) >> PSW_C;
    int newc = aCPU->mSFR[REG_ACC] & 1;
    aCPU->mSFR[REG_ACC] = (aCPU->mSFR[REG_ACC] >> 1) | 
                          (c << 7);
    aCPU->mSFR[REG_PSW] = (aCPU->mSFR[REG_PSW] & ~PSWMASK_C) | (newc << PSW_C);
    aCPU->mPC++;
    return 0;
}

static int dec_a(struct em8051 *aCPU)
{
    aCPU->mSFR[REG_ACC]--;
    aCPU->mPC++;
    return 0;
}

static int dec_mem(struct em8051 *aCPU)
{
    int address = aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)];
    if (address > 0x7f)
    {
        aCPU->mSFR[address - 0x80]--;
        if (aCPU->sfrwrite)
            aCPU->sfrwrite(aCPU, address);
    }
    else
    {
        aCPU->mLowerData[address]--;
    }
    aCPU->mPC += 2;
    return 0;
}

static int dec_indir_rx(struct em8051 *aCPU)
{
    int rx = (aCPU->mCodeMem[(aCPU->mPC)&(aCPU->mCodeMemSize-1)] & 1) + 
             8 * ((aCPU->mSFR[REG_PSW] & (PSWMASK_RS0|PSWMASK_RS1))>>PSW_RS0);
    int address = aCPU->mLowerData[rx];
    if (address > 0x7f)
    {
        if (aCPU->mUpperData)
        {
            aCPU->mUpperData[address - 0x80]--;
        }
        else
        {
            aCPU->mSFR[address - 0x80]--;
            if (aCPU->sfrwrite)
                aCPU->sfrwrite(aCPU, address);
        }
    }
    else
    {
        aCPU->mLowerData[address]--;
    }
    aCPU->mPC++;
    return 0;
}


static int jb_bitaddr_offset(struct em8051 *aCPU)
{
    printf("JB   %03Xh, #%d\n",        
        aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)],
        (signed char)aCPU->mCodeMem[(aCPU->mPC + 2)&(aCPU->mCodeMemSize-1)]);
    return 1;
}

static int ret(struct em8051 *aCPU)
{
    // possible stack underflow problem..
    aCPU->mPC = (aCPU->mLowerData[(aCPU->mSFR[REG_SP]    ) & 0x7f] << 8) |
                (aCPU->mLowerData[(aCPU->mSFR[REG_SP] - 1) & 0x7f] << 0);
    aCPU->mSFR[REG_SP] -= 2;
    if (aCPU->mSFR[REG_SP] >= 0x80)
        if (aCPU->except)
            aCPU->except(aCPU, EXCEPTION_STACK);
    return 1;
}

static int rl_a(struct em8051 *aCPU)
{
    aCPU->mSFR[REG_ACC] = (aCPU->mSFR[REG_ACC] << 1) | 
                          (aCPU->mSFR[REG_ACC] >> 7);
    aCPU->mPC++;
    return 0;
}

static int add_a_imm(struct em8051 *aCPU)
{
    add_solve_flags(aCPU, aCPU->mSFR[REG_ACC], aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)]);
    aCPU->mSFR[REG_ACC] += aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)];
    aCPU->mPC += 2;
    return 0;
}

static int add_a_mem(struct em8051 *aCPU)
{
    int address = aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)];
    if (address > 0x7f)
    {
        int value;
        if (aCPU->sfrread)
            value = aCPU->sfrread(aCPU, address);
        else
            value = aCPU->mSFR[address - 0x80];

        add_solve_flags(aCPU, aCPU->mSFR[REG_ACC], value);
        aCPU->mSFR[REG_ACC] += value;
    }
    else
    {
        add_solve_flags(aCPU, aCPU->mSFR[REG_ACC], aCPU->mLowerData[address]);
        aCPU->mSFR[REG_ACC] += aCPU->mLowerData[address];
    }
    aCPU->mPC += 2;
    return 0;
}

static int add_a_indir_rx(struct em8051 *aCPU)
{
    int rx = (aCPU->mCodeMem[(aCPU->mPC)&(aCPU->mCodeMemSize-1)] & 1) + 
             8 * ((aCPU->mSFR[REG_PSW] & (PSWMASK_RS0|PSWMASK_RS1))>>PSW_RS0);
    int address = aCPU->mLowerData[rx];
    if (address > 0x7f)
    {
        int value;
        if (aCPU->mUpperData)
        {
            value = aCPU->mUpperData[address - 0x80];
        }
        else
        {
            if (aCPU->sfrread)
                value = aCPU->sfrread(aCPU, address);
            else
                value = aCPU->mSFR[address - 0x80];
        }

        add_solve_flags(aCPU, aCPU->mSFR[REG_ACC], value);
        aCPU->mSFR[REG_ACC] += value;
    }
    else
    {
        add_solve_flags(aCPU, aCPU->mSFR[REG_ACC], aCPU->mLowerData[address]);
        aCPU->mSFR[REG_ACC] += aCPU->mLowerData[address];
    }

    aCPU->mPC++;
    return 0;
}

static int jnb_bitaddr_offset(struct em8051 *aCPU)
{
    printf("JNB   %03Xh, #%d\n",        
        aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)],
        (signed char)aCPU->mCodeMem[(aCPU->mPC + 2)&(aCPU->mCodeMemSize-1)]);
    return 1;
}

static int reti(struct em8051 *aCPU)
{
    printf("RETI");
    return 1;
}

static int rlc_a(struct em8051 *aCPU)
{
    int c = (aCPU->mSFR[REG_PSW] & PSWMASK_C) >> PSW_C;
    int newc = aCPU->mSFR[REG_ACC] >> 7;
    aCPU->mSFR[REG_ACC] = (aCPU->mSFR[REG_ACC] << 1) | 
                          (c);
    aCPU->mSFR[REG_PSW] = (aCPU->mSFR[REG_PSW] & ~PSWMASK_C) | (newc << PSW_C);
    aCPU->mPC++;
    return 0;
}

static int addc_a_imm(struct em8051 *aCPU)
{
    int carry = (aCPU->mSFR[REG_PSW] & PSWMASK_C) >> PSW_C;
    add_solve_flags(aCPU, aCPU->mSFR[REG_ACC], carry + aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)]);
    aCPU->mSFR[REG_ACC] += aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)] + carry;

    aCPU->mPC += 2;
    return 0;
}

static int addc_a_mem(struct em8051 *aCPU)
{
    int carry = (aCPU->mSFR[REG_PSW] & PSWMASK_C) >> PSW_C;
    int address = aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)];
    if (address > 0x7f)
    {
        int value;
        if (aCPU->sfrread)
            value = aCPU->sfrread(aCPU, address) + carry;
        else
            value = aCPU->mSFR[address - 0x80] + carry;
        add_solve_flags(aCPU, aCPU->mSFR[REG_ACC], value);
        aCPU->mSFR[REG_ACC] += value;
    }
    else
    {
        add_solve_flags(aCPU, aCPU->mSFR[REG_ACC], aCPU->mLowerData[address] + carry);
        aCPU->mSFR[REG_ACC] += aCPU->mLowerData[address] + carry;
    }
    aCPU->mPC += 2;
    return 0;
}

static int addc_a_indir_rx(struct em8051 *aCPU)
{
    int carry = (aCPU->mSFR[REG_PSW] & PSWMASK_C) >> PSW_C;
    int rx = (aCPU->mCodeMem[(aCPU->mPC)&(aCPU->mCodeMemSize-1)] & 1) + 
             8 * ((aCPU->mSFR[REG_PSW] & (PSWMASK_RS0|PSWMASK_RS1))>>PSW_RS0);
    int address = aCPU->mLowerData[rx];
    if (address > 0x7f)
    {
        int value;
        if (aCPU->mUpperData)
        {
            value = aCPU->mUpperData[address - 0x80];
        }
        else
        {
            if (aCPU->sfrread)
                value = aCPU->sfrread(aCPU, address);
            else
                value = aCPU->mSFR[address - 0x80];
        }

        add_solve_flags(aCPU, aCPU->mSFR[REG_ACC], value);
        aCPU->mSFR[REG_ACC] += value;
    }
    else
    {
        add_solve_flags(aCPU, aCPU->mSFR[REG_ACC], aCPU->mLowerData[address] + carry);
        aCPU->mSFR[REG_ACC] += aCPU->mLowerData[address] + carry;
    }
    aCPU->mPC++;
    return 0;
}


static int jc_offset(struct em8051 *aCPU)
{
    if (aCPU->mSFR[REG_PSW] & PSWMASK_C)
    {
        aCPU->mPC += (signed char)aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)] + 2;
    }
    else
    {
        aCPU->mPC += 2;
    }
    return 1;
}

static int orl_mem_a(struct em8051 *aCPU)
{
    printf("ORL   %03Xh, A\n",        
        aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)]);
    //if (aCPU->sfrwrite)
    //    aCPU->sfrwrite(aCPU, address);
    aCPU->mPC += 2;
    return 0;
}

static int orl_mem_imm(struct em8051 *aCPU)
{
    printf("ORL   %03Xh, #%03Xh\n",        
        aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)],
        aCPU->mCodeMem[(aCPU->mPC + 2)&(aCPU->mCodeMemSize-1)]);
//    if (aCPU->sfrwrite)
//        aCPU->sfrwrite(aCPU, address);
    
    aCPU->mPC += 3;
    return 1;
}

static int orl_a_imm(struct em8051 *aCPU)
{
    printf("ORL   A, #%03Xh\n",        
        aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)]);
    aCPU->mPC += 2;
    return 0;
}

static int orl_a_mem(struct em8051 *aCPU)
{
    int address = aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)];
    if (address > 0x7f)
    {
        int value;
        if (aCPU->sfrread)
            value = aCPU->sfrread(aCPU, address);
        else
            value = aCPU->mSFR[address - 0x80];
        aCPU->mSFR[REG_ACC] |= value;
    }
    else
    {
        aCPU->mSFR[REG_ACC] |= aCPU->mLowerData[address];
    }
    aCPU->mPC += 2;
    return 0;
}

static int orl_a_indir_rx(struct em8051 *aCPU)
{
    int rx = (aCPU->mCodeMem[(aCPU->mPC)&(aCPU->mCodeMemSize-1)] & 1) + 
             8 * ((aCPU->mSFR[REG_PSW] & (PSWMASK_RS0|PSWMASK_RS1))>>PSW_RS0);
    int address = aCPU->mLowerData[rx];
    if (address > 0x7f)
    {
        int value;
        if (aCPU->mUpperData)
        {
            value = aCPU->mUpperData[address - 0x80];
        }
        else
        {
            if (aCPU->sfrread)
                value = aCPU->sfrread(aCPU, address);
            else
                value = aCPU->mSFR[address - 0x80];
        }

        aCPU->mSFR[REG_ACC] |= value;
    }
    else
    {
        aCPU->mSFR[REG_ACC] |= aCPU->mLowerData[address];
    }

    aCPU->mPC++;
    return 0;
}


static int jnc_offset(struct em8051 *aCPU)
{
    if (aCPU->mSFR[REG_PSW] & PSWMASK_C)
    {
        aCPU->mPC += 2;
    }
    else
    {
        aCPU->mPC += (signed char)aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)] + 2;
    }
    return 1;
}

static int anl_mem_a(struct em8051 *aCPU)
{
    printf("ANL   %03Xh, A\n",
        aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)]);
//        if (aCPU->sfrwrite)
//            aCPU->sfrwrite(aCPU, address);
    aCPU->mPC += 2;
    return 0;
}

static int anl_mem_imm(struct em8051 *aCPU)
{
    int address = aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)];
    if (address > 0x7f)
    {
        aCPU->mSFR[address - 0x80] &= aCPU->mCodeMem[(aCPU->mPC + 2)&(aCPU->mCodeMemSize-1)];
        if (aCPU->sfrwrite)
            aCPU->sfrwrite(aCPU, address);
    }
    else
    {
        aCPU->mLowerData[address] &= aCPU->mCodeMem[(aCPU->mPC + 2)&(aCPU->mCodeMemSize-1)];
    }
    aCPU->mPC += 3;
    return 1;
}

static int anl_a_imm(struct em8051 *aCPU)
{
    aCPU->mSFR[REG_ACC] &= aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)];
    aCPU->mPC += 2;
    return 0;
}

static int anl_a_mem(struct em8051 *aCPU)
{
    printf("ANL   A, %03Xh\n",
        aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)]);
/*  
    int value;
        if (aCPU->sfrread)
            value = aCPU->sfrread(aCPU, address);
        else
            value = aCPU->mSFR[address - 0x80];
*/
    aCPU->mPC += 2;
    return 0;
}

static int anl_a_indir_rx(struct em8051 *aCPU)
{
    int rx = (aCPU->mCodeMem[(aCPU->mPC)&(aCPU->mCodeMemSize-1)] & 1) + 
             8 * ((aCPU->mSFR[REG_PSW] & (PSWMASK_RS0|PSWMASK_RS1))>>PSW_RS0);
    int address = aCPU->mLowerData[rx];
    if (address > 0x7f)
    {
        int value;
        if (aCPU->mUpperData)
        {
            value = aCPU->mUpperData[address - 0x80];
        }
        else
        {
            if (aCPU->sfrread)
                value = aCPU->sfrread(aCPU, address);
            else
                value = aCPU->mSFR[address - 0x80];
        }

        aCPU->mSFR[REG_ACC] &= value;
    }
    else
    {
        aCPU->mSFR[REG_ACC] &= aCPU->mLowerData[address];
    }
    aCPU->mPC++;
    return 0;
}


static int jz_offset(struct em8051 *aCPU)
{
    if (!aCPU->mSFR[REG_ACC])
    {
        aCPU->mPC += (signed char)aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)] + 2;
    }
    else
    {
        aCPU->mPC += 2;
    }
    return 1;
}

static int xrl_mem_a(struct em8051 *aCPU)
{
    printf("XRL   %03Xh, A\n",
        aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)]);
//        if (aCPU->sfrwrite)
//            aCPU->sfrwrite(aCPU, address);
    aCPU->mPC += 2;
    return 0;
}

static int xrl_mem_imm(struct em8051 *aCPU)
{
    int address = aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)];
    if (address > 0x7f)
    {
        aCPU->mSFR[address - 0x80] ^= aCPU->mCodeMem[(aCPU->mPC + 2)&(aCPU->mCodeMemSize-1)];
        if (aCPU->sfrwrite)
            aCPU->sfrwrite(aCPU, address);
    }
    else
    {
        aCPU->mLowerData[address] ^= aCPU->mCodeMem[(aCPU->mPC + 2)&(aCPU->mCodeMemSize-1)];
    }
    aCPU->mPC += 3;
    return 1;
}

static int xrl_a_imm(struct em8051 *aCPU)
{
    printf("XRL   A, #%03Xh\n",
        aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)]);
    aCPU->mPC += 2;
    return 0;
}

static int xrl_a_mem(struct em8051 *aCPU)
{
    printf("XRL   A, %03Xh\n",
        aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)]);
/*
        int value;
        if (aCPU->sfrread)
            value = aCPU->sfrread(aCPU, address);
        else
            value = aCPU->mSFR[address - 0x80];

*/
    aCPU->mPC += 2;
    return 0;
}

static int xrl_a_indir_rx(struct em8051 *aCPU)
{
    int rx = (aCPU->mCodeMem[(aCPU->mPC)&(aCPU->mCodeMemSize-1)] & 1) + 
             8 * ((aCPU->mSFR[REG_PSW] & (PSWMASK_RS0|PSWMASK_RS1))>>PSW_RS0);
    int address = aCPU->mLowerData[rx];
    if (address > 0x7f)
    {
        int value;
        if (aCPU->mUpperData)
        {
            value = aCPU->mUpperData[address - 0x80];
        }
        else
        {
            if (aCPU->sfrread)
                value = aCPU->sfrread(aCPU, address);
            else
                value = aCPU->mSFR[address - 0x80];
        }

        aCPU->mSFR[REG_ACC] ^= value;
    }
    else
    {
        aCPU->mSFR[REG_ACC] ^= aCPU->mLowerData[address];
    }
    aCPU->mPC++;;
    return 0;
}


static int jnz_offset(struct em8051 *aCPU)
{
    if (aCPU->mSFR[REG_ACC])
    {
        aCPU->mPC += (signed char)aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)] + 2;
    }
    else
    {
        aCPU->mPC += 2;
    }
    return 1;
}

static int orl_c_bitaddr(struct em8051 *aCPU)
{
    printf("ORL   C, %03Xh\n",
        aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)]);
    aCPU->mPC += 2;
    return 1;
}

static int jmp_indir_a_dptr(struct em8051 *aCPU)
{
    printf("JMP   @A+DPTR");
    return 1;
}

static int mov_a_imm(struct em8051 *aCPU)
{
    aCPU->mSFR[REG_ACC] = aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)];
    aCPU->mPC += 2;
    return 0;
}

static int mov_mem_imm(struct em8051 *aCPU)
{
    int address = aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)];
    if (address > 0x7f)
    {
        aCPU->mSFR[address - 0x80] = aCPU->mCodeMem[(aCPU->mPC + 2)&(aCPU->mCodeMemSize-1)];
        if (aCPU->sfrwrite)
            aCPU->sfrwrite(aCPU, address);
    }
    else
    {
        aCPU->mLowerData[address] = aCPU->mCodeMem[(aCPU->mPC + 2)&(aCPU->mCodeMemSize-1)];
    }

    aCPU->mPC += 3;
    return 1;
}

static int mov_indir_rx_imm(struct em8051 *aCPU)
{
    int rx = (aCPU->mCodeMem[(aCPU->mPC)&(aCPU->mCodeMemSize-1)] & 1) + 
             8 * ((aCPU->mSFR[REG_PSW] & (PSWMASK_RS0|PSWMASK_RS1))>>PSW_RS0);
    int address = aCPU->mLowerData[rx];
    int value = aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)];
    if (address > 0x7f)
    {
        if (aCPU->mUpperData)
        {
            aCPU->mUpperData[address - 0x80] = value;
        }
        else
        {
            aCPU->mSFR[address - 0x80] = value;
            if (aCPU->sfrwrite)
                aCPU->sfrwrite(aCPU, address);
        }
    }
    else
    {
        aCPU->mLowerData[address] = value;
    }

    aCPU->mPC += 2;
    return 0;
}


static int sjmp_offset(struct em8051 *aCPU)
{
    aCPU->mPC += (signed char)(aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)]) + 2;
    return 1;
}

static int anl_c_bitaddr(struct em8051 *aCPU)
{
    printf("ANL   C, %03Xh\n",
        aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)]);
    aCPU->mPC += 2;
    return 0;
}

static int movc_a_indir_a_pc(struct em8051 *aCPU)
{
    int address = aCPU->mPC + 1 + aCPU->mSFR[REG_ACC];
    aCPU->mSFR[REG_ACC] = aCPU->mCodeMem[address & (aCPU->mCodeMemSize - 1)];
    aCPU->mPC++;
    return 0;
}

static int div_ab(struct em8051 *aCPU)
{
    int a = aCPU->mSFR[REG_ACC];
    int b = aCPU->mSFR[REG_B];
    int res;
    aCPU->mSFR[REG_PSW] &= ~(PSWMASK_C|PSWMASK_OV);
    if (b)
    {
        res = a/b;
        b = a % b;
        a = res;
    }
    else
    {
        aCPU->mSFR[REG_PSW] |= PSWMASK_OV;
    }
    aCPU->mSFR[REG_ACC] = a;
    aCPU->mSFR[REG_B] = b;
    aCPU->mPC++;
    return 3;
}

static int mov_mem_mem(struct em8051 *aCPU)
{
    int address1 = aCPU->mCodeMem[(aCPU->mPC + 2)&(aCPU->mCodeMemSize-1)];
    int address2 = aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)];
    if (address1 > 0x7f)
    {
        if (address2 > 0x7f)
        {
            int value;
            if (aCPU->sfrread)
                value = aCPU->sfrread(aCPU, address2);
            else
                value = aCPU->mSFR[address2 - 0x80];
            aCPU->mSFR[address1 - 0x80] = value;
            if (aCPU->sfrwrite)
                aCPU->sfrwrite(aCPU, address1);
        }
        else
        {
            aCPU->mSFR[address1 - 0x80] = aCPU->mLowerData[address2];
            if (aCPU->sfrwrite)
                aCPU->sfrwrite(aCPU, address1);
        }
    }
    else
    {
        if (address2 > 0x7f)
        {
            int value;
            if (aCPU->sfrread)
                value = aCPU->sfrread(aCPU, address2);
            else
                value = aCPU->mSFR[address2 - 0x80];
            aCPU->mLowerData[address1] = value;
        }
        else
        {
            aCPU->mLowerData[address1] = aCPU->mLowerData[address2];
        }
    }

    aCPU->mPC += 3;
    return 1;
}

static int mov_mem_indir_rx(struct em8051 *aCPU)
{
    printf("MOV   %03Xh, @R%d\n",        
        aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)],
        aCPU->mCodeMem[(aCPU->mPC)&(aCPU->mCodeMemSize-1)]&1);
//        if (aCPU->sfrwrite)
//            aCPU->sfrwrite(aCPU, address);
    aCPU->mPC += 2;
    return 1;
}


static int mov_dptr_imm(struct em8051 *aCPU)
{
    aCPU->mSFR[REG_DPH] = aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)];
    aCPU->mSFR[REG_DPL] = aCPU->mCodeMem[(aCPU->mPC + 2)&(aCPU->mCodeMemSize-1)];
    aCPU->mPC += 3;
    return 1;
}

static int mov_bitaddr_c(struct em8051 *aCPU) 
{
    int address = aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)];
    int carry = (aCPU->mSFR[REG_PSW] & PSWMASK_C)?1:0;
    if (address > 0x7f)
    {
        // Data sheet does not explicitly say that the modification source
        // is read from output latch, but we'll assume that is what happens.
        int bit = address & 7;
        int bitmask = (1 << bit);
        address &= 0xf8;        
        aCPU->mSFR[address - 0x80] = (aCPU->mSFR[address - 0x80] & ~bitmask) | (carry << bit);
        if (aCPU->sfrwrite)
            aCPU->sfrwrite(aCPU, address);
    }
    else
    {
        int bit = address & 7;
        int bitmask = (1 << bit);
        address >>= 3;
        address += 0x20;
        aCPU->mLowerData[address] = (aCPU->mLowerData[address] & ~bitmask) | (carry << bit);
    }
    aCPU->mPC += 2;
    return 1;
}

static int movc_a_indir_a_dptr(struct em8051 *aCPU)
{
    int address = (aCPU->mSFR[REG_DPH] << 8) | (aCPU->mSFR[REG_DPL] << 0) + aCPU->mSFR[REG_ACC];
    aCPU->mSFR[REG_ACC] = aCPU->mCodeMem[address & (aCPU->mCodeMemSize - 1)];
    aCPU->mPC++;
    return 1;
}

static int subb_a_imm(struct em8051 *aCPU)
{
    printf("SUBB  A, #%03Xh\n",
        aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)]);
    aCPU->mPC += 2;
    return 0;
}

static int subb_a_mem(struct em8051 *aCPU) 
{
    printf("SUBB  A, %03Xh\n",
        aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)]);
/*
        int value;
        if (aCPU->sfrread)
            value = aCPU->sfrread(aCPU, address);
        else
            value = aCPU->mSFR[address - 0x80];

*/

    aCPU->mPC += 2;
    return 0;
}
static int subb_a_indir_rx(struct em8051 *aCPU)
{
    printf("SUBB  A, @R%d\n",        
        aCPU->mCodeMem[(aCPU->mPC)&(aCPU->mCodeMemSize-1)]&1);
    aCPU->mPC++;
    return 0;
}


static int orl_c_compl_bitaddr(struct em8051 *aCPU)
{
    printf("ORL   C, /%03Xh\n",
        aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)]);
    aCPU->mPC += 2;
    return 0;
}

static int mov_c_bitaddr(struct em8051 *aCPU) 
{
    int address = aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)];
    int carry = (aCPU->mSFR[REG_PSW] & PSWMASK_C) ? 1 : 0;
    if (address > 0x7f)
    {
        int bit = address & 7;
        int bitmask = (1 << bit);
        int value;
        address &= 0xf8;        
        if (aCPU->sfrread)
            value = aCPU->sfrread(aCPU, address);
        else
            value = aCPU->mSFR[address - 0x80];

        value = (value & bitmask) ? 1 : 0;

        aCPU->mSFR[REG_PSW] = (aCPU->mSFR[REG_PSW] & ~PSWMASK_C) | (PSWMASK_C * value);
    }
    else
    {
        int bit = address & 7;
        int bitmask = (1 << bit);
        int value;
        address >>= 3;
        address += 0x20;
        value = (aCPU->mLowerData[address] & bitmask) ? 1 : 0;
        aCPU->mSFR[REG_PSW] = (aCPU->mSFR[REG_PSW] & ~PSWMASK_C) | (PSWMASK_C * value);
    }

    aCPU->mPC += 2;
    return 0;
}

static int inc_dptr(struct em8051 *aCPU)
{
    aCPU->mSFR[REG_DPL]++;
    if (!aCPU->mSFR[REG_DPL])
        aCPU->mSFR[REG_DPH]++;
    aCPU->mPC++;
    return 1;
}

static int mul_ab(struct em8051 *aCPU)
{
    int a = aCPU->mSFR[REG_ACC];
    int b = aCPU->mSFR[REG_B];
    int res = a*b;
    aCPU->mSFR[REG_ACC] = res & 0xff;
    aCPU->mSFR[REG_B] = res >> 8;
    aCPU->mSFR[REG_PSW] &= ~(PSWMASK_C|PSWMASK_OV);
    if (aCPU->mSFR[REG_B])
        aCPU->mSFR[REG_PSW] |= PSWMASK_OV;
    aCPU->mPC++;
    return 3;
}

static int mov_indir_rx_mem(struct em8051 *aCPU)
{
    printf("MOV   @R%d, %03Xh\n",        
        aCPU->mCodeMem[(aCPU->mPC)&(aCPU->mCodeMemSize-1)]&1,
        aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)]);
/*
        int value;
        if (aCPU->sfrread)
            value = aCPU->sfrread(aCPU, address);
        else
            value = aCPU->mSFR[address - 0x80];

*/
    aCPU->mPC += 2;
    return 1;
}


static int anl_c_compl_bitaddr(struct em8051 *aCPU)
{
    printf("ANL   C, /%03Xh\n",
        aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)]);
    aCPU->mPC += 2;
    return 0;
}


static int cpl_bitaddr(struct em8051 *aCPU)
{
    printf("CPL   %03Xh\n",
        aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)]);
    aCPU->mPC += 2;
    return 0;
}

static int cpl_c(struct em8051 *aCPU)
{
    aCPU->mSFR[REG_PSW] = (aCPU->mSFR[REG_PSW] & ~PSWMASK_C) |
                          (~aCPU->mSFR[REG_PSW] & PSWMASK_C);
    aCPU->mPC++;
    return 0;
}

static int cjne_a_imm_offset(struct em8051 *aCPU)
{
    printf("CJNE  A, #%03Xh, #%d\n",
        aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)],
        (signed char)(aCPU->mCodeMem[(aCPU->mPC + 2)&(aCPU->mCodeMemSize-1)]));
    return 1;
}

static int cjne_a_mem_offset(struct em8051 *aCPU)
{
    int address = aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)];
    int value;
    if (address > 0x7f)
    {
        if (aCPU->sfrread)
            value = aCPU->sfrread(aCPU, address);
        else
            value = aCPU->mSFR[address - 0x80];
    }
    else
    {
        value = aCPU->mLowerData[address];
    }  

    if (aCPU->mSFR[REG_ACC] < value)
    {
        aCPU->mSFR[REG_PSW] |= PSWMASK_C;
    }
    else
    {
        aCPU->mSFR[REG_PSW] &= ~PSWMASK_C;
    }

    if (aCPU->mSFR[REG_ACC] != value)
    {
        aCPU->mPC += (signed char)aCPU->mCodeMem[(aCPU->mPC + 2)&(aCPU->mCodeMemSize-1)] + 3;
    }
    else
    {
        aCPU->mPC += 3;
    }
    return 1;
}
static int cjne_indir_rx_imm_offset(struct em8051 *aCPU)
{
    printf("CJNE  R%d, #%03Xh, #%d\n",
        aCPU->mCodeMem[(aCPU->mPC)&(aCPU->mCodeMemSize-1)]&1,
        aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)],
        (signed char)(aCPU->mCodeMem[(aCPU->mPC + 2)&(aCPU->mCodeMemSize-1)]));
    return 1;
}

static int push_mem(struct em8051 *aCPU)
{
    int address = aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)];
    if (address > 0x7f)
    {
        int value;
        if (aCPU->sfrread)
            value = aCPU->sfrread(aCPU, address);
        else
            value = aCPU->mSFR[address - 0x80];
        aCPU->mSFR[REG_SP]++;
        aCPU->mLowerData[aCPU->mSFR[REG_SP] & 0x7f] = value;
    }
    else
    {
        aCPU->mSFR[REG_SP]++;
        aCPU->mLowerData[aCPU->mSFR[REG_SP] & 0x7f] = aCPU->mLowerData[address];
    }

    if (aCPU->mSFR[REG_SP] >= 0x80)
        if (aCPU->except)
            aCPU->except(aCPU, EXCEPTION_STACK);
   
    aCPU->mPC += 2;
    return 1;
}


static int clr_bitaddr(struct em8051 *aCPU)
{
    int address = aCPU->mCodeMem[(aCPU->mPC + 1) & (aCPU->mCodeMemSize - 1)];
    if (address > 0x7f)
    {
        // Data sheet does not explicitly say that the modification source
        // is read from output latch, but we'll assume that is what happens.
        int bit = address & 7;
        int bitmask = (1 << bit);
        address &= 0xf8;        
        aCPU->mSFR[address - 0x80] &= ~bitmask;
        if (aCPU->sfrwrite)
            aCPU->sfrwrite(aCPU, address);
    }
    else
    {
        int bit = address & 7;
        int bitmask = (1 << bit);
        address >>= 3;
        address += 0x20;
        aCPU->mLowerData[address] &= ~bitmask;
    }
    aCPU->mPC += 2;
    return 0;
}

static int clr_c(struct em8051 *aCPU)
{
    aCPU->mSFR[REG_PSW] &= ~PSWMASK_C;
    aCPU->mPC++;
    return 0;
}

static int swap_a(struct em8051 *aCPU)
{
    aCPU->mSFR[REG_ACC] = (aCPU->mSFR[REG_ACC] << 4) | (aCPU->mSFR[REG_ACC] >> 4);
    aCPU->mPC++;
    return 0;
}

static int xch_a_mem(struct em8051 *aCPU)
{
    printf("XCH   A, %03Xh\n",
        aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)]);
//        if (aCPU->sfrwrite)
//            aCPU->sfrwrite(aCPU, address);
/*
        int value;
        if (aCPU->sfrread)
            value = aCPU->sfrread(aCPU, address);
        else
            value = aCPU->mSFR[address - 0x80];

*/
    aCPU->mPC += 2;
    return 0;
}

static int xch_a_indir_rx(struct em8051 *aCPU)
{
    printf("XCH   A, @R%d\n",        
        aCPU->mCodeMem[(aCPU->mPC)&(aCPU->mCodeMemSize-1)]&1);
    aCPU->mPC++;
    return 0;
}


static int pop_mem(struct em8051 *aCPU)
{
    int address = aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)];
    if (address > 0x7f)
    {
        aCPU->mSFR[address - 0x80] = aCPU->mLowerData[aCPU->mSFR[REG_SP] & 0x7f];
        aCPU->mSFR[REG_SP]--;
        if (aCPU->sfrwrite)
            aCPU->sfrwrite(aCPU, address);
    }
    else
    {
        aCPU->mLowerData[address] = aCPU->mLowerData[aCPU->mSFR[REG_SP] & 0x7f];
        aCPU->mSFR[REG_SP]--;
    }

    if (aCPU->mSFR[REG_SP] >= 0x80)
        if (aCPU->except)
            aCPU->except(aCPU, EXCEPTION_STACK);

    aCPU->mPC += 2;
    return 1;
}

static int setb_bitaddr(struct em8051 *aCPU)
{
    int address = aCPU->mCodeMem[(aCPU->mPC + 1) & (aCPU->mCodeMemSize - 1)];
    if (address > 0x7f)
    {
        // Data sheet does not explicitly say that the modification source
        // is read from output latch, but we'll assume that is what happens.
        int bit = address & 7;
        int bitmask = (1 << bit);
        address &= 0xf8;        
        aCPU->mSFR[address - 0x80] |= bitmask;
        if (aCPU->sfrwrite)
            aCPU->sfrwrite(aCPU, address);
    }
    else
    {
        int bit = address & 7;
        int bitmask = (1 << bit);
        address >>= 3;
        address += 0x20;
        aCPU->mLowerData[address] |= bitmask;
    }
    aCPU->mPC += 2;
    return 0;
}

static int setb_c(struct em8051 *aCPU)
{
    aCPU->mSFR[REG_PSW] |= PSWMASK_C;
    aCPU->mPC++;
    return 0;
}

static int da_a(struct em8051 *aCPU)
{
    // data sheets for this operation are a bit unclear..
    // - should AC (or C) ever be cleared?
    // - should this be done in two steps?

    int result = aCPU->mSFR[REG_ACC];
    if ((result & 0xf) > 9 || (aCPU->mSFR[REG_PSW] & PSWMASK_AC))
        result += 0x6;
    if ((result & 0xff0) > 0x90 || (aCPU->mSFR[REG_PSW] & PSWMASK_C))
        result += 0x60;
    if (result > 0x99)
        aCPU->mSFR[REG_PSW] |= PSWMASK_C;
    aCPU->mSFR[REG_ACC] = result;

 /*
    // this is basically what intel datasheet says the op should do..
    int adder = 0;
    if (aCPU->mSFR[REG_ACC] & 0xf > 9 || aCPU->mSFR[REG_PSW] & PSWMASK_AC)
        adder = 6;
    if (aCPU->mSFR[REG_ACC] & 0xf0 > 0x90 || aCPU->mSFR[REG_PSW] & PSWMASK_C)
        adder |= 0x60;
    adder += aCPU[REG_ACC];
    if (adder > 0x99)
        aCPU->mSFR[REG_PSW] |= PSWMASK_C;
    aCPU[REG_ACC] = adder;
*/
    aCPU->mPC++;
    return 0;
}

static int djnz_mem_offset(struct em8051 *aCPU)
{
    printf("DJNZ  %03Xh, #%d\n",        
        aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)],
        (signed char)(aCPU->mCodeMem[(aCPU->mPC + 2)&(aCPU->mCodeMemSize-1)]));
//        if (aCPU->sfrwrite)
//            aCPU->sfrwrite(aCPU, address);
    return 1;
}

static int xchd_a_indir_rx(struct em8051 *aCPU)
{
    printf("XCHD  A, @R%d\n",        
        aCPU->mCodeMem[(aCPU->mPC)&(aCPU->mCodeMemSize-1)]&1);
    aCPU->mPC++;
    return 0;
}


static int movx_a_indir_dptr(struct em8051 *aCPU)
{
    int dptr = (aCPU->mSFR[REG_DPH] << 8) | aCPU->mSFR[REG_DPL];
    if (aCPU->mExtData)
        aCPU->mSFR[REG_ACC] = aCPU->mExtData[dptr & (aCPU->mExtDataSize - 1)];
    aCPU->mPC++;
    return 1;
}

static int movx_a_indir_rx(struct em8051 *aCPU)
{
    int rx = (aCPU->mCodeMem[(aCPU->mPC)&(aCPU->mCodeMemSize-1)] & 1) + 
             8 * ((aCPU->mSFR[REG_PSW] & (PSWMASK_RS0|PSWMASK_RS1))>>PSW_RS0);
    int address = aCPU->mLowerData[rx];
    aCPU->mSFR[REG_ACC] = aCPU->mExtData[address];

    aCPU->mPC++;
    return 1;
}

static int clr_a(struct em8051 *aCPU)
{
    aCPU->mSFR[REG_ACC] = 0;
    aCPU->mPC++;
    return 0;
}

static int mov_a_mem(struct em8051 *aCPU)
{
    // mov a,acc is not a valid instruction
    int address = aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)];
    if (address > 0x7f)
    {
        int value;
        if (aCPU->sfrread)
            value = aCPU->sfrread(aCPU, address);
        else
            value = aCPU->mSFR[address - 0x80];
        aCPU->mSFR[REG_ACC] = value;
        if (REG_ACC == address - 0x80)
            if (aCPU->except)
                aCPU->except(aCPU, EXCEPTION_ACC_TO_A);
    }
    else
    {
        aCPU->mSFR[REG_ACC] = aCPU->mLowerData[address];
    }

    aCPU->mPC += 2;
    return 0;
}

static int mov_a_indir_rx(struct em8051 *aCPU)
{
    int rx = (aCPU->mCodeMem[(aCPU->mPC)&(aCPU->mCodeMemSize-1)] & 1) + 
             8 * ((aCPU->mSFR[REG_PSW] & (PSWMASK_RS0|PSWMASK_RS1))>>PSW_RS0);
    int address = aCPU->mLowerData[rx];
    if (address > 0x7f)
    {
        int value;
        if (aCPU->mUpperData)
        {
            value = aCPU->mUpperData[address - 0x80];
        }
        else
        {
            if (aCPU->sfrread)
                value = aCPU->sfrread(aCPU, address);
            else
                value = aCPU->mSFR[address - 0x80];
        }

        aCPU->mSFR[REG_ACC] = value;
    }
    else
    {
        aCPU->mSFR[REG_ACC] = aCPU->mLowerData[address];
    }

    aCPU->mPC++;
    return 0;
}


static int movx_indir_dptr_a(struct em8051 *aCPU)
{
    int dptr = (aCPU->mSFR[REG_DPH] << 8) | aCPU->mSFR[REG_DPL];
    if (aCPU->mExtData)
        aCPU->mExtData[dptr & (aCPU->mExtDataSize - 1)] = aCPU->mSFR[REG_ACC];
    aCPU->mPC++;
    return 1;
}

static int movx_indir_rx_a(struct em8051 *aCPU)
{
    int rx = (aCPU->mCodeMem[(aCPU->mPC)&(aCPU->mCodeMemSize-1)] & 1) + 
             8 * ((aCPU->mSFR[REG_PSW] & (PSWMASK_RS0|PSWMASK_RS1))>>PSW_RS0);
    int address = aCPU->mLowerData[rx];
    aCPU->mExtData[address] = aCPU->mSFR[REG_ACC];
    aCPU->mPC++;
    return 1;
}

static int cpl_a(struct em8051 *aCPU)
{
    aCPU->mSFR[REG_ACC] = ~aCPU->mSFR[REG_ACC];
    aCPU->mPC++;
    return 0;
}

static int mov_mem_a(struct em8051 *aCPU)
{
    int address = aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)];
    if (address > 0x7f)
    {
        aCPU->mSFR[address - 0x80] = aCPU->mSFR[REG_ACC];
        if (aCPU->sfrwrite)
            aCPU->sfrwrite(aCPU, address);
    }
    else
    {
        aCPU->mLowerData[address] = aCPU->mSFR[REG_ACC];
    }
    aCPU->mPC += 2;
    return 0;
}

static int mov_indir_rx_a(struct em8051 *aCPU)
{
    int rx = (aCPU->mCodeMem[(aCPU->mPC)&(aCPU->mCodeMemSize-1)] & 1) + 
             8 * ((aCPU->mSFR[REG_PSW] & (PSWMASK_RS0|PSWMASK_RS1))>>PSW_RS0);
    int address = aCPU->mLowerData[rx];
    if (address > 0x7f)
    {
        if (aCPU->mUpperData)
            aCPU->mUpperData[address - 0x80] = aCPU->mSFR[REG_ACC];
        else
            aCPU->mSFR[address - 0x80] = aCPU->mSFR[REG_ACC];
    }
    else
    {
        aCPU->mLowerData[address] = aCPU->mSFR[REG_ACC];
    }

    aCPU->mPC++;
    return 0;
}

static int nop(struct em8051 *aCPU)
{
    if (aCPU->mCodeMem[aCPU->mPC & (aCPU->mCodeMemSize - 1)] != 0)
        if (aCPU->except)
            aCPU->except(aCPU, EXCEPTION_ILLEGAL_OPCODE);
    aCPU->mPC++;
    return 0;
}

static int inc_rx(struct em8051 *aCPU)
{
    int rx = (aCPU->mCodeMem[(aCPU->mPC)&(aCPU->mCodeMemSize-1)] & 7) + 
             8 * ((aCPU->mSFR[REG_PSW] & (PSWMASK_RS0|PSWMASK_RS1))>>PSW_RS0);
    aCPU->mLowerData[rx]++;
    aCPU->mPC++;
    return 0;
}

static int dec_rx(struct em8051 *aCPU)
{
    int rx = (aCPU->mCodeMem[(aCPU->mPC)&(aCPU->mCodeMemSize-1)] & 7) + 
             8 * ((aCPU->mSFR[REG_PSW] & (PSWMASK_RS0|PSWMASK_RS1))>>PSW_RS0);
    aCPU->mLowerData[rx]--;
    aCPU->mPC++;
    return 0;
}

static int add_a_rx(struct em8051 *aCPU)
{
    int rx = (aCPU->mCodeMem[(aCPU->mPC)&(aCPU->mCodeMemSize-1)] & 7) + 
             8 * ((aCPU->mSFR[REG_PSW] & (PSWMASK_RS0|PSWMASK_RS1))>>PSW_RS0);
    add_solve_flags(aCPU, aCPU->mLowerData[rx], aCPU->mSFR[REG_ACC]);
    aCPU->mSFR[REG_ACC] += aCPU->mLowerData[rx];
    aCPU->mPC++;
    return 0;
}

static int addc_a_rx(struct em8051 *aCPU)
{
    int rx = (aCPU->mCodeMem[(aCPU->mPC)&(aCPU->mCodeMemSize-1)] & 7) + 
             8 * ((aCPU->mSFR[REG_PSW] & (PSWMASK_RS0|PSWMASK_RS1))>>PSW_RS0);
    int carry = (aCPU->mSFR[REG_PSW] & PSWMASK_C) ? 1 : 0;
    add_solve_flags(aCPU, aCPU->mLowerData[rx] + carry, aCPU->mSFR[REG_ACC]);
    aCPU->mSFR[REG_ACC] += aCPU->mLowerData[rx] + carry;
    aCPU->mPC++;
    return 0;
}

static int orl_a_rx(struct em8051 *aCPU)
{
    int rx = (aCPU->mCodeMem[(aCPU->mPC)&(aCPU->mCodeMemSize-1)] & 7) + 
             8 * ((aCPU->mSFR[REG_PSW] & (PSWMASK_RS0|PSWMASK_RS1))>>PSW_RS0);
    aCPU->mSFR[REG_ACC] |= aCPU->mLowerData[rx];
    aCPU->mPC++;
    return 0;
}

static int anl_a_rx(struct em8051 *aCPU)
{
    int rx = (aCPU->mCodeMem[(aCPU->mPC)&(aCPU->mCodeMemSize-1)] & 7) + 
             8 * ((aCPU->mSFR[REG_PSW] & (PSWMASK_RS0|PSWMASK_RS1))>>PSW_RS0);
    aCPU->mSFR[REG_ACC] &= aCPU->mLowerData[rx];
    aCPU->mPC++;
    return 0;
}

static int xrl_a_rx(struct em8051 *aCPU)
{
    int rx = (aCPU->mCodeMem[(aCPU->mPC)&(aCPU->mCodeMemSize-1)] & 7) + 
             8 * ((aCPU->mSFR[REG_PSW] & (PSWMASK_RS0|PSWMASK_RS1))>>PSW_RS0);
    aCPU->mSFR[REG_ACC] ^= aCPU->mLowerData[rx];    
    aCPU->mPC++;
    return 0;
}


static int mov_rx_imm(struct em8051 *aCPU)
{
    int rx = (aCPU->mCodeMem[(aCPU->mPC)&(aCPU->mCodeMemSize-1)] & 7) + 
             8 * ((aCPU->mSFR[REG_PSW] & (PSWMASK_RS0|PSWMASK_RS1))>>PSW_RS0);
    aCPU->mLowerData[rx] = aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)];
    aCPU->mPC += 2;
    return 0;
}

static int mov_mem_rx(struct em8051 *aCPU)
{
    int rx = (aCPU->mCodeMem[(aCPU->mPC)&(aCPU->mCodeMemSize-1)] & 7) + 
             8 * ((aCPU->mSFR[REG_PSW] & (PSWMASK_RS0|PSWMASK_RS1))>>PSW_RS0);
    int address = aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)];
    if (address > 0x7f)
    {
        aCPU->mSFR[address - 0x80] = aCPU->mLowerData[rx];
        if (aCPU->sfrwrite)
            aCPU->sfrwrite(aCPU, address);
    }
    else
    {
        aCPU->mLowerData[address] = aCPU->mLowerData[rx];
    }
    aCPU->mPC += 2;
    return 1;
}

static int subb_a_rx(struct em8051 *aCPU)
{
    int rx = (aCPU->mCodeMem[(aCPU->mPC)&(aCPU->mCodeMemSize-1)] & 7) + 
             8 * ((aCPU->mSFR[REG_PSW] & (PSWMASK_RS0|PSWMASK_RS1))>>PSW_RS0);
    int carry = (aCPU->mSFR[REG_PSW] & PSWMASK_C) ? 1 : 0;
    sub_solve_flags(aCPU, aCPU->mSFR[REG_ACC], aCPU->mLowerData[rx] + carry);
    aCPU->mSFR[REG_ACC] -= aCPU->mLowerData[rx] + carry;
    aCPU->mPC++;
    return 0;
}

static int mov_rx_mem(struct em8051 *aCPU)
{
    int rx = (aCPU->mCodeMem[(aCPU->mPC)&(aCPU->mCodeMemSize-1)] & 7) + 
             8 * ((aCPU->mSFR[REG_PSW] & (PSWMASK_RS0|PSWMASK_RS1))>>PSW_RS0);
    int address = aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)];
    if (address > 0x7f)
    {
        int value;
        if (aCPU->sfrread)
            value = aCPU->sfrread(aCPU, address);
        else
            value = aCPU->mSFR[address - 0x80];
        aCPU->mLowerData[rx] = value;
    }
    else
    {
        aCPU->mLowerData[rx] = aCPU->mLowerData[address];
    }

    aCPU->mPC += 2;
    return 1;
}

static int cjne_rx_imm_offset(struct em8051 *aCPU)
{
    int rx = (aCPU->mCodeMem[(aCPU->mPC)&(aCPU->mCodeMemSize-1)] & 7) + 
             8 * ((aCPU->mSFR[REG_PSW] & (PSWMASK_RS0|PSWMASK_RS1))>>PSW_RS0);
    int value = aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)];
    
    if (aCPU->mLowerData[rx] < value)
    {
        aCPU->mSFR[REG_PSW] |= PSWMASK_C;
    }
    else
    {
        aCPU->mSFR[REG_PSW] &= ~PSWMASK_C;
    }

    if (aCPU->mLowerData[rx] != value)
    {
        aCPU->mPC += (signed char)aCPU->mCodeMem[(aCPU->mPC + 2)&(aCPU->mCodeMemSize-1)] + 3;
    }
    else
    {
        aCPU->mPC += 3;
    }
    return 1;
}

static int xch_a_rx(struct em8051 *aCPU)
{
    int rx = (aCPU->mCodeMem[(aCPU->mPC)&(aCPU->mCodeMemSize-1)] & 7) + 
             8 * ((aCPU->mSFR[REG_PSW] & (PSWMASK_RS0|PSWMASK_RS1))>>PSW_RS0);
    int a = aCPU->mSFR[REG_ACC];
    aCPU->mSFR[REG_ACC] = aCPU->mLowerData[rx];
    aCPU->mLowerData[rx] = a;
    aCPU->mPC++;
    return 0;
}

static int djnz_rx_offset(struct em8051 *aCPU)
{
    int rx = (aCPU->mCodeMem[(aCPU->mPC)&(aCPU->mCodeMemSize-1)] & 7) + 
             8 * ((aCPU->mSFR[REG_PSW] & (PSWMASK_RS0|PSWMASK_RS1))>>PSW_RS0);
    aCPU->mLowerData[rx]--;
    if (aCPU->mLowerData[rx])
    {
        aCPU->mPC += (signed char)aCPU->mCodeMem[(aCPU->mPC + 1)&(aCPU->mCodeMemSize-1)] + 2;
    }
    else
    {
        aCPU->mPC += 2;
    }
    return 1;
}

static int mov_a_rx(struct em8051 *aCPU)
{
    int rx = (aCPU->mCodeMem[(aCPU->mPC)&(aCPU->mCodeMemSize-1)] & 7) + 
             8 * ((aCPU->mSFR[REG_PSW] & (PSWMASK_RS0|PSWMASK_RS1))>>PSW_RS0);
    aCPU->mSFR[REG_ACC] = aCPU->mLowerData[rx];

    aCPU->mPC++;
    return 0;
}

static int mov_rx_a(struct em8051 *aCPU)
{
    int rx = (aCPU->mCodeMem[(aCPU->mPC)&(aCPU->mCodeMemSize-1)] & 7) + 
             8 * ((aCPU->mSFR[REG_PSW] & (PSWMASK_RS0|PSWMASK_RS1))>>PSW_RS0);
    aCPU->mLowerData[rx] = aCPU->mSFR[REG_ACC];
    aCPU->mPC++;
    return 0;
}

void op_setptrs(struct em8051 *aCPU)
{
    int i;
    for (i = 0; i < 8; i++)
    {
        aCPU->op[0x08 + i] = &inc_rx;
        aCPU->op[0x18 + i] = &dec_rx;
        aCPU->op[0x28 + i] = &add_a_rx;
        aCPU->op[0x38 + i] = &addc_a_rx;
        aCPU->op[0x48 + i] = &orl_a_rx;
        aCPU->op[0x58 + i] = &anl_a_rx;
        aCPU->op[0x68 + i] = &xrl_a_rx;
        aCPU->op[0x78 + i] = &mov_rx_imm;
        aCPU->op[0x88 + i] = &mov_mem_rx;
        aCPU->op[0x98 + i] = &subb_a_rx;
        aCPU->op[0xa8 + i] = &mov_rx_mem;
        aCPU->op[0xb8 + i] = &cjne_rx_imm_offset;
        aCPU->op[0xc8 + i] = &xch_a_rx;
        aCPU->op[0xd8 + i] = &djnz_rx_offset;
        aCPU->op[0xe8 + i] = &mov_a_rx;
        aCPU->op[0xf8 + i] = &mov_rx_a;
    }
    aCPU->op[0x00] = &nop;
    aCPU->op[0x01] = &ajmp_offset;
    aCPU->op[0x02] = &ljmp_address;
    aCPU->op[0x03] = &rr_a;
    aCPU->op[0x04] = &inc_a;
    aCPU->op[0x05] = &inc_mem;
    aCPU->op[0x06] = &inc_indir_rx;
    aCPU->op[0x07] = &inc_indir_rx;

    aCPU->op[0x10] = &jbc_bitaddr_offset;
    aCPU->op[0x11] = &acall_offset;
    aCPU->op[0x12] = &lcall_address;
    aCPU->op[0x13] = &rrc_a;
    aCPU->op[0x14] = &dec_a;
    aCPU->op[0x15] = &dec_mem;
    aCPU->op[0x16] = &dec_indir_rx;
    aCPU->op[0x17] = &dec_indir_rx;

    aCPU->op[0x20] = &jb_bitaddr_offset;
    aCPU->op[0x21] = &ajmp_offset;
    aCPU->op[0x22] = &ret;
    aCPU->op[0x23] = &rl_a;
    aCPU->op[0x24] = &add_a_imm;
    aCPU->op[0x25] = &add_a_mem;
    aCPU->op[0x26] = &add_a_indir_rx;
    aCPU->op[0x27] = &add_a_indir_rx;

    aCPU->op[0x30] = &jnb_bitaddr_offset;
    aCPU->op[0x31] = &acall_offset;
    aCPU->op[0x32] = &reti;
    aCPU->op[0x33] = &rlc_a;
    aCPU->op[0x34] = &addc_a_imm;
    aCPU->op[0x35] = &addc_a_mem;
    aCPU->op[0x36] = &addc_a_indir_rx;
    aCPU->op[0x37] = &addc_a_indir_rx;

    aCPU->op[0x40] = &jc_offset;
    aCPU->op[0x41] = &ajmp_offset;
    aCPU->op[0x42] = &orl_mem_a;
    aCPU->op[0x43] = &orl_mem_imm;
    aCPU->op[0x44] = &orl_a_imm;
    aCPU->op[0x45] = &orl_a_mem;
    aCPU->op[0x46] = &orl_a_indir_rx;
    aCPU->op[0x47] = &orl_a_indir_rx;

    aCPU->op[0x50] = &jnc_offset;
    aCPU->op[0x51] = &acall_offset;
    aCPU->op[0x52] = &anl_mem_a;
    aCPU->op[0x53] = &anl_mem_imm;
    aCPU->op[0x54] = &anl_a_imm;
    aCPU->op[0x55] = &anl_a_mem;
    aCPU->op[0x56] = &anl_a_indir_rx;
    aCPU->op[0x57] = &anl_a_indir_rx;

    aCPU->op[0x60] = &jz_offset;
    aCPU->op[0x61] = &ajmp_offset;
    aCPU->op[0x62] = &xrl_mem_a;
    aCPU->op[0x63] = &xrl_mem_imm;
    aCPU->op[0x64] = &xrl_a_imm;
    aCPU->op[0x65] = &xrl_a_mem;
    aCPU->op[0x66] = &xrl_a_indir_rx;
    aCPU->op[0x67] = &xrl_a_indir_rx;

    aCPU->op[0x70] = &jnz_offset;
    aCPU->op[0x71] = &acall_offset;
    aCPU->op[0x72] = &orl_c_bitaddr;
    aCPU->op[0x73] = &jmp_indir_a_dptr;
    aCPU->op[0x74] = &mov_a_imm;
    aCPU->op[0x75] = &mov_mem_imm;
    aCPU->op[0x76] = &mov_indir_rx_imm;
    aCPU->op[0x77] = &mov_indir_rx_imm;

    aCPU->op[0x80] = &sjmp_offset;
    aCPU->op[0x81] = &ajmp_offset;
    aCPU->op[0x82] = &anl_c_bitaddr;
    aCPU->op[0x83] = &movc_a_indir_a_pc;
    aCPU->op[0x84] = &div_ab;
    aCPU->op[0x85] = &mov_mem_mem;
    aCPU->op[0x86] = &mov_mem_indir_rx;
    aCPU->op[0x87] = &mov_mem_indir_rx;

    aCPU->op[0x90] = &mov_dptr_imm;
    aCPU->op[0x91] = &acall_offset;
    aCPU->op[0x92] = &mov_bitaddr_c;
    aCPU->op[0x93] = &movc_a_indir_a_dptr;
    aCPU->op[0x94] = &subb_a_imm;
    aCPU->op[0x95] = &subb_a_mem;
    aCPU->op[0x96] = &subb_a_indir_rx;
    aCPU->op[0x97] = &subb_a_indir_rx;

    aCPU->op[0xa0] = &orl_c_compl_bitaddr;
    aCPU->op[0xa1] = &ajmp_offset;
    aCPU->op[0xa2] = &mov_c_bitaddr;
    aCPU->op[0xa3] = &inc_dptr;
    aCPU->op[0xa4] = &mul_ab;
    aCPU->op[0xa5] = &nop; // unused
    aCPU->op[0xa6] = &mov_indir_rx_mem;
    aCPU->op[0xa7] = &mov_indir_rx_mem;

    aCPU->op[0xb0] = &anl_c_compl_bitaddr;
    aCPU->op[0xb1] = &acall_offset;
    aCPU->op[0xb2] = &cpl_bitaddr;
    aCPU->op[0xb3] = &cpl_c;
    aCPU->op[0xb4] = &cjne_a_imm_offset;
    aCPU->op[0xb5] = &cjne_a_mem_offset;
    aCPU->op[0xb6] = &cjne_indir_rx_imm_offset;
    aCPU->op[0xb7] = &cjne_indir_rx_imm_offset;

    aCPU->op[0xc0] = &push_mem;
    aCPU->op[0xc1] = &ajmp_offset;
    aCPU->op[0xc2] = &clr_bitaddr;
    aCPU->op[0xc3] = &clr_c;
    aCPU->op[0xc4] = &swap_a;
    aCPU->op[0xc5] = &xch_a_mem;
    aCPU->op[0xc6] = &xch_a_indir_rx;
    aCPU->op[0xc7] = &xch_a_indir_rx;

    aCPU->op[0xd0] = &pop_mem;
    aCPU->op[0xd1] = &acall_offset;
    aCPU->op[0xd2] = &setb_bitaddr;
    aCPU->op[0xd3] = &setb_c;
    aCPU->op[0xd4] = &da_a;
    aCPU->op[0xd5] = &djnz_mem_offset;
    aCPU->op[0xd6] = &xchd_a_indir_rx;
    aCPU->op[0xd7] = &xchd_a_indir_rx;

    aCPU->op[0xe0] = &movx_a_indir_dptr;
    aCPU->op[0xe1] = &ajmp_offset;
    aCPU->op[0xe2] = &movx_a_indir_rx;
    aCPU->op[0xe3] = &movx_a_indir_rx;
    aCPU->op[0xe4] = &clr_a;
    aCPU->op[0xe5] = &mov_a_mem;
    aCPU->op[0xe6] = &mov_a_indir_rx;
    aCPU->op[0xe7] = &mov_a_indir_rx;

    aCPU->op[0xf0] = &movx_indir_dptr_a;
    aCPU->op[0xf1] = &acall_offset;
    aCPU->op[0xf2] = &movx_indir_rx_a;
    aCPU->op[0xf3] = &movx_indir_rx_a;
    aCPU->op[0xf4] = &cpl_a;
    aCPU->op[0xf5] = &mov_mem_a;
    aCPU->op[0xf6] = &mov_indir_rx_a;
    aCPU->op[0xf7] = &mov_indir_rx_a;
}