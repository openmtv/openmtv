#!/usr/bin/env python3
"""Recorded play sessions as regression tests (tests/scenarios/).

    scenario.py add RECORDING NAME   recording (./play.sh --record) -> test
    scenario.py check [--coverage FILE] [NAME ...]
                                      replay all (or the named) scenarios
    scenario.py update [NAME ...]     re-record the expected hashes after a
                                      deliberate runtime change (prints what changed)

add: inserts a hash checkpoint every 2000 frames and at the end, then runs
the machine-level build (original timing) on fresh game files and stores
what it prints -- memory/screen hashes and the values of the script's
peek/call lines -- as tests/scenarios/NAME.txt + NAME.expected.

check also writes build/scenario_coverage.txt: every decompiled function that
passed the difftest, with the scenarios that exercised it.

A scenario may end its difftest run early with '# difftest-until FRAME' (the
machine-level hashes still cover the whole script) when the difftest finds
nothing new after that frame -- measure it with DIFFTEST_VERBOSE=1.

check, per scenario:
  1. build/mtv must reproduce NAME.expected exactly (machine-level C ==
     original program),
  2. build/mtv-dec --interp --difftest: every decompiled function called
     along the way must match the original (0 failing functions), and the
     run itself must print NAME.expected too (up to a difftest-until cut):
     the difftest rewinds after each test, so it plays the recorded game;
  3. the native decompiled build (no interpreter) must play the same game as
     the machine-level build when both run on the logical clock
     (--logical-time, runtime.h): same game state (STATE: data segment below
     the stack + far tables), screen and peek/call values at every
     checkpoint, up to a '# logical-until FRAME' cut -- where the original
     reads stack garbage the decompiled build cannot reproduce (the header
     says which).
Game files come from $GAME (default: the directory above decomp/).
"""
import os, re, sys, shutil, subprocess, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..'))
SCEN = os.path.join(ROOT, 'tests', 'scenarios')
GAME = os.environ.get('GAME', os.path.join(ROOT, '..'))
STEP = 2000
KEEP = ('HASH', 'PEEK', 'CALL')     # the output lines a scenario expects


def kept(text):
    return [l for l in text.split('\n') if l.startswith(KEEP)]


def result_words():
    """words of the result per decompiled function (src/game_table.c dec_ret:
    0 = none, 16 = AX, 32 = DX:AX), keyed 'SEG:OFF'"""
    t = open(os.path.join(ROOT, 'src', 'game_table.c')).read()
    keys = re.findall(r'0x([0-9A-F]{8})', t.split('dec_keys[] = {')[1].split('};')[0])
    rets = t.split('dec_ret[] = {')[1].split('};')[0].split(', ')
    return {'%s:%s' % (k[:4], k[4:]): int(r) // 16 for k, r in zip(keys, rets)}


def frame_of(line):
    return int(line.split()[2])     # "HASH|PEEK|CALL frame N ..."


def first_diff(got, expected):
    """'' if equal, else the first differing line pair, readable."""
    for a, b in zip(got, expected):
        if a != b:
            return 'expected "%s", got "%s"' % (b, a)
    if len(got) != len(expected):
        return '%d lines expected, %d printed' % (len(expected), len(got))
    return ''


def fresh_game():
    d = tempfile.mkdtemp(prefix='mtv_scen_')
    for f in ('SOURCE1.RSC', 'SOURCE2.RSC'):
        shutil.copy(os.path.join(GAME, f), d)
    shutil.copy(os.path.join(ROOT, 'build', 'MTV_U.EXE'), d)
    return d


def run(binary, script, extra=(), lang='e', coverage=None):
    d = fresh_game()
    try:
        env = dict(os.environ)
        if coverage:
            env['DIFFTEST_COVERAGE'] = coverage
        p = subprocess.run([os.path.join(ROOT, binary), '--headless', '--nosound', '--game', d,
                            '--args', lang, '--script', script] + list(extra),
                           capture_output=True, text=True, env=env)
        return p.stdout, p.stderr
    finally:
        shutil.rmtree(d, ignore_errors=True)


def lang_of(lines):
    for l in lines:
        m = re.search(r'--args "?([de])', l)
        if m:
            return m.group(1)
    return 'e'


def add(rec, name):
    lines = open(rec).read().split('\n')
    body = [l for l in lines if l.strip() and not l.startswith('#')]
    frames = [int(l.split()[0]) for l in body]
    end = (max(frames) if frames else 0) + 2000
    for l in lines:
        m = re.match(r'#\s*end\s+(\d+)', l)
        if m:
            end = int(m.group(1))
    hashes = ['%d hash' % f for f in range(STEP, end, STEP)] + ['%d hash' % end, '%d quit' % (end + 1)]
    out = [l for l in lines if l.startswith('#')] + sorted(body + hashes, key=lambda l: int(l.split()[0]))
    os.makedirs(SCEN, exist_ok=True)
    path = os.path.join(SCEN, name + '.txt')
    open(path, 'w').write('\n'.join(out) + '\n')
    stdout, _ = run('build/mtv', path, lang=lang_of(lines))
    got = kept(stdout)
    open(os.path.join(SCEN, name + '.expected'), 'w').write('\n'.join(got) + '\n')
    print('%s: %d frames, %d checkpoints, %d values' % (
        name, end, sum(l.startswith('HASH') for l in got), sum(not l.startswith('HASH') for l in got)))


def update(names):
    """Re-record the expected hashes of existing scenarios with build/mtv
    (after a deliberate runtime change); reports what changed."""
    if not names:
        names = sorted(f[:-4] for f in os.listdir(SCEN) if f.endswith('.txt'))
    for name in names:
        path = os.path.join(SCEN, name + '.txt')
        exp = os.path.join(SCEN, name + '.expected')
        oldl = kept(open(exp).read())
        stdout, _ = run('build/mtv', path, lang=lang_of(open(path).read().split('\n')))
        got = kept(stdout)
        old = [l.split() for l in oldl if l.startswith('HASH')]
        new = [l.split() for l in got if l.startswith('HASH')]
        mem = sum(1 for a, b in zip(old, new) if a[4] != b[4])
        vram = sum(1 for a, b in zip(old, new) if a[6] != b[6])
        vals = [l for l in got if not l.startswith('HASH')]
        oldv = [l for l in oldl if not l.startswith('HASH')]
        open(exp, 'w').write('\n'.join(got) + '\n')
        print('%-24s %d checkpoints (was %d): memory hash changed at %d, screen hash at %d; '
              '%d values (was %d), %d changed' % (
            name, len(new), len(old), mem, vram, len(vals), len(oldv),
            sum(1 for a, b in zip(oldv, vals) if a != b)))


def check(names):
    """All runs at once (one process each, the long difftests first); the
    report keeps the scenario order.  '--coverage FILE' in names: where the
    coverage list goes (default build/scenario_coverage.txt)."""
    covfile = os.path.join(ROOT, 'build', 'scenario_coverage.txt')
    if '--coverage' in names:
        i = names.index('--coverage')
        covfile = os.path.abspath(names[i + 1])
        names = names[:i] + names[i + 2:]
    if not names:
        names = sorted(f[:-4] for f in os.listdir(SCEN) if f.endswith('.txt')) if os.path.isdir(SCEN) else []
    if not names:
        print('no scenarios in tests/scenarios/')
        sys.exit(0)
    from concurrent.futures import ThreadPoolExecutor
    info, lcut = {}, {}
    for name in names:
        path = os.path.join(SCEN, name + '.txt')
        lines = open(path).read().split('\n')
        frames = [int(l.split()[0]) for l in lines if l.strip() and not l.startswith('#')]
        cut = None          # '# difftest-until FRAME': end the difftest run there
        for l in lines:
            m = re.match(r'#\s*difftest-until\s+(\d+)', l)
            if m:
                cut = int(m.group(1))
            m = re.match(r'#\s*logical-until\s+(\d+)', l)
            if m:
                lcut[name] = int(m.group(1))
        info[name] = (path, lang_of(lines), max(frames) if frames else 0, cut)
    jobs = [(name, kind) for name in names for kind in ('dec', 'mtv', 'lmtv', 'ldec')]
    jobs.sort(key=lambda j: (j[1] != 'dec', -(info[j[0]][3] or info[j[0]][2])))   # longest difftests first
    tmp = tempfile.mkdtemp(prefix='mtv_scen_cut_')
    with ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as ex:
        fut = {}
        for name, kind in jobs:
            path, lang, _, cut = info[name]
            if kind == 'mtv':
                fut[name, kind] = ex.submit(run, 'build/mtv', path, (), lang)
            elif kind in ('lmtv', 'ldec'):
                fut[name, kind] = ex.submit(run, 'build/mtv' if kind == 'lmtv' else 'build/mtv-dec', path,
                                            ['--logical-time'], lang)
            else:
                if cut:
                    body = [l for l in open(path).read().split('\n')
                            if l.strip() and not l.startswith('#') and int(l.split()[0]) < cut]
                    path = os.path.join(tmp, name + '.txt')
                    open(path, 'w').write('\n'.join(body + ['%d quit' % cut]) + '\n')
                fut[name, kind] = ex.submit(run, 'build/mtv-dec', path, ['--interp', '--difftest'], lang,
                                            os.path.join(tmp, name + '.cov'))
        bad = 0
        for name in names:
            expected = kept(open(os.path.join(SCEN, name + '.expected')).read())
            stdout, _ = fut[name, 'mtv'].result()
            why1 = first_diff(kept(stdout), expected)
            ok1 = not why1
            dstdout, stderr = fut[name, 'dec'].result()
            cut = info[name][3]
            why3 = first_diff(kept(dstdout), [l for l in expected if not cut or frame_of(l) < cut])
            log = os.path.join(ROOT, 'build', 'scenario_%s.err' % name)
            open(log, 'w').write(stderr)
            blame = subprocess.run([sys.executable, os.path.join(HERE, 'dtblame.py'), log],
                                   capture_output=True, text=True).stdout
            ok2 = blame.startswith('0 failing')
            tests = re.findall(r'\[difftest\] (\d+) tests', stderr)
            undef = sorted(set(re.findall(r'never-written local at ([0-9A-F]{4}:[0-9A-F]{4})', stderr)))
            print('%-24s machine-level %s (%d lines), decompiled %s (%s difftests%s)%s' % (
                name, 'ok' if ok1 else 'DIFFERS: ' + why1, len(expected),
                'ok' if ok2 else 'FAILS: ' + blame.split('\n')[0],
                tests[-1] if tests else '?',
                '; original reads stack garbage at ' + ', '.join(undef) if undef else '',
                '' if not why3 else '; difftest run LEFT THE RECORDING: ' + why3))
            sys.stdout.flush()
            # 3. native decompiled == machine-level, both on the logical clock
            def logical(text):
                out = []
                for l in text.split('\n'):
                    if not l.startswith(('STATE', 'PEEK', 'CALL')):
                        continue
                    if l.startswith('CALL'):
                        # only the words the function returns: the rest of AX/DX
                        # is whatever the code left there (an unknown function: AX)
                        p = l.split()
                        n = rwords.get(p[3].split(',')[0], 1)
                        l = ' '.join(p[:4] + p[4:4 + n])
                    if name in lcut and frame_of(l) >= lcut[name]:
                        continue
                    out.append(l)
                return out
            rwords = result_words()
            lref = logical(fut[name, 'lmtv'].result()[0])
            why4 = first_diff(logical(fut[name, 'ldec'].result()[0]), lref)
            print('%-24s native decompiled on the logical clock: %s' % (
                '', ('same game (%d lines%s)' % (len(lref), ', until frame %d' % lcut[name] if name in lcut else ''))
                if not why4 else 'DIFFERS: ' + why4))
            sys.stdout.flush()
            bad += (not ok1) + (not ok2) + bool(why3) + bool(why4)
    # which decompiled functions the difftests passed, per scenario
    cov = {}
    for name in names:
        try:
            for l in open(os.path.join(tmp, name + '.cov')):
                cov.setdefault(l.split()[0], []).append(name)
        except OSError:
            pass
    with open(covfile, 'w') as f:
        f.write('# decompiled functions passing the difftest in tests/scenarios (scenario.py check)\n')
        for fn in sorted(cov):
            f.write('%s %s\n' % (fn, ' '.join(cov[fn])))
    print('difftest coverage of these scenarios: %d functions (%s)' % (len(cov), os.path.relpath(covfile, ROOT)))
    shutil.rmtree(tmp, ignore_errors=True)
    sys.exit(1 if bad else 0)

if __name__ == '__main__':
    if len(sys.argv) >= 4 and sys.argv[1] == 'add':
        add(sys.argv[2], sys.argv[3])
    elif len(sys.argv) >= 2 and sys.argv[1] == 'check':
        check(sys.argv[2:])
    elif len(sys.argv) >= 2 and sys.argv[1] == 'update':
        update(sys.argv[2:])
    else:
        print(__doc__)
        sys.exit(2)
