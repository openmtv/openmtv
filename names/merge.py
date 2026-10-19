#!/usr/bin/env python3
"""Merge naming proposals (names/proposals/*.txt) into names/symbols.txt.

Entries already in symbols.txt win.  Proposals are validated (C identifiers,
no keyword clashes), duplicates of the same name for different addresses get
a numeric suffix, and conflicts are reported.

Usage: merge.py [--dry-run]
"""
import sys, os, re, glob, collections

HERE = os.path.dirname(os.path.abspath(__file__))
SYM = os.path.join(HERE, 'symbols.txt')
C_KEYWORDS = set('''auto break case char const continue default do double else enum extern float for goto if
int long register return short signed sizeof static struct switch typedef union unsigned void volatile
while main exit abs rand srand malloc free open read write close lseek strlen strcpy memset'''.split())
RESERVED = {'R', 'M', 'T', 'DSEG', 'FP8', 'FP16', 'FP32', 'G8', 'G16', 'G32', 'MEM8', 'MEM16', 'MEM32',
            'LO16', 'HI16', 'MK32', 'POLL', 'TICK', 'cc', 'ax', 'bx', 'cx', 'dx', 'si', 'di', 'es', 'bp_', 'rv_'}


def parse(path):
    out = []
    for n, line in enumerate(open(path)):
        s = line.strip()
        if not s or s.startswith('#'):
            continue
        p = s.split(None, 3)
        if p[0] == 'fn' and len(p) >= 3:
            s_, o_ = p[1].split(':')
            key = ('fn', '%04X:%04X' % (int(s_, 16), int(o_, 16)))
            out.append((key, p[2], p[3] if len(p) > 3 else '', path, n + 1))
        elif p[0] == 'g' and len(p) >= 4:
            q = s.split(None, 4)
            key = ('g', '%04X' % int(q[1], 16), q[2])
            out.append((key, q[3], q[4] if len(q) > 4 else '', path, n + 1))
        elif p[0] == 'rec':
            q = s.split(None, 6)
            if len(q) >= 6:
                key = ('rec', '%04X' % int(q[1], 16), '%X' % int(q[2], 16), '%X' % int(q[3], 16), q[4])
                out.append((key, q[5], q[6] if len(q) > 6 else '', path, n + 1))
    return out


def valid(name):
    return re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]{0,60}', name) and name not in C_KEYWORDS and name not in RESERVED


def main():
    dry = '--dry-run' in sys.argv
    base = parse(SYM)
    have = {k: (nm, d) for k, nm, d, _, _ in base}
    used = collections.Counter(nm for k, nm, d, _, _ in base)
    added, conflicts, bad = [], [], []
    for path in sorted(glob.glob(os.path.join(HERE, 'proposals', '*.txt'))):
        for k, nm, d, src, line in parse(path):
            if not valid(nm):
                bad.append('%s:%d invalid name %r' % (os.path.basename(src), line, nm))
                continue
            if k in have:
                if have[k][0] != nm:
                    conflicts.append('%s:%d %s: keeping %s, proposal %s' % (os.path.basename(src), line, ' '.join(k[1:]), have[k][0], nm))
                continue
            final = nm
            if used[nm]:
                final = '%s_%d' % (nm, used[nm] + 1)
            used[nm] += 1
            have[k] = (final, d)
            added.append((k, final, d))
    print('added %d, conflicts %d, invalid %d' % (len(added), len(conflicts), len(bad)))
    for c in conflicts[:40]:
        print('  conflict', c)
    for b in bad[:40]:
        print('  invalid', b)
    if dry or not added:
        return
    with open(SYM, 'a') as f:
        f.write('\n# ---- merged from names/proposals/*.txt\n')
        for k, nm, d in sorted(added):
            if k[0] == 'fn':
                f.write('fn %s %-28s %s\n' % (k[1], nm, d))
            elif k[0] == 'g':
                f.write('g %s %s %-26s %s\n' % (k[1], k[2], nm, d))
            else:
                f.write('rec %s %s %s %s %-24s %s\n' % (k[1], k[2], k[3], k[4], nm, d))


if __name__ == '__main__':
    main()
