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

#include <stdlib.h>
#include <string.h>
#include "emu8051.h"

static void serial_tx(struct em8051 *aCPU) {
	// Test if still something to send
	if (! aCPU->serial_out_remaining_bits)
	       return;

	aCPU->serial_out_remaining_bits--;
	bool tx_bit = (aCPU->mSFR[REG_SBUF] >> aCPU->serial_out_remaining_bits);
	// Set P3.1 according to the currently clocked out SERIAL bit
	aCPU->mSFR[REG_P3] &= ~(1 << 1);
	if (tx_bit) aCPU->mSFR[REG_P3] |= (1 << 1);

	// If everything is sent now, add it to the visual buffer & raise interrupt
	if (aCPU->serial_out_remaining_bits == 0) {
		aCPU->serial_out[aCPU->serial_out_idx] = aCPU->mSFR[REG_SBUF];
		aCPU->serial_out_idx = (aCPU->serial_out_idx + 1) % sizeof(aCPU->serial_out);
		aCPU->mSFR[REG_SCON] |= (1<<1); // Set TI bit
		if (aCPU->mSFR[REG_IE] & IEMASK_ES) aCPU->serial_interrupt_trigger = 1; // Trigger Serial Interrupt
	}
}


static void timer_tick(struct em8051 *aCPU)
{
    uint8_t increment;
    uint16_t v;

    // TODO: External int 0 flag

    if ((aCPU->mSFR[REG_TMOD] & (TMODMASK_M0_0 | TMODMASK_M1_0)) == (TMODMASK_M0_0 | TMODMASK_M1_0))
    {
        // timer/counter 0 in mode 3

        increment = 0;
        
        // Check if we're run enabled
        // TODO: also run if GATE is one and INT is one (external interrupt)
        if (!(aCPU->mSFR[REG_TMOD] & TMODMASK_GATE_0) && 
            (aCPU->mSFR[REG_TCON] & TCONMASK_TR0))
        {
            // check timer / counter mode
            if (aCPU->mSFR[REG_TMOD] & TMODMASK_CT_0)
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
            v = aCPU->mSFR[REG_TL0];
            v++;
            aCPU->mSFR[REG_TL0] = v & 0xff;
            if (v > 0xff)
            {
                // TL0 overflowed
                aCPU->mSFR[REG_TCON] |= TCONMASK_TF0;
            }
        }

        increment = 0;
        
        // Check if we're run enabled
        // TODO: also run if GATE is one and INT is one (external interrupt)
        if (!(aCPU->mSFR[REG_TMOD] & TMODMASK_GATE_1) && 
            (aCPU->mSFR[REG_TCON] & TCONMASK_TR1))
        {
            // check timer / counter mode
            if (aCPU->mSFR[REG_TMOD] & TMODMASK_CT_1)
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
            v = aCPU->mSFR[REG_TH0];
            v++;
            aCPU->mSFR[REG_TH0] = v & 0xff;
            if (v > 0xff)
            {
                // TH0 overflowed
                aCPU->mSFR[REG_TCON] |= TCONMASK_TF1;
            }
        }

    }

    {   // Timer/counter 0
        
        increment = 0;
        
        // Check if we're run enabled
        // TODO: also run if GATE is one and INT is one (external interrupt)
        if (!(aCPU->mSFR[REG_TMOD] & TMODMASK_GATE_0) && 
            (aCPU->mSFR[REG_TCON] & TCONMASK_TR0))
        {
            // check timer / counter mode
            if (aCPU->mSFR[REG_TMOD] & TMODMASK_CT_0)
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
            switch (aCPU->mSFR[REG_TMOD] & (TMODMASK_M0_0 | TMODMASK_M1_0))
            {
            case 0: // 13-bit timer
                v = aCPU->mSFR[REG_TL0] & 0x1f; // lower 5 bits of TL0
                v++;
                aCPU->mSFR[REG_TL0] = (aCPU->mSFR[REG_TL0] & ~0x1f) | (v & 0x1f);
                if (v > 0x1f)
                {
                    // TL0 overflowed
                    v = aCPU->mSFR[REG_TH0];
                    v++;
                    aCPU->mSFR[REG_TH0] = v & 0xff;
                    if (v > 0xff)
                    {
                        // TH0 overflowed; set bit
                        aCPU->mSFR[REG_TCON] |= TCONMASK_TF0;
                    }
                }
                break;
            case TMODMASK_M0_0: // 16-bit timer/counter
                v = aCPU->mSFR[REG_TL0];
                v++;
                aCPU->mSFR[REG_TL0] = v & 0xff;
                if (v > 0xff)
                {
                    // TL0 overflowed
                    v = aCPU->mSFR[REG_TH0];
                    v++;
                    aCPU->mSFR[REG_TH0] = v & 0xff;
                    if (v > 0xff)
                    {
                        // TH0 overflowed; set bit
                        aCPU->mSFR[REG_TCON] |= TCONMASK_TF0;
                    }
                }
                break;
            case TMODMASK_M1_0: // 8-bit auto-reload timer
                v = aCPU->mSFR[REG_TL0];
                v++;
                aCPU->mSFR[REG_TL0] = v & 0xff;
                if (v > 0xff)
                {
                    // TL0 overflowed; reload
                    aCPU->mSFR[REG_TL0] = aCPU->mSFR[REG_TH0];
                    aCPU->mSFR[REG_TCON] |= TCONMASK_TF0;
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

        if (!(aCPU->mSFR[REG_TMOD] & TMODMASK_GATE_1) && 
            (aCPU->mSFR[REG_TCON] & TCONMASK_TR1))
        {
            if (aCPU->mSFR[REG_TMOD] & TMODMASK_CT_1)
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
            switch (aCPU->mSFR[REG_TMOD] & (TMODMASK_M0_1 | TMODMASK_M1_1))
            {
            case 0: // 13-bit timer
                v = aCPU->mSFR[REG_TL1] & 0x1f; // lower 5 bits of TL0
                v++;
                aCPU->mSFR[REG_TL1] = (aCPU->mSFR[REG_TL1] & ~0x1f) | (v & 0x1f);
                if (v > 0x1f)
                {
                    // TL1 overflowed
                    v = aCPU->mSFR[REG_TH1];
                    v++;
                    aCPU->mSFR[REG_TH1] = v & 0xff;
                    if (v > 0xff)
                    {
                        // TH1 overflowed; set bit
                        // Only update TF1 if timer 0 is not in "mode 3"
                    if (!((aCPU->mSFR[REG_TMOD] & T0_MODE3_MASK ) == T0_MODE3_MASK))
                        aCPU->mSFR[REG_TCON] |= TCONMASK_TF1;

                    } 
                }
                break;
            case TMODMASK_M0_1: // 16-bit timer/counter
                v = aCPU->mSFR[REG_TL1];
                v++;
                aCPU->mSFR[REG_TL1] = v & 0xff;
                if (v > 0xff)
                {
                    // TL1 overflowed
                    v = aCPU->mSFR[REG_TH1];
                    v++;
                    aCPU->mSFR[REG_TH1] = v & 0xff;
                    if (v > 0xff)
                    {
                        // TH1 overflowed; set bit
                        // Only update TF1 if timer 0 is not in "mode 3"
              
                        if (!((aCPU->mSFR[REG_TMOD] & T0_MODE3_MASK)  == T0_MODE3_MASK))
                            aCPU->mSFR[REG_TCON] |= TCONMASK_TF1;

                    }
                }
                break;
            case TMODMASK_M1_1: // 8-bit auto-reload timer
                v = aCPU->mSFR[REG_TL1];
                v++;
                aCPU->mSFR[REG_TL1] = v & 0xff;
                if (v > 0xff)
                {
                    // TL0 overflowed; reload
                    aCPU->mSFR[REG_TL1] = aCPU->mSFR[REG_TH1];
                    // Only update TF1 if timer 0 is not in "mode 3"
                    
            
                    if (!((aCPU->mSFR[REG_TMOD] & T0_MODE3_MASK ) == T0_MODE3_MASK))
                        aCPU->mSFR[REG_TCON] |= TCONMASK_TF1;
                    
                }
                break;
            default: // disabled
                break;
            }

	    // If Timer1 overflowed, see if we need to send a serial bit
            if (aCPU->mSFR[REG_TCON] & TCONMASK_TF1) {
                if (aCPU->mSFR[REG_SCON] & SCONMASK_SM1) {
                    serial_tx(aCPU);
		    aCPU->mSFR[REG_TCON] &= ~TCONMASK_TF1; // clear overflow flag
                }
            }
        }
    }

    // TODO: serial port, timer2, other stuff
}

void handle_interrupts(struct em8051 *aCPU)
{
    int16_t dest_ip = -1;
    uint8_t hi = 0;
    uint8_t lo = 0;

    // can't interrupt high level
    if (aCPU->mInterruptActive > 1) 
        return;    

    if (aCPU->mSFR[REG_IE] & IEMASK_EA)
    {
        // Interrupts enabled
        if (aCPU->mSFR[REG_IE] & IEMASK_EX0 && aCPU->mSFR[REG_TCON] & TCONMASK_IE0)
        {
            // External int 0 
            dest_ip = ISR_INT0;
            if (aCPU->mSFR[REG_IP] & IPMASK_PX0)
                hi = 1;
            lo = 1;
        }
        if (aCPU->mSFR[REG_IE] & IEMASK_ET0 && aCPU->mSFR[REG_TCON] & TCONMASK_TF0 && !hi)
        {
            // Timer/counter 0 
            if (!lo)
            {
                dest_ip = ISR_TF0;
                lo = 1;
            }
            if (aCPU->mSFR[REG_IP] & IPMASK_PT0)
            {
                hi = 1;
                dest_ip = ISR_TF0;
            }
        }
        if (aCPU->mSFR[REG_IE] & IEMASK_EX1 && aCPU->mSFR[REG_TCON] & TCONMASK_IE1 && !hi)
        {
            // External int 1 
            if (!lo)
            {
                dest_ip = ISR_INT1;
                lo = 1;
            }
            if (aCPU->mSFR[REG_IP] & IPMASK_PX1)
            {
                hi = 1;
                dest_ip = ISR_INT1;
            }
        }
        if (aCPU->mSFR[REG_IE] & IEMASK_ET1 && aCPU->mSFR[REG_TCON] & TCONMASK_TF1 && !hi)
        {
            // Timer/counter 1 enabled
            if (!lo)
            {
                dest_ip = ISR_TF1;
                lo = 1;
            }
            if (aCPU->mSFR[REG_IP] & IPMASK_PT1)
            {
                hi = 1;
                dest_ip = ISR_TF1;
            }
        }
        if (aCPU->mSFR[REG_IE] & IEMASK_ES && aCPU->serial_interrupt_trigger && !hi)
        {
            // Serial port interrupt 
            if (!lo)
            {
                dest_ip = ISR_SR;
                lo = 1;
            }
            if (aCPU->mSFR[REG_IP] & IPMASK_PS)
            {
                hi = 1;
                dest_ip = ISR_SR;
            }
            // TODO
        }
#ifdef __8052__
        if (aCPU->mSFR[REG_IE] & IEMASK_ET2 && !hi)
        {
            // Timer 2 (8052 only)
            if (!lo)
            {
                dest_ip = ISR_SR;
                lo = 1;
            }
            if (aCPU->mSFR[REG_IP] & IPMASK_PT2)
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
    if (aCPU->mInterruptActive == 1 && !hi)
        return; 

    // some interrupt occurs; perform LCALL
    aCPU->mSFR[REG_PCON] &= ~0x01; // clear idle flag, but not Power down flag
    push_to_stack(aCPU, aCPU->mPC & 0xff);
    push_to_stack(aCPU, aCPU->mPC >> 8);
    aCPU->mPC = dest_ip;
    // wait for 2 ticks instead of one since we were not executing
    // this LCALL before.
    aCPU->mTickDelay = 2;
    switch (dest_ip)
    {
    case ISR_TF0:
        aCPU->mSFR[REG_TCON] &= ~TCONMASK_TF0; // clear overflow flag
        break;
    case ISR_TF1:
        aCPU->mSFR[REG_TCON] &= ~TCONMASK_TF1; // clear overflow flag
        break;
    case ISR_SR:
        aCPU->serial_interrupt_trigger = 0; // handled the serial interrupt trigger
        break;
    }

    if (hi)
    {
        aCPU->mInterruptActive |= 2;
    }
    else
    {
        aCPU->mInterruptActive = 1;
    }
    aCPU->int_a[hi] = aCPU->mSFR[REG_ACC];
    aCPU->int_psw[hi] = aCPU->mSFR[REG_PSW];
    aCPU->int_sp[hi] = aCPU->mSFR[REG_SP];
}

bool tick(struct em8051 *aCPU)
{
    uint8_t v;
    bool ticked = false;

    if (aCPU->mTickDelay)
    {
        aCPU->mTickDelay--;
    }

    // Test for Power Down
    if (aCPU->mTickDelay == 0 && (aCPU->mSFR[REG_PCON]) & 0x02) {
        aCPU->mTickDelay = 1;
        return 1;
    }

    // Interrupts are sent if the following cases are not true:
    // 1. interrupt of equal or higher priority is in progress (tested inside function)
    // 2. current cycle is not the final cycle of instruction (tickdelay = 0)
    // 3. the instruction in progress is RETI or any write to the IE or IP regs (TODO)
    if (aCPU->mTickDelay == 0)
    {
        handle_interrupts(aCPU);
    }

    if (aCPU->mTickDelay == 0)
    {
        // IDL activate the idle mode to save power
        bool is_idle = (aCPU->mSFR[REG_PCON]) & 0x01;
        if (is_idle) {
            aCPU->mTickDelay = 1;
        } else {
            aCPU->mTickDelay = aCPU->op[aCPU->mCodeMem[aCPU->mPC & (aCPU->mCodeMemMaxIdx)]](aCPU);
        }
        ticked = true;
        // update parity bit
        v = aCPU->mSFR[REG_ACC];
        v ^= v >> 4;
        v &= 0xf;
        v = (0x6996 >> v) & 1;
        aCPU->mSFR[REG_PSW] = (aCPU->mSFR[REG_PSW] & ~PSWMASK_P) | (v * PSWMASK_P);
    }

    timer_tick(aCPU);

    return ticked;
}

uint8_t decode(struct em8051 *aCPU, uint16_t aPosition, char *aBuffer)
{
    bool is_idle = (aCPU->mSFR[REG_PCON]) & 0x01;
    if (is_idle) {
        strcpy(aBuffer, "IDLE");
        return 0;
    }
    bool is_powerdown = (aCPU->mSFR[REG_PCON]) & 0x02;
    if (is_powerdown) {
        strcpy(aBuffer, "POWER DOWN");
        return 0;
    }
    return aCPU->dec[aCPU->mCodeMem[aPosition & (aCPU->mCodeMemMaxIdx)]](aCPU, aPosition, aBuffer);
}

void disasm_setptrs(struct em8051 *aCPU);
void op_setptrs(struct em8051 *aCPU);

void reset(struct em8051 *aCPU, bool aWipe)
{
    // clear memory, set registers to bootup values, etc    
    if (aWipe)
    {
        memset(aCPU->mCodeMem, 0, aCPU->mCodeMemMaxIdx+1);
        memset(aCPU->mExtData, 0, aCPU->mExtDataMaxIdx+1);
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

    // Power-off flag will be 1 only after a power on (cold reset).
    // A warm reset doesn’t affect the value of this bit
    // ... Therefore, we only set it if aWipe is 1
    if (aWipe)
        aCPU->mSFR[REG_PCON] |= (1<<4);

    // Random values
    if (aWipe)
        aCPU->mSFR[REG_SBUF] = rand();

    // build function pointer lists

    disasm_setptrs(aCPU);
    op_setptrs(aCPU);

    // Clean internal variables
    aCPU->mInterruptActive = 0;

    // Clean Serial
    aCPU->serial_interrupt_trigger = 0;
    aCPU->serial_out_remaining_bits = 0;
}
