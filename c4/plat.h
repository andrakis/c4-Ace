// plat.h -- what xwin-c4.c asks of where it runs: plat-c4ix.c, plat-bare.c
// (the bare machine and C4DOS, under grin or not), plat-host.c (gcc's
// build, which has no display and only runs -demo).
#ifndef ACE_PLAT_H
#define ACE_PLAT_H
int  ace_take_display(void);       // 1: the display is the game's
void ace_give_display(void);
int  ace_win_open(char *title, int w, int h);   // a window instead; 1 if there is one
void ace_win_frame(char *pixels, int w, int h, int pitch, int *pal);
int  ace_win_poll(int *ev);        // as gui_poll; -1: the window has closed
void ace_win_close(void);
int  ace_plat_ms(void);            // milliseconds
void ace_plat_sleep(int ms);
void ace_wait(int ms, int *ring);  // until ms have gone or an event has come
#endif
