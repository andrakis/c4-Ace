/* Xlib.h -- all that's left of X here: what lib/help.c and lib/table.c name,
   which xwin-c4.c carries out on Ace's own framebuffer. */
#ifndef ACE_XLIB_H
#define ACE_XLIB_H

typedef int Display;
typedef int Window;
typedef int GC;
typedef int Pixmap;

typedef struct {
  int ascent, descent, fid, width;
} XFontStruct;

typedef struct {
  int lbearing, rbearing, width;
} XCharStruct;

XFontStruct *XLoadQueryFont(Display *d, char *name);
int XTextWidth(XFontStruct *f, char *s, int n);
void XTextExtents(XFontStruct *f, char *s, int n, int *dir, int *asc, int *desc, XCharStruct *cs);
void XSetForeground(Display *d, GC g, int c);
void XSetFont(Display *d, GC g, int f);
void XDrawString(Display *d, Window w, GC g, int x, int y, char *s, int n);
void XFillRectangle(Display *d, Window w, GC g, int x, int y, int rw, int rh);
void XDrawPoint(Display *d, Window w, GC g, int x, int y);
void XDrawLine(Display *d, Window w, GC g, int x0, int y0, int x1, int y1);

#endif
