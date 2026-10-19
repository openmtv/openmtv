#!/usr/bin/env python3
"""Where do two engines part on the logical clock?

    clockdiff.py SCENARIO F0-F1 [--fixes...]

Runs tests/scenarios/SCENARIO.txt on build/mtv and on build/mtv-dec, both
with --logical-time and CLOCK_TRACE=F0-F1 (runtime/clock.c: every counted
function entry and I/O operation in that frame window), names the entries
(names/symbols.txt) and prints the first place where the two sequences
differ, with context.  Use it after scenario.py check reports a logical-clock
difference: narrow the frame window with the STATE lines first."""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import scenario


def names():
    n = {}
    for l in open(os.path.join(scenario.ROOT, 'names', 'symbols.txt')):
        p = l.split()
        if len(p) >= 3 and p[0] == 'fn':
            n[p[1]] = p[2]
    return n


# C functions of src/ whose name differs from the original function's
# (the decompiled build logs __func__): the same entry under both names
ALIAS = {'str_format_args': 'str_format'}


def trace(binary, path, window, extra):
    os.environ['CLOCK_TRACE'] = window
    try:
        _, err = scenario.run(binary, path, ['--logical-time'] + extra)
    finally:
        del os.environ['CLOCK_TRACE']
    nm, out = names(), []
    for l in err.split('\n'):
        if l.startswith('E '):
            x = l[2:].strip()
            x = nm.get(x, x)
            out.append('E ' + ALIAS.get(x, x))
        elif l.startswith('I '):
            out.append(l.strip())
    return out


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(2)
    name, window, extra = sys.argv[1], sys.argv[2], sys.argv[3:]
    path = os.path.join(scenario.SCEN, name + '.txt')
    a = trace('build/mtv', path, window, extra)
    b = trace('build/mtv-dec', path, window, extra)
    print('machine-level: %d events, decompiled: %d events' % (len(a), len(b)))
    i = 0
    while i < len(a) and i < len(b) and a[i] == b[i]:     # equal up to the first difference
        i += 1
    if i < len(a) or i < len(b):
        print('first difference after %d equal events:' % i)
        print('  before:        ' + ' / '.join(a[max(0, i - 12):i]))
        print('  machine-level: ' + ' / '.join(a[i:i + 12]))
        print('  decompiled:    ' + ' / '.join(b[i:i + 12]))
        return
    print('no difference in this window')


if __name__ == '__main__':
    main()
