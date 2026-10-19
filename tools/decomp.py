#!/usr/bin/env python3
"""Decompiler: Turbo C 2.0 machine code of MTV.EXE -> readable C.

Works on the CFGs built by lift.py and the register liveness from regflow.py.
Each supported function becomes `c_SSSS_OOOO(params)` with real parameters,
locals and expressions, plus a wrapper `f_SSSS_OOOO(void)` that lets
machine-level code call it (overriding the lifted version at link time).

Anything outside the supported subset raises Unsupported and that function
keeps its lifted implementation, so the build is always complete.
"""
import copy, sys, os, json, collections, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lift, regflow, structure, cleanup, structs
from regflow import BIT

DGROUP = 0x39D4


class Unsupported(Exception):
    pass


DGROUP_STRINGS = {}
FUNC_NAMES = {}       # (seg, off) -> name
FUNC_DESC = {}
GLOBAL_NAMES = {}     # (offset, width) -> name
GLOBAL_DESC = {}
REC_NAMES = {}        # (base global, record size, field offset, width) -> name
REC_DESC = {}
REC_USED = {}         # key -> 'far' (table behind a far pointer) | 'ds' (static array)
STRUCTS = []          # structs.Table list (names/structs.txt)
PTR_TYPES = {}        # pointer local -> structs.Table (set per function by FuncGen)
ENUMS = {}            # enum name -> (prefix, {value: member})
ENUM_TARGETS = []     # (regex on rendered lvalue, enum name)
ENUM_PARAMS = {}      # (function name, argument word index) -> enum name
ARRAY_GLOBALS = set() # (offset, width) of named globals the code indexes: rendered name[i]
ARRAY_PENDING = set() # found in this round, rendered as arrays from the next round on
GLOBAL_HALVES = set() # (offset of a named 32-bit global, 'lo'|'hi') accessed as 16-bit words
ARRAY_HALVES = set()  # (offset of a 32-bit array global, 'lo'|'hi'): name_lo(i) / name_hi(i)
GLOBAL_INDEX = {}


def load_symbols(path):
    if not os.path.exists(path):
        return
    for line in open(path):
        line = line.split('#', 1)[0].rstrip() if not line.lstrip().startswith('#') else ''
        parts = line.split(None, 3)
        if not parts:
            continue
        if parts[0] == 'fn':
            s, o = [int(x, 16) for x in parts[1].split(':')]
            FUNC_NAMES[(s, o)] = parts[2]
            if len(parts) > 3:
                FUNC_DESC[(s, o)] = parts[3]
        elif parts[0] == 'g':
            off, w = int(parts[1], 16), int(parts[2])
            rest = line.split(None, 4)
            GLOBAL_NAMES[(off, w)] = rest[3]
            if len(rest) > 4:
                GLOBAL_DESC[(off, w)] = rest[4]
        elif parts[0] == 'rec':
            rest = line.split(None, 6)
            key = (int(rest[1], 16), int(rest[2], 16), int(rest[3], 16), int(rest[4]))
            REC_NAMES[key] = rest[5]
            if len(rest) > 6:
                REC_DESC[key] = rest[6]


def load_enums(path):
    if not os.path.exists(path):
        return
    cur = None
    for line in open(path):
        t = line.split('#', 1)[0].split()
        if not t:
            continue
        if t[0] == 'enum':
            cur = t[1]
            ENUMS[cur] = (t[2], {})
        elif t[0] == 'target':
            ENUM_TARGETS.append((re.compile(t[1]), cur))
        elif t[0] == 'param':
            ENUM_PARAMS[(t[1], int(t[2]))] = cur
        else:
            v = int(t[0], 0)
            if 0 <= v < 0x8000:
                ENUMS[cur][1][v] = t[1]


def strip_casts(s):
    prev = None
    while prev != s:
        prev = s
        s = re.sub(r'^\((?:u|i)(?:8|16|32)\)', '', s.strip()).strip()
        if s.startswith('(') and s.endswith(')'):
            depth = 0
            for n, ch in enumerate(s):
                depth += ch == '('
                depth -= ch == ')'
                if depth == 0 and n != len(s) - 1:
                    break
            else:
                s = s[1:-1]
    return s


def enum_of(text):
    t = strip_casts(text)
    for rx, en in ENUM_TARGETS:
        if rx.search(t):
            return en
    return None


def enum_name(en, x):
    """member name for constant x of enum en, or None"""
    if en is None or x is None or x.k != 'const':
        return None
    prefix, members = ENUMS[en]
    v = x.v & 0xFFFF
    if v in members:
        return '%s_%s' % (prefix, members[v])
    return None


def cname(k):
    return FUNC_NAMES.get(k, 'c_%04X_%04X' % k)


def load_strings(img):
    """printable NUL-terminated strings in DGROUP, for comments in the output"""
    base = DGROUP * 16
    data = img[base:base + 0xA730]
    for m in re.finditer(rb'[\x20-\x7e\x81-\xfe]{3,}\x00', data):
        s = m.group()[:-1].decode('latin1')
        if sum(c.isalpha() for c in s) < 2 and '#' not in s:
            continue
        s = s.replace('\\', '\\\\').replace('*/', '* /').replace('"', '\\"')
        if len(s) > 60:
            s = s[:57] + '...'
        DGROUP_STRINGS[m.start()] = '"%s"' % s


# ---------------------------------------------------------------- expressions
class X:
    """Expression tree.  Every value is a bit pattern of width w (8/16/32).
    `exact` is True when the rendered C value is already within [0, 2^w)."""
    __slots__ = ('k', 'w', 'a', 'b', 'v', 'mem')

    def __init__(self, k, w, a=None, b=None, v=None, mem=False):
        self.k, self.w, self.a, self.b, self.v = k, w, a, b, v
        self.mem = mem or (a is not None and isinstance(a, X) and a.mem) or \
            (b is not None and isinstance(b, X) and b.mem)

    def __repr__(self):
        return render(self)


def C(v, w=16):
    return X('const', w, v=v & ((1 << w) - 1))


def V(name, w=16):
    return X('var', w, v=name)


def is_const(x, val=None):
    return x.k == 'const' and (val is None or x.v == val)


U = {8: 'u8', 16: 'u16', 32: 'u32'}
S = {8: 'i8', 16: 'i16', 32: 'i32'}


def exact(x):
    if x.k in ('const', 'var', 'g', 'fpm', 'memx', 'sx_zero', 'call', 'dsp', 'segof', 'offof', 'lo', 'hi',
               'zx', 'cast', 'loc'):
        return True
    if x.k == 'bin' and x.v in ('&', '|', '^', '>>u'):
        return exact(x.a) and exact(x.b)
    if x.k == 'bin' and x.v in ('/u', '%u'):
        return True
    return False


def cu(x, w=None):
    """render as unsigned value of width w (cast if needed)"""
    w = w or x.w
    s = render(x)
    if exact(x) and x.w <= w:
        return s
    return '(%s)%s' % (U[w], paren(s))


def cs(x, w=None):
    """render as signed value of width w"""
    w = w or x.w
    if x.k == 'const':
        v = x.v
        if v >= 1 << (w - 1):
            v -= 1 << w
        return str(v)
    return '(%s)%s' % (S[w], paren(render(x)))


def paren(s):
    if re.fullmatch(r'[A-Za-z_][A-Za-z_0-9]*|-?\d+|0x[0-9A-Fa-f]+|[A-Za-z_0-9]+\([^()]*\)', s):
        return s
    depth = 0
    if s.startswith('(') and s.endswith(')'):
        for n, ch in enumerate(s):
            depth += ch == '('
            depth -= ch == ')'
            if depth == 0 and n != len(s) - 1:
                break
        else:
            return s
    return '(' + s + ')'


def hexc(v):
    return '0x%X' % v if v > 9 else str(v)


def render(x):
    k = x.k
    if k == 'const':
        return hexc(x.v)
    if k == 'var':
        if x.v.startswith('__dead_') or x.v.startswith('__mach_'):
            raise Unsupported('use of clobbered register ' + x.v.split('_')[-1])
        if x.v.startswith('__entry_'):
            raise Unsupported('reads register value from caller: ' + x.v[8:])
        return x.v
    if k == 'g':                      # DGROUP memory: G16(off)
        if x.a.k == 'const' and (x.a.v, x.w) in GLOBAL_NAMES:
            if (x.a.v, x.w) in ARRAY_GLOBALS:
                return '%s(0)' % GLOBAL_NAMES[(x.a.v, x.w)]
            return GLOBAL_NAMES[(x.a.v, x.w)]
        if x.a.k == 'const' and x.w == 16:
            for o, half in ((x.a.v, 'lo'), (x.a.v - 2, 'hi')):
                if (o, 32) in GLOBAL_NAMES and (o, 32) not in ARRAY_GLOBALS:
                    GLOBAL_HALVES.add((o, half))
                    return '%s_%s' % (GLOBAL_NAMES[(o, 32)], half)
            for k in range(0, 8):
                for half, d in (('lo', 0), ('hi', 2)):
                    o = x.a.v - d - 4 * k
                    if (o, 32) in ARRAY_GLOBALS:
                        ARRAY_HALVES.add((o, half))
                        return '%s_%s(%d)' % (GLOBAL_NAMES[(o, 32)], half, k)
        if x.a.k == 'const':
            r = const_array_element(x.a.v, x.w)
            if r is not None:
                return r
        r = render_struct(x)
        if r is not None:
            return r
        r = render_global_array(x)
        if r is not None:
            return r
        r = render_ds_record(x)
        if r is not None:
            return r
        return 'G%d(%s)' % (x.w, roff(x.a))
    if k == 'fpm':                    # far pointer deref: FP16(p, k)
        r = render_struct(x)
        if r is not None:
            return r
        r = render_record(x)
        if r is not None:
            return r
        return 'FP%d(%s, %s)' % (x.w, render(x.a), roff(x.b))
    if k == 'memx':
        return 'MEM%d(%s, %s)' % (x.w, cu(x.a, 16), roff(x.b))
    if k == 'loc':
        if x.v < 0:
            return 'ARG%d(%s)' % (x.w, hexc(-x.v))
        return 'LOC%d(%s)' % (x.w, hexc(x.v))
    if k == 'dsp':
        s = DGROUP_STRINGS.get(x.v)
        if s is not None:
            return 'DSP(%s) /* %s */' % (hexc(x.v), s)
        return 'DSP(%s)' % hexc(x.v)
    if k == 'call':
        return x.v
    if k == 'bin':
        op = x.v
        if op in ('+', '-', '*', '&', '|', '^'):
            return '%s %s %s' % (paren(render(x.a)), op, paren(render(x.b)))
        if op == '<<':
            return '%s << %s' % (paren(render(x.a)), paren(render(x.b)))
        if op == '>>u':
            return '%s >> %s' % (paren(cu(x.a)), paren(render(x.b)))
        if op == '>>s':
            return '%s >> %s' % (paren(cs(x.a)), paren(render(x.b)))
        if op == '/u':
            return '%s / %s' % (paren(cu(x.a)), paren(cu(x.b)))
        if op == '%u':
            return '%s %% %s' % (paren(cu(x.a)), paren(cu(x.b)))
        if op == '/s':
            return '%s / %s' % (paren(cs(x.a)), paren(cs(x.b)))
        if op == '%s':
            return '%s %% %s' % (paren(cs(x.a)), paren(cs(x.b)))
        raise Unsupported('binop ' + op)
    if k == 'un':
        return '%s%s' % (x.v, paren(render(x.a)))
    if k == 'sx':                     # sign-extend a (width a.w) to x.w
        return '(%s)%s' % (U[x.w], cs(x.a))
    if k == 'zx':
        return cu(x.a)
    if k == 'tr':                     # truncate to x.w
        return '(%s)%s' % (U[x.w], paren(render(x.a)))
    if k == 'lo':
        return 'LO16(%s)' % render(x.a)
    if k == 'hi':
        return 'HI16(%s)' % render(x.a)
    if k == 'cat':                    # 32-bit from hi:lo
        ta = table_addr(x)
        if ta is not None:
            ta[0].addr_used = True
            return '%s_at(%s)' % (ta[0].name, ', '.join(render(i) for i in ta[1]))
        return 'MK32(%s, %s)' % (render(x.a), render(x.b))
    if k == 'segof':
        return 'FPSEG(%s)' % render(x.a)
    if k == 'offof':
        return 'FPOFF(%s)' % render(x.a)
    if k == 'cast':
        return '(%s)%s' % (x.v, paren(render(x.a)))
    raise Unsupported('render ' + k)


def field_ref(fld):
    name, idx = fld
    return name + ''.join('[%s]' % render(i) for i in idx)


def struct_index(terms, k):
    e = None
    for t, m in terms:
        v = t if m == 1 else B('*', t, C(m))
        e = v if e is None else B('+', e, v)
    if e is None:
        return C(k)
    return B('+', e, C(k)) if k else e


def table_addr(x):
    """(table, index expressions) if 32-bit value x is the far address of a
    table element: MK32(DSEG, base + i*size) or MK32(FPSEG(p), FPOFF(p) + i*size)"""
    if not STRUCTS or x.k != 'cat':
        return None
    hi, lo = x.a, x.b
    if hi.k == 'var' and hi.v == 'DSEG':
        terms, const = structs.flatten(lo)
        cands = [t for t in STRUCTS if not t.far]
    elif hi.k == 'segof' and hi.a.k == 'g' and hi.a.w == 32 and hi.a.a.k == 'const':
        terms, const = structs.flatten(lo)
        pr = render(hi.a)
        rest = [(e, c) for e, c in terms if not (e.k == 'offof' and c == 1 and render(e.a) == pr)]
        if len(rest) != len(terms) - 1:
            return None
        terms = rest
        cands = [t for t in STRUCTS if t.far and t.base == hi.a.a.v]
    else:
        return None
    best = None
    for t in cands:
        idx = structs.match_addr(t, terms, const, struct_index)
        if idx is not None and (best is None or t.base > best[0].base):
            best = (t, idx)
    return best


def const_array_element(c, w):
    """constant address inside a named array global: name(k) -- the nearest
    named global below c must be that array"""
    import bisect
    if '_sorted' not in GLOBAL_INDEX or GLOBAL_INDEX['_n'] != len(GLOBAL_NAMES):
        GLOBAL_INDEX['_sorted'] = sorted(GLOBAL_NAMES)
        GLOBAL_INDEX['_n'] = len(GLOBAL_NAMES)
    keys = GLOBAL_INDEX['_sorted']
    n = bisect.bisect_right(keys, (c, 99))
    best = keys[n - 1] if n else None
    if best is None or best[1] != w or best not in ARRAY_GLOBALS or best[0] == c:
        return None
    k, rem = divmod(c - best[0], w // 8)
    if rem or k > 1024:
        return None
    return '%s(%d)' % (GLOBAL_NAMES[best], k)


def render_global_array(x):
    """G16((i << 1) + base) with a named global at base -> name[i]"""
    if not GLOBAL_NAMES:
        return None
    terms, const = structs.flatten(x.a)
    if not terms:
        return None
    stride = x.w // 8
    if any(c % stride or c > 4 * stride for _, c in terms):
        return None
    best = None
    for k in range(0, 5):
        key = ((const - k * stride) & 0xFFFF, x.w)
        # a base below the address only counts for a confirmed array
        if key in GLOBAL_NAMES and (k == 0 or key in ARRAY_GLOBALS):
            best = (key, k)
            break
    if best is None:
        if x.w == 16 and all(c % 4 == 0 for _, c in terms):
            for k in range(0, 5):
                for half, d in (('lo', 0), ('hi', 2)):
                    key32 = ((const - d - 4 * k) & 0xFFFF, 32)
                    if key32 in ARRAY_GLOBALS:
                        ARRAY_HALVES.add((key32[0], half))
                        idx = struct_index([(e, c // 4) for e, c in terms], k)
                        return '%s_%s(%s)' % (GLOBAL_NAMES[key32], half, render(idx))
        return None
    key, k = best
    if key not in ARRAY_GLOBALS:
        ARRAY_PENDING.add(key)
        return None
    idx = struct_index([(e, c // stride) for e, c in terms], k)
    return '%s(%s)' % (GLOBAL_NAMES[key], render(idx))


def render_struct(x):
    """field of a record table from names/structs.txt: table(i).field"""
    if not STRUCTS:
        return None
    if x.k == 'fpm' and x.a.k == 'var' and x.a.v in PTR_TYPES:
        t = PTR_TYPES[x.a.v]
        terms, const = structs.flatten(x.b)
        if not 0 <= const < t.size:
            return None
        fld = structs.resolve_field(t, const, x.w, terms, struct_index)
        if fld is None or fld[1]:
            return None                  # array fields through pointers stay FP(p, off)
        t.used = True
        return 'AT(%s, %s).%s' % (t.struct, x.a.v, field_ref(fld))
    if x.k == 'g':
        terms, const = structs.flatten(x.a)
        cands = [t for t in STRUCTS if not t.far]
    elif x.a.k == 'g' and x.a.w == 32 and x.a.a.k == 'const':
        terms, const = structs.flatten(x.b)
        cands = [t for t in STRUCTS if t.far and t.base == x.a.a.v]
    else:
        return None
    best = None
    for t in cands:
        m = structs.match(t, terms, const, x.w, struct_index)
        if m is not None and (best is None or t.base > best[0].base):
            best = (t, m)
    if best is None:
        return None
    t, (idx, fld) = best
    if fld[1]:
        # array field: one macro computing the whole 16-bit offset (exact wrap)
        if t.array_used is None:
            t.array_used = set()
        t.array_used.add(fld[0])
        return '%s_%s(%s)' % (t.name, fld[0], ', '.join(render(i) for i in idx + fld[1]))
    t.used = True
    return '%s(%s).%s' % (t.name, ', '.join(render(i) for i in idx), field_ref(fld))


def render_record(x):
    """FPw(G32(base), idx*size + off) -> name(idx) when the field is named"""
    if not REC_NAMES or x.a.k != 'g' or x.a.w != 32 or x.a.a.k != 'const':
        return None
    base = x.a.a.v
    off, idx_t = 0, x.b
    if idx_t.k == 'bin' and idx_t.v == '+' and idx_t.b.k == 'const':
        off, idx_t = idx_t.b.v, idx_t.a
    size = None
    if idx_t.k == 'bin' and idx_t.v == '*' and idx_t.b.k == 'const':
        size, idx = idx_t.b.v, idx_t.a
    elif idx_t.k == 'bin' and idx_t.v == '<<' and idx_t.b.k == 'const':
        size, idx = 1 << idx_t.b.v, idx_t.a
    if size is None:
        return None
    nm = REC_NAMES.get((base, size, off, x.w))
    if nm is None:
        half = rec_half(base, size, off, x.w, 'far')
        if half is None:
            return None
        return '%s(%s)' % (half, render(idx))
    REC_USED[(base, size, off, x.w)] = 'far'
    return '%s(%s)' % (nm, render(idx))


def rec_half(base, size, off, w, kind):
    """16-bit access to the low/high word of a named 32-bit field -> name_lo / name_hi"""
    if w != 16:
        return None
    for o, suf in ((off, 'lo'), (off - 2, 'hi')):
        nm = REC_NAMES.get((base, size, o, 32))
        if nm is not None and REC_USED.get((base, size, o, 32, suf), kind) == kind:
            REC_USED[(base, size, o, 32, suf)] = kind
            return '%s_%s' % (nm, suf)
    return None


def render_ds_record(x):
    """Gw(base + idx*size + off) for a named field of a static DGROUP array"""
    if not REC_NAMES:
        return None
    e = x.a
    if not (e.k == 'bin' and e.v == '+' and e.b.k == 'const'):
        return None
    c, t = e.b.v, e.a
    if t.k == 'bin' and t.v == '*' and t.b.k == 'const':
        size, idx = t.b.v, t.a
    elif t.k == 'bin' and t.v == '<<' and t.b.k == 'const':
        size, idx = 1 << t.b.v, t.a
    else:
        return None
    best = None
    for (base, rsz, off, w), nm in REC_NAMES.items():
        if rsz == size and w == x.w and base + off == c and REC_USED.get((base, rsz, off, w)) != 'far':
            if best is None or base > best[0][0]:
                best = ((base, rsz, off, w), nm)
    if best is None:
        for (base, rsz, off, w), nm in REC_NAMES.items():
            if rsz == size and w == 32 and x.w == 16 and c in (base + off, base + off + 2):
                half = rec_half(base, rsz, c - base, 16, 'ds')
                if half is not None:
                    return '%s(%s)' % (half, render(idx))
        return None
    REC_USED[best[0]] = 'ds'
    return '%s(%s)' % (best[1], render(idx))


def roff(x):
    """offset expression inside an address macro (macros apply u16 wrap)"""
    return render(x)


# ---- constructors with light simplification ----
def B(op, a, b, w=None):
    w = w or max(a.w, b.w)
    if is_const(a) and is_const(b):
        m = (1 << w) - 1
        av, bv = a.v, b.v
        if op == '+': return C(av + bv, w)
        if op == '-': return C(av - bv, w)
        if op == '*': return C(av * bv, w)
        if op == '&': return C(av & bv, w)
        if op == '|': return C(av | bv, w)
        if op == '^': return C(av ^ bv, w)
        if op == '<<': return C(av << bv, w)
        if op == '>>u': return C((av & m) >> bv, w)
    if op in ('+', '|', '^', '-', '<<', '>>u', '>>s') and is_const(b, 0):
        return a
    if op == '+' and is_const(a, 0):
        return b
    if op == '*' and is_const(b, 1):
        return a
    if op == '&' and is_const(b, (1 << w) - 1) and exact(a) and a.w <= w:
        return a
    if op == '+' and a.k == 'bin' and a.v == '+' and is_const(a.b) and is_const(b):
        return B('+', a.a, C(a.b.v + b.v, w), w)
    if op == '<<' and a.k == 'bin' and a.v == '<<' and is_const(a.b) and is_const(b):
        return B('<<', a.a, C(a.b.v + b.v, w), w)
    return X('bin', w, a, b, v=op)


def NOT(a): return X('un', a.w, a, v='~')
def NEG(a): return X('un', a.w, a, v='-')
def SX(a, w): return X('sx', w, a)
def ZX(a, w): return X('zx', w, a) if exact(a) else X('tr', a.w, a)  # value already fits
def TR(a, w):
    if a.k == 'const':
        return C(a.v, w)
    if a.k in ('sx', 'zx') and a.a.w == w:
        return a.a
    if a.k == 'cat' and w == 16:
        return a.b
    return X('tr', w, a)
def LO(a):
    if a.k == 'cat': return a.b
    if a.k == 'const': return C(a.v & 0xFFFF)
    return X('lo', 16, a)
def HI(a):
    if a.k == 'cat': return a.a
    if a.k == 'const': return C(a.v >> 16)
    return X('hi', 16, a)
def split_const(e):
    """expression -> (non-constant part rendered or None, constant)"""
    if e.k == 'const':
        return None, e.v
    if e.k == 'bin' and e.v == '+' and e.b.k == 'const':
        r, c = split_const(e.a)
        return r, c + e.b.v
    return render(e), 0


def adjacent(hi, lo):
    """hi is the 16-bit word right after lo (same base expression, offset + 2)"""
    hr, hc = split_const(hi)
    lr, lc = split_const(lo)
    return hr == lr and ((hc - lc) & 0xFFFF) == 2


def CAT(hi, lo):
    if hi.w == 16 and lo.w == 16 and hi.k == lo.k and hi.mem and lo.mem:
        if hi.k == 'g' and adjacent(hi.a, lo.a):
            return X('g', 32, lo.a, mem=True)
        if hi.k in ('fpm', 'memx') and render(hi.a) == render(lo.a) and adjacent(hi.b, lo.b):
            return X(hi.k, 32, lo.a, lo.b, mem=True)
        if hi.k == 'loc' and hi.v == lo.v - 2:
            return X('loc', 32, v=lo.v, mem=True)
    if hi.k == 'hi' and lo.k == 'lo' and (hi.a is lo.a or render(hi.a) == render(lo.a)):
        return hi.a
    if hi.k == 'segof' and lo.k == 'offof' and render(hi.a) == render(lo.a):
        return hi.a
    if is_const(hi) and is_const(lo):
        return C((hi.v << 16) | lo.v, 32)
    if hi.k == 'var' and hi.v == 'DSEG' and lo.k == 'const':
        return X('dsp', 32, v=lo.v)
    return X('cat', 32, hi, lo)
def SEGOF(p):
    if p.k == 'cat': return p.a
    if p.k == 'dsp': return V('DSEG')
    return X('segof', 16, p)
def OFFOF(p):
    if p.k == 'cat': return p.b
    if p.k == 'dsp': return C(p.v)
    return X('offof', 16, p)


# ---------------------------------------------------------------- prototypes
class Proto:
    def __init__(self):
        self.nwords = None         # stack argument words (None = unknown)
        self.params = []           # list of (name, width 16|32, word index)
        self.ret = 16              # 0, 16 or 32
        self.varargs = False
        self.frame = None
        self.regs = []             # registers the caller sets (passed as extra parameters)
        self.stack_abi = False     # takes the address of an argument: arguments stay on the emulated stack


class Frame:
    """Stack frame layout derived from the prologue."""
    def __init__(self):
        self.has_bp = False
        self.pre = []              # registers pushed before 'push bp'
        self.post = []             # registers pushed after the bp setup
        self.locals = 0            # bytes reserved by sub sp
        self.prologue = set()      # instruction offsets that form the prologue
        self.argbase = 0           # bp offset of first argument


def analyze_frame(f):
    fr = Frame()
    o = f.off
    seq = []
    while o in f.insns and len(seq) < 12:
        i = f.insns[o]
        seq.append(i)
        o = i.next
    n = 0
    def isreg(i, op, name):
        return i.op == op and i.ops and i.ops[0].kind == 'reg' and i.ops[0].name == name
    while n < len(seq) and seq[n].op == 'push' and seq[n].ops[0].kind == 'reg' and seq[n].ops[0].name in ('si', 'di'):
        fr.pre.append(seq[n].ops[0].name); fr.prologue.add(seq[n].off); n += 1
    if n + 1 < len(seq) and isreg(seq[n], 'push', 'bp') and seq[n + 1].op == 'mov' and \
            seq[n + 1].ops[0].kind == 'reg' and seq[n + 1].ops[0].name == 'bp' and \
            seq[n + 1].ops[1].kind == 'reg' and seq[n + 1].ops[1].name == 'sp':
        fr.has_bp = True
        fr.prologue.update((seq[n].off, seq[n + 1].off))
        n += 2
        if n < len(seq) and seq[n].op == 'sub' and isreg(seq[n], 'sub', 'sp') and seq[n].ops[1].kind == 'imm':
            fr.locals = seq[n].ops[1].val; fr.prologue.add(seq[n].off); n += 1
        elif n + 1 < len(seq) and isreg(seq[n], 'dec', 'sp') and isreg(seq[n + 1], 'dec', 'sp'):
            fr.locals = 2; fr.prologue.update((seq[n].off, seq[n + 1].off)); n += 2
        while n < len(seq) and seq[n].op == 'push' and seq[n].ops[0].kind == 'reg' and seq[n].ops[0].name in ('si', 'di'):
            fr.post.append(seq[n].ops[0].name); fr.prologue.add(seq[n].off); n += 1
    far = any(i.op == 'retf' for i in f.insns.values())
    fr.argbase = 2 + 2 * len(fr.pre) + (4 if far else 2)     # saved bp + return address
    return fr


# ---------------------------------------------------------------- decompiler
class Decompiler:
    def __init__(self, exe, names_path):
        self.L = lift.Lifter(exe)
        self.L.build_all()
        self.L.flags_fixpoint()
        self.names = {int(k, 16): v for k, v in json.load(open(names_path)).items()}
        self.RF = regflow.RegFlow(self.L, self.names).run()
        self.protos = {}
        self.ok = {}           # key -> C source
        self.fail = {}         # key -> reason
        self.img = self.L.img
        self.strings = {}
        load_strings(self.img)

    def name_of(self, k):
        return self.names.get(k[0] * 16 + k[1])

    # ---- prototypes ----
    def infer_returns(self):
        """least fixpoint: a function returns AX (DX:AX) only if some caller
        reads AX (DX) after calling it before overwriting it"""
        ret = collections.defaultdict(int)
        for it in range(50):
            self.RF.ret_use = {k: (BIT['ax'] | BIT['dx'] if ret[k] == 32 else BIT['ax'] if ret[k] == 16 else 0)
                               for k in self.L.funcs}
            # the program entry's callees (main) feed exit(): keep conservative there
            self.RF.violations = []; self.RF.exceptions = []
            self.RF.run()
            new = collections.defaultdict(int)
            for ck, cf in self.L.funcs.items():
                lo = self.RF.live_out[ck]
                for co, ci in cf.insns.items():
                    if ci.op in ('call', 'callf'):
                        t = self.L.call_target(ci)
                        mw = self.RF.mayw.get(t, regflow.CLOBBER)
                        if lo[co] & BIT['dx'] & mw:
                            new[t] = 32
                        elif lo[co] & BIT['ax'] & mw and new[t] < 16:
                            new[t] = 16
            for k in new:
                new[k] = max(new[k], ret[k])
            if dict(new) == {k: v for k, v in ret.items() if v}:
                break
            ret = new
        self.ret_type = ret
        print('return types after %d iterations: %d x 32-bit, %d x 16-bit' %
              (it + 1, sum(1 for v in ret.values() if v == 32), sum(1 for v in ret.values() if v == 16)))

    def infer_protos(self):
        self.infer_returns()
        cleanup = collections.defaultdict(set)
        for k, f in self.L.funcs.items():
            for o, i in f.insns.items():
                if i.op in ('callf', 'call'):
                    t = self.L.call_target(i)
                    cleanup[t].add(self.arg_bytes_after(f, i))
        for k, f in self.L.funcs.items():
            p = Proto()
            fr = analyze_frame(f)
            p.frame = fr
            sizes = {}
            if fr.has_bp:
                for i in f.insns.values():
                    for opd in i.ops:
                        if opd.kind == 'mem' and opd.base == ('bp',) and opd.disp >= fr.argbase:
                            idx = (opd.disp - fr.argbase) // 2
                            wd = 32 if i.op in ('les', 'lds') else opd.w
                            sizes[idx] = max(sizes.get(idx, 0), wd)
            if fr.has_bp:
                for lo_word in self.arg_pairs(f, fr):
                    if sizes.get(lo_word, 16) <= 16 and sizes.get(lo_word + 1, 16) <= 16 and \
                            sizes.get(lo_word - 1) != 32:
                        sizes[lo_word] = 32
                        sizes.pop(lo_word + 1, None)
            cl = cleanup.get(k, set())
            known = [c for c in cl if c]
            hi = max(sizes) + (2 if sizes[max(sizes)] == 32 else 1) if sizes else 0
            nw = max(known) // 2 if known else 0
            # variadic: callers pass more words than the function itself declares
            if len(set(known)) > 1 and max(known) // 2 > hi and min(known) // 2 >= hi:
                p.varargs = True
            if nw < hi:
                nw = hi
            p.nwords = nw
            idx = 0
            while idx < nw:
                if sizes.get(idx) == 32:
                    p.params.append(('a%d' % idx, 32, idx)); idx += 2
                else:
                    p.params.append(('a%d' % idx, 16, idx)); idx += 1
            p.ret = self.ret_type[k]
            p.far = any(i.op == 'retf' for i in f.insns.values())
            # &argument (lea r, [bp+N] above the return address): the arguments
            # must live in emulated memory where the original has them
            p.stack_abi = fr.has_bp and any(
                i.op == 'lea' and i.ops[1].kind == 'mem' and 'bp' in i.ops[1].base and i.ops[1].disp >= fr.argbase
                for i in f.insns.values())
            p.near = any(i.op == 'ret' for i in f.insns.values())
            self.protos[k] = p


    @staticmethod
    def arg_pairs(f, fr):
        """argument words a function uses as the low half of a 32-bit value:
        loaded into DX:AX by adjacent instructions, or used by add/adc, sub/sbb
        (les is handled by the caller)"""
        out = set()
        def argw(o):
            if o.kind == 'mem' and o.base == ('bp',) and o.disp >= fr.argbase and o.w == 16 and \
                    (o.disp - fr.argbase) % 2 == 0:
                return (o.disp - fr.argbase) // 2
            return None
        for o, i in f.insns.items():
            j = f.insns.get(i.next)
            if j is None:
                continue
            pairs = []
            if i.op == j.op == 'mov' and i.ops[0].kind == j.ops[0].kind == 'reg' and \
                    {i.ops[0].name, j.ops[0].name} == {'ax', 'dx'}:
                # DX:AX = long argument (the Turbo C long convention)
                lo_i, hi_i = (i, j) if i.ops[0].name == 'ax' else (j, i)
                pairs.append((argw(lo_i.ops[1]), argw(hi_i.ops[1])))
            elif (i.op, j.op) in (('add', 'adc'), ('sub', 'sbb')):
                pairs.append((argw(i.ops[1]), argw(j.ops[1])))
                pairs.append((argw(i.ops[0]), argw(j.ops[0])))
            for a, b in pairs:
                if a is not None and b is not None and b == a + 1:
                    out.add(a)
        return out

    def arg_bytes_after(self, f, i):
        """bytes of arguments removed after a call (add sp,N / pop cx / inc sp)"""
        o = i.next
        j = f.insns.get(o)
        if j is None:
            return None
        if j.op == 'add' and j.ops[0].kind == 'reg' and j.ops[0].name == 'sp' and j.ops[1].kind == 'imm':
            return j.ops[1].val
        if j.op == 'pop' and j.ops[0].kind == 'reg' and j.ops[0].name == 'cx':
            j2 = f.insns.get(j.next)
            if j2 is not None and j2.op == 'pop' and j2.ops[0].kind == 'reg' and j2.ops[0].name == 'cx':
                return 4
            return 2
        if j.op == 'inc' and j.ops[0].kind == 'reg' and j.ops[0].name == 'sp':
            j2 = f.insns.get(j.next)
            if j2 is not None and j2.op == 'inc' and j2.ops[0].name == 'sp':
                return 2
        if j.op == 'mov' and j.ops[0].kind == 'reg' and j.ops[0].name == 'sp' and j.ops[1].kind == 'reg' and j.ops[1].name == 'bp':
            return -1                               # all pending words dropped
        return 0

    def passthru_lifted(self, key):
        """registers a machine-level function reads at entry only to return
        them unchanged: live at entry with its return use, dead without"""
        if not hasattr(self, '_pt_lifted'):
            self._pt_lifted = {}
        if key not in self._pt_lifted:
            f = self.L.funcs.get(key)
            res = set()
            if f is not None:
                saved = self.RF.ret_use.get(key)
                self.RF.ret_use[key] = 0
                try:
                    lin, _ = self.RF.liveness(f)
                finally:
                    if saved is None:
                        del self.RF.ret_use[key]
                    else:
                        self.RF.ret_use[key] = saved
                noret = lin[f.off]
                for r in ('ax', 'bx', 'cx', 'dx', 'es'):
                    if self.RF.reg_in.get(key, 0) & BIT[r] and not noret & BIT[r]:
                        res.add(r)
            self._pt_lifted[key] = res
        return self._pt_lifted[key]

    def passthru_regs(self, key):
        """register parameters of decompiled function key that only flow into
        its return value (rv_ = ax; / return ax;)"""
        if not hasattr(self, '_passthru'):
            self._passthru, self._pt_busy = {}, set()
        if key in self._passthru:
            return self._passthru[key]
        if key in self._pt_busy or key not in self.decomp_set:
            return set()
        self._pt_busy.add(key)
        try:
            txt = self.decompile(key)
        except Unsupported:
            txt = None
        finally:
            self._pt_busy.discard(key)
        res = set()
        if txt is not None:
            body = txt.split('{', 1)[1]
            for r in self.protos[key].regs:
                reads = [l for l in body.split('\n') if re.search(r'\b%s\b' % r, l)
                         and not re.match(r'\s*%s = ' % r, l)]
                if reads and all(re.fullmatch(r'\s*(rv_ = %s|return %s);' % (r, r), l) for l in reads):
                    res.add(r)
        self._passthru[key] = res
        return res

    # ---- one function ----
    def decompile(self, key):
        f = self.L.funcs[key]
        p = self.protos[key]
        PTR_TYPES.clear()
        fn = FuncGen(self, f, p)
        src = fn.generate()
        # pointer locals that only ever hold the address of one table's
        # elements: generate again with them typed (AT(struct, p).field)
        by_name = {t.name: t for t in STRUCTS}
        typed = {v: by_name[next(iter(s))] for v, s in fn.ptr_seen.items() if len(s) == 1 and None not in s}
        if typed:
            PTR_TYPES.update(typed)
            try:
                src = FuncGen(self, f, p).generate()
            finally:
                PTR_TYPES.clear()
        return src


class FuncGen:
    def __init__(self, D, f, proto):
        self.ptr_seen = collections.defaultdict(set)   # pointer local -> tables it is assigned
        self.D, self.f, self.p = D, f, proto
        self.fr = proto.frame
        self.key = (f.seg, f.off)
        self.vars = {}               # name -> c type
        self.tmpn = 0
        self.lines = []
        self.live = D.RF.live_out[self.key]

    def newtmp(self, w):
        self.tmpn += 1
        n = 't%d' % self.tmpn
        self.vars[n] = U[w]
        return n

    # stack-relative operand classification
    def stackvar(self, opd, w):
        d = opd.disp
        fr = self.fr
        if d >= fr.argbase and self.p.stack_abi:
            return ('frame', None, -d)              # ARG(d): the caller's pushed argument
        if d >= fr.argbase:
            idx = (d - fr.argbase) // 2
            if (d - fr.argbase) % 2:
                raise Unsupported('odd arg offset')
            for name, pw, pidx in self.p.params:
                if pidx == idx:
                    if pw == 32:
                        return ('arg32', name, 0)
                    return ('arg16', name, 0)
                if pw == 32 and pidx + 1 == idx:
                    return ('arg32', name, 1)
            raise Unsupported('arg beyond prototype')
        if d < 0:
            off = -d
            if getattr(self, 'frame_mode', False):
                return ('frame', None, off)
            for lo in getattr(self, 'local32', ()):
                if lo <= d < lo + 4:
                    return ('local32', 'l_%X' % -lo, d - lo)
            name = 'l_%X' % off
            return ('local', name, off)
        raise Unsupported('bp+%d inside frame' % d)

    def find_local32(self, acc):
        """stack slot pairs (d, d+2) the code uses as one 32-bit value (far
        pointer or long): les/lds from it, or both halves stored/loaded by
        adjacent instructions from/into DX:AX or with a segment register"""
        f = self.f
        cand = set()
        def slot(o):
            return o.disp if o.kind == 'mem' and o.base == ('bp',) and o.disp < 0 and o.w == 16 else None
        for o_, i in f.insns.items():
            if i.op in ('les', 'lds') and i.ops[1].kind == 'mem' and i.ops[1].base == ('bp',) and i.ops[1].disp < 0:
                cand.add(i.ops[1].disp)
            j = f.insns.get(i.next)
            if j is None or i.op != 'mov' or j.op != 'mov':
                continue
            for a_, b_ in ((i, j), (j, i)):
                da, db = slot(a_.ops[0]), slot(b_.ops[0])          # stores
                if da is not None and db == da + 2:
                    srcs = {a_.ops[1].kind, b_.ops[1].kind}
                    names = {getattr(a_.ops[1], 'name', None), getattr(b_.ops[1], 'name', None)}
                    if 'sreg' in srcs or names == {'ax', 'dx'}:
                        cand.add(da)
                da, db = slot(a_.ops[1]), slot(b_.ops[1])          # loads into DX:AX
                if da is not None and db == da + 2 and a_.ops[0].kind == b_.ops[0].kind == 'reg' and \
                        (a_.ops[0].name, b_.ops[0].name) == ('ax', 'dx'):
                    cand.add(da)
        out = set()
        for d in sorted(cand):
            if d + 2 >= 0 or d - 2 in out:
                continue
            # no other variable may overlap the 4 bytes
            if any(x in acc for x in (d + 1, d + 3)) or (d - 1 in acc and 16 in acc[d - 1]):
                continue
            out.add(d)
        return out

    def generate(self):
        f = self.f
        fr = self.fr
        if not fr.has_bp and any(o.kind == 'mem' and 'bp' in o.base for i in f.insns.values() for o in i.ops):
            raise Unsupported('bp use without frame')
        for i in f.insns.values():
            if i.op in ('int', 'in', 'out', 'hlt', 'cli', 'sti', 'iret', 'pushf', 'popf', 'lds', 'jmpf',
                        'jmpfi', 'esc', 'xlat', 'lahf', 'sahf', 'daa', 'das', 'aaa', 'aas', 'aam', 'aad',
                        'pusha', 'popa', 'enter', 'leave', 'into', 'rol', 'ror'):
                raise Unsupported('instruction ' + i.op)
            if i.op == 'jmpi' and i.off not in f.jumptables:
                raise Unsupported('indirect jump')
            if i.op == 'lea' and i.ops[1].kind == 'mem' and 'bp' in i.ops[1].base and \
                    (i.ops[1].base != ('bp',) or i.ops[1].disp >= 0) and not self.p.stack_abi:
                raise Unsupported('address of argument / indexed local')
            if i.op == 'mov' and i.ops[0].kind == 'sreg' and i.ops[0].name in ('ds', 'ss'):
                raise Unsupported('segment register change')
            if i.op in ('movs', 'cmps', 'stos', 'lods', 'scas'):
                raise Unsupported('string op')
            for n, o in enumerate(i.ops):
                if o.kind == 'mem' and o.seg == 'cs' and n == 0 and i.op not in ('cmp', 'test', 'push', 'jmpi'):
                    raise Unsupported('write to cs: data')
        self.frame_mode = self.p.stack_abi or any(
            i.op == 'lea' and i.ops[1].kind == 'mem' and i.ops[1].base == ('bp',)
            and i.ops[1].disp < 0 for i in f.insns.values())
        # local slots: (start, width).  Word variables cover [d, d+2); a dword
        # access is two word variables.  Any partial overlap -> memory frame.
        acc = collections.defaultdict(set)
        for i in f.insns.values():
            for n, o in enumerate(i.ops):
                if o.kind == 'mem' and o.base == ('bp',) and o.disp < 0:
                    wd = 32 if i.op in ('les', 'lds') else o.w
                    if i.op == 'lea':
                        continue
                    if wd == 32:
                        acc[o.disp].add(16); acc[o.disp + 2].add(16)
                    else:
                        acc[o.disp].add(wd)
        # a local read before it is written on some path (uninitialized
        # variable in the original source) must see the same stale stack
        # bytes as the original: keep the frame in emulated memory
        if not self.frame_mode and self.reads_uninitialized_local():
            self.frame_mode = True
        # stack memory addressed through a register (pointer to a local) or a
        # local array indexed by a register ([bp+di-N])
        if any(o.kind == 'mem' and o.base and o.base != ('bp',) and (o.seg == 'ss' or 'bp' in o.base)
               for i in f.insns.values() for o in i.ops):
            self.frame_mode = True
        self.local32 = set() if self.frame_mode else self.find_local32(acc)
        self.byte_locals = set()
        cover = {}
        for d, ws in acc.items():
            span = 2 if 16 in ws else 1
            for b in range(d, d + span):
                if b in cover and cover[b] != d:
                    self.frame_mode = True
                cover[b] = d
            if ws == {8}:
                self.byte_locals.add(d)
        if self.frame_mode:
            self.vars['bp_'] = 'u16'
        self.in_args = {}
        self.in_flags = {}
        self.done_blocks = set()
        self.preds = collections.defaultdict(set)
        for o in f.insns:
            for s_ in self.D.L.succs(f, o):
                if s_ != f.insns[o].next or f.insns[o].op in ('jcc', 'jmp', 'jmpi', 'loop', 'jcxz') or True:
                    self.preds[s_].add(o)
        blocks = self.blocks()
        out = []
        for b in blocks:
            out.append(self.gen_block(b))
            self.done_blocks.add(b[0])
        S = structure.Structurer(out, f.off, self.vars)
        body = S.run()
        in_loop = set()
        for h, members in S.loops.items():
            in_loop |= members
        straight = sum(b.ninsn for b in out if b.id not in in_loop)
        return self.assemble(['    FUNC_TICK(%d);' % straight] + body)

    def reads_uninitialized_local(self):
        f = self.f
        offs = sorted(f.insns)
        slots = set()
        acc = {}
        for o in offs:
            i = f.insns[o]
            rd, wr = set(), set()
            for n, op in enumerate(i.ops):
                if op.kind == 'mem' and op.base == ('bp',) and op.disp < 0 and i.op != 'lea':
                    w = 32 if i.op in ('les', 'lds') else op.w
                    bytes_ = set(range(op.disp, op.disp + w // 8))
                    slots |= bytes_
                    is_dst = n == 0 and i.op in ('mov', 'pop') or (n == 0 and i.op not in ('cmp', 'test', 'push', 'les', 'lds'))
                    if i.op == 'mov' and n == 0:
                        wr |= bytes_
                    elif i.op == 'pop' and n == 0:
                        wr |= bytes_
                    else:
                        rd |= bytes_
            acc[o] = (rd, wr)
        if not slots:
            return False
        full = frozenset(slots)
        inn = {o: full for o in offs}
        inn[f.off] = frozenset()
        changed = True
        while changed:
            changed = False
            for o in offs:
                rd, wr = acc[o]
                out = inn[o] | wr
                for s_ in self.D.L.succs(f, o):
                    if s_ in inn:
                        new = inn[s_] & out
                        if s_ != f.off and new != inn[s_]:
                            inn[s_] = new
                            changed = True
        return any(acc[o][0] - inn[o] for o in offs)

    def blocks(self):
        f = self.f
        offs = sorted(f.insns)
        leaders = {f.off}
        for o in offs:
            i = f.insns[o]
            if i.op in ('jcc', 'jmp', 'loop', 'loope', 'loopne', 'jcxz'):
                leaders.add(i.ops[0].val)
                leaders.add(i.next)
            if i.op == 'jmpi':
                for t in f.jumptables[o]['targets']:
                    leaders.add(t)
            if i.op in ('ret', 'retf'):
                leaders.add(i.next)
        out = []
        cur = None
        for o in offs:
            if o in leaders or cur is None or (cur and f.insns[cur[-1]].next != o):
                cur = []
                out.append(cur)
            cur.append(o)
        return out

    # ------------------------------------------------------------ block translation
    def gen_block(self, offs):
        f = self.f
        st = BlockState(self, offs[0])
        st.block_offs = set(offs)
        lines = []
        st.lines = lines
        n_insn = len(offs)
        if offs[0] == f.off and self.frame_mode:
            lines.append('    bp_ = (u16)(R.sp - 2); R.sp -= %d;   /* locals live in the emulated stack frame */' %
                         (2 + self.fr.locals))
        k = self.in_args.get(offs[0], 0)
        st.args = [V('s%d' % j) for j in range(k)]
        if offs[0] in self.in_flags:
            st.flag = self.in_flags[offs[0]]
        idx = 0
        while idx < len(offs):
            o = offs[idx]
            i = f.insns[o]
            if o in self.fr.prologue:
                idx += 1
                continue
            st.translate(i)
            idx += 1
        st.term = None
        st.finish(offs)
        stmts = [l[4:] if l.startswith('    ') else l for l in lines]
        return structure.Block(offs[0], stmts, st.term, n_insn)

    def assemble(self, body):
        k = self.key
        p = self.p
        rt = {0: 'void', 16: 'u16', 32: 'u32'}[p.ret]
        params = proto_params(p)
        out = ['/* %04X:%04X */' % k]
        nm = self.D.name_of(k)
        if nm:
            out[0] = '/* %04X:%04X %s */' % (k[0], k[1], nm)
        if k in FUNC_DESC:
            out[0] = '/* %04X:%04X  %s */' % (k[0], k[1], FUNC_DESC[k])
        out.append('%s %s(%s) {' % (rt, cname(k), params))
        body = cleanup.run(body, self.vars)
        for v, t in sorted(self.vars.items(), key=lambda kv: (kv[0][0] == 't', len(kv[0]), kv[0])):
            if v not in p.regs:
                out.append('    %s %s;' % (t, v))
        out.extend(body)
        out.append('}')
        return '\n'.join(out) + '\n'


REG16 = ('ax', 'bx', 'cx', 'dx', 'si', 'di', 'es')
HALF = {'al': ('ax', 0), 'ah': ('ax', 1), 'bl': ('bx', 0), 'bh': ('bx', 1),
        'cl': ('cx', 0), 'ch': ('cx', 1), 'dl': ('dx', 0), 'dh': ('dx', 1)}


class BlockState:
    """Symbolic machine state while translating one basic block."""

    def __init__(self, g, start):
        self.g = g
        self.r = {}                  # reg16 -> X (16-bit)
        for r in REG16:
            # at function entry scratch registers hold caller garbage
            if start == g.f.off and r in ('ax', 'bx', 'cx', 'dx', 'es') and r not in g.p.regs:
                self.r[r] = V('__entry_' + r)
            else:
                self.r[r] = V(r)
        self.args = []               # pushed words (X)
        self.flag = None             # (kind, a, b, res, w)
        self.carry_src = None
        self.lines = None
        self.pending_call_words = 0
        self.skip = set()
        self.pre = []
        self.carry_src = None
        self.shift_carry = None

    # ---- emission helpers ----
    def emit(self, s):
        self.lines.append('    ' + s)

    def var(self, name, ctype):
        self.g.vars.setdefault(name, ctype)

    def flush_mem(self, live=None, keep_args=0):
        """materialize pending expressions that read memory (before a store/call).
        Only registers in `live` (and all pending pushed words except the top
        `keep_args`, which the current call consumes) need to survive."""
        for r in REG16:
            if live is not None and not (live & BIT[r]):
                continue
            x = self.r[r]
            if isinstance(x, tuple):
                if x[0].mem or x[1].mem:
                    x = self.get(r)
                else:
                    continue
            if x.mem:
                t = self.g.newtmp(16)
                self.emit('%s = %s;' % (t, render(x)))
                self.r[r] = V(t)
        for n, x in enumerate(self.args[:len(self.args) - keep_args]):
            if x.mem:
                t = self.g.newtmp(16)
                self.emit('%s = %s;' % (t, render(x)))
                self.args[n] = V(t)
        if self.flag and live is None:
            kind, a, b, res, w = self.flag
            if (a is not None and a.mem) or (b is not None and b.mem) or (res is not None and res.mem):
                a2, b2, r2 = a, b, res
                if a is not None and a.mem:
                    t = self.g.newtmp(max(16, a.w)); self.emit('%s = %s;' % (t, render(a))); a2 = V(t, a.w)
                if b is not None and b.mem:
                    t = self.g.newtmp(max(16, b.w)); self.emit('%s = %s;' % (t, render(b))); b2 = V(t, b.w)
                if res is not None and res.mem:
                    t = self.g.newtmp(max(16, res.w)); self.emit('%s = %s;' % (t, render(res))); r2 = V(t, res.w)
                self.flag = (kind, a2, b2, r2, w)

    # ---- register access ----
    # ax..dx are held either whole (X16) or as two byte halves (tuple lo, hi)
    def get(self, name):
        if name in self.r:
            x = self.r[name]
            if isinstance(x, tuple):
                lo, hi = x
                if is_const(hi, 0):
                    return ZX8(lo)
                return B('|', B('<<', ZX8(hi), C(8)), ZX8(lo))
            return x
        if name in HALF:
            r, hi = HALF[name]
            x = self.r[r]
            if isinstance(x, tuple):
                return x[hi]
            if hi:
                return TR(B('>>u', x, C(8)), 8)
            return TR(x, 8)
        if name == 'ds':
            return V('DSEG')
        if name == 'ss':
            return V('R.ss')
        if name == 'cs':
            return V('SEGV(0x%04X)' % self.g.f.seg)
        if name in ('sp', 'bp'):
            raise Unsupported('direct %s use' % name)
        raise Unsupported('reg ' + name)

    def put(self, name, x):
        if name in REG16:
            if x.w == 8:
                x = ZX8(x)
            self.r[name] = x
            return
        if name in HALF:
            r, hi = HALF[name]
            cur = self.r[r]
            if isinstance(cur, tuple):
                lo_x, hi_x = cur
            else:
                dead = cur.k == 'var' and (cur.v.startswith('__dead') or cur.v.startswith('__mach'))
                lo_x = cur if dead else TR(cur, 8)
                hi_x = cur if dead else TR(B('>>u', cur, C(8)), 8)
            if hi:
                hi_x = x
            else:
                lo_x = x
            self.r[r] = (lo_x, hi_x)
            return
        if name == 'es':
            self.r['es'] = x
            return
        raise Unsupported('write ' + name)

    # ---- memory operands ----
    def addr(self, o):
        """(kind, seg, off) for a memory operand"""
        off = C(o.disp & 0xFFFF)
        for b in o.base:
            if b == 'bp':
                continue
            off = B('+', self.get(b), off)
        return off

    def load(self, o, w=None):
        w = w or o.w
        if o.kind in ('reg', 'sreg'):
            return self.get(o.name)
        if o.kind == 'imm':
            if o.reloc:
                if o.val == DGROUP:
                    return V('DSEG')
                return V('SEGV(0x%04X)' % o.val)
            return C(o.val, w)
        if o.kind == 'mem':
            return self.memref(o, w)
        raise Unsupported('operand ' + o.kind)

    def frame_addr(self, o):
        """emulated-stack offset of a bp-relative operand (frame mode)"""
        off = B('-', V('bp_'), C(-o.disp)) if o.disp < 0 else B('+', V('bp_'), C(o.disp))
        for b in o.base:
            if b != 'bp':
                off = B('+', off, self.get(b))
        return off

    def memref(self, o, w):
        if 'bp' in o.base and o.base != ('bp',):
            if not self.g.frame_mode:
                raise Unsupported('indexed stack access')
            return X('memx', w, V('R.ss'), self.frame_addr(o), mem=True)
        if 'bp' in o.base:
            kind, name, part = self.g.stackvar(o, w)
            if kind == 'arg16':
                if w == 8:
                    return TR(V(name), 8)
                if w == 32:
                    nxt = self.g.stackvar(type(o)('mem', 16, seg=o.seg, base=o.base, disp=o.disp + 2), 16)
                    if nxt[0] != 'arg16':
                        raise Unsupported('dword over mixed args')
                    return CAT(V(nxt[1]), V(name))
                return V(name)
            if kind == 'arg32':
                v = V(name, 32)
                if w == 32:
                    return v
                if w == 16:
                    return HI(v) if part else LO(v)
                return TR(HI(v) if part else LO(v), 8)
            if kind == 'frame':
                return X('loc', w, v=part, mem=True)
            if kind == 'local32':
                self.var(name, 'u32')
                v = V(name, 32)
                if w == 32 and part == 0:
                    return v
                if w == 16 and part in (0, 2):
                    return HI(v) if part else LO(v)
                if w == 8:
                    h = HI(v) if part >= 2 else LO(v)
                    return TR(B('>>u', h, C(8)), 8) if part % 2 else TR(h, 8)
                raise Unsupported('misaligned access to a 32-bit local')
            # local slot
            if w == 32:
                lo = self.local(o.disp, 16)
                hi = self.local(o.disp + 2, 16)
                return CAT(hi, lo)
            return self.local(o.disp, w)
        seg = o.seg
        off = self.addr(o)
        if seg == 'ds':
            return X('g', w, off, mem=True)
        if seg == 'ss':
            if not self.g.frame_mode:
                raise Unsupported('ss: non-bp access')
            return X('memx', w, V('R.ss'), off, mem=True)
        if seg == 'es':
            es = self.r['es']
            return self.farmem(es, off, w)
        if seg == 'cs':
            return X('memx', w, V('SEGV(0x%04X)' % self.g.f.seg), off, mem=True)
        raise Unsupported('segment ' + seg)

    def farmem(self, segx, off, w):
        if segx.k == 'var' and segx.v == 'DSEG':
            return X('g', w, off, mem=True)
        # es = SEG(P), off = OFF(P) + k  ->  FP(P, k)
        k = C(0)
        base = off
        if base.k == 'bin' and base.v in ('+', '-') and is_const(base.b):
            k = base.b if base.v == '+' else C(-base.b.v)
            base = base.a
        if segx.k == 'segof' and base.k == 'offof' and render(segx.a) == render(base.a):
            return X('fpm', w, segx.a, k, mem=True)
        if segx.k == 'segof':
            terms = []
            def flat(x):
                if x.k == 'bin' and x.v == '+':
                    flat(x.a); flat(x.b)
                else:
                    terms.append(x)
            flat(off)
            p = render(segx.a)
            for n, t in enumerate(terms):
                if t.k == 'offof' and render(t.a) == p:
                    rest = terms[:n] + terms[n + 1:]
                    acc = rest[0] if rest else C(0)
                    for r in rest[1:]:
                        acc = B('+', acc, r)
                    return X('fpm', w, segx.a, acc, mem=True)
        return X('memx', w, segx, off, mem=True)

    def local(self, disp, w):
        name = 'l_%X' % (-disp) if disp < 0 else None
        if name is None:
            raise Unsupported('local at positive offset')
        if disp in self.g.byte_locals:
            self.var(name, 'u8')
            return V(name, 8)
        self.var(name, 'u16')
        v = V(name)
        if w == 8:
            return TR(v, 8)
        return v

    def store(self, o, x):
        """emit a store of x (width o.w) to operand o"""
        w = o.w
        if o.kind in ('reg',):
            self.put(o.name, x)
            return
        if o.kind == 'sreg':
            if o.name == 'es':
                self.r['es'] = x
                return
            raise Unsupported('store ' + o.name)
        self.flush_mem(live=self.g.live[self.cur.off] | BIT['si'] | BIT['di'])
        if 'bp' in o.base and o.base != ('bp',):
            if not self.g.frame_mode:
                raise Unsupported('indexed stack access')
            m = X('memx', w, V('R.ss'), self.frame_addr(o), mem=True)
            self.preserve_pending_mem()
            self.emit('%s = %s;' % (render(m), render(x)))
            return
        if 'bp' in o.base and self.g.stackvar(o, w)[0] == 'frame':
            m = X('loc', w, v=-o.disp, mem=True)
            self.preserve_pending_mem()
            self.emit('%s = %s;' % (render(m), render(x)))
            return
        if 'bp' in o.base:
            kind, name, part = self.g.stackvar(o, w)
            if kind == 'arg16':
                rhs = ('(u16)((%s & 0xFF00) | %s)' % (name, cu(x, 8))) if w == 8 else render(x)
                self.assign_var(name, rhs)
                return
            if kind == 'arg32':
                if w != 16:
                    raise Unsupported('store into dword arg')
                if part:
                    rhs = 'MK32(%s, LO16(%s))' % (render(x), name)
                else:
                    rhs = 'MK32(HI16(%s), %s)' % (name, render(x))
                self.assign_var(name, rhs)
                return
            if kind == 'local32':
                self.var(name, 'u32')
                if w == 32 and part == 0:
                    ta = table_addr(x)
                    self.g.ptr_seen[name].add(ta[0].name if ta else None)
                    rhs = render(x)
                elif w == 16 and part in (0, 2):
                    rhs = ('MK32(%s, LO16(%s))' % (render(x), name)) if part else ('MK32(HI16(%s), %s)' % (name, render(x)))
                elif w == 8:
                    sh = 8 * part
                    rhs = '(%s & 0x%X) | ((u32)%s << %d)' % (name, 0xFFFFFFFF ^ (0xFF << sh), cu(x, 8), sh)
                else:
                    raise Unsupported('misaligned store to a 32-bit local')
                if not (w == 32 and part == 0):
                    self.g.ptr_seen[name].add(None)
                self.assign_var(name, rhs)
                return
            lname = 'l_%X' % (-o.disp)
            if o.disp in self.g.byte_locals:
                self.var(lname, 'u8')
                self.assign_var(lname, render(x) if x.w == 8 else cu(x, 8))
                return
            self.var(lname, 'u16')
            rhs = ('(u16)((%s & 0xFF00) | %s)' % (lname, cu(x, 8))) if w == 8 else render(x)
            self.assign_var(lname, rhs)
            return
        m = self.memref(o, w)
        self.preserve_pending_mem()
        lv = render(m)
        self.emit('%s = %s;' % (lv, enum_name(enum_of(lv), x) or render(x)))

    # ---- 32-bit values (Turbo C long arithmetic on word pairs) ----
    def next_in_block(self, i):
        j = self.g.f.insns.get(i.next)
        if j is None or j.off not in self.block_offs or j.off in self.skip:
            return None
        return j

    def mem_pair(self, lo, hi):
        if not (lo.kind == 'mem' and hi.kind == 'mem' and lo.w == 16 and hi.w == 16 and
                lo.seg == hi.seg and lo.base == hi.base and hi.disp == lo.disp + 2):
            return False
        if 'bp' not in lo.base:
            return True
        return lo.base == ('bp',) and lo.disp in self.g.local32

    @staticmethod
    def as32(o):
        o2 = copy.copy(o)
        o2.w = 32
        return o2

    def fuse32(self, i):
        """add/adc, sub/sbb and word-pair stores that form one 32-bit operation"""
        op, ops = i.op, i.ops
        if i.w != 16 or op not in ('mov', 'add', 'sub', 'neg'):
            return False
        j = self.next_in_block(i)
        if j is None:
            return False
        if op == 'neg':
            # Turbo C long negation: neg hi; neg lo; sbb hi, 0
            k = self.next_in_block(j)
            if k is None or j.op != 'neg' or k.op != 'sbb' or ops[0].kind != 'reg' or j.ops[0].kind != 'reg' or \
                    k.ops[0].kind != 'reg' or k.ops[0].name != ops[0].name or k.ops[1].kind != 'imm' or k.ops[1].val != 0 or \
                    ops[0].name not in REG16 or j.ops[0].name not in REG16 or ops[0].name == j.ops[0].name:
                return False
            hi_r, lo_r = ops[0].name, j.ops[0].name
            t = self.g.newtmp(32)
            self.emit('%s = %s;' % (t, render(NEG(CAT(self.get(hi_r), self.get(lo_r))))))
            self.put(lo_r, LO(V(t, 32)))
            self.put(hi_r, HI(V(t, 32)))
            self.flag = None
            self.carry_src = None
            self.skip.add(j.off)
            self.skip.add(k.off)
            return True
        if op == 'mov' and j.op == 'mov' and ops[1].kind in ('reg', 'imm', 'sreg') and j.ops[1].kind in ('reg', 'imm', 'sreg'):
            for lo_i, hi_i in ((i, j), (j, i)):
                if self.mem_pair(lo_i.ops[0], hi_i.ops[0]):
                    if any(x.kind == 'reg' and x.name in lo_i.ops[0].base + hi_i.ops[0].base
                           for x in (lo_i.ops[1], hi_i.ops[1])):
                        return False
                    v = CAT(self.load(hi_i.ops[1], 16), self.load(lo_i.ops[1], 16))
                    self.store(self.as32(lo_i.ops[0]), v)
                    self.skip.add(j.off)
                    return True
            return False
        if op == 'mov':
            return False
        carry_op = {'add': 'adc', 'sub': 'sbb'}[op]
        if j.op != carry_op:
            return False
        sym = '+' if op == 'add' else '-'
        if ops[0].kind == 'mem' and self.mem_pair(ops[0], j.ops[0]) and \
                ops[1].kind in ('reg', 'imm') and j.ops[1].kind in ('reg', 'imm'):
            o32 = self.as32(ops[0])
            res = B(sym, self.load(o32, 32), CAT(self.load(j.ops[1], 16), self.load(ops[1], 16)), 32)
            self.store(o32, res)
        elif ops[0].kind == 'reg' and j.ops[0].kind == 'reg' and ops[0].name in REG16 and j.ops[0].name in REG16 \
                and ops[0].name != j.ops[0].name and \
                not any(x.kind == 'reg' and x.name == ops[0].name for x in j.ops[1:]) and \
                (j.ops[1].kind != 'mem' or ops[0].name not in j.ops[1].base):
            lo_r, hi_r = ops[0].name, j.ops[0].name
            res = B(sym, CAT(self.get(hi_r), self.get(lo_r)), CAT(self.load(j.ops[1], 16), self.load(ops[1], 16)), 32)
            t = self.g.newtmp(32)
            self.emit('%s = %s;' % (t, render(res)))
            self.put(lo_r, LO(V(t, 32)))
            self.put(hi_r, HI(V(t, 32)))
        else:
            return False
        self.flag = None
        self.carry_src = None
        self.skip.add(j.off)
        return True

    def preserve_pending_mem(self):
        """carry/flag operands that read memory must see the value before a store"""
        def keep(v):
            if v is None or not v.mem:
                return v
            t = self.g.newtmp(max(16, v.w))
            self.emit('%s = %s;' % (t, render(v)))
            return V(t, v.w)
        if self.carry_src:
            cop, ca, cb, cw = self.carry_src
            self.carry_src = (cop, keep(ca), keep(cb), cw)
        if self.flag:
            kind, fa, fb, fr, fw = self.flag
            self.flag = (kind, keep(fa), keep(fb), fr, fw)
        if self.shift_carry is not None and self.shift_carry.mem:
            self.shift_carry = keep(self.shift_carry)

    def assign_var(self, name, rhs):
        self.pre = []
        self.invalidate(name)
        for l in self.pre:
            self.emit(l)
        self.emit('%s = %s;' % (name, rhs))

    def invalidate(self, name):
        """a variable changed: expressions still referring to the old value must be materialized first"""
        # expressions are rendered later, so references to `name` in pending
        # register expressions would see the new value.  Materialize them.
        pat = re.compile(r'\b%s\b' % re.escape(name))
        for r in REG16:
            x = self.get(r)
            if x.k == 'var' and (x.v == r or x.v.startswith('__dead') or x.v.startswith('__entry')):
                continue
            try:
                txt = render(x)
            except Unsupported:
                continue
            if pat.search(txt):
                t = self.g.newtmp(16)
                self.pre.append('%s = %s;' % (t, txt))
                self.r[r] = V(t)
        for n, x in enumerate(self.args):
            if pat.search(render(x)):
                t = self.g.newtmp(16)
                self.pre.append('%s = %s;' % (t, render(x)))
                self.args[n] = V(t)
        # pending flag / carry operands still refer to the old value
        def keep(x):
            if x is None:
                return None
            try:
                txt = render(x)
            except Unsupported:
                return x
            if pat.search(txt):
                t = self.g.newtmp(max(16, x.w))
                self.pre.append('%s = %s;' % (t, txt))
                return V(t, x.w)
            return x
        if self.carry_src:
            op, a, b, w = self.carry_src
            self.carry_src = (op, keep(a), keep(b), w)
        if self.flag:
            kind, a, b, res, w = self.flag
            self.flag = (kind, keep(a), keep(b), res, w)

    # ---- instruction translation ----
    def translate(self, i):
        op = i.op
        ops = i.ops
        w = i.w
        g = self.g
        self.cur = i
        if i.off in self.skip:
            return
        if op == 'mov' and ops[0].kind == 'reg' and ops[0].name == 'sp' and ops[1].kind == 'reg' and ops[1].name == 'bp':
            self.args = []                          # epilogue or bulk argument cleanup
            return
        if op == 'add' and ops[0].kind == 'reg' and ops[0].name == 'sp' and ops[1].kind == 'imm':
            n = ops[1].val // 2
            if n > len(self.args):
                raise Unsupported('add sp beyond pushed words')
            del self.args[len(self.args) - n:]
            return
        if op == 'inc' and ops[0].kind == 'reg' and ops[0].name == 'sp':
            raise Unsupported('stray inc sp')
        if self.fuse32(i):
            return
        if op == 'mov':
            self.store(ops[0], self.load(ops[1], w) if ops[1].kind != 'mem' else self.load(ops[1], ops[0].w if ops[0].kind != 'mem' else w))
            return
        if op in ('add', 'sub', 'and', 'or', 'xor'):
            dst, src = ops
            a = self.load(dst, w)
            b = self.load(src, w)
            if op == 'xor' and dst.kind == 'reg' and src.kind == 'reg' and dst.name == src.name:
                self.store(dst, C(0, w)); self.flag = ('logic', C(0, w), None, C(0, w), w)
                return
            sym = {'add': '+', 'sub': '-', 'and': '&', 'or': '|', 'xor': '^'}[op]
            if op in ('and', 'or') and dst.kind == 'reg' and src.kind == 'reg' and dst.name == src.name:
                self.flag = ('logic', a, None, a, w)
                return
            res = B(sym, a, b, w)
            if dst.kind == 'mem':
                # read-modify-write: materialize old value via expression
                self.flag = ('arith' if op in ('add', 'sub') else 'logic', a, b, None, w)
                self.carry_src = (op, a, b, w)
                self.store(dst, res)
                self.flag = (self.flag[0], a, b, self.load(dst, w), w)
                return
            self.carry_src = (op, a, b, w)
            self.store(dst, res)
            self.flag = ('sub' if op == 'sub' else ('add' if op == 'add' else 'logic'), a, b, self.load(dst, w), w)
            return
        if op in ('adc', 'sbb'):
            if not self.carry_src:
                raise Unsupported('adc without preceding add')
            cop, ca, cb, cw = self.carry_src
            if cop == 'neg':
                carry = X('call', 16, v='(%s != 0)' % cu(ca))
            elif (op == 'adc') != (cop == 'add'):
                raise Unsupported('mixed carry')
            if cop == 'neg':
                pass
            elif op == 'adc':
                carry = X('call', 16, v='((u32)%s + %s > 0x%X)' % (cu(ca), cu(cb), (1 << cw) - 1))
            else:
                carry = X('call', 16, v='(%s < %s)' % (cu(ca), cu(cb)))
            dst, src = ops
            a = self.load(dst, w)
            b = self.load(src, w)
            sym = '+' if op == 'adc' else '-'
            self.store(dst, B(sym, B(sym, a, b, w), carry, w))
            self.flag = None
            self.carry_src = None
            return
        if op in ('cmp', 'test', 'mul', 'imul', 'div', 'idiv'):
            self.carry_src = None             # these overwrite CF
        if op in ('shl', 'sal', 'shr', 'sar'):
            self.carry_src = None
        if op in ('cmp', 'test'):
            a = self.load(ops[0], w)
            b = self.load(ops[1], w)
            if op == 'cmp':
                self.flag = ('cmp', a, b, None, w)
            else:
                self.flag = ('logic', a, b, B('&', a, b, w), w)
            return
        if op in ('inc', 'dec'):
            a = self.load(ops[0], w)
            res = B('+' if op == 'inc' else '-', a, C(1, w), w)
            self.store(ops[0], res)
            # b carries the step (+1 / -1) for signed conditions on the result
            self.flag = ('incdec', a, C(1 if op == 'inc' else -1, 32), self.load(ops[0], w), w)
            return
        if op == 'neg':
            a = self.load(ops[0], w)
            if a.mem or a.k not in ('const',):
                t = self.g.newtmp(16)
                self.emit('%s = %s;' % (t, render(a)))
                a = V(t, w)
            self.store(ops[0], NEG(a))
            self.flag = ('neg', a, None, self.load(ops[0], w), w)
            self.carry_src = ('neg', a, None, w)          # CF = (operand != 0)
            return
        if op == 'not':
            a = self.load(ops[0], w)
            self.store(ops[0], NOT(a))
            return
        if op in ('shl', 'sal', 'shr', 'sar', 'rcl', 'rcr'):
            a = self.load(ops[0], w)
            cnt = self.load(ops[1], 8)
            if op in ('rcl', 'rcr'):
                # 32-bit shift idioms: shl lo,1 / rcl hi,1  and  shr|sar hi,1 / rcr lo,1
                if not (is_const(cnt, 1) and self.shift_carry is not None):
                    raise Unsupported('rotate through carry')
                c = self.shift_carry
                if op == 'rcl':
                    res = B('|', B('<<', a, C(1), w), c, w)
                else:
                    res = B('|', B('>>u', a, C(1), w), B('<<', c, C(w - 1), w), w)
                self.store(ops[0], res)
                self.shift_carry = None
                self.flag = None
                return
            sym = {'shl': '<<', 'sal': '<<', 'shr': '>>u', 'sar': '>>s'}[op]
            if cnt.k != 'const':
                cnt = X('zx', 16, cnt)
            # remember the bit shifted out for a following rcl/rcr
            if is_const(cnt, 1):
                self.shift_carry = B('>>u', a, C(w - 1), w) if sym == '<<' else B('&', a, C(1), w)
                if a.mem or (a.k != 'const' and ops[0].kind == 'reg'):
                    t = self.g.newtmp(16)
                    self.emit('%s = %s;' % (t, render(self.shift_carry)))
                    self.shift_carry = V(t)
            else:
                self.shift_carry = None
            self.store(ops[0], B(sym, a, cnt, w))
            self.flag = ('logic', None, None, self.load(ops[0], w), w)
            return
        if op in ('mul', 'imul'):
            if w == 8:
                a = self.get('al')
                b = self.load(ops[0], 8)
                if op == 'mul':
                    self.put('ax', B('*', ZX8(a), ZX8(b), 16))
                else:
                    self.put('ax', B('*', SX(a, 16), SX(b, 16), 16))
                self.flag = None
                return
            a = self.get('ax')
            b = self.load(ops[0], 16)
            if op == 'mul':
                prod = X('call', 32, v='((u32)%s * %s)' % (cu(a), cu(b)))
            else:
                prod = X('call', 32, v='((u32)((i32)%s * %s))' % (cs(a), cs(b)))
            if self.dx_live_after(i):
                t = self.g.newtmp(32)
                self.flush_mem()
                self.emit('%s = %s;' % (t, render(prod)))
                self.r['ax'] = LO(V(t, 32)); self.r['dx'] = HI(V(t, 32))
            else:
                self.r['ax'] = B('*', a, b, 16)
            self.flag = None
            return
        if op in ('div', 'idiv'):
            if w == 8:
                raise Unsupported('8-bit div')
            a_lo = self.get('ax'); a_hi = self.get('dx')
            b = self.load(ops[0], 16)
            self.flush_mem()
            q = self.g.newtmp(16); rm = self.g.newtmp(16)
            if op == 'idiv':
                num = '((i32)MK32(%s, %s))' % (render(a_hi), render(a_lo))
                if a_hi.k == 'bin' and a_hi.v == '>>s' and render(a_hi.a) == render(a_lo):
                    num = '(i32)%s' % cs(a_lo)
                self.emit('%s = (u16)(%s / %s);' % (q, num, cs(b)))
                self.emit('%s = (u16)(%s %% %s);' % (rm, num, cs(b)))
            else:
                num = 'MK32(%s, %s)' % (render(a_hi), render(a_lo))
                if is_const(a_hi, 0):
                    num = cu(a_lo)
                self.emit('%s = (u16)(%s / %s);' % (q, num, cu(b)))
                self.emit('%s = (u16)(%s %% %s);' % (rm, num, cu(b)))
            self.r['ax'] = V(q); self.r['dx'] = V(rm)
            self.flag = None
            return
        if op == 'cbw':
            self.r['ax'] = SX(self.get('al'), 16)
            return
        if op == 'cwd':
            self.r['dx'] = B('>>s', self.get('ax'), C(15), 16)
            return
        if op == 'xchg':
            a = self.load(ops[0]); b = self.load(ops[1])
            if ops[0].kind == 'mem' or ops[1].kind == 'mem':
                raise Unsupported('xchg mem')
            self.put(ops[0].name, b); self.put(ops[1].name, a)
            return
        if op == 'lea':
            if ops[1].base == ('bp',) and ops[1].disp >= 0:
                self.put(ops[0].name, B('+', V('bp_'), C(ops[1].disp)))
            elif ops[1].base == ('bp',):
                self.put(ops[0].name, B('-', V('bp_'), C(-ops[1].disp)))
            else:
                self.put(ops[0].name, self.addr(ops[1]))
            return
        if op == 'les':
            o = ops[1]
            if 'bp' in o.base:
                pv = self.memref(o, 32)
            else:
                pv = self.memref(o, 32)
            # keep the load inline: FP8(G32(x), ...) reads better than a
            # temporary; flush_mem saves it if a store or call intervenes
            self.put(ops[0].name, OFFOF(pv))
            self.r['es'] = SEGOF(pv)
            return
        if op == 'push':
            o = ops[0]
            if o.kind == 'reg' and o.name in ('bp', 'si', 'di') and self.in_epilogue_push(i):
                return
            x = self.load(o, 16)
            self.args.append(x)
            return
        if op == 'pop':
            o = ops[0]
            if o.kind == 'reg' and o.name in ('bp', 'si', 'di') and not self.args:
                return              # epilogue
            if not self.args:
                raise Unsupported('pop with empty arg stack')
            x = self.args.pop()
            if o.kind == 'reg':
                self.put(o.name, x)
            elif o.kind == 'sreg' and o.name == 'es':
                self.r['es'] = x
            else:
                raise Unsupported('pop to ' + (o.name if o.kind == 'sreg' else 'memory'))
            return
        if op in ('call', 'callf'):
            self.do_call(i)
            return
        if op in ('calli', 'callfi'):
            self.do_icall(i)
            return
        if op == 'add_sp':
            return
        if op in ('cld', 'nop', 'clc', 'stc', 'cmc', 'std'):
            if op == 'std':
                raise Unsupported('std')
            return
        if op in ('jcc', 'jmp', 'loop', 'jcxz', 'loope', 'loopne', 'ret', 'retf', 'jmpi'):
            self.term = i
            return
        raise Unsupported('no translation for ' + op)

    def in_epilogue_push(self, i):
        return False

    def dx_live_after(self, i):
        return bool(self.g.live[i.off] & BIT['dx'])

    def stack_cleanup(self, i):
        """instructions right after a call that drop the arguments"""
        return self.g.D.arg_bytes_after(self.g.f, i)

    def do_call(self, i):
        g = self.g
        D = g.D
        t = D.L.call_target(i)
        nm = D.name_of(t)
        if nm in regflow.HELPERS:
            self.helper_call(i, nm)
            return
        proto = D.protos.get(t)
        far_frame = i.op == 'callf'
        if i.op == 'call':
            # Turbo C large model: 'push cs; call near' is a far call frame
            if self.args and self.args[-1].k == 'var' and self.args[-1].v == 'SEGV(0x%04X)' % g.f.seg:
                self.args.pop()
                far_frame = True
            elif not (t in D.decomp_set and proto is not None and proto.near and not proto.far):
                raise Unsupported('true near call')     # a decompiled near callee is a plain C call
        nbytes = self.stack_cleanup(i)
        if nbytes is None:
            raise Unsupported('call at block end')
        if nbytes == -1:
            # 'mov sp, bp' resets the stack to the frame: every pending word
            # belongs to this call
            nbytes = 2 * len(self.args)
        nwords = nbytes // 2
        if nbytes == 0 and self.args and proto is not None and proto.nwords and not proto.varargs:
            nwords = proto.nwords            # arguments dropped later by the epilogue
            if nwords > len(self.args):
                raise Unsupported('call with args from outside block')
        if nwords > len(self.args):
            raise Unsupported('call with args from outside block')
        words = self.args[len(self.args) - nwords:] if nwords else []
        words = list(reversed(words))           # first argument = last pushed
        mw = D.RF.mayw.get(t, regflow.CLOBBER)
        survive = BIT['si'] | BIT['di'] | (regflow.CLOBBER & ~mw)
        self.flush_mem(live=survive, keep_args=nwords)
        words = [self.mat_if_mem(x) for x in words]
        callee_dec = t in D.decomp_set
        if callee_dec and proto is not None and not proto.varargs and not proto.stack_abi and proto.nwords == nwords:
            argv = []
            for name, pw, pidx in proto.params:
                if pw == 32:
                    argv.append(render(CAT(words[pidx + 1], words[pidx])))
                else:
                    argv.append(enum_name(ENUM_PARAMS.get((cname(t), pidx)), words[pidx]) or render(words[pidx]))
            for r in proto.regs:
                x = self.get(r)
                if x.k == 'var' and x.v == '__mach_' + r:
                    argv.append('R.' + r)
                    continue
                if (r in D.passthru_lifted(t) and not (g.live[i.off] & BIT[r])) or \
                        (x.k == 'var' and x.v.startswith(('__dead_', '__mach_')) and
                         r in D.passthru_regs(t) and not (g.live[i.off] & mw & (BIT['ax'] | BIT['dx']))):
                    # the callee only hands this register back as its result,
                    # and the result is not used here: the value cannot matter
                    argv.append('0 /* unused %s */' % r)
                    continue
                txt = render(x)
                if any(re.search(r'\b%s\b' % v, txt) and v not in g.vars and v not in g.p.regs for v in REG16):
                    raise Unsupported('passthrough of undefined register ' + r)
                argv.append(txt)
            call = '%s(%s)' % (cname(t), ', '.join(argv))
            rw = proto.ret
        else:
            seg = 'SEGV(0x%04X)' % t[0]
            fn = 'f_%04X_%04X' % t
            if D.name_of(t):
                fn += ' /* %s */' % D.name_of(t).lstrip('_')
            elif t in FUNC_NAMES:
                fn += ' /* %s */' % FUNC_NAMES[t]
            rendered = []
            j = 0
            while j < len(words):
                x = words[j]
                if j + 1 < len(words):
                    pair = CAT(words[j + 1], x)
                    if pair.k != 'cat' and pair.k != 'const' and pair.k != 'dsp':
                        # a 32-bit value passed as two words (far pointer, long)
                        rendered.append('W2(%s)' % render(pair))
                        j += 2
                        continue
                r_ = enum_name(ENUM_PARAMS.get((D.name_of(t) or FUNC_NAMES.get(t, ''), j)), x) or render(x)
                if x.k == 'const' and j + 1 < len(words) and words[j + 1].k == 'var' and \
                        words[j + 1].v == 'DSEG' and x.v in DGROUP_STRINGS:
                    r_ += ' /* %s */' % DGROUP_STRINGS[x.v]
                rendered.append(r_)
                j += 1
            argl = ''.join(', ' + r_ for r_ in rendered)
            call = 'call_lifted(%s, %s, %d%s)' % (fn, seg, nwords, argl)
            rw = 32
            # a machine-level callee that reads scratch registers on entry
            # (e.g. returns its caller's AX on some path) gets them as the
            # original code would have had them
            rin = D.RF.reg_in.get(t, 0) & regflow.CLOBBER
            skipped = set()
            for r in ('ax', 'bx', 'cx', 'dx', 'es'):
                if rin & BIT[r]:
                    if r in D.passthru_lifted(t) and not (g.live[i.off] & BIT[r]):
                        # the callee only hands this register back and the
                        # result is not used: its value cannot matter
                        x = self.get(r)
                        if not (x.k == 'var' and x.v == '__mach_' + r):
                            skipped.add(r)              # (the machine value stays exact)
                        continue
                    x = self.get(r)
                    if x.k == 'var' and x.v == '__mach_' + r:
                        continue           # the machine register still holds it
                    txt = render(x)
                    if any(re.search(r'\b%s\b' % v, txt) and v not in g.vars and v not in g.p.regs for v in REG16):
                        raise Unsupported('passthrough of undefined register ' + r)
                    self.emit('R.%s = %s;' % (r, txt))
        # mark consumed words: they stay on the arg stack until removed by add sp
        self.consumed = nwords
        self.clobber_after_call(mw, lifted=call.startswith('call_lifted('))
        if call.startswith('call_lifted('):
            # right after a machine-level call R.* hold the callee's leftovers
            for r in ('ax', 'bx', 'cx', 'dx', 'es'):
                if mw & BIT[r]:
                    self.r[r] = V('__mach_' + r)
            for r in skipped:
                self.r[r] = V('__dead_' + r)    # not the original's value: no later use allowed
        live = g.live[i.off] & mw
        need_ax = live & BIT['ax']
        need_dx = live & BIT['dx']
        if need_ax or need_dx:
            if rw == 32 or need_dx:
                tname = g.newtmp(32)
                self.emit('%s = %s;' % (tname, call))
                self.r['ax'] = LO(V(tname, 32)); self.r['dx'] = HI(V(tname, 32))
            else:
                tname = g.newtmp(16)
                self.emit('%s = %s;' % (tname, call))
                self.r['ax'] = V(tname)
        else:
            self.emit('%s;' % call)
        self.flag = None
        # drop the argument words now; the cleanup instructions become no-ops
        if nbytes and self.stack_cleanup(i) != -1:
            del self.args[len(self.args) - nwords:]
            j = g.f.insns.get(i.next)
            self.skip.add(i.next)
            if j is not None and j.op in ('pop', 'inc') and nbytes == (4 if j.op == 'pop' else 2):
                self.skip.add(j.next)

    def mat_if_mem(self, x):
        return x

    def clobber_after_call(self, mw=regflow.CLOBBER, lifted=False):
        for r in ('ax', 'bx', 'cx', 'dx', 'es'):
            x = self.r[r]
            # a machine-level callee leaves registers outside its write set
            # exactly as they were; decompiled code may change R.* on the way
            if mw & BIT[r] or (not lifted and isinstance(x, X) and x.k == 'var' and x.v.startswith('__mach_')):
                self.r[r] = V('__dead_' + r)

    def helper_call(self, i, nm):
        g = self.g
        if nm in ('LDIV@', 'LUDIV@', 'LMOD@', 'LUMOD@', 'SCOPY@'):
            if len(self.args) < 4:
                raise Unsupported('helper args across blocks')
            self.flush_mem(live=BIT['si'] | BIT['di'], keep_args=4)
            w = list(reversed(self.args[-4:]))
            del self.args[-4:]
            if nm == 'SCOPY@':
                n = self.get('cx')
                self.emit('struct_copy(%s, %s, %s);' % (render(CAT(w[3], w[2])), render(CAT(w[1], w[0])), render(n)))
                self.clobber_after_call(BIT['cx'] | BIT['es'])     # SCOPY@ saves si, di, ds
                self.flag = None
                return
            a = render(CAT(w[1], w[0])); b = render(CAT(w[3], w[2]))
            op = '/' if nm in ('LDIV@', 'LUDIV@') else '%'
            if nm in ('LDIV@', 'LMOD@'):
                e = '(u32)((i32)%s %s (i32)%s)' % (a, op, b)
            else:
                e = '(u32)%s %s (u32)%s' % (a, op, b)
        elif nm == 'LXMUL@':
            self.flush_mem(live=BIT['si'] | BIT['di'])
            e = '(u32)(%s * %s)' % (render(CAT(self.get('dx'), self.get('ax'))), render(CAT(self.get('cx'), self.get('bx'))))
        elif nm in ('LXLSH@', 'LLSH@'):
            self.flush_mem(live=BIT['si'] | BIT['di'])
            cl = render(self.get('cl'))
            e = '(%s >= 32 ? 0u : (u32)%s << %s)' % (paren(cl), render(CAT(self.get('dx'), self.get('ax'))), paren(cl))
        elif nm in ('PADD@', 'PADA@', 'PCMP@'):
            self.flush_mem(live=BIT['si'] | BIT['di'])
            t = g.D.L.call_target(i)
            for r in ('ax', 'bx', 'cx', 'dx'):
                self.emit('R.%s = %s;' % (r, render(self.get(r))))
            self.emit('push16(SEGV(0x0000)); push16(0xFFFF); f_%04X_%04X();   /* %s */' % (t[0], t[1], nm))
            if nm == 'PCMP@':
                self.clobber_after_call()
                self.flag = ('rflags', None, None, None, 16)
                return
            e = 'MK32(R.dx, R.ax)'
        else:
            raise Unsupported('helper ' + nm)
        t = g.newtmp(32)
        self.emit('%s = %s;' % (t, e))
        self.clobber_after_call()
        self.r['ax'] = LO(V(t, 32)); self.r['dx'] = HI(V(t, 32))
        self.flag = None

    def do_icall(self, i):
        raise Unsupported('indirect call')

    # ---- block end ----
    def finish(self, offs):
        g = self.g
        f = g.f
        last = f.insns[offs[-1]]
        term = last if last.op in ('jcc', 'jmp', 'loop', 'jcxz', 'loope', 'loopne', 'ret', 'retf', 'jmpi') else None
        lout = g.live[last.off] if term is None else g.live[last.off]
        # condition must be computed before registers are spilled
        cond = None
        fused = None
        if term is not None and term.op == 'loop':
            # loop L: cx = cx - 1; if (cx != 0) goto L
            self.put('cx', B('-', self.get('cx'), C(1)))
            cond = '%s != 0' % paren(cu(self.get('cx'), 16))
        if term is not None and term.op == 'jcc':
            fused = self.long_compare(term)
            cond = fused[0] if fused else self.condition(term.cc)
            # a successor that is just another conditional jump on the same
            # flags (32-bit compares: jl/jg/jb chains) inherits them
            for s_ in (term.next, term.ops[0].val):
                si = f.insns.get(s_)
                if si is not None and si.op == 'jcc' and len(g.preds[s_]) == 1 and s_ not in g.done_blocks:
                    g.in_flags[s_] = self.materialized_flag()
        if term is not None and term.op in ('ret', 'retf'):
            self.ret(term)
            self.term = ('ret', [])
            return
        # spill live registers into their variables
        live = g.live[last.off]
        if term is not None and term.op == 'jcc':
            pass
        if self.args:
            succ = g.D.L.succs(f, last.off) if term is not None else [last.next]
            for s_ in succ:
                if s_ in g.done_blocks or g.in_args.get(s_, len(self.args)) != len(self.args):
                    raise Unsupported('pushed arguments live across block end')
                g.in_args[s_] = len(self.args)
            for j, x in enumerate(self.args):
                g.vars['s%d' % j] = 'u16'
                if not (x.k == 'var' and x.v == 's%d' % j):
                    self.emit('s%d = %s;' % (j, render(x)))
            self.args = [V('s%d' % j) for j in range(len(self.args))]
        spilled = [r for r in ('ax', 'bx', 'cx', 'dx', 'si', 'di', 'es') if live & BIT[r]]
        if cond is not None and any(re.search(r'\b%s\b' % r, cond) for r in spilled):
            self.g.vars['cc'] = 'int'
            self.emit('cc = %s;' % cond)
            cond = 'cc'
        self.spill(live)
        if term is None:
            nxt = last.next
            self.term = ('goto', nxt)
            return
        if term.op == 'jmp':
            self.term = ('goto', term.ops[0].val)
            return
        if term.op == 'loop':
            self.term = ('if', cond, term.ops[0].val, term.next)
            return
        if term.op == 'jcc':
            if fused:
                self.term = ('if', cond, fused[1], fused[2])
            else:
                self.term = ('if', cond, term.ops[0].val, term.next)
            return
        if term.op == 'jmpi':
            jt = f.jumptables[term.off]
            self.switch(term, jt)
            return
        raise Unsupported('terminator ' + term.op)

    SIGNED_CC = {'l', 'le', 'g', 'ge'}
    UNSIGNED_CC = {'b', 'be', 'a', 'ae'}
    EQ_CC = {'e', 'ne'}

    def long_compare(self, last):
        """Turbo C compares longs as: cmp hi; jcc; [jcc]; cmp lo; jcc.  When
        the chain decides like one 32-bit relation, return (condition, true
        target, false target) for the first block; the rest becomes dead."""
        g, f = self.g, self.g.f
        if self.flag is None:
            return None
        kind, a, b, res, w = self.flag
        if w != 16:
            return None
        if kind == 'cmp':
            hi = (a, b)
        elif kind == 'logic' and b is None and res is not None:
            hi = (res, C(0))
        else:
            return None
        ok_hi = self.SIGNED_CC | self.UNSIGNED_CC | self.EQ_CC
        if last.cc not in ok_hi:
            return None
        chain = [(last.cc, last.ops[0].val)]
        prev, nxt = last.off, last.next
        j = f.insns.get(nxt)
        if j is not None and j.op == 'jcc' and g.preds[nxt] == {prev}:
            if j.cc not in ok_hi:
                return None
            chain.append((j.cc, j.ops[0].val))
            prev, nxt = j.off, j.next
        c = f.insns.get(nxt)
        if c is None or g.preds[nxt] != {prev} or c.w != 16:
            return None
        if c.op == 'cmp':
            lo_ops = (c.ops[0], c.ops[1])
        elif c.op in ('or', 'test') and c.ops[0].kind == c.ops[1].kind == 'reg' and c.ops[0].name == c.ops[1].name:
            lo_ops = (c.ops[0], None)
        else:
            return None
        d = f.insns.get(c.next)
        if d is None or d.op != 'jcc' or d.cc not in self.UNSIGNED_CC | self.EQ_CC or g.preds[c.next] != {c.off}:
            return None
        ccs = [cc for cc, _ in chain]
        signed = any(cc in self.SIGNED_CC for cc in ccs)
        if signed and any(cc in self.UNSIGNED_CC for cc in ccs):
            return None
        def holds(cc, rel):
            return {'e': rel == 'eq', 'ne': rel != 'eq', 'l': rel == 'lt', 'b': rel == 'lt',
                    'le': rel != 'gt', 'be': rel != 'gt', 'g': rel == 'gt', 'a': rel == 'gt',
                    'ge': rel != 'lt', 'ae': rel != 'lt'}[cc]
        def outcome(hr, lr):
            for cc, t in chain:
                if holds(cc, hr):
                    return t
            return d.ops[0].val if holds(d.cc, lr) else d.next
        comb = {}
        for hr in ('lt', 'eq', 'gt'):
            for lr in ('lt', 'eq', 'gt'):
                o = hr if hr != 'eq' else lr
                if comb.setdefault(o, outcome(hr, lr)) != outcome(hr, lr):
                    return None
        targets = set(comb.values())
        if len(targets) != 2:
            return None
        rels = {('lt',): '<', ('eq', 'lt'): '<=', ('gt',): '>', ('eq', 'gt'): '>=', ('eq',): '==', ('gt', 'lt'): '!='}
        for T in sorted(targets):
            key = tuple(sorted(o for o, t in comb.items() if t == T))
            if key in rels:
                break
        else:
            return None
        F = (targets - {T}).pop()
        rel = rels[key]
        lo_a = self.load(lo_ops[0], 16)
        lo_b = self.load(lo_ops[1], 16) if lo_ops[1] is not None else C(0)
        X, Y = CAT(hi[0], lo_a), CAT(hi[1], lo_b)
        if rel in ('==', '!=') or not signed:
            cond = '%s %s %s' % (paren(cu(X, 32)), rel, paren(cu(Y, 32)))
        else:
            cond = '%s %s %s' % (paren(cs(X, 32)), rel, paren(cs(Y, 32)))
        return cond, T, F

    def materialized_flag(self):
        """flag state with operands copied into temporaries (valid in other blocks)"""
        kind, a, b, res, w = self.flag
        def keep(x):
            if x is None or x.k == 'const':
                return x
            t = self.g.newtmp(max(16, x.w))
            self.emit('%s = %s;' % (t, render(x)))
            return V(t, x.w)
        return (kind, keep(a), keep(b), keep(res), w)

    def spill(self, live):
        assigns = []
        for r in ('ax', 'bx', 'cx', 'dx', 'si', 'di', 'es'):
            if not (live & BIT[r]):
                continue
            x = self.get(r)
            if x.k == 'var' and x.v == r:
                continue
            if x.k == 'var' and x.v.startswith('__dead_'):
                raise Unsupported('clobbered register %s live' % r)
            self.g.vars.setdefault(r, 'u16')
            assigns.append((r, x))
        # parallel assignment: evaluate into temps if any rhs references another assigned reg
        names = {r for r, _ in assigns}
        if any(any(re.search(r'\b%s\b' % n, render(x)) for n in names if n != r) for r, x in assigns):
            tmps = []
            for r, x in assigns:
                t = self.g.newtmp(16)
                self.emit('%s = %s;' % (t, render(x)))
                tmps.append((r, t))
            for r, t in tmps:
                self.emit('%s = %s;' % (r, t))
        else:
            for r, x in assigns:
                self.emit('%s = %s;' % (r, render(x)))
        for r, x in assigns:
            self.r[r] = V(r)

    def condition(self, cc):
        if self.flag is None:
            raise Unsupported('condition without known flags')
        kind, a, b, res, w = self.flag
        if kind == 'rflags':
            return lift.CC_R[cc]
        if kind == 'cmp':
            en = enum_name(enum_of(render(a)), b) if b is not None and b.k == 'const' else None
            m = {'e': '%s == %s', 'ne': '%s != %s'}
            if cc in ('e', 'ne'):
                return m[cc] % (paren(cu(a, w)), en or paren(cu(b, w)))
            if cc in ('b', 'ae', 'be', 'a'):
                o = {'b': '<', 'ae': '>=', 'be': '<=', 'a': '>'}[cc]
                return '%s %s %s' % (paren(cu(a, w)), o, en or paren(cu(b, w)))
            if cc in ('l', 'ge', 'le', 'g'):
                o = {'l': '<', 'ge': '>=', 'le': '<=', 'g': '>'}[cc]
                return '%s %s %s' % (paren(cs(a, w)), o, en or paren(cs(b, w)))
            if cc in ('s', 'ns'):
                return '%s %s 0' % (paren(cs(B('-', a, b, w), w)), '<' if cc == 's' else '>=')
            raise Unsupported('cmp cc ' + cc)
        if kind in ('logic', 'add', 'sub', 'incdec', 'neg'):
            r = res
            if r is None:
                raise Unsupported('no result for flags')
            if cc in ('e', 'ne'):
                return '%s %s 0' % (paren(cu(r, w)), '==' if cc == 'e' else '!=')
            if cc in ('s', 'ns'):
                return '%s %s 0' % (paren(cs(r, w)), '<' if cc == 's' else '>=')
            if kind == 'logic':
                if cc in ('l',): return '%s < 0' % paren(cs(r, w))
                if cc in ('ge',): return '%s >= 0' % paren(cs(r, w))
                if cc in ('le',): return '%s <= 0' % paren(cs(r, w))
                if cc in ('g',): return '%s > 0' % paren(cs(r, w))
                if cc == 'be': return '%s == 0' % paren(cu(r, w))
                if cc == 'a': return '%s != 0' % paren(cu(r, w))
            if kind == 'logic' and cc in ('b', 'ae'):
                return '0' if cc == 'b' else '1'        # AND/OR/XOR/TEST clear CF
            if kind in ('incdec', 'add') and cc in ('l', 'ge', 'le', 'g') and a is not None:
                # SF == OF  <=>  the exact (non-wrapping) result is >= 0
                if kind == 'incdec':
                    if b is None or b.k != 'const':
                        raise Unsupported('flags incdec/' + cc)
                    exact = '(i32)%s %s 1' % (paren(cs(a, w)), '+' if b.v == 1 else '-')
                elif b is not None:
                    exact = '(i32)%s + (i32)%s' % (paren(cs(a, w)), paren(cs(b, w)))
                else:
                    exact = None
                if exact is not None:
                    o = {'l': '<', 'ge': '>=', 'le': '<=', 'g': '>'}[cc]
                    return '(%s) %s 0' % (exact, o)
            if kind == 'sub' and a is not None and b is not None:
                if cc in ('b', 'ae', 'be', 'a'):
                    o = {'b': '<', 'ae': '>=', 'be': '<=', 'a': '>'}[cc]
                    return '%s %s %s' % (paren(cu(a, w)), o, paren(cu(b, w)))
                if cc in ('l', 'ge', 'le', 'g'):
                    o = {'l': '<', 'ge': '>=', 'le': '<=', 'g': '>'}[cc]
                    return '%s %s %s' % (paren(cs(a, w)), o, paren(cs(b, w)))
            raise Unsupported('flags %s/%s' % (kind, cc))
        raise Unsupported('flag kind ' + kind)

    def switch(self, term, jt):
        o = term.ops[0]
        g = self.g
        if jt['kind'] == 'dense' and o.kind == 'mem' and o.base == ('bx',):
            # jmp cs:[bx+table] with bx = 2*index: switch on the index
            bx = self.get('bx')
            if bx.k == 'bin' and bx.v == '<<' and is_const(bx.b, 1):
                idx, scale = bx.a, 1
            else:
                idx, scale = bx, 2
            en = enum_of(render(idx))
            cases = [(enum_name(en, C(n if scale == 1 else 2 * n)) or hexc(n if scale == 1 else 2 * n), t)
                     for n, t in enumerate(jt['targets'])]
            e = cu(idx)
            dflt = 'cpu_halt("bad switch index %%u in %04X:%04X", %s);' % (g.f.seg, g.f.off, e)
        else:
            tgt = self.load(o, 16)          # sparse: target address read from the table
            e = cu(tgt)
            cases = [('0x%04X' % t, t) for t in sorted(set(jt['targets']))]
            dflt = 'cpu_halt("bad switch target %%04X in %04X:%04X", %s);' % (g.f.seg, g.f.off, e)
        self.term = ('switch', e, cases, dflt)

    def ret(self, term):
        g = self.g
        p = g.p
        if self.args:
            raise Unsupported('return with pushed words')
        if g.frame_mode:
            self.emit('R.sp += %d;' % (2 + g.fr.locals))
        def mach(x, r):
            return V('R.' + r) if (x.k == 'var' and x.v == '__mach_' + r) else x
        if p.ret == 32:
            ax = mach(self.get('ax'), 'ax'); dx = mach(self.get('dx'), 'dx')
            if ax.k == 'var' and ax.v.startswith('__dead') or dx.k == 'var' and dx.v.startswith('__dead'):
                self.emit('return 0; /* undefined */')
                return
            if g.frame_mode:
                g.vars['rv_'] = 'u32'
                self.lines.insert(len(self.lines) - 1, '    rv_ = %s;' % render(CAT(dx, ax)))
                self.emit('return rv_;')
            else:
                self.emit('return %s;' % render(CAT(dx, ax)))
        elif p.ret == 16:
            ax = mach(self.get('ax'), 'ax')
            if ax.k == 'var' and ax.v.startswith('__dead'):
                self.emit('return 0; /* undefined */')
                return
            if g.frame_mode:
                g.vars['rv_'] = 'u32'
                self.lines.insert(len(self.lines) - 1, '    rv_ = %s;' % render(ax))
                self.emit('return (u16)rv_;')
            else:
                self.emit('return %s;' % render(ax))
        else:
            self.emit('return;')


def ZX8(x):
    if x.w == 16:
        return x
    if exact(x):
        return X('zx', 16, x)
    return X('tr', 8, x)


# ---------------------------------------------------------------- driver
GAME_SKIP_SEGS = {0x0000, 0x371E, 0x3736, 0x3737, 0x373B, 0x373F, 0x3741, 0x3742, 0x3743, 0x3745, 0x3747,
                  0x374E, 0x3755, 0x375B, 0x3762, 0x376D, 0x377A, 0x3786, 0x3795, 0x37A2, 0x3858, 0x3859,
                  0x4456, 0x36F4}


def proto_params(p):
    if p.stack_abi:
        return ', '.join('u16 %s' % r for r in p.regs) or 'void'
    ps = ['%s %s' % ('u32' if w == 32 else 'u16', n) for n, w, _ in p.params] + ['u16 %s' % r for r in p.regs]
    return ', '.join(ps) or 'void'


# only a genuine read of the caller's register becomes a parameter; an
# undefined value passed on (leftover of an earlier call) stays unsupported
REG_REASON = re.compile(r'reads register value from caller: (ax|bx|cx|dx|es)$')


def wrapper(k, p):
    lines = ['void f_%04X_%04X(void) {' % k]
    base = 4 if p.far else 2
    args = []
    for name, w, idx in ([] if p.stack_abi else p.params):
        if w == 32:
            args.append('rd(R.ss, (u16)(R.sp + %d))' % (base + 2 * idx))
        else:
            args.append('rw(R.ss, (u16)(R.sp + %d))' % (base + 2 * idx))
    args += ['R.' + r for r in p.regs]
    call = '%s(%s)' % (cname(k), ', '.join(args))
    if p.ret == 32:
        lines.append('    u32 r = %s;' % call)
        lines.append('    R.ax = LO16(r); R.dx = HI16(r);')
    elif p.ret == 16:
        lines.append('    R.ax = %s;' % call)
    else:
        lines.append('    %s;' % call)
    lines.append('    R.sp += %d;' % base)
    lines.append('}')
    return '\n'.join(lines)


def explain_failures(D, keys):
    """debugging: decompile each key once more and show where it fails"""
    cur = {}
    orig = BlockState.translate
    def tr(self, i):
        cur[self.g.key] = i
        return orig(self, i)
    BlockState.translate = tr
    try:
        for k in keys:
            D.decomp_set.add(k)
            try:
                D.decompile(k)
                print('%04X:%04X decompiles now' % k)
            except Unsupported as e:
                i = cur.get(k)
                f = D.L.funcs[k]
                offs = sorted(f.insns)
                n = offs.index(i.off) if i is not None else 0
                print('%04X:%04X %s: %s at %04X' % (k[0], k[1], cname(k), e, i.off if i else 0))
                for o in offs[max(0, n - 6):n + 3]:
                    ins = f.insns[o]
                    tgt = ''
                    if ins.op in ('call', 'callf'):
                        t = D.L.call_target(ins)
                        tgt = '  [%s%s regs=%s pt=%s]' % (cname(t), ' dec' if t in D.decomp_set else '',
                                                          D.protos[t].regs, sorted(D.passthru_lifted(t)))
                    print('   %s %04X  %s %s%s' % ('>' if i is not None and o == i.off else ' ', o, ins.op,
                                               ', '.join(str(x) for x in ins.ops), tgt))
            D.decomp_set.discard(k)
    finally:
        BlockState.translate = orig


def main():
    exe, names_path, outdir = sys.argv[1:4]
    here = os.path.dirname(os.path.abspath(__file__))
    load_symbols(os.path.join(here, '..', 'names', 'symbols.txt'))
    STRUCTS[:] = structs.load(os.path.join(here, '..', 'names', 'structs.txt'), REC_NAMES, REC_DESC)
    load_enums(os.path.join(here, '..', 'names', 'enums.txt'))
    structure.CALL_NAMES.update(FUNC_NAMES.values())
    D = Decompiler(exe, names_path)
    D.infer_protos()
    for k, p_ in D.protos.items():
        cleanup.RET_TYPES[cname(k)] = {0: 'void', 16: 'u16', 32: 'u32'}[p_.ret]
    cleanup.MEM_NAMES.update(t.name for t in STRUCTS)
    for nm in REC_NAMES.values():
        cleanup.MEM_NAMES.update((nm, nm + '_lo', nm + '_hi'))
    cleanup.GLOBAL_VARS.update(GLOBAL_NAMES.values())
    # callees whose callers read registers beyond the C convention after the call
    relied_on = {v[2] for v in D.RF.violations}
    cands = set()
    for k, f in D.L.funcs.items():
        if k[0] in GAME_SKIP_SEGS or D.name_of(k):
            continue
        p = D.protos[k]
        if p.far and p.near:
            continue
        if k in relied_on:
            continue
        cands.add(k)
    D.decomp_set = set(cands)
    reasons = {}
    def converge():
        src = {}
        for rnd in range(40):
            D._passthru, D._pt_busy = {}, set()
            GLOBAL_HALVES.clear()
            ARRAY_HALVES.clear()
            src = {}
            failed = set()
            grew = False
            for k in sorted(D.decomp_set):
                try:
                    src[k] = D.decompile(k)
                except Unsupported as e:
                    m = REG_REASON.search(str(e))
                    if m and m.group(1) not in D.protos[k].regs:
                        # a value the caller leaves in a register: make it a parameter
                        D.protos[k].regs.append(m.group(1))
                        grew = True
                        continue
                    failed.add(k)
                    reasons[k] = str(e)
            new_arrays = ARRAY_PENDING - ARRAY_GLOBALS
            if new_arrays:
                ARRAY_GLOBALS.update(new_arrays)    # render them as arrays in another round
            ARRAY_PENDING.clear()
            if not failed and not grew and not new_arrays:
                break
            D.decomp_set -= failed
        return src

    src = converge()
    # functions that failed before the prototypes settled (register
    # parameters of their callees) get another chance
    for attempt in range(6):
        added = False
        for k in sorted(cands - D.decomp_set):
            D.decomp_set.add(k)
            D._passthru, D._pt_busy = {}, set()
            try:
                D.decompile(k)
                added = True
            except Unsupported as e:
                m = REG_REASON.search(str(e))
                if m and m.group(1) not in D.protos[k].regs:
                    D.protos[k].regs.append(m.group(1))
                    added = True
                else:
                    D.decomp_set.discard(k)
                    reasons[k] = str(e)
        if not added:
            break
        src = converge()
    reasons = {k: v for k, v in reasons.items() if k not in src}
    if os.environ.get('DECOMP_WHY'):
        explain_failures(D, sorted(reasons) if os.environ['DECOMP_WHY'] == 'all' else
                         [tuple(int(x, 16) for x in a.split(':')) for a in os.environ['DECOMP_WHY'].split(',')])
    print('decompiled %d of %d candidates (%d functions total)' % (len(src), len(cands), len(D.L.funcs)))
    cnt = collections.Counter(re.sub(r'[0-9A-Fa-f]{4}', 'N', r) for r in reasons.values())
    for r, n in cnt.most_common(40):
        print('  %4d  %s' % (n, r))
    os.makedirs(outdir, exist_ok=True)
    for fn in os.listdir(outdir):
        if fn.startswith('game_') and fn.endswith('.c'):
            os.remove(os.path.join(outdir, fn))
    hdr = ['/* Generated by tools/decomp.py -- prototypes of decompiled functions */',
           '#ifndef GAME_H', '#define GAME_H', '#include "dec.h"', '#include "../gen/lifted.h"', '']
    for k in sorted(src):
        p = D.protos[k]
        rt = {0: 'void', 16: 'u16', 32: 'u32'}[p.ret]
        params = proto_params(p)
        hdr.append('%s %s(%s);' % (rt, cname(k), params))
    hdr.append('')
    hdr.append('/* enumerations (names/enums.txt) */')
    for en, (prefix, members) in ENUMS.items():
        hdr.append('enum %s { %s };' % (en, ', '.join('%s_%s = 0x%X' % (prefix, nm, v) for v, nm in sorted(members.items()))))
    hdr.append('')
    hdr += structs.c_definitions(STRUCTS)
    hdr.append('')
    hdr.append('/* named fields of record arrays: name(i) = field of record i */')
    for (base, size, off, w), nm in sorted(REC_NAMES.items()):
        d = REC_DESC.get((base, size, off, w), '')
        kind = REC_USED.get((base, size, off, w))
        if kind == 'far':
            hdr.append('#define %s(i) FP%d(G32(0x%04X), (u16)((i) * 0x%X + 0x%X))%s' % (nm, w, base, size, off, ('   /* %s */' % d) if d else ''))
        elif kind == 'ds':
            hdr.append('#define %s(i) G%d((u16)((i) * 0x%X + 0x%X))%s' % (nm, w, size, base + off, ('   /* %s */' % d) if d else ''))
        for suf, o in (('lo', off), ('hi', off + 2)):
            hk = REC_USED.get((base, size, off, w, suf))
            if hk == 'far':
                hdr.append('#define %s_%s(i) FP16(G32(0x%04X), (u16)((i) * 0x%X + 0x%X))' % (nm, suf, base, size, o))
            elif hk == 'ds':
                hdr.append('#define %s_%s(i) G16((u16)((i) * 0x%X + 0x%X))' % (nm, suf, size, base + o))
    hdr.append('')
    hdr.append('/* named DGROUP globals */')
    for (off, w), nm in sorted(GLOBAL_NAMES.items()):
        d = GLOBAL_DESC.get((off, w), '')
        if (off, w) in ARRAY_GLOBALS:
            hdr.append('#define %-24s G%d((u16)(0x%04X + (i) * %d))%s' % (nm + '(i)', w, off, w // 8, ('   /* array; %s */' % d) if d else '   /* array */'))
        else:
            hdr.append('#define %-24s G%d(0x%04X)%s' % (nm, w, off, ('   /* %s */' % d) if d else ''))
    for (off, half) in sorted(ARRAY_HALVES):
        hdr.append('#define %-24s G16((u16)(0x%04X + (i) * 4))   /* %s word of %s(i) */' % (
            '%s_%s(i)' % (GLOBAL_NAMES[(off, 32)], half), off + (2 if half == 'hi' else 0),
            'low' if half == 'lo' else 'high', GLOBAL_NAMES[(off, 32)]))
    for (off, half) in sorted(h for h in GLOBAL_HALVES if (h[0], 32) not in ARRAY_GLOBALS):
        hdr.append('#define %-24s G16(0x%04X)   /* %s word of %s */' % (
            '%s_%s' % (GLOBAL_NAMES[(off, 32)], half), off + (2 if half == 'hi' else 0),
            'low' if half == 'lo' else 'high', GLOBAL_NAMES[(off, 32)]))
    hdr.append('#endif')
    open(os.path.join(outdir, 'game.h'), 'w').write('\n'.join(hdr) + '\n')
    byseg = collections.defaultdict(list)
    for k in sorted(src):
        byseg[k[0]].append(k)
    for seg, ks in byseg.items():
        out = ['/* Decompiled from MTV.EXE segment %04X by tools/decomp.py */' % seg, '#include "game.h"', '']
        for k in ks:
            out.append(src[k])
            out.append(wrapper(k, D.protos[k]))
            out.append('')
        open(os.path.join(outdir, 'game_%04X.c' % seg), 'w').write('\n'.join(out) + '\n')
    # output masks for the differential tester: which return registers matter
    tab = ['/* Generated: per decompiled function, the registers its callers read */',
           '#include "dec.h"', 'const u32 dec_keys[] = {']
    ks = sorted(src, key=lambda k: (k[0] << 16) | k[1])
    tab.append(', '.join('0x%04X%04X' % k for k in ks) + '};')
    tab.append('const u8 dec_ret[] = {' + ', '.join(str(D.protos[k].ret) for k in ks) + '};')
    tab.append('const int dec_count = %d;' % len(ks))
    # every function that follows the C convention (lifted or decompiled):
    # the tester compares only its return registers and skips its arguments
    asm = {k for k, regs in D.RF.exceptions if set(regs) - {'ax', 'dx', 'al', 'ah', 'dl', 'dh'}}
    conv = sorted((k for k in D.L.funcs if k[0] not in GAME_SKIP_SEGS and k not in asm and k not in D.RF.helper),
                  key=lambda k: (k[0] << 16) | k[1])
    tab.append('const u32 conv_keys[] = {' + ', '.join('0x%04X%04X' % k for k in conv) + '};')
    tab.append('const u8 conv_ret[] = {' + ', '.join(str(D.protos[k].ret) for k in conv) + '};')
    tab.append('const u8 conv_nwords[] = {' + ', '.join(str(D.protos[k].nwords or 0) for k in conv) + '};')
    tab.append('const int conv_count = %d;' % len(conv))
    open(os.path.join(outdir, 'game_table.c'), 'w').write('\n'.join(tab) + '\n')
    json.dump({'%04X:%04X' % k: v for k, v in reasons.items()}, open(os.path.join(outdir, 'unsupported.json'), 'w'), indent=1)


if __name__ == '__main__':
    main()
