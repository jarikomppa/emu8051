/* 8051 emulator 
 * Copyright 2006 Jari Komppa
 * Released under GPL
 *
 * emu.c
 * Curses-based emulator front-end
 */

/*
short term TODO:
- refactor this mess
*/

#ifdef _MSC_VER
#include <windows.h>
#else
#include <sys/time.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "curses.h"
#include "emu8051.h"
#include "emulator.h"

// code box (PC, opcode, assembly)
WINDOW *codebox = NULL, *codeoutput = NULL;
char *codelines[HISTORY_LINES];

// Registers box (a, r0..7, b, dptr)
WINDOW *regbox = NULL, *regoutput = NULL;
char *reglines[HISTORY_LINES];

// RAM view/edit box
WINDOW *rambox = NULL, *ramview = NULL;

// stack view
WINDOW *stackbox = NULL, *stackview = NULL;

// program status word box
WINDOW *pswbox = NULL, *pswoutput = NULL;
char *pswlines[HISTORY_LINES];

// IO registers box
WINDOW *ioregbox = NULL, *ioregoutput = NULL;
char *ioreglines[HISTORY_LINES];

// special registers box
WINDOW *spregbox = NULL, *spregoutput = NULL;
char *spreglines[HISTORY_LINES];

// misc. stuff box
WINDOW *miscbox = NULL, *miscview = NULL;

// store the object filename
char filename[256];

// current line in the history cyclic buffers
int historyline = 0;
// last known columns and rows; for screen resize detection
int oldcols, oldrows;
// are we in single-step or run mode
int runmode = 0;
// current run speed, lower is faster
int speed = 6;
// focused component
int focus = 0;
// memory window cursor position
int memcursorpos = 0;
// memory window offset
int memoffset = 0;
// pointer to the memory area being viewed
unsigned char *memarea = NULL;

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
    }
}

int main(int parc, char ** pars) 
{
    char assembly[256];
    int ch = 0;
    struct em8051 emu;
    int i;
    int ticked = 1;

    for (i = 0; i < HISTORY_LINES; i++)
    {
        codelines[i] = malloc(128);
        strcpy(codelines[i],"\n");
        reglines[i] = malloc(128);
        strcpy(reglines[i],"\n");
        pswlines[i] = malloc(128);
        strcpy(pswlines[i],"\n");
        ioreglines[i] = malloc(128);
        strcpy(ioreglines[i],"\n");
        spreglines[i] = malloc(128);
        strcpy(spreglines[i],"\n");
    }

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
    memarea = emu.mLowerData;
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

    /*  Initialize ncurses  */

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
   

    /*  Switch of echoing and enable keypad (for arrow keys)  */

    cbreak(); // no buffering
    noecho(); // no echoing
    keypad(stdscr, TRUE); // cursors entered as single characters

    build_main_view();

    /*  Loop until user hits 'shift-Q' to quit  */

    do
    {
        if (LINES != oldrows ||
            COLS != oldcols)
        {
            refreshview();
        }
        switch (ch)
        {
        case 'v':
            change_view((view + 1) % 2);
            break;
        case '\t':
            memcursorpos = 0;
            memoffset = 0;
            focus++;
            if (focus == 1 && emu.mUpperData == NULL) 
                focus++;
            if (focus == 3 && emu.mExtDataSize == 0)
                focus++;
            if (focus == 5)
                focus = 0;
            switch (focus)
            {
            case 0:
                memarea = emu.mLowerData;
                break;
            case 1:
                memarea = emu.mUpperData;
                break;
            case 2:
                memarea = emu.mSFR;
                break;
            case 3:
                memarea = emu.mExtData;
                break;
            case 4:
                memarea = emu.mCodeMem;
                break;
            }
            break;
        case 'k':
            if (breakpoint != -1)
            {
                breakpoint = -1;
                emu_popup("Breakpoint", "Breakpoint cleared.");
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
            emu_help();
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
            int opcode_bytes;
            int stringpos;
            int targettime;
            unsigned int targetclocks;
            int rx;
            int i;
            targetclocks = clocks;
            targettime = getTick();
            if (speed < 2 && runmode)
            {
                targettime += 10;
                targetclocks += 120000;
            }

            do
            {
                int old_pc;
                old_pc = emu.mPC;
                clocks += 12;
                ticked = tick(&emu);
                if (emu.mPC == breakpoint)
                    emu_exception(&emu, -1);
                if (ticked && (speed != 0 || runmode == 0))
                {
                    historyline = (historyline + 1) % HISTORY_LINES;

                    opcode_bytes = decode(&emu, old_pc, assembly);
                    stringpos = 0;
                    stringpos += sprintf(codelines[historyline] + stringpos,"\n%04X  ", old_pc & 0xffff);
                    for (i = 0; i < opcode_bytes; i++)
                        stringpos += sprintf(codelines[historyline] + stringpos,"%02X ", emu.mCodeMem[(old_pc + i) & (emu.mCodeMemSize - 1)]);
                    for (i = opcode_bytes; i < 3; i++)
                        stringpos += sprintf(codelines[historyline] + stringpos,"   ");
                    sprintf(codelines[historyline] + stringpos," %s",assembly);                    
                    rx = 8 * ((emu.mSFR[REG_PSW] & (PSWMASK_RS0|PSWMASK_RS1))>>PSW_RS0);
                    
                    sprintf(reglines[historyline], "\n%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %04X",
                        emu.mSFR[REG_ACC],
                        emu.mLowerData[0 + rx],
                        emu.mLowerData[1 + rx],
                        emu.mLowerData[2 + rx],
                        emu.mLowerData[3 + rx],
                        emu.mLowerData[4 + rx],
                        emu.mLowerData[5 + rx],
                        emu.mLowerData[6 + rx],
                        emu.mLowerData[7 + rx],
                        emu.mSFR[REG_B],
                        (emu.mSFR[REG_DPH]<<8)|emu.mSFR[REG_DPL]);

                    sprintf(pswlines[historyline], "\n%d %d %d %d %d %d %d %d",
                        (emu.mSFR[REG_PSW] >> 7) & 1,
                        (emu.mSFR[REG_PSW] >> 6) & 1,
                        (emu.mSFR[REG_PSW] >> 5) & 1,
                        (emu.mSFR[REG_PSW] >> 4) & 1,
                        (emu.mSFR[REG_PSW] >> 3) & 1,
                        (emu.mSFR[REG_PSW] >> 2) & 1,
                        (emu.mSFR[REG_PSW] >> 1) & 1,
                        (emu.mSFR[REG_PSW] >> 0) & 1);

                    sprintf(ioreglines[historyline], "\n%02X %02X %02X %02X %02X %02X %02X",
                        emu.mSFR[REG_SP],
                        emu.mSFR[REG_P0],
                        emu.mSFR[REG_P1],
                        emu.mSFR[REG_P2],
                        emu.mSFR[REG_P3],
                        emu.mSFR[REG_IP],
                        emu.mSFR[REG_IE]);

                    sprintf(spreglines[historyline], "\n%02X    %02X    %02X  %02X   %02X  %02X   %02X   %02X",
                        emu.mSFR[REG_TIMOD],
                        emu.mSFR[REG_TCON],
                        emu.mSFR[REG_TH0],
                        emu.mSFR[REG_TL0],
                        emu.mSFR[REG_TH1],
                        emu.mSFR[REG_TL1],
                        emu.mSFR[REG_SCON],
                        emu.mSFR[REG_PCON]);

                    if (view == MAIN_VIEW && (speed > 0 || runmode == 0))
                    {
                        wprintw(codeoutput," %s",codelines[historyline]);
                        wprintw(regoutput,"%s",reglines[historyline]);
                        wprintw(pswoutput,"%s",pswlines[historyline]);
                        wprintw(ioregoutput,"%s",ioreglines[historyline]);
                        wprintw(spregoutput,"%s",spreglines[historyline]);
                    }
                }
            }
            while (targettime > getTick() && targetclocks > clocks);
            
            while (targettime > getTick())
            {
                Sleep(1);
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
        }
    }
    while ( (ch = getch()) != 'Q' );

    endwin();

    return EXIT_SUCCESS;
}