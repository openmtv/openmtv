"""Readability passes over structured C function bodies (text level).
Every pass is semantics-preserving for the code the decompiler emits:
expressions are side-effect free except calls, and calls are kept."""
import re

CTL = ('break;', 'continue;', 'return;')


def indent_of(l):
    return len(l) - len(l.lstrip(' '))


def drop_empty_else(lines):
    out = []
    i = 0
    while i < len(lines):
        if lines[i].strip() == '} else {' and i + 1 < len(lines) and lines[i + 1].strip() == '}' and \
                indent_of(lines[i + 1]) == indent_of(lines[i]):
            out.append(lines[i].replace('} else {', '}'))
            i += 2
            continue
        out.append(lines[i])
        i += 1
    return out


def inline_cc(lines):
    """cc = X; if (cc) {  ->  if (X) {   (also 'if (!(cc))')"""
    out = []
    i = 0
    while i < len(lines):
        m = re.match(r'^(\s*)cc = (.*);$', lines[i])
        if m and i + 1 < len(lines):
            nxt = lines[i + 1]
            m2 = re.match(r'^(\s*)if \((!\()?cc\)?\) \{$', nxt)
            if m2 and indent_of(nxt) == len(m.group(1)):
                cond = m.group(2)
                if m2.group(2):
                    from structure import negate
                    cond = negate(cond)
                out.append('%sif (%s) {' % (m2.group(1), cond))
                i += 2
                continue
        out.append(lines[i])
        i += 1
    return out


def one_line_ctl(lines):
    """if (c) {\\n break; \\n}  ->  if (c) break;"""
    out = []
    i = 0
    while i < len(lines):
        l = lines[i]
        if l.rstrip().endswith('{') and l.lstrip().startswith('if (') and i + 2 < len(lines):
            body = lines[i + 1].strip()
            if (body in CTL or body.startswith('goto ') or body.startswith('return ')) and \
                    lines[i + 2].strip() == '}' and indent_of(lines[i + 2]) == indent_of(l):
                out.append(l.rstrip()[:-1].rstrip() + ' ' + body)
                i += 3
                continue
        out.append(l)
        i += 1
    return out


def loops(lines):
    """for (;;) { LOOP_TICK(n); if (c) break; ...}  ->  while (!c) { LOOP_TICK(n); ... };
    drop a trailing 'continue;' right before a loop's closing brace"""
    out = list(lines)
    # trailing continue
    res = []
    for i, l in enumerate(out):
        if l.strip() == 'continue;' and i + 1 < len(out) and out[i + 1].strip() == '}' and \
                indent_of(out[i + 1]) == indent_of(l) - 4:
            # is the closing brace a loop's? find its opener
            d = indent_of(out[i + 1])
            j = i
            while j >= 0 and not (indent_of(out[j]) == d and out[j].rstrip().endswith('{')):
                j -= 1
            if j >= 0 and (out[j].strip().startswith('for (') or out[j].strip().startswith('while (')):
                continue
        res.append(l)
    out = res
    res = []
    i = 0
    while i < len(out):
        l = out[i]
        if l.strip() == 'for (;;) {' and i + 2 < len(out):
            tick = out[i + 1].strip()
            m = re.match(r'^if \((.*)\) break;$', out[i + 2].strip())
            if tick.startswith('LOOP_TICK(') and m:
                from structure import negate
                res.append(l.replace('for (;;) {', 'while (%s) {' % negate(m.group(1))))
                res.append(out[i + 1])
                i += 3
                continue
        res.append(l)
        i += 1
    return res


def dead_temps(lines, decls):
    """remove assignments to t* temporaries that are never read"""
    changed = True
    while changed:
        changed = False
        text = '\n'.join(lines)
        for v in list(decls):
            if not re.fullmatch(r't\d+', v):
                continue
            uses = len(re.findall(r'\b%s\b' % v, text))
            defs = re.findall(r'^\s*%s = (.*);$' % v, text, re.M)
            if uses == len(defs):
                new = []
                for l in lines:
                    m = re.match(r'^(\s*)%s = (.*);$' % v, l)
                    if m:
                        rhs = m.group(2)
                        if re.search(r'\b(c_[0-9A-F]{4}_[0-9A-F]{4}|call_lifted|call_far_addr|f_[0-9A-F]{4}_[0-9A-F]{4})\(', rhs):
                            new.append('%s%s;' % (m.group(1), rhs))
                        continue
                    new.append(l)
                lines = new
                del decls[v]
                changed = True
                break
    return lines


def unused_decls(lines, decls):
    text = '\n'.join(lines)
    for v in list(decls):
        if not re.search(r'\b%s\b' % re.escape(v), text):
            del decls[v]


def else_if(lines):
    """} else { if (c) {...} [else ...] }  ->  } else if (c) {...} [else ...]"""
    changed = True
    while changed:
        changed = False
        for n in range(len(lines) - 2):
            l0 = lines[n]
            if l0.strip() != '} else {':
                continue
            ind = indent_of(l0)
            l1 = lines[n + 1]
            if indent_of(l1) != ind + 4 or not (l1.strip().startswith('if (') and l1.rstrip().endswith('{')):
                continue
            # find the end of the inner if-chain at indent ind+4
            end = None
            for m in range(n + 2, len(lines)):
                lm = lines[m]
                if not lm.strip() or re.match(r'^L_[0-9A-F]+:$', lm.strip()):
                    continue
                im = indent_of(lm)
                if im < ind + 4:
                    break
                if im == ind + 4:
                    st = lm.strip()
                    if st == '}':
                        end = m
                        break
                    if st == '} else {' or (st.startswith('} else if (') and st.endswith('{')):
                        continue
                    break
            if end is None or end + 1 >= len(lines) or lines[end + 1].strip() != '}' or indent_of(lines[end + 1]) != ind:
                continue
            new = lines[:n] + [' ' * ind + '} else ' + l1.strip()]
            for lm in lines[n + 2:end + 1]:
                new.append(lm[4:] if lm.startswith(' ' * (ind + 4)) else lm)
            new += lines[end + 2:]
            lines = new
            changed = True
            break
    return lines


RET_TYPES = {'call_lifted': 'u32', 'call_far_addr': 'u32'}   # decomp.py adds decompiled functions
CALL_RE = re.compile(r'\b([A-Za-z_][A-Za-z_0-9]*)\s*\(')
IDENT_RE = re.compile(r'\b[A-Za-z_][A-Za-z_0-9]*\b')
MACRO_TYPES = {'G8': 'u8', 'MEM8': 'u8', 'FP8': 'u8', 'LOC8': 'u8', 'G16': 'u16', 'MEM16': 'u16',
               'FP16': 'u16', 'LOC16': 'u16', 'LO16': 'u16', 'HI16': 'u16', 'G32': 'u32', 'MEM32': 'u32',
               'FP32': 'u32', 'LOC32': 'u32', 'MK32': 'u32'}
MEM_MACROS = {'G8', 'G16', 'G32', 'FP8', 'FP16', 'FP32', 'MEM8', 'MEM16', 'MEM32', 'LOC8', 'LOC16', 'LOC32'}
PURE_MACROS = {'LO16', 'HI16', 'MK32', 'FPSEG', 'FPOFF', 'SEGV'}
MEM_NAMES = set()        # decomp.py adds struct tables, record-field macros (function-like) ...
GLOBAL_VARS = set()      # ... and named DGROUP globals (object-like macros)


def has_call(e):
    return any(m.group(1) not in MEM_MACROS | PURE_MACROS | MEM_NAMES for m in CALL_RE.finditer(e))


def reads_memory(e):
    if re.search(r'\bR\.|\bDSEG\b', e):
        return True
    if any(m.group(1) in MEM_MACROS | MEM_NAMES for m in CALL_RE.finditer(e)):
        return True
    return any(x in GLOBAL_VARS for x in IDENT_RE.findall(e))


def whole_call(e):
    """name if e is exactly one call/macro application name(...)"""
    m = re.fullmatch(r'([A-Za-z_][A-Za-z_0-9]*)\((.*)\)', e)
    if not m:
        return None
    depth = 0
    for ch in m.group(2):
        depth += ch == '('
        depth -= ch == ')'
        if depth < 0:
            return None
    return m.group(1) if depth == 0 else None


def typed(e, t, decls):
    """e as a value of the temporary's type t (cast only when needed)"""
    if re.fullmatch(r'[A-Za-z_][A-Za-z_0-9]*', e) and decls.get(e) == t:
        return e
    if re.fullmatch(r'-?(0x[0-9A-Fa-f]+|\d+)', e):
        return e
    n = whole_call(e)
    if n is not None and (MACRO_TYPES.get(n) == t or RET_TYPES.get(n) == t):
        return e
    if re.fullmatch(r'\(%s\)' % t + r'\(.*\)', e) and whole_call(e[len(t) + 2:]) is not None:
        return e
    return '(%s)(%s)' % (t, e) if t in ('u8', 'u16') else '(%s)' % e


def inline_temps(lines, decls):
    """tN = e; used once in the next statement (possibly after plain local
    assignments that do not touch e) -> substitute e there"""
    changed = True
    while changed:
        changed = False
        text = '\n'.join(lines)
        for i, l in enumerate(lines):
            m = re.match(r'^(\s*)(t\d+) = (.*);$', l)
            if not m or m.group(2) not in decls:
                continue
            ind, t, e = m.group(1), m.group(2), m.group(3)
            if len(re.findall(r'\b%s\b' % t, text)) != 2:
                continue
            ids = set(re.findall(r'\b[A-Za-z_][A-Za-z_0-9]*\b', e))
            j = i + 1
            ok = True
            while j < len(lines) and not re.search(r'\b%s\b' % t, lines[j]):
                mj = re.match(r'^(\s*)([A-Za-z_][A-Za-z_0-9]*) = (.*);$', lines[j])
                # only plain C locals may be skipped (a named global is memory),
                # and a call never moves past a statement that reads memory
                if not mj or mj.group(1) != ind or mj.group(2) in ids or mj.group(2) in GLOBAL_VARS or \
                        has_call(mj.group(3)) or (has_call(e) and reads_memory(mj.group(3))):
                    ok = False
                    break
                j += 1
            if not ok or j >= len(lines):
                continue
            u = lines[j]
            st = u.strip()
            if not u.startswith(ind) or (len(u) - len(u.lstrip()) != len(ind)) or \
                    st.startswith(('while', 'for', 'do', '}', 'else', 'L_')) or st.endswith(':'):
                continue
            rest = re.sub(r'\b%s\b' % t, '', u)
            if has_call(e):
                lhs, eq, rhs = rest.partition(' = ')
                if eq and not st.startswith(('if', 'return', 'switch')):
                    # a call into an assignment: the target address must not read memory or call
                    # (the store address may only use locals and constants)
                    addr = re.sub(r'^\s*[A-Za-z_][A-Za-z_0-9]*\(', '(', lhs, 1)
                    if has_call(rest) or reads_memory(rhs) or reads_memory(addr):
                        continue
                elif has_call(rest) or reads_memory(rest):
                    continue
            elif reads_memory(e) and has_call(rest):
                continue
            rep = typed(e, decls[t], decls)
            lines = lines[:i] + lines[i + 1:j] + [re.sub(r'\b%s\b' % t, lambda _: rep, u)] + lines[j + 1:]
            del decls[t]
            changed = True
            break
    return lines


def run(lines, decls):
    lines = drop_empty_else(lines)
    lines = inline_cc(lines)
    lines = dead_temps(lines, decls)
    lines = inline_temps(lines, decls)
    for _ in range(3):
        lines = drop_empty_else(lines)
        lines = one_line_ctl(lines)
        lines = loops(lines)
    lines = else_if(lines)
    unused_decls(lines, decls)
    return lines
