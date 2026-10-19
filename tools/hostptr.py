#!/usr/bin/env python3
"""ROADMAP Phase 3, stage 4: far-pointer parameters -> host pointers.

A decompiled function that receives the address of a caller's local (an
out-parameter, a buffer) takes it as a far pointer (u32 seg:off) and goes
through the emulated memory (FP8/16/32(p, k)).  That keeps the caller's
local in the emulated stack frame.  This retypes such a parameter as a host
pointer (u8 *), so a caller can pass the address of a C local:

  * in the function: FPw(p, k) -> HPw(p, k); any other use of p is listed
    and left alone (the conversion is refused: fix it by hand first);
  * the prototype in game_protos.h;
  * its glue wrapper f_SEG_OFF in glue.c: HPTR(the far pointer from the
    emulated stack) -- machine-level callers keep working;
  * every call: MK32(R.ss, bp_ - k) -> LOCP(k) (the slot, still in the
    emulated frame; tools/frame2locals.py moves it to C later), any other
    far pointer E -> HPTR(E).

    hostptr.py [--dry] FUNCTION PARAM [PARAM ...]
"""
import glob
import re
import sys


def balanced(text, i):
    depth = 1
    while depth:
        if text[i] == '(':
            depth += 1
        elif text[i] == ')':
            depth -= 1
        i += 1
    return i


def split_args(s):
    parts, depth, cur = [], 0, ''
    for ch in s:
        if ch == ',' and depth == 0:
            parts.append(cur); cur = ''; continue
        if ch == '(':
            depth += 1
        elif ch == ')':
            depth -= 1
        cur += ch
    parts.append(cur)
    return parts


def main():
    args = sys.argv[1:]
    dry = '--dry' in args
    args = [a for a in args if a != '--dry']
    fn, params = args[0], args[1:]
    files = {p: open(p).read() for p in glob.glob('src/*.c') + ['src/game_protos.h']}
    # the definition
    dm = None
    for p, t in files.items():
        m = re.search(r'^([a-z0-9_ ]+?) ?\b%s\(([^)]*)\) \{\n' % fn, t, re.M)
        if m and p.endswith('.c'):
            dm = (p, m)
            break
    if not dm:
        sys.exit('%s: no definition' % fn)
    path, m = dm
    t = files[path]
    plist = [x.strip() for x in m.group(2).split(',')]
    pnames = [x.split()[-1] for x in plist]
    idx = []
    for prm in params:
        if prm not in pnames:
            sys.exit('%s has no parameter %s' % (fn, prm))
        i = pnames.index(prm)
        if not plist[i].startswith('u32 '):
            sys.exit('%s: %s is not a far pointer (%s)' % (fn, prm, plist[i]))
        idx.append(i)
    body_end = t.index('\n}\n', m.end())
    body = t[m.end():body_end]
    for prm in params:
        body = re.sub(r'\bFP(8|16|32)\(%s, ' % prm, r'HP\1(%s, ' % prm, body)
        other = [l.strip() for l in body.split('\n') if re.search(r'\b%s\b' % prm, re.sub(r'HP(8|16|32)\(%s, ' % prm, '', l))]
        if other:
            print('%s: other uses of %s, convert by hand first:' % (fn, prm))
            for l in other:
                print('    ' + l)
            sys.exit(1)
    newplist = [('u8 *' + pnames[i]) if i in idx else plist[i] for i in range(len(plist))]
    t = t[:m.start(2)] + ', '.join(newplist) + t[m.end(2):m.end()] + body + t[body_end:]
    files[path] = t
    # prototype
    ph = files['src/game_protos.h']
    pm = re.search(r'^([a-z0-9_ ]+?) ?\b%s\(([^)]*)\);' % fn, ph, re.M)
    if pm:
        pl = [x.strip() for x in pm.group(2).split(',')]
        pl = [('u8 *' + pl[i].split()[-1]) if i in idx else pl[i] for i in range(len(pl))]
        files['src/game_protos.h'] = ph[:pm.start(2)] + ', '.join(pl) + ph[pm.end(2):]
    # calls (incl. the glue wrapper)
    ncalls = 0
    for p in list(files):
        if not p.endswith('.c'):
            continue
        s = files[p]
        out, pos = [], 0
        for cm in re.finditer(r'\b%s\(' % fn, s):
            if cm.start() < pos:
                continue
            # the definition itself
            if s[cm.end():].startswith(', '.join(newplist) + ')'):
                continue
            end = balanced(s, cm.end())
            a = split_args(s[cm.end():end - 1])
            if len(a) != len(plist):
                print('%s: call with %d arguments, skipped: %s' % (p, len(a), s[cm.start():end][:100]))
                continue
            for i in idx:
                e = a[i].strip()
                lm = re.fullmatch(r'MK32\(R\.ss, bp_ - (0x[0-9A-Fa-f]+|\d+)\)', e)
                if lm:
                    e = 'LOCP(%s)' % lm.group(1)
                else:
                    e = 'HPTR(%s)' % e
                a[i] = (' ' if i else '') + e
            out.append(s[pos:cm.end()]); out.append(','.join(a)); out.append(')')
            pos = end
            ncalls += 1
        out.append(s[pos:])
        files[p] = ''.join(out)
    if not dry:
        for p, s in files.items():
            if open(p).read() != s:
                open(p, 'w').write(s)
    print('%s: %s now host pointers, %d calls' % (fn, ', '.join(params), ncalls))


if __name__ == '__main__':
    main()
