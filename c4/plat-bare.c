// plat-bare.c -- Ace on the bare machine and under C4DOS: the display is
// the game's to take. Under grin, C4DOS's graphical shell, the display is
// grin's, and grin publishes a table in C4DOS's slot 25 while it has it
// (c4's docs/grin-plan.md 3.6): then the game plays in a window of grin's,
// and grin's loop runs in its polls. As games/tyrian/tyr-bare.c does.

#include <stdlib.h>
#define C4IX_SHARED 1
#include "gui.h"
#undef C4IX_SHARED
#include "plat.h"

// grin's table (c4's src/grin/gkit.h): its first eight words
struct grin_table {
	int magic, version, gen;
	void *env;
	void (*hello)(void);
	int  (*fwin_open)(char *title, int w, int h, int flags);
	void (*fwin_frame)(char *pixels, int w, int h, int pitch, int *pal);
	int  (*fwin_poll)(int *ev);
};
extern int *__c4dos_api;                // bare.c's: C4DOS's table, 0 on the bare machine

static struct grin_table *grin(void) {
	struct grin_table *t;
	if (!__c4dos_api || __c4dos_api[0] != 4404292 || __c4dos_api[5] < 4) return 0;
	t = (struct grin_table *) __c4dos_api[25];
	return t && t->magic == 1196575054 && t->version >= 2 ? t : 0;
}

static struct grin_table *g;

int  ace_take_display(void) { return !grin(); }
void ace_give_display(void) {}
// grin's window, the table's size (grin wants its frames that size). Flags
// 2: the game has the mouse, with the pointer free, for cards to be clicked.
int  ace_win_open(char *title, int w, int h) { return (g = grin()) != 0 && g->fwin_open(title, w, h, 2); }
void ace_win_frame(char *pixels, int w, int h, int pitch, int *pal) {
	if (g) g->fwin_frame(pixels, w, h, pitch, pal);
}
int  ace_win_poll(int *ev) { return g ? g->fwin_poll(ev) : 0; }
void ace_win_close(void) {}

int  ace_plat_ms(void) { return __time(); }
void ace_plat_sleep(int ms) { if (ms) __c4_usleep(ms * 1000); }
void ace_wait(int ms, int *ring) { (void) ring; __c4_usleep(ms * 1000); }
