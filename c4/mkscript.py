#!/usr/bin/env python3
# mkscript.py OUTDIR -- each game's demo, c4/scripts/NAME.txt: what -demo
# plays and test-ace compares. A fixed list of the mouse's clicks and drags
# and of keys, chosen by a small generator of its own so that the scripts
# never change by themselves. They are committed; run this again only to make
# a different demo. It knows the games' layout only roughly, as a player
# aiming at the columns would: the game decides what a click means, so a
# script plays real moves and tries a good many illegal ones.
#
# The script's language is xwin-c4.c's (demo_next): d x y b, u x y b, m x y,
# k CODE, w MS.
import os, sys

M, W, H, F = 6, 73, 97, 19
KEY_DOWN, KEY_UP, KEY_PGDN = 0x202, 0x201, 0x206


class Lcg:
    def __init__(self, seed):
        self.s = seed

    def next(self, n):
        self.s = (self.s * 1103515245 + 12345) & 0x7fffffff
        return (self.s >> 8) % n


def cols(n, x0=M, dx=W + M, w=W):
    return [x0 + i * dx + w // 2 for i in range(n)]


# for each game: where the mouse goes (x positions, and the y range), how
# many moves, and the keys it may press besides. Esc and q would end it.
GAMES = {
    'solitaire': dict(xs=cols(7), y0=M + 20, y1=420, n=240, keys=[8, ord(' '), 8], restart=ord('r')),
    'canfield': dict(xs=cols(6), y0=M + 20, y1=420, n=200, keys=[8, ord(' ')], restart=None),
    'freecell': dict(xs=cols(8), y0=M + 20, y1=440, n=240, keys=[8, ord(' ')], restart=ord('r')),
    # the bottom cards of its columns, and the deck
    'golf': dict(xs=cols(7), y0=100, y1=185, n=160, keys=[8, ord('d')], restart=ord('r'), hot=[(42, 250)], hotp=35),
    # four colours and Return, by the keys, until it is over; then a click
    'mastermind': dict(xs=list(range(40, 240, 30)), y0=20, y1=210, n=200,
                       keys=[ord(c) for c in 'roygbp'] + [13, 13], keyp=85, restart=None),
    'merlin': dict(xs=[50, 150, 250], y0=50, y1=250, n=60, keys=[], restart=ord('r'), grid=100),
    # the penguin starts it again, once a mine has gone off
    'minesweeper': dict(xs=list(range(12, 400, 25)), y0=40, y1=420, n=200, keys=[ord('a')], restart=ord('r'), right=True,
                        hot=[(200, 15)], hotp=8),
    'pegged': dict(xs=list(range(30, 310, 40)), y0=30, y1=300, n=120, keys=[8], restart=ord('r')),
    'spider': dict(xs=cols(10), y0=M + 20, y1=500, n=240, keys=[8, ord(' ')], restart=ord('r')),
    'taipei': dict(xs=list(range(40, 620, 25)), y0=30, y1=420, n=160, keys=[8, ord('j')], restart=ord('r')),
    'thornq': dict(xs=cols(8, M, W + M), y0=M + 20, y1=460, n=240, keys=[8, ord(' '), ord('a')], restart=ord('r')),
}


def script(name, g):
    r = Lcg(sum(map(ord, name)) * 7919 + 1)
    out = ['# %s: made by c4/mkscript.py' % name, 'w 1000']

    def where():
        x = g['xs'][r.next(len(g['xs']))]
        if 'grid' in g:
            y = g['y0'] + g['grid'] * r.next((g['y1'] - g['y0']) // g['grid'] + 1)
        else:
            y = g['y0'] + r.next(g['y1'] - g['y0'])
        return x, y

    def click(x, y, b=1):
        out.append('d %d %d %d' % (x, y, b))
        out.append('u %d %d %d' % (x, y, b))

    # the splash goes on the first click
    click(*where())
    out.append('w 1000')
    for i in range(g['n']):
        c = r.next(100)
        x, y = where()
        if r.next(100) < g.get('hotp', 0):
            click(*g['hot'][r.next(len(g['hot']))])
        elif r.next(100) < g.get('keyp', 0):
            out.append('k %d' % g['keys'][r.next(len(g['keys']))])
        elif c < 40 and 'grid' not in g:
            # a drag, by way of two places on the way
            x2, y2 = where()
            out.append('d %d %d 1' % (x, y))
            out.append('m %d %d' % ((x + x2) // 2, (y + y2) // 2))
            out.append('m %d %d' % (x2 + 3, y2 + 2))
            out.append('u %d %d 1' % (x2 + 3, y2 + 2))
        elif c < 60 and 'grid' not in g:
            # a double click
            click(x, y)
            out.append('w 100')
            click(x, y)
        elif c < 70 and g.get('right'):
            click(x, y, 3)
        elif c < 95 or not g['keys']:
            click(x, y)
        else:
            out.append('k %d' % g['keys'][r.next(len(g['keys']))])
        # not a double click by accident
        out.append('w 1000')
        if i == g['n'] // 2:
            # the help, a page down and up, and back
            if name != 'canfield':
                out += ['k %d' % ord('h'), 'k %d' % KEY_DOWN, 'k %d' % KEY_PGDN, 'k %d' % KEY_UP, 'k 27', 'w 1000']
            if g['restart']:
                out += ['k %d' % g['restart'], 'w 1000']
                click(*where())
                out.append('w 1000')
    return '\n'.join(out) + '\n'


outdir = sys.argv[1]
for name, g in GAMES.items():
    with open(os.path.join(outdir, name + '.txt'), 'w') as f:
        f.write(script(name, g))
