#!/usr/bin/env python3
"""Native decompiled build, classic (instruction-timed) mode: does a change
keep every run bit-identical?  For rewrites that must not change behaviour
(ROADMAP Phase 3: run-time library in C, platform interface, ...).

    python3 tools/nativebase.py record     before the change
    python3 tools/nativebase.py check      after it

record runs build/mtv-dec on tests/session1.txt and every scenario and keeps
the HASH/STATE/PEEK/CALL lines in build/native_base/, plus a copy of the
binary.  check runs them again and compares.  Where only a HASH memory value
differs (game state, screen and peeks equal), it dumps memory at those
frames with both binaries and compares byte by byte, ignoring the stacks:
code rewritten in C does not push frames, so interrupt frames written below
the stack pointers hold other register values -- the main stack (SS 45F7,
which the HASH line already leaves out) and the sound driver's interrupt
stack (segment 4828 below its SP 07C4: the driver, loaded at run time, sets
SS = CS there).  And the code segments (load segment 01B0 up to DGROUP),
which the decompiled build leaves empty when it runs without MTV.EXE
(tools/exeimage.py): there a byte may differ where one run has 0 -- except
the bytes src/rtl_snd.c writes in 371E.  Any other differing byte is a
failure."""
import os, shutil, subprocess, sys, tempfile
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import scenario

BASE = os.path.join(scenario.ROOT, 'build', 'native_base')
SKIP = [(0x45F70, 0x47F70), (0x48280 + 0x600, 0x48280 + 0x7C4)]
CODE = (0x1B00, 0x1B00 + 0x39D40)                       # empty without the EXE
SND_C = [(0x38CE0, 0x38CE0 + 0x24), (0x38CE0 + 0x63, 0x38CE0 + 0xC9)]   # 371E, written by src/rtl_snd.c


def same_but_code(i, x, y):
    """byte i differs only because one run has no EXE code there"""
    return CODE[0] <= i < CODE[1] and (x == 0 or y == 0) and not any(lo <= i < hi for lo, hi in SND_C)


def script_of(n):
    return os.path.join(scenario.ROOT, 'tests', 'session1.txt') if n == 'session1' else os.path.join(scenario.SCEN, n + '.txt')


def run(n, binary, script=None):
    script = script or script_of(n)
    if n == 'session1':
        d = scenario.fresh_game()
        try:
            out = subprocess.run([binary, '--headless', '--game', d, '--script', script],
                                 capture_output=True, text=True).stdout
        finally:
            shutil.rmtree(d, ignore_errors=True)
    else:
        out, _ = scenario.run(binary, script, lang=scenario.lang_of(open(script_of(n)).read().split('\n')))
    return [l for l in out.split('\n') if l.startswith(('HASH', 'STATE', 'PEEK', 'CALL'))]


def dumps_differ(n, frames, new, old):
    """bytes outside SKIP that differ at FRAMES between the two binaries"""
    tmp = tempfile.mkdtemp(prefix='nativebase_')
    try:
        for tag, binary in (('new', new), ('old', old)):
            out = []
            for l in open(script_of(n)).read().split('\n'):
                out.append(l)
                f = l.split()
                if len(f) >= 2 and f[1] == 'hash' and f[0] in frames:
                    out.append('%s dump %s/%s_%s.bin' % (f[0], tmp, tag, f[0]))
            s = os.path.join(tmp, tag + '.txt')
            open(s, 'w').write('\n'.join(out))
            run(n, binary, s)
        res = []
        for fr in frames:
            a = open('%s/new_%s.bin' % (tmp, fr), 'rb').read()
            b = open('%s/old_%s.bin' % (tmp, fr), 'rb').read()
            res.append((fr, [i for i in range(0x500, 0xA0000)
                             if a[i] != b[i] and not any(lo <= i < hi for lo, hi in SKIP)
                             and not same_but_code(i, a[i], b[i])]))
        return res
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def check_one(n, new, old):
    path = os.path.join(BASE, n + '.txt')
    if not os.path.exists(path):        # a scenario added after the last record
        return False, 'no baseline (record it with a build that passed the check)'
    lines = run(n, new)
    want = open(path).read().split('\n')[:-1]
    if len(lines) != len(want):
        return False, '%d lines expected, %d printed' % (len(want), len(lines))
    memonly = []
    for a, b in zip(lines, want):
        if a == b:
            continue
        if a.startswith('HASH') and a.split()[5:] == b.split()[5:]:
            memonly.append(a.split()[2])
            continue
        return False, 'expected "%s", got "%s"' % (b, a)
    why = ['frame %s: %d bytes differ outside the stacks, first %s' % (fr, len(bad), ' '.join('%05X' % x for x in bad[:6]))
           for fr, bad in dumps_differ(n, memonly, new, old) if bad] if memonly else []
    if why:
        return False, '; '.join(why)
    return True, ('same (stack scratch only at %d checkpoints)' % len(memonly)) if memonly else 'same'


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else 'check'
    new = os.path.join(scenario.ROOT, 'build', 'mtv-dec')
    old = os.path.join(BASE, 'mtv-dec')
    names = ['session1'] + sorted(f[:-4] for f in os.listdir(scenario.SCEN) if f.endswith('.txt'))
    if mode == 'record':
        os.makedirs(BASE, exist_ok=True)
        shutil.copy(new, old)
        with ThreadPoolExecutor(6) as ex:
            for n, lines in zip(names, ex.map(lambda n: run(n, old), names)):
                open(os.path.join(BASE, n + '.txt'), 'w').write('\n'.join(lines) + '\n')
        print('recorded %d runs in %s' % (len(names), BASE))
        return 0
    bad = 0
    with ThreadPoolExecutor(6) as ex:
        for n, (ok, why) in zip(names, ex.map(lambda n: check_one(n, new, old), names)):
            if not ok:
                bad += 1
            if not ok or why != 'same':
                print('%-24s %s%s' % (n, '' if ok else 'DIFFERS: ', why[:200]))
    print('%d runs, %d differ' % (len(names), bad))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
