"""Register liveness under the Turbo C calling convention.

Assumption (checked, not propagated): every call reads no scratch register
and clobbers AX BX CX DX ES; every return delivers AX/DX and preserves
SI DI BP DS.  Runtime helpers with register conventions use their documented
behaviour.  Functions that still read scratch registers on entry, and callers
that read BX/CX/ES after a call, are reported as exceptions.
"""
import collections

# byte halves are tracked separately so that 'mov cl, 4' does not keep CH alive
HALVES = ['al', 'ah', 'bl', 'bh', 'cl', 'ch', 'dl', 'dh']
OTHER = ['si', 'di', 'bp', 'es', 'ds']
BIT = {r: 1 << n for n, r in enumerate(HALVES + OTHER)}
for _r in 'abcd':
    BIT[_r + 'x'] = BIT[_r + 'l'] | BIT[_r + 'h']
REGS = ['ax', 'bx', 'cx', 'dx', 'si', 'di', 'bp', 'es', 'ds']
ALL = (1 << (len(HALVES) + len(OTHER))) - 1
SUB = {}
CLOBBER = BIT['ax'] | BIT['bx'] | BIT['cx'] | BIT['dx'] | BIT['es']
PRESERVED = BIT['si'] | BIT['di'] | BIT['bp'] | BIT['ds']

HELPERS = {
    # (registers read, registers written) -- write sets read off the helper code in CL.LIB
    'LXMUL@': (BIT['ax'] | BIT['bx'] | BIT['cx'] | BIT['dx'], BIT['ax'] | BIT['cx'] | BIT['dx']),   # DX:AX *= CX:BX
    'LXLSH@': (BIT['ax'] | BIT['cl'] | BIT['dx'], BIT['ax'] | BIT['bx'] | BIT['cx'] | BIT['dx']),  # DX:AX <<= CL
    'LXRSH@': (BIT['ax'] | BIT['cl'] | BIT['dx'], BIT['ax'] | BIT['bx'] | BIT['cx'] | BIT['dx']),
    'LXURSH@': (BIT['ax'] | BIT['cl'] | BIT['dx'], BIT['ax'] | BIT['bx'] | BIT['cx'] | BIT['dx']),
    'LLSH@': (BIT['ax'] | BIT['cx'] | BIT['dx'], BIT['ax'] | BIT['cx'] | BIT['dx']),
    'PADA@': (BIT['ax'] | BIT['bx'] | BIT['cx'] | BIT['dx'], CLOBBER),
    'PADD@': (BIT['ax'] | BIT['bx'] | BIT['cx'] | BIT['dx'], BIT['ax'] | BIT['bx'] | BIT['cx'] | BIT['dx']),
    'PSUB@': (BIT['ax'] | BIT['bx'] | BIT['cx'] | BIT['dx'], BIT['ax'] | BIT['bx'] | BIT['cx'] | BIT['dx']),
    'PCMP@': (BIT['ax'] | BIT['bx'] | BIT['cx'] | BIT['dx'], BIT['ax'] | BIT['bx'] | BIT['cx'] | BIT['dx']),
    'SCOPY@': (BIT['cx'], BIT['cx'] | BIT['es']),                          # si, di, ds saved
    'LDIV@': (0, CLOBBER), 'LUDIV@': (0, CLOBBER), 'LMOD@': (0, CLOBBER), 'LUMOD@': (0, CLOBBER),
}


def rbit(name):
    return BIT.get(SUB.get(name, name), 0)


def opnd_uses(o):
    if o is None:
        return 0
    if o.kind in ('reg', 'sreg'):
        return rbit(o.name)
    if o.kind == 'mem':
        m = BIT[o.seg] if o.seg in BIT else 0
        for b in o.base:
            m |= BIT[b]
        return m
    return 0


def addr_uses(o):
    return opnd_uses(o) if (o is not None and o.kind == 'mem') else 0


def dst_def(o):
    return rbit(o.name) if (o is not None and o.kind in ('reg', 'sreg')) else 0


def partial(o):
    return False


def names(mask):
    out = []
    for r in REGS:
        b = BIT[r]
        if mask & b == b:
            out.append(r)
        elif mask & b:
            out.append(r[0] + ('l' if mask & BIT[r[0] + 'l'] else 'h'))
    return out


# PALLADIX sound-driver stubs (371E) jump into the driver loaded from
# SOURCE1.RSC.  These entries take their arguments on the stack only:
# verified by a liveness analysis of the loaded driver code (no register is
# live at their entry), see docs/STATE.md.
DRIVER_STACK_ONLY = {(0x371E, o): 0 for o in (0x0088, 0x008D, 0x0092, 0x0097, 0x00A6, 0x00AB)}

# INT 33h (mouse driver) by function: (registers read, registers written)
MOUSE_FUNCS = {
    0x0000: (BIT['ax'], BIT['ax'] | BIT['bx']),
    0x0001: (BIT['ax'], 0), 0x0002: (BIT['ax'], 0),
    0x0003: (BIT['ax'], BIT['bx'] | BIT['cx'] | BIT['dx']),
    0x0004: (BIT['ax'] | BIT['cx'] | BIT['dx'], 0),
    0x0005: (BIT['ax'] | BIT['bx'], BIT['ax'] | BIT['bx'] | BIT['cx'] | BIT['dx']),
    0x0006: (BIT['ax'] | BIT['bx'], BIT['ax'] | BIT['bx'] | BIT['cx'] | BIT['dx']),
    0x0007: (BIT['ax'] | BIT['cx'] | BIT['dx'], 0), 0x0008: (BIT['ax'] | BIT['cx'] | BIT['dx'], 0),
    0x000B: (BIT['ax'], BIT['cx'] | BIT['dx']),
    0x000F: (BIT['ax'] | BIT['cx'] | BIT['dx'], 0),
}
# BIOS video (int 10h), by AH (or AX for function 10h): registers read, written
VIDEO_FUNCS = {
    0x00: (BIT['ax'], BIT['ax']),                                   # set mode
    0x0F: (BIT['ax'], BIT['ax'] | BIT['bx']),                       # get mode: AL mode, AH columns, BH page
    0x1012: (BIT['ax'] | BIT['bx'] | BIT['cx'] | BIT['dx'] | BIT['es'], 0),   # set DAC block
    0x1017: (BIT['ax'] | BIT['bx'] | BIT['cx'] | BIT['dx'] | BIT['es'], 0),   # read DAC block
}


class RegFlow:
    def __init__(self, lifter, symnames=None):
        self.L = lifter
        self.names = symnames or {}
        self.helper = {}
        for k in lifter.funcs:
            nm = self.names.get(k[0] * 16 + k[1], '')
            if nm in HELPERS:
                self.helper[k] = HELPERS[nm]
        self.reg_in = {}
        self.live_out = {}
        self.ret_use = {}               # per-function registers a return delivers
        self.mayw = {}                  # scratch registers a call to F may change
        self.entry_saves = {}
        for k, f in lifter.funcs.items():
            saves = set()
            o = f.off
            while o in f.insns:
                i = f.insns[o]
                if i.op == 'push' and i.ops[0].kind in ('reg', 'sreg') and i.ops[0].name != 'cs':
                    saves.add(o)
                elif i.op == 'mov' and i.ops[0].kind == 'reg' and i.ops[0].name == 'bp' and i.ops[1].kind == 'reg' and i.ops[1].name == 'sp':
                    pass
                elif i.op == 'sub' and i.ops[0].kind == 'reg' and i.ops[0].name == 'sp':
                    pass
                else:
                    break
                o = i.next
            self.entry_saves[k] = saves
        self.passthru = {}
        self.genuine_in = {}
        self.compute_mayw()
        self.compute_passthru()
        self.compute_genuine_in()
        self.dx_used = collections.defaultdict(bool)   # some caller reads DX after the call
        self.exceptions = []                            # (func, scratch regs read on entry)
        self.violations = []                            # (func, insn off, callee, regs)

    def compute_mayw(self):
        """least fixpoint of scratch registers each function can modify,
        including through its callees"""
        for k in self.L.funcs:
            self.mayw[k] = 0
        for k in self.helper:
            self.mayw[k] = self.helper[k][1] & CLOBBER
        changed = True
        while changed:
            changed = False
            for k, f in self.L.funcs.items():
                if k in self.helper:
                    continue
                m = 0
                for i in f.insns.values():
                    if i.op in ('call', 'callf', 'jmpf'):
                        t = self.L.call_target(i)
                        m |= self.mayw.get(t, CLOBBER)
                    elif i.op in ('calli', 'callfi', 'jmpfi', 'int') or (i.op == 'jmpi' and i.off not in f.jumptables):
                        m |= CLOBBER
                    else:
                        u, d = self.use_def(f, i)
                        m |= d
                m &= CLOBBER
                if m != self.mayw[k]:
                    self.mayw[k] = m
                    changed = True

    def compute_passthru(self):
        """least fixpoint: scratch registers that reach some return of the
        function without being written (they may come back unchanged)"""
        for k in self.L.funcs:
            self.passthru[k] = 0
        changed = True
        while changed:
            changed = False
            for k, f in self.L.funcs.items():
                if k in self.helper:
                    continue
                offs = sorted(f.insns)
                lin = {o: 0 for o in offs}
                again = True
                while again:
                    again = False
                    for o in reversed(offs):
                        i = f.insns[o]
                        if i.op in ('ret', 'retf', 'iret'):
                            inn = CLOBBER
                        else:
                            out = 0
                            for s_ in self.L.succs(f, o):
                                out |= lin.get(s_, 0)
                            if i.op in ('call', 'callf'):
                                t = self.L.call_target(i)
                                kill = (self.helper[t][1] if t in self.helper else
                                        self.mayw.get(t, CLOBBER) & ~self.passthru.get(t, 0))
                            elif i.op in ('calli', 'callfi', 'int', 'jmpf', 'jmpfi') or \
                                    (i.op == 'jmpi' and i.off not in f.jumptables):
                                kill = CLOBBER
                            else:
                                kill = self.use_def(f, i)[1]
                            inn = out & ~kill
                        if inn != lin[o]:
                            lin[o] = inn
                            again = True
                p = lin[f.off] & CLOBBER
                if p != self.passthru[k]:
                    self.passthru[k] = p
                    changed = True

    def compute_genuine_in(self):
        """least fixpoint: registers each function reads at entry for its own
        computation (liveness with its return delivering nothing)"""
        for k in self.L.funcs:
            self.genuine_in[k] = 0
        for it in range(20):
            changed = False
            for k, f in self.L.funcs.items():
                if k in self.helper:
                    self.genuine_in[k] = self.helper[k][0]
                    continue
                saved = self.ret_use.get(k)
                self.ret_use[k] = 0
                try:
                    lin, _ = self.liveness(f)
                finally:
                    if saved is None:
                        del self.ret_use[k]
                    else:
                        self.ret_use[k] = saved
                g = lin[f.off] & CLOBBER
                if g != self.genuine_in[k]:
                    self.genuine_in[k] = g
                    changed = True
            if not changed:
                break

    def use_def(self, f, i):
        op, ops = i.op, i.ops
        u = d = 0
        if op == 'mov':
            dst, src = ops
            u = opnd_uses(src) | addr_uses(dst)
            d = dst_def(dst)
            if partial(dst):
                u |= d
        elif op in ('add', 'sub', 'and', 'or', 'xor', 'adc', 'sbb'):
            dst, src = ops
            if op in ('xor', 'sub') and src.kind == 'reg' and dst.kind == 'reg' and src.name == dst.name:
                d = dst_def(dst)
                if partial(dst):
                    u |= d
            else:
                u = opnd_uses(dst) | opnd_uses(src)
                d = dst_def(dst)
        elif op in ('cmp', 'test'):
            u = opnd_uses(ops[0]) | opnd_uses(ops[1])
        elif op in ('inc', 'dec', 'neg', 'not'):
            u = opnd_uses(ops[0]); d = dst_def(ops[0])
        elif op in ('rol', 'ror', 'rcl', 'rcr', 'shl', 'shr', 'sal', 'sar'):
            u = opnd_uses(ops[0]) | opnd_uses(ops[1]); d = dst_def(ops[0])
        elif op in ('mul', 'imul', 'div', 'idiv'):
            u = opnd_uses(ops[0]) | (BIT['ax'] if (i.w == 16 or op in ('div', 'idiv')) else BIT['al'])
            if i.w == 16:
                if op in ('div', 'idiv'):
                    u |= BIT['dx']
                d = BIT['ax'] | BIT['dx']
            else:
                d = BIT['ax']
        elif op == 'imul3':
            u = opnd_uses(ops[1]); d = dst_def(ops[0])
        elif op == 'xchg':
            u = opnd_uses(ops[0]) | opnd_uses(ops[1]); d = dst_def(ops[0]) | dst_def(ops[1])
        elif op == 'lea':
            u = addr_uses(ops[1]); d = dst_def(ops[0])
        elif op in ('les', 'lds'):
            u = addr_uses(ops[1]); d = dst_def(ops[0]) | BIT['es' if op == 'les' else 'ds']
        elif op == 'push':
            # register saves at function entry are not real inputs
            u = 0 if i.off in self.entry_saves.get((f.seg, f.off), ()) else opnd_uses(ops[0])
        elif op == 'pop':
            d = dst_def(ops[0]); u = addr_uses(ops[0])
        elif op == 'cbw':
            u = BIT['al']; d = BIT['ax']
        elif op == 'cwd':
            u = BIT['ax']; d = BIT['dx']
        elif op == 'lahf':
            d = BIT['ah']
        elif op == 'sahf':
            u = BIT['ah']
        elif op in ('movs', 'cmps', 'stos', 'lods', 'scas'):
            acc = 'ax' if i.w == 16 else 'al'
            m = {'movs': ('si', 'di', 'ds', 'es'), 'cmps': ('si', 'di', 'ds', 'es'),
                 'stos': ('di', 'es', acc), 'lods': ('si', 'ds'), 'scas': ('di', 'es', acc)}[op]
            for r in m:
                u |= BIT[r]
            if i.segovr in BIT:
                u |= BIT[i.segovr]
            d = (BIT['si'] if 'si' in m else 0) | (BIT['di'] if 'di' in m else 0)
            if op == 'lods':
                d |= BIT['ax'] if i.w == 16 else BIT['al']
            if i.rep:
                u |= BIT['cx']; d |= BIT['cx']
        elif op == 'in':
            d = BIT['ax'] if i.w == 16 else BIT['al']; u = opnd_uses(ops[0])
        elif op == 'out':
            u = (BIT['ax'] if i.w == 16 else BIT['al']) | opnd_uses(ops[0])
        elif op == 'xlat':
            u = BIT['al'] | BIT['bx'] | BIT['ds']; d = BIT['al']
        elif op in ('loop', 'loope', 'loopne', 'jcxz'):
            u = BIT['cx']; d = BIT['cx'] if op != 'jcxz' else 0
        elif op in ('call', 'callf'):
            key = self.L.call_target(i)
            if key in self.helper:
                u, d = self.helper[key]
            else:
                # AX the callee really reads (not just hands back) is used here
                u = BIT['ds'] | (self.genuine_in.get(key, 0) & BIT['ax'])
                d = self.mayw.get(key, CLOBBER) if self.mayw else CLOBBER
                # a register the callee may hand back unchanged is not killed
                d &= ~self.passthru.get(key, 0)
        elif op in ('calli', 'callfi'):
            u = BIT['ds'] | addr_uses(ops[0]) | opnd_uses(ops[0]); d = CLOBBER
        elif op == 'int':
            u = BIT['ax'] | BIT['bx'] | BIT['cx'] | BIT['dx'] | BIT['ds'] | BIT['es']
            d = BIT['ax'] | BIT['bx'] | BIT['cx'] | BIT['dx'] | BIT['es']
            fn = self.int_function(f, i)
            if ops[0].val == 0x16 and fn is not None and (fn >> 8) in (0x00, 0x01, 0x02, 0x10, 0x11, 0x12):
                u, d = BIT['ax'], BIT['ax']            # BIOS keyboard: AH in, AX out
            elif ops[0].val == 0x33 and fn is not None and fn in MOUSE_FUNCS:
                u, d = MOUSE_FUNCS[fn]
            elif ops[0].val == 0x10 and fn is not None and (fn if fn >> 8 == 0x10 else fn >> 8) in VIDEO_FUNCS:
                u, d = VIDEO_FUNCS[fn if fn >> 8 == 0x10 else fn >> 8]
        elif op in ('ret', 'retf', 'iret'):
            u = self.ret_use.get((f.seg, f.off), BIT['ax'] | BIT['dx']) | PRESERVED
        elif op == 'jmpi' and i.off in f.jumptables:
            u = opnd_uses(ops[0])
        elif op in ('jmpf', 'jmpfi', 'jmpi'):
            u = DRIVER_STACK_ONLY.get((f.seg, f.off), BIT['ax'] | BIT['dx']) | PRESERVED | opnd_uses(ops[0])
        elif op in ('daa', 'das', 'aaa', 'aas', 'aam', 'aad', 'salc'):
            u = d = BIT['ax']
        elif op == 'pusha':
            u = ALL
        elif op == 'popa':
            d = ALL & ~BIT['es'] & ~BIT['ds']
        return u, d

    def int_function(self, f, i):
        """AX (or AH << 8) set by a 'mov ax/ah, imm' (or xor ax, ax) earlier in the same straight-line run"""
        prev = {j.next: j for j in f.insns.values()}
        o = i.off
        for _ in range(8):
            j = prev.get(o)
            if j is None or j.op in ('jmp', 'jcc', 'call', 'callf', 'ret', 'retf', 'int'):
                return None
            if j.op == 'mov' and j.ops[0].kind == 'reg' and j.ops[1].kind == 'imm':
                if j.ops[0].name == 'ax':
                    return j.ops[1].val
                if j.ops[0].name == 'ah':
                    return j.ops[1].val << 8
            if j.op in ('xor', 'sub') and j.ops[0].kind == 'reg' and j.ops[0].name == 'ax' and \
                    j.ops[1].kind == 'reg' and j.ops[1].name == 'ax':
                return 0
            if any(op_.kind == 'reg' and op_.name in ('ax', 'ah', 'al') for op_ in j.ops[:1]):
                return None
            o = j.off
        return None

    def liveness(self, f):
        offs = sorted(f.insns)
        ud = {o: self.use_def(f, f.insns[o]) for o in offs}
        lin = {o: 0 for o in offs}
        lout = {o: 0 for o in offs}
        changed = True
        while changed:
            changed = False
            for o in reversed(offs):
                out = 0
                for s in self.L.succs(f, o):
                    out |= lin.get(s, 0)
                u, d = ud[o]
                inn = u | (out & ~d)
                if inn != lin[o] or out != lout[o]:
                    lin[o], lout[o] = inn, out
                    changed = True
        return lin, lout

    def run(self):
        for k, f in self.L.funcs.items():
            lin, lout = self.liveness(f)
            self.live_out[k] = lout
            self.reg_in[k] = lin[f.off]
            if lin[f.off] & CLOBBER and k not in self.helper:
                self.exceptions.append((k, names(lin[f.off] & CLOBBER)))
            for o, i in f.insns.items():
                if i.op in ('call', 'callf'):
                    t = self.L.call_target(i)
                    if lout[o] & BIT['dx']:
                        self.dx_used[t] = True
                    bad = lout[o] & (BIT['bx'] | BIT['cx'] | BIT['es'])
                    if bad and t not in self.helper:
                        self.violations.append((k, o, t, names(bad)))
        return self
