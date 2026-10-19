/* Drawing at 4x resolution (--filter hr; docs/hochaufloesend.md): every
 * byte of the 1 MB carries a cell of 4x4 sub-pixels, kept by the drawing
 * helpers (src/rtl_gfx.c), made when a sprite bank is decoded (src/huffman.c)
 * and copied by far_memcpy and setmem.  The display shows a screen byte's
 * cell while the byte still holds the value the cell belongs to (its base);
 * a byte written past these hooks shows the MMPX picture there instead.
 * The game never reads a cell: game state, time and recordings are the same
 * with and without the option.  The hooks run before the helper writes the
 * byte, so the memory still holds its old value. */
#ifndef MTV_HR_H
#define MTV_HR_H
#include <string.h>

typedef struct {
    u8 px[16];                          /* palette indices, row by row */
    u16 mask;                           /* opaque sub-pixels (bit 4 * row + column) */
    u16 base;                           /* the byte value this cell belongs to; HR_NONE: none */
    /* AI pictures (docs/ki-bilder.md): the cell as the picture set has it -- what the
     * display shows while they are on (Ctrl+K switches); kvalid 0: the same as px */
    u8 kpx[16];
    u16 kmask;
    u8 kvalid;
} HrCell;
#define HR_NONE 0x100

extern int hr_on;                       /* --filter hr in the decompiled build */
extern HrCell *hr_cells;                /* by linear address, MEM_SIZE of them */
/* HR_KIMAP=FILE (docs/ki-bilder.md): where each byte's pixel came from -- a
 * sprite of a bank and its pixel -- carried along by the hooks; at the end
 * the file says, for every sprite drawn, what lay beneath its pixels */
extern uint64_t *hr_org;
void hr_ki_draw(u16 sseg, u16 soff, u16 sstride, u16 w, u16 h, u16 dseg, u16 doff, u16 dstride, int transparent);

void hr_compose(u32 d, const HrCell *s, u8 v, u8 old);   /* a partly opaque source cell (hr.c) */
void hr_res_read(u16 id, const u8 *buf);                 /* a packed resource was read into buf */
void hr_bank_begin(u32 packed);                          /* a sprite bank is decoded from packed */
void hr_bank_done(u32 bank);                             /* ... into bank: make its sprites' cells */
void hr_memcpy(u8 *dst, const u8 *src, u16 n);           /* far_memcpy, before it copies */
void hr_setmem(u32 p, u16 n, u8 v);                      /* setmem / calloc */
void hr_flat_px(u16 seg, u16 off, u8 v);                 /* our own code wrote a pixel: a flat cell */
const void *hr_ki_picture(u16 res, u16 k, const u8 *p, int w, int h, int stride);   /* a picture of ours in the AI set */
void hr_ki_px(const void *pic, int x, int y, u16 seg, u16 off, u8 v);   /* ... a pixel of it (pic NULL: a flat cell) */
void hr_bank_redo(u16 res, u32 bank);                    /* our own code changed a bank's pixels */
void hr_save(HrCell **buf, u16 cap, u16 seg, u16 off, u16 n);     /* n cells into *buf (allocated) */
void hr_restore(const HrCell *buf, u16 seg, u16 off, u16 n);
void hr_get(HrCell *buf, u16 seg, u16 off, u16 n);                /* n cells into buf */
void hr_glyph_px(u16 ss, u16 so, u16 ds, u16 dof, u8 v, u8 col);  /* a font pixel drawn by our own code */
/* finer than the game's pixels (stage 5): hooks after the original's drawing */
void hr_betty_meter(u32 bmp, u16 love, u32 atlas);
int hr_chart_frac(u32 v, u16 scale, u16 h);
void hr_chart_bar(int x, int ytop, int f, u32 cap);
void hr_txmap_begin(u16 player);
void hr_txmap_column(int mx);

static inline void hr_set_flat(HrCell *c, u8 v) {
    memset(c->px, v, 16);
    c->mask = v ? 0xFFFF : 0;
    c->base = v;
    c->kvalid = 0;
}

/* an opaque copy of one byte */
static inline void hr_copy1(u16 ss, u16 so, u16 ds, u16 dof) {
    if (!hr_on) return;
    hr_cells[lin(ds, dof)] = hr_cells[lin(ss, so)];
    if (hr_org) hr_org[lin(ds, dof)] = hr_org[lin(ss, so)];
}
/* an opaque copy of two bytes (one word: both cells read before either is written) */
static inline void hr_copy2(u16 ss, u16 so, u16 ds, u16 dof) {
    if (!hr_on) return;
    HrCell a = hr_cells[lin(ss, so)], b = hr_cells[lin(ss, (u16)(so + 1))];
    hr_cells[lin(ds, dof)] = a;
    hr_cells[lin(ds, (u16)(dof + 1))] = b;
    if (hr_org) {
        uint64_t oa = hr_org[lin(ss, so)], ob = hr_org[lin(ss, (u16)(so + 1))];
        hr_org[lin(ds, dof)] = oa;
        hr_org[lin(ds, (u16)(dof + 1))] = ob;
    }
}
/* a transparent copy of one byte of value v (0: the destination keeps its byte) */
static inline void hr_tput(u16 ss, u16 so, u16 ds, u16 dof, u8 v) {
    if (!hr_on) return;
    if (hr_org && v) hr_org[lin(ds, dof)] = hr_org[lin(ss, so)];
    const HrCell *s = &hr_cells[lin(ss, so)];
    if (s->base != v) {                 /* the source was written past the hooks */
        if (v) hr_cells[lin(ds, dof)] = *s;   /* (and so is the destination now: its base is not v) */
        return;
    }
    if (s->mask == 0xFFFF && v && (!s->kvalid || s->kmask == 0xFFFF)) { hr_cells[lin(ds, dof)] = *s; return; }
    if (!s->mask && !v && (!s->kvalid || !s->kmask)) return;
    hr_compose(lin(ds, dof), s, v, rb(ds, dof));
}
static inline void hr_fill(u16 ds, u16 dof, u8 v) {
    if (!hr_on) return;
    hr_set_flat(&hr_cells[lin(ds, dof)], v);
    if (hr_org) hr_org[lin(ds, dof)] = 0;
}
/* the intro's decoders write without cells */
static inline void hr_raw(u16 ds, u16 dof) {
    if (!hr_on) return;
    hr_cells[lin(ds, dof)].base = HR_NONE;
    if (hr_org) hr_org[lin(ds, dof)] = 0;
}

#endif
