#!/usr/bin/env python3
"""Checks of the system arrow (--mouse system, src/fixes.c); verify.sh step 9.

The harness line 'N pointer X Y' puts the system pointer over a game pixel
(as runtime/host.c does for a real pointer).  The same script runs with
--mouse free and --mouse system, on the decompiled build and the logical
clock:
1. the same game: the game-state hashes (STATE) are equal -- the arrow is
   drawn with the same calls either way, so time and the game do not change;
2. the picture: with the pointer on the game's cursor the system run leaves
   out the game's arrow there and changes nothing else; after the arrow keys
   moved the cursor away from the pointer, and while an item is carried (a
   transmitter on the map), both pictures are the same: the game's own cursor.
4. a network game, an item carried (tests/mp/ad_agency.txt: seat 1 takes an ad):
   the window draws it at the pointer (runtime/host.c draw_drag) -- left out of
   the shown seat's picture, the game the same (an input comes back from the
   server some frames later: the game's own item trailed the mouse; fixes.c
   carry_wanted) -- and what the window's present sees ("arrowstate"): the
   guest's window draws the item.
3. a network game (tests/mp/pointer.txt: two humans, the pointer over the
   network; build/openmtv serve): on each machine only the seat it shows has
   the system pointer over it -- that seat's arrow is left out where the
   bundle took the cursor, the other seat's screen is as with --mouse free
   (the arrow was once judged by seat 0's cursor and by the pointer before
   the server brought it: the two arrows took turns at every motion).
"""
import os, shutil, subprocess, sys, tempfile
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mmpxtest import read_png
import netserver  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.environ.get('GAME', os.path.join(ROOT, '..'))
BIN = os.path.join(ROOT, 'build', 'mtv-dec')
START = ['400 key esc', '1000 click 64 100', '1300 click 172 185', '2100 click 230 60', '2500 click 230 60']
# (frame, command); 'shot NAME KIND [X Y]': KIND out = the arrow at X,Y left out, same = no difference
ROOMS = [
    (2600, 'pointer 120 50'), (2650, 'shot office out 120 50'), (2650, 'hash'),
    (2660, 'key right'), (2661, 'key right'), (2662, 'key right'), (2700, 'shot office_keys same'),
    (2750, 'pointer 130 60'), (2800, 'shot office_back out 130 60'),
    (2900, 'click 20 60'),                                    # out of the office door: the building
    (3600, 'pointer 150 60'), (3700, 'shot building out 150 60'), (3700, 'hash'),
    (3900, 'key left'), (3901, 'key left'), (3950, 'shot building_keys same'),
    (4000, 'pointer 100 150'), (4050, 'shot panel out 100 150'),
    (4300, 'pointer 310 195'), (4400, 'shot corner out 303 189'), (4400, 'hash'),
]
# the transmitter map with a transmitter picked up (tests/scenarios/room_txmap_buy.txt)
CARRY = [(4460, 'pointer 250 150'), (4500, 'shot carry same'), (4500, 'hash')]


def script(tmp, name, mode, base, lines):
    out = [(int(l.split()[0]), 0, l) for l in base]
    shots = []
    for f, cmd in lines:
        p = cmd.split()
        if p[0] == 'shot':
            shots.append((p[1], p[2:]))
            cmd = 'shot ' + os.path.join(tmp, '%s_%s_%s.png' % (name, mode, p[1]))
        out.append((f, 1, '%d %s' % (f, cmd)))
    out.sort(key=lambda t: (t[0], t[1]))
    path = os.path.join(tmp, '%s_%s.txt' % (name, mode))
    open(path, 'w').write('\n'.join(t[2] for t in out) + '\n%d quit\n' % (max(t[0] for t in out) + 1))
    return path, shots


def run(tmp, name, mode, base, lines):
    g = os.path.join(tmp, 'game_%s_%s' % (name, mode))
    os.makedirs(g)
    for f in ('SOURCE1.RSC', 'SOURCE2.RSC'):
        shutil.copy(os.path.join(GAME, f), g)
    path, shots = script(tmp, name, mode, base, lines)
    p = subprocess.run([BIN, '--headless', '--nosound', '--logical-time', '--game', g, '--args', 'e',
                        '--script', path, '--mouse=' + mode], capture_output=True, text=True)
    if p.returncode:
        sys.exit('%s %s: exit %d\n%s' % (name, mode, p.returncode, p.stderr[-2000:]))
    return [l for l in p.stdout.splitlines() if l.startswith('STATE')], shots


def diff_box(a, b):
    """the bounding box (x0, y0, x1, y1) of the pixels that differ, or None"""
    _, _, ra = read_png(a)
    _, _, rb = read_png(b)
    xs, ys = [], []
    for y, (p, q) in enumerate(zip(ra, rb)):
        for x in range(len(p) // 3):
            if p[3 * x:3 * x + 3] != q[3 * x:3 * x + 3]:
                xs.append(x)
                ys.append(y)
    return (min(xs), min(ys), max(xs) + 1, max(ys) + 1) if xs else None


# the network game: (frame, seat, x, y) -- a shot of that seat's screen after its pointer
NET_SHOTS = [(6040, 1, 88, 70), (6240, 0, 200, 120), (6840, 1, 150, 100)]


def net_run(tmp, mode):
    """tests/mp/pointer.txt over the network, both clients with --mouse MODE, shots of both seats"""
    import seattest
    import mptest
    n = 'mp:pointer'
    d = os.path.join(tmp, 'net_' + mode)
    os.makedirs(d)
    base, _ = seattest.mp_script(n, d)
    lines = [l for l in open(base).read().split('\n') if l.strip()]
    port = mptest.free_port()
    srv = subprocess.Popen(netserver.command() + ['--seats', '2', '--host', '127.0.0.1',
                            '--port', str(port), '--pace', 'fast', '--window', '64'], stderr=subprocess.DEVNULL)
    try:
        procs = []
        for k in (0, 1):
            shots = ['%d %sshot %s' % (f, 'seat %d ' % s if s else '', os.path.join(tmp, 'net_%s_c%d_s%d_%d.png' % (mode, k, s, f)))
                     for f, s, _, _ in NET_SHOTS]
            sc = os.path.join(d, 'c%d.txt' % k)
            open(sc, 'w').write('\n'.join(sorted(lines + shots, key=lambda l: int(l.split()[0]))) + '\n')
            procs.append(subprocess.Popen(mptest.client_args(mptest.game_dir(d, k), sc, '1,2', port, k, n) + ['--mouse=' + mode],
                                          stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True))
        for p in procs:
            _, err = p.communicate(timeout=600)
            if p.returncode:
                sys.exit('network game %s: exit %d\n%s' % (mode, p.returncode, err[-2000:]))
    finally:
        srv.kill()


def net_carry_run(tmp, mode):
    """tests/mp/ad_agency.txt over the network until seat 1 carries an ad, its pointer moved; shots"""
    import seattest
    import mptest
    n = 'mp:ad_agency'
    d = os.path.join(tmp, 'carry_' + mode)
    os.makedirs(d)
    base, _ = seattest.mp_script(n, d)
    lines = [l for l in open(base).read().split('\n') if l.strip() and int(l.split()[0]) < 5150]
    port = mptest.free_port()
    srv = subprocess.Popen(netserver.command() + ['--seats', '2', '--host', '127.0.0.1',
                            '--port', str(port), '--pace', 'fast', '--window', '64'], stderr=subprocess.DEVNULL)
    states, arrows = [], []
    try:
        procs = []
        for k in (0, 1):
            extra = ['5060 seat 1 pointer 150 70', '5100 hash', '5101 seat 1 hash', '5100 arrowstate', '5150 quit'] + \
                    ['5100 %sshot %s' % ('seat %d ' % s if s else '', os.path.join(tmp, 'carry_%s_c%d_s%d.png' % (mode, k, s))) for s in (0, 1)]
            sc = os.path.join(d, 'c%d.txt' % k)
            open(sc, 'w').write('\n'.join(sorted(lines + extra, key=lambda l: int(l.split()[0]))) + '\n')
            procs.append(subprocess.Popen(mptest.client_args(mptest.game_dir(d, k), sc, '1,2', port, k, n) + ['--mouse=' + mode],
                                          stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True))
        for p in procs:
            out, err = p.communicate(timeout=600)
            if p.returncode:
                sys.exit('network game %s: exit %d\n%s' % (mode, p.returncode, err[-2000:]))
            states.append([l.split()[3] for l in out.splitlines() if l.startswith('STATE')])
            arrows.append([int(l.split()[4]) for l in out.splitlines() if l.startswith('ARROW')])
    finally:
        srv.kill()
    return states, arrows


def net_check(tmp):
    with ThreadPoolExecutor(2) as ex:
        list(ex.map(lambda m: net_run(tmp, m), ('free', 'system')))
        carry = dict(zip(('free', 'system'), ex.map(lambda m: net_carry_run(tmp, m), ('free', 'system'))))
    arrows = carry['system'][1]
    carry = {m: c[0] for m, c in carry.items()}
    fails = []
    for f, s, x, y in NET_SHOTS:
        for k in (0, 1):
            box = diff_box(*(os.path.join(tmp, 'net_%s_c%d_s%d_%d.png' % (m, k, s, f)) for m in ('free', 'system')))
            shown = k == s
            ok = (box is not None and box[0] >= x and box[1] >= y and box[2] <= x + 16 and box[3] <= y + 10) if shown else box is None
            print('net %d seat %d %s %s' % (f, s, 'shown here  out' if shown else 'elsewhere   same', 'ok' if ok else 'FAIL (differs in %s)' % (box,)))
            if not ok:
                fails.append('network game: frame %d, seat %d on machine %d' % (f, s, k))
    # something carried (an ad, seat 1): the window draws it -- left out of the shown
    # seat's picture where the pointer is (150,70 less the item's hot spot), the rest the same,
    # and the same game
    same_game = carry['free'] == carry['system'] and all(carry['free'])
    for k in (0, 1):
        for s in (0, 1):
            box = diff_box(*(os.path.join(tmp, 'carry_%s_c%d_s%d.png' % (m, k, s)) for m in ('free', 'system')))
            shown = k == s == 1
            ok = (box is not None and box[0] <= 150 < box[2] and box[1] <= 70 < box[3] and box[2] - box[0] <= 40 and box[3] - box[1] <= 40) if shown else box is None
            print('carry seat %d %s %s' % (s, 'shown here  out' if shown else ('elsewhere   same' if k != s else 'not carrying same'), 'ok' if ok else 'FAIL (differs in %s)' % (box,)))
            if not ok:
                fails.append('network game, an item carried: seat %d on machine %d' % (s, k))
    if not same_game:
        fails.append('network game, an item carried: another game with the system cursor (%s / %s)' % (carry['free'], carry['system']))
    # what the window's present sees (seat 0's turn, the shown seat's state in memory): the
    # guest's machine draws the item (3, SYS_ARROW_DRAG) -- once it asked the running seat,
    # and a guest's item was never drawn by its window; the host's not (it carries nothing)
    ok = len(arrows) == 2 and arrows[1] == [3] and len(arrows[0]) == 1 and arrows[0] != [3]
    print('carry present  host %s, guest %s %s' % (arrows[0], arrows[1], 'ok' if ok else 'FAIL'))
    if not ok:
        fails.append('network game, an item carried: the window would not draw it (states %s)' % (arrows,))
    return fails


def main():
    tmp = tempfile.mkdtemp(prefix='mtv_arrow_')
    carry_base = [l.strip() for l in open(os.path.join(ROOT, 'tests', 'scenarios', 'room_txmap_buy.txt'))
                  if l.split() and l.split()[0].isdigit() and int(l.split()[0]) < 4450
                  and l.split()[1] not in ('hash', 'peek8', 'peek16', 'peek32', 'call')]
    jobs = [(n, m, b, l) for n, b, l in (('rooms', START, ROOMS), ('carry', carry_base, CARRY)) for m in ('free', 'system')]
    with ThreadPoolExecutor(4) as ex:
        res = dict(((j[0], j[1]), r) for j, r in zip(jobs, ex.map(lambda j: run(tmp, *j), jobs)))
    fails = []
    for name in ('rooms', 'carry'):
        (sf, shots), (ss, _) = res[(name, 'free')], res[(name, 'system')]
        if not sf or [l.split()[2] for l in sf] != [l.split()[2] for l in ss]:
            fails.append('%s: game state differs\n  free:   %s\n  system: %s' % (name, sf, ss))
        for shot, want in shots:
            box = diff_box(*(os.path.join(tmp, '%s_%s_%s.png' % (name, m, shot)) for m in ('free', 'system')))
            if want[0] == 'same':
                ok = box is None
            else:
                x, y = int(want[1]), int(want[2])
                ok = box is not None and box[0] >= x and box[1] >= y and box[2] <= x + 16 and box[3] <= y + 10
            print('%-16s %-4s %s' % (shot, want[0], 'ok' if ok else 'FAIL (differs in %s)' % (box,)))
            if not ok:
                fails.append('%s %s' % (name, shot))
    fails += net_check(tmp)
    if fails:
        print('\n'.join(fails))
        print('kept', tmp)
        sys.exit(1)
    shutil.rmtree(tmp)
    print('system arrow: the same game, only the arrow left out (%d game-state checkpoints); '
          'a network game: on the seat each machine shows' % sum(len(res[(n, 'free')][0]) for n in ('rooms', 'carry')))


if __name__ == '__main__':
    main()
