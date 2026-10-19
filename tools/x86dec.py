"""Table-driven 8086 decoder producing a small IR for the lifter.

Semantics deliberately mirror runtime/interp.c so that recompiled code and
the reference interpreter agree by construction.
"""
import struct

R16 = ['ax', 'cx', 'dx', 'bx', 'sp', 'bp', 'si', 'di']
R8 = ['al', 'cl', 'dl', 'bl', 'ah', 'ch', 'dh', 'bh']
SREG = ['es', 'cs', 'ss', 'ds']
ALU = ['add', 'or', 'adc', 'sbb', 'and', 'sub', 'xor', 'cmp']
SHIFTS = ['rol', 'ror', 'rcl', 'rcr', 'shl', 'shr', 'sal', 'sar']
CC = ['o', 'no', 'b', 'ae', 'e', 'ne', 'be', 'a', 's', 'ns', 'p', 'np', 'l', 'ge', 'le', 'g']
EA_BASE = [('bx', 'si'), ('bx', 'di'), ('bp', 'si'), ('bp', 'di'), ('si',), ('di',), ('bp',), ('bx',)]


class Op:
    """Operand. kind: reg, sreg, imm, mem, rel, far"""
    __slots__ = ('kind', 'w', 'name', 'val', 'reloc', 'seg', 'base', 'disp', 'off')

    def __init__(self, kind, w=16, **kw):
        self.kind, self.w = kind, w
        self.name = self.val = self.seg = self.base = self.disp = self.off = None
        self.reloc = False
        for k, v in kw.items():
            setattr(self, k, v)

    def __repr__(self):
        if self.kind == 'reg': return self.name
        if self.kind == 'sreg': return self.name
        if self.kind == 'imm': return ('SEG+' if self.reloc else '') + hex(self.val)
        if self.kind == 'mem':
            return '%s:[%s%+d]' % (self.seg, '+'.join(self.base), self.disp)
        if self.kind == 'rel': return 'L%04X' % self.val
        if self.kind == 'far': return '%s%04X:%04X' % ('SEG+' if self.reloc else '', self.seg, self.off)
        return self.kind


class Insn:
    __slots__ = ('lin', 'seg', 'off', 'size', 'op', 'w', 'ops', 'cc', 'rep', 'segovr',
                 'raw', 'imm_pos', 'next')

    def __repr__(self):
        r = (self.rep + ' ') if self.rep else ''
        return '%04X:%04X %s%s%s %s' % (self.seg, self.off, r, self.op,
                                         ('.' + self.cc) if self.cc else '',
                                         ', '.join(map(repr, self.ops)))


class Decoder:
    def __init__(self, img, relocs=()):
        self.img = img
        self.relocs = set(relocs)   # linear addresses of relocated words

    def decode(self, seg, off):
        img = self.img
        lin0 = seg * 16 + off
        p = lin0
        i = Insn()
        i.seg, i.off, i.lin = seg, off, lin0
        i.rep = i.segovr = i.cc = None
        i.ops = []
        i.w = 16
        i.imm_pos = None

        def u8():
            nonlocal p
            v = img[p]; p += 1; return v

        def s8():
            v = u8(); return v - 256 if v >= 128 else v

        def u16():
            nonlocal p
            v = img[p] | (img[p + 1] << 8); p += 2; return v

        def imm(w):
            pos = p
            v = u8() if w == 8 else u16()
            return Op('imm', w, val=v, reloc=(w == 16 and pos in self.relocs))

        def modrm(w):
            m = u8()
            mod, reg, rm = m >> 6, (m >> 3) & 7, m & 7
            if mod == 3:
                rmop = Op('reg', w, name=(R16 if w == 16 else R8)[rm])
            else:
                if mod == 0 and rm == 6:
                    base, disp, dseg = (), u16(), 'ds'
                else:
                    base = EA_BASE[rm]
                    dseg = 'ss' if 'bp' in base else 'ds'
                    disp = 0
                    if mod == 1: disp = s8()
                    elif mod == 2: disp = u16()
                    if disp >= 0x8000: disp -= 0x10000
                rmop = Op('mem', w, seg=i.segovr or dseg, base=base, disp=disp)
            return reg, rmop

        def regop(w, n):
            return Op('reg', w, name=(R16 if w == 16 else R8)[n])

        # prefixes
        while True:
            b = img[p]
            if b in (0x26, 0x2E, 0x36, 0x3E):
                i.segovr = SREG[(b >> 3) & 3]; p += 1
            elif b == 0xF2: i.rep = 'repne'; p += 1
            elif b == 0xF3: i.rep = 'rep'; p += 1
            elif b == 0xF0: p += 1
            else:
                break
        op = u8()
        o = op
        if o < 0x40 and (o & 7) < 6:
            a = ALU[o >> 3]
            w = 8 if (o & 1) == 0 else 16
            if (o & 7) < 4:
                reg, rm = modrm(w)
                r = regop(w, reg)
                i.op, i.w = a, w
                i.ops = [rm, r] if (o & 2) == 0 else [r, rm]
            else:
                i.op, i.w = a, w
                i.ops = [regop(w, 0), imm(w)]
        elif o in (0x06, 0x0E, 0x16, 0x1E):
            i.op, i.ops = 'push', [Op('sreg', name=SREG[o >> 3])]
        elif o in (0x07, 0x17, 0x1F, 0x0F):
            i.op, i.ops = 'pop', [Op('sreg', name=SREG[o >> 3])]
        elif o in (0x27, 0x2F, 0x37, 0x3F):
            i.op = {0x27: 'daa', 0x2F: 'das', 0x37: 'aaa', 0x3F: 'aas'}[o]
        elif 0x40 <= o < 0x50:
            i.op = 'inc' if o < 0x48 else 'dec'
            i.ops = [regop(16, o & 7)]
        elif 0x50 <= o < 0x60:
            i.op = 'push' if o < 0x58 else 'pop'
            i.ops = [regop(16, o & 7)]
        elif o == 0x60: i.op = 'pusha'
        elif o == 0x61: i.op = 'popa'
        elif o == 0x68: i.op, i.ops = 'push', [imm(16)]
        elif o == 0x6A:
            v = s8(); i.op, i.ops = 'push', [Op('imm', 16, val=v & 0xFFFF)]
        elif o in (0x69, 0x6B):
            reg, rm = modrm(16)
            if o == 0x69: im = imm(16)
            else: im = Op('imm', 16, val=s8() & 0xFFFF)
            i.op, i.ops = 'imul3', [regop(16, reg), rm, im]
        elif 0x70 <= o < 0x80:
            d = s8()
            i.op, i.cc = 'jcc', CC[o & 15]
            i.ops = [Op('rel', val=(off + (p - lin0) + d) & 0xFFFF)]
        elif o in (0x80, 0x81, 0x82, 0x83):
            w = 8 if o in (0x80, 0x82) else 16
            reg, rm = modrm(w)
            if o == 0x83:
                v = s8() & 0xFFFF; im = Op('imm', 16, val=v)
            else:
                im = imm(w)
            i.op, i.w, i.ops = ALU[reg], w, [rm, im]
        elif o in (0x84, 0x85):
            w = 8 if o == 0x84 else 16
            reg, rm = modrm(w)
            i.op, i.w, i.ops = 'test', w, [rm, regop(w, reg)]
        elif o in (0x86, 0x87):
            w = 8 if o == 0x86 else 16
            reg, rm = modrm(w)
            i.op, i.w, i.ops = 'xchg', w, [rm, regop(w, reg)]
        elif 0x88 <= o <= 0x8B:
            w = 8 if (o & 1) == 0 else 16
            reg, rm = modrm(w)
            r = regop(w, reg)
            i.op, i.w = 'mov', w
            i.ops = [rm, r] if o < 0x8A else [r, rm]
        elif o == 0x8C:
            reg, rm = modrm(16)
            i.op, i.ops = 'mov', [rm, Op('sreg', name=SREG[reg & 3])]
        elif o == 0x8D:
            reg, rm = modrm(16)
            i.op, i.ops = 'lea', [regop(16, reg), rm]
        elif o == 0x8E:
            reg, rm = modrm(16)
            i.op, i.ops = 'mov', [Op('sreg', name=SREG[reg & 3]), rm]
        elif o == 0x8F:
            reg, rm = modrm(16)
            i.op, i.ops = 'pop', [rm]
        elif o == 0x90: i.op = 'nop'
        elif 0x91 <= o <= 0x97:
            i.op, i.ops = 'xchg', [regop(16, 0), regop(16, o & 7)]
        elif o == 0x98: i.op = 'cbw'
        elif o == 0x99: i.op = 'cwd'
        elif o == 0x9A:
            ofs = u16(); spos = p; sg = u16()
            i.op, i.ops = 'callf', [Op('far', seg=sg, off=ofs, reloc=spos in self.relocs)]
        elif o == 0x9B: i.op = 'nop'
        elif o == 0x9C: i.op = 'pushf'
        elif o == 0x9D: i.op = 'popf'
        elif o == 0x9E: i.op = 'sahf'
        elif o == 0x9F: i.op = 'lahf'
        elif 0xA0 <= o <= 0xA3:
            w = 8 if (o & 1) == 0 else 16
            m = Op('mem', w, seg=i.segovr or 'ds', base=(), disp=u16())
            r = regop(w, 0)
            i.op, i.w = 'mov', w
            i.ops = [r, m] if o < 0xA2 else [m, r]
        elif o in (0xA4, 0xA5, 0xA6, 0xA7, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF):
            i.op = {0xA4: 'movs', 0xA6: 'cmps', 0xAA: 'stos', 0xAC: 'lods', 0xAE: 'scas'}[o & 0xFE]
            i.w = 8 if (o & 1) == 0 else 16
        elif o in (0xA8, 0xA9):
            w = 8 if o == 0xA8 else 16
            i.op, i.w, i.ops = 'test', w, [regop(w, 0), imm(w)]
        elif 0xB0 <= o < 0xB8:
            i.op, i.w, i.ops = 'mov', 8, [regop(8, o & 7), imm(8)]
        elif 0xB8 <= o < 0xC0:
            i.op, i.w, i.ops = 'mov', 16, [regop(16, o & 7), imm(16)]
        elif o in (0xC0, 0xC1, 0xD0, 0xD1, 0xD2, 0xD3):
            w = 8 if (o & 1) == 0 else 16
            reg, rm = modrm(w)
            if o in (0xC0, 0xC1): cnt = Op('imm', 8, val=u8())
            elif o in (0xD0, 0xD1): cnt = Op('imm', 8, val=1)
            else: cnt = Op('reg', 8, name='cl')
            i.op, i.w, i.ops = SHIFTS[reg], w, [rm, cnt]
        elif o == 0xC2: i.op, i.ops = 'ret', [Op('imm', 16, val=u16())]
        elif o == 0xC3: i.op = 'ret'
        elif o in (0xC4, 0xC5):
            reg, rm = modrm(16)
            i.op, i.ops = ('les' if o == 0xC4 else 'lds'), [regop(16, reg), rm]
        elif o in (0xC6, 0xC7):
            w = 8 if o == 0xC6 else 16
            reg, rm = modrm(w)
            i.op, i.w, i.ops = 'mov', w, [rm, imm(w)]
        elif o == 0xC8:
            sz = u16(); lv = u8()
            i.op, i.ops = 'enter', [Op('imm', 16, val=sz), Op('imm', 8, val=lv)]
        elif o == 0xC9: i.op = 'leave'
        elif o == 0xCA: i.op, i.ops = 'retf', [Op('imm', 16, val=u16())]
        elif o == 0xCB: i.op = 'retf'
        elif o == 0xCC: i.op, i.ops = 'int', [Op('imm', 8, val=3)]
        elif o == 0xCD: i.op, i.ops = 'int', [Op('imm', 8, val=u8())]
        elif o == 0xCE: i.op = 'into'
        elif o == 0xCF: i.op = 'iret'
        elif o in (0xD4, 0xD5):
            i.op, i.ops = ('aam' if o == 0xD4 else 'aad'), [Op('imm', 8, val=u8())]
        elif o == 0xD6: i.op = 'salc'
        elif o == 0xD7: i.op = 'xlat'
        elif 0xD8 <= o <= 0xDF:
            modrm(16); i.op = 'esc'
        elif o in (0xE0, 0xE1, 0xE2, 0xE3):
            d = s8()
            i.op = {0xE0: 'loopne', 0xE1: 'loope', 0xE2: 'loop', 0xE3: 'jcxz'}[o]
            i.ops = [Op('rel', val=(off + (p - lin0) + d) & 0xFFFF)]
        elif o in (0xE4, 0xE5):
            i.op, i.w, i.ops = 'in', (8 if o == 0xE4 else 16), [Op('imm', 8, val=u8())]
        elif o in (0xE6, 0xE7):
            i.op, i.w, i.ops = 'out', (8 if o == 0xE6 else 16), [Op('imm', 8, val=u8())]
        elif o == 0xE8:
            d = u16()
            i.op, i.ops = 'call', [Op('rel', val=(off + (p - lin0) + d) & 0xFFFF)]
        elif o == 0xE9:
            d = u16()
            i.op, i.ops = 'jmp', [Op('rel', val=(off + (p - lin0) + d) & 0xFFFF)]
        elif o == 0xEA:
            ofs = u16(); spos = p; sg = u16()
            i.op, i.ops = 'jmpf', [Op('far', seg=sg, off=ofs, reloc=spos in self.relocs)]
        elif o == 0xEB:
            d = s8()
            i.op, i.ops = 'jmp', [Op('rel', val=(off + (p - lin0) + d) & 0xFFFF)]
        elif o in (0xEC, 0xED):
            i.op, i.w, i.ops = 'in', (8 if o == 0xEC else 16), [Op('reg', 16, name='dx')]
        elif o in (0xEE, 0xEF):
            i.op, i.w, i.ops = 'out', (8 if o == 0xEE else 16), [Op('reg', 16, name='dx')]
        elif o == 0xF4: i.op = 'hlt'
        elif o == 0xF5: i.op = 'cmc'
        elif o in (0xF6, 0xF7):
            w = 8 if o == 0xF6 else 16
            reg, rm = modrm(w)
            if reg in (0, 1):
                i.op, i.w, i.ops = 'test', w, [rm, imm(w)]
            else:
                i.op, i.w, i.ops = ['', '', 'not', 'neg', 'mul', 'imul', 'div', 'idiv'][reg], w, [rm]
        elif o == 0xF8: i.op = 'clc'
        elif o == 0xF9: i.op = 'stc'
        elif o == 0xFA: i.op = 'cli'
        elif o == 0xFB: i.op = 'sti'
        elif o == 0xFC: i.op = 'cld'
        elif o == 0xFD: i.op = 'std'
        elif o == 0xFE:
            reg, rm = modrm(8)
            if reg > 1: raise ValueError('bad FE /%d at %04X:%04X' % (reg, seg, off))
            i.op, i.w, i.ops = ('inc', 'dec')[reg], 8, [rm]
        elif o == 0xFF:
            reg, rm = modrm(16)
            i.op = ['inc', 'dec', 'calli', 'callfi', 'jmpi', 'jmpfi', 'push', '?'][reg]
            if i.op == '?': raise ValueError('bad FF /7')
            i.ops = [rm]
        else:
            raise ValueError('undecodable opcode %02X at %04X:%04X' % (o, seg, off))
        i.size = p - lin0
        i.raw = bytes(img[lin0:p])
        i.next = (off + i.size) & 0xFFFF
        # any relocation inside this instruction not attributed to an operand?
        return i
