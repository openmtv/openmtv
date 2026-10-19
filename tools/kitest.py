#!/usr/bin/env python3
"""Checks of mtv-dec --make-ki (src/ki_make.c, docs/ki-bilder.md) against
tools/hrai.py, without a model or ONNX Runtime: the model's output stands in
as each input picture 4x nearest.

1. the model's input: --ki-dump writes the same pictures as hrai.py export
   (every one, pixel for pixel);
2. the set: from the same output (--ki-from) mtv-dec makes the set hrai.py
   import makes, byte for byte -- with the game's 31 colours and with extra
   colours (a palette of 223 made up here);
   (with --ki-parent this script: its parent, so it runs), and with --ki-parent
   of a process that is not its parent it ends at once and writes nothing --
   the launcher ended outright;
3. a network game (tests/mp/film_offers: the film dealer with 17 tapes) with
   one client showing that set (HR pixel; Ctrl+K off and on at the dealer's:
   pictures taken from it) and the other the plain game: both play the
   recording's game exactly -- the pictures are display only.

  kitest.py [GAMEDIR]      (default $GAME, run/ or the directory above; needs Pillow and numpy)
"""
import os, re, shutil, subprocess, sys, tempfile
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
BIN = os.path.join(ROOT, 'build', 'mtv-dec')


def main():
    run = os.path.join(ROOT, 'run')
    game = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.environ.get('GAME')
                           or (run if os.path.exists(os.path.join(run, 'SOURCE1.RSC')) else os.path.join(ROOT, '..')))
    tmp = tempfile.mkdtemp(prefix='kitest_')
    env = dict(os.environ, GAME=game)
    fails = 0
    try:
        py_in, c_in, out_png, out_ppm = (os.path.join(tmp, d) for d in ('py_in', 'c_in', 'out_png', 'out_ppm'))
        for d in (c_in, out_png, out_ppm):
            os.makedirs(d)
        subprocess.run([sys.executable, os.path.join(HERE, 'hrai.py'), 'export', py_in], check=True, env=env, stdout=subprocess.DEVNULL)
        subprocess.run([BIN, '--make-ki', 'none', '--game', game, '--ki-dump', c_in], check=True, stdout=subprocess.DEVNULL)
        py = sorted(f[:-4] for f in os.listdir(py_in) if f.endswith('.png'))
        c = sorted(f[:-4] for f in os.listdir(c_in))
        same = [n for n in py if n in c and np.array_equal(np.asarray(Image.open(os.path.join(py_in, n + '.png')).convert('RGB')),
                                                       np.asarray(Image.open(os.path.join(c_in, n + '.ppm')).convert('RGB')))]
        ok = py == c and len(same) == len(py)
        fails += not ok
        print('%s the model input: %d pictures from hrai.py, %d from mtv-dec, %d the same' % ('ok  ' if ok else 'FAIL', len(py), len(c), len(same)))
        for n in py:                       # the stand-in output: 4x nearest
            a = np.asarray(Image.open(os.path.join(py_in, n + '.png')).convert('RGB'))
            b = a.repeat(4, 0).repeat(4, 1)
            Image.fromarray(b).save(os.path.join(out_png, n + '.png'))
            Image.fromarray(b).save(os.path.join(out_ppm, n + '.ppm'))
        pal = np.random.default_rng(7).integers(0, 256, 768, dtype=np.uint8)     # any 223 extra colours
        pal_path = os.path.join(tmp, 'extra.pal')
        open(pal_path, 'wb').write(pal.tobytes())
        for colours, extra in (('31 colours', []), ('extra colours', ['--pal', pal_path])):
            a, b = os.path.join(tmp, 'py.ki'), os.path.join(tmp, 'c.ki')
            subprocess.run([sys.executable, os.path.join(HERE, 'hrai.py'), 'import', py_in, out_png, a, '--model', 'test'] + extra,
                           check=True, env=env, stdout=subprocess.DEVNULL)
            subprocess.run([BIN, '--make-ki', 'none', '--game', game, '--ki-from', out_ppm, '--ki-out', b, '--ki-model', 'test',
                            '--ki-parent', str(os.getpid())] + (['--ki-palette', pal_path] if extra else []), check=True, stdout=subprocess.DEVNULL)
            ok = open(a, 'rb').read() == open(b, 'rb').read()
            fails += not ok
            print('%s the set, %s: %s' % ('ok  ' if ok else 'FAIL', colours, 'byte for byte the same' if ok else 'differs'))
        if os.name != 'nt':             # (Windows waits on the process itself: its own check by hand)
            orphan = os.path.join(tmp, 'orphan.ki')
            p = subprocess.run([BIN, '--make-ki', 'none', '--game', game, '--ki-from', out_ppm, '--ki-out', orphan, '--ki-parent', '1'],
                               capture_output=True, text=True)
            ok = p.returncode != 0 and 'has ended' in p.stderr and 'ki 1 of' not in p.stdout and not os.path.exists(orphan)
            fails += not ok
            print('%s its launcher gone (--ki-parent not its parent): %s' % ('ok  ' if ok else 'FAIL',
                  'ends before the first picture, no set' if ok else 'exit %d, %r' % (p.returncode, p.stderr[-200:])))
        fails += not network(game, tmp, b)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    print('kitest: %d failed' % fails)
    sys.exit(1 if fails else 0)


def network(game, tmp, ki):
    """3.: seat 1 with the set KI and two switches, seat 0 without -> ok"""
    os.environ['GAME'] = game
    sys.path.insert(0, HERE)
    import mptest, seattest
    plain = mptest.client_args
    mptest.client_args = lambda g, script, humans, port, seat, n: plain(g, script, humans, port, seat, n) + (
        ['--filter', 'hr-pixel', '--ai-pictures', ki] if seat == 1 else [])
    n, d = 'mp:film_offers', os.path.join(tmp, 'mp')
    os.makedirs(d)
    script, _ = seattest.mp_script(n, d)
    lines = [l for l in open(script).read().split('\n') if l] + ['4800 kitoggle', '5000 kitoggle']
    lines.sort(key=lambda l: int(l.split()[0]))
    open(script, 'w').write('\n'.join(lines) + '\n')
    got, _, _, codes, errs = mptest.net_game(n, d, script)
    want = mptest.recorded(n)
    # (the set loaded, its pictures taken -- src/hr.c counts them --, switched off and on)
    taken = [int(m) for m in re.findall(r'\[hr\] AI pictures o(?:n|ff) \((\d+) pictures taken', errs[1])]
    shown = 'AI pictures:' in errs[1] and len(taken) == 2 and taken[0] > 0
    ok = all(g == want for g in got) and shown and not any(codes)
    print('%s a network game, one client with the set and Ctrl+K, one without: %s' % ('ok  ' if ok else 'FAIL',
          'both the recording\'s game' if ok else 'seat 0 %s, seat 1 %s, the set %s' % (
              'same' if got[0] == want else mptest.diff(want, got[0]), 'same' if got[1] == want else mptest.diff(want, got[1]),
              'shown and switched' if shown else 'not shown')))
    if not ok:
        for l in errs[1].split('\n'):
            if l.startswith(('[hr]', '[ki]', '[net]')):
                print('    seat 1: ' + l)
    return ok


if __name__ == '__main__':
    main()
