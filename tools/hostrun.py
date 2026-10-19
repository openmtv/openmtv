#!/usr/bin/env python3
"""The same game on every system: what the decompiled build computes for each
test script, to compare between Linux, macOS and Windows (README, Windows).

    python3 tools/hostrun.py run OUT [--bin FILE] [--game DIR] [--jobs N]
    python3 tools/hostrun.py compare A B

run plays tests/session1.txt and every scenario (tests/scenarios/, with
--nosound and the language of its header, like tools/scenario.py) in
instruction time and on the logical clock (--logical-time), and the runs of
every fix test (tests/fixes/, as tools/fixtest.py makes and judges them),
each on fresh copies of SOURCE1.RSC and SOURCE2.RSC alone -- from --game DIR
(default: $GAME, else the directory above decomp/, like the tests).  The
binary: --bin, default build/mtv-dec (build/mtv-dec.exe on Windows).  For each
run OUT/NAME.MODE.txt holds the HASH, STATE, PEEK and CALL lines and how the
run ended (the [quit] line with the time and the registers, a divide error,
the exit code).  compare reports the files that differ (line ends aside) and
exits 1 if any does.  Standard library only (also on Windows)."""
import os
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..'))
sys.path.insert(0, HERE)
import fixtest  # noqa: E402
import scenario  # noqa: E402

KEEP = ('HASH', 'STATE', 'PEEK', 'CALL')
END = ('[quit]', '  CS=', '[script] quit', '[DOS] program exit', 'Divide error')


def default_bin():
    return os.path.join(ROOT, 'build', 'mtv-dec.exe' if os.name == 'nt' else 'mtv-dec')


def play(binary, game, args, script_lines=None, script=None):
    """the kept lines of one run on fresh game files"""
    d = tempfile.mkdtemp(prefix='mtv_host_')
    try:
        for f in ('SOURCE1.RSC', 'SOURCE2.RSC'):
            shutil.copy(os.path.join(game, f), d)
        if script_lines is not None:
            script = os.path.join(d, 'test.txt')
            with open(script, 'w') as f:
                f.write('\n'.join(script_lines) + '\n')
        p = subprocess.run([binary, '--headless', '--game', d, '--script', script] + args,
                           capture_output=True, text=True, errors='replace')
        out = [l for l in p.stdout.splitlines() if l.startswith(KEEP)]
        out += [l for l in p.stderr.splitlines() if l.startswith(END)]
        return out + ['EXIT %d' % p.returncode]
    finally:
        shutil.rmtree(d, ignore_errors=True)


def jobs_of():
    """[(file name, args, script lines or None, script path or None, fix test or None)]"""
    jobs = []
    s1 = os.path.join(ROOT, 'tests', 'session1.txt')
    for clock, extra in (('instr', []), ('logical', ['--logical-time'])):
        jobs.append(('session1.%s' % clock, extra, None, s1, None))
        for n in sorted(f[:-4] for f in os.listdir(scenario.SCEN) if f.endswith('.txt')):
            path = os.path.join(scenario.SCEN, n + '.txt')
            lang = scenario.lang_of(open(path).read().split('\n'))
            jobs.append(('%s.%s' % (n, clock), ['--nosound', '--args', lang] + extra, None, path, None))
    for n in sorted(f[:-4] for f in os.listdir(fixtest.TESTS) if f.endswith('.txt')):
        for mode, mine, lines, extra in fixtest.plan(n) or []:
            jobs.append(('fix_%s.%s' % (n, mode), ['--fast', '--nosound', '--args', 'e'] + extra, lines, None, (n, mode, mine)))
    return jobs


def run(out, binary, game, njobs):
    os.makedirs(out, exist_ok=True)
    jobs = jobs_of()
    with ThreadPoolExecutor(njobs) as ex:
        results = list(ex.map(lambda j: play(binary, game, j[1], j[2], j[3]), jobs))
    fixes = {}
    for (name, _, _, _, fix), lines in zip(jobs, results):
        with open(os.path.join(out, name + '.txt'), 'w', newline='\n') as f:
            f.write('\n'.join(lines) + '\n')
        if fix:
            fixes.setdefault(fix[0], []).append((fix[1], fix[2], '\n'.join(lines)))
    bad = sum(not fixtest.judge(n, r) for n, r in fixes.items())
    print('%d runs in %s; fix tests: %d of %d fail' % (len(jobs), out, bad, len(fixes)))
    return 1 if bad else 0


def compare(a, b):
    names = sorted(set(os.listdir(a)) | set(os.listdir(b)))
    bad = 0
    for n in names:
        pa, pb = os.path.join(a, n), os.path.join(b, n)
        if not (os.path.exists(pa) and os.path.exists(pb)):
            print('%-40s only in %s' % (n, a if os.path.exists(pa) else b))
            bad += 1
            continue
        la, lb = open(pa).read().splitlines(), open(pb).read().splitlines()
        if la != lb:
            bad += 1
            first = next((i for i, (x, y) in enumerate(zip(la, lb)) if x != y), min(len(la), len(lb)))
            print('%-40s DIFFERS at line %d: %r / %r' % (n, first + 1, la[first] if first < len(la) else None,
                                                         lb[first] if first < len(lb) else None))
    print('%d files, %d differ' % (len(names), bad))
    return 1 if bad else 0


def main():
    a = sys.argv[1:]
    opt = {'--bin': default_bin(), '--game': scenario.GAME, '--jobs': str(os.cpu_count() or 4)}
    rest = []
    while a:
        x = a.pop(0)
        if x in opt and a:
            opt[x] = a.pop(0)
        else:
            rest.append(x)
    if rest[:1] == ['run'] and len(rest) == 2:
        return run(rest[1], os.path.abspath(opt['--bin']), opt['--game'], int(opt['--jobs']))
    if rest[:1] == ['compare'] and len(rest) == 3:
        return compare(rest[1], rest[2])
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main())
