#!/usr/bin/env python3
"""Static recompiler: 8086 machine code of MTV.EXE -> C.

Every function found by disasm.py becomes one C function operating on the
shared machine state (runtime/cpu.h).  Flag computations are pruned by a
liveness analysis, and the common compare+branch pairs are fused into plain
C conditions.  Output: gen/*.c, gen/lifted.h, gen/table.c
"""
import sys, os, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import disasm
from x86dec import Decoder, Op

DGROUP = 0x39D4
FC, FP, FA, FZ, FS, FO = 1, 2, 4, 8, 16, 32
FALL = 63
CC_USE = {'o': FO, 'no': FO, 'b': FC, 'ae': FC, 'e': FZ, 'ne': FZ, 'be': FC | FZ, 'a': FC | FZ,
          's': FS, 'ns': FS, 'p': FP, 'np': FP, 'l': FS | FO, 'ge': FS | FO, 'le': FZ | FS | FO,
          'g': FZ | FS | FO}
CC_R = {'o': 'R.of', 'no': '!R.of', 'b': 'R.cf', 'ae': '!R.cf', 'e': 'R.zf', 'ne': '!R.zf',
        'be': '(R.cf || R.zf)', 'a': '(!R.cf && !R.zf)', 's': 'R.sf', 'ns': '!R.sf', 'p': 'R.pf',
        'np': '!R.pf', 'l': '(R.sf != R.of)', 'ge': '(R.sf == R.of)',
        'le': '(R.zf || R.sf != R.of)', 'g': '(!R.zf && R.sf == R.of)'}

# Addresses whose far-jump operands are patched at runtime (PALLADIX stubs)
DYNAMIC_JMPF_SEGS = {0x371E}


def fname(seg, off):
    return 'f_%04X_%04X' % (seg, off)


class Func:
    def __init__(self, seg, off):
        self.seg, self.off = seg, off
        self.insns = {}        # off -> Insn
        self.labels = set()
        self.loopheads = set()
        self.calls = []        # (insn, target (seg,off) or None)
        self.ret_flags = 0     # flags callers read after we return
        self.in_flags = 0      # flags read before written
        self.jumptables = {}   # off of jmp -> list of targets


class Lifter:
    def __init__(self, exe):
        self.exe_path = exe
        self.d = disasm.build(exe)
        self.img = self.d.img
        self.dec = Decoder(self.img, self.d.m.reloc_lin)
        self.funcs = {}
        entries = {(s, o) for s, o, k in self.d.func_entries.values()}
        # PALLADIX driver stubs (13 far-jump slots patched by the loader) and
        # the video-mode restore routine registered as an exit handler.
        if self.d.dgroup == 0x39D4:      # MTV.EXE: PALLADIX stubs + exit handler
            for k in range(13):
                entries.add((0x371E, 0x83 + 5 * k))
            entries.add((0x3741, 0x0000))
        self.entries = sorted(entries)
        self.jt = {}           # lin of jmp -> jumptable dict (from disasm)
        for lin, t in self.d.jumptables.items():
            self.jt[lin] = t

    # ---------------- CFG ----------------
    def build_func(self, seg, off):
        f = Func(seg, off)
        work = [off]
        while work:
            o = work.pop()
            while o not in f.insns:
                i = self.dec.decode(seg, o)
                f.insns[o] = i
                op = i.op
                if op == 'jcc' or op in ('loop', 'loope', 'loopne', 'jcxz'):
                    t = i.ops[0].val
                    f.labels.add(t); work.append(t)
                    o = i.next; continue
                if op == 'jmp':
                    t = i.ops[0].val
                    f.labels.add(t); work.append(t)
                    break
                if op == 'jmpi':
                    jt = self.jt.get(i.lin)
                    if jt:
                        f.jumptables[o] = jt
                        for t in jt['targets']:
                            f.labels.add(t); work.append(t)
                    break
                if op in ('ret', 'retf', 'iret', 'jmpf', 'jmpfi', 'hlt_stop'):
                    break
                if op == 'int' and i.ops[0].val == 0x20:
                    break
                o = i.next
        # loop heads: targets of branches that go backwards (or to self)
        for o, i in f.insns.items():
            if i.op in ('jcc', 'jmp', 'loop', 'loope', 'loopne', 'jcxz') and i.ops[0].val <= o:
                f.loopheads.add(i.ops[0].val)
            if i.op == 'jmpi' and o in f.jumptables:
                for t in f.jumptables[o]['targets']:
                    if t <= o: f.loopheads.add(t)
        return f

    def find_interp_only(self):
        """Functions that compute their return point (Turbo C's printf engine
        pops its return address, adds 3 and jumps to it) cannot be C
        functions.  They and their direct callers run in the interpreter."""
        dyn = {k for k, f in self.funcs.items()
               if any(i.op == 'jmpi' and o not in f.jumptables and i.ops[0].kind == 'reg'
                      for o, i in f.insns.items())}
        callers = {k for k, f in self.funcs.items()
                   if any(i.op in ('call', 'callf') and self.call_target(i) in dyn for i in f.insns.values())}
        self.interp_only = dyn | callers
        if self.interp_only:
            print('interpreted (computed returns): ' + ' '.join('%04X:%04X' % k for k in sorted(self.interp_only)))

    def build_all(self):
        todo = list(self.entries)
        while todo:
            s, o = todo.pop()
            if (s, o) in self.funcs:
                continue
            f = self.funcs[(s, o)] = self.build_func(s, o)
            for i in f.insns.values():
                t = self.call_target(i)
                if t and (i.op != 'callf' or i.ops[0].reloc) and t not in self.funcs:
                    if i.op != 'jmpf' or f.seg not in DYNAMIC_JMPF_SEGS:
                        todo.append(t)

    # ---------------- flags ----------------
    def succs(self, f, o):
        i = f.insns[o]
        op = i.op
        if op == 'jcc' or op in ('loop', 'loope', 'loopne', 'jcxz'):
            return [i.next, i.ops[0].val]
        if op == 'jmp':
            return [i.ops[0].val]
        if op == 'jmpi':
            return list(f.jumptables[o]['targets']) if o in f.jumptables else []
        if op in ('ret', 'retf', 'iret', 'jmpf', 'jmpfi'):
            return []
        if op == 'int' and i.ops[0].val == 0x20:
            return []
        return [i.next]

    def use_def(self, f, i):
        """(use, def) flag masks for one instruction."""
        op = i.op
        if op in ('add', 'sub', 'cmp', 'neg'):
            return 0, FALL
        if op in ('adc', 'sbb'):
            return FC, FALL
        if op in ('and', 'or', 'xor', 'test'):
            return 0, FALL
        if op in ('inc', 'dec'):
            return 0, FALL & ~FC
        if op in ('rol', 'ror', 'rcl', 'rcr', 'shl', 'shr', 'sal', 'sar'):
            cnt = i.ops[1]
            use = FC if op in ('rcl', 'rcr') else 0
            d = (FC | FO) if op in ('rol', 'ror', 'rcl', 'rcr') else FALL
            if cnt.kind == 'reg':           # count may be 0: flags unchanged
                return use | d, d
            return use, d if cnt.val else 0
        if op in ('mul', 'imul', 'div', 'idiv', 'imul3'):
            return 0, FALL
        if op == 'jcc':
            return CC_USE[i.cc], 0
        if op in ('loope', 'loopne'):
            return FZ, 0
        if op in ('pushf', 'lahf'):
            return FALL, 0
        if op in ('popf', 'sahf'):
            return 0, FALL
        if op in ('clc', 'stc'):
            return 0, FC
        if op == 'cmc':
            return FC, FC
        if op in ('daa', 'das'):
            return FC | FA, FALL
        if op in ('aaa', 'aas'):
            return FA, FALL
        if op in ('aam', 'aad'):
            return 0, FALL
        if op in ('cmps', 'scas'):
            return (FALL if i.rep else 0), FALL
        if op == 'int':
            return 0, FALL
        if op in ('call', 'callf'):
            tgt = self.call_target(i)
            g = self.funcs.get(tgt)
            return (g.in_flags if g else 0), FALL
        if op in ('calli', 'callfi'):
            return 0, FALL
        if op in ('ret', 'retf'):
            return f.ret_flags, 0
        if op == 'salc':
            return FC, 0
        if op == 'iret':
            return 0, 0
        return 0, 0

    def call_target(self, i):
        if i.op == 'call':
            return (i.seg, i.ops[0].val)
        if i.op == 'callf':
            return (i.ops[0].seg, i.ops[0].off)
        if i.op == 'jmpf':
            return (i.ops[0].seg, i.ops[0].off)
        return None

    def liveness(self, f):
        """Per-instruction live-out flag sets."""
        offs = sorted(f.insns)
        live_in = {o: 0 for o in offs}
        live_out = {o: 0 for o in offs}
        ud = {o: self.use_def(f, f.insns[o]) for o in offs}
        changed = True
        while changed:
            changed = False
            for o in reversed(offs):
                out = 0
                for s in self.succs(f, o):
                    out |= live_in.get(s, 0)
                u, d = ud[o]
                inn = u | (out & ~d)
                if out != live_out[o] or inn != live_in[o]:
                    live_out[o], live_in[o] = out, inn
                    changed = True
        f.live_out = live_out
        return live_in[f.off]

    def flags_fixpoint(self):
        for it in range(20):
            changed = False
            for f in self.funcs.values():
                inn = self.liveness(f)
                if inn != f.in_flags:
                    f.in_flags = inn; changed = True
            for f in self.funcs.values():
                for o, i in f.insns.items():
                    if i.op in ('call', 'callf'):
                        need = f.live_out[o]
                        g = self.funcs.get(self.call_target(i))
                        if g and (g.ret_flags | need) != g.ret_flags:
                            g.ret_flags |= need; changed = True
            if not changed:
                break
        nret = sum(1 for f in self.funcs.values() if f.ret_flags)
        nin = sum(1 for f in self.funcs.values() if f.in_flags)
        print('flags fixpoint after %d iterations: %d funcs return flags, %d read flags on entry' % (it + 1, nret, nin))

    # ---------------- codegen ----------------
    def csv(self, f):
        return 'SEGV(0x%04X)' % f.seg

    def segexpr(self, f, s):
        return self.csv(f) if s == 'cs' else 'R.' + s

    def offexpr(self, m):
        parts = ['R.' + b for b in m.base]
        if m.disp or not parts:
            parts.append('0x%X' % (m.disp & 0xFFFF) if m.disp >= 0 or not parts else '%d' % m.disp)
        e = ' + '.join(parts)
        return e if len(parts) == 1 and not m.base else '(u16)(%s)' % e

    def rd(self, f, o, w=None):
        w = w or o.w
        if o.kind == 'reg':
            return 'R.' + o.name
        if o.kind == 'sreg':
            return self.csv(f) if o.name == 'cs' else 'R.' + o.name
        if o.kind == 'imm':
            if o.reloc:
                return 'SEGV(0x%04X)' % o.val
            return '0x%X' % o.val
        if o.kind == 'mem':
            return '%s(%s, %s)' % ('rb' if w == 8 else 'rw', self.segexpr(f, o.seg), self.offexpr(o))
        raise ValueError(o.kind)

    def wr(self, f, o, val, w=None):
        w = w or o.w
        if o.kind == 'reg':
            return 'R.%s = %s;' % (o.name, val)
        if o.kind == 'sreg':
            if o.name == 'cs':
                return 'cpu_halt("write to CS");'
            return 'R.%s = %s;' % (o.name, val)
        if o.kind == 'mem':
            return '%s(%s, %s, %s);' % ('wb' if w == 8 else 'ww', self.segexpr(f, o.seg), self.offexpr(o), val)
        raise ValueError(o.kind)

    def falls_through(self, f, i):
        return i.next in self.succs(f, i.off) and i.op != 'jmp'

    # instructions whose effect can observe the virtual clock T (or leave the
    # block); T is brought up to date right before them
    OBSERVE = {'in', 'out', 'int', 'into', 'hlt', 'call', 'callf', 'calli', 'callfi', 'jmpi',
               'jmpfi', 'jmpf', 'ret', 'retf', 'iret', 'movs', 'cmps', 'stos', 'lods', 'scas',
               'sti', 'jcc', 'jmp', 'loop', 'loope', 'loopne', 'jcxz'}

    def emit_func(self, f, out):
        offs = sorted(f.insns)
        need_label = set(f.labels)
        for o, t in f.jumptables.items():
            need_label.update(t['targets'])
        # explicit gotos where fallthrough is not the next emitted instruction
        for n, o in enumerate(offs):
            i = f.insns[o]
            if self.falls_through(f, i) and (n + 1 >= len(offs) or offs[n + 1] != i.next):
                need_label.add(i.next)
        polls = {o for o in offs if (f.seg * 16 + o) in self.poll_lin}
        out.append('__attribute__((weak)) void %s(void) {' % fname(f.seg, f.off))
        out.append('    CHK_ENTER(0x%04X, 0x%04X);' % (f.seg, f.off))
        if any(i.op == 'jmpfi' or (i.op == 'jmpi' and o not in f.jumptables) for o, i in f.insns.items()):
            out.append('    const u16 ra_off_ = rw(R.ss, R.sp), ra_seg_ = rw(R.ss, (u16)(R.sp + 2));')
        if offs[0] != f.off:
            out.append('    goto L_%04X;' % f.off)
            need_label.add(f.off)
        pending = 0

        def flush():
            nonlocal pending
            if pending:
                out.append('    T += %d;' % pending)
                pending = 0

        k = 0
        while k < len(offs):
            o = offs[k]
            i = f.insns[o]
            if o in need_label or o in polls:
                flush()
            if o in need_label:
                out.append('L_%04X:' % o)
            if o in polls:
                out.append('    POLL();')
            last = i
            nxt = f.insns.get(i.next) if k + 1 < len(offs) and offs[k + 1] == i.next else None
            code = None
            if nxt is not None and nxt.op == 'jcc' and i.next not in need_label and i.next not in polls:
                code = self.try_fuse(f, i, nxt)
            if code is not None:
                out.append('    /* %s ; %s */' % (self.asm(i), self.asm(nxt)))
                pending += 2
                flush()
                out.extend('    ' + c for c in code)
                last = nxt
                k += 2
            else:
                out.append('    /* %s */' % self.asm(i))
                pending += 1
                if i.op in self.OBSERVE:
                    flush()
                for c in self.emit_insn(f, i):
                    out.append('    ' + c)
                k += 1
            if self.falls_through(f, last) and (k >= len(offs) or offs[k] != last.next):
                flush()
                out.append('    goto L_%04X;' % last.next)
        flush()
        out.append('}')
        out.append('')

    def asm(self, i):
        return repr(i).split(' ', 1)[1].replace('*/', '* /')

    # fused compare/test + conditional jump
    def try_fuse(self, f, d, j):
        op = d.op
        jo = d.off
        live_after_j = f.live_out[j.off]
        u, df = self.use_def(f, d)
        if live_after_j & df:
            return None
        if f.live_out[d.off] & ~CC_USE[j.cc]:
            return None
        cc = j.cc
        w = d.w
        U = 'u8' if w == 8 else 'u16'
        S = 's8' if w == 8 else 's16'
        tgt = 'goto L_%04X;' % j.ops[0].val
        code = []
        if op in ('cmp', 'sub'):
            a, b = self.rd(f, d.ops[0]), self.rd(f, d.ops[1], w)
            conds = {'e': 'a_ == b_', 'ne': 'a_ != b_', 'b': 'a_ < b_', 'ae': 'a_ >= b_',
                     'be': 'a_ <= b_', 'a': 'a_ > b_', 'l': '(%s)a_ < (%s)b_' % (S, S),
                     'ge': '(%s)a_ >= (%s)b_' % (S, S), 'le': '(%s)a_ <= (%s)b_' % (S, S),
                     'g': '(%s)a_ > (%s)b_' % (S, S), 's': '(%s)(a_ - b_) < 0' % S,
                     'ns': '(%s)(a_ - b_) >= 0' % S}
            if cc not in conds:
                return None
            code.append('{ %s a_ = %s, b_ = %s;' % (U, a, b))
            if op == 'sub':
                code.append('  ' + self.wr(f, d.ops[0], '(%s)(a_ - b_)' % U))
            code.append('  if (%s) %s }' % (conds[cc], tgt))
            return code
        if op in ('test', 'and', 'or', 'xor'):
            a, b = self.rd(f, d.ops[0]), self.rd(f, d.ops[1], w)
            sym = {'test': '&', 'and': '&', 'or': '|', 'xor': '^'}[op]
            if op in ('or', 'and') and d.ops[0].kind == 'reg' and d.ops[1].kind == 'reg' and \
                    d.ops[0].name == d.ops[1].name:
                rexpr = a
            else:
                rexpr = '(%s)(%s %s %s)' % (U, a, sym, b)
            conds = {'e': 'r_ == 0', 'ne': 'r_ != 0', 's': '(%s)r_ < 0' % S, 'ns': '(%s)r_ >= 0' % S,
                     'l': '(%s)r_ < 0' % S, 'ge': '(%s)r_ >= 0' % S, 'le': '(%s)r_ <= 0' % S,
                     'g': '(%s)r_ > 0' % S, 'b': '0', 'ae': '1', 'be': 'r_ == 0', 'a': 'r_ != 0',
                     'o': '0', 'no': '1'}
            if cc not in conds:
                return None
            code.append('{ %s r_ = %s;' % (U, rexpr))
            if op in ('and', 'or', 'xor') and rexpr != a:
                code.append('  ' + self.wr(f, d.ops[0], 'r_'))
            code.append('  if (%s) %s }' % (conds[cc], tgt))
            return code
        if op in ('inc', 'dec', 'add'):
            if cc not in ('e', 'ne', 's', 'ns'):
                return None
            a = self.rd(f, d.ops[0])
            if op == 'add':
                rexpr = '(%s)(%s + %s)' % (U, a, self.rd(f, d.ops[1], w))
            else:
                rexpr = '(%s)(%s %s 1)' % (U, a, '+' if op == 'inc' else '-')
            conds = {'e': 'r_ == 0', 'ne': 'r_ != 0', 's': '(%s)r_ < 0' % S, 'ns': '(%s)r_ >= 0' % S}
            code.append('{ %s r_ = %s;' % (U, rexpr))
            code.append('  ' + self.wr(f, d.ops[0], 'r_'))
            code.append('  if (%s) %s }' % (conds[cc], tgt))
            return code
        return None

    def emit_insn(self, f, i):
        op = i.op
        live = f.live_out[i.off]
        w = i.w
        U = 'u8' if w == 8 else 'u16'
        S = 's8' if w == 8 else 's16'
        sfx = '8' if w == 8 else '16'
        ops = i.ops
        c = []
        if op == 'mov':
            c.append(self.wr(f, ops[0], self.rd(f, ops[1], w)))
        elif op in ('add', 'sub', 'adc', 'sbb', 'cmp'):
            a, b = self.rd(f, ops[0]), self.rd(f, ops[1], w)
            carry = 'R.cf' if op in ('adc', 'sbb') else '0'
            fn = ('add' if op in ('add', 'adc') else 'sub') + sfx
            if op == 'cmp':
                if live:
                    c.append('%s(%s, %s, 0);' % (fn, a, b))
            elif live:
                c.append(self.wr(f, ops[0], '%s(%s, %s, %s)' % (fn, a, b, carry)))
            else:
                sym = '+' if fn.startswith('add') else '-'
                cexp = ' %s R.cf' % sym if op in ('adc', 'sbb') else ''
                c.append(self.wr(f, ops[0], '(%s)(%s %s %s%s)' % (U, a, sym, b, cexp)))
        elif op in ('and', 'or', 'xor', 'test'):
            a, b = self.rd(f, ops[0]), self.rd(f, ops[1], w)
            sym = {'test': '&', 'and': '&', 'or': '|', 'xor': '^'}[op]
            if op == 'xor' and ops[0].kind == 'reg' and ops[1].kind == 'reg' and ops[0].name == ops[1].name:
                expr = '0'
            else:
                expr = '(%s)(%s %s %s)' % (U, a, sym, b)
            if op == 'test':
                if live:
                    c.append('logic%s(%s);' % (sfx, expr))
            elif live:
                c.append(self.wr(f, ops[0], 'logic%s(%s)' % (sfx, expr)))
            else:
                if not (op in ('and', 'or') and expr == '(%s)(%s %s %s)' % (U, a, sym, a)):
                    c.append(self.wr(f, ops[0], expr))
        elif op in ('inc', 'dec'):
            a = self.rd(f, ops[0])
            if live:
                c.append(self.wr(f, ops[0], '%s%s(%s)' % (op, sfx, a)))
            else:
                c.append(self.wr(f, ops[0], '(%s)(%s %s 1)' % (U, a, '+' if op == 'inc' else '-')))
        elif op == 'neg':
            a = self.rd(f, ops[0])
            c.append(self.wr(f, ops[0], ('neg%s(%s)' % (sfx, a)) if live else '(%s)-%s' % (U, a)))
        elif op == 'not':
            a = self.rd(f, ops[0])
            c.append(self.wr(f, ops[0], '(%s)~%s' % (U, a)))
        elif op in ('rol', 'ror', 'rcl', 'rcr', 'shl', 'shr', 'sal', 'sar'):
            a = self.rd(f, ops[0])
            cnt = ops[1]
            kind = 'SH_' + op.upper()
            if cnt.kind == 'imm' and not live and op in ('shl', 'sal', 'shr', 'sar') and cnt.val < w:
                if op in ('shl', 'sal'):
                    expr = '(%s)(%s << %d)' % (U, a, cnt.val)
                elif op == 'shr':
                    expr = '(%s)(%s >> %d)' % (U, a, cnt.val)
                else:
                    expr = '(%s)((%s)%s >> %d)' % (U, S, a, cnt.val)
                c.append(self.wr(f, ops[0], expr))
            else:
                cexpr = 'R.cl' if cnt.kind == 'reg' else str(cnt.val)
                c.append(self.wr(f, ops[0], 'shift%s(%s, %s, %s)' % (sfx, kind, a, cexpr)))
        elif op in ('mul', 'imul', 'div', 'idiv'):
            c.append('%s%s(%s);' % (op, sfx, self.rd(f, ops[0])))
        elif op == 'imul3':
            c.append('{ s32 r_ = (s32)(s16)%s * (s16)%s; R.%s = (u16)r_; R.cf = R.of = r_ != (s16)r_; }' %
                     (self.rd(f, ops[1]), self.rd(f, ops[2]), ops[0].name))
        elif op == 'xchg':
            a, b = ops
            c.append('{ %s t_ = %s; %s %s }' % (U, self.rd(f, a), self.wr(f, a, self.rd(f, b)), self.wr(f, b, 't_')))
        elif op == 'lea':
            c.append('R.%s = %s;' % (ops[0].name, self.offexpr(ops[1])))
        elif op in ('les', 'lds'):
            m = ops[1]
            sg = self.segexpr(f, m.seg)
            c.append('{ u16 o_ = %s; u16 v_ = rw(%s, o_), s_ = rw(%s, (u16)(o_ + 2)); R.%s = v_; R.%s = s_; }' %
                     (self.offexpr(m), sg, sg, ops[0].name, 'es' if op == 'les' else 'ds'))
        elif op == 'push':
            o = ops[0]
            if o.kind == 'reg' and o.name == 'sp':
                c.append('{ u16 v_ = R.sp - 2; push16(v_); }')
            else:
                c.append('push16(%s);' % self.rd(f, o, 16))
        elif op == 'pop':
            o = ops[0]
            if o.kind == 'mem':
                c.append('{ u16 v_ = pop16(); %s }' % self.wr(f, o, 'v_', 16))
            else:
                c.append(self.wr(f, o, 'pop16()', 16))
        elif op == 'pusha':
            c.append('{ u16 sp_ = R.sp; push16(R.ax); push16(R.cx); push16(R.dx); push16(R.bx); push16(sp_); push16(R.bp); push16(R.si); push16(R.di); }')
        elif op == 'popa':
            c.append('R.di = pop16(); R.si = pop16(); R.bp = pop16(); pop16(); R.bx = pop16(); R.dx = pop16(); R.cx = pop16(); R.ax = pop16();')
        elif op == 'cbw':
            c.append('R.ax = (u16)(s8)R.al;')
        elif op == 'cwd':
            c.append('R.dx = (R.ax & 0x8000) ? 0xFFFF : 0;')
        elif op == 'pushf':
            c.append('push16(get_flags());')
        elif op == 'popf':
            c.append('set_flags(pop16());')
        elif op == 'sahf':
            c.append('{ u8 f_ = R.ah; R.cf = f_ & 1; R.pf = (f_ >> 2) & 1; R.af = (f_ >> 4) & 1; R.zf = (f_ >> 6) & 1; R.sf = (f_ >> 7) & 1; }')
        elif op == 'lahf':
            c.append('R.ah = (u8)get_flags();')
        elif op == 'clc': c.append('R.cf = 0;')
        elif op == 'stc': c.append('R.cf = 1;')
        elif op == 'cmc': c.append('R.cf ^= 1;')
        elif op == 'cld': c.append('R.df = 0;')
        elif op == 'std': c.append('R.df = 1;')
        elif op == 'cli': c.append('R.ifl = 0;')
        elif op == 'sti': c.append('R.ifl = 1; POLL();')
        elif op == 'hlt': c.append('runtime_halt_wait();')
        elif op in ('nop', 'esc'): c.append(';')
        elif op in ('daa', 'das', 'aaa', 'aas'): c.append('%s();' % op)
        elif op in ('aam', 'aad'): c.append('%s(0x%X);' % (op, ops[0].val))
        elif op == 'salc': c.append('R.al = R.cf ? 0xFF : 0;')
        elif op == 'xlat':
            c.append('R.al = rb(%s, (u16)(R.bx + R.al));' % self.segexpr(f, i.segovr or 'ds'))
        elif op in ('movs', 'cmps', 'stos', 'lods', 'scas'):
            base = {'movs': 0xA4, 'cmps': 0xA6, 'stos': 0xAA, 'lods': 0xAC, 'scas': 0xAE}[op]
            opc = base + (1 if w == 16 else 0)
            rep = {None: '0', 'rep': '0xF3', 'repne': '0xF2'}[i.rep]
            c.append('string_op_ex(0x%02X, %s, %s);' % (opc, rep, self.segexpr(f, i.segovr or 'ds')))
        elif op == 'in':
            port = 'R.dx' if ops[0].kind == 'reg' else '0x%X' % ops[0].val
            c.append('R.%s = port_in%d(%s);' % ('al' if w == 8 else 'ax', w, port))
        elif op == 'out':
            port = 'R.dx' if ops[0].kind == 'reg' else '0x%X' % ops[0].val
            c.append('port_out%d(%s, R.%s);' % (w, port, 'al' if w == 8 else 'ax'))
        elif op == 'int':
            c.append('do_int(0x%02X);' % ops[0].val)
        elif op == 'into':
            c.append('if (R.of) do_int(4);')
        elif op == 'jcc':
            c.append('if (%s) goto L_%04X;' % (CC_R[i.cc], ops[0].val))
        elif op == 'jmp':
            c.append('goto L_%04X;' % ops[0].val)
        elif op == 'loop':
            c.append('if (--R.cx) goto L_%04X;' % ops[0].val)
        elif op == 'loope':
            c.append('if (--R.cx && R.zf) goto L_%04X;' % ops[0].val)
        elif op == 'loopne':
            c.append('if (--R.cx && !R.zf) goto L_%04X;' % ops[0].val)
        elif op == 'jcxz':
            c.append('if (!R.cx) goto L_%04X;' % ops[0].val)
        elif op == 'call':
            t = (f.seg, ops[0].val)
            if t in getattr(self, 'interp_only', ()):
                c.append('push16(0x%04X); call_near(%s, 0x%04X);' % (i.next, self.csv(f), t[1]))
            else:
                c.append('push16(0x%04X); %s();' % (i.next, fname(*t)))
        elif op == 'callf':
            fo = ops[0]
            if not fo.reloc:
                c.append('push16(%s); push16(0x%04X); call_far(0x%04X, 0x%04X);' % (self.csv(f), i.next, fo.seg, fo.off))
            elif (fo.seg, fo.off) in self.funcs and (fo.seg, fo.off) not in getattr(self, 'interp_only', ()):
                c.append('push16(%s); push16(0x%04X); %s();' % (self.csv(f), i.next, fname(fo.seg, fo.off)))
            else:
                c.append('push16(%s); push16(0x%04X); call_far(SEGV(0x%04X), 0x%04X);' % (self.csv(f), i.next, fo.seg, fo.off))
        elif op == 'calli':
            c.append('{ u16 t_ = %s; push16(0x%04X); call_near(%s, t_); }' % (self.rd(f, ops[0]), i.next, self.csv(f)))
        elif op == 'callfi':
            m = ops[0]
            sg = self.segexpr(f, m.seg)
            c.append('{ u16 a_ = %s; u16 o_ = rw(%s, a_), s_ = rw(%s, (u16)(a_ + 2)); push16(%s); push16(0x%04X); call_far(s_, o_); }' %
                     (self.offexpr(m), sg, sg, self.csv(f), i.next))
        elif op == 'jmpi':
            if i.off in f.jumptables:
                jt = f.jumptables[i.off]
                c.append('switch (%s) {' % self.rd(f, ops[0]))
                for t in sorted(set(jt['targets'])):
                    c.append('case 0x%04X: goto L_%04X;' % (t, t))
                c.append('default: cpu_halt("bad switch target %%04X at %04X:%04X", %s); }' %
                         (f.seg, i.off, self.rd(f, ops[0])))
            else:
                c.append('{ u16 t_ = %s; if (t_ == ra_off_) return; call_near(%s, t_); return; }' % (self.rd(f, ops[0]), self.csv(f)))
        elif op == 'jmpfi':
            m = ops[0]
            sg = self.segexpr(f, m.seg)
            c.append('{ u16 a_ = %s; u16 o_ = rw(%s, a_), s_ = rw(%s, (u16)(a_ + 2));' % (self.offexpr(m), sg, sg))
            c.append('  if (o_ == ra_off_ && s_ == ra_seg_) return;  /* jump to own return address */')
            c.append('  call_far(s_, o_); return; }')
        elif op == 'jmpf':
            fo = ops[0]
            if f.seg in DYNAMIC_JMPF_SEGS:
                c.append('call_far(rw(%s, 0x%04X), rw(%s, 0x%04X)); return;  /* patched at runtime */' %
                         (self.csv(f), (i.off + 3) & 0xFFFF, self.csv(f), (i.off + 1) & 0xFFFF))
            elif fo.reloc and (fo.seg, fo.off) in self.funcs:
                c.append('%s(); return;' % fname(fo.seg, fo.off))
            else:
                seg = 'SEGV(0x%04X)' % fo.seg if fo.reloc else '0x%04X' % fo.seg
                c.append('call_far(%s, 0x%04X); return;' % (seg, fo.off))
        elif op == 'ret':
            n = ops[0].val if ops else 0
            c.append('RET_NEAR(0x%04X, %d);' % (i.off, n))
        elif op == 'retf':
            n = ops[0].val if ops else 0
            c.append('RET_FAR(0x%04X, %d);' % (i.off, n))
        elif op == 'iret':
            c.append('RET_IRET(0x%04X);' % i.off)
        else:
            raise ValueError('no codegen for %r' % i)
        return c

    def compute_polls(self):
        self.poll_lin = set()
        for (sg, o), f in self.funcs.items():
            self.poll_lin.add(sg * 16 + o)
            for h in f.loopheads:
                self.poll_lin.add(sg * 16 + h)

    def generate(self, outdir):
        self.compute_polls()
        if not hasattr(self, 'interp_only'):
            self.find_interp_only()
        for k in self.interp_only:
            self.funcs.pop(k, None)
        os.makedirs(outdir, exist_ok=True)
        for fn in os.listdir(outdir):
            if fn.endswith('.c') or fn.endswith('.h'):
                os.remove(os.path.join(outdir, fn))
        bysegs = collections.defaultdict(list)
        for (s, o), f in sorted(self.funcs.items()):
            bysegs[s].append(f)
        hdr = ['/* Generated by tools/lift.py -- do not edit. */', '#ifndef LIFTED_H', '#define LIFTED_H',
               '#include "runtime.h"',
               '#define SEGV(s) ((u16)(LOADSEG_C + (s)))',
               '#define POLL() do { LOGICAL_SYNC(); if (T >= next_event_T) runtime_events(); if (irq_pending && R.ifl) irq_poll(); } while (0)',
               '/* Every function remembers SP on entry.  A RET executed with more on the',
               '   stack than at entry is a computed jump (push seg; push off; retf). */',
               'void chk_ret_fail(u16 seg, u16 off, u16 at, u16 entry_sp);',
               'void computed_ret(int far, u16 cs);',
               '#define CHK_ENTER(s, o) const u16 chk_seg_ = (s), chk_off_ = (o), chk_sp_ = R.sp; (void)chk_seg_; (void)chk_off_; clock_entry((s), (o))',
               '#ifdef RETCHECK',
               '#define CHK_BAD(at) chk_ret_fail(chk_seg_, chk_off_, (at), chk_sp_)',
               '#else',
               '#define CHK_BAD(at) do {} while (0)',
               '#endif',
               '#define RET_NEAR(at, n) do { if (R.sp == chk_sp_) { R.sp += 2 + (n); return; } \\',
               '    if ((s16)(chk_sp_ - R.sp) > 0) { computed_ret(0, SEGV(chk_seg_)); return; } CHK_BAD(at); R.sp += 2 + (n); return; } while (0)',
               '#define RET_FAR(at, n) do { if (R.sp == chk_sp_) { R.sp += 4 + (n); return; } \\',
               '    if ((s16)(chk_sp_ - R.sp) > 0) { computed_ret(1, 0); return; } CHK_BAD(at); R.sp += 4 + (n); return; } while (0)',
               '#define RET_IRET(at) do { if (R.sp != chk_sp_) CHK_BAD(at); R.sp += 4; set_flags(pop16()); return; } while (0)', '']
        for (s, o) in sorted(self.funcs):
            hdr.append('void %s(void);' % fname(s, o))
        hdr.append('#endif')
        open(os.path.join(outdir, 'lifted.h'), 'w').write('\n'.join(hdr) + '\n')
        total_lines = 0
        for s, fl in sorted(bysegs.items()):
            out = ['/* Generated by tools/lift.py from %s segment %04X -- do not edit. */' % (os.path.basename(self.exe_path), s),
                   '#include "lifted.h"', '']
            for f in fl:
                self.emit_func(f, out)
            open(os.path.join(outdir, 'seg_%04X.c' % s), 'w').write('\n'.join(out) + '\n')
            total_lines += len(out)
        tab = ['/* Generated by tools/lift.py -- do not edit. */', '#include "lifted.h"', '',
               'const native_entry native_table[] = {']
        for (s, o) in sorted(self.funcs, key=lambda k: (k[0] << 16) | k[1]):
            tab.append('    {0x%04X%04X, %s},' % (s, o, fname(s, o)))
        tab.append('};')
        tab.append('/* image-relative linear addresses where both engines may take interrupts */')
        tab.append('const u32 poll_points[] = {')
        pl = sorted(self.poll_lin)
        for n in range(0, len(pl), 10):
            tab.append('    ' + ', '.join('0x%05X' % x for x in pl[n:n + 10]) + ',')
        tab.append('};')
        tab.append('const int poll_count = %d;' % len(pl))
        tab.append('const u8 native_retflags[] = {')
        tab.append(', '.join(str(self.funcs[k].ret_flags) for k in sorted(self.funcs, key=lambda k: (k[0] << 16) | k[1])))
        tab.append('};')
        tab.append('const int native_count = %d;' % len(self.funcs))
        tab.append('void native_entry_point(void) { R.cs = SEGV(0x%04X); %s(); }' %
                   (self.d.m.cs, fname(self.d.m.cs, self.d.m.ip)))
        open(os.path.join(outdir, 'table.c'), 'w').write('\n'.join(tab) + '\n')
        print('generated %d functions, %d segments, %d lines' % (len(self.funcs), len(bysegs), total_lines))


if __name__ == '__main__':
    L = Lifter(sys.argv[1])
    L.build_all()
    L.flags_fixpoint()
    L.generate(sys.argv[2])
