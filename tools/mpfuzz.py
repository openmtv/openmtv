#!/usr/bin/env python3
"""Random play of two or three humans, to find what breaks in a multiplayer game.

    python3 tools/mpfuzz.py [--seeds N] [--first S] [--frames F] [--humans 1,2]
                            [--net] [--logical] [--jobs J] [--keep]

For each seed a script of random input for every seat: clicks and right
clicks anywhere on the screen (doors, the elevator, a room's things), Esc
now and then, pointer moves, drags (press, move, release: an ad into the
bag, a film onto the shelf) -- from the first morning of a new game
(--new-game), for F frames (default 30000: a game day is ~70000).  Every 500
frames the game state's hash, every seat's screen hash.

  * local: the seat scheduler plays it (--humans), with SEAT_PTRCHECK=1 (a
    far pointer into a seat's own heap left in the shared data: another
    seat's turn would see its memory) and SEAT_STATS=1 (the longest wait of
    a seat for its turn);
  * --net: the same over the network (build/openmtv serve, pace fast), one
    client per human, each computing every seat: their lines must be the
    local run's, the server must see no DESYNC.

--logical: on the logical clock (--logical-time), as the launcher plays.

A run that crashes, hangs (no end within the time a run takes at ~1000
frames a second, twice over), ends before the script does, leaves a pointer
check message or differs over the network is reported with its seed; --keep
keeps its script and output in build/mpfuzz/SEED/.  Standard library only."""
import os
import random
import shutil
import socket
import subprocess
import sys
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..'))
sys.path.insert(0, HERE)
import netserver  # noqa: E402
import scenario  # noqa: E402

BIN = os.path.join(ROOT, 'build', 'mtv-dec')


def opt(name, default=None):
    i = sys.argv.index(name) + 1 if name in sys.argv else len(sys.argv)
    return sys.argv[i] if i < len(sys.argv) else default


def free_port():
    s = socket.socket()
    s.bind(('127.0.0.1', 0))
    p = s.getsockname()[1]
    s.close()
    return p


def script(seed, frames, seats):
    """random input for every seat; seat 0's lines plain, the others' 'seat S'"""
    rnd = random.Random(seed)
    lines = []
    for s in range(seats):
        pre = '' if s == 0 else 'seat %d ' % s
        f = 700 + rnd.randrange(200)            # (the others join in the first morning, ~520)
        while f < frames:
            r = rnd.random()
            if r < 0.55:
                lines.append((f, '%s%s %d %d' % (pre, 'click' if rnd.random() < 0.9 else 'rclick',
                                                 rnd.randrange(320), rnd.randrange(200))))
            elif r < 0.75:
                lines.append((f, '%skey esc' % pre))
            elif r < 0.85:
                lines.append((f, '%spointer %d %d' % (pre, rnd.randrange(320), rnd.randrange(200))))
            elif r < 0.93:                       # a drag: take something (an ad, a film, a news item) and put it down
                x, y = rnd.randrange(320), rnd.randrange(200)
                lines.append((f, '%spointer %d %d' % (pre, x, y)))
                lines.append((f + 6, '%sdown 1' % pre))
                lines.append((f + 14, '%spointer %d %d' % (pre, rnd.randrange(320), rnd.randrange(200))))
                lines.append((f + 28, '%sup 1' % pre))
                f += 28
            else:                                # the elevator (the building's middle) and a floor's doors
                lines.append((f, '%sclick %d %d' % (pre, rnd.choice((153, 60, 100, 200, 250)), rnd.randrange(20, 110))))
            f += rnd.randrange(30, 160)
    for f in range(500, frames, 500):
        lines.append((f, 'hash'))
        for s in range(1, seats):
            lines.append((f + s, 'seat %d hash' % s))
    for f in range(100, frames, 100):                # the room each seat is in (g_room, DS:838C): what the play reached
        lines.append((f, 'peek16 39D4:838C'))
        for s in range(1, seats):
            lines.append((f, 'seat %d peek16 39D4:838C' % s))
    lines.sort(key=lambda t: t[0])
    return '\n'.join('%d %s' % t for t in lines) + '\n%d quit\n' % frames


def game_dir(d, name):
    g = os.path.join(d, name)
    os.makedirs(g)
    for f in ('SOURCE1.RSC', 'SOURCE2.RSC'):
        shutil.copy(os.path.join(scenario.GAME, f), g)
    return g


def args_for(g, path, humans, seed):
    return [BIN, '--headless', '--nosound', '--game', g, '--script', path, '--args', 'e', '--humans', humans,
            '--new-game', '--seed', str(seed)] + (['--logical-time'] if '--logical' in sys.argv else [])


def lines_of(out):
    return [l for l in out.split('\n') if l.startswith(('HASH', 'STATE', 'PEEK'))]


def rooms_of(lines):
    """the rooms each seat was seen in: {seat: set}"""
    out = {}
    for l in lines:
        w = l.split()
        if w[0] == 'PEEK':
            seat = int(w[2]) if w[1] == 'seat' else 0
            out.setdefault(seat, set()).add(int(w[-1]))
    return out


def run_seed(seed, frames, humans, net, keep):
    seats = len(humans.split(','))
    d = tempfile.mkdtemp(prefix='mpfuzz_')
    problems = []
    path = os.path.join(d, 'script.txt')
    open(path, 'w').write(script(seed, frames, seats))
    limit = max(120, frames / 500)
    t0 = time.time()
    env = dict(os.environ, SEAT_PTRCHECK='1', SEAT_STATS='1')
    try:
        p = subprocess.run(args_for(game_dir(d, 'local'), path, humans, seed), capture_output=True, text=True,
                           timeout=limit, env=env)
        local, err, code = lines_of(p.stdout), p.stderr, p.returncode
    except subprocess.TimeoutExpired as e:
        local, err, code = [], (e.stderr or b'').decode(errors='replace') if isinstance(e.stderr, bytes) else (e.stderr or ''), 'timeout'
        problems.append('hangs (no end within %d s)' % limit)
    open(os.path.join(d, 'local.err'), 'w').write(err)
    if code not in (0, 'timeout'):
        problems.append('exit %s: %s' % (code, ' | '.join([l for l in err.split('\n') if l][-3:])))
    if '[quit]' not in err and code == 0:
        problems.append('ended early: %s' % ' | '.join([l for l in err.split('\n') if l.startswith('[seats]')][-2:]))
    ptr = [l for l in err.split('\n') if l.startswith('[ptrcheck]')]
    if ptr:
        problems.append('%d pointer check message(s), first: %s' % (len(ptr), ptr[0]))
    waits = [l for l in err.split('\n') if 'waited at most' in l or 'the world stood' in l]
    if net and local:
        port = free_port()
        srv = subprocess.Popen(netserver.command() + ['--seats', str(seats), '--host', '127.0.0.1', '--port', str(port),
                                '--pace', 'fast', '--window', '64', '--drop-after', '5'],
                               stderr=subprocess.PIPE, text=True)
        time.sleep(0.3)
        clients = [subprocess.Popen(args_for(game_dir(d, 'c%d' % k), path, humans, seed) +
                                    ['--mp', '127.0.0.1:%d' % port, '--seat', str(k)],
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True) for k in range(seats)]
        outs = []
        for k, c in enumerate(clients):
            try:
                o, e = c.communicate(timeout=limit * 2)
                outs.append(lines_of(o))
                open(os.path.join(d, 'c%d.err' % k), 'w').write(e)
            except subprocess.TimeoutExpired:
                c.kill()
                outs.append(None)
                problems.append('network client %d hangs' % k)
        srv.kill()
        serr = srv.stderr.read()
        if 'DESYNC' in serr or 'desync' in serr.lower():
            problems.append('server: %s' % [l for l in serr.split('\n') if 'esync' in l][:1])
        for k, o in enumerate(outs):
            if o is not None and o != local:
                i = next((i for i, (a, b) in enumerate(zip(local, o)) if a != b), min(len(local), len(o)))
                problems.append('network client %d differs from the local run at line %d: %s / %s' %
                                (k, i, local[i] if i < len(local) else '-', o[i] if i < len(o) else '-'))
    if keep or problems:
        kd = os.path.join(ROOT, 'build', 'mpfuzz', str(seed))
        shutil.rmtree(kd, ignore_errors=True)
        shutil.copytree(d, kd, ignore=shutil.ignore_patterns('*.RSC', 'SOURCE*'))
    shutil.rmtree(d, ignore_errors=True)
    rooms = rooms_of(local)
    return seed, problems, time.time() - t0, waits + ['rooms ' + ' / '.join('%d: %s' % (k, ','.join(str(r) for r in sorted(v)))
                                                                 for k, v in sorted(rooms.items()))]


def main():
    seeds = int(opt('--seeds', '6'))
    first = int(opt('--first', '1'))
    frames = int(opt('--frames', '30000'))
    humans = opt('--humans', '1,2')
    net = '--net' in sys.argv
    keep = '--keep' in sys.argv
    jobs = int(opt('--jobs', str(max(1, (os.cpu_count() or 2) // (3 if net else 1)))))
    bad = 0
    with ThreadPoolExecutor(jobs) as ex:
        for seed, problems, t, waits in ex.map(lambda s: run_seed(s, frames, humans, net, keep), range(first, first + seeds)):
            wait = '; '.join(w.replace('[seats] ', '') for w in waits)
            print('seed %d: %s (%.0f s)%s' % (seed, 'ok' if not problems else 'PROBLEM', t, ('  ' + wait) if wait else ''))
            for pr in problems:
                print('    ' + pr)
            bad += bool(problems)
    print('%d of %d seeds with problems%s' % (bad, seeds, ' (kept in build/mpfuzz/)' if bad else ''))
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
