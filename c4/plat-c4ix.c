// plat-c4ix.c -- Ace on C4IX: the display is the desktop's, which gives it
// way until the game ends (include/display.h), or else the Terminal window
// the game was started from (window.h), as wolf3d --window plays. gui.h for
// its declarations only: xwin-c4.c carries its functions.

#include <stdlib.h>
#include <string.h>
#define C4IX_SHARED 1
#include "gui.h"
#undef C4IX_SHARED
#include "display.h"
#include "window.h"
#include "plat.h"

int uevwait(int ms, int *ring, int pipes);     // libc4ix (c4ix_user.h)

int  ace_take_display(void) { return udisplay_take(0); }
void ace_give_display(void) { udisplay_give(); }

// The desktop scales what it is given to the window, so the table goes in
// the middle of a frame the window's own shape, on the table's colour (index
// 0, the first colour xwin-c4.c asks for), and its cards keep theirs. The
// desktop reads the frame when it next draws, and says so (WIN_FRAME); until
// then there is nothing to send.
static int waiting, again, closed;
static char *f_px, *frame;
static int f_w, f_h, f_pitch, *f_pal, ox, oy;
static int canvas_w = WIN_W, canvas_h = WIN_H;

int ace_win_open(char *title, int w, int h) {
	frame = (char *) calloc(WIN_W * WIN_H, 1);
	if (!frame) return 0;
	ox = (WIN_W - w) / 2;
	oy = (WIN_H - h) / 2;
	return win_open(title);
}

static void send_frame(void) {
	int y;
	for (y = 0; y < f_h; y++)
		memcpy(frame + (y + oy) * WIN_W + ox, f_px + y * f_pitch, f_w);
	win_fb(frame, WIN_W, WIN_H, WIN_W, f_pal);
	win_present();
	waiting = 1;
	again = 0;
}

void ace_win_frame(char *pixels, int w, int h, int pitch, int *pal) {
	if (closed) return;
	f_px = pixels; f_w = w; f_h = h; f_pitch = pitch; f_pal = pal;
	if (waiting) again = 1;
	else send_frame();
}

// the next event, in the display's form; the mouse in the table's pixels,
// from the window's, which the desktop may have scaled the frame to
int ace_win_poll(int *ev) {
	int w[3], k;
	if (closed) return 0;
	while (1) {
		k = win_poll(w);
		if (k == WIN_NONE) return 0;
		if (k < 0) { closed = 1; return -1; }
		if (k == WIN_FRAME) {
			waiting = 0;
			if (again) send_frame();
			continue;
		}
		if (k == WIN_SIZE) {
			canvas_w = w[0] > 0 ? w[0] : WIN_W;
			canvas_h = w[1] > 0 ? w[1] : WIN_H;
			continue;
		}
		if (k == WIN_DOWN || k == WIN_UP || k == WIN_MOVE) {
			ev[0] = k == WIN_DOWN ? GUI_EV_DOWN : k == WIN_UP ? GUI_EV_UP : GUI_EV_MOVE;
			ev[1] = w[0] * WIN_W / canvas_w - ox;
			ev[2] = w[1] * WIN_H / canvas_h - oy;
			ev[3] = w[2];
			return 1;
		}
		if (k == WIN_KEY || k == WIN_KEYUP) {
			ev[0] = k == WIN_KEY ? GUI_EV_KEYDOWN : GUI_EV_KEYUP;
			ev[1] = w[0]; ev[2] = w[1]; ev[3] = w[2];
			return 1;
		}
	}
}

void ace_win_close(void) { if (!closed) { win_close(); closed = 1; } }

int  ace_plat_ms(void) { return __time(); }
void ace_plat_sleep(int ms) { if (ms) __c4_usleep(ms * 1000); }
void ace_wait(int ms, int *ring) { uevwait(ms, ring, ring ? 0 : 1); }
