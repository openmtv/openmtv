/* Stage 1 of drawing at 4x resolution (docs/hochaufloesend.md): the survey.
 *
 * Linked only into build/mtv-dec-hrs (make build/mtv-dec-hrs): src/ is
 * compiled there with -DHR_SURVEY, so the hooks of src/hrs.h call in here,
 * and with -finstrument-functions, so a shadow stack knows the decompiled
 * functions that called a drawing helper.
 *
 * A shadow of every byte of the 1 MB (by linear address) plays the cells of
 * stage 2 without their sub-pixels: the value a helper (or the sprite
 * decoder, setmem, far_memcpy) last left there ("base"), whether that is
 * known, and where it came from (bank resource and sprite, fill, calloc,
 * the intro's decoders).  Copies carry it along as stage 2 will carry the
 * cells.  A byte is "valid" while its value still equals its base; whoever
 * writes past the hooks leaves an invalid byte behind, and stage 2 would
 * show the MMPX picture there.  At every frame the screen is checked.
 *
 * HR_SURVEY=FILE writes at exit (functions as run-time addresses; resolve
 * them with nm against "base", tools/hrsurvey.py does):
 *   base ADDR                 the run-time address of hrs_helper
 *   frames N
 *   draw RES SPR HELPER F1 F2 F3 BYTES CALLS    pixels read straight from a sprite bank
 *                             (F1 the helper's caller, F2 F3 the first callers above it
 *                             that are not drawing wrappers: draw_sprite_to, text_draw_*, ...)
 *   taint HELPER F1 F2 F3 KIND RES SPR SRC W8 H BYTES CALLS   a helper read invalid bytes
 *   memcpy F1 F2 F3 KIND RES BYTES             far_memcpy moved pixels
 *   bank RES DECODES
 *   screen KIND RES PIXELFRAMES                 valid pixels on the screen by origin
 *   invalid CLASS PIXELFRAMES FRAMES            1 = copied from an invalid source,
 *                                               2 = intro decoder, 3 = written past the helpers
 * and FILE.episodes: "episode FRAME COUNT X0 Y0 X1 Y1 C1 C2 C3" whenever the
 * invalid pixels of a frame (count, bounding box, per class) change, FILE.heat.pgm (invalid pixels over the run), FILE.snapNN_fFRAME.ppm
 * (screens with invalid pixels marked: red = class 1, blue = 2, magenta = 3;
 * one per new class and 16-pixel bounding box, at most HR_SNAPS, default 40). */
#define _GNU_SOURCE                     /* REG_RIP, REG_EFL */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <ucontext.h>
#include <sys/mman.h>
#include "game.h"
#include "hrs.h"

#define NOI __attribute__((no_instrument_function))

/* ---- the shadow stack ---- */
#define DEPTH 4096
static void *st_fn[DEPTH];
static int depth;
void __cyg_profile_func_enter(void *fn, void *site) NOI;
void __cyg_profile_func_exit(void *fn, void *site) NOI;
void __cyg_profile_func_enter(void *fn, void *site) {
    (void)site;
    if (depth < DEPTH) st_fn[depth] = fn;
    depth++;
}
void __cyg_profile_func_exit(void *fn, void *site) {
    (void)fn; (void)site;
    if (depth > 0) depth--;
}

/* ---- the shadow of every byte ---- */
enum { K_INIT, K_BANK, K_FILL, K_CALLOC, K_RAW };
#define O_RES(o)    ((o) & 0x3FF)
#define O_SPR(o)    (((o) >> 10) & 0xFF)
#define O_KIND(o)   (((o) >> 20) & 0xF)
#define O_INBANK    (1u << 24)          /* still in its bank (or a far_memcpy copy of it) */
#define O_TAINT     (1u << 25)          /* copied from an invalid byte */
#define O_MEMCPY    (1u << 26)          /* went through far_memcpy */
#define MK_O(kind, res, spr) ((u32)(res) | (u32)(spr) << 10 | (u32)(kind) << 20)
static u8 *SB, *SV;                     /* base value, base known */
static u32 *SO;                         /* origin */
static u16 *res_at;                     /* packed resource read to this address */
static int on;
static int taint_log;                   /* HR_TAINTLOG=N: the first N invalid reads on stderr */
static const char *out_path;

/* the current helper call */
static int cur_helper;
static u64 cur_call;
static void *cur_f1, *cur_f2, *cur_f3;
static u32 cur_src;
static u16 cur_w8, cur_h, cur_res;

/* ---- counters: (kind of line, key words, three functions) -> bytes, calls ---- */
typedef struct { u64 k1, k2; void *f1, *f2, *f3; u64 bytes, calls, last_call; int used; } Slot;
#define SLOTS (1u << 18)
static Slot *tab;
static NOI Slot *slot(u64 k1, u64 k2, void *f1, void *f2, void *f3) {
    u64 h = k1 * 0x9E3779B97F4A7C15ull ^ k2 * 0xC2B2AE3D27D4EB4Full ^ (u64)(uintptr_t)f1 * 31 ^ (u64)(uintptr_t)f2 * 131 ^ (u64)(uintptr_t)f3 * 1031;
    for (u32 i = (u32)(h >> 40) & (SLOTS - 1);; i = (i + 1) & (SLOTS - 1)) {
        Slot *s = &tab[i];
        if (!s->used) { s->used = 1; s->k1 = k1; s->k2 = k2; s->f1 = f1; s->f2 = f2; s->f3 = f3; return s; }
        if (s->k1 == k1 && s->k2 == k2 && s->f1 == f1 && s->f2 == f2 && s->f3 == f3) return s;
    }
}
static NOI void count(Slot *s, u64 n) {
    s->bytes += n;
    if (s->last_call != cur_call) { s->last_call = cur_call; s->calls++; }
}
/* line kinds in k1's top byte */
enum { L_DRAW = 1, L_TAINT, L_MEMCPY, L_BANK };

static u64 frames, comp[8][1024], inv_px[4], inv_frames[4];
static u32 *heat;

static NOI long lin_of(const u8 *p) {
    if (p >= M && p < M + MEM_SIZE) return p - M;
    if (p >= g_dgroup && p < g_dgroup + DG_END) return (long)DSLIN + (p - g_dgroup);
    for (int i = 0; i < HOST_REGIONS; i++)
        if (host_regions[i].host && p >= host_regions[i].host && p < host_regions[i].host + host_regions[i].len)
            return (long)host_regions[i].lin + (p - host_regions[i].host);
    return -1;
}

/* a helper read the byte at s (value v): bank statistics, invalid sources */
static Slot *last_draw, *last_taint;
static u64 last_draw_k, last_taint_k;
static NOI void reading(u32 s, u8 v) {
    u32 o = SO[s];
    if ((o & O_INBANK) && O_KIND(o) == K_BANK) {
        u64 k = (u64)L_DRAW << 56 | (u64)O_RES(o) << 16 | (u64)O_SPR(o) << 8 | (u64)cur_helper;
        if (!last_draw || last_draw_k != k) { last_draw = slot(k, 0, cur_f1, cur_f2, cur_f3); last_draw_k = k; }
        count(last_draw, 1);
    }
    if (!(SV[s] && SB[s] == v)) {
        if (taint_log > 0) {
            taint_log--;
            fprintf(stderr, "[hrs] frame %llu helper %d reads %05X = %02X: base %02X known %d origin %08X; src %05X %ux%u; fns %p %p %p\n",
                    (unsigned long long)frame_no(), cur_helper, s, v, SB[s], SV[s], o, cur_src, cur_w8 * 8, cur_h, cur_f1, cur_f2, cur_f3);
        }
        u64 k = (u64)L_TAINT << 56 | (u64)O_KIND(o) << 32 | (u64)O_RES(o) << 16 | (u64)O_SPR(o) << 8 | (u64)cur_helper;
        u64 k2 = (u64)cur_src << 32 | (u64)cur_w8 << 16 | cur_h;
        if (!last_taint || last_taint_k != (k ^ k2)) { last_taint = slot(k, k2, cur_f1, cur_f2, cur_f3); last_taint_k = k ^ k2; }
        count(last_taint, 1);
    }
}

/* the drawing wrappers between a screen's code and the helpers: skipped when
 * naming the callers (F1 is the immediate caller, F2 and F3 the first two
 * others above it) */
static void *const generic[] = {
    (void *)blit_transparent, (void *)blit_opaque_rect, (void *)blit_opaque, (void *)blit_opaque_to,
    (void *)bitmap_put, (void *)bitmap_copy, (void *)bitmap_put_screen, (void *)tile_draw_opaque,
    (void *)tile_draw_at_x, (void *)draw_sprite, (void *)draw_sprite_to, (void *)draw_atlas_tile,
    (void *)sprite_draw_target, (void *)ui_frame_tile, (void *)ui_frame_px, (void *)ui_frame, (void *)ui_tile_box_to,
    (void *)text_draw_2, (void *)text_draw_full, (void *)text_draw, (void *)text_draw_right_free, (void *)text_draw_free,
    (void *)text_draw_to, (void *)text_draw_wrap, (void *)text_draw_to_free, (void *)text_draw_center_free,
    (void *)text_draw_center, (void *)text_draw_center_to, (void *)text_draw_center_ex, (void *)text_draw_ex_free,
    (void *)text_draw_full_free, (void *)text_draw_wrap_free,
};
static NOI int is_generic(void *f) {
    for (unsigned i = 0; i < sizeof generic / sizeof generic[0]; i++) if (generic[i] == f) return 1;
    return 0;
}
/* the callers of the function on top of the stack: *f1 its caller, *f2 and *f3 the first non-wrappers above */
static NOI void callers(void **f1, void **f2, void **f3) {
    *f1 = *f2 = *f3 = NULL;
    int i = depth - 2;
    if (i >= 0 && i < DEPTH) *f1 = st_fn[i];
    for (i = depth - 2; i >= 0 && !*f3; i--) {
        if (i >= DEPTH || is_generic(st_fn[i])) continue;
        if (!*f2) *f2 = st_fn[i];
        else if (st_fn[i] != *f2) *f3 = st_fn[i];
    }
}

void hrs_helper(int id, u16 sseg, u16 soff, u16 w8, u16 h) {
    if (!on) return;
    cur_helper = id;
    cur_call++;
    callers(&cur_f1, &cur_f2, &cur_f3);
    cur_src = lin(sseg, soff);
    cur_w8 = w8; cur_h = h;
    last_draw = last_taint = NULL;
}

static NOI void carry(u32 s, u32 d, u8 v) {
    int ok = SV[s] && SB[s] == v;
    SV[d] = SV[s]; SB[d] = SB[s];
    SO[d] = (SO[s] & ~(O_INBANK | O_TAINT)) | (ok ? 0 : O_TAINT);
}
void hrs_copy(u16 sseg, u16 soff, u16 dseg, u16 doff, u8 v) {
    if (!on) return;
    u32 s = lin(sseg, soff), d = lin(dseg, doff);
    reading(s, v);
    carry(s, d, v);
}
void hrs_tput(u16 sseg, u16 soff, u16 dseg, u16 doff, u8 v) {
    if (!on) return;
    u32 s = lin(sseg, soff);
    reading(s, v);
    if (v) carry(s, lin(dseg, doff), v);
}
void hrs_fill(u16 dseg, u16 doff, u8 v) {
    if (!on) return;
    u32 d = lin(dseg, doff);
    SV[d] = 1; SB[d] = v; SO[d] = MK_O(K_FILL, 0, 0);
}
void hrs_raw(u16 dseg, u16 doff) {
    if (!on) return;
    u32 d = lin(dseg, doff);
    SV[d] = 0; SO[d] = MK_O(K_RAW, 0, 0);
}
void hrs_res_read(u16 id, const u8 *buf) {
    if (!on) return;
    long a = lin_of(buf);
    if (a >= 0) res_at[a] = id;
}
void hrs_bank_begin(u32 packed) {
    if (!on) return;
    cur_res = res_at[lin(FPSEG(packed), FPOFF(packed))];
    count(slot((u64)L_BANK << 56 | cur_res, 0, NULL, NULL, NULL), 1);
}
void hrs_bank_pixel(u32 dst, u8 v, u16 sprite) {
    if (!on) return;
    u32 d = lin(FPSEG(dst), FPOFF(dst));
    SV[d] = 1; SB[d] = v; SO[d] = MK_O(K_BANK, cur_res & 0x3FF, sprite & 0xFF) | O_INBANK;
}
void hrs_memcpy(u8 *dst, const u8 *src, u16 n) {
    if (!on) return;
    void *f1, *f2, *f3;
    callers(&f1, &f2, &f3);
    cur_call++;
    for (u16 i = 0; i < n; i++) {
        long s = lin_of(src + i), d = lin_of(dst + i);
        if (s < 0 || d < 0) continue;
        u32 o = SO[s];
        if (O_KIND(o) == K_BANK || O_KIND(o) == K_FILL)
            count(slot((u64)L_MEMCPY << 56 | (u64)O_KIND(o) << 16 | O_RES(o), 0, f1, f2, f3), 1);
        int ok = SV[s] && SB[s] == src[i];
        SV[d] = SV[s]; SB[d] = SB[s];
        SO[d] = (o & ~O_TAINT) | O_MEMCPY | (ok ? 0 : O_TAINT);
    }
}
void hrs_setmem(u32 p, u16 n, u8 v) {
    if (!on) return;
    for (u16 i = 0; i < n; i++) {
        u32 d = lin(FPSEG(p), (u16)(FPOFF(p) + i));
        SV[d] = 1; SB[d] = v; SO[d] = MK_O(K_CALLOC, 0, 0);
    }
}

/* ---- every frame: the screen ---- */
static int nsnap, max_snaps = 40;
static u32 seen[512];
static int nseen;
static u32 ep_last[8];

static NOI void snapshot(u64 f, const u8 *cls) {
    char path[1024];
    snprintf(path, sizeof path, "%s.snap%02d_f%llu.ppm", out_path, nsnap, (unsigned long long)f);
    FILE *o = fopen(path, "wb");
    if (!o) return;
    fprintf(o, "P6\n320 200\n255\n");
    for (int i = 0; i < 64000; i++) {
        u8 *c = vga_dac[M[0xA0000 + i]];
        u8 rgb[3] = { (u8)(c[0] * 255 / 63), (u8)(c[1] * 255 / 63), (u8)(c[2] * 255 / 63) };
        if (cls[i] == 1) { rgb[0] = 255; rgb[1] /= 3; rgb[2] /= 3; }
        else if (cls[i] == 2) { rgb[0] /= 3; rgb[1] /= 3; rgb[2] = 255; }
        else if (cls[i] == 3) { rgb[0] = 255; rgb[1] = 0; rgb[2] = 255; }
        fwrite(rgb, 1, 3, o);
    }
    fclose(o);
    nsnap++;
}

/* HR_WATCH=LO[-HI][@FRAME]: who writes the linear bytes LO..HI (hex) from frame
 * FRAME on -- the page is write-protected, a write faults, runs one instruction
 * with the trap flag and the page is protected again.  stderr "[watch]" lines:
 * frame, address, old -> new value, the instruction and the shadow stack
 * (resolve with nm against the "base" line). */
static u32 w_lo, w_hi;
static u64 w_from;
static u8 *w_page, w_old[4096];
static long w_psz;
static int w_armed;
static void *w_rip;
static NOI void w_segv(int sig, siginfo_t *si, void *vctx) {
    ucontext_t *uc = vctx;
    u8 *a = si->si_addr;
    if (!w_page || a < w_page || a >= w_page + w_psz) { signal(sig, SIG_DFL); return; }
    w_rip = (void *)uc->uc_mcontext.gregs[REG_RIP];
    for (u32 i = w_lo; i <= w_hi; i++) w_old[i - w_lo] = M[i];
    mprotect(w_page, w_psz, PROT_READ | PROT_WRITE);
    uc->uc_mcontext.gregs[REG_EFL] |= 0x100;
}
static NOI void w_trap(int sig, siginfo_t *si, void *vctx) {
    (void)sig; (void)si;
    ucontext_t *uc = vctx;
    uc->uc_mcontext.gregs[REG_EFL] &= ~0x100L;
    for (u32 i = w_lo; i <= w_hi; i++)
        if (M[i] != w_old[i - w_lo]) {
            fprintf(stderr, "[watch] frame %llu %05X %02X -> %02X rip %p stack", (unsigned long long)frame_no(), i, w_old[i - w_lo], M[i], w_rip);
            for (int k = 1; k <= 5 && depth - k >= 0; k++) fprintf(stderr, " %p", depth - k < DEPTH ? st_fn[depth - k] : NULL);
            fprintf(stderr, "\n");
        }
    mprotect(w_page, w_psz, PROT_READ);
}
static NOI void w_arm(void) {
    w_psz = sysconf(_SC_PAGESIZE);
    w_page = (u8 *)((uintptr_t)(M + w_lo) & ~(uintptr_t)(w_psz - 1));
    if ((u8 *)((uintptr_t)(M + w_hi) & ~(uintptr_t)(w_psz - 1)) != w_page || w_hi - w_lo >= 4096) {
        fprintf(stderr, "[watch] %05X-%05X must lie in one page\n", w_lo, w_hi);
        exit(1);
    }
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = w_segv; sigaction(SIGSEGV, &sa, NULL);
    sa.sa_sigaction = w_trap; sigaction(SIGTRAP, &sa, NULL);
    fprintf(stderr, "[watch] base %p, %05X-%05X from frame %llu\n", (void *)hrs_helper, w_lo, w_hi, (unsigned long long)w_from);
    mprotect(w_page, w_psz, PROT_READ);
    w_armed = 1;
}

extern void (*hrs_frame_hook)(void);
static NOI void hrs_frame(void) {
    if (w_hi && !w_armed && frame_no() >= w_from) w_arm();
    if (vga_mode != 0x13) return;
    static u8 cls[64000];
    u32 n[4] = {0};
    int x0 = 320, y0 = 200, x1 = -1, y1 = -1;
    frames++;
    for (int i = 0; i < 64000; i++) {
        u32 a = 0xA0000 + i;
        u8 m = M[a];
        u32 o = SO[a];
        if (SV[a] && SB[a] == m) {
            comp[O_KIND(o)][O_RES(o)]++;
            cls[i] = 0;
            continue;
        }
        int c = (o & O_TAINT) ? 1 : O_KIND(o) == K_RAW ? 2 : 3;
        cls[i] = (u8)c;
        n[c]++;
        heat[i]++;
        int x = i % 320, y = i / 320;
        if (x < x0) x0 = x;
        if (x > x1) x1 = x;
        if (y < y0) y0 = y;
        if (y > y1) y1 = y;
    }
    u32 total = n[1] + n[2] + n[3];
    for (int c = 1; c < 4; c++) { inv_px[c] += n[c]; if (n[c]) inv_frames[c]++; }
    u64 f = frame_no();
    u32 ep[8] = { total, (u32)x0, (u32)y0, (u32)x1, (u32)y1, n[1], n[2], n[3] };
    if (memcmp(ep, ep_last, sizeof ep)) {
        static FILE *eo;
        if (!eo) { char p[1024]; snprintf(p, sizeof p, "%s.episodes", out_path); eo = fopen(p, "w"); }
        if (eo) fprintf(eo, "episode %llu %u %d %d %d %d %u %u %u\n", (unsigned long long)f, total, x0, y0, x1, y1, n[1], n[2], n[3]);
        memcpy(ep_last, ep, sizeof ep);
    }
    if (!total || nsnap >= max_snaps) return;
    u32 key = (u32)(x0 / 16) | (u32)(y0 / 16) << 5 | (u32)(x1 / 16) << 10 | (u32)(y1 / 16) << 15
            | (n[1] ? 1u : 0) << 20 | (n[2] ? 1u : 0) << 21 | (n[3] ? 1u : 0) << 22;
    for (int i = 0; i < nseen; i++) if (seen[i] == key) return;
    if (nseen < 512) seen[nseen++] = key;
    snapshot(f, cls);
}

static NOI void write_out(void) {
    FILE *o = fopen(out_path, "w");
    if (!o) { perror(out_path); return; }
    fprintf(o, "base %p\nframes %llu\n", (void *)hrs_helper, (unsigned long long)frames);
    for (u32 i = 0; i < SLOTS; i++) {
        Slot *s = &tab[i];
        if (!s->used) continue;
        int kind = (int)(s->k1 >> 56);
        if (kind == L_DRAW)
            fprintf(o, "draw %u %u %u %p %p %p %llu %llu\n", (unsigned)(s->k1 >> 16 & 0xFFFF), (unsigned)(s->k1 >> 8 & 0xFF),
                    (unsigned)(s->k1 & 0xFF), s->f1, s->f2, s->f3, (unsigned long long)s->bytes, (unsigned long long)s->calls);
        else if (kind == L_TAINT)
            fprintf(o, "taint %u %p %p %p %u %u %u %05X %u %u %llu %llu\n", (unsigned)(s->k1 & 0xFF), s->f1, s->f2, s->f3,
                    (unsigned)(s->k1 >> 32 & 0xFF), (unsigned)(s->k1 >> 16 & 0xFFFF), (unsigned)(s->k1 >> 8 & 0xFF),
                    (unsigned)(s->k2 >> 32), (unsigned)(s->k2 >> 16 & 0xFFFF), (unsigned)(s->k2 & 0xFFFF),
                    (unsigned long long)s->bytes, (unsigned long long)s->calls);
        else if (kind == L_MEMCPY)
            fprintf(o, "memcpy %p %p %p %u %u %llu\n", s->f1, s->f2, s->f3, (unsigned)(s->k1 >> 16 & 0xFF),
                    (unsigned)(s->k1 & 0xFFFF), (unsigned long long)s->bytes);
        else if (kind == L_BANK)
            fprintf(o, "bank %u %llu\n", (unsigned)(s->k1 & 0xFFFF), (unsigned long long)s->bytes);
    }
    for (int k = 0; k < 8; k++)
        for (int r = 0; r < 1024; r++)
            if (comp[k][r]) fprintf(o, "screen %d %d %llu\n", k, r, (unsigned long long)comp[k][r]);
    for (int c = 1; c < 4; c++)
        fprintf(o, "invalid %d %llu %llu\n", c, (unsigned long long)inv_px[c], (unsigned long long)inv_frames[c]);
    fclose(o);
    char p[1024];
    snprintf(p, sizeof p, "%s.heat.pgm", out_path);
    FILE *h = fopen(p, "wb");
    if (h) {
        u32 mx = 1;
        for (int i = 0; i < 64000; i++) if (heat[i] > mx) mx = heat[i];
        fprintf(h, "P5\n320 200\n255\n");
        for (int i = 0; i < 64000; i++) { u8 v = heat[i] ? (u8)(64 + 191ull * heat[i] / mx) : 0; fputc(v, h); }
        fclose(h);
    }
}

__attribute__((constructor)) static NOI void hrs_init(void) {
    out_path = getenv("HR_SURVEY");
    if (!out_path || !*out_path) return;
    if (getenv("HR_SNAPS")) max_snaps = atoi(getenv("HR_SNAPS"));
    if (getenv("HR_TAINTLOG")) taint_log = atoi(getenv("HR_TAINTLOG"));
    if (getenv("HR_WATCH")) {
        const char *e = getenv("HR_WATCH");
        char *p;
        w_lo = w_hi = (u32)strtoul(e, &p, 16);
        if (*p == '-') w_hi = (u32)strtoul(p + 1, &p, 16);
        if (*p == '@') w_from = strtoull(p + 1, &p, 10);
    }
    SB = calloc(MEM_SIZE, 1); SV = malloc(MEM_SIZE); SO = calloc(MEM_SIZE, sizeof *SO);
    res_at = calloc(MEM_SIZE, sizeof *res_at); tab = calloc(SLOTS, sizeof *tab); heat = calloc(64000, sizeof *heat);
    if (!SB || !SV || !SO || !res_at || !tab || !heat) { fprintf(stderr, "[hrs] out of memory\n"); exit(1); }
    memset(SV, 1, MEM_SIZE);            /* every cell starts flat 0, like the memory */
    on = 1;
    hrs_frame_hook = hrs_frame;
    atexit(write_out);
}
