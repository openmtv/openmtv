#!/usr/bin/env python3
"""Tests of the original-bug fixes (tests/fixes/).

    fixtest.py [NAME ...]      run all (or the named) tests

Each tests/fixes/NAME.txt is an input script (like tests/scenarios/) with
header lines that name the fix (or a game option) and state what must hold:

    # fix: intruder
    # expect classic 215000 word 39D4:71E4 == 2    (the game hangs on day 2)
    # expect fixed   215000 word 39D4:71E4 >= 4
    # expect classic 3000 call 0D5E:0229,0,1 halted

    expect MODE FRAME byte|word SEG:OFF  OP VALUE | halted
    expect MODE FRAME call SEG:OFF,ARG,...  OP VALUE | halted
    expect MODE FRAME seat S byte|word SEG:OFF  OP VALUE
      MODE   classic (no fixes) or fixed (--fixes=<the named fix> only)
             or option (the options of a "# option: --start-money=N" line:
             a player's choice, not a fix)
      byte|word  game memory at SEG:OFF; call: AX returned by the function
      seat S another seat's state (a network game's test: --humans in its option line)
      OP     == != < <= > >=          VALUE decimal or 0x hex
      halted the program stopped before that frame (crash, divide error)
      SEG    relative to the load segment (39D4 = DGROUP)
    # args: --logical-time      more options for every run of the test (all modes)

The decompiled build runs the script twice on fresh game files: classic must
show the bug, the fix must remove it.  Game files come from $GAME (default:
the directory above decomp/).
"""
import os, re, sys, shutil, subprocess, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..'))
TESTS = os.path.join(ROOT, 'tests', 'fixes')
GAME = os.environ.get('GAME', os.path.join(ROOT, '..'))
OPS = {'==': lambda a, b: a == b, '!=': lambda a, b: a != b, '<': lambda a, b: a < b,
       '<=': lambda a, b: a <= b, '>': lambda a, b: a > b, '>=': lambda a, b: a >= b}
EXPECT = re.compile(r'#\s*expect\s+(classic|fixed|option)\s+(\d+)\s+(?:seat\s+(\d+)\s+)?(byte|word|call)\s+([0-9A-Fa-f]{4}:[0-9A-Fa-f]{4}\S*)\s+'
                    r'(?:(==|!=|<=|>=|<|>)\s+(0x[0-9A-Fa-f]+|-?\d+)|(halted))')


def parse(path):
    fix, option, args, expects, body = None, None, [], [], []
    for line in open(path).read().split('\n'):
        m = re.match(r'#\s*fix:\s*(\S+)', line)
        if m:
            fix = m.group(1)
        m = re.match(r'#\s*args:\s*(.+)', line)
        if m:
            args = m.group(1).split()
        m = re.match(r'#\s*option:\s*(.+)', line)
        if m:
            option = m.group(1).split()
        m = EXPECT.match(line)
        if m:
            mode, frame, seat, kind, target, op, val, halted = m.groups()
            expects.append((mode, int(frame), kind, target.upper(), 'halted' if halted else op,
                            None if halted else int(val, 0), int(seat or 0)))
        if line.strip() and not line.startswith('#'):
            body.append(line)
    return fix, option, args, expects, body


def run(script_lines, extra):
    d = tempfile.mkdtemp(prefix='mtv_fix_')
    try:
        for f in ('SOURCE1.RSC', 'SOURCE2.RSC'):
            shutil.copy(os.path.join(GAME, f), d)
        shutil.copy(os.path.join(ROOT, 'build', 'MTV_U.EXE'), d)
        script = os.path.join(d, 'test.txt')
        open(script, 'w').write('\n'.join(script_lines) + '\n')
        p = subprocess.run([os.path.join(ROOT, 'build', 'mtv-dec'), '--headless', '--fast', '--nosound',
                            '--game', d, '--args', 'e', '--script', script] + extra,
                           capture_output=True, text=True)
        return p.stdout
    finally:
        shutil.rmtree(d, ignore_errors=True)


def plan(name):
    """The runs a test needs: [(mode, expectations, script lines, extra args)]."""
    fix, option, args, expects, body = parse(os.path.join(TESTS, name + '.txt'))
    if not (fix or option) or not expects:
        return None
    runs = []
    extra = {'classic': [], 'fixed': ['--fixes=%s' % fix] if fix else None, 'option': option}
    for mode in ('classic', 'fixed', 'option'):
        mine = [e for e in expects if e[0] == mode]
        if not mine:
            continue
        peeks = ['%d %s%s %s' % (e[1], 'seat %d ' % e[6] if e[6] else '', {'byte': 'peek8', 'word': 'peek16', 'call': 'call'}[e[2]], e[3]) for e in mine]
        end = max(e[1] for e in mine) + 1
        lines = sorted(body + peeks + ['%d quit' % end], key=lambda l: int(l.split()[0]))
        if extra[mode] is None:
            raise SystemExit('%s: "expect %s" needs a "# %s:" line' % (name, mode, 'fix' if mode == 'fixed' else 'option'))
        runs.append((mode, mine, lines, extra[mode] + args))
    return runs


def judge(name, results):
    """results: [(mode, expectations, program output)] -> print, return ok"""
    ok = True
    notes = []
    for mode, mine, out in results:
        got = {}
        for l in out.split('\n'):
            m = re.match(r'(PEEK|CALL) (?:seat (\d+) )?frame (\d+) (\S+) (\d+)', l)
            if m:
                got[(int(m.group(3)), m.group(1), m.group(4).upper(), int(m.group(2) or 0))] = int(m.group(5))
        for e in mine:
            key = (e[1], 'CALL' if e[2] == 'call' else 'PEEK', e[3], e[6])
            v = got.get(key)
            good = v is None if e[4] == 'halted' else (v is not None and OPS[e[4]](v, e[5]))
            ok &= good
            notes.append('%s %s%s@%d=%s %s %s %s' % (mode, 'seat %d ' % e[6] if e[6] else '', e[3], e[1], 'halted' if v is None else v, e[4],
                                                   '' if e[5] is None else e[5], 'ok' if good else 'FAIL'))
    print('%-22s %s  (%s)' % (name, 'ok' if ok else 'FAILS', '; '.join(notes)))
    sys.stdout.flush()
    return ok


def main():
    """All runs of all tests at once (one process each); report in name order."""
    from concurrent.futures import ThreadPoolExecutor
    names = sys.argv[1:] or sorted(f[:-4] for f in os.listdir(TESTS) if f.endswith('.txt'))
    plans = {n: plan(n) for n in names}
    bad = 0
    with ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as ex:
        fut = {n: [(mode, mine, ex.submit(run, lines, extra)) for mode, mine, lines, extra in p]
               for n, p in plans.items() if p}
        for n in names:
            if not plans[n]:
                print('%-22s no "# fix:"/"# option:" or "# expect" lines' % n)
                bad += 1
                continue
            bad += not judge(n, [(mode, mine, f.result()) for mode, mine, f in fut[n]])
    sys.exit(1 if bad else 0)

if __name__ == '__main__':
    main()
