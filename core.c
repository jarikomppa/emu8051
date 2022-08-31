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
 * core.c
 * General emulation functions
 */

#define T0_MODE3_MASK (TMODMASK_M0_0 | TMODMASK_M1_0)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "emu8051.h"

static void serial_tx() {
	// Test if still something to send
	if (! CPU.serial_out_remaining_bits)
	       return;

	CPU.serial_out_remaining_bits--;
	uint8_t tx_bit = (CPU.mSFR[REG_SBUF] >> CPU.serial_out_remaining_bits) & 0x01;
	// Set P3.1 according to the currently clocked out SERIAL bit
	CPU.mSFR[REG_P3] &= ~(1 << 1);
	if (tx_bit) CPU.mSFR[REG_P3] |= (1 << 1);

	// If everything is sent now, add it to the visual buffer & raise interrupt
	if (CPU.serial_out_remaining_bits == 0) {
		CPU.serial_out[CPU.serial_out_idx] = CPU.mSFR[REG_SBUF];
		CPU.serial_out_idx = (CPU.serial_out_idx + 1) % sizeof(CPU.serial_out);
		CPU.mSFR[REG_SCON] |= (1<<1); // Set TI bit
		if (CPU.mSFR[REG_IE] & IEMASK_ES) CPU.serial_interrupt_trigger = 1; // Trigger Serial Interrupt
	}
}


static void timer_tick()
{
    uint8_t increment;
    uint16_t v;

    // TODO: External int 0 flag

    if ((CPU.mSFR[REG_TMOD] & (TMODMASK_M0_0 | TMODMASK_M1_0)) == (TMODMASK_M0_0 | TMODMASK_M1_0))
    {
        // timer/counter 0 in mode 3

        increment = 0;
        
        // Check if we're run enabled
        // TODO: also run if GATE is one and INT is one (external interrupt)
        if (!(CPU.mSFR[REG_TMOD] & TMODMASK_GATE_0) &&
            (CPU.mSFR[REG_TCON] & TCONMASK_TR0))
        {
            // check timer / counter mode
            if (CPU.mSFR[REG_TMOD] & TMODMASK_CT_0)
            {
                // counter op;
                // counter works if T0 pin was 1 and is now 0 (P3.4 on AT89C2051)
                increment = 0; // TODO
            }
            else
            {
                increment = 1;
            }
        }
        if (increment)
        {
            v = CPU.mSFR[REG_TL0];
            v++;
            CPU.mSFR[REG_TL0] = v & 0xff;
            if (v > 0xff)
            {
                // TL0 overflowed
                CPU.mSFR[REG_TCON] |= TCONMASK_TF0;
            }
        }

        increment = 0;
        
        // Check if we're run enabled
        // TODO: also run if GATE is one and INT is one (external interrupt)
        if (!(CPU.mSFR[REG_TMOD] & TMODMASK_GATE_1) &&
            (CPU.mSFR[REG_TCON] & TCONMASK_TR1))
        {
            // check timer / counter mode
            if (CPU.mSFR[REG_TMOD] & TMODMASK_CT_1)
            {
                // counter op;
                // counter works if T1 pin was 1 and is now 0
                increment = 0; // TODO
            }
            else
            {
                increment = 1;
            }
        }

        if (increment)
        {
            v = CPU.mSFR[REG_TH0];
            v++;
            CPU.mSFR[REG_TH0] = v & 0xff;
            if (v > 0xff)
            {
                // TH0 overflowed
                CPU.mSFR[REG_TCON] |= TCONMASK_TF1;
            }
        }

    }

    {   // Timer/counter 0
        
        increment = 0;
        
        // Check if we're run enabled
        // TODO: also run if GATE is one and INT is one (external interrupt)
        if (!(CPU.mSFR[REG_TMOD] & TMODMASK_GATE_0) &&
            (CPU.mSFR[REG_TCON] & TCONMASK_TR0))
        {
            // check timer / counter mode
            if (CPU.mSFR[REG_TMOD] & TMODMASK_CT_0)
            {
                // counter op;
                // counter works if T0 pin was 1 and is now 0 (P3.4 on AT89C2051)
                increment = 0; // TODO
            }
            else
            {
                increment = 1;
            }
        }
        
        if (increment)
        {
            switch (CPU.mSFR[REG_TMOD] & (TMODMASK_M0_0 | TMODMASK_M1_0))
            {
            case 0: // 13-bit timer
                v = CPU.mSFR[REG_TL0] & 0x1f; // lower 5 bits of TL0
                v++;
                CPU.mSFR[REG_TL0] = (CPU.mSFR[REG_TL0] & ~0x1f) | (v & 0x1f);
                if (v > 0x1f)
                {
                    // TL0 overflowed
                    v = CPU.mSFR[REG_TH0];
                    v++;
                    CPU.mSFR[REG_TH0] = v & 0xff;
                    if (v > 0xff)
                    {
                        // TH0 overflowed; set bit
                        CPU.mSFR[REG_TCON] |= TCONMASK_TF0;
                    }
                }
                break;
            case TMODMASK_M0_0: // 16-bit timer/counter
                v = CPU.mSFR[REG_TL0];
                v++;
                CPU.mSFR[REG_TL0] = v & 0xff;
                if (v > 0xff)
                {
                    // TL0 overflowed
                    v = CPU.mSFR[REG_TH0];
                    v++;
                    CPU.mSFR[REG_TH0] = v & 0xff;
                    if (v > 0xff)
                    {
                        // TH0 overflowed; set bit
                        CPU.mSFR[REG_TCON] |= TCONMASK_TF0;
                    }
                }
                break;
            case TMODMASK_M1_0: // 8-bit auto-reload timer
                v = CPU.mSFR[REG_TL0];
                v++;
                CPU.mSFR[REG_TL0] = v & 0xff;
                if (v > 0xff)
                {
                    // TL0 overflowed; reload
                    CPU.mSFR[REG_TL0] = CPU.mSFR[REG_TH0];
                    CPU.mSFR[REG_TCON] |= TCONMASK_TF0;
                }
                break;
            default: // two 8-bit timers
                // TODO
                break;
            }
        }
    }

    // TODO: External int 1 

    {   // Timer/counter 1 
        
        increment = 0;

        if (!(CPU.mSFR[REG_TMOD] & TMODMASK_GATE_1) &&
            (CPU.mSFR[REG_TCON] & TCONMASK_TR1))
        {
            if (CPU.mSFR[REG_TMOD] & TMODMASK_CT_1)
            {
                // counter op;
                // counter works if T1 pin was 1 and is now 0
                increment = 0; // TODO
            }
            else
            {
                increment = 1;
            }
        }

        if (increment)
        {
            switch (CPU.mSFR[REG_TMOD] & (TMODMASK_M0_1 | TMODMASK_M1_1))
            {
            case 0: // 13-bit timer
                v = CPU.mSFR[REG_TL1] & 0x1f; // lower 5 bits of TL0
                v++;
                CPU.mSFR[REG_TL1] = (CPU.mSFR[REG_TL1] & ~0x1f) | (v & 0x1f);
                if (v > 0x1f)
                {
                    // TL1 overflowed
                    v = CPU.mSFR[REG_TH1];
                    v++;
                    CPU.mSFR[REG_TH1] = v & 0xff;
                    if (v > 0xff)
                    {
                        // TH1 overflowed; set bit
                        // Only update TF1 if timer 0 is not in "mode 3"
                    if (!((CPU.mSFR[REG_TMOD] & T0_MODE3_MASK ) == T0_MODE3_MASK))
                        CPU.mSFR[REG_TCON] |= TCONMASK_TF1;

                    } 
                }
                break;
            case TMODMASK_M0_1: // 16-bit timer/counter
                v = CPU.mSFR[REG_TL1];
                v++;
                CPU.mSFR[REG_TL1] = v & 0xff;
                if (v > 0xff)
                {
                    // TL1 overflowed
                    v = CPU.mSFR[REG_TH1];
                    v++;
                    CPU.mSFR[REG_TH1] = v & 0xff;
                    if (v > 0xff)
                    {
                        // TH1 overflowed; set bit
                        // Only update TF1 if timer 0 is not in "mode 3"
              
                        if (!((CPU.mSFR[REG_TMOD] & T0_MODE3_MASK)  == T0_MODE3_MASK))
                            CPU.mSFR[REG_TCON] |= TCONMASK_TF1;

                    }
                }
                break;
            case TMODMASK_M1_1: // 8-bit auto-reload timer
                v = CPU.mSFR[REG_TL1];
                v++;
                CPU.mSFR[REG_TL1] = v & 0xff;
                if (v > 0xff)
                {
                    // TL0 overflowed; reload
                    CPU.mSFR[REG_TL1] = CPU.mSFR[REG_TH1];
                    // Only update TF1 if timer 0 is not in "mode 3"
                    
            
                    if (!((CPU.mSFR[REG_TMOD] & T0_MODE3_MASK ) == T0_MODE3_MASK))
                        CPU.mSFR[REG_TCON] |= TCONMASK_TF1;
                    
                }
                break;
            default: // disabled
                break;
            }

	    // If Timer1 overflowed, see if we need to send a serial bit
            if (CPU.mSFR[REG_TCON] & TCONMASK_TF1) {
                if (CPU.mSFR[REG_SCON] & SCONMASK_SM1) {
                    serial_tx();
		    CPU.mSFR[REG_TCON] &= ~TCONMASK_TF1; // clear overflow flag
                }
            }
        }
    }

    // TODO: serial port, timer2, other stuff
}

void handle_interrupts()
{
    int16_t dest_ip = -1;
    uint8_t hi = 0;
    uint8_t lo = 0;

    // can't interrupt high level
    if (CPU.mInterruptActive > 1)
        return;    

    if (CPU.mSFR[REG_IE] & IEMASK_EA)
    {
        // Interrupts enabled
        if (CPU.mSFR[REG_IE] & IEMASK_EX0 && CPU.mSFR[REG_TCON] & TCONMASK_IE0)
        {
            // External int 0 
            dest_ip = ISR_INT0;
            if (CPU.mSFR[REG_IP] & IPMASK_PX0)
                hi = 1;
            lo = 1;
        }
        if (CPU.mSFR[REG_IE] & IEMASK_ET0 && CPU.mSFR[REG_TCON] & TCONMASK_TF0 && !hi)
        {
            // Timer/counter 0 
            if (!lo)
            {
                dest_ip = ISR_TF0;
                lo = 1;
            }
            if (CPU.mSFR[REG_IP] & IPMASK_PT0)
            {
                hi = 1;
                dest_ip = ISR_TF0;
            }
        }
        if (CPU.mSFR[REG_IE] & IEMASK_EX1 && CPU.mSFR[REG_TCON] & TCONMASK_IE1 && !hi)
        {
            // External int 1 
            if (!lo)
            {
                dest_ip = ISR_INT1;
                lo = 1;
            }
            if (CPU.mSFR[REG_IP] & IPMASK_PX1)
            {
                hi = 1;
                dest_ip = ISR_INT1;
            }
        }
        if (CPU.mSFR[REG_IE] & IEMASK_ET1 && CPU.mSFR[REG_TCON] & TCONMASK_TF1 && !hi)
        {
            // Timer/counter 1 enabled
            if (!lo)
            {
                dest_ip = ISR_TF1;
                lo = 1;
            }
            if (CPU.mSFR[REG_IP] & IPMASK_PT1)
            {
                hi = 1;
                dest_ip = ISR_TF1;
            }
        }
        if (CPU.mSFR[REG_IE] & IEMASK_ES && CPU.serial_interrupt_trigger && !hi)
        {
            // Serial port interrupt 
            if (!lo)
            {
                dest_ip = ISR_SR;
                lo = 1;
            }
            if (CPU.mSFR[REG_IP] & IPMASK_PS)
            {
                hi = 1;
                dest_ip = ISR_SR;
            }
            // TODO
        }
#ifdef __8052__
        if (CPU.mSFR[REG_IE] & IEMASK_ET2 && !hi)
        {
            // Timer 2 (8052 only)
            if (!lo)
            {
                dest_ip = ISR_SR;
                lo = 1;
            }
            if (CPU.mSFR[REG_IP] & IPMASK_PT2)
            {
                hi = 1;
                dest_ip = ISR_SR;
            }
            // TODO
        }
#endif // __8052__
    }
    
    // no interrupt
    if (dest_ip == -1)
        return;

    // can't interrupt same-level
    if (CPU.mInterruptActive == 1 && !hi)
        return; 

    // some interrupt occurs; perform LCALL
    CPU.mSFR[REG_PCON] &= ~0x01; // clear idle flag, but not Power down flag
    push_to_stack(CPU.mPC & 0xff);
    push_to_stack(CPU.mPC >> 8);
    CPU.mPC = dest_ip;
    // wait for 2 ticks instead of one since we were not executing
    // this LCALL before.
    CPU.mTickDelay = 2;
    switch (dest_ip)
    {
    case ISR_TF0:
        CPU.mSFR[REG_TCON] &= ~TCONMASK_TF0; // clear overflow flag
        break;
    case ISR_TF1:
        CPU.mSFR[REG_TCON] &= ~TCONMASK_TF1; // clear overflow flag
        break;
    case ISR_SR:
        CPU.serial_interrupt_trigger = 0; // handled the serial interrupt trigger
        break;
    }

    if (hi)
    {
        CPU.mInterruptActive |= 2;
    }
    else
    {
        CPU.mInterruptActive = 1;
    }
    CPU.int_a[hi] = CPU.mSFR[REG_ACC];
    CPU.int_psw[hi] = CPU.mSFR[REG_PSW];
    CPU.int_sp[hi] = CPU.mSFR[REG_SP];
}

uint8_t tick()
{
    uint8_t v;
    uint8_t ticked = 0;

    if (CPU.mTickDelay)
    {
        CPU.mTickDelay--;
    }

    // Test for Power Down
    if (CPU.mTickDelay == 0 && (CPU.mSFR[REG_PCON]) & 0x02) {
        CPU.mTickDelay = 1;
        return 1;
    }

    // Interrupts are sent if the following cases are not true:
    // 1. interrupt of equal or higher priority is in progress (tested inside function)
    // 2. current cycle is not the final cycle of instruction (tickdelay = 0)
    // 3. the instruction in progress is RETI or any write to the IE or IP regs (TODO)
    if (CPU.mTickDelay == 0)
    {
        handle_interrupts();
    }

    if (CPU.mTickDelay == 0)
    {
        // IDL activate the idle mode to save power
        uint8_t is_idle = (CPU.mSFR[REG_PCON]) & 0x01;
        if (is_idle) {
            CPU.mTickDelay = 1;
        } else {
            CPU.mTickDelay = CPU.op[CPU.mCodeMem[CPU.mPC & (CPU.mCodeMemMaxIdx)]]();
        }
        ticked = 1;
        // update parity bit
        v = CPU.mSFR[REG_ACC];
        v ^= v >> 4;
        v &= 0xf;
        v = (0x6996 >> v) & 1;
        CPU.mSFR[REG_PSW] = (CPU.mSFR[REG_PSW] & ~PSWMASK_P) | (v * PSWMASK_P);
    }

    timer_tick();

    return ticked;
}

uint8_t decode(uint16_t aPosition, char *aBuffer)
{
    uint8_t is_idle = (CPU.mSFR[REG_PCON]) & 0x01;
    if (is_idle) {
        sprintf(aBuffer, "IDLE");
        return 0;
    }
    uint8_t is_powerdown = (CPU.mSFR[REG_PCON]) & 0x02;
    if (is_powerdown) {
        sprintf(aBuffer, "POWER DOWN");
        return 0;
    }
    return CPU.dec[CPU.mCodeMem[aPosition & (CPU.mCodeMemMaxIdx)]](aPosition, aBuffer);
}

void disasm_setptrs();
void op_setptrs();

void reset(uint8_t aWipe)
{
    // clear memory, set registers to bootup values, etc    
    if (aWipe)
    {
        memset(CPU.mCodeMem, 0, CPU.mCodeMemMaxIdx+1);
        memset(CPU.mExtData, 0, CPU.mExtDataMaxIdx+1);
        memset(CPU.mLowerData, 0, 128);
        if (CPU.mUpperData)
            memset(CPU.mUpperData, 0, 128);
    }

    memset(CPU.mSFR, 0, 128);

    CPU.mPC = 0;
    CPU.mTickDelay = 0;
    CPU.mSFR[REG_SP] = 7;
    CPU.mSFR[REG_P0] = 0xff;
    CPU.mSFR[REG_P1] = 0xff;
    CPU.mSFR[REG_P2] = 0xff;
    CPU.mSFR[REG_P3] = 0xff;

    // Power-off flag will be 1 only after a power on (cold reset).
    // A warm reset doesn’t affect the value of this bit
    // ... Therefore, we only set it if aWipe is 1
    if (aWipe)
        CPU.mSFR[REG_PCON] |= (1<<4);

    // Random values
    if (aWipe)
        CPU.mSFR[REG_SBUF] = rand();

    // build function pointer lists

    disasm_setptrs();
    op_setptrs();

    // Clean internal variables
    CPU.mInterruptActive = 0;

    // Clean Serial
    CPU.serial_interrupt_trigger = 0;
    CPU.serial_out_remaining_bits = 0;
}


int readbyte(FILE * f)
{
    char data[3];
    data[0] = fgetc(f);
    data[1] = fgetc(f);
    data[2] = 0;
    return strtol(data, NULL, 16);
}

int load_obj(char *aFilename)
{
    FILE *f;    
    if (aFilename == 0 || aFilename[0] == 0)
        return -1;
    f = fopen(aFilename, "r");
    if (!f) return -1;
    if (fgetc(f) != ':')
    {
    	  fclose(f);
        return -2; // unsupported file format
    }
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
            CPU.mCodeMem[address + i] = data;
        }
        i = readbyte(f);
        checksum &= 0xff;
        checksum = 256 - checksum;
        if (i != (checksum & 0xff))
            return -4; // checksum failure
        while (fgetc(f) != ':' && !feof(f)) {} // skip newline        
    }
	  fclose(f);
    return -5;
}
