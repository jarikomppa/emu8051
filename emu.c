/* 8051 emulator
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
 * CPU.c
 * Curses-based emulator front-end
 */

#ifdef _MSC_VER
#include <windows.h>
#undef MOUSE_MOVED
#else
#include <sys/time.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
#include <curses.h>
#else
#include "curses.h"
#endif
#include "emu8051.h"
#include "emulator.h"

unsigned char history[HISTORY_LINES * (128 + 64 + sizeof(int))];


// current line in the history cyclic buffers
int historyline = 0;
// last known columns and rows; for screen resize detection
int oldcols, oldrows;
// are we in single-step or run mode
int runmode = 0;
// current run speed, lower is faster
int speed = 6;

// instruction count; needed to replay history correctly
unsigned int icount = 0;

// current clock count
unsigned int clocks = 0;

// currently active view
int view = MAIN_VIEW;

// old port out values
int pout[4] = { 0 };

int breakpoint = -1;

// returns time in 1ms units
int getTick()
{
#ifdef _MSC_VER
    return GetTickCount();
#else
    struct timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec * 1000 + now.tv_usec / 1000;
#endif
}

void emu_sleep(int value)
{
#ifdef _MSC_VER
    Sleep(value);
#else
    usleep(value * 1000);
#endif
}

void setSpeed(int speed, int runmode)
{
    switch (speed)
    {
    case 7:
        slk_set(5, "+/-|.5Hz", 0);
        break;
    case 6:
        slk_set(5, "+/-|1Hz", 0);
        break;
    case 5:
        slk_set(5, "+/-|2Hz", 0);
        break;
    case 4:
        slk_set(5, "+/-|10Hz", 0);
        break;
    case 3:
        slk_set(5, "+/-|fast", 0);
        break;
    case 2:
        slk_set(5, "+/-|f+", 0);
        break;
    case 1:
        slk_set(5, "+/-|f++", 0);
        break;
    case 0:
        slk_set(5, "+/-|f*", 0);
        break;
    }

    if (runmode == 0)
    {
        slk_set(4, "r)un", 0);
        slk_refresh();
        nocbreak();
        cbreak();
        nodelay(stdscr, FALSE);
        return;
    }
    else
    {
        slk_set(4, "r)unning", 0);
        slk_refresh();
    }

    if (speed < 4)
    {
        nocbreak();
        cbreak();
        nodelay(stdscr, TRUE);
    }
    else
    {
        switch(speed)
        {
        case 7:
            halfdelay(20);
            break;
        case 6:
            halfdelay(10);
            break;
        case 5:
            halfdelay(5);
            break;
        case 4:
            halfdelay(1);
            break;
        }
    }
}


void emu_sfrwrite_SBUF(uint8_t aRegister)
{
    CPU.serial_out_remaining_bits = 8;
}

uint8_t emu_sfrread(uint8_t aRegister)
{
    int outputbyte = -1;

    if (view == LOGICBOARD_VIEW)
    {
        if (aRegister == REG_P0 + 0x80)
        {
            outputbyte = pout[0];
        }
        if (aRegister == REG_P1 + 0x80)
        {
            outputbyte = pout[1];
        }
        if (aRegister == REG_P2 + 0x80)
        {
            outputbyte = pout[2];
        }
        if (aRegister == REG_P3 + 0x80)
        {
            outputbyte = pout[3];
        }
    }
    else
    {
        if (aRegister == REG_P0 + 0x80)
        {
            outputbyte = pout[0] = emu_readvalue("P0 port read", pout[0], 2);
        }
        if (aRegister == REG_P1 + 0x80)
        {
            outputbyte = pout[1] = emu_readvalue("P1 port read", pout[1], 2);
        }
        if (aRegister == REG_P2 + 0x80)
        {
            outputbyte = pout[2] = emu_readvalue("P2 port read", pout[2], 2);
        }
        if (aRegister == REG_P3 + 0x80)
        {
            outputbyte = pout[3] = emu_readvalue("P3 port read", pout[3], 2);
        }
    }
    if (outputbyte != -1)
    {
        if (opt_input_outputlow == 1)
        {
            // option: output 1 even though ouput latch is 0
            return outputbyte;
        }
        if (opt_input_outputlow == 0)
        {
            // option: output 0 if output latch is 0
            return outputbyte & CPU.mSFR[aRegister - 0x80];
        }
        // option: dump random values for output bits with
        // output latches set to 0
        return (outputbyte & CPU.mSFR[aRegister - 0x80]) |
            (rand() & ~CPU.mSFR[aRegister - 0x80]);
    }
    return CPU.mSFR[aRegister - 0x80];

}

void refreshview()
{
    change_view(view);
}

void change_view(int changeto)
{
    switch (view)
    {
    case MAIN_VIEW:
        wipe_main_view();
        break;
    case LOGICBOARD_VIEW:
        wipe_logicboard_view();
        break;
    case MEMEDITOR_VIEW:
        wipe_memeditor_view();
        break;
    case OPTIONS_VIEW:
        wipe_options_view();
        break;
    }
    view = changeto;
    switch (view)
    {
    case MAIN_VIEW:
        build_main_view();
        break;
    case LOGICBOARD_VIEW:
        build_logicboard_view();
        break;
    case MEMEDITOR_VIEW:
        build_memeditor_view();
        break;
    case OPTIONS_VIEW:
        build_options_view();
        break;
    }
}

int main(int parc, char ** pars)
{
    int ch = 0;
    int i;
    int ticked = 1;

    memset(&CPU, 0, sizeof(CPU));
    CPU.mCodeMemMaxIdx = 65536-1;
    CPU.mCodeMem     = calloc(CPU.mCodeMemMaxIdx+1, sizeof(unsigned char));
    CPU.mExtDataMaxIdx = 65536-1;
    CPU.mExtData     = calloc(CPU.mExtDataMaxIdx+1, sizeof(unsigned char));
    CPU.mUpperData   = calloc(128, sizeof(unsigned char));
    CPU.except       = &emu_exception;
    CPU.xread = NULL;
    CPU.xwrite = NULL;

    CPU.sfrwrite[REG_SBUF] = emu_sfrwrite_SBUF;

    CPU.sfrread[REG_P0] = emu_sfrread;
    CPU.sfrread[REG_P1] = emu_sfrread;
    CPU.sfrread[REG_P2] = emu_sfrread;
    CPU.sfrread[REG_P3] = emu_sfrread;

    reset(1);

    if (parc > 1)
    {
        for (i = 1; i < parc; i++)
        {
            if (pars[i][0] == '-' || pars[i][0] == '/')
            {
                if (strcmp("step_instruction",pars[i]+1) == 0)
                {
                    opt_step_instruction = 1;
                }
                else
                if (strcmp("si",pars[i]+1) == 0)
                {
                    opt_step_instruction = 1;
                }
                else
                if (strcmp("noexc_iret_sp",pars[i]+1) == 0)
                {
                    opt_exception_iret_sp = 0;
                }
                else
                if (strcmp("nosp",pars[i]+1) == 0)
                {
                    opt_exception_iret_sp = 0;
                }
                else
                if (strcmp("noexc_iret_acc",pars[i]+1) == 0)
                {
                    opt_exception_iret_acc = 0;
                }
                else
                if (strcmp("noacc",pars[i]+1) == 0)
                {
                    opt_exception_iret_acc = 0;
                }
                else
                if (strcmp("noexc_iret_psw",pars[i]+1) == 0)
                {
                    opt_exception_iret_psw = 0;
                }
                else
                if (strcmp("nopsw",pars[i]+1) == 0)
                {
                    opt_exception_iret_psw = 0;
                }
                else
                if (strcmp("noexc_acc_to_a",pars[i]+1) == 0)
                {
                    opt_exception_acc_to_a = 0;
                }
                else
                if (strcmp("noaa",pars[i]+1) == 0)
                {
                    opt_exception_acc_to_a = 0;
                }
                else
                if (strcmp("noexc_stack",pars[i]+1) == 0)
                {
                    opt_exception_stack = 0;
                }
                else
                if (strcmp("nostk",pars[i]+1) == 0)
                {
                    opt_exception_stack = 0;
                }
                else
                if (strcmp("noexc_invalid_op",pars[i]+1) == 0)
                {
                    opt_exception_invalid = 0;
                }
                else
                if (strcmp("noiop",pars[i]+1) == 0)
                {
                    opt_exception_invalid = 0;
                }
                else
                if (strcmp("iolowlow",pars[i]+1) == 0)
                {
                    opt_input_outputlow = 0;
                }
                else
                if (strcmp("iolowrand",pars[i]+1) == 0)
                {
                    opt_input_outputlow = 2;
                }
                else
                if (strncmp("clock=",pars[i]+1,6) == 0)
                {
                    opt_clock_select = 12;
                    opt_clock_hz = atoi(pars[i]+7);
                    if (opt_clock_hz <= 0)
                        opt_clock_hz = 1;
                }
                else
                {
                    printf("Help:\n\n"
                        "emu8051 [options] [filename]\n\n"
                        "Both the filename and options are optional. Available options:\n\n"
                        "Option            Alternate   description\n"
                        "-step_instruction -si         Step one instruction at a time\n"
                        "-noexc_iret_sp    -nosp       Disable sp iret exception\n"
                        "-noexc_iret_acc   -noacc      Disable acc iret exception\n"
                        "-noexc_iret_psw   -nopsw      Disable pdw iret exception\n"
                        "-noexc_acc_to_a   -noaa       Disable acc-to-a invalid instruction exception\n"
                        "-noexc_stack      -nostk      Disable stack abnormal behaviour exception\n"
                        "-noexc_invalid_op -noiop      Disable invalid opcode exception\n"
                        "-iolowlow         If out pin is low, hi input from same pin is low\n"
                        "-iolowrand        If out pin is low, hi input from same pin is random\n"
                        "-clock=value      Set clock speed, in Hz\n"
                        );
                    return -1;
                }
            }
            else
            {
                if (load_obj(pars[i]) != 0)
                {
                    printf("File '%s' load failure\n\n",pars[i]);
                    return -1;
                }
                else
                {
                    strcpy(filename, pars[i]);
                }
            }
        }
    }

    //  Initialize ncurses

    slk_init(1);
    if ( (initscr()) == NULL ) {
	    fprintf(stderr, "Error initialising ncurses.\n");
	    exit(EXIT_FAILURE);
    }

    slk_set(1, "h)elp", 0);
    slk_set(2, "l)oad", 0);
    slk_set(3, "spc=step", 0);
    slk_set(4, "r)un", 0);
    slk_set(6, "v)iew", 0);
    slk_set(7, "home=rst", 0);
    slk_set(8, "s-Q)quit", 0);
    setSpeed(speed, runmode);


    //  Switch of echoing and enable keypad (for arrow keys etc)

    cbreak(); // no buffering
    noecho(); // no echoing
    keypad(stdscr, TRUE); // cursors entered as single characters

    build_main_view();

    // Loop until user hits 'shift-Q'

    do
    {
        if (LINES != oldrows ||
            COLS != oldcols)
        {
            refreshview();
        }
        switch (ch)
        {
        case KEY_F(1):
            change_view(0);
            break;
        case KEY_F(2):
            change_view(1);
            break;
        case KEY_F(3):
            change_view(2);
            break;
        case KEY_F(4):
            change_view(3);
            break;
        case 'v':
            change_view((view + 1) % 4);
            break;
        case 'k':
            if (breakpoint != -1)
            {
                breakpoint = -1;
                emu_popup("Breakpoint", "Breakpoint cleared.");
            }
            else
            {
                breakpoint = emu_readvalue("Set Breakpoint", CPU.mPC, 4);
            }
            break;
        case 'g':
            CPU.mPC = emu_readvalue("Set Program Counter", CPU.mPC, 4);
            break;
        case 'h':
            emu_help();
            break;
        case 'l':
            emu_load();
            break;
        case ' ':
            runmode = 0;
            setSpeed(speed, runmode);
            break;
        case 'r':
            if (runmode)
            {
                runmode = 0;
                setSpeed(speed, runmode);
            }
            else
            {
                runmode = 1;
                setSpeed(speed, runmode);
            }
            break;
#ifdef __PDCURSES__
        case PADPLUS:
#endif
        case '+':
            speed--;
            if (speed < 0)
                speed = 0;
            setSpeed(speed, runmode);
            break;
#ifdef __PDCURSES__
        case PADMINUS:
#endif
        case '-':
            speed++;
            if (speed > 7)
                speed = 7;
            setSpeed(speed, runmode);
            break;
        case KEY_HOME:
            if (emu_reset())
            {
                clocks = 0;
                ticked = 1;
            }
            break;
        case 'z':
	    // Equivalent of "R)eset (init regs, set PC to zero)"
	    reset(0);
	    break;
        case 'Z':
	    // Equivalent of "W)ipe (init regs, set PC to zero, clear memory)"
	    reset(1);
	    break;
        case KEY_END:
            clocks = 0;
            ticked = 1;
            break;
        default:
            // by default, send keys to the current view
            switch (view)
            {
            case MAIN_VIEW:
                mainview_editor_keys(ch);
                break;
            case LOGICBOARD_VIEW:
                logicboard_editor_keys(ch);
                break;
            case MEMEDITOR_VIEW:
                memeditor_editor_keys(ch);
                break;
            case OPTIONS_VIEW:
                options_editor_keys(ch);
                break;
            }
            break;
        }

        if (ch == 32 || runmode)
        {
            int targettime;
            unsigned int targetclocks;
            targetclocks = 1;
            targettime = getTick();

            if (speed == 2 && runmode)
            {
                targettime += 1;
                targetclocks += (opt_clock_hz / 12000) - 1;
            }
            if (speed < 2 && runmode)
            {
                targettime += 10;
                targetclocks += (opt_clock_hz / 1200) - 1;
            }

            do
            {
                int old_pc;
                old_pc = CPU.mPC;
                if (opt_step_instruction)
                {
                    ticked = 0;
                    while (!ticked)
                    {
                        targetclocks--;
                        clocks += 12;
                        ticked = tick();
                        logicboard_tick();
                    }
                }
                else
                {
                    targetclocks--;
                    clocks += 12;
                    ticked = tick();
                    logicboard_tick();
                }

                if (CPU.mPC == breakpoint)
                    emu_exception(-1);

                if (ticked)
                {
                    icount++;

                    historyline = (historyline + 1) % HISTORY_LINES;

                    memcpy(history + (historyline * (128 + 64 + sizeof(int))), CPU.mSFR, 128);
                    memcpy(history + (historyline * (128 + 64 + sizeof(int))) + 128, CPU.mLowerData, 64);
                    memcpy(history + (historyline * (128 + 64 + sizeof(int))) + 128 + 64, &old_pc, sizeof(int));
                }
            }
            while (targettime > getTick() && targetclocks > 0);

            while (targettime > getTick())
            {
                emu_sleep(1);
            }
        }

        switch (view)
        {
        case MAIN_VIEW:
            mainview_update();
            break;
        case LOGICBOARD_VIEW:
            logicboard_update();
            break;
        case MEMEDITOR_VIEW:
            memeditor_update();
            break;
        case OPTIONS_VIEW:
            options_update();
            break;
        }
    }
    while ( (ch = getch()) != 'Q' );

    endwin();

    return EXIT_SUCCESS;
}
