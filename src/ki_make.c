/* AI pictures made on the player's machine (docs/ki-bilder.md, stage 3): what
 * tools/hrai.py export, a model run and tools/hrai.py import do, in one, without
 * Python -- for the launcher's button.
 *
 *   mtv-dec --make-ki MODEL.onnx --game DIR --ki-out SET.ki [--ki-palette PAL]
 *           [--ki-model NAME] [--ki-ort LIBRARY] [--ki-threads N] [--ki-gpu]
 *           [--ki-cache DIR] [--ki-parent PID] [--ki-dump DIR] [--ki-from DIR] [--ki-limit N] [--ki-dump-out DIR]
 *
 * The game's own files (SOURCE1.RSC, SOURCE2.RSC in DIR) give the pictures; the
 * model (an ESRGAN as ONNX, 4x or 8x) runs in ONNX Runtime, a library loaded here
 * at run time (dlopen / LoadLibrary: the game itself needs none of it); PAL the
 * model's 223 extra colours (tools/hrai.py palette; without it the 31 of the
 * palette).  The set's format: the comment in src/hr.c.  Lines on stdout tell
 * how far it is ("ki N of M (P of Q)": pictures, and the model's pixels for a
 * time left).  --ki-gpu: on macOS CoreML (the graphics chip
 * and the Neural Engine; else, or where it fails, the CPU), --ki-cache DIR where
 * it keeps the compiled model (else $TMPDIR, anew each run).  --ki-parent PID: the
 * launcher that started it -- once it has ended (killed, crashed: it could not stop
 * this), the making ends too, before the next picture.  Tests: --ki-dump
 * DIR writes the model's input pictures as PPM (NAME.ppm, NAME_m.ppm: tools/
 * hrai.py export's names), --ki-dump-out DIR its output, --ki-from DIR reads
 * the output from PPM files there instead of running a model, --ki-limit N
 * runs the model on the first N pictures only (and writes no set).
 *
 * Everything as tools/hrai.py does it -- the same pictures, the same order, the
 * same arithmetic (float where numpy computes in float32, CIELAB in double) --
 * so the set is the same: the rooms as one picture of both halves, the groups
 * (pictures shown together, runtime/ki_parts.txt) as one each, every other
 * picture sprite (rule mmpx), the film dealer's new tapes, each animation part
 * (runtime/ki_parts.txt, else the pixel match in its bank) on what lies beneath
 * it, the frames of a run kept still against the previous and the first, the
 * ring around a sharp sprite that always lies on a picture kept where flat. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "fixes.h"
#include "hr_data.h"
#include "../third_party/onnxruntime/onnxruntime_c_api.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

#define PAD 8                     /* border of repeated edge pixels for the model */
#define EXT 223                   /* extra colours: indices 32..254 */
#define KEEP 7.0f                 /* a sub-pixel this close (CIELAB) to its original pixel's colour keeps it */
#define STILL 2                   /* radius of the neighbourhood that must be unchanged */
#define SAME 3.0f                 /* a part's sub-pixel this close to the picture without it shows that picture */
#define MARGIN 8                  /* pixels around a part in its pictures for the model */
#define MAXLAYERS 16
#define KI_GROUP 16               /* sprites in a group at most (tools/hrdata.py checks) */

static void die(const char *fmt, const char *a) { fprintf(stderr, "--make-ki: "); fprintf(stderr, fmt, a); fputc('\n', stderr); exit(1); }
/* (no memory: a reason for the launcher, not a crash) */
static void *xmalloc(size_t n) { void *p = malloc(n ? n : 1); if (!p) die("out of memory%s", ""); return p; }
static void *xcalloc(size_t n, size_t k) { void *p = calloc(n ? n : 1, k ? k : 1); if (!p) die("out of memory%s", ""); return p; }
static void *xrealloc(void *q, size_t n) { void *p = realloc(q, n ? n : 1); if (!p) die("out of memory%s", ""); return p; }

/* ---- the game's files (tools/rsc.py) ---- */
typedef struct { const u8 *data; u32 size; } Res;
static Res res_tab[600];
static u8 *rsc_file[2];
static u8 game_pal[256][3];

static void rsc_open(const char *dir) {
    const char *names[2] = { "SOURCE1.RSC", "SOURCE2.RSC" };
    for (int f = 0; f < 2; f++) {
        char path[4096];
        snprintf(path, sizeof path, "%s/%s", dir, names[f]);
        FILE *fp = fopen(path, "rb");
        if (!fp) die("no %s", path);
        fseek(fp, 0, SEEK_END);
        long n = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        rsc_file[f] = xmalloc((size_t)n);
        if (!rsc_file[f] || fread(rsc_file[f], 1, (size_t)n, fp) != (size_t)n) die("%s: not read", path);
        fclose(fp);
        if (n < 3600) die("%s: no index", path);
        for (int i = 0; i < 600; i++) {
            const u8 *e = rsc_file[f] + 6 * i;
            u32 off = (u32)e[0] | (u32)e[1] << 8 | (u32)e[2] << 16 | (u32)e[3] << 24, size = (u32)e[4] | (u32)e[5] << 8;
            if (off && (u64)off + size <= (u64)n) { res_tab[i].data = rsc_file[f] + off; res_tab[i].size = size; }
        }
    }
    const Res *p = &res_tab[1];
    if (!p->data || p->size < 2) die("%s: no palette", dir);
    int n = p->data[0] | p->data[1] << 8;
    for (int i = 0; i < 256; i++)
        for (int c = 0; c < 3; c++)
            game_pal[i][c] = i < n && 2u + 3u * i + c < p->size ? (u8)(p->data[2 + 3 * i + c] * 255 / 63) : 0;
}

/* sprite banks: order-1 context Huffman (37B4:04AC, tools/rsc.py decode_bank) */
typedef struct { int w, h; u8 *px; } Spr;
typedef struct { int n; Spr *s; } Bank;
static Bank banks[600];
static int bank_tried[600];

typedef struct { const u8 *d; u32 n, p; int cur, left, bad; } Bits;
static int bit(Bits *b) {
    if (b->left) { b->cur >>= 1; b->left--; }
    else { if (b->p >= b->n) { b->bad = 1; return 0; } b->cur = b->d[b->p++]; b->left = 7; }
    return b->cur & 1;
}
static int byte8(Bits *b) { int v = 0; for (int i = 0; i < 8; i++) v = bit(b) << 7 | v >> 1; return v; }
static int *tl, *tr, next_id, max_nodes;
static int tree_rec(Bits *b, int *first, int depth) {
    if (b->bad || depth > 4000) { b->bad = 1; return 0; }
    if (bit(b)) {
        if (*first) { *first = 0; return 0x100; }
        return byte8(b);
    }
    int id = next_id++;
    if (id - 0x101 >= max_nodes) { b->bad = 1; return 0; }
    int l = tree_rec(b, first, depth + 1), r = tree_rec(b, first, depth + 1);
    tl[id - 0x101] = l; tr[id - 0x101] = r;
    return id;
}
static int tree(Bits *b) { int first = 1; return tree_rec(b, &first, 0); }
static int walk(Bits *b, int si) {
    while (si > 0x100 && !b->bad) si = bit(b) ? tr[si - 0x101] : tl[si - 0x101];
    return si;
}

static const Bank *bank(int res) {
    if (res < 0 || res >= 600) return NULL;
    if (bank_tried[res]) return banks[res].n ? &banks[res] : NULL;
    bank_tried[res] = 1;
    const Res *r = &res_tab[res];
    if (!r->data || r->size < 7) return NULL;
    const u8 *d = r->data;
    int n = d[0];
    if (!n || 1u + 2u * n + 6 > r->size) return NULL;
    u32 pos = 1 + 2 * n, total = 0;
    for (int i = 0; i < n; i++) total += (u32)d[1 + 2 * i] * d[2 + 2 * i];
    max_nodes = d[pos] << 8 | d[pos + 1];
    u32 count = (u32)d[pos + 2] << 24 | (u32)d[pos + 3] << 16 | (u32)d[pos + 4] << 8 | d[pos + 5];
    if (count != total) return NULL;
    Bits b = { d, r->size, pos + 6, 0, 0, 0 };
    tl = xcalloc((size_t)max_nodes + 1, sizeof *tl); tr = xcalloc((size_t)max_nodes + 1, sizeof *tr);
    next_id = 0x101;
    int esc = tree(&b), ctx[256];
    for (int i = 0; i < 256 && !b.bad; i++) ctx[i] = tree(&b);
    u8 *out = xmalloc(total ? total : 1);
    u32 o = 0;
    if (!b.bad && total) {
        int si = walk(&b, esc), v = si == 0x100 ? byte8(&b) : si;
        out[o++] = (u8)v;
        while (o < total && !b.bad) {
            si = walk(&b, ctx[v]);
            if (si == 0x100) { si = walk(&b, esc); v = si == 0x100 ? byte8(&b) : si; }
            else v = si;
            out[o++] = (u8)v;
        }
    }
    free(tl); free(tr);
    if (b.bad || o != total) { free(out); return NULL; }
    banks[res].n = n;
    banks[res].s = xcalloc((size_t)n, sizeof *banks[res].s);
    for (int i = 0, p = 0; i < n; i++) {
        Spr *s = &banks[res].s[i];
        s->w = d[1 + 2 * i]; s->h = d[2 + 2 * i];
        s->px = out + p;
        p += s->w * s->h;
    }
    return &banks[res];
}

/* ---- the sprites taking part ---- */
typedef struct { int res, k, x, y; } Ref;
typedef struct {
    int res, k, w, h;
    const u8 *px;                 /* the original's pixels (w x h) */
    int own;                      /* its picture for the model */
    int px0, py0;                 /* its place in that picture */
    float *L;                     /* its upscaled colours (CIELAB, 4h x 4w x 3) */
    u8 *m;                        /* its mask (4h x 4w) */
    u8 *q;                        /* its cells (palette indices, 4h x 4w) */
    int has_chain;                /* a part on what lies beneath it */
    int root_room, root_res, root_k, nlayers, cx, cy, win[4];
    Ref layers[MAXLAYERS];
    int ctx, und;                 /* its window's pictures (with it, without it) */
    float *Lc, *Lu;
    int nrefs;                    /* a frame: the frames its still parts take */
    int fin;                      /* finish: 1 under way, 2 done */
    Ref refs[2];
    int part_done, in_sprites;
    int grp;                      /* one of a group's: no chain, no frame refs of its own */
} Sp;
static Sp *sp_tab[600][256];
static Sp **sp_list;
static int sp_n, sp_cap;
static Sp *sp_get(int res, int k) { return res >= 0 && res < 600 && k >= 0 && k < 256 ? sp_tab[res][k] : NULL; }
static Sp *sp_add(int res, int k, int w, int h, const u8 *px) {
    Sp *s = xcalloc(1, sizeof *s);
    s->res = res; s->k = k; s->w = w; s->h = h; s->px = px; s->own = -1; s->ctx = s->und = -1;
    sp_tab[res][k] = s;
    if (sp_n == sp_cap) { sp_cap = sp_cap ? 2 * sp_cap : 256; sp_list = xrealloc(sp_list, sizeof *sp_list * (size_t)sp_cap); }
    sp_list[sp_n++] = s;
    return s;
}
static u8 tape_px[7][64 * 64];
static int tape_w[7], tape_h[7], tapes_made;
/* the pixels of a sprite (tapes 101..107 of bank 155 too) */
static const u8 *pix_of(int res, int k, int *w, int *h) {
    if (res == 155 && k > 100 && k <= 107 && tapes_made) { *w = tape_w[k - 101]; *h = tape_h[k - 101]; return tape_px[k - 101]; }
    const Bank *b = bank(res);
    if (!b || k < 0 || k >= b->n) return NULL;
    *w = b->s[k].w; *h = b->s[k].h;
    return b->s[k].px;
}

static int raw_rule(int res, int k) {
    for (int i = 0; i < hr_rule_count; i++)
        if (hr_rules[i].res == res && k >= hr_rules[i].first && k <= hr_rules[i].last) return hr_rules[i].rule;
    return HR_MMPX;
}
static int want(int res, int k) {
    int w, h;
    const u8 *p = pix_of(res, k, &w, &h);
    if (!p || !w || !h || w > 255 || h > 255 || raw_rule(res, k) != HR_MMPX) return 0;
    for (int i = 0; i < w * h; i++) if (p[i]) return 1;
    return 0;
}
static const KiPart *part_of(int res, int k) {
    for (int lo = 0, hi = ki_part_count; lo < hi;) {
        int m = (lo + hi) / 2;
        const KiPart *p = &ki_parts[m];
        if (p->res == res && p->k == k) return p;
        if (p->res < res || (p->res == res && p->k < k)) lo = m + 1; else hi = m;
    }
    return NULL;
}

/* where a and b (w x h, rows sa and sb apart) agree, a is ink, in the whole (2r+1)^2 neighbourhood */
static void still(const u8 *a, int sa, const u8 *b, int sb, int w, int h, int r, u8 *out) {
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int ok = 1;
            for (int dy = -r; dy <= r && ok; dy++)
                for (int dx = -r; dx <= r && ok; dx++) {
                    int yy = y + dy, xx = x + dx;
                    if (yy < 0 || yy >= h || xx < 0 || xx >= w) { ok = 0; break; }
                    u8 v = a[yy * sa + xx];
                    ok = v && v == b[yy * sb + xx];
                }
            out[y * w + x] = (u8)ok;
        }
}

/* (base k, x, y) when sprite k of bank res sits on a larger sprite of its bank: the best
 * pixel match (half its ink the same), an eighth of its ink unchanged with the 3x3 around */
static int find_base(int res, int k, int *kb_out, int *x_out, int *y_out) {
    Sp *s = sp_get(res, k);
    int h = s->h, w = s->w, ink = 0;
    for (int i = 0; i < w * h; i++) ink += s->px[i] != 0;
    if (ink < 16) return 0;
    long best = -1;
    int bk = -1, bx = 0, by = 0;
    const Bank *b = bank(res);
    for (int kb = 0; kb < b->n; kb++) {
        Sp *o = sp_get(res, kb);
        if (!o || !o->in_sprites || kb == k) continue;
        int H = o->h, W = o->w;
        if (w > W || h > H || (w == W && h == H) || W * H < 400) continue;
        long top = -1; int tx = 0, ty = 0;
        for (int y = 0; y + h <= H; y++)
            for (int x = 0; x + w <= W; x++) {
                long sc = 0;
                for (int yy = 0; yy < h; yy++) {
                    const u8 *pa = s->px + yy * w, *pb = o->px + (y + yy) * W + x;
                    for (int xx = 0; xx < w; xx++) sc += pa[xx] && pa[xx] == pb[xx];
                }
                if (sc > top) { top = sc; tx = x; ty = y; }
            }
        if (top >= 0.5 * ink && top > best) { best = top; bk = kb; bx = tx; by = ty; }
    }
    if (bk < 0) return 0;
    Sp *o = sp_get(res, bk);
    u8 *st = xmalloc((size_t)w * h);
    still(s->px, w, o->px + by * o->w + bx, o->w, w, h, 1, st);
    int n = 0;
    for (int i = 0; i < w * h; i++) n += st[i];
    free(st);
    if (n < ink / 8.0) return 0;
    *kb_out = bk; *x_out = bx; *y_out = by;
    return 1;
}

/* ---- the pictures for the model ---- */
typedef struct {
    char name[32];
    int w, h, mask;               /* its size (no border); 1: it has a mask picture */
    int nparts;
    Ref parts[KI_GROUP];          /* own pictures: the sprites in it, drawn in this order (a room: both halves) */
    Sp *of;                       /* ctx / und: the part */
    int kind;                     /* 0 own, 1 ctx, 2 und */
    u8 *rgb, *mrgb;               /* the model's 4x pictures, border cut off (4h x 4w x 3) */
} Job;
static Job *jobs;
static int njobs, jobs_cap;
static Job *job_add(const char *name, int w, int h, int mask, int kind) {
    if (njobs == jobs_cap) { jobs_cap = jobs_cap ? 2 * jobs_cap : 512; jobs = xrealloc(jobs, sizeof *jobs * (size_t)jobs_cap); }
    Job *j = &jobs[njobs++];
    memset(j, 0, sizeof *j);
    snprintf(j->name, sizeof j->name, "%s", name);
    j->w = w; j->h = h; j->mask = mask; j->kind = kind;
    return j;
}

/* a room's picture: both halves side by side */
static u8 *room_pix(int n, int *w, int *h) {
    int wl, hl, wr, hr;
    const u8 *l = pix_of(n, 0, &wl, &hl), *r = pix_of(n - 50, 0, &wr, &hr);
    *w = 160 + wr; *h = 109;
    u8 *a = xcalloc((size_t)*w * *h, 1);
    for (int y = 0; y < 109; y++) {
        memcpy(a + y * *w, l + y * wl, (size_t)wl);
        memcpy(a + y * *w + 160, r + y * wr, (size_t)wr);
    }
    return a;
}
static u8 *root_pix(const Sp *s, int *w, int *h) {
    if (s->root_room) return room_pix(s->root_room, w, h);
    const u8 *p = pix_of(s->root_res, s->root_k, w, h);
    u8 *a = xmalloc((size_t)*w * *h);
    memcpy(a, p, (size_t)*w * *h);
    return a;
}
/* the root's pixels with the layers (and the part itself if with) over it, colour 0 clear */
static u8 *layered(const Sp *s, int with, int *W, int *H) {
    u8 *a = root_pix(s, W, H);
    for (int i = 0; i < s->nlayers + with; i++) {
        Ref l = i < s->nlayers ? s->layers[i] : (Ref){ s->res, s->k, s->cx, s->cy };
        int w, h;
        const u8 *o = pix_of(l.res, l.k, &w, &h);
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                int X = l.x + x, Y = l.y + y;
                if (X >= 0 && Y >= 0 && X < *W && Y < *H && o[y * w + x]) a[Y * *W + X] = o[y * w + x];
            }
    }
    return a;
}

/* the chain of a part: root, the layers beneath it, its place (tools/hrai.py chain) */
static int fb_k[600][256], fb_x[600][256], fb_y[600][256];
static u8 fb_has[600][256];
typedef struct { int room, res, k, nl, x, y; Ref layers[MAXLAYERS]; } Chain;
/* the base of sprite res:k: runtime/ki_parts.txt, else (not listed there) the pixel match */
static int base_of(int res, int k, int *kind, int *a, int *b, int *x, int *y) {
    const KiPart *p = part_of(res, k);
    if (p) {
        if (!p->kind || p->kind == 3) return 0;     /* (3: one of a group's) */
        *kind = p->kind; *a = p->a; *b = p->b; *x = p->x; *y = p->y;
        return 1;
    }
    if (!fb_has[res][k]) return 0;
    *kind = 2; *a = res; *b = fb_k[res][k]; *x = fb_x[res][k]; *y = fb_y[res][k];
    return 1;
}
/* 0 where tools/hrai.py's chain() gives None: no base, or the sprite met again */
static int chain_rec(int res, int k, int *seen, int ns, Chain *c) {
    int kind, a, b, x, y;
    if (!base_of(res, k, &kind, &a, &b, &x, &y)) return 0;
    for (int i = 0; i < ns; i++) if (seen[i] == (res << 9 | k)) return 0;
    if (kind == 1) { c->room = a; c->res = c->k = 0; c->nl = 0; c->x = x; c->y = y; return 1; }
    if (ns >= 64) return 0;
    seen[ns] = res << 9 | k;
    Chain up;
    if (!chain_rec(a, b, seen, ns + 1, &up)) { c->room = 0; c->res = a; c->k = b; c->nl = 0; c->x = x; c->y = y; return 1; }
    *c = up;
    if (c->nl >= MAXLAYERS) return 0;
    c->layers[c->nl++] = (Ref){ a, b, up.x, up.y };
    c->x = up.x + x; c->y = up.y + y;
    return 1;
}
/* a room's two halves as the room jobs take them (109 rows, the left at most 160 wide) */
static int room_ok(int n) {
    const Bank *l = bank(n), *r = n >= 50 ? bank(n - 50) : NULL;
    return l && r && l->n >= 1 && r->n >= 1 && l->s[0].h == 109 && r->s[0].h == 109 && l->s[0].w <= 160 && r->s[0].w <= 160;
}
static int chain_top(Sp *s) {
    int seen[64], w, h;
    Chain c;
    if (!chain_rec(s->res, s->k, seen, 0, &c)) return 0;
    /* (runtime/ki_parts.txt is made from the original's files: game files that differ --
       a room or a sprite it names missing or of another size -- leave the part alone) */
    if (c.room ? !room_ok(c.room) : !pix_of(c.res, c.k, &w, &h)) return 0;
    for (int i = 0; i < c.nl; i++) if (!pix_of(c.layers[i].res, c.layers[i].k, &w, &h)) return 0;
    s->root_room = c.room; s->root_res = c.res; s->root_k = c.k;
    s->nlayers = c.nl;
    memcpy(s->layers, c.layers, sizeof c.layers);
    s->cx = c.x; s->cy = c.y;
    return 1;
}

/* ---- the model's picture: transparent pixels filled from their neighbours ---- */
static void fill_rgb(const u8 *ix, int w, int h, float *rgb) {
    u8 *have = xmalloc((size_t)w * h), *nw = xmalloc((size_t)w * h);
    float *s = xmalloc(sizeof *s * 3 * (size_t)w * h), *n = xmalloc(sizeof *n * (size_t)w * h);
    int any = 0, all = 1;
    for (int i = 0; i < w * h; i++) {
        for (int c = 0; c < 3; c++) rgb[3 * i + c] = game_pal[ix[i]][c];
        have[i] = ix[i] != 0;
        any |= have[i]; all &= have[i];
    }
    while (any && !all) {
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                int i = y * w + x;
                float sv[3] = { 0, 0, 0 }, cn = 0;
                /* up + down + left + right, as tools/hrai.py fill sums them */
                const int nb[4][2] = { { 0, -1 }, { 0, 1 }, { -1, 0 }, { 1, 0 } };
                for (int t = 0; t < 4; t++) {
                    int xx = x + nb[t][0], yy = y + nb[t][1];
                    float hv = xx >= 0 && yy >= 0 && xx < w && yy < h && have[yy * w + xx] ? 1.0f : 0.0f;
                    for (int c = 0; c < 3; c++) {
                        float v = xx >= 0 && yy >= 0 && xx < w && yy < h ? rgb[3 * (yy * w + xx) + c] * hv : 0.0f;
                        sv[c] = t ? sv[c] + v : v;
                    }
                    cn = t ? cn + hv : hv;
                }
                for (int c = 0; c < 3; c++) s[3 * i + c] = sv[c];
                n[i] = cn;
                nw[i] = !have[i] && cn > 0;
            }
        all = 1;
        for (int i = 0; i < w * h; i++) {
            if (nw[i]) { for (int c = 0; c < 3; c++) rgb[3 * i + c] = s[3 * i + c] / n[i]; have[i] = 1; }
            all &= have[i];
        }
    }
    free(have); free(nw); free(s); free(n);
}
/* a picture for the model: (h + 2 PAD) x (w + 2 PAD) RGB, the border its edge repeated */
static u8 *model_input(const u8 *ix, int w, int h, int *W, int *H) {
    float *rgb = xmalloc(sizeof *rgb * 3 * (size_t)w * h);
    fill_rgb(ix, w, h, rgb);
    *W = w + 2 * PAD; *H = h + 2 * PAD;
    u8 *out = xmalloc((size_t)3 * *W * *H);
    for (int y = 0; y < *H; y++)
        for (int x = 0; x < *W; x++) {
            int sx = x - PAD < 0 ? 0 : x - PAD >= w ? w - 1 : x - PAD, sy = y - PAD < 0 ? 0 : y - PAD >= h ? h - 1 : y - PAD;
            for (int c = 0; c < 3; c++) {
                float v = rgb[3 * (sy * w + sx) + c] + 0.5f;
                out[3 * (y * *W + x) + c] = (u8)(v < 0 ? 0 : v > 255 ? 255 : v);
            }
        }
    free(rgb);
    return out;
}
static u8 *mask_input(const u8 *ix, int w, int h) {
    int W = w + 2 * PAD, H = h + 2 * PAD;
    u8 *out = xcalloc((size_t)3 * W * H, 1);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            if (ix[y * w + x]) memset(out + 3 * ((y + PAD) * W + x + PAD), 255, 3);
    return out;
}

static void write_ppm(const char *dir, const char *name, const u8 *rgb, int w, int h) {
    char path[4096];
    snprintf(path, sizeof path, "%s/%s.ppm", dir, name);
    FILE *f = fopen(path, "wb");
    if (!f) die("cannot write %s", path);
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    fwrite(rgb, 1, (size_t)3 * w * h, f);
    fclose(f);
}
static u8 *read_ppm(const char *dir, const char *name, int *w, int *h) {
    char path[4096];
    snprintf(path, sizeof path, "%s/%s.ppm", dir, name);
    FILE *f = fopen(path, "rb");
    if (!f) die("no %s", path);
    int mx;
    if (fscanf(f, "P6 %d %d %d", w, h, &mx) != 3 || mx != 255 || *w <= 0 || *h <= 0 || *w > 8192 || *h > 8192 || fgetc(f) == EOF)
        die("%s: not a PPM", path);
    u8 *p = xmalloc((size_t)3 * *w * *h);
    if (fread(p, 1, (size_t)3 * *w * *h, f) != (size_t)3 * *w * *h) die("%s: cut short", path);
    fclose(f);
    return p;
}

/* ---- ONNX Runtime, loaded at run time ---- */
static const OrtApi *ort;
static OrtEnv *ort_env;
static OrtSession *ort_sess;
static OrtMemoryInfo *ort_mem;
static void ort_check(OrtStatus *st, const char *what) {
    if (!st) return;
    fprintf(stderr, "--make-ki: ONNX Runtime, %s: %s\n", what, ort->GetErrorMessage(st));
    ort->ReleaseStatus(st);
    exit(1);
}
static void ort_open(const char *lib, const char *model, int threads, int gpu, const char *cache) {
    typedef const OrtApiBase *(ORT_API_CALL *GetBase)(void);
    GetBase get = NULL;
#ifdef _WIN32
    wchar_t wl[4096], wm[4096];     /* (the paths as argv has them: the ANSI code page, as fopen takes them) */
    if (!MultiByteToWideChar(CP_ACP, 0, lib ? lib : "onnxruntime.dll", -1, wl, 4096)) die("a path too long: %s", lib);
    HMODULE h = LoadLibraryW(wl);
    if (h) get = (GetBase)(void *)GetProcAddress(h, "OrtGetApiBase");
#else
    const char *names[] = { lib,
#ifdef __APPLE__
                            "libonnxruntime.dylib", "libonnxruntime.1.dylib",
#else
                            "libonnxruntime.so.1", "libonnxruntime.so",
#endif
                            NULL };
    static char why[600];
    for (int i = lib ? 0 : 1; names[i] && !get; i++) {     /* (--ki-ort LIB: that one only) */
        void *h = dlopen(names[i], RTLD_NOW | RTLD_LOCAL);
        if (h) get = (GetBase)dlsym(h, "OrtGetApiBase");
        else if (lib) { snprintf(why, sizeof why, "%s", dlerror()); break; }
    }
    if (!get && lib) die("no ONNX Runtime library (%s)", why[0] ? why : lib);
#endif
    if (!get) die("no ONNX Runtime library (%s)", lib ? lib : "--ki-ort LIBRARY");
    ort = get()->GetApi(ORT_API_VERSION);
    if (!ort) die("the ONNX Runtime library is older than %s", "1.31");
    ort_check(ort->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "openmtv", &ort_env), "environment");
    OrtSessionOptions *so;
    ort_check(ort->CreateSessionOptions(&so), "options");
    if (threads > 0) ort_check(ort->SetIntraOpNumThreads(so, threads), "threads");
    ort_check(ort->SetSessionGraphOptimizationLevel(so, ORT_ENABLE_ALL), "optimization");
    if (gpu) {
#ifdef __APPLE__
        /* (a cache directory: the compiled model kept there and used again -- without one CoreML
           leaves it in $TMPDIR, some 130 MB a run, also when the run is cancelled) */
        const char *keys[] = { "ModelFormat", "MLComputeUnits", "ModelCacheDirectory" }, *vals[] = { "MLProgram", "ALL", cache };
        OrtStatus *st = ort->SessionOptionsAppendExecutionProvider(so, "CoreML", keys, vals, cache ? 3 : 2);
        if (st) { fprintf(stderr, "[ki] CoreML: %s -- the CPU instead\n", ort->GetErrorMessage(st)); ort->ReleaseStatus(st); }
        else fprintf(stderr, "[ki] CoreML\n");
#else
        fprintf(stderr, "[ki] --ki-gpu: none here yet, the CPU\n");
#endif
    }
#ifdef _WIN32
    if (!MultiByteToWideChar(CP_ACP, 0, model, -1, wm, 4096)) die("a path too long: %s", model);
    OrtStatus *made = ort->CreateSession(ort_env, wm, so, &ort_sess);
#else
    OrtStatus *made = ort->CreateSession(ort_env, model, so, &ort_sess);
#endif
    ort->ReleaseSessionOptions(so);
    if (made && gpu) {                  /* CoreML could not take the model: the CPU */
        fprintf(stderr, "[ki] CoreML: %s -- the CPU instead\n", ort->GetErrorMessage(made));
        ort->ReleaseStatus(made);
        ort_check(ort->CreateSessionOptions(&so), "options");
        if (threads > 0) ort_check(ort->SetIntraOpNumThreads(so, threads), "threads");
        ort_check(ort->SetSessionGraphOptimizationLevel(so, ORT_ENABLE_ALL), "optimization");
#ifdef _WIN32
        made = ort->CreateSession(ort_env, wm, so, &ort_sess);
#else
        made = ort->CreateSession(ort_env, model, so, &ort_sess);
#endif
        ort->ReleaseSessionOptions(so);
    }
    ort_check(made, model);
    ort_check(ort->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &ort_mem), "memory");
}
/* the model over a W x H RGB picture: 4W x 4H RGB (an 8x model's 2x2 averaged), as tools/hrai_up.py */
static u8 *ort_run(const u8 *rgb, int W, int H) {
    size_t n = (size_t)W * H;
    float *in = xmalloc(sizeof *in * 3 * n);
    for (size_t i = 0; i < n; i++)
        for (int c = 0; c < 3; c++) in[c * n + i] = (float)rgb[3 * i + c] / 255.0f;
    int64_t shape[4] = { 1, 3, H, W };
    OrtValue *iv = NULL, *ov = NULL;
    ort_check(ort->CreateTensorWithDataAsOrtValue(ort_mem, in, sizeof *in * 3 * n, shape, 4, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &iv), "input");
    const char *in_names[] = { "input" }, *out_names[] = { "output" };
    ort_check(ort->Run(ort_sess, NULL, in_names, (const OrtValue *const *)&iv, 1, out_names, 1, &ov), "run");
    float *y;
    ort_check(ort->GetTensorMutableData(ov, (void **)&y), "output");
    OrtTensorTypeAndShapeInfo *info;
    ort_check(ort->GetTensorTypeAndShape(ov, &info), "output shape");
    int64_t d[4] = { 0, 0, 0, 0 };
    size_t nd = 0;
    ort_check(ort->GetDimensionsCount(info, &nd), "output shape");
    if (nd == 4) ort_check(ort->GetDimensions(info, d, 4), "output shape");
    ort->ReleaseTensorTypeAndShapeInfo(info);
    int k = nd == 4 && d[1] == 3 && d[2] % H == 0 && d[3] == d[2] / H * W ? (int)(d[2] / H) : 0;
    if (k != 4 && k != 8) die("the model's output is not 4 or 8 times its input (%s)", "an ESRGAN model?");
    size_t on = (size_t)d[2] * d[3];
    u8 *out = xmalloc((size_t)3 * 16 * n);
    int OW = 4 * W, OH = 4 * H;
    for (int yy = 0; yy < OH; yy++)
        for (int xx = 0; xx < OW; xx++)
            for (int c = 0; c < 3; c++) {
                float v;
                if (k == 4) {
                    v = y[c * on + (size_t)yy * OW + xx];
                    v = v < 0 ? 0 : v > 1 ? 1 : v;
                } else {
                    size_t rw = (size_t)d[3];
                    float a = y[c * on + (size_t)(2 * yy) * rw + 2 * xx], b = y[c * on + (size_t)(2 * yy) * rw + 2 * xx + 1];
                    float e = y[c * on + (size_t)(2 * yy + 1) * rw + 2 * xx], f = y[c * on + (size_t)(2 * yy + 1) * rw + 2 * xx + 1];
                    a = a < 0 ? 0 : a > 1 ? 1 : a; b = b < 0 ? 0 : b > 1 ? 1 : b;
                    e = e < 0 ? 0 : e > 1 ? 1 : e; f = f < 0 ? 0 : f > 1 ? 1 : f;
                    v = (a + b + e + f) / 4.0f;
                }
                out[3 * ((size_t)yy * OW + xx) + c] = (u8)(v * 255.0f + 0.5f);
            }
    ort->ReleaseValue(ov);
    ort->ReleaseValue(iv);
    free(in);
    return out;
}

/* ---- colours ---- */
static void lab_of(const u8 *rgb, float *L) {
    double c[3];
    for (int i = 0; i < 3; i++) {
        double v = rgb[i] / 255.0;
        c[i] = v > 0.04045 ? pow((v + 0.055) / 1.055, 2.4) : v / 12.92;
    }
    double x = (c[0] * 0.4124 + c[1] * 0.3576 + c[2] * 0.1805) / 0.95047;
    double y = (c[0] * 0.2126 + c[1] * 0.7152 + c[2] * 0.0722) / 1.0;
    double z = (c[0] * 0.0193 + c[1] * 0.1192 + c[2] * 0.9505) / 1.08883;
    double f[3] = { x, y, z };
    for (int i = 0; i < 3; i++) f[i] = f[i] > 216.0 / 24389 ? cbrt(f[i]) : (24389.0 / 27 * f[i] + 16) / 116;
    L[0] = (float)(116 * f[1] - 16);
    L[1] = (float)(500 * (f[0] - f[1]));
    L[2] = (float)(200 * (f[1] - f[2]));
}
static float pal_L[256][3];       /* the game's palette, CIELAB */
static float cen[31 + EXT][3];    /* the colours a sub-pixel may take */
static u8 cen_id[31 + EXT];
static int ncen;
static u8 *near_cache;            /* RGB -> index, 0: not yet */
static u8 nearest_of(const u8 *rgb, const float *L) {
    u32 key = (u32)rgb[0] << 16 | (u32)rgb[1] << 8 | rgb[2];
    if (near_cache[key]) return near_cache[key];
    int best = 0;
    float bd = 0;
    for (int i = 0; i < ncen; i++) {
        float a = L[0] - cen[i][0], b = L[1] - cen[i][1], c = L[2] - cen[i][2];
        float d = a * a + b * b;
        d = d + c * c;
        if (!i || d < bd) { bd = d; best = i; }
    }
    return near_cache[key] = cen_id[best];
}
static int keeps(const float *L, u8 o) {
    if (!o || o >= 32) return 0;
    float a = L[0] - pal_L[o][0], b = L[1] - pal_L[o][1], c = L[2] - pal_L[o][2];
    float d = a * a + b * b;
    d = d + c * c;
    return d < KEEP * KEEP;
}

/* ---- the launcher that started it (--ki-parent) ---- */
#ifdef _WIN32
static HANDLE parent_h;                 /* (held from the start: its number is not given to another meanwhile) */
#else
static long parent_pid;
#endif
static void parent_watch(long pid) {
#ifdef _WIN32
    if (!(parent_h = OpenProcess(SYNCHRONIZE, FALSE, (DWORD)pid))) die("the launcher (%s) has ended", "--ki-parent");
#else
    parent_pid = pid;
#endif
}
static void parent_check(void) {
#ifdef _WIN32
    if (parent_h && WaitForSingleObject(parent_h, 0) == WAIT_OBJECT_0) die("the launcher (%s) has ended", "--ki-parent");
#else
    if (parent_pid && (long)getppid() != parent_pid) die("the launcher (%s) has ended", "--ki-parent");   /* (an orphan gets another parent) */
#endif
}

/* ---- the set ---- */
static size_t rle(const u8 *d, size_t n, u8 *out) {
    size_t o = 0, i = 0, ls = 0, ln = 0;    /* the literal run: start, length */
    while (i < n) {
        size_t j = i;
        while (j < n && j - i < 129 && d[j] == d[i]) j++;
        if (j - i >= 2) {
            while (ln) { size_t t = ln < 128 ? ln : 128; out[o++] = (u8)(t - 1); memcpy(out + o, d + ls, t); o += t; ls += t; ln -= t; }
            out[o++] = (u8)(j - i + 126); out[o++] = d[i];
            i = j;
        } else {
            if (!ln) ls = i;
            ln++; i++;
        }
    }
    while (ln) { size_t t = ln < 128 ? ln : 128; out[o++] = (u8)(t - 1); memcpy(out + o, d + ls, t); o += t; ls += t; ln -= t; }
    return o;
}
static void put32(u8 *p, u32 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24); }
static void put16(u8 *p, u32 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); }
static int sp_cmp(const void *a, const void *b) {
    const Sp *x = *(Sp *const *)a, *y = *(Sp *const *)b;
    return x->res != y->res ? x->res - y->res : x->k - y->k;
}

/* ---- the parts on what lies beneath them (tools/hrai.py do_import, part) ---- */
static u8 *final_of(int res, int k, int *w, int *h) {
    Sp *s = sp_get(res, k);
    if (s && s->q) { *w = 4 * s->w; *h = 4 * s->h; return s->q; }
    int pw, ph;
    const u8 *p = pix_of(res, k, &pw, &ph);
    static u8 *tmp[8]; static int ti;
    u8 *a = tmp[ti] = xrealloc(tmp[ti], (size_t)16 * pw * ph);
    ti = (ti + 1) % 8;
    for (int y = 0; y < 4 * ph; y++) for (int x = 0; x < 4 * pw; x++) a[y * 4 * pw + x] = p[(y / 4) * pw + x / 4];
    *w = 4 * pw; *h = 4 * ph;
    return a;
}
static void part(Sp *s);
static void part_of_layers(Sp *s) {
    for (int i = 0; i < s->nlayers; i++) {
        Sp *l = sp_get(s->layers[i].res, s->layers[i].k);
        if (l && l->has_chain) part(l);
    }
}
static u8 quant1(const float *L, const u8 *rgb, u8 o) { return keeps(L, o) ? o : nearest_of(rgb, L); }
/* a frame: its still parts from its references (final by then) */
static void frame(Sp *s) {
    u8 *st = xmalloc((size_t)s->w * s->h);
    for (int r = 0; r < s->nrefs; r++) {
        Sp *f = sp_get(s->refs[r].res, s->refs[r].k);
        still(s->px, s->w, f->px + s->refs[r].y * f->w + s->refs[r].x, f->w, s->w, s->h, STILL, st);
        for (int y = 0; y < 4 * s->h; y++)
            for (int x = 0; x < 4 * s->w; x++)
                if (st[(y / 4) * s->w + x / 4]) s->q[(size_t)y * 4 * s->w + x] = f->q[(size_t)(4 * s->refs[r].y + y) * 4 * f->w + 4 * s->refs[r].x + x];
    }
    free(st);
}
/* around the sharp sprites that always lie on it (the TV's button frames): the ring just
 * outside keeps the original's colour where that is flat beside what they cover (no edge
 * beneath them spread into it) */
static void ring(Sp *s) {
    u8 *cov = NULL;                     /* 1: covered, 2: beside a cover */
    for (int i = 0; i < ki_cover_count; i++) {
        const KiCover *c = &ki_covers[i];
        int w, h;
        if (c->r2 != s->res || c->k2 != s->k || !pix_of(c->res, c->k, &w, &h)) continue;
        if (!cov) cov = xcalloc((size_t)s->w * s->h, 1);
        for (int y = c->y - 1; y <= c->y + h; y++)
            for (int x = c->x - 1; x <= c->x + w; x++)
                if (x >= 0 && y >= 0 && x < s->w && y < s->h) {
                    u8 *p = &cov[y * s->w + x];
                    if (x >= c->x && y >= c->y && x < c->x + w && y < c->y + h) *p = 1;
                    else if (!*p) *p = 2;
                }
    }
    if (!cov) return;
    for (int py = 0; py < s->h; py++)
        for (int px = 0; px < s->w; px++) {
            u8 v = s->px[py * s->w + px];
            if (cov[py * s->w + px] != 2 || !v || v >= 32) continue;
            int flat = 1;
            for (int ny = py - 1; ny <= py + 1; ny++)
                for (int nx = px - 1; nx <= px + 1; nx++)
                    if (ny >= 0 && nx >= 0 && ny < s->h && nx < s->w && cov[ny * s->w + nx] != 1 && s->px[ny * s->w + nx] != v) flat = 0;
            if (flat)
                for (int y = 4 * py; y < 4 * py + 4; y++) memset(s->q + (size_t)y * 4 * s->w + 4 * px, v, 4);
        }
    free(cov);
}
/* a sprite's cells final: a frame after its references, a part after what lies beneath it --
 * each of them final first (a frame may take a part's, a part may lie on a frame) --, then the rings */
static void finish(Sp *s) {
    if (!s || s->fin) return;           /* (1: under way -- a loop takes the cells as they are) */
    s->fin = 1;
    if (s->nrefs > 0) {
        for (int r = 0; r < s->nrefs; r++) finish(sp_get(s->refs[r].res, s->refs[r].k));
        frame(s);
    } else if (s->has_chain) {
        if (s->root_room) { finish(sp_get(s->root_room, 0)); finish(sp_get(s->root_room - 50, 0)); }
        else finish(sp_get(s->root_res, s->root_k));
        for (int i = 0; i < s->nlayers; i++) finish(sp_get(s->layers[i].res, s->layers[i].k));
        part(s);
    }
    if (s->q) ring(s);
    s->fin = 2;
}
static void part(Sp *s) {
    if (s->part_done) return;
    s->part_done = 1;
    part_of_layers(s);
    int W, H;
    /* what the display shows beneath: the root's cells, the layers' over them */
    u8 *bf;
    if (s->root_room) {
        int lw, lh, rw, rh;
        const u8 *l = final_of(s->root_room, 0, &lw, &lh);
        u8 *lc = xmalloc((size_t)lw * lh); memcpy(lc, l, (size_t)lw * lh);
        const u8 *r = final_of(s->root_room - 50, 0, &rw, &rh);
        W = 640 + rw; H = 4 * 109;
        bf = xcalloc((size_t)W * H, 1);
        for (int y = 0; y < H; y++) { memcpy(bf + y * W, lc + y * lw, (size_t)lw); memcpy(bf + y * W + 640, r + y * rw, (size_t)rw); }
        free(lc);
    } else {
        const u8 *f = final_of(s->root_res, s->root_k, &W, &H);
        bf = xmalloc((size_t)W * H); memcpy(bf, f, (size_t)W * H);
    }
    for (int i = 0; i < s->nlayers; i++) {
        int fw, fh;
        const u8 *f = final_of(s->layers[i].res, s->layers[i].k, &fw, &fh);
        int lx = 4 * s->layers[i].x, ly = 4 * s->layers[i].y;
        for (int y = 0; y < fh; y++)
            for (int x = 0; x < fw; x++) {
                int X = lx + x, Y = ly + y;
                if (X >= 0 && Y >= 0 && X < W && Y < H && f[y * fw + x]) bf[Y * W + X] = f[y * fw + x];
            }
    }
    int uw, uh;
    u8 *under = layered(s, 0, &uw, &uh);   /* the original's pixels beneath */
    int x0 = s->win[0], y0 = s->win[1], x1 = s->win[2], y1 = s->win[3], ww = 4 * (x1 - x0);
    int h4 = 4 * s->h, w4 = 4 * s->w;
    int px0 = 4 * (x0 - s->cx) > 0 ? 4 * (x0 - s->cx) : 0, py0 = 4 * (y0 - s->cy) > 0 ? 4 * (y0 - s->cy) : 0;
    int px1 = 4 * (x1 - s->cx) < w4 ? 4 * (x1 - s->cx) : w4, py1 = 4 * (y1 - s->cy) < h4 ? 4 * (y1 - s->cy) : h4;
    const Job *jc = &jobs[s->ctx];
    for (int py = py0; py < py1; py++)
        for (int pxx = px0; pxx < px1; pxx++) {
            int rx = 4 * s->cx + pxx, ry = 4 * s->cy + py;          /* in the root's sub-pixels */
            int wx = rx - 4 * x0, wy = ry - 4 * y0;                  /* in the window's */
            size_t wi = (size_t)wy * ww + wx;
            const float *Lc = s->Lc + 3 * wi, *Lu = s->Lu + 3 * wi;
            float a = Lc[0] - Lu[0], b = Lc[1] - Lu[1], c = Lc[2] - Lu[2];
            float dd = a * a + b * b;
            dd = dd + c * c;
            int same = sqrtf(dd) < SAME;
            u8 v = s->px[(py / 4) * s->w + pxx / 4], beneath = bf[(size_t)ry * W + rx];
            u8 ov = v ? v : under[(ry / 4) * uw + rx / 4];
            u8 *dst = &s->q[(size_t)py * w4 + pxx];
            if (v) *dst = same && beneath ? beneath : quant1(Lc, jc->rgb + 3 * wi, ov);
            else *dst = s->m[(size_t)py * w4 + pxx] && !same ? quant1(Lc, jc->rgb + 3 * wi, ov) : 0;
        }
    free(bf); free(under);
}

/* ---- the whole ---- */
int ki_make_main(int argc, char **argv) {
    const char *model = NULL, *game = NULL, *out = NULL, *pal_path = NULL, *name = "", *lib = NULL, *dump = NULL, *from = NULL, *dump_out = NULL;
    int threads = 0, gpu = 0, limit = 0;
    const char *cache = NULL;
    for (int i = 0; i < argc; i++) {
        const char *a = argv[i], *v = i + 1 < argc ? argv[i + 1] : NULL;
        if (!strcmp(a, "--make-ki") && v) { model = v; i++; }
        else if (!strcmp(a, "--game") && v) { game = v; i++; }
        else if (!strcmp(a, "--ki-out") && v) { out = v; i++; }
        else if (!strcmp(a, "--ki-palette") && v) { pal_path = v; i++; }
        else if (!strcmp(a, "--ki-model") && v) { name = v; i++; }
        else if (!strcmp(a, "--ki-ort") && v) { lib = v; i++; }
        else if (!strcmp(a, "--ki-threads") && v) { threads = atoi(v); i++; }
        else if (!strcmp(a, "--ki-dump") && v) { dump = v; i++; }
        else if (!strcmp(a, "--ki-from") && v) { from = v; i++; }
        else if (!strcmp(a, "--ki-dump-out") && v) { dump_out = v; i++; }
        else if (!strcmp(a, "--ki-limit") && v) { limit = atoi(v); i++; }
        else if (!strcmp(a, "--ki-gpu")) gpu = 1;
        else if (!strcmp(a, "--ki-cache") && v) { cache = v; i++; }
        else if (!strcmp(a, "--ki-parent") && v) { parent_watch(atol(v)); i++; }
        else die("usage: --make-ki MODEL.onnx --game DIR --ki-out SET.ki [--ki-palette PAL] [--ki-model NAME] [--ki-ort LIBRARY] [--ki-threads N] [--ki-gpu] [--ki-cache DIR] [--ki-parent PID] [--ki-dump DIR] [--ki-from DIR] [--ki-limit N] [--ki-dump-out DIR] (not %s)", a);
    }
    if (!model || !game || (!out && !dump)) die("usage: --make-ki MODEL.onnx --game DIR --ki-out SET.ki ... (%s)", "see src/ki_make.c");
    rsc_open(game);

    /* the sprites and their pictures (tools/hrai.py jobs) */
    static u8 room_done[600];
    for (int n = 101; n < 128; n++) {
        const Bank *l = bank(n), *r = bank(n - 50);
        if (!l || !r || l->n != 1 || r->n != 1) continue;
        if (l->s[0].h != 109 || r->s[0].h != 109 || l->s[0].w > 160 || !want(n, 0) || !want(n - 50, 0)) continue;
        char nm[32]; snprintf(nm, sizeof nm, "room%03d", n);
        Job *j = job_add(nm, 160 + r->s[0].w, 109, 1, 0);
        j->nparts = 2;
        j->parts[0] = (Ref){ n, 0, 0, 0 }; j->parts[1] = (Ref){ n - 50, 0, 160, 0 };
        Sp *a = sp_add(n, 0, l->s[0].w, 109, l->s[0].px), *b = sp_add(n - 50, 0, r->s[0].w, 109, r->s[0].px);
        a->own = b->own = njobs - 1; a->px0 = 0; b->px0 = 160;
        room_done[n] = room_done[n - 50] = 1;
    }
    for (int i = 0; i < ki_part_count; i++) {  /* the groups, by their first sprite: one picture each */
        const KiPart *g = &ki_parts[i];
        if (g->kind != 3 || g->res != g->a || g->k != g->b) continue;
        Ref m[KI_GROUP];
        int n = 0, W = 0, H = 0;
        for (int t = 0; t < ki_part_count; t++) {
            const KiPart *p = &ki_parts[t];
            int w, h;
            if (p->kind != 3 || p->a != g->a || p->b != g->b || room_done[p->res] || !want(p->res, p->k) || n == KI_GROUP) continue;
            pix_of(p->res, p->k, &w, &h);
            m[n++] = (Ref){ p->res, p->k, p->x, p->y };
            if (p->x + w > W) W = p->x + w;
            if (p->y + h > H) H = p->y + h;
        }
        if (n < 2) continue;                   /* (other game files: what is left of it on its own) */
        char nm[32]; snprintf(nm, sizeof nm, "grp%03d_%d", g->a, g->b);
        Job *j = job_add(nm, W, H, 1, 0);
        j->nparts = n;
        for (int t = 0; t < n; t++) {
            int w, h;
            const u8 *px = pix_of(m[t].res, m[t].k, &w, &h);
            Sp *s = sp_add(m[t].res, m[t].k, w, h, px);
            j->parts[t] = m[t];
            s->own = njobs - 1; s->px0 = m[t].x; s->py0 = m[t].y; s->in_sprites = s->grp = 1;
        }
    }
    for (int n = 0; n < 600; n++) {
        if (room_done[n]) continue;
        const Bank *b = bank(n);
        if (!b) continue;
        for (int k = 0; k < b->n; k++) {
            if (!want(n, k) || sp_get(n, k)) continue;     /* (a group's: in its picture) */
            char nm[32]; snprintf(nm, sizeof nm, "%03d_%d", n, k);
            Job *j = job_add(nm, b->s[k].w, b->s[k].h, 1, 0);
            j->nparts = 1; j->parts[0] = (Ref){ n, k, 0, 0 };
            Sp *s = sp_add(n, k, b->s[k].w, b->s[k].h, b->s[k].px);
            s->own = njobs - 1; s->in_sprites = 1;
        }
        for (int k = 0; k < b->n; k++) {
            Sp *s = sp_get(n, k);
            if (!s || !s->in_sprites || part_of(n, k)) continue;
            int kb, x, y;
            if (find_base(n, k, &kb, &x, &y)) { fb_has[n][k] = 1; fb_k[n][k] = kb; fb_x[n][k] = x; fb_y[n][k] = y; }
        }
    }
    {   /* the film dealer's new tapes (src/film_offers.c), sprites 101.. of bank 155 */
        const u8 *orig[10];
        static const u8 ow[10] = { 17, 16, 19, 17, 15, 16, 16, 18, 16, 14 }, oh[10] = { 36, 35, 33, 33, 32, 31, 30, 29, 27, 26 };
        const Bank *b = bank(155);
        int ok = b && b->n >= 10;
        static u8 cut[10][64 * 64];
        for (int k = 0; k < 10 && ok; k++) {
            if (b->s[k].h != oh[k] || b->s[k].w < ow[k]) { ok = 0; break; }
            for (int r = 0; r < oh[k]; r++) memcpy(cut[k] + r * ow[k], b->s[k].px + r * b->s[k].w, ow[k]);
            orig[k] = cut[k];
        }
        for (int j = 1; j <= 7 && ok; j++)
            if (!filmoffers_tape(orig, j, tape_px[j - 1], &tape_w[j - 1], &tape_h[j - 1])) ok = 0;
        tapes_made = ok;
        for (int j = 1; j <= 7 && ok; j++) {
            char nm[32]; snprintf(nm, sizeof nm, "155_%d", 100 + j);
            Job *jb = job_add(nm, tape_w[j - 1], tape_h[j - 1], 1, 0);
            jb->nparts = 1; jb->parts[0] = (Ref){ 155, 100 + j, 0, 0 };
            Sp *s = sp_add(155, 100 + j, tape_w[j - 1], tape_h[j - 1], tape_px[j - 1]);
            s->own = njobs - 1;
        }
    }
    int nsp = sp_n;
    for (int i = 0; i < nsp; i++) {            /* the parts' chains and windows */
        Sp *s = sp_list[i];
        if (!s->in_sprites || s->grp || !chain_top(s)) continue;
        int W, H;
        u8 *rp = root_pix(s, &W, &H); free(rp);
        int x = s->cx, y = s->cy;
        int wx0 = x - MARGIN > 0 ? x - MARGIN : 0, wy0 = y - MARGIN > 0 ? y - MARGIN : 0;
        int wx1 = x + s->w + MARGIN < W ? x + s->w + MARGIN : W, wy1 = y + s->h + MARGIN < H ? y + s->h + MARGIN : H;
        if (wx0 >= wx1 || wy0 >= wy1 || x >= W || y >= H || x + s->w <= 0 || y + s->h <= 0) continue;
        s->has_chain = 1;
        s->win[0] = wx0; s->win[1] = wy0; s->win[2] = wx1; s->win[3] = wy1;
        char nm[32];
        snprintf(nm, sizeof nm, "ctx%03d_%d", s->res, s->k);
        job_add(nm, wx1 - wx0, wy1 - wy0, 0, 1)->of = s; s->ctx = njobs - 1;
        snprintf(nm, sizeof nm, "und%03d_%d", s->res, s->k);
        job_add(nm, wx1 - wx0, wy1 - wy0, 0, 2)->of = s; s->und = njobs - 1;
    }
    for (int i = 0; i < nsp; i++) {            /* the frames of a run of one size: the previous and the first */
        Sp *s = sp_list[i];
        if (!s->in_sprites || s->grp || s->has_chain) continue;
        int first = s->k;
        for (;;) {
            Sp *p = sp_get(s->res, first - 1);
            if (!p || !p->in_sprites || p->w != s->w || p->h != s->h) break;    /* (a group's too: 192:2 from 192:1) */
            first--;
        }
        if (first == s->k) continue;
        int cand[2] = { s->k - 1, first }, nc = first == s->k - 1 ? 1 : 2;
        u8 *st = xmalloc((size_t)s->w * s->h);
        for (int c = 0; c < nc; c++) {
            Sp *f = sp_get(s->res, cand[c]);
            still(s->px, s->w, f->px, f->w, s->w, s->h, STILL, st);
            int any = 0;
            for (int t = 0; t < s->w * s->h; t++) any |= st[t];
            if (any) s->refs[s->nrefs++] = (Ref){ s->res, cand[c], 0, 0 };
        }
        free(st);
    }

    /* the model over every picture */
    if (!from && !dump) ort_open(lib, model, threads, gpu, cache);
    long long work = 0, work_done = 0;         /* the model's pixels, for the launcher's time left */
    for (int i = 0; i < njobs; i++) work += (long long)(jobs[i].w + 2 * PAD) * (jobs[i].h + 2 * PAD) * (jobs[i].mask ? 2 : 1);
    for (int i = 0; i < njobs; i++) {
        parent_check();
        if (limit && i >= limit) { printf("ki %d pictures (--ki-limit)\n", limit); return 0; }
        Job *j = &jobs[i];
        u8 *ix;
        int w = j->w, h = j->h;
        if (j->kind) {
            int W, H;
            u8 *a = layered(j->of, j->kind == 1, &W, &H);
            ix = xmalloc((size_t)w * h);
            for (int y = 0; y < h; y++) memcpy(ix + y * w, a + (j->of->win[1] + y) * W + j->of->win[0], (size_t)w);
            free(a);
        } else {                               /* its sprites, colour 0 clear (a room: both halves) */
            ix = xcalloc((size_t)w * h, 1);
            for (int p = 0; p < j->nparts; p++) {
                int pw, ph;
                const u8 *o = pix_of(j->parts[p].res, j->parts[p].k, &pw, &ph);
                for (int y = 0; y < ph; y++)
                    for (int x = 0; x < pw; x++)
                        if (o[y * pw + x]) ix[(size_t)(j->parts[p].y + y) * w + j->parts[p].x + x] = o[y * pw + x];
            }
        }
        work_done += (long long)(w + 2 * PAD) * (h + 2 * PAD) * (j->mask ? 2 : 1);
        int all = 1;
        for (int t = 0; t < w * h; t++) all &= ix[t] != 0;
        j->mask = j->mask && !all;
        int W, H;
        u8 *in = model_input(ix, w, h, &W, &H), *min = j->mask ? mask_input(ix, w, h) : NULL;
        free(ix);
        if (dump) {
            write_ppm(dump, j->name, in, W, H);
            if (min) { char nm[40]; snprintf(nm, sizeof nm, "%s_m", j->name); write_ppm(dump, nm, min, W, H); }
            free(in); free(min);
            continue;
        }
        u8 *o4, *m4 = NULL;
        if (from) {
            int ow, oh;
            o4 = read_ppm(from, j->name, &ow, &oh);
            if (ow != 4 * W || oh != 4 * H) die("%s: not 4x its picture", j->name);
            if (min) {
                char nm[40]; snprintf(nm, sizeof nm, "%s_m", j->name); m4 = read_ppm(from, nm, &ow, &oh);
                if (ow != 4 * W || oh != 4 * H) die("%s: not 4x its picture", nm);
            }
        } else {
            o4 = ort_run(in, W, H);
            if (min) m4 = ort_run(min, W, H);
            if (dump_out) {
                write_ppm(dump_out, j->name, o4, 4 * W, 4 * H);
                if (m4) { char nm[40]; snprintf(nm, sizeof nm, "%s_m", j->name); write_ppm(dump_out, nm, m4, 4 * W, 4 * H); }
            }
        }
        free(in); free(min);
        /* the border off */
        j->rgb = xmalloc((size_t)3 * 16 * w * h);
        if (m4) j->mrgb = xmalloc((size_t)3 * 16 * w * h);
        for (int y = 0; y < 4 * h; y++) {
            memcpy(j->rgb + (size_t)3 * y * 4 * w, o4 + 3 * ((size_t)(y + 4 * PAD) * 4 * W + 4 * PAD), (size_t)3 * 4 * w);
            if (m4) memcpy(j->mrgb + (size_t)3 * y * 4 * w, m4 + 3 * ((size_t)(y + 4 * PAD) * 4 * W + 4 * PAD), (size_t)3 * 4 * w);
        }
        free(o4); free(m4);
        printf("ki %d of %d (%lld of %lld)\n", i + 1, njobs, work_done, work);
        fflush(stdout);
    }
    if (dump) { printf("ki %d pictures -> %s\n", njobs, dump); return 0; }
    if (ort_sess) {                     /* (the model's memory back before the colours) */
        ort->ReleaseSession(ort_sess); ort->ReleaseMemoryInfo(ort_mem); ort->ReleaseEnv(ort_env);
        ort_sess = NULL;
    }

    /* the colours */
    for (int i = 0; i < 256; i++) lab_of(game_pal[i], pal_L[i]);
    u8 full[256][3];
    memcpy(full, game_pal, sizeof full);
    if (pal_path) {
        FILE *f = fopen(pal_path, "rb");
        if (!f || fread(full, 1, sizeof full, f) != sizeof full) die("%s: not a palette (768 bytes)", pal_path);
        fclose(f);
    }
    ncen = 0;
    for (int i = 1; i < 32; i++) { lab_of(game_pal[i], cen[ncen]); cen_id[ncen++] = (u8)i; }
    if (pal_path) for (int i = 32; i < 32 + EXT; i++) { lab_of(full[i], cen[ncen]); cen_id[ncen++] = (u8)i; }
    near_cache = xcalloc(1u << 24, 1);

    /* each sprite's own cells */
    for (int i = 0; i < nsp; i++) {
        Sp *s = sp_list[i];
        const Job *j = &jobs[s->own];
        int w4 = 4 * s->w, h4 = 4 * s->h, jw = 4 * j->w;
        s->L = xmalloc(sizeof *s->L * 3 * (size_t)w4 * h4);
        s->m = xmalloc((size_t)w4 * h4);
        s->q = xmalloc((size_t)w4 * h4);
        for (int y = 0; y < h4; y++)
            for (int x = 0; x < w4; x++) {
                size_t ji = (size_t)(y + 4 * s->py0) * jw + 4 * s->px0 + x, si = (size_t)y * w4 + x;
                const u8 *rgb = j->rgb + 3 * ji;
                float *L = s->L + 3 * si;
                lab_of(rgb, L);
                u8 m = 1;
                if (j->mrgb) {
                    const u8 *mr = j->mrgb + 3 * ji;
                    m = ((mr[0] * 19595 + mr[1] * 38470 + mr[2] * 7471 + 0x8000) >> 16) >= 128;
                }
                s->m[si] = m;
                u8 o = s->px[(y / 4) * s->w + x / 4];
                s->q[si] = m ? quant1(L, rgb, o) : 0;
            }
    }
    /* the parts' windows in CIELAB */
    for (int i = 0; i < nsp; i++) {
        Sp *s = sp_list[i];
        if (!s->has_chain) continue;
        Job *jc = &jobs[s->ctx], *ju = &jobs[s->und];
        size_t n = (size_t)16 * jc->w * jc->h;
        s->Lc = xmalloc(sizeof *s->Lc * 3 * n); s->Lu = xmalloc(sizeof *s->Lu * 3 * n);
        for (size_t t = 0; t < n; t++) { lab_of(jc->rgb + 3 * t, s->Lc + 3 * t); lab_of(ju->rgb + 3 * t, s->Lu + 3 * t); }
    }
    /* the frames' still parts and the parts, each sprite after those it takes from (tools/hrai.py finish) */
    Sp **order = xmalloc(sizeof *order * (size_t)nsp);
    memcpy(order, sp_list, sizeof *order * (size_t)nsp);
    qsort(order, (size_t)nsp, sizeof *order, sp_cmp);
    for (int i = 0; i < nsp; i++) finish(order[i]);

    /* the set */
    char head[256];
    snprintf(head, sizeof head, "{\"model\": \"%s\", \"colours\": %d, \"tool\": 1}", name, pal_path ? 254 : 32);
    size_t hl = strlen(head), off = 8 + 4 + hl + 768 + 8 + 20 * (size_t)nsp, total = off;
    u8 **blob = xmalloc(sizeof *blob * (size_t)nsp);
    size_t *blen = xmalloc(sizeof *blen * (size_t)nsp);
    for (int i = 0; i < nsp; i++) {
        Sp *s = order[i];
        size_t n = (size_t)16 * s->w * s->h;
        blob[i] = xmalloc(n + n / 2 + 16);      /* (rle's worst: a literal and a run of 2 -- 4 bytes for 3) */
        blen[i] = rle(s->q, n, blob[i]);
        total += blen[i];
    }
    u8 *data = xmalloc(total);
    memcpy(data, "OMTVKI\2\0", 8);
    put32(data + 8, (u32)hl);
    memcpy(data + 12, head, hl);
    memcpy(data + 12 + hl, full, 768);
    put32(data + 12 + hl + 768, pal_path ? 1 : 0);
    put32(data + 12 + hl + 772, (u32)nsp);
    u8 *ix = data + 12 + hl + 776;
    for (int i = 0; i < nsp; i++) {
        Sp *s = order[i];
        put16(ix, (u32)s->res); put16(ix + 2, (u32)s->k); put16(ix + 4, (u32)s->w); put16(ix + 6, (u32)s->h);
        put32(ix + 8, (u32)off); put32(ix + 12, (u32)blen[i]);
        u32 sum = 2166136261u;          /* FNV-1a of the pixels it was made from (src/hr.c ki_sum) */
        for (int t = 0; t < s->w * s->h; t++) sum = (sum ^ s->px[t]) * 16777619u;
        put32(ix + 16, sum);
        memcpy(data + off, blob[i], blen[i]);
        off += blen[i]; ix += 20;
    }
    FILE *f = fopen(out, "wb");
    if (!f || fwrite(data, 1, total, f) != total || fclose(f)) die("cannot write %s", out);
    printf("ki done: %d pictures -> %s (%.1f MB)\n", nsp, out, total / 1e6);
    return 0;
}
