#!/usr/bin/env python3
"""Find the root causes in a difftest log: failing functions none of whose
callees fail.  Usage: dtblame.py difftest.err"""
import sys, re, os, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lift
fails = collections.defaultdict(list)
for line in open(sys.argv[1]):
    m = re.match(r'DIFF f_([0-9A-F]{4})_([0-9A-F]{4}): (.*)', line)
    if m:
        fails[(int(m.group(1), 16), int(m.group(2), 16))].append(m.group(3))
dec = set()
if os.path.exists('src/game_table.c'):
    t = open('src/game_table.c').read()
    for k in re.findall(r'0x([0-9A-F]{8})', t.split('dec_keys')[1].split('};')[0]):
        dec.add((int(k[:4], 16), int(k[4:], 16)))
L = lift.Lifter('build/MTV_U.EXE'); L.build_all()
def callees(k):
    out = set()
    stack = [k]; seen = set()
    while stack:
        c = stack.pop()
        if c in seen: continue
        seen.add(c)
        f = L.funcs.get(c)
        if not f: continue
        for i in f.insns.values():
            t = L.call_target(i)
            if t and i.op in ('call', 'callf') and t in L.funcs:
                out.add(t); stack.append(t)
    return out
roots = []
for k in fails:
    if not (callees(k) & set(fails)):
        roots.append(k)
print('%d failing functions, %d root causes (%d of them decompiled)' % (len(fails), len(roots), sum(1 for r in roots if r in dec)))
for r in sorted(roots):
    print('%04X:%04X %s  %s' % (r[0], r[1], 'DEC' if r in dec else 'lifted', ' | '.join(fails[r][:4])))
