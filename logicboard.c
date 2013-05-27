/* 8051 emulator 
 * Copyright 2006 Jari Komppa
 * Released under GPL
 *
 * logicboard.c
 * Logic board view-related stuff for the curses-based emulator front-end
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "curses.h"
#include "emu8051.h"
#include "emulator.h"

static int position;
static int logicmode = 0;
static int portmode = 0;
static int oldports[4];
static unsigned char shiftregisters[4*4];

void logicboard_tick(struct em8051 *aCPU)
{
    int i;
    if (logicmode == 2)
    {
        for (i = 0; i < 4; i++)
        {
            int clockmask = 2 << (i * 2);
            if ((oldports[0] & clockmask) == 0 && (aCPU->mSFR[REG_P0] & clockmask))
            {
                shiftregisters[i] <<= 1;
                shiftregisters[i] |= (aCPU->mSFR[REG_P0] & (clockmask >> 1)) != 0;
            }
            if ((oldports[1] & clockmask) == 0 && (aCPU->mSFR[REG_P1] & clockmask))
            {
                shiftregisters[i + 4] <<= 1;
                shiftregisters[i + 4] |= (aCPU->mSFR[REG_P1] & (clockmask >> 1)) != 0;
            }
            if ((oldports[2] & clockmask) == 0 && (aCPU->mSFR[REG_P2] & clockmask))
            {
                shiftregisters[i + 8] <<= 1;
                shiftregisters[i + 8] |= (aCPU->mSFR[REG_P2] & (clockmask >> 1)) != 0;
            }
            if ((oldports[3] & clockmask) == 0 && (aCPU->mSFR[REG_P3] & clockmask))
            {
                shiftregisters[i + 12] <<= 1;
                shiftregisters[i + 12] |= (aCPU->mSFR[REG_P3] & (clockmask >> 1)) != 0;
            }
        }
        oldports[0] = aCPU->mSFR[REG_P0];
        oldports[1] = aCPU->mSFR[REG_P1];
        oldports[2] = aCPU->mSFR[REG_P2];
        oldports[3] = aCPU->mSFR[REG_P3];
    }
}

static void logicboard_render_7segs(struct em8051 *aCPU)
{
    int input1 = aCPU->mSFR[REG_P0];
    int input2 = aCPU->mSFR[REG_P1];
    int input3 = aCPU->mSFR[REG_P2];
    int input4 = aCPU->mSFR[REG_P3];
    mvprintw(2, 40, " %c   %c   %c   %c ", " -"[(input4 >> 0)&1], " -"[(input3 >> 0)&1], " -"[(input2 >> 0)&1], " -"[(input1 >> 0)&1]);
    mvprintw(3, 40, "%c %c %c %c %c %c %c %c", " |"[(input4 >> 5)&1], " |"[(input4 >> 1)&1], " |"[(input3 >> 5)&1], " |"[(input3 >> 1)&1], " |"[(input2 >> 5)&1], " |"[(input2 >> 1)&1], " |"[(input1 >> 5)&1], " |"[(input1 >> 1)&1]);
    mvprintw(4, 40, " %c   %c   %c   %c ", " -"[(input4 >> 6)&1], " -"[(input3 >> 6)&1], " -"[(input2 >> 6)&1], " -"[(input1 >> 6)&1]);
    mvprintw(5, 40, "%c %c %c %c %c %c %c %c", " |"[(input4 >> 4)&1], " |"[(input4 >> 2)&1], " |"[(input3 >> 4)&1], " |"[(input3 >> 2)&1], " |"[(input2 >> 4)&1], " |"[(input2 >> 2)&1], " |"[(input1 >> 4)&1], " |"[(input1 >> 2)&1]);
    mvprintw(6, 40, " %c%c  %c%c  %c%c  %c%c", " -"[(input4 >> 3)&1], " ."[(input4 >> 7)&1], " -"[(input3 >> 3)&1], " ."[(input3 >> 7)&1], " -"[(input2 >> 3)&1], " ."[(input2 >> 7)&1], " -"[(input1 >> 3)&1], " ."[(input1 >> 7)&1]);
}

static void logicboard_render_registers()
{
    mvprintw(2, 40, "P0.0/1: %02Xh     P2.0/1: %02Xh", shiftregisters[0], shiftregisters[8]);
    mvprintw(3, 40, "P0.2/3: %02Xh     P2.2/3: %02Xh", shiftregisters[1], shiftregisters[9]);
    mvprintw(4, 40, "P0.4/5: %02Xh     P2.4/5: %02Xh", shiftregisters[2], shiftregisters[10]);
    mvprintw(5, 40, "P0.6/7: %02Xh     P2.6/7: %02Xh", shiftregisters[3], shiftregisters[11]);
    mvprintw(7, 40, "P1.0/1: %02Xh     P3.0/1: %02Xh", shiftregisters[4], shiftregisters[12]);
    mvprintw(8, 40, "P1.2/3: %02Xh     P3.2/3: %02Xh", shiftregisters[5], shiftregisters[13]);
    mvprintw(9, 40, "P1.4/5: %02Xh     P3.4/5: %02Xh", shiftregisters[6], shiftregisters[14]);
    mvprintw(10, 40, "P1.6/7: %02Xh     P3.6/7: %02Xh", shiftregisters[7], shiftregisters[15]);
}

static void logicboard_entermode()
{
}

static void logicboard_leavemode()
{
    switch (logicmode)
    {
    case 1:
        mvprintw(2, 40, "               ");
        mvprintw(3, 40, "               ");
        mvprintw(4, 40, "               ");
        mvprintw(5, 40, "               ");
        mvprintw(6, 40, "               ");
        break;
    case 2:
        mvprintw(2, 40, "                           ");
        mvprintw(3, 40, "                           ");
        mvprintw(4, 40, "                           ");
        mvprintw(5, 40, "                           ");
        mvprintw(7, 40, "                           ");
        mvprintw(8, 40, "                           ");
        mvprintw(9, 40, "                           ");
        mvprintw(10, 40, "                           ");
        break;
    }
}

void wipe_logicboard_view()
{
    logicboard_leavemode();
}

void build_logicboard_view(struct em8051 *aCPU)
{
    erase();
    logicboard_entermode();
}


void logicboard_editor_keys(struct em8051 *aCPU, int ch)
{
    int xorvalue = -1;
    switch (ch)
    {
    case KEY_RIGHT:
        if (position == 4)
        {
            logicboard_leavemode();
            logicmode++;
            if (logicmode > 2) logicmode = 2;
            logicboard_entermode();
        }
        if (position == 5)
        {
            portmode++;
            if (portmode > 5) portmode = 5;
        }
        break;
    case KEY_LEFT:
        if (position == 4)
        {
            logicboard_leavemode();
            logicmode--;
            if (logicmode < 0) logicmode = 0;
            logicboard_entermode();
        }
        if (position == 5)
        {
            portmode--;
            if (portmode < 0) portmode = 0;
        }
        break;
    case KEY_DOWN:
        position++;
        if (position > 5) position = 5;
        break;
    case KEY_UP:
        position--;
        if (position < 0) position = 0;
        break;
    case '1':
        xorvalue = 7;
        break;
    case '2':
        xorvalue = 6;
        break;
    case '3':
        xorvalue = 5;
        break;
    case '4':
        xorvalue = 4;
        break;
    case '5':
        xorvalue = 3;
        break;
    case '6':
        xorvalue = 2;
        break;
    case '7':
        xorvalue = 1;
        break;
    case '8':
        xorvalue = 0;
        break;
    }
    if (xorvalue != -1)
    {
        switch (position)
        {
        case 0:
            p0out ^= 1 << xorvalue;
            break;
        case 1:
            p1out ^= 1 << xorvalue;
            break;
        case 2:
            p2out ^= 1 << xorvalue;
            break;
        case 3:
            p3out ^= 1 << xorvalue;
            break;
        }
    }
}

void logicboard_update(struct em8051 *aCPU)
{
    char ledstate[]="_*";
    char swstate[]="01";
    int data;
    mvprintw( 1, 1, "Logic board view");

    mvprintw( 3, 5, "1 2 3 4 5 6 7 8");
    data = aCPU->mSFR[REG_P0];
    mvprintw( 4, 2, "P0 %c %c %c %c %c %c %c %c",
        ledstate[(data>>7)&1],
        ledstate[(data>>6)&1],
        ledstate[(data>>5)&1],
        ledstate[(data>>4)&1],
        ledstate[(data>>3)&1],
        ledstate[(data>>2)&1],
        ledstate[(data>>1)&1],
        ledstate[(data>>0)&1]);

    data = p0out;
    mvprintw( 5, 2, "   %c %c %c %c %c %c %c %c",
        swstate[(data>>7)&1],
        swstate[(data>>6)&1],
        swstate[(data>>5)&1],
        swstate[(data>>4)&1],
        swstate[(data>>3)&1],
        swstate[(data>>2)&1],
        swstate[(data>>1)&1],
        swstate[(data>>0)&1]);

    data = aCPU->mSFR[REG_P1];
    mvprintw( 7, 2, "P1 %c %c %c %c %c %c %c %c",
        ledstate[(data>>7)&1],
        ledstate[(data>>6)&1],
        ledstate[(data>>5)&1],
        ledstate[(data>>4)&1],
        ledstate[(data>>3)&1],
        ledstate[(data>>2)&1],
        ledstate[(data>>1)&1],
        ledstate[(data>>0)&1]);

    data = p1out;
    mvprintw( 8, 2, "   %c %c %c %c %c %c %c %c",
        swstate[(data>>7)&1],
        swstate[(data>>6)&1],
        swstate[(data>>5)&1],
        swstate[(data>>4)&1],
        swstate[(data>>3)&1],
        swstate[(data>>2)&1],
        swstate[(data>>1)&1],
        swstate[(data>>0)&1]);

    data = aCPU->mSFR[REG_P2];
    mvprintw(10, 2, "P2 %c %c %c %c %c %c %c %c",
        ledstate[(data>>7)&1],
        ledstate[(data>>6)&1],
        ledstate[(data>>5)&1],
        ledstate[(data>>4)&1],
        ledstate[(data>>3)&1],
        ledstate[(data>>2)&1],
        ledstate[(data>>1)&1],
        ledstate[(data>>0)&1]);

    data = p2out;
    mvprintw(11, 2, "   %c %c %c %c %c %c %c %c",
        swstate[(data>>7)&1],
        swstate[(data>>6)&1],
        swstate[(data>>5)&1],
        swstate[(data>>4)&1],
        swstate[(data>>3)&1],
        swstate[(data>>2)&1],
        swstate[(data>>1)&1],
        swstate[(data>>0)&1]);

    data = aCPU->mSFR[REG_P3];
    mvprintw(13, 2, "P3 %c %c %c %c %c %c %c %c",
        ledstate[(data>>7)&1],
        ledstate[(data>>6)&1],
        ledstate[(data>>5)&1],
        ledstate[(data>>4)&1],
        ledstate[(data>>3)&1],
        ledstate[(data>>2)&1],
        ledstate[(data>>1)&1],
        ledstate[(data>>0)&1]);

    data = p3out;
    mvprintw(14, 2, "   %c %c %c %c %c %c %c %c",
        swstate[(data>>7)&1],
        swstate[(data>>6)&1],
        swstate[(data>>5)&1],
        swstate[(data>>4)&1],
        swstate[(data>>3)&1],
        swstate[(data>>2)&1],
        swstate[(data>>1)&1],
        swstate[(data>>0)&1]);

    mvprintw(17, 2, "  ");
    mvprintw(20, 2, "  ");

    attron(A_REVERSE);
    switch (logicmode)
    {
    case 0:
        mvprintw(17, 4, "< No additional hw     >");
        mvprintw(20, 4, "< n/a    >");
        break;
    case 1:
        mvprintw(17, 4, "< 7-seg displays       >");
        mvprintw(20, 4, "< n/a    >");
        break;
    case 2:
        mvprintw(17, 4, "< 8bit shift registers >");
        mvprintw(20, 4, "< n/a    >");
        break;
    }
/*
    if (logicmode != 0)
    {
        switch (portmode)
        {
        case 0:
            mvprintw(20, 4, "< P0, P1 >");
            break;
        case 1:
            mvprintw(20, 4, "< P0, P2 >");
            break;
        case 2:
            mvprintw(20, 4, "< P0, P3 >");
            break;
        case 3:
            mvprintw(20, 4, "< P1, P2 >");
            break;
        case 4:
            mvprintw(20, 4, "< P1, P3 >");
            break;
        case 5:
            mvprintw(20, 4, "< P2, P3 >");
            break;
        }
    }
*/
    attroff(A_REVERSE);

    mvprintw(position*3+5,2,"->");

    switch (logicmode)
    {
    case 1:
        logicboard_render_7segs(aCPU);
        break;
    case 2:
        logicboard_render_registers();
        break;
    }

    refresh();
}
