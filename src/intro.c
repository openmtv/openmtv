/* Intro player: LZW clips of the title sequence.
 *
 * Original module 365F of MTV.EXE (Rainbow Arts, 1991).  Decompiled by
 * tools/decomp.py and hand-maintained since the freeze (git tag
 * generated-final).  Every function keeps its original address in the
 * comment above it; that address ties it to the machine-level twin in gen/
 * and to the difftest tables -- keep it when editing.
 */
#include "game.h"

/* 365F:000B  load resource id: read 2-byte unpacked size from its header, alloc output and 8 KB dictionary, LZW-decode (36F4:0009, 9-bit codes growing, 0x100 reset, 0x101 widen); used for intro animations */
u32 res_load_lzw(u16 a0) {
    u16 ax;
    u16 dx;
    u32 rv_;
    u32 t1;
    u32 t2;
    u32 t3;
    u8 fr_[16] = {0};                  /* bytes bp-16 .. bp-1 of the original frame */
    u16 bp_ = (u16)(R.sp - 2);           /* the original frame's address, for AX values */
    FUNC_TICK(56);
    res_read_part(a0, (fr_ + 16 - 0xE), 0, 2, bp_ - 0xE);
    t1 = mem_calloc(FR16(fr_, 16, 0xE), 1);
    FR16(fr_, 16, 0x6) = HI16(t1);
    FR16(fr_, 16, 0x8) = LO16(t1);
    t2 = mem_calloc(0x2000, 1);
    FR16(fr_, 16, 0x2) = HI16(t2);
    FR16(fr_, 16, 0x4) = LO16(t2);
    t3 = res_load_raw(a0, LO16(t2));
    FR16(fr_, 16, 0xA) = HI16(t3);
    FR16(fr_, 16, 0xC) = LO16(t3);
    rtl_lzw_decode_w(W2(FR32(fr_, 16, 0x8)), W2(FR32(fr_, 16, 0xC)), W2(FR32(fr_, 16, 0x4)));
    mem_free(FR16(fr_, 16, 0xC), FR16(fr_, 16, 0xA));
    mem_free(FR16(fr_, 16, 0x4), FR16(fr_, 16, 0x2));
    ax = FR16(fr_, 16, 0x8);
    dx = FR16(fr_, 16, 0x6);
    rv_ = MK32(dx, ax);
    return rv_;
}

/* 365F:00A5  free previous intro scene, load background bank (scene table 0x161E: bg + up to 3 LZW delta clips per scene) into the 208x144 frame bitmap and put it on screen at 32,g_intro_row */
void intro_scene_load(void) {
    u32 t4;
    u32 t12;
    u32 t20;
    u32 t28;
    FUNC_TICK(150);
    if ((g_intro_clip1_lo | g_intro_clip1_hi) != 0) {
        mem_free_ptr(HPTR(DSP(0x1610)));
    }
    if ((g_intro_clip2_lo | g_intro_clip2_hi) != 0) {
        mem_free_ptr(HPTR(DSP(0x1614)));
    }
    if ((g_intro_clip3_lo | g_intro_clip3_hi) != 0) {
        mem_free_ptr(HPTR(DSP(0x1618)));
    }
    if ((g_intro_bg_lo | g_intro_bg_hi) != 0) {
        mem_free_ptr(HPTR(DSP(0x160C)));
    }
    t4 = res_load0(G16(((g_intro_scene - 1) << 3) + 0x161E));
    g_intro_bg = t4;
    bitmap_put(MK32(g_intro_bg_hi, g_intro_bg_lo + 2), g_intro_frame_bmp, 0, 0);
    if (g_intro_scene == 1) {
        mem_free_ptr(HPTR(DSP(0x160C)));
    }
    if ((G16(((g_intro_scene - 1) << 3) + 0x1620)) != 0) {
        t12 = res_load_lzw(G16(((g_intro_scene - 1) << 3) + 0x1620));
        g_intro_clip1 = t12;
    }
    if ((G16(((g_intro_scene - 1) << 3) + 0x1622)) != 0) {
        t20 = res_load_lzw(G16(((g_intro_scene - 1) << 3) + 0x1622));
        g_intro_clip2 = t20;
    }
    if (g_intro_skip(g_intro_scene * 4) != 0) {
        t28 = res_load_lzw(g_intro_skip(g_intro_scene * 4));
        g_intro_clip3 = t28;
    }
    rtl_vga_put_bitmap_w(W2(g_intro_frame_bmp), 4, g_intro_row);
    return;
}

/* 365F:0229  wait for vertical retrace, then apply the next delta frame from g_intro_anim_ptr to the frame bitmap (4456:0000; opcodes 'b' end, 'c' run/skip-run, 'd' skip pixel, else literal) */
void intro_decode_frame(void) {
    FUNC_TICK(9);
    rtl_wait_retrace();
    rtl_delta_apply_w(0x9062, DSEG, W2(g_intro_frame_bmp));
    return;
}

/* 365F:0244  one intro tick: at clip start pick next clip slot (restoring background on slot 1) and count passes (scenes 3 and 9 play 3 passes, scene 5 two, others one -> g_intro_scene_done); else decode next frame (scene 2 holds 5 ticks at frame 35); put frame on VGA */
void intro_scene_step(void) {
    u16 ax;
    FUNC_TICK(108);
    if (g_intro_frame == 0) {
        if (((i16)g_intro_clip_slot) > ((i16)g_intro_clip_count)) {
            g_intro_clip_slot = 1;
            g_intro_passes = g_intro_passes + 1;
            if ((g_intro_scene != 3 || g_intro_passes == 3) && (g_intro_scene != 9 || g_intro_passes == 3) && (g_intro_scene != 5 || g_intro_passes == 2)) {
                g_intro_scene_done = 1;
            }
        }
        if (g_intro_clip_slot == 1) {
            if (g_intro_scene != 1) {
                bitmap_put(MK32(g_intro_bg_hi, g_intro_bg_lo + 2), g_intro_frame_bmp, 0, 0);
            }
            g_intro_hold = 0;
        }
        ax = g_intro_clip_slot;
        if (g_intro_clip_slot == 1) {
            g_intro_clip = g_intro_clip1;
            g_intro_anim_ptr = MK32(FPSEG(g_intro_clip1), FPOFF(g_intro_clip1) + 1);
        } else if (ax == 2) {
            g_intro_clip = g_intro_clip2;
            g_intro_anim_ptr = MK32(FPSEG(g_intro_clip2), FPOFF(g_intro_clip2) + 1);
        } else if (ax == 3) {
            g_intro_clip = g_intro_clip3;
            g_intro_anim_ptr = MK32(FPSEG(g_intro_clip3), FPOFF(g_intro_clip3) + 1);
        }
        g_intro_clip_slot = g_intro_clip_slot + 1;
        g_intro_frame = g_intro_frame + 1;
    } else if (((i16)g_intro_hold) > 1000) {
        g_intro_hold = g_intro_hold - 1;
    } else {
        intro_decode_frame();
        g_intro_frame = g_intro_frame + 1;
        if (((u16)(((u16)(i8)FP8(g_intro_clip, 0)) + 1)) == g_intro_frame) {
            g_intro_frame = 0;
        }
    }
    if (g_intro_scene_done != 0) {
        g_intro_clip_slot = 1;
    } else {
        if (((u32)MK32(g_intro_scene, g_intro_frame)) == 0x20023 && g_intro_hold == 0) {
            g_intro_hold = 0x3ED;
        }
        rtl_vga_put_bitmap_w(W2(g_intro_frame_bmp), 4, g_intro_row);
    }
    return;
}

/* 365F:03A3  play the current intro scene until it ends; key or click sets g_intro_skip */
void intro_play_scene(void) {
    u16 t1;
    FUNC_TICK(5);
    g_intro_frame = 0;
    g_intro_scene_done = 0;
    for (;;) {
        LOOP_TICK(20);
        timer_start(0);
        intro_scene_step();
        for (;;) {
            LOOP_TICK(3);
            t1 = timer_read();
            if (t1 < g_intro_frame_ticks) {
                continue;
            } else {
                break;
            }
        }
        if (LO16((rtl_key_read())) != 0xFFFF || ((i16)LO16((rtl_mouse_read()))) > 0) {
            g_intro_skip(0) = 1;
        }
        if (((u32)(MK32(g_intro_scene_done, g_intro_skip(0)))) == 0) {
            continue;
        } else {
            break;
        }
    }
    return;
}

/* 365F:03F2  title / intro sequence (skipped by key or click) */
void intro_play(void) {
    u32 t1;
    u32 t2;
    u32 t3;
    u32 t4;
    u32 t5;
    u32 t10;
    FUNC_TICK(226);
    t1 = bitmap_alloc(0x140, 0xC8);
    g_intro_clip1 = t1;
    rtl_bitmap_fill_w(W2(g_intro_clip1), 0, 0, 0x28, 0xC8, 0);
    t2 = res_load0(0x133);
    g_intro_clip2 = t2;
    bitmap_put(MK32(g_intro_clip2_hi, g_intro_clip2_lo + 2), g_intro_clip1, 0, 1);
    mem_free(g_intro_clip2_lo, g_intro_clip2_hi);
    t3 = res_load0(0x134);
    g_intro_clip2 = t3;
    bitmap_put(MK32(g_intro_clip2_hi, g_intro_clip2_lo + 2), g_intro_clip1, 0x70, 1);
    mem_free(g_intro_clip2_lo, g_intro_clip2_hi);
    t4 = res_load0(0x135);
    g_intro_clip2 = t4;
    bitmap_put(MK32(g_intro_clip2_hi, g_intro_clip2_lo + 2), g_intro_clip1, 0xD0, 1);
    mem_free(g_intro_clip2_lo, g_intro_clip2_hi);
    rtl_vga_put_bitmap_w(W2(g_intro_clip1), 0, 0);
    mem_free(g_intro_clip1_lo, g_intro_clip1_hi);
    t5 = bitmap_alloc(0xD0, 0x90);
    g_intro_frame_bmp = t5;
    rtl_bitmap_fill_w(W2(g_intro_frame_bmp), 0, 0, 0x1A, 0x90, 0);
    g_intro_clip1 = 0;
    g_intro_clip2 = 0;
    timer_start(0);
    for (;;) {
        LOOP_TICK(3);
        if ((timer_read()) < 0x64) {
            continue;
        } else {
            break;
        }
    }
    for (;;) {
        LOOP_TICK(3);
        if (LO16((rtl_key_read())) != 0xFFFF) {
            continue;
        } else {
            break;
        }
    }
    g_intro_skip(0) = 0;
    while (g_intro_skip(0) == 0) {
        LOOP_TICK(205);
        g_intro_scene = 7;
        g_intro_row = 0x10;
        intro_scene_load();
        g_intro_clip_count = 1;
        g_intro_frame_ticks = 0xF;
        intro_play_scene();
        if (g_intro_skip(0) == 0) {
            g_intro_scene = 2;
            g_intro_row = 0x11;
            intro_scene_load();
            g_intro_clip_count = 1;
            g_intro_frame_ticks = 0xA;
            intro_play_scene();
        }
        if (g_intro_skip(0) == 0) {
            g_intro_scene = 9;
            g_intro_row = 0x10;
            intro_scene_load();
            g_intro_clip_count = 1;
            g_intro_frame_ticks = 0xA;
            g_intro_passes = 0;
            intro_play_scene();
        }
        if (g_intro_skip(0) == 0) {
            g_intro_scene = 8;
            g_intro_row = 0x10;
            intro_scene_load();
            g_intro_clip_count = 1;
            g_intro_frame_ticks = 7;
            intro_play_scene();
        }
        if (g_intro_skip(0) == 0) {
            g_intro_scene = 0xD;
            g_intro_row = 0x10;
            intro_scene_load();
            g_intro_clip_count = 2;
            g_intro_frame_ticks = 7;
            intro_play_scene();
        }
        if (g_intro_skip(0) == 0) {
            g_intro_scene = 1;
            g_intro_row = 0x10;
            intro_scene_load();
            g_intro_clip_count = 3;
            g_intro_frame_ticks = 2;
            intro_play_scene();
        }
        if (g_intro_skip(0) == 0) {
            g_intro_scene = 0xA;
            g_intro_row = 0x10;
            intro_scene_load();
            timer_start(0);
            for (;;) {
                LOOP_TICK(3);
                if ((timer_read()) < 0x96) {
                    continue;
                } else {
                    break;
                }
            }
        }
        if (g_intro_skip(0) == 0) {
            g_intro_scene = 5;
            g_intro_row = 0x10;
            intro_scene_load();
            g_intro_clip_count = 1;
            g_intro_frame_ticks = 0xA;
            g_intro_passes = 0;
            intro_play_scene();
        }
        if (g_intro_skip(0) == 0) {
            g_intro_scene = 0xA;
            g_intro_row = 0x10;
            intro_scene_load();
            g_intro_clip_count = 1;
            g_intro_frame_ticks = 7;
            intro_play_scene();
            bitmap_put(MK32(g_intro_bg_hi, g_intro_bg_lo + 2), g_intro_frame_bmp, 0, 0);
            rtl_vga_put_bitmap_w(W2(g_intro_frame_bmp), 4, g_intro_row);
        }
        if (g_intro_skip(0) == 0) {
            g_intro_scene = 3;
            g_intro_row = 0x10;
            intro_scene_load();
            g_intro_clip_count = 2;
            g_intro_frame_ticks = 2;
            g_intro_passes = 0;
            intro_play_scene();
        }
        if (g_intro_skip(0) == 0) {
            g_intro_scene = 4;
            g_intro_row = 0x11;
            intro_scene_load();
            g_intro_clip_count = 1;
            g_intro_frame_ticks = 6;
            intro_play_scene();
        }
        if (g_intro_skip(0) == 0) {
            g_intro_scene = 3;
            g_intro_row = 0x10;
            intro_scene_load();
            g_intro_clip_count = 2;
            g_intro_frame_ticks = 2;
            g_intro_passes = 0;
            intro_play_scene();
        }
        if (g_intro_skip(0) == 0) {
            g_intro_scene = 6;
            intro_scene_load();
            g_intro_clip_count = 1;
            g_intro_frame_ticks = 9;
            intro_play_scene();
        }
        if (g_intro_skip(0) == 0) {
            g_intro_scene = 3;
            intro_scene_load();
            g_intro_clip_count = 2;
            g_intro_frame_ticks = 2;
            g_intro_passes = 0;
            intro_play_scene();
        }
        if (g_intro_skip(0) == 0) {
            g_intro_scene = 0xC;
            intro_scene_load();
            g_intro_clip_count = 3;
            g_intro_frame_ticks = 5;
            intro_play_scene();
        }
        if (g_intro_skip(0) == 0) {
            g_intro_scene = 0xE;
            intro_scene_load();
            timer_start(0);
            for (;;) {
                LOOP_TICK(3);
                if ((timer_read()) < 0x96) {
                    continue;
                } else {
                    break;
                }
            }
            g_intro_clip_count = 1;
            g_intro_frame_ticks = 4;
            intro_play_scene();
        }
        if (g_intro_skip(0) != 0) continue;
        g_intro_scene = 0xB;
        intro_scene_load();
        g_intro_clip_count = 1;
        g_intro_frame_ticks = 0x96;
        intro_play_scene();
    }
    if ((g_intro_clip1_lo | g_intro_clip1_hi) != 0) {
        mem_free_ptr(HPTR(DSP(0x1610)));
    }
    if ((g_intro_clip2_lo | g_intro_clip2_hi) != 0) {
        mem_free_ptr(HPTR(DSP(0x1614)));
    }
    if ((g_intro_clip3_lo | g_intro_clip3_hi) != 0) {
        mem_free_ptr(HPTR(DSP(0x1618)));
    }
    if ((g_intro_bg_lo | g_intro_bg_hi) != 0) {
        mem_free_ptr(HPTR(DSP(0x160C)));
    }
    mem_free_ptr(HPTR(DSP(0x9056)));
    t10 = bitmap_alloc(0x140, 0xC8);
    g_intro_frame_bmp = t10;
    rtl_bitmap_fill_w(W2(g_intro_frame_bmp), 0, 0, 0x28, 0xC8, 0);
    screen_wipe(g_intro_frame_bmp_lo, g_intro_frame_bmp_hi, 0, 0, 0x140, 0xC8, 1, 1);
    mem_free_ptr(HPTR(DSP(0x9056)));
    return;
}
