#!/usr/bin/env python3
"""Saving a network game at night and loading it (src/mpsave.c, docs/multiplayer.md stage 7).

    python3 tools/mpsavetest.py [NAME ...]        every test in tests/mpsave
    python3 tools/mpsavetest.py --record          write the .seats files anew
    python3 tools/mpsavetest.py --quick           without the network runs and the fired human
    python3 tools/mpsavetest.py --no-net          without the network runs

The tests in tests/mpsave are multiplayer tests like those in tests/mp
("# humans", "# options", "# play FILE until N", "N seat S ..."); an option
in braces, {NAME}, is the savegame test NAME's run wrote at its last night.
A test that saves (one no other test's options load from, run first):

  - played with --mp-save and without: the same lines (the save at night
    changes nothing of the game), and a savegame written;
  - over the network (tools/mptest.py's server, one client per seat, each
    with its own copy of the game files and its own --mp-save): every
    client's lines the same, and every client's savegame the same bytes.

The lobby server (build/openmtv serve, a public server's room alike) refuses
savegame messages it cannot check -- humans that are no list of stations,
too much data -- and goes on; when the host leaves, the room ends.

A test that loads:

  - MPSAVE_CHECK=1: what the game loaded, written again, is the same bytes
    (src/mpsave.c), the lines as NAME.seats;
  - over the network (as tools/mptest.py: the clients, then the server's log
    played again locally): every client's lines as NAME.seats.

Every machine computes every seat, so all of them write the same savegame and
start the same game from it: a desync would show as different lines."""
import os, shutil, subprocess, sys, tempfile, time
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import scenario, seattest, mptest, netserver

ROOT = scenario.ROOT
BIN = seattest.BIN
TESTS = os.path.join(ROOT, 'tests', 'mpsave')
seattest.MP = TESTS                      # (seattest's and mptest's helpers read tests from here)
SAVES = {}                               # test name -> the savegame its run wrote last
_options_of = seattest.options_of


def options_of(n):
    """the test's options with {NAME} -> that test's savegame"""
    out = []
    for w in _options_of(n):
        if w.startswith('{') and w.endswith('}'):
            if w[1:-1] not in SAVES:
                raise SystemExit('%s: no savegame of %s (run it first)' % (n[3:], w[1:-1]))
            w = SAVES[w[1:-1]]
        out.append(w)
    return out


seattest.options_of = options_of


def names():
    return sorted(f[:-4] for f in os.listdir(TESTS) if f.endswith('.txt'))


def loads_from(name):
    """the save test this test loads, or None"""
    for w in _options_of('mp:' + name):
        if w.startswith('{') and w.endswith('}'):
            return w[1:-1]
    return None


def game_dir(d):
    for f in ('SOURCE1.RSC', 'SOURCE2.RSC'):
        shutil.copy(os.path.join(scenario.GAME, f), d)
    return d


def newest_save(d):
    fs = sorted(f for f in os.listdir(d) if f.startswith('tag-') and f.endswith('.mtvmp'))
    return os.path.join(d, fs[-1]) if fs else None


def run(n, extra, env=None):
    """test n played locally -> (lines, stderr)"""
    d = tempfile.mkdtemp(prefix='mpsavetest_')
    try:
        script, base = seattest.mp_script(n, d)
        lang = scenario.lang_of(open(base).read().split('\n')) if base else 'e'
        args = [BIN, '--headless', '--game', game_dir(d), '--script', script, '--nosound', '--args', lang,
                '--humans', seattest.humans_of(n)] + options_of(n) + extra
        p = subprocess.run(args, capture_output=True, text=True, env=dict(os.environ, **(env or {})))
        return mptest.lines_of(p.stdout, p.stderr, p.returncode), p.stderr
    finally:
        shutil.rmtree(d, ignore_errors=True)


def save_lines(d):
    """'SAVE tag-NNN.mtvmp HASH' for every savegame in d (the hash as the game's parameters name it):
    recorded with the test's lines, so every machine and system must write the same bytes"""
    out = []
    for f in sorted(os.listdir(d)):
        if f.endswith('.mtvmp'):
            h = 1469598103934665603
            for b in open(os.path.join(d, f), 'rb').read():
                h = ((h ^ b) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
            out.append('SAVE %s %016x' % (f, h))
    return out


def with_saves(lines, saves):
    return lines[:-1] + saves + lines[-1:]       # (the end -- [quit] -- stays the last line)


def without_saves(lines):
    return [l for l in lines if not l.startswith('SAVE ')]


def compare(n, got, record):
    if record:
        open(os.path.join(TESTS, n[3:] + '.seats'), 'w').write('\n'.join(got) + '\n')
        return []
    want = mptest.recorded(n)
    return [] if got == want else ['against %s.seats: %s' % (n[3:], mptest.diff(want, got))]


def check_save(name, keep, record):
    """a test that saves: with and without --mp-save, the savegame kept in keep"""
    n = 'mp:' + name
    out = os.path.join(keep, name)
    os.makedirs(out)
    with ThreadPoolExecutor(2) as ex:
        a = ex.submit(run, n, ['--mp-save', out])
        b = ex.submit(run, n, [])
        (got, err), (plain, _) = a.result(), b.result()
    bad = []
    if got != plain:
        bad.append('the game with --mp-save is another: %s' % mptest.diff(plain, got))
    saved = [l for l in err.split('\n') if l.startswith('[mpsave]')]
    path = newest_save(out)
    if not path:
        bad.append('no savegame written (%s)' % ' | '.join(saved[-2:]))
    else:
        SAVES[name] = path
    return bad + compare(n, with_saves(got, save_lines(out)), record), saved


def check_load(name, record):
    n = 'mp:' + name
    got, err = run(n, [], {'MPSAVE_CHECK': '1'})
    bad = []
    if '[mpsave] round trip: the same' not in err:
        bad.append('the round trip: %s' % ' | '.join(l for l in err.split('\n') if 'round trip' in l or l.startswith('--mp-load')))
    return bad + compare(n, got, record)


def check_net_save(name):
    """a saving test over the network: the same lines everywhere, the same savegame bytes"""
    n = 'mp:' + name
    humans = seattest.humans_of(n)
    k = len(humans.split(','))
    d = tempfile.mkdtemp(prefix='mpsavetest_')
    try:
        script, _ = seattest.mp_script(n, d)
        port = mptest.free_port()
        srv = subprocess.Popen(netserver.command() + ['--seats', str(k), '--host', '127.0.0.1', '--port', str(port),
                                '--pace', 'fast', '--window', '64', '--drop-after', '1'], stderr=subprocess.PIPE, text=True)
        time.sleep(0.3)
        procs, outs = [], []
        for seat in range(k):
            g, o = os.path.join(d, 'game%d' % seat), os.path.join(d, 'save%d' % seat)
            os.mkdir(g); os.mkdir(o)
            outs.append(o)
            procs.append(subprocess.Popen(mptest.client_args(game_dir(g), script, humans, port, seat, n) + ['--mp-save', o],
                                          stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True))
        def finish(p):                   # (a client that hangs fails the test, it does not hang it)
            try:
                return p.communicate(timeout=900)
            except subprocess.TimeoutExpired:
                p.kill()
                return p.communicate()
        with ThreadPoolExecutor(k) as ex:
            res = list(ex.map(finish, procs))
        try:
            srv.communicate(timeout=10)
        except subprocess.TimeoutExpired:
            srv.kill()
        lines = [with_saves(mptest.lines_of(o, e, p.returncode), save_lines(out)) for (o, e), p, out in zip(res, procs, outs)]
        bad = ['client %d: %s' % (s, mptest.diff(lines[0], l)) for s, l in enumerate(lines) if l != lines[0]]
        want = mptest.recorded(n)
        if lines[0] != want:
            bad.append('client 0 against %s.seats: %s' % (name, mptest.diff(want, lines[0])))
        files = [sorted(f for f in os.listdir(o) if f.endswith('.mtvmp')) for o in outs]
        if not files[0] or any(f != files[0] for f in files):
            bad.append('the savegames written: %s' % files)
        else:
            for f in files[0]:
                data = [open(os.path.join(o, f), 'rb').read() for o in outs]
                if any(x != data[0] for x in data):
                    bad.append('%s differs between the clients' % f)
        return bad
    finally:
        shutil.rmtree(d, ignore_errors=True)


def check_server():
    """the lobby server refuses broken savegame messages (a public server's room must not break on
    them: a room that broke kept running) and stays usable; the host leaving ends it"""
    import json, socket, struct
    port = mptest.free_port()
    srv = subprocess.Popen(netserver.command() + ['--lobby', '--key', 'k', '--host', '127.0.0.1', '--port', str(port)],
                           stderr=subprocess.PIPE, text=True)
    bad = []
    try:
        for _ in range(50):
            try:
                so = socket.create_connection(('127.0.0.1', port), timeout=5)
                break
            except OSError:
                time.sleep(0.1)
        buf = [b'']

        def send(doc):
            b = json.dumps(doc).encode()
            try:
                so.sendall(struct.pack('<I', len(b)) + b)
            except OSError:
                pass

        def recv(kind):                  # the next message of that kind, or None (none, or the connection broke)
            end = time.time() + 5
            while time.time() < end:
                while len(buf[0]) >= 4 and len(buf[0]) >= 4 + struct.unpack('<I', buf[0][:4])[0]:
                    n = struct.unpack('<I', buf[0][:4])[0]
                    m, buf[0] = json.loads(buf[0][4:4 + n]), buf[0][4 + n:]
                    if m.get('t') == kind:
                        return m
                try:
                    d = so.recv(65536)
                except OSError:
                    return None
                if not d:
                    return None
                buf[0] += d
            return None
        send(dict(t='join', name='Host', key='k', version='v', files='f'))
        recv('state')
        for meta in ({'humans': [5]}, {'humans': 'xy'}, {'humans': [{'station': [1]}]}, {'humans': [{'station': 1}, {'station': 1}]},
                     {'humans': [{'station': 1, 'name': 'x' * 9000}]}):
            send(dict(t='save', hash='abc', data='', meta=meta))
            e = recv('error')
            if not e or e.get('code') != 'save':
                bad.append('not refused: %s' % json.dumps(meta)[:60])
        send(dict(t='save', hash='abc', data='x' * (300 << 10), meta={'humans': [{'station': 1}]}))
        e = recv('error')
        if not e or e.get('code') != 'save':
            bad.append('300 KB of savegame data not refused')
        send(dict(t='save', hash='abc', data='', meta={'humans': [{'station': 1, 'name': 'Anna'}], 'day': 3}))
        st = recv('state')
        if not st or not st.get('save') or st['save']['meta']['humans'][0]['name'] != 'Anna':
            bad.append('a good savegame message not taken: %s' % (st or {}).get('save'))
        send(dict(t='leave'))
        try:
            srv.wait(timeout=10)
        except subprocess.TimeoutExpired:
            bad.append('the host left, the lobby server runs on')
        so.close()
    finally:
        if srv.poll() is None:
            srv.kill()
            srv.wait()
    return bad


def check_public_rooms():
    """a public server's room opened by a connection that never joins (a first message with
    leading blanks) ends with that connection: such rooms had stayed until the server was full"""
    import json, struct
    import wsclient
    port = mptest.free_port()
    srv = subprocess.Popen(netserver.command() + ['--public', '--host', '127.0.0.1', '--port', str(port), '--pace', 'fast'],
                           stderr=subprocess.PIPE, text=True)
    try:
        for _ in range(50):
            try:
                ws = wsclient.ws_connect('ws://127.0.0.1:%d/lobby' % port, timeout=5)
                break
            except (OSError, ValueError):
                time.sleep(0.1)
        b = b' ' + json.dumps(dict(t='join', name='x', create=True, version='v', files='f')).encode()
        ws.sendall(struct.pack('<I', len(b)) + b)
        time.sleep(0.5)
        ws.close()
        time.sleep(1.5)
    finally:
        srv.terminate()
        _, err = srv.communicate(timeout=10)
    opened, closed = err.count(' opened'), err.count(' closed')
    return [] if opened == closed else ['%d room(s) opened, %d closed: %s' % (opened, closed, ' | '.join(err.strip().split('\n')[-3:]))]


def check_refusals(path):
    """the game refuses a savegame it cannot load whole, and other stations than its humans' -- at
    the start, with a message (a file from the network must not crash or hang every machine)"""
    import struct
    data = open(path, 'rb').read()
    at, parts = 8, []
    for _ in range(3):
        (n,) = struct.unpack_from('<I', data, at)
        parts.append(data[at + 4:at + 4 + n])
        at += 4 + n

    def file_of(meta, game, humans):
        return data[:8] + b''.join(struct.pack('<I', len(x)) + x for x in (meta, game, humans))
    d = tempfile.mkdtemp(prefix='mpsavetest_')
    bad = []
    try:
        game_dir(d)
        open(os.path.join(d, 'quit.txt'), 'w').write('600 quit\n')
        cases = [('a cut savegame part', file_of(parts[0], parts[1][:100], parts[2]), '1,2', 'savegame part has'),
                 ('an extension counting 65535 films', file_of(parts[0], parts[1] + b'MTVX\x03\x00\xff\xff\x32\x00\x32\x00', parts[2]), '1,2', 'counts too many'),
                 ('an extension of the wrong length', file_of(parts[0], parts[1] + b'MTVX\x03\x00\xd2\x00\x32\x00\x32\x00', parts[2]), '1,2', 'its counts'),
                 ('other stations than its humans\'', data, '1,3', 'other stations')]
        for what, f, humans, why in cases:
            p = os.path.join(d, 'bad.mtvmp')
            open(p, 'wb').write(f)
            r = subprocess.run([BIN, '--headless', '--game', d, '--script', os.path.join(d, 'quit.txt'), '--nosound', '--args', 'e',
                                '--humans', humans, '--mp-load', p], capture_output=True, text=True, timeout=120)
            if r.returncode != 2 or why not in r.stderr:
                bad.append('%s: exit %d, %s' % (what, r.returncode, ' | '.join(r.stderr.strip().split('\n')[-2:])))
    finally:
        shutil.rmtree(d, ignore_errors=True)
    return bad


def main():
    record = '--record' in sys.argv
    quick = '--quick' in sys.argv
    net = not quick and '--no-net' not in sys.argv and not record
    asked = [a for a in sys.argv[1:] if not a.startswith('--')]
    todo = asked or names()
    if quick:
        todo = [t for t in todo if not t.startswith('fired')]
    # the saves the chosen tests need come first
    need = set(todo) | {loads_from(t) for t in todo if loads_from(t)}
    savers = [t for t in names() if t in need and not loads_from(t)]
    loaders = [t for t in names() if t in todo and loads_from(t)]
    keep = tempfile.mkdtemp(prefix='mpsavetest_saves_')
    fails = 0
    try:
        t0 = time.time()
        bad = check_server()
        print('the lobby server and broken savegame messages: %s' % ('FAIL' if bad else 'ok (refused, the lobby goes on, it ends with its host)'))
        for b in bad:
            print('    ' + b)
        fails += bool(bad)
        bad = check_public_rooms()
        print('a public room nobody joins: %s' % ('FAIL' if bad else 'ok (it ends with its connection)'))
        for b in bad:
            print('    ' + b)
        fails += bool(bad)
        with ThreadPoolExecutor(max(1, len(savers))) as ex:
            res = dict(zip(savers, ex.map(lambda t: check_save(t, keep, record), savers)))
        for t in savers:
            bad, saved = res[t]
            print('%s: %s (%s)' % (t, 'FAIL' if bad else 'ok', '; '.join(l[9:].split(': ')[0] for l in saved) or 'nothing saved'))
            for b in bad:
                print('    ' + b)
            fails += bool(bad)
        if 'night' in SAVES:
            bad = check_refusals(SAVES['night'])
            print('broken savegames and other stations: %s' % ('FAIL' if bad else 'ok (refused at the start, with a message)'))
            for b in bad:
                print('    ' + b)
            fails += bool(bad)
        with ThreadPoolExecutor(max(1, len(loaders))) as ex:
            res = dict(zip(loaders, ex.map(lambda t: check_load(t, record), loaders)))
        for t in loaders:
            print('%s: %s (loads %s)' % (t, 'FAIL' if res[t] else 'ok', os.path.basename(SAVES.get(loads_from(t), '?'))))
            for b in res[t]:
                print('    ' + b)
            fails += bool(res[t])
        if net:
            for t in savers:
                if t not in todo:
                    continue
                bad = check_net_save(t)
                print('%s over the network: %s' % (t, 'FAIL' if bad else 'ok (the same lines and savegames on every client)'))
                for b in bad:
                    print('    ' + b)
                fails += bool(bad)
            for t in loaders:
                _, bad = mptest.check('mp:' + t)
                print('%s over the network: %s' % (t, 'FAIL' if bad else 'ok (every client and the log played again)'))
                for b in bad:
                    print('    ' + b)
                fails += bool(bad)
        print('%s in %.0f s' % ('recorded' if record else ('all ok' if not fails else '%d FAILED' % fails), time.time() - t0))
    finally:
        shutil.rmtree(keep, ignore_errors=True)
    sys.exit(1 if fails else 0)


if __name__ == '__main__':
    main()
