#!/usr/bin/env python3
"""Checks of --load-slot N (src/main_loop.c): a saved game of the office safe
straight from the start, as the launcher's safe starts it; verify.sh step 19.

The original game files in a temporary directory, the decompiled build:
1. a save of our own in slot 7 (the safe's second column, row 2): the setup's
   "Load game" and the shipped slot 1, the money poked to tell the two apart,
   the safe in the office, "Save", slot 7, the name "Seven", Enter;
2. --load-slot 7: no intro, no setup screen -- the human's office (room 1),
   the game running (g_game_mode 3) with the save's day, clock, money and
   station, the random seed offset 0 (as after a load from the setup);
3. the same values as clicking it: the setup's "Load game", slot 7;
4. --load-slot 1, the shipped save (day 1, 17:03:24 -- the clock runs on
   after the load --, $ 200 000);
5. --load-slot 4, an empty slot: nothing loaded -- the safe in load mode
   (room 58, g_room_mode 4), the game on the setup (g_game_mode 1), the
   message on stderr;
6. --load-slot 11, --load-slot with --new-game: refused (exit 2).
"""
import os, shutil, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.environ.get('GAME', os.path.join(ROOT, '..'))
DEC = os.path.join(ROOT, 'build', 'mtv-dec')

# room, g_room_mode, g_game_mode, day, clock, money (station 1), g_player, the random seed offset
PEEKS = ['peek16 39D4:838C', 'peek16 39D4:8BA4', 'peek16 39D4:7C78', 'peek16 39D4:71E4',
         'peek32 39D4:6143', 'peek32 39D4:8BFB', 'peek16 39D4:8BA8', 'peek32 39D4:8EAC']
NAMES = ['room', 'mode', 'game', 'day', 'clock', 'money', 'player', 'seed']

failed = 0


def check(cond, what):
    global failed
    print(('ok   ' if cond else 'FAIL ') + what)
    if not cond:
        failed += 1


def run(d, lines, extra=()):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write('\n'.join(lines) + '\n')
        script = f.name
    try:
        p = subprocess.run([DEC, '--headless', '--nosound', '--logical-time', '--game', d, '--args', 'e',
                            '--script', script] + list(extra), capture_output=True, text=True, timeout=300)
    finally:
        os.unlink(script)
    vals = {}
    for l in p.stdout.splitlines():
        parts = l.split()
        if len(parts) == 5 and parts[0] == 'PEEK':
            vals[NAMES[PEEKS.index(next(x for x in PEEKS if x.endswith(parts[3])))]] = int(parts[4])
    return p.returncode, vals, p.stderr


def peeks(frame):
    return ['%d %s' % (frame, x) for x in PEEKS]


def main():
    if not os.path.exists(DEC):
        print('build/mtv-dec is not built'); return 1
    d = tempfile.mkdtemp(prefix='mtv_loadslot_')
    try:
        for f in ('SOURCE1.RSC', 'SOURCE2.RSC'):
            shutil.copy(os.path.join(GAME, f), d)
        # 1. slot 7 saved: slot 1 loaded from the setup, the money poked, the safe, save, slot 7
        rc, _, err = run(d, ['400 key esc', '1000 click 160 100', '1200 click 100 28',
                             '1400 poke16 39D4:8BFB 4321', '1400 poke16 39D4:8BFD 0',
                             '1600 click 76 35', '1800 click 134 75', '2000 click 220 45',
                             '2100 key S', '2110 key e', '2120 key v', '2130 key e', '2140 key n',
                             '2250 key enter', '2300 peek16 39D4:838C', '2400 quit'])
        check(rc == 0, 'slot 7 saved (exit %d)' % rc)
        # 2. --load-slot 7
        rc, a, err = run(d, peeks(800) + ['900 quit'], ['--load-slot', '7'])
        check(rc == 0 and '[load-slot] slot 7 loaded' in err, '--load-slot 7: loaded (%s)' % ' '.join(l for l in err.splitlines() if 'load-slot' in l))
        check(a.get('room') == 1 and a.get('game') == 3 and a.get('money') == 4321 and a.get('player') == 1 and a.get('seed') == 0,
              '--load-slot 7: the office, the game running, the save\'s money, station 1, seed offset 0 (%s)' % a)
        # 3. the same as clicking it
        rc, b, _ = run(d, ['400 key esc', '1000 click 160 100', '1200 click 220 45'] + peeks(1300) + ['1400 quit'])
        keys = ['room', 'mode', 'game', 'day', 'clock', 'money', 'player', 'seed']
        check(all(a.get(k) == b.get(k) for k in keys), 'as the setup\'s "Load game" and a click on slot 7 (%s)' % b)
        # 4. the shipped slot 1
        rc, c, _ = run(d, peeks(800) + ['900 quit'], ['--load-slot', '1'])
        check(c.get('room') == 1 and c.get('game') == 3 and c.get('day') == 1 and 10200 <= c.get('clock', 0) < 10500 and c.get('money') == 200000,
              '--load-slot 1: the shipped save, day 1, from 17:03:24 on (the clock runs), $ 200 000 (%s)' % c)
        # 5. an empty slot
        rc, e, err = run(d, peeks(800) + ['900 quit'], ['--load-slot', '4'])
        check(e.get('room') == 0x3A and e.get('mode') == 4 and e.get('game') == 1 and 'slot 4 not loaded' in err,
              '--load-slot 4 (empty): the safe in load mode on the setup, the message (%s)' % e)
        # 6. refused
        rc, _, err = run(d, ['100 quit'], ['--load-slot', '11'])
        check(rc == 2 and '1 to 10' in err, '--load-slot 11 refused (%s)' % err.strip())
        rc, _, err = run(d, ['100 quit'], ['--load-slot', '2', '--new-game'])
        check(rc == 2 and 'not with --new-game' in err, '--load-slot with --new-game refused (%s)' % err.strip())
    finally:
        shutil.rmtree(d, ignore_errors=True)
    print('loadslottest: %s' % ('all ok' if not failed else '%d failed' % failed))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
