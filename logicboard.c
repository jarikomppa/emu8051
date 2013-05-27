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

void wipe_logicboard_view()
{
}

void build_logicboard_view()
{
    erase();
}

void logicboard_editor_keys(struct em8051 *aCPU, int ch)
{
    int xorvalue = -1;
    switch (ch)
    {
    case KEY_DOWN:
        position++;
        if (position > 3) position = 3;
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


    mvprintw(position*3+5,2,"->");

    refresh();
}
