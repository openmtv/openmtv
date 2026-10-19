#!/usr/bin/env python3
"""Recursive-descent disassembler for the unpacked MTV image.

Builds a database of every reachable instruction, function and jump table,
starting from the entry point and every far-call target the relocation table
reveals.  Everything downstream (listing, lifter, decompiler) reads db.json.
"""
import sys, json, collections, struct
from capstone import Cs, CS_ARCH_X86, CS_MODE_16
from capstone import x86_const as X
sys.path.insert(0, sys.path[0])
from mzimage import MZ

DGROUP = 0x39D4

class Disasm:
    def __init__(self, mz, extra_entries=()):
        self.m = mz
        self.img = mz.image
        self.md = Cs(CS_ARCH_X86, CS_MODE_16)
        self.md.detail = True
        self.insn = {}          # lin -> capstone insn (address = offset within seg)
        self.iseg = {}          # lin -> seg
        self.owner = {}         # lin(byte) -> lin of insn covering it
        self.func_entries = {}  # lin -> (seg, off, kind)
        self.jumptables = {}    # lin of jmp insn -> dict(table=lin, targets=[off..], kind)
        self.conflicts = []
        self.indirect = []      # unresolved indirect jumps/calls
        self.xref_calls = collections.defaultdict(set)  # target lin -> caller lin
        self.noreturn = set()
        self.code_segs = set()
        self.data_in_code = {}  # lin -> size (tables)
        self.work = []
        # Turbo C startup begins with 'mov dx, DGROUP'
        e = mz.cs * 16 + mz.ip
        self.dgroup = mz.w(e + 1) if mz.image[e] == 0xBA and (e + 1) in mz.reloc_set else 0xFFFF
        self._collect_far_targets()
        for s, o, k in extra_entries:
            self.add_func(s, o, k)

    # ---------- seeds ----------
    def _collect_far_targets(self):
        m = self.m
        self.add_func(m.cs, m.ip, 'entry')
        for l in m.reloc_lin:
            seg = m.w(l)
            if seg == self.dgroup or l < 3:
                continue
            if self.img[l - 3] in (0x9A, 0xEA):
                # only trust it if the call instruction itself lies in code we will decode;
                # we verify later that the call was actually reached
                self.code_segs.add(seg)

    def add_func(self, seg, off, kind):
        lin = seg * 16 + off
        if lin not in self.func_entries:
            self.func_entries[lin] = (seg, off, kind)
            self.work.append((seg, off))

    # ---------- decode ----------
    def decode(self, seg, off):
        lin = seg * 16 + off
        if lin in self.insn:
            return self.insn[lin]
        if lin >= len(self.img):
            return None
        try:
            i = next(self.md.disasm(self.img[lin:lin + 16], off, 1))
        except StopIteration:
            return None
        for k in range(lin, lin + i.size):
            if k in self.owner and self.owner[k] != lin:
                self.conflicts.append((seg, off, self.owner[k]))
        self.insn[lin] = i
        self.iseg[lin] = seg
        for k in range(lin, lin + i.size):
            self.owner.setdefault(k, lin)
        return i

    def run(self):
        while self.work:
            seg, off = self.work.pop()
            self.trace(seg, off)

    def trace(self, seg, off):
        while True:
            lin = seg * 16 + off
            if lin in self.insn:
                return
            i = self.decode(seg, off)
            if i is None:
                self.conflicts.append((seg, off, 'undecodable'))
                return
            mn = i.mnemonic
            nxt = (off + i.size) & 0xFFFF
            if mn in ('jmp', 'ljmp'):
                t = self.branch_target(i)
                if t is not None:
                    if isinstance(t, tuple):        # far jmp
                        self.add_func(t[0], t[1], 'jmpf')
                    else:
                        self.work.append((seg, t))
                else:
                    self.indirect_jump(seg, off, i)
                return
            if mn.startswith('j') or mn in ('loop', 'loope', 'loopne', 'jcxz'):
                t = self.branch_target(i)
                self.work.append((seg, t))
                off = nxt
                continue
            if mn in ('ret', 'retf', 'iret', 'hlt', 'retfw', 'iretw'):
                return
            if mn in ('call', 'lcall'):
                t = self.branch_target(i)
                if isinstance(t, tuple):
                    self.add_func(t[0], t[1], 'callf')
                    self.xref_calls[t[0] * 16 + t[1]].add(lin)
                    if t[0] * 16 + t[1] in self.noreturn:
                        return
                elif t is not None:
                    self.add_func(seg, t, 'calln')
                    self.xref_calls[seg * 16 + t].add(lin)
                    if seg * 16 + t in self.noreturn:
                        return
                else:
                    self.indirect.append((seg, off, i.mnemonic + ' ' + i.op_str))
                off = nxt
                continue
            if mn == 'int' and i.operands[0].imm == 0x20:
                return
            off = nxt

    def branch_target(self, i):
        op = i.operands[0]
        if op.type == X.X86_OP_IMM:
            if i.bytes[0] in (0x9A, 0xEA):
                o, s = struct.unpack_from('<HH', bytes(i.bytes), 1)
                return (s, o)
            return op.imm & 0xFFFF
        return None

    # ---------- switch tables ----------
    def prev_insns(self, seg, lin, n):
        """Linear predecessors (by address) within already-decoded code."""
        out = []
        cur = lin
        for _ in range(n):
            k = cur - 1
            while k >= cur - 8 and k not in self.insn:
                k -= 1
            if k not in self.insn or k + self.insn[k].size != cur:
                break
            out.append((k, self.insn[k]))
            cur = k
        return out

    def indirect_jump(self, seg, off, i):
        lin = seg * 16 + off
        b = bytes(i.bytes)
        disp = None
        # jmp word ptr cs:[bx+disp]   2E FF A7 d16  |  2E FF 67 d8
        if b[:3] == b'\x2e\xff\xa7' and len(b) == 5:
            disp = struct.unpack_from('<H', b, 3)[0]
        elif b[:3] == b'\x2e\xff\x67' and len(b) == 4:
            disp = b[3]
        if disp is None:
            self.indirect.append((seg, off, i.mnemonic + ' ' + i.op_str))
            return
        prev = self.prev_insns(seg, lin, 14)
        movbx = movcx = cmpimm = None
        for k, p in prev:
            ops = p.operands
            if p.mnemonic == 'mov' and len(ops) == 2 and ops[1].type == X.X86_OP_IMM and \
                    ops[0].type == X.X86_OP_REG:
                if p.op_str.startswith('bx,') and movbx is None: movbx = ops[1].imm & 0xFFFF
                if p.op_str.startswith('cx,') and movcx is None: movcx = ops[1].imm & 0xFFFF
            if p.mnemonic == 'cmp' and len(ops) == 2 and ops[0].type == X.X86_OP_REG and \
                    ops[1].type == X.X86_OP_IMM and cmpimm is None:
                cmpimm = ops[1].imm & 0xFFFF
        if movbx is not None and movcx is not None and disp == 2 * movcx:
            kind, count, base = 'sparse', movcx, movbx
            vlin = seg * 16 + base
            vals = [self.m.w(vlin + 2 * j) for j in range(count)]
            targets = [self.m.w(vlin + disp + 2 * j) for j in range(count)]
            self.data_in_code[vlin] = 4 * count
        elif cmpimm is not None and cmpimm < 512:
            kind, count, base, vals = 'dense', cmpimm + 1, disp, None
            tlin = seg * 16 + base
            targets = [self.m.w(tlin + 2 * j) for j in range(count)]
            self.data_in_code[tlin] = 2 * count
        else:
            self.indirect.append((seg, off, 'jmptable? ' + i.op_str))
            return
        self.jumptables[lin] = dict(seg=seg, table=base, kind=kind, count=count,
                                    targets=targets, values=vals)
        for t in targets:
            self.work.append((seg, t))

    # ---------- second-pass seeds ----------
    def seed_pointers(self):
        """Far function pointers built from a relocated segment immediate, near
        pointers pushed with 'push cs', and interrupt handlers installed via
        INT 21h/25h."""
        added = 0
        lins = sorted(self.insn)
        idx = {l: n for n, l in enumerate(lins)}
        for l in self.m.reloc_lin:
            seg = self.m.w(l)
            if seg not in self.code_segs or self.img[l - 3] in (0x9A, 0xEA):
                continue
            # instruction containing this reloc
            own = self.owner.get(l)
            if own is None:
                continue
            n = idx[own]
            for k in range(max(0, n - 4), min(len(lins), n + 5)):
                p = self.insn[lins[k]]
                if p.mnemonic in ('mov', 'push') and p.operands and \
                        p.operands[-1].type == X.X86_OP_IMM and lins[k] != own:
                    off = p.operands[-1].imm & 0xFFFF
                    if self.looks_like_code(seg, off):
                        self.add_func(seg, off, 'fptr'); added += 1
        for n, l in enumerate(lins):
            i = self.insn[l]
            if i.mnemonic == 'push' and i.op_str == 'cs' and n + 2 < len(lins):
                for k in (n + 1, n + 2):
                    p = self.insn[lins[k]]
                    if p.mnemonic in ('mov', 'push') and p.operands and \
                            p.operands[-1].type == X.X86_OP_IMM:
                        off = p.operands[-1].imm & 0xFFFF
                        if self.looks_like_code(self.iseg[l], off):
                            self.add_func(self.iseg[l], off, 'fptr-cs'); added += 1
        return added

    def looks_like_code(self, seg, off):
        lin = seg * 16 + off
        if lin in self.func_entries:
            return False
        if lin in self.insn:
            return False   # inside known code: it is a label, not a new function
        b = self.img[lin:lin + 3]
        return b[:3] == b'\x55\x8b\xec' or b[:1] in (b'\x1e', b'\x56', b'\x57', b'\x50', b'\x51', b'\x52', b'\x53', b'\xb8', b'\xfa', b'\x2e')

    def seed_orphans(self, seg_ranges):
        """Unreached code that starts with a standard prologue: dead or
        pointer-called functions.  Lifted anyway so nothing is missing."""
        added = 0
        for a, b, seg in seg_ranges:
            k = a
            while k < b:
                if k in self.owner or any(t <= k < t + n for t, n in self.data_in_code.items()):
                    k += 1; continue
                if self.img[k:k + 3] == b'\x55\x8b\xec' or \
                        (self.img[k:k + 5] == b'\x56\x57\x55\x8b\xec'):
                    self.add_func(seg, k - seg * 16, 'orphan'); added += 1
                    self.run()
                k += 1
        return added

    def seg_ranges(self):
        segs = sorted(s for s in (self.code_segs | {0}) if s < self.dgroup)
        bounds = [s * 16 for s in segs] + [self.dgroup * 16]
        return [(bounds[n], bounds[n + 1], segs[n]) for n in range(len(segs))]


def build(path, orphans=True):
    mz = MZ(path)
    d = Disasm(mz)
    d.run()
    while d.seed_pointers():
        d.run()
    if orphans:
        d.seed_orphans(d.seg_ranges())
        while d.seed_pointers():
            d.run()
    return d

if __name__ == '__main__':
    d = build(sys.argv[1])
    print('instructions', len(d.insn), 'functions', len(d.func_entries),
          'jumptables', len(d.jumptables), 'conflicts', len(d.conflicts))
    print('indirect:', len(d.indirect))
    for x in d.indirect[:80]:
        print('  %04X:%04X %s' % x)
    for c in d.conflicts[:30]:
        print('  conflict', c)
