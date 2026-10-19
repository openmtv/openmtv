"""Add functions to the decompiled-function list of src/game_table.c.

    python3 tools/dectable.py SEG:OFF=RET ...      RET = 0, 16 or 32

dec_keys/dec_ret (and dec_count) stay sorted; tools/dtblame.py,
tools/fuzzargs.py and the coverage report read them.  conv_* (the calling
convention of every C-convention function, lifted or decompiled) is not
touched: a function rewritten by hand keeps the entry it already has."""
import re, sys

P = 'src/game_table.c'
t = open(P).read()
keys = re.findall(r'0x([0-9A-F]{8})', t.split('dec_keys[] = {')[1].split('};')[0])
rets = t.split('dec_ret[] = {')[1].split('};')[0].split(', ')
d = dict(zip(keys, rets))
for a in sys.argv[1:]:
    addr, ret = a.split('=')
    seg, off = addr.split(':')
    assert ret in ('0', '16', '32'), a
    d['%04X%04X' % (int(seg, 16), int(off, 16))] = ret
ks = sorted(d, key=lambda k: int(k, 16))
t = re.sub(r'const u32 dec_keys\[\] = \{\n.*?\};', lambda m: 'const u32 dec_keys[] = {\n' + ', '.join('0x' + k for k in ks) + '};', t, count=1, flags=re.S)
t = re.sub(r'const u8 dec_ret\[\] = \{.*?\};', lambda m: 'const u8 dec_ret[] = {' + ', '.join(d[k] for k in ks) + '};', t, count=1, flags=re.S)
t = re.sub(r'const int dec_count = \d+;', 'const int dec_count = %d;' % len(ks), t, count=1)
open(P, 'w').write(t)
print('%d decompiled functions' % len(ks))
