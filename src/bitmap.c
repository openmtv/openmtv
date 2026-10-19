/* Bitmaps: bitmap allocation and sprite-bank sizes.
 *
 * Original modules 382C, 3833, 3839, 3848 of MTV.EXE (Rainbow Arts, 1991).  Decompiled by
 * tools/decomp.py and hand-maintained since the freeze (git tag
 * generated-final).  Every function keeps its original address in the
 * comment above it; that address ties it to the machine-level twin in gen/
 * and to the difftest tables -- keep it when editing.
 */
#include "game.h"

/* 382C:000E  allocate an image buffer (w, h) with an 8-byte header */
u32 bitmap_alloc(u16 a0, u16 a1) {
    u16 si;
    u32 l_4;
    FUNC_TICK(40);
    si = ((i16)(((i16)(((i16)a0) >> 1)) >> 1)) >> 1;
    if ((a0 & 7) != 0) {
        si = si + 1;
    }
    l_4 = rtl_malloc(((si * a1) << 3) + 8);
    FP16(l_4, 0) = si;
    FP16(l_4, 2) = a1;
    FP32(l_4, 4) = MK32(HI16(l_4), LO16(l_4) + 8);
    return l_4;
}

/* 3833:0001  identical copy of bitmap_alloc (w,h) used for the drag cursor image */
u32 bitmap_alloc2(u16 a0, u16 a1) {
    u16 si;
    u32 l_4;
    FUNC_TICK(40);
    si = ((i16)(((i16)(((i16)a0) >> 1)) >> 1)) >> 1;
    if ((a0 & 7) != 0) {
        si = si + 1;
    }
    l_4 = rtl_malloc(((si * a1) << 3) + 8);
    FP16(l_4, 0) = si;
    FP16(l_4, 2) = a1;
    FP32(l_4, 4) = MK32(HI16(l_4), LO16(l_4) + 8);
    return l_4;
}

/* 3839:0004  in-memory size of a decoded sprite bank: 2 + count*8 + sum(w8*h*8) */
u16 bank_mem_size(u32 a0) {
    u16 di;
    u16 si;
    u16 t14;
    FUNC_TICK(17);
    si = 0;
    di = (FP16(a0, 0) << 3) + 2;
    while (FP16(a0, 0) != si) {
        LOOP_TICK(23);
        t14 = di + (((FP16(a0, (si << 3) + 2)) * (FP16(a0, (si << 3) + 4))) << 3);
        si = (u16)(si + 1);
        di = t14;
    }
    return di;
}

/* 3848:0002  sprite bank resource a0: *a1 = packed size, *a3 = unpacked size (2 + n*8 + sum ceil(w/8)*8*h) */
void res_bank_sizes(u16 a0, u8 *a1, u8 *a3, u16 a5) {
    int cc;
    u16 di;
    u16 si;
    u16 l_2;
    u16 l_4;
    u32 l_8;
    FUNC_TICK(73);
    l_8 = rtl_malloc(1);
    res_read_part(a0, HPTR(l_8), 0, 1, 0);
    l_2 = FP8(l_8, 0);
    rtl_free(LO16(l_8), HI16(l_8));
    l_8 = rtl_malloc(l_2 << 1);
    res_read_part(a0, HPTR(l_8), 1, l_2 << 1, 1);
    l_4 = (l_2 << 3) + 2;
    si = 0;
    while (si != l_2) {
        LOOP_TICK(31);
        cc = (FP8(l_8, si << 1) & 7) == 0;
        di = ((i16)(((i16)(((i16)FP8(l_8, si << 1)) >> 1)) >> 1)) >> 1;
        if (!(cc)) {
            di = di + 1;
        }
        l_4 = l_4 + (((FP8(l_8, (si << 1) + 1)) * di) << 3);
        si = si + 1;
    }
    HP16(a3, 0) = l_4;
    HP16(a1, 0) = res_exists(a0, l_4);
    rtl_free(LO16(l_8), HI16(l_8));
    return;
}
