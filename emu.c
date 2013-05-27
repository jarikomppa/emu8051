/* 8051 emulator 
 * Copyright 2006 Jari Komppa
 * Released under GPL
 *
 * emu.c
 * Curses-based emulator front-end
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "curses.h"
#include "emu8051.h"

#define HISTORY_LINES 20

WINDOW *codebox = NULL, *codeoutput = NULL;
char *codelines[HISTORY_LINES];
WINDOW *regbox = NULL, *regoutput = NULL;
char *reglines[HISTORY_LINES];
WINDOW *rambox = NULL, *ramview = NULL;
WINDOW *stackbox = NULL, *stackview = NULL;
WINDOW *pswbox = NULL, *pswoutput = NULL;
char *pswlines[HISTORY_LINES];
WINDOW *ioregbox = NULL, *ioregoutput = NULL;
char *ioreglines[HISTORY_LINES];
WINDOW *spregbox = NULL, *spregoutput = NULL;
char *spreglines[HISTORY_LINES];
WINDOW *miscbox = NULL, *miscview = NULL;

int historyline = 0;
int oldcols, oldrows;
int runmode = 0;
int speed = 6;
int focus = 0;


void setspeed(int speed, int runmode)
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

void wipe_main_view()
{
    delwin(codebox);
    delwin(codeoutput);
    delwin(regbox);
    delwin(regoutput);
    delwin(rambox);
    delwin(ramview);
    delwin(stackbox);
    delwin(stackview);
    delwin(pswbox);
    delwin(pswoutput);
    delwin(ioregbox);
    delwin(ioregoutput);
    delwin(spregbox);
    delwin(spregoutput);
    delwin(miscbox);
    delwin(miscview);
}

void build_main_view()
{
    int i;
    erase();

    oldcols = COLS;
    oldrows = LINES;

    codebox = subwin(stdscr, LINES-16, 42, 16, 0);
    box(codebox,ACS_VLINE,ACS_HLINE);
    mvwaddstr(codebox, 0, 2, "PC");
    mvwaddstr(codebox, 0, 8, "Opcodes");
    mvwaddstr(codebox, 0, 18, "Assembly");
    mvwaddstr(codebox, LINES-18, 0, ">");
    mvwaddstr(codebox, LINES-18, 41, "<");
    codeoutput = subwin(codebox, LINES-18, 39, 17, 2);
    scrollok(codeoutput, TRUE);
    for (i = 0; i < HISTORY_LINES; i++)
        wprintw(codeoutput, "%s", codelines[(i + historyline + 1) % HISTORY_LINES]);    

    regbox = subwin(stdscr, LINES-16, 38, 16, 42);
    box(regbox,0,0);
    mvwaddstr(regbox, 0, 2, "A -R0-R1-R2-R3-R4-R5-R6-R7-B -DPTR");
    mvwaddstr(regbox, LINES-18, 0, ">");
    mvwaddstr(regbox, LINES-18, 37, "<");
    regoutput = subwin(regbox, LINES-18, 35, 17, 44);
    scrollok(regoutput, TRUE);
    for (i = 0; i < HISTORY_LINES; i++)
        wprintw(regoutput, "%s", reglines[(i + historyline + 1) % HISTORY_LINES]);    


    rambox = subwin(stdscr, 10, 31, 0, 0);
    box(rambox,0,0);
    mvwaddstr(rambox, 0, 2, "RAM");
    ramview = subwin(rambox, 8, 28, 1, 2);

    stackbox = subwin(stdscr, 16, 6, 0, 31);
    box(stackbox,0,0);
    mvwaddstr(stackbox, 0, 1, "Stck");
    mvwaddstr(stackbox, 8, 0, ">");
    mvwaddstr(stackbox, 8, 5, "<");
    stackview = subwin(stackbox, 14, 4, 1, 32);

    ioregbox = subwin(stdscr, 8, 24, 0, 37);
    box(ioregbox,0,0);
    mvwaddstr(ioregbox, 0, 2, "SP-P0-P1-P2-P3-IP-IE");
    mvwaddstr(ioregbox, 6, 0, ">");
    mvwaddstr(ioregbox, 6, 23, "<");
    ioregoutput = subwin(ioregbox, 6, 21, 1, 39);
    scrollok(ioregoutput, TRUE);
    for (i = 0; i < HISTORY_LINES; i++)
        wprintw(ioregoutput, "%s", ioreglines[(i + historyline + 1) % HISTORY_LINES]);    

    pswbox = subwin(stdscr, 8, 19, 0, 61);
    box(pswbox,0,0);
    mvwaddstr(pswbox, 0, 2, "C-ACF0R1R0Ov--P");
    mvwaddstr(pswbox, 6, 0, ">");
    mvwaddstr(pswbox, 6, 18, "<");
    pswoutput = subwin(pswbox, 6, 16, 1, 63);
    scrollok(pswoutput, TRUE);
    for (i = 0; i < HISTORY_LINES; i++)
        wprintw(pswoutput, "%s", pswlines[(i + historyline + 1) % HISTORY_LINES]);    

    spregbox = subwin(stdscr, 8, 43, 8, 37);
    box(spregbox,0,0);
    mvwaddstr(spregbox, 0, 2, "TIMOD-TCON--TH0-TL0--TH1-TL1--SCON-PCON");
    mvwaddstr(spregbox, 6, 0, ">");
    mvwaddstr(spregbox, 6, 42, "<");
    spregoutput = subwin(spregbox, 6, 40, 9, 39);
    scrollok(spregoutput, TRUE);
    for (i = 0; i < HISTORY_LINES; i++)
        wprintw(spregoutput, "%s", spreglines[(i + historyline + 1) % HISTORY_LINES]);    

    miscbox = subwin(stdscr, 6, 31, 10, 0);
    box(miscbox,0,0);
    miscview = subwin(miscbox, 4, 28, 11, 2);
    

    slk_set(1, "h)elp", 0);
    slk_set(2, "l)oad", 0);
    slk_set(3, "spc=step", 0);
    slk_set(4, "r)un", 0);
    slk_set(6, "v)iew", 0);
    slk_set(7, "home=rst", 0);
    slk_set(8, "s-Q)quit", 0);

    setspeed(speed, runmode);

    refresh();
}

void emu_exception(struct em8051 *aCPU, int aCode)
{
    WINDOW * exc;
    nocbreak();
    cbreak();
    nodelay(stdscr, FALSE);
    halfdelay(1);
    while (getch() > 0) {}


    runmode = 0;
    setspeed(speed, runmode);
    exc = subwin(stdscr, 7, 50, (LINES-6)/2, (COLS-50)/2);
    wattron(exc,A_REVERSE);
    werase(exc);
    box(exc,ACS_VLINE,ACS_HLINE);
    mvwaddstr(exc, 0, 2, "Exception");
    wattroff(exc,A_REVERSE);
    wmove(exc, 2, 2);

    switch (aCode)
    {
    case EXCEPTION_STACK: waddstr(exc,"SP exception: stack address > 127");
                          wmove(exc, 3, 2);
                          waddstr(exc,"with no upper memory, or SP roll over."); 
                          break;
    case EXCEPTION_ACC_TO_A: waddstr(exc,"Invalid operation: acc-to-a move operation"); 
                             break;
    case EXCEPTION_IRET_PSW_MISMATCH: waddstr(exc,"PSW not preserved over interrupt call"); 
                                      break;
    case EXCEPTION_IRET_SP_MISMATCH: waddstr(exc,"SP not preserved over interrupt call"); 
                                     break;
    case EXCEPTION_ILLEGAL_OPCODE: waddstr(exc,"Invalid opcode: 0xA5 encountered"); 
                                   break;
    default:
        waddstr(exc,"Unknown exception"); 
    }
    wmove(exc, 5, 12);
    waddstr(exc, "Press any key to continue");

    wrefresh(exc);

    getch();
    delwin(exc);
    wipe_main_view();
    build_main_view();
}

void emu_load(struct em8051 *aCPU)
{
    WINDOW * exc;
    char temp[256];
    int pos = 0;
    int ch = 0;
    int result;
    temp[0] = 0;

    runmode = 0;
    setspeed(speed, runmode);
    exc = subwin(stdscr, 5, 50, (LINES-6)/2, (COLS-50)/2);
    wattron(exc,A_REVERSE);
    werase(exc);
    box(exc,ACS_VLINE,ACS_HLINE);
    mvwaddstr(exc, 0, 2, "Load Intel HEX File");
    wattroff(exc,A_REVERSE);
    wmove(exc, 2, 2);
    //            12345678901234567890123456780123456789012345
    waddstr(exc,"[____________________________________________]"); 
    wmove(exc,2,3);
    wrefresh(exc);

    while (ch != '\n')
    {
        ch = getch();
        if (ch > 31 && ch < 127 || ch > 127 && ch < 255)
        {
            if (pos < 44)
            {
                temp[pos] = ch;
                pos++;
                temp[pos] = 0;
                waddch(exc,ch);
                wrefresh(exc);
            }
        }
        if (ch == 8)
        {
            if (pos > 0)
            {
                pos--;
                temp[pos] = 0;
                wmove(exc,2,3+pos);
                waddch(exc,'_');
                wmove(exc,2,3+pos);
                wrefresh(exc);
            }
        }
    }

    result = load_obj(aCPU, temp);
    wmove(exc, 1, 12);
    switch (result)
    {
    case 0:
        waddstr(exc,"File loaded successfully.");
        break;
    case -1:
        waddstr(exc,"File not found.");
        break;
    case -2:
        waddstr(exc,"Bad file format.");
        break;
    case -3:
        waddstr(exc,"Unsupported HEX file version.");
        break;
    case -4:
        waddstr(exc,"Checksum failure.");
        break;
    case -5:
        waddstr(exc,"No end of data marker found.");
        break;
    }
    wmove(exc, 3, 12);
    waddstr(exc, "Press any key to continue");
    wrefresh(exc);

    getch();
    delwin(exc);
    wipe_main_view();
    build_main_view();
}

int emu_reset(struct em8051 *aCPU)
{
    WINDOW * exc;
    char temp[256];
    int pos = 0;
    int ch = 0;
    int result;
    temp[0] = 0;

    runmode = 0;
    setspeed(speed, runmode);
    exc = subwin(stdscr, 7, 60, (LINES-7)/2, (COLS-60)/2);
    wattron(exc,A_REVERSE);
    werase(exc);
    box(exc,ACS_VLINE,ACS_HLINE);
    mvwaddstr(exc, 0, 2, "Reset");
    wattroff(exc,A_REVERSE);
    wrefresh(exc);

    wmove(exc, 2, 2);
    waddstr(exc, "S)et PC = 0");
    wmove(exc, 3, 2);
    waddstr(exc, "R)eset (init regs, set PC to zero)");
    wmove(exc, 4, 2);
    waddstr(exc, "W)ipe (init regs, set PC to zero, clear memory)");
    wrefresh(exc);

    result = 0;
    ch = getch();
    switch (ch)
    {
    case 's':
    case 'S':
        aCPU->mPC = 0;
        result = 1;
        break;
    case 'r':
    case 'R':
        reset(aCPU, 0);
        result = 1;
        break;
    case 'w':
    case 'W':
        reset(aCPU, 1);
        result = 1;
        break;
    }
    delwin(exc);
    wipe_main_view();
    build_main_view();
    return result;
}

void emu_help()
{
    WINDOW * exc;
    char temp[256];
    int pos = 0;
    int ch = 0;
    temp[0] = 0;

    runmode = 0;
    setspeed(speed, runmode);
    exc = subwin(stdscr, 7, 60, (LINES-7)/2, (COLS-60)/2);
    wattron(exc,A_REVERSE);
    werase(exc);
    box(exc,ACS_VLINE,ACS_HLINE);
    mvwaddstr(exc, 0, 2, "Help");
    wattroff(exc,A_REVERSE);
    wrefresh(exc);

    wmove(exc, 2, 2);
    waddstr(exc, "8051 Emulator v. 0.1");
    wmove(exc, 3, 2);
    waddstr(exc, "Copyright (c) 2006 Jari Komppa");
    wmove(exc, 5, 17);
    waddstr(exc, "Press any key to continue");
    wrefresh(exc);

    ch = getch();

    delwin(exc);
    wipe_main_view();
    build_main_view();
}


int main(int parc, char ** pars) 
{
    char temp[256];    
    char assembly[256];
    int ch = 0;
    struct em8051 emu;
    int clocks = 0, i;
    int ticked = 1;

    for (i = 0; i < HISTORY_LINES; i++)
    {
        codelines[i] = strdup("\n");
        reglines[i] = strdup("\n");
        pswlines[i] = strdup("\n");
        ioreglines[i] = strdup("\n");
        spreglines[i] = strdup("\n");
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
    reset(&emu, 1);
    i = 0x100;

    if (parc > 1)
    if (load_obj(&emu, pars[1]) != 0)
    {
        printf("File '%s' load failure\n\n",pars[1]);
        return -1;
    }

    /*  Initialize ncurses  */

    slk_init(1);
    if ( (initscr()) == NULL ) {
	    fprintf(stderr, "Error initialising ncurses.\n");
	    exit(EXIT_FAILURE);
    }
    

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
            wipe_main_view();
            build_main_view();
        }
        switch (ch)
        {
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
                setspeed(speed, runmode);
            }
            break;
#ifdef __PDCURSES__
        case PADPLUS:
#endif
        case '+':
            speed--;
            if (speed < 0)
                speed = 0;
            setspeed(speed, runmode);
            break;
#ifdef __PDCURSES__
        case PADMINUS:
#endif
        case '-':
            speed++;
            if (speed > 7)
                speed = 7;
            setspeed(speed, runmode);
            break;
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
            break;
        case KEY_F(1):
            emu.mSFR[REG_P1] = 1;
            break;
        case KEY_F(2):
            emu.mSFR[REG_P1] = 2;
            break;
        case KEY_F(3):
            emu.mSFR[REG_P1] = 4;
            break;
        case KEY_F(4):
            emu.mSFR[REG_P1] = 8;
            break;
        case KEY_F(5):
            emu.mSFR[REG_P1] = 0;
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
            int l, m, r, i, j;
            int rx;
            r = 1;
            if (speed < 2)
                r = 1000;
            for (i = 0; i < r; i++)
            {
                int old_pc;
                old_pc = emu.mPC;
                clocks += 12;
                ticked = tick(&emu);
                if (ticked && speed != 0)
                {
                    historyline = (historyline + 1) % HISTORY_LINES;

                    l = decode(&emu, old_pc, assembly);
                    j = 0;
                    j += sprintf(temp + j,"\n%04X  ", old_pc & 0xffff);
                    for (m = 0; m < l; m++)
                        j += sprintf(temp + j,"%02X ", emu.mCodeMem[old_pc+m]);
                    for (m = l; m < 3; m++)
                        j += sprintf(temp + j,"   ");
                    sprintf(temp + j," %s",assembly);
                    wprintw(codeoutput," %s",temp);
                    /*
                    wprintw(codeoutput,"\n%04X  ", old_pc & 0xffff);
                    for (m = 0; m < l; m++)
                        wprintw(codeoutput,"%02X ", emu.mCodeMem[old_pc+m]);
                    for (m = l; m < 3; m++)
                        wprintw(codeoutput,"   ");
                    wprintw(codeoutput," %s",temp);
                    */
                    free(codelines[historyline]);
                    codelines[historyline] = strdup(temp);

                    rx = 8 * ((emu.mSFR[REG_PSW] & (PSWMASK_RS0|PSWMASK_RS1))>>PSW_RS0);
                    
                    sprintf(temp, "\n%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %04X",
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
                    wprintw(regoutput,"%s",temp);
                    free(reglines[historyline]);
                    reglines[historyline] = strdup(temp);

                    sprintf(temp, "\n%d %d %d %d %d %d %d %d",
                        (emu.mSFR[REG_PSW] >> 7) & 1,
                        (emu.mSFR[REG_PSW] >> 6) & 1,
                        (emu.mSFR[REG_PSW] >> 5) & 1,
                        (emu.mSFR[REG_PSW] >> 4) & 1,
                        (emu.mSFR[REG_PSW] >> 3) & 1,
                        (emu.mSFR[REG_PSW] >> 2) & 1,
                        (emu.mSFR[REG_PSW] >> 1) & 1,
                        (emu.mSFR[REG_PSW] >> 0) & 1);
                    wprintw(pswoutput,"%s",temp);
                    free(pswlines[historyline]);
                    pswlines[historyline] = strdup(temp);

                    sprintf(temp, "\n%02X %02X %02X %02X %02X %02X %02X",
                        emu.mSFR[REG_SP],
                        emu.mSFR[REG_P0],
                        emu.mSFR[REG_P1],
                        emu.mSFR[REG_P2],
                        emu.mSFR[REG_P3],
                        emu.mSFR[REG_IP],
                        emu.mSFR[REG_IE]);
                    wprintw(ioregoutput,"%s",temp);
                    free(ioreglines[historyline]);
                    ioreglines[historyline] = strdup(temp);

                    sprintf(temp, "\n%02X    %02X    %02X  %02X   %02X  %02X   %02X   %02X",
                        emu.mSFR[REG_TIMOD],
                        emu.mSFR[REG_TCON],
                        emu.mSFR[REG_TH0],
                        emu.mSFR[REG_TL0],
                        emu.mSFR[REG_TH1],
                        emu.mSFR[REG_TL1],
                        emu.mSFR[REG_SCON],
                        emu.mSFR[REG_PCON]);
                    wprintw(spregoutput,"%s",temp);
                    free(spreglines[historyline]);
                    spreglines[historyline] = strdup(temp);
                }
            }

            werase(miscview);
            wprintw(miscview, "Cycles: %9d\nTime  : %9.3fms (@12MHz)", clocks, 1000.0f * clocks * (1.0f/(12.0f*1000*1000)));
            wrefresh(miscview);

            if (speed != 0)
            {
                wrefresh(codeoutput);
                wrefresh(regoutput);
                wrefresh(ioregoutput);
                wrefresh(spregoutput);
                wrefresh(pswoutput);
            }
        }

        werase(ramview);
        for (i = 0; i < 8; i++)
        {
            wprintw(ramview,"%02X  %02X %02X %02X %02X %02X %02X %02X %02X\n", 
                i*8, 
                emu.mLowerData[i*8+0], emu.mLowerData[i*8+1], emu.mLowerData[i*8+2], emu.mLowerData[i*8+3],
                emu.mLowerData[i*8+4], emu.mLowerData[i*8+5], emu.mLowerData[i*8+6], emu.mLowerData[i*8+7]);
        }
/*
        wattron(ramview, A_REVERSE);
        wmove(ramview,5,5);
        wprintw(ramview,"huuhaa");
        wattroff(ramview, A_REVERSE);
*/
        werase(stackview);
        for (i = 0; i < 14; i++)
        {
            wprintw(stackview," %02X\n", 
                emu.mLowerData[(i+emu.mSFR[REG_SP]-7)&0x7f]);
        }

        if (speed != 0)
        {
            wrefresh(ramview);
            wrefresh(stackview);
        }
    }
    while ( (ch = getch()) != 'Q' );


    /*  Clean up after ourselves  */

    endwin();
//    refresh();

    return EXIT_SUCCESS;
}