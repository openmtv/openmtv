"""Control-flow structuring for decompiled functions.

Input: basic blocks with straight-line C statements and a terminator:
    ('goto', t) | ('if', cond, t_true, t_false) | ('switch', expr, [(value, t)], default_line)
    | ('ret', [lines])
Output: C text using if/else, for(;;)/while loops with break/continue and
switch; a labelled goto is used only where the flow is not reducible to
those forms.  Every block is emitted exactly once.
"""
import collections, re
import cleanup

CALL_NAMES = {'call_lifted', 'call_far_addr', 'struct_copy', 'cpu_halt'}   # decomp.py adds all function names
IDENT_CALL_RE = re.compile(r'\b([A-Za-z_][A-Za-z_0-9]*)\s*\(')
MEM_RE = re.compile(r'\b(?:G|FP|MEM|LOC)(?:8|16|32)\(|\)\.[a-z_]|\bR\.')


class Block:
    __slots__ = ('id', 'stmts', 'term', 'ninsn')

    def __init__(self, id, stmts, term, ninsn):
        self.id, self.stmts, self.term, self.ninsn = id, stmts, term, ninsn


def succs(b):
    t = b.term
    if t[0] == 'goto':
        return [t[1]]
    if t[0] == 'if':
        return [t[2], t[3]]
    if t[0] == 'switch':
        return [x for _, x in t[2]]
    return []


def negate(c):
    c = c.strip()
    pairs = [(' == ', ' != '), (' != ', ' == '), (' < ', ' >= '), (' >= ', ' < '), (' > ', ' <= '), (' <= ', ' > ')]
    depth = 0
    top_ops = []
    for n, ch in enumerate(c):
        depth += ch == '('
        depth -= ch == ')'
        if depth == 0:
            for a, b in pairs:
                if c.startswith(a, n):
                    top_ops.append((n, a, b))
    if len(top_ops) == 1 and '&&' not in c and '||' not in c and '?' not in c:
        n, a, b = top_ops[0]
        return c[:n] + b + c[n + len(a):]
    if c.startswith('!') and c[1:].startswith('(') and c.endswith(')'):
        return c[2:-1]
    return '!(%s)' % c


def top_level_ops(c):
    """operators && / || that appear outside parentheses"""
    depth, ops = 0, set()
    for n, ch in enumerate(c):
        depth += ch == '('
        depth -= ch == ')'
        if depth == 0 and c.startswith('&&', n):
            ops.add('&&')
        if depth == 0 and c.startswith('||', n):
            ops.add('||')
    return ops


def combine(a, op, b):
    def wrap(c):
        other = top_level_ops(c) - {op}
        return '(%s)' % c if other or '?' in c else c
    return '%s %s %s' % (wrap(a), op, wrap(b))


def is_call_free(e):
    return not any(m.group(1) in CALL_NAMES or m.group(1).startswith(('c_', 'f_'))
                   for m in IDENT_CALL_RE.finditer(e))


def simplify_conditions(blocks, entry, protected=(), decls=None):
    """inline a block's last temporary into its condition, then merge blocks
    that only test a condition into && / || chains (short-circuit operators
    keep the original evaluation order)"""
    B = {b.id: b for b in blocks}
    text = lambda: '\n'.join(sum((b.stmts + [str(b.term)] for b in B.values()), []))
    # 0) jump threading: a target that is an empty block with just a goto
    def final(t):
        seen = set()
        while t in B and not B[t].stmts and B[t].term[0] == 'goto' and t not in seen and t != entry:
            seen.add(t)
            t = B[t].term[1]
        return t
    for b in blocks:
        tm = b.term
        if tm[0] == 'goto':
            b.term = ('goto', final(tm[1]))
        elif tm[0] == 'if':
            b.term = ('if', tm[1], final(tm[2]), final(tm[3]))
        elif tm[0] == 'switch':
            b.term = ('switch', tm[1], [(v, final(x)) for v, x in tm[2]], tm[3])
    # 1) last statement 'tN = expr;' used only by this block's condition
    alltext = text()
    for b in blocks:
        while b.term[0] == 'if' and b.stmts:
            m = re.fullmatch(r'(t\d+) = (.*);', b.stmts[-1])
            if not m:
                break
            t, e = m.group(1), m.group(2)
            if len(re.findall(r'\b%s\b' % t, alltext)) != 2 or len(re.findall(r'\b%s\b' % t, b.term[1])) != 1:
                break
            rest = re.sub(r'\b%s\b' % t, '', b.term[1])
            if cleanup.has_call(e) and (cleanup.has_call(rest) or cleanup.reads_memory(rest)):
                break                       # a call must not move across other reads
            if cleanup.reads_memory(e) and cleanup.has_call(rest):
                break
            if decls is None or t not in decls:
                break
            rep = cleanup.typed(e, decls[t], decls)
            cond = re.sub(r'\b%s\b' % t, lambda _: rep if rep != e or re.fullmatch(r'\w+', e) else '(%s)' % e, b.term[1])
            b.term = ('if', cond, b.term[2], b.term[3])
            b.stmts = b.stmts[:-1]
            alltext = text()
    # 2) merge condition-only blocks
    preds = collections.defaultdict(set)
    for b in blocks:
        for x in succs(b):
            preds[x].add(b.id)
    changed = True
    while changed:
        changed = False
        for a in blocks:
            if a.id not in B or a.term[0] != 'if':
                continue
            _, c1, at, af = a.term
            for inner, other, first_true in ((at, af, True), (af, at, False)):
                bb = B.get(inner)
                if bb is None or bb.id == a.id or bb.id == entry or bb.id in protected or bb.stmts or \
                        bb.term[0] != 'if' or preds[bb.id] != {a.id}:
                    continue
                _, c2, bt, bf = bb.term
                if other not in (bt, bf) or bt == bf:
                    continue
                if first_true:              # a: c1 -> bb, else other
                    if bf == other:
                        new = ('if', combine(c1, '&&', c2), bt, other)
                    else:
                        new = ('if', combine(c1, '&&', negate(c2)), bf, other)
                else:                       # a: c1 -> other, else bb
                    if bt == other:
                        new = ('if', combine(c1, '||', c2), other, bf)
                    else:
                        new = ('if', combine(c1, '||', negate(c2)), other, bt)
                a.term = new
                a.ninsn += bb.ninsn         # the tick accounting moves with it
                bb.ninsn = 0
                for x in succs(bb):
                    preds[x].discard(bb.id)
                    preds[x].add(a.id)
                del B[bb.id]
                changed = True
                break
    return [b for b in blocks if b.id in B]


class Structurer:
    def __init__(self, blocks, entry, decls=None):
        blocks = simplify_conditions(blocks, entry, decls=decls)
        self.B = {b.id: b for b in blocks}
        self.order = [b.id for b in blocks]          # address order
        self.entry = entry
        self.preds = collections.defaultdict(set)
        for b in blocks:
            for s in succs(b):
                self.preds[s].add(b.id)
        self.reach = self.reachable()
        self.dom = self.dominators()
        self.pdom = self.postdominators()
        self.loops = self.find_loops()
        self.emitted = set()
        self.labels = set()
        self.out = []

    def reachable(self):
        seen, st = set(), [self.entry]
        while st:
            n = st.pop()
            if n in seen or n not in self.B:
                continue
            seen.add(n)
            st.extend(succs(self.B[n]))
        return seen

    def dominators(self):
        nodes = [n for n in self.order if n in self.reach]
        dom = {n: set(nodes) for n in nodes}
        dom[self.entry] = {self.entry}
        changed = True
        while changed:
            changed = False
            for n in nodes:
                if n == self.entry:
                    continue
                ps = [p for p in self.preds[n] if p in self.reach]
                new = set.intersection(*(dom[p] for p in ps)) if ps else set()
                new = new | {n}
                if new != dom[n]:
                    dom[n] = new
                    changed = True
        return dom

    def postdominators(self):
        nodes = [n for n in self.order if n in self.reach]
        EXIT = 'EXIT'
        sx = {n: [s for s in succs(self.B[n]) if s in self.reach] or [EXIT] for n in nodes}
        pd = {n: set(nodes) | {EXIT} for n in nodes}
        pd[EXIT] = {EXIT}
        changed = True
        while changed:
            changed = False
            for n in reversed(nodes):
                new = set.intersection(*(pd[s] for s in sx[n])) | {n}
                if new != pd[n]:
                    pd[n] = new
                    changed = True
        return pd

    def ipdom(self, n):
        cands = self.pdom[n] - {n}
        # the immediate post-dominator is the candidate post-dominated by all others
        for c in cands:
            if all(c == o or o in self.pdom.get(c, {c}) or o == 'EXIT' and c != 'EXIT' and False
                   for o in cands):
                pass
        best = None
        for c in cands:
            if c == 'EXIT':
                continue
            if all(o == c or o in self.pdom[c] for o in cands):
                best = c
                break
        return best

    def find_loops(self):
        loops = {}
        for n in self.order:
            if n not in self.reach:
                continue
            for s in succs(self.B[n]):
                if s in self.dom.get(n, ()):          # back edge n -> s
                    body = loops.setdefault(s, {s})
                    st = [n]
                    while st:
                        x = st.pop()
                        if x in body:
                            continue
                        body.add(x)
                        st.extend(p for p in self.preds[x] if p in self.reach)
        return loops

    # ---------------------------------------------------------------- emission
    def line(self, depth, s):
        self.out.append('    ' * depth + s)

    def loop_follow(self, h):
        body = self.loops[h]
        exits = collections.Counter()
        for n in body:
            for s in succs(self.B[n]):
                if s not in body:
                    exits[s] += 1
        if not exits:
            return None
        # prefer the exit that is reached from the header, else the most used
        hs = [s for s in succs(self.B[h]) if s not in body]
        if hs:
            return hs[0]
        return exits.most_common(1)[0][0]

    def jump(self, t, depth, ctx):
        """emit control transfer to t from inside structured code; True if
        the transfer was handled (no fallthrough continues)"""
        for h, follow in reversed(ctx):
            if t == h:
                self.line(depth, 'continue;' if ctx[-1][0] == h else 'goto L_%04X;' % t)
                if ctx[-1][0] != h:
                    self.labels.add(t)
                return True
            if t == follow:
                self.line(depth, 'break;' if ctx[-1][1] == t else 'goto L_%04X;' % t)
                if ctx[-1][1] != t:
                    self.labels.add(t)
                return True
        if t in self.emitted:
            if self.duplicate(t, depth, ctx):
                return True
            self.line(depth, 'goto L_%04X;' % t)
            self.labels.add(t)
            return True
        return False

    def ends_structured(self, t, ctx, budget=2):
        """block t can be copied: few statements and it ends in return or a
        jump that needs no label (continue/break, or another such block)"""
        b = self.B.get(t)
        if b is None or t in self.loops or len(b.stmts) > 3 or budget == 0:
            return False
        if b.term[0] == 'ret':
            return True
        if b.term[0] != 'goto':
            return False
        n = b.term[1]
        if self.is_ctl(n, ctx) and any(n == h or n == f for h, f in ctx):
            return True
        return n in self.emitted and self.ends_structured(n, ctx, budget - 1)

    def duplicate(self, t, depth, ctx):
        if not self.ends_structured(t, ctx):
            return False
        b = self.B[t]
        for st in b.stmts:
            self.line(depth, st)
        if b.term[0] == 'ret':
            for st in b.term[1]:
                self.line(depth, st)
            return True
        return self.jump(b.term[1], depth, ctx)

    def emit_seq(self, n, stop, depth, ctx, loop_body=None):
        while n is not None and n != stop:
            if loop_body is not None and n not in loop_body:
                # leaving the loop without going through its follow
                self.jump(n, depth, ctx) or (self.line(depth, 'goto L_%04X;' % n), self.labels.add(n))
                return
            if n in self.emitted:
                self.jump(n, depth, ctx)
                return
            if ctx and n == ctx[-1][0] and n in self.emitted:
                self.line(depth, 'continue;')
                return
            if n in self.loops and not any(h == n for h, _ in ctx):
                follow = self.loop_follow(n)
                self.line(depth, 'for (;;) {')
                self.emit_block(n, depth + 1, ctx + [(n, follow)], self.loops[n], is_head=True)
                self.line(depth, '}')
                if follow is None:
                    return
                n = follow
                continue
            n = self.emit_block(n, depth, ctx, loop_body)
            if n == 'DONE':
                return
        if n is not None and n == stop:
            return

    def emit_block(self, n, depth, ctx, loop_body, is_head=False):
        """emit block n; returns the next block to continue with (or 'DONE')"""
        b = self.B[n]
        self.emitted.add(n)
        self.out.append(('LABEL', n))
        if is_head:
            ninsn = sum(self.B[x].ninsn for x in loop_body)
            self.line(depth, 'LOOP_TICK(%d);' % ninsn)
        for s in b.stmts:
            self.line(depth, s)
        t = b.term
        inner = loop_body if is_head else loop_body
        if t[0] == 'ret':
            for s in t[1]:
                self.line(depth, s)
            return 'DONE'
        if t[0] == 'goto':
            nx = t[1]
            if self.jump(nx, depth, ctx):
                return 'DONE'
            if inner is not None and nx not in inner:
                self.line(depth, 'goto L_%04X;' % nx); self.labels.add(nx)
                return 'DONE'
            if is_head:
                self.emit_seq(nx, None, depth, ctx, inner)
                return 'DONE'
            return nx
        if t[0] == 'if':
            cond, tt, ff = t[1], t[2], t[3]
            # branches that leave the current loop / continue it
            for a, bb, c in ((tt, ff, cond), (ff, tt, negate(cond))):
                if self.is_ctl(a, ctx) and not self.is_ctl(bb, ctx):
                    self.line(depth, 'if (%s) {' % c)
                    self.jump(a, depth + 1, ctx)
                    self.line(depth, '}')
                    if self.jump(bb, depth, ctx):
                        return 'DONE'
                    if inner is not None and bb not in inner:
                        self.line(depth, 'goto L_%04X;' % bb); self.labels.add(bb)
                        return 'DONE'
                    if is_head:
                        self.emit_seq(bb, None, depth, ctx, inner)
                        return 'DONE'
                    return bb
            join = self.ipdom(n)
            if inner is not None and join is not None and join not in inner:
                join = None
            if join == tt:
                tt, ff, cond = ff, tt, negate(cond)
            self.line(depth, 'if (%s) {' % cond)
            self.emit_seq(tt, join, depth + 1, ctx, inner)
            if ff != join:
                self.line(depth, '} else {')
                self.emit_seq(ff, join, depth + 1, ctx, inner)
            self.line(depth, '}')
            if join is None:
                return 'DONE'
            if is_head:
                self.emit_seq(join, None, depth, ctx, inner)
                return 'DONE'
            return join
        if t[0] == 'switch':
            expr, cases, dflt = t[1], t[2], t[3]
            targets = []
            for v, x in cases:
                if x not in targets:
                    targets.append(x)
            join = self.ipdom(n)
            if inner is not None and join is not None and join not in inner:
                join = None
            self.line(depth, 'switch (%s) {' % expr)
            bytarget = collections.OrderedDict()
            for v, x in cases:
                bytarget.setdefault(x, []).append(v)
            for x, vals in bytarget.items():
                for v in vals:
                    self.line(depth, 'case %s:' % v)
                if x == join:
                    self.line(depth + 1, 'break;')
                    continue
                if x in self.emitted or self.is_ctl(x, ctx):
                    self.jump(x, depth + 1, ctx)
                    continue
                before = len(self.out)
                self.emit_seq(x, join, depth + 1, ctx + [(None, join)] if join is not None else ctx, inner)
                last = next((l for l in reversed(self.out[before:]) if isinstance(l, str)), '')
                if last.strip() not in ('break;', 'continue;') and not last.strip().startswith(('return', 'goto ')):
                    self.line(depth + 1, 'break;')
            self.line(depth, 'default:')
            self.line(depth + 1, dflt)
            self.line(depth, '}')
            if join is None:
                return 'DONE'
            if is_head:
                self.emit_seq(join, None, depth, ctx, inner)
                return 'DONE'
            return join
        raise ValueError(t)

    def is_ctl(self, t, ctx):
        return any(t == h or t == f for h, f in ctx if h is not None) or any(t == f for h, f in ctx if h is None)

    def run(self, depth=1):
        self.emit_seq(self.entry, None, depth, [])
        # blocks never reached by structured emission (irreducible pieces)
        for n in self.order:
            if n in self.reach and n not in self.emitted:
                self.labels.add(n)
                self.emit_seq(n, None, depth, [])
        lines = []
        for x in self.out:
            if isinstance(x, tuple):
                if x[1] in self.labels:
                    lines.append('L_%04X:' % x[1])
                continue
            lines.append(x)
        return lines
