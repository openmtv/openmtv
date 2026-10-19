#!/usr/bin/env python3
"""The measuring run for drawing at 4x resolution (docs/hochaufloesend.md, stage 1).

  hrsurvey.py run OUT [--play] [-j N]   every scenario (tests/scenarios) and one
                                        computer-player day (tests/aiday.txt) through
                                        build/mtv-dec-hrs on the logical clock (as
                                        play.sh; the scripts' clicks land as meant);
                                        OUT/<name>/<name>.hrs etc.
                                        --play: with the options a player has on
                                        (fixes, role fit, more offers, topicality bar)
                                        and tools/castfittest.py's game
  hrsurvey.py report OUT [OUT...]       the tables of stage 1 (Markdown, stdout) and
                                        OUT/report/: snapshots as PNG, heat map

What the lines mean: tools/hrsurvey.c.  Functions are resolved with nm against
the binary (each file's "base" is the run-time address of hrs_helper).
Game files: $GAME (default: run/ of this tree).
"""
import os, re, sys, glob, shutil, subprocess, tempfile, collections
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..'))
BIN = os.path.join(ROOT, 'build', 'mtv-dec-hrs')
GAME = os.environ.get('GAME', os.path.join(ROOT, 'run'))
sys.path.insert(0, HERE)
import rsc

HELPERS = {1: 'vga_put_bitmap', 2: 'vga_get_bitmap', 3: 'vga_put_rect', 4: 'bitmap_fill', 5: 'bitmap_put',
           6: 'blit_opaque_aligned', 7: 'blit_transparent_aligned', 8: 'blit_opaque_shifted',
           9: 'blit_transparent_shifted', 10: 'sprite_put', 11: 'glyph_put', 12: 'delta_apply', 13: 'lzw_decode'}
KINDS = {0: 'Anfang (0)', 1: 'Sprite-Bank', 2: 'Füllen', 3: 'calloc/setmem', 4: 'LZW/Delta (Intro, Klangdaten)'}
PLAY = ['--fixes', '--cast-fit=yes', '--series-topicality=yes', '--ad-offers=17', '--film-offers=17']


def jobs(out, play):
    """(name, script, language, extra options)"""
    j = [(os.path.basename(p)[:-4], p, lang_of(p), []) for p in sorted(glob.glob(os.path.join(ROOT, 'tests', 'scenarios', '*.txt')))]
    j.append(('aiday', os.path.join(ROOT, 'tests', 'aiday.txt'), 'e', []))
    if play:                             # the role fit in the shop (tools/castfittest.py's game)
        import castfittest
        d = os.path.join(out, 'castfit')
        os.makedirs(d, exist_ok=True)
        path = os.path.join(d, 'script.txt')
        open(path, 'w').write(castfittest.SCRIPT.format(d=d))
        j.append(('castfit', path, 'd', []))
    return j


def lang_of(path):
    for l in open(path):
        m = re.search(r'--args "?([de])', l)
        if m:
            return m.group(1)
    return 'e'


def run_one(out, name, script, lang, extra):
    d = tempfile.mkdtemp(prefix='mtv_hrs_')
    try:
        for f in ('SOURCE1.RSC', 'SOURCE2.RSC'):
            shutil.copy(os.path.join(GAME, f), d)
        shutil.copy(os.path.join(ROOT, 'build', 'MTV_U.EXE'), d)
        od = os.path.join(out, name)
        os.makedirs(od, exist_ok=True)
        env = dict(os.environ, HR_SURVEY=os.path.join(od, name + '.hrs'))
        p = subprocess.run([BIN, '--headless', '--nosound', '--logical-time', '--game', d, '--args', lang,
                            '--script', script] + extra, capture_output=True, text=True, env=env)
        open(os.path.join(od, 'stdout.txt'), 'w').write(p.stdout)
        open(os.path.join(od, 'stderr.txt'), 'w').write(p.stderr)
        return name, p.returncode
    finally:
        shutil.rmtree(d, ignore_errors=True)


def cmd_run(args):
    out = args[0]
    extra = PLAY if '--play' in args else []
    n = int(args[args.index('-j') + 1]) if '-j' in args else os.cpu_count()
    os.makedirs(out, exist_ok=True)
    with ThreadPoolExecutor(n) as ex:
        for name, rc in ex.map(lambda j: run_one(out, j[0], j[1], j[2], j[3] + extra), jobs(out, bool(extra))):
            print('%-28s %s' % (name, 'ok' if rc == 0 else 'exit %d' % rc), flush=True)


# ---- report ----

_syms = None
def symbols():
    global _syms
    if _syms is None:
        lines = subprocess.run(['nm', BIN], capture_output=True, text=True).stdout.split('\n')
        s = []
        for l in lines:
            p = l.split()
            if len(p) == 3 and p[1] in 'tTwW':
                s.append((int(p[0], 16), p[2]))
        s.sort()
        _syms = (s, dict((n, a) for a, n in s))
    return _syms


def resolver(base):
    s, by_name = symbols()
    delta = int(base, 16) - by_name['hrs_helper']
    addrs = [a for a, _ in s]
    import bisect
    cache = {}
    def name(p):
        if p in ('(nil)', '0x0', '0'):
            return '-'
        if p not in cache:
            a = int(p, 16) - delta
            i = bisect.bisect_right(addrs, a) - 1
            cache[p] = s[i][1] if i >= 0 else p
        return cache[p]
    return name


def load(path):
    """the lines of one .hrs file, functions resolved"""
    lines = open(path).read().split('\n')
    name = resolver(lines[0].split()[1])
    r = collections.defaultdict(list)
    for l in lines[1:]:
        p = l.split()
        if not p:
            continue
        if p[0] == 'draw':
            r['draw'].append((int(p[1]), int(p[2]), int(p[3]), name(p[4]), name(p[5]), name(p[6]), int(p[7]), int(p[8])))
        elif p[0] == 'taint':
            r['taint'].append((int(p[1]), name(p[2]), name(p[3]), name(p[4]), int(p[5]), int(p[6]), int(p[7]), p[8],
                               int(p[9]), int(p[10]), int(p[11]), int(p[12])))
        elif p[0] == 'memcpy':
            r['memcpy'].append((name(p[1]), name(p[2]), name(p[3]), int(p[4]), int(p[5]), int(p[6])))
        elif p[0] == 'bank':
            r['bank'].append((int(p[1]), int(p[2])))
        elif p[0] == 'screen':
            r['screen'].append((int(p[1]), int(p[2]), int(p[3])))
        elif p[0] == 'invalid':
            r['invalid'].append((int(p[1]), int(p[2]), int(p[3])))
        elif p[0] == 'frames':
            r['frames'] = int(p[1])
    return r


def chain(f1, f2, f3):
    """a screen's functions (F2 <- F3, the drawing wrappers already left out by
    tools/hrsurvey.c), the helper's immediate caller in brackets if it differs"""
    fs = [f for f in (f2, f3) if f != '-']
    t = ' ← '.join(fs) if fs else f1
    return t if f1 in ('-', f2) else '%s (%s)' % (t, f1)


sprite_dims = {}
def bank_info():
    idx = rsc.read_index([os.path.join(GAME, 'SOURCE1.RSC'), os.path.join(GAME, 'SOURCE2.RSC')])
    info = {}
    for i in sorted(idx):
        try:
            s = rsc.decode_bank(rsc.raw(idx, i))
        except Exception:
            continue
        dims = collections.Counter((w, h) for w, h, _ in s)
        info[i] = (len(s), dims)
        for k, (w, h, _) in enumerate(s):
            sprite_dims[(i, k)] = '%dx%d' % (w, h)
    return info


def fmt(n):
    return '{:,}'.format(n).replace(',', '.')


def cmd_report(args):
    runs = {}
    for out in args:
        for f in sorted(glob.glob(os.path.join(out, '*', '*.hrs'))):
            runs[(out, os.path.basename(f)[:-4])] = load(f)
    if not runs:
        sys.exit('no .hrs files under ' + ' '.join(args))
    rep = os.path.join(args[0], 'report')
    os.makedirs(rep, exist_ok=True)
    w = sys.stdout.write
    multi = len(args) > 1
    label = lambda k: ('%s/%s' % (os.path.basename(k[0].rstrip('/')), k[1])) if multi else k[1]

    # 1. invalid pixels per run
    w('### Ungültige Pixel je Lauf\n\n')
    w('Pixel×Bilder, deren Schattenwert nicht mehr stimmt (dort zeigte Stufe 2 MMPX).  '
      'Klasse 1 = von einer ungültigen Quelle kopiert, 2 = Intro-Decoder, 3 = am Helfer vorbei auf den Schirm.\n\n')
    w('| Lauf | Bilder | Klasse 1 | Klasse 2 | Klasse 3 | Bilder mit ungültigen Pixeln |\n|---|---:|---:|---:|---:|---:|\n')
    tot = [0, 0, 0, 0]
    for k, r in sorted(runs.items()):
        inv = {c: (px, fr) for c, px, fr in r['invalid']}
        frs = max(fr for _, fr in inv.values()) if inv else 0
        tot[0] += r['frames']
        for c in (1, 2, 3):
            tot[c] += inv.get(c, (0, 0))[0]
        w('| %s | %s | %s | %s | %s | %s |\n' % (label(k), fmt(r['frames']), *(fmt(inv.get(c, (0, 0))[0]) for c in (1, 2, 3)), fmt(frs)))
    w('| **alle** | %s | %s | %s | %s | |\n\n' % tuple(fmt(t) for t in tot))

    # 2. who read invalid bytes
    taint = collections.defaultdict(lambda: [0, 0, set(), set()])
    for k, r in runs.items():
        for (h, f1, f2, f3, kind, res, spr, src, w8, hh, by, ca) in r['taint']:
            t = taint[(HELPERS.get(h, h), chain(f1, f2, f3), KINDS.get(kind, kind), res)]
            t[0] += by; t[1] += ca; t[2].add(label(k)); t[3].add('%s %dx%d' % (src, w8 * 8, hh))
    w('### Helfer, die ungültige Bytes lasen\n\n')
    if taint:
        w('| Helfer | Aufrufer | Herkunft laut Schatten | Res | Bytes | Aufrufe | Quelle (Pixel, Größe) | Läufe |\n|---|---|---|---:|---:|---:|---|---|\n')
        for key, (by, ca, rs, srcs) in sorted(taint.items(), key=lambda x: -x[1][0]):
            w('| %s | %s | %s | %s | %s | %s | %s | %s |\n' % (key[0], key[1], key[2], key[3] or '', fmt(by), fmt(ca),
                                                               ', '.join(sorted(srcs)[:3]) + (' …' if len(srcs) > 3 else ''),
                                                               ', '.join(sorted(rs)[:4]) + (' …' if len(rs) > 4 else '')))
    else:
        w('Keine.\n')
    w('\n')

    # 3. far_memcpy carrying pixels
    mc = collections.defaultdict(lambda: [0, set()])
    for k, r in runs.items():
        for (f1, f2, f3, kind, res, by) in r['memcpy']:
            m = mc[(chain(f1, f2, f3), KINDS.get(kind, kind), res)]
            m[0] += by; m[1].add(label(k))
    w('### Pixel, die `far_memcpy` bewegt (Stufe 2 muss die Zellen dort mittragen)\n\n')
    if mc:
        w('| Aufrufer | Herkunft | Res | Bytes | Läufe |\n|---|---|---:|---:|---:|\n')
        for key, (by, rs) in sorted(mc.items(), key=lambda x: (x[0][0], x[0][2])):
            w('| %s | %s | %d | %s | %d |\n' % (key[0], key[1], key[2], fmt(by), len(rs)))
    w('\n')

    # 4. the screen by origin
    comp = collections.Counter()
    scr_res = collections.Counter()
    for r in runs.values():
        for kind, res, n in r['screen']:
            comp[kind] += n
            if kind == 1:
                scr_res[res] += n
    allpx = sum(comp.values()) or 1
    w('### Woher die gültigen Bildschirmpixel stammen\n\n| Herkunft | Anteil |\n|---|---:|\n')
    for kind, n in comp.most_common():
        w('| %s | %.1f %% |\n' % (KINDS.get(kind, kind), 100.0 * n / allpx))
    w('\n')

    # 5. banks -> helpers
    info = bank_info()
    banks = collections.defaultdict(lambda: {'helpers': collections.Counter(), 'callers': collections.Counter(),
                                             'sprites': set(), 'runs': set(), 'decodes': 0})
    for k, r in runs.items():
        for (res, spr, h, f1, f2, f3, by, ca) in r['draw']:
            b = banks[res]
            b['helpers'][HELPERS.get(h, h)] += by
            b['callers'][chain(f1, f2, f3)] += by
            b['sprites'].add(spr)
            b['runs'].add(label(k))
        for res, n in r['bank']:
            banks[res]['decodes'] += n
    w('### Sprite-Bänke: womit sie gezeichnet werden\n\n')
    w('Bytes, die ein Helfer direkt aus der Bank (oder ihrer `far_memcpy`-Kopie) las; Bildschirm = Anteil an allen gültigen Bildschirmpixeln.\n\n')
    w('| Res | Sprites (benutzt/alle) | Größen | Helfer | Aufrufer | Bildschirm | Läufe |\n|---:|---|---|---|---|---:|---:|\n')
    for res in sorted(banks):
        b = banks[res]
        n, dims = info.get(res, (0, collections.Counter()))
        tot_b = sum(b['helpers'].values()) or 1
        hs = ', '.join('%s %d %%' % (h, round(100 * v / tot_b)) for h, v in b['helpers'].most_common(3))
        cs = '; '.join(c for c, _ in b['callers'].most_common(3))
        ds = ', '.join('%dx%d' % wh + ('×%d' % c if c > 1 else '') for wh, c in dims.most_common(3)) + (' …' if len(dims) > 3 else '')
        w('| %d | %d/%d | %s | %s | %s | %.2f %% | %d |\n' % (res, len(b['sprites']), n, ds, hs, cs,
                                                            100.0 * scr_res[res] / allpx, len(b['runs'])))
    w('\n')

    # per sprite (OUT/report/sprites.md): which helper, which screens
    per = collections.defaultdict(lambda: [collections.Counter(), collections.Counter()])
    for k, r in runs.items():
        for (res, spr, h, f1, f2, f3, by, ca) in r['draw']:
            per[(res, spr)][0][HELPERS.get(h, h)] += ca
            per[(res, spr)][1][chain(f1, f2, f3)] += ca
    with open(os.path.join(rep, 'sprites.md'), 'w') as o:
        o.write('| Res | Sprite | Größe | Helfer (Aufrufe) | Aufrufer (Aufrufe) |\n|---:|---:|---|---|---|\n')
        for (res, spr) in sorted(per):
            hs, cs = per[(res, spr)]
            o.write('| %d | %d | %s | %s | %s |\n' % (res, spr, sprite_dims.get((res, spr), ''),
                    ', '.join('%s %d' % x for x in hs.most_common(3)), '; '.join('%s %d' % x for x in cs.most_common(4))))

    # images: snapshots and heat maps as PNG
    try:
        from PIL import Image
    except ImportError:
        return
    for out in args:
        for f in sorted(glob.glob(os.path.join(out, '*', '*.ppm')) + glob.glob(os.path.join(out, '*', '*.heat.pgm'))):
            dst = os.path.join(rep, os.path.basename(os.path.dirname(f)) + '__' + os.path.basename(f).rsplit('.', 1)[0] + '.png')
            Image.open(f).save(dst)


if __name__ == '__main__':
    if len(sys.argv) < 3 or sys.argv[1] not in ('run', 'report'):
        sys.exit(__doc__)
    {'run': cmd_run, 'report': cmd_report}[sys.argv[1]](sys.argv[2:])
