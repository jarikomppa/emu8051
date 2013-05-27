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


WINDOW *codebox = NULL, *codeoutput = NULL;
WINDOW *regbox = NULL, *regoutput = NULL;
WINDOW *rambox = NULL, *ramview = NULL;
WINDOW *stackbox = NULL, *stackview = NULL;
WINDOW *pswbox = NULL, *pswoutput = NULL;
WINDOW *ioregbox = NULL, *ioregoutput = NULL;
WINDOW *spregbox = NULL, *spregoutput = NULL;
WINDOW *miscbox = NULL, *miscview = NULL;

int oldcols, oldrows;
int runmode = 0;
int speed = 6;
int focus = 0;

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
    oldcols = COLS;
    oldrows = LINES;

    codebox = subwin(stdscr, LINES-16, 42, 16, 0);
    box(codebox,ACS_VLINE,ACS_HLINE);
    mvwaddstr(codebox, 0, 2, "PC");
    mvwaddstr(codebox, 0, 8, "Opcodes");
    mvwaddstr(codebox, 0, 18, "Assembly");
    mvwaddstr(codebox, LINES-18, 0, ">");
    mvwaddstr(codebox, LINES-18, 41, "<");
    codeoutput = subwin(stdscr, LINES-18, 39, 17, 2);
    wmove(codeoutput,LINES-19,0);

    regbox = subwin(stdscr, LINES-16, 38, 16, 42);
    box(regbox,0,0);
    mvwaddstr(regbox, 0, 2, "A -R0-R1-R2-R3-R4-R5-R6-R7-B -DPTR");
    mvwaddstr(regbox, LINES-18, 0, ">");
    mvwaddstr(regbox, LINES-18, 37, "<");
    regoutput = subwin(stdscr, LINES-18, 35, 17, 44);
    wmove(regoutput,LINES-19,0);

    rambox = subwin(stdscr, 10, 31, 0, 0);
    box(rambox,0,0);
    mvwaddstr(rambox, 0, 2, "RAM");
    ramview = subwin(stdscr, 8, 28, 1, 2);

    stackbox = subwin(stdscr, 16, 6, 0, 31);
    box(stackbox,0,0);
    mvwaddstr(stackbox, 0, 1, "Stck");
    mvwaddstr(stackbox, 8, 0, ">");
    mvwaddstr(stackbox, 8, 5, "<");
    stackview = subwin(stdscr, 14, 4, 1, 32);

    ioregbox = subwin(stdscr, 8, 24, 0, 37);
    box(ioregbox,0,0);
    mvwaddstr(ioregbox, 0, 2, "SP-P0-P1-P2-P3-IP-IE");
    mvwaddstr(ioregbox, 6, 0, ">");
    mvwaddstr(ioregbox, 6, 23, "<");
    ioregoutput = subwin(stdscr, 6, 21, 1, 39);
    wmove(ioregoutput,5,0);

    pswbox = subwin(stdscr, 8, 19, 0, 61);
    box(pswbox,0,0);
    mvwaddstr(pswbox, 0, 2, "C-ACF0R1R0Ov--P");
    mvwaddstr(pswbox, 6, 0, ">");
    mvwaddstr(pswbox, 6, 18, "<");
    pswoutput = subwin(stdscr, 6, 16, 1, 63);
    wmove(pswoutput,5,0);

    spregbox = subwin(stdscr, 8, 43, 8, 37);
    box(spregbox,0,0);
    mvwaddstr(spregbox, 0, 2, "TIMOD-TCON--TH0-TL0--TH1-TL1--SCON-PCON");
    mvwaddstr(spregbox, 6, 0, ">");
    mvwaddstr(spregbox, 6, 42, "<");
    spregoutput = subwin(stdscr, 6, 40, 9, 39);
    wmove(spregoutput,5,0);

    miscbox = subwin(stdscr, 6, 31, 10, 0);
    box(miscbox,0,0);
    miscview = subwin(stdscr, 4, 28, 11, 2);
    

    slk_set(1, "h)elp", 0);
    slk_set(2, "l)oad", 0);
    slk_set(3, "spc=step", 0);
    slk_set(4, "r)un", 0);
    slk_set(6, "v)iew", 0);
    slk_set(7, "home=rst", 0);
    slk_set(8, "s-Q)quit", 0);

    setspeed(speed, runmode);

    scrollok(codeoutput, TRUE);
    scrollok(regoutput, TRUE);
    scrollok(pswoutput, TRUE);
    scrollok(spregoutput, TRUE);
    scrollok(ioregoutput, TRUE);

    refresh();
}

int main(void) {
    
    int      ch;
    struct em8051 emu;
    int clocks = 0, i;
    int ticked = 1;

    memset(&emu, 0, sizeof(emu));
    emu.mCodeMem     = malloc(65536);
    emu.mCodeMemSize = 65536;
    emu.mExtData     = malloc(65536);
    emu.mExtDataSize = 65536;
    emu.mLowerData   = malloc(128);
    emu.mUpperData   = malloc(128);
    emu.mSFR         = malloc(128);
    reset(&emu, 1);
    i = 0x100;

    if (load_obj(&emu, "megatest.obj") != 0)
        return;

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



    /*  Loop until user hits 'q' to quit  */

    while ( (ch = getch()) != 'Q' ) 
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
            break;
        case 'l':
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
            reset(&emu, 0);
            clocks = 0;
            ticked = 1;
            break;
        }

        if (ch == 32 || runmode)
        {
            char temp[256];
            int l, m, r, i;
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
                    l = decode(&emu, old_pc, temp);
                    wprintw(codeoutput,"\n%04X  ", old_pc & 0xffff);
                    for (m = 0; m < l; m++)
                        wprintw(codeoutput,"%02X ", emu.mCodeMem[old_pc+m]);
                    for (m = l; m < 3; m++)
                        wprintw(codeoutput,"   ");
                    wprintw(codeoutput," %s",temp);

                    rx = 8 * ((emu.mSFR[REG_PSW] & (PSWMASK_RS0|PSWMASK_RS1))>>PSW_RS0);
                    
                    wprintw(regoutput, "\n%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %04X",
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

                    wprintw(pswoutput, "\n%d %d %d %d %d %d %d %d",
                        (emu.mSFR[REG_PSW] >> 7) & 1,
                        (emu.mSFR[REG_PSW] >> 6) & 1,
                        (emu.mSFR[REG_PSW] >> 5) & 1,
                        (emu.mSFR[REG_PSW] >> 4) & 1,
                        (emu.mSFR[REG_PSW] >> 3) & 1,
                        (emu.mSFR[REG_PSW] >> 2) & 1,
                        (emu.mSFR[REG_PSW] >> 1) & 1,
                        (emu.mSFR[REG_PSW] >> 0) & 1);

                    wprintw(ioregoutput, "\n%02X %02X %02X %02X %02X %02X %02X",
                        emu.mSFR[REG_SP],
                        emu.mSFR[REG_P0],
                        emu.mSFR[REG_P1],
                        emu.mSFR[REG_P2],
                        emu.mSFR[REG_P3],
                        emu.mSFR[REG_IP],
                        emu.mSFR[REG_IE]);

                    wprintw(spregoutput, "\n%02X    %02X    %02X  %02X   %02X  %02X   %02X   %02X",
                        emu.mSFR[REG_TIMOD],
                        emu.mSFR[REG_TCON],
                        emu.mSFR[REG_TH0],
                        emu.mSFR[REG_TL0],
                        emu.mSFR[REG_TH1],
                        emu.mSFR[REG_TL1],
                        emu.mSFR[REG_SCON],
                        emu.mSFR[REG_PCON]);
                }
            }

            werase(miscview);
            wprintw(miscview, "Cycles: %9d\n\n\nTime  : %9.3fms (@12MHz)", clocks, 1000.0f * clocks * (1.0f/(12.0f*1000*1000)));
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


    /*  Clean up after ourselves  */

    endwin();
//    refresh();

    return EXIT_SUCCESS;
}