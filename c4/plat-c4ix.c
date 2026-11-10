// plat-c4ix.c -- Ace on C4IX: the window the game was started in (window.h),
// a Terminal or one the desktop opened for the game alone; or, on the
// console or with -fullscreen, the display, which the desktop gives way
// until the game ends (include/display.h). gui.h for its declarations only:
// xwin-c4.c carries its functions.

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
// in a window of the desktop's (a Terminal, or the game's own): stay there
int  ace_win_here(void) { return win_here(); }

// The desktop scales what it is given to the window, so the table goes in
// the middle of a frame the window's own shape, on the table's colour (index
// 0, the first colour xwin-c4.c asks for), and its cards keep theirs. A
// table no bigger than the window is a frame of the window's size, drawn as
// it is; a bigger one, a frame as big as the table needs, which the desktop
// scales down. The desktop reads the frame when it next draws, and says so
// (WIN_FRAME); until then there is nothing to send.
static int waiting, again, closed;
static char *f_px, *frame, *old_frame;
static int f_w, f_h, f_pitch, *f_pal, ox, oy;
static int fr_w = WIN_W, fr_h = WIN_H;  // the frame
static int canvas_w = WIN_W, canvas_h = WIN_H;

int ace_win_open(char *title, int w, int h) {
	(void) w; (void) h;
	return win_open(title);
}

// a frame for a table w x h: the window's shape, and at least the window's
// size. The one before stays until the next, since the desktop may draw the
// last frame it was given again meanwhile.
static int frame_for(int w, int h) {
	int fw, fh;
	fw = WIN_W;
	if (w > fw) fw = w;
	if (h * WIN_W > fw * WIN_H) fw = (h * WIN_W + WIN_H - 1) / WIN_H;
	fh = fw * WIN_H / WIN_W;
	if (fh < h) fh = h;
	if (frame && fw == fr_w && fh == fr_h) return 1;
	if (old_frame) free(old_frame);
	old_frame = frame;
	frame = (char *) calloc(fw * fh, 1);
	if (!frame) { frame = old_frame; old_frame = 0; return 0; }
	fr_w = fw; fr_h = fh;
	return 1;
}

static void send_frame(void) {
	int y;
	if (!frame_for(f_w, f_h)) return;
	ox = (fr_w - f_w) / 2;
	oy = (fr_h - f_h) / 2;
	for (y = 0; y < f_h; y++)
		memcpy(frame + (y + oy) * fr_w + ox, f_px + y * f_pitch, f_w);
	win_fb(frame, fr_w, fr_h, fr_w, f_pal);
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
			ev[1] = w[0] * fr_w / canvas_w - ox;
			ev[2] = w[1] * fr_h / canvas_h - oy;
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
