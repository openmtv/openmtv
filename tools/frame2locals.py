#!/usr/bin/env python3
"""ROADMAP Phase 3, stage 4: locals out of the emulated stack frame.

The decompiler kept some functions' locals in the emulated stack frame
(`bp_ = R.sp - 2; R.sp -= N;` and LOC8/16/32(k) = the bytes at SS:bp_-k).
For a function whose frame is only ever reached through LOCw(constant) --
no address of a local is taken, no local array is indexed at run time, no
stack argument is read through ARGw -- this rewrites the frame as C locals:

  * a slot always accessed with the same width and overlapping no other
    slot becomes a variable `l_K` (u8/u16/u32);
  * slots accessed with different widths or overlapping each other stay
    together in a byte array `fr_[...]`, accessed with FR8/16/32(k), which
    keep the byte layout (little-endian, as in the emulated memory);
  * a local array indexed at run time -- MEMw(R.ss, (index) + (bp_ - K)) --
    puts the whole frame into `fr_` (FRX8/16/32: element K plus a byte
    offset), so array elements and scalars alias as in the original;
  * a slot whose address is handed to a callee as a host pointer
    (LOCP(k), see tools/hostptr.py) also puts the frame into `fr_`;
  * the frame allocation and every `R.sp += N` that releases it go.

Locals start as 0: the C build never depended on what an emulated slot
held before its first write except where src/ reproduces original stack
garbage explicitly (not through these slots) -- tools/nativebase.py checks
that classic runs stay bit-identical.

    frame2locals.py [--dry] FILE.c [FUNCTION ...]     (default: all eligible)
"""
import re
import sys

HEAD = re.compile(r'^/\* ([0-9A-F]{4}:[0-9A-F]{4})  [^\n]*\n(?:[^\n]*\n)*?([a-z0-9_ ]+?) ?\**([a-z_][a-z0-9_]*)\([^)]*\) \{\n', re.M)
FRAME = re.compile(r'    bp_ = \(u16\)\(R\.sp - 2\); R\.sp -= (0x[0-9A-Fa-f]+|\d+);   /\* locals live in the emulated stack frame \*/\n')
LOC = re.compile(r'LOC(8|16|32)\((0x[0-9A-Fa-f]+|\d+)\)')
CTYPE = {8: 'u8', 16: 'u16', 32: 'u32'}


def functions(text):
    """(name, start of body, end of body) for each function with an address comment"""
    heads = list(HEAD.finditer(text))
    for i, m in enumerate(heads):
        end = text.index('\n}\n', m.end()) + 1
        yield m.group(3), m.end(), end


def balanced(text, i):
    """index just past the parenthesis closing the one opened before text[i]"""
    depth = 1
    while depth:
        if text[i] == '(':
            depth += 1
        elif text[i] == ')':
            depth -= 1
        i += 1
    return i


def arrays(text, size):
    """MEMw(R.ss, ... (bp_ - K) ...) -> FRXw(fr_, size, K, ...): (new text, count)"""
    out, pos, count = [], 0, 0
    for m in re.finditer(r'MEM(8|16|32)\(R\.ss, ', text):
        if m.start() < pos:
            continue
        end = balanced(text, m.end())
        arg = text[m.end():end - 1]
        terms = re.findall(r'\(bp_ - (0x[0-9A-Fa-f]+|\d+)\)', arg)
        if len(terms) != 1 or arg.count('bp_') != 1:
            continue
        k = int(terms[0], 0)
        d = re.sub(r'\(bp_ - (0x[0-9A-Fa-f]+|\d+)\)', '0', arg)
        out.append(text[pos:m.start()])
        out.append('FRX%s(fr_, %d, 0x%X, %s)' % (m.group(1), size, k, d))
        pos = end
        count += 1
    out.append(text[pos:])
    return ''.join(out), count


def element_ptrs(text, size):
    """HPTR(MK32(R.ss, ... (bp_ - K) ...)) -> a pointer into fr_: (new text, count)"""
    out, pos, count = [], 0, 0
    for m in re.finditer(r'HPTR\(MK32\(R\.ss, ', text):
        if m.start() < pos:
            continue
        end = balanced(text, m.end())         # closes MK32(
        arg = text[m.end():end - 1]
        if text[end:end + 1] != ')' or arg.count('bp_') != 1:
            continue
        terms = re.findall(r'\(?bp_ - (0x[0-9A-Fa-f]+|\d+)\)?', arg)
        if len(terms) != 1:
            continue
        k = int(terms[0], 0)
        d = re.sub(r'\(?bp_ - (0x[0-9A-Fa-f]+|\d+)\)?', '0', arg)
        out.append(text[pos:m.start()])
        out.append('(fr_ + %d - 0x%X + (int)(i16)(%s))' % (size, k, d))
        pos = end + 1
        count += 1
    out.append(text[pos:])
    return ''.join(out), count


def convert(body):
    """the rewritten body, or (None, reason)"""
    fm = FRAME.search(body)
    if not fm:
        return None, 'no emulated frame'
    n = int(fm.group(1), 0)
    rest = body[:fm.start()] + body[fm.end():]
    rest, narrays = arrays(rest, n)
    rest, nelem = element_ptrs(rest, n)
    narrays += nelem
    # addresses of slots handed to callees as host pointers (tools/hostptr.py)
    nptrs = len(re.findall(r'LOCP\((0x[0-9A-Fa-f]+|\d+)\)', rest))
    rest = re.sub(r'LOCP\((0x[0-9A-Fa-f]+|\d+)\)', lambda mm: '(fr_ + %d - 0x%X)' % (n, int(mm.group(1), 0)), rest)
    narrays += nptrs
    uses = [(int(w), int(k, 0)) for w, k in LOC.findall(rest)]
    stripped = LOC.sub('', rest)
    # the frame's address as a plain value (the original's `lea ax, [bp-K]`
    # left in AX, which a callee may read): keep bp_ as a value
    # (also passed on as a callee's AX argument); an address of the frame in
    # a far pointer or a memory access is an escape
    rest_bp = stripped.replace('u16 bp_;', '')
    if re.search(r'(MK32|MEM8|MEM16|MEM32)\(R\.ss,[^;]*\bbp_\b', rest_bp) or re.search(r'R\.ss, \(?u16\)?\(?bp_', rest_bp) \
            or re.search(r'\bbp_ [-+] (0x[0-9A-Fa-f]+|\d+)\)?, R\.ss\b', rest_bp):
        return None, 'address of a local taken'
    keep_bp = 'bp_' in rest_bp
    if re.search(r'\bARG(8|16|32)\(', rest):
        return None, 'reads stack arguments'
    releases = re.findall(r'R\.sp \+= (0x[0-9A-Fa-f]+|\d+);', rest)
    if any(int(r, 0) != n for r in releases):
        return None, 'unexpected stack adjustment'
    if 'R.sp' in re.sub(r'R\.sp \+= (0x[0-9A-Fa-f]+|\d+);', '', rest):
        return None, 'other use of R.sp'
    # byte ranges: LOCw(k) covers bp_-k .. bp_-k+w/8-1
    slots = {}
    for w, k in uses:
        slots.setdefault(k, set()).add(w)
    single, shared = {}, set()
    if narrays:                                 # everything in fr_
        shared = set(slots)
        slots = {k: ws for k, ws in slots.items()}
    for k, ws in slots.items():
        if len(ws) > 1:
            shared.add(k)
            continue
        w = next(iter(ws))
        lo, hi = -k, -k + w // 8
        for k2, ws2 in slots.items():
            if k2 == k:
                continue
            for w2 in ws2:
                if -k2 < hi and -k2 + w2 // 8 > lo:
                    shared.add(k)
        if k not in shared:
            single[k] = w
    # a slot overlapping a shared one is shared too (fixpoint)
    changed = True
    while changed:
        changed = False
        for k in list(single):
            w = single[k]
            for k2 in shared:
                for w2 in slots[k2]:
                    if -k2 < -k + w // 8 and -k2 + w2 // 8 > -k:
                        shared.add(k); del single[k]; changed = True
                        break
                if k not in single:
                    break
    out = rest
    decls = []
    for k in sorted(single, reverse=True) if not narrays else []:
        w = single[k]
        name = 'l_%X' % k
        out = re.sub(r'LOC%d\((0x%X|%d)\)' % (w, k, k), name, out)
        decls.append('    %s %s = 0;\n' % (CTYPE[w], name))
    if shared or narrays:
        size = n if narrays else max(k for k in shared)
        decls.append('    u8 fr_[%d] = {0};                  /* bytes bp-%d .. bp-1 of the original frame */\n' % (size, size))
        for k in shared:
            for w in slots[k]:
                out = re.sub(r'LOC%d\((0x%X|%d)\)' % (w, k, k), 'FR%d(fr_, %d, 0x%X)' % (w, size, k), out)
    out = re.sub(r'    R\.sp \+= (0x[0-9A-Fa-f]+|\d+);\n', '', out)
    if keep_bp:
        decls.append('    u16 bp_ = (u16)(R.sp - 2);           /* the original frame\'s address, for AX values */\n')
    out = out.replace('    u16 bp_;\n', '', 1)
    # declarations after the existing ones (before the first statement)
    lines = out.split('\n')
    i = 0
    while i < len(lines) and re.match(r'    (u8|u16|u32|i16|i32|int|u16 \*|const) [^=;(]*;$', lines[i]):
        i += 1
    lines[i:i] = [d.rstrip('\n') for d in decls]
    return '\n'.join(lines), None


def main():
    args = sys.argv[1:]
    dry = '--dry' in args
    args = [a for a in args if a != '--dry']
    path, names = args[0], set(args[1:])
    text = open(path).read()
    pieces, pos, done = [], 0, []
    for name, start, end in functions(text):
        if names and name not in names:
            continue
        new, why = convert(text[start:end])
        if new is None:
            if names or why != 'no emulated frame':
                print('%-28s kept: %s' % (name, why))
            continue
        pieces.append(text[pos:start]); pieces.append(new); pos = end
        done.append(name)
    pieces.append(text[pos:])
    if not dry and done:
        open(path, 'w').write(''.join(pieces))
    for name in done:
        print('%-28s converted' % name)


if __name__ == '__main__':
    main()
