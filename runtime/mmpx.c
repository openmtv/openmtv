/* The MMPX display filter (--filter mmpx; presentation only: game state,
 * timing, scripts and recordings never depend on it).
 *
 * MMPX (McGuire & Gagiu 2021, "MMPX Style-Preserving Pixel Art
 * Magnification", JCGT 10(2)): 2x by rules over each pixel's neighbourhood,
 * applied twice for 4x.  It only ever repeats colours of the picture, so the
 * 32-colour art keeps its palette; edges and diagonals get smoother.
 * (HD packs -- AI-upscaled pictures for the sprites, carried by pixel tags --
 * were tried here and dropped for their artefacts: git tag hdpack-experiment.) */
#include <stdlib.h>
#include <string.h>
#include "runtime.h"

/* ---- MMPX ---- */

static inline u32 luma(u32 c) { return (c >> 16 & 255) + (c >> 8 & 255) + (c & 255); }

/* one 2x pass over the source rows [y0, y1) of a w x h picture given with a
 * 3-pixel border of repeated edge pixels (stride w + 6); writes rows 2*y0 ..
 * 2*y1 - 1 of the 2w x 2h result */
static void mmpx2_rows(const u32 *pad, int w, int y0, int y1, u32 *out) {
    int ps = w + 6;
    for (int y = y0; y < y1; y++) {
        u32 *o0 = out + (size_t)(2 * y) * (2 * w), *o1 = o0 + 2 * w;
        for (int x = 0; x < w; x++) {
            const u32 *c = pad + (size_t)(y + 3) * ps + (x + 3);
#define S(dx, dy) c[(dy) * ps + (dx)]
            u32 A = S(-1, -1), B = S(0, -1), C = S(1, -1), D = S(-1, 0), E = S(0, 0), F = S(1, 0);
            u32 G = S(-1, 1), H = S(0, 1), I = S(1, 1), P = S(0, -2), Q = S(-2, 0), Rr = S(2, 0), Ss = S(0, 2);
            u32 Bl = luma(B), Dl = luma(D), El = luma(E), Fl = luma(F), Hl = luma(H);
            u32 J = E, K = E, L = E, N = E;
            /* 1:1 slopes */
            if (D == B && D != H && D != F && (El >= Dl || E == A) && (E == A || E == C || E == G) &&
                (El < Dl || A != D || E != P || E != Q)) J = D;
            if (B == F && B != D && B != H && (El >= Bl || E == C) && (E == A || E == C || E == I) &&
                (El < Bl || C != B || E != P || E != Rr)) K = B;
            if (H == D && H != F && H != B && (El >= Hl || E == G) && (E == A || E == G || E == I) &&
                (El < Hl || G != H || E != Ss || E != Q)) L = H;
            if (F == H && F != B && F != D && (El >= Fl || E == I) && (E == C || E == G || E == I) &&
                (El < Fl || I != H || E != Rr || E != Ss)) N = F;
            /* intersections */
            if (E != F && E == C && E == I && E == D && E == Q && F == B && F == H && F != S(3, 0)) K = N = F;
            if (E != D && E == A && E == G && E == F && E == Rr && D == B && D == H && D != S(-3, 0)) J = L = D;
            if (E != H && E == G && E == I && E == B && E == P && H == D && H == F && H != S(0, 3)) L = N = H;
            if (E != B && E == A && E == C && E == H && E == Ss && B == D && B == F && B != S(0, -3)) J = K = B;
            if (Bl < El && E == G && E == H && E == I && E == Ss && E != A && E != D && E != C && E != F) J = K = B;
            if (Hl < El && E == A && E == B && E == C && E == P && E != D && E != G && E != I && E != F) L = N = H;
            if (Fl < El && E == A && E == D && E == G && E == Q && E != B && E != C && E != I && E != H) K = N = F;
            if (Dl < El && E == C && E == F && E == I && E == Rr && E != B && E != A && E != G && E != H) J = L = D;
            /* 2:1 slopes */
            if (H != B) {
                if (H != A && H != E && H != C) {
                    if (H == G && H == F && H == Rr && H != D && H != S(2, -1)) L = N;
                    if (H == I && H == D && H == Q && H != F && H != S(-2, -1)) N = L;
                }
                if (B != I && B != G && B != E) {
                    if (B == A && B == F && B == Rr && B != D && B != S(2, 1)) J = K;
                    if (B == C && B == D && B == Q && B != F && B != S(-2, 1)) K = J;
                }
            }
            if (F != D) {
                if (D != I && D != E && D != C) {
                    if (D == A && D == H && D == Ss && D != B && D != S(1, 2)) J = L;
                    if (D == G && D == B && D == P && D != H && D != S(1, -2)) L = J;
                }
                if (F != E && F != A && F != G) {
                    if (F == C && F == H && F == Ss && F != B && F != S(-1, 2)) K = N;
                    if (F == I && F == B && F == P && F != H && F != S(-1, -2)) N = K;
                }
            }
#undef S
            o0[2 * x] = J; o0[2 * x + 1] = K; o1[2 * x] = L; o1[2 * x + 1] = N;
        }
    }
}

/* the picture with a 3-pixel border of repeated edge pixels */
static void pad3(const u32 *src, int w, int h, u32 *pad) {
    int ps = w + 6;
    for (int y = -3; y < h + 3; y++) {
        const u32 *s = src + (size_t)(y < 0 ? 0 : y >= h ? h - 1 : y) * w;
        u32 *d = pad + (size_t)(y + 3) * ps;
        for (int i = 0; i < 3; i++) { d[i] = s[0]; d[w + 3 + i] = s[w - 1]; }
        memcpy(d + 3, s, sizeof *s * w);
    }
}

/* ... or of a picture that repeats (a tile laid side by side) */
static void pad3_wrap(const u32 *src, int w, int h, u32 *pad) {
    int ps = w + 6;
    for (int y = -3; y < h + 3; y++)
        for (int x = -3; x < w + 3; x++)
            pad[(size_t)(y + 3) * ps + (x + 3)] = src[(size_t)(((y % h) + h) % h) * w + ((x % w) + w) % w];
}

/* any picture, w x h -> 4w x 4h: the cells of a sprite (src/hr.c, --filter hr),
 * its edges repeated, or wrapped around for a tile (wrap) */
void mmpx_scale4(const u32 *src, int w, int h, int wrap, u32 *out) {
    u32 *pad1 = malloc(sizeof *pad1 * (size_t)(w + 6) * (h + 6)), *mid = malloc(sizeof *mid * (size_t)4 * w * h);
    u32 *pad2 = malloc(sizeof *pad2 * (size_t)(2 * w + 6) * (2 * h + 6));
    if (!pad1 || !mid || !pad2) { free(pad1); free(mid); free(pad2); return; }
    (wrap ? pad3_wrap : pad3)(src, w, h, pad1);
    mmpx2_rows(pad1, w, 0, h, mid);
    (wrap ? pad3_wrap : pad3)(mid, 2 * w, 2 * h, pad2);
    mmpx2_rows(pad2, 2 * w, 0, 2 * h, out);
    free(pad1); free(mid); free(pad2);
}

/* 320x200 -> 1280x800; only the rows that changed since the last call
 * (and the rows whose result depends on them) are computed again */
static void mmpx4(const u32 *src, u32 *out) {
    static u32 prev[320 * 200], pad1[326 * 206], mid[640 * 400], pad2[646 * 406];
    static int valid;
    int y0 = 0, y1 = 200;
    if (valid) {
        while (y0 < 200 && !memcmp(src + y0 * 320, prev + y0 * 320, 320 * 4)) y0++;
        if (y0 == 200) return;
        while (y1 > y0 && !memcmp(src + (y1 - 1) * 320, prev + (y1 - 1) * 320, 320 * 4)) y1--;
    }
    memcpy(prev, src, sizeof prev);
    valid = 1;
    int a = y0 - 3 < 0 ? 0 : y0 - 3, b = y1 + 3 > 200 ? 200 : y1 + 3;     /* rules look 3 pixels away */
    pad3(src, 320, 200, pad1);
    mmpx2_rows(pad1, 320, a, b, mid);
    int c = 2 * a - 3 < 0 ? 0 : 2 * a - 3, d = 2 * b + 3 > 400 ? 400 : 2 * b + 3;
    pad3(mid, 640, 400, pad2);
    mmpx2_rows(pad2, 640, c, d, out);
}

/* the 4x picture of a 320x200 frame (RGB) */
void mmpx_render(const u32 *rgb, u32 *out) {
    static u32 pic[1280 * 800];          /* kept: MMPX recomputes only the rows that changed */
    mmpx4(rgb, pic);
    memcpy(out, pic, sizeof pic);
}
