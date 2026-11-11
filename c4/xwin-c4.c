// xwin-c4.c -- what lib/xwin.c does with X, here with no X: Ace draws into
// an 8-bit framebuffer of its own, and what changed goes to libjs's display,
// to a C4IX window or to one of grin's. The colours are found as they are
// asked for (pixel_for): all of Ace's images come to 212. X's pixmaps and
// masks are arrays of bytes; X's fonts are font.h; lib/help.c's X calls, by
// way of c4/X11/Xlib.h, are at the end.
//
// The images are not PNGs by now: c4/imglib.py decoded them on the host,
// and an image's file_data is its palette and its runs.
//
// -demo plays the script compiled into the game (c4/scripts/NAME.txt), on a
// clock of its own, and prints what the screen holds after each event: the
// lines make test-ace compares, gcc's with c4lc's. -seed N fixes the deal.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct pixels_type {
  unsigned char *px;       // w x h colour indices
  unsigned char *mask;     // w x h, 1 where the image shows; or 0
  unsigned char *rot;      // turned half way round, as PUT_ROTATED wants
  unsigned char *rotmask;
  unsigned char *inv;      // its colours inverted, as PUT_INVERTED wants
} pixels_type;
#define PIXELS_TYPE pixels_type *

#include "imagelib.h"
#include "table.h"
#include "cards.h"
#include "xwin.h"
#include "gui.h"
#include "font.h"
#include "X11/Xlib.h"
#include "plat.h"

// the largest table a game may have, which is all but one wants anyway; and
// the window's, where the display is another's. The same for every build:
// a game lays itself out by them.
enum { DISPLAY_W = 800, DISPLAY_H = 600, WINDOW_W = 640, WINDOW_H = 400, RING = 65536 };

static int opt_window, opt_fullscreen, opt_demo, opt_seed, opt_shotat;
static char *opt_shot;
static OptionDesc xwin_options_list[] = {
  { "-window", OPTION_BOOLEAN, &opt_window },
  { "-fullscreen", OPTION_BOOLEAN, &opt_fullscreen },
  { "-demo", OPTION_BOOLEAN, &opt_demo },
  { "-seed", OPTION_INTEGER, &opt_seed },
  { "-shot", OPTION_STRING, &opt_shot },
  { "-shotat", OPTION_INTEGER, &opt_shotat },
  { 0, 0, 0 }
};
OptionDesc *xwin_options = xwin_options_list;

int table_background, help_background;
int font_width, font_height;
Display *display;
Window window;
GC gc;

static char *name;
static int on;          // the display is ours
static int want_win;    // a window will be, once the table's size is known
static int win;         // a window is
static int *ring;

static image_list display_list;
static image display_img;
image *display_image;
static pixels_type display_pixels;

static unsigned char *fb;
static int fbw, fbh;

// what is to be drawn in: the clip rectangle, in the framebuffer
static int clip_x0, clip_y0, clip_x1, clip_y1;

// what has changed since the display last heard
static int dirty, dirty_x0, dirty_y0, dirty_x1, dirty_y1;

static void mark(int x0, int y0, int x1, int y1)
{
  if (!dirty) {
    dirty = 1;
    dirty_x0 = x0; dirty_y0 = y0; dirty_x1 = x1; dirty_y1 = y1;
    return;
  }
  if (x0 < dirty_x0) dirty_x0 = x0;
  if (y0 < dirty_y0) dirty_y0 = y0;
  if (x1 > dirty_x1) dirty_x1 = x1;
  if (y1 > dirty_y1) dirty_y1 = y1;
}

/************************************************************************/
/*      Colours                                                         */
/************************************************************************/

static int pal[256];
static int npal;

int pixel_for(int r, int g, int b)
{
  static int last_rgb = -1, last_i;
  int rgb = (r << 16) | (g << 8) | b, i, best = 0, best_d = 0x7fffffff;
  if (rgb == last_rgb)
    return last_i;
  for (i = 0; i < npal; i++)
    if (pal[i] == rgb)
      break;
  if (i == npal) {
    if (npal < 256) {
      pal[npal] = rgb;
      npal++;
    } else {
      // full, which none of the games comes near: the nearest
      for (i = 0; i < 256; i++) {
        int dr = ((pal[i] >> 16) & 255) - r, dg = ((pal[i] >> 8) & 255) - g, db = (pal[i] & 255) - b;
        int d = dr * dr + dg * dg + db * db;
        if (d < best_d) { best_d = d; best = i; }
      }
      i = best;
    }
  }
  last_rgb = rgb;
  last_i = i;
  return i;
}

/************************************************************************/
/*      Drawing                                                         */
/************************************************************************/

// w x h from (x, y) of src, a sw x sh array, to (tx, ty) of dst, dw x dh,
// only where mask says, and inside the clip if dst is the framebuffer
static void copy_rect(unsigned char *src, unsigned char *mask, int sw, int sh,
                      int x, int y, int w, int h,
                      unsigned char *dst, int dw, int dh, int tx, int ty)
{
  int x0 = 0, y0 = 0, x1 = dw, y1 = dh, i, j;
  if (dst == fb) { x0 = clip_x0; y0 = clip_y0; x1 = clip_x1; y1 = clip_y1; }
  if (x < 0) { w += x; tx -= x; x = 0; }
  if (y < 0) { h += y; ty -= y; y = 0; }
  if (x + w > sw) w = sw - x;
  if (y + h > sh) h = sh - y;
  if (tx < x0) { w -= x0 - tx; x += x0 - tx; tx = x0; }
  if (ty < y0) { h -= y0 - ty; y += y0 - ty; ty = y0; }
  if (tx + w > x1) w = x1 - tx;
  if (ty + h > y1) h = y1 - ty;
  if (w <= 0 || h <= 0) return;
  if (dst == fb) mark(tx, ty, tx + w, ty + h);
  for (j = 0; j < h; j++) {
    unsigned char *s = src + (y + j) * sw + x;
    unsigned char *d = dst + (ty + j) * dw + tx;
    if (!mask) {
      memcpy(d, s, w);
    } else {
      unsigned char *m = mask + (y + j) * sw + x;
      for (i = 0; i < w; i++)
        if (m[i]) d[i] = s[i];
    }
  }
}

// a rectangle of one colour, as copy_rect clips it
static void fill_rect(unsigned char *dst, int dw, int dh, int x, int y, int w, int h, int c)
{
  int x0 = 0, y0 = 0, x1 = dw, y1 = dh, j;
  if (dst == fb) { x0 = clip_x0; y0 = clip_y0; x1 = clip_x1; y1 = clip_y1; }
  if (x < x0) { w -= x0 - x; x = x0; }
  if (y < y0) { h -= y0 - y; y = y0; }
  if (x + w > x1) w = x1 - x;
  if (y + h > y1) h = y1 - y;
  if (w <= 0 || h <= 0) return;
  if (dst == fb) mark(x, y, x + w, y + h);
  for (j = 0; j < h; j++)
    memset(dst + (y + j) * dw + x, c, w);
}

// text at (x, baseline), in the framebuffer: through the clip, on bg if it
// is not -1
static void draw_text(int font, int x, int baseline, int fg, int bg, char *s, int n)
{
  int w = font_w[font], asc = font_ascent[font], h = asc + font_descent[font];
  const unsigned short *rows = font_rows[font];
  int i, j, k;
  for (i = 0; i < n; i++, x += w) {
    int c = s[i] & 255;
    const unsigned short *g;
    if (bg != -1) fill_rect(fb, fbw, fbh, x, baseline - asc, w, h, bg);
    if (c < 32 || c > 126) continue;
    g = rows + (c - 32) * h;
    for (j = 0; j < h; j++) {
      int y = baseline - asc + j, bits = g[j];
      if (!bits || y < clip_y0 || y >= clip_y1) continue;
      for (k = 0; k < w; k++)
        if ((bits & (0x8000 >> k)) && x + k >= clip_x0 && x + k < clip_x1) {
          fb[y * fbw + x + k] = fg;
          mark(x + k, y, x + k + 1, y + 1);
        }
    }
  }
}

void xwin_clip(int x, int y, int w, int h)
{
  clip_x0 = x < 0 ? 0 : x;
  clip_y0 = y < 0 ? 0 : y;
  clip_x1 = x + w > fbw ? fbw : x + w;
  clip_y1 = y + h > fbh ? fbh : y + h;
}

void xwin_noclip()
{
  clip_x0 = 0; clip_y0 = 0; clip_x1 = fbw; clip_y1 = fbh;
}

void clear(int x, int y, int w, int h)
{
  fill_rect(fb, fbw, fbh, x, y, w, h, help_is_showing ? help_background : table_background);
}

void text(char *t, int x, int y)
{
  draw_text(1, x, y - font_descent[1], pixel_for(255, 255, 255), table_background, t, strlen(t));
}

void beep() {}

/************************************************************************/
/*      Images                                                          */
/************************************************************************/

static void build_image(image *src)
{
  const unsigned char *p;
  unsigned char map[256];
  pixels_type *t;
  int n, at, i, npix, has_mask;

  t = (pixels_type *) malloc(sizeof(pixels_type));
  memset(t, 0, sizeof(pixels_type));
  src->pixels = t;
  n = src->width * src->height;
  if (src->synth_func) {
    t->px = (unsigned char *) malloc(n);
    memset(t->px, 0, n);
    src->synth_func(src);
    return;
  }
  p = src->file_data;
  if (!p) return;
  npix = p[0] | (p[1] << 8);
  p += 2;
  for (i = 0; i < npix; i++, p += 3)
    map[i] = pixel_for(p[0], p[1], p[2]);
  has_mask = *p++;
  t->px = (unsigned char *) malloc(n);
  if (has_mask) t->mask = (unsigned char *) malloc(n);
  for (at = 0; at < n; at += p[0], p += 2) {
    int run = p[0], v = p[1];
    if (v == npix) {
      memset(t->px + at, 0, run);
      memset(t->mask + at, 0, run);
    } else {
      memset(t->px + at, map[v], run);
      if (has_mask) memset(t->mask + at, 1, run);
    }
  }
}

static unsigned char *turned(unsigned char *a, int n)
{
  unsigned char *b = (unsigned char *) malloc(n);
  int i;
  for (i = 0; i < n; i++) b[i] = a[n - 1 - i];
  return b;
}

static unsigned char *inverted(unsigned char *a, int n)
{
  unsigned char *b = (unsigned char *) malloc(n);
  unsigned char map[256];
  int i, have = npal;
  // what the image holds is among the colours so far
  for (i = 0; i < have; i++)
    map[i] = pixel_for(255 - ((pal[i] >> 16) & 255), 255 - ((pal[i] >> 8) & 255), 255 - (pal[i] & 255));
  for (i = 0; i < n; i++) b[i] = map[a[i]];
  return b;
}

void put_image(image *src, int x, int y, int w, int h,
               image *dest, int dx, int dy, int flags)
{
  unsigned char *which, *mask;
  pixels_type *t;
  int n;

  if (!src->pixels) build_image(src);
  if (!dest->pixels) build_image(dest);
  t = src->pixels;
  if (!t->px) return;
  n = src->width * src->height;
  which = t->px;
  mask = t->mask;
  if (flags & PUT_ROTATED) {
    int nx, ny;
    if (!t->rot) t->rot = turned(t->px, n);
    if (t->mask && !t->rotmask) t->rotmask = turned(t->mask, n);
    which = t->rot;
    mask = t->rotmask;
    nx = src->width - x - w;
    ny = src->height - y - h;
    dx += x - nx;
    dy += y - ny;
    x = nx;
    y = ny;
  }
  if (flags & PUT_INVERTED) {
    if (!t->inv) t->inv = inverted(t->px, n);
    which = t->inv;
    mask = t->mask;
  }
  copy_rect(which, mask, src->width, src->height, x, y, w, h,
            dest->pixels->px, dest->width, dest->height, dx + x, dy + y);
}

void put_mask(image *src, int x, int y, int w, int h,
              image *dest, int dx, int dy, int flags)
{
  pixels_type *t;
  int n;
  if (!src->pixels) build_image(src);
  if (!dest->pixels) build_image(dest);
  t = dest->pixels;
  if (!src->pixels->px || !src->pixels->mask) return;
  if (!t->mask) {
    n = dest->width * dest->height;
    t->mask = (unsigned char *) malloc(n);
    memset(t->mask, 1, n);
  }
  copy_rect(src->pixels->mask, 0, src->width, src->height, x, y, w, h,
            t->mask, dest->width, dest->height, dx + x, dy + y);
}

void fill_image(image *dest, int x, int y, int w, int h, int r, int g, int b)
{
  if (!dest->pixels) build_image(dest);
  if (!dest->pixels->px) return;
  fill_rect(dest->pixels->px, dest->width, dest->height, x, y, w, h, pixel_for(r, g, b));
}

/************************************************************************/
/*      The display                                                     */
/************************************************************************/

static unsigned int roll, flushes;    // -demo: all that has been shown

static void present()
{
  if (!dirty) return;
  dirty = 0;
  if (opt_demo) {
    int y, x;
    unsigned int h = roll;
    h = (h ^ dirty_x0) * 16777619U;
    h = (h ^ dirty_y0) * 16777619U;
    h = (h ^ dirty_x1) * 16777619U;
    h = (h ^ dirty_y1) * 16777619U;
    for (y = dirty_y0; y < dirty_y1; y++) {
      unsigned char *row = fb + y * fbw;
      for (x = dirty_x0; x < dirty_x1; x++)
        h = (h ^ row[x]) * 16777619U;
    }
    roll = h;
    flushes++;
  }
  if (on) {
    int w = dirty_x1 - dirty_x0, h = dirty_y1 - dirty_y0;
    gui_blit8(dirty_x0, dirty_y0, w, h, (char *) fb + dirty_y0 * fbw + dirty_x0, fbw, pal, w, h);
    gui_show();
  } else if (win) {
    ace_win_frame((char *) fb, fbw, fbh, fbw, pal);
  }
}

void flush() { present(); }
void flushsync() { present(); }

static void set_size(int w, int h)
{
  unsigned char *old = fb;
  fb = (unsigned char *) malloc(w * h);
  memset(fb, table_background, w * h);
  if (old) free(old);
  fbw = w;
  fbh = h;
  display_img.width = w;
  display_img.height = h;
  display_pixels.px = fb;
  xwin_noclip();
  dirty = 0;
}

static void all_done()
{
  if (on) {
    gui_detach();
    ace_give_display();
    on = 0;
  }
  if (win) {
    ace_win_close();
    win = 0;
  }
}

/************************************************************************/
/*      The window's menus                                              */
/************************************************************************/

// In a window of its own (C4IX's desktop) a game has a menu bar. Each item
// is a key the game already takes, in its key() in games/NAME.c, so a pick
// is that key pressed where the pointer is. A row is for the games named
// in its first field, or for every game when that is 0. No game has a
// redo or a deal played again: F2 deals anew in every one, and Backspace
// undoes in those that undo. Canfield's help is commented out upstream.
struct menu_row { char *games; char *menu; char *label; int key; };
static struct menu_row menu_rows[] = {
  { 0, "Game", "New Game   F2", KEY_F(2) },
  { "solitaire canfield freecell golf spider thornq pegged taipei", "Game", "Undo   Backspace", 8 },
  { "thornq", "Game", "Hint   A", 'a' },
  { "thornq", "Game", "Hints On/Off", 'A' },
  { "minesweeper", "Game", "Autoplay   A", 'a' },
  { "minesweeper", "Game", "Autoplay On/Off", 'A' },
  { "spider", "Game", "Arrows On/Off   A", 'a' },
  { 0, "Game", "-", 0 },
  { 0, "Game", "Exit   Q", 'q' },
  { "spider", "Suits", "One Suit   1", '1' },
  { "spider", "Suits", "Two Suits   2", '2' },
  { "spider", "Suits", "Four Suits   4", '4' },
  { "minesweeper", "Level", "8x8, 6 Mines", '1' },
  { "minesweeper", "Level", "11x11, 20 Mines", '2' },
  { "minesweeper", "Level", "16x16, 45 Mines", '3' },
  { "minesweeper", "Level", "32x32, 200 Mines", '4' },
  { "pegged", "Layout", "Cross", '1' },
  { "pegged", "Layout", "Plus", '2' },
  { "pegged", "Layout", "Fireplace", '3' },
  { "pegged", "Layout", "Up Arrow", '4' },
  { "pegged", "Layout", "Pyramid", '5' },
  { "pegged", "Layout", "Diamond", '6' },
  { "pegged", "Layout", "Solitaire", '7' },
  { "pegged", "Layout", "Triangle of 5", '8' },
  { "pegged", "Layout", "Triangle of 7", '9' },
  { "taipei", "Layout", "Standard", '1' },
  { "taipei", "Layout", "Bridge", '2' },
  { "taipei", "Layout", "Castle", '3' },
  { "taipei", "Layout", "Cube", '4' },
  { "taipei", "Layout", "Glyph", '5' },
  { "taipei", "Layout", "Pyramid", '6' },
  { "taipei", "Layout", "Spiral", '7' },
  { "taipei", "Layout", "Simple", '8' },
  { "taipei", "Layout", "Smiley", '9' },
  { "taipei", "Layout", "Jason", 'j' },
  { "taipei", "Layout", "Rebecca", 'r' },
  { "solitaire freecell golf spider thornq mastermind merlin minesweeper pegged taipei", "Help", "Help   F1", KEY_F(1) },
  { 0, 0, 0, 0 }
};
enum { MENUS = 4, MENU_ITEMS = 16 };
static int menu_keys[MENUS][MENU_ITEMS];
static int menu_n[MENUS];

// name is one of the words of games
static int for_game(char *games, char *game)
{
  int n = strlen(game);
  if (!games) return 1;
  while (*games) {
    while (*games == ' ') games++;
    if (!strncmp(games, game, n) && (games[n] == ' ' || !games[n])) return 1;
    while (*games && *games != ' ') games++;
  }
  return 0;
}

static void send_menus(void)
{
  char *titles[MENUS];
  int nm = 0, i, m;
  for (i = 0; menu_rows[i].menu; i++) {
    if (!for_game(menu_rows[i].games, name)) continue;
    for (m = 0; m < nm && strcmp(titles[m], menu_rows[i].menu); m++)
      ;
    if (m == nm) {
      if (nm == MENUS) continue;
      titles[nm] = menu_rows[i].menu;
      menu_n[nm] = 0;
      ace_win_menu(nm, titles[nm]);
      nm++;
    }
    if (menu_n[m] == MENU_ITEMS) continue;
    ace_win_item(m, menu_rows[i].label);
    menu_keys[m][menu_n[m]++] = menu_rows[i].key;
  }
}

// the key an item of the menu bar stands for, or 0
static int menu_key(int m, int item)
{
  if (m < 0 || m >= MENUS || item < 0 || item >= menu_n[m]) return 0;
  return menu_keys[m][item];
}

int xwin_init(int argc, char **argv)
{
  char *sl;

  name = argv[0];
  sl = strrchr(name, '/');
  if (sl) name = sl + 1;
  // started by its path (the desktop's Files), the window has its name alone
  if (strlen(name) > 4 && !strcmp(name + strlen(name) - 4, ".c4r"))
    name[strlen(name) - 4] = 0;

  // the games that deal no cards ask rand() alone, which starts from the
  // time unless it is told otherwise
  if (opt_demo || opt_seed) srand(ace_seed());

  table_type = TABLE_COLOR;
  // the first colour, which is what a new image holds
  table_background = pixel_for(0x00, 0x66, 0x00);
  font_width = font_w[1];
  font_height = font_ascent[1] + font_descent[1];

  display_width = DISPLAY_W;
  display_height = DISPLAY_H;

  // A window, as much as one holds: where the game was started in one (a
  // Terminal of C4IX's desktop, or the window the desktop opened for it),
  // or -window says, or the display is another's. Otherwise, or with
  // -fullscreen, the whole display. -demo with no display at all has no
  // need of either, and stays off a window it was not asked for.
  if (!opt_fullscreen && !opt_demo && ace_win_here())
    opt_window = 1;
  if (!opt_window && gui_present() && ace_take_display())
    on = 1;
  else if (opt_window || (gui_present() && !opt_demo)) {
    want_win = 1;
    display_width = WINDOW_W;
    display_height = WINDOW_H;
  } else if (!opt_demo) {
    fprintf(stderr, "%s: no display to draw on\n", name);
    return 1;
  }
  if (on || want_win) atexit(all_done);
  return 0;
}

// the events X would give a new window, or one that has changed size
enum { EVQ = 8 };
static XWin_Event evq[EVQ];
static int evq_n, evq_at;

static void queue_size()
{
  memset(evq, 0, sizeof(evq));
  evq[0].type = ev_resize;
  evq[0].w = fbw;
  evq[0].h = fbh;
  evq[1].type = ev_expose;
  evq[1].w = fbw;
  evq[1].h = fbh;
  evq_n = 2;
  evq_at = 0;
}

void xwin_create(int width, int height)
{
  display_list.name = "display";
  display_list.across = 1;
  display_list.down = 1;
  display_img.list = &display_list;
  display_img.pixels = &display_pixels;
  display_image = &display_img;

  if (on) {
    ring = (int *) malloc(RING);
    if (!ring || !gui_attach(ring, RING, width, height)) {
      fprintf(stderr, "%s: the display would not have us\n", name);
      exit(1);
    }
    gui_events(GUI_M_KEYS | GUI_M_BUTTONS | GUI_M_MOVE);
    gui_cursor(GUI_PTR_ARROW);
    gui_clear(0);
    gui_show();
  }
  if (want_win) {
    if (!ace_win_open(name, width, height)) {
      fprintf(stderr, "%s: no display to draw on, and no window\n", name);
      exit(1);
    }
    win = 1;
    send_menus();
  }
  set_size(width, height);
  queue_size();
}

void xwin_fixed_size(int width, int height)
{
  if (width == fbw && height == fbh) return;
  if (on) gui_size(width, height);
  set_size(width, height);
  queue_size();
}

/************************************************************************/
/*      The clock, and the deal                                         */
/************************************************************************/

static int demo_clock;

int ace_ms()
{
  // -demo: time passes as it is looked at, so an animation takes as many
  // frames in every build
  if (opt_demo) return demo_clock += 7;
  return ace_plat_ms();
}

void ace_pause()
{
  if (!opt_demo) ace_plat_sleep(1);
}

int ace_seed()
{
  return opt_seed ? opt_seed : (opt_demo ? 1 : ace_plat_ms());
}

/************************************************************************/
/*      -demo                                                           */
/************************************************************************/

extern char ace_demo_script[];
static char *demo_at;
static int demo_n;

// -shot FILE -shotat N: the screen after event N, as a PPM, to look at
static void demo_shot()
{
  FILE *f = fopen(opt_shot, "wb");
  int i;
  if (!f) return;
  fprintf(f, "P6\n%d %d\n255\n", fbw, fbh);
  for (i = 0; i < fbw * fbh; i++) {
    int c = pal[fb[i]];
    fputc((c >> 16) & 255, f);
    fputc((c >> 8) & 255, f);
    fputc(c & 255, f);
  }
  fclose(f);
}

static void demo_report()
{
  unsigned int h = 2166136261U, *w = (unsigned int *) fb;
  int i, n = fbw * fbh / 4;
  if (opt_shot && demo_n == opt_shotat) demo_shot();
  for (i = 0; i < n; i++)
    h = (h ^ w[i]) * 16777619U;
  for (i = 0; i < npal; i++)
    h = (h ^ pal[i]) * 16777619U;
  printf("ace %d %d %d %08x %08x\n", demo_n, (int) flushes, npal, roll, h);
  demo_n++;
}

static int demo_int(char **p)
{
  char *s = *p;
  int v = 0, neg = 0;
  while (*s == ' ') s++;
  if (*s == '-') { neg = 1; s++; }
  while (*s >= '0' && *s <= '9') v = v * 10 + *s++ - '0';
  *p = s;
  return neg ? -v : v;
}

// d x y b, u x y b, m x y: the mouse; k c: a key, by its code; w ms: time
// goes by. # starts a comment. After the last, the game ends.
static int demo_next(XWin_Event *ev)
{
  char *p;
  int op;
  present();
  demo_report();
  if (on) ace_plat_sleep(150);
  if (!demo_at) demo_at = ace_demo_script;
  while (1) {
    p = demo_at;
    if (!*p) {
      printf("ace end\n");
      exit(0);
    }
    op = *p;
    if (op != '\n' && op != '#') p++;
    memset(ev, 0, sizeof(*ev));
    switch (op) {
    case 'w':
      demo_clock += demo_int(&p);
      op = 0;
      break;
    case 'd':
    case 'u':
      ev->type = op == 'd' ? ev_buttondown : ev_buttonup;
      ev->x = demo_int(&p);
      ev->y = demo_int(&p);
      ev->button = demo_int(&p);
      break;
    case 'm':
      ev->type = ev_motion;
      ev->x = demo_int(&p);
      ev->y = demo_int(&p);
      break;
    case 'k':
      ev->type = ev_keypress;
      ev->key = demo_int(&p);
      break;
    default:
      op = 0;
      break;
    }
    while (*p && *p != '\n') p++;
    if (*p) p++;
    demo_at = p;
    if (op) {
      demo_clock += 50;
      ev->time = demo_clock;
      return ev->type;
    }
  }
}

/************************************************************************/
/*      Events                                                          */
/************************************************************************/

static int key_of(int code, int ch, int mods)
{
  if (ch > 0 && ch < 256) {
    if ((mods & 2) && ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')))
      return ch & 31;
    return ch;
  }
  switch (code) {
  case 8: return 8;
  case 9: return 9;
  case 13: return 13;
  case 27: return 27;
  case 46: return KEY_DELETE;
  case 37: return KEY_LEFT;
  case 38: return KEY_UP;
  case 39: return KEY_RIGHT;
  case 40: return KEY_DOWN;
  case 33: return KEY_PGUP;
  case 34: return KEY_PGDN;
  case 36: return KEY_HOME;
  }
  if (code >= 112 && code <= 123) return KEY_F(code - 111);
  return 0;
}

static int shifted;
static int mouse_x, mouse_y;
static int held[4];          // an event read too soon (a move's successor)
static int have_held;

static int next_gui(int *e)
{
  int k;
  if (have_held) {
    memcpy(e, held, sizeof(held));
    have_held = 0;
    return 1;
  }
  if (on) return gui_poll(e);
  k = ace_win_poll(e);
  return k;
}

int xwin_nextevent(XWin_Event *ev)
{
  static int click_button;
  int e[4], k;

  if (evq_at < evq_n) {
    *ev = evq[evq_at];
    evq_at++;
    return ev->type;
  }
  if (opt_demo) return demo_next(ev);

  present();
  while (1) {
    if (!(on || win)) exit(0);
    k = next_gui(e);
    if (k < 0) {
      ev->type = ev_quit;
      return ev_quit;
    }
    if (!k) {
      ace_wait(50, on ? gui_events_ring() : 0);
      continue;
    }
    memset(ev, 0, sizeof(*ev));
    switch (e[0]) {
    case GUI_EV_MOVE:
      // only the last of a run of moves, as XCheckMaskEvent gave
      while ((k = next_gui(held)) > 0) {
        if (held[0] != GUI_EV_MOVE) { have_held = 1; break; }
        memcpy(e, held, sizeof(held));
      }
      if (k < 0) {
        ev->type = ev_quit;
        return ev_quit;
      }
      mouse_x = e[1];
      mouse_y = e[2];
      if (!e[3]) break;
      ev->type = ev_motion;
      ev->x = e[1];
      ev->y = e[2];
      ev->button = click_button;
      ev->time = ace_plat_ms();
      return ev_motion;
    case GUI_EV_DOWN:
      ev->type = ev_buttondown;
      ev->x = e[1];
      ev->y = e[2];
      click_button = e[3] + 1;
      if (shifted) click_button++;
      ev->button = click_button;
      ev->time = ace_plat_ms();
      return ev_buttondown;
    case GUI_EV_UP:
      ev->type = ev_buttonup;
      ev->x = e[1];
      ev->y = e[2];
      ev->button = click_button;
      ev->time = ace_plat_ms();
      return ev_buttonup;
    case GUI_EV_KEYDOWN:
      if (e[1] == 16) shifted = 1;
      ev->key = key_of(e[1], e[2], e[3]);
      if (!ev->key) break;
      ev->type = ev_keypress;
      ev->x = mouse_x;
      ev->y = mouse_y;
      ev->time = ace_plat_ms();
      return ev_keypress;
    case GUI_EV_KEYUP:
      if (e[1] == 16) shifted = 0;
      break;
    case ACE_EV_SIZE:
      // the window's new size: a game that lays itself out (its resize
      // callback) gets a table that size, or the window's first size where
      // it is smaller; one that keeps its size has it back (table_loop). The
      // table is drawn again either way, and plat-c4ix.c fits it to the window.
      set_size(e[1] > WINDOW_W ? e[1] : WINDOW_W, e[2] > WINDOW_H ? e[2] : WINDOW_H);
      queue_size();
      *ev = evq[evq_at++];
      return ev->type;
    case ACE_EV_MENU:
      ev->key = menu_key(e[1], e[2]);
      if (!ev->key) break;
      ev->type = ev_keypress;
      ev->x = mouse_x;
      ev->y = mouse_y;
      ev->time = ace_plat_ms();
      return ev_keypress;
    }
  }
}

/************************************************************************/
/*      lib/help.c's X (c4/X11/Xlib.h)                                  */
/************************************************************************/

static int cur_fg, cur_font;
static XFontStruct x_fonts[ACE_FONTS];

// help.c asks by an XLFD name: big, bold, slanted or none of those
XFontStruct *XLoadQueryFont(Display *d, char *n)
{
  int i = 0;
  XFontStruct *f;
  if (strstr(n, "-180-")) i = 3;
  else if (strstr(n, "-bold-")) i = 1;
  else if (strstr(n, "-i-") || strstr(n, "-o-")) i = 2;
  f = x_fonts + i;
  f->fid = i;
  f->width = font_w[i];
  f->ascent = font_ascent[i];
  f->descent = font_descent[i];
  return f;
}

int XTextWidth(XFontStruct *f, char *s, int n) { return f->width * n; }

void XTextExtents(XFontStruct *f, char *s, int n, int *dir, int *asc, int *desc, XCharStruct *cs)
{
  cs->lbearing = 0;
  cs->rbearing = f->width * n;
  cs->width = f->width * n;
}

void XSetForeground(Display *d, GC g, int c) { cur_fg = c; }
void XSetFont(Display *d, GC g, int f) { cur_font = f; }

void XDrawString(Display *d, Window w, GC g, int x, int y, char *s, int n)
{
  draw_text(cur_font, x, y, cur_fg, -1, s, n);
}

void XFillRectangle(Display *d, Window w, GC g, int x, int y, int rw, int rh)
{
  fill_rect(fb, fbw, fbh, x, y, rw, rh, cur_fg);
}

void XDrawPoint(Display *d, Window w, GC g, int x, int y)
{
  fill_rect(fb, fbw, fbh, x, y, 1, 1, cur_fg);
}

// help.c's lines run along the axes
void XDrawLine(Display *d, Window w, GC g, int x0, int y0, int x1, int y1)
{
  int t;
  if (x0 > x1) { t = x0; x0 = x1; x1 = t; }
  if (y0 > y1) { t = y0; y0 = y1; y1 = t; }
  fill_rect(fb, fbw, fbh, x0, y0, x1 - x0 + 1, y1 - y0 + 1, cur_fg);
}
