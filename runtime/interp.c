/* 8086/80186 real-mode interpreter.
 *
 * Used for code that only exists at runtime (sound drivers loaded from the
 * RSC file), as the fallback for any address without a recompiled function,
 * and as the reference engine for differential testing of recompiled code.
 *
 * interp_call() runs from R.cs:R.ip until the stack pointer rises above the
 * value it had on entry, i.e. until the caller's return frame is popped.
 */
#include <stdio.h>
#include <stdlib.h>
#include "cpu.h"
#include "runtime.h"

static u16 seg_ovr;     /* segment override for current insn, or 0xFFFF */
static int seg_ovr_set;
static u8 modrm;
static u16 ea_seg, ea_off;
static int ea_is_reg;

static inline u8 fetch8(void) { u8 v = rb(R.cs, R.ip); R.ip++; return v; }
static inline u16 fetch16(void) { u16 v = rw(R.cs, R.ip); R.ip += 2; return v; }

static u16 *reg16_tab[8];
static u8 *reg8_tab[8];
static u16 *sreg_tab[4];

static void init_tabs(void) {
    static int done;
    if (done) return;
    done = 1;
    reg16_tab[0] = &R.ax; reg16_tab[1] = &R.cx; reg16_tab[2] = &R.dx; reg16_tab[3] = &R.bx;
    reg16_tab[4] = &R.sp; reg16_tab[5] = &R.bp; reg16_tab[6] = &R.si; reg16_tab[7] = &R.di;
    reg8_tab[0] = &R.al; reg8_tab[1] = &R.cl; reg8_tab[2] = &R.dl; reg8_tab[3] = &R.bl;
    reg8_tab[4] = &R.ah; reg8_tab[5] = &R.ch; reg8_tab[6] = &R.dh; reg8_tab[7] = &R.bh;
    sreg_tab[0] = &R.es; sreg_tab[1] = &R.cs; sreg_tab[2] = &R.ss; sreg_tab[3] = &R.ds;
}

static void decode_modrm(void) {
    modrm = fetch8();
    int mod = modrm >> 6, rm = modrm & 7;
    if (mod == 3) { ea_is_reg = 1; return; }
    ea_is_reg = 0;
    u16 off = 0, seg = R.ds;
    switch (rm) {
    case 0: off = R.bx + R.si; break;
    case 1: off = R.bx + R.di; break;
    case 2: off = R.bp + R.si; seg = R.ss; break;
    case 3: off = R.bp + R.di; seg = R.ss; break;
    case 4: off = R.si; break;
    case 5: off = R.di; break;
    case 6: if (mod == 0) off = fetch16(); else { off = R.bp; seg = R.ss; } break;
    case 7: off = R.bx; break;
    }
    if (mod == 1) off += (s8)fetch8();
    else if (mod == 2) off += fetch16();
    ea_seg = seg_ovr_set ? seg_ovr : seg;
    ea_off = off;
}

#define REGF ((modrm >> 3) & 7)
static inline u8 get_rm8(void) { return ea_is_reg ? *reg8_tab[modrm & 7] : rb(ea_seg, ea_off); }
static inline u16 get_rm16(void) { return ea_is_reg ? *reg16_tab[modrm & 7] : rw(ea_seg, ea_off); }
static inline void set_rm8(u8 v) { if (ea_is_reg) *reg8_tab[modrm & 7] = v; else wb(ea_seg, ea_off, v); }
static inline void set_rm16(u16 v) { if (ea_is_reg) *reg16_tab[modrm & 7] = v; else ww(ea_seg, ea_off, v); }

static u8 alu8(int op, u8 a, u8 b) {
    switch (op) {
    case 0: return add8(a, b, 0);
    case 1: return logic8(a | b);
    case 2: return add8(a, b, R.cf);
    case 3: return sub8(a, b, R.cf);
    case 4: return logic8(a & b);
    case 5: return sub8(a, b, 0);
    case 6: return logic8(a ^ b);
    default: sub8(a, b, 0); return a;   /* cmp */
    }
}
static u16 alu16(int op, u16 a, u16 b) {
    switch (op) {
    case 0: return add16(a, b, 0);
    case 1: return logic16(a | b);
    case 2: return add16(a, b, R.cf);
    case 3: return sub16(a, b, R.cf);
    case 4: return logic16(a & b);
    case 5: return sub16(a, b, 0);
    case 6: return logic16(a ^ b);
    default: sub16(a, b, 0); return a;
    }
}

static int cond(int cc) {
    switch (cc) {
    case 0x0: return R.of;
    case 0x1: return !R.of;
    case 0x2: return R.cf;
    case 0x3: return !R.cf;
    case 0x4: return R.zf;
    case 0x5: return !R.zf;
    case 0x6: return R.cf || R.zf;
    case 0x7: return !R.cf && !R.zf;
    case 0x8: return R.sf;
    case 0x9: return !R.sf;
    case 0xA: return R.pf;
    case 0xB: return !R.pf;
    case 0xC: return R.sf != R.of;
    case 0xD: return R.sf == R.of;
    case 0xE: return R.zf || (R.sf != R.of);
    default:  return !R.zf && (R.sf == R.of);
    }
}

u64 interp_insns;

/* Transfer to a far/near target from inside the interpreter.  If a recompiled
 * function exists there, run it natively; the return address is already on
 * the stack and the native code pops it, after which we resume at ret_ip. */
static int try_native(u16 seg, u16 off, u16 ret_cs, u16 ret_ip, int far) {
    if (difftest_on && !difftest_active) {
        native_fn g = native_lookup_any(seg, off);
        if (g) difftest_call(seg, off, g, far);     /* tests, rewinds: the call runs interpreted */
        clock_entry((u16)(seg - LOADSEG), off);     /* (native code counts in its prologue) */
        return 0;
    }
    native_fn f = native_lookup(seg, off);
    if (!f) { clock_entry((u16)(seg - LOADSEG), off); return 0; }
    u16 saved_cs = R.cs;
    R.cs = seg;
    int dec = native_decompiled(seg, off);
    if (dec) state_reload();         /* decompiled code keeps game state in host memory (src/state.c) */
    f();
    if (dec) state_export();
    (void)saved_cs;
    R.cs = ret_cs;
    R.ip = ret_ip;
    return 1;
}

static void string_op(u8 op, int rep) {
    string_op_ex(op, rep, seg_ovr_set ? seg_ovr : R.ds);
}

/* Software interrupt raised by interpreted code. */
extern int bios_trace;
static void interp_int(u8 n) {
    u16 vo = rw(0, n * 4), vs = rw(0, n * 4 + 2);
    if (bios_is_stub(vs, vo)) { bios_dispatch(n); if (bios_trace && n == 0x16) fprintf(stderr, "  after int16: zf=%d ax=%04X next=%04X:%04X\n", R.zf, R.ax, R.cs, R.ip); return; }
    push16(get_flags()); push16(R.cs); push16(R.ip);
    R.ifl = 0; R.tf = 0;
    R.cs = vs; R.ip = vo;
}

/* Run until the frame present at entry has been popped (SP rises above the
 * entry SP).  Returns normally; the caller restores whatever it needs. */
int pic_ack(void);

/* Hardware interrupt taken while interpreting: build a real frame so the
 * handler's IRET resumes the interrupted instruction stream. */
static void interp_irq(void) {
    int v = pic_ack();
    if (v < 0) return;
    u16 vo = rw(0, v * 4), vs = rw(0, v * 4 + 2);
    if (bios_is_stub(vs, vo)) { bios_dispatch((u8)vo); return; }
    push16(get_flags()); push16(R.cs); push16(R.ip);
    R.ifl = 0; R.tf = 0;
    native_fn f = native_lookup(vs, vo);
    if (f) {   /* recompiled ISR: runs to its IRET, which pops our frame */
        u16 cs = R.cs, ip = R.ip;
        R.cs = vs; f(); R.cs = cs; R.ip = ip;
        (void)cs;
        return;
    }
    R.cs = vs; R.ip = vo;
}

u64 difftest_budget;
static int last_ret;
static int after_sti;

/* Interrupts and timed events are only taken where recompiled code polls
 * (function entries, loop heads, after STI) while executing the EXE image, so
 * both engines see exactly the same interleaving.  Code outside the image
 * (runtime-loaded drivers, BIOS stubs) is always interpreted and may poll at
 * every instruction. */
extern const u32 poll_points[];
extern const int poll_count;
static u8 *poll_map;
static u32 image_lo, image_hi;
static int may_poll(void) {
    if (after_sti) { after_sti = 0; return 1; }
    u32 a = lin(R.cs, R.ip);
    if (a < image_lo || a >= image_hi || !poll_count) return 1;
    u32 r = a - image_lo;
    return (poll_map[r >> 3] >> (r & 7)) & 1;
}
void interp_init_polls(u32 image_bytes) {
    image_lo = (u32)LOADSEG << 4;
    image_hi = image_lo + image_bytes;
    poll_map = calloc(1, image_bytes / 8 + 1);
    for (int i = 0; i < poll_count; i++) if (poll_points[i] < image_bytes) poll_map[poll_points[i] >> 3] |= 1 << (poll_points[i] & 7);
}

/* Run until the caller's frame is gone: either a RET-type instruction popped
 * past the entry SP, or some other jump landed exactly on the caller's return
 * address (e.g. Turbo C's _setargv pops its return address, moves SP and
 * jumps back).  `far` tells whether the frame holds CS:IP or just IP. */
static void interp_loop(int run_forever, int far) {
    if (difftest_active && !difftest_budget) difftest_budget = interp_insns + 30000000ull;
    init_tabs();
    u16 entry_sp = R.sp;
    u16 entry_ss = R.ss;
    u16 ra_ip = rw(R.ss, R.sp);
    u16 ra_cs = far ? rw(R.ss, R.sp + 2) : R.cs;
    last_ret = 0;
    int started = 0;
    for (;;) {
        if (!run_forever && started && R.ss == entry_ss) {
            if (last_ret) {
                if (R.sp > entry_sp && (s16)(R.sp - entry_sp) > 0) return;
            } else if (R.ip == ra_ip && R.cs == ra_cs) {
                return;
            }
        }
        started = 1;
        if (difftest_active) {
            if (interp_insns > difftest_budget) difftest_abort("budget");
        } else if (may_poll()) {
            LOGICAL_SYNC();
            if (T >= next_event_T) runtime_events();
            if (irq_pending && R.ifl && !seat_frozen) { interp_irq(); continue; }
        }
        if (R.cs == BIOS_SEG) {   /* executing a BIOS/DOS stub: native service + IRET */
            bios_stub_iret((u8)R.ip);
            last_ret = 1;
            continue;
        }
        interp_step();
    }
}

/* --stackchk (and --difftest): reads of local variables that were never
 * written.  `sub sp, N` marks the new frame dead, every write by the runtime
 * revives a byte (cpu.h, runtime objects are built with -DSTACKCHK), reading
 * a dead byte is reported.  The value such a read returns is whatever an
 * earlier call left on the stack: an original bug the decompiled build cannot
 * reproduce, because its stack layout differs. */
int stackchk_on;
u8 stk_dead[MEM_SIZE];
static u8 stk_seen[MEM_SIZE];
static u16 stk_cs, stk_ip;
int stackchk_hits;                  /* counted instead of printed during a difftest */
u16 stackchk_hit_cs, stackchk_hit_ip;

/* --stackchk (not the difftest) also notes the last writer of every byte --
 * the instruction and the far return address of the BP frame it ran in -- and
 * the report names it: where the garbage comes from.  For a helper without a
 * BP frame that is its caller's frame.  STACKCHK_ALL=1 reports every read,
 * not only the first one of each instruction. */
int stk_who_on;
static u32 stk_who[MEM_SIZE], stk_who_ret[MEM_SIZE];
static u16 raw16(u16 s, u16 o) { return M[lin(s, o)] | M[lin(s, (u16)(o + 1))] << 8; }   /* no SCHK_R */
void stackchk_wrote(u32 a) {
    stk_who[a] = ((u32)(u16)(stk_cs - LOADSEG) << 16) | stk_ip;
    stk_who_ret[a] = ((u32)(u16)(raw16(R.ss, (u16)(R.bp + 4)) - LOADSEG) << 16) | raw16(R.ss, (u16)(R.bp + 2));
}

void stackchk_read(u32 a) {
    stk_dead[a] = 0;                /* one report per byte and frame */
    if (difftest_on) {
        if (difftest_active && !stackchk_hits++) { stackchk_hit_cs = stk_cs; stackchk_hit_ip = stk_ip; }
        return;
    }
    static int all = -1;
    if (all < 0) all = getenv("STACKCHK_ALL") != NULL;
    u32 at = lin(stk_cs, stk_ip);
    if (stk_seen[at] && !all) return;
    stk_seen[at] = 1;
    u32 bp = lin(R.ss, R.bp);
    fprintf(stderr, "[uninit] %04X:%04X reads %s%04X (linear %05X, T %llu, frame %llu, called from %04X:%04X): "
            "%02X, last written by %04X:%04X (frame returning to %04X:%04X)\n", (u16)(stk_cs - LOADSEG), stk_ip,
            a >= bp ? "bp+" : "bp-", (unsigned)(a >= bp ? a - bp : bp - a), a, (unsigned long long)T,
            (unsigned long long)frame_no(), (u16)(raw16(R.ss, (u16)(R.bp + 4)) - LOADSEG), raw16(R.ss, (u16)(R.bp + 2)),
            M[a], stk_who[a] >> 16, stk_who[a] & 0xFFFF, stk_who_ret[a] >> 16, stk_who_ret[a] & 0xFFFF);
}

/* Interrupt frames and pushed flags are cleared once popped.  Recompiled code
 * tracks neither its IP nor its flags, so an interrupt taken there pushes
 * other values than the same interrupt taken in the interpreter; dead, those
 * words would still differ, and the original reads never-written locals
 * (see --stackchk) -- the two engines would drift apart after some game days.
 * All IRET/POPF run here (driver code, BIOS stubs), in both engines. */
void scrub_popped(int n) {
    for (int i = 1; i <= n; i++) wb(R.ss, (u16)(R.sp - i), 0);
}

u64 trace_from = ~0ull, trace_to;
u32 watch_entry_lin;
void interp_step(void) {
    u16 start_ip = R.ip;
    stk_cs = R.cs; stk_ip = R.ip;
    if (watch_entry_lin && lin(R.cs, R.ip) == watch_entry_lin)
        fprintf(stderr, "[entry interp] AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X DF=%d SP=%04X ret %04X:%04X args %04X %04X %04X %04X\n",
                R.ax, R.bx, R.cx, R.dx, R.si, R.di, R.ds, R.es, R.df, R.sp, (u16)(rw(R.ss, R.sp + 2) - LOADSEG), rw(R.ss, R.sp), rw(R.ss, R.sp + 4), rw(R.ss, R.sp + 6), rw(R.ss, R.sp + 8), rw(R.ss, R.sp + 10)),
        fprintf(stderr, "    header %04X %04X %04X %04X\n", rw(rw(R.ss, R.sp + 6), rw(R.ss, R.sp + 4)), rw(rw(R.ss, R.sp + 6), rw(R.ss, R.sp + 4) + 2), rw(rw(R.ss, R.sp + 6), rw(R.ss, R.sp + 4) + 4), rw(rw(R.ss, R.sp + 6), rw(R.ss, R.sp + 4) + 6));
    if (T >= trace_from && T <= trace_to)
        fprintf(stderr, "T%llu %04X:%04X (rel %04X) %02X %02X %02X %02X  AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X SP=%04X BP=%04X DS=%04X ES=%04X Z%d C%d\n",
                (unsigned long long)T, R.cs, R.ip, (u16)(R.cs - LOADSEG), rb(R.cs, R.ip), rb(R.cs, R.ip + 1), rb(R.cs, R.ip + 2), rb(R.cs, R.ip + 3),
                R.ax, R.bx, R.cx, R.dx, R.si, R.di, R.sp, R.bp, R.ds, R.es, R.zf, R.cf);
    int rep = 0;
    seg_ovr_set = 0;
    T += 1;
    interp_insns++;
    u8 op;
    for (;;) {
        op = fetch8();
        switch (op) {
        case 0x26: seg_ovr = R.es; seg_ovr_set = 1; continue;
        case 0x2E: seg_ovr = R.cs; seg_ovr_set = 1; continue;
        case 0x36: seg_ovr = R.ss; seg_ovr_set = 1; continue;
        case 0x3E: seg_ovr = R.ds; seg_ovr_set = 1; continue;
        case 0xF0: continue;                         /* lock */
        case 0xF2: case 0xF3: rep = op; continue;
        }
        break;
    }
    u16 a, b;
    switch (op) {
    /* ALU r/m, reg forms: 00-3F except prefixes/push/pop/daa... */
    case 0x00: case 0x08: case 0x10: case 0x18: case 0x20: case 0x28: case 0x30: case 0x38:
        decode_modrm(); { u8 r = alu8(op >> 3, get_rm8(), *reg8_tab[REGF]); if ((op >> 3) != 7) set_rm8(r); } break;
    case 0x01: case 0x09: case 0x11: case 0x19: case 0x21: case 0x29: case 0x31: case 0x39:
        decode_modrm(); { u16 r = alu16(op >> 3, get_rm16(), *reg16_tab[REGF]); if ((op >> 3) != 7) set_rm16(r); } break;
    case 0x02: case 0x0A: case 0x12: case 0x1A: case 0x22: case 0x2A: case 0x32: case 0x3A:
        decode_modrm(); { u8 r = alu8(op >> 3, *reg8_tab[REGF], get_rm8()); if ((op >> 3) != 7) *reg8_tab[REGF] = r; } break;
    case 0x03: case 0x0B: case 0x13: case 0x1B: case 0x23: case 0x2B: case 0x33: case 0x3B:
        decode_modrm(); { u16 r = alu16(op >> 3, *reg16_tab[REGF], get_rm16()); if ((op >> 3) != 7) *reg16_tab[REGF] = r; } break;
    case 0x04: case 0x0C: case 0x14: case 0x1C: case 0x24: case 0x2C: case 0x34: case 0x3C:
        { u8 r = alu8(op >> 3, R.al, fetch8()); if ((op >> 3) != 7) R.al = r; } break;
    case 0x05: case 0x0D: case 0x15: case 0x1D: case 0x25: case 0x2D: case 0x35: case 0x3D:
        { u16 r = alu16(op >> 3, R.ax, fetch16()); if ((op >> 3) != 7) R.ax = r; } break;
    case 0x06: push16(R.es); break;
    case 0x07: R.es = pop16(); break;
    case 0x0E: push16(R.cs); break;
    case 0x0F: R.cs = pop16(); break;   /* 8086 POP CS */
    case 0x16: push16(R.ss); break;
    case 0x17: R.ss = pop16(); break;
    case 0x1E: push16(R.ds); break;
    case 0x1F: R.ds = pop16(); break;
    case 0x27: daa(); break;
    case 0x2F: das(); break;
    case 0x37: aaa(); break;
    case 0x3F: aas(); break;
    case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47:
        *reg16_tab[op & 7] = inc16(*reg16_tab[op & 7]); break;
    case 0x48: case 0x49: case 0x4A: case 0x4B: case 0x4C: case 0x4D: case 0x4E: case 0x4F:
        *reg16_tab[op & 7] = dec16(*reg16_tab[op & 7]); break;
    case 0x50: case 0x51: case 0x52: case 0x53: case 0x55: case 0x56: case 0x57:
        push16(*reg16_tab[op & 7]); break;
    case 0x54: { u16 v = R.sp - 2; push16(v); } break;  /* 286+ semantics; 8086 pushes new SP */
    case 0x58: case 0x59: case 0x5A: case 0x5B: case 0x5C: case 0x5D: case 0x5E: case 0x5F:
        *reg16_tab[op & 7] = pop16(); break;
    case 0x60: { u16 sp = R.sp; push16(R.ax); push16(R.cx); push16(R.dx); push16(R.bx);
                 push16(sp); push16(R.bp); push16(R.si); push16(R.di); } break;
    case 0x61: R.di = pop16(); R.si = pop16(); R.bp = pop16(); pop16(); R.bx = pop16();
               R.dx = pop16(); R.cx = pop16(); R.ax = pop16(); break;
    case 0x62: decode_modrm(); break;  /* bound: ignore */
    case 0x68: push16(fetch16()); break;
    case 0x69: decode_modrm(); { s32 r = (s32)(s16)get_rm16() * (s16)fetch16();
               *reg16_tab[REGF] = (u16)r; R.cf = R.of = r != (s16)r; } break;
    case 0x6A: push16((u16)(s8)fetch8()); break;
    case 0x6B: decode_modrm(); { s32 r = (s32)(s16)get_rm16() * (s8)fetch8();
               *reg16_tab[REGF] = (u16)r; R.cf = R.of = r != (s16)r; } break;
    case 0x6C: case 0x6D: case 0x6E: case 0x6F: string_op(op, rep); break;
    case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77:
    case 0x78: case 0x79: case 0x7A: case 0x7B: case 0x7C: case 0x7D: case 0x7E: case 0x7F: {
        s8 d = (s8)fetch8(); if (cond(op & 0xF)) R.ip += d; } break;
    case 0x80: case 0x82: decode_modrm(); { u8 r = alu8(REGF, get_rm8(), fetch8()); if (REGF != 7) set_rm8(r); } break;
    case 0x81: case 0x83: {
        u16 old_sp = R.sp;
        decode_modrm(); { u16 r = alu16(REGF, get_rm16(), op == 0x81 ? fetch16() : (u16)(s8)fetch8()); if (REGF != 7) set_rm16(r); }
        if (stackchk_on && modrm == 0xEC && (u16)(old_sp - R.sp) < 0x1000)   /* sub sp, N: a new frame */
            for (u16 o = R.sp; o != old_sp; o++) stk_dead[lin(R.ss, o)] = 1;
    } break;
    case 0x84: decode_modrm(); logic8(get_rm8() & *reg8_tab[REGF]); break;
    case 0x85: decode_modrm(); logic16(get_rm16() & *reg16_tab[REGF]); break;
    case 0x86: decode_modrm(); { u8 t = get_rm8(); set_rm8(*reg8_tab[REGF]); *reg8_tab[REGF] = t; } break;
    case 0x87: decode_modrm(); { u16 t = get_rm16(); set_rm16(*reg16_tab[REGF]); *reg16_tab[REGF] = t; } break;
    case 0x88: decode_modrm(); set_rm8(*reg8_tab[REGF]); break;
    case 0x89: decode_modrm(); set_rm16(*reg16_tab[REGF]); break;
    case 0x8A: decode_modrm(); *reg8_tab[REGF] = get_rm8(); break;
    case 0x8B: decode_modrm(); *reg16_tab[REGF] = get_rm16(); break;
    case 0x8C: decode_modrm(); set_rm16(*sreg_tab[REGF & 3]); break;
    case 0x8D: decode_modrm(); *reg16_tab[REGF] = ea_off; break;
    case 0x8E: decode_modrm(); *sreg_tab[REGF & 3] = get_rm16(); break;
    case 0x8F: decode_modrm(); { u16 v = pop16(); set_rm16(v); } break;
    case 0x90: break;
    case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97: {
        u16 t = R.ax; R.ax = *reg16_tab[op & 7]; *reg16_tab[op & 7] = t; } break;
    case 0x98: R.ax = (u16)(s8)R.al; break;
    case 0x99: R.dx = (R.ax & 0x8000) ? 0xFFFF : 0; break;
    case 0x9A: { u16 o = fetch16(), s = fetch16();
        push16(R.cs); push16(R.ip);
        if (!try_native(s, o, R.cs, R.ip, 1)) { R.cs = s; R.ip = o; } } break;
    case 0x9B: break;  /* wait */
    case 0x9C: push16(get_flags()); break;
    case 0x9D: set_flags(pop16()); scrub_popped(2); break;
    case 0x9E: { u8 f = R.ah; R.cf = f & 1; R.pf = (f >> 2) & 1; R.af = (f >> 4) & 1;
                 R.zf = (f >> 6) & 1; R.sf = (f >> 7) & 1; } break;
    case 0x9F: R.ah = (u8)get_flags(); break;
    case 0xA0: R.al = rb(seg_ovr_set ? seg_ovr : R.ds, fetch16()); break;
    case 0xA1: R.ax = rw(seg_ovr_set ? seg_ovr : R.ds, fetch16()); break;
    case 0xA2: wb(seg_ovr_set ? seg_ovr : R.ds, fetch16(), R.al); break;
    case 0xA3: ww(seg_ovr_set ? seg_ovr : R.ds, fetch16(), R.ax); break;
    case 0xA4: case 0xA5: case 0xA6: case 0xA7: case 0xAA: case 0xAB: case 0xAC: case 0xAD:
    case 0xAE: case 0xAF: string_op(op, rep); break;
    case 0xA8: logic8(R.al & fetch8()); break;
    case 0xA9: logic16(R.ax & fetch16()); break;
    case 0xB0: case 0xB1: case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6: case 0xB7:
        *reg8_tab[op & 7] = fetch8(); break;
    case 0xB8: case 0xB9: case 0xBA: case 0xBB: case 0xBC: case 0xBD: case 0xBE: case 0xBF:
        *reg16_tab[op & 7] = fetch16(); break;
    case 0xC0: case 0xC1: case 0xD0: case 0xD1: case 0xD2: case 0xD3: {
        decode_modrm();
        u8 cnt = (op <= 0xC1) ? fetch8() : (op <= 0xD1) ? 1 : R.cl;
        if (op & 1) set_rm16(shift16(REGF, get_rm16(), cnt));
        else set_rm8(shift8(REGF, get_rm8(), cnt));
    } break;
    case 0xC2: { u16 n = fetch16(); R.ip = pop16(); R.sp += n; } break;
    case 0xC3: R.ip = pop16(); break;
    case 0xC4: decode_modrm(); *reg16_tab[REGF] = rw(ea_seg, ea_off); R.es = rw(ea_seg, ea_off + 2); break;
    case 0xC5: decode_modrm(); *reg16_tab[REGF] = rw(ea_seg, ea_off); R.ds = rw(ea_seg, ea_off + 2); break;
    case 0xC6: decode_modrm(); set_rm8(fetch8()); break;
    case 0xC7: decode_modrm(); set_rm16(fetch16()); break;
    case 0xC8: { u16 sz = fetch16(); u8 lvl = fetch8() & 31; push16(R.bp); u16 fp = R.sp;
        for (int i = 1; i < lvl; i++) { R.bp -= 2; push16(rw(R.ss, R.bp)); }
        if (lvl) push16(fp);
        R.bp = fp; R.sp -= sz; } break;
    case 0xC9: R.sp = R.bp; R.bp = pop16(); break;
    case 0xCA: { u16 n = fetch16(); R.ip = pop16(); R.cs = pop16(); R.sp += n; } break;
    case 0xCB: R.ip = pop16(); R.cs = pop16(); break;
    case 0xCC: interp_int(3); break;
    case 0xCD: interp_int(fetch8()); break;
    case 0xCE: if (R.of) interp_int(4); break;
    case 0xCF: R.ip = pop16(); R.cs = pop16(); set_flags(pop16()); scrub_popped(6); break;
    case 0xD4: aam(fetch8()); break;
    case 0xD5: aad(fetch8()); break;
    case 0xD6: R.al = R.cf ? 0xFF : 0; break;
    case 0xD7: R.al = rb(seg_ovr_set ? seg_ovr : R.ds, R.bx + R.al); break;
    case 0xD8: case 0xD9: case 0xDA: case 0xDB: case 0xDC: case 0xDD: case 0xDE: case 0xDF:
        decode_modrm(); break;   /* no FPU */
    case 0xE0: { s8 d = (s8)fetch8(); R.cx--; if (R.cx && !R.zf) R.ip += d; } break;
    case 0xE1: { s8 d = (s8)fetch8(); R.cx--; if (R.cx && R.zf) R.ip += d; } break;
    case 0xE2: { s8 d = (s8)fetch8(); R.cx--; if (R.cx) R.ip += d; } break;
    case 0xE3: { s8 d = (s8)fetch8(); if (!R.cx) R.ip += d; } break;
    case 0xE4: R.al = port_in8(fetch8()); break;
    case 0xE5: R.ax = port_in16(fetch8()); break;
    case 0xE6: port_out8(fetch8(), R.al); break;
    case 0xE7: port_out16(fetch8(), R.ax); break;
    case 0xE8: { s16 d = (s16)fetch16(); push16(R.ip); u16 t = R.ip + d;
        if (!try_native(R.cs, t, R.cs, R.ip, 0)) R.ip = t; } break;
    case 0xE9: { s16 d = (s16)fetch16(); R.ip += d; } break;
    case 0xEA: { u16 o = fetch16(), s = fetch16(); R.cs = s; R.ip = o; } break;
    case 0xEB: { s8 d = (s8)fetch8(); R.ip += d; } break;
    case 0xEC: R.al = port_in8(R.dx); break;
    case 0xED: R.ax = port_in16(R.dx); break;
    case 0xEE: port_out8(R.dx, R.al); break;
    case 0xEF: port_out16(R.dx, R.ax); break;
    case 0xF4: runtime_halt_wait(); break;
    case 0xF5: R.cf ^= 1; break;
    case 0xF6: case 0xF7: {
        decode_modrm();
        int w = op & 1;
        switch (REGF) {
        case 0: case 1: if (w) logic16(get_rm16() & fetch16()); else logic8(get_rm8() & fetch8()); break;
        case 2: if (w) set_rm16(~get_rm16()); else set_rm8(~get_rm8()); break;
        case 3: if (w) set_rm16(neg16(get_rm16())); else set_rm8(neg8(get_rm8())); break;
        case 4: if (w) mul16(get_rm16()); else mul8(get_rm8()); break;
        case 5: if (w) imul16(get_rm16()); else imul8(get_rm8()); break;
        case 6: if (w) div16(get_rm16()); else div8(get_rm8()); break;
        case 7: if (w) idiv16(get_rm16()); else idiv8(get_rm8()); break;
        }
    } break;
    case 0xF8: R.cf = 0; break;
    case 0xF9: R.cf = 1; break;
    case 0xFA: R.ifl = 0; break;
    case 0xFB: R.ifl = 1; after_sti = 1; break;
    case 0xFC: R.df = 0; break;
    case 0xFD: R.df = 1; break;
    case 0xFE: decode_modrm();
        if (REGF == 0) set_rm8(inc8(get_rm8()));
        else if (REGF == 1) set_rm8(dec8(get_rm8()));
        else goto bad;
        break;
    case 0xFF: decode_modrm();
        switch (REGF) {
        case 0: set_rm16(inc16(get_rm16())); break;
        case 1: set_rm16(dec16(get_rm16())); break;
        case 2: { u16 t = get_rm16(); push16(R.ip);
            if (!try_native(R.cs, t, R.cs, R.ip, 0)) R.ip = t; } break;
        case 3: { u16 o = rw(ea_seg, ea_off), s = rw(ea_seg, ea_off + 2);
            push16(R.cs); push16(R.ip);
            if (!try_native(s, o, R.cs, R.ip, 1)) { R.cs = s; R.ip = o; } } break;
        case 4: R.ip = get_rm16(); break;
        case 5: { u16 o = rw(ea_seg, ea_off), s = rw(ea_seg, ea_off + 2); R.cs = s; R.ip = o; } break;
        case 6: push16(get_rm16()); break;
        default: goto bad;
        }
        break;
    default:
    bad:
        cpu_halt("interp: unknown opcode %02X at %04X:%04X", op, R.cs, start_ip);
    }
    last_ret = (op == 0xC2 || op == 0xC3 || op == 0xCA || op == 0xCB || op == 0xCF);
    (void)a; (void)b;
}

void interp_run(int far) { interp_loop(0, far); }
void interp_run_forever(void) { interp_loop(1, 1); }

/* Called by the dispatcher when no native function exists for seg:off. The
 * return address is already on the stack. */
void interp_call(u16 seg, u16 off, int far) {
    R.cs = seg; R.ip = off;
    interp_run(far);
}
