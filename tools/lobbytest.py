#!/usr/bin/env python3
"""Two launchers on one machine meet in a network game's lobby and start it
(multiplayer stage 5, docs/multiplayer.md).

    python3 tools/lobbytest.py [--pack NAME] [--frames N] [--real] [--internet [URL]] [--load]

Two launchers (build/openmtv launcher, launcher/lobby.go; no browser) play two
players through their pages' interface (/api/mp/...); a launcher opens a LAN game
with its own server (openmtv serve); --internet: the public server.  Their packs
are the test's content packs (tests/packs, in a copy: $MADTV_MODS):

  * A opens a game (the lobby server as A's child process, on this
    machine only, pace fast) and sets it: the language, the fixes, the
    difficulty, and the content pack NAME (default test-filme) -- its
    tables go through the server; B builds them from its own original
    files and reports the same files' hash as A's build.
  * B finds the game by the LAN search, joins, takes station 2 (station 1
    is A's: refused), says it is ready; A's settings changed after that
    make B not ready again.
  * A cannot start while B is not ready; then it starts.  Both launchers
    are dry: they say how they would start their games.  This test starts
    those two command lines, without windows (--headless --nosound and a
    script of hashes): both games must meet in the server, come into the
    first morning and compute the same game (their STATE lines equal).
  * --real: the launchers start their games themselves, as for players
    (SDL's dummy drivers: no window, no sound); both must
    meet in the server and play on; then they are ended.
  * --internet: the same on a public server (stage 6): this test starts
    one (--public, pace fast, on this machine; or URL,
    e.g. wss://openmtv.net: its pace), A opens a room there by
    WebSocket, B finds it in the server's list and joins by its code; the
    games go through their launchers' bridges.
  * --cut (with --real --internet, its own server): the launchers reach the
    server through a relay; once both games play, every connection is cut --
    the lobby's and the games' bridges': each game comes back on a new
    connection through its launcher's bridge (RESUME) and plays on.
  * --load: on from a savegame (src/mpsave.c) instead of the pack: first a
    network game of two humans is played to its first night here
    (tests/mpsave/night.txt, saved into A's savegames); A chooses it in the
    lobby, B fetches it through the server (B has none), both must sit on
    its stations; both start with --mp-load (each its own copy, the same
    bytes) and compute the same game, on from the saved day.

Standard library only."""
import atexit
import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time
import urllib.parse
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..'))
sys.path.insert(0, HERE)
import netserver  # noqa: E402

_MODS = []


def mods_dir():
    """the launchers' packs: a copy of tests/packs for this run ($MADTV_MODS)"""
    if not _MODS:
        d = tempfile.mkdtemp(prefix='lobbytest-mods-')
        atexit.register(shutil.rmtree, d, True)
        for n in os.listdir(os.path.join(ROOT, 'tests', 'packs')):
            shutil.copytree(os.path.join(ROOT, 'tests', 'packs', n), os.path.join(d, n))
        _MODS.append(d)
    return _MODS[0]


def free_port():
    s = socket.socket()
    s.bind(('127.0.0.1', 0))
    p = s.getsockname()[1]
    s.close()
    return p


def opt(name, default=None):
    i = sys.argv.index(name) + 1 if name in sys.argv else len(sys.argv)
    return sys.argv[i] if i < len(sys.argv) else default


class Launcher:
    def __init__(self, label, port):
        self.label, self.port, self.kind = label, port, 'go'
        # (--real: games without windows; the settings files -- multiplayer.ini -- in a folder of
        # the test's own, never the player's)
        self.cfg = tempfile.mkdtemp(prefix='lobbytest-cfg-')
        self.saves = os.path.join(self.cfg, 'mp-saves')       # (its network games' savegames: its own)
        env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', XDG_CONFIG_HOME=self.cfg, APPDATA=self.cfg,
                   MADTV_MP_SAVES=self.saves, MADTV_MODS=mods_dir())
        # (the Go program: $MADTV_GO_EXE if set -- tools/mpedgetest.py's race detector build)
        go_exe = os.environ.get('MADTV_GO_EXE') or os.path.join(ROOT, 'build', 'openmtv' + ('.exe' if os.name == 'nt' else ''))
        cmd = [go_exe, 'launcher', '--no-browser', '--idle', '0', '--port', str(port)]
        self.errpath = os.path.join(self.cfg, 'stderr.txt')     # (a file: a pipe nobody reads could stop the launcher)
        self.proc = subprocess.Popen(cmd, cwd=ROOT, stdout=subprocess.DEVNULL, stderr=open(self.errpath, 'w'), env=env)
        for _ in range(300):
            try:
                self.get('/api/mp')
                return
            except OSError:
                time.sleep(0.1)
        raise SystemExit('%s: the launcher did not start: %s' % (label, self.errors()))

    def errors(self):
        try:
            with open(self.errpath, encoding='utf-8', errors='replace') as f:
                return f.read()
        except OSError:
            return ''

    def call(self, method, path, body=None, lang='en'):
        req = urllib.request.Request('http://127.0.0.1:%d%s' % (self.port, path), method=method,
                                     data=json.dumps(body).encode() if body is not None else None,
                                     headers={'Content-Type': 'application/json', 'X-UI-Lang': lang})
        try:
            with urllib.request.urlopen(req, timeout=120) as r:
                return json.loads(r.read().decode())
        except urllib.error.HTTPError as e:
            return dict(http_error=e.code, **json.loads(e.read().decode() or '{}'))

    def get(self, path):
        return self.call('GET', path)

    def post(self, what, body=None, lang='en'):
        return self.call('POST', '/api/mp/' + what, body or {}, lang)

    def state(self):
        return self.get('/api/mp')

    def stop(self):
        try:
            self.post('leave')
        except OSError:
            pass
        self.proc.terminate()
        self.proc.wait(timeout=10)
        self.stderr = self.errors()
        shutil.rmtree(self.cfg, ignore_errors=True)


def until(what, f, timeout=60):
    end = time.time() + timeout
    while time.time() < end:
        v = f()
        if v:
            return v
        time.sleep(0.2)
    raise AssertionError('timed out: ' + what)


def night_save(into):
    """a network game of two humans to its first night (tests/mpsave/night.txt): its savegame in
    into/GAME/, with the game.json a launcher writes -> (game, file)"""
    sys.path.insert(0, HERE)
    import seattest, mptest
    seattest.MP = os.path.join(ROOT, 'tests', 'mpsave')
    game = 'lobbytest-night'
    d = os.path.join(into, game)
    os.makedirs(d)
    t = tempfile.mkdtemp(prefix='lobbytest_night_')
    try:
        script, _ = seattest.mp_script('mp:night', t)
        for f in ('SOURCE1.RSC', 'SOURCE2.RSC'):
            shutil.copy(os.path.join(seattest.scenario.GAME, f), t)
        p = subprocess.run([seattest.BIN, '--headless', '--game', t, '--script', script, '--nosound', '--args', 'e',
                            '--humans', seattest.humans_of('mp:night')] + seattest.options_of('mp:night') + ['--fixes', '--logical-time', '--mp-save', d],
                           capture_output=True, text=True)
    finally:
        shutil.rmtree(t, ignore_errors=True)
    files = sorted(f for f in os.listdir(d) if f.endswith('.mtvmp'))
    if not files:
        raise SystemExit('no savegame from the night: %s' % ' | '.join(p.stderr.split('\n')[-4:]))
    with open(os.path.join(d, 'game.json'), 'w') as f:
        json.dump(dict(game=game, created='2026-10-11 20:00', packs=[], settings=dict(lang='e', fixes='all', speed4=False)), f)
    return game, files[-1]


def main():
    pack = opt('--pack', 'test-filme')
    load = '--load' in sys.argv
    frames = int(opt('--frames', '3000'))
    real = '--real' in sys.argv
    dry = not real
    port = free_port()
    fails = []
    internet = '--internet' in sys.argv
    url = opt('--internet') if internet else None
    url = url if url and not url.startswith('--') else None
    server = None
    srvlog = os.path.join(ROOT, 'build', 'mp', 'server.log')
    if internet and not url:
        os.makedirs(os.path.dirname(srvlog), exist_ok=True)
        srvlog = os.path.join(ROOT, 'build', 'mp', 'public.log')
        server = subprocess.Popen(netserver.command() + ['--public', '--host', '127.0.0.1',
                                   '--port', str(port), '--pace', 'fast'], cwd=ROOT, stdin=subprocess.DEVNULL,
                                  stdout=subprocess.DEVNULL, stderr=open(srvlog, 'w'))
        url = 'ws://127.0.0.1:%d' % port
        if '--cut' in sys.argv:              # (the launchers through a relay: its connections cut later)
            import mptest
            relay = mptest.Relay(port)
            url = 'ws://127.0.0.1:%d' % relay.port
        for _ in range(100):
            try:
                urllib.request.urlopen('http://127.0.0.1:%d/healthz' % port, timeout=2).read()
                break
            except OSError:
                if server.poll() is not None:
                    raise SystemExit('the public server did not start: %s' % open(srvlog).read())
                time.sleep(0.1)

    def check(ok, msg):
        print('%s %s' % ('ok  ' if ok else 'FAIL', msg))
        if not ok:
            fails.append(msg)

    a = Launcher('A', free_port())
    b = Launcher('B', free_port())
    d = tempfile.mkdtemp(prefix='lobbytest_')
    games = []
    try:
        if load:
            t0 = time.time()
            save_game, save_file = night_save(a.saves)
            check(True, 'a night saved for A: %s/%s (%.0f s)' % (save_game, save_file, time.time() - t0))
        settings = dict(lang='e', fixes='all', difficulty=3, speed4=False, aiimp='strong', money=200000, offers=15, filmoffers=12, sertop=False, knock=10, castfit=True)
        if internet:
            r = a.post('host', dict(name='Anna', internet=True, server=url, dry=dry, settings=settings, packs=''))
            room = r.get('room')
            check(r.get('role') == 'host' and room and not r.get('http_error'), 'A opened a game on %s: room %s %s' % (url, room, r.get('error') or ''))
            q = urllib.parse.quote(url, safe='')
            found = until('the server lists the game', lambda: [g for g in a.get('/api/mp/rooms?server=' + q) if g.get('room') == room])
            check(found[0]['name'] == 'Anna' and found[0]['players'] == 1, 'B found it in the list: %s' % json.dumps(found[0]))
            r = b.post('join', dict(room=room, server=url, name='Ben', dry=dry))
        else:
            r = a.post('host', dict(name='Anna', port=port, listen='127.0.0.1', dry=dry, pace='fast', settings=settings, packs=''))
            check(r.get('role') == 'host' and not r.get('http_error'), 'A opened a game on port %d' % port)
            found = until('the search finds the game', lambda: [g for g in b.get('/api/mp/find?port=%d' % port) if g.get('address', '').endswith(':%d' % port)])
            check(found[0]['name'] == 'Anna' and found[0]['players'] == 1, 'B found it: %s' % json.dumps(found[0]))
            r = b.post('join', dict(address=found[0]['address'], name='Ben', dry=dry))
        check(r.get('role') == 'guest', 'B joined')
        st = until('both in the lobby', lambda: (lambda s: s if s.get('state') and len(s['state']['players']) == 2 else None)(a.state()))
        names = sorted(p['name'] for p in st['state']['players'])
        check(names == ['Anna', 'Ben'], 'both in the lobby: %s' % names)
        b.post('me', dict(station=1))
        err = until('station 1 refused', lambda: b.state().get('error'))
        check(err['code'] == 'station', 'B may not take station 1 (A\'s): %s' % err['msg'])
        b.post('me', dict(station=2, ready=True))
        st = until('B ready on station 2', lambda: (lambda s: s if any(p['name'] == 'Ben' and p['station'] == 2 and p['ready'] for p in s['state']['players']) else None)(a.state()))
        check(True, 'B on station 2 and ready')
        if load:
            r = a.post('save', dict(game=save_game, file=save_file))
            check(not r.get('http_error') and r['save']['status'] == 'ok', 'A chose the savegame: %s' % (r.get('error') or r.get('save')))
            h = r['save']['hash']
            sb = until('B fetched the savegame', lambda: (lambda s: s if s['save'].get('status') in ('ok', 'failed') else None)(b.state()), timeout=60)
            got = os.path.join(b.saves, save_game, save_file)
            check(sb['save']['status'] == 'ok' and sb['save']['hash'] == h and os.path.exists(got)
                  and open(got, 'rb').read() == open(os.path.join(a.saves, save_game, save_file), 'rb').read(),
                  'B fetched it through the server: the same bytes in its own savegames (%s)' % sb['save'])
            b.post('me', dict(station=3))
            until('B on station 3', lambda: any(p['name'] == 'Ben' and p['station'] == 3 for p in a.state()['state']['players']))
            probs = until('the stations refused', lambda: [p['code'] for p in a.state()['state']['problems'] if p['code'].startswith('save')])
            check(sorted(probs) == ['savefree', 'savestation'], 'B on station 3 (the computer\'s in the savegame), station 2 free: %s' % probs)
            b.post('me', dict(station=2, ready=True))
            until('nothing in the way', lambda: not a.state()['state']['problems'])
            a.post('start')
            ga = until('A\'s start', lambda: a.state().get('argv'))
            gb = until('B\'s start', lambda: b.state().get('argv'))
            check('--mp-load' in ga and '--mp-load' in gb and '--new-game' not in ga + gb
                  and ga[ga.index('--mp-save') + 1].endswith(save_game) and gb[gb.index('--mp-save') + 1].endswith(save_game),
                  'both %s it and go on saving in the game\'s directory: %s' % ('load' if real else 'would load', ' '.join(gb)))
            check('--ai-improve=off' in ga and '--ai-improve=off' in gb, 'both with the savegame\'s AI improvement (off: saved without it), not the host\'s (strong)')
            check('--ad-offers=10' in ga and '--ad-offers=10' in gb, 'both with the savegame\'s ad offers (10: saved without the option), not the host\'s (15)')
            check('--film-offers=10' in ga and '--film-offers=10' in gb, 'both with the savegame\'s film offers (10), not the host\'s (12)')
            check('--knock=10' in ga and '--knock=10' in gb, 'both with the host\'s knocking (10 game minutes: not a rule of the savegame)')
            check('--cast-fit=yes' in ga and '--cast-fit=yes' in gb, 'both with the host\'s role fit in the shop (not a rule of the savegame)')
            check('--series-topicality=no' in ga and '--series-topicality=no' in gb, 'both with the host\'s series card (without the topicality: not a rule of the savegame)')
            if real:                         # the launchers started them: both meet in the server and play on
                pa = until('A\'s game', lambda: (a.state().get('go') or {}).get('pid'))
                pb = until('B\'s game', lambda: (b.state().get('go') or {}).get('pid'))
                started = until('both games in the server', lambda: a.state()['state']['phase'] == 'playing', timeout=90)
                time.sleep(5)
                alive = []
                for pid in (pa, pb):
                    try:
                        os.kill(pid, 0)
                        alive.append(pid)
                    except OSError:
                        pass
                check(started and len(alive) == 2, 'both launchers started their games from the savegame (pids %d, %d): they play on' % (pa, pb))
                for pid in (pa, pb):
                    try:
                        os.kill(pid, 15)
                    except OSError:
                        pass
                raise StopIteration
            script = os.path.join(d, 'hashes.txt')
            with open(script, 'w') as f:
                f.write('300 peek16 39D4:71E4\n1500 seat 1 peek32 39D4:7ADE\n%d hash\n%d seat 1 hash\n%d quit\n' % (frames, frames + 1, frames + 10))
            outs = []
            for argv in (ga, gb):
                games.append(subprocess.Popen(argv + ['--headless', '--nosound', '--script', script], cwd=ROOT,
                                              stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True))
            for p in games:
                o, e = p.communicate(timeout=600)
                outs.append(([l for l in o.split('\n') if l.startswith(('HASH', 'STATE', 'PEEK'))], e, p.returncode))
            check(all(c == 0 for _, _, c in outs) and outs[0][0] == outs[1][0] and 'PEEK frame 300 39D4:71E4 2' in outs[0][0]
                  and 'PEEK seat 1 frame 1500 39D4:7ADE 250000' in outs[0][0],
                  'both games loaded day 2 (seat 1\'s credit $250 000 back) and computed the same game: %s' % outs[0][0][:3])
            if outs[0][0] != outs[1][0] or any(c for _, _, c in outs):
                for o, e, c in outs:
                    print('   exit %s: %s' % (c, ' | '.join([l for l in e.split('\n') if l][-4:])))
            raise StopIteration
        r = a.post('game', dict(settings=settings, packs=pack))
        check(not r.get('http_error'), 'A chose the pack %s (%s)' % (pack, r.get('error') or r.get('mods')))
        st = until('B built the pack', lambda: (lambda s: s if s['mods']['status'] in ('ok', 'differ', 'failed') else None)(b.state()), timeout=180)
        ah = a.state()['mods']['hash']
        check(st['mods']['status'] == 'ok' and st['mods']['hash'] == ah, 'B built the pack: %s, the same files as A (%s)' % (st['mods']['status'], ah))
        sa = until('B not ready after the change', lambda: (lambda s: s if any(p['name'] == 'Ben' and not p['ready'] for p in s['state']['players']) else None)(a.state()))
        check(True, 'A\'s change of the game made B not ready')
        r = a.post('start')
        err = until('the start refused', lambda: a.state().get('error'))
        check(err['code'] == 'not_yet', 'A may not start yet: %s' % err['msg'])
        b.post('me', dict(ready=True))
        until('nothing in the way', lambda: not a.state()['state']['problems'])
        a.post('start')
        if real:
            pa = until('A\'s game', lambda: (a.state().get('go') or {}).get('pid'))
            pb = until('B\'s game', lambda: (b.state().get('go') or {}).get('pid'))
            if internet and not server:          # (a server elsewhere: its log is there; the lobby says it)
                started = until('both games in the server', lambda: a.state()['state']['phase'] == 'playing', timeout=90)
            else:
                started = until('both games in the server', lambda: 'the game starts' in open(srvlog).read(), timeout=90)
            time.sleep(5)
            alive = []
            for pid in (pa, pb):
                try:
                    os.kill(pid, 0)
                    alive.append(pid)
                except OSError:
                    pass
            check(started and len(alive) == 2, 'both launchers started their games (pids %d, %d): they met in the server and play on' % (pa, pb))
            if '--cut' in sys.argv and server:
                had = relay.connections
                relay.cut()
                back = until('both games back', lambda: open(srvlog).read().count('back on a new connection') >= 2 or None, timeout=40)
                time.sleep(5)
                alive = [pid for pid in (pa, pb) if os.path.exists('/proc/%d' % pid)]
                check(back and len(alive) == 2, 'every connection cut (%d): both games came back through their bridges and play on (%d new connections)'
                      % (had, relay.connections - had))
            for pid in (pa, pb):
                try:
                    os.kill(pid, 15)
                except OSError:
                    pass
            raise StopIteration
        ga = until('A\'s start', lambda: a.state().get('argv'))
        gb = until('B\'s start', lambda: b.state().get('argv'))
        check('--new-game' in ga and '--seat' in ga and '/build/mp/' in ga[ga.index('--game') + 1].replace(os.sep, '/')
              and (not internet or ga[ga.index('--mp') + 1].startswith('127.0.0.1:')),
              'A would start (the pack built in build/mp/%s): %s' % ('; through its bridge' if internet else '', ' '.join(ga)))
        check(gb[gb.index('--seat') + 1] == '1' and gb[gb.index('--names') + 1] == 'Anna,Ben,'
              and ga[ga.index('--seed') + 1] == gb[gb.index('--seed') + 1], 'B would start as seat 1, names Anna,Ben, the same seed')
        check('--ai-improve=strong' in ga and '--ai-improve=strong' in gb, 'both with the host\'s AI improvement (strong)')
        check('--ad-offers=15' in ga and '--ad-offers=15' in gb, 'both with the host\'s ad offers (15; the public server knows them once redeployed)')
        check('--film-offers=12' in ga and '--film-offers=12' in gb, 'both with the host\'s film offers (12; the public server knows them once redeployed)')
        check('--knock=10' in ga and '--knock=10' in gb, 'both with the host\'s knocking (10 game minutes; the public server knows it once redeployed)')
        check('--cast-fit=yes' in ga and '--cast-fit=yes' in gb, 'both with the host\'s role fit in the shop, over their own setting (the public server knows it once redeployed)')
        check('--series-topicality=no' in ga and '--series-topicality=no' in gb, 'both with the host\'s series card (without the topicality; the public server knows it once redeployed)')
        # the two games, without windows
        script = os.path.join(d, 'hashes.txt')
        with open(script, 'w') as f:
            f.write('1000 hash\n1001 seat 1 hash\n%d hash\n%d seat 1 hash\n%d quit\n' % (frames, frames + 1, frames + 10))
        outs = []
        for argv in (ga, gb):
            games.append(subprocess.Popen(argv + ['--headless', '--nosound', '--script', script], cwd=ROOT,
                                          stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True))
        for p in games:
            o, e = p.communicate(timeout=600)
            outs.append(([l for l in o.split('\n') if l.startswith(('HASH', 'STATE'))], e, p.returncode))
        check(all(c == 0 for _, _, c in outs) and outs[0][0] == outs[1][0] and len(outs[0][0]) == 6,
              'both games played to frame %d and computed the same game (%d lines)' % (frames, len(outs[0][0])))
        if outs[0][0] != outs[1][0] or any(c for _, _, c in outs):
            for o, e, c in outs:
                print('   exit %s: %s' % (c, ' | '.join([l for l in e.split('\n') if l][-4:])))
        joined = [l for l in outs[1][1].split('\n') if 'seat 1 joins' in l]
        check(bool(joined), 'seat 1 joined in the first morning: %s' % (joined[0].strip() if joined else '-'))
    except StopIteration:
        pass
    except AssertionError as e:
        check(False, str(e))
    finally:
        for p in games:
            if p.poll() is None:
                p.kill()
        a.stop()
        b.stop()
        if server:
            server.terminate()
            server.wait(timeout=10)
        shutil.rmtree(d, ignore_errors=True)
    print('%d failed' % len(fails) if fails else 'all ok')
    sys.exit(1 if fails else 0)


if __name__ == '__main__':
    main()
