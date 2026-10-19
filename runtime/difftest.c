/* Differential tester: recompiled function vs. reference interpreter.
 *
 * Run the game with --interp --difftest.  When interpreted code calls an
 * address that has a recompiled function, the first few calls are executed
 * twice from the same snapshot -- once fully interpreted (reference), once
 * natively -- and the resulting machine states are compared.  Then the state
 * is rewound and the call runs once more, interpreted as in a plain --interp
 * run (events on), so the whole run follows the same game as build/mtv: the
 * held timer would otherwise leave g_timer_ticks behind, and random_n() seeds
 * from it.  Callees of a tested call get tested during that third run.
 *
 * During a test, timers/IRQs/host events are frozen, port 3DA returns a
 * deterministic sequence, and file positions and the device state (VGA,
 * PIC/PIT, mouse, ...: runtime.h StateBlock) are snapshotted.  Anything that
 * cannot be replayed (blocking keyboard read, file writes, runaway loops)
 * aborts the test and the call simply runs interpreted.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include "runtime.h"

int difftest_on;
int difftest_active;              /* inside a test: freeze events */
u32 difftest_port_seq;
static jmp_buf abort_jb;
static int can_abort;
static u8 *snap0, *snapI;
static CPU r0, rI;
static u64 t0;
static int max_tests_per_fn = 4;
static double ref_seconds;     /* wall time of the reference (interpreted) run */
static int difftest_force;
static int n_tests, n_fail, n_abort, n_undef;
static u8 *snapD;                 /* stack shadow (stk_dead) before the call */

/* device state (runtime.h StateBlock): saved with snap0, restored before the
 * native run and at the rewind */
static StateBlock dev[96];
static int ndev;
static u8 *dev0;
static void dev_save(void) {
    if (!dev0) {
        size_t total = 0;
        ndev = hw_state_blocks(dev);
        ndev += vga_state_blocks(dev + ndev);
        ndev += bios_state_blocks(dev + ndev);
        ndev += dos_state_blocks(dev + ndev);
        ndev += adlib_state_blocks(dev + ndev);
        for (int i = 0; i < ndev; i++) total += dev[i].n;
        dev0 = malloc(total);
    }
    u8 *q = dev0;
    for (int i = 0; i < ndev; i++) { memcpy(q, dev[i].p, dev[i].n); q += dev[i].n; }
}
static void dev_restore(void) {
    const u8 *q = dev0;
    for (int i = 0; i < ndev; i++) { memcpy(dev[i].p, q, dev[i].n); q += dev[i].n; }
}
static const char *diff_tag = "DIFF";
static u8 tested[0x10000 * 4];    /* per table index */
static u8 failed_fn[0x10000];
static u8 passed[0x10000 * 4];
extern const u8 native_retflags[];
extern u64 interp_insns;
int dos_snapshot_pos(long *pos, int n);
void dos_restore_pos(const long *pos, int n);
static long fpos[64];
static sigjmp_buf native_jb;
static int in_native;
static char native_halt_msg[300];

/* cpu_halt() inside the native half of a test: record it and recover */
int difftest_native_halt(const char *msg) {
    if (!in_native) return 0;
    snprintf(native_halt_msg, sizeof native_halt_msg, "native run halted: %s", msg);
    siglongjmp(native_jb, 1);
}

void difftest_abort(const char *why) {
    if (!difftest_active || !can_abort) return;
    (void)why;
    longjmp(abort_jb, 1);
}

static int table_index(native_fn f) {
    for (int i = 0; i < native_count; i++) if (native_table[i].fn == f) return i;
    return -1;
}

static void report(int idx, const char *what) {
    u32 k = native_table[idx].key;
    fprintf(stderr, "%s f_%04X_%04X: %s\n", diff_tag, k >> 16, k & 0xFFFF, what);
}

/* decompiled functions only promise the C calling convention: compare the
 * return registers their callers use, the preserved registers and memory.
 * The table is src/game_table.c; other builds link the empty default in
 * dispatch.c (conv_count = 0). */
extern const u32 conv_keys[];
extern const u8 conv_ret[];
extern const u8 conv_nwords[];
extern const int conv_count;
static int conv_nw;
static int dec_ret_of(u32 key) {
    conv_nw = 0;
    for (int i = 0; i < conv_count; i++)
        if (conv_keys[i] == key) { conv_nw = conv_nwords[i]; return conv_ret[i]; }
    return -1;
}

/* runtime-library functions rewritten in C keep the original's exact time
 * (src/rtl*.c): their T delta must match the reference run's -- unless a
 * decompiled game function ran inside the call (its FUNC_TICK time is an
 * estimate; FUNC_TICK counts clock_entries_n): farmalloc calling the
 * decompiled heap_allocate_partial_block, say */
extern const u32 rtl_exact_keys[];
extern const int rtl_exact_count;
static int rtl_exact(u32 key) {
    for (int i = 0; i < rtl_exact_count; i++) if (rtl_exact_keys[i] == key) return 1;
    return 0;
}
static u64 tI, ceN;

static int compare(int idx) {
    int bad = 0;
    char buf[256];
    int dr = dec_ret_of(native_table[idx].key);
#define CMP(r) if (R.r != rI.r) { snprintf(buf, sizeof buf, #r " native=%04X interp=%04X", R.r, rI.r); report(idx, buf); bad++; }
    if (dr < 0) { CMP(ax) CMP(bx) CMP(cx) CMP(dx) CMP(es) }
    else { if (dr >= 16) { CMP(ax) } if (dr == 32) { CMP(dx) } }
    CMP(si) CMP(di) CMP(bp) CMP(sp) CMP(ds) CMP(ss)
    CMP(df) CMP(ifl)
    u8 rf = dr < 0 ? native_retflags[idx] : 0;
    if ((rf & 1) && R.cf != rI.cf) { report(idx, "CF"); bad++; }
    if ((rf & 8) && R.zf != rI.zf) { report(idx, "ZF"); bad++; }
    if ((rf & 16) && R.sf != rI.sf) { report(idx, "SF"); bad++; }
    if ((rf & 32) && R.of != rI.of) { report(idx, "OF"); bad++; }
    /* memory, ignoring the dead part of the stack below SP */
    u32 dead_lo = lin(rI.ss, 0), dead_hi = lin(rI.ss, rI.sp);
    if (dr >= 0) dead_hi = lin(rI.ss, (u16)(rI.sp + 2 * conv_nw));   /* caller-owned argument words */
    int shown = 0, ndiff = 0;
    for (u32 a = 0; a < MEM_SIZE; a++) {
        if (M[a] != snapI[a]) {
            if (a >= dead_lo && a < dead_hi) continue;
            ndiff++;
            if (shown < 8) {
                snprintf(buf, sizeof buf, "mem %05X native=%02X interp=%02X (was %02X)", a, M[a], snapI[a], snap0[a]);
                report(idx, buf); shown++;
            }
        }
    }
    if (ndiff > shown) { snprintf(buf, sizeof buf, "... %d bytes differ", ndiff); report(idx, buf); }
    if (rtl_exact(native_table[idx].key) && clock_entries_n == ceN && T - t0 != tI - t0) {
        snprintf(buf, sizeof buf, "time native=%llu interp=%llu", (unsigned long long)(T - t0), (unsigned long long)(tI - t0));
        report(idx, buf); bad++;
    }
    return bad + ndiff;
}

/* Called from the interpreter instead of jumping to seg:off when a native
 * function exists.  Return address already pushed.  Always returns 0 with the
 * state as on entry: the caller then runs the call interpreted. */
int difftest_call(u16 seg, u16 off, native_fn f, int far) {
    int idx = table_index(f);
    if (idx < 0 || difftest_active) return 0;
    if (!difftest_force && (tested[idx] >= max_tests_per_fn || failed_fn[idx])) return 0;
    tested[idx]++;
    if (!snap0) { snap0 = malloc(MEM_SIZE); snapI = malloc(MEM_SIZE); snapD = malloc(MEM_SIZE); }
    memcpy(snap0, M, MEM_SIZE);
    memcpy(snapD, stk_dead, MEM_SIZE);
    dev_save();
    r0 = R; t0 = T;
    dos_snapshot_pos(fpos, 64);
    difftest_active = 1;
    /* 1) reference: interpret, no native code at all */
    can_abort = 1;
    if (setjmp(abort_jb)) {
        difftest_active = 0; can_abort = 0;
        memcpy(M, snap0, MEM_SIZE); R = r0; T = t0; dos_restore_pos(fpos, 64);
        memcpy(stk_dead, snapD, MEM_SIZE);
        dev_restore();
        n_abort++;
        tested[idx] = max_tests_per_fn;   /* do not retry this function */
        return 0;
    }
    int saved_native = use_native;
    use_native = 0;
    difftest_port_seq = 0;
    u64 start_insns = interp_insns;
    R.cs = seg; R.ip = off;
    { extern u64 difftest_budget; difftest_budget = interp_insns + 30000000ull; }
    struct timespec ts0, ts1; clock_gettime(CLOCK_MONOTONIC, &ts0);
    stackchk_hits = 0;
    clock_entry((u16)(seg - LOADSEG), off);
    interp_run(far);
    int undef = stackchk_hits;    /* the reference read never-written stack */
    clock_gettime(CLOCK_MONOTONIC, &ts1);
    ref_seconds = (ts1.tv_sec - ts0.tv_sec) + (ts1.tv_nsec - ts0.tv_nsec) / 1e9;
    use_native = saved_native;
    (void)start_insns;
    rI = R; tI = T;
    memcpy(snapI, M, MEM_SIZE);
    /* 2) native, from the same snapshot */
    memcpy(M, snap0, MEM_SIZE); R = r0; T = t0; dos_restore_pos(fpos, 64);
    dev_restore();
    difftest_port_seq = 0;
    can_abort = 0;
    use_native = 1;
    R.cs = seg;
    int bad;
    if (difftest_force) alarm(5 + (unsigned)(20 * ref_seconds));   /* native may not be far slower */
    in_native = 1;
    if (getenv("DIFFTEST_VERBOSE")) { u32 k = native_table[idx].key; fprintf(stderr, "[test] f_%04X_%04X\n", k >> 16, k & 0xFFFF); }
    ceN = clock_entries_n;
    int dec = native_decompiled(seg, off);
    if (dec) state_activate();   /* decompiled code: game state in host memory (src/state.c) from the snapshot */
    if (sigsetjmp(native_jb, 1) == 0) {
        f();
        state_export();      /* ... and back into the original layout for the comparison */
        state_drop();
        in_native = 0;
        use_native = saved_native;
        /* a result that depends on stack garbage is not comparable: the
         * decompiled build's stack layout differs from the original's */
        if (undef) diff_tag = "UNDEF";
        bad = compare(idx);
        diff_tag = "DIFF";
        if (bad && undef) {
            char buf[120];
            snprintf(buf, sizeof buf, "differs, but the original read a never-written local at %04X:%04X (its result is stack garbage)",
                     (u16)(stackchk_hit_cs - LOADSEG), stackchk_hit_ip);
            diff_tag = "UNDEF"; report(idx, buf); diff_tag = "DIFF";
            n_undef++; bad = 0; undef = -1;
        }
    } else {
        state_drop();
        in_native = 0;
        use_native = saved_native;
        report(idx, native_halt_msg);
        bad = 1;
    }
    n_tests++;
    if (!bad && undef >= 0 && passed[idx] < 255) passed[idx]++;
    if (bad) { n_fail++; failed_fn[idx] = 1; }
    /* rewind: the caller runs the call interpreted, with events */
    memcpy(M, snap0, MEM_SIZE); R = r0; T = t0; dos_restore_pos(fpos, 64);
    memcpy(stk_dead, snapD, MEM_SIZE);
    dev_restore();
    difftest_active = 0;
    return 0;
}

void difftest_summary(void) {
    if (!difftest_on) return;
    const char *cov = getenv("DIFFTEST_COVERAGE");
    if (cov) {
        FILE *f = fopen(cov, "w");
        if (f) {
            for (int i = 0; i < native_count; i++)
                if (passed[i]) fprintf(f, "%04X:%04X %d\n", native_table[i].key >> 16, native_table[i].key & 0xFFFF, passed[i]);
            fclose(f);
        }
    }
    int covered = 0;
    for (int i = 0; i < native_count; i++) if (tested[i]) covered++;
    fprintf(stderr, "[difftest] %d tests, %d failed, %d aborted; %d/%d functions exercised\n",
            n_tests, n_fail, n_abort, covered, native_count);
    if (n_undef) fprintf(stderr, "[difftest] %d differing results not counted: the original read a never-written local (UNDEF lines)\n", n_undef);
}

/* ------------------------------------------------------------------------
 * Differential fuzzing: from the live game state, call decompiled functions
 * directly with argument vectors built from static call-site constants, far
 * pointers the game holds in DGROUP, and small boundary values.  Every call
 * is a full difftest (reference interpreter vs native), and the state is
 * restored afterwards so each function starts from the same snapshot.
 * Script command:  N fuzz <argsfile> <reps>
 */
#include <signal.h>
static sigjmp_buf alarm_jb __attribute__((unused));
#if defined(__GLIBC__) || defined(__APPLE__)
#include <execinfo.h>
#endif
static void on_segv(int sig) {
    (void)sig;
    if (in_native) difftest_native_halt("native run crashed (SIGSEGV, e.g. runaway recursion)");
    signal(SIGSEGV, SIG_DFL);
    raise(SIGSEGV);
}
static void on_alarm(int sig) {
    (void)sig;
    if (!in_native) return;
#if defined(__GLIBC__) || defined(__APPLE__)
    if (getenv("DIFFTEST_BT")) { void *bt[24]; int n = backtrace(bt, 24); backtrace_symbols_fd(bt, n, 2); }
#endif
    difftest_native_halt("native run took over 20x the reference time");
}

static u32 rng_state;
static u32 rnd(void) { rng_state = rng_state * 1103515245u + 12345u; return rng_state >> 8; }

void difftest_fuzz(const char *path, int reps) {
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); return; }
    /* far pointers held in DGROUP that point into program memory */
    u32 ptrs[512]; int np = 0;
    u16 dseg = LOADSEG + 0x39D4;
    u32 code_lo = (u32)LOADSEG << 4, code_hi = (u32)dseg << 4;
    u32 stk_lo = (u32)R.ss << 4, stk_hi = stk_lo + 0x2000;
    for (u32 o = 0; o + 4 <= 0xA700 && np < 512; o += 2) {
        u16 off = rw(dseg, o), seg = rw(dseg, o + 2);
        u32 l = lin(seg, off);
        if (seg >= LOADSEG && seg < 0xA000 && seg != dseg && off < 0xF000 &&
            !(l >= code_lo && l < code_hi) && !(l >= stk_lo && l < stk_hi))
            ptrs[np++] = ((u32)seg << 16) | off;
    }
    for (u32 o = 0x100; o < 0xA000 && np < 512; o += 0x400) ptrs[np++] = ((u32)dseg << 16) | o;
    signal(SIGALRM, on_alarm);
    {   /* SIGSEGV from host stack overflow needs its own stack */
        static char altstack[1 << 16];
        stack_t ss = { .ss_sp = altstack, .ss_size = sizeof altstack, .ss_flags = 0 };
        sigaltstack(&ss, NULL);
        struct sigaction sa = {0};
        sa.sa_handler = on_segv; sa.sa_flags = SA_ONSTACK | SA_NODEFER;
        sigaction(SIGSEGV, &sa, NULL);
    }
    u8 *base = malloc(MEM_SIZE);
    CPU rb0; u64 tb0; long fp0[64];
    char line[512];
    int nfun = 0, ncall = 0;
    int fails0 = n_fail;
    while (fgets(line, sizeof line, f)) {
        unsigned seg, off, nw, mask; char cs_[400] = "";
        if (sscanf(line, "%x:%x %u %x %399s", &seg, &off, &nw, &mask, cs_) < 4) continue;
        native_fn fn = native_lookup_any(LOADSEG + seg, off);
        if (!fn) continue;
        /* constants per argument word */
        u16 cv[16][8]; int cn[16] = {0};
        char *p = cs_;
        for (unsigned w = 0; w < nw && w < 16 && p && *p; w++) {
            char *end = strchr(p, ',');
            if (end) *end = 0;
            if (*p != '-') {
                char *q = p;
                while (q && *q && cn[w] < 8) { cv[w][cn[w]++] = (u16)strtoul(q, &q, 16); if (*q == '/') q++; else break; }
            }
            p = end ? end + 1 : NULL;
        }
        nfun++;
        for (int r = 0; r < reps; r++) {
            rng_state = (seg << 16 | off) * 2654435761u + r * 97;
            u16 words[16];
            for (unsigned w = 0; w < nw && w < 16; w++) {
                if (mask & (1u << w)) {
                    u32 fp = np ? ptrs[rnd() % np] : 0;
                    words[w] = (u16)fp; if (w + 1 < nw) words[++w] = fp >> 16;
                    continue;
                }
                static const u16 small[] = {0, 1, 2, 3, 4, 5, 7, 10, 0xFFFF, 20};
                if (cn[w] && (rnd() & 3)) words[w] = cv[w][rnd() % cn[w]];
                else words[w] = small[rnd() % 10];
            }
            memcpy(base, M, MEM_SIZE); rb0 = R; tb0 = T; dos_snapshot_pos(fp0, 64);
            for (int w = (int)nw - 1; w >= 0; w--) push16(words[w]);
            push16(R.cs); push16(0xFFFF);
            difftest_force = 1;
            alarm(5);              /* re-armed below once the reference time is known */
            difftest_call(LOADSEG + seg, off, fn, 1);
            alarm(0);
            difftest_force = 0;
            ncall++;
            memcpy(M, base, MEM_SIZE); R = rb0; T = tb0; dos_restore_pos(fp0, 64);
        }
    }
    fclose(f);
    free(base);
    fprintf(stderr, "[fuzz] %d functions, %d calls, %d new failures\n", nfun, ncall, n_fail - fails0);
}
