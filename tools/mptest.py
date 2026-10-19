#!/usr/bin/env python3
"""Does the network game compute the same game on every machine?

    python3 tools/mptest.py [NAME ...]      the multiplayer tests with humans (tests/mp)

Multiplayer stage 4 (runtime/net.h, build/openmtv serve): each test with
"# humans A,B" is played over the network on this machine -- the frame
server (pace fast) and one client per seat, each with its own copy of the
game files, each with the whole script (every client sends only its own
seat's input; the server's bundles bring everyone's).  Every client's
HASH/STATE/PEEK lines and its end ([quit]: time and registers) must be the
local game's (--humans, the recording tests/mp/NAME.seats) exactly.  Then
the server's log -- the inputs as the server ordered them -- is played
again locally, with the script's other lines (hash, peek, poke16, ...): the
same once more.

    --desync      also the negative test: one client gets one poke the
                  others do not; the server must stop the game (DESYNC)
    --leave       also: a client killed in the middle; the others wait for
                  it (--drop-after 1 s), then play on to the end
    --silent      also: a client silent (SIGSTOP) in a realtime game, as a network that
                  broke quietly: the server waits for it after --silent 3 s and goes
                  on when it speaks again; silent too long it is dropped and told
                  so; a player's Enter (NET_DROP_WAIT) drops it at once
    --reconnect   also: connections broken under the clients (a relay between each client
                  and the server): cut in a fast game -- the client comes back on a
                  new one (RESUME) and the game is the recording's, bit for bit;
                  every connection cut at once; a connection gone quiet (realtime);
                  no way back for too long -- dropped and told so
    --netem SPEC  play in a network namespace of its own (unshare -n, root)
                  whose loopback has `tc qdisc ... netem SPEC`, e.g.
                  "delay 30ms 10ms loss 2%": nothing else on the machine
                  is touched
    --pace realtime   the server's 70 frames a second instead of fast
    --no-tests    only the checks asked for (--desync, --leave)
    --sound       the clients with their sound on: each but seat 0's plays its
                  seat's own sound from a copy of the driver (src/rtl_snd.c),
                  which must leave the game as it is

Across machines (the LAN test): the server on one of them,

    build/openmtv serve --seats 3 --pace fast --window 64 --port 7641

and on each machine one seat of the same test, against it:

    python3 tools/mptest.py --client HOST:7641 --seat K three_humans

-- its lines must be the recording's, as above."""
import os, select, shutil, signal, socket, subprocess, sys, tempfile, threading, time
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import netserver
import scenario
import seattest

ROOT = scenario.ROOT
BIN = seattest.BIN


def opt(name, default=None):
    if name in sys.argv:
        return sys.argv[sys.argv.index(name) + 1]
    return default


def free_port():
    s = socket.socket()
    s.bind(('127.0.0.1', 0))
    p = s.getsockname()[1]
    s.close()
    return p


def end_of(line):
    """an end without [quit] (seattest: 'exit N: the last lines'): the exit code and the last line
    (the lines before it differ: a network game warns of no ignored input, the server took it)"""
    if line.startswith('exit '):
        code, _, tail = line.partition(': ')
        return '%s: %s' % (code, tail.split(' | ')[-1])
    return line


def lines_of(stdout, stderr, code):
    out = [l for l in stdout.split('\n') if l.startswith(('HASH', 'STATE', 'PEEK', 'CALL'))]
    end = [l for l in stderr.split('\n') if l.startswith('[quit]')]
    if end:
        return out + [end[0]]
    tail = [l for l in stderr.split('\n') if l.startswith('[seats]') and not l.startswith(('[seats] rounds', '[seats] buffer'))]
    return out + ['exit %d: %s' % (code, tail[-1] if tail else '(nothing)')]


def game_dir(d, k):
    g = os.path.join(d, 'game%d' % k)
    os.mkdir(g)
    for f in ('SOURCE1.RSC', 'SOURCE2.RSC'):
        shutil.copy(os.path.join(scenario.GAME, f), g)
    return g


def client_args(g, script, humans, port, seat, n):
    server = port if isinstance(port, str) else '127.0.0.1:%d' % port
    sound = [] if '--sound' in sys.argv else ['--nosound']
    return [BIN, '--headless', '--game', g, '--script', script, '--args', 'e',
            '--humans', humans, '--mp', server, '--seat', str(seat)] + seattest.options_of(n) + sound


def client(n, server, seat):
    """one seat of test n against a server elsewhere: its lines as recorded?"""
    d = tempfile.mkdtemp(prefix='mptest_')
    try:
        script, _ = seattest.mp_script(n, d)
        t0 = time.time()
        p = subprocess.run(client_args(game_dir(d, seat), script, seattest.humans_of(n), server, seat, n),
                           capture_output=True, text=True)
        got, want = lines_of(p.stdout, p.stderr, p.returncode), recorded(n)
        print('%s seat %d: %s (%.1f s, %d lines)' % (n[3:], seat, 'ok' if got == want else 'FAIL', time.time() - t0, len(got)))
        if got != want:
            print('    ' + diff(want, got))
            for l in p.stderr.split('\n'):
                if l.startswith('[net]'):
                    print('    ' + l)
        return got == want
    finally:
        shutil.rmtree(d, ignore_errors=True)


def wrap(argv, netem):
    """in the test's own network namespace (--netem) or as it is"""
    if not netem:
        return argv
    return ['nsenter', '--net=' + netem] + argv


class Relay:
    """a TCP relay between a client and the server, its connections to be broken: cut() closes
    them (a reset network), quiet() keeps them but carries nothing more (a connection broken
    quietly), refuse(s) takes no new ones for s seconds"""
    def __init__(self, to_port):
        self.to, self.pairs, self.lock = to_port, [], threading.Lock()
        self.refuse_until, self.connections = 0, 0
        self.ln = socket.socket()
        self.ln.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.ln.bind(('127.0.0.1', 0))
        self.ln.listen(8)
        self.port = self.ln.getsockname()[1]
        threading.Thread(target=self.accept, daemon=True).start()

    def accept(self):
        while True:
            try:
                a, _ = self.ln.accept()
            except OSError:
                return
            if time.time() < self.refuse_until:
                a.close()
                continue
            try:
                b = socket.create_connection(('127.0.0.1', self.to))
            except OSError:
                a.close()
                continue
            pair = {'socks': (a, b), 'quiet': False}
            with self.lock:
                self.pairs.append(pair)
                self.connections += 1
            threading.Thread(target=self.carry, args=(pair,), daemon=True).start()

    def carry(self, pair):
        a, b = pair['socks']
        try:
            while True:
                r, _, _ = select.select([a, b], [], [], 0.1)
                if pair['quiet']:
                    time.sleep(0.1)
                    continue
                for x in r:
                    data = x.recv(65536)
                    if not data:
                        raise OSError
                    (b if x is a else a).sendall(data)
        except OSError:
            pass
        for x in (a, b):
            try:
                x.close()
            except OSError:
                pass

    def cut(self):
        with self.lock:
            for p in self.pairs:
                for x in p['socks']:
                    try:
                        x.shutdown(socket.SHUT_RDWR)
                    except OSError:
                        pass
            self.pairs = []

    def quiet(self):
        with self.lock:
            for p in self.pairs:
                p['quiet'] = True
            self.pairs = []

    def refuse(self, secs):
        self.refuse_until = time.time() + secs


def net_game(n, d, script, extra_line=None, kill_seat=None, netem=None, stop_seat=None, server_args=None, env0=None, relays=None, breaks=None):
    """the test over the network -> ([lines] per client, server stderr, log path, client exit codes)"""
    humans = seattest.humans_of(n)
    k = len(humans.split(','))
    port = free_port() if not netem else 7640
    log = os.path.join(d, 'server.log')
    srv = subprocess.Popen(wrap(netserver.command() + ['--seats', str(k), '--host', '127.0.0.1',
                                 '--port', str(port), '--pace', opt('--pace', 'fast'), '--window', '64',
                                 '--drop-after', '1', '--log', log] + (server_args or []), netem),
                           stderr=subprocess.PIPE, text=True)
    time.sleep(0.3)
    procs = []
    for seat in range(k):
        sc = script
        if extra_line and seat == k - 1:     # the one client that differs
            sc = os.path.join(d, 'odd.txt')
            ls = open(script).read().split('\n')
            ls = [l for l in ls if l] + [extra_line]
            ls.sort(key=lambda l: int(l.split()[0]))
            open(sc, 'w').write('\n'.join(ls) + '\n')
        g = game_dir(d, seat)
        to = port
        if relays is not None:               # (a relay of its own between this client and the server)
            relays.append(Relay(port))
            to = relays[-1].port
        procs.append(subprocess.Popen(wrap(client_args(g, sc, humans, to, seat, n), netem),
                                      stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                                      env=dict(os.environ, **env0) if env0 and seat == 0 else None))
    if kill_seat is not None:
        time.sleep(kill_seat[1])
        procs[kill_seat[0]].kill()
    for at, what in breaks or []:          # (after s, a function of the relays)
        time.sleep(at)
        what(relays)
    if stop_seat is not None:              # (seat, after s, for s): silent -- a network broken quietly
        time.sleep(stop_seat[1])
        procs[stop_seat[0]].send_signal(signal.SIGSTOP)
        time.sleep(stop_seat[2])
        procs[stop_seat[0]].send_signal(signal.SIGCONT)
    with ThreadPoolExecutor(len(procs)) as ex:
        res = list(ex.map(lambda p: p.communicate(), procs))
    try:
        _, serr = srv.communicate(timeout=10)
    except subprocess.TimeoutExpired:
        srv.kill()
        _, serr = srv.communicate()
    return [lines_of(o, e, p.returncode) for (o, e), p in zip(res, procs)], serr, log, [p.returncode for p in procs], [e for _, e in res]


def replay(n, d, script, log):
    """the server's log with the script's other lines, played locally"""
    inputs = [l for l in open(log).read().split('\n') if l and not l.startswith('#')]
    keep = []
    for l in open(script).read().split('\n'):
        w = l.split()
        if not w:
            continue
        cmd = w[3] if len(w) > 3 and w[1] == 'seat' else w[1]
        if cmd not in ('key', 'kcode', 'down', 'up', 'click', 'rclick', 'mouse', 'move', 'pointer', 'goto'):
            keep.append(l)
    both = sorted(inputs + keep, key=lambda l: int(l.split()[0]))   # (stable: within a frame, inputs first)
    path = os.path.join(d, 'replay.txt')
    open(path, 'w').write('\n'.join(both) + '\n')
    g = game_dir(d, 9)
    p = subprocess.run([BIN, '--headless', '--game', g, '--script', path, '--nosound', '--args', 'e',
                        '--humans', seattest.humans_of(n)] + seattest.options_of(n), capture_output=True, text=True)
    return lines_of(p.stdout, p.stderr, p.returncode)


def recorded(n):
    ls = [l for l in open(os.path.join(seattest.MP, n[3:] + '.seats')).read().split('\n') if l]
    return ls[:-1] + [end_of(ls[-1])]


def diff(want, got):
    k = seattest.first_diff(want, got)
    return 'line %d: %s / %s' % (k, want[k:k + 1], got[k:k + 1])


def check(n, netem=None):
    d = tempfile.mkdtemp(prefix='mptest_')
    try:
        script, _ = seattest.mp_script(n, d)
        want = recorded(n)
        t0 = time.time()
        got, serr, log, codes, errs = net_game(n, d, script, netem=netem)
        t = time.time() - t0
        bad = []
        for seat, g in enumerate(got):
            if g != want:
                bad.append('client %d: %s' % (seat, diff(want, g)))
                bad += ['  ' + l for l in errs[seat].split('\n') if l.startswith('[net]')][-3:]
        if not bad:
            r = replay(n, d, script, log)
            if r != want:
                bad.append('the log played again: %s' % diff(want, r))
        if bad:
            bad += ['  ' + l for l in serr.split('\n') if l][-4:]
        return t, bad
    finally:
        shutil.rmtree(d, ignore_errors=True)


def check_desync(n):
    """one poke on one client only: the game must stop with DESYNC"""
    d = tempfile.mkdtemp(prefix='mptest_')
    try:
        script, _ = seattest.mp_script(n, d)
        got, serr, log, codes, errs = net_game(n, d, script, extra_line='1500 poke16 39D4:9034 4660')
        if 'DESYNC' not in serr or any(c != 4 for c in codes):
            return ['no DESYNC: exit codes %s, server: %s' % (codes, serr.strip().split('\n')[-1:])]
        return []
    finally:
        shutil.rmtree(d, ignore_errors=True)


def check_leave(n):
    """the last seat's client killed after 3 s: the others wait, then end the game -- and show
    it (the window's notices, "[notice]" on stderr: the connection broken, then lost); and a
    client that ends early (its script's quit, as a closed window: BYE): the others show that
    it left"""
    d = tempfile.mkdtemp(prefix='mptest_')
    try:
        script, _ = seattest.mp_script(n, d)
        got, serr, log, codes, errs = net_game(n, d, script, kill_seat=(1, 3.0))
        bad = []
        if 'WAIT' not in serr or 'no longer waited for' not in serr:
            bad.append('the server did not wait: %s' % serr.strip().split('\n')[-3:])
        for seat, g in enumerate(got):
            if seat != 1 and not g[-1].startswith('[quit]'):
                bad.append('client %d did not end the game: %s' % (seat, g[-1]))
        notes = [l for l in errs[0].split('\n') if l.startswith('[notice]')]
        if not any('connection to Player 2 is broken' in l for l in notes) or not any('connection to Player 2 is lost' in l for l in notes):
            bad.append('client 0 did not show the broken and lost connection: %s' % notes)
        shutil.rmtree(d, ignore_errors=True)
        os.makedirs(d)
        script, _ = seattest.mp_script(n, d)
        got, serr, log, codes, errs = net_game(n, d, script, extra_line='3000 quit')
        notes = [l for l in errs[0].split('\n') if l.startswith('[notice]')]
        if not any('Player 2 left the game' in l for l in notes):
            bad.append('client 0 did not show that seat 1 left: %s' % notes)
        if not got[0][-1].startswith('[quit]'):
            bad.append('client 0 did not play on to its end: %s' % got[0][-1])
        return bad
    finally:
        shutil.rmtree(d, ignore_errors=True)


def check_silent(n):
    """the last seat silent (SIGSTOP) in a realtime game, 2500 frames: (a) 6 s -- the others wait
    after 3 s (a notice), it comes back, the game goes on: every client ends alike; (b) 15 s with
    --drop-after 6 -- dropped, the others play on, it is told when it wakes; (c) 15 s, the host's
    Enter after 2 s of waiting (NET_DROP_WAIT) drops it at once"""
    def one(case):
        d = tempfile.mkdtemp(prefix='mptest_')
        try:
            script, _ = seattest.mp_script(n, d)
            ls = [l for l in open(script).read().split('\n') if l and int(l.split()[0]) < 2500] + ['2500 quit']
            open(script, 'w').write('\n'.join(ls) + '\n')
            secs, drop, env0 = {'a': (6, '20', None), 'b': (15, '6', None), 'c': (15, '60', {'NET_DROP_WAIT': '2'})}[case]
            got, serr, log, codes, errs = net_game(n, d, script, stop_seat=(1, 8, secs), env0=env0,
                                                   server_args=['--pace', 'realtime', '--silent', '3', '--drop-after', drop])
            notes = [l for l in errs[0].split('\n') if l.startswith('[notice]')]
            notes1 = [l for l in errs[1].split('\n') if l.startswith('[notice]')]
            bad = []
            if 'seat 1 silent for 3 s' not in serr or not any('connection to Player 2 is broken' in l for l in notes):
                bad.append('(%s) no wait for the silent seat: %s / %s' % (case, serr.strip().split('\n')[-3:], notes))
            if not got[0][-1].startswith('[quit]'):
                bad.append('(%s) client 0 did not play to its end: %s' % (case, got[0][-1:]))
            if case == 'a' and (got[0] != got[1] or 'seat 1 is back' not in serr or notes1):
                bad.append('(a) not on alike after the silence: %s / %s, notices of the silent one %s' % (got[0][-1:], got[1][-1:], notes1))
            if case in 'bc' and (not any('could not be reached for too long' in l for l in notes1) or 'no longer waited for' not in serr):
                bad.append('(%s) the silent one was not dropped and told: %s' % (case, notes1))
            if case == 'c' and not any('goes on without Player 2' in l for l in notes):
                bad.append('(c) no Enter: %s' % notes)
            return bad
        finally:
            shutil.rmtree(d, ignore_errors=True)
    with ThreadPoolExecutor(3) as ex:
        return sum(ex.map(one, 'abc'), [])


def check_reconnect(n):
    """connections broken under the clients: (cut) seat 1's cut twice in a fast game, (all) every
    connection cut at once -- each comes back on a new one and the game is the recording's, the
    server's log played again too; (quiet) seat 1's connection carries nothing more in a realtime
    game (2500 frames) -- the client finds the silence and comes back; (gone) seat 1's cut and no
    way back for 20 s with --drop-after 6: dropped, told so when it is back, seat 0 plays on"""
    def one(case):
        d = tempfile.mkdtemp(prefix='mptest_')
        try:
            script, _ = seattest.mp_script(n, d)
            want = recorded(n)
            relays, server_args = [], []
            if case in ('quiet', 'gone'):
                ls = [l for l in open(script).read().split('\n') if l and int(l.split()[0]) < 2500] + ['2500 quit']
                open(script, 'w').write('\n'.join(ls) + '\n')
                server_args = ['--pace', 'realtime'] + (['--drop-after', '6'] if case == 'gone' else ['--drop-after', '60'])
            breaks = {'cut': [(2, lambda r: r[1].cut()), (3, lambda r: r[1].cut())],
                      'all': [(3, lambda r: [x.cut() for x in r])],
                      'quiet': [(8, lambda r: r[1].quiet())],
                      'gone': [(8, lambda r: (r[1].refuse(20), r[1].cut()))]}[case]
            got, serr, log, codes, errs = net_game(n, d, script, relays=relays, breaks=breaks, server_args=server_args)
            bad = []
            backs = [l for l in errs[1].split('\n') if 'back on a new connection' in l]
            if case in ('cut', 'all'):
                for seat, g in enumerate(got):
                    if g != want:
                        bad.append('(%s) client %d: %s' % (case, seat, diff(want, g)))
                        bad += ['  ' + l for l in errs[seat].split('\n') if l.startswith('[net]')][-4:]
                if not backs or relays[1].connections < 2:
                    bad.append('(%s) seat 1 did not come back: %d connections' % (case, relays[1].connections))
                if not bad:
                    r = replay(n, d, script, log)
                    if r != want:
                        bad.append('(%s) the log played again: %s' % (case, diff(want, r)))
            if case == 'quiet':
                if got[0] != got[1] or not got[0][-1].startswith('[quit]') or not backs:
                    bad.append('(quiet) not on alike after the silence: %s / %s, %s' % (got[0][-1:], got[1][-1:], backs))
            if case == 'gone':
                notes1 = [l for l in errs[1].split('\n') if l.startswith('[notice]')]
                if not got[0][-1].startswith('[quit]') or 'no longer waited for' not in serr or \
                        not any('could not be reached for too long' in l or 'broke off' in l for l in notes1):
                    bad.append('(gone) not dropped and told: %s, %s' % (got[0][-1:], notes1[-2:]))
            if bad:
                bad += ['  ' + l for l in serr.split('\n') if l][-5:]
            return bad
        finally:
            shutil.rmtree(d, ignore_errors=True)
    cases = os.environ.get('MPTEST_CASES', 'cut,all,quiet,gone').split(',')   # (one of them: debugging)
    with ThreadPoolExecutor(len(cases)) as ex:
        return sum(ex.map(one, cases), [])


class Netns:
    """a network namespace of its own, its loopback delayed and lossy (tc netem)"""
    def __init__(self, spec):
        self.spec = spec

    def __enter__(self):
        # a process holds the namespace; nsenter --net=/proc/PID/ns/net joins it
        self.holder = subprocess.Popen(['unshare', '-n', 'sh', '-c',
                                        'ip link set lo up && tc qdisc add dev lo root netem %s && '
                                        'echo ready && exec sleep 100000' % self.spec],
                                       stdout=subprocess.PIPE, text=True)
        if self.holder.stdout.readline().strip() != 'ready':
            sys.exit('mptest: no network namespace with netem %s (root, iproute2 and sch_netem needed)' % self.spec)
        return '/proc/%d/ns/net' % self.holder.pid

    def __exit__(self, *a):
        self.holder.send_signal(signal.SIGTERM)
        self.holder.wait()


def main():
    names = [a for a in sys.argv[1:] if not a.startswith('--') and a not in
             (opt('--netem'), opt('--pace'), opt('--client'), opt('--seat'))]
    if opt('--client'):
        n = names[0] if names[0].startswith('mp:') else 'mp:' + names[0]
        sys.exit(0 if client(n, opt('--client'), int(opt('--seat', '0'))) else 1)
    if not names:
        names = sorted('mp:' + f[:-4] for f in os.listdir(seattest.MP) if f.endswith('.txt'))
    else:
        names = [n if n.startswith('mp:') else 'mp:' + n for n in names]
    names = [n for n in names if seattest.humans_of(n)] if '--no-tests' not in sys.argv else []
    spec = opt('--netem')
    fails = 0

    def one(n, ns=None):
        t, bad = check(n, ns)
        return n, t, bad

    if spec:
        with Netns(spec) as ns:
            results = [one(n, ns) for n in names]    # one port in the namespace: one at a time
    else:
        with ThreadPoolExecutor(max(1, (os.cpu_count() or 2) // 2)) as ex:
            results = list(ex.map(one, names))
    for n, t, bad in results:
        print('%-22s %s (%.1f s)' % (n[3:], 'ok' if not bad else 'FAIL', t))
        for b in bad:
            print('    ' + b)
        fails += bool(bad)
    if '--desync' in sys.argv:
        bad = check_desync('mp:two_humans')
        print('%-22s %s' % ('desync detected', 'ok' if not bad else 'FAIL'))
        for b in bad:
            print('    ' + b)
        fails += bool(bad)
    if '--reconnect' in sys.argv:
        bad = check_reconnect('mp:two_humans')
        print('%-22s %s' % ('connections broken', 'ok' if not bad else 'FAIL'))
        for b in bad:
            print('    ' + b)
        fails += bool(bad)
    if '--silent' in sys.argv:
        bad = check_silent('mp:two_humans')
        print('%-22s %s' % ('a client silent', 'ok' if not bad else 'FAIL'))
        for b in bad:
            print('    ' + b)
        fails += bool(bad)
    if '--leave' in sys.argv:
        bad = check_leave('mp:two_humans')
        print('%-22s %s' % ('a client leaves', 'ok' if not bad else 'FAIL'))
        for b in bad:
            print('    ' + b)
        fails += bool(bad)
    print('%d of %d failed' % (fails, len(results) + ('--desync' in sys.argv) + ('--leave' in sys.argv) + ('--silent' in sys.argv) + ('--reconnect' in sys.argv))
          if fails else 'all %d ok' % (len(results) + ('--desync' in sys.argv) + ('--leave' in sys.argv) + ('--silent' in sys.argv) + ('--reconnect' in sys.argv)))
    sys.exit(1 if fails else 0)


if __name__ == '__main__':
    main()
