/* Utilities: min/max helpers.
 *
 * Original module 2DD4 of MTV.EXE (Rainbow Arts, 1991).  Decompiled by
 * tools/decomp.py and hand-maintained since the freeze (git tag
 * generated-final).  Every function keeps its original address in the
 * comment above it; that address ties it to the machine-level twin in gen/
 * and to the difftest tables -- keep it when editing.
 */
#include "game.h"

/* 2DD4:0007  signed min(a, b) */
u16 util_min(u16 a0, u16 a1) {
    u16 ax;
    FUNC_TICK(11);
    if (((i16)a0) <= ((i16)a1)) {
        ax = a0;
    } else {
        ax = a1;
    }
    return ax;
}

/* 2DD4:001E  signed max(a, b) (decompiler shows a 3rd, unused argument) */
u16 util_max(u16 a0, u16 a1, u16 a2) {
    u16 ax;
    FUNC_TICK(11);
    if (((i16)a0) <= ((i16)a1)) {
        ax = a1;
    } else {
        ax = a0;
    }
    return ax;
}

/* 2DD4:0035  signed 32-bit max of (a_lo,a_hi) and (b_lo,b_hi) */
u32 util_lmax(u32 a0, u32 a2) {
    u16 ax;
    u16 dx;
    FUNC_TICK(17);
    ax = LO16(a0);
    if (((i32)a0) > ((i32)a2)) {
        ax = LO16(a0);
        dx = HI16(a0);
    } else {
        ax = LO16(a2);
        dx = HI16(a2);
    }
    return MK32(dx, ax);
}
