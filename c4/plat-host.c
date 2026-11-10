// plat-host.c -- gcc's build (make ace-host), the reference test-ace holds
// c4lc's to: no display and no window, so only -demo runs, and the clock.

#include <unistd.h>
#include <sys/time.h>
#include "plat.h"

int  ace_take_display(void) { return 0; }
void ace_give_display(void) {}
int  ace_win_here(void) { return 0; }
int  ace_win_open(char *title, int w, int h) { (void) title; (void) w; (void) h; return 0; }
void ace_win_frame(char *pixels, int w, int h, int pitch, int *pal) {
	(void) pixels; (void) w; (void) h; (void) pitch; (void) pal;
}
int  ace_win_poll(int *ev) { (void) ev; return 0; }
void ace_win_close(void) {}
void ace_win_menu(int n, char *title) { (void) n; (void) title; }
void ace_win_item(int n, char *label) { (void) n; (void) label; }
int  ace_plat_ms(void) {
	struct timeval tv;
	gettimeofday(&tv, 0);
	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
void ace_plat_sleep(int ms) { usleep(ms * 1000); }
void ace_wait(int ms, int *ring) { (void) ring; usleep(ms * 1000); }

// what gui.c asks the machine: here, nothing is fitted
int __c4_info(void) { return 0; }
