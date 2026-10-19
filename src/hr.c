/* Drawing at 4x resolution (--filter hr): the cells of every byte (src/hr.h,
 * docs/hochaufloesend.md).  This file keeps them: the store, how a sprite
 * bank's cells are made, the copies outside the drawing helpers
 * (far_memcpy, setmem), the 1280x800 picture for the display and the
 * self-test.
 *
 * How a sprite's cells are made: runtime/hr_rules.txt (built into
 * build/hr_data.c by tools/hrdata.py): nearest, the 4x font (tools/hrfont/),
 * MMPX of the sprite on its own (mmpx: its edges repeated; mmpx-wrap: a tile,
 * wrapped around).  MMPX keeps each palette index apart (the index rides in
 * the top byte, the colour below it for MMPX's brightness tests), so colour 0
 * stays colour 0: clear where a helper draws it clear.
 * HR_RULES=nearest makes every sprite nearest (the self-test's picture);
 * --filter hr-pixel makes the pictures' rules (mmpx, mmpx-wrap) nearest,
 * the font and the finer-than-a-pixel parts stay.  HR_FONT=NAME draws the
 * text with the trial font tools/hrfont/try/NAME/ (hrfont.py ttf) instead.
 *
 * HR_SELFTEST=1 (with every rule nearest, HR_RULES=nearest): at every frame
 * each screen byte whose cell is valid must have a flat cell of its own value
 * -- the whole cell machinery against the plain 4x enlargement; at the end a
 * line "[hr] selftest: ..." on stderr.  HR_WATCHPX=X,Y (with HR_SELFTEST)
 * prints that screen pixel's cell whenever it changes (debugging). */
#include <stdio.h>
#include <stdlib.h>
#include "game.h"
#include "seat.h"
#include "hr.h"
#include "hr_data.h"

int hr_on;
HrCell *hr_cells;
static u16 *res_at;                     /* the resource read to a linear address (packed banks) */
static u16 cur_res;                     /* the bank being decoded */

/* the linear address of a host pointer into the game's memory (-1: none) */
static long lin_of(const u8 *p) {
    if (p >= M && p < M + MEM_SIZE) return p - M;
    if (p >= g_dgroup && p < g_dgroup + DG_END) return (long)DSLIN + (p - g_dgroup);
    for (int i = 0; i < HOST_REGIONS; i++)
        if (host_regions[i].host && p >= host_regions[i].host && p < host_regions[i].host + host_regions[i].len)
            return (long)host_regions[i].lin + (p - host_regions[i].host);
    return -1;
}

/* a partly opaque source cell over byte d (old: its value now; v: its value
 * after the copy, 0 = unchanged): a destination written past the hooks
 * becomes the flat cell of its value first, so nothing stale shows through */
void hr_compose(u32 d, const HrCell *s, u8 v, u8 old) {
    HrCell *c = &hr_cells[d];
    if (c->base != old) {
        if (!s->mask && !v) return;       /* (clear in the plain cell: the destination stays as it is, as without a set) */
        hr_set_flat(c, old);
    }
    if (s->kvalid || c->kvalid) {         /* the AI pictures' cell: each its own, or the plain one */
        if (!c->kvalid) { memcpy(c->kpx, c->px, 16); c->kmask = c->mask; }
        const u8 *sk = s->kvalid ? s->kpx : s->px;
        u16 sm = s->kvalid ? s->kmask : s->mask;
        for (int i = 0; i < 16; i++)
            if (sm >> i & 1) c->kpx[i] = sk[i];
        c->kmask |= sm;
        c->kvalid = 1;
    }
    for (int i = 0; i < 16; i++)
        if (s->mask >> i & 1) c->px[i] = s->px[i];
    c->mask |= s->mask;
    c->base = v ? v : old;
}

void hr_flat_px(u16 seg, u16 off, u8 v) {
    if (!hr_on) return;
    hr_set_flat(&hr_cells[lin(seg, off)], v);
    if (hr_org) hr_org[lin(seg, off)] = 0;
}

void hr_save(HrCell **buf, u16 cap, u16 seg, u16 off, u16 n) {
    if (!hr_on) return;
    if (!*buf && !(*buf = malloc(sizeof **buf * cap))) return;
    for (u16 i = 0; i < n && i < cap; i++) (*buf)[i] = hr_cells[lin(seg, (u16)(off + i))];
}
void hr_get(HrCell *buf, u16 seg, u16 off, u16 n) {
    if (!hr_on) return;
    for (u16 i = 0; i < n; i++) buf[i] = hr_cells[lin(seg, (u16)(off + i))];
}

/* a glyph pixel our own code draws (src/cast_fit.c): the font's cell of
 * byte ss:so (value v), its ink in colour col (0: its own), as a transparent
 * copy -- v = 0 keeps the destination's byte but gets the cell's edges */
void hr_glyph_px(u16 ss, u16 so, u16 ds, u16 dof, u8 v, u8 col) {
    if (!hr_on) return;
    HrCell c = hr_cells[lin(ss, so)];
    u32 d = lin(ds, dof);
    u8 nv = v && col ? col : v;
    if (c.base != v) { if (v) hr_set_flat(&hr_cells[d], nv); return; }
    if (!c.mask) return;
    if (col)
        for (int i = 0; i < 16; i++)
            if (c.mask >> i & 1) c.px[i] = col;
    c.base = nv;
    if (c.mask == 0xFFFF && v) { hr_cells[d] = c; return; }
    hr_compose(d, &c, nv, rb(ds, dof));
}

void hr_restore(const HrCell *buf, u16 seg, u16 off, u16 n) {
    if (!hr_on || !buf) return;
    for (u16 i = 0; i < n; i++) hr_cells[lin(seg, (u16)(off + i))] = buf[i];
    if (hr_org)
        for (u16 i = 0; i < n; i++) hr_org[lin(seg, (u16)(off + i))] = 0;
}

void hr_res_read(u16 id, const u8 *buf) {
    if (!hr_on) return;
    long a = lin_of(buf);
    if (a >= 0) res_at[a] = id;
}

void hr_bank_begin(u32 packed) {
    if (hr_on) cur_res = res_at[lin(FPSEG(packed), FPOFF(packed))];
}

static int all_nearest;                  /* HR_RULES=nearest */

/* the rule for sprite k of bank res (runtime/hr_rules.txt; a bank not there: mmpx) */
static int pictures_rule(u16 res, int k) {
    for (int i = 0; i < hr_rule_count; i++)
        if (hr_rules[i].res == res && k >= hr_rules[i].first && k <= hr_rules[i].last) return hr_rules[i].rule;
    return HR_MMPX;
}
static int rule_of(u16 res, int k) {
    if (all_nearest) return HR_NEAREST;
    int r = pictures_rule(res, k);
    return r == HR_MMPX || r == HR_WRAP ? HR_NEAREST : r;   /* (the pictures' pixels as they are: --filter hr, which smoothed them, went 2026-10-18) */
}

/* ---- HR_KIMAP=FILE: what lies beneath the sprites the game draws ----
 * Every byte's origin (a bank sprite's pixel; 0: none) goes along with its
 * cell.  At each drawing helper, for every pixel a sprite puts down, the
 * pixel beneath counts towards "sprite A over sprite B, B's pixel at A's
 * plus (ox, oy)".  Every KIMAP_EVERY frames a look at the screen: which
 * sprites show there, and which lie beside one another -- two neighbouring
 * pixels from different sprites, "sprite A beside sprite B, B's origin at
 * A's plus (ox, oy)" (A the lower res:k).  At the end the file lists, for
 * every sprite drawn:
 *   sprite RES K draws N pixels P places Q      (Q: different destinations)
 *   under RES K RES2 K2 OX OY PIXELS            (the relations, most first)
 *   seen RES K LOOKS                            (looks at the screen it was on)
 *   beside RES K RES2 K2 OX OY LOOKS PAIRS
 * tools/hrai.py takes from it where an animation part sits on its picture,
 * which pictures the game puts together from pieces, and where a picture
 * the model does not see lies on one it does. */
#define KI_ORG(res, k, x, y) ((uint64_t)((res) + 1) << 32 | (uint64_t)(k) << 16 | (uint64_t)((y) & 0xFF) << 8 | ((x) & 0xFF))
#define KI_KEY(o) (((o) >> 32) << 8 | ((o) >> 16 & 0xFF))     /* res + 1 and k in 18 bits */
uint64_t *hr_org;
static const char *kimap_path;
typedef struct { uint64_t key, n; } KiCount;
#define KIMAP_SLOTS (1u << 20)
static KiCount *kimap_under, *kimap_sprite, *kimap_place;
#define KIMAP_EVERY 8
typedef struct { uint64_t key, n, pairs; uint32_t last; } KiLook;
static KiLook *kimap_seen, *kimap_beside;
static uint32_t kimap_looks;
static KiCount *ki_slot(KiCount *t, uint64_t key) {
    uint32_t i = (uint32_t)((key * 0x9E3779B97F4A7C15ull) >> 44) & (KIMAP_SLOTS - 1);
    for (uint32_t tries = 0; tries < KIMAP_SLOTS; tries++, i = (i + 1) & (KIMAP_SLOTS - 1))
        if (!t[i].n || t[i].key == key) { t[i].key = key; return &t[i]; }
    static KiCount full;                /* (every slot taken: what is new is not counted) */
    full.n = 0;
    return &full;
}

void hr_ki_draw(u16 sseg, u16 soff, u16 sstride, u16 w, u16 h, u16 dseg, u16 doff, u16 dstride, int transparent) {
    if (!hr_org || !kimap_under) return;
    uint64_t first = 0;
    for (u16 y = 0; y < h; y++)
        for (u16 x = 0; x < w; x++) {
            u16 so = (u16)(soff + y * sstride + x), dof = (u16)(doff + y * dstride + x);
            if (transparent && !rb(sseg, so)) continue;
            uint64_t o = hr_org[lin(sseg, so)], b = hr_org[lin(dseg, dof)];
            if (!o) continue;
            uint64_t sk = KI_KEY(o);
            if (!first) {
                first = sk;
                ki_slot(kimap_sprite, sk)->n++;
                ki_slot(kimap_place, sk << 24 | lin(dseg, doff))->n++;
            }
            ki_slot(kimap_sprite, sk | 1ull << 63)->n++;
            if (!b) continue;
            int ox = (int)(b & 0xFF) - (int)(o & 0xFF), oy = (int)(b >> 8 & 0xFF) - (int)(o >> 8 & 0xFF);
            ki_slot(kimap_under, sk << 36 | KI_KEY(b) << 18 | (uint64_t)(ox + 256) << 9 | (uint64_t)(oy + 256))->n++;
        }
}

/* a count once per look at the screen (and how often in it) */
static void ki_look(KiLook *t, uint64_t key) {
    uint32_t i = (uint32_t)((key * 0x9E3779B97F4A7C15ull) >> 44) & (KIMAP_SLOTS - 1);
    for (uint32_t tries = 0; tries < KIMAP_SLOTS; tries++, i = (i + 1) & (KIMAP_SLOTS - 1))
        if (!t[i].pairs || t[i].key == key) {
            t[i].key = key;
            t[i].pairs++;
            if (t[i].last != kimap_looks) { t[i].last = kimap_looks; t[i].n++; }
            return;
        }
}
/* the screen: the sprites on it, and those beside one another (hr_frame_hook) */
static void kimap_frame(void) {
    static unsigned n;
    if (n++ % KIMAP_EVERY) return;
    kimap_looks++;
    const uint64_t *v = hr_org + 0xA0000;
    for (int y = 0; y < 200; y++)
        for (int x = 0; x < 320; x++) {
            uint64_t a = v[y * 320 + x];
            if (!a) continue;
            ki_look(kimap_seen, KI_KEY(a));
            for (int t = 0; t < 2; t++) {               /* the right and the lower neighbour */
                int x2 = x + !t, y2 = y + t;
                if (x2 >= 320 || y2 >= 200) continue;
                uint64_t b = v[y2 * 320 + x2];
                if (!b || KI_KEY(a) == KI_KEY(b)) continue;
                uint64_t p = a, q = b;
                int px = x, py = y, qx = x2, qy = y2;
                if (KI_KEY(q) < KI_KEY(p)) { p = b; q = a; px = x2; py = y2; qx = x; qy = y; }
                int ox = (qx - (int)(q & 0xFF)) - (px - (int)(p & 0xFF)), oy = (qy - (int)(q >> 8 & 0xFF)) - (py - (int)(p >> 8 & 0xFF));
                ki_look(kimap_beside, KI_KEY(p) << 38 | KI_KEY(q) << 20 | (uint64_t)(ox + 512) << 10 | (uint64_t)(oy + 512));
            }
        }
}

static int ki_cmp(const void *a, const void *b) {
    const KiCount *x = a, *y = b;
    return x->n < y->n ? 1 : x->n > y->n ? -1 : (x->key > y->key) - (x->key < y->key);
}
static int ki_look_cmp(const void *a, const void *b) {
    const KiLook *x = a, *y = b;
    return x->n < y->n ? 1 : x->n > y->n ? -1 : (x->key > y->key) - (x->key < y->key);
}
static void kimap_write(void) {
    FILE *f = fopen(kimap_path, "w");
    if (!f) { perror(kimap_path); return; }
    fprintf(f, "# HR_KIMAP (src/hr.c): for every sprite drawn, what lay beneath its pixels; the looks at the screen\n");
    KiCount *u = malloc(sizeof *u * KIMAP_SLOTS);
    u32 nu = 0;
    for (u32 i = 0; i < KIMAP_SLOTS; i++) if (kimap_under[i].n) u[nu++] = kimap_under[i];
    qsort(u, nu, sizeof *u, ki_cmp);
    for (u32 i = 0; i < KIMAP_SLOTS; i++) {
        const KiCount *c = &kimap_sprite[i];
        if (!c->n || c->key >> 63) continue;
        uint64_t sk = c->key;
        u32 places = 0;
        for (u32 j = 0; j < KIMAP_SLOTS; j++) if (kimap_place[j].n && kimap_place[j].key >> 24 == sk) places++;
        fprintf(f, "sprite %u %u draws %llu pixels %llu places %u\n", (unsigned)(sk >> 8) - 1, (unsigned)(sk & 0xFF),
                (unsigned long long)c->n, (unsigned long long)ki_slot(kimap_sprite, sk | 1ull << 63)->n, places);
    }
    for (u32 i = 0; i < nu; i++) {
        uint64_t key = u[i].key, sk = key >> 36, bk = (key >> 18) & 0x3FFFF;
        int ox = (int)((key >> 9) & 0x1FF) - 256, oy = (int)(key & 0x1FF) - 256;
        fprintf(f, "under %u %u %u %u %d %d %llu\n", (unsigned)(sk >> 8) - 1, (unsigned)(sk & 0xFF),
                (unsigned)(bk >> 8) - 1, (unsigned)(bk & 0xFF), ox, oy, (unsigned long long)u[i].n);
    }
    free(u);
    KiLook *l = malloc(sizeof *l * KIMAP_SLOTS);
    for (int t = 0; t < 2 && l; t++) {
        KiLook *tab = t ? kimap_beside : kimap_seen;
        nu = 0;
        for (u32 i = 0; i < KIMAP_SLOTS; i++) if (tab[i].n) l[nu++] = tab[i];
        qsort(l, nu, sizeof *l, ki_look_cmp);
        for (u32 i = 0; i < nu; i++) {
            uint64_t key = l[i].key, a = t ? key >> 38 : key, b = (key >> 20) & 0x3FFFF;
            int ox = (int)((key >> 10) & 0x3FF) - 512, oy = (int)(key & 0x3FF) - 512;
            if (!t) fprintf(f, "seen %u %u %llu\n", (unsigned)(a >> 8) - 1, (unsigned)(a & 0xFF), (unsigned long long)l[i].n);
            else fprintf(f, "beside %u %u %u %u %d %d %llu %llu\n", (unsigned)(a >> 8) - 1, (unsigned)(a & 0xFF),
                         (unsigned)(b >> 8) - 1, (unsigned)(b & 0xFF), ox, oy, (unsigned long long)l[i].n, (unsigned long long)l[i].pairs);
        }
    }
    free(l);
    fclose(f);
}

/* ---- AI pictures (docs/ki-bilder.md) ----
 * A picture set, made from the player's own game files (mtv-dec --make-ki,
 * src/ki_make.c, for the launcher; tools/hrai.py): the cells of the pictures
 * (sprites of rule mmpx) upscaled by a model, and its own colours 32..254 for
 * them (the AI cells only: a few of the original's pictures use those indices,
 * black in its palette).  The file:
 *   "OMTVKI\2\0", u32 n + n bytes of JSON (model, colours), 768 bytes RGB
 *   (0..255, the extra colours at 32..254), u32 flags (1: extra colours),
 *   u32 count, count x {u16 res, u16 sprite, u16 w, u16 h, u32 offset, u32 length,
 *   u32 sum} sorted by res and sprite, then the pictures: 4w x 4h palette indices
 *   each (0: transparent), run-length coded: a byte c < 128 is followed by c + 1
 *   bytes as they are, c >= 128 by one byte to repeat c - 126 times.  sum: FNV-1a
 *   (32 bits) of the w x h pixels it was made from -- a sprite that differs (our
 *   own tower sign over the original's, other game files) keeps its plain cells.
 * All little-endian.  [display] ai_pictures (or HR_KI=FILE) loads one. */
typedef struct { u16 res, k, w, h; u32 off, len, sum; } KiEntry;
static u8 *ki_file;
static KiEntry *ki_index;
static u32 ki_count;
static u8 ki_pal[768];
static int ki_ext;                      /* the set brings the colours 32..254 */
static int ki_shown;                    /* a set is loaded and its pictures shown (Ctrl+K: hr_ki_toggle) */

static u32 le32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }

static int ki_open(const char *path) {
    FILE *f = settings_fopen(path, "rb");      /* (from the settings file: UTF-8) */
    if (!f) { fprintf(stderr, "[hr] %s: no picture set there\n", path); return 0; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    u8 *d = n > 0 ? malloc((size_t)n) : NULL;
    if (!d || fread(d, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(d); return 0; }
    fclose(f);
    const char *bad = NULL;
    uint64_t p = 12 + (uint64_t)(n >= 12 ? le32(d + 8) : 0);
    if (n < 12 || memcmp(d, "OMTVKI", 6)) bad = "not a picture set";
    else if (memcmp(d + 6, "\2\0", 2)) bad = "a set of another version (make it again in the launcher)";
    else if (p + 776 > (uint64_t)n) bad = "cut short";
    else {
        memcpy(ki_pal, d + p, 768);
        ki_ext = le32(d + p + 768) & 1;
        ki_count = le32(d + p + 772);
        p += 776;
        if (p + (uint64_t)ki_count * 20 > (uint64_t)n) bad = "cut short";
    }
    if (!bad && !(ki_index = calloc(ki_count ? ki_count : 1, sizeof *ki_index))) bad = "no memory for it";
    if (!bad) {
        for (u32 i = 0; i < ki_count && !bad; i++) {
            const u8 *e = d + p + 20 * i;
            KiEntry *k = &ki_index[i];
            k->res = (u16)(e[0] | e[1] << 8); k->k = (u16)(e[2] | e[3] << 8);
            k->w = (u16)(e[4] | e[5] << 8); k->h = (u16)(e[6] | e[7] << 8);
            k->off = le32(e + 8); k->len = le32(e + 12); k->sum = le32(e + 16);
            if ((uint64_t)k->off + k->len > (uint64_t)n) bad = "a picture outside the file";
            if (!k->w || !k->h || k->w > 320 || k->h > 255) bad = "a picture of a size the game has none of";
            if (i && (k->res < k[-1].res || (k->res == k[-1].res && k->k <= k[-1].k))) bad = "index not sorted";
        }
    }
    if (bad) {
        fprintf(stderr, "[hr] %s: %s\n", path, bad);
        free(d); free(ki_index); ki_index = NULL; ki_count = 0;
        return 0;
    }
    ki_file = d;
    ki_shown = 1;
    fprintf(stderr, "[hr] AI pictures: %s (%u pictures%s)\n", path, ki_count, ki_ext ? ", own colours" : "");
    return 1;
}

static u32 ki_used;                     /* pictures taken from the set (sprites decoded, tapes drawn) */

/* Ctrl+K (runtime/host.c): the AI pictures off and on again; -1 without a set */
int hr_ki_toggle(void) {
    if (!hr_on || !ki_index) return -1;
    ki_shown = !ki_shown;
    fprintf(stderr, "[hr] AI pictures %s (%u pictures taken from the set so far)\n", ki_shown ? "on" : "off", ki_used);
    return ki_shown;
}

/* FNV-1a of a w x h picture (rows STRIDE apart): what tools/hrai.py and src/ki_make.c write as sum */
static u32 ki_sum(const u8 *p, int w, int h, int stride) {
    u32 v = 2166136261u;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) v = (v ^ p[(size_t)y * stride + x]) * 16777619u;
    return v;
}

static const KiEntry *ki_find(u16 res, int k) {
    u32 lo = 0, hi = ki_count;
    while (lo < hi) {
        u32 m = (lo + hi) / 2;
        const KiEntry *e = &ki_index[m];
        if (e->res == res && e->k == k) return e;
        if (e->res < res || (e->res == res && e->k < k)) lo = m + 1; else hi = m;
    }
    return NULL;
}

/* a picture of the set, decoded (kept: the rooms come back; one that does not decode kept as
 * none -- it would be tried again for every pixel of a tape) */
static u8 **ki_dec;
static u8 ki_broken;
static const u8 *ki_pixels(const KiEntry *e) {
    u32 i = (u32)(e - ki_index);
    if (!ki_dec && !(ki_dec = calloc(ki_count, sizeof *ki_dec))) return NULL;
    if (ki_dec[i] == &ki_broken) return NULL;
    if (ki_dec[i]) return ki_dec[i];
    size_t n = (size_t)16 * e->w * e->h, o = 0;
    u8 *px = malloc(n);
    if (!px) return NULL;
    const u8 *s = ki_file + e->off, *end = s + e->len;
    while (s < end && o < n) {
        int c = *s++;
        if (c < 128) { for (int i = 0; i <= c && s < end && o < n; i++) px[o++] = *s++; }
        else if (s < end) { u8 v = *s++; for (int i = 0; i < c - 126 && o < n; i++) px[o++] = v; }
    }
    if (o != n) { free(px); ki_dec[i] = &ki_broken; return NULL; }
    return ki_dec[i] = px;
}

/* a picture our own code draws (src/film_offers.c's new tapes: sprites 101.. of bank 155,
 * made at run time; W x H pixels at P, rows STRIDE apart): its picture in the set, if the
 * set has it as it is now (its size and sum), else NULL */
const void *hr_ki_picture(u16 res, u16 k, const u8 *p, int w, int h, int stride) {
    if (!hr_on || !ki_index || all_nearest) return NULL;
    const KiEntry *e = ki_find(res, k);
    if (!e || e->w != w || e->h != h || e->sum != ki_sum(p, w, h, stride) || !ki_pixels(e)) return NULL;
    ki_used++;
    return e;
}
/* its pixel x, y, put down as a sprite's pixel of colour v (not 0) is; without the picture
 * (PIC NULL) a flat cell */
void hr_ki_px(const void *pic, int x, int y, u16 seg, u16 off, u8 v) {
    if (!hr_on) return;
    const KiEntry *e = pic;
    const u8 *px = e && x >= 0 && y >= 0 && x < e->w && y < e->h ? ki_pixels(e) : NULL;
    if (!px) { hr_flat_px(seg, off, v); return; }
    u32 d = lin(seg, off);
    HrCell *c = &hr_cells[d];
    if (c->base != rb(seg, off)) hr_set_flat(c, rb(seg, off));
    u8 kp[16];
    u16 km = c->kvalid ? c->kmask : c->mask;
    memcpy(kp, c->kvalid ? c->kpx : c->px, 16);
    for (int i = 0; i < 16; i++) {
        u8 p = px[(size_t)(4 * y + i / 4) * 4 * e->w + 4 * x + i % 4];
        if (p) { kp[i] = p; km |= (u16)(1 << i); }
    }
    hr_set_flat(c, v);                  /* (the plain display: as before) */
    memcpy(c->kpx, kp, 16);
    c->kmask = km;
    c->kvalid = 1;
    if (hr_org) hr_org[d] = 0;
}

/* the AI pictures' cells of sprite k from the set (beside the plain ones); 0 if it has none,
 * it does not fit, or its pixels are not the ones the set was made from */
static int ki_cells(u16 res, int k, u16 seg, u16 off, int w8, int h) {
    const KiEntry *e = ki_find(res, k);
    if (!e || (e->w + 7) / 8 != w8 || e->h != h) return 0;
    int w = w8 * 8, pw = 4 * e->w;
    u32 v0 = 2166136261u;                       /* (ki_sum over the game's memory) */
    for (int y = 0; y < h; y++)
        for (int x = 0; x < e->w; x++) v0 = (v0 ^ rb(seg, (u16)(off + y * w + x))) * 16777619u;
    if (v0 != e->sum) return 0;
    const u8 *px = ki_pixels(e);
    if (!px) return 0;
    ki_used++;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            HrCell *c = &hr_cells[lin(seg, (u16)(off + y * w + x))];
            c->kmask = 0;
            for (int i = 0; i < 16; i++) {
                u8 v = x < e->w ? px[(size_t)(4 * y + i / 4) * pw + 4 * x + i % 4] : 0;
                c->kpx[i] = v;
                if (v) c->kmask |= (u16)(1 << i);
            }
            c->kvalid = 1;
        }
    return 1;
}

static const HrFontSet *font_set = &hr_font_sets[0];   /* HR_FONT=NAME: a trial font */

static const HrGlyph *glyph_of(u16 res, int k) {
    for (int i = 0; i < font_set->count; i++)
        if (font_set->glyphs[i].res == res && font_set->glyphs[i].sprite == k) return &font_set->glyphs[i];
    return NULL;
}

/* the cells of one sprite (w8 * 8 x h pixels at seg:off, rows w8 * 8 apart): by its rule,
 * and the AI pictures' beside them where the set has the sprite */
static void sprite_cells_rule(u16 res, int k, u16 seg, u16 off, int w8, int h);
static void sprite_cells(u16 res, int k, u16 seg, u16 off, int w8, int h) {
    int w = w8 * 8;
    if (hr_org)
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) hr_org[lin(seg, (u16)(off + y * w + x))] = KI_ORG(res, k, x, y);
    sprite_cells_rule(res, k, seg, off, w8, h);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) hr_cells[lin(seg, (u16)(off + y * w + x))].kvalid = 0;
    if (ki_index && !all_nearest && pictures_rule(res, k) == HR_MMPX) ki_cells(res, k, seg, off, w8, h);
}
static void sprite_cells_rule(u16 res, int k, u16 seg, u16 off, int w8, int h) {
    int w = w8 * 8, rule = rule_of(res, k);
    if (rule == HR_FONT) {
        const HrGlyph *g = glyph_of(res, k);
        if (g && w == 8 && g->h == h) {        /* the glyph's 8-pixel box from the 4x font */
            for (int y = 0; y < h; y++)
                for (int x = 0; x < 8; x++) {
                    u16 o = (u16)(off + y * 8 + x);
                    HrCell *c = &hr_cells[lin(seg, o)];
                    c->mask = 0;
                    for (int i = 0; i < 16; i++) {
                        u8 v = g->px[(4 * y + i / 4) * 32 + 4 * x + i % 4];
                        c->px[i] = v;
                        if (v) c->mask |= (u16)(1 << i);
                    }
                    c->base = rb(seg, o);
                }
            return;
        }
    }
    if (rule == HR_THIN) {                 /* a line: its middle two sub-pixels (rows, or columns of column 0) */
        int vert = h > 1;
        for (int y = 0; y < h && vert; y++)
            for (int x = 1; x < w; x++) if (rb(seg, (u16)(off + y * w + x))) vert = 0;
        if (h == 1 || vert) {
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++) {
                    u16 o = (u16)(off + y * w + x);
                    u8 v = rb(seg, o);
                    HrCell *c = &hr_cells[lin(seg, o)];
                    hr_set_flat(c, v);
                    if (!v) continue;
                    c->mask = 0;
                    for (int i = 0; i < 16; i++) {
                        int mid = vert ? (i % 4 == 1 || i % 4 == 2) : (i / 4 == 1 || i / 4 == 2);
                        if (mid) c->mask |= (u16)(1 << i); else c->px[i] = 0;
                    }
                }
            return;
        }
    }
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            u16 o = (u16)(off + y * w + x);
            hr_set_flat(&hr_cells[lin(seg, o)], rb(seg, o));
        }
}

/* a bank as huff_unpack_bank leaves it: u16 count, count headers { u16 w8,
 * u16 h, far pointer to the pixels } */
void hr_bank_done(u32 bank) {
    if (!hr_on) return;
    u16 n = FP16(bank, 0);
    for (u16 k = 0; k < n; k++) {
        u32 hd = MK32(FPSEG(bank), (u16)(FPOFF(bank) + 2 + 8 * k)), p = FP32(hd, 4);
        sprite_cells(cur_res, k, FPSEG(p), FPOFF(p), FP16(hd, 0), FP16(hd, 2));
    }
}

void hr_bank_redo(u16 res, u32 bank) {
    if (!hr_on) return;
    u16 keep = cur_res;
    cur_res = res;
    hr_bank_done(bank);
    cur_res = keep;
}

void hr_memcpy(u8 *dst, const u8 *src, u16 n) {
    if (!hr_on) return;
    for (u16 i = 0; i < n; i++) {
        long s = lin_of(src + i), d = lin_of(dst + i);
        if (s >= 0 && d >= 0) { hr_cells[d] = hr_cells[s]; if (hr_org) hr_org[d] = hr_org[s]; }
    }
}

void hr_setmem(u32 p, u16 n, u8 v) {
    if (!hr_on) return;
    for (u16 i = 0; i < n; i++) {
        hr_set_flat(&hr_cells[lin(FPSEG(p), (u16)(FPOFF(p) + i))], v);
        if (hr_org) hr_org[lin(FPSEG(p), (u16)(FPOFF(p) + i))] = 0;
    }
}

/* ---- a network game's seats (--seats; runtime/seat.c) ----
 * What a seat owns and runtime/seat.c swaps -- its screen, the part of the
 * seat window its heap used, the buffers drawn per view (src/seatctx.c) --
 * carries its cells: saved and loaded with the seat. */
int seat_buf_range(int i, u32 *lin, u32 *len);
static HrCell *seat_cells[SEATS_MAX];
static u32 seat_cells_cap[SEATS_MAX], seat_cells_n[SEATS_MAX];
typedef struct { u32 lin, len; } HrRange;
static int seat_ranges(int seat, HrRange *r) {
    int n = 0;
    r[n++] = (HrRange){ 0xA0000, 0x10000 };
    if (seat > 0 && seat_heap_peak[seat]) r[n++] = (HrRange){ (u32)SEAT_HEAP_SEG << 4, seat_heap_peak[seat] };
    u32 lin, len;
    for (int i = 0; seat_buf_range(i, &lin, &len); i++)
        if (len && lin + len <= MEM_SIZE) r[n++] = (HrRange){ lin, len };
    return n;
}
void hr_seat_save(int seat) {
    if (!hr_on || seat < 0 || seat >= SEATS_MAX) return;
    HrRange r[16];
    int n = seat_ranges(seat, r);
    u32 total = 0;
    for (int i = 0; i < n; i++) total += r[i].len;
    if (total > seat_cells_cap[seat]) {
        free(seat_cells[seat]);
        seat_cells[seat] = malloc(sizeof (HrCell) * (seat_cells_cap[seat] = total + 0x4000));
        if (!seat_cells[seat]) { seat_cells_cap[seat] = seat_cells_n[seat] = 0; return; }
    }
    u32 k = 0;
    for (int i = 0; i < n; i++) { memcpy(seat_cells[seat] + k, hr_cells + r[i].lin, sizeof (HrCell) * r[i].len); k += r[i].len; }
    seat_cells_n[seat] = k;
}
void hr_seat_load(int seat) {
    if (!hr_on || seat < 0 || seat >= SEATS_MAX || !seat_cells[seat]) return;
    HrRange r[16];
    int n = seat_ranges(seat, r);
    u32 k = 0;
    for (int i = 0; i < n && k + r[i].len <= seat_cells_n[seat]; i++) {
        memcpy(hr_cells + r[i].lin, seat_cells[seat] + k, sizeof (HrCell) * r[i].len);
        k += r[i].len;
    }
}

/* ---- the picture ---- */

/* the 1280x800 picture of the screen (A000:0000) from the cells, colours
 * from pal; bad[i] = 1 where screen pixel i's cell is not valid (the caller
 * takes MMPX there).  Returns the number of those. */
int hr_render(u32 *out, const u32 *pal, u8 *bad) {
    if (!hr_on) return -1;
    u32 own[256];
    const u32 *kpal = pal;                /* the AI cells' colours: the set's 32..254 */
    if (ki_shown && ki_ext) {
        memcpy(own, pal, sizeof own);
        for (int i = 32; i < 255; i++)
            own[i] = 0xFF000000u | (u32)ki_pal[3 * i] << 16 | (u32)ki_pal[3 * i + 1] << 8 | ki_pal[3 * i + 2];
        kpal = own;
    }
    int n = 0;
    for (int y = 0; y < 200; y++)
        for (int x = 0; x < 320; x++) {
            u32 a = 0xA0000 + y * 320 + x;
            const HrCell *c = &hr_cells[a];
            int i = y * 320 + x;
            if (c->base != M[a]) { bad[i] = 1; n++; continue; }
            bad[i] = 0;
            u32 *o = out + (size_t)(4 * y) * 1280 + 4 * x;
            int ai = ki_shown && c->kvalid;
            const u8 *px = ai ? c->kpx : c->px;
            const u32 *cp = ai ? kpal : pal;
            for (int r = 0; r < 4; r++, o += 1280)
                for (int q = 0; q < 4; q++) o[q] = cp[px[4 * r + q]];
        }
    return n;
}

/* ---- self-test ---- */
static u64 st_frames, st_bad, st_invalid;
static void selftest_frame(void) {
    st_frames++;
    {   /* HR_WATCHPX=X,Y: the screen pixel's cell whenever it changes (debugging) */
        static int wx = -2, wy;
        static HrCell last;
        if (wx == -2) { const char *e = getenv("HR_WATCHPX"); wx = -1; if (e) sscanf(e, "%d,%d", &wx, &wy); }
        if (wx >= 0) {
            const HrCell *c = &hr_cells[0xA0000 + wy * 320 + wx];
            if (memcmp(c, &last, sizeof last)) {
                fprintf(stderr, "[hr] frame %llu pixel %d,%d M %02X base %03X mask %04X px", (unsigned long long)frame_no(), wx, wy, M[0xA0000 + wy * 320 + wx], c->base, c->mask);
                for (int i = 0; i < 16; i++) fprintf(stderr, " %02X", c->px[i]);
                fprintf(stderr, "\n");
                last = *c;
            }
        }
    }
    for (u32 a = 0xA0000; a < 0xA0000 + 64000; a++) {
        const HrCell *c = &hr_cells[a];
        u8 m = M[a];
        if (c->base != m) { st_invalid++; continue; }
        int ok = c->mask == (m ? 0xFFFF : 0);
        for (int i = 0; i < 16 && ok; i++) ok = c->px[i] == m;
        if (!ok) {
            if (st_bad < 5) fprintf(stderr, "[hr] selftest: frame %llu pixel %u,%u value %02X: cell not flat\n",
                                    (unsigned long long)frame_no(), (a - 0xA0000) % 320, (a - 0xA0000) / 320, m);
            st_bad++;
        }
    }
}
static void selftest_end(void) {
    fprintf(stderr, "[hr] selftest: %llu frames, %llu cells not flat, %llu invalid pixel-frames\n",
            (unsigned long long)st_frames, (unsigned long long)st_bad, (unsigned long long)st_invalid);
}

extern void (*hr_frame_hook)(void);
int hr_init(void) {
    hr_cells = calloc(MEM_SIZE, sizeof *hr_cells);   /* every cell flat 0, like the memory */
    res_at = calloc(MEM_SIZE, sizeof *res_at);
    if (!hr_cells || !res_at) { free(hr_cells); free(res_at); hr_cells = NULL; return 0; }
    hr_on = 1;
    all_nearest = getenv("HR_RULES") && !strcmp(getenv("HR_RULES"), "nearest");
    const char *fn = getenv("HR_FONT");
    for (int i = 0; fn && i < hr_font_set_count; i++)
        if (!strcmp(hr_font_sets[i].name, fn)) font_set = &hr_font_sets[i];
    if (fn && font_set == &hr_font_sets[0] && *fn)
        fprintf(stderr, "[hr] HR_FONT=%s: no such trial font (tools/hrfont/try/), the draft's\n", fn);
    if (getenv("HR_SELFTEST")) { hr_frame_hook = selftest_frame; atexit(selftest_end); }
    const char *ki = getenv("HR_KI") && *getenv("HR_KI") ? getenv("HR_KI") : ai_pictures_path;   /* (HR_KI: the tests) */
    if (ki && *ki) ki_open(ki);
    if ((kimap_path = getenv("HR_KIMAP")) && *kimap_path) {
        hr_org = calloc(MEM_SIZE, sizeof *hr_org);
        kimap_under = calloc(KIMAP_SLOTS, sizeof *kimap_under);
        kimap_sprite = calloc(KIMAP_SLOTS, sizeof *kimap_sprite);
        kimap_place = calloc(KIMAP_SLOTS, sizeof *kimap_place);
        kimap_seen = calloc(KIMAP_SLOTS, sizeof *kimap_seen);
        kimap_beside = calloc(KIMAP_SLOTS, sizeof *kimap_beside);
        if (hr_org && kimap_under && kimap_sprite && kimap_place && kimap_seen && kimap_beside) {
            atexit(kimap_write);
            if (!hr_frame_hook) hr_frame_hook = kimap_frame;
        } else hr_org = NULL;
    }
    return 1;
}

/* ---- finer than the game's pixels (docs/hochaufloesend.md, 3c: stage 5) ----
 * Hooks after the original's drawing: from the value behind a picture they
 * draw its cells at 4x.  They write cells only, never the game's bytes (each
 * cell keeps the base of its byte), skip a byte written past the hooks, and
 * call no game function (game time stays as it is).  HR_RULES=nearest (the
 * self-test) turns them off. */

/* the colour of the first pixel of sprite k (1-based, as col_to_px counts) of a bank */
static u8 sprite_colour(u32 bank, int k, int row) {
    u32 hd = MK32(FPSEG(bank), (u16)(FPOFF(bank) + k * 8 - 6)), p = FP32(hd, 4);
    return FP8(p, (u16)(row * (FP16(hd, 0) << 3)));
}

/* the Betty meter (ui_draw_betty_meter): pixels 1..46 of the 48 x 2 bitmap
 * bmp, a pixel filled when love >= its share of 100 -- here 184 steps */
void hr_betty_meter(u32 bmp, u16 love, u32 atlas) {
    if (!hr_on || all_nearest) return;
    u8 col[2][2];                       /* [filled, empty][row]: the atlas's sprites 0xC and 0xD */
    for (int k = 0; k < 2; k++)
        for (int y = 0; y < 2; y++) col[k][y] = sprite_colour(atlas, 0xC + k, y);
    int fill = (i16)love <= 0 ? 0 : (i16)love >= 100 ? 184 : (i16)love * 184 / 100;
    u32 pix = FP32(bmp, 4);
    int stride = FP16(bmp, 0) << 3;
    for (int y = 0; y < 2; y++)
        for (int x = 1; x <= 46; x++) {
            u16 o = (u16)(FPOFF(pix) + y * stride + x);
            HrCell *c = &hr_cells[lin(FPSEG(pix), o)];
            if (c->base != rb(FPSEG(pix), o)) continue;
            for (int i = 0; i < 16; i++) c->px[i] = col[(x - 1) * 4 + i % 4 < fill ? 0 : 1][y];
            c->mask = 0xFFFF;
            c->kvalid = 0;
        }
}

/* the quarter pixels a bar of the statistics chart has beyond its whole
 * pixels h: the game's height is v * 15 / (scale * 1000) (ui_draw_stats_chart) */
int hr_chart_frac(u32 v, u16 scale, u16 h) {
    i32 d = (i32)((u32)(MK32(((i16)scale) >> 0xF, scale) * 0x3E8));
    if (d == 0) return 0;
    i32 h4 = (i32)(v * 0x3C) / d;
    int f = (int)(h4 - 4 * (i32)(i16)h);
    return f < 0 ? 0 : f > 3 ? 3 : f;
}

/* a bar of the statistics chart in the screen buffer: its cap (the atlas
 * sprite cap) at row ytop, its foot at row 90, f quarter pixels higher than
 * the game's: the bar's sub-pixel rows from the cap down go f rows up (the
 * foot's repeat), in the columns where the cap's top row is opaque. */
void hr_chart_bar(int x, int ytop, int f, u32 cap) {
    if (!hr_on || all_nearest || f <= 0 || f > 3 || ytop < 1 || ytop > 90) return;
    u32 bmp = g_screen_buf, pix = FP32(bmp, 4), cpix = FP32(cap, 4);
    int stride = FP16(bmp, 0) << 3, w = FP16(cap, 0) << 3;
    u16 seg = FPSEG(pix), off = FPOFF(pix);
    int y0 = ytop - 1, m = 4 * (91 - y0);   /* from the pixel above the cap */
    for (int cx = x; cx < x + w && cx < stride; cx++) {
        if (!FP8(cpix, (u16)(cx - x))) continue;
        int ok = 1;
        for (int y = y0; y <= 90 && ok; y++) {
            u16 o = (u16)(off + y * stride + cx);
            ok = hr_cells[lin(seg, o)].base == rb(seg, o);
        }
        if (!ok) continue;
        for (int q = 0; q < 4; q++) {
            u8 sub[4 * 92];
            for (int y = y0; y <= 90; y++)
                for (int r = 0; r < 4; r++) sub[4 * (y - y0) + r] = hr_cells[lin(seg, (u16)(off + y * stride + cx))].px[4 * r + q];
            for (int Y = 4 - f; Y < m; Y++) {
                u8 v = sub[Y + f < m ? Y + f : m - 1];
                HrCell *c = &hr_cells[lin(seg, (u16)(off + (y0 + Y / 4) * stride + cx))];
                c->px[4 * (Y % 4) + q] = v;
                c->mask |= (u16)(1 << (4 * (Y % 4) + q));
                c->kvalid = 0;
            }
        }
    }
}

/* the transmitter map (ui_transmitter_info): map column mx, drawn at the
 * screen buffer's (mx + 0x30, y + 4).  A populated point is a dot, in the
 * colour of its density where a transmitter covers it: the game covers whole
 * points (a 16 x 16 stencil, DS:11F4, coverage_stamp); here every sub-pixel
 * against the circle the stencil approximates, radius 8 around its middle. */
static int tx_n, tx_west, tx_east;
static float tx_cx[32], tx_cy[32];
void hr_txmap_begin(u16 player) {
    if (!hr_on || all_nearest) return;
    tx_n = tx_west = tx_east = 0;
    for (u16 k = 1; k <= 32; k++) {
        if (!(((contracts(5, ((player * 0x42) + (k * 2)) + 9).ad) | (G16((((player * 0x1CE) + 0x1D09) + (k * 0xE)) + 6))) != 0
              && (tx(player, k).state) != 2))
            continue;
        if (k == 1) tx_west = 1;               /* the satellites: a whole half */
        else if (k == 2) tx_east = 1;
        else { tx_cx[tx_n] = (i16)tx(player, k).x + 8.0f; tx_cy[tx_n] = (i16)tx(player, k).y + 8.0f; tx_n++; }
    }
}
static u16 density(u32 map, int x, int y) {   /* map_density_at, without its time */
    return G16(((((FP16(map, ((y * 0x15) + (x >> 3)) << 1)) >> (((u8)x & 7) << 1)) & 3) << 1) + 0x1274);
}
void hr_txmap_column(int mx) {
    if (!hr_on || all_nearest || (mx < 84 && tx_west) || (mx >= 84 && tx_east)) return;
    u32 bmp = g_screen_buf, pix = FP32(bmp, 4), dots = g_room_gfx1;
    int stride = FP16(bmp, 0) << 3;
    u16 seg = FPSEG(pix), off = FPOFF(pix);
    for (int my = 0; my <= 108; my++) {
        u16 pw = density(g_population_map, mx, my);
        if (!pw) continue;
        u8 dot = sprite_colour(dots, 0xF, 0), cov = sprite_colour(dots, pw == 1 ? 0x10 : pw == 10 ? 0x11 : 0x12, 0);
        u8 px[16];
        int in_n = 0;
        for (int i = 0; i < 16; i++) {
            float sx = mx + (i % 4 + 0.5f) / 4, sy = my + (i / 4 + 0.5f) / 4;
            int in = 0;
            for (int t = 0; t < tx_n && !in; t++) {
                float dx = sx - tx_cx[t], dy = sy - tx_cy[t];
                in = dx * dx + dy * dy <= 64.0f;
            }
            px[i] = in ? cov : dot;
            in_n += in;
        }
        if (in_n == 0 || in_n == 16) continue;   /* wholly in or out: the game's dot as it is */
        u16 o = (u16)(off + (my + 4) * stride + mx + 0x30);
        HrCell *c = &hr_cells[lin(seg, o)];
        if (c->base != rb(seg, o)) continue;
        memcpy(c->px, px, 16);
        c->mask = 0xFFFF;
        c->kvalid = 0;
    }
}
