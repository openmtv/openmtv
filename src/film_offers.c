/* More film offers (--film-offers=N, settings [game] film_offers = N, 10..17): the
 * film dealer offers N films and series instead of the original's 10 (a game
 * option, fixes.h; ROADMAP Phase 6, "More film agency offers").
 *
 * The original's counter (ui_draw_dealer_shelf 26A7:0FE5): 10 tapes in a row,
 * smaller towards the back right -- each slot a sprite of its own (resource
 * 155, sprites 1..10, 17 x 36 down to 14 x 26, the room's g_room_gfx3) and a
 * rectangle of its own (g_filmshelf_rects DS:032E: slot 1 81,66-97,101 ...
 * slot 10 159,60-172,85), drawn from 10 down to 1, so each tape covers its
 * right neighbour's dark side face.  From 11 the row grows to the left, one
 * tape per offer (the layout chosen with the user, the mock-up
 * https://claude.ai/artifact/1bmf7kkbbJ3q1znnRBhBEM): new tape j (1..7,
 * counted from slot 1) has tape 1's shape scaled to 36 + j rows and the spine
 * width, label lines and colour mark of another original tape, a different
 * one each; it stands where its right edge ends just before its
 * neighbour's label face, its bottom on the row's line extended.  Built once
 * from the decoded bank -- the rules and the arithmetic of the mock-up's
 * generator (Python's round, Pillow's nearest scaling), so the picture is
 * the mock-up's pixel for pixel -- and drawn here, into the screen and into
 * Mr. Jabble's animation buffer (whose rectangle x 16..87, y 18..70 the tops
 * of tapes 11..15 reach), not through the game's sprite bank: no new heap
 * block (the shelf's background bitmap grows from 112 x 44 to 176 x 54),
 * and every seat of a network game draws the same.
 *
 * The slots 11..N themselves: g_dealer_extra behind FILM_OFFERS_SEG
 * (game_types.h dealer_at), their rectangles g_filmshelf_extra behind
 * FILM_RECTS_SEG (filmshelf_rect_at), both host regions (src/state.c). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "hr.h"
#include "fixes.h"

u16 film_offers_count(void) { return difftest_on ? 10 : (u16)FILM_OFFERS_OPT(); }   /* (main.c takes 10..17 only) */

/* the far pointer to slot si's shelf rectangle (x1, y1, x2, y2) */
u32 filmshelf_rect_at(u16 si) {
    return FILM_OFFER_EXTRA(si) ? MK32(FILM_RECTS_SEG, (u16)((si - 11) * 8)) : MK32(DSEG, (u16)((si << 3) + 0x32E));
}

/* ---- the new tapes ---- */
#define NEW_TAPES (FILM_OFFERS_MAX - 10)
#define TMAX 64                          /* (the biggest new tape: 20-odd x 43) */
#define LINE 2                           /* the label's lines */
#define FACE 4                           /* the label face */
#define ROOM_LEFT 4                      /* the room's picture: x 4.., y ..112 */
#define ROOM_BOTTOM 112
static const u8 orig_w[10] = { 17, 16, 19, 17, 15, 16, 16, 18, 16, 14 };   /* resource 155's tapes (its packed header; */
static const u8 orig_h[10] = { 36, 35, 33, 33, 32, 31, 30, 29, 27, 26 };   /*  the decoded bank keeps only w / 8) */
static const u8 lend[9] = { 2, 5, 3, 7, 1, 6, 4, 8, 9 };   /* which original tape (0-based) lends its lines and mark to new tape 1, 2, ... */

typedef struct { int w, h; u8 px[TMAX][TMAX]; } Grid;
typedef struct { int ready, x, y1, w, h; u8 px[TMAX][TMAX]; } Tape;
static Tape tapes[NEW_TAPES];
static int tapes_state;                  /* 0 not built, 1 built, -1 the bank is not the original's */

static int fixed_colour(u8 v) { return v == 0 || v == 1 || v == 2 || v == 4 || v == 31; }
static int pyround(double v) { return (int)nearbyint(v); }   /* Python's round: half to even */

/* original tape k (0-based) out of the decoded bank */
static int orig_tape(u32 bank, int k, Grid *g) {
    u32 hdr = MK32(FPSEG(bank), (u16)(FPOFF(bank) + 2 + k * 8));
    u16 wcols = FP16(hdr, 0), h = FP16(hdr, 2);
    u32 pix = FP32(hdr, 4);
    if (wcols != (orig_w[k] + 7) / 8 || h != orig_h[k]) return 0;
    g->w = orig_w[k]; g->h = orig_h[k];
    for (int r = 0; r < g->h; r++)
        for (int c = 0; c < g->w; c++) g->px[r][c] = FP8(pix, (u16)(r * wcols * 8 + c));
    return 1;
}

/* the label face: the columns of FACE/LINE in the middle row, the rows from the
 * first full one to the last of that run (not the rows below the mark) */
static int label_box(const Grid *g, int *L, int *R, int *T, int *B) {
    const u8 *mid = g->px[g->h / 2];
    *L = -1; *R = -1;
    for (int c = 0; c < g->w; c++)
        if (mid[c] == FACE || mid[c] == LINE) { if (*L < 0) *L = c; *R = c; }
    if (*L < 0) return 0;                /* (no label: not the original's tape) */
    int full[TMAX];
    for (int r = 0; r < g->h; r++) {
        full[r] = 1;
        for (int c = *L; c <= *R; c++) if (g->px[r][c] != FACE && g->px[r][c] != LINE) { full[r] = 0; break; }
    }
    *T = 0;
    while (*T < g->h && !full[*T]) (*T)++;
    *B = *T;
    while (*B + 1 < g->h && full[*B + 1]) (*B)++;
    return *T < g->h;
}

static int marks(const Grid *g, u8 *out) {   /* the colours besides the fixed ones, ascending */
    int seen[256] = { 0 }, n = 0;
    for (int r = 0; r < g->h; r++) for (int c = 0; c < g->w; c++) if (!fixed_colour(g->px[r][c])) seen[g->px[r][c]] = 1;
    for (int v = 0; v < 256; v++) if (seen[v]) out[n++] = (u8)v;
    return n;
}

/* Pillow's nearest scaling (ImagingScaleAffine): the source index of each target index */
static void scale_tab(int src, int dst, int *tab) {
    double a = (double)src / dst, xo = a * 0.5;
    for (int i = 0; i < dst; i++) { tab[i] = xo < 0.0 ? -1 : (int)xo; xo += a; }
}

/* new tape j (1..) with the lines and mark of original tape k; its edge column (side | label) */
static int make_tape(const Grid *t1, const Grid *tk, int j, Grid *g, int *edge) {
    int H = t1->h + j, W = pyround((double)(t1->w * H) / t1->h);
    u8 m0[256], mk[256], map[256];
    int n0 = marks(t1, m0), nk = marks(tk, mk);
    for (int v = 0; v < 256; v++) map[v] = (u8)v;
    for (int i = 0; i < n0 && i < nk; i++) map[m0[i]] = mk[i];
    int xt[TMAX], yt[TMAX];
    if (W > TMAX || H > TMAX) return 0;
    scale_tab(t1->w, W, xt); scale_tab(t1->h, H, yt);
    Grid s;
    s.w = W; s.h = H;
    for (int r = 0; r < H; r++) for (int c = 0; c < W; c++) s.px[r][c] = map[t1->px[yt[r]][xt[c]]];
    int L, R, T, B, Lk, Rk, Tk, Bk;
    if (!label_box(&s, &L, &R, &T, &B) || !label_box(tk, &Lk, &Rk, &Tk, &Bk) || L < 1) return 0;
    /* the spine as thick as tape k's (one more for the biggest), the columns right of it scaled to fit */
    int lw = Rk - Lk + 1 + (H >= 40 ? 1 : 0);
    int old = W - L, neu = lw + (W - 1 - R);
    if (neu < 2 || L + neu > TMAX) return 0;
    g->w = L + neu; g->h = H;
    for (int r = 0; r < H; r++) {
        for (int c = 0; c < L; c++) g->px[r][c] = s.px[r][c];
        for (int i = 0; i < neu; i++) g->px[r][L + i] = s.px[r][L + pyround((double)(i * (old - 1)) / (neu - 1))];
    }
    R = L + lw - 1;
    for (int r = T; r <= B; r++)
        for (int c = L; c <= R; c++) if (g->px[r][c] == FACE || g->px[r][c] == LINE) g->px[r][c] = FACE;
    /* tape k's lines, scaled to this label: they end at least 2 rows above its end (the mark) */
    int n = B - T + 1, nkr = Bk - Tk + 1;
    for (int c0 = Lk; c0 <= Rk; c0++) {
        int r = Tk;
        while (r <= Bk) {
            if (tk->px[r][c0] == LINE) {
                int st = r;
                while (r <= Bk && tk->px[r][c0] == LINE) r++;
                int en = r - 1, c = L + (c0 - Lk);
                if (c < R) {
                    int a = T + pyround((double)((st - Tk) * n) / nkr);
                    int zz = pyround((double)((Bk - en) * n) / nkr) + 1;
                    int z = B - (zz > 2 ? zz : 2);
                    int last = a + 1 > z ? a + 1 : z;
                    for (int rr = a; rr <= last && rr < H; rr++) g->px[rr][c] = LINE;
                }
            }
            r++;
        }
    }
    *edge = L - 1;
    return 1;
}

/* new tape j (1..) out of the ten original tapes, orig[k] the w x h pixels of resource 155's
 * sprite k (rows w apart, the sizes orig_w, orig_h): for src/ki_make.c, which makes the AI
 * pictures without a game; its pixels into px (rows *w apart, TMAX x TMAX at most) */
int filmoffers_tape(const u8 *const orig[10], int j, u8 *px, int *w, int *h) {
    static Grid g[10], t;
    int edge;
    if (j < 1 || j > NEW_TAPES) return 0;
    for (int k = 0; k < 10; k++) {
        g[k].w = orig_w[k]; g[k].h = orig_h[k];
        for (int r = 0; r < g[k].h; r++) memcpy(g[k].px[r], orig[k] + r * g[k].w, (size_t)g[k].w);
    }
    if (!make_tape(&g[0], &g[lend[j - 1]], j, &t, &edge)) return 0;
    *w = t.w; *h = t.h;
    for (int r = 0; r < t.h; r++) memcpy(px + r * t.w, t.px[r], (size_t)t.w);
    return 1;
}

/* the tapes and their rectangles, out of the room's bank (once: the same for every seat).
 * Called where the counter is drawn on entering the agency (ui_draw_dealer_shelf mode 0),
 * when g_room_gfx3 surely is resource 155 -- other rooms keep other bitmaps there; the
 * click, the tip, the hotspot and the animation only use what is built */
void filmoffers_prepare(void) {
    if (g_nfilmoffers <= 10 || tapes_state != 0) return;
    u32 bank = g_room_gfx3;
    static Grid orig[10];
    if (!bank || FP16(bank, 0) < 10) { tapes_state = -1; fprintf(stderr, "--film-offers: resource 155 is not the original's, no more tapes\n"); return; }
    for (int k = 0; k < 10; k++)
        if (!orig_tape(bank, k, &orig[k])) { tapes_state = -1; fprintf(stderr, "--film-offers: resource 155 is not the original's, no more tapes\n"); return; }
    int L, R, T, B;
    if (!label_box(&orig[0], &L, &R, &T, &B)) { tapes_state = -1; fprintf(stderr, "--film-offers: resource 155 is not the original's, no more tapes\n"); return; }
    int x_r = 81, edge_r = L - 1;        /* slot 1 and its side/label edge (column 9) */
    for (int j = 1; j <= NEW_TAPES; j++) {
        static Grid g;
        int edge;
        if (!make_tape(&orig[0], &orig[lend[j - 1]], j, &g, &edge)) { tapes_state = -1; fprintf(stderr, "--film-offers: tape %d not made\n", j); return; }
        int x = x_r + edge_r - g.w;
        int y2 = pyround(101 + 0.205 * (81 - x));   /* the row's bottom line, extended */
        tapes[j - 1].x = x; tapes[j - 1].y1 = y2 - g.h + 1;
        tapes[j - 1].w = g.w; tapes[j - 1].h = g.h;
        memcpy(tapes[j - 1].px, g.px, sizeof g.px);
        tapes[j - 1].ready = 1;
        u16 *rc = &g_filmshelf_extra[(j - 1) * 4];   /* the rectangle: the tape, inside the room's picture */
        rc[0] = (u16)(x < ROOM_LEFT ? ROOM_LEFT : x); rc[1] = (u16)tapes[j - 1].y1;
        rc[2] = (u16)(x + g.w - 1); rc[3] = (u16)(y2 > ROOM_BOTTOM ? ROOM_BOTTOM : y2);
        x_r = x; edge_r = edge;
    }
    tapes_state = 1;
    const char *dump = getenv("FILMOFFERS_DUMP");   /* (a test: tools/hrai.py makes the same tapes) */
    FILE *f = dump && *dump ? fopen(dump, "w") : NULL;
    for (int j = 0; f && j < NEW_TAPES; j++) {
        fprintf(f, "tape %d %d %d\n", j + 1, tapes[j].w, tapes[j].h);
        for (int r = 0; r < tapes[j].h; r++) {
            for (int c = 0; c < tapes[j].w; c++) fprintf(f, "%s%d", c ? " " : "", tapes[j].px[r][c]);
            fputc('\n', f);
        }
    }
    if (f) fclose(f);
}

/* slot si (11..) holds an offer that stands on the counter */
static int on_counter(u16 si) { return (i8)dealer(si).shelf >= 0 && dealer(si).program != 0; }

/* the new tapes on the counter into bitmap bmp (header: u16 width / 8, u16 height, far pointer
 * to the pixels), which shows the screen from (ox, oy): right to left, each over its right
 * neighbour, after the original's ten; colour 0 is clear */
void filmoffers_draw(u32 bmp, int ox, int oy) {
    if (g_nfilmoffers <= 10 || tapes_state != 1 || !bmp) return;
    int bw = FP16(bmp, 0) << 3, bh = FP16(bmp, 2);
    u32 pix = FP32(bmp, 4);
    for (u16 si = 11; si <= g_nfilmoffers; si++) {
        if (!on_counter(si)) continue;
        const Tape *t = &tapes[si - 11];
        const void *pic = hr_ki_picture(155, (u16)(100 + si - 10), &t->px[0][0], t->w, t->h, (int)sizeof t->px[0]);
        for (int r = 0; r < t->h; r++) {
            int y = t->y1 + r, by = y - oy;
            if (y > ROOM_BOTTOM || by < 0 || by >= bh) continue;
            for (int c = 0; c < t->w; c++) {
                int x = t->x + c, bx = x - ox;
                u8 v = t->px[r][c];
                if (!v || x < ROOM_LEFT || bx < 0 || bx >= bw) continue;
                hr_ki_px(pic, c, r, FPSEG(pix), (u16)(FPOFF(pix) + by * bw + bx), v);
                wb(FPSEG(pix), (u16)(FPOFF(pix) + by * bw + bx), v);
            }
        }
    }
}

/* the new slot whose tape is under the cursor, the front (left) one first; 0 for none */
u16 filmoffer_at_cursor(void) {
    if (g_nfilmoffers <= 10 || tapes_state != 1) return 0;
    for (u16 si = g_nfilmoffers; si >= 11; si--) {
        u32 rc = filmshelf_rect_at(si);
        if (on_counter(si) && mouse_in_rect(FP16(rc, 0), FP16(rc, 2), FP16(rc, 4), FP16(rc, 6))) return si;
    }
    return 0;
}

/* the dealer's hotspot 0x18 (a fresh film given back, 05FF:0359) is (81,61)-(173,97) for the
 * carried tape's corner; with more tapes it reaches as far left as the row */
u16 filmoffer_hotspot(void) {
    if (g_nfilmoffers <= 10 || tapes_state != 1 || g_room != ROOM_FILM_AGENCY || (g_room_mode != 1 && g_room_mode != 5)) return 0;
    int x = tapes[g_nfilmoffers - 11].x;
    if (mouse_in_rect((u16)(x < ROOM_LEFT ? ROOM_LEFT : x), 61, 80, 97)) return 0x18;
    return 0;
}

/* the background ui_draw_dealer_shelf saves and puts back: the original's 112 x 44 at
 * (72, 59); with more tapes from x 8 (bitmap_put_screen takes x / 8) to 183, y 59..112 */
void filmoffers_shelf_bg(u16 *x, u16 *w, u16 *h) {
    if (g_nfilmoffers > 10) { *x = 8; *w = 0xB0; *h = 0x36; }
    else { *x = 0x48; *w = 0x70; *h = 0x2C; }
}

/* the slots past 10 empty and off the counter (a new program; a savegame without them):
 * the dealer's next refill fills them -- the new game's first (new_day), the computer
 * players' after a purchase, a human leaving the agency, the night's.  Not at once when
 * loading: a refill takes films off the market, and the new game's setup still draws its
 * starting programmes from the market (newgame_starting_programs; with several humans its
 * draws found no free cultural film any more and never ended) */
void filmoffers_clear(u16 from) {
    for (u16 si = from < 11 ? 11 : from; si <= FILM_OFFERS_MAX; si++) {
        u8 *o = g_dealer_extra + (si - 11) * 8;
        memset(o, 0, 8);
        o[7] = 0x9C;                     /* shelf -100: not on the counter until the refill fills it */
    }
}

/* offers of a savegame past this game's slots: back to the market, as the night's cleanup
 * gives back what was not sold (market_day_cleanup 1DA8:1227) -- else they would stay off it */
void filmoffers_return_to_market(const u8 *slot) {
    u16 program = (u16)(slot[0] | slot[1] << 8);
    if (!program || (i8)slot[7] < 0) return;   /* (an empty one; one bought: it is someone's now) */
    if ((i16)program >= 1000) {
        if (program - 1000 >= 1 && program - 1000 <= g_nseries) series_flags(program + 0xFC18) = series_flags(program + 0xFC18) | 0x20;
    } else if (program <= g_nfilms) films(program).flags_16 = films(program).flags_16 | 0x20;   /* (a bigger mod's: not here) */
}
