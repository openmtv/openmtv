/* Resource files: the .rsc file manager (index, read, write).
 *
 * Original module 385D of MTV.EXE (Rainbow Arts, 1991).  Decompiled by
 * tools/decomp.py and hand-maintained since the freeze (git tag
 * generated-final).  Every function keeps its original address in the
 * comment above it; that address ties it to the machine-level twin in gen/
 * and to the difftest tables -- keep it when editing.
 */
#include "game.h"

/* 385D:0001  init resource manager: info {1 drive, current 0}, no files open, allocate 600 x 8-byte index */
u16 res_init_2(u32 a0) {
    u32 t1;
    FUNC_TICK(17);
    FP16(a0, 0) = 1;
    FP16(a0, 2) = 0;
    G32(0xA68A) = 0;
    t1 = rtl_malloc(0x12C0);
    g_res_index = t1;
    return 0;
}

/* 385D:0037  stub (returns 0): drive selection removed */
void res_select_drive(u16 a0) {
    FUNC_TICK(5);
    return;
}

/* 385D:003E  stub, called before res_close_all */
void res_flush(void) {
    FUNC_TICK(4);
    return;
}

/* 385D:0043  close all open *.rsc handles */
void res_close_all(u16 ax) {
    u16 si;
    FUNC_TICK(10);
    si = 0;
    while (si != g_res_nfiles) {
        LOOP_TICK(9);
        R.ax = ax;
        ax = LO16(rtl_close(g_res_handles(si)));
        si = si + 1;
    }
    G32(0xA68A) = 0;
    return;
}

/* 385D:0200  write count + ids of all resources with nonzero size to buffer a0 */
u16 res_list_present(u32 a0) {
    u16 di;
    u16 si;
    FUNC_TICK(14);
    si = 0;
    di = 0;
    while (si != 0x258) {
        LOOP_TICK(18);
        if ((FP16(g_res_index, (si << 3) + 2)) != 0) {
            FP16(a0, (di + 1) << 1) = si;
            di = di + 1;
        }
        si = si + 1;
    }
    FP16(a0, 0) = di;
    return 0;
}

/* 385D:0241  resource id lookup; returns 0xFFFF if absent */
u16 res_exists(u16 a0, u16 ax) {
    u16 si;
    u32 l_4;
    FUNC_TICK(36);
    if (g_res_open != 0) {
        si = 0;
    } else {
        R.ax = ax;
        rtl_res_open_all();
        si = 1;
    }
    l_4 = MK32(0, FP16(g_res_index, (a0 << 3) + 2));
    ax = FP16(g_res_index, (a0 << 3) + 2);
    if (si != 0) {
        res_close_all(ax);
    }
    if ((LO16(l_4) | HI16(l_4)) == 0) {
        l_4 = 0xFFFFFFFF;
    }
    return LO16(l_4);
}

/* 385D:029A  read whole resource a0 into far buffer (a1 off, a2 seg) */
u16 res_read(u16 a0, u8 *buf, u16 ax) {
    u16 di;
    u16 si;
    u32 l_4;
    u32 t13;
    FUNC_TICK(64);
    si = a0;
    if (g_res_open != 0) {
        di = 0;
    } else {
        R.ax = ax;
        rtl_res_open_all();
        di = 1;
    }
    R.es = FPSEG(g_res_index);
    rtl_lseek(FP16(g_res_index, si << 3), W2(FP32(g_res_index, (si << 3) + 4)), 0);
    t13 = rtl_read(FP16(g_res_index, si << 3), buf, FP16(g_res_index, (si << 3) + 2));
    l_4 = MK32(0, LO16(t13));
    ax = LO16(t13);
    if (di != 0) {
        res_close_all(ax);
    }
    return LO16(l_4);
}

/* 385D:0336  write a3 bytes from far buffer (a1,a2) over resource a0 in its RSC file (save games) */
u16 res_write(u16 a0, const u8 *buf, u16 a3, u16 ax) {
    u16 di;
    u16 si;
    u32 l_4;
    u32 t10;
    FUNC_TICK(58);
    di = a0;
    if (g_res_open != 0) {
        si = 0;
    } else {
        R.ax = ax;
        rtl_res_open_all();
        si = 1;
    }
    R.es = FPSEG(g_res_index);
    rtl_lseek(FP16(g_res_index, di << 3), W2(FP32(g_res_index, (di << 3) + 4)), 0);
    t10 = rtl_write(FP16(g_res_index, di << 3), buf, a3);
    l_4 = MK32(0, LO16(t10));
    ax = LO16(t10);
    if (si != 0) {
        res_close_all(ax);
    }
    return LO16(l_4);
}

/* 385D:03C3  read a4 bytes at offset a3 of resource a0 into (a1,a2) */
void res_read_part(u16 a0, u8 *buf, u16 a3, u16 a4, u16 ax) {
    u16 di;
    u16 si;
    u32 l_4;
    u32 t4;
    u32 t11;
    FUNC_TICK(64);
    di = a0;
    if (g_res_open != 0) {
        si = 0;
    } else {
        R.ax = ax;
        rtl_res_open_all();
        si = 1;
    }
    t4 = (FP32(g_res_index, (di << 3) + 4)) + MK32(0, a3);
    R.es = FPSEG(g_res_index);
    rtl_lseek(FP16(g_res_index, di << 3), W2(t4), 0);
    t11 = rtl_read(FP16(g_res_index, di << 3), buf, a4);
    l_4 = MK32(0, LO16(t11));
    ax = LO16(t11);
    if (si != 0) {
        res_close_all(ax);
    }
    return;
}

/* 385D:045B  write a4 bytes at offset a3 of resource a0 from (a1,a2) */
void res_write_part(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 ax) {
    u16 di;
    u16 si;
    u32 l_4;
    u32 t4;
    u32 t11;
    FUNC_TICK(64);
    di = a0;
    if (g_res_open != 0) {
        si = 0;
    } else {
        R.ax = ax;
        rtl_res_open_all();
        si = 1;
    }
    t4 = (FP32(g_res_index, (di << 3) + 4)) + MK32(0, a3);
    R.es = FPSEG(g_res_index);
    rtl_lseek(FP16(g_res_index, di << 3), W2(t4), 0);
    t11 = rtl_write(FP16(g_res_index, di << 3), HPTR(MK32(a2, a1)), a4);
    l_4 = MK32(0, LO16(t11));
    ax = LO16(t11);
    if (si != 0) {
        res_close_all(ax);
    }
    return;
}
