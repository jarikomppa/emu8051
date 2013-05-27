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

/*
    The history-based display assumes that there's no
    self-modifying code. To get self-modifying code
    you'd have to do some rather creative wiring anyway,
    so I doubt this will be an issue. And the worst that
    can happen is that the disassembly looks wrong.
 */

// Last clock we updated the view
static unsigned int lastclock = 0;

// focused component
static int focus = 0;

// memory window cursor position
static int memcursorpos = 0;

// memory window offset
static int memoffset = 0;

// pointer to the memory area being viewed
static unsigned char *memarea = NULL;

// code box (PC, opcode, assembly)
WINDOW *codebox = NULL, *codeoutput = NULL;

// Registers box (a, r0..7, b, dptr)
WINDOW *regbox = NULL, *regoutput = NULL;

// RAM view/edit box
WINDOW *rambox = NULL, *ramview = NULL;

// stack view
WINDOW *stackbox = NULL, *stackview = NULL;

// program status word box
WINDOW *pswbox = NULL, *pswoutput = NULL;

// IO registers box
WINDOW *ioregbox = NULL, *ioregoutput = NULL;

// special registers box
WINDOW *spregbox = NULL, *spregoutput = NULL;

// misc. stuff box
WINDOW *miscbox = NULL, *miscview = NULL;


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

void build_main_view(struct em8051 *aCPU)
{
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

    regbox = subwin(stdscr, LINES-16, 38, 16, 42);
    box(regbox,0,0);
    mvwaddstr(regbox, 0, 2, "A -R0-R1-R2-R3-R4-R5-R6-R7-B -DPTR");
    mvwaddstr(regbox, LINES-18, 0, ">");
    mvwaddstr(regbox, LINES-18, 37, "<");
    regoutput = subwin(regbox, LINES-18, 35, 17, 44);
    scrollok(regoutput, TRUE);


    rambox = subwin(stdscr, 10, 31, 0, 0);
    box(rambox,0,0);
    mvwaddstr(rambox, 0, 2, memtypes[focus]);
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

    pswbox = subwin(stdscr, 8, 19, 0, 61);
    box(pswbox,0,0);
    mvwaddstr(pswbox, 0, 2, "C-ACF0R1R0Ov--P");
    mvwaddstr(pswbox, 6, 0, ">");
    mvwaddstr(pswbox, 6, 18, "<");
    pswoutput = subwin(pswbox, 6, 16, 1, 63);
    scrollok(pswoutput, TRUE);

    spregbox = subwin(stdscr, 8, 43, 8, 37);
    box(spregbox,0,0);
    mvwaddstr(spregbox, 0, 2, "TMOD-TCON--TH0-TL0--TH1-TL1--SCON-PCON");
    mvwaddstr(spregbox, 6, 0, ">");
    mvwaddstr(spregbox, 6, 42, "<");
    spregoutput = subwin(spregbox, 6, 40, 9, 39);
    scrollok(spregoutput, TRUE);

    miscbox = subwin(stdscr, 6, 31, 10, 0);
    box(miscbox,0,0);
    miscview = subwin(miscbox, 4, 28, 11, 2);
    
    refresh();
    wrefresh(codeoutput);
    wrefresh(regoutput);
    wrefresh(pswoutput);
    wrefresh(ioregoutput);
    wrefresh(spregoutput);

    lastclock = icount - 8;

    memarea = aCPU->mLowerData;

}

void mainview_editor_keys(struct em8051 *aCPU, int ch)
{
    int insert_value = -1;
    int maxmem;
    switch(ch)
    {
    case KEY_NEXT:
    case '\t':
        memcursorpos = 0;
        memoffset = 0;
        focus++;
        if (focus == 1 && aCPU->mUpperData == NULL) 
            focus++;
        if (focus == 3 && aCPU->mExtDataSize == 0)
            focus++;
        if (focus == 5)
            focus = 0;
        switch (focus)
        {
        case 0:
            memarea = aCPU->mLowerData;
            break;
        case 1:
            memarea = aCPU->mUpperData;
            break;
        case 2:
            memarea = aCPU->mSFR;
            break;
        case 3:
            memarea = aCPU->mExtData;
            break;
        case 4:
            memarea = aCPU->mCodeMem;
            break;
        }
        mvwaddstr(rambox, 0, 2, memtypes[focus]);
        wrefresh(rambox);
        break;
    case KEY_NPAGE:
        memcursorpos += 8 * 16;
        break;
    case KEY_PPAGE:
        memcursorpos -= 8 * 16;
        break;
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

    while (memcursorpos < 0)
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
    while (memcursorpos > 128 - 1)
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

    int opcode_bytes;
    int stringpos;
    int rx;
    unsigned int hline;

    if ((speed != 0 || !runmode) && lastclock != icount)
    {
        // make sure we only display HISTORY_LINES worth of data
        if (icount - lastclock > HISTORY_LINES)
            lastclock = icount - HISTORY_LINES;

        // + HISTORY_LINES to force positive result
        hline = historyline - (icount - lastclock) + HISTORY_LINES;

        while (lastclock != icount)
        {
            char assembly[128];
            char temp[256];
            int old_pc;
            int hoffs;
            
            hline = (hline + 1) % HISTORY_LINES;

            hoffs = (hline * (128 + 64 + sizeof(int)));

            memcpy(&old_pc, history + hoffs + 128 + 64, sizeof(int));
            opcode_bytes = decode(aCPU, old_pc, assembly);
            stringpos = 0;
            stringpos += sprintf(temp + stringpos,"\n%04X  ", old_pc & 0xffff);
            
            for (i = 0; i < opcode_bytes; i++)
                stringpos += sprintf(temp + stringpos,"%02X ", aCPU->mCodeMem[(old_pc + i) & (aCPU->mCodeMemSize - 1)]);
            
            for (i = opcode_bytes; i < 3; i++)
                stringpos += sprintf(temp + stringpos,"   ");

            sprintf(temp + stringpos," %s",assembly);                    

            wprintw(codeoutput, "%s", temp);

            rx = 8 * ((history[hoffs + REG_PSW] & (PSWMASK_RS0|PSWMASK_RS1))>>PSW_RS0);
            
            sprintf(temp, "\n%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %04X",
                history[hoffs + REG_ACC],
                history[hoffs + 128 + 0 + rx],
                history[hoffs + 128 + 1 + rx],
                history[hoffs + 128 + 2 + rx],
                history[hoffs + 128 + 3 + rx],
                history[hoffs + 128 + 4 + rx],
                history[hoffs + 128 + 5 + rx],
                history[hoffs + 128 + 6 + rx],
                history[hoffs + 128 + 7 + rx],
                history[hoffs + REG_B],
                (history[hoffs + REG_DPH]<<8)|history[hoffs + REG_DPL]);
            wprintw(regoutput,"%s",temp);

            sprintf(temp, "\n%d %d %d %d %d %d %d %d",
                (history[hoffs + REG_PSW] >> 7) & 1,
                (history[hoffs + REG_PSW] >> 6) & 1,
                (history[hoffs + REG_PSW] >> 5) & 1,
                (history[hoffs + REG_PSW] >> 4) & 1,
                (history[hoffs + REG_PSW] >> 3) & 1,
                (history[hoffs + REG_PSW] >> 2) & 1,
                (history[hoffs + REG_PSW] >> 1) & 1,
                (history[hoffs + REG_PSW] >> 0) & 1);
            wprintw(pswoutput,"%s",temp);

            sprintf(temp, "\n%02X %02X %02X %02X %02X %02X %02X",
                history[hoffs + REG_SP],
                history[hoffs + REG_P0],
                history[hoffs + REG_P1],
                history[hoffs + REG_P2],
                history[hoffs + REG_P3],
                history[hoffs + REG_IP],
                history[hoffs + REG_IE]);
            wprintw(ioregoutput,"%s",temp);

            sprintf(temp, "\n%02X   %02X    %02X  %02X   %02X  %02X   %02X   %02X",
                history[hoffs + REG_TMOD],
                history[hoffs + REG_TCON],
                history[hoffs + REG_TH0],
                history[hoffs + REG_TL0],
                history[hoffs + REG_TH1],
                history[hoffs + REG_TL1],
                history[hoffs + REG_SCON],
                history[hoffs + REG_PCON]);
            wprintw(spregoutput, "%s", temp);

            lastclock++;
        }
    }


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
