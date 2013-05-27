/* 8051 emulator 
 * Copyright 2006 Jari Komppa
 * Released under GPL
 *
 * popups.c
 * Popup windows for the curses-based emulator front-end
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "curses.h"
#include "emu8051.h"
#include "emulator.h"

void emu_popup(char *aTitle, char *aMessage)
{
    WINDOW * exc;
    nocbreak();
    cbreak();
    nodelay(stdscr, FALSE);
    halfdelay(1);
    while (getch() > 0) {}

    runmode = 0;
    setSpeed(speed, runmode);
    exc = subwin(stdscr, 5, 40, (LINES-5)/2, (COLS-40)/2);
    wattron(exc,A_REVERSE);
    werase(exc);
    box(exc,ACS_VLINE,ACS_HLINE);
    mvwaddstr(exc, 0, 2, aTitle);
    wattroff(exc,A_REVERSE);
    wmove(exc, 2, 2);

    waddstr(exc, aMessage);

    wmove(exc, 4, 4);
    wattron(exc,A_REVERSE);
    waddstr(exc, "Press any key to continue");
    wattroff(exc,A_REVERSE);

    wrefresh(exc);

    getch();
    delwin(exc);
    refreshview();
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
    setSpeed(speed, runmode);
    exc = subwin(stdscr, 7, 50, (LINES-6)/2, (COLS-50)/2);
    wattron(exc,A_REVERSE);
    werase(exc);
    box(exc,ACS_VLINE,ACS_HLINE);
    mvwaddstr(exc, 0, 2, "Exception");
    wattroff(exc,A_REVERSE);
    wmove(exc, 2, 2);

    switch (aCode)
    {
    case -1: waddstr(exc,"Breakpoint reached");
             break;
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
    wmove(exc, 6, 12);
    wattron(exc,A_REVERSE);
    waddstr(exc, "Press any key to continue");
    wattroff(exc,A_REVERSE);

    wrefresh(exc);

    getch();
    delwin(exc);
    change_view(MAIN_VIEW);
}

void emu_load(struct em8051 *aCPU)
{
    WINDOW * exc;
    int pos = 0;
    int ch = 0;
    int result;
    pos = (int)strlen(filename);

    runmode = 0;
    setSpeed(speed, runmode);
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
    waddstr(exc, filename);
    wrefresh(exc);

    while (ch != '\n')
    {
        ch = getch();
        if (ch > 31 && ch < 127 || ch > 127 && ch < 255)
        {
            if (pos < 44)
            {
                filename[pos] = ch;
                pos++;
                filename[pos] = 0;
                waddch(exc,ch);
                wrefresh(exc);
            }
        }
        if (ch == 8)
        {
            if (pos > 0)
            {
                pos--;
                filename[pos] = 0;
                wmove(exc,2,3+pos);
                waddch(exc,'_');
                wmove(exc,2,3+pos);
                wrefresh(exc);
            }
        }
    }

    result = load_obj(aCPU, filename);
    delwin(exc);
    refreshview();

    switch (result)
    {
    case -1:
        emu_popup("Load error", "File not found.");
        break;
    case -2:
        emu_popup("Load error", "Bad file format.");
        break;
    case -3:
        emu_popup("Load error", "Unsupported HEX file version.");
        break;
    case -4:
        emu_popup("Load error", "Checksum failure.");
        break;
    case -5:
        emu_popup("Load error", "No end of data marker found.");
        break;
    }
}

int emu_readvalue(struct em8051 *aCPU, const char *aPrompt, int aOldvalue, int aValueSize)
{
    WINDOW * exc;
    int pos = 0;
    int ch = 0;
    char temp[16];
    pos = aValueSize;    

    runmode = 0;
    setSpeed(speed, runmode);
    if (aValueSize == 2)
        exc = subwin(stdscr, 5, 30, (LINES-6)/2, (COLS-30)/2);
    else
        exc = subwin(stdscr, 5, 50, (LINES-6)/2, (COLS-50)/2);
    wattron(exc,A_REVERSE);
    werase(exc);
    box(exc,ACS_VLINE,ACS_HLINE);
    mvwaddstr(exc, 0, 2, aPrompt);
    wattroff(exc,A_REVERSE);
    wmove(exc, 2, 2);
    if (aValueSize == 2)
    {
        sprintf(temp, "%02X", aOldvalue);
        waddstr(exc,"[__]"); 
        wmove(exc,2,3);
        waddstr(exc, temp);
    }
    else
    {
        sprintf(temp, "%04X", aOldvalue);
        waddstr(exc,"[____]"); 
        wmove(exc,2,3);
        waddstr(exc, temp);
    }
    wrefresh(exc);

    do
    {
        if (aValueSize == 2)
        {
            int currvalue = strtol(temp, NULL, 16);
            wmove(exc,2,5 + aValueSize);
            wprintw(exc,"%d %d %d %d %d %d %d %d",
                (currvalue >> 7) & 1,
                (currvalue >> 6) & 1,
                (currvalue >> 5) & 1,
                (currvalue >> 4) & 1,
                (currvalue >> 3) & 1,
                (currvalue >> 2) & 1,
                (currvalue >> 1) & 1,
                (currvalue >> 0) & 1);
        }
        else
        {
            int currvalue = strtol(temp, NULL, 16);
            char assembly[64];
            decode(aCPU, currvalue, assembly);
            wmove(exc,2,5 + aValueSize);
            waddstr(exc, "                                        ");
            wmove(exc,2,5 + aValueSize);
            waddstr(exc, assembly);
        }
        wmove(exc,2,3 + pos);
        wrefresh(exc);
        ch = getch();
        if (ch >= '0' && ch <= '9' || ch >= 'a' && ch <= 'f' || ch >= 'A' && ch <= 'F')
        {
            if (pos < aValueSize)
            {
                temp[pos] = ch;
                pos++;
                temp[pos] = 0;
                waddch(exc,ch);
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
            }
        }
    }
    while (ch != '\n');

    delwin(exc);
    refreshview();
    return strtol(temp, NULL, 16);
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
    setSpeed(speed, runmode);
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
    refreshview();
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
    setSpeed(speed, runmode);
    exc = subwin(stdscr, 14, 70, (LINES-14)/2, (COLS-70)/2);
    wattron(exc,A_REVERSE);
    werase(exc);
    box(exc,ACS_VLINE,ACS_HLINE);
    mvwaddstr(exc, 0, 2, "Help");
    wattroff(exc,A_REVERSE);
    wrefresh(exc);

    wmove(exc, 2, 2);
    waddstr(exc, "8051 Emulator v. 0.3 - http://iki.fi/sol/");
    wmove(exc, 3, 2);
    waddstr(exc, "Copyright (c) 2006 Jari Komppa");
    wmove(exc, 13, 22);
    wattron(exc,A_REVERSE);
    waddstr(exc, "Press any key to continue");
    wattroff(exc,A_REVERSE);


    mvwaddstr(exc, 5, 6, "h - This help");
    mvwaddstr(exc, 6, 6, "l - Load intel hex file");
    mvwaddstr(exc, 7, 2, "space - Single step");
    mvwaddstr(exc, 8, 6, "r - Toggle run mode");
    mvwaddstr(exc, 9, 2, "+ & - - Adjust run speed");
    mvwaddstr(exc, 10, 6, "v - Change views");
    mvwaddstr(exc, 11, 3, "home - Reset (with options)");

    mvwaddstr(exc, 5, 32, "shift-q - Quit");
    mvwaddstr(exc, 6, 32, "cursors - Move cursor");
    mvwaddstr(exc, 7, 32, "0-9,a-f - Adjust values");
    mvwaddstr(exc, 8, 36, "tab - Switch mem editor modes");
    mvwaddstr(exc, 9, 36, "end - Reset tick/time counter");
    mvwaddstr(exc, 10, 38, "k - Set or clear breakpoint");
    mvwaddstr(exc, 11, 38, "g - Go to address (adjust PC)");

    wrefresh(exc);

    ch = getch();

    delwin(exc);
    refreshview();
}
