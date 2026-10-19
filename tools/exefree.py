#!/usr/bin/env python3
"""Does the decompiled build play the same game without MTV.EXE?

    python3 tools/exefree.py [NAME ...]      session1 and every scenario

build/mtv-dec loads its built-in image (build/exe_image.c, from
tools/exeimage.py) unless --exe names an EXE.  Each run here plays twice
with the same binary: once from a game directory that holds only
SOURCE1.RSC and SOURCE2.RSC, once with --exe MTV_U.EXE -- in instruction
time (classic) and on the logical clock.  They must agree on:

  * every STATE/PEEK/CALL line and every screen hash;
  * the time and the registers at the end ([quit] line);
  * the whole memory at every checkpoint, byte for byte, except the code
    segments (load segment 01B0 up to DGROUP 3B84), which are empty
    without the EXE: there a byte may only differ where the EXE run has
    the original's code and the run without it has 0 -- what the program
    writes there at run time (start-up variables, the sound stubs) must be
    equal -- and the bytes src/rtl_snd.c writes (371E:0000..0023,
    0063..00C8) must equal the EXE's."""
import os, re, shutil, subprocess, sys, tempfile
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import scenario

ROOT = scenario.ROOT
BIN = os.path.join(ROOT, 'build', 'mtv-dec')
EXE = os.path.join(ROOT, 'build', 'MTV_U.EXE')
LOAD = 0x01B0 * 16
CODE = (LOAD, LOAD + 0x39D4 * 16)
SND = LOAD + 0x371E * 16
SND_C = [(SND + 0x00, SND + 0x24), (SND + 0x63, SND + 0xC9)]


def script_of(n):
    return os.path.join(ROOT, 'tests', 'session1.txt') if n == 'session1' else os.path.join(scenario.SCEN, n + '.txt')


def play(n, with_exe, logical, tmp):
    """-> (output lines, [quit] line, {frame: dump path})"""
    d = tempfile.mkdtemp(prefix='game_', dir=tmp)
    for f in ('SOURCE1.RSC', 'SOURCE2.RSC'):
        shutil.copy(os.path.join(scenario.GAME, f), d)
    tag = ('exe' if with_exe else 'free') + ('_lt' if logical else '')
    lines, dumps = [], {}
    for l in open(script_of(n)).read().split('\n'):
        lines.append(l)
        f = l.split()
        if len(f) >= 2 and f[1] == 'hash':
            dumps[f[0]] = os.path.join(tmp, '%s_%s.bin' % (tag, f[0]))
            lines.append('%s dump %s' % (f[0], dumps[f[0]]))
    s = os.path.join(tmp, tag + '.txt')
    open(s, 'w').write('\n'.join(lines))
    args = [BIN, '--headless', '--game', d, '--script', s]
    if n != 'session1':
        args += ['--nosound', '--args', scenario.lang_of(open(script_of(n)).read().split('\n'))]
    if with_exe:
        args += ['--exe', EXE]
    if logical:
        args.append('--logical-time')
    p = subprocess.run(args, capture_output=True, text=True)
    load = [l for l in p.stderr.split('\n') if l.startswith('[load]')]
    if (not with_exe) != bool(load and 'built in' in load[0]):
        return None, 'wrong image loaded: %s' % (load[:1] or ['no [load] line']), {}
    quit_ = [l for l in p.stderr.split('\n') if l.startswith('[quit]')]
    out = [re.sub(r' mem [0-9a-f]+', '', l) for l in p.stdout.split('\n')
           if l.startswith(('HASH', 'STATE', 'PEEK', 'CALL'))]
    return out, (quit_[0] if quit_ else 'no [quit] line, exit %d: %s' % (p.returncode, p.stderr[-300:])), dumps


def compare(n, logical):
    tmp = tempfile.mkdtemp(prefix='exefree_')
    try:
        a = play(n, True, logical, tmp)
        b = play(n, False, logical, tmp)
        if a[0] is None or b[0] is None:
            return a[1] if a[0] is None else b[1]
        if a[0] != b[0]:
            k = next(i for i in range(min(len(a[0]), len(b[0])) + 1)
                     if i == min(len(a[0]), len(b[0])) or a[0][i] != b[0][i])
            return 'output differs at line %d: %s / %s' % (k, a[0][k:k + 1], b[0][k:k + 1])
        if a[1] != b[1]:
            return 'end differs: %s / %s' % (a[1], b[1])
        for fr in a[2]:
            ea, eb = os.path.exists(a[2][fr]), os.path.exists(b[2][fr])
            if ea != eb:
                return 'frame %s: a memory dump %s' % (fr, 'only with the EXE' if ea else 'only without it')
            if not ea:                  # the game ended before (an ending exits)
                continue
            x, y = open(a[2][fr], 'rb').read(), open(b[2][fr], 'rb').read()
            if x == y:
                continue
            for i in range(len(x)):
                if x[i] == y[i]:
                    continue
                if CODE[0] <= i < CODE[1] and y[i] == 0 and not any(lo <= i < hi for lo, hi in SND_C):
                    continue
                return 'memory differs at frame %s, %05X: %02X with the EXE, %02X without' % (fr, i, x[i], y[i])
        return ''
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def main():
    names = sys.argv[1:] or ['session1'] + sorted(f[:-4] for f in os.listdir(scenario.SCEN) if f.endswith('.txt'))
    jobs = [(n, lt) for n in names for lt in (False, True)]
    with ThreadPoolExecutor(os.cpu_count() or 4) as ex:
        res = list(ex.map(lambda j: compare(*j), jobs))
    bad = 0
    for (n, lt), r in zip(jobs, res):
        if r:
            bad += 1
            print('FAIL %-28s %s  %s' % (n, 'logical' if lt else 'classic', r))
    print('%d of %d runs the same game without MTV.EXE (%d tests, classic and logical clock)' %
          (len(jobs) - bad, len(jobs), len(names)))
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
