/* Text drawing: text rendering into bitmaps.
 *
 * Original module 0DDD of MTV.EXE (Rainbow Arts, 1991).  Decompiled by
 * tools/decomp.py and hand-maintained since the freeze (git tag
 * generated-final).  Every function keeps its original address in the
 * comment above it; that address ties it to the machine-level twin in gen/
 * and to the difftest tables -- keep it when editing.
 */
#include "game.h"

/* 0DDD:0001  draw a string on the screen buffer at x,y */
u16 text_draw(u16 a0, u16 a1, u16 a2, u16 a3) {
    FUNC_TICK(18);
    return text_draw_full(a0, a1, MK32(a3, a2), 0, 0, g_screen_buf_lo, g_screen_buf_hi, 0xFFFF);
}

/* 0DDD:002B  draw a string right-aligned at x into a buffer, then free it */
void text_draw_right_free(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5) {
    u16 t1;
    FUNC_TICK(28);
    t1 = text_width_px(MK32(a3, a2));
    text_draw_full(a0 - t1, a1, MK32(a3, a2), 0, 0, a4, a5, 0xFFFF);
    mem_free(a2, a3);
    return;
}

/* 0DDD:0071  draw a string on the screen buffer, then free it */
u16 text_draw_free(u16 a0, u16 a1, u16 a2, u16 a3) {
    FUNC_TICK(22);
    text_draw_full(a0, a1, MK32(a3, a2), 0, 0, g_screen_buf_lo, g_screen_buf_hi, 0xFFFF);
    return mem_free(a2, a3);
}

/* 0DDD:00A8  draw a string into a buffer at x,y */
void text_draw_to(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5) {
    FUNC_TICK(18);
    text_draw_full(a0, a1, MK32(a3, a2), 0, 0, a4, a5, 0xFFFF);
    return;
}

/* 0DDD:00D0  draw a string on the screen wrapped at n columns with flags */
u16 text_draw_wrap(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5) {
    FUNC_TICK(16);
    return text_draw_full(a0, a1, MK32(a3, a2), a4, a5, g_screen_buf_lo, g_screen_buf_hi, 0xFFFF);
}

/* 0DDD:00FA  draw a string into a buffer, then free it */
void text_draw_to_free(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5) {
    FUNC_TICK(22);
    text_draw_full(a0, a1, MK32(a3, a2), 0, 0, a4, a5, 0xFFFF);
    mem_free(a2, a3);
    return;
}

/* 0DDD:012F  draw a string centred on x into a buffer, then free it */
void text_draw_center_free(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5) {
    u16 t2;
    FUNC_TICK(31);
    t2 = (u16)((i32)(i16)text_width_px(MK32(a3, a2)) / 2);
    text_draw_full(a0 - t2, a1, MK32(a3, a2), 0, 0, a4, a5, 0xFFFF);
    mem_free(a2, a3);
    return;
}

/* 0DDD:017B  draw a string centred on x on the screen buffer */
u16 text_draw_center(u16 a0, u16 a1, u16 a2, u16 a3) {
    u16 t1;
    u16 t2;
    u16 t4;
    FUNC_TICK(27);
    t1 = g_screen_buf_hi;
    t2 = g_screen_buf_lo;
    t4 = (u16)((i32)(i16)text_width_px(MK32(a3, a2)) / 2);
    return text_draw_full(a0 - t4, a1, MK32(a3, a2), 0, 0, t2, t1, 0xFFFF);
}

/* 0DDD:01BC  draw a string centred on x into a buffer */
void text_draw_center_to(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5) {
    u16 t2;
    FUNC_TICK(27);
    t2 = (u16)((i32)(i16)text_width_px(MK32(a3, a2)) / 2);
    text_draw_full(a0 - t2, a1, MK32(a3, a2), 0, 0, a4, a5, 0xFFFF);
    return;
}

/* 0DDD:01FB  draw centred with all options (columns, flags, buffer, max lines) */
void text_draw_center_ex(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5, u16 a6, u16 a7, u16 a8) {
    u16 t2;
    FUNC_TICK(24);
    t2 = (u16)((i32)(i16)text_width_px(MK32(a3, a2)) / 2);
    text_draw_full(a0 - t2, a1, MK32(a3, a2), a4, a5, a6, a7, a8);
    return;
}

/* 0DDD:0239  draw with columns/flags into a buffer, then free the string */
void text_draw_ex_free(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5, u16 a6, u16 a7) {
    FUNC_TICK(20);
    text_draw_full(a0, a1, MK32(a3, a2), a4, a5, a6, a7, 0xFFFF);
    mem_free(a2, a3);
    return;
}

/* 0DDD:026E  draw with all options (columns, flags, buffer, max lines), then free the string */
void text_draw_full_free(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5, u16 a6, u16 a7, u16 a8) {
    FUNC_TICK(19);
    text_draw_full(a0, a1, MK32(a3, a2), a4, a5, a6, a7, a8);
    mem_free(a2, a3);
    return;
}

/* 0DDD:02A2  draw on the screen wrapped at n columns with flags, then free the string */
u16 text_draw_wrap_free(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5) {
    FUNC_TICK(20);
    text_draw_full(a0, a1, MK32(a3, a2), a4, a5, g_screen_buf_lo, g_screen_buf_hi, 0xFFFF);
    return mem_free(a2, a3);
}

/* 0DDD:02D9  draw text with word wrap: (x, y, str, columns (0 = 80), flags bit0 = break at space/'-', bit1 = draw style, dest, max lines); '~' = new line, line height 8 or 7 (0x8BA6) */
u16 text_draw_full(u16 a0, u16 a1, u32 a2, u16 a4, u16 a5, u16 a6, u16 a7, u16 a8) {
    u16 ax;
    int cc;
    u16 di;
    u16 si;
    u16 l_2;
    u32 l_6;
    u32 l_A;
    u32 l_E;
    u16 t2;
    u32 t4;
    u16 t9;
    FUNC_TICK(31);
    l_2 = 0;
    l_A = a2;
    l_6 = MK32(FPSEG(l_A), FPOFF(l_A) + LO16(rtl_strlen_w(W2(l_A))));
    if (a4 == 0) {
        a4 = 0x50;
    }
    for (;;) {
        LOOP_TICK(129);
        ax = LO16(l_A);
        if (LO16(l_A) > LO16(l_6)) break;
        ax = LO16(l_A) | HI16(l_A);
        if ((LO16(l_A) | HI16(l_A)) == 0) break;
        l_2 = 0;
        t2 = text_fit_chars(l_A, a4);
        l_E = mem_calloc(t2 + 1, 1);
        rtl_strncpy_w(W2(l_E), W2(l_A), t2);
        si = 0;
        di = t2;
        while (((i16)si) < ((i16)di)) {
            LOOP_TICK(6);
            if (FP8(l_E, si) != 0x7E) {
                si = si + 1;
                continue;
            } else {
                goto L_0366;
            }
        }
L_0372:
        if (((i16)si) >= ((i16)di)) {
            t4 = rtl_strlen_w(W2(l_A));
            if (((i16)LO16(t4)) <= ((i16)di)) {
                si = di;
            } else {
                si = di - 1;
                while (((i16)si) > 0) {
                    LOOP_TICK(14);
                    if (FP8(l_E, si) == 0x20) {
L_039F:
                        if ((a5 & 1) != 0) {
                            goto L_03AF;
                        } else {
L_03A6:
                            if (FP8(l_E, si) != 0) {
                                si = si - 1;
                                continue;
                            } else {
                                goto L_03AF;
                            }
                        }
                    } else if (FP8(l_E, si) != 0x2D) {
                        goto L_03A6;
                    } else {
                        goto L_039F;
                    }
                }
L_03C7:
                if (si == 0) {
                    si = di;
                }
            }
        }
        rtl_strncpy_w(LO16(l_E) + si, HI16(l_E), 0xD1A, DSEG, 1);
        if (g_font != 0) {
            if (g_font == 1) {
                a1 = a1 + 7;
            }
        } else {
            a1 = a1 + 8;
        }
        if ((a5 & 2) == 0) {
            text_draw_2(a0, a1, l_E, MK32(a7, a6), 0);
        } else {
            text_draw_2(a0, a1, l_E, MK32(a7, a6), 1);
        }
        t9 = mem_free(LO16(l_E), HI16(l_E));
        cc = FP8(l_A, si) != 0;
        ax = t9;
        if (!(cc)) break;
        a8 = a8 - 1;
        if (a8 == 0) break;
        l_A = MK32(HI16(l_A), LO16(l_A) + (si + l_2));
    }
    return ax;
L_0366:
    l_2 = 1;
    goto L_0372;
L_03AF:
    if (FP8(l_E, si) == 0x2D) {
        si = si + 1;
        goto L_03C7;
    } else {
        l_2 = 1;
        goto L_03C7;
    }
    goto L_03C7;
}

/* 0DDD:048D  number of lines a string wraps into at n columns */
u16 text_count_lines(u32 a0, u16 a2, u16 a3) {
    u16 ax;
    u16 di;
    u16 si;
    u16 l_2;
    u16 l_4;
    u16 l_6;
    u16 l_8;
    FUNC_TICK(25);
    l_6 = 0;
    l_4 = LO16(rtl_strlen_w(W2(a0)));
    l_2 = 0;
    while (FP8(a0, 0) != 0) {
        LOOP_TICK(68);
        l_8 = text_fit_chars(a0, a2);
        si = 0;
        di = 0;
        while (((i16)si) < ((i16)l_8)) {
            LOOP_TICK(6);
            if (FP8(a0, si) != 0x7E) {
                si = si + 1;
                continue;
            } else {
                goto L_04D6;
            }
        }
L_04E1:
        if (((i16)si) >= ((i16)l_8)) {
            if (((i16)LO16((rtl_strlen_w(W2(a0))))) <= ((i16)l_8)) {
                si = l_8;
            } else {
                si = l_8 - 1;
                while (((i16)si) > 0) {
                    LOOP_TICK(14);
                    if (FP8(a0, si) == 0x20) {
L_0511:
                        if ((a3 & 1) != 0) {
                            goto L_0521;
                        } else {
L_0518:
                            if (FP8(a0, si) != 0) {
                                si = si - 1;
                                continue;
                            } else {
                                goto L_0521;
                            }
                        }
                    } else if (FP8(a0, si) != 0x2D) {
                        goto L_0518;
                    } else {
                        goto L_0511;
                    }
                }
L_0537:
                if (si == 0) {
                    si = l_8;
                }
            }
        }
        l_6 = l_6 + 1;
        if (FP8(a0, si) == 0) break;
        a0 = MK32(HI16(a0), LO16(a0) + (si + di));
        l_2 = l_2 + (si + di);
        if (((i16)l_2) < ((i16)l_4)) {
            continue;
        } else {
            break;
        }
    }
    ax = l_6;
    return ax;
L_04D6:
    di = 1;
    goto L_04E1;
L_0521:
    if (FP8(a0, si) == 0x2D) {
        si = si + 1;
        goto L_0537;
    } else {
        di = 1;
        goto L_0537;
    }
    goto L_0537;
}
