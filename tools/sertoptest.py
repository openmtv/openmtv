#!/usr/bin/env python3
"""Checks of the series card's topicality (--series-topicality, src/fixes.c
series_top_*); verify.sh step 15.

The film agency of tests/scenarios/room_dealer_buy.txt in German, on the
decompiled build and the logical clock, the mouse on dealer slot 10 at
frame 5700, the screen at 5760:
1. "Ein Colt für alle Fälle" (series 30, one-line episode title; its five
   episodes poked to quality 40 and topicality 20, in both runs), without
   the option: the original's card -- row 11 (y 92..99) empty on the dark
   band, the last row (y 100..108) light, the shadow at y 110..111;
2. the same with the option: row 11 a light band with the bar (20 red
   pixels, as the film card draws text 417 with 20), the last row dark,
   the card as high and the same above row 11;
3. "Im Reich der wilden Tiere" (series 20) at episode 10, whose title takes
   two lines: without the option the original's 12 rows; with it 13 -- the
   bar (fresh: full, 39 red pixels as on the film card) on a light band at
   y 100..107, the last row dark at y 108..116, the shadow at y 118..119,
   and the same above y 100.

4. the programme planner (tests/scenarios/room_planner_schedule.txt up to
   frame 3600, English): it draws its cards into one window bitmap it
   allocates on entry -- the original's 0x3A88 bytes take its film and
   series cards (18 x 13 cells) exactly, the series card with the band is
   up to 18 x 15.  window_create (105E:3002) halts the game where a window is
   larger than that buffer; called there directly: without the option
   18 x 13 fits and 18 x 14 halts, with it 18 x 15 fits.  (Before, the
   taller card wrote up to 2304 bytes past the buffer, over the next heap
   block: wrong pictures, then a hang or an exit some time later.)

The pokes need the series table's segment: it lies where the texts loaded
before it end (German 4F04, English 4EC7), so a first run reads the game's
far pointer g_series (DS:2646) in the film agency and the pokes use that.
"""
import os, shutil, subprocess, sys, tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mmpxtest import read_png

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.environ.get('GAME', os.path.join(ROOT, '..'))
BIN = os.path.join(ROOT, 'build', 'mtv-dec')
LOAD_SEG = 0x01B0                        # the harness's segments are relative to the load segment

RED, LIGHT, DARK, BLACK = (222, 36, 36), (178, 178, 178), (129, 129, 129), (0, 0, 0)


def series_at(seg, n, field):
    return '%04X:%04X' % (seg, 8 + n * 0x39 + field)


def script(d, which, worn, seg=None):
    """seg None: a probe -- the script up to the agency and a peek of g_series"""
    """the dealer scenario up to the second visit, slot 10 = series 30 or 20"""
    lines = []
    for l in open(os.path.join(ROOT, 'tests', 'scenarios', 'room_dealer_buy.txt')):
        if l.startswith('#') or not l.strip():
            continue
        f = l.split()
        if int(f[0]) >= 5700 or f[1] in ('hash', 'peek8', 'peek16', 'peek32', 'quit'):
            continue
        if f[1] == 'poke16' and f[2].startswith('4DA5:'):   # (the English film table's: not needed, elsewhere in German)
            continue
        if which == 20 and f[1] == 'poke16':          # slot 10: series 20 instead of 30
            if f[2] == '39D4:1A3D':
                l = '5200 poke16 39D4:1A3D 1020\n'
            elif f[2].startswith('4EC7:'):
                continue
        elif f[1] == 'poke16' and f[2].startswith('4EC7:'):
            if seg is None:
                continue
            l = '%s poke16 %04X:%s %s\n' % (f[0], seg, f[2][5:], f[3])
        lines.append(l if l.endswith('\n') else l + '\n')
    if seg is None:
        return ''.join(lines + ['5650 peek32 39D4:2646\n', '5651 quit\n'])
    if which == 20:
        lines += ['5200 poke16 %s 2560\n' % series_at(seg, 20, 0x24),
                  '5600 poke16 %s 10\n' % series_at(seg, 20, 0x31)]        # its next episode: 10
    if worn:
        lines += ['5600 poke16 %s %d\n' % (series_at(seg, 30, e * 3), 40 + 20 * 256) for e in range(1, 6)]
    lines += ['5700 goto 168 75\n', '5760 shot %s/card.png\n' % d, '5770 quit\n']
    lines.sort(key=lambda l: int(l.split()[0]))
    return ''.join(lines)


def planner_window(tmp, rows, on):
    """window_create of 18 x ROWS cells in the planner: None if it fits, else the halt"""
    lines = []
    for l in open(os.path.join(ROOT, 'tests', 'scenarios', 'room_planner_schedule.txt')):
        f = l.split()
        if l.startswith('#') or not f or int(f[0]) >= 3700 or f[1] in ('hash', 'peek8', 'peek16', 'peek32', 'quit'):
            continue
        lines.append(l.rstrip('\n'))
    lines += ['3700 call 105E:3002,1,1,18,%d' % rows, '3701 quit']
    d = os.path.join(tmp, 'planner_%d_%s' % (rows, 'on' if on else 'off'))
    g = os.path.join(d, 'game')
    os.makedirs(g)
    for f in ('SOURCE1.RSC', 'SOURCE2.RSC'):
        shutil.copy(os.path.join(GAME, f), g)
    path = os.path.join(d, 'script.txt')
    open(path, 'w').write('\n'.join(lines) + '\n')
    p = subprocess.run([BIN, '--headless', '--nosound', '--logical-time', '--game', g, '--args', 'e', '--script', path,
                        '--series-topicality=' + ('yes' if on else 'no')], capture_output=True, text=True)
    if 'CALL frame 3700 ' in p.stdout and p.returncode == 0:
        return None
    halt = [l for l in p.stderr.splitlines() if 'HALT' in l]
    return halt[0] if halt else 'exit %d' % p.returncode


def game(d, text, on):
    g = os.path.join(d, 'game')
    os.makedirs(g)
    for f in ('SOURCE1.RSC', 'SOURCE2.RSC'):
        shutil.copy(os.path.join(GAME, f), g)
    path = os.path.join(d, 'script.txt')
    open(path, 'w').write(text)
    p = subprocess.run([BIN, '--headless', '--nosound', '--logical-time', '--game', g, '--args', 'd', '--script', path,
                        '--series-topicality=' + ('yes' if on else 'no')], capture_output=True, text=True)
    if p.returncode:
        sys.exit('sertoptest: exit %d\n%s' % (p.returncode, p.stderr[-2000:]))
    return p.stdout


def series_segment(tmp):
    """the series table's segment, relative to the load segment, from g_series in the agency"""
    d = os.path.join(tmp, 'probe')
    out = game(d, script(d, 30, False), False)
    for l in out.splitlines():
        if l.startswith('PEEK frame 5650 39D4:2646 '):
            return (int(l.split()[-1]) >> 16) - LOAD_SEG
    sys.exit('sertoptest: no g_series in the probe run\n' + out[-1000:])


def run(tmp, seg, name, which, on, worn=False):
    d = os.path.join(tmp, name)
    game(d, script(d, which, worn, seg), on)
    return read_png(os.path.join(d, 'card.png'))[2]


def px(rows, x, y):
    return tuple(rows[y][3 * x:3 * x + 3])


def band(rows, y0, y1):
    """the background of rows y0..y1-1 inside the card (x 10..148): text and bars aside"""
    return {px(rows, x, y) for y in range(y0, y1) for x in range(10, 149)} - {BLACK, RED, (161, 218, 242)}


def red_in_bar(rows, y):
    return sum(px(rows, x, y) == RED for x in range(17, 56))


def same(a, b, y0, y1):
    return all(px(a, x, y) == px(b, x, y) for y in range(y0, y1) for x in range(9, 152))


def main():
    if not os.path.exists(BIN):
        sys.exit('sertoptest: no %s (make build/mtv-dec)' % BIN)
    fails = 0

    def check(ok, what):
        nonlocal fails
        print('%-72s %s' % (what, 'ok' if ok else 'FAIL'))
        fails += not ok

    with tempfile.TemporaryDirectory(prefix='sertop_') as tmp:
        seg = series_segment(tmp)
        print('the series table at %04X:0008' % seg)
        off = run(tmp, seg, 'colt_off', 30, False, worn=True)
        on = run(tmp, seg, 'colt_on', 30, True, worn=True)
        check(band(off, 92, 100) == {DARK} and band(off, 100, 109) == {LIGHT} and px(off, 100, 110) == BLACK,
              'without the option: the original\'s card (row 11 empty and dark, the last light)')
        check(same(off, on, 10, 92), 'with it: the same card above row 11')
        check(band(on, 92, 100) == {LIGHT} and band(on, 100, 109) == {DARK}, 'row 11 light, the last row dark')
        check(red_in_bar(on, 94) == 20, 'the bar: topicality 20 of quality 40 -> 20 pixels (%d)' % red_in_bar(on, 94))
        check(px(on, 100, 110) == BLACK and px(on, 100, 112) != BLACK, 'the card as high as the original\'s (12 rows)')
        off = run(tmp, seg, 'long_off', 20, False)
        on = run(tmp, seg, 'long_on', 20, True)
        check(px(off, 100, 110) == BLACK and band(off, 92, 100) == {DARK}, 'a two-line episode title without it: 12 rows, row 11 its second line')
        check(same(off, on, 10, 100), 'with it: the same card above row 12')
        check(band(on, 100, 108) == {LIGHT} and red_in_bar(on, 102) == 39, 'row 12: the full bar on a light band (39 pixels, as the film card)')
        check(band(on, 108, 117) == {DARK} and px(on, 100, 118) == BLACK and px(on, 100, 119) == BLACK,
              'the last row dark, the card one row taller (its shadow at 118..119)')
        check(planner_window(tmp, 13, False) is None, 'the planner without it: a window of 18 x 13 cells fits its buffer')
        r = planner_window(tmp, 14, False)
        check(r is not None and 'does not fit the window buffer' in r, 'the planner without it: 18 x 14 halts' + ('' if r else ' (it did not)'))
        r = planner_window(tmp, 15, True)
        check(r is None, 'the planner with it: the tallest series card, 18 x 15, fits (%s)' % r)
    print('sertoptest: %s' % ('all ok' if not fails else '%d FAILED' % fails))
    sys.exit(1 if fails else 0)


if __name__ == '__main__':
    main()
