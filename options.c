/* 8051 emulator 
 * Copyright 2006 Jari Komppa
 * Released under GPL
 *
 * options.c
 * Options view
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "curses.h"
#include "emu8051.h"
#include "emulator.h"

static int activerow = 0;

int opt_exception_iret_sp = 1;
int opt_exception_iret_acc = 1;
int opt_exception_iret_psw = 1;
int opt_exception_acc_to_a = 1;
int opt_exception_stack = 1;
int opt_exception_invalid = 1;
int opt_clock_select = 3;
int opt_clock_hz = 12*1000*1000;

int clockspeeds[] = { 
    33*1000*1000,
    24*1000*1000, 
    22*1000*1000, 
    12*1000*1000, 
    11*1000*1000, 
       73728*100,
     6*1000*1000,
     5*1000*1000,
       49152*100,
     3*1000*1000,
       1000*1000,
        100*1000
};

void wipe_options_view()
{
}

void build_options_view(struct em8051 *aCPU)
{
    erase();
}

void options_editor_keys(struct em8051 *aCPU, int ch)
{
    switch (ch)
    {
    case KEY_UP:
        activerow--;
        if (activerow < 0) 
            activerow = 0;
        if (activerow == 2 || activerow == 3)
            activerow = 1;
        break;
    case KEY_DOWN:
        activerow++;
        if (activerow > 9) 
            activerow = 9;
        if (activerow == 2 || activerow == 3)
            activerow = 4;
        break;
    case KEY_LEFT:
        switch (activerow)
        {
        case 1:
            opt_clock_select--;
            if (opt_clock_select < 0)
                opt_clock_select = 0;
            opt_clock_hz = clockspeeds[opt_clock_select];
            break;
        case 4:
            opt_exception_iret_sp = !opt_exception_iret_sp;
            break;
        case 5:
            opt_exception_iret_acc = !opt_exception_iret_acc;
            break;
        case 6:
            opt_exception_iret_psw = !opt_exception_iret_psw;
            break;
        case 7:
            opt_exception_acc_to_a = !opt_exception_acc_to_a;
            break;
        case 8:
            opt_exception_stack = !opt_exception_stack;
            break;
        case 9:
            opt_exception_invalid = !opt_exception_invalid;
            break;
        }
        break;
    case KEY_RIGHT:
        switch (activerow)
        {
        case 1:
            opt_clock_select++;
            if (opt_clock_select > 12)
                opt_clock_select = 12;
            if (opt_clock_select == 12)
                opt_clock_hz = emu_readhz(aCPU, "Enter custom clock speed", opt_clock_hz);
            else
                opt_clock_hz = clockspeeds[opt_clock_select];
            if (opt_clock_hz == 0) 
                opt_clock_hz = 1;
            break;
        case 4:
            opt_exception_iret_sp = !opt_exception_iret_sp;
            break;
        case 5:
            opt_exception_iret_acc = !opt_exception_iret_acc;
            break;
        case 6:
            opt_exception_iret_psw = !opt_exception_iret_psw;
            break;
        case 7:
            opt_exception_acc_to_a = !opt_exception_acc_to_a;
            break;
        case 8:
            opt_exception_stack = !opt_exception_stack;
            break;
        case 9:
            opt_exception_invalid = !opt_exception_invalid;
            break;
        }
        break;
    }
}

void options_update(struct em8051 *aCPU)
{
    int i;
    mvprintw(1, 1, "Options");
    for (i = 0; i < 10; i++)
        mvprintw(i * 2 + 3, 2, "  ");
    attron(A_REVERSE);
    mvprintw(3, 4, "< Hardware: 'super' 8051 >");
    mvprintw(5, 4, "< Clock at % 8.3f MHz >", opt_clock_hz / (1000*1000.0f));
    /*
    mvprintw(7, 4, "< option >");
    mvprintw(9, 4, "< option >");
    */
    mvprintw(11, 4, "< Interrupt handler sp watch exception %s >", opt_exception_iret_sp?"enabled ":"disabled");
    mvprintw(13, 4, "< Interrupt handler acc watch exception %s >", opt_exception_iret_acc?"enabled ":"disabled");
    mvprintw(15, 4, "< Interrupt handler psw watch exception %s >", opt_exception_iret_psw?"enabled ":"disabled");
    mvprintw(17, 4, "< Acc-to-a opcode exception %s >", opt_exception_acc_to_a?"enabled ":"disabled");
    mvprintw(19, 4, "< Stack exception %s >", opt_exception_stack?"enabled ":"disabled");
    mvprintw(21, 4, "< Illegal opcode exception %s >", opt_exception_invalid?"enabled ":"disabled"); 
    attroff(A_REVERSE);
    mvprintw(activerow * 2 + 3, 2, "->");
}

