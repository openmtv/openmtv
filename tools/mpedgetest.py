#!/usr/bin/env python3
"""The network game's edge cases in the launcher (build/openmtv, launcher/lobby.go): what
the players see must be what tests/golden/mpedgetest.json holds (tools/golden.py;
recorded 2026-10-19, after the Python launcher it replaced, tools/deprecated/ until
then, had shown the same in every case).

    python3 tools/mpedgetest.py [--race] [-v] [--record] [CASE ...]

The players are tools/lobbytest.py's (Launcher: no browser, its own settings and
savegames, the test's content packs, the page's interface /api/mp/...); the LAN's
server as its host starts it (openmtv serve), the internet's one this test starts
(--public, on this machine).  Compared per case: the answers and states it names --
error messages without the system's own words in parentheses, in English and German
where the page shows them.  Each case also checks that the edge was reached (an
error where one belongs, the state that follows).

--race: the Go launchers are a build with Go's race detector (go build -race,
needs cgo: build/openmtv-race); any race it reports fails the case.

The cases:
  join-nowhere       a game's address where none is
  internet-down      the public server not there (no connection)
  not-a-server       an HTTP server that is no game server; wss:// to a plain port
  rooms-down         the server's list from a server that does not answer
  unknown-room       a room code the server does not know
  host-leaves        the host leaves the lobby: the guest's lobby closes
  guest-leaves       a guest leaves: the host is alone again
  four-players       a fourth player (three stations): refused
  packs-refused      packs unknown, twice, not playable (English and German)
  packs-back         the host's packs, then the original game again: the guest follows
  server-dies        the LAN server ends in the lobby: both lobbies close
  saves-refused      savegames: missing, not one, damaged, packs without tables, other files
  other-copy         the guest has another copy of that night: kept beside, the host's fetched
  garbage            a server sending what is not JSON, then too large a message
  quit-hosting       the launcher ended while it hosts: its server goes too
  port-taken         a LAN game opened on a port something else holds
  many-at-once       fifty changes at once while the lobby is read

Standard library only (Python 3.8+)."""
import json
import os
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time
import urllib.parse
import urllib.request
from http.server import BaseHTTPRequestHandler, HTTPServer

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..'))
sys.path.insert(0, HERE)
import golden  # noqa: E402
import lobbytest  # noqa: E402
import netserver  # noqa: E402
from lobbytest import Launcher, free_port  # noqa: E402

VERBOSE = '-v' in sys.argv
SETTINGS = dict(lang='e', fixes='all', difficulty=2, speed4=False, aiimp='off', money=200000)


def until(f, timeout=20):
    end = time.time() + timeout
    while time.time() < end:
        v = f()
        if v:
            return v
        time.sleep(0.15)
    return None


def short(msg):
    """an error message without the system's own words (in parentheses at its end)"""
    msg = msg or ''
    i = msg.find(' (')
    return msg[:i] if i >= 0 and msg.endswith(')') else msg


def err_of(r):
    return (r.get('http_error'), short(r.get('error')) if isinstance(r.get('error'), str) else r.get('error'))


def phase(s):
    return ((s or {}).get('state') or {}).get('phase')


def players(s):
    return sorted(p['name'] for p in ((s or {}).get('state') or {}).get('players', []))


def pids_of(text):
    """the processes whose command line holds TEXT (not this one)"""
    out = []
    if os.name == 'nt':
        r = subprocess.run(['powershell', '-NoProfile', '-Command',
                            'Get-CimInstance Win32_Process | ForEach-Object { "$($_.ProcessId) $($_.CommandLine)" }'], capture_output=True, text=True)
    else:
        r = subprocess.run(['ps', '-axo', 'pid=,command='], capture_output=True, text=True)
    for line in r.stdout.splitlines():
        pid, _, cmd = line.strip().partition(' ')
        if pid.isdigit() and text in cmd and int(pid) != os.getpid() and 'ps -axo' not in cmd and 'Get-CimInstance' not in cmd:
            out.append(int(pid))
    return out


class Plain(BaseHTTPRequestHandler):
    """an HTTP server that is no game server"""
    def do_GET(self):
        self.send_response(200)
        self.send_header('Content-Length', '5')
        self.end_headers()
        self.wfile.write(b'hello')

    def log_message(self, *a):
        pass


def plain_server():
    srv = HTTPServer(('127.0.0.1', 0), Plain)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    return srv


class Public:
    """the internet's server, on this machine (openmtv serve --public, pace fast)"""
    def __init__(self):
        self.port = free_port()
        self.log = tempfile.NamedTemporaryFile(prefix='mpedge-public-', suffix='.log', delete=False)
        self.proc = subprocess.Popen(netserver.command() + ['--public', '--host', '127.0.0.1',
                                      '--port', str(self.port), '--pace', 'fast'], cwd=ROOT, stdin=subprocess.DEVNULL,
                                     stdout=subprocess.DEVNULL, stderr=self.log)
        self.url = 'ws://127.0.0.1:%d' % self.port
        until(lambda: self._up(), 15)

    def _up(self):
        try:
            urllib.request.urlopen('http://127.0.0.1:%d/healthz' % self.port, timeout=2).read()
            return True
        except OSError:
            return False

    def stop(self):
        self.proc.terminate()
        self.proc.wait(timeout=10)
        self.log.close()
        with open(self.log.name, encoding='utf-8', errors='replace') as f:
            self.said = f.read()
        os.unlink(self.log.name)


def lan_game(a, b=None, **more):
    """A opens a game in the LAN (dry, this machine only), B joins -> port"""
    port = free_port()
    r = a.post('host', dict(dict(name='Anna', port=port, listen='127.0.0.1', dry=True, pace='fast'), **more))
    if r.get('http_error'):
        raise AssertionError('no game opened: %s' % r)
    if b:
        r = b.post('join', dict(address='127.0.0.1:%d' % port, name='Ben', dry=True))
        if r.get('http_error'):
            raise AssertionError('not joined: %s' % r)
        until(lambda: len(players(a.state())) == 2)
    return port


# ---------------------------------------------------------------- the cases: -> (what was seen, the edge reached)

def case_join_nowhere(L):
    a = L('A')
    port = free_port()
    r = a.post('join', dict(address='127.0.0.1:%d' % port, name='Ben', dry=True))
    s = a.state()
    return (dict(answer=[x.replace(str(port), 'PORT') if isinstance(x, str) else x for x in err_of(r)], role=s['role'], connected=s['connected']),
            r.get('http_error') == 400 and 'no game at' in r.get('error', ''))


def case_internet_down(L):
    a = L('A')
    url = 'ws://127.0.0.1:%d' % free_port()
    r = a.post('host', dict(name='Anna', internet=True, server=url, dry=True))
    r2 = a.post('join', dict(name='Anna', room='abcd-efgh', server=url, dry=True))
    return dict(host=err_of(r), join=err_of(r2)), 'no connection to 127.0.0.1' in r.get('error', '')


def case_not_a_server(L):
    a = L('A')
    srv = plain_server()
    try:
        p = srv.server_address[1]
        r = a.post('join', dict(name='Ben', room='abcd-efgh', server='ws://127.0.0.1:%d' % p, dry=True))
        r2 = a.post('join', dict(name='Ben', room='abcd-efgh', server='wss://127.0.0.1:%d' % p, dry=True))
    finally:
        srv.shutdown()
    return (dict(plain=(r.get('http_error'), (r.get('error') or '').replace(str(p), 'PORT')), tls=err_of(r2)),
            'no game server there' in r.get('error', '') and 'no secure connection' in r2.get('error', ''))


def case_rooms_down(L):
    a = L('A')
    port = free_port()
    url = 'ws://127.0.0.1:%d' % port
    r = a.call('GET', '/api/mp/rooms?server=' + urllib.parse.quote(url, safe=''))
    r = r if isinstance(r, dict) else {'list': r}
    return (dict(answer=[x.replace(str(port), 'PORT') if isinstance(x, str) else x for x in err_of(r)]),
            r.get('http_error') == 400 and 'does not answer' in r.get('error', ''))


def case_unknown_room(L, public):
    a = L('A')
    r = a.post('join', dict(name='Ben', room='Nix-Da', server=public.url, dry=True))
    e = until(lambda: a.state().get('error'))
    s = a.state()
    return dict(answer=r.get('http_error'), room=s['room'], error=e and e.get('code'), phase=phase(s)), bool(e)


def case_host_leaves(L):
    a, b = L('A'), L('B')
    lan_game(a, b)
    a.post('leave')
    # (the host sends "leave" and ends its server: whether the server still tells the guest
    # "the host closed the game" or only the connection breaks is a race -- in both launchers)
    s = until(lambda: (lambda x: x if phase(x) == 'closed' and not x['connected'] else None)(b.state()))
    sa = a.state()
    why = ((s or {}).get('state') or {}).get('why')
    return (dict(guest_phase=phase(s), why=why in ('the host closed the game', 'the connection to the game is gone'),
                 guest_connected=(s or {}).get('connected'), host_role=sa['role'], host_state=sa['state']), s is not None)


def case_guest_leaves(L):
    a, b = L('A'), L('B')
    lan_game(a, b)
    b.post('leave')
    s = until(lambda: (lambda x: x if len(players(x)) == 1 else None)(a.state()))
    until(lambda: b.state()['state'] is None, 5)  # (the guest's own launcher may be a moment later)
    sb = b.state()
    return dict(host_players=players(s), host_phase=phase(s), guest_role=sb['role'], guest_state=sb['state']), s is not None


def case_four_players(L):
    a, b = L('A'), L('B')
    port = lan_game(a, b)
    addr = '127.0.0.1:%d' % port
    r1 = a.post('bot', dict(action='join', address=addr, name='Cleo'))
    until(lambda: len(players(a.state())) == 3)
    r2 = b.post('bot', dict(action='join', address=addr, name='Dora'))
    e = until(lambda: b.post('bot', dict(action='noop')).get('error'))
    s = a.state()
    return dict(third=r1.get('http_error'), fourth=r2.get('http_error'), error=e and e.get('code'), players=players(s)), bool(e) and len(players(s)) == 3


def case_packs_refused(L):
    a = L('A')
    lan_game(a)
    halb = os.path.join(lobbytest.mods_dir(), 'zz-edge-halb')
    out = {}
    try:
        write_half_pack(halb)
        for spec in ('gibtsnicht', 'test-filme+test-filme', 'zz-edge-halb', 'test-filme+zz-edge-halb'):
            for lang in ('en', 'de'):
                r = a.post('game', dict(settings=SETTINGS, packs=spec), lang=lang)
                out['%s/%s' % (spec, lang)] = (r.get('http_error'), r.get('error'))
    finally:
        shutil.rmtree(halb, ignore_errors=True)
    s = a.state()
    out['mods'] = s['mods']['status']
    return out, all(v[0] == 400 for k, v in out.items() if k != 'mods')


def write_half_pack(d):
    """a pack with one new film on the market, without its German title and genre"""
    import content, datafixes
    res = content.Resources([os.path.join(lobbytest_game(), f) for f in ('SOURCE1.RSC', 'SOURCE2.RSC')])
    o = json.loads(json.dumps(datafixes.apply(content.export(res))[0], ensure_ascii=False))
    r = json.loads(json.dumps(next(x for x in o['films']['records'] if x['id'] == 5)))
    r['id'], r['genre'], r['flags'] = 201, 0, r.get('flags', 0) | 0x20
    r['text']['de'][0] = ''
    o['films']['records'].append(r)
    os.makedirs(d)
    with open(os.path.join(d, 'mod.json'), 'w', encoding='utf-8') as f:
        json.dump({'title': 'Halb'}, f)
    with open(os.path.join(d, 'films.json'), 'w', encoding='utf-8') as f:
        json.dump(o['films'], f, ensure_ascii=False)


def lobbytest_game():
    return os.path.abspath(os.path.join(ROOT, os.environ.get('GAME', '..')))


def case_packs_back(L):
    a, b = L('A'), L('B')
    lan_game(a, b)
    a.post('game', dict(settings=SETTINGS, packs='test-werbung'))
    s1 = until(lambda: (lambda x: x if x['mods']['status'] in ('ok', 'failed', 'differ') else None)(b.state()), 120)
    host_hash = a.state()['mods']['hash']
    a.post('game', dict(settings=SETTINGS, packs=''))
    s2 = until(lambda: (lambda x: x if x['mods']['status'] == 'none' else None)(b.state()), 30)
    sa = a.state()
    return (dict(built=s1 and (s1['mods']['status'], bool(host_hash) and s1['mods']['hash'] == host_hash),
                 back=s2 and (s2['mods']['status'], s2['mods']['hash']), host=(sa['mods']['status'], sa['mods']['hash']),
                 server_mods=(sa['state'].get('settings') or {}).get('mods')), bool(s1 and s2))


def case_server_dies(L):
    a, b = L('A'), L('B')
    port = lan_game(a, b)
    pids = pids_of('--port %d' % port)
    for p in pids:
        if os.name == 'nt':
            subprocess.run(['taskkill', '/F', '/PID', str(p)], capture_output=True)
        else:
            os.kill(p, 9)
    sa = until(lambda: (lambda x: x if phase(x) == 'closed' else None)(a.state()))
    sb = until(lambda: (lambda x: x if phase(x) == 'closed' else None)(b.state()))
    return (dict(killed=len(pids), host=(phase(sa), (sa or {}).get('connected')), guest=(phase(sb), (sb or {}).get('connected')),
                 why=((sb or {}).get('state') or {}).get('why')), len(pids) == 1 and sa and sb)


def save_bytes(meta):
    m = json.dumps(meta).encode()
    hl = 627
    hum = bytes([1, 0, 2]) + b''.join(bytes([s, 0, hl & 255, hl >> 8]) + bytes(hl) for s in (1, 2))
    return b'MTVMPSV1' + struct.pack('<I', len(m)) + m + struct.pack('<I', 37605) + bytes(37605) + struct.pack('<I', len(hum)) + hum


def case_saves_refused(L):
    a = L('A')
    lan_game(a)
    base = a.post('testsave', dict(game='edge', day=3))
    good = os.path.join(a.saves, 'edge', 'tag-003.mtvmp')
    meta = json.loads(open(good, 'rb').read()[12:12 + struct.unpack_from('<I', open(good, 'rb').read(), 8)[0]])
    for game, data, info in (('damaged', b'MTVMPSV1\x05\x00', {}),
                             ('packs', open(good, 'rb').read(), dict(game='packs', packs=['Test-Filme'], settings={})),
                             ('files', save_bytes(dict(meta, files='0123456789abcdef')), dict(game='files', packs=[], settings={}))):
        d = os.path.join(a.saves, game)
        os.makedirs(d)
        with open(os.path.join(d, 'tag-003.mtvmp'), 'wb') as f:
            f.write(data)
        with open(os.path.join(d, 'game.json'), 'w') as f:
            json.dump(info, f)
    out = {}
    for name, body in (('missing', dict(game='edge', file='tag-009.mtvmp')), ('not-one', dict(game='edge', file='x.txt')),
                       ('damaged', dict(game='damaged', file='tag-003.mtvmp')), ('packs', dict(game='packs', file='tag-003.mtvmp')),
                       ('files', dict(game='files', file='tag-003.mtvmp')), ('good', dict(game='edge', file='tag-003.mtvmp'))):
        for lang in ('en', 'de'):
            r = a.post('save', body, lang=lang)
            out['%s/%s' % (name, lang)] = (r.get('http_error'), r.get('error')) if r.get('http_error') else ('ok', r['save']['status'], r['save']['day'])
    lst = a.call('GET', '/api/mp/saves')
    out['list'] = sorted((g['game'], len(g['saves'])) for g in lst)
    return out, isinstance(base, list) and out['good/en'][0] == 'ok' and all(v[0] == 400 for k, v in out.items() if k not in ('list', 'good/en', 'good/de'))


def case_other_copy(L):
    a, b = L('A'), L('B')
    lan_game(a, b)
    a.post('testsave', dict(game='edge', day=3))
    mine = os.path.join(a.saves, 'edge', 'tag-003.mtvmp')
    data = open(mine, 'rb').read()
    other = data[:-1] + b'\x01'                  # (another copy of that night: one byte else)
    d = os.path.join(b.saves, 'edge')
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, 'tag-003.mtvmp'), 'wb') as f:
        f.write(other)
    r = a.post('save', dict(game='edge', file='tag-003.mtvmp'))
    s = until(lambda: (lambda x: x if x['save'].get('status') in ('ok', 'failed') else None)(b.state()), 30)
    files = sorted(os.listdir(d))
    got = open(os.path.join(d, 'tag-003.mtvmp'), 'rb').read()
    bak = [f for f in files if f.endswith('.bak')]
    return (dict(host=r.get('save', {}).get('status'), guest=s and s['save']['status'], files=[f if not f.endswith('.bak') else 'BAK' for f in files],
                 same=got == data, bak_is_old=bool(bak) and open(os.path.join(d, bak[0]), 'rb').read() == other),
            bool(s) and got == data and bool(bak))


def case_garbage(L):
    a = L('A')
    srv = socket.socket()
    srv.bind(('127.0.0.1', 0))
    srv.listen(1)
    port = srv.getsockname()[1]
    seen = {}

    def serve():
        c, _ = srv.accept()
        n = struct.unpack('<I', c.recv(4))[0]
        hello = b''
        while len(hello) < n:
            hello += c.recv(n - len(hello))
        seen['hello'] = json.loads(hello)
        for m in (b'this is no JSON', json.dumps(dict(t='state', phase='lobby', players=[dict(name='Ben', station=0, ready=False)],
                                                       problems=[], settings={})).encode()):
            c.sendall(struct.pack('<I', len(m)) + m)
        time.sleep(1.0)
        c.sendall(struct.pack('<I', 0x7FFFFFFF) + b'xx')    # (too large a message: never all of it)
        time.sleep(1.0)
        c.close()
    t = threading.Thread(target=serve, daemon=True)
    t.start()
    r = a.post('join', dict(address='127.0.0.1:%d' % port, name='Ben', dry=True))
    s1 = until(lambda: (lambda x: x if phase(x) == 'lobby' else None)(a.state()), 10)
    log1 = list((s1 or {}).get('log') or [])
    s2 = until(lambda: (lambda x: x if phase(x) == 'closed' else None)(a.state()), 15)
    t.join(5)
    srv.close()
    h = seen.get('hello') or {}
    # (seatkey: the Go launcher's game takes its seat's secret; the deprecated Python one never will)
    return (dict(joined=r.get('role'), hello=sorted(k for k in h if k != 'seatkey'), hello_files=len(h.get('files', '')), state=phase(s1),
                 noted_error=any(x.startswith('error:') for x in log1), end=phase(s2), connected=(s2 or {}).get('connected')),
            bool(s1 and s2) and any(x.startswith('error:') for x in log1))


def case_quit_hosting(L):
    a = L('A')
    port = lan_game(a)
    before = pids_of('--port %d' % port)
    try:
        r = a.call('POST', '/api/quit', {})
    except OSError:
        r = {}
    gone = until(lambda: not pids_of('--port %d' % port), 15)
    ended = until(lambda: a.proc.poll() is not None, 15)
    return dict(answer=r, server_before=len(before), server_gone=bool(gone), launcher_ended=ended is not None), len(before) == 1 and gone


def case_port_taken(L):
    a = L('A')
    hold = socket.socket()
    hold.bind(('127.0.0.1', 0))
    hold.listen(5)
    port = hold.getsockname()[1]
    try:
        r = a.post('host', dict(name='Anna', port=port, listen='127.0.0.1', dry=True, pace='fast'))
        s = a.state()
    finally:
        hold.close()
    return (dict(answer=(r.get('http_error'), short(r.get('error')).replace(str(port), 'PORT')), role=s['role'], connected=s['connected']),
            r.get('http_error') == 400 and 'no game opened on port' in r.get('error', ''))


def case_many_at_once(L):
    a, b = L('A'), L('B')
    lan_game(a, b)
    errs = []

    def poke(i):
        try:
            b.post('me', dict(station=2 + i % 2, ready=bool(i % 3)))
            a.state()
            b.state()
            a.post('game', dict(settings=dict(SETTINGS, difficulty=1 + i % 3), packs=''))
        except Exception as e:
            errs.append(repr(e))
    ts = [threading.Thread(target=poke, args=(i,)) for i in range(50)]
    for t in ts:
        t.start()
    for t in ts:
        t.join(60)
    b.post('me', dict(station=3, ready=True))
    s = until(lambda: (lambda x: x if any(p['name'] == 'Ben' and p['station'] == 3 and p['ready'] for p in x['state']['players']) else None)(a.state()))
    return dict(errors=errs, settled=bool(s), players=players(s)), not errs and bool(s)


CASES = [('join-nowhere', case_join_nowhere), ('internet-down', case_internet_down), ('not-a-server', case_not_a_server),
         ('rooms-down', case_rooms_down), ('unknown-room', case_unknown_room), ('host-leaves', case_host_leaves),
         ('guest-leaves', case_guest_leaves), ('four-players', case_four_players), ('packs-refused', case_packs_refused),
         ('packs-back', case_packs_back), ('server-dies', case_server_dies), ('saves-refused', case_saves_refused),
         ('other-copy', case_other_copy), ('garbage', case_garbage), ('quit-hosting', case_quit_hosting),
         ('port-taken', case_port_taken), ('many-at-once', case_many_at_once)]


def main(argv):
    only = [a for a in argv if not a.startswith('-')]
    race = '--race' in argv
    if race:
        exe = os.path.join(ROOT, 'build', 'madtv-race')
        r = subprocess.run(['go', 'build', '-race', '-o', exe, '.'], cwd=os.path.join(ROOT, 'launcher'),
                           env=dict(os.environ, CGO_ENABLED='1'), capture_output=True, text=True)
        if r.returncode:
            sys.exit('mpedgetest: no race detector build: ' + r.stderr)
        os.environ['MADTV_GO_EXE'] = exe            # (the launchers, their LAN servers, the public server)
    public = Public()
    gold = golden.Golden('mpedgetest', '--record' in argv)
    failed = 0
    try:
        for name, fn in CASES:
            if only and name not in only:
                continue
            races, made = [], []

            def L(label):
                x = Launcher(label, free_port())
                made.append(x)
                return x
            try:
                if fn.__code__.co_argcount == 2:
                    seen, reached = fn(L, public)
                else:
                    seen, reached = fn(L)
            except Exception as e:      # (the case itself broke: shown as what was seen)
                seen, reached = {'broke': repr(e)}, False
            finally:
                for x in made:
                    if x.proc.poll() is None:
                        x.stop()
                    else:
                        x.stderr = x.errors()
                        shutil.rmtree(x.cfg, ignore_errors=True)
                    if 'DATA RACE' in (x.stderr or ''):
                        races.append(x.stderr[x.stderr.index('WARNING: DATA RACE'):][:1500])
                try:                    # (the LAN server its host started: build/mp/server.log)
                    with open(os.path.join(ROOT, 'build', 'mp', 'server.log'), encoding='utf-8', errors='replace') as f:
                        said = f.read()
                    if 'DATA RACE' in said:
                        races.append('server: ' + said[said.index('WARNING: DATA RACE'):][:1500])
                except OSError:
                    pass
            same, detail = gold.check(name, json.loads(json.dumps(seen, sort_keys=True, default=str)))
            good = same and reached and not races
            failed += not good
            print('%-14s %s' % (name, 'ok' if good else 'DIFFERENT' if not same else 'NOT REACHED' if not races else 'RACE'))
            if not good or VERBOSE:
                print(detail or '    %s%s' % ('' if reached else '(edge not reached) ', json.dumps(seen, ensure_ascii=False, sort_keys=True, default=str)[:900]))
                for r in races:
                    print('    ' + r.replace('\n', '\n    '))
    finally:
        public.stop()
    gold.save()
    if 'DATA RACE' in getattr(public, 'said', ''):
        print('public server: RACE\n    ' + public.said[public.said.index('WARNING: DATA RACE'):][:1500])
        failed += 1
    print('mpedgetest: %d of %d cases not the same' % (failed, len([c for c in CASES if not only or c[0] in only])) if failed
          else 'mpedgetest: all the same' + (' (race detector: none)' if race else ''))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
