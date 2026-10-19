/* Control transfer between recompiled code, the interpreter and BIOS stubs. */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "runtime.h"

CPU R;
u8 *M;
u16 LOADSEG = LOADSEG_C;
int exe_code_loaded = 1;              /* 0: loaded from the built-in image (runtime/main.c) */
int use_native = 1;
int trace_calls;

/* Calling-convention table of the decompiled functions, read by the difftest
 * (difftest.c).  The decompiled build defines it in src/game_table.c; the
 * other builds get these empty defaults.  Weak definitions, not weak
 * references: macOS's linker rejects weak references left undefined.  They
 * live outside difftest.c so that its code cannot assume the default size. */
__attribute__((weak)) const u32 conv_keys[1] = {0};
__attribute__((weak)) const u8 conv_ret[1] = {0};
__attribute__((weak)) const u8 conv_nwords[1] = {0};
__attribute__((weak)) const int conv_count = 0;

native_fn native_lookup(u16 seg, u16 off) {
    if (!use_native) return 0;
    return native_lookup_any(seg, off);
}

/* Which native function sits at a linear address of the program image:
 * built on first use (the table and the load segment do not change).  The
 * interpreter asks on every call it executes, and most calls go to code
 * without a native twin, so this must be cheap. */
#define IMAGE_SPAN 0x50000
static u16 *lin_index;                 /* table index + 1, 0 = none; past native_count: src_native_table */
/* C functions of src/ for code the lifter has no function for (entered
 * only through a vector or a function pointer: the start-up's hooks and its
 * divide-error handler); none without src/ (clock_none.c) */
extern const native_entry src_native_table[];
extern const int src_native_count;
static void build_lin_index(void) {
    lin_index = calloc(IMAGE_SPAN, sizeof *lin_index);
    if (!lin_index) { fprintf(stderr, "out of memory\n"); exit(1); }
    for (int i = 0; i < src_native_count; i++) {
        u32 k = src_native_table[i].key;
        u32 l = ((k >> 16) << 4) + (k & 0xFFFF);
        if (l < IMAGE_SPAN) lin_index[l] = (u16)(native_count + i + 1);
    }
    for (int i = native_count - 1; i >= 0; i--) {       /* first entry wins, like the old scan */
        u32 k = native_table[i].key;
        u32 l = ((k >> 16) << 4) + (k & 0xFFFF);
        if (l < IMAGE_SPAN) lin_index[l] = (u16)(i + 1);
    }
}
native_fn native_lookup_any(u16 seg, u16 off) {
    u32 l = lin(seg, off), base = (u32)LOADSEG << 4;
    if (l < base || l >= base + IMAGE_SPAN) return 0;
    if (!lin_index) build_lin_index();
    u16 i = lin_index[l - base];
    if (i > native_count) return src_native_table[i - 1 - native_count].fn;
    return i ? native_table[i - 1].fn : 0;
}

/* Is the native function at seg:off decompiled C (src/), or lifted machine
 * code (gen/)?  The decompiled build keeps game state in host memory
 * (src/state.c), lifted code and the interpreter use the emulated memory:
 * crossing between them syncs the two. */
extern const u32 dec_keys[];
extern const int dec_count;
int native_decompiled(u16 seg, u16 off) {
    u32 k = ((u32)(u16)(seg - LOADSEG) << 16) | off;
    int lo = 0, hi = dec_count - 1;
    while (lo <= hi) {
        int m = (lo + hi) / 2;
        if (dec_keys[m] == k) return 1;
        if (dec_keys[m] < k) lo = m + 1; else hi = m - 1;
    }
    return 0;
}

/* ENGINE_STATS=1: how often native C code entered machine-level code --
 * lifted functions (gen/), the interpreter in the program's code segments,
 * the interpreter elsewhere (the sound driver, BIOS stubs) -- printed at
 * exit.  Every way into either passes call_any (do_int too). */
static u64 calls_lifted, calls_interp_program, calls_interp_other;
static void engine_stats(void) {
    fprintf(stderr, "[engine] calls into lifted code %llu, into the interpreter: program code %llu, other (driver) %llu\n",
            (unsigned long long)calls_lifted, (unsigned long long)calls_interp_program, (unsigned long long)calls_interp_other);
}
void engine_stats_init(void) { if (getenv("ENGINE_STATS")) atexit(engine_stats); }

int interp_depth;
static void call_any(u16 seg, u16 off, int far);
void call_far(u16 seg, u16 off) { call_any(seg, off, 1); }
void call_near(u16 seg, u16 off) { call_any(seg, off, 0); }

static void call_any(u16 seg, u16 off, int far) {
    if (trace_calls) fprintf(stderr, "callf %04X:%04X (rel %04X) sp=%04X T=%llu\n", seg, off, (u16)(seg - LOADSEG), R.sp, (unsigned long long)T);
    native_fn f = native_lookup(seg, off);
    if (f) {
        int lifted = !native_decompiled(seg, off);
        u16 cs = R.cs; R.cs = seg;
        calls_lifted += lifted;
        if (lifted) state_export();      /* lifted code works on the emulated memory */
        f();
        if (lifted) state_reload();
        R.cs = cs;
        return;
    }
    if (seg == 0 && off == 0)
        cpu_halt("far call to 0000:0000 from %04X:%04X (rel %04X)", rw(R.ss, R.sp + 2), rw(R.ss, R.sp),
                 (u16)(rw(R.ss, R.sp + 2) - LOADSEG));
    if (seg == BIOS_SEG) {    /* far call/jump straight into a BIOS stub (e.g. chained vector) */
        u16 cs = R.cs, ip = R.ip;
        bios_stub_iret((u8)off);
        R.cs = cs; R.ip = ip;
        return;
    }
    u16 cs = R.cs, ip = R.ip;
    if (seg >= LOADSEG && seg < (u16)(LOADSEG + 0x39D4)) {                     /* below DGROUP */
        /* without the EXE the code segments are empty (runtime/main.c) */
        if (!exe_code_loaded)
            cpu_halt("the original's machine code at %04X:%04X (rel %04X) is not in this build: run with --exe MTV_U.EXE",
                     seg, off, (u16)(seg - LOADSEG));
        calls_interp_program++;
    }
    else calls_interp_other++;
    clock_entry((u16)(seg - LOADSEG), off);
    /* into the interpreter (the sound driver, its timer interrupt, start-up
     * and exit stubs): the game state the decompiled build keeps in host
     * memory (src/state.c) goes to the emulated memory and back */
    state_export();
    interp_depth++;
    interp_call(seg, off, far);
    interp_depth--;
    state_reload();
    R.cs = cs; R.ip = ip;
}

/* INT n executed by recompiled code (or a hardware IRQ delivered to it). */
void do_int(u8 n) {
    u16 vo = rw(0, n * 4), vs = rw(0, n * 4 + 2);
    if (bios_is_stub(vs, vo)) { bios_dispatch((u8)vo); return; }
    push16(get_flags());
    push16(R.cs);
    push16(0xFFFF);           /* return IP is never used by handlers we run */
    u8 ifl = R.ifl;
    R.ifl = 0; R.tf = 0;
    call_far(vs, vo);         /* handler's IRET pops flags back into R */
    (void)ifl;
}

int difftest_native_halt(const char *msg);
void cpu_halt(const char *why, ...) {
    va_list ap;
    char msg[256];
    va_start(ap, why);
    vsnprintf(msg, sizeof msg, why, ap);
    va_end(ap);
    difftest_native_halt(msg);          /* native half of a test: report and recover */
    difftest_abort(msg);                /* reference half: abandon this test */
    va_start(ap, why);
    fprintf(stderr, "\n*** HALT: ");
    vfprintf(stderr, why, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    dump_state("halt");
    exit(3);
}

void dump_state(const char *why) {
    fprintf(stderr, "[%s] T=%llu AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X BP=%04X SP=%04X\n"
            "  CS=%04X(rel %04X) IP=%04X DS=%04X ES=%04X SS=%04X  C%d Z%d S%d O%d I%d D%d\n",
            why, (unsigned long long)T, R.ax, R.bx, R.cx, R.dx, R.si, R.di, R.bp, R.sp,
            R.cs, (u16)(R.cs - LOADSEG), R.ip, R.ds, R.es, R.ss, R.cf, R.zf, R.sf, R.of, R.ifl, R.df);
}

void chk_ret_fail(u16 seg, u16 off, u16 at, u16 entry_sp) {
    cpu_halt("stack imbalance in f_%04X_%04X: ret at %04X with SP=%04X, entry SP=%04X",
             seg, off, at, R.sp, entry_sp);
}

/* RET/RETF used as a jump: the target was pushed on top of the real return
 * address.  Pop it and run the target, which eventually returns to our
 * caller through the original return address. */
void computed_ret(int far, u16 cs) {
    u16 off = pop16();
    u16 seg = far ? pop16() : cs;
    if (far) call_far(seg, off); else call_near(seg, off);
}

#ifdef MEMWATCH
#include <execinfo.h>
u32 watch_lo = 0x84, watch_hi = 0x88;     /* INT 21h vector by default */
void watch_hit(u32 a, u32 v) {
    if (getenv("WATCH_QUIET")) return;
    fprintf(stderr, "[watch] write %05X <- %04X at T=%llu\n", a, v, (unsigned long long)T);
    void *bt[16]; int n = backtrace(bt, 16);
    backtrace_symbols_fd(bt, n, 2);
}
#endif
