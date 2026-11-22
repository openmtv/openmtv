#!/usr/bin/env python3
"""Checks of the rule "Unfertige Drehbuecher integrieren" (--original-screenplays, src/origscripts.c);
verify.sh step 20.  The original game files in a temporary directory, the decompiled
build, the logical clock:
1. without the rule (a new game): screenplays 30 (a show) and 44 (a series), series 44,
   film 140 as the 1991 data has them -- not offered, no price, no list, 0 episodes;
2. with it: 30 offered (flags 6) at $480 000 with its list (base 64, role 1 first; its second
   production's the same: a show's ten productions each have the first one's) and its roles'
   requirements, 44 offered (flags 3) with a list for each of its 8 episodes, series
   44 with 8 episodes of one block (the eighth's base quality 38), film 140 X rated; 25 keeps
   its own price;
3. the screenplay agency (tests/scenarios/room_scripts_buy.txt, German) with the rule: the
   shelf offers some of them (the run is the same every time: 32, 29 and 25 among the
   five), and the card of 32 "Alpha Zero" shows its agency text -- the rule's, the
   original's is empty -- in its four lines (dark text pixels on the grey);
4. savegames: with the rule the shipped slot 1 (a savegame without it) loaded from the
   setup gets the rule's lists, saved as slot 7; without the rule --load-slot 7 has them
   all (the savegame holds the lists: the rule comes with it), --load-slot 1 has none;
   without the rule slot 7 and then slot 1 loaded from the safe: back to the original's; a
   savegame without the rule where screenplay 38 has a list of its own (as a pack can give it):
   no sign of the rule, loaded without it the original's.
"""
import os, shutil, subprocess, sys, tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mmpxtest import read_png

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.environ.get('GAME', os.path.join(ROOT, '..'))
BIN = os.path.join(ROOT, 'build', 'mtv-dec')
LOAD_SEG = 0x01B0                        # the harness's segments are relative to the load segment
failed = 0


def check(cond, what):
    global failed
    print(('ok   ' if cond else 'FAIL ') + what)
    if not cond:
        failed += 1


def run(d, lines, extra=(), lang='e'):
    path = os.path.join(d, 'script.txt')
    open(path, 'w').write('\n'.join(lines) + '\n')
    p = subprocess.run([BIN, '--headless', '--nosound', '--logical-time', '--game', d, '--args', lang, '--script', path] + list(extra),
                       capture_output=True, text=True, timeout=600)
    vals = {}
    for l in p.stdout.splitlines():
        f = l.split()
        if len(f) == 5 and f[0] == 'PEEK':
            vals.setdefault(f[3], int(f[4]))
    return p.returncode, vals, p.stderr


def game_dir(tmp, name):
    d = os.path.join(tmp, name)
    os.makedirs(d)
    for f in ('SOURCE1.RSC', 'SOURCE2.RSC'):
        shutil.copy(os.path.join(GAME, f), d)
    return d


class Tables:
    """the screenplay, series and film tables' places (the game's far pointers), the peeks of a record's bytes"""
    def __init__(self, sp, se, fi):
        self.ptr = {'sp': sp, 'se': se, 'fi': fi}

    def addr(self, t, i, off):
        v = self.ptr[t]
        size = {'sp': 0x1B3, 'se': 0x39, 'fi': 0x17}[t]
        return '%04X:%04X' % ((v >> 16) - LOAD_SEG, (v & 0xFFFF) + i * size + off)

    FIELDS = {'sp30 flags': ('sp', 30, 5, 8), 'sp30 price': ('sp', 30, 0, 32), 'sp30 base': ('sp', 30, 8, 8), 'sp30 item1': ('sp', 30, 12, 8), 'sp30 prod2 base': ('sp', 30, 8 + 0x23, 8),
              'sp30 humor1': ('sp', 30, 0x168, 8), 'sp44 flags': ('sp', 44, 5, 8), 'sp44 ep8 base': ('sp', 44, 8 + 7 * 0x23, 8),
              'sp25 price': ('sp', 25, 0, 32), 'se44 episodes': ('se', 44, 0x22, 8), 'se44 blocks': ('se', 44, 0x23, 8),
              'fi140 flags2': ('fi', 140, 7, 8)}

    def peeks(self, frame):
        return ['%d peek%d %s' % (frame, bits, self.addr(t, i, off)) for (t, i, off, bits) in self.FIELDS.values()]

    def values(self, vals):
        return {k: vals.get(self.addr(t, i, off).replace(':', ':')) for k, (t, i, off, _) in self.FIELDS.items()}


ORIGINAL = {'sp30 flags': 0, 'sp30 price': 0, 'sp30 base': 0, 'sp30 item1': 0, 'sp30 prod2 base': 0, 'sp30 humor1': 0, 'sp44 flags': 1, 'sp44 ep8 base': 0,
            'sp25 price': 25000, 'se44 episodes': 0, 'se44 blocks': 0, 'fi140 flags2': 4}
RULE = {'sp30 flags': 6, 'sp30 price': 480000, 'sp30 base': 64, 'sp30 item1': 1, 'sp30 prod2 base': 64, 'sp30 humor1': 20, 'sp44 flags': 3, 'sp44 ep8 base': 38,
        'sp25 price': 25000, 'se44 episodes': 8, 'se44 blocks': 1, 'fi140 flags2': 12}


def same(got, want):
    return all(got.get(k) == v for k, v in want.items())


def diff(got, want):
    return ', '.join('%s %s (not %s)' % (k, got.get(k), v) for k, v in want.items() if got.get(k) != v) or 'all as expected'


def main():
    if not os.path.exists(BIN):
        print('build/mtv-dec is not built')
        return 1
    tmp = tempfile.mkdtemp(prefix='mtv_origscripts_')
    try:
        d = game_dir(tmp, 'new')
        rc, v, err = run(d, ['300 peek32 39D4:715C', '300 peek32 39D4:2646', '300 peek32 39D4:24A9', '301 quit'], ['--new-game'])
        if rc or len(v) != 3:
            print('FAIL the tables\' places (exit %d)\n%s' % (rc, err[-1500:]))
            return 1
        tb = Tables(v['39D4:715C'], v['39D4:2646'], v['39D4:24A9'])
        # 1, 2: a new game without and with the rule
        for opt, want, what in (('no', ORIGINAL, 'without the rule: the 1991 data'), ('yes', RULE, 'with the rule: filled and offered')):
            rc, v, err = run(d, tb.peeks(300) + ['301 quit'], ['--new-game', '--original-screenplays=' + opt])
            got = tb.values(v)
            check(rc == 0 and same(got, want), 'a new game %s (%s)' % (what, diff(got, want)))
        # 3: the agency with the rule, German
        d = game_dir(tmp, 'agency')
        lines = [l.rstrip('\n') for l in open(os.path.join(ROOT, 'tests', 'scenarios', 'room_scripts_buy.txt'))
                 if l.strip() and not l.startswith('#') and int(l.split()[0]) < 3100 and l.split()[1] not in ('hash', 'quit')]
        offers = ['3050 peek16 39D4:%04X' % (0x196A + 9 * i) for i in range(1, 6)]
        shot = os.path.join(d, 'card.png')
        rc, v, err = run(d, lines + offers + ['3050 goto 124 48', '3110 shot ' + shot, '3111 quit'], ['--original-screenplays=yes'], 'd')
        shelf = [v.get('39D4:%04X' % (0x196A + 9 * i)) for i in range(1, 6)]
        check(rc == 0 and {32, 29, 25} <= set(shelf), 'the agency with the rule offers some of them (the shelf: %s)' % shelf)
        if os.path.exists(shot):
            w, h, px = read_png(shot)
            at = lambda x, y: tuple(px[y][3 * x:3 * x + 3])
            dark = sum(1 for y in range(28, 60) for x in range(164, 300) if at(x, y)[0] < 40 and at(x, y)[1] < 40 and at(x, y)[2] < 40)
            red = sum(1 for x in range(164, 300) if at(x, 23) == (222, 36, 36))
            check(red > 50 and dark > 150, 'the card of 32 "Alpha Zero": its title band and the agency text in its lines (%d dark pixels, %d red)' % (dark, red))
        else:
            check(False, 'the card of 32: no screenshot (exit %d)' % rc)
        # 4: savegames
        d = game_dir(tmp, 'saves')
        save7 = ['400 key esc', '1000 click 160 100', '1200 click 100 28', '1600 click 76 35', '1800 click 134 75', '2000 click 220 45',
                 '2100 key S', '2110 key e', '2120 key v', '2130 key e', '2140 key n', '2250 key enter']
        rc, v, err = run(d, save7 + tb.peeks(2400) + ['2401 quit'], ['--original-screenplays=yes'])
        got = tb.values(v)
        check(rc == 0 and same(got, RULE), 'with the rule: the shipped slot 1 (without it) loaded gets the rule\'s lists, saved as slot 7 (%s)' % diff(got, RULE))
        rc, v, err = run(d, tb.peeks(800) + ['801 quit'], ['--original-screenplays=no', '--load-slot', '7'])
        got = tb.values(v)
        check(rc == 0 and same(got, RULE), 'without the rule --load-slot 7: the savegame brings the rule\'s screenplays (%s)' % diff(got, RULE))
        rc, v, err = run(d, tb.peeks(800) + ['801 quit'], ['--original-screenplays=no', '--load-slot', '1'])
        got = tb.values(v)
        check(rc == 0 and same(got, ORIGINAL), 'without the rule --load-slot 1: the original\'s (%s)' % diff(got, ORIGINAL))
        rc, v, err = run(d, ['600 click 76 35', '800 click 193 75', '1000 click 100 28'] + tb.peeks(1300) + ['1301 quit'],
                         ['--original-screenplays=no', '--load-slot', '7'])
        got = tb.values(v)
        check(rc == 0 and same(got, ORIGINAL), 'without the rule slot 7, then slot 1 from the safe: back to the original\'s (%s)' % diff(got, ORIGINAL))
        own = tb.addr('sp', 38, 8 + 4)                     # screenplay 38's first list: item 1 (a role), weight 10
        save8 = ['400 key esc', '1000 click 160 100', '1200 click 100 28', '1400 poke16 %s %d' % (own, 1 + 10 * 256),
                 '1600 click 76 35', '1800 click 134 75', '2000 click 220 62', '2100 key E', '2110 key i', '2120 key g', '2250 key enter']
        rc, v, err = run(d, save8 + ['2400 peek8 ' + own, '2401 quit'], ['--original-screenplays=no'])
        check(rc == 0 and v.get(own) == 1, 'without the rule: screenplay 38 a list of its own, saved as slot 8 (exit %d, %s)' % (rc, v))
        rc, v, err = run(d, tb.peeks(800) + ['801 quit'], ['--original-screenplays=no', '--load-slot', '8'])
        got = tb.values(v)
        check(rc == 0 and same(got, ORIGINAL), 'without the rule --load-slot 8: a list of its own is no sign of the rule (%s)' % diff(got, ORIGINAL))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    print('origscriptstest: %s' % ('all ok' if not failed else '%d failed' % failed))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
