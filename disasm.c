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
 * disasm.c
 * Disassembler functions
 *
 * These functions decode 8051 operations into text strings, useful in
 * interactive debugger.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "emu8051.h"

#define CODEMEM(x) CPU.mCodeMem[(x)&(CPU.mCodeMemMaxIdx)]
#define OPCODE CODEMEM(aPosition + 0)
#define OPERAND1 CODEMEM(aPosition + 1)
#define OPERAND2 CODEMEM(aPosition + 2)


//#define static

void mem_memonic(int aValue, char *aBuffer)
{
    int done = 0;

    if (aValue > 0x7f)
    {
        switch (aValue - 0x80)
        {
        case REG_ACC:
            strcpy(aBuffer, "ACC"); 
            done = 1;
            break;
        case REG_B:
            strcpy(aBuffer, "B");
            done = 1;
            break;
        case REG_PSW:
            strcpy(aBuffer, "PSW");
            done = 1;
            break;
        case REG_SP:
            strcpy(aBuffer, "SP");
            done = 1;
            break;
        case REG_DPL:
            strcpy(aBuffer, "DPL");
            done = 1;
            break;
        case REG_DPH:
            strcpy(aBuffer, "DPH");
            done = 1;
            break;
        case REG_P0:
            strcpy(aBuffer, "P0");
            done = 1;
            break;
        case REG_P1:
            strcpy(aBuffer, "P1");
            done = 1;
            break;
        case REG_P2:
            strcpy(aBuffer, "P2");
            done = 1;
            break;
        case REG_P3:
            strcpy(aBuffer, "P3");
            done = 1;
            break;
        case REG_IP:
            strcpy(aBuffer, "IP");
            done = 1;
            break;
        case REG_IE:
            strcpy(aBuffer, "IE");
            done = 1;
            break;
        case REG_TMOD:
            strcpy(aBuffer, "TMOD");
            done = 1;
            break;
        case REG_TCON:
            strcpy(aBuffer, "TCON");
            done = 1;
            break;
        case REG_TH0:
            strcpy(aBuffer, "TH0");
            done = 1;
            break;
        case REG_TL0:
            strcpy(aBuffer, "TL0");
            done = 1;
            break;
        case REG_TH1:
            strcpy(aBuffer, "TH1");
            done = 1;
            break;
        case REG_TL1:
            strcpy(aBuffer, "TL1");
            done = 1;
            break;
        case REG_SCON:
            strcpy(aBuffer, "SCON");
            done = 1;
            break;
        case REG_PCON:
            strcpy(aBuffer, "PCON");
            done = 1;
            break;
        case REG_SBUF:
            strcpy(aBuffer, "SBUF");
            done = 1;
            break;
        }
    }
    if (!done)
    {
        sprintf(aBuffer, "%02Xh", aValue);
    }
}

void bitaddr_memonic(int aValue, char *aBuffer)
{
    char regname[16];

    if (aValue > 0x7f)
    {
        switch ((aValue & 0xf8) - 0x80)
        {
        case REG_ACC:
            strcpy(regname, "ACC"); 
            break;
        case REG_B:
            strcpy(regname, "B");
            break;
        case REG_PSW:
            strcpy(regname, "PSW");
            break;
        case REG_SP:
            strcpy(regname, "SP");
            break;
        case REG_DPL:
            strcpy(regname, "DPL");
            break;
        case REG_DPH:
            strcpy(regname, "DPH");
            break;
        case REG_P0:
            strcpy(regname, "P0");
            break;
        case REG_P1:
            strcpy(regname, "P1");
            break;
        case REG_P2:
            strcpy(regname, "P2");
            break;
        case REG_P3:
            strcpy(regname, "P3");
            break;
        case REG_IP:
            strcpy(regname, "IP");
            break;
        case REG_IE:
            strcpy(regname, "IE");
            break;
        case REG_TMOD:
            strcpy(regname, "TMOD");
            break;
        case REG_TCON:
            strcpy(regname, "TCON");
            break;
        case REG_TH0:
            strcpy(regname, "TH0");
            break;
        case REG_TL0:
            strcpy(regname, "TL0");
            break;
        case REG_TH1:
            strcpy(regname, "TH1");
            break;
        case REG_TL1:
            strcpy(regname, "TL1");
            break;
        case REG_SCON:
            strcpy(regname, "SCON");
            break;
        case REG_PCON:
            strcpy(regname, "PCON");
            break;
        default:
            sprintf(regname, "%02Xh",(aValue & 0xF8));
            break;
        }
    }
    else
    {
        sprintf(regname, "%02Xh", aValue >> 3);
    }

    sprintf(aBuffer, "%s.%d", regname, aValue & 7);
}


static uint8_t disasm_ajmp_offset(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"AJMP  #%04Xh",
        ((aPosition + 2) & 0xf800) |
        OPERAND1 |
        ((OPCODE & 0xe0) << 3));
    return 2;
}

static uint8_t disasm_ljmp_address(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"LJMP  #%04Xh",        
        (OPERAND1 << 8) |
        OPERAND2);
    return 3;
}

static uint8_t disasm_rr_a(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"RR    A");
    return 1;
}

static uint8_t disasm_inc_a(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"INC   A");
    return 1;
}

static uint8_t disasm_inc_mem(uint16_t aPosition, char *aBuffer)
{
    char mem[16];mem_memonic(OPERAND1, mem);
    sprintf(aBuffer,"INC   %s",        
        mem);
    
    return 2;
}

static uint8_t disasm_inc_indir_rx(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"INC   @R%d",        
        OPCODE & 1);
    return 1;
}

static uint8_t disasm_jbc_bitaddr_offset(uint16_t aPosition, char *aBuffer)
{
    char baddr[16];
    bitaddr_memonic(OPERAND1, baddr);
    sprintf(aBuffer,"JBC   %s, #%+d",        
        baddr,
        (signed char)OPERAND2);
    return 3;
}

static uint8_t disasm_acall_offset(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"ACALL %04Xh",
        ((aPosition + 2) & 0xf800) |
        OPERAND1 |
        ((OPCODE & 0xe0) << 3));
    return 2;
}

static uint8_t disasm_lcall_address(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"LCALL #%04Xh",        
        (OPERAND1 << 8) |
        OPERAND2);
    return 3;
}

static uint8_t disasm_rrc_a(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"RRC   A");
    return 1;
}

static uint8_t disasm_dec_a(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"DEC   A");
    return 1;
}

static uint8_t disasm_dec_mem(uint16_t aPosition, char *aBuffer)
{
    char mem[16];mem_memonic(OPERAND1, mem);
    sprintf(aBuffer,"DEC   %s",        
        mem);
    
    return 2;
}

static uint8_t disasm_dec_indir_rx(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"DEC   @R%d",        
        OPCODE & 1);
    return 1;
}


static uint8_t disasm_jb_bitaddr_offset(uint16_t aPosition, char *aBuffer)
{
    char baddr[16];
    bitaddr_memonic(OPERAND1, baddr);
    sprintf(aBuffer,"JB   %s, #%+d",        
        baddr,
        (signed char)OPERAND2);
    return 3;
}

static uint8_t disasm_ret(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"RET");
    return 1;
}

static uint8_t disasm_rl_a(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"RL    A");
    return 1;
}

static uint8_t disasm_add_a_imm(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"ADD   A, #%02Xh",        
        OPERAND1);
    return 2;
}

static uint8_t disasm_add_a_mem(uint16_t aPosition, char *aBuffer)
{
    char mem[16];mem_memonic(OPERAND1, mem);
    sprintf(aBuffer,"ADD   A, %s",        
        mem);
    
    return 2;
}

static uint8_t disasm_add_a_indir_rx(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"ADD   A, @R%d",        
        OPCODE&1);
    return 1;
}

static uint8_t disasm_jnb_bitaddr_offset(uint16_t aPosition, char *aBuffer)
{
    char baddr[16];
    bitaddr_memonic(OPERAND1, baddr);
    sprintf(aBuffer,"JNB   %s, #%+d",        
        baddr,
        (signed char)OPERAND2);
    return 3;
}

static uint8_t disasm_reti(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"RETI");
    return 1;
}

static uint8_t disasm_rlc_a(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"RLC   A");
    return 1;
}

static uint8_t disasm_addc_a_imm(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"ADDC  A, #%02Xh",        
        OPERAND1);
    return 2;
}

static uint8_t disasm_addc_a_mem(uint16_t aPosition, char *aBuffer)
{
    char mem[16];mem_memonic(OPERAND1, mem);
    sprintf(aBuffer,"ADDC  A, %s",        
        mem);
    
    return 2;
}

static uint8_t disasm_addc_a_indir_rx(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"ADDC  A, @R%d",        
        OPCODE&1);
    return 1;
}


static uint8_t disasm_jc_offset(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"JC    #%+d",        
        (signed char)OPERAND1);
    return 2;
}

static uint8_t disasm_orl_mem_a(uint16_t aPosition, char *aBuffer)
{
    char mem[16];mem_memonic(OPERAND1, mem);
    sprintf(aBuffer,"ORL   %s, A",        
        mem);
    
    return 2;
}

static uint8_t disasm_orl_mem_imm(uint16_t aPosition, char *aBuffer)
{
    char mem[16];mem_memonic(OPERAND1, mem);
    sprintf(aBuffer,"ORL   %s, #%02Xh",        
        mem,
        OPERAND2);
    
    return 3;
}

static uint8_t disasm_orl_a_imm(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"ORL   A, #%02Xh",        
        OPERAND1);
    return 2;
}

static uint8_t disasm_orl_a_mem(uint16_t aPosition, char *aBuffer)
{
    char mem[16];mem_memonic(OPERAND1, mem);
    sprintf(aBuffer,"ORL   A, %s",        
        mem);
    
    return 2;
}

static uint8_t disasm_orl_a_indir_rx(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"ORL   A, @R%d",        
        OPCODE&1);
    return 1;
}


static uint8_t disasm_jnc_offset(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"JNC   #%+d",        
        (signed char)OPERAND1);
    return 2;
}

static uint8_t disasm_anl_mem_a(uint16_t aPosition, char *aBuffer)
{
    char mem[16];mem_memonic(OPERAND1, mem);
    sprintf(aBuffer,"ANL   %s, A",
        mem);
    
    return 2;
}

static uint8_t disasm_anl_mem_imm(uint16_t aPosition, char *aBuffer)
{
    char mem[16];mem_memonic(OPERAND1, mem);
    sprintf(aBuffer,"ANL   %s, #%02Xh",        
        mem,
        OPERAND2);
    
    return 3;
}

static uint8_t disasm_anl_a_imm(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"ANL   A, #%02Xh",
        OPERAND1);
    return 2;
}

static uint8_t disasm_anl_a_mem(uint16_t aPosition, char *aBuffer)
{
    char mem[16];mem_memonic(OPERAND1, mem);
    sprintf(aBuffer,"ANL   A, %s",
        mem);
    
    return 2;
}

static uint8_t disasm_anl_a_indir_rx(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"ANL   A, @R%d",        
        OPCODE&1);
    return 1;
}


static uint8_t disasm_jz_offset(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"JZ    #%+d",        
        (signed char)OPERAND1);
    return 2;
}

static uint8_t disasm_xrl_mem_a(uint16_t aPosition, char *aBuffer)
{
    char mem[16];mem_memonic(OPERAND1, mem);
    sprintf(aBuffer,"XRL   %s, A",
        mem);
    
    return 2;
}

static uint8_t disasm_xrl_mem_imm(uint16_t aPosition, char *aBuffer)
{
    char mem[16];mem_memonic(OPERAND1, mem);
    sprintf(aBuffer,"XRL   %s, #%02Xh",        
        mem,
        OPERAND2);
    
    return 3;
}

static uint8_t disasm_xrl_a_imm(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"XRL   A, #%02Xh",
        OPERAND1);
    return 2;
}

static uint8_t disasm_xrl_a_mem(uint16_t aPosition, char *aBuffer)
{
    char mem[16];mem_memonic(OPERAND1, mem);
    sprintf(aBuffer,"XRL   A, %s",
        mem);
    
    return 2;
}

static uint8_t disasm_xrl_a_indir_rx(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"XRL   A, @R%d",        
        OPCODE&1);
    return 1;
}


static uint8_t disasm_jnz_offset(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"JNZ   #%+d",        
        (signed char)OPERAND1);
    return 2;
}

static uint8_t disasm_orl_c_bitaddr(uint16_t aPosition, char *aBuffer)
{
    char baddr[16];
    bitaddr_memonic(OPERAND1, baddr);
    sprintf(aBuffer,"ORL   C, %s",
        baddr);
    return 2;
}

static uint8_t disasm_jmp_indir_a_dptr(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"JMP   @A+DPTR");
    return 1;
}

static uint8_t disasm_mov_a_imm(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"MOV   A, #%02Xh",
        OPERAND1);
    return 2;
}

static uint8_t disasm_mov_mem_imm(uint16_t aPosition, char *aBuffer)
{
    char mem[16];mem_memonic(OPERAND1, mem);
    sprintf(aBuffer,"MOV   %s, #%02Xh",        
        mem,
        OPERAND2);
    
    return 3;
}

static uint8_t disasm_mov_indir_rx_imm(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"MOV   @R%d, #%02Xh",        
        OPCODE&1,
        OPERAND1);
    return 2;
}


static uint8_t disasm_sjmp_offset(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"SJMP  #%+d",        
        (signed char)(OPERAND1));
    return 2;
}

static uint8_t disasm_anl_c_bitaddr(uint16_t aPosition, char *aBuffer)
{
    char baddr[16];
    bitaddr_memonic(OPERAND1, baddr);
    sprintf(aBuffer,"ANL   C, %s",
        baddr);
    return 2;
}

static uint8_t disasm_movc_a_indir_a_pc(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"MOVC  A, @A+PC");
    return 1;
}

static uint8_t disasm_div_ab(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"DIV   AB");
    return 1;
}

static uint8_t disasm_mov_mem_mem(uint16_t aPosition, char *aBuffer)
{
    char mem1[16];
    char mem2[16];
    mem_memonic(OPERAND2, mem1);
    mem_memonic(OPERAND1, mem2);
    sprintf(aBuffer,"MOV   %s, %s",        
        mem1,
        mem2);
    return 3;
}

static uint8_t disasm_mov_mem_indir_rx(uint16_t aPosition, char *aBuffer)
{
    char mem[16];mem_memonic(OPERAND1, mem);
    sprintf(aBuffer,"MOV   %s, @R%d",        
        mem,
        OPCODE&1);
    
    return 2;
}


static uint8_t disasm_mov_dptr_imm(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"MOV   DPTR, #0%02X%02Xh",        
        OPERAND1,
        OPERAND2);
    return 3;
}

static uint8_t disasm_mov_bitaddr_c(uint16_t aPosition, char *aBuffer)
{
    char baddr[16];
    bitaddr_memonic(OPERAND1, baddr);
    sprintf(aBuffer,"MOV   %s, C",
        baddr);
    return 2;
}

static uint8_t disasm_movc_a_indir_a_dptr(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"MOVC  A, @A+DPTR");
    return 1;
}

static uint8_t disasm_subb_a_imm(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"SUBB  A, #%02Xh",
        OPERAND1);
    return 2;
}

static uint8_t disasm_subb_a_mem(uint16_t aPosition, char *aBuffer)
{
    char mem[16];mem_memonic(OPERAND1, mem);
    sprintf(aBuffer,"SUBB  A, %s",
        mem);
    
    return 2;
}
static uint8_t disasm_subb_a_indir_rx(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"SUBB  A, @R%d",        
        OPCODE&1);
    return 1;
}


static uint8_t disasm_orl_c_compl_bitaddr(uint16_t aPosition, char *aBuffer)
{
    char baddr[16];
    bitaddr_memonic(OPERAND1, baddr);
    sprintf(aBuffer,"ORL   C, /%s",
        baddr);
    return 2;
}

static uint8_t disasm_mov_c_bitaddr(uint16_t aPosition, char *aBuffer)
{
    char baddr[16];
    bitaddr_memonic(OPERAND1, baddr);
    sprintf(aBuffer,"MOV   C, %s",
        baddr);
    return 2;
}

static uint8_t disasm_inc_dptr(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"INC   DPTR");
    return 1;
}

static uint8_t disasm_mul_ab(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"MUL   AB");
    return 1;
}

static uint8_t disasm_mov_indir_rx_mem(uint16_t aPosition, char *aBuffer)
{
    char mem[16];mem_memonic(OPERAND1, mem);
    sprintf(aBuffer,"MOV   @R%d, %s",        
        OPCODE&1,
        mem);
    
    return 2;
}


static uint8_t disasm_anl_c_compl_bitaddr(uint16_t aPosition, char *aBuffer)
{
    char baddr[16];
    bitaddr_memonic(OPERAND1, baddr);
    sprintf(aBuffer,"ANL   C, /%s",
        baddr);
    return 2;
}


static uint8_t disasm_cpl_bitaddr(uint16_t aPosition, char *aBuffer)
{
    char baddr[16];
    bitaddr_memonic(OPERAND1, baddr);
    sprintf(aBuffer,"CPL   %s",
        baddr);
    return 2;
}

static uint8_t disasm_cpl_c(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"CPL   C");
    return 1;
}

static uint8_t disasm_cjne_a_imm_offset(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"CJNE  A, #%02Xh, #%+d",
        OPERAND1,
        (signed char)(OPERAND2));
    return 3;
}

static uint8_t disasm_cjne_a_mem_offset(uint16_t aPosition, char *aBuffer)
{
    char mem[16];mem_memonic(OPERAND1, mem);
    sprintf(aBuffer,"CJNE  A, %s, #%+d",
        mem,
        (signed char)(OPERAND2));
    
    return 3;
}
static uint8_t disasm_cjne_indir_rx_imm_offset(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"CJNE  @R%d, #%02Xh, #%+d",
        OPCODE&1,
        OPERAND1,
        (signed char)(OPERAND2));
    return 3;
}

static uint8_t disasm_push_mem(uint16_t aPosition, char *aBuffer)
{
    char mem[16];mem_memonic(OPERAND1, mem);
    sprintf(aBuffer,"PUSH  %s",
        mem);
    
    return 2;
}


static uint8_t disasm_clr_bitaddr(uint16_t aPosition, char *aBuffer)
{
    char baddr[16];
    bitaddr_memonic(OPERAND1, baddr);
    sprintf(aBuffer,"CLR   %s",
        baddr);
    return 2;
}

static uint8_t disasm_clr_c(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"CLR   C");
    return 1;
}

static uint8_t disasm_swap_a(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"SWAP  A");
    return 1;
}

static uint8_t disasm_xch_a_mem(uint16_t aPosition, char *aBuffer)
{
    char mem[16];mem_memonic(OPERAND1, mem);
    sprintf(aBuffer,"XCH   A, %s",
        mem);
    
    return 2;
}

static uint8_t disasm_xch_a_indir_rx(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"XCH   A, @R%d",        
        OPCODE&1);
    return 1;
}


static uint8_t disasm_pop_mem(uint16_t aPosition, char *aBuffer)
{
    char mem[16];mem_memonic(OPERAND1, mem);
    sprintf(aBuffer,"POP   %s",
        mem);
    
    return 2;
}

static uint8_t disasm_setb_bitaddr(uint16_t aPosition, char *aBuffer)
{
    char baddr[16];
    bitaddr_memonic(OPERAND1, baddr);
    sprintf(aBuffer,"SETB  %s",
        baddr);
    return 2;
}

static uint8_t disasm_setb_c(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"SETB  C");
    return 1;
}

static uint8_t disasm_da_a(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"DA    A");
    return 1;
}

static uint8_t disasm_djnz_mem_offset(uint16_t aPosition, char *aBuffer)
{
    char mem[16];mem_memonic(OPERAND1, mem);
    sprintf(aBuffer,"DJNZ  %s, #%+d",        
        mem,
        (signed char)(OPERAND2));
    return 3;
}

static uint8_t disasm_xchd_a_indir_rx(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"XCHD  A, @R%d",        
        OPCODE&1);
    return 1;
}


static uint8_t disasm_movx_a_indir_dptr(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"MOVX  A, @DPTR");
    return 1;
}

static uint8_t disasm_movx_a_indir_rx(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"MOVX  A, @R%d",        
        OPCODE&1);
    return 1;
}

static uint8_t disasm_clr_a(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"CLR   A");
    return 1;
}

static uint8_t disasm_mov_a_mem(uint16_t aPosition, char *aBuffer)
{
    char mem[16];mem_memonic(OPERAND1, mem);
    sprintf(aBuffer,"MOV   A, %s",
        mem);
    
    return 2;
}

static uint8_t disasm_mov_a_indir_rx(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"MOV   A, @R%d",        
        OPCODE&1);
    return 1;
}


static uint8_t disasm_movx_indir_dptr_a(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"MOVX  @DPTR, A");
    return 1;
}

static uint8_t disasm_movx_indir_rx_a(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"MOVX  @R%d, A",        
        OPCODE&1);
    return 1;
}

static uint8_t disasm_cpl_a(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"CPL   A");
    return 1;
}

static uint8_t disasm_mov_mem_a(uint16_t aPosition, char *aBuffer)
{
    char mem[16];mem_memonic(OPERAND1, mem);
    sprintf(aBuffer,"MOV   %s, A",
        mem);
    
    return 2;
}

static uint8_t disasm_mov_indir_rx_a(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"MOV   @R%d, A",
        OPCODE&1);
    return 1;
}

static uint8_t disasm_nop(uint16_t aPosition, char *aBuffer)
{
    if (OPCODE)
        sprintf(aBuffer,"??UNKNOWN");
    else
        sprintf(aBuffer,"NOP");
    return 1;
}

static uint8_t disasm_inc_rx(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"INC   R%d",OPCODE&7);
    return 1;
}

static uint8_t disasm_dec_rx(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"DEC   R%d",OPCODE&7);
    return 1;
}

static uint8_t disasm_add_a_rx(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"ADD   A, R%d",OPCODE&7);
    return 1;
}

static uint8_t disasm_addc_a_rx(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"ADDC  A, R%d",OPCODE&7);
    return 1;
}

static uint8_t disasm_orl_a_rx(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"ORL   A, R%d",OPCODE&7);
    return 1;
}

static uint8_t disasm_anl_a_rx(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"ANL   A, R%d",OPCODE&7);
    return 1;
}

static uint8_t disasm_xrl_a_rx(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"XRL   A, R%d",OPCODE&7);
    return 1;
}


static uint8_t disasm_mov_rx_imm(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"MOV   R%d, #%02Xh",
        OPCODE&7,
        OPERAND1);
    return 2;
}

static uint8_t disasm_mov_mem_rx(uint16_t aPosition, char *aBuffer)
{
    char mem[16];mem_memonic(OPERAND1, mem);
    sprintf(aBuffer,"MOV   %s, R%d",
        mem,
        OPCODE&7);
    
    return 2;
}

static uint8_t disasm_subb_a_rx(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"SUBB  A, R%d",OPCODE&7);
    return 1;
}

static uint8_t disasm_mov_rx_mem(uint16_t aPosition, char *aBuffer)
{
    char mem[16];mem_memonic(OPERAND1, mem);
    sprintf(aBuffer,"MOV   R%d, %s",
        OPCODE&7,
        mem);
    
    return 2;
}

static uint8_t disasm_cjne_rx_imm_offset(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"CJNE  R%d, #%02Xh, #%+d",
        OPCODE&7,
        OPERAND1,
        (signed char)OPERAND2);
    return 3;
}

static uint8_t disasm_xch_a_rx(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"XCH   A, R%d",OPCODE&7);
    return 1;
}

static uint8_t disasm_djnz_rx_offset(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"DJNZ  R%d, #%+d",
        OPCODE&7,
        (signed char)OPERAND1);
    return 2;
}

static uint8_t disasm_mov_a_rx(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"MOV   A, R%d",OPCODE&7);
    return 1;
}

static uint8_t disasm_mov_rx_a(uint16_t aPosition, char *aBuffer)
{
    sprintf(aBuffer,"MOV   R%d, A",OPCODE&7);
    return 1;
}

void disasm_setptrs()
{
    int i;
    for (i = 0; i < 8; i++)
    {
        CPU.dec[0x08 + i] = &disasm_inc_rx;
        CPU.dec[0x18 + i] = &disasm_dec_rx;
        CPU.dec[0x28 + i] = &disasm_add_a_rx;
        CPU.dec[0x38 + i] = &disasm_addc_a_rx;
        CPU.dec[0x48 + i] = &disasm_orl_a_rx;
        CPU.dec[0x58 + i] = &disasm_anl_a_rx;
        CPU.dec[0x68 + i] = &disasm_xrl_a_rx;
        CPU.dec[0x78 + i] = &disasm_mov_rx_imm;
        CPU.dec[0x88 + i] = &disasm_mov_mem_rx;
        CPU.dec[0x98 + i] = &disasm_subb_a_rx;
        CPU.dec[0xa8 + i] = &disasm_mov_rx_mem;
        CPU.dec[0xb8 + i] = &disasm_cjne_rx_imm_offset;
        CPU.dec[0xc8 + i] = &disasm_xch_a_rx;
        CPU.dec[0xd8 + i] = &disasm_djnz_rx_offset;
        CPU.dec[0xe8 + i] = &disasm_mov_a_rx;
        CPU.dec[0xf8 + i] = &disasm_mov_rx_a;
    }
    CPU.dec[0x00] = &disasm_nop;
    CPU.dec[0x01] = &disasm_ajmp_offset;
    CPU.dec[0x02] = &disasm_ljmp_address;
    CPU.dec[0x03] = &disasm_rr_a;
    CPU.dec[0x04] = &disasm_inc_a;
    CPU.dec[0x05] = &disasm_inc_mem;
    CPU.dec[0x06] = &disasm_inc_indir_rx;
    CPU.dec[0x07] = &disasm_inc_indir_rx;

    CPU.dec[0x10] = &disasm_jbc_bitaddr_offset;
    CPU.dec[0x11] = &disasm_acall_offset;
    CPU.dec[0x12] = &disasm_lcall_address;
    CPU.dec[0x13] = &disasm_rrc_a;
    CPU.dec[0x14] = &disasm_dec_a;
    CPU.dec[0x15] = &disasm_dec_mem;
    CPU.dec[0x16] = &disasm_dec_indir_rx;
    CPU.dec[0x17] = &disasm_dec_indir_rx;

    CPU.dec[0x20] = &disasm_jb_bitaddr_offset;
    CPU.dec[0x21] = &disasm_ajmp_offset;
    CPU.dec[0x22] = &disasm_ret;
    CPU.dec[0x23] = &disasm_rl_a;
    CPU.dec[0x24] = &disasm_add_a_imm;
    CPU.dec[0x25] = &disasm_add_a_mem;
    CPU.dec[0x26] = &disasm_add_a_indir_rx;
    CPU.dec[0x27] = &disasm_add_a_indir_rx;

    CPU.dec[0x30] = &disasm_jnb_bitaddr_offset;
    CPU.dec[0x31] = &disasm_acall_offset;
    CPU.dec[0x32] = &disasm_reti;
    CPU.dec[0x33] = &disasm_rlc_a;
    CPU.dec[0x34] = &disasm_addc_a_imm;
    CPU.dec[0x35] = &disasm_addc_a_mem;
    CPU.dec[0x36] = &disasm_addc_a_indir_rx;
    CPU.dec[0x37] = &disasm_addc_a_indir_rx;

    CPU.dec[0x40] = &disasm_jc_offset;
    CPU.dec[0x41] = &disasm_ajmp_offset;
    CPU.dec[0x42] = &disasm_orl_mem_a;
    CPU.dec[0x43] = &disasm_orl_mem_imm;
    CPU.dec[0x44] = &disasm_orl_a_imm;
    CPU.dec[0x45] = &disasm_orl_a_mem;
    CPU.dec[0x46] = &disasm_orl_a_indir_rx;
    CPU.dec[0x47] = &disasm_orl_a_indir_rx;

    CPU.dec[0x50] = &disasm_jnc_offset;
    CPU.dec[0x51] = &disasm_acall_offset;
    CPU.dec[0x52] = &disasm_anl_mem_a;
    CPU.dec[0x53] = &disasm_anl_mem_imm;
    CPU.dec[0x54] = &disasm_anl_a_imm;
    CPU.dec[0x55] = &disasm_anl_a_mem;
    CPU.dec[0x56] = &disasm_anl_a_indir_rx;
    CPU.dec[0x57] = &disasm_anl_a_indir_rx;

    CPU.dec[0x60] = &disasm_jz_offset;
    CPU.dec[0x61] = &disasm_ajmp_offset;
    CPU.dec[0x62] = &disasm_xrl_mem_a;
    CPU.dec[0x63] = &disasm_xrl_mem_imm;
    CPU.dec[0x64] = &disasm_xrl_a_imm;
    CPU.dec[0x65] = &disasm_xrl_a_mem;
    CPU.dec[0x66] = &disasm_xrl_a_indir_rx;
    CPU.dec[0x67] = &disasm_xrl_a_indir_rx;

    CPU.dec[0x70] = &disasm_jnz_offset;
    CPU.dec[0x71] = &disasm_acall_offset;
    CPU.dec[0x72] = &disasm_orl_c_bitaddr;
    CPU.dec[0x73] = &disasm_jmp_indir_a_dptr;
    CPU.dec[0x74] = &disasm_mov_a_imm;
    CPU.dec[0x75] = &disasm_mov_mem_imm;
    CPU.dec[0x76] = &disasm_mov_indir_rx_imm;
    CPU.dec[0x77] = &disasm_mov_indir_rx_imm;

    CPU.dec[0x80] = &disasm_sjmp_offset;
    CPU.dec[0x81] = &disasm_ajmp_offset;
    CPU.dec[0x82] = &disasm_anl_c_bitaddr;
    CPU.dec[0x83] = &disasm_movc_a_indir_a_pc;
    CPU.dec[0x84] = &disasm_div_ab;
    CPU.dec[0x85] = &disasm_mov_mem_mem;
    CPU.dec[0x86] = &disasm_mov_mem_indir_rx;
    CPU.dec[0x87] = &disasm_mov_mem_indir_rx;

    CPU.dec[0x90] = &disasm_mov_dptr_imm;
    CPU.dec[0x91] = &disasm_acall_offset;
    CPU.dec[0x92] = &disasm_mov_bitaddr_c;
    CPU.dec[0x93] = &disasm_movc_a_indir_a_dptr;
    CPU.dec[0x94] = &disasm_subb_a_imm;
    CPU.dec[0x95] = &disasm_subb_a_mem;
    CPU.dec[0x96] = &disasm_subb_a_indir_rx;
    CPU.dec[0x97] = &disasm_subb_a_indir_rx;

    CPU.dec[0xa0] = &disasm_orl_c_compl_bitaddr;
    CPU.dec[0xa1] = &disasm_ajmp_offset;
    CPU.dec[0xa2] = &disasm_mov_c_bitaddr;
    CPU.dec[0xa3] = &disasm_inc_dptr;
    CPU.dec[0xa4] = &disasm_mul_ab;
    CPU.dec[0xa5] = &disasm_nop; // unused
    CPU.dec[0xa6] = &disasm_mov_indir_rx_mem;
    CPU.dec[0xa7] = &disasm_mov_indir_rx_mem;

    CPU.dec[0xb0] = &disasm_anl_c_compl_bitaddr;
    CPU.dec[0xb1] = &disasm_acall_offset;
    CPU.dec[0xb2] = &disasm_cpl_bitaddr;
    CPU.dec[0xb3] = &disasm_cpl_c;
    CPU.dec[0xb4] = &disasm_cjne_a_imm_offset;
    CPU.dec[0xb5] = &disasm_cjne_a_mem_offset;
    CPU.dec[0xb6] = &disasm_cjne_indir_rx_imm_offset;
    CPU.dec[0xb7] = &disasm_cjne_indir_rx_imm_offset;

    CPU.dec[0xc0] = &disasm_push_mem;
    CPU.dec[0xc1] = &disasm_ajmp_offset;
    CPU.dec[0xc2] = &disasm_clr_bitaddr;
    CPU.dec[0xc3] = &disasm_clr_c;
    CPU.dec[0xc4] = &disasm_swap_a;
    CPU.dec[0xc5] = &disasm_xch_a_mem;
    CPU.dec[0xc6] = &disasm_xch_a_indir_rx;
    CPU.dec[0xc7] = &disasm_xch_a_indir_rx;

    CPU.dec[0xd0] = &disasm_pop_mem;
    CPU.dec[0xd1] = &disasm_acall_offset;
    CPU.dec[0xd2] = &disasm_setb_bitaddr;
    CPU.dec[0xd3] = &disasm_setb_c;
    CPU.dec[0xd4] = &disasm_da_a;
    CPU.dec[0xd5] = &disasm_djnz_mem_offset;
    CPU.dec[0xd6] = &disasm_xchd_a_indir_rx;
    CPU.dec[0xd7] = &disasm_xchd_a_indir_rx;

    CPU.dec[0xe0] = &disasm_movx_a_indir_dptr;
    CPU.dec[0xe1] = &disasm_ajmp_offset;
    CPU.dec[0xe2] = &disasm_movx_a_indir_rx;
    CPU.dec[0xe3] = &disasm_movx_a_indir_rx;
    CPU.dec[0xe4] = &disasm_clr_a;
    CPU.dec[0xe5] = &disasm_mov_a_mem;
    CPU.dec[0xe6] = &disasm_mov_a_indir_rx;
    CPU.dec[0xe7] = &disasm_mov_a_indir_rx;

    CPU.dec[0xf0] = &disasm_movx_indir_dptr_a;
    CPU.dec[0xf1] = &disasm_acall_offset;
    CPU.dec[0xf2] = &disasm_movx_indir_rx_a;
    CPU.dec[0xf3] = &disasm_movx_indir_rx_a;
    CPU.dec[0xf4] = &disasm_cpl_a;
    CPU.dec[0xf5] = &disasm_mov_mem_a;
    CPU.dec[0xf6] = &disasm_mov_indir_rx_a;
    CPU.dec[0xf7] = &disasm_mov_indir_rx_a;
}
