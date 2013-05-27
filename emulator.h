/* 8051 emulator 
 * Copyright 2006 Jari Komppa
 * Released under GPL
 *
 * emu.c
 * Curses-based emulator front-end
 */

// how many lines of history to remember
#define HISTORY_LINES 20

enum EMU_VIEWS
{
    MAIN_VIEW = 0,
    LOGICBOARD_VIEW = 1
};

// code box (PC, opcode, assembly)
extern WINDOW *codebox, *codeoutput;
extern char *codelines[];

// Registers box (a, r0..7, b, dptr)
extern WINDOW *regbox, *regoutput;
extern char *reglines[];

// RAM view/edit box
extern WINDOW *rambox, *ramview;

// stack view
extern WINDOW *stackbox, *stackview;

// program status word box
extern WINDOW *pswbox, *pswoutput;
extern char *pswlines[];

// IO registers box
extern WINDOW *ioregbox, *ioregoutput;
extern char *ioreglines[];

// special registers box
extern WINDOW *spregbox, *spregoutput;
extern char *spreglines[];

// misc. stuff box
extern WINDOW *miscbox, *miscview;

// store the object filename
extern char filename[];

// current line in the history cyclic buffers
extern int historyline;
// last known columns and rows; for screen resize detection
extern int oldcols, oldrows;
// are we in single-step or run mode
extern int runmode;
// current run speed, lower is faster
extern int speed;
// focused component
extern int focus;
// memory window cursor position
extern int memcursorpos;
// memory window offset
extern int memoffset;
// pointer to the memory area being viewed
extern unsigned char *memarea;

// currently active view
extern int view;

// old port out values
extern int p0out;
extern int p1out;
extern int p2out;
extern int p3out;

// current clock count
unsigned int clocks;


// emu.c
extern int getTick();
extern void setSpeed(int speed, int runmode);
extern int emu_sfrread(struct em8051 *aCPU, int aRegister);
extern void refreshview();
extern void change_view(int changeto);

// popups.c
extern void emu_help();
extern int emu_reset(struct em8051 *aCPU);
extern int emu_readvalue(struct em8051 *aCPU, const char *aPrompt, int aOldvalue, int aValueSize);
extern void emu_load(struct em8051 *aCPU);
extern void emu_exception(struct em8051 *aCPU, int aCode);

// mainview.c
extern void mainview_editor_keys(struct em8051 *aCPU, int ch);
extern void build_main_view();
extern void wipe_main_view();
extern void mainview_update(struct em8051 *aCPU);

// logicboard.c
extern void wipe_logicboard_view();
extern void build_logicboard_view();
extern void logicboard_editor_keys(struct em8051 *aCPU, int ch);
extern void logicboard_update(struct em8051 *aCPU);





