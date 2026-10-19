/* Sound: driver initialisation, samples, room jingles.
 *
 * Original module 35F8 of MTV.EXE (Rainbow Arts, 1991).  Decompiled by
 * tools/decomp.py and hand-maintained since the freeze (git tag
 * generated-final).  Every function keeps its original address in the
 * comment above it; that address ties it to the machine-level twin in gen/
 * and to the difftest tables -- keep it when editing.
 */
#include "game.h"

/* 35F8:0003  load detector resource 99 and probe hardware; if a device is found load driver resource 98 else 95; digital sound disabled when resource 300 (SOURCE2) is missing */
u16 snd_init(void) {
    u16 ax;
    u16 si;
    u32 l_4;
    u32 l_8;
    u32 l_C;
    u32 t1;
    u32 t2;
    u32 t8;
    u32 t9;
    u32 t10;
    u32 t11;
    u32 t12;
    u32 t13;
    u16 t14;
    FUNC_TICK(171);
    t1 = mem_calloc(0x15C, 1);
    g_snd_detect_mem = t1;
    t2 = ptr_align_para(g_snd_detect_mem_lo, g_snd_detect_mem_hi);
    g_snd_detect = t2;
    res_read(0x63, HPTR(g_snd_detect), 0x63);
    l_4 = rtl_snd_detect_w(W2(g_snd_detect));
    si = 0;
    while (((i16)FP16(l_4, si * 0x50)) >= 0) {
        LOOP_TICK(15);
        if ((FP16(l_4, (si * 0x50) + 2)) == 1) break;
        si = si + 1;
    }
    if (((i16)FP16(l_4, si * 0x50)) < 0) {
        si = 0;
        while (((i16)FP16(l_4, si * 0x50)) >= 0) {
            LOOP_TICK(15);
            if ((FP16(l_4, (si * 0x50) + 2)) == 0) break;
            si = si + 1;
        }
        g_snd_digi = 0;
        l_C = FP32(l_4, (si * 0x50) + 0x48);
        l_8 = FP32(l_4, (si * 0x50) + 0x4C);
    } else {
        g_snd_digi = 1;
        l_C = FP32(l_4, (si * 0x50) + 0x48);
        l_8 = FP32(l_4, (si * 0x50) + 0x4C);
    }
    mem_free(g_snd_detect_mem_lo, g_snd_detect_mem_hi);
    if (g_snd_digi != 1) {
        t11 = mem_calloc(0xE1E, 1);
        g_snd_drv_mem = t11;
        t12 = ptr_align_para(g_snd_drv_mem_lo, g_snd_drv_mem_hi);
        g_snd_drv = t12;
        res_read(0x5F, HPTR(g_snd_drv), 0x5F);
        t13 = rtl_snd_drv_link_w(W2(g_snd_drv), W2(l_C), W2(l_8));
        R.ax = 0;
        R.dx = HI16(t13);
        rtl_snd_drv_output(0);
    } else {
        t8 = mem_calloc(0x1574, 1);
        g_snd_drv_mem = t8;
        t9 = ptr_align_para(g_snd_drv_mem_lo, g_snd_drv_mem_hi);
        g_snd_drv = t9;
        res_read(0x62, HPTR(g_snd_drv), 0x62);
        t10 = rtl_snd_drv_link_w(W2(g_snd_drv), W2(l_C), W2(l_8));
        R.ax = 0;
        R.dx = HI16(t10);
        rtl_snd_drv_output(0);
    }
    t14 = res_exists(0x12C, 0x12C);
    ax = t14;
    if (t14 == 0xFFFF) {
        g_snd_digi = 0;
    }
    return ax;
}

/* 35F8:0330  free the sample buffer */
void snd_free_sample(void) {
    FUNC_TICK(5);
    mem_free(g_snd_sample_mem_lo, g_snd_sample_mem_hi);
    return;
}

/* 35F8:0341  start playback of the loaded sample (driver entries 5 and 9: output on, start) */
u16 snd_play_sample(void) {
    FUNC_TICK(12);
    R.ax = 1;
    rtl_snd_drv_output(1);
    R.ax = 4;
    return LO16(rtl_snd_drv_play(4, 0));
}

/* 35F8:035C  stop sample playback (driver entries 9 and 5: stop, output muted) */
void snd_stop_sample(void) {
    FUNC_TICK(12);
    R.ax = 2;
    rtl_snd_drv_play(2, 0);
    R.ax = 0;
    rtl_snd_drv_output(0);
    return;
}

/* 35F8:037C  (room, owner): choose the room jingle sample (own rooms 1..5, rival room 0x5D, public rooms by id, building 0xFF) and play it; sub-screens 0x3D/0x3E/0x0D/0x40 keep the current one; skipped when Music option off */
u16 snd_room_music_start(u16 a0, u16 a1, u16 ax) {
    int cc;
    u16 di;
    u16 si;
    FUNC_TICK(172);
    si = a0;
    di = a1;
    if (g_opt_music == 0) {
        if (di != g_player) {
L_03BA:
            cc = si != 0xA;
            if (cc || g_snd_room_on == 0) {
                if (di == 0) {
L_03DB:
                    if (si == 0x3E) {
L_0408:
                        g_snd_room_on = si;
                    } else if (si == 0x3D) {
                        goto L_0408;
                    } else if (si == 0xD) {
                        goto L_0408;
                    } else if (si == 0x40) {
                        goto L_0408;
                    } else {
                        cc = si != 0;
                        if (cc || !(g_snd_room_on == 0x3D || g_snd_room_on == 0xD || g_snd_room_on == 0x40)) {
                            G32(0x1606) = MK32(si, 0);
                            if (g_snd_digi == 1) {
                                if (si != 0) {
                                    if (di != g_player) {
                                        if (si == 0) {
L_0491:
                                            if (si != 0xA) {
                                                if (si != 0xE) {
                                                    if (si != 0x18) {
                                                        if (si != 0x13) {
                                                            if (si != 0x12) {
                                                                if (si != 0xB) {
                                                                    if (si != 0x14) {
                                                                        if (si != 0x15) {
                                                                            if (si != 0x17) {
                                                                                if (si != 0xC) {
                                                                                    if (si != 0x10) {
                                                                                        if (si != 0x11) {
                                                                                            if (si != 8) {
                                                                                                if (si != 9) {
                                                                                                    if (si != 0x16) {
                                                                                                        if (si != 0x42) {
                                                                                                            if (si == 0x1F4) {
                                                                                                                g_snd_sample_res = 0x95;
                                                                                                            }
                                                                                                        } else {
                                                                                                            g_snd_sample_res = 0x56;
                                                                                                        }
                                                                                                    } else {
                                                                                                        g_snd_sample_res = 0x94;
                                                                                                    }
                                                                                                } else {
                                                                                                    g_snd_sample_res = 0x93;
                                                                                                }
                                                                                            } else {
                                                                                                g_snd_sample_res = 0x4F;
                                                                                            }
                                                                                        } else {
                                                                                            g_snd_sample_res = 0xFE;
                                                                                        }
                                                                                    } else {
                                                                                        g_snd_sample_res = 0x50;
                                                                                    }
                                                                                } else {
                                                                                    g_snd_sample_res = 0x51;
                                                                                }
                                                                            } else {
                                                                                g_snd_sample_res = 0x53;
                                                                            }
                                                                        } else {
                                                                            g_snd_sample_res = 0x54;
                                                                        }
                                                                    } else {
                                                                        g_snd_sample_res = 0x55;
                                                                    }
                                                                } else {
                                                                    g_snd_sample_res = 0x58;
                                                                }
                                                            } else {
                                                                g_snd_sample_res = 0x59;
                                                            }
                                                        } else {
                                                            g_snd_sample_res = 0x5A;
                                                        }
                                                    } else {
                                                        g_snd_sample_res = 0x5B;
                                                    }
                                                } else {
                                                    g_snd_sample_res = 0x5C;
                                                }
                                            } else {
                                                g_snd_sample_res = 0x5E;
                                            }
                                        } else {
                                            cc = di == 0;
                                            if (cc || di == g_player) {
                                                goto L_0491;
                                            } else {
                                                g_snd_sample_res = 0x5D;
                                            }
                                        }
                                    } else if (si != 1) {
                                        if (si != 2) {
                                            if (si != 4) {
                                                if (si != 5) {
                                                    if (si == 3) {
                                                        g_snd_sample_res = 0x60;
                                                    }
                                                } else {
                                                    g_snd_sample_res = 0x4E;
                                                }
                                            } else {
                                                g_snd_sample_res = 0x52;
                                            }
                                        } else {
                                            g_snd_sample_res = 0x57;
                                        }
                                    } else {
                                        g_snd_sample_res = 0x61;
                                    }
                                } else {
                                    g_snd_sample_res = 0xFF;
                                }
                                if (g_snd_sample_res != 0) {
                                    rtl_snd_load_song(g_snd_sample_res);
                                    ax = snd_play_sample();
                                }
                            }
                        } else {
                            goto L_0408;
                        }
                    }
                } else if (di == g_player) {
                    goto L_03DB;
                } else {
                    cc = si == 0;
                    if (cc || g_snd_room_on == 0) {
                        goto L_03DB;
                    } else {
                        goto L_0408;
                    }
                }
            } else {
                goto L_0408;
            }
        } else if (si != 0x19) {
            if (si != 0x3A) {
                cc = si != 1;
                if (cc || g_snd_room_on == 0 || g_snd_room_on == 0x1F) {
                    goto L_03BA;
                } else {
                    goto L_0408;
                }
            } else {
                goto L_0408;
            }
        } else {
            goto L_0408;
        }
    } else {
        goto L_0408;
    }
    return ax;
}

/* 35F8:0636  round a far pointer up to the next paragraph (offset & 0xFFF0, segment + 1) */
u32 ptr_align_para(u16 a0, u16 a1) {
    u16 ax;
    u16 dx;
    FUNC_TICK(15);
    ax = a0 & 0xFFF0;
    dx = a1 + 1;
    return MK32(dx, ax);
}
