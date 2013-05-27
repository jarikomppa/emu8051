/* 8051 emulator core
 * Copyright 2006 Jari Komppa
 * Released under LGPL
 *
 * core.c
 * General emulation functions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "curses.h"
#include "emu8051.h"

int decode(struct em8051 *aCPU, int aPosition, unsigned char *aBuffer)
{
    return aCPU->dec[aCPU->mCodeMem[aPosition & (aCPU->mCodeMemSize - 1)]](aCPU, aPosition, aBuffer);
}

int tick(struct em8051 *aCPU)
{
    if (aCPU->mTickDelay)
    {
        aCPU->mTickDelay--;
        return 0;
    }
    aCPU->op[aCPU->mCodeMem[aCPU->mPC & (aCPU->mCodeMemSize - 1)]](aCPU);
    return 1;
}


void disasm_setptrs(struct em8051 *aCPU);
void op_setptrs(struct em8051 *aCPU);

void reset(struct em8051 *aCPU)
{
    // clear memory, set registers to bootup values, etc    

    memset(aCPU->mCodeMem, 0, aCPU->mCodeMemSize);
    memset(aCPU->mExtData, 0, aCPU->mExtDataSize);
    memset(aCPU->mLowerData, 0, 128);
    memset(aCPU->mSFR, 0, 128);
    if (aCPU->mUpperData) 
        memset(aCPU->mUpperData, 0, 128);
    aCPU->mPC = 0;
    aCPU->mTickDelay = 0;
    aCPU->mSFR[REG_SP] = 7;
    aCPU->mSFR[REG_P0] = 0xff;
    aCPU->mSFR[REG_P1] = 0xff;
    aCPU->mSFR[REG_P2] = 0xff;
    aCPU->mSFR[REG_P3] = 0xff;

    // build function pointer lists

    disasm_setptrs(aCPU);
    op_setptrs(aCPU);
}


/////////////////////////////////////////////////////////////////////

int readbyte(FILE * f)
{
    char data[3];
    data[0] = fgetc(f);
    data[1] = fgetc(f);
    data[2] = 0;
    return strtol(data, NULL, 16);
}

int load_obj(struct em8051 *aCPU, char *aFilename)
{
    FILE *f;
    f = fopen(aFilename, "r");
    if (!f) return -1;
    while (!feof(f))
    {
        char sig;
        int recordlength;
        int address;
        int recordtype;
        int checksum;
        int i;
        sig = fgetc(f);
        if (sig != ':')
            return -1; // unsupported file format
        recordlength = readbyte(f);
        address = readbyte(f);
        address <<= 8;
        address |= readbyte(f);
        recordtype = readbyte(f);
        if (recordtype == 1)
            return 0; // we're done
        if (recordtype != 0)
            return -1; // unsupported record type
        checksum = recordtype + recordlength + (address & 0xff) + (address >> 8); // final checksum = 1 + not(checksum)
        for (i = 0; i < recordlength; i++)
        {
            int data = readbyte(f);
            checksum += data;
            aCPU->mCodeMem[address + i] = data;
        }
        i = readbyte(f);
        if (i != (256 - (checksum&0xff)))
            return -1; // checksum failure
        fgetc(f); // skip newline
    }
    return -1;
}

void setspeed(int speed, int runmode)
{   
    switch (speed)
    {
    case 7:
        slk_set(5, "+/-|0.5Hz", 1);
        break;
    case 6:
        slk_set(5, "+/- | 1Hz", 1);
        break;
    case 5:
        slk_set(5, "+/- | 2Hz", 1);
        break;
    case 4:
        slk_set(5, "+/- |10Hz", 1);
        break;
    case 3:
        slk_set(5, "+/- |fast", 1);
        break;
    case 2:
        slk_set(5, "+/- | f+", 1);
        break;
    case 1:
        slk_set(5, "+/- | f++", 1);
        break;
    case 0:
        slk_set(5, "+/- | f*", 1);
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

int main(void) {
    
    WINDOW *codebox, *codeoutput;
    WINDOW *regbox, *regoutput;
    WINDOW *rambox, *ramview;
    WINDOW *stackbox, *stackview;
    WINDOW *pswbox, *pswoutput;
    WINDOW *ioregbox, *ioregoutput;
    WINDOW *spregbox, *spregoutput;
    WINDOW *miscbox, *miscview;
    int      ch;
    struct em8051 emu;
    int clocks = 0, i;
    int runmode = 0;
    int speed = 6;
    int ticked = 1;

    emu.mCodeMem     = malloc(65536);
    emu.mCodeMemSize = 65536;
    emu.mExtData     = malloc(65536);
    emu.mExtDataSize = 65536;
    emu.mLowerData   = malloc(128);
    emu.mUpperData   = malloc(128);
    emu.mSFR         = malloc(128);
    reset(&emu);
    i = 0x100;

    if (load_obj(&emu, "spede.obj") != 0)
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

    codebox = subwin(stdscr, 8, 42, 16, 0);
    box(codebox,0,0);
    mvwaddstr(codebox, 0, 2, "PC");
    mvwaddstr(codebox, 0, 9, "Opcodes");
    mvwaddstr(codebox, 0, 20, "Assembly");
    mvwaddstr(codebox, 6, 0, ">");
    mvwaddstr(codebox, 6, 41, "<");
    codeoutput = subwin(stdscr, 6, 39, 17, 2);
    wprintw(codeoutput,"\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");

    regbox = subwin(stdscr, 8, 38, 16, 42);
    box(regbox,0,0);
    mvwaddstr(regbox, 0, 2, "A -R0-R1-R2-R3-R4-R5-R6-R7-B -DPTR");
    mvwaddstr(regbox, 6, 0, ">");
    mvwaddstr(regbox, 6, 37, "<");
    regoutput = subwin(stdscr, 6, 35, 17, 44);
    wprintw(regoutput,"\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");

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
    wprintw(ioregoutput,"\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");

    pswbox = subwin(stdscr, 8, 19, 0, 61);
    box(pswbox,0,0);
    mvwaddstr(pswbox, 0, 2, "C-ACF0R1R0Ov--P");
    mvwaddstr(pswbox, 6, 0, ">");
    mvwaddstr(pswbox, 6, 18, "<");
    pswoutput = subwin(stdscr, 6, 16, 1, 63);
    wprintw(pswoutput,"\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");

    spregbox = subwin(stdscr, 8, 43, 8, 37);
    box(spregbox,0,0);
    mvwaddstr(spregbox, 0, 2, "TIMOD-TCON--TH0-TL0--TH1-TL1--SCON-PCON");
    mvwaddstr(spregbox, 6, 0, ">");
    mvwaddstr(spregbox, 6, 42, "<");
    spregoutput = subwin(stdscr, 6, 40, 9, 39);
    wprintw(spregoutput,"\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");

    miscbox = subwin(stdscr, 6, 31, 10, 0);
    box(miscbox,0,0);
    miscview = subwin(stdscr, 4, 28, 11, 2);
    

    slk_set(1, "(h)elp", 1);
    slk_set(2, "(l)oad", 1);
    slk_set(3, "spc=step", 1);
    slk_set(4, "(r)un", 1);
    slk_set(6, "1-9)view", 1);
    slk_set(7, "home=rst", 1);
    slk_set(8, "s-Q)quit", 1);

    setspeed(speed, runmode);

    scrollok(codeoutput, TRUE);
    scrollok(regoutput, TRUE);
    scrollok(pswoutput, TRUE);
    scrollok(spregoutput, TRUE);
    scrollok(ioregoutput, TRUE);

    refresh();


    /*  Loop until user hits 'q' to quit  */

    while ( (ch = getch()) != 'Q' ) 
    {
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
            reset(&emu);
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
                if (ticked && speed != 0)
                {
                    l = decode(&emu, emu.mPC, temp);
                    wprintw(codeoutput,"\n%04X   ", emu.mPC & 0xffff);
                    for (m = 0; m < l; m++)
                        wprintw(codeoutput,"%02X ", emu.mCodeMem[emu.mPC+m]);
                    for (m = l; m < 3; m++)
                        wprintw(codeoutput,"   ");
                    wprintw(codeoutput,"  %s",temp);

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
                clocks += 12;
                ticked = tick(&emu);
            }

            wclear(miscview);
            wprintw(miscview, "Cycles: %9d\n\n\nTime  : %9.3fms (@12MHz)", clocks, 1000.0f * clocks * (1.0f/(12.0f*1000*1000)));
            wnoutrefresh(miscview);

            if (speed != 0)
            {
                wnoutrefresh(codeoutput);
                wnoutrefresh(regoutput);
                wnoutrefresh(ioregoutput);
                wnoutrefresh(spregoutput);
                wnoutrefresh(pswoutput);
            }
        }

        wclear(ramview);
        for (i = 0; i < 8; i++)
        {
            wprintw(ramview,"%02X  %02X %02X %02X %02X %02X %02X %02X %02X\n", 
                i*8, 
                emu.mLowerData[i*8+0], emu.mLowerData[i*8+1], emu.mLowerData[i*8+2], emu.mLowerData[i*8+3],
                emu.mLowerData[i*8+4], emu.mLowerData[i*8+5], emu.mLowerData[i*8+6], emu.mLowerData[i*8+7]);
        }

        wclear(stackview);
        for (i = 0; i < 14; i++)
        {
            wprintw(stackview," %02X\n", 
                emu.mLowerData[(i+emu.mSFR[REG_SP]-8)&0xff]);
        }

        if (speed != 0)
        {
            wnoutrefresh(ramview);
            wnoutrefresh(stackview);
        }
        doupdate();
    }


    /*  Clean up after ourselves  */

    delwin(codebox);
    delwin(codeoutput);
    endwin();
//    refresh();

    return EXIT_SUCCESS;
}