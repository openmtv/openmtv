#!/usr/bin/env python3
"""Does the seat scheduler (runtime/seat.h, --seats N) leave the game alone?

    python3 tools/seattest.py [NAME ...]     session1 and every scenario

Multiplayer stage 1: the program runs as seat 0's coroutine, game_frame
yields to the scheduler at its end, the world parts run in seat 0.  With
one seat that must be the game exactly: each test plays twice with
build/mtv-dec -- without --seats and with --seats 1 -- in instruction time
(classic) and on the logical clock, session1 also with --fixes; every
HASH/STATE/PEEK/CALL line (the HASH lines hash the whole memory but the
stack segment) and the time and registers at the end ([quit]) must be the
same.

Stage 2, the spectator: with --seats 2 a second seat joins as soon as
seat 0 is in the building of a running game and shows the building view
once a frame (its own screen, cursor, input; the world's time stands while
it runs).  Seat 0's game must still be the single-player game, bit for bit
-- the proof that switching seats keeps everything apart: every HASH line
(the whole memory and the screen), the game state, every checked value,
the time and the registers at the end.  The spectator has a far heap of
its own in the seat window (src/seatctx.c), so even the free heap blocks
stay seat 0's.  These runs also have the scheduler's leak check on
(SEAT_CHECK=1, runtime/seat.c): seat 0's memory as it left it must be what
it finds when it runs again -- a byte another seat changed and the switch
did not put back fails the test at once, even if it is overwritten later.
Each test is also played that way (classic); the report says in how many
the spectator joined; tools/seatdiff.py shows where a game differs.

The multiplayer tests (tests/mp/NAME.txt): lines for other seats ("N seat
S command", runtime/host.c), and "# play FILE until N" to play the start of
another script first.  Played without seats (the seat lines are skipped)
and with --seats 2: seat 0's lines must be the same, and the HASH seat lines
(the other seats' screens) those in tests/mp/NAME.seats.  A test with
"# humans A,B" plays with --humans A,B (multiplayer stage 3: a human on
each of those stations, seat 0 first): another game than without seats, so
every HASH/STATE/PEEK line of every seat must be as recorded, and there is
no leak check (other seats change the world, rightly).  "# options ..."
gives such a test more command-line options (e.g. --new-game).

    python3 tools/seattest.py --record-mp     write the .seats files anew
    python3 tools/seattest.py --quick         session1, four short scenarios and tests/mp
                                              (a check between steps; the whole set at milestones)

Every game run is made once and all of them in parallel, the longest first
(the run without seats serves both comparisons)."""
import os, shutil, subprocess, sys, tempfile
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import scenario

ROOT = scenario.ROOT
BIN = os.path.join(ROOT, 'build', 'mtv-dec.exe' if os.name == 'nt' else 'mtv-dec')
MP = os.path.join(ROOT, 'tests', 'mp')
SESSION = os.path.join(ROOT, 'tests', 'session1.txt')


def script_of(n):
    if n.startswith('mp:'):
        return os.path.join(MP, n[3:] + '.txt')
    return SESSION if n == 'session1' else os.path.join(scenario.SCEN, n + '.txt')


def humans_of(n):
    """the stations of a multiplayer test's '# humans A,B' line, or None"""
    if not n.startswith('mp:'):
        return None
    for l in open(script_of(n)).read().split('\n'):
        w = l.split()
        if len(w) == 3 and w[:2] == ['#', 'humans']:
            return w[2]
    return None


def options_of(n):
    """a multiplayer test's '# options ...' line: more command-line options (e.g. --new-game)"""
    if not n.startswith('mp:'):
        return []
    for l in open(script_of(n)).read().split('\n'):
        w = l.split()
        if len(w) >= 2 and w[:2] == ['#', 'options']:
            return w[2:]
    return []


def mp_script(n, d):
    """tests/mp/NAME.txt with the start it plays first, as one script in d -> (path, base)"""
    base, lines = None, []
    for l in open(script_of(n)).read().split('\n'):
        w = l.split()
        if len(w) == 5 and w[:2] == ['#', 'play'] and w[3] == 'until':
            base = os.path.normpath(os.path.join(ROOT, w[2]))
            lines += [b for b in open(base).read().split('\n')
                      if b.strip() and not b.startswith('#') and int(b.split()[0]) < int(w[4])]
        elif l.strip() and not l.startswith('#'):
            lines.append(l)
    lines.sort(key=lambda l: int(l.split()[0]))
    path = os.path.join(d, 'mp.txt')
    open(path, 'w').write('\n'.join(lines) + '\n')
    return path, base


def play(n, seats, extra, stats=None, leaks=None):
    d = tempfile.mkdtemp(prefix='seattest_')
    try:
        for f in ('SOURCE1.RSC', 'SOURCE2.RSC'):
            shutil.copy(os.path.join(scenario.GAME, f), d)
        script, base = mp_script(n, d) if n.startswith('mp:') else (script_of(n), script_of(n))
        args = [BIN, '--headless', '--game', d, '--script', script] + extra
        if base and base != SESSION:
            args += ['--nosound', '--args', scenario.lang_of(open(base).read().split('\n'))]
        elif humans_of(n):
            args += ['--nosound', '--args', 'e']
        if seats == 'humans':
            args += ['--humans', humans_of(n)] + options_of(n)
        elif seats:
            args += ['--seats', str(seats)]
        env = dict(os.environ)
        if stats is not None:
            env['SEAT_STATS'] = '1'
        if leaks is not None:
            env['SEAT_CHECK'] = '1'
        p = subprocess.run(args, capture_output=True, text=True, env=env)
        if stats is not None:
            stats.extend(l for l in p.stderr.split('\n') if l.startswith('[seats]'))
        if leaks is not None:
            leaks.extend(l for l in p.stderr.split('\n') if l.startswith('[seat check]'))
        out = [l for l in p.stdout.split('\n') if l.startswith(('HASH', 'STATE', 'PEEK', 'CALL'))]
        end = [l for l in p.stderr.split('\n') if l.startswith('[quit]')]
        if end:
            return out, end[0]
        tail = [l for l in p.stderr.split('\n') if l and not l.startswith(('[seats] rounds', '[seats] buffer'))
                and 'heap reached' not in l and 'waited at most' not in l and 'the world stood' not in l][-3:]
        return out, 'exit %d: %s' % (p.returncode, ' | '.join(tail))
    finally:
        shutil.rmtree(d, ignore_errors=True)


# every game run once, all in parallel, the longest first: (name, extra
# args, seats) -> (lines, end, [seats] lines, leak lines)
RUNS = {}


def run_key(n, extra, seats):
    return (n, tuple(extra), seats)


def run_one(key):
    n, extra, seats = key
    st, lk = [], []
    out, end = play(n, seats, list(extra), st if seats else None, lk if seats == 2 else None)
    return key, (out, end, st, lk)


def frames_of(n):
    """the script's length in frames (its start played first included), for the order"""
    p = script_of(n)
    last = 0
    for l in open(p).read().split('\n'):
        w = l.split()
        if len(w) == 5 and w[:2] == ['#', 'play'] and w[3] == 'until':
            last = max(last, int(w[4]))
        elif w and w[0].isdigit():
            last = max(last, int(w[0]))
    return last


def first_diff(a, b):
    return next(i for i in range(min(len(a), len(b)) + 1) if i == min(len(a), len(b)) or a[i] != b[i])


def compare_humans(n):
    """a multiplayer test with humans: every seat's lines as recorded"""
    b = RUNS[run_key(n, [], 'humans')]
    got = b[0] + [b[1]]
    exp = os.path.join(MP, n[3:] + '.seats')
    if '--record-mp' in sys.argv:
        open(exp, 'w').write('\n'.join(got) + '\n')
        return ''
    want = [l for l in open(exp).read().split('\n') if l] if os.path.exists(exp) else None
    if want is None:
        return 'no %s (--record-mp)' % os.path.relpath(exp, ROOT)
    if got != want:
        k = first_diff(want, got)
        return 'differs from the recording at line %d: %s / %s' % (k, want[k:k + 1], got[k:k + 1])
    return ''


def compare_mp(n):
    """a multiplayer test: seat 0 as without seats, the other seats' screens as recorded"""
    if humans_of(n):
        return compare_humans(n)
    a = RUNS[run_key(n, [], 0)]
    b = RUNS[run_key(n, [], 2)]
    if b[3]:
        return 'leak: %s' % b[3][0]
    mine = [l for l in b[0] if not l.startswith(('HASH seat', 'PEEK seat'))]
    theirs = [l for l in b[0] if l.startswith(('HASH seat', 'PEEK seat'))]
    if a[0] != mine:
        k = first_diff(a[0], mine)
        return 'seat 0 differs at line %d: %s / %s' % (k, a[0][k:k + 1], mine[k:k + 1])
    if a[1] != b[1]:
        return 'end differs: %s / %s' % (a[1], b[1])
    exp = os.path.join(MP, n[3:] + '.seats')
    if '--record-mp' in sys.argv:
        open(exp, 'w').write('\n'.join(theirs) + '\n')
        return ''
    want = [l for l in open(exp).read().split('\n') if l] if os.path.exists(exp) else None
    if want is None:
        return 'no %s (--record-mp)' % os.path.relpath(exp, ROOT)
    if theirs != want:
        k = first_diff(want, theirs)
        return 'the other seats differ at line %d: %s / %s' % (k, want[k:k + 1], theirs[k:k + 1])
    return ''


def compare(job):
    n, extra, seats = job
    if seats == 'mp':
        return compare_mp(n)
    a = RUNS[run_key(n, extra, 0)]
    b = RUNS[run_key(n, extra, seats)]
    if seats == 2 and not any('joins' in l for l in b[2]):
        return 'no join'
    if b[3]:
        return 'leak: %s' % b[3][0]
    if a[0] != b[0]:
        k = first_diff(a[0], b[0])
        return 'output differs at line %d: %s / %s' % (k, a[0][k:k + 1], b[0][k:k + 1])
    if a[1] != b[1]:
        return 'end differs: %s / %s' % (a[1], b[1])
    return ''


QUICK = ['session1', 'event_bailiff', 'room_board_porter', 'room_evening_air', 'save_continue']


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    mps = sorted('mp:' + f[:-4] for f in os.listdir(MP) if f.endswith('.txt')) if os.path.isdir(MP) else []
    names = [a for a in args if not a.startswith('mp:')] if args else \
        ['session1'] + sorted(f[:-4] for f in os.listdir(scenario.SCEN) if f.endswith('.txt'))
    if '--quick' in sys.argv and not args:
        names = QUICK
    if args:
        mps = [a for a in args if a.startswith('mp:')]
    if '--record-mp' in sys.argv:
        names = []
    jobs = [(n, x, 1) for n in names for x in ([], ['--logical-time'])]
    if 'session1' in names:
        jobs.append(('session1', ['--fixes'], 1))
    jobs += [(n, [], 2) for n in names]
    jobs += [(n, [], 'mp') for n in mps]
    keys = set()
    for n, x, k in jobs:
        if k == 'mp' and humans_of(n):
            keys.add(run_key(n, x, 'humans'))
            continue
        keys.add(run_key(n, x, 0))
        keys.add(run_key(n, x, 2 if k == 'mp' else k))
    order = sorted(keys, key=lambda k: -frames_of(k[0]) * (1.5 if k[2] in (2, 'humans') else 1))
    with ThreadPoolExecutor(os.cpu_count() or 4) as ex:
        for key, r in ex.map(run_one, order):
            RUNS[key] = r
    res = [compare(j) for j in jobs]
    bad = nojoin = 0
    for (n, x, k), r in zip(jobs, res):
        if r == 'no join':
            nojoin += 1
        elif r:
            bad += 1
            print('FAIL %-28s %-16s %s' % (n, (' '.join(x) or 'classic') + (' +spectator' if k == 2 else ' seats' if k == 'mp' else ''), r))
    one = [j for j in jobs if j[2] == 1]
    if one:
        print('%d of %d runs the same game with --seats 1 (%d tests: classic, logical clock, session1 with fixes)' %
              (len(one) - sum(1 for j, r in zip(jobs, res) if j[2] == 1 and r), len(one), len(names)))
    two = [j for j in jobs if j[2] == 2]
    if two:
        print('%d of %d the same with a spectator seat (--seats 2; it joined in %d, the others never reach the building)' %
              (len(two) - sum(1 for j, r in zip(jobs, res) if j[2] == 2 and r and r != 'no join'), len(two), len(two) - nojoin))
    mp = [j for j in jobs if j[2] == 'mp']
    if mp:
        print('%d of %d multiplayer tests (tests/mp: seat 0 the same, the other seats as recorded)%s' %
              (len(mp) - sum(1 for j, r in zip(jobs, res) if j[2] == 'mp' and r), len(mp), ', recorded' if '--record-mp' in sys.argv else ''))
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
