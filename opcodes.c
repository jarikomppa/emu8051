/* 8051 emulator core
 * Copyright 2006 Jari Komppa
 *
 * Permission is hereby granted, free of charge, to any person obtaining 
 * a copy of this software and associated documentation files (the 
 * "Software"), to deal in the Software without restriction, including 
 * without limitation the rights to use, copy, modify, merge, publish, 
 * distribute, sublicense, and/or sell copies of the Software, and to 
 * permit persons to whom the Software is furnished to do so, subject 
 * to the following conditions: 
 *
 * The above copyright notice and this permission notice shall be included 
 * in all copies or substantial portions of the Software. 
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS 
 * IN THE SOFTWARE. 
 *
 * (i.e. the MIT License)
 *
 * opcodes.c
 * 8051 opcode simulation functions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "emu8051.h"

#define BAD_VALUE 0x77
#define PSW CPU.mSFR[REG_PSW]
#define ACC CPU.mSFR[REG_ACC]
#define PC CPU.mPC
#define CODEMEM(x) CPU.mCodeMem[(x)&(CPU.mCodeMemMaxIdx)]
#define EXTDATA(x) CPU.mExtData[(x)&(CPU.mExtDataMaxIdx)]
#define UPRDATA(x) CPU.mUpperData[(x) - 0x80]
#define OPCODE CODEMEM(PC + 0)
#define OPERAND1 CODEMEM(PC + 1)
#define OPERAND2 CODEMEM(PC + 2)
#define INDIR_RX_ADDRESS (CPU.mLowerData[(OPCODE & 1) + 8 * ((PSW & (PSWMASK_RS0|PSWMASK_RS1))>>PSW_RS0)])
#define RX_ADDRESS ((OPCODE & 7) + 8 * ((PSW & (PSWMASK_RS0|PSWMASK_RS1))>>PSW_RS0))
#define CARRY ((PSW & PSWMASK_C) >> PSW_C)

static uint8_t read_mem(uint8_t aAddress)
{
    if (aAddress > 0x7f)
    {
        if (CPU.sfrread[aAddress - 0x80])
            return CPU.sfrread[aAddress - 0x80](aAddress);
        else
            return CPU.mSFR[aAddress - 0x80];
    }
    else
    {
        return CPU.mLowerData[aAddress];
    }
}

void push_to_stack(uint8_t aValue)
{
    CPU.mSFR[REG_SP]++;
    if (CPU.mSFR[REG_SP] > 0x7f)
    {
        if (CPU.mUpperData)
        {
            UPRDATA(CPU.mSFR[REG_SP]) = aValue;
        }
        else
        {
            if (CPU.except)
                CPU.except(EXCEPTION_STACK);
        }
    }
    else
    {
        CPU.mLowerData[CPU.mSFR[REG_SP]] = aValue;
    }
    if (CPU.mSFR[REG_SP] == 0)
        if (CPU.except)
            CPU.except(EXCEPTION_STACK);
}

static uint8_t pop_from_stack()
{
    uint8_t value = BAD_VALUE;
    if (CPU.mSFR[REG_SP] > 0x7f)
    {
        if (CPU.mUpperData)
        {
            value = UPRDATA(CPU.mSFR[REG_SP]);
        }
        else
        {
            if (CPU.except)
                CPU.except(EXCEPTION_STACK);
        }
    }
    else
    {
        value = CPU.mLowerData[CPU.mSFR[REG_SP]];
    }
    CPU.mSFR[REG_SP]--;

    if (CPU.mSFR[REG_SP] == 0xff)
        if (CPU.except)
            CPU.except(EXCEPTION_STACK);
    return value;
}


static void add_solve_flags(uint8_t value1, uint8_t value2, uint8_t acc)
{
    /* Carry: overflow from 7th bit to 8th bit */
    uint8_t carry = ((value1 & 255) + (value2 & 255) + acc) >> 8;
    
    /* Auxiliary carry: overflow from 3th bit to 4th bit */
    uint8_t auxcarry = ((value1 & 7) + (value2 & 7) + acc) >> 3;
    
    /* Overflow: overflow from 6th or 7th bit, but not both */
    uint8_t overflow = (((value1 & 127) + (value2 & 127) + acc) >> 7)^carry;
    
    PSW = (PSW & ~(PSWMASK_C | PSWMASK_AC | PSWMASK_OV)) |
          (carry << PSW_C) | (auxcarry << PSW_AC) | (overflow << PSW_OV);
}

static void sub_solve_flags(uint8_t value1, uint8_t value2)
{
    uint8_t carry = (((value1 & 255) - (value2 & 255)) >> 8) & 1;
    uint8_t auxcarry = (((value1 & 7) - (value2 & 7)) >> 3) & 1;
    uint8_t overflow = ((((value1 & 127) - (value2 & 127)) >> 7) & 1)^carry;
    PSW = (PSW & ~(PSWMASK_C|PSWMASK_AC|PSWMASK_OV)) |
                          (carry << PSW_C) | (auxcarry << PSW_AC) | (overflow << PSW_OV);
}


static uint8_t ajmp_offset()
{
    uint16_t address = ( (PC + 2) & 0xf800 ) |
                  OPERAND1 | 
                  ((OPCODE & 0xe0) << 3);

    PC = address;

    return 1;
}

static uint8_t ljmp_address()
{
    uint16_t address = (OPERAND1 << 8) | OPERAND2;
    PC = address;

    return 1;
}


static uint8_t rr_a()
{
    ACC = (ACC >> 1) | (ACC << 7);
    PC++;
    return 0;
}

static uint8_t inc_a()
{
    ACC++;
    PC++;
    return 0;
}

static uint8_t inc_mem()
{
    uint8_t address = OPERAND1;
    if (address > 0x7f)
    {
        CPU.mSFR[address - 0x80]++;
        if (CPU.sfrwrite[address - 0x80])
            CPU.sfrwrite[address - 0x80](address);
    }
    else
    {
        CPU.mLowerData[address]++;
    }
    PC += 2;
    return 0;
}

static uint8_t inc_indir_rx()
{    
    uint8_t address = INDIR_RX_ADDRESS;
    if (address > 0x7f)
    {
        if (CPU.mUpperData)
        {
            CPU.mUpperData[address - 0x80]++;
        }
    }
    else
    {
        CPU.mLowerData[address]++;
    }
    PC++;
    return 0;
}

static uint8_t jbc_bitaddr_offset()
{
    // "Note: when this instruction is used to test an output pin, the value used 
    // as the original data will be read from the output data latch, not the input pin"
    uint8_t address = OPERAND1;
    if (address > 0x7f)
    {
        uint8_t bit = address & 7;
        uint8_t bitmask = (1 << bit);
        uint8_t value;
        address &= 0xf8;        
        value = CPU.mSFR[address - 0x80];
        
        if (value & bitmask)
        {
            CPU.mSFR[address - 0x80] &= ~bitmask;
            PC += (signed char)OPERAND2 + 3;
            if (CPU.sfrwrite[address - 0x80])
                CPU.sfrwrite[address - 0x80](address);
        }
        else
        {
            PC += 3;
        }
    }
    else
    {
        uint8_t bit = address & 7;
        uint8_t bitmask = (1 << bit);
        address >>= 3;
        address += 0x20;
        if (CPU.mLowerData[address] & bitmask)
        {
            CPU.mLowerData[address] &= ~bitmask;
            PC += (signed char)OPERAND2 + 3;
        }
        else
        {
            PC += 3;
        }
    }
    return 1;
}

static uint8_t acall_offset()
{
    uint16_t address = ((PC + 2) & 0xf800) | OPERAND1 | ((OPCODE & 0xe0) << 3);
    push_to_stack((PC + 2) & 0xff);
    push_to_stack((PC + 2) >> 8);
    PC = address;
    return 1;
}

static uint8_t lcall_address()
{
    push_to_stack((PC + 3) & 0xff);
    push_to_stack((PC + 3) >> 8);
    PC = (OPERAND1 << 8) |
         (OPERAND2 << 0);
    return 1;
}

static uint8_t rrc_a()
{
    uint8_t c = (PSW & PSWMASK_C) >> PSW_C;
    uint8_t newc = ACC & 1;
    ACC = (ACC >> 1) | (c << 7);
    PSW = (PSW & ~PSWMASK_C) | (newc << PSW_C);
    PC++;
    return 0;
}

static uint8_t dec_a()
{
    ACC--;
    PC++;
    return 0;
}

static uint8_t dec_mem()
{
    uint8_t address = OPERAND1;
    if (address > 0x7f)
    {
        CPU.mSFR[address - 0x80]--;
        if (CPU.sfrwrite[address - 0x80])
            CPU.sfrwrite[address - 0x80](address);
    }
    else
    {
        CPU.mLowerData[address]--;
    }
    PC += 2;
    return 0;
}

static uint8_t dec_indir_rx()
{
    uint8_t address = INDIR_RX_ADDRESS;
    if (address > 0x7f)
    {
        if (CPU.mUpperData)
        {
            CPU.mUpperData[address - 0x80]--;
        }
    }
    else
    {
        CPU.mLowerData[address]--;
    }
    PC++;
    return 0;
}


static uint8_t jb_bitaddr_offset()
{
    uint8_t address = OPERAND1;
    if (address > 0x7f)
    {
        uint8_t bit = address & 7;
        uint8_t bitmask = (1 << bit);
        uint8_t value;
        address &= 0xf8;        
        if (CPU.sfrread[address - 0x80])
            value = CPU.sfrread[address - 0x80](address);
        else
            value = CPU.mSFR[address - 0x80];
        
        if (value & bitmask)
        {
            PC += (signed char)OPERAND2 + 3;
        }
        else
        {
            PC += 3;
        }
    }
    else
    {
        uint8_t bit = address & 7;
        uint8_t bitmask = (1 << bit);
        address >>= 3;
        address += 0x20;
        if (CPU.mLowerData[address] & bitmask)
        {
            PC += (signed char)OPERAND2 + 3;
        }
        else
        {
            PC += 3;
        }
    }
    return 1;
}

static uint8_t ret()
{
    PC = pop_from_stack() << 8;
    PC |= pop_from_stack();
    return 1;
}

static uint8_t rl_a()
{
    ACC = (ACC << 1) | (ACC >> 7);
    PC++;
    return 0;
}

static uint8_t add_a_imm()
{
    add_solve_flags(ACC, OPERAND1, 0);
    ACC += OPERAND1;
    PC += 2;
    return 0;
}

static uint8_t add_a_mem()
{
    uint8_t value = read_mem(OPERAND1);
    add_solve_flags(ACC, value, 0);
    ACC += value;
	PC += 2;
    return 0;
}

static uint8_t add_a_indir_rx()
{
    uint8_t address = INDIR_RX_ADDRESS;
    if (address > 0x7f)
    {
        uint8_t value = BAD_VALUE;
        if (CPU.mUpperData)
        {
            value = CPU.mUpperData[address - 0x80];
        }

        add_solve_flags(ACC, value, 0);
        ACC += value;
    }
    else
    {
        add_solve_flags(ACC, CPU.mLowerData[address], 0);
        ACC += CPU.mLowerData[address];
    }

    PC++;
    return 0;
}

static uint8_t jnb_bitaddr_offset()
{
    uint8_t address = OPERAND1;
    if (address > 0x7f)
    {
        uint8_t bit = address & 7;
        uint8_t bitmask = (1 << bit);
        uint8_t value;
        address &= 0xf8;        
        if (CPU.sfrread[address - 0x80])
            value = CPU.sfrread[address - 0x80](address);
        else
            value = CPU.mSFR[address - 0x80];
        
        if (!(value & bitmask))
        {
            PC += (signed char)OPERAND2 + 3;
        }
        else
        {
            PC += 3;
        }
    }
    else
    {
        uint8_t bit = address & 7;
        uint8_t bitmask = (1 << bit);
        address >>= 3;
        address += 0x20;
        if (!(CPU.mLowerData[address] & bitmask))
        {
            PC += (signed char)OPERAND2 + 3;
        }
        else
        {
            PC += 3;
        }
    }
    return 1;
}

static uint8_t reti()
{
    if (CPU.mInterruptActive)
    {
        if (CPU.except)
        {
            uint8_t hi = 0;
            if (CPU.mInterruptActive > 1)
                hi = 1;
            if (CPU.int_a[hi] != CPU.mSFR[REG_ACC])
                CPU.except(EXCEPTION_IRET_ACC_MISMATCH);
            if (CPU.int_sp[hi] != CPU.mSFR[REG_SP])
                CPU.except(EXCEPTION_IRET_SP_MISMATCH);
            if ((CPU.int_psw[hi] & (PSWMASK_OV | PSWMASK_RS0 | PSWMASK_RS1 | PSWMASK_AC | PSWMASK_C)) !=
                (CPU.mSFR[REG_PSW] & (PSWMASK_OV | PSWMASK_RS0 | PSWMASK_RS1 | PSWMASK_AC | PSWMASK_C)))
                CPU.except(EXCEPTION_IRET_PSW_MISMATCH);
        }

        if (CPU.mInterruptActive & 2)
            CPU.mInterruptActive &= ~2;
        else
            CPU.mInterruptActive = 0;
    }

    PC = pop_from_stack() << 8;
    PC |= pop_from_stack();
    return 1;
}

static uint8_t rlc_a()
{
    uint8_t carry = CARRY;
    uint8_t new_carry = ACC >> 7;
    ACC = (ACC << 1) | carry;
    PSW = (PSW & ~PSWMASK_C) | (new_carry << PSW_C);
    PC++;
    return 0;
}

static uint8_t addc_a_imm()
{
    uint8_t carry = CARRY;
    add_solve_flags(ACC, OPERAND1, carry);
    ACC += OPERAND1 + carry;
    PC += 2;
    return 0;
}

static uint8_t addc_a_mem()
{
    uint8_t carry = CARRY;
    uint8_t value = read_mem(OPERAND1);
    add_solve_flags(ACC, value, carry);
    ACC += value + carry;
    PC += 2;
    return 0;
}

static uint8_t addc_a_indir_rx()
{
    uint8_t carry = CARRY;
    uint8_t address = INDIR_RX_ADDRESS;
    if (address > 0x7f)
    {
        uint8_t value = BAD_VALUE;
        if (CPU.mUpperData)
        {
            value = CPU.mUpperData[address - 0x80];
        }

        add_solve_flags(ACC, value, carry);
        ACC += value + carry;
    }
    else
    {
        add_solve_flags(ACC, CPU.mLowerData[address], carry);
        ACC += CPU.mLowerData[address] + carry;
    }
    PC++;
    return 0;
}


static uint8_t jc_offset()
{
    if (PSW & PSWMASK_C)
    {
        PC += (signed char)OPERAND1 + 2;
    }
    else
    {
        PC += 2;
    }
    return 1;
}

static uint8_t orl_mem_a()
{
    uint8_t address = OPERAND1;
    if (address > 0x7f)
    {
        CPU.mSFR[address - 0x80] |= ACC;
        if (CPU.sfrwrite[address - 0x80])
            CPU.sfrwrite[address - 0x80](address);
    }
    else
    {
        CPU.mLowerData[address] |= ACC;
    }
    PC += 2;
    return 0;
}

static uint8_t orl_mem_imm()
{
    uint8_t address = OPERAND1;
    if (address > 0x7f)
    {
        CPU.mSFR[address - 0x80] |= OPERAND2;
        if (CPU.sfrwrite[address - 0x80])
            CPU.sfrwrite[address - 0x80](address);
    }
    else
    {
        CPU.mLowerData[address] |= OPERAND2;
    }
    
    PC += 3;
    return 1;
}

static uint8_t orl_a_imm()
{
    ACC |= OPERAND1;
    PC += 2;
    return 0;
}

static uint8_t orl_a_mem()
{
    uint8_t value = read_mem(OPERAND1);
    ACC |= value;
    PC += 2;
    return 0;
}

static uint8_t orl_a_indir_rx()
{
    uint8_t address = INDIR_RX_ADDRESS;
    if (address > 0x7f)
    {
        uint8_t value = BAD_VALUE;
        if (CPU.mUpperData)
        {
            value = CPU.mUpperData[address - 0x80];
        }

        ACC |= value;
    }
    else
    {
        ACC |= CPU.mLowerData[address];
    }

    PC++;
    return 0;
}


static uint8_t jnc_offset()
{
    if (PSW & PSWMASK_C)
    {
        PC += 2;
    }
    else
    {
        PC += (signed char)OPERAND1 + 2;
    }
    return 1;
}

static uint8_t anl_mem_a()
{
    uint8_t address = OPERAND1;
    if (address > 0x7f)
    {
        CPU.mSFR[address - 0x80] &= ACC;
        if (CPU.sfrwrite[address - 0x80])
            CPU.sfrwrite[address - 0x80](address);
    }
    else
    {
        CPU.mLowerData[address] &= ACC;
    }
    PC += 2;
    return 0;
}

static uint8_t anl_mem_imm()
{
    uint8_t address = OPERAND1;
    if (address > 0x7f)
    {
        CPU.mSFR[address - 0x80] &= OPERAND2;
        if (CPU.sfrwrite[address - 0x80])
            CPU.sfrwrite[address - 0x80](address);
    }
    else
    {
        CPU.mLowerData[address] &= OPERAND2;
    }
    PC += 3;
    return 1;
}

static uint8_t anl_a_imm()
{
    ACC &= OPERAND1;
    PC += 2;
    return 0;
}

static uint8_t anl_a_mem()
{
    uint8_t value = read_mem(OPERAND1);
    ACC &= value;
    PC += 2;
    return 0;
}

static uint8_t anl_a_indir_rx()
{
    uint8_t address = INDIR_RX_ADDRESS;
    if (address > 0x7f)
    {
        uint8_t value = BAD_VALUE;
        if (CPU.mUpperData)
        {
            value = CPU.mUpperData[address - 0x80];
        }

        ACC &= value;
    }
    else
    {
        ACC &= CPU.mLowerData[address];
    }
    PC++;
    return 0;
}


static uint8_t jz_offset()
{
    if (!ACC)
    {
        PC += (signed char)OPERAND1 + 2;
    }
    else
    {
        PC += 2;
    }
    return 1;
}

static uint8_t xrl_mem_a()
{
    uint8_t address = OPERAND1;
    if (address > 0x7f)
    {
        CPU.mSFR[address - 0x80] ^= ACC;
        if (CPU.sfrwrite[address - 0x80])
            CPU.sfrwrite[address - 0x80](address);
    }
    else
    {
        CPU.mLowerData[address] ^= ACC;
    }
    PC += 2;
    return 0;
}

static uint8_t xrl_mem_imm()
{
    uint8_t address = OPERAND1;
    if (address > 0x7f)
    {
        CPU.mSFR[address - 0x80] ^= OPERAND2;
        if (CPU.sfrwrite[address - 0x80])
            CPU.sfrwrite[address - 0x80](address);
    }
    else
    {
        CPU.mLowerData[address] ^= OPERAND2;
    }
    PC += 3;
    return 1;
}

static uint8_t xrl_a_imm()
{
    ACC ^= OPERAND1;
    PC += 2;
    return 0;
}

static uint8_t xrl_a_mem()
{
    uint8_t value = read_mem(OPERAND1);
    ACC ^= value;
    PC += 2;
    return 0;
}

static uint8_t xrl_a_indir_rx()
{
    uint8_t address = INDIR_RX_ADDRESS;
    if (address > 0x7f)
    {
        uint8_t value = BAD_VALUE;
        if (CPU.mUpperData)
        {
            value = CPU.mUpperData[address - 0x80];
        }

        ACC ^= value;
    }
    else
    {
        ACC ^= CPU.mLowerData[address];
    }
    PC++;;
    return 0;
}


static uint8_t jnz_offset()
{
    if (ACC)
    {
        PC += (signed char)OPERAND1 + 2;
    }
    else
    {
        PC += 2;
    }
    return 1;
}

static uint8_t orl_c_bitaddr()
{
    uint8_t address = OPERAND1;
    uint8_t carry = CARRY;
    if (address > 0x7f)
    {
        uint8_t bit = address & 7;
        uint8_t bitmask = (1 << bit);
        uint8_t value;
        address &= 0xf8;        
        if (CPU.sfrread[address - 0x80])
            value = CPU.sfrread[address - 0x80](address);
        else
            value = CPU.mSFR[address - 0x80];

        value = (value & bitmask) ? 1 : carry;

        PSW = (PSW & ~PSWMASK_C) | (PSWMASK_C * value);
    }
    else
    {
        uint8_t bit = address & 7;
        uint8_t bitmask = (1 << bit);
        uint8_t value;
        address >>= 3;
        address += 0x20;
        value = (CPU.mLowerData[address] & bitmask) ? 1 : carry;
        PSW = (PSW & ~PSWMASK_C) | (PSWMASK_C * value);
    }
    PC += 2;
    return 1;
}

static uint8_t jmp_indir_a_dptr()
{
    PC = ((CPU.mSFR[REG_DPH] << 8) | (CPU.mSFR[REG_DPL])) + ACC;
    return 1;
}

static uint8_t mov_a_imm()
{
    ACC = OPERAND1;
    PC += 2;
    return 0;
}

static uint8_t mov_mem_imm()
{
    uint8_t address = OPERAND1;
    if (address > 0x7f)
    {
        CPU.mSFR[address - 0x80] = OPERAND2;
        if (CPU.sfrwrite[address - 0x80])
            CPU.sfrwrite[address - 0x80](address);
    }
    else
    {
        CPU.mLowerData[address] = OPERAND2;
    }

    PC += 3;
    return 1;
}

static uint8_t mov_indir_rx_imm()
{
    uint8_t address = INDIR_RX_ADDRESS;
    uint8_t value = OPERAND1;
    if (address > 0x7f)
    {
        if (CPU.mUpperData)
        {
            CPU.mUpperData[address - 0x80] = value;
        }
    }
    else
    {
        CPU.mLowerData[address] = value;
    }

    PC += 2;
    return 0;
}


static uint8_t sjmp_offset()
{
    PC += (signed char)(OPERAND1) + 2;
    return 1;
}

static uint8_t anl_c_bitaddr()
{
    uint8_t address = OPERAND1;
    uint8_t carry = CARRY;
    if (address > 0x7f)
    {
        uint8_t bit = address & 7;
        uint8_t bitmask = (1 << bit);
        uint8_t value;
        address &= 0xf8;        
        if (CPU.sfrread[address - 0x80])
            value = CPU.sfrread[address - 0x80](address);
        else
            value = CPU.mSFR[address - 0x80];

        value = (value & bitmask) ? carry : 0;

        PSW = (PSW & ~PSWMASK_C) | (PSWMASK_C * value);
    }
    else
    {
        uint8_t bit = address & 7;
        uint8_t bitmask = (1 << bit);
        uint8_t value;
        address >>= 3;
        address += 0x20;
        value = (CPU.mLowerData[address] & bitmask) ? carry : 0;
        PSW = (PSW & ~PSWMASK_C) | (PSWMASK_C * value);
    }
    PC += 2;
    return 0;
}

static uint8_t movc_a_indir_a_pc()
{
    uint16_t address = PC + 1 + ACC;
    ACC = CODEMEM(address);
    PC++;
    return 0;
}

static uint8_t div_ab()
{
    uint8_t a = ACC;
    uint8_t b = CPU.mSFR[REG_B];
    uint8_t res;
    PSW &= ~(PSWMASK_C|PSWMASK_OV);
    if (b)
    {
        res = a/b;
        b = a % b;
        a = res;
    }
    else
    {
        PSW |= PSWMASK_OV;
    }
    ACC = a;
    CPU.mSFR[REG_B] = b;
    PC++;
    return 3;
}

static uint8_t mov_mem_mem()
{
    uint8_t address1 = OPERAND2;
    uint8_t value = read_mem(OPERAND1);

    if (address1 > 0x7f)
    {
        CPU.mSFR[address1 - 0x80] = value;
        if (CPU.sfrwrite[address1 - 0x80])
            CPU.sfrwrite[address1 - 0x80](address1);
    }
    else
    {
        CPU.mLowerData[address1] = value;
    }

    PC += 3;
    return 1;
}

static uint8_t mov_mem_indir_rx()
{
    uint8_t address1 = OPERAND1;
    uint8_t address2 = INDIR_RX_ADDRESS;
    if (address1 > 0x7f)
    {
        if (address2 > 0x7f)
        {
            uint8_t value = BAD_VALUE;
            if (CPU.mUpperData)
            {
                value = CPU.mUpperData[address2 - 0x80];
            }
            CPU.mSFR[address1 - 0x80] = value;
            if (CPU.sfrwrite[address1 - 0x80])
                CPU.sfrwrite[address1 - 0x80](address1);
        }
        else
        {
            CPU.mSFR[address1 - 0x80] = CPU.mLowerData[address2];
            if (CPU.sfrwrite[address1 - 0x80])
                CPU.sfrwrite[address1 - 0x80](address1);
        }
    }
    else
    {
        if (address2 > 0x7f)
        {
            uint8_t value = BAD_VALUE;
            if (CPU.mUpperData)
            {
                value = CPU.mUpperData[address2 - 0x80];
            }
            CPU.mLowerData[address1] = value;
        }
        else
        {
            CPU.mLowerData[address1] = CPU.mLowerData[address2];
        }
    }

    PC += 2;
    return 1;
}


static uint8_t mov_dptr_imm()
{
    CPU.mSFR[REG_DPH] = OPERAND1;
    CPU.mSFR[REG_DPL] = OPERAND2;
    PC += 3;
    return 1;
}

static uint8_t mov_bitaddr_c()
{
    uint8_t address = OPERAND1;
    uint8_t carry = CARRY;
    if (address > 0x7f)
    {
        // Data sheet does not explicitly say that the modification source
        // is read from output latch, but we'll assume that is what happens.
        uint8_t bit = address & 7;
        uint8_t bitmask = (1 << bit);
        address &= 0xf8;        
        CPU.mSFR[address - 0x80] = (CPU.mSFR[address - 0x80] & ~bitmask) | (carry << bit);
        if (CPU.sfrwrite[address - 0x80])
            CPU.sfrwrite[address - 0x80](address);
    }
    else
    {
        uint8_t bit = address & 7;
        uint8_t bitmask = (1 << bit);
        address >>= 3;
        address += 0x20;
        CPU.mLowerData[address] = (CPU.mLowerData[address] & ~bitmask) | (carry << bit);
    }
    PC += 2;
    return 1;
}

static uint8_t movc_a_indir_a_dptr()
{
    uint16_t address = (CPU.mSFR[REG_DPH] << 8) | ((CPU.mSFR[REG_DPL] << 0) + ACC);
    ACC = CODEMEM(address);
    PC++;
    return 1;
}

static uint8_t subb_a_imm()
{
    uint8_t carry = CARRY;
    sub_solve_flags(ACC, OPERAND1 + carry);
    ACC -= OPERAND1 + carry;
    PC += 2;
    return 0;
}

static uint8_t subb_a_mem()
{
    uint8_t carry = CARRY;
    uint8_t value = read_mem(OPERAND1) + carry;
    sub_solve_flags(ACC, value);
    ACC -= value;

    PC += 2;
    return 0;
}
static uint8_t subb_a_indir_rx()
{
    uint8_t carry = CARRY;
    uint8_t address = INDIR_RX_ADDRESS;
    if (address > 0x7f)
    {
        uint8_t value = BAD_VALUE;
        if (CPU.mUpperData)
        {
            value = CPU.mUpperData[address - 0x80];
        }

        sub_solve_flags(ACC, value);
        ACC -= value;
    }
    else
    {
        sub_solve_flags(ACC, CPU.mLowerData[address] + carry);
        ACC -= CPU.mLowerData[address] + carry;
    }
    PC++;
    return 0;
}


static uint8_t orl_c_compl_bitaddr()
{
    uint8_t address = OPERAND1;
    uint8_t carry = CARRY;
    if (address > 0x7f)
    {
        uint8_t bit = address & 7;
        uint8_t bitmask = (1 << bit);
        uint8_t value;
        address &= 0xf8;        
        if (CPU.sfrread[address - 0x80])
            value = CPU.sfrread[address - 0x80](address);
        else
            value = CPU.mSFR[address - 0x80];

        value = (value & bitmask) ? carry : 1;

        PSW = (PSW & ~PSWMASK_C) | (PSWMASK_C * value);
    }
    else
    {
        uint8_t bit = address & 7;
        uint8_t bitmask = (1 << bit);
        uint8_t value;
        address >>= 3;
        address += 0x20;
        value = (CPU.mLowerData[address] & bitmask) ? carry : 1;
        PSW = (PSW & ~PSWMASK_C) | (PSWMASK_C * value);
    }
    PC += 2;
    return 0;
}

static uint8_t mov_c_bitaddr()
{
    uint8_t address = OPERAND1;
    if (address > 0x7f)
    {
        uint8_t bit = address & 7;
        uint8_t bitmask = (1 << bit);
        uint8_t value;
        address &= 0xf8;        
        if (CPU.sfrread[address - 0x80])
            value = CPU.sfrread[address - 0x80](address);
        else
            value = CPU.mSFR[address - 0x80];

        value = (value & bitmask) ? 1 : 0;

        PSW = (PSW & ~PSWMASK_C) | (PSWMASK_C * value);
    }
    else
    {
        uint8_t bit = address & 7;
        uint8_t bitmask = (1 << bit);
        uint8_t value;
        address >>= 3;
        address += 0x20;
        value = (CPU.mLowerData[address] & bitmask) ? 1 : 0;
        PSW = (PSW & ~PSWMASK_C) | (PSWMASK_C * value);
    }

    PC += 2;
    return 0;
}

static uint8_t inc_dptr()
{
    CPU.mSFR[REG_DPL]++;
    if (!CPU.mSFR[REG_DPL])
        CPU.mSFR[REG_DPH]++;
    PC++;
    return 1;
}

static uint8_t mul_ab()
{
    uint8_t a = ACC;
    uint8_t b = CPU.mSFR[REG_B];
    uint16_t res = a*b;
    ACC = res & 0xff;
    CPU.mSFR[REG_B] = res >> 8;
    PSW &= ~(PSWMASK_C|PSWMASK_OV);
    if (CPU.mSFR[REG_B])
        PSW |= PSWMASK_OV;
    PC++;
    return 3;
}

static uint8_t mov_indir_rx_mem()
{
    uint8_t address1 = INDIR_RX_ADDRESS;
    uint8_t value = read_mem(OPERAND1);
    if (address1 > 0x7f)
    {
        if (CPU.mUpperData)
        {
            CPU.mUpperData[address1 - 0x80] = value;
        }
    }
    else
    {
        CPU.mLowerData[address1] = value;
    }
    PC += 2;
    return 1;
}


static uint8_t anl_c_compl_bitaddr()
{
    uint8_t address = OPERAND1;
    uint8_t carry = CARRY;
    if (address > 0x7f)
    {
        uint8_t bit = address & 7;
        uint8_t bitmask = (1 << bit);
        uint8_t value;
        address &= 0xf8;        
        if (CPU.sfrread[address - 0x80])
            value = CPU.sfrread[address - 0x80](address);
        else
            value = CPU.mSFR[address - 0x80];

        value = (value & bitmask) ? 0 : carry;

        PSW = (PSW & ~PSWMASK_C) | (PSWMASK_C * value);
    }
    else
    {
        uint8_t bit = address & 7;
        uint8_t bitmask = (1 << bit);
        uint8_t value;
        address >>= 3;
        address += 0x20;
        value = (CPU.mLowerData[address] & bitmask) ? 0 : carry;
        PSW = (PSW & ~PSWMASK_C) | (PSWMASK_C * value);
    }
    PC += 2;
    return 0;
}


static uint8_t cpl_bitaddr()
{
    uint8_t address = OPERAND1;
    if (address > 0x7f)
    {
        // Data sheet does not explicitly say that the modification source
        // is read from output latch, but we'll assume that is what happens.
        uint8_t bit = address & 7;
        uint8_t bitmask = (1 << bit);
        address &= 0xf8;        
        CPU.mSFR[address - 0x80] ^= bitmask;
        if (CPU.sfrwrite[address - 0x80])
            CPU.sfrwrite[address - 0x80](address);
    }
    else
    {
        uint8_t bit = address & 7;
        uint8_t bitmask = (1 << bit);
        address >>= 3;
        address += 0x20;
        CPU.mLowerData[address] ^= bitmask;
    }
    PC += 2;
    return 0;
}

static uint8_t cpl_c()
{
    PSW ^= PSWMASK_C;
    PC++;
    return 0;
}

static uint8_t cjne_a_imm_offset()
{
    uint8_t value = OPERAND1;

    if (ACC < value)
    {
        PSW |= PSWMASK_C;
    }
    else
    {
        PSW &= ~PSWMASK_C;
    }

    if (ACC != value)
    {
        PC += (signed char)OPERAND2 + 3;
    }
    else
    {
        PC += 3;
    }
    return 1;
}

static uint8_t cjne_a_mem_offset()
{
    uint8_t address = OPERAND1;
    uint8_t value;
    if (address > 0x7f)
    {
        if (CPU.sfrread[address - 0x80])
            value = CPU.sfrread[address - 0x80](address);
        else
            value = CPU.mSFR[address - 0x80];
    }
    else
    {
        value = CPU.mLowerData[address];
    }  

    if (ACC < value)
    {
        PSW |= PSWMASK_C;
    }
    else
    {
        PSW &= ~PSWMASK_C;
    }

    if (ACC != value)
    {
        PC += (signed char)OPERAND2 + 3;
    }
    else
    {
        PC += 3;
    }
    return 1;
}
static uint8_t cjne_indir_rx_imm_offset()
{
    uint8_t address = INDIR_RX_ADDRESS;
    uint8_t value1 = BAD_VALUE;
    uint8_t value2 = OPERAND1;
    if (address > 0x7f)
    {
        if (CPU.mUpperData)
        {
            value1 = CPU.mUpperData[address - 0x80];
        }
    }
    else
    {
        value1 = CPU.mLowerData[address];
    }  

    if (value1 < value2)
    {
        PSW |= PSWMASK_C;
    }
    else
    {
        PSW &= ~PSWMASK_C;
    }

    if (value1 != value2)
    {
        PC += (signed char)OPERAND2 + 3;
    }
    else
    {
        PC += 3;
    }
    return 1;
}

static uint8_t push_mem()
{
    uint8_t value = read_mem(OPERAND1);
    push_to_stack(value);
    PC += 2;
    return 1;
}


static uint8_t clr_bitaddr()
{
    uint8_t address = OPERAND1;
    if (address > 0x7f)
    {
        // Data sheet does not explicitly say that the modification source
        // is read from output latch, but we'll assume that is what happens.
        uint8_t bit = address & 7;
        uint8_t bitmask = (1 << bit);
        address &= 0xf8;        
        CPU.mSFR[address - 0x80] &= ~bitmask;
        if (CPU.sfrwrite[address - 0x80])
            CPU.sfrwrite[address - 0x80](address);
    }
    else
    {
        uint8_t bit = address & 7;
        uint8_t bitmask = (1 << bit);
        address >>= 3;
        address += 0x20;
        CPU.mLowerData[address] &= ~bitmask;
    }
    PC += 2;
    return 0;
}

static uint8_t clr_c()
{
    PSW &= ~PSWMASK_C;
    PC++;
    return 0;
}

static uint8_t swap_a()
{
    ACC = (ACC << 4) | (ACC >> 4);
    PC++;
    return 0;
}

static uint8_t xch_a_mem()
{
    uint8_t address = OPERAND1;
    uint8_t value = read_mem(OPERAND1);
    if (address > 0x7f)
    {
        CPU.mSFR[address - 0x80] = ACC;
        ACC = value;
        if (CPU.sfrwrite[address - 0x80])
            CPU.sfrwrite[address - 0x80](address);
    }
    else
    {
        CPU.mLowerData[address] = ACC;
        ACC = value;
    }
    PC += 2;
    return 0;
}

static uint8_t xch_a_indir_rx()
{
    uint8_t address = INDIR_RX_ADDRESS;
    if (address > 0x7f)
    {
        uint8_t value;
        if (CPU.mUpperData)
        {
            value = CPU.mUpperData[address - 0x80];
            CPU.mUpperData[address - 0x80] = ACC;
            ACC = value;
        }
    }
    else
    {
        uint8_t value = CPU.mLowerData[address];
        CPU.mLowerData[address] = ACC;
        ACC = value;
    }
    PC++;
    return 0;
}


static uint8_t pop_mem()
{
    uint8_t address = OPERAND1;
    if (address > 0x7f)
    {
        CPU.mSFR[address - 0x80] = pop_from_stack();
        if (CPU.sfrwrite[address - 0x80])
            CPU.sfrwrite[address - 0x80](address);
    }
    else
    {
        CPU.mLowerData[address] = pop_from_stack();
    }

    PC += 2;
    return 1;
}

static uint8_t setb_bitaddr()
{
    uint8_t address = OPERAND1;
    if (address > 0x7f)
    {
        // Data sheet does not explicitly say that the modification source
        // is read from output latch, but we'll assume that is what happens.
        uint8_t bit = address & 7;
        uint8_t bitmask = (1 << bit);
        address &= 0xf8;        
        CPU.mSFR[address - 0x80] |= bitmask;
        if (CPU.sfrwrite[address - 0x80])
            CPU.sfrwrite[address - 0x80](address);
    }
    else
    {
        uint8_t bit = address & 7;
        uint8_t bitmask = (1 << bit);
        address >>= 3;
        address += 0x20;
        CPU.mLowerData[address] |= bitmask;
    }
    PC += 2;
    return 0;
}

static uint8_t setb_c()
{
    PSW |= PSWMASK_C;
    PC++;
    return 0;
}

static uint8_t da_a()
{
    // data sheets for this operation are a bit unclear..
    // - should AC (or C) ever be cleared?
    // - should this be done in two steps?

    uint16_t result = ACC;
    if ((result & 0xf) > 9 || (PSW & PSWMASK_AC))
        result += 0x6;
    if ((result & 0xff0) > 0x90 || (PSW & PSWMASK_C))
        result += 0x60;
    if (result > 0x99)
        PSW |= PSWMASK_C;
    ACC = result;

 /*
    // this is basically what intel datasheet says the op should do..
    int adder = 0;
    if (ACC & 0xf > 9 || PSW & PSWMASK_AC)
        adder = 6;
    if (ACC & 0xf0 > 0x90 || PSW & PSWMASK_C)
        adder |= 0x60;
    adder += aCPU[REG_ACC];
    if (adder > 0x99)
        PSW |= PSWMASK_C;
    aCPU[REG_ACC] = adder;
*/
    PC++;
    return 0;
}

static uint8_t djnz_mem_offset()
{
    uint8_t address = OPERAND1;
    uint8_t value;
    if (address > 0x7f)
    {
        CPU.mSFR[address - 0x80]--;
        value = CPU.mSFR[address - 0x80];
        if (CPU.sfrwrite[address - 0x80])
            CPU.sfrwrite[address - 0x80](address);
    }
    else
    {
        CPU.mLowerData[address]--;
        value = CPU.mLowerData[address];
    }
    if (value)
    {
        PC += (signed char)OPERAND2 + 3;
    }
    else
    {
        PC += 3;
    }
    return 1;
}

static uint8_t xchd_a_indir_rx()
{
    uint8_t address = INDIR_RX_ADDRESS;
    if (address > 0x7f)
    {
        uint8_t value;
        if (CPU.mUpperData)
        {
            value = CPU.mUpperData[address - 0x80];
            CPU.mUpperData[address - 0x80] = (CPU.mUpperData[address - 0x80] & 0xf0) | (ACC & 0x0f);
            ACC = (ACC & 0xf0) | (value & 0x0f);
        }
    }
    else
    {
        uint8_t value = CPU.mLowerData[address];
        CPU.mLowerData[address] = (CPU.mLowerData[address] & 0x0f) | (ACC & 0x0f);
        ACC = (ACC & 0xf0) | (value & 0x0f);
    }
    PC++;
    return 0;
}


static uint8_t movx_a_indir_dptr()
{
    uint16_t dptr = (CPU.mSFR[REG_DPH] << 8) | CPU.mSFR[REG_DPL];
    if (CPU.xread)
    {
        ACC = CPU.xread(dptr);
    }
    else
    {
        if (CPU.mExtData)
            ACC = EXTDATA(dptr);
    }
    PC++;
    return 1;
}

static uint8_t movx_a_indir_rx()
{
    uint16_t address = INDIR_RX_ADDRESS;
    if (CPU.xread)
    {
        ACC = CPU.xread(address);
    }
    else
    {
        if (CPU.mExtData)
            ACC = EXTDATA(address);
    }

    PC++;
    return 1;
}

static uint8_t clr_a()
{
    ACC = 0;
    PC++;
    return 0;
}

static uint8_t mov_a_mem()
{
    // mov a,acc is not a valid instruction
    uint8_t address = OPERAND1;
    uint8_t value = read_mem(address);
    if (REG_ACC == address - 0x80)
        if (CPU.except)
            CPU.except(EXCEPTION_ACC_TO_A);
    ACC = value;

    PC += 2;
    return 0;
}

static uint8_t mov_a_indir_rx()
{
    uint8_t address = INDIR_RX_ADDRESS;
    if (address > 0x7f)
    {
        uint8_t value = BAD_VALUE;
        if (CPU.mUpperData)
        {
            value = CPU.mUpperData[address - 0x80];
        }

        ACC = value;
    }
    else
    {
        ACC = CPU.mLowerData[address];
    }

    PC++;
    return 0;
}


static uint8_t movx_indir_dptr_a()
{
    uint16_t dptr = (CPU.mSFR[REG_DPH] << 8) | CPU.mSFR[REG_DPL];
    if (CPU.xwrite)
    {
        CPU.xwrite(dptr, ACC);
    }
    else
    {
        if (CPU.mExtData)
            EXTDATA(dptr) = ACC;
    }

    PC++;
    return 1;
}

static uint8_t movx_indir_rx_a()
{
    uint16_t address = INDIR_RX_ADDRESS;

    if (CPU.xwrite)
    {
        CPU.xwrite(address, ACC);
    }
    else
    {
        if (CPU.mExtData)
            EXTDATA(address) = ACC;
    }

    PC++;
    return 1;
}

static uint8_t cpl_a()
{
    ACC = ~ACC;
    PC++;
    return 0;
}

static uint8_t mov_mem_a()
{
    uint8_t address = OPERAND1;
    if (address > 0x7f)
    {
        CPU.mSFR[address - 0x80] = ACC;
        if (CPU.sfrwrite[address - 0x80])
            CPU.sfrwrite[address - 0x80](address);
    }
    else
    {
        CPU.mLowerData[address] = ACC;
    }
    PC += 2;
    return 0;
}

static uint8_t mov_indir_rx_a()
{
    uint8_t address = INDIR_RX_ADDRESS;
    if (address > 0x7f)
    {
        if (CPU.mUpperData)
            CPU.mUpperData[address - 0x80] = ACC;
    }
    else
    {
        CPU.mLowerData[address] = ACC;
    }

    PC++;
    return 0;
}

static uint8_t nop()
{
    if (CODEMEM(PC) != 0)
        if (CPU.except)
            CPU.except(EXCEPTION_ILLEGAL_OPCODE);
    PC++;
    return 0;
}

static uint8_t inc_rx()
{
    uint8_t rx = RX_ADDRESS;
    CPU.mLowerData[rx]++;
    PC++;
    return 0;
}

static uint8_t dec_rx()
{
    uint8_t rx = RX_ADDRESS;
    CPU.mLowerData[rx]--;
    PC++;
    return 0;
}

static uint8_t add_a_rx()
{
    uint8_t rx = RX_ADDRESS;
    add_solve_flags(CPU.mLowerData[rx], ACC, 0);
    ACC += CPU.mLowerData[rx];
    PC++;
    return 0;
}

static uint8_t addc_a_rx()
{
    uint8_t rx = RX_ADDRESS;
    uint8_t carry = CARRY;
    add_solve_flags(CPU.mLowerData[rx], ACC, carry);
    ACC += CPU.mLowerData[rx] + carry;
    PC++;
    return 0;
}

static uint8_t orl_a_rx()
{
    uint8_t rx = RX_ADDRESS;
    ACC |= CPU.mLowerData[rx];
    PC++;
    return 0;
}

static uint8_t anl_a_rx()
{
    uint8_t rx = RX_ADDRESS;
    ACC &= CPU.mLowerData[rx];
    PC++;
    return 0;
}

static uint8_t xrl_a_rx()
{
    uint8_t rx = RX_ADDRESS;
    ACC ^= CPU.mLowerData[rx];
    PC++;
    return 0;
}


static uint8_t mov_rx_imm()
{
    uint8_t rx = RX_ADDRESS;
    CPU.mLowerData[rx] = OPERAND1;
    PC += 2;
    return 0;
}

static uint8_t mov_mem_rx()
{
    uint8_t rx = RX_ADDRESS;
    uint8_t address = OPERAND1;
    if (address > 0x7f)
    {
        CPU.mSFR[address - 0x80] = CPU.mLowerData[rx];
        if (CPU.sfrwrite[address - 0x80])
            CPU.sfrwrite[address - 0x80](address);
    }
    else
    {
        CPU.mLowerData[address] = CPU.mLowerData[rx];
    }
    PC += 2;
    return 1;
}

static uint8_t subb_a_rx()
{
    uint8_t rx = RX_ADDRESS;
    uint8_t carry = CARRY;
    sub_solve_flags(ACC, CPU.mLowerData[rx] + carry);
    ACC -= CPU.mLowerData[rx] + carry;
    PC++;
    return 0;
}

static uint8_t mov_rx_mem()
{
    uint8_t rx = RX_ADDRESS;
    uint8_t value = read_mem(OPERAND1);
    CPU.mLowerData[rx] = value;

    PC += 2;
    return 1;
}

static uint8_t cjne_rx_imm_offset()
{
    uint8_t rx = RX_ADDRESS;
    uint8_t value = OPERAND1;
    
    if (CPU.mLowerData[rx] < value)
    {
        PSW |= PSWMASK_C;
    }
    else
    {
        PSW &= ~PSWMASK_C;
    }

    if (CPU.mLowerData[rx] != value)
    {
        PC += (signed char)OPERAND2 + 3;
    }
    else
    {
        PC += 3;
    }
    return 1;
}

static uint8_t xch_a_rx()
{
    uint8_t rx = RX_ADDRESS;
    uint8_t a = ACC;
    ACC = CPU.mLowerData[rx];
    CPU.mLowerData[rx] = a;
    PC++;
    return 0;
}

static uint8_t djnz_rx_offset()
{
    uint8_t rx = RX_ADDRESS;
    CPU.mLowerData[rx]--;
    if (CPU.mLowerData[rx])
    {
        PC += (signed char)OPERAND1 + 2;
    }
    else
    {
        PC += 2;
    }
    return 1;
}

static uint8_t mov_a_rx()
{
    uint8_t rx = RX_ADDRESS;
    ACC = CPU.mLowerData[rx];

    PC++;
    return 0;
}

static uint8_t mov_rx_a()
{
    uint8_t rx = RX_ADDRESS;
    CPU.mLowerData[rx] = ACC;
    PC++;
    return 0;
}

void op_setptrs()
{
    uint8_t i;
    for (i = 0; i < 8; i++)
    {
        CPU.op[0x08 + i] = &inc_rx;
        CPU.op[0x18 + i] = &dec_rx;
        CPU.op[0x28 + i] = &add_a_rx;
        CPU.op[0x38 + i] = &addc_a_rx;
        CPU.op[0x48 + i] = &orl_a_rx;
        CPU.op[0x58 + i] = &anl_a_rx;
        CPU.op[0x68 + i] = &xrl_a_rx;
        CPU.op[0x78 + i] = &mov_rx_imm;
        CPU.op[0x88 + i] = &mov_mem_rx;
        CPU.op[0x98 + i] = &subb_a_rx;
        CPU.op[0xa8 + i] = &mov_rx_mem;
        CPU.op[0xb8 + i] = &cjne_rx_imm_offset;
        CPU.op[0xc8 + i] = &xch_a_rx;
        CPU.op[0xd8 + i] = &djnz_rx_offset;
        CPU.op[0xe8 + i] = &mov_a_rx;
        CPU.op[0xf8 + i] = &mov_rx_a;
    }
    CPU.op[0x00] = &nop;
    CPU.op[0x01] = &ajmp_offset;
    CPU.op[0x02] = &ljmp_address;
    CPU.op[0x03] = &rr_a;
    CPU.op[0x04] = &inc_a;
    CPU.op[0x05] = &inc_mem;
    CPU.op[0x06] = &inc_indir_rx;
    CPU.op[0x07] = &inc_indir_rx;

    CPU.op[0x10] = &jbc_bitaddr_offset;
    CPU.op[0x11] = &acall_offset;
    CPU.op[0x12] = &lcall_address;
    CPU.op[0x13] = &rrc_a;
    CPU.op[0x14] = &dec_a;
    CPU.op[0x15] = &dec_mem;
    CPU.op[0x16] = &dec_indir_rx;
    CPU.op[0x17] = &dec_indir_rx;

    CPU.op[0x20] = &jb_bitaddr_offset;
    CPU.op[0x21] = &ajmp_offset;
    CPU.op[0x22] = &ret;
    CPU.op[0x23] = &rl_a;
    CPU.op[0x24] = &add_a_imm;
    CPU.op[0x25] = &add_a_mem;
    CPU.op[0x26] = &add_a_indir_rx;
    CPU.op[0x27] = &add_a_indir_rx;

    CPU.op[0x30] = &jnb_bitaddr_offset;
    CPU.op[0x31] = &acall_offset;
    CPU.op[0x32] = &reti;
    CPU.op[0x33] = &rlc_a;
    CPU.op[0x34] = &addc_a_imm;
    CPU.op[0x35] = &addc_a_mem;
    CPU.op[0x36] = &addc_a_indir_rx;
    CPU.op[0x37] = &addc_a_indir_rx;

    CPU.op[0x40] = &jc_offset;
    CPU.op[0x41] = &ajmp_offset;
    CPU.op[0x42] = &orl_mem_a;
    CPU.op[0x43] = &orl_mem_imm;
    CPU.op[0x44] = &orl_a_imm;
    CPU.op[0x45] = &orl_a_mem;
    CPU.op[0x46] = &orl_a_indir_rx;
    CPU.op[0x47] = &orl_a_indir_rx;

    CPU.op[0x50] = &jnc_offset;
    CPU.op[0x51] = &acall_offset;
    CPU.op[0x52] = &anl_mem_a;
    CPU.op[0x53] = &anl_mem_imm;
    CPU.op[0x54] = &anl_a_imm;
    CPU.op[0x55] = &anl_a_mem;
    CPU.op[0x56] = &anl_a_indir_rx;
    CPU.op[0x57] = &anl_a_indir_rx;

    CPU.op[0x60] = &jz_offset;
    CPU.op[0x61] = &ajmp_offset;
    CPU.op[0x62] = &xrl_mem_a;
    CPU.op[0x63] = &xrl_mem_imm;
    CPU.op[0x64] = &xrl_a_imm;
    CPU.op[0x65] = &xrl_a_mem;
    CPU.op[0x66] = &xrl_a_indir_rx;
    CPU.op[0x67] = &xrl_a_indir_rx;

    CPU.op[0x70] = &jnz_offset;
    CPU.op[0x71] = &acall_offset;
    CPU.op[0x72] = &orl_c_bitaddr;
    CPU.op[0x73] = &jmp_indir_a_dptr;
    CPU.op[0x74] = &mov_a_imm;
    CPU.op[0x75] = &mov_mem_imm;
    CPU.op[0x76] = &mov_indir_rx_imm;
    CPU.op[0x77] = &mov_indir_rx_imm;

    CPU.op[0x80] = &sjmp_offset;
    CPU.op[0x81] = &ajmp_offset;
    CPU.op[0x82] = &anl_c_bitaddr;
    CPU.op[0x83] = &movc_a_indir_a_pc;
    CPU.op[0x84] = &div_ab;
    CPU.op[0x85] = &mov_mem_mem;
    CPU.op[0x86] = &mov_mem_indir_rx;
    CPU.op[0x87] = &mov_mem_indir_rx;

    CPU.op[0x90] = &mov_dptr_imm;
    CPU.op[0x91] = &acall_offset;
    CPU.op[0x92] = &mov_bitaddr_c;
    CPU.op[0x93] = &movc_a_indir_a_dptr;
    CPU.op[0x94] = &subb_a_imm;
    CPU.op[0x95] = &subb_a_mem;
    CPU.op[0x96] = &subb_a_indir_rx;
    CPU.op[0x97] = &subb_a_indir_rx;

    CPU.op[0xa0] = &orl_c_compl_bitaddr;
    CPU.op[0xa1] = &ajmp_offset;
    CPU.op[0xa2] = &mov_c_bitaddr;
    CPU.op[0xa3] = &inc_dptr;
    CPU.op[0xa4] = &mul_ab;
    CPU.op[0xa5] = &nop; // unused
    CPU.op[0xa6] = &mov_indir_rx_mem;
    CPU.op[0xa7] = &mov_indir_rx_mem;

    CPU.op[0xb0] = &anl_c_compl_bitaddr;
    CPU.op[0xb1] = &acall_offset;
    CPU.op[0xb2] = &cpl_bitaddr;
    CPU.op[0xb3] = &cpl_c;
    CPU.op[0xb4] = &cjne_a_imm_offset;
    CPU.op[0xb5] = &cjne_a_mem_offset;
    CPU.op[0xb6] = &cjne_indir_rx_imm_offset;
    CPU.op[0xb7] = &cjne_indir_rx_imm_offset;

    CPU.op[0xc0] = &push_mem;
    CPU.op[0xc1] = &ajmp_offset;
    CPU.op[0xc2] = &clr_bitaddr;
    CPU.op[0xc3] = &clr_c;
    CPU.op[0xc4] = &swap_a;
    CPU.op[0xc5] = &xch_a_mem;
    CPU.op[0xc6] = &xch_a_indir_rx;
    CPU.op[0xc7] = &xch_a_indir_rx;

    CPU.op[0xd0] = &pop_mem;
    CPU.op[0xd1] = &acall_offset;
    CPU.op[0xd2] = &setb_bitaddr;
    CPU.op[0xd3] = &setb_c;
    CPU.op[0xd4] = &da_a;
    CPU.op[0xd5] = &djnz_mem_offset;
    CPU.op[0xd6] = &xchd_a_indir_rx;
    CPU.op[0xd7] = &xchd_a_indir_rx;

    CPU.op[0xe0] = &movx_a_indir_dptr;
    CPU.op[0xe1] = &ajmp_offset;
    CPU.op[0xe2] = &movx_a_indir_rx;
    CPU.op[0xe3] = &movx_a_indir_rx;
    CPU.op[0xe4] = &clr_a;
    CPU.op[0xe5] = &mov_a_mem;
    CPU.op[0xe6] = &mov_a_indir_rx;
    CPU.op[0xe7] = &mov_a_indir_rx;

    CPU.op[0xf0] = &movx_indir_dptr_a;
    CPU.op[0xf1] = &acall_offset;
    CPU.op[0xf2] = &movx_indir_rx_a;
    CPU.op[0xf3] = &movx_indir_rx_a;
    CPU.op[0xf4] = &cpl_a;
    CPU.op[0xf5] = &mov_mem_a;
    CPU.op[0xf6] = &mov_indir_rx_a;
    CPU.op[0xf7] = &mov_indir_rx_a;
}

uint8_t do_op()
{
    switch (OPCODE)
    {
    case 0x00: return nop();
    case 0x01: return ajmp_offset();
    case 0x02: return ljmp_address();
    case 0x03: return rr_a();
    case 0x04: return inc_a();
    case 0x05: return inc_mem();
    case 0x06: return inc_indir_rx();
    case 0x07: return inc_indir_rx();

    case 0x08:
    case 0x09:
    case 0x0a:
    case 0x0b:
    case 0x0c:
    case 0x0d:
    case 0x0e:
    case 0x0f: return inc_rx();

    case 0x10: return jbc_bitaddr_offset();
    case 0x11: return acall_offset();
    case 0x12: return lcall_address();
    case 0x13: return rrc_a();
    case 0x14: return dec_a();
    case 0x15: return dec_mem();
    case 0x16: return dec_indir_rx();
    case 0x17: return dec_indir_rx();

    case 0x18:
    case 0x19:
    case 0x1a:
    case 0x1b:
    case 0x1c:
    case 0x1d:
    case 0x1e:
    case 0x1f: return dec_rx();

    case 0x20: return jb_bitaddr_offset();
    case 0x21: return ajmp_offset();
    case 0x22: return ret();
    case 0x23: return rl_a();
    case 0x24: return add_a_imm();
    case 0x25: return add_a_mem();
    case 0x26: return add_a_indir_rx();
    case 0x27: return add_a_indir_rx();

    case 0x28:
    case 0x29:
    case 0x2a:
    case 0x2b:
    case 0x2c:
    case 0x2d:
    case 0x2e:
    case 0x2f: return add_a_rx();

    case 0x30: return jnb_bitaddr_offset();
    case 0x31: return acall_offset();
    case 0x32: return reti();
    case 0x33: return rlc_a();
    case 0x34: return addc_a_imm();
    case 0x35: return addc_a_mem();
    case 0x36: return addc_a_indir_rx();
    case 0x37: return addc_a_indir_rx();

    case 0x38:
    case 0x39:
    case 0x3a:
    case 0x3b:
    case 0x3c:
    case 0x3d:
    case 0x3e:
    case 0x3f: return addc_a_rx();

    case 0x40: return jc_offset();
    case 0x41: return ajmp_offset();
    case 0x42: return orl_mem_a();
    case 0x43: return orl_mem_imm();
    case 0x44: return orl_a_imm();
    case 0x45: return orl_a_mem();
    case 0x46: return orl_a_indir_rx();
    case 0x47: return orl_a_indir_rx();

    case 0x48:
    case 0x49:
    case 0x4a:
    case 0x4b:
    case 0x4c:
    case 0x4d:
    case 0x4e:
    case 0x4f: return orl_a_rx();

    case 0x50: return jnc_offset();
    case 0x51: return acall_offset();
    case 0x52: return anl_mem_a();
    case 0x53: return anl_mem_imm();
    case 0x54: return anl_a_imm();
    case 0x55: return anl_a_mem();
    case 0x56: return anl_a_indir_rx();
    case 0x57: return anl_a_indir_rx();

    case 0x58:
    case 0x59:
    case 0x5a:
    case 0x5b:
    case 0x5c:
    case 0x5d:
    case 0x5e:
    case 0x5f: return anl_a_rx();

    case 0x60: return jz_offset();
    case 0x61: return ajmp_offset();
    case 0x62: return xrl_mem_a();
    case 0x63: return xrl_mem_imm();
    case 0x64: return xrl_a_imm();
    case 0x65: return xrl_a_mem();
    case 0x66: return xrl_a_indir_rx();
    case 0x67: return xrl_a_indir_rx();

    case 0x68:
    case 0x69:
    case 0x6a:
    case 0x6b:
    case 0x6c:
    case 0x6d:
    case 0x6e:
    case 0x6f: return xrl_a_rx();

    case 0x70: return jnz_offset();
    case 0x71: return acall_offset();
    case 0x72: return orl_c_bitaddr();
    case 0x73: return jmp_indir_a_dptr();
    case 0x74: return mov_a_imm();
    case 0x75: return mov_mem_imm();
    case 0x76: return mov_indir_rx_imm();
    case 0x77: return mov_indir_rx_imm();

    case 0x78:
    case 0x79:
    case 0x7a:
    case 0x7b:
    case 0x7c:
    case 0x7d:
    case 0x7e:
    case 0x7f: return mov_rx_imm();

    case 0x80: return sjmp_offset();
    case 0x81: return ajmp_offset();
    case 0x82: return anl_c_bitaddr();
    case 0x83: return movc_a_indir_a_pc();
    case 0x84: return div_ab();
    case 0x85: return mov_mem_mem();
    case 0x86: return mov_mem_indir_rx();
    case 0x87: return mov_mem_indir_rx();

    case 0x88:
    case 0x89:
    case 0x8a:
    case 0x8b:
    case 0x8c:
    case 0x8d:
    case 0x8e:
    case 0x8f: return mov_mem_rx();

    case 0x90: return mov_dptr_imm();
    case 0x91: return acall_offset();
    case 0x92: return mov_bitaddr_c();
    case 0x93: return movc_a_indir_a_dptr();
    case 0x94: return subb_a_imm();
    case 0x95: return subb_a_mem();
    case 0x96: return subb_a_indir_rx();
    case 0x97: return subb_a_indir_rx();

    case 0x98:
    case 0x99:
    case 0x9a:
    case 0x9b:
    case 0x9c:
    case 0x9d:
    case 0x9e:
    case 0x9f: return subb_a_rx();

    case 0xa0: return orl_c_compl_bitaddr();
    case 0xa1: return ajmp_offset();
    case 0xa2: return mov_c_bitaddr();
    case 0xa3: return inc_dptr();
    case 0xa4: return mul_ab();
    case 0xa5: return nop(); // unused
    case 0xa6: return mov_indir_rx_mem();
    case 0xa7: return mov_indir_rx_mem();

    case 0xa8:
    case 0xa9:
    case 0xaa:
    case 0xab:
    case 0xac:
    case 0xad:
    case 0xae:
    case 0xaf: return mov_rx_mem();

    case 0xb0: return anl_c_compl_bitaddr();
    case 0xb1: return acall_offset();
    case 0xb2: return cpl_bitaddr();
    case 0xb3: return cpl_c();
    case 0xb4: return cjne_a_imm_offset();
    case 0xb5: return cjne_a_mem_offset();
    case 0xb6: return cjne_indir_rx_imm_offset();
    case 0xb7: return cjne_indir_rx_imm_offset();

    case 0xb8:
    case 0xb9:
    case 0xba:
    case 0xbb:
    case 0xbc:
    case 0xbd:
    case 0xbe:
    case 0xbf: return cjne_rx_imm_offset();

    case 0xc0: return push_mem();
    case 0xc1: return ajmp_offset();
    case 0xc2: return clr_bitaddr();
    case 0xc3: return clr_c();
    case 0xc4: return swap_a();
    case 0xc5: return xch_a_mem();
    case 0xc6: return xch_a_indir_rx();
    case 0xc7: return xch_a_indir_rx();

    case 0xc8:
    case 0xc9:
    case 0xca:
    case 0xcb:
    case 0xcc:
    case 0xcd:
    case 0xce:
    case 0xcf: return xch_a_rx();

    case 0xd0: return pop_mem();
    case 0xd1: return acall_offset();
    case 0xd2: return setb_bitaddr();
    case 0xd3: return setb_c();
    case 0xd4: return da_a();
    case 0xd5: return djnz_mem_offset();
    case 0xd6: return xchd_a_indir_rx();
    case 0xd7: return xchd_a_indir_rx();

    case 0xd8:
    case 0xd9:
    case 0xda:
    case 0xdb:
    case 0xdc:
    case 0xdd:
    case 0xde:
    case 0xdf: return djnz_rx_offset();

    case 0xe0: return movx_a_indir_dptr();
    case 0xe1: return ajmp_offset();
    case 0xe2: return movx_a_indir_rx();
    case 0xe3: return movx_a_indir_rx();
    case 0xe4: return clr_a();
    case 0xe5: return mov_a_mem();
    case 0xe6: return mov_a_indir_rx();
    case 0xe7: return mov_a_indir_rx();

    case 0xe8:
    case 0xe9:
    case 0xea:
    case 0xeb:
    case 0xec:
    case 0xed:
    case 0xee:
    case 0xef: return mov_a_rx();

    case 0xf0: return movx_indir_dptr_a();
    case 0xf1: return acall_offset();
    case 0xf2: return movx_indir_rx_a();
    case 0xf3: return movx_indir_rx_a();
    case 0xf4: return cpl_a();
    case 0xf5: return mov_mem_a();
    case 0xf6: return mov_indir_rx_a();
    case 0xf7: return mov_indir_rx_a();

    case 0xf8:
    case 0xf9:
    case 0xfa:
    case 0xfb:
    case 0xfc:
    case 0xfd:
    case 0xfe:
    case 0xff: return mov_rx_a();
   }
    return 0;
}

