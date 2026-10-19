/* Graphics assembler helpers in C (see rtl.h): copies between the game's
 * bitmaps and the VGA screen (mode 13h, 320 bytes per row), rectangle fill,
 * transparent sprites and font glyphs, and the two decoders of the intro
 * animations (LZW, delta frames).
 *
 * A bitmap is a header { u16 width in 8-pixel units, u16 height, far pointer
 * to the pixels }, one byte per pixel, rows width * 8 bytes apart; colour 0
 * is transparent where a helper says so.  Most helpers take a far pointer to
 * a parameter block the caller fills in its stack frame.  All arithmetic is
 * 16-bit as in the original (offsets wrap inside their segment, counters of
 * 0 run 65536 times); the string instructions run forward (the game keeps DF
 * clear).  Time: each loop adds the sum of the original's basic blocks per
 * pass, 1 more per rep iteration and per pixel stored, with POLL() at the
 * original's loop heads.  The assembler helpers save and restore every
 * register except AX, which is set to what the original leaves there (some
 * callers pass it on).  The hr_* calls keep the 4x cells of --filter hr
 * (src/hr.h, nothing without it); the HRS(...) lines are the hooks of the
 * measuring run for it (src/hrs.h), empty in every other build. */
#include "game.h"
#include "hrs.h"
#include "hr.h"

typedef struct { u16 w8, h, off, seg; } Bitmap;

/* the bitmap header at seg:off */
static Bitmap bitmap_at(u16 off, u16 seg) {
    Bitmap b = { rw(seg, off), rw(seg, (u16)(off + 2)), rw(seg, (u16)(off + 4)), rw(seg, (u16)(off + 6)) };
    return b;
}
/* the bitmap a far pointer at seg:off points to */
static Bitmap bitmap_ptr_at(u16 off, u16 seg) {
    return bitmap_at(rw(seg, off), rw(seg, (u16)(off + 2)));
}
/* the bitmap a far pointer stored at host address p points to */
static Bitmap bitmap_ptr_hp(const u8 *p) {
    return bitmap_at(HP16(p, 0), HP16(p, 2));
}
#define BLK(n) HP16(blk, n)                     /* word n of the parameter block */

/* rep movsw: n words from sseg:*si to dseg:*di */
static void copy_words(u16 sseg, u16 *si, u16 dseg, u16 *di, u16 n) {
    for (; n; n--) {
        u16 w = rw(sseg, *si);
        HRS(hrs_copy(sseg, *si, dseg, *di, (u8)w));
        HRS(hrs_copy(sseg, (u16)(*si + 1), dseg, (u16)(*di + 1), (u8)(w >> 8)));
        hr_copy2(sseg, *si, dseg, *di);
        ww(dseg, *di, w); *si += 2; *di += 2;
    }
}

/* 8 pixels from sseg:*si to dseg:*di, colour 0 transparent: four lodsw, a
 * byte store per non-zero pixel.  *ax = the last word loaded; returns the
 * number of pixels stored (1 time unit each) */
static unsigned copy8_transparent(u16 sseg, u16 *si, u16 dseg, u16 *di, u16 *ax) {
    unsigned stored = 0;
    for (int k = 0; k < 4; k++) {
        u16 s0 = *si, w = rw(sseg, s0);
        *si += 2;
        HRS(hrs_tput(sseg, s0, dseg, *di, (u8)w));
        hr_tput(sseg, s0, dseg, *di, (u8)w);
        if (w & 0xFF) { wb(dseg, *di, (u8)w); stored++; }
        ++*di;
        HRS(hrs_tput(sseg, (u16)(s0 + 1), dseg, *di, (u8)(w >> 8)));
        hr_tput(sseg, (u16)(s0 + 1), dseg, *di, (u8)(w >> 8));
        if (w >> 8) { wb(dseg, *di, (u8)(w >> 8)); stored++; }
        ++*di;
        *ax = w;
    }
    return stored;
}

/* ---- bitmaps and the screen ---- */

/* 374E:0009  copy a whole bitmap to the screen (segment g_vga_seg) at (x8 * 8, y) */
u32 rtl_vga_put_bitmap_w(u16 boff, u16 bseg, u16 x8, u16 y) {
    RTL_ENTRY();
    Bitmap b = bitmap_at(boff, bseg);
    u16 skip = (u16)(0x140 - (u16)(b.w8 << 3)), words = (u16)(b.w8 << 2);
    u16 di = (u16)((u32)y * 0x140 + (u16)(x8 << 3)), si = b.off;
    u16 vseg = g_vga_seg, rows = b.h;
    HRS(hrs_helper(HRS_VGA_PUT, b.seg, b.off, b.w8, b.h));
    hr_ki_draw(b.seg, si, (u16)(b.w8 << 3), (u16)(b.w8 << 3), rows, vseg, di, 0x140, 0);
    T += 40;
    do {
        POLL();
        copy_words(b.seg, &si, vseg, &di, words);
        di += skip;
        T += 2 + words + 3;
    } while (--rows);
    T += 10;
    R.ax = 0;
    return MK32(R.dx, R.ax);
}
void f_374E_0009(void) {
    u16 sp = R.sp, x8 = rw(R.ss, (u16)(sp + 8));
    ww(R.ss, (u16)(sp + 8), (u16)(x8 << 3));   /* the original shifts its argument in place */
    rtl_vga_put_bitmap_w(rw(R.ss, (u16)(sp + 4)), rw(R.ss, (u16)(sp + 6)), x8, rw(R.ss, (u16)(sp + 10)));
    R.sp += 4;
}

/* 3747:0006  fill a whole bitmap from the screen at (x8 * 8, y): saves what
 * lies under a cursor, a popup or a sprite */
u32 rtl_vga_get_bitmap_w(u16 boff, u16 bseg, u16 x8, u16 y) {
    RTL_ENTRY();
    Bitmap b = bitmap_at(boff, bseg);
    u16 skip = (u16)(0x140 - (u16)(b.w8 << 3)), words = (u16)(b.w8 << 2);
    u16 si = (u16)((u32)y * 0x140 + (u16)(x8 << 3)), di = b.off;
    u16 vseg = g_vga_seg, rows = b.h;
    HRS(hrs_helper(HRS_VGA_GET, vseg, si, 40, b.h));
    T += 40;
    do {
        POLL();
        copy_words(vseg, &si, b.seg, &di, words);
        si += skip;
        T += 2 + words + 3;
    } while (--rows);
    T += 10;
    R.ax = 0;
    return MK32(R.dx, R.ax);
}
void f_3747_0006(void) {
    u16 sp = R.sp, x8 = rw(R.ss, (u16)(sp + 8));
    ww(R.ss, (u16)(sp + 8), (u16)(x8 << 3));   /* the original shifts its argument in place */
    rtl_vga_get_bitmap_w(rw(R.ss, (u16)(sp + 4)), rw(R.ss, (u16)(sp + 6)), x8, rw(R.ss, (u16)(sp + 10)));
    R.sp += 4;
}

/* 383E:0007  opaque copy of a w8 * 8 x h rectangle at (sx8 * 8, sy) of a
 * bitmap to the screen (A000) at (dx8 * 8, dy).
 * Block: { src bitmap, sx8, sy, w8, h, dx8, dy } */
u32 rtl_vga_put_rect(const u8 *blk) {
    RTL_ENTRY();
    Bitmap src = bitmap_ptr_hp(blk);
    u16 width = (u16)(BLK(8) << 3), rows = BLK(10);
    u16 di = (u16)((u32)BLK(14) * 0x140 + (u16)(BLK(12) << 3));
    u16 sstride = (u16)(src.w8 << 3);
    u16 si = (u16)(src.off + (u16)(BLK(4) << 3) + (u16)((u32)BLK(6) * sstride));
    u16 sskip = (u16)(sstride - width), dskip = (u16)(0x140 - width), words = width >> 1;
    HRS(hrs_helper(HRS_VGA_RECT, src.seg, src.off, src.w8, src.h));
    hr_ki_draw(src.seg, si, sstride, width, rows, 0xA000, di, 0x140, 0);
    T += 58;
    do {
        POLL();
        copy_words(src.seg, &si, 0xA000, &di, words);
        si += sskip;
        di += dskip;
        T += 2 + words + 4;
    } while (--rows);
    T += 10;
    R.ax = dskip;
    return MK32(R.dx, R.ax);
}
void f_383E_0007(void) {
    rtl_vga_put_rect(HPTR(MK32(rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 4)))));
    R.sp += 4;
}

/* ---- bitmap to bitmap ---- */

/* 3755:000D  fill w8 * 8 x h pixels of a bitmap at (x8 * 8, y) with a colour */
u32 rtl_bitmap_fill_w(u16 boff, u16 bseg, u16 x8, u16 y, u16 w8, u16 h, u16 colour) {
    RTL_ENTRY();
    Bitmap b = bitmap_at(boff, bseg);
    u16 stride = (u16)(b.w8 << 3), width = (u16)(w8 << 3);
    u16 di = (u16)(b.off + (u16)(x8 << 3) + (u16)((u32)y * stride));
    u16 skip = (u16)(stride - width), words = width >> 1;
    u16 v = (u16)((colour & 0xFF) * 0x101), rows = h;
    HRS(hrs_helper(HRS_FILL, 0, 0, 0, 0));
    T += 35;
    do {
        POLL();
        for (u16 n = words; n; n--) {   /* rep stosw */
            HRS(hrs_fill(b.seg, di, (u8)v));
            HRS(hrs_fill(b.seg, (u16)(di + 1), (u8)v));
            hr_fill(b.seg, di, (u8)v);
            hr_fill(b.seg, (u16)(di + 1), (u8)v);
            ww(b.seg, di, v); di += 2;
        }
        di += skip;
        T += 2 + words + 3;
    } while (--rows);
    T += 8;
    R.ax = v;
    return MK32(R.dx, R.ax);
}
void f_3755_000D(void) {
    u16 sp = R.sp;
    rtl_bitmap_fill_w(rw(R.ss, (u16)(sp + 4)), rw(R.ss, (u16)(sp + 6)), rw(R.ss, (u16)(sp + 8)), rw(R.ss, (u16)(sp + 10)),
                      rw(R.ss, (u16)(sp + 12)), rw(R.ss, (u16)(sp + 14)), rw(R.ss, (u16)(sp + 16)));
    R.sp += 4;
}

/* 375B:0009  opaque copy of a whole bitmap into another at (x8 * 8, y).
 * Block: { src bitmap, dst bitmap, x8, y } */
u32 rtl_bitmap_put(const u8 *blk) {
    RTL_ENTRY();
    Bitmap dst = bitmap_ptr_hp(blk + 4), src = bitmap_ptr_hp(blk);
    u16 dstride = (u16)(dst.w8 << 3), width = (u16)(src.w8 << 3);
    u16 di = (u16)(dst.off + (u16)((u32)BLK(10) * dstride) + (u16)(BLK(8) << 3));
    u16 dskip = (u16)(dstride - width), words = width >> 1, si = src.off, rows = src.h;
    HRS(hrs_helper(HRS_BMP_PUT, src.seg, src.off, src.w8, src.h));
    hr_ki_draw(src.seg, si, width, width, rows, dst.seg, di, dstride, 0);
    T += 43;
    do {
        POLL();
        copy_words(src.seg, &si, dst.seg, &di, words);
        di += dskip;
        T += 2 + words + 3;
    } while (--rows);
    T += 9;
    R.ax = 0;
    return MK32(R.dx, R.ax);
}
void f_375B_0009(void) {
    rtl_bitmap_put(HPTR(MK32(rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 4)))));
    R.sp += 4;
}

/* 3762:0008  opaque copy of a w8 * 8 x h rectangle from (sx8 * 8, sy) of one
 * bitmap to (dx8 * 8, dy) of another.
 * Block: { src bitmap, sx8, sy, w8, h, dst bitmap, dx8, dy } */
u32 rtl_blit_opaque_aligned(const u8 *blk) {
    RTL_ENTRY();
    Bitmap dst = bitmap_ptr_hp(blk + 12), src = bitmap_ptr_hp(blk);
    u16 width = (u16)(BLK(8) << 3), rows = BLK(10);
    u16 dstride = (u16)(dst.w8 << 3), sstride = (u16)(src.w8 << 3);
    u16 di = (u16)(dst.off + (u16)((u32)BLK(18) * dstride) + (u16)(BLK(16) << 3));
    u16 si = (u16)(src.off + (u16)(BLK(4) << 3) + (u16)((u32)BLK(6) * sstride));
    u16 sskip = (u16)(sstride - width), dskip = (u16)(dstride - width), words = width >> 1;
    HRS(hrs_helper(HRS_BLIT_OA, src.seg, src.off, src.w8, src.h));
    hr_ki_draw(src.seg, si, sstride, width, rows, dst.seg, di, dstride, 0);
    T += 64;
    do {
        POLL();
        copy_words(src.seg, &si, dst.seg, &di, words);
        si += sskip;
        di += dskip;
        T += 2 + words + 4;
    } while (--rows);
    T += 10;
    R.ax = dskip;
    return MK32(R.dx, R.ax);
}
void f_3762_0008(void) {
    rtl_blit_opaque_aligned(HPTR(MK32(rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 4)))));
    R.sp += 4;
}

/* 3786:0005  as 3762:0008, colour 0 transparent */
u32 rtl_blit_transparent_aligned(const u8 *blk) {
    RTL_ENTRY();
    Bitmap dst = bitmap_ptr_hp(blk + 12), src = bitmap_ptr_hp(blk);
    u16 width = (u16)(BLK(8) << 3), rows = BLK(10);
    u16 dstride = (u16)(dst.w8 << 3), sstride = (u16)(src.w8 << 3);
    u16 di = (u16)(dst.off + (u16)((u32)BLK(18) * dstride) + (u16)(BLK(16) << 3));
    u16 si = (u16)(src.off + (u16)(BLK(4) << 3) + (u16)((u32)BLK(6) * sstride));
    u16 sskip = (u16)(sstride - width), dskip = (u16)(dstride - width), groups = width >> 3, ax = 0;
    HRS(hrs_helper(HRS_BLIT_TA, src.seg, src.off, src.w8, src.h));
    hr_ki_draw(src.seg, si, sstride, (u16)(groups << 3), rows, dst.seg, di, dstride, 1);
    T += 67;
    do {
        POLL();
        T += 1;
        u16 n = groups;
        do {                             /* loop: CX = 0 runs 65536 times */
            POLL();
            T += 29 + copy8_transparent(src.seg, &si, dst.seg, &di, &ax);
        } while (--n);
        si += sskip;
        di += dskip;
        T += 4;
    } while (--rows);
    T += 10;
    R.ax = ax;
    return MK32(R.dx, R.ax);
}
void f_3786_0005(void) {
    rtl_blit_transparent_aligned(HPTR(MK32(rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 4)))));
    R.sp += 4;
}

/* Setup of the shifted blits 376D and 37A2: block { src bitmap, shift, sx8,
 * sy, w8, h, dst bitmap, dx8, dy }, destination x = dx8 * 8 + shift.  Where
 * the rectangle does not end at the source's right edge, 8 - shift more
 * pixels are taken; where sx8 * 8 has a non-zero low byte (the original
 * tests a byte: sx8 not a multiple of 32) the copy also starts shift pixels
 * further left in both bitmaps.  The row advances follow that width, but the
 * copy loops round it down (to words, to 8-pixel groups): with a width that
 * is not a multiple of the unit the rows drift, as in the original (only a
 * whole sprite from sx8 = 0 is safe). */
typedef struct { u16 sseg, si, dseg, di, width, sskip, dskip, rows, ax; } ShiftedBlit;
static ShiftedBlit shifted_setup(const u8 *blk) {
    ShiftedBlit s;
    Bitmap dst = bitmap_ptr_hp(blk + 14), src = bitmap_ptr_hp(blk);
    u16 dstride = (u16)(dst.w8 << 3), sstride = (u16)(src.w8 << 3);
    u16 shift = BLK(4), sx = (u16)(BLK(6) << 3), sy = BLK(8);
    s.dseg = dst.seg;
    s.di = (u16)(dst.off + (u16)((u32)BLK(20) * dstride) + (u16)(BLK(18) << 3) + shift);
    s.rows = BLK(12);
    s.width = (u16)(BLK(10) << 3);
    T += 59;
    if ((u16)(sx + s.width) != sstride) { s.width = (u16)(s.width + 8 - shift); T += 2; }
    s.sskip = (u16)(sstride - s.width);
    s.dskip = (u16)(dstride - s.width);
    s.sseg = src.seg;
    s.ax = (u16)((u32)sy * sstride);
    s.si = (u16)(src.off + sx + s.ax);
    T += 12;
    if ((u8)sx != 0) {
        s.di -= shift; s.width += shift; s.si -= shift; s.dskip -= shift; s.sskip -= shift;
        s.ax = shift;
        T += 6;
    }
    return s;
}

/* 376D:0003  opaque copy of a w8 * 8 x h rectangle from (sx8 * 8, sy) of one
 * bitmap to (dx8 * 8 + shift, dy) of another (see shifted_setup) */
u32 rtl_blit_opaque_shifted(const u8 *blk) {
    RTL_ENTRY();
    ShiftedBlit s = shifted_setup(blk);
    u16 words = s.width >> 1;
    HRS({ Bitmap b = bitmap_ptr_hp(blk); hrs_helper(HRS_BLIT_OS, b.seg, b.off, b.w8, b.h); });
    hr_ki_draw(s.sseg, s.si, (u16)(s.width + s.sskip), (u16)(words << 1), s.rows, s.dseg, s.di, (u16)(s.width + s.dskip), 0);
    T += 1;
    do {
        POLL();
        copy_words(s.sseg, &s.si, s.dseg, &s.di, words);
        s.di += s.dskip;
        s.si += s.sskip;
        T += 2 + words + 4;
    } while (--s.rows);
    T += 10;
    R.ax = s.ax;
    return MK32(R.dx, R.ax);
}
void f_376D_0003(void) {
    rtl_blit_opaque_shifted(HPTR(MK32(rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 4)))));
    R.sp += 4;
}

/* 37A2:0001  as 376D:0003, colour 0 transparent */
u32 rtl_blit_transparent_shifted(const u8 *blk) {
    RTL_ENTRY();
    ShiftedBlit s = shifted_setup(blk);
    u16 groups = s.width >> 3, ax = 0;
    HRS({ Bitmap b = bitmap_ptr_hp(blk); hrs_helper(HRS_BLIT_TS, b.seg, b.off, b.w8, b.h); });
    hr_ki_draw(s.sseg, s.si, (u16)(s.width + s.sskip), (u16)(groups << 3), s.rows, s.dseg, s.di, (u16)(s.width + s.dskip), 1);
    T += 3;
    do {
        POLL();
        T += 1;
        u16 n = groups;
        do {                             /* loop: CX = 0 runs 65536 times */
            POLL();
            T += 29 + copy8_transparent(s.sseg, &s.si, s.dseg, &s.di, &ax);
        } while (--n);
        s.di += s.dskip;
        s.si += s.sskip;
        T += 4;
    } while (--s.rows);
    T += 10;
    R.ax = ax;
    return MK32(R.dx, R.ax);
}
void f_37A2_0001(void) {
    rtl_blit_transparent_shifted(HPTR(MK32(rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 4)))));
    R.sp += 4;
}

/* 3795:000C  draw a whole sprite bitmap, colour 0 transparent, into a bitmap
 * at (x8 * 8 + xoff, y).  Block: { sprite bitmap, xoff, dst bitmap, x8, y } */
u32 rtl_sprite_put(const u8 *blk) {
    RTL_ENTRY();
    Bitmap dst = bitmap_ptr_hp(blk + 6), spr = bitmap_ptr_hp(blk);
    u16 dstride = (u16)(dst.w8 << 3), width = (u16)(spr.w8 << 3);
    u16 di = (u16)(dst.off + (u16)((u32)BLK(12) * dstride) + (u16)(BLK(10) << 3) + BLK(4));
    u16 dskip = (u16)(dstride - width), groups = width >> 3, rows = spr.h, si = spr.off, ax = 0;
    HRS(hrs_helper(HRS_SPRITE, spr.seg, spr.off, spr.w8, spr.h));
    hr_ki_draw(spr.seg, si, width, width, rows, dst.seg, di, dstride, 1);
    T += 48;
    do {
        POLL();
        T += 1;
        u16 n = groups;
        do {                             /* loop: CX = 0 runs 65536 times */
            POLL();
            T += 29 + copy8_transparent(spr.seg, &si, dst.seg, &di, &ax);
        } while (--n);
        di += dskip;
        T += 3;
    } while (--rows);
    T += 10;
    R.ax = ax;
    return MK32(R.dx, R.ax);
}
void f_3795_000C(void) {
    rtl_sprite_put(HPTR(MK32(rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 4)))));
    R.sp += 4;
}

/* 377A:000C  draw an 8-pixel-wide glyph or tile, colour 0 transparent, into
 * a bitmap at (x8 * 8 + xoff, y); the row counter is a byte (height mod 256,
 * 0 = 256 rows).  Block: { glyph bitmap, xoff, dst bitmap, x8, y } */
u32 rtl_glyph_put(const u8 *blk) {
    RTL_ENTRY();
    Bitmap dst = bitmap_ptr_hp(blk + 6), g = bitmap_ptr_hp(blk);
    u16 dstride = (u16)(dst.w8 << 3), dskip = (u16)(dstride - 8);
    u16 di = (u16)(dst.off + (u16)((u32)BLK(12) * dstride) + (u16)(BLK(10) << 3) + BLK(4));
    u16 si = g.off, ax = 0;
    u8 rows = (u8)g.h;
    HRS(hrs_helper(HRS_GLYPH, g.seg, g.off, g.w8, g.h));
    T += 42;
    do {
        POLL();
        T += 31 + copy8_transparent(g.seg, &si, dst.seg, &di, &ax);
        di += dskip;
    } while (--rows);
    T += 10;
    R.ax = ax;
    return MK32(R.dx, R.ax);
}
void f_377A_000C(void) {
    rtl_glyph_put(HPTR(MK32(rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 4)))));
    R.sp += 4;
}

/* ---- intro animations ---- */

/* 4456:0000  apply one delta frame to a bitmap's pixels (8 bytes after its
 * header).  The stream is read through the far pointer at poff:pseg, whose
 * offset is advanced past the frame: 'b' ends the frame, 'd' skips a pixel,
 * 'c' n... v is a run (n = the sum of bytes up to and with the first one
 * that is not FF) of colour v, or of skipped pixels if v is 'd'; any other
 * byte is a pixel.  Saves and restores every register; clears DF. */
u32 rtl_delta_apply_w(u16 poff, u16 pseg, u16 boff, u16 bseg) {
    RTL_ENTRY();
    u16 sseg = rw(pseg, (u16)(poff + 2)), si = rw(pseg, poff), di = (u16)(boff + 8);
    T += 18;
    for (;;) {
        POLL();
        u8 c = rb(sseg, si++);
        T += 1 + 2;
        if (c == 'b') break;
        T += 2;
        if (c == 'c') {
            u16 n = 0;
            u8 v;
            T += 2;
            do {
                POLL();
                v = rb(sseg, si++);
                n += v;
                T += 1 + 3;
            } while (v == 0xFF);
            v = rb(sseg, si++);
            T += 1 + 2;
            if (v == 'd') {
                di += n;
                T += 2;
            } else {
                for (u16 k = n; k; k--) { HRS(hrs_raw(bseg, di)); hr_raw(bseg, di); wb(bseg, di++, v); }   /* rep stosb */
                T += 1 + n + 1;
            }
        } else {
            T += 2;
            if (c == 'd') {
                di++;
                T += 2;
            } else {
                HRS(hrs_raw(bseg, di));
                hr_raw(bseg, di);
                wb(bseg, di++, c);
                T += 1 + 1;
            }
        }
    }
    ww(pseg, poff, si);
    T += 13;
    R.df = 0;
    return MK32(R.dx, R.ax);
}
void f_4456_0000(void) {
    u16 sp = R.sp;
    rtl_delta_apply_w(rw(R.ss, (u16)(sp + 4)), rw(R.ss, (u16)(sp + 6)), rw(R.ss, (u16)(sp + 8)), rw(R.ss, (u16)(sp + 10)));
    R.sp += 4;
}

/* LZW decoder state in DGROUP (36F4) */
#define lzw_dict         G32(0x9110)   /* far pointer to 4-byte entries { u16 prefix code (FFFF: none), u8 first byte, u8 last byte } */
#define lzw_bitpos       G16(0x9118)   /* bit position in the current input byte */
#define lzw_code         G16(0x911C)   /* the code just read */
#define lzw_width        G16(0xA128)   /* code width in bits */
#define lzw_left         G16(0xA12A)   /* input bytes left */
#define lzw_out          G32(0xA12C)   /* output pointer (the offset advances) */
#define lzw_out_off      G16(0xA12C)
#define lzw_in           G32(0xA230)   /* input pointer (the offset advances) */
#define lzw_in_off       G16(0xA230)
#define lzw_prev         G16(0xA434)   /* the previous code */
#define lzw_stack(i)     G8((u16)(0x906C + (i)))      /* a string is spelled backwards into 906C..910B */
#define lzw_mask(i)      G16((u16)(0x1692 + (i)))     /* i = 2 * bits */
/* dictionary entry c: segment, offset of a field */
#define LZW_SEG          HI16(lzw_dict)
#define LZW_AT(c, k)     ((u16)(LO16(lzw_dict) + (u16)((c) << 2) + (k)))

static u16 shl_cl(u16 v, u8 cl) { cl &= 0x1F; return cl < 16 ? (u16)(v << cl) : 0; }
static u8 shr_cl(u8 v, u8 cl) { cl &= 0x1F; return cl < 8 ? (u8)(v >> cl) : 0; }

/* 36F4:010C  entries 0..255 of the dictionary: the single bytes */
static void rtl_lzw_dict_init(void) {
    RTL_ENTRY();
    u16 i;
    T += 5;
    for (i = 0; T += 2, (i16)i < 0x100; i++) {
        POLL();
        wb(LZW_SEG, LZW_AT(i, 3), (u8)i);
        wb(LZW_SEG, LZW_AT(i, 2), (u8)i);
        ww(LZW_SEG, LZW_AT(i, 0), 0xFFFF);
        T += 20;
    }
    T += 3;
    R.ax = R.dx = 0xFF << 2;
    R.bx = LZW_AT(0xFF, 0); R.es = LZW_SEG;
}
void f_36F4_010C(void) {
    rtl_lzw_dict_init();
    R.sp += 4;
}

/* 36F4:015E  read the next lzw_width-bit code into lzw_code: high bits first,
 * from two or three input bytes, masks from the table at 1692 */
static void rtl_lzw_read_code(void) {
    RTL_ENTRY();
    u16 n = (u16)(lzw_width + lzw_bitpos);   /* bit position after the code */
    u16 ax, bx, dx;
    u8 cl, al;
    dx = rb(HI16(lzw_in), lzw_in_off);
    ax = lzw_mask((u16)(lzw_bitpos << 1)) & dx;
    cl = (u8)(n + 0xF8);
    lzw_code = shl_cl(ax, cl);
    lzw_in_off++; lzw_left--;
    T += 21;
    if ((i16)n > 16) {                   /* the code spans three bytes */
        lzw_bitpos -= 8;
        n -= 8;
        ax = rb(HI16(lzw_in), lzw_in_off);
        cl = (u8)(n + 0xF8);
        lzw_code |= shl_cl(ax, cl);
        lzw_in_off++; lzw_left--;
        T += 11;
    }
    al = rb(HI16(lzw_in), lzw_in_off);
    cl = (u8)(0x10 - (u8)n);
    al = shr_cl(al, cl);
    bx = (u16)((u16)(0x10 - n) << 1);
    lzw_code |= al & lzw_mask(bx);
    lzw_bitpos += (u16)(lzw_width + 0xFFF8);
    ax = lzw_bitpos;
    T += 17;
    if ((i16)ax > 7) {
        lzw_bitpos -= 8;
        lzw_in_off++; lzw_left--;
        T += 3;
    }
    T += 3;
    R.ax = ax; R.bx = bx; R.cl = cl; R.dx = dx; R.es = HI16(lzw_in);
}
void f_36F4_015E(void) {
    rtl_lzw_read_code();
    R.sp += 4;
}

/* 36F4:0205  write the string of lzw_code to the output: follow the prefix
 * chain (lzw_code ends as its first code), spelling it backwards into the
 * stack at 906C, then copy it out */
static void rtl_lzw_put_string(void) {
    RTL_ENTRY();
    u16 i = 0xA0;
    u16 next = rw(LZW_SEG, LZW_AT(lzw_code, 0));
    i--;
    u8 al = rb(LZW_SEG, LZW_AT(lzw_code, 3));
    lzw_stack(i) = al;
    T += 21;
    while (T += 2, next != 0xFFFF) {
        POLL();
        lzw_code = next;
        next = rw(LZW_SEG, LZW_AT(lzw_code, 0));
        i--;
        al = rb(LZW_SEG, LZW_AT(lzw_code, 3));
        lzw_stack(i) = al;
        T += 18;
    }
    T += 1;
    u16 bx = i, es = LZW_SEG;
    while (T += 2, (i16)i < 0xA0) {
        POLL();
        al = lzw_stack(i);
        bx = lzw_out_off; es = HI16(lzw_out);
        HRS(hrs_raw(es, bx));
        hr_raw(es, bx);
        wb(es, bx, al);
        i++;
        lzw_out_off++;
        T += 6;
    }
    T += 3;
    R.ax = (u16)((u16)(lzw_code << 2) & 0xFF00) | al;
    R.bx = bx; R.es = es;
}
void f_36F4_0205(void) {
    rtl_lzw_put_string();
    R.sp += 4;
}

/* 36F4:0009  LZW-decode a resource (input: 8-byte header, the compressed
 * size in its word 1) into out, with an 8 KB dictionary: 9-bit codes to
 * start, 100 resets the dictionary, 101 widens the codes by one bit, new
 * codes from 102 */
u32 rtl_lzw_decode_w(u16 ooff, u16 oseg, u16 ioff, u16 iseg, u16 doff, u16 dseg) {
    RTL_ENTRY();
    lzw_dict = MK32(dseg, doff);
    lzw_in = MK32(iseg, ioff);
    lzw_left = rw(iseg, (u16)(ioff + 2));
    lzw_out = MK32(oseg, ooff);
    lzw_in_off += 8;
    lzw_width = 9;
    lzw_bitpos = 0;
    u16 next = 0x102;                    /* the next free code */
    T += 20;
    rtl_lzw_dict_init();
    T += 1;
    rtl_lzw_read_code();
    lzw_prev = lzw_code;
    T += 3;
    rtl_lzw_put_string();
    T += 1;
    rtl_lzw_read_code();
    T += 1;
    while (T += 2, lzw_left != 0) {
        T += 1;
        POLL();
        u16 code = lzw_code;
        T += 3;
        if (code == 0x100) {             /* reset */
            lzw_width = 9;
            next = 0x102;
            T += 3;
            rtl_lzw_read_code();
            lzw_prev = lzw_code;
            T += 3;
            rtl_lzw_put_string();
            T += 1;
        } else if (T += 2, code == 0x101) {   /* one bit wider */
            lzw_width++;
            T += 2;
        } else {                         /* new entry: the previous string + the first byte of this one */
            T += 1;
            u16 eseg = LZW_SEG, e = LZW_AT(next, 0);
            next++;
            ww(eseg, e, lzw_prev);
            wb(eseg, (u16)(e + 2), rb(LZW_SEG, LZW_AT(lzw_prev, 2)));
            lzw_prev = lzw_code;
            /* read after the write above: a code that is this very entry works */
            wb(eseg, (u16)(e + 3), rb(LZW_SEG, LZW_AT(lzw_code, 2)));
            T += 29;
            rtl_lzw_put_string();
            T += 1;
        }
        T += 1;
        rtl_lzw_read_code();
    }
    T += 3;
    return MK32(R.dx, R.ax);
}
void f_36F4_0009(void) {
    u16 sp = R.sp;
    rtl_lzw_decode_w(rw(R.ss, (u16)(sp + 4)), rw(R.ss, (u16)(sp + 6)), rw(R.ss, (u16)(sp + 8)),
                     rw(R.ss, (u16)(sp + 10)), rw(R.ss, (u16)(sp + 12)), rw(R.ss, (u16)(sp + 14)));
    R.sp += 4;
}
