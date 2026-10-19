/* Far heap: Borland far heap (runtime library).
 *
 * Original modules 38C6, 3985 of MTV.EXE (Rainbow Arts, 1991).  Decompiled by
 * tools/decomp.py and hand-maintained since the freeze (git tag
 * generated-final).  Every function keeps its original address in the
 * comment above it; that address ties it to the machine-level twin in gen/
 * and to the difftest tables -- keep it when editing.
 */
#include "game.h"

/* 38C6:0087  Borland far heap: split size a2:a3 off the end of free block a0, return data ptr */
u32 heap_allocate_partial_block(u32 a0, u32 a2) {
    u16 ax;
    u16 dx;
    u32 l_4;
    u32 t3;
    FUNC_TICK(57);
    FP32(a0, 0) = FP32(a0, 0) - a2;
    l_4 = rtl_padd(a0, FP32(a0, 0));
    FP32(l_4, 0) = (a2 + 1);
    FP32(l_4, 4) = a0;
    if (rtl_pcmp(g_heap_last, a0) != 0) {
        t3 = rtl_padd(l_4, a2);
        a0 = MK32(HI16(t3), LO16(a0));
        a0 = MK32(HI16(a0), LO16(t3));
        FP32(a0, 4) = l_4;
    } else {
        g_heap_last = l_4;
    }
    ax = LO16(l_4) + 8;
    dx = HI16(l_4);
    return MK32(dx, ax);
}

/* 38C6:0137  Borland far heap: grow heap by sbrk for a new used block of size a0:a1 */
u32 heap_create_block(u32 a0) {
    u16 ax;
    u16 dx;
    u32 l_4;
    FUNC_TICK(39);
    l_4 = rtl_sbrk(a0);
    if (l_4 == 0xFFFFFFFF) {
        ax = 0;
        dx = 0;
    } else {
        FP32(l_4, 4) = g_heap_last;
        FP32(l_4, 0) = (a0 + 1);
        g_heap_last = l_4;
        ax = g_heap_last_lo + 8;
        dx = g_heap_last_hi;
    }
    return MK32(dx, ax);
}

/* 38C6:01A5  Borland far heap: first allocation, sets heap first/last */
u32 heap_create_first_block(u32 a0) {
    u16 ax;
    u16 dx;
    u32 l_4;
    FUNC_TICK(37);
    l_4 = rtl_sbrk(a0);
    if (l_4 == 0xFFFFFFFF) {
        ax = 0;
        dx = 0;
    } else {
        g_heap_first = l_4;
        g_heap_last = l_4;
        FP32(l_4, 0) = (a0 + 1);
        ax = LO16(l_4) + 8;
        dx = HI16(l_4);
    }
    return MK32(dx, ax);
}

/* 3985:000D  Borland free(ptr) (large model) -> farfree */
u16 rtl_free(u16 a0, u16 a1) {
    FUNC_TICK(8);
    return rtl_farfree(MK32(a1, a0));
}

/* 3985:001F  Borland far heap: link block into the circular free list (rover) */
void heap_insert_free_block(u32 a0) {
    u32 l_4;
    FUNC_TICK(50);
    if (rtl_pcmp(g_heap_rover, 0) == 0) {
        g_heap_rover = a0;
        FP32(a0, 8) = a0;
        FP32(a0, 0xC) = a0;
    } else {
        l_4 = FP32(g_heap_rover, 0xC);
        MEM32((u16)(FPSEG(g_heap_rover)), (u16)(FPOFF(g_heap_rover)) + 0xC) = a0;
        FP32(l_4, 8) = a0;
        FP32(a0, 0xC) = l_4;
        FP32(a0, 8) = g_heap_rover;
    }
    return;
}

/* 3985:00BE  Borland far heap: merge block a2 into preceding free block a0 */
u16 heap_join_free_blocks(u32 a0, u32 a2) {
    u32 l_4;
    FUNC_TICK(40);
    FP32(a0, 0) = FP32(a0, 0) + FP32(a2, 0);
    if (rtl_pcmp(g_heap_last, a2) != 0) {
        l_4 = rtl_padd(a2, FP32(a2, 0));
        FP32(l_4, 4) = a0;
    } else {
        g_heap_last = a0;
    }
    return LO16(rtl_pull_free_block(a2));
}

/* 3985:0136  Borland far heap: release the last block (and a free predecessor) back to DOS via brk */
u16 heap_free_last_block(void) {
    u16 ax;
    u32 l_4;
    u32 t1;
    u32 t5;
    FUNC_TICK(73);
    if (rtl_pcmp(g_heap_first, g_heap_last) != 0) {
        l_4 = FP32(g_heap_last, 4);
        if (((FP16(l_4, 2) & 0) | (FP16(l_4, 0) & 1)) != 0) {
            t5 = rtl_brk(g_heap_last);
            g_heap_last = l_4;
            ax = LO16(t5);
        } else {
            rtl_pull_free_block(l_4);
            if (rtl_pcmp(l_4, g_heap_first) != 0) {
                g_heap_last = FP32(l_4, 4);
            } else {
                g_heap_last = 0;
                g_heap_first = 0;
            }
            ax = LO16(rtl_brk(l_4));
        }
    } else {
        t1 = rtl_brk(g_heap_first);
        g_heap_last = 0;
        g_heap_first = 0;
        ax = LO16(t1);
    }
    return ax;
}

/* 3985:021B  Borland far heap: mark block free, merge with free neighbours */
u16 heap_free_inner_block(u32 a0) {
    u16 ax;
    u32 l_4;
    u32 l_8;
    FUNC_TICK(69);
    FP32(a0, 0) = FP32(a0, 0) - 1;
    l_8 = rtl_padd(a0, FP32(a0, 0));
    l_4 = FP32(a0, 4);
    if (((FP16(l_4, 2) & 0) | (FP16(l_4, 0) & 1)) != 0) {
L_02AF:
        heap_insert_free_block(a0);
    } else {
        if (rtl_pcmp(a0, g_heap_first) == 0) {
            goto L_02AF;
        } else {
            FP32(l_4, 0) = FP32(l_4, 0) + FP32(a0, 0);
            FP32(l_8, 4) = l_4;
            a0 = MK32(FPSEG(l_4), LO16(a0));
            a0 = MK32(HI16(a0), FPOFF(l_4));
        }
    }
    ax = FP16(l_8, 0) & 1;
    if (((FP16(l_8, 2) & 0) | (FP16(l_8, 0) & 1)) == 0) {
        ax = heap_join_free_blocks(a0, l_8);
    }
    return ax;
}

/* 3985:02E7  Borland farfree(ptr): header = ptr-8; last block -> free_last_block else free_inner_block */
u16 rtl_farfree(u32 a0) {
    u16 ax;
    u32 t1;
    FUNC_TICK(29);
    state_block_freed(a0);               /* a block in host memory goes back first (state.c) */
    ax = LO16(a0) | HI16(a0);
    if ((LO16(a0) | HI16(a0)) != 0) {
        t1 = rtl_padd(a0, 0xFFFFFFF8u);
        a0 = MK32(HI16(t1), LO16(a0));
        a0 = MK32(HI16(a0), LO16(t1));
        ax = LO16(a0);
        if (a0 == g_heap_last) {
            ax = heap_free_last_block();
        } else {
            ax = heap_free_inner_block(a0);
        }
    }
    return ax;
}
