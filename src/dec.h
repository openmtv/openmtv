/* Support header for the decompiled (readable) C sources in src/.
 *
 * Decompiled functions are ordinary C functions with typed parameters and
 * return values.  Game data stays in the emulated 1 MB address space, so
 * decompiled code, recompiled machine-level code (gen/) and the reference
 * interpreter share one memory image and can be compared byte for byte.
 */
#ifndef MTV_DEC_H
#define MTV_DEC_H
#include <stdarg.h>
#include "runtime.h"

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef uint32_t fptr;               /* far pointer as stored by the game: seg << 16 | off */

/* ---- data segment (DGROUP) ---- */
#define DSEG ((u16)(LOADSEG_C + 0x39D4))
#define DSLIN ((u32)DSEG << 4)
/* Game state lives in host memory (ROADMAP Phase 3, stage 5; src/state.c):
 * the game's variables DGROUP 0000..A72F in g_dgroup once state_activate()
 * has copied them in (the stack segment begins at A730), except the tick
 * block DS:902C..904B that the sound driver's timer interrupt counts in the
 * emulated memory; the film, series and screenplay tables in host arrays.
 * Far pointers stay valid handles: host_addr() resolves one into a moved
 * area to the host copy, anything else (resources, bitmaps, VRAM, the
 * emulated stack) to the emulated memory. */
#define DG_END 0xA730u
#define DG_SHARED_LO 0x902Cu
#define DG_SHARED_HI 0x904Cu
extern u8 g_dgroup[DG_END];
extern int dg_active;
#ifdef DG_TRACE                        /* build/mtv-dec-trace: who touches what (tools/dgtrace.c) */
void dg_trace(u16 o);
#define DG_TRACE_HIT(o) dg_trace(o)
#else
#define DG_TRACE_HIT(o) ((void)0)
#endif
static inline u8 *dg_addr(u16 o) {
    DG_TRACE_HIT(o);
    if (dg_active && o < DG_END && !(o >= DG_SHARED_LO && o < DG_SHARED_HI)) return g_dgroup + o;
    return M + DSLIN + o;
}
#define G8(o)  (*(u8 *)dg_addr((u16)(o)))
#define G16(o) (*(u16 *)dg_addr((u16)(o)))
#define G32(o) (*(u32 *)dg_addr((u16)(o)))
#define DSP(o) (((u32)DSEG << 16) | (u16)(o))          /* far pointer to DGROUP data */

/* ---- far pointers ---- */
#define FPSEG(p) ((u16)((u32)(p) >> 16))
#define FPOFF(p) ((u16)(p))
#define MKFP(s, o) (((u32)(u16)(s) << 16) | (u16)(o))
#define FPADD(p, n) (((u32)(p) & 0xFFFF0000u) | (u16)((u16)(p) + (n)))   /* far ptr arithmetic wraps the offset */
/* moved areas other than DGROUP: the tables and handles for C buffers */
typedef struct { u32 lin, len; u8 *host; } HostRegion;
#define HOST_REGIONS 19
extern HostRegion host_regions[HOST_REGIONS];
static inline u8 *host_addr(u16 seg, u16 off) {
    u32 a = lin(seg, off);
    if (dg_active && a - DSLIN < DG_END) return dg_addr((u16)(a - DSLIN));
    for (int i = 0; i < HOST_REGIONS; i++)
        if (a - host_regions[i].lin < host_regions[i].len) return host_regions[i].host + (a - host_regions[i].lin);
    return M + a;
}
static inline int host_moved(const u8 *p) { return p < M || p >= M + MEM_SIZE; }
#define FP8(p, k)  (*(u8 *)host_addr(FPSEG(p), (u16)(FPOFF(p) + (k))))
#define FP16(p, k) (*(u16 *)host_addr(FPSEG(p), (u16)(FPOFF(p) + (k))))
#define FP32(p, k) (*(u32 *)host_addr(FPSEG(p), (u16)(FPOFF(p) + (k))))

/* the run-time library's byte/word accessors, through host_addr(); memory
 * that is not moved goes through the runtime's (VGA write semantics, ...) */
static inline u8 cpu_rb(u16 s, u16 o) { return rb(s, o); }
static inline u16 cpu_rw(u16 s, u16 o) { return rw(s, o); }
static inline void cpu_wb(u16 s, u16 o, u8 v) { wb(s, o, v); }
static inline void cpu_ww(u16 s, u16 o, u16 v) { ww(s, o, v); }
static inline u8 src_rb(u16 s, u16 o) { u8 *p = host_addr(s, o); return host_moved(p) ? *p : cpu_rb(s, o); }
static inline void src_wb(u16 s, u16 o, u8 v) { u8 *p = host_addr(s, o); if (host_moved(p)) *p = v; else cpu_wb(s, o, v); }
static inline u16 src_rw(u16 s, u16 o) { return (u16)(src_rb(s, o) | (src_rb(s, (u16)(o + 1)) << 8)); }
static inline void src_ww(u16 s, u16 o, u16 v) { src_wb(s, o, (u8)v); src_wb(s, (u16)(o + 1), (u8)(v >> 8)); }
#define rb(s, o) src_rb((s), (o))
#define wb(s, o, v) src_wb((s), (o), (v))
#define rw(s, o) src_rw((s), (o))
#define ww(s, o, v) src_ww((s), (o), (v))
#define rd(s, o) ((u32)src_rw((s), (o)) | ((u32)src_rw((s), (u16)((o) + 2)) << 16))

/* ---- arbitrary segment:offset ---- */
#define MEM8(s, o)  (*(u8 *)host_addr((s), (u16)(o)))
#define MEM16(s, o) (*(u16 *)host_addr((s), (u16)(o)))
#define MEM32(s, o) (*(u32 *)host_addr((s), (u16)(o)))

/* ---- 32-bit values held in DX:AX ---- */
#define HI16(v) ((u16)((u32)(v) >> 16))
#define LO16(v) ((u16)(v))
#define MK32(hi, lo) (((u32)(u16)(hi) << 16) | (u16)(lo))
#define W2(x)    LO16(x), HI16(x)   /* a 32-bit value as two word arguments (low first) */

/* ---- emulated-stack frame for locals whose address is taken ---- */
#define SSPTR(o) (((u32)R.ss << 16) | (u16)(o))

/* ---- timing: keep the virtual clock close to the original ---- */
#define TICK(n) (T += (n))
#define POLL() do { LOGICAL_SYNC(); if (T >= next_event_T) runtime_events(); if (irq_pending && R.ifl) irq_poll(); } while (0)
/* function entry / loop head: account the original instruction count, take interrupts */
#define FUNC_TICK(n) do { T += (n); clock_entries_n++; if (clock_trace) clock_trace_line("E %s", __func__); POLL(); } while (0)
/* a function the logical clock does not count (tools/clock_table.py leaves it
 * out of the gen/interpreter count too): its call count depends on original
 * stack garbage, so the engines would count it differently */
#define FUNC_TICK_NOCLOCK(n) do { T += (n); POLL(); } while (0)
#define LOOP_TICK(n) do { T += (n); POLL(); } while (0)
/* fast room changes (runtime.h fast_rooms: several humans, or alone --fast-rooms): the
 * sprite decoder's work takes no game time.  A room's pictures took up to two seconds of
 * logical time -- the original's 386: with several humans the world, which runs in the
 * host's frames, stood still for everyone (runtime/seat.c), alone the player waited.  The
 * same on every machine; else the original's time. */
#define FUNC_TICK_FAST_ROOMS(n) do { if (!fast_rooms()) FUNC_TICK(n); } while (0)
/* entry of a run-time library / assembler helper rewritten in C (rtl.c): interrupts
 * where the machine-level code checks them, no logical-clock entry (clock_table.py) */
#define RTL_ENTRY() POLL()
/* ... of one the machine-level code entered without a poll point (code the
 * lifter had no function for: start-up hooks, the divide-error handler) */
#define RTL_ENTRY_NOPOLL() do { } while (0)

/* ---- calling machine-level (lifted or interpreted) far functions ---- */
u32 call_lifted(native_fn f, u16 seg, int nwords, ...);
u32 call_lifted_near(native_fn f, u16 seg, int nwords, ...);
u32 call_far_addr(fptr target, int nwords, ...);

/* argument access inside wrappers: SP points at the far return address */
#define WARG16(n) rw(R.ss, (u16)(R.sp + 4 + 2 * (n)))
#define WARG32(n) rd(R.ss, (u16)(R.sp + 4 + 2 * (n)))
#define WRET_FAR() (R.sp += 4)

/* ---- Turbo C struct assignment (SCOPY@): forward byte copy ---- */
static inline void struct_copy(u8 *dst, const u8 *src, u16 n) {
    for (u16 i = 0; i < n; i++) dst[i] = src[i];
}

/* ---- emulated stack frame for functions taking addresses of locals ---- */
#define LOC8(n)  MEM8(R.ss, (u16)(bp_ - (n)))
#define LOC16(n) MEM16(R.ss, (u16)(bp_ - (n)))
#define LOC32(n) MEM32(R.ss, (u16)(bp_ - (n)))
#define ARG8(n)  MEM8(R.ss, (u16)(bp_ + (n)))    /* argument of a stack-ABI function (frame offset) */
#define ARG16(n) MEM16(R.ss, (u16)(bp_ + (n)))
#define ARG32(n) MEM32(R.ss, (u16)(bp_ + (n)))
/* a frame kept as C bytes (tools/frame2locals.py): bp-k in a u8 array of n
 * bytes holding bp-n .. bp-1, same layout as the emulated frame */
#define FR8(a, n, k)  (*(u8 *)((a) + (n) - (k)))
#define FR16(a, n, k) (*(u16 *)((a) + (n) - (k)))
#define FR32(a, n, k) (*(u32 *)((a) + (n) - (k)))
/* host pointers (tools/hostptr.py): a far pointer's bytes in the emulated
 * memory, a slot of the emulated frame, and accesses through them */
#define HPTR(fp) host_addr(FPSEG(fp), FPOFF(fp))
#define LOCP(n)  (M + lin(R.ss, (u16)(bp_ - (n))))
#define HP8(p, k)  (*(u8 *)((p) + (k)))
#define HP16(p, k) (*(u16 *)((p) + (k)))
#define HP32(p, k) (*(u32 *)((p) + (k)))
/* element of a local array at bp-k plus byte offset d (a 16-bit address offset) */
#define FRX8(a, n, k, d)  (*(u8 *)((a) + (n) - (k) + (int)(i16)(d)))
#define FRX16(a, n, k, d) (*(u16 *)((a) + (n) - (k) + (int)(i16)(d)))
#define FRX32(a, n, k, d) (*(u32 *)((a) + (n) - (k) + (int)(i16)(d)))

#endif
