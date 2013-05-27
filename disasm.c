/* 8051 emulator core
 * Copyright 2006 Jari Komppa
 * Released under LGPL
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

char * mem_memonic(int aValue)
{
    char *res = NULL;

    if (aValue > 0x7f)
    {
        switch (aValue - 0x80)
        {
        case REG_ACC:
            res = strdup("ACC");
            break;
        case REG_B:
            res = strdup("B");
            break;
        case REG_PSW:
            res = strdup("PSW");
            break;
        case REG_SP:
            res = strdup("SP");
            break;
        case REG_DPL:
            res = strdup("DPL");
            break;
        case REG_DPH:
            res = strdup("DPH");
            break;
        case REG_P0:
            res = strdup("P0");
            break;
        case REG_P1:
            res = strdup("P1");
            break;
        case REG_P2:
            res = strdup("P2");
            break;
        case REG_P3:
            res = strdup("P3");
            break;
        case REG_IP:
            res = strdup("IP");
            break;
        case REG_IE:
            res = strdup("IE");
            break;
        case REG_TIMOD:
            res = strdup("TIMOD");
            break;
        case REG_TCON:
            res = strdup("TCON");
            break;
        case REG_TH0:
            res = strdup("TH0");
            break;
        case REG_TL0:
            res = strdup("TL0");
            break;
        case REG_TH1:
            res = strdup("TH1");
            break;
        case REG_TL1:
            res = strdup("TL1");
            break;
        case REG_SCON:
            res = strdup("SCON");
            break;
        case REG_PCON:
            res = strdup("PCON");
            break;
        }
    }
    if (res == NULL)
    {
        res = malloc(16);
        sprintf(res, "%03Xh", aValue);
    }
    return res;
}

static int disasm_ajmp_offset(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"AJMP  #%05Xh",
        (aPosition + 2) & 0xf800 |
        aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)] | 
        ((aCPU->mCodeMem[(aPosition)&(aCPU->mCodeMemSize-1)] & 0xe0) << 3));
    return 2;
}

static int disasm_ljmp_address(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"LJMP  #%05Xh",        
        (aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)] << 8) | 
        aCPU->mCodeMem[(aPosition + 2)&(aCPU->mCodeMemSize-1)]);
    return 3;
}

static int disasm_rr_a(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"RR    A");
    return 1;
}

static int disasm_inc_a(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"INC   A");
    return 1;
}

static int disasm_inc_mem(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    char * mem = mem_memonic(aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    sprintf(aBuffer,"INC   %s",        
        mem);
    free(mem);
    return 2;
}

static int disasm_inc_indir_rx(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"INC   @R%d",        
        aCPU->mCodeMem[(aPosition)&(aCPU->mCodeMemSize-1)] & 1);
    return 1;
}

static int disasm_jbc_bitaddr_offset(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"JBC   %03Xh, #%+d",        
        aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)],
        (signed char)aCPU->mCodeMem[(aPosition + 2)&(aCPU->mCodeMemSize-1)]);
    return 3;
}

static int disasm_acall_offset(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"ACALL %05Xh",
        (aPosition + 2) & 0xf800 |
        aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)] | 
        ((aCPU->mCodeMem[(aPosition)&(aCPU->mCodeMemSize-1)] & 0xe0) << 3));
    return 2;
}

static int disasm_lcall_address(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"LCALL #%05Xh",        
        (aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)] << 8) | 
        aCPU->mCodeMem[(aPosition + 2)&(aCPU->mCodeMemSize-1)]);
    return 3;
}

static int disasm_rrc_a(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"RRC   A");
    return 1;
}

static int disasm_dec_a(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"DEC   A");
    return 1;
}

static int disasm_dec_mem(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    char * mem = mem_memonic(aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    sprintf(aBuffer,"DEC   %s",        
        mem);
    free(mem);
    return 2;
}

static int disasm_dec_indir_rx(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"DEC   @R%d",        
        aCPU->mCodeMem[(aPosition)&(aCPU->mCodeMemSize-1)] & 1);
    return 1;
}


static int disasm_jb_bitaddr_offset(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"JB   %03Xh, #%+d",        
        aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)],
        (signed char)aCPU->mCodeMem[(aPosition + 2)&(aCPU->mCodeMemSize-1)]);
    return 3;
}

static int disasm_ret(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"RET");
    return 1;
}

static int disasm_rl_a(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"RL    A");
    return 1;
}

static int disasm_add_a_imm(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"ADD   A, #%03Xh",        
        aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    return 2;
}

static int disasm_add_a_mem(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    char * mem = mem_memonic(aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    sprintf(aBuffer,"ADD   A, %s",        
        mem);
    free(mem);
    return 2;
}

static int disasm_add_a_indir_rx(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"ADD   A, @R%d",        
        aCPU->mCodeMem[(aPosition)&(aCPU->mCodeMemSize-1)]&1);
    return 1;
}

static int disasm_jnb_bitaddr_offset(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"JNB   %03Xh, #%+d",        
        aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)],
        (signed char)aCPU->mCodeMem[(aPosition + 2)&(aCPU->mCodeMemSize-1)]);
    return 3;
}

static int disasm_reti(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"RETI");
    return 1;
}

static int disasm_rlc_a(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"RLC   A");
    return 1;
}

static int disasm_addc_a_imm(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"ADD   AC, #%03Xh",        
        aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    return 2;
}

static int disasm_addc_a_mem(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    char * mem = mem_memonic(aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    sprintf(aBuffer,"ADDC  A, %s",        
        mem);
    free(mem);
    return 2;
}

static int disasm_addc_a_indir_rx(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"ADDC  A, @R%d",        
        aCPU->mCodeMem[(aPosition)&(aCPU->mCodeMemSize-1)]&1);
    return 1;
}


static int disasm_jc_offset(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"JC    #%+d",        
        (signed char)aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    return 2;
}

static int disasm_orl_mem_a(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    char * mem = mem_memonic(aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    sprintf(aBuffer,"ORL   %s, A",        
        mem);
    free(mem);
    return 2;
}

static int disasm_orl_mem_imm(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    char * mem = mem_memonic(aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    sprintf(aBuffer,"ORL   %s, #%03Xh",        
        mem,
        aCPU->mCodeMem[(aPosition + 2)&(aCPU->mCodeMemSize-1)]);
    free(mem);
    return 3;
}

static int disasm_orl_a_imm(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"ORL   A, #%03Xh",        
        aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    return 2;
}

static int disasm_orl_a_mem(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    char * mem = mem_memonic(aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    sprintf(aBuffer,"ORL   A, %s",        
        mem);
    free(mem);
    return 2;
}

static int disasm_orl_a_indir_rx(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"ORL   A, @R%d",        
        aCPU->mCodeMem[(aPosition)&(aCPU->mCodeMemSize-1)]&1);
    return 1;
}


static int disasm_jnc_offset(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"JNC   #%+d",        
        (signed char)aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    return 2;
}

static int disasm_anl_mem_a(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    char * mem = mem_memonic(aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    sprintf(aBuffer,"ANL   %s, A",
        mem);
    free(mem);
    return 2;
}

static int disasm_anl_mem_imm(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    char * mem = mem_memonic(aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    sprintf(aBuffer,"ANL   %s, #%03Xh",        
        mem,
        aCPU->mCodeMem[(aPosition + 2)&(aCPU->mCodeMemSize-1)]);
    free(mem);
    return 3;
}

static int disasm_anl_a_imm(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"ANL   A, #%03Xh",
        aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    return 2;
}

static int disasm_anl_a_mem(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    char * mem = mem_memonic(aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    sprintf(aBuffer,"ANL   A, %s",
        mem);
    free(mem);
    return 2;
}

static int disasm_anl_a_indir_rx(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"ANL   A, @R%d",        
        aCPU->mCodeMem[(aPosition)&(aCPU->mCodeMemSize-1)]&1);
    return 1;
}


static int disasm_jz_offset(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"JZ    #%+d",        
        (signed char)aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    return 2;
}

static int disasm_xrl_mem_a(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    char * mem = mem_memonic(aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    sprintf(aBuffer,"XRL   %s, A",
        mem);
    free(mem);
    return 2;
}

static int disasm_xrl_mem_imm(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    char * mem = mem_memonic(aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    sprintf(aBuffer,"XRL   %s, #%03Xh",        
        mem,
        aCPU->mCodeMem[(aPosition + 2)&(aCPU->mCodeMemSize-1)]);
    free(mem);
    return 3;
}

static int disasm_xrl_a_imm(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"XRL   A, #%03Xh",
        aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    return 2;
}

static int disasm_xrl_a_mem(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    char * mem = mem_memonic(aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    sprintf(aBuffer,"XRL   A, %s",
        mem);
    free(mem);
    return 2;
}

static int disasm_xrl_a_indir_rx(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"XRL A, @R%d",        
        aCPU->mCodeMem[(aPosition)&(aCPU->mCodeMemSize-1)]&1);
    return 1;
}


static int disasm_jnz_offset(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"JNZ   #%+d",        
        (signed char)aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    return 2;
}

static int disasm_orl_c_bitaddr(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"ORL   C, %03Xh",
        aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    return 2;
}

static int disasm_jmp_indir_a_dptr(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"JMP   @A+DPTR");
    return 1;
}

static int disasm_mov_a_imm(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"MOV   A, #%03Xh",
        aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    return 2;
}

static int disasm_mov_mem_imm(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    char * mem = mem_memonic(aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    sprintf(aBuffer,"MOV   %s, #%03Xh",        
        mem,
        aCPU->mCodeMem[(aPosition + 2)&(aCPU->mCodeMemSize-1)]);
    free(mem);
    return 3;
}

static int disasm_mov_indir_rx_imm(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"MOV   @R%d, #%03Xh",        
        aCPU->mCodeMem[(aPosition)&(aCPU->mCodeMemSize-1)]&1,
        aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    return 2;
}


static int disasm_sjmp_offset(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"SJMP  #%+d",        
        (signed char)(aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]));
    return 2;
}

static int disasm_anl_c_bitaddr(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"ANL   C, %03Xh",
        aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    return 2;
}

static int disasm_movc_a_indir_a_pc(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"MOVC  A, @A+PC");
    return 1;
}

static int disasm_div_ab(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"DIV   AB");
    return 1;
}

static int disasm_mov_mem_mem(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    char * mem1 = mem_memonic(aCPU->mCodeMem[(aPosition + 2)&(aCPU->mCodeMemSize-1)]);
    char * mem2 = mem_memonic(aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    sprintf(aBuffer,"MOV   %s, %s",        
        mem1,
        mem2);
    free(mem2);
    free(mem1);
    return 3;
}

static int disasm_mov_mem_indir_rx(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    char * mem = mem_memonic(aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    sprintf(aBuffer,"MOV   %s, @R%d",        
        mem,
        aCPU->mCodeMem[(aPosition)&(aCPU->mCodeMemSize-1)]&1);
    free(mem);
    return 2;
}


static int disasm_mov_dptr_imm(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"MOV   DPTR, #0%02X%02Xh",        
        aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)],
        aCPU->mCodeMem[(aPosition + 2)&(aCPU->mCodeMemSize-1)]);
    return 3;
}

static int disasm_mov_bitaddr_c(struct em8051 *aCPU, int aPosition, char *aBuffer) 
{
    sprintf(aBuffer,"MOV   %03Xh, C",
        aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    return 2;
}

static int disasm_movc_a_indir_a_dptr(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"MOVC  A, @A+DPTR");
    return 1;
}

static int disasm_subb_a_imm(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"SUBB  A, #%03Xh",
        aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    return 2;
}

static int disasm_subb_a_mem(struct em8051 *aCPU, int aPosition, char *aBuffer) 
{
    char * mem = mem_memonic(aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    sprintf(aBuffer,"SUBB  A, %s",
        mem);
    free(mem);
    return 2;
}
static int disasm_subb_a_indir_rx(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"SUBB  A, @R%d",        
        aCPU->mCodeMem[(aPosition)&(aCPU->mCodeMemSize-1)]&1);
    return 1;
}


static int disasm_orl_c_compl_bitaddr(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"ORL   C, /%03Xh",
        aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    return 2;
}

static int disasm_mov_c_bitaddr(struct em8051 *aCPU, int aPosition, char *aBuffer) 
{
    sprintf(aBuffer,"MOV   C, %03Xh",
        aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    return 2;
}

static int disasm_inc_dptr(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"INC   DPTR");
    return 1;
}

static int disasm_mul_ab(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"MUL   AB");
    return 1;
}

static int disasm_mov_indir_rx_mem(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    char * mem = mem_memonic(aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    sprintf(aBuffer,"MOV   @R%d, %s",        
        aCPU->mCodeMem[(aPosition)&(aCPU->mCodeMemSize-1)]&1,
        mem);
    free(mem);
    return 2;
}


static int disasm_anl_c_compl_bitaddr(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"ANL   C, /%03Xh",
        aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    return 2;
}


static int disasm_cpl_bitaddr(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"CPL   %03Xh",
        aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    return 2;
}

static int disasm_cpl_c(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"CPL   C");
    return 1;
}

static int disasm_cjne_a_imm_offset(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"CJNE  A, #%03Xh, #%+d",
        aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)],
        (signed char)(aCPU->mCodeMem[(aPosition + 2)&(aCPU->mCodeMemSize-1)]));
    return 3;
}

static int disasm_cjne_a_mem_offset(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    char * mem = mem_memonic(aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    sprintf(aBuffer,"CJNE  A, %s, #%+d",
        mem,
        (signed char)(aCPU->mCodeMem[(aPosition + 2)&(aCPU->mCodeMemSize-1)]));
    free(mem);
    return 3;
}
static int disasm_cjne_indir_rx_imm_offset(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"CJNE  R%d, #%03Xh, #%+d",
        aCPU->mCodeMem[(aPosition)&(aCPU->mCodeMemSize-1)]&1,
        aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)],
        (signed char)(aCPU->mCodeMem[(aPosition + 2)&(aCPU->mCodeMemSize-1)]));
    return 3;
}

static int disasm_push_mem(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    char * mem = mem_memonic(aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    sprintf(aBuffer,"PUSH  %s",
        mem);
    free(mem);
    return 2;
}


static int disasm_clr_bitaddr(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"CLR   %03Xh",
        aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    return 2;
}

static int disasm_clr_c(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"CLR   C");
    return 1;
}

static int disasm_swap_a(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"SWAP  A");
    return 1;
}

static int disasm_xch_a_mem(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    char * mem = mem_memonic(aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    sprintf(aBuffer,"XCH   A, %s",
        mem);
    free(mem);
    return 2;
}

static int disasm_xch_a_indir_rx(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"XCH   A, @R%d",        
        aCPU->mCodeMem[(aPosition)&(aCPU->mCodeMemSize-1)]&1);
    return 1;
}


static int disasm_pop_mem(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    char * mem = mem_memonic(aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    sprintf(aBuffer,"POP   %s",
        mem);
    free(mem);
    return 2;
}

static int disasm_setb_bitaddr(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"SETB  %03Xh",
        aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    return 2;
}

static int disasm_setb_c(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"SETB  C");
    return 1;
}

static int disasm_da_a(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"DA    A");
    return 1;
}

static int disasm_djnz_mem_offset(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    char * mem = mem_memonic(aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    sprintf(aBuffer,"DJNZ  %s, #%+d",        
        mem,
        (signed char)(aCPU->mCodeMem[(aPosition + 2)&(aCPU->mCodeMemSize-1)]));
    return 3;
}

static int disasm_xchd_a_indir_rx(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"XCHD  A, @R%d",        
        aCPU->mCodeMem[(aPosition)&(aCPU->mCodeMemSize-1)]&1);
    return 1;
}


static int disasm_movx_a_indir_dptr(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"MOVX  A, @DPTR");
    return 1;
}

static int disasm_movx_a_indir_rx(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"MOVX  A, @R%d",        
        aCPU->mCodeMem[(aPosition)&(aCPU->mCodeMemSize-1)]&1);
    return 1;
}

static int disasm_clr_a(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"CLR   A");
    return 1;
}

static int disasm_mov_a_mem(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    char * mem = mem_memonic(aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    sprintf(aBuffer,"MOV   A, %s",
        mem);
    free(mem);
    return 2;
}

static int disasm_mov_a_indir_rx(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"MOV   A, @R%d",        
        aCPU->mCodeMem[(aPosition)&(aCPU->mCodeMemSize-1)]&1);
    return 1;
}


static int disasm_movx_indir_dptr_a(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"MOVX  @DPTR, A");
    return 1;
}

static int disasm_movx_indir_rx_a(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"MOVX  @R%d, A",        
        aCPU->mCodeMem[(aPosition)&(aCPU->mCodeMemSize-1)]&1);
    return 1;
}

static int disasm_cpl_a(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"CPL   A");
    return 1;
}

static int disasm_mov_mem_a(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    char * mem = mem_memonic(aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    sprintf(aBuffer,"MOV   %s, A",
        mem);
    free(mem);
    return 2;
}

static int disasm_mov_indir_rx_a(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"MOV   @R%d, A",
        aCPU->mCodeMem[(aPosition)&(aCPU->mCodeMemSize-1)]&1);
    return 1;
}

static int disasm_nop(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"NOP");
    return 1;
}

static int disasm_inc_rx(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"INC   R%d",aCPU->mCodeMem[aPosition&(aCPU->mCodeMemSize-1)]&7);
    return 1;
}

static int disasm_dec_rx(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"DEC   R%d",aCPU->mCodeMem[aPosition&(aCPU->mCodeMemSize-1)]&7);
    return 1;
}

static int disasm_add_a_rx(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"ADD   A, R%d",aCPU->mCodeMem[aPosition&(aCPU->mCodeMemSize-1)]&7);
    return 1;
}

static int disasm_addc_a_rx(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"ADDC  A, R%d",aCPU->mCodeMem[aPosition&(aCPU->mCodeMemSize-1)]&7);
    return 1;
}

static int disasm_orl_a_rx(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"ORL   A, R%d",aCPU->mCodeMem[aPosition&(aCPU->mCodeMemSize-1)]&7);
    return 1;
}

static int disasm_anl_a_rx(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"ANL   A, R%d",aCPU->mCodeMem[aPosition&(aCPU->mCodeMemSize-1)]&7);
    return 1;
}

static int disasm_xrl_a_rx(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"XRL   A, R%d",aCPU->mCodeMem[aPosition&(aCPU->mCodeMemSize-1)]&7);
    return 1;
}


static int disasm_mov_rx_imm(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"MOV   R%d, #%03Xh",
        aCPU->mCodeMem[aPosition&(aCPU->mCodeMemSize-1)]&7,
        aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    return 2;
}

static int disasm_mov_mem_rx(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    char * mem = mem_memonic(aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    sprintf(aBuffer,"MOV   %s, R%d",
        mem,
        aCPU->mCodeMem[aPosition&(aCPU->mCodeMemSize-1)]&7);
    free(mem);
    return 2;
}

static int disasm_subb_a_rx(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"SUBB  A, R%d",aCPU->mCodeMem[aPosition&(aCPU->mCodeMemSize-1)]&7);
    return 1;
}

static int disasm_mov_rx_mem(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    char * mem = mem_memonic(aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    sprintf(aBuffer,"MOV   R%d, %s",
        aCPU->mCodeMem[aPosition&(aCPU->mCodeMemSize-1)]&7,
        mem);
    free(mem);
    return 2;
}

static int disasm_cjne_rx_imm_offset(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"CJNE  R%d, #%03Xh, #%+d",
        aCPU->mCodeMem[aPosition&(aCPU->mCodeMemSize-1)]&7,
        aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)],
        (signed char)aCPU->mCodeMem[(aPosition + 2)&(aCPU->mCodeMemSize-1)]);
    return 3;
}

static int disasm_xch_a_rx(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"XCH   A, R%d",aCPU->mCodeMem[aPosition&(aCPU->mCodeMemSize-1)]&7);
    return 1;
}

static int disasm_djnz_rx_offset(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"DJNZ  R%d, #%+d",
        aCPU->mCodeMem[aPosition&(aCPU->mCodeMemSize-1)]&7,
        (signed char)aCPU->mCodeMem[(aPosition + 1)&(aCPU->mCodeMemSize-1)]);
    return 2;
}

static int disasm_mov_a_rx(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"MOV   A, R%d",aCPU->mCodeMem[aPosition&(aCPU->mCodeMemSize-1)]&7);
    return 1;
}

static int disasm_mov_rx_a(struct em8051 *aCPU, int aPosition, char *aBuffer)
{
    sprintf(aBuffer,"MOV   R%d, A",aCPU->mCodeMem[aPosition&(aCPU->mCodeMemSize-1)]&7);
    return 1;
}

void disasm_setptrs(struct em8051 *aCPU)
{
    int i;
    for (i = 0; i < 8; i++)
    {
        aCPU->dec[0x08 + i] = &disasm_inc_rx;
        aCPU->dec[0x18 + i] = &disasm_dec_rx;
        aCPU->dec[0x28 + i] = &disasm_add_a_rx;
        aCPU->dec[0x38 + i] = &disasm_addc_a_rx;
        aCPU->dec[0x48 + i] = &disasm_orl_a_rx;
        aCPU->dec[0x58 + i] = &disasm_anl_a_rx;
        aCPU->dec[0x68 + i] = &disasm_xrl_a_rx;
        aCPU->dec[0x78 + i] = &disasm_mov_rx_imm;
        aCPU->dec[0x88 + i] = &disasm_mov_mem_rx;
        aCPU->dec[0x98 + i] = &disasm_subb_a_rx;
        aCPU->dec[0xa8 + i] = &disasm_mov_rx_mem;
        aCPU->dec[0xb8 + i] = &disasm_cjne_rx_imm_offset;
        aCPU->dec[0xc8 + i] = &disasm_xch_a_rx;
        aCPU->dec[0xd8 + i] = &disasm_djnz_rx_offset;
        aCPU->dec[0xe8 + i] = &disasm_mov_a_rx;
        aCPU->dec[0xf8 + i] = &disasm_mov_rx_a;
    }
    aCPU->dec[0x00] = &disasm_nop;
    aCPU->dec[0x01] = &disasm_ajmp_offset;
    aCPU->dec[0x02] = &disasm_ljmp_address;
    aCPU->dec[0x03] = &disasm_rr_a;
    aCPU->dec[0x04] = &disasm_inc_a;
    aCPU->dec[0x05] = &disasm_inc_mem;
    aCPU->dec[0x06] = &disasm_inc_indir_rx;
    aCPU->dec[0x07] = &disasm_inc_indir_rx;

    aCPU->dec[0x10] = &disasm_jbc_bitaddr_offset;
    aCPU->dec[0x11] = &disasm_acall_offset;
    aCPU->dec[0x12] = &disasm_lcall_address;
    aCPU->dec[0x13] = &disasm_rrc_a;
    aCPU->dec[0x14] = &disasm_dec_a;
    aCPU->dec[0x15] = &disasm_dec_mem;
    aCPU->dec[0x16] = &disasm_dec_indir_rx;
    aCPU->dec[0x17] = &disasm_dec_indir_rx;

    aCPU->dec[0x20] = &disasm_jb_bitaddr_offset;
    aCPU->dec[0x21] = &disasm_ajmp_offset;
    aCPU->dec[0x22] = &disasm_ret;
    aCPU->dec[0x23] = &disasm_rl_a;
    aCPU->dec[0x24] = &disasm_add_a_imm;
    aCPU->dec[0x25] = &disasm_add_a_mem;
    aCPU->dec[0x26] = &disasm_add_a_indir_rx;
    aCPU->dec[0x27] = &disasm_add_a_indir_rx;

    aCPU->dec[0x30] = &disasm_jnb_bitaddr_offset;
    aCPU->dec[0x31] = &disasm_acall_offset;
    aCPU->dec[0x32] = &disasm_reti;
    aCPU->dec[0x33] = &disasm_rlc_a;
    aCPU->dec[0x34] = &disasm_addc_a_imm;
    aCPU->dec[0x35] = &disasm_addc_a_mem;
    aCPU->dec[0x36] = &disasm_addc_a_indir_rx;
    aCPU->dec[0x37] = &disasm_addc_a_indir_rx;

    aCPU->dec[0x40] = &disasm_jc_offset;
    aCPU->dec[0x41] = &disasm_ajmp_offset;
    aCPU->dec[0x42] = &disasm_orl_mem_a;
    aCPU->dec[0x43] = &disasm_orl_mem_imm;
    aCPU->dec[0x44] = &disasm_orl_a_imm;
    aCPU->dec[0x45] = &disasm_orl_a_mem;
    aCPU->dec[0x46] = &disasm_orl_a_indir_rx;
    aCPU->dec[0x47] = &disasm_orl_a_indir_rx;

    aCPU->dec[0x50] = &disasm_jnc_offset;
    aCPU->dec[0x51] = &disasm_acall_offset;
    aCPU->dec[0x52] = &disasm_anl_mem_a;
    aCPU->dec[0x53] = &disasm_anl_mem_imm;
    aCPU->dec[0x54] = &disasm_anl_a_imm;
    aCPU->dec[0x55] = &disasm_anl_a_mem;
    aCPU->dec[0x56] = &disasm_anl_a_indir_rx;
    aCPU->dec[0x57] = &disasm_anl_a_indir_rx;

    aCPU->dec[0x60] = &disasm_jz_offset;
    aCPU->dec[0x61] = &disasm_ajmp_offset;
    aCPU->dec[0x62] = &disasm_xrl_mem_a;
    aCPU->dec[0x63] = &disasm_xrl_mem_imm;
    aCPU->dec[0x64] = &disasm_xrl_a_imm;
    aCPU->dec[0x65] = &disasm_xrl_a_mem;
    aCPU->dec[0x66] = &disasm_xrl_a_indir_rx;
    aCPU->dec[0x67] = &disasm_xrl_a_indir_rx;

    aCPU->dec[0x70] = &disasm_jnz_offset;
    aCPU->dec[0x71] = &disasm_acall_offset;
    aCPU->dec[0x72] = &disasm_orl_c_bitaddr;
    aCPU->dec[0x73] = &disasm_jmp_indir_a_dptr;
    aCPU->dec[0x74] = &disasm_mov_a_imm;
    aCPU->dec[0x75] = &disasm_mov_mem_imm;
    aCPU->dec[0x76] = &disasm_mov_indir_rx_imm;
    aCPU->dec[0x77] = &disasm_mov_indir_rx_imm;

    aCPU->dec[0x80] = &disasm_sjmp_offset;
    aCPU->dec[0x81] = &disasm_ajmp_offset;
    aCPU->dec[0x82] = &disasm_anl_c_bitaddr;
    aCPU->dec[0x83] = &disasm_movc_a_indir_a_pc;
    aCPU->dec[0x84] = &disasm_div_ab;
    aCPU->dec[0x85] = &disasm_mov_mem_mem;
    aCPU->dec[0x86] = &disasm_mov_mem_indir_rx;
    aCPU->dec[0x87] = &disasm_mov_mem_indir_rx;

    aCPU->dec[0x90] = &disasm_mov_dptr_imm;
    aCPU->dec[0x91] = &disasm_acall_offset;
    aCPU->dec[0x92] = &disasm_mov_bitaddr_c;
    aCPU->dec[0x93] = &disasm_movc_a_indir_a_dptr;
    aCPU->dec[0x94] = &disasm_subb_a_imm;
    aCPU->dec[0x95] = &disasm_subb_a_mem;
    aCPU->dec[0x96] = &disasm_subb_a_indir_rx;
    aCPU->dec[0x97] = &disasm_subb_a_indir_rx;

    aCPU->dec[0xa0] = &disasm_orl_c_compl_bitaddr;
    aCPU->dec[0xa1] = &disasm_ajmp_offset;
    aCPU->dec[0xa2] = &disasm_mov_c_bitaddr;
    aCPU->dec[0xa3] = &disasm_inc_dptr;
    aCPU->dec[0xa4] = &disasm_mul_ab;
    aCPU->dec[0xa5] = &disasm_nop; // unused
    aCPU->dec[0xa6] = &disasm_mov_indir_rx_mem;
    aCPU->dec[0xa7] = &disasm_mov_indir_rx_mem;

    aCPU->dec[0xb0] = &disasm_anl_c_compl_bitaddr;
    aCPU->dec[0xb1] = &disasm_acall_offset;
    aCPU->dec[0xb2] = &disasm_cpl_bitaddr;
    aCPU->dec[0xb3] = &disasm_cpl_c;
    aCPU->dec[0xb4] = &disasm_cjne_a_imm_offset;
    aCPU->dec[0xb5] = &disasm_cjne_a_mem_offset;
    aCPU->dec[0xb6] = &disasm_cjne_indir_rx_imm_offset;
    aCPU->dec[0xb7] = &disasm_cjne_indir_rx_imm_offset;

    aCPU->dec[0xc0] = &disasm_push_mem;
    aCPU->dec[0xc1] = &disasm_ajmp_offset;
    aCPU->dec[0xc2] = &disasm_clr_bitaddr;
    aCPU->dec[0xc3] = &disasm_clr_c;
    aCPU->dec[0xc4] = &disasm_swap_a;
    aCPU->dec[0xc5] = &disasm_xch_a_mem;
    aCPU->dec[0xc6] = &disasm_xch_a_indir_rx;
    aCPU->dec[0xc7] = &disasm_xch_a_indir_rx;

    aCPU->dec[0xd0] = &disasm_pop_mem;
    aCPU->dec[0xd1] = &disasm_acall_offset;
    aCPU->dec[0xd2] = &disasm_setb_bitaddr;
    aCPU->dec[0xd3] = &disasm_setb_c;
    aCPU->dec[0xd4] = &disasm_da_a;
    aCPU->dec[0xd5] = &disasm_djnz_mem_offset;
    aCPU->dec[0xd6] = &disasm_xchd_a_indir_rx;
    aCPU->dec[0xd7] = &disasm_xchd_a_indir_rx;

    aCPU->dec[0xe0] = &disasm_movx_a_indir_dptr;
    aCPU->dec[0xe1] = &disasm_ajmp_offset;
    aCPU->dec[0xe2] = &disasm_movx_a_indir_rx;
    aCPU->dec[0xe3] = &disasm_movx_a_indir_rx;
    aCPU->dec[0xe4] = &disasm_clr_a;
    aCPU->dec[0xe5] = &disasm_mov_a_mem;
    aCPU->dec[0xe6] = &disasm_mov_a_indir_rx;
    aCPU->dec[0xe7] = &disasm_mov_a_indir_rx;

    aCPU->dec[0xf0] = &disasm_movx_indir_dptr_a;
    aCPU->dec[0xf1] = &disasm_acall_offset;
    aCPU->dec[0xf2] = &disasm_movx_indir_rx_a;
    aCPU->dec[0xf3] = &disasm_movx_indir_rx_a;
    aCPU->dec[0xf4] = &disasm_cpl_a;
    aCPU->dec[0xf5] = &disasm_mov_mem_a;
    aCPU->dec[0xf6] = &disasm_mov_indir_rx_a;
    aCPU->dec[0xf7] = &disasm_mov_indir_rx_a;
}