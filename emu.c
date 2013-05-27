/* 8051 emulator 
 * Copyright 2006 Jari Komppa
 * Released under GPL
 *
 * emu.c
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
#include "curses.h"
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
int p0out = 0;
int p1out = 0;
int p2out = 0;
int p3out = 0;

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

    slk_refresh();        
    if (runmode == 0)
    {
        nocbreak();
        cbreak();
        nodelay(stdscr, FALSE);        
        return;
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




int emu_sfrread(struct em8051 *aCPU, int aRegister)
{
    if (view == LOGICBOARD_VIEW)
    {
        if (aRegister == REG_P0 + 0x80)
            return p0out;
        if (aRegister == REG_P1 + 0x80)
            return p1out;
        if (aRegister == REG_P2 + 0x80)
            return p2out;
        if (aRegister == REG_P3 + 0x80)
            return p3out;
    }
    else
    {
        if (aRegister == REG_P0 + 0x80)
        {
            return p0out = emu_readvalue(aCPU, "P0 port read", p0out, 2);
        }
        if (aRegister == REG_P1 + 0x80)
        {
            return p1out = emu_readvalue(aCPU, "P1 port read", p1out, 2);
        }
        if (aRegister == REG_P2 + 0x80)
        {
            return p2out = emu_readvalue(aCPU, "P2 port read", p2out, 2);
        }
        if (aRegister == REG_P3 + 0x80)
        {
            return p3out = emu_readvalue(aCPU, "P3 port read", p3out, 2);
        }
    }
    return aCPU->mSFR[aRegister - 0x80];
    
}

void refreshview(struct em8051 *aCPU)
{
    change_view(aCPU, view);
}

void change_view(struct em8051 *aCPU, int changeto)
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
        build_main_view(aCPU);
        break;
    case LOGICBOARD_VIEW:
        build_logicboard_view(aCPU);
        break;
    case MEMEDITOR_VIEW:
        build_memeditor_view(aCPU);
        break;
    case OPTIONS_VIEW:
        build_options_view(aCPU);
        break;
    }
}

int main(int parc, char ** pars) 
{
    int ch = 0;
    struct em8051 emu;
    int i;
    int ticked = 1;

    memset(&emu, 0, sizeof(emu));
    emu.mCodeMem     = malloc(65536);
    emu.mCodeMemSize = 65536;
    emu.mExtData     = malloc(65536);
    emu.mExtDataSize = 65536;
    emu.mLowerData   = malloc(128);
    emu.mUpperData   = malloc(128);
    emu.mSFR         = malloc(128);
    emu.except       = &emu_exception;
    emu.sfrread      = &emu_sfrread;
    reset(&emu, 1);
    i = 0x100;

    if (parc > 1)
    {
        if (load_obj(&emu, pars[1]) != 0)
        {
            printf("File '%s' load failure\n\n",pars[1]);
            return -1;
        }
        else
        {
            strcpy(filename, pars[1]);
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

    build_main_view(&emu);

    // Loop until user hits 'shift-Q'

    do
    {
        if (LINES != oldrows ||
            COLS != oldcols)
        {
            refreshview(&emu);
        }
        switch (ch)
        {
        case 'v':
            change_view(&emu, (view + 1) % 4);
            break;
        case 'k':
            if (breakpoint != -1)
            {
                breakpoint = -1;
                emu_popup(&emu, "Breakpoint", "Breakpoint cleared.");
            }
            else
            {
                breakpoint = emu_readvalue(&emu, "Set Breakpoint", emu.mPC, 4);
            }
            break;
        case 'g':
            emu.mPC = emu_readvalue(&emu, "Set Program Counter", emu.mPC, 4);
            break;
        case 'h':
            emu_help(&emu);
            break;
        case 'l':
            emu_load(&emu);
            break;
        case ' ':
            runmode = 0;
            nocbreak();
            cbreak();
            nodelay(stdscr, FALSE);
            break;
        case 'r':
            if (runmode)
            {
                runmode = 0;
                nocbreak();
                cbreak();
                nodelay(stdscr, FALSE);
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
        case KEY_NEXT:
        case '\t':
        case KEY_NPAGE:
        case KEY_PPAGE:
        case KEY_LEFT:
        case KEY_RIGHT:
        case KEY_DOWN:
        case KEY_UP:
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case '0':
        case 'a':
        case 'A':
        case 'b':
        case 'B':
        case 'c':
        case 'C':
        case 'd':
        case 'D':
        case 'e':
        case 'E':
        case 'f':
        case 'F':
            switch (view)
            {
            case MAIN_VIEW:
                mainview_editor_keys(&emu, ch);
                break;
            case LOGICBOARD_VIEW:
                logicboard_editor_keys(&emu, ch);
                break;
            case MEMEDITOR_VIEW:
                memeditor_editor_keys(&emu, ch);
                break;
            case OPTIONS_VIEW:
                options_editor_keys(&emu, ch);
                break;
            }
            break;
        case KEY_HOME:
            if (emu_reset(&emu))
            {
                clocks = 0;
                ticked = 1;
            }
            break;
        case KEY_END:
            {
                clocks = 0;
                ticked = 1;
            }
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
                targetclocks += (opt_clock_hz / 1000) - 1;
            }
            if (speed < 2 && runmode)
            {
                targettime += 10;
                targetclocks += (opt_clock_hz / 100) - 1;
            }

            do
            {
                int old_pc;
                old_pc = emu.mPC;
                clocks += 12;
                ticked = tick(&emu);
                logicboard_tick(&emu);

                if (emu.mPC == breakpoint)
                    emu_exception(&emu, -1);

                if (ticked)
                {
                    icount++;

                    historyline = (historyline + 1) % HISTORY_LINES;

                    memcpy(history + (historyline * (128 + 64 + sizeof(int))), emu.mSFR, 128);
                    memcpy(history + (historyline * (128 + 64 + sizeof(int))) + 128, emu.mLowerData, 64);
                    memcpy(history + (historyline * (128 + 64 + sizeof(int))) + 128 + 64, &old_pc, sizeof(int));
                }
                targetclocks--;
            }
            while (targettime > getTick() && targetclocks);

            while (targettime > getTick())
            {
                emu_sleep(1);
            }
        }

        switch (view)
        {
        case MAIN_VIEW:
            mainview_update(&emu);
            break;
        case LOGICBOARD_VIEW:
            logicboard_update(&emu);
            break;
        case MEMEDITOR_VIEW:
            memeditor_update(&emu);
            break;
        case OPTIONS_VIEW:
            options_update(&emu);
            break;
        }
    }
    while ( (ch = getch()) != 'Q' );

    endwin();

    return EXIT_SUCCESS;
}