#!/usr/bin/env python3
"""The packs' build in Go (build/openmtv build, launcher/packs.go): the same packs must
give the same files, byte for byte, as when tests/golden/buildtest.json was recorded
(tools/golden.py; recorded 2026-10-19 after the Python editor it replaced had built
the same files).

    python3 tools/buildtest.py [--exe PATH] [-v] [--record] [CASE ...]

Every case runs in a fresh copy of a small decomp directory under a temporary
directory: the Go program in build/, the test's content packs (tests/packs) in mods/
-- no Python there.  The cases' own packs are written into mods/ from the original's
tables (tools/content.py, tools/datafixes.py), then each run builds a spec (NAME or
A+B+C).  Compared: the exit code, stdout (the copy's path made ROOT) and every file
below the copy afterwards (build/mods/ and mods/), as SHA-1s.  The messages are
launcher/build_test.go's (launcher/testdata/); here they are only shown.

The cases: the test's content packs alone, in every order and
combination, built again over their copies; packs made here -- empty, new
films / series / ads, two packs that both add films and change the same one
(a screenplay's target follows), a complete pack, the starting pool -- and
the errors: names, a pack twice or held by a complete one, text longer than
255 bytes, characters outside code page 437, an incomplete film, too few
films for a new game, numbers out of range, lists of the wrong length, more
than 999 films, texts beyond 64 KB, a damaged table file -- both refuse them.

$GAME: the original game files (default: one directory above decomp/).
Standard library only (Python 3.8+)."""
import copy
import hashlib
import itertools
import json
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..'))
sys.path.insert(0, HERE)
import content, datafixes, golden  # noqa: E402

WIN = os.name == 'nt'
EXE = '.exe' if WIN else ''
GAME_FILES = ('SOURCE1.RSC', 'SOURCE2.RSC', 'MTV.EXE')
SHIPPED = ('test-filme', 'test-serien', 'test-werbung')
PY311 = sys.version_info[:2] == (3, 11)


def original_dir():
    return os.path.abspath(os.path.join(ROOT, os.environ.get('GAME', '..')))


ORIGINAL = None


def original():
    """the original's tables as the editor has them (with the data fixes), as JSON gives them"""
    global ORIGINAL
    if ORIGINAL is None:
        res = content.Resources([os.path.join(original_dir(), f) for f in ('SOURCE1.RSC', 'SOURCE2.RSC')])
        ORIGINAL = json.loads(json.dumps(datafixes.apply(content.export(res))[0], ensure_ascii=False))
    return copy.deepcopy(ORIGINAL)


def rec(docs, table, i):
    return next(r for r in docs[table]['records'] if r['id'] == i)


# ---------------------------------------------------------------- the cases' packs
# a pack: {file name: JSON value (or text: the file as it is)}

def pack(meta=None, **tables):
    out = {'mod.json': meta or {'title': 'test'}}
    for t, doc in tables.items():
        out[t + '.json'] = doc
    return out


def new_films(first, n, base=1, market=False):
    """films FIRST.. as copies of BASE.., off the market (or on it)"""
    o = original()
    doc = o['films']
    for k in range(n):
        r = copy.deepcopy(rec(o, 'films', base + k % 150))
        r['id'] = first + k
        r['owner'], r['flags2'] = 0, 0
        r['flags'] = (r.get('flags', 0) | 0x20) if market else (r.get('flags', 0) & ~0x20)
        doc['records'].append(r)
    return doc


def new_series(first, n, market=True):
    o = original()
    doc = o['series']
    for k in range(n):
        r = copy.deepcopy(rec(o, 'series', 1 + k % 50))
        r['id'], r['owner'], r['flags2'] = first + k, 0, 0
        r['flags'] = (r.get('flags', 0) | 0x20) if market else (r.get('flags', 0) & ~0x20)
        doc['records'].append(r)
    return doc


def new_ads(first, n, text=None):
    o = original()
    doc = o['ads']
    for k in range(n):
        r = copy.deepcopy(rec(o, 'ads', 1 + k % 50))
        r['id'] = first + k
        if text:
            r['fee'], r['holder'] = 0, 0
            r['text'] = {'de': [text % k, text % (k + 1)], 'en': [text % k, text % (k + 2)]}
        doc['records'].append(r)
    return doc


def changed(table, i, fn):
    o = original()
    fn(rec(o, table, i))
    return o[table]


def two_packs():
    """modtest.py's "packs": both add films from #201 and retitle film 31; the second's
    screenplay 1 turns into its second new film"""
    o = original()
    a = new_films(201, 3, base=1, market=True)
    rec({'films': a}, 'films', 31)['text']['en'][0] = 'Title of A'
    b = new_films(201, 2, base=40, market=True)
    rec({'films': b}, 'films', 31)['text']['en'][0] = 'Title of B'
    sp = o['screenplays']
    s1 = rec(o, 'screenplays', 1)
    s1['target'], s1['flags'] = 202, s1.get('flags', 0) & ~1
    return {'pack-a': pack(films=a), 'pack-b': pack(films=b, screenplays=sp)}


def pools():
    o = original()
    s = rec(o, 'series', 23)
    s['flags2'] = s.get('flags2', 0) | 0x10
    for i in (1, 2, 3):
        f = rec(o, 'films', i)
        f['flags2'] = f.get('flags2', 0) | 0x10
    return {'pools': pack(films=o['films'], series=o['series'])}


def no_start_films():
    o = original()
    for r in o['films']['records']:
        r['flags2'] = r.get('flags2', 0) & ~0x10
        if r.get('genre') == 6:
            r['flags'] = r.get('flags', 0) & ~0x20
    return {'leer': pack(films=o['films'])}


def complete_pack():
    """a complete pack made from test-filme: all tables, the original's and the pack's (the
    shipped pack keeps only its own films)"""
    o = original()
    with open(os.path.join(ROOT, 'tests', 'packs', 'test-filme', 'films.json'), encoding='utf-8') as f:
        films = json.load(f)
    own = {r['id'] for r in films['records']}
    films['records'] = sorted([r for r in o['films']['records'] if r['id'] not in own] + films['records'], key=lambda r: r['id'])
    out = pack({'title': 'voll', 'complete': True, 'from': ['test-filme']}, **{t: o[t] for t in content.TABLES})
    out['films.json'] = films
    return {'voll': out}


def text_overflow():
    """600 films off the market with long descriptions: the German texts pass 64 KB"""
    doc = new_films(201, 600)
    for r in doc['records']:
        if r['id'] >= 201:
            r['text']['de'][1] = ('Eine sehr lange Beschreibung %d ' % r['id']) * 6
    return {'lang': pack(films=doc)}


def incomplete_new():
    """two new films on the market, the second without a German title and a genre"""
    doc = new_films(201, 2, market=True)
    r = rec({'films': doc}, 'films', 202)
    r['text']['de'][0], r['genre'] = '  ', 0
    return {'halb': pack(films=doc)}


def twice_in_pack():
    doc = new_films(201, 1)
    doc['records'].append(dict(rec(original(), 'films', 9), id=201))
    return {'doppelt': pack(films=doc)}


def bad(table, i, fn, name='kaputt'):
    return {name: pack(**{table: changed(table, i, fn)})}


def setitem(key, value):
    def fn(r):
        r[key] = value
    return fn


def settext(lang, k, value):
    def fn(r):
        r['text'][lang][k] = value
    return fn


def longer_title():
    """the second build of 'test-filme-kopie': a longer title (its texts no longer fit in place)"""
    with open(os.path.join(ROOT, 'tests', 'packs', 'test-filme', 'films.json'), encoding='utf-8') as f:
        films = json.load(f)
    for r in films['records']:
        if r['id'] == 201:
            r['text']['de'][1] += ' Und noch ein Satz, damit der Text nicht mehr an seinen Platz passt.'
    return {'test-filme-kopie': pack(films=films)}


def shipped_copy():
    with open(os.path.join(ROOT, 'tests', 'packs', 'test-filme', 'films.json'), encoding='utf-8') as f:
        return {'test-filme-kopie': pack(films=json.load(f))}


ORDERS = ['+'.join(p) for n in (2, 3) for p in itertools.permutations(SHIPPED, n)]

# (name, packs (a function -> {name: pack}), runs, options): a run is a spec, or a
# function -> {name: pack} that changes the packs between two builds; options: msg
# (False: the message is not compared; 'py311': only on Python 3.11)
CASES = [
    ('shipped', None, list(SHIPPED), {}),
    ('shipped-orders', None, ORDERS, {}),
    ('rebuild', None, ['test-filme', 'test-filme', 'test-filme+test-werbung', 'test-filme+test-werbung'], {}),
    ('empty', lambda: {'leer': pack()}, ['leer', 'leer+test-serien'], {}),
    ('films300', lambda: {'f300': pack(films=new_films(201, 100))}, ['f300', 'test-filme+f300', 'f300+test-filme'], {}),
    ('series60', lambda: {'s60': pack(series=new_series(51, 10, market=False)), 's60m': pack(series=new_series(51, 10))},
     ['s60', 's60+test-serien', 'test-serien+s60', 's60m'], {}),
    ('ads60', lambda: {'a60': pack(ads=new_ads(51, 10))}, ['a60', 'test-werbung+a60'], {}),
    ('two-packs', two_packs, ['pack-a+pack-b', 'pack-b+pack-a', 'pack-a', 'pack-b'], {}),
    ('pools', pools, ['pools'], {}),
    ('complete', complete_pack, ['voll', 'voll+test-filme', 'test-filme+voll', 'voll+test-serien'], {}),
    ('changed-rebuild', shipped_copy, ['test-filme-kopie', longer_title, 'test-filme-kopie'], {}),
    ('names', None, ['bad name!', 'gibtsnicht', 'test-filme+test-filme', '', 'test-filme+', 'x' * 41], {}),
    ('text-255', lambda: bad('films', 5, settext('de', 1, 'x' * 256)), ['kaputt'], {}),
    ('cp437', lambda: bad('ads', 3, settext('de', 1, 'Preis: 5 €✓✓ ok, ☺')), ['kaputt'], {}),
    ('cp437-raw', lambda: bad('films', 7, settext('en', 0, 'Smile ☺')), ['kaputt'], {}),
    ('incomplete-new', incomplete_new, ['halb', 'test-filme+halb'], {}),
    ('incomplete-film', lambda: bad('films', 12, setitem('genre', 0)), ['kaputt'], {}),
    ('incomplete-ad', lambda: bad('ads', 4, setitem('spots', 0)), ['kaputt'], {}),
    ('incomplete-series', lambda: bad('series', 18, setitem('episodes', 0)), ['kaputt'], {}),
    ('no-start-films', no_start_films, ['leer'], {}),
    ('id-range', lambda: {'s1000': pack(series=new_series(1000, 1)),
                          'sp60': pack(screenplays=dict(original()['screenplays'],
                                                        records=original()['screenplays']['records'] +
                                                        [dict(rec(original(), 'screenplays', 5), id=60)]))},
     ['s1000', 'sp60', 'sp60+test-filme'], {}),
    ('over-999', lambda: {'f999': pack(films=new_films(999, 1)), 'f201': pack(films=new_films(201, 1))},
     ['f999', 'f999+f201', 'f201+f999'], {}),
    ('twice-in-pack', twice_in_pack, ['doppelt', 'test-filme+doppelt'], {}),
    ('value-range', lambda: bad('films', 5, setitem('quality', 300)), ['kaputt'], {'msg': 'py311'}),
    ('value-type', lambda: bad('films', 5, setitem('price', 1.5)), ['kaputt'], {}),
    ('list-length', lambda: bad('series', 3, lambda r: r.__setitem__('episodes_qtf', r['episodes_qtf'][:32])), ['kaputt'], {}),
    ('phrases', lambda: bad('news_topics', 3, lambda r: r['text'].__setitem__('de', r['text']['de'][:9])), ['kaputt'], {}),
    ('texts-64k', text_overflow, ['lang'], {'msg': 'py311'}),
    ('ads-64k', lambda: {'viele': pack(ads=new_ads(51, 940, text='Werbekunde mit langem Namen Nummer %05d'))},
     ['viele'], {}),
    ('raw-text', lambda: bad('films', 8, lambda r: r['text'].__setitem__('de', {'raw': 'zz'})), ['kaputt'], {}),
    ('json-syntax', lambda: {'kaputt': {'mod.json': {}, 'films.json': '{"table": "films", "records": [}'}},
     ['kaputt'], {'msg': False}),
    ('missing-game-file', None, ['test-filme'], {'game': 'no-source2'}),
]


def write_packs(d, packs):
    for name, files in packs.items():
        p = os.path.join(d, 'mods', name)
        os.makedirs(p, exist_ok=True)
        for f, v in files.items():
            with open(os.path.join(p, f), 'w', encoding='utf-8', newline='\n') as fh:
                fh.write(v if isinstance(v, str) else json.dumps(v, ensure_ascii=False, indent=1))


def make_root(d, go_exe):
    os.makedirs(os.path.join(d, 'build'))
    shutil.copytree(os.path.join(ROOT, 'tests', 'packs'), os.path.join(d, 'mods'))      # (the test's content packs)
    shutil.copy(go_exe, os.path.join(d, 'build', 'openmtv' + EXE))


def make_game(base, kind):
    d = os.path.join(base, 'game-' + kind)
    if not os.path.isdir(d):
        os.makedirs(d)
        for f in GAME_FILES:
            if (kind, f) != ('no-source2', 'SOURCE2.RSC'):
                shutil.copy(os.path.join(original_dir(), f), d)
    return d


def tree(d):
    out = {}
    for top, dirs, files in os.walk(d):
        rel = os.path.relpath(top, d)
        for x in files:
            r = os.path.normpath(os.path.join(rel, x)).replace(os.sep, '/')
            if r == 'build/openmtv' + EXE:
                continue
            with open(os.path.join(top, x), 'rb') as f:
                out[r] = hashlib.sha1(f.read()).hexdigest()
    return out


PREFIXES = ('openmtv build: ', 'editor: ')


def message(err):
    lines = err.replace('\r\n', '\n').splitlines()
    return [next((l[len(p):] for p in PREFIXES if l.startswith(p)), l) for l in lines]


def run_case(base, name, make, runs, opt, go_exe, verbose, gold):
    case = os.path.join(base, name)
    d = os.path.join(case, 'go')
    make_root(d, go_exe)
    if make:
        write_packs(d, make())
    game = make_game(base, opt.get('game', 'full'))
    env = dict(os.environ, PYTHONIOENCODING='utf-8', PYTHONDONTWRITEBYTECODE='1')
    env.pop('GAME', None)
    bad = []
    for k, spec in enumerate(runs):
        if callable(spec):
            write_packs(d, spec())
            continue
        r = subprocess.run([os.path.join(d, 'build', 'openmtv' + EXE), 'build', spec, '--game', game], cwd=case, env=env,
                           capture_output=True, timeout=300)
        out = r.stdout.decode('utf-8', 'replace').replace('\r\n', '\n').replace(d, 'ROOT').replace('\\', '/')
        msg = message(r.stderr.decode('utf-8', 'replace').replace(d, 'ROOT'))      # (launcher/build_test.go's)
        ok, detail = gold.check('%s, run %d: %s' % (name, k + 1, spec), {'result': [0 if r.returncode == 0 else 'error', out],
                                                                          'files': tree(d)})
        if not ok:
            bad.append('  %r:\n%s\n    (its message: %s)' % (spec, detail, (msg or [''])[-1][:200]))
        elif verbose:
            print('  %r -> %s %s' % (spec, 0 if r.returncode == 0 else 'error', (msg or [''])[-1][:150]))
    return bad


def main(argv):
    go_exe = os.path.join(ROOT, 'build', 'openmtv' + EXE)
    if '--exe' in argv:
        go_exe = os.path.abspath(argv[argv.index('--exe') + 1])
    verbose = '-v' in argv
    gold = golden.Golden('buildtest', '--record' in argv)
    only, skip = [], False
    for a in argv:
        if not skip and not a.startswith('-'):
            only.append(a)
        skip = a == '--exe'
    if not os.path.exists(go_exe):
        sys.exit('buildtest: %s is not built (make launcher)' % go_exe)
    for f in GAME_FILES:
        if not os.path.exists(os.path.join(original_dir(), f)):
            sys.exit('buildtest: %s not found in %s (set GAME)' % (f, original_dir()))
    base = os.path.realpath(tempfile.mkdtemp(prefix='buildtest-'))
    failed = n = 0
    try:
        for name, make, runs, opt in CASES:
            if only and name not in only:
                continue
            n += 1
            bad = run_case(base, name, make, runs, opt, go_exe, verbose, gold)
            print('%-20s %s' % (name, 'ok' if not bad else 'DIFFERENT'))
            for b in bad:
                print(b)
            failed += bool(bad)
    finally:
        shutil.rmtree(base, ignore_errors=True)
    gold.save()
    print('buildtest: %d of %d cases different' % (failed, n) if failed else 'buildtest: all the same')
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
