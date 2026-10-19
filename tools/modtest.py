#!/usr/bin/env python3
"""Tests of the content extensions with changed data (verify.sh step 6).

    modtest.py [NAME ...]      run all (or the named) tests

Each test builds its own game files from the original ones with
tools/content.py (as the editor's mods do), plays a new game in the
decompiled build with an input script and checks the game's memory
(dumps of the emulated memory: the harness writes the game state back
into the original layout first).

    films300  films 201-300 (copies of 1-100; the original non-cultural
              films off the market, 281-300 in the auction pool): the
              dealer offers new films; a film bought, saved, given away,
              loaded back; the start savegame (films 1-200 only) puts it
              back on the market; its title from the film texts; its card
              shows the year 2005 (the original prints "19" + two digits); the savegame holds 100 x 18 bytes more;
              days go by.
    series60  series 51-60 (copies of 1-10; the original series and the
              non-cultural films off the market, 59-60 in the auction pool):
              the dealer offers new series; one bought, saved, given away,
              loaded back, put back on the market by the start savegame; its
              title from the series texts; the savegame's extension ("MTVX",
              counts, 10 x 0x29 bytes); days go by.
    ads60     ads 51-60 (copies of 1-10; the original ads without a fee, out
              of the game): the ad agency offers new ads, their names load,
              the computer players take them; saved, the start savegame frees
              them, loaded back; the savegame's extension; days go by.
    packs     two mods played together (tools/modpacks.py): both add films
              from #201 and both retitle film 31, the second's screenplay 1
              turns into its second new film: the first keeps 201-203, the
              second's films follow as 204-205, film 31 is the second's, the
              screenplay's target follows (205); the game loads the titles.
    pools     series 23 (2-hour episodes) and films 1-3 (3 hours) marked
              as the starting pool: station 1 gets them, the other
              stations' series come from the original pool (18-20); the
              human's first evening ends at slot 6 (the original draws for
              ever there: the new game never begins, day 0).
    shrunk    a savegame made with 300 films (film 280 in the library),
              loaded in the load and save room of a game with 250 (a pack
              rebuilt smaller, its build folder's saves kept): not loaded --
              a warning says why, the game goes on as it was (loaded, its
              libraries and schedules would hold films past the table).
    savesize  999 films and 375 series: tools/content.py refuses them (a
              savegame would not fit in 64 KB); 374 series: the game saves
              65 283 bytes (the buffer's size computed in 32 bits: in 16 it
              wrapped and the save ran over the heap) and loads them back;
              380 series past the check: the game does not save, a warning
              says why.
    oversize  hand-made tables: the films' resource with 1100 records
              (more than the game takes: it plays the original's 200), the
              series' 7 bytes longer than its records: read only as far as
              their blocks hold, the heap stays whole.

Game files come from $GAME (default: the directory above decomp/).
"""
import copy, json, os, shutil, struct, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..'))
GAME = os.environ.get('GAME', os.path.join(ROOT, '..'))
sys.path.insert(0, HERE)
import content, datafixes, modpacks          # noqa: E402

LOADSEG = 0x1B0
DS = (LOADSEG + 0x39D4) << 4
NEW_GAME = ['400 key esc', '1000 click 64 100', '1300 click 172 185']


def original_docs():
    res = content.Resources([os.path.join(GAME, f) for f in ('SOURCE1.RSC', 'SOURCE2.RSC')])
    docs, _ = datafixes.apply(content.export(res))
    return json.loads(json.dumps(docs, ensure_ascii=False)), res


def build(d, docs, res):
    new = content.build(docs)
    new[content.START_SAVE] = content.patch_start_save(res.get(content.START_SAVE), new[0xC9], new[0xCA], new[0xCB], new[0xCC])
    byfile = {}
    for i, data in new.items():
        byfile.setdefault(res.where(i), {})[i] = data
    for n, f in enumerate(('SOURCE1.RSC', 'SOURCE2.RSC')):
        with open(os.path.join(GAME, f), 'rb') as fh:
            data = content.write_rsc(fh.read(), byfile.get(n, {}))
        with open(os.path.join(d, f), 'wb') as fh:
            fh.write(data)
    shutil.copy(os.path.join(ROOT, 'build', 'MTV_U.EXE'), d)


def play(d, lines):
    script = os.path.join(d, 'test.txt')
    with open(script, 'w') as f:
        f.write('\n'.join(lines) + '\n')
    try:
        p = subprocess.run([os.path.join(ROOT, 'build', 'mtv-dec'), '--headless', '--fast', '--nosound',
                            '--game', d, '--args', 'e', '--script', script], capture_output=True, text=True, timeout=300)
        return p.stdout
    except subprocess.TimeoutExpired as e:      # a hang: the checks then miss their dumps
        return e.stdout.decode() if isinstance(e.stdout, bytes) else (e.stdout or '')


class Mem:
    def __init__(self, path):
        with open(path, 'rb') as f:
            self.m = f.read()

    def b(self, a): return self.m[a]
    def w(self, a): return struct.unpack_from('<H', self.m, a)[0]
    def far(self, a): o, s = struct.unpack_from('<HH', self.m, a); return (s << 4) + o
    def g16(self, off): return self.w(DS + off)
    def film(self, i): return self.far(DS + 0x24A9) + i * 0x17
    def series(self, i): return self.far(DS + 0x2646) + i * 0x39
    def cstr(self, a): return self.m[a:self.m.index(b'\0', a)].decode('cp437')


def test_films300(d):
    docs, res = original_docs()
    films = docs['films']['records']
    byid = {r['id']: r for r in films}
    for r in films:
        if r['id'] and r.get('genre') != 6 and r['id'] not in (29, 58, 187):
            r['flags'] &= ~0x60
    for k in range(1, 101):
        c = copy.deepcopy(byid[k])
        c.update(id=200 + k, owner=0, flags2=c.get('flags2', 0) & ~0x10, flags=(c['flags'] | 0x20) & ~0x40)
        if k > 80:
            c['flags'] |= 0x40
        c['price'] = c.get('price') or 50000
        c['text']['en'][0] = 'New: ' + c['text']['en'][0]
        if k == 50:                                 # film 250: West Germany, 2005 (card bytes +0x13, +0x14 = 105)
            u = bytearray.fromhex(c.get('unknown', {}).get('0x13', '00000000'))
            u[0], u[1] = 2, 105
            c.setdefault('unknown', {})['0x13'] = u.hex()
        films.append(c)
    build(d, docs, res)
    dump = lambda n: os.path.join(d, 'd%d.bin' % n)
    out = play(d, NEW_GAME + [
        '1500 dump ' + dump(0),
        '1502 call 1DA8:22F6,250,1,0,0',            # prog_give_to_player(250, station 1, price 0)
        '1504 call 229E:200D,0xD1,0xD1',            # game_save(slot 0xD1)
        '1506 call 1DA8:22F6,250,2,0,0',
        '1508 dump ' + dump(1),
        '1510 call 229E:1386,0xD1,0xD1',            # game_load(0xD1)
        '1512 dump ' + dump(2),
        '1514 call 229E:1386,0x2B,0x2B',            # the start savegame: films 1-200 only
        '1516 dump ' + dump(3),
        '1518 call 229E:1386,0xD1,0xD1',
        '1520 call 048C:025B,0,250',                # txt_film_cache_load(0, 250): film 250 into slot 0
        '1521 call 14E1:4172,250',                  # its info card: the year in g_tip_text3
        '1521 dump ' + dump(4),
        '120000 peek16 39D4:71E4',
        '120001 quit'])
    if not os.path.exists(dump(4)):
        return ['halted before frame 1522']
    m = [Mem(dump(i)) for i in range(5)]
    offers = [m[0].g16(0x19ED + 8 * i) for i in range(1, 11)]
    year_line = m[4].cstr(m[4].far(DS + 0x8F78) + 8).split('~')[0]
    with open(os.path.join(d, 'SOURCE1.RSC'), 'rb') as f:
        save_size = struct.unpack_from('<IH', f.read(6 * 0x100), 6 * 0xD1)[1]
    day = [int(l.split()[4]) for l in out.split('\n') if l.startswith('PEEK frame 120000')]
    return [('the new game set up (day %d)' % m[0].g16(0x71E4), m[0].g16(0x71E4) >= 1),
            ('film count from the table', m[0].far(DS + 0x2646) - m[0].film(0) >= 301 * 0x17),
            ('the dealer offers new films %s' % offers, sum(200 < o < 1000 for o in offers) >= 5),
            ('given to station 2', m[1].b(m[1].film(250) + 5) == 2),
            ('loaded: station 1 again', m[2].b(m[2].film(250) + 5) == 1),
            ('start savegame: on the market', m[3].b(m[3].film(250) + 5) == 0),
            ('its title "%s"' % m[4].cstr(m[4].far(DS + 0x63E7)), m[4].cstr(m[4].far(DS + 0x63E7)) == 'New: ' + byid[50]['text']['en'][0]),
            ('savegame %d bytes' % save_size, save_size == 37605 + 12 + 100 * 0x12),
            # the card function frees its texts when it has drawn them: the heap's free-list links
            # overwrite the first 8 bytes of "West Germany 2005~..."; the year comes after them
            ('its card "...%s"' % year_line, year_line == 'many 2005'),
            ('day %s at frame 120000' % (day or ['-'])[0], bool(day) and day[0] >= 2)]


def test_series60(d):
    docs, res = original_docs()
    series = docs['series']['records']
    byid = {r['id']: r for r in series}
    for r in series:                                # the original series and films off the market
        if r['id']:
            r['flags'] &= ~0x60
    for r in docs['films']['records']:
        if r['id'] and r.get('genre') != 6 and r['id'] not in (29, 58, 187):
            r['flags'] &= ~0x60
    for k in range(1, 11):
        c = copy.deepcopy(byid[k])
        c.update(id=50 + k, owner=0, flags2=c.get('flags2', 0) & ~0x10, flags=(c['flags'] | 0x20) & ~0x40)
        if k > 8:
            c['flags'] |= 0x40
        c['text']['en'][0] = 'New: ' + c['text']['en'][0]
        series.append(c)
    build(d, docs, res)
    dump = lambda n: os.path.join(d, 'd%d.bin' % n)
    # few programmes on the market: the dealer's first refill draws long (day 1 from about frame 3000)
    out = play(d, NEW_GAME + [
        '3500 dump ' + dump(0),
        '3502 call 1DA8:22F6,1055,1,0,0',           # prog_give_to_player(series 55, station 1, price 0)
        '3504 call 229E:200D,0xD1,0xD1',            # game_save(slot 0xD1)
        '3506 call 1DA8:22F6,1055,2,0,0',
        '3508 dump ' + dump(1),
        '3510 call 229E:1386,0xD1,0xD1',            # game_load(0xD1)
        '3512 dump ' + dump(2),
        '3514 call 229E:1386,0x2B,0x2B',            # the start savegame: series 1-50 only
        '3516 dump ' + dump(3),
        '3518 call 229E:1386,0xD1,0xD1',
        '3520 call 048C:0B69,0,55',                 # txt_series_cache_load(0, 55): series 55 into slot 0
        '3521 dump ' + dump(4),
        '120000 peek16 39D4:71E4',
        '120001 quit'])
    if not os.path.exists(dump(4)):
        return ['halted before frame 3521']
    m = [Mem(dump(i)) for i in range(5)]
    owner = lambda k, i: m[k].b(m[k].series(i) + 0x24)
    offers = [m[0].g16(0x19ED + 8 * i) for i in range(1, 11)]
    title = m[4].cstr(m[4].far(DS + 0x65CF + 2))
    with open(os.path.join(d, 'SOURCE1.RSC'), 'rb') as f:
        save_size = struct.unpack_from('<IH', f.read(6 * 0x100), 6 * 0xD1)[1]
    day = [int(l.split()[4]) for l in out.split('\n') if l.startswith('PEEK frame 120000')]
    return [('the new game set up (day %d)' % m[0].g16(0x71E4), m[0].g16(0x71E4) >= 1),
            ('the dealer offers new series %s' % offers, any(1050 < o < 1061 for o in offers)),
            ('given to station 2', owner(1, 55) == 2),
            ('loaded: station 1 again', owner(2, 55) == 1),
            ('start savegame: on the market', owner(3, 55) == 0),
            ('its title "%s"' % title, title == 'New: ' + byid[5]['text']['en'][0]),
            ('savegame %d bytes' % save_size, save_size == 37605 + 12 + 10 * 0x29),
            ('day %s at frame 120000' % (day or ['-'])[0], bool(day) and day[0] >= 2)]


def test_ads60(d):
    docs, res = original_docs()
    ads = docs['ads']['records']
    byid = {r['id']: copy.deepcopy(r) for r in ads}
    for r in ads:                                   # the original ads out of the game (no fee)
        if r['id']:
            r['fee'] = r['penalty'] = 0
    for k in range(1, 11):
        c = copy.deepcopy(byid[k])
        c.update(id=50 + k, holder=0)
        c['text']['en'][0] = 'New: ' + c['text']['en'][0]
        ads.append(c)
    build(d, docs, res)
    dump = lambda n: os.path.join(d, 'd%d.bin' % n)
    out = play(d, NEW_GAME + [
        '60000 dump ' + dump(0),                    # mid day 1: the computer players took ads
        '60002 call 229E:200D,0xD1,0xD1',           # game_save(slot 0xD1)
        '60004 call 229E:1386,0x2B,0x2B',           # the start savegame: ads 1-50 only
        '60006 dump ' + dump(1),
        '60008 call 229E:1386,0xD1,0xD1',
        '60010 dump ' + dump(2),
        '150000 peek16 39D4:71E4',
        '150001 quit'])
    if not os.path.exists(dump(2)):
        return ['halted before frame 60010']
    m = [Mem(dump(i)) for i in range(3)]
    extra = (0xE800 << 4)                           # ads 51..: the far-pointer segment ADS_EXTRA_SEG
    holders = lambda k: [m[k].b(extra + (i - 51) * 0x18 + 0xF) for i in range(51, 61)]
    offers = [m[0].g16(0x19A0 + 7 * i) for i in range(1, 11)]
    name = m[0].cstr(m[0].far(extra + 4 * 0x18))
    with open(os.path.join(d, 'SOURCE1.RSC'), 'rb') as f:
        save_size = struct.unpack_from('<IH', f.read(6 * 0x100), 6 * 0xD1)[1]
    day = [int(l.split()[4]) for l in out.split('\n') if l.startswith('PEEK frame 150000')]
    return [('the ad agency offers new ads %s' % offers, any(50 < o < 61 for o in offers)),
            ('ad 55\'s name "%s"' % name, name == 'New: ' + byid[5]['text']['en'][0]),
            ('taken by stations %s' % holders(0), any(holders(0))),
            ('start savegame: free again %s' % holders(1), not any(holders(1))),
            ('loaded: taken again %s' % holders(2), holders(2) == holders(0)),
            ('savegame %d bytes' % save_size, save_size == 37605 + 12 + 10),
            ('day %s at frame 150000' % (day or ['-'])[0], bool(day) and day[0] >= 2)]


def test_pools(d):
    docs, res = original_docs()
    for r in docs['series']['records']:
        if r['id'] == 23:
            r['flags2'] |= 0x10
    for r in docs['films']['records']:
        if r['id'] in (1, 2, 3):
            r['flags2'] |= 0x10
    build(d, docs, res)
    dump = os.path.join(d, 'd.bin')
    play(d, NEW_GAME + ['1500 dump ' + dump, '1501 quit'])
    if not os.path.exists(dump):
        return ['halted before frame 1500 (a new game hangs)']
    m = Mem(dump)
    owner = lambda i: m.b(m.series(i) + 0x24)
    evening = [m.g16(0x4417 + 0x5A + 2 * k) for k in range(1, 8)]
    return [('the new game set up (day %d)' % m.g16(0x71E4), m.g16(0x71E4) >= 1),
            ('series 23 to station 1', owner(23) == 1),
            ('stations 2 and 3: series from 18-20 (%s)' % [owner(i) for i in (18, 19, 20)],
             sorted(owner(i) for i in (18, 19, 20)) == [0, 2, 3]),
            ('films 1-3 to station 1', all(m.b(m.film(i) + 5) == 1 for i in (1, 2, 3))),
            ('first evening %s ends at slot 6' % evening, all(evening[:6]) and evening[6] == 0)]


def test_packs(d):
    docs, res = original_docs()
    byid = {r['id']: r for r in docs['films']['records']}

    def pack(name, n, base):
        p = copy.deepcopy(docs)
        for k in range(n):
            c = copy.deepcopy(byid[base + k])
            c.update(id=201 + k, owner=0, flags2=c.get('flags2', 0) & ~0x10)
            c['text']['en'][0] = 'Pack %s %d' % (name, k + 1)
            p['films']['records'].append(c)
        next(r for r in p['films']['records'] if r['id'] == 31)['text']['en'][0] = 'Pack %s 31' % name
        return p
    a, b = pack('A', 3, 1), pack('B', 2, 6)
    sp = next(r for r in b['screenplays']['records'] if r['id'] == 1)
    sp.update(flags=sp.get('flags', 0) & ~1, target=202)            # B's screenplay 1 becomes B's film 202
    docs2, rep = modpacks.combine(docs, [('a', a), ('b', b)])
    title = lambda i: next(r for r in docs2['films']['records'] if r['id'] == i)['text']['en'][0]
    target = next(r for r in docs2['screenplays']['records'] if r['id'] == 1)['target']
    checks = [('A keeps 201-203, B appended as 204-205 (%s)' % [p['added'] for p in rep['packs']],
               [p['added'] for p in rep['packs']] == [{'films': [201, 203, 3]}, {'films': [204, 205, 2]}]
               and [title(i) for i in range(201, 206)] == ['Pack A 1', 'Pack A 2', 'Pack A 3', 'Pack B 1', 'Pack B 2']),
              ('film 31 from B, reported (%s)' % rep['conflicts'],
               title(31) == 'Pack B 31' and rep['conflicts'] == [{'table': 'films', 'id': 31, 'packs': ['a', 'b'], 'wins': 'b'}]),
              ('the screenplay follows its film (%d)' % target, target == 205)]
    build(d, docs2, res)
    dump = lambda n: os.path.join(d, 'd%d.bin' % n)
    play(d, NEW_GAME + ['1500 call 048C:025B,0,204', '1500 dump ' + dump(0),      # txt_film_cache_load(0, 204)
                        '1502 call 048C:025B,0,31', '1502 dump ' + dump(1), '1503 quit'])
    if not os.path.exists(dump(1)):
        return checks + ['halted before frame 1502']
    m = [Mem(dump(i)) for i in range(2)]
    t204, t31 = (x.cstr(x.far(DS + 0x63E7)) for x in m)
    script_target = m[0].w(m[0].far(DS + 0x715C) + 0x1B3 + 6)
    return checks + [('the new game set up (day %d)' % m[0].g16(0x71E4), m[0].g16(0x71E4) >= 1),
                     ('film count from the table', m[0].far(DS + 0x2646) - m[0].film(0) >= 206 * 0x17),
                     ('in the game: film 204 "%s", film 31 "%s"' % (t204, t31), t204 == 'Pack B 1' and t31 == 'Pack B 31'),
                     ('in the game: screenplay 1 becomes film %d' % script_target, script_target == 205)]


def test_shrunk(d):
    def films(n):
        docs, res = original_docs()
        byid = {r['id']: r for r in docs['films']['records']}
        for k in range(1, n - 200 + 1):
            c = copy.deepcopy(byid[k])
            c.update(id=200 + k, owner=0, flags2=c.get('flags2', 0) & ~0x10)
            docs['films']['records'].append(c)
        return docs, res
    a, b = os.path.join(d, 'a'), os.path.join(d, 'b')
    os.makedirs(a)
    os.makedirs(b)
    dump = lambda n: os.path.join(d, 'd%d.bin' % n)
    # the office (as tests/scenarios/save_continue.txt), the picture: the safe (room 58)
    office = ['2100 click 230 60', '2500 click 230 60', '2700 click 88 70', '3200 click 76 35']
    build(a, *films(300))
    play(a, NEW_GAME + ['2000 call 1DA8:22F6,280,1,0,0'] + office + [   # prog_give_to_player(280, station 1, price 0)
        '3400 click 134 75', '3600 click 100 45', '3700 key A', '3800 key enter',   # save: slot 2 (res 0xD3), "A"
        '3900 dump ' + dump(0), '3901 quit'])
    build(b, *films(250))
    saved = content.Resources([os.path.join(a, f) for f in ('SOURCE1.RSC', 'SOURCE2.RSC')])
    try:
        saved = {i: saved.get(i) for i in (0xD1, 0xD3)}             # the slots' names, slot 2
    except KeyError:
        return ['no savegame made with 300 films']
    with open(os.path.join(b, 'SOURCE1.RSC'), 'rb') as f:              # the build folder's saves kept
        data = content.write_rsc(f.read(), saved)
    with open(os.path.join(b, 'SOURCE1.RSC'), 'wb') as f:
        f.write(data)
    out = play(b, NEW_GAME + office + [
        '3300 dump ' + dump(1),
        '3400 click 192 75', '3600 click 100 45',                   # load: slot 2
        '3700 dump ' + dump(2), '3700 peek16 39D4:838C',
        '3800 click 160 100', '3900 peek16 39D4:838C',              # the warning away: the safe again
        '4000 key esc', '4200 key esc', '4400 peek16 39D4:838C',    # the office (the clock runs again)
        '120000 peek16 39D4:71E4', '120001 quit'])
    if not os.path.exists(dump(2)):
        return ['halted before frame 3700']
    m = [Mem(dump(i)) for i in range(3)]
    lib = lambda k: [m[k].g16(0x6F19 + 0x6C + 2 * i) for i in range(0x30)]   # station 1's library
    peek = lambda f: [int(l.split()[4]) for l in out.split('\n') if l.startswith('PEEK frame %d ' % f)]
    day = peek(120000)
    return [('saved with 300 films: film 280 in station 1\'s library', 280 in lib(0)),
            ('savegame %d bytes' % len(saved[0xD3]), len(saved[0xD3]) == 37605 + 12 + 100 * 0x12),
            ('with 250 films not loaded: the library as it was, no film past 250 (%s)' % sorted(set(lib(2)) - {0}),
             lib(2) == lib(1) and max(lib(2)) <= 250),
            ('the warning (room %s)' % peek(3700), peek(3700) == [0xFFFF]),
            ('then the safe again (room %s), the office (%s)' % (peek(3900), peek(4400)), peek(3900) == [58] and peek(4400) == [1]),
            ('day %s at frame 120000' % (day or ['-'])[0], bool(day) and day[0] >= 2)]


def test_savesize(d):
    def big(nseries):
        docs, res = original_docs()
        def short(r, n):
            r['text'] = {l: ['X%d' % r['id']] + [''] * (n - 1) for l in ('de', 'en')}
        films = docs['films']['records']
        byid = {r['id']: r for r in films}
        for k in range(201, 1000):
            c = copy.deepcopy(byid[1 + (k - 1) % 200])
            c.update(id=k, owner=0, flags2=c.get('flags2', 0) & ~0x10)
            short(c, 2)
            films.append(c)
        series = docs['series']['records']
        sid = {r['id']: r for r in series}
        for k in range(51, nseries + 1):
            c = copy.deepcopy(sid[1 + (k - 1) % 50])
            c.update(id=k, owner=0, flags2=c.get('flags2', 0) & ~0x10)
            short(c, 22)
            series.append(c)
        return docs, res
    checks = []
    try:
        content.build(big(375)[0])
        checks.append(('999 films, 375 series: refused', False))
    except ValueError as e:
        checks.append(('999 films, 375 series: refused (%s)' % str(e).split(':')[1].strip()[:40], 'a savegame would have 65521 bytes' in str(e)))
    a, b = os.path.join(d, 'a'), os.path.join(d, 'b')
    os.makedirs(a)
    os.makedirs(b)
    dump = lambda n: os.path.join(d, 'd%d.bin' % n)
    # 374 series: the savegame is 65 283 bytes; the original's buffer of 40 000 + the extension
    # (27 678) wrapped at 16 bits to 2 142 bytes and the save ran over the heap
    build(a, *big(374))
    play(a, NEW_GAME + ['1500 call 1DA8:22F6,374,1,0,0',          # prog_give_to_player: film 374 and series 374
                        '1500 call 1DA8:22F6,1374,1,0,0',         # to station 1
                        '1502 call 229E:200D,0xD1,0xD1', '1504 dump ' + dump(0),   # game_save(0xD1)
                        '1506 call 229E:1386,0x2B,0x2B',          # the start savegame
                        '1508 call 229E:1386,0xD1,0xD1', '1510 dump ' + dump(1), '1511 quit'])
    if not os.path.exists(dump(1)):
        return checks + ['halted before frame 1510']
    with open(os.path.join(a, 'SOURCE1.RSC'), 'rb') as f:
        save_size = struct.unpack_from('<IH', f.read(6 * 0x100), 6 * 0xD1)[1]
    m = [Mem(dump(i)) for i in range(2)]
    buf = m[0].far(DS + 0x8FCA)                                 # g_save_buf, its heap block's size before it
    block = (struct.unpack_from('<I', m[0].m, buf - 8)[0] & ~1) - 8
    checks += [('999 films, 374 series: savegame %d bytes' % save_size, save_size == 37605 + 12 + 799 * 0x12 + 324 * 0x29),
               ('in a buffer of %d bytes' % block, block >= save_size),
               ('loaded: film 374 and series 374 station 1\'s', m[1].b(m[1].film(374) + 5) == 1 and m[1].b(m[1].series(374) + 0x24) == 1)]
    # 380 series past the editor's check: the game does not save, the room says why
    keep = getattr(content, 'SAVE_MAX', 0x20000)
    content.SAVE_MAX = 0x20000
    try:
        build(b, *big(380))
    finally:
        content.SAVE_MAX = keep
    office = ['2100 click 230 60', '2500 click 230 60', '2700 click 88 70', '3200 click 76 35']
    out = play(b, NEW_GAME + office + ['3400 click 134 75', '3600 click 100 45', '3700 key A', '3800 key enter',  # save: slot 2
                                       '3900 peek16 39D4:838C', '3901 peek16 39D4:7A98', '3902 quit'])
    peek = lambda f, a: [int(l.split()[4]) for l in out.split('\n') if l.startswith('PEEK frame %d 39D4:%s ' % (f, a))]
    with open(os.path.join(b, 'SOURCE1.RSC'), 'rb') as f:
        slot = struct.unpack_from('<IH', f.read(6 * 0x100), 6 * 0xD3)
    return checks + [('999 films, 380 series: not saved (slot 2 %s, g_save_error %s)' % ('empty' if not slot[1] else '%d bytes' % slot[1], peek(3901, '7A98')),
                      not slot[1] and peek(3901, '7A98') == [1]),
                     ('the warning (room %s)' % peek(3900, '838C'), peek(3900, '838C') == [0xFFFF])]


def heap_ok(m):
    """the far heap's blocks from the first to the last, each header's size leading to the next"""
    p, last = m.far(DS + 0x1824), m.far(DS + 0x1828)
    for _ in range(100000):
        size = struct.unpack_from('<I', m.m, p)[0] & ~1
        if not size or p > last:
            return False
        if p == last:
            return True
        p += size
    return False


def test_oversize(d):
    docs, res = original_docs()
    new = content.build(docs)
    new[content.START_SAVE] = res.get(content.START_SAVE)
    films = new[0xC9]                           # 201 records; hand-made: 1100 (more than 999 films)
    new[0xC9] = films + films[0x17:] * 5 + films[0x17:0x17 * 100]
    series = new[0xCA]                          # and the series' table 7 bytes longer
    new[0xCA] = series + b'\xAA' * 7
    byfile = {}
    for i, data in new.items():
        byfile.setdefault(res.where(i), {})[i] = data
    for n, f in enumerate(('SOURCE1.RSC', 'SOURCE2.RSC')):
        with open(os.path.join(GAME, f), 'rb') as fh:
            data = content.write_rsc(fh.read(), byfile.get(n, {}))
        with open(os.path.join(d, f), 'wb') as fh:
            fh.write(data)
    shutil.copy(os.path.join(ROOT, 'build', 'MTV_U.EXE'), d)
    dump = os.path.join(d, 'd.bin')
    play(d, NEW_GAME + ['1500 dump ' + dump, '1501 quit'])
    if not os.path.exists(dump):
        return ['halted before frame 1500']
    m = Mem(dump)
    return [('the new game set up (day %d)' % m.g16(0x71E4), m.g16(0x71E4) >= 1),
            ('200 films (the films\' block %d bytes)' % (m.far(DS + 0x2646) - m.film(0)), m.far(DS + 0x2646) - m.film(0) < 202 * 0x17),
            ('the heap whole', heap_ok(m)),
            ('series 1 as its record', m.m[m.series(1):m.series(1) + 0x21] == series[0x39:0x39 + 0x21])]


TESTS = {'films300': test_films300, 'series60': test_series60, 'ads60': test_ads60, 'pools': test_pools, 'packs': test_packs,
         'shrunk': test_shrunk, 'savesize': test_savesize, 'oversize': test_oversize}


def run(name):
    d = tempfile.mkdtemp(prefix='mtv_mod_')
    try:
        return TESTS[name](d)
    finally:
        shutil.rmtree(d, ignore_errors=True)


def main():
    from concurrent.futures import ThreadPoolExecutor
    names = sys.argv[1:] or list(TESTS)
    bad = 0
    with ThreadPoolExecutor(max_workers=len(names)) as ex:
        res = {n: ex.submit(run, n) for n in names}
        for n in names:
            checks = res[n].result()
            ok = all(isinstance(c, tuple) and c[1] for c in checks)
            bad += not ok
            notes = '; '.join(c if isinstance(c, str) else '%s %s' % (c[0], 'ok' if c[1] else 'FAIL') for c in checks)
            print('%-10s %s  (%s)' % (n, 'ok' if ok else 'FAILS', notes))
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
