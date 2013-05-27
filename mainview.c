/* 8051 emulator 
 * Copyright 2006 Jari Komppa
 * Released under GPL
 *
 * mainview.c
 * Main view-related stuff for the curses-based emulator front-end
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "curses.h"
#include "emu8051.h"
#include "emulator.h"

char *memtypes[]={"Low","Upr","SFR","Ext","ROM"};

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
    ramview = subwin(rambox, 8, 29, 1, 1);

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
    
    refresh();
    wrefresh(codeoutput);
    wrefresh(regoutput);
    wrefresh(pswoutput);
    wrefresh(ioregoutput);
    wrefresh(spregoutput);

}

void mainview_editor_keys(struct em8051 *aCPU, int ch)
{
    int insert_value = -1;
    int maxmem;
    switch(ch)
    {
    case KEY_RIGHT:
        memcursorpos++;
        break;
    case KEY_LEFT:
        memcursorpos--;
        break;
    case KEY_UP:
        memcursorpos-=16;
        break;
    case KEY_DOWN:
        memcursorpos+=16;
        break;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        insert_value = ch - '0';
        break;
    case 'a':
    case 'A':
        insert_value = 0xa;
        break;
    case 'b':
    case 'B':
        insert_value = 0xb;
        break;
    case 'c':
    case 'C':
        insert_value = 0xc;
        break;
    case 'd':
    case 'D':
        insert_value = 0xd;
        break;
    case 'e':
    case 'E':
        insert_value = 0xe;
        break;
    case 'f':
    case 'F':
        insert_value = 0xf;
        break;
    }

    switch (focus)
    {
    case 0:
    case 1:
    case 2:
        maxmem = 128;
        break;
    case 3:
        maxmem = aCPU->mExtDataSize;
        break;
    case 4:
        maxmem = aCPU->mCodeMemSize;
        break;
    }

    if (insert_value != -1)
    {
        if (memcursorpos & 1)
            memarea[memoffset + (memcursorpos / 2)] = (memarea[memoffset + (memcursorpos / 2)] & 0xf0) | insert_value;
        else
            memarea[memoffset + (memcursorpos / 2)] = (memarea[memoffset + (memcursorpos / 2)] & 0x0f) | (insert_value << 4);
        memcursorpos++;
    }

    if (memcursorpos < 0)
    {
        memoffset -= 8;
        if (memoffset < 0)
        {
            memoffset = 0;
            memcursorpos = 0;
        }
        else
        {
            memcursorpos += 16;
        }
    }
    if (memcursorpos > 128 - 1)
    {
        memoffset += 8;
        if (memoffset > (maxmem - 8*8))
        {
            memoffset = (maxmem - 8*8);
            memcursorpos = 128 - 1;
        }
        else
        {
            memcursorpos -= 16;
        }
    }
}

void mainview_update(struct em8051 *aCPU)
{
    int bytevalue;
    int i;
    werase(miscview);
    wprintw(miscview, "Cycles:%10u\n", clocks);
    wprintw(miscview, "Time  :%10.3fms (@12MHz)\n", 1000.0f * clocks * (1.0f/(12.0f*1000*1000)));

    werase(ramview);
    for (i = 0; i < 8; i++)
    {
        wprintw(ramview,"%04X %02X %02X %02X %02X %02X %02X %02X %02X\n", 
            i*8+memoffset, 
            memarea[i*8+0+memoffset], memarea[i*8+1+memoffset], memarea[i*8+2+memoffset], memarea[i*8+3+memoffset],
            memarea[i*8+4+memoffset], memarea[i*8+5+memoffset], memarea[i*8+6+memoffset], memarea[i*8+7+memoffset]);
    }

    bytevalue = memarea[memcursorpos / 2 + memoffset];
    wattron(ramview, A_REVERSE);
    wmove(ramview, memcursorpos / 16, 5 + ((memcursorpos % 16) / 2) * 3 + (memcursorpos & 1));
    wprintw(ramview,"%X", (bytevalue >> (4 * (!(memcursorpos & 1)))) & 0xf);
    wattroff(ramview, A_REVERSE);
    wprintw(miscview, "%s %04X: %d %d %d %d %d %d %d %d", 
            memtypes[focus],
            memcursorpos / 2 + memoffset,
            (bytevalue >> 7) & 1,
            (bytevalue >> 6) & 1,
            (bytevalue >> 5) & 1,
            (bytevalue >> 4) & 1,
            (bytevalue >> 3) & 1,
            (bytevalue >> 2) & 1,
            (bytevalue >> 1) & 1,
            (bytevalue >> 0) & 1);
    for (i = 0; i < 14; i++)
    {
        wprintw(stackview," %02X\n", 
            aCPU->mLowerData[(i + aCPU->mSFR[REG_SP]-7)&0x7f]);
    }

    if (speed != 0 || runmode == 0)
    {
        wrefresh(ramview);
        wrefresh(stackview);
    }
    werase(stackview);
    wrefresh(miscview);
    if (speed != 0 || runmode == 0)
    {
        wrefresh(codeoutput);
        wrefresh(regoutput);
        wrefresh(ioregoutput);
        wrefresh(spregoutput);
        wrefresh(pswoutput);
    }
}
