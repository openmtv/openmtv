#!/usr/bin/env python3
"""A network game under stress, to find what breaks: busy humans, a bad network.

    python3 tools/mpchaos.py [--seeds N] [--first S] [--frames F] [--humans 1,2[,3]]
                             [--jobs J] [--keep] [--calm] [--hr] [--kill] [--logical]

For each seed:

1. a script of busy players: tools/mpfuzz.py's random play (clicks on doors,
   the elevator and whatever a room has, right clicks, Esc, pointer moves,
   drags of ads, films and news) and now and then a pause held for a while
   (Space, then Space again) -- from the first morning of a new game for F
   frames (default 150000: two game days and their nights);
2. the local game (--humans, the seat scheduler) as the reference, with
   SEAT_PTRCHECK=1 and --mp-save;
3. the same over the network: build/openmtv serve --pace fast, one client per
   human, each with its own copy of the game files and --mp-save, each behind a
   relay of its own that, from the seed, every 0.4..2.5 s does one of: hold
   everything (0.2..3 s; what TCP makes of lost packets: a stall), delay every
   piece by 10..250 ms (1..4 s), pass it on in bits of 1..9 bytes (1..4 s),
   cut the connection (the client must come back on a new one: RESUME), cut
   it and refuse new ones for 0.3..3 s -- nothing (--calm: no relay faults).

Checks: every client ends ([quit]) in time, exit 0; the server reports no
DESYNC; every client's HASH/STATE/PEEK lines are the local game's, line for
line; every night's savegame (build/mp: tag-NNN.mtvmp) is the same file on
every client and in the local game; no pointer check messages; the cuts came
back (RESUME) -- how many is reported.

--hr     client 1 draws with --filter hr-pixel (the game must be the same)
--kill   one client (not seat 0; with two humans or more) is killed at a random moment (2..15 s): the
         server waits for it (--drop-after 20), then the others play to the
         end alike (compared with each other, not with the local game)
--logical  on the logical clock (--logical-time), as the launcher plays

A seed with a problem keeps its script, the outputs and the relays' fault log
in build/mpchaos/SEED/ (--keep: every seed).  Standard library only."""
import os
import random
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import time
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..'))
sys.path.insert(0, HERE)
import mpfuzz  # noqa: E402
import netserver  # noqa: E402


def opt(name, default=None):
    i = sys.argv.index(name) + 1 if name in sys.argv else len(sys.argv)
    return sys.argv[i] if i < len(sys.argv) else default


def script(seed, frames, seats):
    """mpfuzz's random play, and a pause now and then (Space, held 100..900 frames, Space)"""
    lines = [l for l in mpfuzz.script(seed, frames, seats).split('\n') if l and not l.endswith(' quit')]
    rnd = random.Random(seed * 7919 + 1)
    for s in range(seats):
        pre = '' if s == 0 else 'seat %d ' % s
        f = 3000 + rnd.randrange(6000)
        while f < frames - 2000:
            lines.append('%d %skey space' % (f, pre))
            lines.append('%d %skey space' % (f + rnd.randrange(100, 900), pre))
            f += rnd.randrange(8000, 30000)
    lines.sort(key=lambda l: int(l.split()[0]))
    end = max(frames, int(lines[-1].split()[0]) + 10) if lines else frames   # (a drag's last line may lie past F)
    return '\n'.join(lines) + '\n%d quit\n' % end


class ChaosRelay:
    """a TCP relay between a client and the server that makes trouble, from its own RNG"""

    def __init__(self, to_port, rnd, calm):
        self.to, self.rnd, self.calm = to_port, rnd, calm
        self.lock = threading.Lock()
        self.pairs, self.log = [], []
        self.connections = 0
        self.stall_until = self.delay_until = self.frag_until = self.refuse_until = 0.0
        self.delay = (0.0, 0.0)
        self.running = True
        self.ln = socket.socket()
        self.ln.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.ln.bind(('127.0.0.1', 0))
        self.ln.listen(8)
        self.port = self.ln.getsockname()[1]
        self.t0 = time.time()
        threading.Thread(target=self.accept, daemon=True).start()
        if not calm:
            threading.Thread(target=self.trouble, daemon=True).start()

    def note(self, what):
        with self.lock:
            self.log.append('%7.2f %s' % (time.time() - self.t0, what))

    def accept(self):
        while self.running:
            try:
                a, _ = self.ln.accept()
            except OSError:
                return
            if time.time() < self.refuse_until:
                a.close()
                self.note('refused a connection')
                continue
            try:
                b = socket.create_connection(('127.0.0.1', self.to))
            except OSError:
                a.close()
                continue
            for s in (a, b):
                s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            pair = [a, b]
            with self.lock:
                self.pairs.append(pair)
                self.connections += 1
            threading.Thread(target=self.pump, args=(a, b, pair), daemon=True).start()
            threading.Thread(target=self.pump, args=(b, a, pair), daemon=True).start()

    def pump(self, src, dst, pair):
        due = 0.0
        try:
            while True:
                data = src.recv(65536)
                if not data:
                    break
                while time.time() < self.stall_until:
                    time.sleep(0.01)
                now = time.time()
                if now < self.delay_until:
                    due = max(due, now + self.rnd_delay())
                    time.sleep(max(0.0, due - now))
                if time.time() < self.frag_until:
                    i = 0
                    while i < len(data):
                        n = 1 + int(self.rnd.random() * 9)
                        dst.sendall(data[i:i + n])
                        i += n
                        if self.rnd.random() < 0.2:
                            time.sleep(0.001)
                else:
                    dst.sendall(data)
        except OSError:
            pass
        self.close(pair)

    def rnd_delay(self):
        lo, hi = self.delay
        return lo + self.rnd.random() * (hi - lo)

    def close(self, pair):
        for s in pair:
            try:
                s.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            try:
                s.close()
            except OSError:
                pass

    def cut(self):
        """the open connections closed; how many there were"""
        with self.lock:
            pairs, self.pairs = self.pairs, []
        for p in pairs:
            self.close(p)
        return len(pairs)

    def trouble(self):
        """every 0.4..2.5 s one fault, drawn from the seed"""
        time.sleep(1.5)                      # (the game first: the lobby-less start, everyone joined)
        while self.running:
            time.sleep(0.4 + self.rnd.random() * 2.1)
            if not self.running:
                return
            r, now = self.rnd.random(), time.time()
            if r < 0.25:
                d = 0.2 + self.rnd.random() * 2.8
                self.stall_until = now + d
                self.note('stall %.1f s' % d)
            elif r < 0.48:
                d = 1 + self.rnd.random() * 3
                lo = 0.01 + self.rnd.random() * 0.05
                self.delay, self.delay_until = (lo, lo + self.rnd.random() * 0.2), now + d
                self.note('delay %.0f..%.0f ms for %.1f s' % (self.delay[0] * 1000, self.delay[1] * 1000, d))
            elif r < 0.68:
                d = 1 + self.rnd.random() * 3
                self.frag_until = now + d
                self.note('bits of 1..9 bytes for %.1f s' % d)
            elif r < 0.84:
                if self.cut():
                    self.note('cut')
            elif r < 0.95:
                d = 0.3 + self.rnd.random() * 2.7
                self.refuse_until = now + d
                self.note('%s, refused for %.1f s' % ('cut' if self.cut() else 'no connection to cut', d))
            # else: nothing

    def stop(self):
        self.running = False
        try:
            self.ln.close()
        except OSError:
            pass
        self.cut()


def saves_of(d):
    """a client's nights: {tag file name: bytes} (--mp-save writes DIR/tag-NNN.mtvmp)"""
    out = {}
    for root, _, files in os.walk(d):
        for f in files:
            if f.endswith('.mtvmp'):
                out[f] = open(os.path.join(root, f), 'rb').read()
    return out


def first_diff(a, b):
    i = next((i for i, (x, y) in enumerate(zip(a, b)) if x != y), min(len(a), len(b)))
    return 'line %d: %s / %s' % (i, a[i] if i < len(a) else '-', b[i] if i < len(b) else '-')


def run_seed(seed, frames, humans, opts):
    seats = len(humans.split(','))
    d = tempfile.mkdtemp(prefix='mpchaos_')
    problems, info = [], []
    path = os.path.join(d, 'script.txt')
    open(path, 'w').write(script(seed, frames, seats))
    limit = max(300, frames / 120)
    t0 = time.time()
    logical = ['--logical-time'] if opts['logical'] else []
    relays, procs = [], []
    try:
        # 2. the local game
        env = dict(os.environ, SEAT_PTRCHECK='1')
        local_saves = os.path.join(d, 'saves-local')
        args = mpfuzz.args_for(mpfuzz.game_dir(d, 'local'), path, humans, seed) + logical + ['--mp-save', local_saves]
        try:
            p = subprocess.run(args, capture_output=True, text=True, timeout=limit, env=env)
            local, err, code = mpfuzz.lines_of(p.stdout), p.stderr, p.returncode
        except subprocess.TimeoutExpired as e:
            local, err, code = [], e.stderr.decode(errors='replace') if isinstance(e.stderr, bytes) else (e.stderr or ''), 'timeout'
            problems.append('local game hangs (no end within %d s)' % limit)
        open(os.path.join(d, 'local.err'), 'w').write(err)
        if code not in (0, 'timeout'):
            problems.append('local game: exit %s: %s' % (code, ' | '.join([l for l in err.split('\n') if l][-3:])))
        ptr = [l for l in err.split('\n') if l.startswith('[ptrcheck]')]
        if ptr:
            problems.append('%d pointer check message(s), first: %s' % (len(ptr), ptr[0]))
        if not local:
            return seed, problems, time.time() - t0, info
        # 3. over the network, every client behind a relay that makes trouble
        port = mpfuzz.free_port()
        drop = '20' if opts['kill'] else '60'   # (6 s: a survivor's own cuts and refusals outlasted it -- seed 303)
        log = os.path.join(d, 'server.log')
        serr_path = os.path.join(d, 'server.err')
        with open(serr_path, 'w') as serr_file:       # (a file: a pipe read only at the end could fill up and stop the server)
            srv = subprocess.Popen(netserver.command() + ['--seats', str(seats), '--host', '127.0.0.1', '--port', str(port),
                                                          '--pace', 'fast', '--window', '64', '--drop-after', drop, '--log', log],
                                   stderr=serr_file, text=True)
        procs.append(srv)
        time.sleep(0.3)
        rnd = random.Random(seed * 104729 + 7)
        relays = [ChaosRelay(port, random.Random(rnd.random()), opts['calm']) for _ in range(seats)]
        clients = []
        for k in range(seats):
            a = mpfuzz.args_for(mpfuzz.game_dir(d, 'c%d' % k), path, humans, seed) + logical + \
                ['--mp', '127.0.0.1:%d' % relays[k].port, '--seat', str(k), '--mp-save', os.path.join(d, 'saves-c%d' % k)]
            if opts['hr'] and k == 1:
                a += ['--filter', 'hr-pixel']
            clients.append(subprocess.Popen(a, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True))
            procs.append(clients[-1])
        victim = None
        if opts['kill'] and seats > 1:
            victim = 1 + rnd.randrange(seats - 1)
            at = 2 + rnd.random() * 13

            def kill_later(c=clients[victim], at=at):
                time.sleep(at)
                if c.poll() is None:
                    c.kill()
            threading.Thread(target=kill_later, daemon=True).start()
            info.append('client %d killed after %.1f s' % (victim, at))
        outs, errs = [None] * seats, [''] * seats

        def wait(k):
            try:
                o, e = clients[k].communicate(timeout=limit * 3)
                outs[k], errs[k] = o, e
            except subprocess.TimeoutExpired:
                clients[k].kill()
                o, e = clients[k].communicate()
                errs[k] = e
                if k != victim:
                    problems.append('network client %d hangs (no end within %d s)' % (k, limit * 3))
        with ThreadPoolExecutor(seats) as ex:
            list(ex.map(wait, range(seats)))
        for r in relays:
            r.stop()
        srv.kill()
        srv.wait()
        serr = open(serr_path).read()
        for k in range(seats):
            open(os.path.join(d, 'c%d.err' % k), 'w').write(errs[k] or '')
            open(os.path.join(d, 'relay%d.log' % k), 'w').write('\n'.join(relays[k].log) + '\n')
        if 'DESYNC' in serr or 'desync' in serr.lower():
            problems.append('server: %s' % [l for l in serr.split('\n') if 'esync' in l][:2])
        nets = {}
        for k in range(seats):
            if k == victim:
                continue
            code = clients[k].returncode
            if code != 0:
                problems.append('network client %d: exit %s: %s' % (k, code, ' | '.join([l for l in (errs[k] or '').split('\n') if l][-3:])))
            if '[quit]' not in (errs[k] or '') and code == 0:
                problems.append('network client %d ended before its script: %s' % (k, ' | '.join([l for l in errs[k].split('\n') if l][-2:])))
            nets[k] = mpfuzz.lines_of(outs[k] or '')
        ref = local if victim is None else nets.get(0, [])
        for k, lines in nets.items():
            if lines != ref:
                problems.append('network client %d differs from %s at %s' % (k, 'the local game' if victim is None else 'client 0', first_diff(ref, lines)))
        # the nights' savegames, the same everywhere
        saves = {k: saves_of(os.path.join(d, 'saves-c%d' % k)) for k in nets}
        want = saves_of(local_saves) if victim is None else saves.get(0, {})
        for k, s in saves.items():
            if sorted(s) != sorted(want):
                problems.append('client %d saved the nights %s, %s %s' % (k, sorted(s), 'the local game' if victim is None else 'client 0', sorted(want)))
            else:
                bad = [f for f in s if s[f] != want[f]]
                if bad:
                    problems.append('client %d: savegame(s) %s not the same' % (k, bad))
        backs = [sum('back on a new connection' in l for l in (errs[k] or '').split('\n')) for k in range(seats)]
        faults = [len(r.log) for r in relays]
        info.append('nights %d, faults %s, connections %s, back on a new one %s' %
                    (len(want), faults, [r.connections for r in relays], backs))
        cuts = [sum(('cut' in l) for l in r.log) for r in relays]
        for k in nets:
            if cuts[k] and not backs[k] and clients[k].returncode == 0:
                problems.append('client %d: %d cuts, never back on a new connection' % (k, cuts[k]))
        return seed, problems, time.time() - t0, info
    finally:
        for r in relays:
            r.stop()
        for p in procs:                      # (an exception on the way: nothing left running)
            if p.poll() is None:
                p.kill()
                p.wait()
        if opts['keep'] or problems:
            kd = os.path.join(ROOT, 'build', 'mpchaos', str(seed))
            shutil.rmtree(kd, ignore_errors=True)
            shutil.copytree(d, kd, ignore=shutil.ignore_patterns('*.RSC', 'SOURCE*'))
        shutil.rmtree(d, ignore_errors=True)


def main():
    seeds = int(opt('--seeds', '4'))
    first = int(opt('--first', '1'))
    frames = int(opt('--frames', '150000'))
    humans = opt('--humans', '1,2')
    opts = {k: '--' + k in sys.argv for k in ('keep', 'calm', 'hr', 'kill', 'logical')}
    seats = len(humans.split(','))
    jobs = int(opt('--jobs', str(max(1, (os.cpu_count() or 2) // (seats + 1)))))
    bad = 0
    with ThreadPoolExecutor(jobs) as ex:
        for seed, problems, t, info in ex.map(lambda s: run_seed(s, frames, humans, opts), range(first, first + seeds)):
            print('seed %d: %s (%.0f s)  %s' % (seed, 'ok' if not problems else 'PROBLEM', t, '; '.join(info)), flush=True)
            for pr in problems:
                print('    ' + pr, flush=True)
            bad += bool(problems)
    print('%d of %d seeds with problems%s' % (bad, seeds, ' (kept in build/mpchaos/)' if bad else ''))
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
