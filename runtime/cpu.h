/* Shared 8086 machine state for the interpreter and the recompiled code.
 *
 * Both execution engines operate on the same registers (`R`) and the same
 * 1 MB address space (`M`), so control can pass freely between recompiled
 * functions and interpreted code (e.g. sound drivers loaded at runtime).
 */
#ifndef MTV_CPU_H
#define MTV_CPU_H

#include <stdint.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;

#define REG16(name, lo, hi) \
    union { u16 name; struct { u8 lo, hi; }; }

typedef struct {
    REG16(ax, al, ah);
    REG16(bx, bl, bh);
    REG16(cx, cl, ch);
    REG16(dx, dl, dh);
    u16 si, di, bp, sp;
    u16 es, cs, ss, ds;
    u16 ip;           /* only meaningful inside the interpreter */
    u8 cf, pf, af, zf, sf, of, df, ifl, tf;
} CPU;

extern CPU R;
extern u8 *M;              /* MEM_SIZE bytes; linear = seg*16 + off */
#define MEM_SIZE 0x110000  /* 1 MB + HMA so seg:off never runs off the end */

static inline u32 lin(u16 s, u16 o) { return ((u32)s << 4) + o; }

/* Video memory and port I/O hooks (vga.c) */
void vga_write_hook(u32 a);

#ifdef MEMWATCH
extern u32 watch_lo, watch_hi;
void watch_hit(u32 a, u32 v);
#define WATCHCHK(a, v) do { if ((a) >= watch_lo && (a) < watch_hi) watch_hit((a), (v)); } while (0)
#else
#define WATCHCHK(a, v) do {} while (0)
#endif

/* planar VGA modes route A000:0000-FFFF through the graphics controller */
extern int vga_planar;
u8 vga_read8(u32 a);
void vga_write8(u32 a, u8 v);
#define IS_VRAM(a) (vga_planar && (u32)((a) - 0xA0000) < 0x10000)

/* --stackchk: reads of never-written local variables (interp.c).  Only the
 * runtime objects are built with STACKCHK; gen/ and src/ have no hooks. */
#ifdef STACKCHK
extern u8 stk_dead[];
extern int stk_who_on;
void stackchk_read(u32 a);
void stackchk_wrote(u32 a);
#define SCHK_R(a) do { if (stk_dead[a]) stackchk_read(a); } while (0)
#define SCHK_W(a) (stk_dead[a] = 0, stk_who_on ? stackchk_wrote(a) : (void)0)
#else
#define SCHK_R(a) do {} while (0)
#define SCHK_W(a) do {} while (0)
#endif

static inline u8 rb(u16 s, u16 o) {
    u32 a = lin(s, o);
    if (IS_VRAM(a)) return vga_read8(a);
    SCHK_R(a);
    return M[a];
}
static inline u16 rw(u16 s, u16 o) {
    u32 a = lin(s, o);
    if (IS_VRAM(a)) return vga_read8(a) | (vga_read8(lin(s, (u16)(o + 1))) << 8);
    SCHK_R(a); SCHK_R(lin(s, (u16)(o + 1)));
    if (o == 0xFFFF) return M[a] | (M[lin(s, 0)] << 8);
    return M[a] | (M[a + 1] << 8);
}
static inline void wb(u16 s, u16 o, u8 v) {
    u32 a = lin(s, o);
    if (IS_VRAM(a)) { vga_write8(a, v); return; }
    WATCHCHK(a, v); SCHK_W(a); M[a] = v;
}
static inline void ww(u16 s, u16 o, u16 v) {
    u32 a = lin(s, o);
    if (IS_VRAM(a)) { vga_write8(a, (u8)v); vga_write8(lin(s, (u16)(o + 1)), v >> 8); return; }
    if (o == 0xFFFF) { M[a] = (u8)v; M[lin(s, 0)] = v >> 8; return; }
    WATCHCHK(a, v); SCHK_W(a); SCHK_W(a + 1);
    M[a] = (u8)v; M[a + 1] = v >> 8;
}
static inline u32 rd(u16 s, u16 o) { return rw(s, o) | ((u32)rw(s, (u16)(o + 2)) << 16); }

static inline void push16(u16 v) { R.sp -= 2; ww(R.ss, R.sp, v); }
static inline u16 pop16(void) { u16 v = rw(R.ss, R.sp); R.sp += 2; return v; }

/* ---- flags ---- */
static inline u8 parity8(u8 v) { return (0x9669 >> ((v ^ (v >> 4)) & 0xF)) & 1; }

static inline u16 get_flags(void) {
    return 0xF002 | R.cf | (R.pf << 2) | (R.af << 4) | (R.zf << 6) | (R.sf << 7) |
           (R.tf << 8) | (R.ifl << 9) | (R.df << 10) | (R.of << 11);
}
static inline void set_flags(u16 f) {
    R.cf = f & 1; R.pf = (f >> 2) & 1; R.af = (f >> 4) & 1; R.zf = (f >> 6) & 1;
    R.sf = (f >> 7) & 1; R.tf = (f >> 8) & 1; R.ifl = (f >> 9) & 1; R.df = (f >> 10) & 1;
    R.of = (f >> 11) & 1;
}

#define SZP8(r)  do { R.zf = ((u8)(r) == 0); R.sf = ((u8)(r)) >> 7; R.pf = parity8((u8)(r)); } while (0)
#define SZP16(r) do { R.zf = ((u16)(r) == 0); R.sf = ((u16)(r)) >> 15; R.pf = parity8((u8)(r)); } while (0)

static inline u8 add8(u8 a, u8 b, int c) {
    u32 r = a + b + c;
    R.cf = r >> 8; R.af = ((a ^ b ^ r) >> 4) & 1; R.of = (((a ^ r) & (b ^ r)) >> 7) & 1;
    SZP8(r); return (u8)r;
}
static inline u16 add16(u16 a, u16 b, int c) {
    u32 r = (u32)a + b + c;
    R.cf = r >> 16; R.af = ((a ^ b ^ r) >> 4) & 1; R.of = (((a ^ r) & (b ^ r)) >> 15) & 1;
    SZP16(r); return (u16)r;
}
static inline u8 sub8(u8 a, u8 b, int c) {
    u32 r = (u32)a - b - c;
    R.cf = (r >> 8) & 1; R.af = ((a ^ b ^ r) >> 4) & 1; R.of = (((a ^ b) & (a ^ r)) >> 7) & 1;
    SZP8(r); return (u8)r;
}
static inline u16 sub16(u16 a, u16 b, int c) {
    u32 r = (u32)a - b - c;
    R.cf = (r >> 16) & 1; R.af = ((a ^ b ^ r) >> 4) & 1; R.of = (((a ^ b) & (a ^ r)) >> 15) & 1;
    SZP16(r); return (u16)r;
}
static inline u8 logic8(u8 r) { R.cf = R.of = 0; R.af = 0; SZP8(r); return r; }
static inline u16 logic16(u16 r) { R.cf = R.of = 0; R.af = 0; SZP16(r); return r; }
static inline u8 inc8(u8 a) { int c = R.cf; u8 r = add8(a, 1, 0); R.cf = c; return r; }
static inline u16 inc16(u16 a) { int c = R.cf; u16 r = add16(a, 1, 0); R.cf = c; return r; }
static inline u8 dec8(u8 a) { int c = R.cf; u8 r = sub8(a, 1, 0); R.cf = c; return r; }
static inline u16 dec16(u16 a) { int c = R.cf; u16 r = sub16(a, 1, 0); R.cf = c; return r; }
static inline u8 neg8(u8 a) { u8 r = sub8(0, a, 0); R.cf = a != 0; return r; }
static inline u16 neg16(u16 a) { u16 r = sub16(0, a, 0); R.cf = a != 0; return r; }

/* Shifts/rotates: count already masked by caller semantics (8086 does not mask;
 * we mask to 5 bits like 186+, which only matters for counts >= 32). */
u8 shift8(int op, u8 v, u8 cnt);
u16 shift16(int op, u16 v, u8 cnt);
enum { SH_ROL, SH_ROR, SH_RCL, SH_RCR, SH_SHL, SH_SHR, SH_SAL, SH_SAR };

/* mul/div; division by zero / overflow raises INT 0 */
void mul8(u8 v); void imul8(u8 v); void mul16(u16 v); void imul16(u16 v);
void div8(u8 v); void idiv8(u8 v); void div16(u16 v); void idiv16(u16 v);
void daa(void); void das(void); void aaa(void); void aas(void);
void aam(u8 base); void aad(u8 base);

/* ---- I/O ---- */
u8 port_in8(u16 port);
u16 port_in16(u16 port);
void port_out8(u16 port, u8 v);
void port_out16(u16 port, u16 v);

/* ---- control transfer between engines ---- */
/* Call code at seg:off as if by CALL FAR/NEAR: the return address has already
 * been pushed by the caller.  Returns once that return address is popped. */
void call_far(u16 seg, u16 off);
void call_near(u16 seg, u16 off);   /* cs stays seg */
void do_int(u8 n);                 /* software interrupt from any engine */
void irq_poll(void);               /* deliver pending hardware interrupts */
void cpu_halt(const char *why, ...) __attribute__((noreturn, format(printf, 1, 2)));

extern u16 LOADSEG;                /* segment the EXE image is loaded at */
extern volatile int irq_pending;
#endif
