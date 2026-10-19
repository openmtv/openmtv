#!/usr/bin/env python3
"""Inventory for the multiplayer plan (docs/multiplayer.md, stage 0): which
game variable is touched by which function, from which part of the frame.

    python3 tools/mpinventory.py trace OUT     run session1, every scenario and
                                               the computer-player day with
                                               build/mtv-dec-trace (make it first)
    python3 tools/mpinventory.py report OUT    OUT/inventory.json + OUT/inventory.md

The trace build (tools/dgtrace.c) records every access to the data segment
with the decompiled function making it and its context: "W:root" when a
world root runs (computer players, intruder, elevator, figures, the game
clock: once per frame for every station), "S:root" for the innermost seat
root (input, clicks, rooms, drawing: what one human does), "M" otherwise
(start-up, game_main itself).  The report adds the names (src/game_globals.h,
src/game_types.h, names/symbols.txt) and, from the source, who writes a
variable (an assignment, ++/--), and sorts the variables by where they are
touched: only by seats (a seat's own state), only by the world, or both --
the list a seat implementation has to decide on one by one."""
import json, os, re, shutil, subprocess, sys, tempfile
from collections import defaultdict
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import scenario

ROOT = scenario.ROOT
TRACE_BIN = os.path.join(ROOT, 'build', 'mtv-dec-trace')
BULK = {'rtl_startup', 'c0_setargv', 'c0_setenvp', 'c0_save_vectors', 'c0_hook'}   # start-up: clears and fills everything
ranges = []
ASSIGN = re.compile(r'\s*(=(?!=)|\+=|-=|\*=|/=|%=|&=|\|=|\^=|<<=|>>=|\+\+|--)')


# ---------------------------------------------------------------- tracing
def runs():
    out = [('session1', os.path.join(ROOT, 'tests', 'session1.txt'), 'e', False),
           ('aiday', os.path.join(ROOT, 'tests', 'aiday.txt'), 'e', True)]
    for f in sorted(os.listdir(scenario.SCEN)):
        if f.endswith('.txt'):
            p = os.path.join(scenario.SCEN, f)
            out.append((f[:-4], p, scenario.lang_of(open(p).read().split('\n')), True))
    return out


def trace_one(job, outdir):
    name, script, lang, nosound = job
    d = tempfile.mkdtemp(prefix='mpinv_')
    try:
        for f in ('SOURCE1.RSC', 'SOURCE2.RSC'):
            shutil.copy(os.path.join(scenario.GAME, f), d)
        args = [TRACE_BIN, '--headless', '--game', d, '--script', script, '--args', lang]
        if nosound:
            args.append('--nosound')
        env = dict(os.environ, DG_TRACE=os.path.join(outdir, name + '.trace'))
        p = subprocess.run(args + ['--fast'], capture_output=True, text=True, env=env)
        return name, p.returncode
    finally:
        shutil.rmtree(d, ignore_errors=True)


def cmd_trace(outdir):
    if not os.path.exists(TRACE_BIN):
        sys.exit('make build/mtv-dec-trace first')
    os.makedirs(outdir, exist_ok=True)
    shutil.copy(TRACE_BIN, os.path.join(outdir, 'mtv-dec-trace'))
    with ThreadPoolExecutor(os.cpu_count() or 4) as ex:
        for name, rc in ex.map(lambda j: trace_one(j, outdir), runs()):
            print('%-28s exit %d' % (name, rc))


# ---------------------------------------------------------------- names
def anchors():
    """[(offset, label, description, width or stride, kind, fields)] sorted"""
    out = []
    g = open(os.path.join(ROOT, 'src', 'game_globals.h')).read() + open(os.path.join(ROOT, 'src', 'game_types.h')).read()
    for m in re.finditer(r'^#define\s+(\w+)(\([^)]*\))?\s+G(8|16|32)\((?:\(u16\)\()?\s*(0x[0-9A-Fa-f]+)([^\n]*)', g, re.M):
        name, args, w, off, rest = m.groups()
        if re.search(r'_(lo|hi)$', name):
            continue
        d = re.search(r'/\*\s*(.*?)\s*\*/', rest)
        out.append((int(off, 16), name, d.group(1) if d else '', int(w) // 8, 'array' if args else 'var', None))
    syms = open(os.path.join(ROOT, 'names', 'symbols.txt')).read()
    recf = defaultdict(dict)
    for m in re.finditer(r'^rec ([0-9A-F]+) ([0-9A-F]+) ([0-9A-F]+) (\d+) (\w+)\s*(.*)$', syms, re.M):
        recf[int(m.group(1), 16)][int(m.group(3), 16)] = m.group(5)
    t = open(os.path.join(ROOT, 'src', 'game_types.h')).read()
    for m in re.finditer(r'^#define\s+(\w+)\(([^)]*)\)\s+\(\*\(struct (\w+) \*\)dg_addr\(\(u16\)\((.*)\)\)\)', t, re.M):
        name, _, st, expr = m.groups()
        base = int(re.findall(r'0x[0-9A-Fa-f]+', expr)[-1], 16)
        stride = int(re.search(r'\(\w\) \* (0x[0-9A-Fa-f]+)', expr).group(1), 16)
        out.append((base, name, 'struct ' + st, stride, 'table', recf.get(base, {})))
    out.sort(key=lambda a: (a[0], a[4] != 'var'))
    # a table reaches from its base to the first anchor past its first record
    # (record 0's fields are often named as scalars: g_player_money ...)
    ends = {}
    for a in out:
        if a[4] == 'table':
            ends[a[1]] = next((b[0] for b in out if b[0] >= a[0] + a[3]), 0xA730)
    global TABLE_END
    TABLE_END = ends
    return out


TABLE_END = {}


def label_of(off, anc, starts):
    import bisect
    tab = [a for a in anc if a[4] == 'table' and a[0] <= off < TABLE_END[a[1]]]
    if tab:
        a = max(tab, key=lambda a: a[0])
        return a[1], a
    i = bisect.bisect_right(starts, off) - 1
    if i < 0:
        return None, None
    j = i                                   # at one start: an array/table name before a scalar
    while j > 0 and starts[j - 1] == starts[i]:
        j -= 1
    same = [anc[k] for k in range(j, i + 1)]
    a = next((x for x in same if x[4] != 'var'), same[0])
    if a[4] == 'var' and off >= a[0] + a[3]:
        # past a scalar: still it if its description says it is an array
        if re.search(r'\[|array|per player|table', a[2]):
            return a[1], a
        return None, None                   # unnamed data (merged into ranges)
    return a[1], a


def nm_map(binary):
    out = {}
    for l in subprocess.run(['nm', binary], capture_output=True, text=True).stdout.split('\n'):
        f = l.split()
        if len(f) == 3 and f[1] in 'tTwW':
            out[int(f[0], 16)] = f[2]
    return out


# ---------------------------------------------------------------- source
def functions_of_source():
    """{function: text} of src/*.c"""
    out = {}
    for fn in sorted(os.listdir(os.path.join(ROOT, 'src'))):
        if not fn.endswith('.c'):
            continue
        cur, buf = None, []
        for line in open(os.path.join(ROOT, 'src', fn), errors='replace'):
            m = re.match(r'^[A-Za-z_][\w \*]*?\b(\w+)\([^;]*\)\s*\{\s*$', line)
            if m and cur is None:
                cur, buf = m.group(1), []
                continue
            if cur is not None:
                if line.startswith('}'):
                    out[cur] = (fn, ''.join(buf))
                    cur = None
                else:
                    buf.append(line)
    return out


def writes(body, names):
    """names written in body: NAME[(...)][.field] OP  or  ++NAME"""
    w = set()
    for m in re.finditer(r'\b(\w+)\b', body):
        n = m.group(1)
        if n not in names:
            continue
        j = m.end()
        if j < len(body) and body[j] == '(':
            depth = 0
            while j < len(body):
                if body[j] == '(':
                    depth += 1
                elif body[j] == ')':
                    depth -= 1
                    if depth == 0:
                        j += 1
                        break
                j += 1
        f = re.match(r'\.(\w+)', body[j:])
        field = None
        if f:
            field = f.group(1)
            j += f.end()
        if ASSIGN.match(body[j:]) or re.search(r'(\+\+|--)\s*$', body[max(0, m.start() - 4):m.start()]):
            w.add((n, field))
    for m in re.finditer(r'\bG(8|16|32)\(\s*(0x[0-9A-Fa-f]+)\s*\)', body):
        if ASSIGN.match(body[m.end():]):
            w.add(('DS:%04X' % int(m.group(2), 16), None))
    return w


# ---------------------------------------------------------------- report
def cmd_report(outdir):
    binary = os.path.join(outdir, 'mtv-dec-trace')
    syms = nm_map(binary)
    rev = {v: k for k, v in syms.items()}
    anc = anchors()
    starts = [a[0] for a in anc]
    acc = defaultdict(lambda: defaultdict(lambda: defaultdict(int)))   # label -> ctx -> fn -> n
    fields = defaultdict(lambda: defaultdict(set))                      # table -> field -> ctx classes
    fnctx = defaultdict(lambda: defaultdict(int))                       # fn -> ctx -> n
    roots = None
    traced = []
    raw = defaultdict(lambda: defaultdict(lambda: defaultdict(int)))   # unnamed offset -> ctx -> fn -> n
    for f in sorted(os.listdir(outdir)):
        if not f.endswith('.trace'):
            continue
        traced.append(f[:-6])
        lines = open(os.path.join(outdir, f)).read().split('\n')
        base = int(lines[0].split()[1], 16)
        delta = base - rev['dg_trace']
        roots = lines[1].split()[1:]
        for l in lines[3:]:
            p = l.split()
            if len(p) != 4:
                continue
            off, fa, ctx, n = int(p[0], 16), p[1], int(p[2]), int(p[3])
            fn = syms.get(int(fa, 16) - delta, fa) if fa not in ('(nil)', '0') else '-'
            if fn in BULK:
                continue
            c = 'M' if ctx == 0 else roots[ctx - 1]
            label, a = label_of(off, anc, starts)
            if label is None:
                raw[off][c][fn] += n
                fnctx[fn][c] += n
                continue
            acc[label][c][fn] += n
            fnctx[fn][c] += n
            if a and a[4] == 'table':
                k = (off - a[0]) % a[3]
                fname = max(((o, v) for o, v in a[5].items() if o <= k), default=(0, '+%X' % k))[1] if a[5] else '+%X' % k
                fields[label][fname].add(c[0])
    # unnamed bytes: consecutive offsets (gaps up to 2) of the same class as one range
    def cls0(ctxs):
        k = {c[0] for c in ctxs}
        return ''.join(x for x in 'SWM' if x in k)
    cur = None
    for off in sorted(raw):
        k = cls0(raw[off])
        if cur and off <= cur[1] + 3 and cur[2] == k:
            cur[1] = off
        else:
            cur = [off, off, k, []]
            ranges.append(cur)
        cur[3].append(off)
    for lo, hi, k, offs in ranges:
        label = 'DS:%04X' % lo if lo == hi else 'DS:%04X..%04X' % (lo, hi)
        for o in offs:
            for c, f in raw[o].items():
                for fn, n in f.items():
                    acc[label][c][fn] += n
    src = functions_of_source()
    names = {a[1] for a in anc}
    wr = defaultdict(set)                                              # label -> writing functions
    for fn, (_, body) in src.items():
        for n, field in writes(body, names):
            wr[n].add(fn)
    info = {a[1]: a for a in anc}

    def cls(ctxs):
        k = {c[0] for c in ctxs}
        return ''.join(x for x in 'SWM' if x in k)

    rows = []
    for label, ctxs in acc.items():
        a = info.get(label)
        wfn = sorted(wr.get(label, ()))
        rows.append({
            'name': label, 'offset': '%04X' % a[0] if a else label[3:7], 'kind': a[4] if a else 'raw',
            'size': a[3] if a else (int(label[9:13], 16) - int(label[3:7], 16) + 1 if '..' in label else 1),
            'desc': a[2] if a else '(unnamed)', 'class': cls(ctxs),
            'contexts': {c: dict(sorted(f.items(), key=lambda x: -x[1])) for c, f in ctxs.items()},
            'writers': [{'fn': w, 'ctx': cls(fnctx.get(w, {})) or '-'} for w in wfn],
            'fields': {k: ''.join(sorted(v)) for k, v in fields.get(label, {}).items()},
        })
    rows.sort(key=lambda r: int(r['offset'], 16))
    # the g_player branches: function, file, line, the function's contexts
    branches = []
    for fn in sorted(os.listdir(os.path.join(ROOT, 'src'))):
        if fn.endswith('.c'):
            cur = None
            for i, line in enumerate(open(os.path.join(ROOT, 'src', fn), errors='replace'), 1):
                m = re.match(r'^[A-Za-z_][\w \*]*?\b(\w+)\([^;]*\)\s*\{\s*$', line)
                if m:
                    cur = m.group(1)
                for _ in re.finditer(r'[!=]= g_player\b|\bg_player [!=]=', line):
                    branches.append({'file': fn, 'line': i, 'fn': cur, 'ctx': cls(fnctx.get(cur, {})) or 'not run',
                                     'text': line.strip()[:140]})
    data = {'traced': traced, 'roots': roots, 'variables': rows, 'branches': branches,
            'functions': {f: dict(c) for f, c in fnctx.items()}}
    json.dump(data, open(os.path.join(outdir, 'inventory.json'), 'w'), indent=1)
    write_md(data, os.path.join(outdir, 'inventory.md'), src)
    print('%d runs, %d variables/tables touched, %d functions, %d g_player branches -> %s' %
          (len(traced), len(rows), len(fnctx), len(branches), os.path.join(outdir, 'inventory.md')))


def write_md(d, path, src):
    rows = d['variables']
    by = defaultdict(list)
    for r in rows:
        by[r['class']].append(r)
    def fns(r, k):
        out = []
        for c, f in r['contexts'].items():
            if c[0] == k:
                out += list(f)
        seen = []
        for x in out:
            if x not in seen:
                seen.append(x)
        return seen
    L = ['# Inventory (generated by tools/mpinventory.py)', '',
         'Runs: %s.' % ', '.join(d['traced']), '',
         'Classes: S = touched by seat code (input, clicks, rooms, drawing), W = by the world '
         '(computer players, intruder, elevator, figures, clock), M = start-up / game_main itself.', '',
         '| class | variables/tables |', '|---|---|']
    for k in sorted(by):
        L.append('| %s | %d |' % (k, len(by[k])))
    for k, title in (('S', 'Only seats (S, SM): a seat\'s own state'), ('SW', 'Seats and world (SW, SWM): to decide one by one'),
                     ('W', 'Only the world (W, WM)'), ('M', 'Only start-up / main loop (M)')):
        sel = [r for c in by for r in by[c] if c.replace('M', '') == k.replace('M', '') and (k != 'M' or c == 'M')]
        if k == 'M':
            sel = by.get('M', [])
        L += ['', '## ' + title, '', '| DS | name | size | world functions | seat functions | written by | description |',
              '|---|---|---|---|---|---|---|']
        for r in sorted(sel, key=lambda r: int(r['offset'], 16)):
            w = ', '.join('%s(%s)' % (x['fn'], x['ctx']) for x in r['writers'][:6]) + (' …' if len(r['writers']) > 6 else '')
            L.append('| %s | `%s` | %s | %s | %s | %s | %s |' % (
                r['offset'], r['name'], r['size'] or '', ', '.join(fns(r, 'W')[:5]), ', '.join(fns(r, 'S')[:5]),
                w, r['desc'].replace('|', '/')[:120]))
    # where the world touches a seat's state: variables that seat code touches
    # in at least twice as many functions as world code, and the world
    # functions (under their root) touching them
    cross = defaultdict(lambda: defaultdict(set))
    for r in rows:
        if 'S' not in r['class'] or 'W' not in r['class']:
            continue
        sf = set(fns(r, 'S'))
        wf = {(c, f) for c, fs in r['contexts'].items() if c[0] == 'W' for f in fs}
        if len(sf) < 2 * len({f for _, f in wf}):
            continue
        writers = {x['fn'] for x in r['writers']}
        for c, f in wf:
            cross[c[2:]][f].add(r['name'] + (' (w)' if f in writers else ''))
    L += ['', '## Where the world touches a seat\'s state', '',
          'Variables that seat code touches in at least twice as many functions as world code '
          '(UI state, the human\'s own data), and which world function touches them -- under which '
          'world root. (w): the function assigns it.', '',
          '| world root | function | seat-side variables |', '|---|---|---|']
    for root in sorted(cross):
        for f in sorted(cross[root]):
            L.append('| %s | %s | %s |' % (root, f, ', '.join(sorted(cross[root][f]))))
    L += ['', '## The `g_player` comparisons', '', '| file:line | function | contexts | code |', '|---|---|---|---|']
    for b in d['branches']:
        L.append('| %s:%d | %s | %s | `%s` |' % (b['file'], b['line'], b['fn'], b['ctx'], b['text'].replace('|', '/')))
    open(path, 'w').write('\n'.join(L) + '\n')


if __name__ == '__main__':
    if len(sys.argv) != 3 or sys.argv[1] not in ('trace', 'report'):
        sys.exit(__doc__)
    {'trace': cmd_trace, 'report': cmd_report}[sys.argv[1]](sys.argv[2])
