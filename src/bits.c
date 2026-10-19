/* Bit sets: bit set/clear/test.
 *
 * Original module 35F1 of MTV.EXE (Rainbow Arts, 1991).  Decompiled by
 * tools/decomp.py and hand-maintained since the freeze (git tag
 * generated-final).  Every function keeps its original address in the
 * comment above it; that address ties it to the machine-level twin in gen/
 * and to the difftest tables -- keep it when editing.
 */
#include "game.h"

/* 35F1:000A  *(u16 far *)p |= 1 << n */
void bit_set(u8 *a0, u16 a2) {
    u16 l_2;
    FUNC_TICK(13);
    l_2 = 1 << ((u8)((u8)a2));
    HP16(a0, 0) = HP16(a0, 0) | l_2;
    return;
}

/* 35F1:0028  *(u16 far *)p &= ~(1 << n) */
void bit_clear(u32 a0, u16 a2) {
    u16 l_2;
    FUNC_TICK(14);
    l_2 = ~(1 << ((u8)((u8)a2)));
    FP16(a0, 0) = FP16(a0, 0) & l_2;
    return;
}

/* 35F1:0048  (w >> n) & 1 as a byte; used on the AI genre masks (g_ai_ad_req +0, bit genre-1) */
u16 bit_test(u16 a0, u16 a1) {
    u16 ax;
    u8 l_1;
    u16 l_4;
    FUNC_TICK(19);
    l_4 = 1 << ((u8)((u8)a1));
    ax = l_4;
    if ((a0 & l_4) == 0) {
        ax = (((u8)((u8)(ax >> 8))) << 8) | 0;
    } else {
        ax = (((u8)((u8)(ax >> 8))) << 8) | 1;
    }
    l_1 = (u8)ax;
    ax = (((u8)((u8)(ax >> 8))) << 8) | l_1;
    return ax;
}
