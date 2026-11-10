# The Ace of Penguins on c4

DJ Delorie's solitaires and puzzles for X, on upstream's 1.5~rc2 (branch `c4`,
from the history Debian keeps at salsa.debian.org/games-team/ace-of-penguins).
Each game runs on C4IX, the bare machine and C4DOS. Build it with c4's Makefile:
this repository's `c4.mk`, `c4-images.sh` and `c4ix.vfs.txt` are its part there
(c4's `games/README.md`).

**The games:** see `ACE_GAMES` in `c4.mk`. `taipedit`, Taipei's level editor,
stays out, because it draws with X directly.

## What changed

Upstream keeps X behind `lib/`, so the games are almost as they were.

- `c4/xwin-c4.c` takes the place of `lib/xwin.c`. Ace draws into an 8-bit
  framebuffer of its own, with pixmaps and masks as arrays of bytes, and sends
  the frame to a C4IX window or one of grin's, or what changed to the display
  (`gui_blit8`). In C4IX a game started in a window -- a Terminal of the
  desktop's, or a window the desktop opened for it alone -- stays there
  (`window.h`'s `win_here`); on the console, or with `-fullscreen`, it takes
  the display.
- `lib/help.c` stays, drawing through `c4/X11/Xlib.h`, the dozen X calls it
  makes, also in `xwin-c4.c`.
- X's fonts are `c4/font.h`: the public-domain misc-fixed fonts, generated once
  by `c4/pcf2font.py`. Help pages keep their layout code, but every face is
  fixed-width.
- `lib/stack.c` asks `xwin-c4.c` for the clock, and for the seed of its shuffle.
- `lib/table_rn.c`'s `srand` now holds (upstream's first `rand` overwrote it with
  the time).
- Small changes for c4lc: `table.c`'s tentative array has a size, `pegged.c`'s
  bitfields are bytes, and `funcs.c` and `imagelib.c` have their declarations.
- `games/taipeilib.c` compares the tile image's width with 243, but the image
  is 234 wide, which left the tile's size unset; it says 234 now.

Those edits are on this branch; `git diff upstream/1.5_rc2 c4 -- lib games`
shows them all.

## The images

Upstream generates C from its PNGs when it builds, and decodes them at run time
with libpng. Here `c4/imglib.py`, on the host and with Python's standard library,
decodes them when the game builds and writes each one as run-length colour
indices with its own palette, in `.ace/gen` in c4's tree. `c4/text2c.py` does
the same for the help pages. Nothing generated is committed. The cards themselves
are drawn at run time from those images, as upstream's `card_synth` always did.

The PNGs are upstream's source, not data fetched from elsewhere: `COPYING` is
GPLv2, no separate art licence exists, and Debian's `debian/copyright` gives
`Files: *` as GPL-2+. So there is no `NAME-data` and nothing for
`games/fetch.sh` to do: the images are part of the game, and they are tracked
here only because upstream's history tracks them.

## Playing

`solitaire` and the rest are on the `PATH` in C4IX and on the games disk under
C4DOS. The mouse plays them; `h` is help, `q` quits. Upstream's options still
work (`solitaire -vegas`).

In C4IX's desktop each is in Go > Games, the card games under Cards and the
rest under Puzzles, and plays in a window of its own, which closes when the
game ends; a double-click on one in Files does the same (`c4ix.vfs.txt`'s
`#@app` lines, which say `window`). Typed in a Terminal, a game makes that
Terminal its canvas until it ends. On the console it takes the whole display.
`-fullscreen` takes the display anyway, and `-window` asks for a window even
where the display is free.

A window of its own has a menu bar (`window.h`'s `win_menu`): Game (New Game,
Undo where the game undoes, the game's hints or autoplay, Exit), the game's
suits, level or layout where it has them, and Help. Each item is a key the
game's `key()` already takes, listed in `c4/xwin-c4.c`'s `menu_rows`.

A window's canvas is 640x400, and a game lays its table out at that size or
less. A table bigger than that would go in a frame of the window's shape, which
the desktop scales down.

## The test

`-demo` plays the script compiled into the game (`c4/scripts/NAME.txt`, made by
`c4/mkscript.py` and committed) on a clock of its own, with a fixed deal (`-seed
N`, default 1 under `-demo`), and after each event prints a line: the number of
frames shown, the colours in use, a hash of every frame's changed pixels in turn,
and a hash of the whole screen. `make test-ace` holds the c4lc build to a gcc
`-m32` build of the same code, on the bare machine, under C4DOS and in C4IX.

`c4/web-ace.mjs` plays it in a browser with a real mouse, over CDP: on C4IX's
display with `-fullscreen` (a click, a bad drop, a legal move, help, every game
taking the display and giving it back), in the Terminal it was typed in, and in
a window of grin's.
It is run by hand against a page served from c4's tree, and is not part of
`test-ace`.

## What is not here

- `taipedit`.
- Sound: upstream's only sound is `beep()`, X's bell, and it does nothing here.
- Resizing a window: a game lays itself out once, at the display's or the
  window's size, though upstream can lay itself out again.
