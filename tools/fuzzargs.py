#!/usr/bin/env python3
"""Argument vectors for differential fuzzing of decompiled functions.

For every decompiled function: its word count (prototype), which words form
far pointers, and constants that its call sites push (static analysis).
Output lines:  SEG:OFF nwords ptrmask c0,c1,...   (ptrmask bit i = word i
starts a far pointer)."""
import sys, os, re, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import decomp

D = decomp.Decompiler(sys.argv[1], sys.argv[2])
D.infer_protos()
t = open(sys.argv[3]).read()
dec = [(int(k[:4], 16), int(k[4:], 16)) for k in re.findall(r'0x([0-9A-F]{8})', t.split('dec_keys')[1].split('};')[0])]
consts = collections.defaultdict(lambda: collections.defaultdict(set))
for k, f in D.L.funcs.items():
    offs = sorted(f.insns)
    for n, o in enumerate(offs):
        i = f.insns[o]
        if i.op not in ('callf', 'call'):
            continue
        tgt = D.L.call_target(i)
        # walk back over pushes: argument word index 0 = last push
        idx = 0
        m = n - 1
        while m >= 0 and idx < 12:
            j = f.insns[offs[m]]
            if j.op == 'push':
                if j.ops[0].kind == 'imm' and not j.ops[0].reloc:
                    consts[tgt][idx].add(j.ops[0].val)
                elif j.ops[0].kind == 'reg' and m > 0:
                    pj = f.insns[offs[m - 1]]
                    if pj.op == 'mov' and pj.ops[0].kind == 'reg' and pj.ops[0].name == j.ops[0].name and pj.ops[1].kind == 'imm' and not pj.ops[1].reloc:
                        consts[tgt][idx].add(pj.ops[1].val)
                    if pj.op == 'xor' and pj.ops[0].kind == 'reg' and pj.ops[0].name == j.ops[0].name:
                        consts[tgt][idx].add(0)
                idx += 1
            elif j.op in ('callf', 'call', 'jmp', 'ret', 'retf', 'jcc') or offs[m] in f.labels:
                break
            m -= 1
for k in dec:
    p = D.protos[k]
    mask = 0
    for name, w, idx in p.params:
        if w == 32:
            mask |= 1 << idx
    cs = []
    for idx in range(p.nwords or 0):
        vals = sorted(consts[k][idx])[:6]
        cs.append('/'.join('%X' % v for v in vals) or '-')
    print('%04X:%04X %d %X %s' % (k[0], k[1], p.nwords or 0, mask, ','.join(cs) or '-'))
