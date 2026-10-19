/* Measuring run for drawing at 4x resolution (docs/hochaufloesend.md, stage 1).
 *
 * Only build/mtv-dec-hrs is compiled with -DHR_SURVEY (make build/mtv-dec-hrs);
 * everywhere else HRS(...) is empty.  The hooks sit where stage 2 will carry
 * the sub-pixel cells: the drawing helpers (src/rtl_gfx.c), the sprite-bank
 * decoder (src/huffman.c), the RTL's memory fill and copy.  tools/hrsurvey.c
 * keeps a shadow of every byte (the value a helper last left there and where
 * it came from) and checks the screen against it at every frame -- what
 * stage 2's test "M[a] == base" will find.  The hooks only read: game state
 * and time stay as in build/mtv-dec. */
#ifndef MTV_HRS_H
#define MTV_HRS_H

enum {                                 /* the helpers, as tools/hrsurvey.py names them */
    HRS_VGA_PUT = 1, HRS_VGA_GET, HRS_VGA_RECT, HRS_FILL, HRS_BMP_PUT, HRS_BLIT_OA,
    HRS_BLIT_TA, HRS_BLIT_OS, HRS_BLIT_TS, HRS_SPRITE, HRS_GLYPH, HRS_DELTA, HRS_LZW
};

#ifdef HR_SURVEY
void hrs_helper(int id, u16 sseg, u16 soff, u16 w8, u16 h);   /* a helper begins (source bitmap, if any) */
void hrs_copy(u16 sseg, u16 soff, u16 dseg, u16 doff, u8 v);  /* opaque byte copy */
void hrs_tput(u16 sseg, u16 soff, u16 dseg, u16 doff, u8 v);  /* transparent byte (v = 0: not stored) */
void hrs_fill(u16 dseg, u16 doff, u8 v);                      /* a helper fills */
void hrs_raw(u16 dseg, u16 doff);                             /* the intro's decoders: no cells */
void hrs_res_read(u16 id, const u8 *buf);                     /* packed resource read into buf */
void hrs_bank_begin(u32 packed);                              /* a sprite bank is decoded from packed */
void hrs_bank_pixel(u32 dst, u8 v, u16 sprite);               /* one decoded pixel */
void hrs_memcpy(u8 *dst, const u8 *src, u16 n);               /* far_memcpy (before it copies) */
void hrs_setmem(u32 p, u16 n, u8 v);                          /* setmem / calloc */
#define HRS(x) x
#else
#define HRS(x) ((void)0)
#endif

#endif
