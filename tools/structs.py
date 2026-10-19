"""Record tables as C structs (names/structs.txt + rec entries of symbols.txt).

Builds packed struct definitions (overlapping field definitions become
anonymous unions), accessor macros that keep the original 16-bit offset
arithmetic, and matches decompiler address expressions against the tables:

    G16((p * 0x1CE) + 0x1D15)                ->  tx(p, 0).state
    FP32(G32(0x24A9), (i * 0x17) + 8)        ->  films(i).price
"""
import re, collections

C_KEYWORDS = set('''auto break case char const continue default do double else enum extern float for goto if
int long register return short signed sizeof static struct switch typedef union unsigned void volatile
while'''.split())
UT = {8: 'u8', 16: 'u16', 32: 'u32'}


class Table:
    array_used = None

    def __init__(self, name, base, far, size, outer, struct, desc):
        self.name, self.base, self.far, self.size = name, base, far, size
        self.outer, self.struct, self.desc = outer, struct, desc
        self.fields = []           # (offset, width, name, desc)  scalar fields
        self.arrays = []           # (offset, width, dims, name, desc)  array fields
        self.by_off = {}           # (offset, width) -> name
        self.used = False


def prod(xs):
    n = 1
    for x in xs:
        n *= x
    return n


def load(path, rec_names, rec_desc):
    tables = []
    try:
        lines = open(path).read().split('\n')
    except OSError:
        return tables
    arrays = collections.defaultdict(list)
    for line in lines:
        if line.startswith('field '):
            tok = line.split(None, 5)
            m = re.fullmatch(r'(\d+)((?:\[\d+\])*)', tok[3])
            dims = tuple(int(d) for d in re.findall(r'\[(\d+)\]', m.group(2)))
            arrays[tok[1]].append((int(tok[2], 16), int(m.group(1)), dims, tok[4], tok[5] if len(tok) > 5 else ''))
    for line in lines:
        if not line.startswith('table '):
            continue
        tok = line.split()
        name, base_s, size = tok[1], tok[2], int(tok[3], 16)
        k = 4
        outer = None
        if tok[k].startswith('outer='):
            outer = int(tok[k][6:], 16)
            k += 1
        struct = tok[k]
        desc = ' '.join(tok[k + 1:])
        far = base_s.startswith('far:')
        base = int(base_s[4:] if far else base_s, 16)
        t = Table(name, base, far, size, outer, struct, desc)
        raw = [(off, w, nm, rec_desc.get((b, s, off, w), ''))
               for (b, s, off, w), nm in rec_names.items() if b == base and s == size]
        # a field reaching into the next record cannot be a struct member
        t.fields = [f for f in clean_names(raw) if f[0] + f[1] // 8 <= size]
        t.arrays = [a for a in arrays.get(name, [])]
        # explicit fields override rec entries in their byte range
        spans = [(a[0], a[0] + a[1] // 8 * prod(a[2])) for a in t.arrays]
        t.fields = [f for f in t.fields if not any(lo < f[0] + f[1] // 8 and f[0] < hi for lo, hi in spans)]
        t.fields += [(a[0], a[1], a[3], a[4]) for a in t.arrays if not a[2]]
        t.arrays = [a for a in t.arrays if a[2]]
        t.by_off = {(off, w): nm for off, w, nm, _ in t.fields}
        tables.append(t)
    return tables


def clean_names(raw):
    """strip the common prefix (film_genre -> genre) and dedup suffixes (_2)"""
    pref = collections.Counter(re.sub(r'_\d+$', '', nm).split('_')[0] for _, _, nm, _ in raw
                               if '_' in re.sub(r'_\d+$', '', nm))
    common = pref.most_common(1)[0][0] if pref and pref.most_common(1)[0][1] >= 2 else None
    out, seen = [], {}
    # wider fields first, so a byte view of a word gets the suffixed name
    for off, w, nm, d in sorted(raw, key=lambda f: (f[0], f[1])):
        n = re.sub(r'_\d+$', '', nm) or nm
        if common and n.startswith(common + '_') and len(n) > len(common) + 1:
            n2 = n[len(common) + 1:]
            if re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*', n2) and n2 not in C_KEYWORDS:
                n = n2
        if not re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*', n) or n in C_KEYWORDS:
            n = nm
        if n in seen:
            # a byte/word view of the same field keeps the name plus its width,
            # a different field keeps its original (suffixed) name
            n = '%s_%d' % (n, w) if seen[n] == off else nm
        if n in seen:
            n = '%s_%X' % (n, off)
        seen[n] = off
        out.append((off, w, n, d))
    return out


def layout(t):
    """C body of the struct: fields in offset order, overlapping groups as unions"""
    fs = sorted([(o, w, n, d, ()) for o, w, n, d in t.fields] + [(o, w, n, d, dims) for o, w, dims, n, d in t.arrays],
                key=lambda f: (f[0], -f[1]))
    size_of = lambda f: f[1] // 8 * prod(f[4])
    decl = lambda f: '%s %s%s;%s' % (UT[f[1]], f[2], ''.join('[%d]' % d for d in f[4]), ('   /* %s */' % f[3]) if f[3] else '')
    groups, cur, end = [], [], -1
    for f in fs:
        off = f[0]
        if f[0] >= t.size:
            continue
        if cur and off < end:
            cur.append(f)
            end = max(end, off + size_of(f))
        else:
            if cur:
                groups.append(cur)
            cur, end = [f], off + size_of(f)
    if cur:
        groups.append(cur)
    body, pos = [], 0

    def pad(n, at):
        return '    u8 _%02X[%d];' % (at, n) if n > 1 else '    u8 _%02X;' % at
    for g in groups:
        gstart = g[0][0]
        if gstart > pos:
            body.append(pad(gstart - pos, pos))
        if len(g) == 1:
            body.append('    ' + decl(g[0]))
            pos = g[0][0] + size_of(g[0])
            continue
        # layers of non-overlapping fields inside an anonymous union
        layers = []
        for f in g:
            for L in layers:
                if all(f[0] >= x[0] + size_of(x) or x[0] >= f[0] + size_of(f) for x in L):
                    L.append(f)
                    break
            else:
                layers.append([f])
        body.append('    union {')
        gend = max(f[0] + size_of(f) for f in g)
        for L in layers:
            L.sort()
            body.append('        struct {')
            p = gstart
            for f in L:
                if f[0] > p:
                    body.append('    ' + pad(f[0] - p, p))
                body.append('            ' + decl(f))
                p = f[0] + size_of(f)
            body.append('        };')
        body.append('    };')
        pos = gend
    if pos < t.size:
        body.append(pad(t.size - pos, pos))
    return body


def addr_macro(t):
    """#define NAME_at(...): far address of an element (for pointer locals)"""
    if t.far:
        ptr = 'G32(0x%04X)' % t.base
        if t.outer:
            return '#define %s_at(i, j) MK32(FPSEG(%s), (u16)(FPOFF(%s) + (i) * 0x%X + (j) * 0x%X))' % (t.name, ptr, ptr, t.outer, t.size)
        return '#define %s_at(i) MK32(FPSEG(%s), (u16)(FPOFF(%s) + (i) * 0x%X))' % (t.name, ptr, ptr, t.size)
    if t.outer:
        return '#define %s_at(i, j) MK32(DSEG, (u16)((i) * 0x%X + (j) * 0x%X + 0x%04X))' % (t.name, t.outer, t.size, t.base)
    return '#define %s_at(i) MK32(DSEG, (u16)((i) * 0x%X + 0x%04X))' % (t.name, t.size, t.base)


def array_field_macro(t, name):
    off, aw, dims = next((o, w, d) for o, w, d, n, _ in t.arrays if n == name)
    strides = [aw // 8 * prod(dims[k + 1:]) for k in range(len(dims))]
    tix = ['i', 'j'] if t.outer else ['i']
    fix = ['k%d' % n for n in range(len(dims))]
    terms = (['(i) * 0x%X' % t.outer, '(j) * 0x%X' % t.size] if t.outer else ['(i) * 0x%X' % t.size])
    terms += ['(%s) * 0x%X' % (k, st) for k, st in zip(fix, strides)]
    args = ', '.join(tix + fix)
    if t.far:
        ptr = 'G32(0x%04X)' % t.base
        return '#define %s_%s(%s) FP%d(%s, (u16)(FPOFF(%s) + %s + 0x%X))' % (
            t.name, name, args, aw, ptr, ptr, ' + '.join(terms), off)
    return '#define %s_%s(%s) G%d((u16)(%s + 0x%04X))' % (t.name, name, args, aw, ' + '.join(terms), t.base + off)


def c_definitions(tables):
    out = ['/* record tables (names/structs.txt): table(i).field or table(i, j).field */',
           '#pragma pack(push, 1)']
    for t in tables:
        if not (t.used or getattr(t, 'addr_used', False)):
            continue
        out.append('struct %s {   /* %s, 0x%X bytes */' % (t.struct, t.desc, t.size))
        out += layout(t)
        out.append('};')
        out.append('_Static_assert(sizeof(struct %s) == 0x%X, "struct %s");' % (t.struct, t.size, t.struct))
    out.append('#pragma pack(pop)')
    out.append('/* a far pointer to a table element: AT(struct, p).field */')
    out.append('#define AT(T, p) (*(struct T *)(M + lin(FPSEG(p), FPOFF(p))))')
    for t in tables:
        if getattr(t, 'addr_used', False):
            out.append(addr_macro(t))
    out.append('/* array fields: table_field(i, k) computes the 16-bit offset like the original */')
    for t in tables:
        for name in sorted(t.array_used or ()):
            out.append(array_field_macro(t, name))
    for t in tables:
        if not t.used:
            continue
        if t.far:
            ptr = 'G32(0x%04X)' % t.base
            if t.outer:
                out.append('#define %s(i, j) (*(struct %s *)(M + lin(FPSEG(%s), (u16)(FPOFF(%s) + (i) * 0x%X + (j) * 0x%X))))'
                           % (t.name, t.struct, ptr, ptr, t.outer, t.size))
            else:
                out.append('#define %s(i) (*(struct %s *)(M + lin(FPSEG(%s), (u16)(FPOFF(%s) + (i) * 0x%X))))'
                           % (t.name, t.struct, ptr, ptr, t.size))
        elif t.outer:
            out.append('#define %s(i, j) (*(struct %s *)(M + DSLIN + (u16)((i) * 0x%X + (j) * 0x%X + 0x%04X)))'
                       % (t.name, t.struct, t.outer, t.size, t.base))
        else:
            out.append('#define %s(i) (*(struct %s *)(M + DSLIN + (u16)((i) * 0x%X + 0x%04X)))'
                       % (t.name, t.struct, t.size, t.base))
    return out


# ---------------------------------------------------------------- matching
def flatten(e, coef=1, terms=None):
    """address expression -> ([(expr, coefficient)], constant)"""
    if terms is None:
        terms = []
    k = e.k
    if k == 'const':
        return terms, e.v * coef
    if k == 'bin' and e.v == '+':
        t1, c1 = flatten(e.a, coef, terms)
        t2, c2 = flatten(e.b, coef, terms)
        return terms, c1 + c2
    if k == 'bin' and e.v == '-' and e.b.k == 'const':
        t1, c1 = flatten(e.a, coef, terms)
        return terms, c1 - e.b.v * coef
    if k == 'bin' and e.v == '*' and e.b.k == 'const':
        return flatten(e.a, coef * e.b.v, terms)
    if k == 'bin' and e.v == '<<' and e.b.k == 'const':
        return flatten(e.a, coef << e.b.v, terms)
    terms.append((e, coef))
    return terms, 0


def match_addr(t, terms, const, mk_index):
    """index expressions if the address is the start of an element, else None"""
    off = const - (0 if t.far else t.base)
    if off < 0:
        return None
    outer_terms, inner_terms = [], []
    for e, c in terms:
        if t.outer and c % t.outer == 0:
            outer_terms.append((e, c // t.outer))
        elif c % t.size == 0:
            inner_terms.append((e, c // t.size))
        else:
            return None
    if not outer_terms and not inner_terms:
        return None
    if t.outer:
        ko, rem = divmod(off, t.outer)
        ki, f = divmod(rem, t.size)
        if f or ko > 64:
            return None
        return [mk_index(outer_terms, ko), mk_index(inner_terms, ki)]
    if outer_terms:
        return None
    ki, f = divmod(off, t.size)
    if f or ki > 256:
        return None
    return [mk_index(inner_terms, ki)]


def resolve_field(t, f, w, terms, mk_index):
    """field access at byte f of an element with the remaining index terms:
    (name, [index expressions]) -- indices only for array fields"""
    if not terms:
        name = t.by_off.get((f, w))
        if name is not None:
            return name, []
    for off, aw, dims, name, _ in t.arrays:
        size = aw // 8 * prod(dims)
        if aw != w or not off <= f < off + size:
            continue
        strides = [aw // 8 * prod(dims[k + 1:]) for k in range(len(dims))]
        per = [[] for _ in dims]
        for e, c in terms:
            for k, st in enumerate(strides):
                if c % st == 0:
                    per[k].append((e, c // st))
                    break
            else:
                return None
        rem, consts = f - off, []
        for st in strides:
            q, rem = divmod(rem, st)
            consts.append(q)
        if rem:
            return None
        return name, [mk_index(per[k], consts[k]) for k in range(len(dims))]
    return None


def match(t, terms, const, w, mk_index):
    """(index expressions, (field name, field indices)) or None"""
    off = const - (0 if t.far else t.base)
    if off < 0:
        return None
    outer_terms, inner_terms, field_terms = [], [], []
    for e, c in terms:
        if t.outer and c % t.outer == 0:
            outer_terms.append((e, c // t.outer))
        elif c % t.size == 0:
            inner_terms.append((e, c // t.size))
        else:
            field_terms.append((e, c))
    if t.outer:
        ko, rem = divmod(off, t.outer)
        ki, f = divmod(rem, t.size)
        if ko > 64 or (not outer_terms and not inner_terms):
            return None
        fld = resolve_field(t, f, w, field_terms, mk_index)
        if fld is None:
            return None
        return [mk_index(outer_terms, ko), mk_index(inner_terms, ki)], fld
    if outer_terms:
        return None
    ki, f = divmod(off, t.size)
    if ki > 256 or not inner_terms and ki == 0 and t.far:
        return None
    if not inner_terms and not t.far:
        return None                      # constant address: leave to global names
    fld = resolve_field(t, f, w, field_terms, mk_index)
    if fld is None:
        return None
    return [mk_index(inner_terms, ki)], fld
