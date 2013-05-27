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
    LOGICBOARD_VIEW = 1,
    MEMEDITOR_VIEW = 2
};


// binary history buffer
extern unsigned char history[];

// last used filename
extern char filename[];

// instruction count; needed to replay history correctly
extern unsigned int icount;

// current line in the history cyclic buffers
extern int historyline;
// last known columns and rows; for screen resize detection
extern int oldcols, oldrows;
// are we in single-step or run mode
extern int runmode;
// current run speed, lower is faster
extern int speed;

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
extern void refreshview(struct em8051 *aCPU);
extern void change_view(struct em8051 *aCPU, int changeto);

// popups.c
extern void emu_help(struct em8051 *aCPU);
extern int emu_reset(struct em8051 *aCPU);
extern int emu_readvalue(struct em8051 *aCPU, const char *aPrompt, int aOldvalue, int aValueSize);
extern void emu_load(struct em8051 *aCPU);
extern void emu_exception(struct em8051 *aCPU, int aCode);
extern void emu_popup(struct em8051 *aCPU, char *aTitle, char *aMessage);

// mainview.c
extern void mainview_editor_keys(struct em8051 *aCPU, int ch);
extern void build_main_view(struct em8051 *aCPU);
extern void wipe_main_view();
extern void mainview_update(struct em8051 *aCPU);

// logicboard.c
extern void wipe_logicboard_view();
extern void build_logicboard_view(struct em8051 *aCPU);
extern void logicboard_editor_keys(struct em8051 *aCPU, int ch);
extern void logicboard_update(struct em8051 *aCPU);
extern void logicboard_tick(struct em8051 *aCPU);

// memeditor.c
extern void wipe_memeditor_view();
extern void build_memeditor_view(struct em8051 *aCPU);
extern void memeditor_editor_keys(struct em8051 *aCPU, int ch);
extern void memeditor_update(struct em8051 *aCPU);





