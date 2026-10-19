import sys, collections
sys.path.insert(0, sys.path[0])
from mzimage import MZ
m = MZ(sys.argv[1])
segvals = collections.Counter(m.w(l) for l in m.reloc_lin)
print('distinct segment values referenced:', len(segvals))
# classify by preceding opcode
kinds = collections.defaultdict(collections.Counter)
farcalls = collections.defaultdict(set)
for l in m.reloc_lin:
    v = m.w(l)
    op = m.image[l-3] if l >= 3 else None
    if op == 0x9A: kinds[v]['callf'] += 1; farcalls[v].add(m.w(l-2))
    elif op == 0xEA: kinds[v]['jmpf'] += 1; farcalls[v].add(m.w(l-2))
    elif m.image[l-1] in (0xB8,0xB9,0xBA,0xBB,0xBE,0xBF): kinds[v]['movimm'] += 1
    else: kinds[v]['other'] += 1
for v in sorted(segvals):
    print(f'{v:04X} lin {v*16:06X} refs {segvals[v]:4d} {dict(kinds[v])} entries {len(farcalls[v])}')
