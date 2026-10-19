#!/usr/bin/env python3
"""Checks of drawing at 4x resolution (--filter hr-pixel, src/hr.c; docs/hochaufloesend.md).

Every scenario (tests/scenarios) and the computer players' day (tests/aiday.txt),
on the decompiled build and the logical clock:
1. the same game with and without --filter hr-pixel (every rule nearest, and
   the real rules of runtime/hr_rules.txt with the 4x font): the
   HASH/STATE/PEEK/CALL lines are equal (the cells are never read by the game);
2. the self-test (HR_SELFTEST, every rule nearest: HR_RULES=nearest): every
   screen byte whose cell is valid has the flat cell of its own value, at
   every frame -- the cell machinery against the plain 4x enlargement; and
   the pixels without a valid cell (they show their own pixel) are no more than the
   original's own (the intro, tools/hrsurvey.py's measurement), with the
   real rules as well;
3. our own pixel writers with the player's options (tools/castfittest.py's
   game: role fit; --film-offers=17, --fixes: the tower sign) leave no pixel
   without a valid cell after the intro;
4. a network game (tests/mp/ad_agency.txt: two humans, --seats 2): the same
   game with and without --filter hr-pixel, and every seat's screen keeps its
   cells through the seat switches (runtime/seat.c, src/hr.c hr_seat_*):
   the self-test without a single pixel lacking a valid cell;
5. --filter hr (the setting before 2026-10-18, its pictures smoothed) on three
   scenarios: read as hr-pixel -- the same game and the same picture;
6. the picture: in the planner (room_planner_schedule, frame 4290) the
   1280x800 picture of --filter hr-pixel (shothr) with every rule nearest equals
   the 320x200 shot enlarged 4x; and with the real rules, the building's sky
   (tests/aiday.txt, frame 6100: drawn line by line over the boss's office)
   is as flat as the original's -- no old sub-pixels between its lines.

  hrtest.py [-j N] [NAME ...]
"""
import os, re, sys, glob, shutil, subprocess, tempfile
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..'))
BIN = os.path.join(ROOT, 'build', 'mtv-dec')
GAME = os.environ.get('GAME', os.path.join(ROOT, 'run'))
KEEP = ('HASH', 'PEEK', 'CALL', 'STATE')
# pixel-frames without a valid cell in the original (the intro's decoders, the TV
# family's never-painted row): tools/hrsurvey.py, docs/hochaufloesend-messung.md
INVALID_MAX = {'smoke': 229024, 'event_ai_sabotage': 396231, 'event_bailiff': 396231,
               'event_terrorist_board': 396231, 'sammy_win': 396231}
INVALID_DEFAULT = 395895


def lang_of(path):
    for l in open(path):
        m = re.search(r'--args "?([de])', l)
        if m:
            return m.group(1)
    return 'e'


def run(script, lang, extra, env=None):
    d = tempfile.mkdtemp(prefix='mtv_hrt_')
    try:
        for f in ('SOURCE1.RSC', 'SOURCE2.RSC'):
            shutil.copy(os.path.join(GAME, f), d)
        p = subprocess.run([BIN, '--headless', '--nosound', '--logical-time', '--game', d, '--args', lang,
                            '--script', script] + extra, capture_output=True, text=True,
                           env=dict(os.environ, **(env or {})))
        return p.returncode, p.stdout, p.stderr
    finally:
        shutil.rmtree(d, ignore_errors=True)


def selftest(err):
    m = re.search(r'\[hr\] selftest: (\d+) frames, (\d+) cells not flat, (\d+) invalid', err)
    return (int(m.group(2)), int(m.group(3))) if m else (None, None)


def check(job):
    name, script = job
    lang = lang_of(script)
    rc0, out0, _ = run(script, lang, [])
    rc1, out1, err1 = run(script, lang, ['--filter', 'hr-pixel'], {'HR_SELFTEST': '1', 'HR_RULES': 'nearest'})
    rc2, out2, err2 = run(script, lang, ['--filter', 'hr-pixel'], {'HR_SELFTEST': '1'})
    errs = []
    if rc0 or rc1 or rc2:
        errs.append('exit %d / %d / %d' % (rc0, rc1, rc2))
    k0, k1, k2 = ([l for l in o.split('\n') if l.startswith(KEEP)] for o in (out0, out1, out2))
    if k0 != k1 or k0 != k2:
        errs.append('another game with --filter hr-pixel')
    bad, inv = selftest(err1)
    _, inv2 = selftest(err2)
    lim = INVALID_MAX.get(name, INVALID_DEFAULT)
    if bad is None or inv2 is None:
        errs.append('no self-test line')
    else:
        if bad:
            errs.append('%d cells not flat' % bad)
        for what, n in (('nearest', inv), ('rules', inv2)):
            if n > lim:
                errs.append('%s: %d pixel-frames without a valid cell (the original: %d)' % (what, n, lim))
    return name, errs, inv2


def own_writers(tmp):
    """the role fit, 17 film offers and the tower sign: no pixel without a cell after the intro"""
    sys.path.insert(0, HERE)
    import castfittest
    path = os.path.join(tmp, 'castfit.txt')
    open(path, 'w').write(castfittest.SCRIPT.format(d=tmp))
    errs = []
    for name, script, lang, extra in (('castfit', path, 'd', ['--cast-fit']),
                                      ('dealer', os.path.join(ROOT, 'tests', 'scenarios', 'room_dealer_buy.txt'), 'e',
                                       ['--film-offers=17', '--fixes'])):
        rc, out, err = run(script, lang, ['--filter', 'hr-pixel'] + extra, {'HR_SELFTEST': '1'})
        _, inv = selftest(err)
        if rc or inv is None:
            errs.append('%s: exit %d' % (name, rc))
        elif inv > INVALID_DEFAULT:
            errs.append('%s: %d pixel-frames without a valid cell (the intro: %d)' % (name, inv, INVALID_DEFAULT))
    return errs


def network(tmp):
    script = os.path.join(ROOT, 'tests', 'mp', 'ad_agency.txt')
    extra = ['--humans', '1,2', '--seats', '2', '--new-game', '--seed', '29']
    rc0, out0, _ = run(script, 'e', extra)
    rc1, out1, err1 = run(script, 'e', extra + ['--filter', 'hr-pixel'], {'HR_SELFTEST': '1', 'HR_RULES': 'nearest'})
    k0, k1 = ([l for l in o.split('\n') if l.startswith(KEEP)] for o in (out0, out1))
    bad, inv = selftest(err1)
    errs = []
    if rc0 or rc1:
        errs.append('exit %d / %d' % (rc0, rc1))
    if k0 != k1 or not k0:
        errs.append('another game with --filter hr-pixel')
    if bad is None or bad or inv:
        errs.append('self-test: %s cells not flat, %s pixel-frames without a cell' % (bad, inv))
    return errs


def old_hr(tmp):
    """--filter hr, an older setting: played as hr-pixel -- the same game, the same 4x picture"""
    errs = []
    for name, frame in (('room_planner_schedule', 4290), ('room_txmap_buy', 4100), ('room_dealer_buy', 4050)):
        src = os.path.join(ROOT, 'tests', 'scenarios', name + '.txt')
        lines = [l for l in open(src).read().split('\n') if l.strip() and not l.startswith('#')]
        outs, pics = [], []
        for filt in ('hr-pixel', 'hr'):
            path = os.path.join(tmp, '%s_%s.txt' % (name, filt))
            pic = os.path.join(tmp, '%s_%s.png' % (name, filt))
            open(path, 'w').write('\n'.join(sorted(lines + ['%d shothr %s' % (frame, pic)], key=lambda l: int(l.split()[0]))) + '\n')
            rc, out, _ = run(path, 'e', ['--filter', filt])
            outs.append((rc, [l for l in out.split('\n') if l.startswith(KEEP)]))
            pics.append(open(pic, 'rb').read() if os.path.exists(pic) else None)
        if outs[0][0] or outs[1][0] or outs[0][1] != outs[1][1] or not outs[0][1]:
            errs.append('%s: another game (exit %d / %d)' % (name, outs[0][0], outs[1][0]))
        elif pics[0] is None or pics[0] != pics[1]:
            errs.append('%s: another picture with --filter hr' % name)
    return errs


def picture(tmp):
    from PIL import Image, ImageChops
    src = os.path.join(ROOT, 'tests', 'scenarios', 'room_planner_schedule.txt')
    lines = [l for l in open(src).read().split('\n') if l.strip() and not l.startswith('#')]
    lines += ['4290 shot %s/orig.png' % tmp, '4290 shothr %s/hr.png' % tmp]
    lines.sort(key=lambda l: int(l.split()[0]))
    path = os.path.join(tmp, 'plan.txt')
    open(path, 'w').write('\n'.join(lines) + '\n')
    rc, _, err = run(path, 'e', ['--filter', 'hr-pixel'], {'HR_RULES': 'nearest'})
    if rc or not os.path.exists(os.path.join(tmp, 'hr.png')):
        return ['picture: exit %d' % rc]
    o = Image.open(os.path.join(tmp, 'orig.png')).convert('RGB').resize((1280, 800), Image.NEAREST)
    h = Image.open(os.path.join(tmp, 'hr.png')).convert('RGB')
    box = ImageChops.difference(o, h).getbbox()
    errs = ['picture: differs from 4x nearest in %s' % (box,)] if box else []
    lines = [l for l in open(os.path.join(ROOT, 'tests', 'aiday.txt')).read().split('\n')
             if l.strip() and not l.startswith('#') and int(l.split()[0]) < 6000]
    lines += ['6100 shot %s/b.png' % tmp, '6100 shothr %s/bhr.png' % tmp, '6101 quit']
    path = os.path.join(tmp, 'sky.txt')
    open(path, 'w').write('\n'.join(lines) + '\n')
    for filt in ('hr-pixel',):
        rc, _, _ = run(path, 'e', ['--filter', filt])
        if rc or not os.path.exists(os.path.join(tmp, 'bhr.png')):
            errs.append('sky (%s): exit %d' % (filt, rc)); continue
        o = Image.open(os.path.join(tmp, 'b.png')).convert('RGB').resize((1280, 800), Image.NEAREST).crop((0, 0, 200, 160))
        h = Image.open(os.path.join(tmp, 'bhr.png')).convert('RGB').crop((0, 0, 200, 160))
        box = ImageChops.difference(o, h).getbbox()
        if box:
            errs.append('sky (%s): not the original\'s in %s' % (filt, box))
        os.remove(os.path.join(tmp, 'bhr.png'))
    return errs


def main():
    args = sys.argv[1:]
    n = os.cpu_count()
    if '-j' in args:
        i = args.index('-j'); n = int(args[i + 1]); del args[i:i + 2]
    jobs = [(os.path.basename(p)[:-4], p) for p in sorted(glob.glob(os.path.join(ROOT, 'tests', 'scenarios', '*.txt')))]
    jobs.append(('aiday', os.path.join(ROOT, 'tests', 'aiday.txt')))
    if args:
        jobs = [j for j in jobs if j[0] in args]
    fails = 0
    with ThreadPoolExecutor(n) as ex:
        for name, errs, inv in ex.map(check, jobs):
            print('%-26s %s%s' % (name, 'ok' if not errs else 'FAIL: ' + '; '.join(errs),
                                  '' if inv is None else '  (%d pixel-frames without a cell)' % inv), flush=True)
            fails += bool(errs)
    for label, fn in (('own writers', own_writers), ('network game', network), ('old hr setting', old_hr), ('picture', picture)):
        tmp = tempfile.mkdtemp(prefix='mtv_hrt_x_')
        try:
            e = fn(tmp)
        finally:
            shutil.rmtree(tmp, ignore_errors=True)
        print('%-26s %s' % (label, 'ok' if not e else 'FAIL: ' + '; '.join(e)), flush=True)
        fails += bool(e)
    print('hrtest: %d of %d failed' % (fails, len(jobs) + 4))
    sys.exit(1 if fails else 0)


if __name__ == '__main__':
    main()
