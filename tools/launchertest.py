#!/usr/bin/env python3
"""The launcher page's server in Go (build/openmtv launcher, launcher/server.go) and the
content editor in it (launcher/editor.go): the same answers and the same files written
as when tests/golden/launchertest.json was recorded (tools/golden.py: their SHA-256s --
many answers hold the original's tables, texts and pictures; recorded 2026-10-19 after
the Python editor it replaced, the former Python editor (editor.py) until then, had answered the
same), and the pages in the browser.

    python3 tools/launchertest.py [--exe PATH] [--no-chrome] [--no-editor-selftest] [-v] [--record]

It runs in a copy of a small decomp directory under a temporary directory (the Go
program, tools/editor/ -- the pages, no Python; mods/ with the test's content packs,
tests/packs, and some packs of its own; runtime/settings.c, build/mtv-dec of this
checkout), on a free port, with a settings directory of its own and the original game
files in a copy ($GAME, default: one directory above decomp/).

  1. the API in English and German: the packs (/api/mods), their report for single
     packs, combinations, conflicts, incomplete and wrong ones (/api/packs/A+B), the
     settings file read and changed, values refused (/api/settings?test=1, then the
     file itself), the start dry (/api/launch), a build (/api/mods/A+B/build: the
     answer and the files) and the builds refused; the pictures (/api/sprite,
     /api/room, /api/picture: their pixels);
  2. the server itself: a foreign Host and a change without the page's header
     refused, no file outside the pages, a second start shows the first, the end
     without its page (--idle) and by "quit", and a real start: the game runs (SDL
     without a display), its messages in build/game.log;
  3. the editor in English and German: the font, the page's data (/api/meta?editor=1,
     /api/strings), the original's tables and a pack's (/api/original,
     /api/mods/NAME/TABLE), selections (/api/selection/A+B) -- the values and the
     order of every key; packs made (empty, a copy, complete from several), a table
     saved (the page's entries, its order kept), renamed, deleted, and what is refused
     -- the answers and every file written; build/openmtv editor shows the launcher
     running at the editor;
  4. headless Chrome (/opt/google/chrome/chrome, or $CHROME): the page's own
     self-test (?selftest, the player's launcher: selftest.js playerSelftest; at its
     end "Editor" makes the page the editor) in English and German, and the editor's
     full self-test (#editor), in both languages.

Standard library only (Python 3.8+)."""
import hashlib
import json
import os
import shutil
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..'))
sys.path.insert(0, HERE)
import content, datafixes, golden  # noqa: E402

WIN = os.name == 'nt'
EXE = '.exe' if WIN else ''
GAME_FILES = ('SOURCE1.RSC', 'SOURCE2.RSC', 'MTV.EXE')
SHIPPED = ('test-filme', 'test-serien', 'test-werbung')
CHROME = os.environ.get('CHROME', '/opt/google/chrome/chrome')
RESULTS = []


def check(name, good, detail=''):
    RESULTS.append((name, good))
    print('%-58s %s%s' % (name, 'ok' if good else 'DIFFERENT', '' if good or not detail else '\n' + detail))


def original_dir():
    return os.path.abspath(os.path.join(ROOT, os.environ.get('GAME', '..')))


def free_port():
    s = socket.socket()
    s.bind(('127.0.0.1', 0))
    p = s.getsockname()[1]
    s.close()
    return p


# ---------------------------------------------------------------- the test's own packs

def test_packs():
    res = content.Resources([os.path.join(original_dir(), f) for f in ('SOURCE1.RSC', 'SOURCE2.RSC')])
    o = json.loads(json.dumps(datafixes.apply(content.export(res))[0], ensure_ascii=False))
    films = o['films']

    def film(i):
        return json.loads(json.dumps(next(r for r in films['records'] if r['id'] == i)))

    def retitle(title):
        d = json.loads(json.dumps(films))
        next(r for r in d['records'] if r['id'] == 31)['text']['en'][0] = title
        return d
    a, b = retitle('Title of A'), retitle('Title of B')
    for d, base in ((a, 1), (b, 40)):
        for k in range(2):
            r = film(base + k)
            r['id'] = 201 + k
            d['records'].append(r)
    half = json.loads(json.dumps(films))
    r = film(5)
    r['id'], r['genre'], r['flags'] = 201, 0, r.get('flags', 0) | 0x20
    r['text']['de'][0] = ''
    half['records'].append(r)
    with open(os.path.join(ROOT, 'tests', 'packs', 'test-filme', 'films.json'), encoding='utf-8') as f:
        nf = json.load(f)
    own = {r['id'] for r in nf['records']}      # (the shipped pack keeps only its own films)
    nf['records'] = sorted([r for r in films['records'] if r['id'] not in own] + nf['records'], key=lambda r: r['id'])
    voll = {t: o[t] for t in content.TABLES}
    voll['films'] = nf
    packs = {
        'konflikt-a': {'mod.json': {'title': 'Konflikt A'}, 'films.json': a},
        'konflikt-b': {'mod.json': {'title': 'Konflikt B', 'description': 'B'}, 'films.json': b},
        'halb': {'mod.json': {'title': 'Halb'}, 'films.json': half},
        'voll': dict({'mod.json': {'title': 'Voll', 'complete': True, 'from': ['test-filme']}},
                     **{t + '.json': d for t, d in voll.items()}),
        'selftest': {'mod.json': {'title': 'self-test'}},
        # packs that cannot be read: in the list with why, the others stay playable
        'kaputt': {'mod.json': {'title': 'Kaputt'}, 'films.json': '{"table": "films",\n "records": [}'},
        'kaputt-meta': {'mod.json': '{"title": "Ü" "x": 1}'},     # (a missing comma: every Python version puts it at the same place)
        'keine-tabelle': {'mod.json': {'title': 'Keine Tabelle'}, 'ads.json': '[1, 2]'},
    }
    return packs


def write_packs(d, packs):
    for n, files in packs.items():
        os.makedirs(os.path.join(d, 'mods', n), exist_ok=True)
        for f, v in files.items():
            with open(os.path.join(d, 'mods', n, f), 'w', encoding='utf-8') as fh:
                fh.write(v if isinstance(v, str) else json.dumps(v, ensure_ascii=False, indent=1))


def make_root(d, packs, go_exe):
    os.makedirs(os.path.join(d, 'build'))
    os.makedirs(os.path.join(d, 'runtime'))
    shutil.copy(os.path.join(ROOT, 'runtime', 'settings.c'), os.path.join(d, 'runtime'))
    for n in SHIPPED:
        shutil.copytree(os.path.join(ROOT, 'tests', 'packs', n), os.path.join(d, 'mods', n))
    write_packs(d, packs)
    shutil.copytree(os.path.join(HERE, 'editor'), os.path.join(d, 'tools', 'editor'))
    shutil.copy(go_exe, os.path.join(d, 'build', 'openmtv' + EXE))
    libs = ('SDL2.dll', 'opl3.dll') if WIN else ('libopl3.so', 'libopl3.dylib')   # (opl3: OPL3_SHARED=1 builds)
    for f in ('mtv-dec' + EXE,) + tuple(x for x in libs if x == 'SDL2.dll' or os.path.exists(os.path.join(ROOT, 'build', x))):
        src = os.path.join(ROOT, 'build', f)
        if WIN:
            shutil.copy(src, os.path.join(d, 'build', f))
        else:
            os.symlink(src, os.path.join(d, 'build', f))


# ---------------------------------------------------------------- HTTP

def call(port, method, path, body=None, lang='en', headers=None, raw=False):
    h = {'X-UI-Lang': lang} if lang else {}
    h.update(headers or {})
    data = None
    if body is not None:
        data = json.dumps(body).encode()
        h['Content-Type'] = 'application/json'
    req = urllib.request.Request('http://127.0.0.1:%d%s' % (port, path), data=data, method=method, headers=h)
    try:
        with urllib.request.urlopen(req, timeout=60) as r:
            code, b = r.status, r.read()
    except urllib.error.HTTPError as e:
        code, b = e.code, e.read()
    if raw:
        return code, b
    try:
        return code, json.loads(b.decode('utf-8'))
    except ValueError:
        return code, b.decode('utf-8', 'replace')


def wait_up(port, proc, seconds=60):
    end = time.time() + seconds
    while time.time() < end:
        if proc.poll() is not None:
            return False
        try:
            call(port, 'GET', '/api/mods')
            return True
        except OSError:
            pass
        time.sleep(0.2)
    return False


def norm(v, roots):
    """an answer with the copies' paths made ROOT"""
    s = json.dumps(v, ensure_ascii=False, sort_keys=True)
    for r in roots:
        s = s.replace(json.dumps(r)[1:-1], 'ROOT').replace(r.replace('\\', '/'), 'ROOT')
    return json.loads(s)


# ---------------------------------------------------------------- PNG (the pixels)

def png_pixels(data):
    """-> (width, height, RGBA bytes) of an 8-bit RGB or RGBA PNG"""
    assert data[:8] == b'\x89PNG\r\n\x1a\n'
    pos, idat, w = 8, b'', 0
    while pos < len(data):
        n, t = struct.unpack('>I4s', data[pos:pos + 8])
        c = data[pos + 8:pos + 8 + n]
        if t == b'IHDR':
            w, h, depth, ctype = struct.unpack('>IIBB', c[:10])
            assert depth == 8 and ctype in (2, 6), (depth, ctype)
        elif t == b'IDAT':
            idat += c
        pos += 12 + n
    bpp = 4 if ctype == 6 else 3
    raw, out, prev = zlib.decompress(idat), bytearray(), bytearray(w * bpp)
    stride = w * bpp
    for y in range(h):
        f, line = raw[y * (stride + 1)], bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for x in range(stride):
            a = line[x - bpp] if x >= bpp else 0
            b, c = prev[x], prev[x - bpp] if x >= bpp else 0
            if f == 1:
                line[x] = (line[x] + a) & 255
            elif f == 2:
                line[x] = (line[x] + b) & 255
            elif f == 3:
                line[x] = (line[x] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        for x in range(w):
            px = line[x * bpp:x * bpp + bpp]
            out += px if bpp == 4 else px + b'\xff'
        prev = line
    return w, h, bytes(out)


def files_of(d):
    out = {}
    for f in sorted(os.listdir(d)) if os.path.isdir(d) else []:
        with open(os.path.join(d, f), 'rb') as fh:
            out[f] = hashlib.sha1(fh.read()).hexdigest()
    return out


# ---------------------------------------------------------------- the checks

def api_checks(go, roots, verbose, gold):
    def same(name, method, path, body=None, langs=('en', 'de'), transform=None):
        for lang in langs:
            b = call(go, method, path, body, lang)
            if transform:
                b = transform(b)
            b = norm(b, roots)
            ok, detail = gold.check('%s (%s)' % (name, lang), b, hashed=True)
            check('%s (%s)' % (name, lang), ok, detail)
            if verbose and ok:
                print('    ' + json.dumps(b, ensure_ascii=False)[:300])

    same('the packs: /api/mods', 'GET', '/api/mods')
    for spec in ('test-filme', 'test-filme+test-serien', 'test-werbung+test-filme+test-serien', 'konflikt-a+konflikt-b',
                 'konflikt-b+test-filme+konflikt-a', 'halb', 'test-filme+halb', 'voll', 'voll+test-serien', 'voll+test-filme',
                 'gibtsnicht', 'test-filme+test-filme', 'bad%20name!'):
        same('the report: /api/packs/' + spec, 'GET', '/api/packs/' + spec)
    b = call(go, 'GET', '/api/packs/kaputt')[1]
    check('the report of a damaged table: an error', 'error' in b, '    %s' % (b,))
    errs = {m['name']: m.get('error') for m in call(go, 'GET', '/api/mods', lang='de')[1]}
    check('packs that cannot be read: in the list with why (%d)' % sum(1 for e in errs.values() if e),
          [n for n, e in errs.items() if e] == ['kaputt', 'kaputt-meta', 'keine-tabelle'] and errs['test-filme'] is None, '    %s' % errs)
    # the settings: from the defaults, changed, refused
    for d in roots:
        for f in ('selftest-settings.ini', 'selftest-settings.ini.new'):
            if os.path.exists(os.path.join(d, 'build', f)):
                os.remove(os.path.join(d, 'build', f))
    S = '/api/settings?test=1'
    same('the settings file, none yet', 'GET', S, langs=('en',))
    for i, (values, lang) in enumerate([({'display': {'filter': 'mmpx'}}, 'en'),
                                        ({'display': {'scale': '4', 'fullscreen': 'true'}, 'game': {'fixes': 'random0, intruder'}}, 'en'),
                                        ({'game': {'start_money': 'abc'}}, 'de'), ({'game': {'start_money': 'abc'}}, 'en'),
                                        ({'game': {'credit_line': '750000'}}, 'de'), ({'game': {'fixes': 'random0,gibtsnicht'}}, 'de'),
                                        ({'game': {'fixes': ' , '}}, 'en'), ({'foo': {'bar': '1'}}, 'de'), ({'game': {'foo': '1'}}, 'en'),
                                        ({'sound': {'sound': 'yes'}, 'input': {'mouse': 'system'}}, 'en'),
                                        ({'game': {'language': 'german'}, 'display': {'scale': 9}}, 'de'),
                                        ({'game': {'classic': 'yes', 'ai_improve': 'strong', 'terror_damage': 0}}, 'en'),
                                        ({'game': {'ad_offers': '18'}}, 'de'), ({'game': {'ad_offers': '14'}}, 'en'),
                                        ({'game': {'film_offers': '9'}}, 'en'), ({'game': {'film_offers': '13'}}, 'de')]):
        same('the settings changed, %d: %s' % (i + 1, json.dumps(values)[:40]), 'PUT', S, {'values': values}, langs=(lang,))
    with open(os.path.join(roots[0], 'build', 'selftest-settings.ini'), encoding='utf-8') as f:
        ok, detail = gold.check('the settings file written', f.read(), hashed=True)
    check('the settings file written', ok, detail)
    # the start, dry; a build; builds refused
    for body in ({'dry': True}, {'mod': 'test-filme', 'dry': True, 'lang': 'd', 'classic': True},
                 {'mod': 'test-serien+test-werbung', 'dry': True, 'lang': 'x'}, {'mod': 'gibtsnicht', 'dry': True}):
        same('the start, dry: ' + json.dumps(body), 'POST', '/api/launch', body, langs=('en',))
    same('build: test-filme+test-serien', 'POST', '/api/mods/test-filme+test-serien/build', {}, langs=('en',))
    built = files_of(os.path.join(roots[0], 'build', 'mods', 'test-filme+test-serien'))
    ok, detail = gold.check('build: the files', built)
    check('build: the files (%s)' % ', '.join(built), ok and len(built) == 2, detail)
    for spec in ('halb', 'test-filme+halb', 'gibtsnicht', 'voll+test-filme', 'konflikt-a+kaputt', 'keine-tabelle'):
        same('build refused: ' + spec, 'POST', '/api/mods/%s/build' % spec, {},
             transform=(lambda r: (r[0], 'error' in r[1])) if 'kaputt' in spec or 'tabelle' in spec else None)
    same('the start refused: an unknown pack', 'POST', '/api/launch', {'mod': 'gibtsnicht'})
    same('the start refused: a pack that cannot be read', 'POST', '/api/launch', {'mod': 'test-filme+kaputt'},
         transform=lambda r: (r[0], 'error' in r[1]))
    # the pictures
    for path in ('/api/sprite/16/0.png', '/api/room/104.png', '/api/room/112.png', '/api/picture/220.png', '/api/sprite/3/65.png'):
        b = call(go, 'GET', path, raw=True)
        ok, detail = gold.check('the picture ' + path, png_pixels(b[1])[2] if b[0] == 200 else b[0], hashed=True)
        check('the picture %s: its pixels' % path, b[0] == 200 and ok, detail)
    # what another site may ask without the page's header: the pictures, nothing else
    for path in ('/api/mp/rooms?server=' + urllib.parse.quote('http://127.0.0.1:9/', safe=''), '/api/mp/find?port=9',
                 '/api/settings?test=1&mp=1', '/api/mods'):
        b = call(go, 'GET', path, lang=None)[0]
        check('without the page\'s header refused: GET ' + path.split('?')[0], b == 403, '    %s' % b)
    b = call(go, 'GET', '/api/sprite/16/0.png', lang=None, raw=True)[0]
    check('a picture without the page\'s header', b == 200, '    %s' % b)


def ordered(b):
    """a JSON answer with the order of its keys (lists of pairs)"""
    return json.loads(b.decode('utf-8'), object_pairs_hook=lambda pairs: pairs)


def tree(d):
    """every file below D: relative path -> its bytes"""
    out = {}
    for dp, _, fns in os.walk(d):
        for fn in fns:
            p = os.path.join(dp, fn)
            with open(p, 'rb') as f:
                out[os.path.relpath(p, d).replace(os.sep, '/')] = f.read()
    return out


def editor_checks(ge, roots, verbose, gold):
    def same(name, method, path, body=None, langs=('en', 'de'), transform=None, order=False, want=None):
        for lang in langs:
            b = call(ge, method, path, body, lang, raw=order)
            if want and b[0] != want:
                check('editor: %s (%s): %d' % (name, lang, want), False, '    %s %s' % (b[0], str(b[1])[:300]))
                continue
            if order:
                b = (b[0], ordered(b[1]))
            if transform:
                b = transform(b)
            b = norm(b, roots)
            ok, detail = gold.check('editor: %s (%s)' % (name, lang), b, hashed=True)
            check('editor: %s (%s)' % (name, lang), ok, detail)
            if verbose and ok:
                print('    ' + json.dumps(b, ensure_ascii=False)[:300])

    b = call(ge, 'GET', '/font.ttf', raw=True)
    ok, detail = gold.check('editor: the game\'s font', b[1], hashed=True)
    check('editor: the game\'s font, /font.ttf (%d bytes): byte for byte' % len(b[1]), b[0] == 200 and ok, detail)
    same('the page\'s data, /api/meta?editor=1', 'GET', '/api/meta?editor=1', langs=('en',))
    same('the game\'s strings, /api/strings', 'GET', '/api/strings', langs=('en',))
    for t in content.TABLES:
        same('the original\'s %s, in its order' % t, 'GET', '/api/original/' + t, langs=('en',), order=True)
    for name, t in (('test-filme', 'films'), ('konflikt-a', 'films'), ('voll', 'series'), ('test-filme', 'ads'),
                    ('gibtsnicht', 'films'), ('bad%20name!', 'films'), ('test-filme', 'nothing')):
        same('a pack\'s table, /api/mods/%s/%s' % (name, t), 'GET', '/api/mods/%s/%s' % (name, t), langs=('en',), order=True,
             transform=lambda r: r if r[0] == 200 else (r[0], dict(r[1]) if isinstance(r[1], list) else r[1]))
    same('a pack that cannot be read, /api/mods/kaputt/films', 'GET', '/api/mods/kaputt/films', langs=('en',),
         transform=lambda r: (r[0], 'error' in r[1]))
    for spec in ('original', 'test-filme', 'test-filme+test-serien+test-werbung', 'konflikt-b+test-filme+konflikt-a', 'voll+test-serien'):
        same('the selection %s, in its order' % spec, 'GET', '/api/selection/' + spec, langs=('en',), order=True,
             transform=lambda r: (r[0], sorted(dict(r[1])['tables'])))     # (each table in its order; their own order is free)
        same('the selection %s: its report' % spec, 'GET', '/api/selection/' + spec, langs=('en',),
             transform=lambda r: (r[0], r[1]['report']))
    for spec in ('gibtsnicht', 'test-filme+test-filme', 'voll+test-filme'):
        same('a selection refused: ' + spec, 'GET', '/api/selection/' + spec)
    # packs made, changed, renamed, deleted: the same answers, the same files
    for body in ({'name': 'neu', 'title': 'Neu'}, {'name': 'kopie', 'from': 'test-filme'},
                 {'name': 'alles', 'title': 'Alles', 'description': 'Gemacht aus: Test-Filme + Test-Serien + Test-Werbung',
                  'from': 'test-filme+test-serien+test-werbung', 'complete': True},
                 {'name': 'konflikte', 'from': 'konflikt-b+konflikt-a', 'complete': True},
                 {'name': 'orig', 'complete': True}):
        same('a pack made: %s' % json.dumps(body, ensure_ascii=False)[:60], 'POST', '/api/mods', body, langs=('en',), want=200)
    for body, lang in (({'name': 'original'}, 'de'), ({'name': 'neu'}, 'de'), ({'name': 'a b'}, 'de'), ({'name': 'x', 'from': 'gibtsnicht', 'complete': True}, 'en'),
                       ({'name': 'y', 'from': 'test-filme+test-filme', 'complete': True}, 'de'), ({'name': 'z', 'from': 'b a d'}, 'en')):
        same('a pack refused: %s' % json.dumps(body), 'POST', '/api/mods', body, langs=(lang,))
    recs = call(ge, 'GET', '/api/mods/test-filme/films')[1]['records']     # (the whole table, as the page has it)
    recs[0]['text']['de'][0] = 'Jäger des verlorenen "Schatzes" \\ <&> ½'
    new = {'text': {'en': ['New film', 'A film the page made.'], 'de': ['Neuer Film', 'Ein Film der Seite.']}, 'id': 326}
    new.update({k: v for k, v in recs[1].items() if k not in ('id', 'text')})
    recs.append(new)
    same('a table saved: kopie/films (an entry changed, one added in the page\'s order)', 'PUT', '/api/mods/kopie/films',
         {'records': recs}, langs=('en',), want=200)
    same('a table saved: neu/ads (the original\'s)', 'PUT', '/api/mods/neu/ads',
         {'records': call(ge, 'GET', '/api/original/ads')[1]['records']}, langs=('en',), want=200)
    same('a table refused: a protected pack', 'PUT', '/api/mods/test-filme/films', {'records': recs})
    same('a protected pack unlocked on purpose', 'PUT', '/api/mods/test-filme/films', {'records': recs, 'unlock': True}, langs=('en',), want=200)
    same('a table refused: no pack', 'PUT', '/api/mods/gibtsnicht/films', {'records': recs})
    long = json.loads(json.dumps(recs))
    long[0]['text']['de'][1] = 'x' * 300
    same('a table refused: a text over 255 bytes', 'PUT', '/api/mods/kopie/films', {'records': long}, transform=lambda r: (r[0], 'error' in r[1]))
    bad = json.loads(json.dumps(recs))
    bad[0]['text']['de'][1] = 'Ⓧ'
    same('a table refused: a character not in code page 437', 'PUT', '/api/mods/kopie/films', {'records': bad},
         transform=lambda r: (r[0], 'error' in r[1]))      # (Go names the entry and its field)
    for name, body in (('kopie', {'title': 'Kopie 2', 'description': 'Geändert'}), ('neu', {}), ('alles', {'title': '', 'description': None}),
                       ('gibtsnicht', {'title': 'x'})):
        same('renamed: %s %s' % (name, json.dumps(body, ensure_ascii=False)), 'POST', '/api/mods/%s/info' % name, body, langs=('en',),
             want=None if name == 'gibtsnicht' else 200)
    same('the packs after all that, /api/mods', 'GET', '/api/mods')
    for name in ('test-filme', 'gibtsnicht'):
        same('deleting refused: ' + name, 'DELETE', '/api/mods/' + name)
    same('deleted: neu', 'DELETE', '/api/mods/neu', langs=('en',), want=200)
    b = tree(os.path.join(roots[0], 'mods'))
    if WIN:                             # (this test writes its own packs' files with CRLF there; Go the same bytes everywhere)
        b = {k: v.replace(b'\r\n', b'\n') for k, v in b.items()}
    ok, detail = gold.check('editor: every file of mods/', {k: hashlib.sha1(v).hexdigest() for k, v in b.items()})
    check('editor: every file of mods/, byte for byte (%d files)' % len(b), ok, detail)
    for name in ('kopie', 'alles', 'konflikte', 'orig'):          # (the self-tests in the browser start without them)
        same('deleted: ' + name, 'DELETE', '/api/mods/' + name, langs=('en',), want=200)


def go_checks(go, go_root, go_exe, env, game):
    code, b = call(go, 'GET', '/api/hello', headers={'Host': 'evil.example:%d' % go})
    check('Go: another Host refused (DNS rebinding)', code == 403, '    %s %s' % (code, b))
    code, b = call(go, 'POST', '/api/launch', {'dry': True}, lang=None)
    check('Go: a change without the page\'s header refused', code == 403, '    %s %s' % (code, b))
    code, b = call(go, 'GET', '/../../runtime/settings.c')
    code2, _ = call(go, 'GET', '/%2e%2e/%2e%2e/runtime/settings.c')
    check('Go: no file outside the pages', code == 404 and code2 == 404, '    %s %s' % (code, code2))
    code, b = call(go, 'GET', '/', lang=None, raw=True)
    check('Go: the page itself', code == 200 and b'<script>start();</script>' in b)
    r = subprocess.run([go_exe, 'launcher', '--no-browser', '--port', str(go), '--game', game], env=env, capture_output=True, timeout=30)
    check('Go: a second start shows the first', r.returncode == 0 and b'already running' in r.stdout, '    ' + repr(r.stdout + r.stderr))
    port = free_port()
    t0 = time.time()
    p = subprocess.Popen([go_exe, 'launcher', '--no-browser', '--port', str(port), '--game', game, '--idle', '2'], env=env,
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    try:
        out = p.communicate(timeout=30)[0]
        check('Go: the end without its page (--idle 2: %.0f s)' % (time.time() - t0), p.returncode == 0 and b'no page' in out, '    ' + repr(out))
    except subprocess.TimeoutExpired:
        p.kill()
        check('Go: the end without its page (--idle 2)', False, '    still running after 30 s')
    port = free_port()
    p = subprocess.Popen([go_exe, 'launcher', '--no-browser', '--port', str(port), '--game', game, '--idle', '0'], env=env,
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    wait_up(port, p)
    code, b = call(port, 'POST', '/api/quit', {})
    try:
        p.wait(timeout=10)
        check('Go: "quit" ends it', code == 200 and b == {'quit': True} and p.returncode == 0)
    except subprocess.TimeoutExpired:
        p.kill()
        check('Go: "quit" ends it', False)
    r = subprocess.run([go_exe, 'editor', '--no-browser', '--port', str(go), '--game', game], env=env, capture_output=True, timeout=30)
    check('Go: build/openmtv editor shows the launcher running, at the editor', r.returncode == 0 and b'already running' in r.stdout
          and b'/#editor' in r.stdout, '    ' + repr(r.stdout + r.stderr))
    code, m = call(go, 'GET', '/api/meta')
    code2, m2 = call(go, 'GET', '/api/meta?editor=1')
    check('Go: /api/meta the player\'s, /api/meta?editor=1 the editor\'s', code == code2 == 200 and m.get('player') and 'tables' not in m
          and 'tables' in m2 and 'player' not in m2, '    %s %s / %s %s' % (code, str(m)[:100], code2, str(m2)[:100]))
    # a real start: the game runs, its messages in build/game.log
    code, b = call(go, 'POST', '/api/launch', {'lang': 'e'})
    pid = b.get('pid') if isinstance(b, dict) else None
    time.sleep(3)
    alive = False
    if pid:
        if WIN:
            alive = str(pid) in subprocess.run(['tasklist', '/FI', 'PID eq %d' % pid], capture_output=True, text=True).stdout
        else:
            try:
                os.kill(pid, 0)
                alive = True
            except OSError:
                pass
    log = ''
    try:
        with open(os.path.join(go_root, 'build', 'game.log'), encoding='utf-8', errors='replace') as f:
            log = f.read()
    except OSError:
        pass
    check('Go: a real start -- the game runs (pid %s), its messages in build/game.log' % pid,
          code == 200 and alive and '[load]' in log, '    %s %s; log: %r' % (code, b, log[:200]))
    if pid:
        try:
            if WIN:
                subprocess.run(['taskkill', '/F', '/PID', str(pid)], capture_output=True)
            else:
                os.kill(pid, signal.SIGTERM)
        except OSError:
            pass


def chrome(url, profile):
    r = subprocess.run([CHROME, '--headless', '--no-sandbox', '--disable-gpu', '--user-data-dir=' + profile,
                        '--virtual-time-budget=400000', '--dump-dom', url], capture_output=True, timeout=900)
    dom = r.stdout.decode('utf-8', 'replace')
    i = dom.find('<title>')
    return dom[i + 7:dom.find('</title>', i)] if i >= 0 else '(no page: %s)' % r.stderr.decode('utf-8', 'replace')[-300:]


def chrome_checks(go, base, editor_selftest, roots):
    import html
    for d in roots:                     # (the self-tests start from the defaults)
        for f in ('selftest-settings.ini', 'selftest-multiplayer.ini'):
            if os.path.exists(os.path.join(d, 'build', f)):
                os.remove(os.path.join(d, 'build', f))
    runs = [('the player\'s page alone (?player) on the Go server, English', go, '/?selftest&player#launcher'),
            ('the player\'s page alone (?player) on the Go server, German', go, '/?selftest&player&ui=de#launcher')]
    if editor_selftest:
        runs += [('the launcher\'s start (the editor at "Play") on the Go server, English', go, '/?selftest#launcher'),
                 ('the launcher\'s start (the editor at "Play") on the Go server, German', go, '/?selftest&ui=de#launcher')]
    for k, (name, port, path) in enumerate(runs):
        title = html.unescape(chrome('http://127.0.0.1:%d%s' % (port, path), os.path.join(base, 'chrome%d' % k)))
        n = title.count('ok ')
        entries = title.split('; ')
        k_fail = next((i for i, x in enumerate(entries) if x.startswith('FAIL') or x.startswith('SELFTEST FAIL: FAIL')), len(entries))
        shown = entries[max(0, k_fail - 3):k_fail] + [x for x in entries[k_fail:] if 'FAIL' in x[:20]]
        check('%s (%d checks)' % (name, n), title.startswith('SELFTEST OK'), '    ' + '\n    '.join(x[:300] for x in shown))


def main(argv):
    go_exe = os.path.abspath(argv[argv.index('--exe') + 1]) if '--exe' in argv else os.path.join(ROOT, 'build', 'openmtv' + EXE)
    verbose = '-v' in argv
    if not os.path.exists(go_exe):
        sys.exit('launchertest: %s is not built (make launcher)' % go_exe)
    if not os.path.exists(os.path.join(ROOT, 'build', 'mtv-dec' + EXE)):
        sys.exit('launchertest: build/mtv-dec%s is not built (make)' % EXE)
    base = os.path.realpath(tempfile.mkdtemp(prefix='launchertest-'))
    procs = []
    try:
        game = os.path.join(base, 'game')
        os.makedirs(game)
        for f in GAME_FILES:
            shutil.copy(os.path.join(original_dir(), f), game)
        packs = test_packs()
        roots = {'go': os.path.join(base, 'go')}
        make_root(roots['go'], packs, go_exe)
        cfg = os.path.join(base, 'cfg')
        os.makedirs(cfg)
        env = {k: v for k, v in os.environ.items() if k not in ('GAME', 'MADTV_CMD')}
        env.update({'APPDATA' if WIN else 'XDG_CONFIG_HOME': cfg, 'BROWSER': 'cmd /c rem' if WIN else 'true',
                    'SDL_VIDEODRIVER': 'dummy', 'SDL_AUDIODRIVER': 'dummy', 'PYTHONDONTWRITEBYTECODE': '1', 'PYTHONIOENCODING': 'utf-8'})
        go = free_port()
        go_bin = os.path.join(roots['go'], 'build', 'openmtv' + EXE)
        log = open(os.path.join(base, 'go.log'), 'w')
        procs.append(subprocess.Popen([go_bin, 'launcher', '--no-browser', '--port', str(go), '--game', game, '--idle', '0'],
                                      env=env, stdout=log, stderr=subprocess.STDOUT, cwd=roots['go']))
        if not wait_up(go, procs[0]):
            with open(os.path.join(base, 'go.log')) as f:
                print(f.read()[-1000:])
            sys.exit('launchertest: the server did not start')
        gold = golden.Golden('launchertest', '--record' in argv)
        if '--only-chrome' not in argv:
            api_checks(go, list(roots.values()), verbose, gold)
            editor_checks(go, list(roots.values()), verbose, gold)
            go_checks(go, roots['go'], go_bin, env, game)
            gold.save()
        if '--no-chrome' not in argv and os.path.exists(CHROME):
            chrome_checks(go, base, '--no-editor-selftest' not in argv, list(roots.values()))
        elif '--no-chrome' not in argv:
            print('(no Chrome at %s: the self-tests in the browser left out)' % CHROME)
    finally:
        for p in procs:
            p.terminate()
            try:
                p.wait(timeout=10)
            except subprocess.TimeoutExpired:
                p.kill()
        shutil.rmtree(base, ignore_errors=True)
    bad = [n for n, g in RESULTS if not g]
    print('launchertest: %d of %d checks failed' % (len(bad), len(RESULTS)) if bad else 'launchertest: all %d ok' % len(RESULTS))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
