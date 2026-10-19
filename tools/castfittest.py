#!/usr/bin/env python3
"""Checks of the role fit in the shop (--cast-fit, src/cast_fit.c); verify.sh step 14.

The production shop with screenplay 34 ("Die Mafia-Connection", episode 1)
on the shopping list, its third page of actors (Schauspieler 11..15), role
1 (Komissar Tozzi) and then role 2 (Richter Molina) picked; an actor bought
for Molina.  The same script runs with and without --cast-fit, on the
decompiled build and the logical clock, in German:
1. the same game: the game-state hashes (STATE) and the values read are
   equal -- the row is painted over the screen buffer, no time taken;
2. the picture: the bars are gone, the panel ends 14 rows higher (its
   shadow at row 55, the room's picture below), the row's figures in the
   colours of the game's rating -- Tozzi 50 86 59 50 77 (86 green, 50 59 50
   red), Molina 64 94 76 52 94 (both 94 green, 52 red) -- and nothing
   changed outside the panel's rectangle; a scene slot (Strassenzug, the
   list scrolled on) keeps the original's panel.
"""
import os, shutil, subprocess, sys, tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mmpxtest import read_png

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.environ.get('GAME', os.path.join(ROOT, '..'))
BIN = os.path.join(ROOT, 'build', 'mtv-dec')
# the shop (tests/scenarios/room_supermarket_shop.txt) with screenplay 34 on the list
SCRIPT = '''400 key esc
1000 click 64 100
1300 click 172 185
2100 click 230 60
2500 click 230 60
2600 poke16 4F7E:39BA 772
2600 poke16 39D4:61C5 34
2600 poke16 39D4:61C9 1
2601 call 229E:09AF,6
2602 poke16 39D4:7B48 2
2602 poke16 39D4:7B54 1
2602 poke16 39D4:7B4A 0
2602 poke16 39D4:7B46 153
3600 click 25 60
4000 hash
4200 click 130 67
4600 click 84 106
5000 click 83 14
5400 click 83 14
5800 goto 318 198
6100 shot {d}/tozzi.png
6100 hash
6200 click 172 106
6500 goto 318 198
6800 shot {d}/molina.png
7000 click 280 18
7400 click 172 90
7800 goto 318 198
8100 shot {d}/bought.png
8100 hash
8100 peek32 39D4:8BFB
8200 click 16 98
8600 click 16 98
9000 click 84 106
9300 goto 318 198
9600 shot {d}/scene.png
9600 hash
9700 quit
'''
GREEN, RED, BLACK, PANEL_BG = (32, 129, 32), (222, 36, 36), (0, 0, 0), (222, 222, 222)
PANEL = (53, 3, 314, 70)                 # the panel's rectangle with its shadow (x0, y0, x1, y1 exclusive)


def run(tmp, on):
    d = os.path.join(tmp, 'on' if on else 'off')
    g = os.path.join(d, 'game')
    os.makedirs(g)
    for f in ('SOURCE1.RSC', 'SOURCE2.RSC'):
        shutil.copy(os.path.join(GAME, f), g)
    path = os.path.join(d, 'script.txt')
    open(path, 'w').write(SCRIPT.format(d=d))
    p = subprocess.run([BIN, '--headless', '--nosound', '--logical-time', '--game', g, '--args', 'd', '--script', path]
                       + (['--cast-fit'] if on else []), capture_output=True, text=True)
    if p.returncode:
        sys.exit('castfittest: exit %d\n%s' % (p.returncode, p.stderr[-2000:]))
    lines = [l.split(' vram ')[0] for l in p.stdout.splitlines() if l.startswith(('STATE', 'PEEK', 'CALL'))]
    return lines, d


def px(rows, x, y):
    return tuple(rows[y][3 * x:3 * x + 3])


def colours_in(rows, x0, x1, y0, y1):
    return {px(rows, x, y) for y in range(y0, y1) for x in range(x0, x1)}


def main():
    if not os.path.exists(BIN):
        sys.exit('castfittest: no %s (make build/mtv-dec)' % BIN)
    fails = 0

    def check(ok, what):
        nonlocal fails
        print('%-72s %s' % (what, 'ok' if ok else 'FAIL'))
        fails += not ok

    with tempfile.TemporaryDirectory(prefix='castfit_') as tmp:
        off, doff = run(tmp, False)
        on, don = run(tmp, True)
        check(off == on and len(on) >= 5, 'the same game with and without --cast-fit (%d hashes and values)' % len(on))
        for name, want in (('tozzi', [50, 86, 59, 50, 77]), ('molina', [64, 94, 76, 52, 94]), ('bought', [64, 94, 76, 52, 94])):
            _, _, a = read_png(os.path.join(doff, name + '.png'))
            _, _, b = read_png(os.path.join(don, name + '.png'))
            outside = all(px(a, x, y) == px(b, x, y) for y in range(200) for x in range(320)
                          if not (PANEL[0] <= x < PANEL[2] and PANEL[1] <= y < PANEL[3]))
            check(outside, '%s: nothing changed outside the panel' % name)
            bars = colours_in(a, 121, 152, 44, 47)    # the first actor's humor bar, without the option
            check(RED in bars and all(colours_in(b, 120 + 36 * i, 153 + 36 * i, 51, 53) == {PANEL_BG} for i in range(5)),
                  '%s: the bars gone (the panel empty under the row)' % name)
            check(px(b, 200, 53) == (129, 129, 129) and px(b, 200, 54) == BLACK and px(b, 200, 55) == BLACK
                  and PANEL_BG not in colours_in(b, 200, 201, 56, 66),
                  '%s: the panel ends at row 55, the room below' % name)
            best = max(want)
            for i, v in enumerate(want):
                cols = colours_in(b, 120 + 36 * i, 153 + 36 * i, 44, 51)
                exp = GREEN if v == best else RED if v < 60 else BLACK
                check(exp in cols and not ({GREEN, RED} - {exp}) & cols, '%s: actor %d (%d %%) in %s' % (name, i + 1, v,
                      {GREEN: 'green', RED: 'red', BLACK: 'black'}[exp]))
        _, _, a = read_png(os.path.join(doff, 'scene.png'))
        _, _, b = read_png(os.path.join(don, 'scene.png'))
        check(a == b and colours_in(a, 200, 201, 60, 66) == {PANEL_BG}, 'a scene slot (Strassenzug): the original\'s panel')
    print('castfittest: %s' % ('all ok' if not fails else '%d FAILED' % fails))
    sys.exit(1 if fails else 0)


if __name__ == '__main__':
    main()
