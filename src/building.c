/* Building: building view: floors, elevator, walking figures.
 *
 * Original module 193D of MTV.EXE (Rainbow Arts, 1991).  Decompiled by
 * tools/decomp.py and hand-maintained since the freeze (git tag
 * generated-final).  Every function keeps its original address in the
 * comment above it; that address ties it to the machine-level twin in gen/
 * and to the difftest tables -- keep it when editing.
 */
#include "game.h"
#include "seat.h"

/* 193D:000F  render the building into g_building_buf for scroll y (a0): day/night sky switch, scroll the buffer by 30-px floors, draw walls, doors (kind sprite table 0x1180), owner flags, studio marks, floor numbers and elevator shafts */
void bld_render_floors(u16 a0) {
    u16 ax;
    u16 di;
    u16 s0;
    u16 si;
    u32 l_4;
    u32 l_8;
    u32 l_C;
    u32 l_10;
    u16 l_12;
    u16 l_14;
    u16 l_16;
    u16 l_18;
    u16 l_1A;
    u16 l_1C;
    u16 l_1E;
    u16 l_20;
    u16 l_22;
    u32 t3;
    u16 t15;
    u16 t16;
    u16 t17;
    u16 t18;
    u16 t19;
    u16 t20;
    u16 t21;
    u16 t23;
    u16 t26;
    u16 t30;
    u16 t32;
    u16 t37;
    u16 t38;
    u16 t39;
    u16 t40;
    u16 t41;
    u16 t42;
    u16 t43;
    u32 t44;
    FUNC_TICK(177);
    if ((((i16)(clock_hour())) >= 4 || g_daylight == 1) && (((i16)(clock_hour())) < 4 || g_daylight == 2)) {
        if (g_bld_full_redraw == 1) {
            building_draw_sides(g_daylight);
        }
    } else {
        if ((g_building_bg_lo | g_building_bg_hi) != 0) {
            mem_free_ptr(HPTR(DSP(0xF4)));
        }
        s0 = 0;
        if (g_daylight != 2) {
            ax = 1;
        } else {
            ax = 0;
        }
        t3 = res_cache_unpack(ax, s0);
        g_building_bg = t3;
        g_daylight = 3 - g_daylight;
        g_bld_full_redraw = 1;
        g_present_bld_only = 0x270F;
        building_draw_sides(g_daylight);
    }
    if (((u16)((u16)((u16)((i32)(i16)a0 / 30)) + 5)) != g_bld_drawn_top || g_bld_full_redraw == 1) {
        G16(0x7A46) = 1;
        g_bld_top_floor = (u16)((u16)((i32)(i16)a0 / 30)) + 5;
        g_clip_x1 = 0;
        g_clip_x2 = 0xD0;
        if (((u32)MK32(g_bld_drawn_top - 1, g_bld_full_redraw)) == ((u32)MK32(g_bld_top_floor, 0))) {
            g_clip_on = 0;
            l_22 = 0x1E;
            while (((i16)l_22) < 150) {
                LOOP_TICK(21);
                blit_opaque_rect(g_building_buf, g_building_buf, 0, l_22 + 0xFFE2, 0, l_22, 0xD0, 0x1E, 0 /* unused ax */);
                l_22 = l_22 + 0x1E;
            }
            g_clip_y1 = 0x78;
            g_clip_y2 = 0x96;
            l_16 = g_bld_top_floor + 0xFFFC;
            l_14 = g_bld_top_floor + 0xFFFC;
        } else if (((u32)MK32(g_bld_drawn_top + 1, g_bld_full_redraw)) == ((u32)MK32(g_bld_top_floor, 0))) {
            g_clip_on = 0;
            l_22 = 0x5A;
            while (((i16)l_22) > -30) {
                LOOP_TICK(21);
                blit_opaque_rect(g_building_buf, g_building_buf, 0, l_22 + 0x1E, 0, l_22, 0xD0, 0x1E, 0 /* unused ax */);
                l_22 = l_22 - 0x1E;
            }
            g_clip_y1 = 0;
            g_clip_y2 = 0x1E;
            l_16 = g_bld_top_floor;
            l_14 = g_bld_top_floor;
        } else {
            g_clip_y1 = 0;
            g_clip_y2 = 0x96;
            l_16 = g_bld_top_floor + 0xFFFC;
            l_14 = g_bld_top_floor;
        }
        g_clip_on = 1;
        g_bld_drawn_top = g_bld_top_floor;
        if (((i16)l_14) > 13) {
            l_12 = (g_bld_top_floor + 0xFFF3) * 0x1E;
            if (((i16)l_12) > ((i16)g_clip_y2)) {
                l_12 = g_clip_y2;
            }
            if (((i16)(clock_hour())) >= 4) {
                ax = 9;
            } else {
                ax = 1;
            }
            l_20 = ax;
            si = 0;
            while (((i16)si) < 208) {
                LOOP_TICK(52);
                di = 0;
                while (((i16)di) < ((i16)l_12)) {
                    LOOP_TICK(47);
                    if (((i16)(di + 7)) <= ((i16)g_clip_y2)) {
                        t19 = g_building_buf_hi;
                        t20 = g_building_buf_lo;
                        t21 = col_to_px(l_20);
                        bitmap_put(MK32(g_band_tiles_hi, g_band_tiles_lo + t21), MK32(t19, t20), si, di);
                    } else {
                        t15 = g_clip_y2 - di;
                        t16 = g_building_buf_hi;
                        t17 = g_building_buf_lo;
                        t18 = col_to_px(l_20);
                        blit_opaque_rect(MK32(g_band_tiles_hi, g_band_tiles_lo + t18), MK32(t16, t17), si, di, 0, 0, 8, t15, 0 /* unused ax */);
                    }
                    di = di + 8;
                }
                si = si + 8;
            }
            draw_sprite_to(MK32(G16(0xFE), G16(0xFC) + 2), g_building_buf_lo, g_building_buf_hi, 0, ((g_bld_top_floor + 0xFFF3) * 0x1E) + 0xFFDB);
        }
        l_22 = 1;
        while (((i16)l_22) <= 30) {
            LOOP_TICK(44);
            l_12 = (0x1C2 - (l_22 * 0xD)) - ((0xF - g_bld_top_floor) * 0x1E);
            if (!(((i16)l_12) < -12 || ((i16)l_12) > 150)) {
                blit_opaque_to(MK32(g_band_tiles_hi, g_band_tiles_lo + 0x32), g_building_buf_lo, g_building_buf_hi, 0, l_12);
                blit_opaque_to(MK32(g_band_tiles_hi, g_band_tiles_lo + 0x32), g_building_buf_lo, g_building_buf_hi, 0xC0, l_12);
            }
            l_22 = l_22 + 1;
        }
        l_22 = l_16;
        while (((i16)l_22) <= ((i16)l_14)) {
            LOOP_TICK(353);
            l_4 = floors_at(l_22);
            if (((i16)l_22) <= 13) {
                l_12 = (0x96 - (((l_22 - g_bld_top_floor) + 4) * 0x1E)) + 0xFFFD;
                if (!(((i16)l_12) < -2 || ((i16)l_12) > 150)) {
                    si = 0x10;
                    while (((i16)si) < 192) {
                        LOOP_TICK(14);
                        bitmap_put(MK32(g_band_tiles_hi, g_band_tiles_lo + 0x3A), g_building_buf, si, l_12);
                        si = si + 8;
                    }
                }
                t23 = col_to_px(((AT(floor, l_4).wall_style) << 1) - 1);
                l_10 = MK32(FPSEG(g_band_tiles), FPOFF(g_band_tiles) + t23);
                t26 = col_to_px((AT(floor, l_4).wall_style) << 1);
                l_C = MK32(FPSEG(g_band_tiles), FPOFF(g_band_tiles) + t26);
                l_12 = l_12 - 0x1B;
                si = 0x10;
                while (((i16)si) < 192) {
                    LOOP_TICK(45);
                    di = l_12 + 3;
                    while (((i16)(l_12 + 0x1B)) > ((i16)di)) {
                        LOOP_TICK(23);
                        bitmap_put(l_10, g_building_buf, si, di);
                        if (si == 0x10) {
                            bitmap_put(l_C, g_building_buf, si, di);
                        }
                        di = di + 8;
                    }
                    blit_opaque_rect(l_C, g_building_buf, si, l_12, 0, 0, 8, 5, 0 /* unused ax */);
                    si = si + 8;
                }
                si = 0;
                while (((i16)si) <= 10) {
                    LOOP_TICK(186);
                    if (FP8(l_4, si + 0x14) == 0) break;
                    l_20 = 0;
                    l_18 = FP8(l_4, si + 0x14) - 1;
                    t30 = col_to_px((u16)(i8)(G8((l_18 * 3) + 0x1180)));
                    l_8 = MK32(FPSEG(g_bld_tiles), FPOFF(g_bld_tiles) + t30);
                    l_1C = (u16)(i8)(G8((l_18 * 3) + 0x1181));
                    l_1A = (u16)(i8)(G8((l_18 * 3) + 0x1182));
                    if (((i16)l_18) <= 2) {
                        if (FP8(l_4, si + 0x3C) != 0) {
                            t32 = col_to_px(((u16)(i8)(G8((l_18 * 3) + 0x1180))) + 1);
                            l_8 = MK32(FPSEG(g_bld_tiles), FPOFF(g_bld_tiles) + t32);
                        }
                    }
                    if (l_18 == 0xB) {
                        if (((i16)(clock_hour())) >= 4) {
                            l_8 = MK32(FPSEG(g_bld_side_tiles), FPOFF(g_bld_side_tiles) + 0x32);
                        } else {
                            l_20 = 3;
                            l_8 = MK32(FPSEG(g_bld_side_tiles), FPOFF(g_bld_side_tiles) + 0x2A);
                            l_1C = 8;
                            l_1A = 4;
                        }
                    }
                    t37 = FP8(l_4, si + 0xA) + l_20;
                    l_1E = FP8(l_4, si + 0xA) + l_20;
                    if (t37 == 0) {
                        l_1E = (0x1B - l_1A) + 1;
                    }
                    blit_transparent(l_8, g_building_buf, FP8(l_4, si) + 0xFFC8, l_1E + l_12, 0, 0, l_1C, l_1A, 0 /* unused ax */);
                    if (FP8(l_4, si + 0x46) != 0) {
                        if ((players(FP8(l_4, si + 0x46)).out) == 0) {
                            t38 = FP8(l_4, si) + 0xFFDA;
                            t39 = g_building_buf_hi;
                            t40 = g_building_buf_lo;
                            t41 = col_to_px(FP8(l_4, si + 0x46));
                            draw_sprite_to(MK32(g_bld_side_tiles_hi, g_bld_side_tiles_lo + t41), t40, t39, t38, l_12 + 4);
                        }
                    }
                    if (FP8(l_4, si + 0x1E) == 5) {
                        draw_sprite_to(MK32(g_bld_side_tiles_hi, g_bld_side_tiles_lo + 0x1A), g_building_buf_lo, g_building_buf_hi, FP8(l_4, si) + 0xFFC4, l_12 + 4);
                    }
                    si = si + 1;
                }
                draw_sprite_to(MK32(g_bld_sprites_hi, g_bld_sprites_lo + 0x22), g_building_buf_lo, g_building_buf_hi, 2, l_12 + 8);
                t42 = g_building_buf_hi;
                t43 = g_building_buf_lo;
                t44 = str_format(DSP(0x11C6), l_22);
                l_8 = t44;
                text_draw_center_ex(8, l_12 + 2, LO16(t44), HI16(t44), 0, 0, t43, t42, 0xFFFF);
                mem_free(LO16(l_8), HI16(l_8));
            }
            if (((i16)l_22) <= 13) {
                bld_draw_elevator(l_22);
            }
            l_22 = l_22 + 1;
        }
        G16(0x7A46) = 0;
    }
    g_clip_y1 = 0;
    g_clip_y2 = 0x96;
    l_22 = g_bld_top_floor + 0xFFFC;
    while (((i16)l_22) <= ((i16)g_bld_top_floor)) {
        LOOP_TICK(10);
        if (((i16)l_22) <= 13) {
            bld_draw_elevator(l_22);
        }
        l_22 = l_22 + 1;
    }
    return;
}

/* 193D:077D  copy the visible part of g_building_buf (208x109) to the screen buffer at x 56 */
void bld_blit_view(void) {
    FUNC_TICK(24);
    blit_opaque_rect(g_building_buf, g_screen_buf, 0x38, 4, 0, (g_bld_top_floor * 0x1E) - (g_bld_scroll + 0x6D), 0xD0, 0x6D, 0 /* unused ax */);
    return;
}

/* 193D:07BB  draw elevator shaft/cabin on floor a0 (cabin floor g_elevator_floor, offset g_elevator_offset, doors g_elevator_door, figures riding) */
void bld_draw_elevator(u16 a0) {
    u16 di;
    u16 si;
    u16 l_2;
    u16 l_4;
    u16 l_6;
    u16 t1;
    u16 t3;
    u16 t4;
    u16 t5;
    FUNC_TICK(239);
    l_6 = 0;
    l_4 = 0;
    l_2 = 0;
    if (((i16)a0) <= ((i16)g_bld_top_floor) && ((i16)(g_bld_top_floor + 0xFFFC)) <= ((i16)a0)) {
        t1 = (0x96 - (((a0 - g_bld_top_floor) + 4) * 0x1E)) + 0xFFE2;
        draw_sprite_to(MK32(g_bld_sprites_hi, g_bld_sprites_lo + 0x72), g_building_buf_lo, g_building_buf_hi, 0x61, (0x96 - (((a0 - g_bld_top_floor) + 4) * 0x1E)) + 0xFFE4);
        draw_sprite_to(MK32(g_bld_sprites_hi, g_bld_sprites_lo + 0x7A), g_building_buf_lo, g_building_buf_hi, g_elevator_floor + 0x60, t1 + 2);
        si = t1;
        if (a0 != g_elevator_floor) {
            if (((u16)(g_elevator_floor + 1)) != a0 || ((i16)g_elevator_offset) >= 0) {
                if (!(((u16)(g_elevator_floor - 1)) != a0 || ((i16)g_elevator_offset) <= 10)) {
                    l_6 = si + 7;
                    l_4 = 0x1E - g_elevator_offset;
                    l_2 = 0x14 - (0x1E - g_elevator_offset);
                }
            } else {
                l_6 = (g_elevator_offset + si) + 0x25;
                l_4 = 0;
                l_2 = 0x14 - (g_elevator_offset + 0x1E);
            }
        } else if (((i16)g_elevator_offset) <= 0) {
            if (((i16)g_elevator_offset) >= 0 || ((i16)g_elevator_offset) < -20) {
                if (g_elevator_offset == 0) {
                    l_6 = si + 7;
                    l_4 = 0;
                    l_2 = 0x14;
                }
            } else {
                l_6 = si + 7;
                l_4 = -(u16)(g_elevator_offset);
                l_2 = g_elevator_offset + 0x14;
            }
        } else {
            l_6 = (si + g_elevator_offset) + 7;
            l_4 = 0;
            l_2 = 0x14 - g_elevator_offset;
        }
        if (((i16)l_2) > 0) {
            draw_sprite_to(MK32(g_bld_tiles_hi, g_bld_tiles_lo + 0x72), g_building_buf_lo, g_building_buf_hi, 0x5D, si + 4);
            blit_opaque_rect(MK32(g_band_tiles_hi, g_band_tiles_lo + 0x4A), g_building_buf, 0x60, l_6, 0, l_4, 0x10, l_2, 0 /* unused ax */);
            di = 5;
            while (((i16)di) >= 0) {
                LOOP_TICK(39);
                if ((figures(di).motion) == 3) {
                    t3 = g_building_buf_hi;
                    t4 = g_building_buf_lo;
                    t5 = figure_sprite_ofs(di, 7);
                    blit_transparent(MK32(g_figure_sprites_hi, g_figure_sprites_lo + t5), MK32(t3, t4), 0x61, l_6 + 1, 0, l_4, 0x10, l_2 - 1, 0 /* unused ax */);
                }
                di = di - 1;
            }
        } else {
            draw_sprite_to(MK32(g_bld_tiles_hi, g_bld_tiles_lo + 0xEA), g_building_buf_lo, g_building_buf_hi, 0x5D, si + 4);
        }
        if (((u32)MK32(a0, g_elevator_door)) != ((u32)MK32(g_elevator_floor, 2))) {
            if (((u32)MK32(a0, g_elevator_door)) == ((u32)MK32(g_elevator_floor, 1))) {
                draw_sprite_to(MK32(g_bld_tiles_hi, g_bld_tiles_lo + 0x82), g_building_buf_lo, g_building_buf_hi, 0x60, si + 7);
                draw_sprite_to(MK32(g_bld_sprites_hi, g_bld_sprites_lo + 0x1A), g_building_buf_lo, g_building_buf_hi, 0x6A, si + 7);
            } else if (((i16)l_2) > 0) {
                draw_sprite_to(MK32(g_bld_tiles_hi, g_bld_tiles_lo + 0x8A), g_building_buf_lo, g_building_buf_hi, 0x60, si + 7);
            }
        }
    }
    return;
}

/* 193D:0AAF  draw left/right parallax side backgrounds (sky/city, scroll / g_elevator_speed) and street-level sprites near the ground */
void bld_draw_side_scenery(void) {
    u16 t3;
    u16 t7;
    u16 t11;
    u16 t15;
    FUNC_TICK(144);
    if (g_bld_scroll != g_bld_drawn_scroll || G16(0x7A2C) == 1 || g_bld_full_redraw != 0) {
        g_bld_full_redraw = 0;
        t3 = (u16)((i32)(i16)g_bld_scroll / (i16)g_elevator_speed);
        t7 = (u16)((i32)(i16)g_bld_scroll / (i16)g_elevator_speed);
        blit_opaque_rect(MK32(g_building_bg_hi, g_building_bg_lo + 0xA), g_screen_buf, 0, t7 + 4, 0, 0, 0x38, 0x6D - t3, 0 /* unused ax */);
        t11 = (u16)((i32)(i16)g_bld_scroll / (i16)g_elevator_speed);
        t15 = (u16)((i32)(i16)g_bld_scroll / (i16)g_elevator_speed);
        blit_opaque_rect(MK32(g_building_bg_hi, g_building_bg_lo + 2), g_screen_buf, 0x108, t15 + 4, 0, 0, 0x38, 0x6D - t11, 0 /* unused ax */);
        if (((i16)g_bld_scroll) <= 22) {
            blit_transparent(MK32(g_bld_sprites_hi, g_bld_sprites_lo + 0xA), g_screen_buf, 0x108, g_bld_scroll + 0x5B, 0, 0, 0x38, 0x16 - g_bld_scroll, 0 /* unused ax */);
        }
        if (((i16)g_bld_scroll) <= 11) {
            blit_transparent(MK32(g_bld_sprites_hi, g_bld_sprites_lo + 0x12), g_screen_buf, 4, g_bld_scroll + 0x66, 0, 0, 0x38, 0xB - g_bld_scroll, 0 /* unused ax */);
        }
        if (((i16)g_bld_scroll) <= 33) {
            blit_transparent(MK32(g_bld_tiles_hi, g_bld_tiles_lo + 0xD2), g_screen_buf, 0x1D, g_bld_scroll + 0x50, 0, 0, 0x20, 0x21 - g_bld_scroll, 0 /* unused ax */);
        }
    }
    g_bld_drawn_scroll = g_bld_scroll;
    return;
}

/* 193D:0C1C  building tick: elevator, figure AI, render floors/sides/view, animate figures, overlays (scaffold, bomb marker) */
void bld_update_frame(void) {
    u16 si;
    FUNC_TICK(93);
    if (g_clock_halt != 2) {
        if (g_room == ROOM_BUILDING) {
            mouse_poll();
        }
        if (world_now()) {                   /* seats: the world in seat 0 (runtime/seat.h) */
            elevator_update();
        } else {
            bld_camera_follow();             /* another seat: only its camera (src/seatctx.c) */
        }
        if (g_room == ROOM_BUILDING) {
            mouse_poll();
        }
        if (world_now()) {
            fig_think();
        } else {
            seat_doors_draw();               /* another seat: the doors fig_think opened and closed */
        }
        if (g_room == ROOM_BUILDING) {
            mouse_poll();
        }
        if (g_room == ROOM_BUILDING) {
            mouse_poll();
            bld_render_floors(g_bld_scroll);
            mouse_poll();
            bld_draw_side_scenery();
            mouse_poll();
            bld_blit_view();
            mouse_poll();
        }
        if (world_now()) {
            fig_animate();
        } else {
            seat_figures_draw();             /* another seat draws what the world's fig_animate drew */
        }
        if (g_room == ROOM_BUILDING) {
            mouse_poll();
        }
        if (g_room == ROOM_BUILDING) {
            bld_draw_overlays();
            mouse_poll();
            if (G16(0x7A2C) != 0) {
                si = 8;
                while (((i16)si) < 308) {
                    LOOP_TICK(13);
                    draw_sprite(MK32(G16(0x8380), G16(0x837E) + 0xA), si, 0x69);
                    si = si + 0x14;
                }
                draw_sprite(MK32(G16(0x8380), G16(0x837E) + 2), 0x106, 0x5C);
            }
            if (((i16)g_bomb_timer) >= 1000) {
                g_clip_on = 1;
                g_clip_y1 = 4;
                g_clip_y2 = 0x70;
                draw_sprite(MK32(g_bld_side_tiles_hi, g_bld_side_tiles_lo + 0x8A), floors_door_x(g_npc_target_floor, g_npc_target_door) + 0xFFFB, ((0x1C2 - (g_npc_target_floor * 0x1E)) - ((0x1C2 - g_bld_scroll) + 0xFF93)) + 4);
                g_clip_on = 0;
            }
        }
    }
    return;
}

/* 193D:0D62  move figures toward target x by their speed (fig+0x1A, x snapped to multiples), pick walk/elevator/door animation frames, draw visible ones */
void fig_animate(void) {
    u16 ax;
    u16 di;
    u16 si;
    u16 t1;
    u16 t9;
    u16 t12;
    u16 t15;
    u16 t18;
    u16 t61;
    u16 t78;
    u16 t90;
    u16 t92;
    u16 t95;
    u8 fr_[30] = {0};                  /* bytes bp-30 .. bp-1 of the original frame */
    FUNC_TICK(31);
    seat_fig_begin();                    /* seats: what is drawn, for the other seats (src/seatctx.c) */
    FR16(fr_, 30, 0xE) = 0;
    si = 0;
    while (((i16)si) < 6) {
        LOOP_TICK(22);
        t1 = figures(si).motion;
        g_tmp_a = figures(si).motion;
        if (!(((i16)t1) < 2 || g_tmp_a == 3)) {
            FRX16(fr_, 30, 0x1A, (FR16(fr_, 30, 0xE) << 1) + 0) = si;
            FR16(fr_, 30, 0xE) = FR16(fr_, 30, 0xE) + 1;
        }
        si = si + 1;
    }
    si = 0;
    while (((i16)si) < 6) {
        LOOP_TICK(18);
        if (((i16)(figures(si).motion)) <= 1) {
            FRX16(fr_, 30, 0x1A, (FR16(fr_, 30, 0xE) << 1) + 0) = si;
            FR16(fr_, 30, 0xE) = FR16(fr_, 30, 0xE) + 1;
        }
        si = si + 1;
    }
    g_clip_on = 1;
    g_clip_y1 = 4;
    g_clip_y2 = 0x71;
    si = 0;
    while (((i16)si) < ((i16)FR16(fr_, 30, 0xE))) {
        LOOP_TICK(455);
        t9 = ((FRX16(fr_, 30, 0x1A, (si << 1) + 0)) * 0x1E) + 0x7B42;
        FR16(fr_, 30, 0xA) = DSEG;
        FR16(fr_, 30, 0xC) = t9;
        t12 = (FPOFF(FR32(fr_, 30, 0xC))) + 0xC;
        FR16(fr_, 30, 0x6) = FPSEG(FR32(fr_, 30, 0xC));
        FR16(fr_, 30, 0x8) = t12;
        t15 = (FPOFF(FR32(fr_, 30, 0xC))) + 0xE;
        FR16(fr_, 30, 0x2) = FPSEG(FR32(fr_, 30, 0xC));
        FR16(fr_, 30, 0x4) = t15;
        if ((FP16(FR32(fr_, 30, 0xC), 0x10)) == 1) {
L_0E76:
            if (((i16)(u16)((u16)((i32)(i16)(FP16(FR32(fr_, 30, 0xC), 0)) % (i16)(figures(FRX16(fr_, 30, 0x1A, (si << 1) + 0)).speed)))) > 0) {
                FP16(FR32(fr_, 30, 0xC), 0) = (u16)((u16)((i32)(i16)(FP16(FR32(fr_, 30, 0xC), 0)) / (i16)(figures(FRX16(fr_, 30, 0x1A, (si << 1) + 0)).speed))) * (figures(FRX16(fr_, 30, 0x1A, (si << 1) + 0)).speed);
            }
            if (((i16)(u16)((u16)((i32)(i16)(FP16(FR32(fr_, 30, 0xC), 4)) % (i16)(figures(FRX16(fr_, 30, 0x1A, (si << 1) + 0)).speed)))) > 0) {
                FP16(FR32(fr_, 30, 0xC), 4) = (u16)((u16)((i32)(i16)(FP16(FR32(fr_, 30, 0xC), 4)) / (i16)(figures(FRX16(fr_, 30, 0x1A, (si << 1) + 0)).speed))) * (figures(FRX16(fr_, 30, 0x1A, (si << 1) + 0)).speed);
            }
            if ((FP16(FR32(fr_, 30, 0x4), 0)) != 1) {
                FR16(fr_, 30, 0x1C) = 0;
            } else if ((FP16(FR32(fr_, 30, 0xC), 4)) == (FP16(FR32(fr_, 30, 0xC), 0))) {
                FR16(fr_, 30, 0x1C) = 0;
                if (((i16)(FP16(FR32(fr_, 30, 0x4), 0))) < 2) {
                    FP16(FR32(fr_, 30, 0x4), 0) = 0;
                }
            } else {
                if (((i16)(FP16(FR32(fr_, 30, 0xC), 4))) >= ((i16)(FP16(FR32(fr_, 30, 0xC), 0)))) {
                    ax = 1;
                } else {
                    ax = 0xFFFF;
                }
                FR16(fr_, 30, 0x1C) = ax;
            }
            if (((i16)FR16(fr_, 30, 0x1C)) >= 0 || ((i16)(FP16(FR32(fr_, 30, 0x8), 0))) <= -1) {
                if (((i16)FR16(fr_, 30, 0x1C)) <= 0 || ((i16)(FP16(FR32(fr_, 30, 0x8), 0))) >= 1) {
                    if (((i16)FR16(fr_, 30, 0x1C)) <= 0 || (FP16(FR32(fr_, 30, 0x8), 0)) != 2) {
                        t61 = FP16(FR32(fr_, 30, 0x4), 0);
                        g_tmp_a = FP16(FR32(fr_, 30, 0x4), 0);
                        if (t61 == 2 || g_tmp_a == 5) {
                            if ((FP16(FR32(fr_, 30, 0x8), 0)) != 0) {
                                if (((i16)(FP16(FR32(fr_, 30, 0x8), 0))) >= 2) {
                                    FP16(FR32(fr_, 30, 0x8), 0) = (FP16(FR32(fr_, 30, 0x8), 0)) + 1;
                                } else {
                                    FP16(FR32(fr_, 30, 0x8), 0) = 2;
                                }
                            } else {
                                FP16(FR32(fr_, 30, 0x8), 0) = 1;
                            }
                            FR16(fr_, 30, 0x1C) = 0;
                        } else if ((FP16(FR32(fr_, 30, 0x4), 0)) == 4) {
                            if (((i16)(FP16(FR32(fr_, 30, 0x8), 0))) >= 2) {
                                FP16(FR32(fr_, 30, 0x8), 0) = (FP16(FR32(fr_, 30, 0x8), 0)) + 1;
                            } else {
                                FP16(FR32(fr_, 30, 0x8), 0) = 2;
                            }
                        }
                    } else {
                        FP16(FR32(fr_, 30, 0x8), 0) = 1;
                        FR16(fr_, 30, 0x1C) = 0;
                    }
                } else {
                    FP16(FR32(fr_, 30, 0x8), 0) = (FP16(FR32(fr_, 30, 0x8), 0)) + 1;
                    FR16(fr_, 30, 0x1C) = 0;
                }
            } else {
                if ((FP16(FR32(fr_, 30, 0x8), 0)) != 2) {
                    ax = 1;
                } else {
                    ax = 3;
                }
                FP16(FR32(fr_, 30, 0x8), 0) = (FP16(FR32(fr_, 30, 0x8), 0)) - ax;
                FR16(fr_, 30, 0x1C) = 0;
            }
            FP16(FR32(fr_, 30, 0xC), 0) = (FP16(FR32(fr_, 30, 0xC), 0)) + ((figures(FRX16(fr_, 30, 0x1A, (si << 1) + 0)).speed) * FR16(fr_, 30, 0x1C));
            if ((FP16(FR32(fr_, 30, 0x4), 0)) != 3) {
                if (((i16)(FP16(FR32(fr_, 30, 0x4), 0))) <= 1) {
                    ax = (FP16(FR32(fr_, 30, 0x8), 0)) + 1;
                    if (((u16)((FP16(FR32(fr_, 30, 0x8), 0)) + 1)) <= 3) {
                        switch (ax) {
                        case 0:
                            t90 = (u16)((i32)(i16)(u16)((u16)((i32)(i16)(FP16(FR32(fr_, 30, 0xC), 0)) % (i16)((figures(FRX16(fr_, 30, 0x1A, (si << 1) + 0)).speed) * 3))) / (i16)(figures(FRX16(fr_, 30, 0x1A, (si << 1) + 0)).speed));
                            di = t90 + 4;
                            if (((i16)(t90 + 4)) <= 6) break;
                            di = 0xC - di;
                            break;
                        case 1:
                            di = 7;
                            break;
                        case 2:
                            t78 = (u16)((i32)(i16)(u16)((u16)((i32)(i16)(FP16(FR32(fr_, 30, 0xC), 0)) % (i16)((figures(FRX16(fr_, 30, 0x1A, (si << 1) + 0)).speed) * 3))) / (i16)(figures(FRX16(fr_, 30, 0x1A, (si << 1) + 0)).speed));
                            di = t78 + 1;
                            if (((i16)(t78 + 1)) <= 3) break;
                            di = 6 - di;
                            break;
                        case 3:
                            di = 9;
                            break;
                        default:
                            cpu_halt("bad switch index %u in 193D:0D62", ax);
                        }
                    }
                } else {
                    t92 = FP16(FR32(fr_, 30, 0x4), 0);
                    g_tmp_a = FP16(FR32(fr_, 30, 0x4), 0);
                    if (t92 == 2 || g_tmp_a == 5) {
                        ax = (FP16(FR32(fr_, 30, 0x8), 0)) - 1;
                        if (((u16)((FP16(FR32(fr_, 30, 0x8), 0)) - 1)) <= 6) {
                            switch (ax) {
                            case 0:
                                di = 4;
                                break;
                            case 1:
                            case 3:
                                di = 9;
                                break;
                            case 2:
                            case 4:
                                di = 0xA;
                                break;
                            case 5:
                                di = 4;
                                break;
                            case 6:
                                di = 7;
                                break;
                            default:
                                cpu_halt("bad switch index %u in 193D:0D62", ax);
                            }
                        }
                    } else if ((FP16(FR32(fr_, 30, 0x4), 0)) == 4) {
                        ax = ((FP16(FR32(fr_, 30, 0x8), 0)) - 1) - 1;
                        if (((u16)(((FP16(FR32(fr_, 30, 0x8), 0)) - 1) - 1)) <= 3) {
                            switch (ax) {
                            case 0:
                            case 2:
                                di = 8;
                                break;
                            case 1:
                            case 3:
                                di = 7;
                                break;
                            default:
                                cpu_halt("bad switch index %u in 193D:0D62", ax);
                            }
                        }
                    }
                }
                seat_fig_note(FRX16(fr_, 30, 0x1A, (si << 1) + 0), di);
                t95 = FP16(FR32(fr_, 30, 0xC), 2);
                g_tmp_a = FP16(FR32(fr_, 30, 0xC), 2);
                if (!(((i16)t95) > ((i16)g_bld_top_floor) || ((i16)(g_bld_top_floor + 0xFFFC)) > ((i16)g_tmp_a) || g_room != ROOM_BUILDING)) {
                    figure_draw(FRX16(fr_, 30, 0x1A, (si << 1) + 0), di);
                }
                g_tmp_a = FP16(FR32(fr_, 30, 0x8), 0);
                g_tmp_b = FP16(FR32(fr_, 30, 0x4), 0);
                if (!((((i16)g_tmp_a) < 7 || g_tmp_b != 2) && (((i16)g_tmp_a) < 5 || g_tmp_b != 4) && (((i16)g_tmp_a) < 3 || g_tmp_b != 5))) {
                    if ((FP16(FR32(fr_, 30, 0x4), 0)) != 5) {
                        ax = 0;
                    } else {
                        ax = 2;
                    }
                    FP16(FR32(fr_, 30, 0x8), 0) = ax;
                    FP16(FR32(fr_, 30, 0x4), 0) = 0;
                }
            }
        } else {
            t18 = floors_door_kind(FP16(FR32(fr_, 30, 0xC), 2), FP16(FR32(fr_, 30, 0xC), 0x12));
            g_tmp_a = floors_door_kind(FP16(FR32(fr_, 30, 0xC), 2), FP16(FR32(fr_, 30, 0xC), 0x12));
            if (t18 == 6 || g_tmp_a == 0xA) goto L_0E76;
        }
        si = si + 1;
    }
    g_clip_on = 0;
    return;
}

/* 193D:12FA  people logic for the 6 figures: NPC wanderers (figs 3-5) pick random floors/doors (type 0 same floor, type 1 other floor), open/close doors, elevator boarding; player figures blocked at locked own-type rooms of others without master key (msg 0x15C) */
void fig_think(void) {
    int cc;
    u16 di;
    u16 si;
    u16 t1;
    u16 t4;
    u16 t7;
    u16 t8;
    u16 t12;
    u16 t13;
    u16 t15;
    u16 t16;
    u16 t20;
    u16 t24;
    u16 t33;
    u16 t50;
    u8 fr_[16] = {0};                  /* bytes bp-16 .. bp-1 of the original frame */
    FUNC_TICK(33);
    di = 0;
    while (((i16)di) < 6) {
        LOOP_TICK(723);
        FR16(fr_, 16, 0xA) = DSEG;
        FR16(fr_, 16, 0xC) = (di * 0x1E) + 0x7B42;
        t1 = (FPOFF(FR32(fr_, 16, 0xC))) + 0xC;
        FR16(fr_, 16, 0x6) = FPSEG(FR32(fr_, 16, 0xC));
        FR16(fr_, 16, 0x8) = t1;
        t4 = (FPOFF(FR32(fr_, 16, 0xC))) + 0xE;
        FR16(fr_, 16, 0x2) = FPSEG(FR32(fr_, 16, 0xC));
        FR16(fr_, 16, 0x4) = t4;
        if ((FP16(FR32(fr_, 16, 0xC), 0x18)) != 2 || (FP16(FR32(fr_, 16, 0xC), 0x10)) != 0 || (FP16(FR32(fr_, 16, 0xC), 4)) != (FP16(FR32(fr_, 16, 0xC), 0)) || (FP16(FR32(fr_, 16, 0xC), 2)) != (FP16(FR32(fr_, 16, 0xC), 6)) || (FP16(FR32(fr_, 16, 0xC), 8)) != 0) {
            if (((i16)di) >= 3) {
L_1394:
                if ((FP16(FR32(fr_, 16, 0xC), 0x10)) == 0) {
                    if ((FP16(FR32(fr_, 16, 0xC), 0x18)) == 1) {
                        t7 = random_n(0xC, di);
                        t8 = floor_door_count(t7 + 1);
                        FR16(fr_, 16, 0xE) = random_n(t8, di) - 1;
                        floor_pick_door(t7 + 1, (fr_ + 16 - 0xE), 0xFFFF);
                        FP16(FR32(fr_, 16, 0xC), 2) = t7 + 1;
                        FP16(FR32(fr_, 16, 0xC), 0) = (floors_door_x(t7 + 1, FR16(fr_, 16, 0xE))) + 4;
                        FP16(FR32(fr_, 16, 0xC), 0x14) = FR16(fr_, 16, 0xE);
                        si = t7 + 1;
                        for (;;) {
                            LOOP_TICK(10);
                            t12 = random_n(0xC, si);
                            si = t12 + 1;
                            if (((u16)(t12 + 1)) == (FP16(FR32(fr_, 16, 0xC), 2))) {
                                continue;
                            } else {
                                break;
                            }
                        }
                        t13 = floor_door_count(si);
                        FR16(fr_, 16, 0xE) = random_n(t13, si) - 1;
                        floor_pick_door(si, (fr_ + 16 - 0xE), 0xFFFF);
                        FP16(FR32(fr_, 16, 0xC), 0x12) = FR16(fr_, 16, 0xE);
                        FP16(FR32(fr_, 16, 0xC), 6) = si;
                    } else if ((FP16(FR32(fr_, 16, 0xC), 0x18)) == 0) {
                        t15 = random_n(0xB, di);
                        t16 = floor_door_count(t15 + 1);
                        FR16(fr_, 16, 0xE) = random_n(t16, di) - 1;
                        floor_pick_door(t15 + 1, (fr_ + 16 - 0xE), 0xFFFF);
                        FP16(FR32(fr_, 16, 0xC), 2) = t15 + 1;
                        FP16(FR32(fr_, 16, 0xC), 0) = (floors_door_x(t15 + 1, FR16(fr_, 16, 0xE))) + 4;
                        FP16(FR32(fr_, 16, 0xC), 0x14) = FR16(fr_, 16, 0xE);
                        t20 = floor_door_count(t15 + 1);
                        FR16(fr_, 16, 0xE) = random_n(t20, t15 + 1) - 1;
                        floor_pick_door(t15 + 1, (fr_ + 16 - 0xE), FP16(FR32(fr_, 16, 0xC), 0x14));
                        FP16(FR32(fr_, 16, 0xC), 0x12) = FR16(fr_, 16, 0xE);
                        FP16(FR32(fr_, 16, 0xC), 6) = t15 + 1;
                    }
                    if ((FP16(FR32(fr_, 16, 0xC), 6)) != (FP16(FR32(fr_, 16, 0xC), 2))) {
                        FP16(FR32(fr_, 16, 0xC), 4) = 0x99;
                    } else {
                        FP16(FR32(fr_, 16, 0xC), 4) = (floors_door_x(FP16(FR32(fr_, 16, 0xC), 6), FP16(FR32(fr_, 16, 0xC), 0x12))) + 4;
                    }
                    t24 = floors_door_kind(FP16(FR32(fr_, 16, 0xC), 2), FP16(FR32(fr_, 16, 0xC), 0x14));
                    g_tmp_a = floors_door_kind(FP16(FR32(fr_, 16, 0xC), 2), FP16(FR32(fr_, 16, 0xC), 0x14));
                    if (t24 == 6 || g_tmp_a == 0xA) {
                        FP16(FR32(fr_, 16, 0xC), 0x10) = 1;
                    } else {
                        floors_door_open(FP16(FR32(fr_, 16, 0xC), 2), FP16(FR32(fr_, 16, 0xC), 0x14)) = 1;
                        door_draw(FP16(FR32(fr_, 16, 0xC), 2), FP16(FR32(fr_, 16, 0xC), 0x14), 1);
                        FP16(FR32(fr_, 16, 0xC), 0x10) = 2;
                    }
                } else if ((FP16(FR32(fr_, 16, 0xC), 0x10)) != 2) {
                    if ((FP16(FR32(fr_, 16, 0xC), 0x10)) == 3) {
                        t33 = floors_door_kind(FP16(FR32(fr_, 16, 0xC), 2), FP16(FR32(fr_, 16, 0xC), 0x14));
                        g_tmp_a = floors_door_kind(FP16(FR32(fr_, 16, 0xC), 2), FP16(FR32(fr_, 16, 0xC), 0x14));
                        if (!(t33 == 6 || g_tmp_a == 0xA)) {
                            floors_door_open(FP16(FR32(fr_, 16, 0xC), 2), FP16(FR32(fr_, 16, 0xC), 0x14)) = 0;
                            door_draw(FP16(FR32(fr_, 16, 0xC), 2), FP16(FR32(fr_, 16, 0xC), 0x14), 2);
                        }
                        FP16(FR32(fr_, 16, 0xC), 0x10) = 1;
                    } else if ((FP16(FR32(fr_, 16, 0xC), 0x10)) != 1 || (FP16(FR32(fr_, 16, 0x4), 0)) != 0 || (FP16(FR32(fr_, 16, 0xC), 0x12)) != 0xFFFF || (FP16(FR32(fr_, 16, 0xC), 8)) == 0 || (FP16(FR32(fr_, 16, 0xC), 2)) != (FP16(FR32(fr_, 16, 0xC), 6))) {
                        if ((FP16(FR32(fr_, 16, 0xC), 0x10)) != 1 || (FP16(FR32(fr_, 16, 0xC), 4)) == (FP16(FR32(fr_, 16, 0xC), 0)) || (FP16(FR32(fr_, 16, 0x4), 0)) == 1 || (FP16(FR32(fr_, 16, 0xC), 0x16)) != 0) {
                            if (!((FP16(FR32(fr_, 16, 0x4), 0)) != 0 || !((FP16(FR32(fr_, 16, 0xC), 2)) != (FP16(FR32(fr_, 16, 0xC), 6)) || (FP16(FR32(fr_, 16, 0xC), 0x12)) == 0xFFFE) || (FP16(FR32(fr_, 16, 0xC), 0x16)) != 0)) {
                                FP16(FR32(fr_, 16, 0x4), 0) = 5;
                                elevator_queue_add(di);
                                FP16(FR32(fr_, 16, 0xC), 0x16) = 1;
                            }
                        } else {
                            FP16(FR32(fr_, 16, 0x4), 0) = 1;
                            FP16(FR32(fr_, 16, 0xC), 0x1C) = 0;
                        }
                    } else {
                        FP16(FR32(fr_, 16, 0x4), 0) = 1;
                        FP16(FR32(fr_, 16, 0xC), 0x1C) = 0;
                        FP16(FR32(fr_, 16, 0xC), 4) = FP16(FR32(fr_, 16, 0xC), 8);
                        FP16(FR32(fr_, 16, 0xC), 8) = 0;
                    }
                } else {
                    FP16(FR32(fr_, 16, 0xC), 0x10) = 3;
                }
                if (((u32)(MK32(FP16(FR32(fr_, 16, 0xC), 2), g_elevator_door))) == ((u32)MK32(g_elevator_floor, 2))) {
                    if (((u32)(MK32(FP16(FR32(fr_, 16, 0xC), 0x16), di))) == ((u32)(MK32(1, g_betty_letter(0))))) {
                        FP16(FR32(fr_, 16, 0x4), 0) = 2;
                        FP16(FR32(fr_, 16, 0xC), 0x16) = 2;
                    } else {
L_17D4:
                        if ((FP16(FR32(fr_, 16, 0x4), 0)) != 0 || (FP16(FR32(fr_, 16, 0xC), 0x16)) != 2) {
                            if ((FP16(FR32(fr_, 16, 0x4), 0)) != 3 || (FP16(FR32(fr_, 16, 0xC), 6)) != g_elevator_floor || (FP16(FR32(fr_, 16, 0xC), 0x12)) == 0xFFFE || g_elevator_door != 2) {
                                if ((FP16(FR32(fr_, 16, 0x4), 0)) != 0 || (FP16(FR32(fr_, 16, 0xC), 0x16)) != 4) {
                                    if ((FP16(FR32(fr_, 16, 0x4), 0)) == 0 && (FP16(FR32(fr_, 16, 0xC), 4)) == (FP16(FR32(fr_, 16, 0xC), 0)) && (FP16(FR32(fr_, 16, 0xC), 0x16)) == 0 && (FP16(FR32(fr_, 16, 0xC), 0x12)) != 0xFFFF) {
                                        g_tmp_a = floors_door_room(FP16(FR32(fr_, 16, 0xC), 2), FP16(FR32(fr_, 16, 0xC), 0x12));
                                        cc = mp_is_human(di + 1);   /* (the original: di == g_player - 1, the one human -- the others' figures went in anywhere) */
                                        if (cc && (g_tmp_a == 1 || g_tmp_a == 2 || g_tmp_a == 3 || g_tmp_a == 4 || g_tmp_a == 5)) {
                                            cc = (floors_door_owner(FP16(FR32(fr_, 16, 0xC), 2), FP16(FR32(fr_, 16, 0xC), 0x12))) == ((u16)(di + 1));
                                            if (cc || (master_key(di + 1, 0)) != 0) {
L_1986:
                                                t50 = floors_door_kind(FP16(FR32(fr_, 16, 0xC), 2), FP16(FR32(fr_, 16, 0xC), 0x12));
                                                g_tmp_a = floors_door_kind(FP16(FR32(fr_, 16, 0xC), 2), FP16(FR32(fr_, 16, 0xC), 0x12));
                                                if (t50 != 6 && g_tmp_a != 0xA) {
                                                    if ((FP16(FR32(fr_, 16, 0xC), 0x1C)) == 0) {
                                                        if ((floors_door_open(FP16(FR32(fr_, 16, 0xC), 2), FP16(FR32(fr_, 16, 0xC), 0x12))) == 0) {
                                                            floors_door_open(FP16(FR32(fr_, 16, 0xC), 2), FP16(FR32(fr_, 16, 0xC), 0x12)) = 1;
                                                            door_draw(FP16(FR32(fr_, 16, 0xC), 2), FP16(FR32(fr_, 16, 0xC), 0x12), 1);
                                                        }
                                                        FP16(FR32(fr_, 16, 0xC), 0x1C) = 1;
                                                        FP16(FR32(fr_, 16, 0x4), 0) = 5;
                                                    } else if (!((FP16(FR32(fr_, 16, 0xC), 0x1C)) != 1 || (FP16(FR32(fr_, 16, 0x4), 0)) != 0)) {
                                                        floors_door_open(FP16(FR32(fr_, 16, 0xC), 2), FP16(FR32(fr_, 16, 0xC), 0x12)) = 0;
                                                        door_draw(FP16(FR32(fr_, 16, 0xC), 2), FP16(FR32(fr_, 16, 0xC), 0x12), 2);
                                                        FP16(FR32(fr_, 16, 0xC), 0x10) = 0;
                                                        FP16(FR32(fr_, 16, 0xC), 0x1C) = 0;
                                                        FP16(FR32(fr_, 16, 0xC), 0x14) = FP16(FR32(fr_, 16, 0xC), 0x12);
                                                    }
                                                } else if ((FP16(FR32(fr_, 16, 0x4), 0)) != 0 || (FP16(FR32(fr_, 16, 0xC), 0x1C)) != 1) {
                                                    if ((FP16(FR32(fr_, 16, 0x4), 0)) == 0) {
                                                        FP16(FR32(fr_, 16, 0x4), 0) = 5;
                                                        FP16(FR32(fr_, 16, 0xC), 0x1C) = 1;
                                                    }
                                                } else {
                                                    FP16(FR32(fr_, 16, 0xC), 0x1C) = 0;
                                                    FP16(FR32(fr_, 16, 0xC), 0x10) = 0;
                                                    FP16(FR32(fr_, 16, 0xC), 0x14) = FP16(FR32(fr_, 16, 0xC), 0x12);
                                                }
                                            } else {
                                                if (di + 1 != g_player && mp_enter_player(di + 1)) {   /* multiplayer: that human's message */
                                                    g_status_msg = 0x15C;
                                                    mp_leave();
                                                } else
                                                    g_status_msg = 0x15C;
                                                FP16(FR32(fr_, 16, 0xC), 0x12) = 0xFFFF;
                                            }
                                        } else {
                                            goto L_1986;
                                        }
                                    } else if (!((FP16(FR32(fr_, 16, 0x4), 0)) != 0 || (FP16(FR32(fr_, 16, 0xC), 4)) != (FP16(FR32(fr_, 16, 0xC), 0)) || (FP16(FR32(fr_, 16, 0xC), 0x12)) != 0xFFFF || (FP16(FR32(fr_, 16, 0xC), 0x16)) != 0)) {
                                        FP16(FR32(fr_, 16, 0x8), 0) = 0;
                                    }
                                } else {
                                    if ((FP16(FR32(fr_, 16, 0xC), 0x12)) != 0xFFFF) {
                                        FP16(FR32(fr_, 16, 0x4), 0) = 1;
                                        FP16(FR32(fr_, 16, 0xC), 4) = (floors_door_x(FP16(FR32(fr_, 16, 0xC), 2), FP16(FR32(fr_, 16, 0xC), 0x12))) + 4;
                                    }
                                    FP16(FR32(fr_, 16, 0xC), 0x16) = 0;
                                }
                            } else {
                                FP16(FR32(fr_, 16, 0x4), 0) = 4;
                                FP16(FR32(fr_, 16, 0xC), 0x16) = 4;
                            }
                        } else {
                            FP16(FR32(fr_, 16, 0x4), 0) = 3;
                            FP16(FR32(fr_, 16, 0xC), 0x16) = 3;
                        }
                    }
                } else {
                    goto L_17D4;
                }
            } else {
                if ((players(di + 1).out) == 0) goto L_1394;
            }
        }
        di = di + 1;
    }
    return;
}

/* 193D:1B63  move elevator cabin one step (speed g_elevator_speed px, 30 px per floor) toward the riding/calling figure, door open/close counter; camera follows the player's figure (scroll = floor*30-60, 0..321) */
void elevator_update(void) {
    int cc;
    u16 di;
    u16 si;
    u16 l_2;
    u16 t2;
    FUNC_TICK(185);
    l_2 = 0;
    di = 0;
    if (((i16)g_betty_letter(0)) >= 0) {
        g_betty_letter(0) = 0xFFFF;
    }
    si = 0;
    while (((i16)si) <= 6) {
        LOOP_TICK(23);
        if ((figures(si).motion) == 3) {
            l_2 = si + 1;
            di = (u16)(figures(si).target_floor);
        }
        si = si + 1;
    }
    cc = di != 0;
    if (!(cc || ((i16)g_elevator_calls) <= 0)) {
        di = figures(g_elevator_queue(0)).floor;
    }
    si = 0;
    while (((i16)si) <= 6) {
        LOOP_TICK(31);
        t2 = figures(si).motion;
        g_tmp_a = figures(si).motion;
        if (t2 == 2 || g_tmp_a == 4) {
L_1C24:
            l_2 = si + 1;
            di = 0;
        } else if (g_tmp_a == 0) {
            if ((figures(si).lift_phase) == 2) goto L_1C24;
        }
        si = si + 1;
    }
    cc = di != 0;
    if (!(cc || g_elevator_offset == 0)) {
        if (((i16)g_elevator_offset) <= 0) {
            di = g_elevator_floor + 1;
        } else {
            di = g_elevator_floor - 1;
        }
    }
    if (di != 0) {
        if (g_elevator_floor != di) {
            if (((i16)g_elevator_door) <= 0) {
                if (((i16)di) <= ((i16)g_elevator_floor)) {
                    if (((i16)(0x1E - g_elevator_speed)) <= ((i16)g_elevator_offset)) {
                        g_elevator_floor = g_elevator_floor - 1;
                        g_elevator_offset = 0;
                    } else {
                        g_elevator_offset = g_elevator_offset + g_elevator_speed;
                    }
                } else if (((i16)(g_elevator_speed + 0xFFE2)) >= ((i16)g_elevator_offset)) {
                    g_elevator_floor = g_elevator_floor + 1;
                    g_elevator_offset = 0;
                } else {
                    g_elevator_offset = g_elevator_offset - g_elevator_speed;
                }
            } else {
                g_elevator_door = g_elevator_door - 1;
            }
            if (l_2 != 0) {
                shoplists(0x1A, (l_2 * 2) + 4).script = g_elevator_floor;
            }
        } else if (((i16)g_elevator_door) >= 2) {
            if (!(l_2 != 0 || ((i16)g_elevator_calls) <= 0 || (figures(g_elevator_queue(0)).floor) != g_elevator_floor)) {
                g_betty_letter(0) = g_elevator_queue(0);
                elevator_queue_remove(g_elevator_queue(0));
            }
        } else {
            g_elevator_door = g_elevator_door + 1;
            if (((u32)MK32(g_elevator_door, l_2)) == 0x20000) {
                g_betty_letter(0) = g_elevator_queue(0);
                elevator_queue_remove(g_elevator_queue(0));
            }
        }
    }
    bld_camera_follow();
    return;
}

/* the camera follows the player's figure (the end of elevator_update): the
 * scroll from its floor, in the elevator with the cabin.  A seat's view:
 * other seats than 0 run it on their own (bld_update_frame, runtime/seat.h).
 * No time of its own: it is part of 193D:1B63. */
void bld_camera_follow(void) {
    u16 t15;
    g_bld_scroll = ((figures(g_player_figure).floor) * 0x1E) + 0xFFC4;
    if ((figures(g_player_figure).motion) == 3) {
        t15 = ((figures(g_player_figure).floor) * 0x1E) - g_elevator_offset;
        g_tmp_a = ((figures(g_player_figure).floor) * 0x1E) - g_elevator_offset;
        if (t15 != ((u16)(g_bld_scroll + 0x3C))) {
            if ((u16)((u16)((i32)(i16)g_tmp_a % (i16)g_elevator_speed)) == 0) {
                g_bld_scroll = (((figures(g_player_figure).floor) * 0x1E) + 0xFFC4) - g_elevator_offset;
            }
        }
    }
    if (((i16)g_bld_scroll) >= 0) {
        if (((i16)g_bld_scroll) > 321) {
            g_bld_scroll = 0x141;
        }
    } else {
        g_bld_scroll = 0;
    }
}

/* 193D:1DF9  encounter bubbles when two figures meet on a floor, door name tooltip under mouse, status-bar message g_status_msg (incl. robbery texts 0x12A/0x12E), bottom-panel tooltips */
void bld_draw_overlays(void) {
    u16 ax;
    int cc;
    u16 di;
    u16 s0;
    u16 s1;
    u16 si;
    u32 l_4;
    u32 l_8;
    u32 l_C;
    u32 l_10;
    u32 l_14;
    u16 l_16;
    u16 l_18;
    u16 l_1A;
    u16 l_1C;
    u16 l_1E;
    u16 l_20;
    u16 l_22;
    u16 l_24;
    u16 t2;
    u16 t3;
    u16 t4;
    u16 t5;
    u16 t6;
    u16 t7;
    u16 t8;
    u16 t9;
    u16 t10;
    u16 t11;
    u16 t16;
    u16 t18;
    u16 t19;
    u32 t20;
    u16 t34;
    FUNC_TICK(206);
    if (g_room == ROOM_BUILDING) {
        di = 0;
        while (((i16)di) < 6) {
            LOOP_TICK(176);
            l_C = figures_at(di);
            if ((AT(figure, l_C).task) == 1) {
                l_24 = 0;
                si = (u16)((0x71 - (((AT(figure, l_C).floor) * 0x1E) - g_bld_scroll)) + 4);
                while (((i16)l_24) < 6) {
                    LOOP_TICK(81);
                    l_8 = figures_at(l_24);
                    if ((AT(figure, l_C).floor) == (AT(figure, l_8).floor)) {
                        cc = l_24 != di;
                        if (cc && (AT(figure, l_C).motion) != 3 && (AT(figure, l_8).motion) != 3 && (AT(figure, l_8).task) == 1) {
                            t2 = AT(figure, l_C).x;
                            g_tmp_a = AT(figure, l_C).x;
                            t3 = AT(figure, l_8).x;
                            g_tmp_b = AT(figure, l_8).x;
                            if (((i16)t2) >= ((i16)t3) || ((i16)(g_tmp_a + 0x32)) <= ((i16)g_tmp_b)) {
L_1EFD:
                                t6 = AT(figure, l_C).x;
                                g_tmp_a = AT(figure, l_C).x;
                                t7 = AT(figure, l_8).x;
                                g_tmp_b = AT(figure, l_8).x;
                                if (((i16)t6) > ((i16)t7) && ((i16)(g_tmp_a + 0xFFCE)) < ((i16)g_tmp_b)) {
                                    t8 = AT(figure, l_C).anim;
                                    g_tmp_a = AT(figure, l_C).anim;
                                    if (t8 == 0 || g_tmp_a == 0xFFFF) {
                                        t9 = AT(figure, l_8).anim;
                                        g_tmp_a = AT(figure, l_8).anim;
                                        if (t9 == 0 || g_tmp_a == 1) {
                                            goto L_1F56;
                                        } else {
                                            l_24 = l_24 + 1;
                                            continue;
                                        }
                                    } else {
                                        l_24 = l_24 + 1;
                                        continue;
                                    }
                                } else {
                                    l_24 = l_24 + 1;
                                    continue;
                                }
                            } else {
                                t4 = AT(figure, l_C).anim;
                                g_tmp_a = AT(figure, l_C).anim;
                                if (t4 == 0 || g_tmp_a == 1) {
                                    t5 = AT(figure, l_8).anim;
                                    g_tmp_a = AT(figure, l_8).anim;
                                    if (t5 == 0 || g_tmp_a == 0xFFFF) {
                                        goto L_1F56;
                                    } else {
                                        goto L_1EFD;
                                    }
                                } else {
                                    goto L_1EFD;
                                }
                            }
                        } else {
                            l_24 = l_24 + 1;
                            continue;
                        }
                    } else {
                        l_24 = l_24 + 1;
                        continue;
                    }
                }
            }
L_2022:
            di = di + 1;
        }
        if (((figures(g_player_figure).motion) != 3 || (figures(g_player_figure).target_door) != 0xFFFE) && ((i16)g_mouse_y) >= 4 && ((i16)g_mouse_y) <= 113 && g_cursor_shown == 1) {
            l_10 = 0;
            t16 = (u16)((i32)(i16)(((0x71 - g_mouse_y) + g_bld_scroll) - 1) / 30);
            l_18 = t16 + 1;
            if (((i16)(t16 + 1)) <= 13) {
                l_4 = floors_at(l_18);
                si = (0x71 - ((l_18 * 0x1E) - g_bld_scroll)) + 4;
                di = 0;
                while (((i16)di) < 4) {
                    LOOP_TICK(190);
                    t18 = FP8(l_4, di + 0x14);
                    g_tmp_a = FP8(l_4, di + 0x14);
                    if (!((((i16)t18) < 1 || ((i16)g_tmp_a) > 3) && !(g_tmp_a == 6 || g_tmp_a == 0xA))) {
                        t19 = FP8(l_4, di);
                        g_tmp_a = FP8(l_4, di);
                        if (((i16)t19) <= ((i16)g_mouse_x) && ((i16)(g_tmp_a + 0x10)) >= ((i16)g_mouse_x)) {
                            t20 = door_label_text_2(di, l_18);
                            l_14 = t20;
                            if ((HI16(t20) | LO16(t20)) != 0) {
                                l_1C = FP8(l_4, di + 0x50) + 1;
                                l_1A = FP8(l_4, di + 0x5A) + 1;
                                l_20 = (((i16)(((i16)(((i16)(FP8(l_4, di) + 0x10)) >> 1)) >> 1)) >> 1) + 1;
                                if ((figures(g_player_figure).motion) != 3) {
                                    ax = si + 4;
                                } else {
                                    ax = cursor_y() + 0xFFF8;
                                }
                                l_1E = ax;
                                if (((i16)ax) < 4) {
                                    l_1E = 4;
                                }
                                if (((i16)((l_1A << 3) + l_1E)) > 112) {
                                    l_1E = 0x70 - (l_1A << 3);
                                }
                                if (((i16)(0x20 - l_1C)) < ((i16)l_20)) {
                                    l_20 = ((((i16)(((i16)(((i16)(FP8(l_4, di) + 0x10)) >> 1)) >> 1)) >> 1) - l_1C) + 0xFFFE;
                                }
                                if (FP8(l_4, di + 0x64) != 0) {
                                    t34 = col_to_px(FP8(l_4, di + 0x64) + 5);
                                    l_10 = MK32(FPSEG(g_bld_sprites), FPOFF(g_bld_sprites) + t34);
                                }
                                ui_frame_px(l_20 << 3, l_1E, ((l_20 + l_1C) - 1) << 3, ((l_1A - 1) << 3) + l_1E, g_screen_buf, 2);
                                if ((LO16(l_10) | HI16(l_10)) == 0) {
                                    l_16 = 1;
                                } else {
                                    draw_sprite(l_10, ((l_20 + 1) << 3) + 0xFFFB, l_1E + 4);
                                    l_16 = 3;
                                }
                                text_draw_ex_free((((l_20 + 1) << 3) + (l_16 << 3)) + 0xFFF4, l_1E + 0xFFFC, LO16(l_14), HI16(l_14), l_1C - l_16, 1, g_screen_buf_lo, g_screen_buf_hi);
                            }
                        }
                    }
                    di = di + 1;
                }
            }
        }
        if (((i16)g_status_msg) > 1) {
            if (g_status_msg == 0x12A || g_status_msg == 0x12E) {
                l_14 = str_format(g_text(g_status_msg), W2(players(g_caught_figure).name), W2(g_status_amount));
            } else {
                l_14 = str_format(g_text(g_status_msg));
                if (g_status_msg == 0x12B && seat_humans && knock_opt) mp_knock_status_text(l_14);   /* knocked: the minutes left (seatctx.c) */
            }
            ui_frame(8, 1, 0x16, 6, g_screen_buf_lo, g_screen_buf_hi, 1);
            text_draw_full(0x44, 4, l_14, 0xD, 1, g_screen_buf_lo, g_screen_buf_hi, 5);
            mem_free(LO16(l_14), HI16(l_14));
        }
        if (g_cursor_shown == 1) {
            if (((i16)(cursor_y())) < 120 || ((i16)(cursor_x())) > 120 || ((i16)(clock_hour())) < 1) {
                if ((mouse_in_rect(0x99, 0x80, 0xC1, 0x88)) == 0) {
                    if ((mouse_in_rect(0x99, 0xB5, 0xC1, 0xBD)) != 0) {
                        day_box_show();
                    }
                } else {
                    money_box_show();
                }
            } else {
                status_window_draw();
            }
        }
    }
    return;
L_1F56:
    if (((i16)si) >= -14) {
        if (((i16)si) <= 113) {
            if (((i16)si) < 4) {
                si = 4;
            }
            if (((i16)si) > 106) {
                si = 0x6A;
            }
            cc = ((i16)di) > 2;
            if (cc || ((i16)l_24) > 2) {
                l_22 = 0;
            } else {
                l_22 = 1;
            }
            if (((i16)(AT(figure, l_C).x)) <= ((i16)(AT(figure, l_8).x))) {
                s0 = si;
                if (l_22 != 1) {
                    ax = 9;
                } else {
                    ax = 0xB;
                }
                s1 = (AT(figure, l_C).x) - ax;
                if (l_22 != 1) {
                    ax = 8;
                } else {
                    ax = 0x10;
                }
                t11 = col_to_px(ax);
                draw_sprite(MK32(g_bld_side_tiles_hi, g_bld_side_tiles_lo + t11), s1, s0);
                goto L_2022;
            } else {
                s0 = si;
                s1 = (AT(figure, l_C).x) + 0xD;
                if (l_22 != 1) {
                    ax = 9;
                } else {
                    ax = 0x11;
                }
                t10 = col_to_px(ax);
                draw_sprite(MK32(g_bld_side_tiles_hi, g_bld_side_tiles_lo + t10), s1, s0);
                goto L_2022;
            }
        }
    }
    goto L_2022;
}

/* 193D:2424  allocated label for door a0 on floor a1: 'Office of <player>', '<name> <station>' for own-type rooms, 'Studio of <station>', plain room name; NULL if owner is out of the game (or room type 0, see ORIGINAL BUG below) */
u32 door_label_text_2(u16 a0, u16 a1) {
    u16 ax;
    u16 di;
    u16 dx;
    u16 si;
    u32 rv_;
    u32 t4;
    u32 t7;
    u32 t8;
    u32 t9;
    u16 l_8 = 0;
    u16 l_6 = 0;
    u8 fr_[4] = {0};                  /* bytes bp-4 .. bp-1 of the original frame */
    FUNC_TICK(127);
    FR16(fr_, 4, 0x2) = DSEG;
    FR16(fr_, 4, 0x4) = (a1 * 0x7A) + 0x7C82;
    si = FP8(FR32(fr_, 4, 0x4), a0 + 0x46);
    di = FP8(FR32(fr_, 4, 0x4), a0 + 0x28);
    if ((FP8(FR32(fr_, 4, 0x4), a0 + 0x46)) == 0) {
L_2483:
        ax = FP8(FR32(fr_, 4, 0x4), a0 + 0x1E);
        if ((FP8(FR32(fr_, 4, 0x4), a0 + 0x1E)) <= 5) {
            switch (ax) {
            case 0:
                /* ORIGINAL BUG (193D:2424): returns the never-written locals
                 * bp-6/bp-8, i.e. stack leftovers, for room type 0.  Not
                 * reproducible here (different stack layout): NULL, which the
                 * caller treats as "no label".  names/FINDINGS.md */
                l_6 = 0;
                l_8 = 0;
                break;
            case 1:
                t4 = str_format(DSP(0x11C9) /* "#s #s" */, W2(g_text(di)), W2(players(si).name));
                l_6 = HI16(t4);
                l_8 = LO16(t4);
                break;
            case 2:
            case 3:
            case 4:
                t7 = str_format(DSP(0x11CF) /* "#s #s " */, W2(g_text(di)), W2(players(si).station));
                l_6 = HI16(t7);
                l_8 = LO16(t7);
                break;
            case 5:
                t8 = str_format(DSP(0x11D6) /* "#s #s " */, W2(g_text(139)), W2(players(si).station));
                l_6 = HI16(t8);
                l_8 = LO16(t8);
                break;
            default:
                cpu_halt("bad switch index %u in 193D:2424", ax);
            }
        } else {
            t9 = text_dup(di);
            l_6 = HI16(t9);
            l_8 = LO16(t9);
        }
        ax = l_8;
        dx = l_6;
    } else if ((players(si).out) == 0) {
        goto L_2483;
    } else {
        ax = 0;
        dx = 0;
    }
    rv_ = MK32(dx, ax);
    return rv_;
}

/* 193D:257A  player g_fired_player is thrown out (falling animation); computer player: marked out of game, loses contracts, films, series, studios, Betty's love; human player: game over, back to title (load res 0x2B) */
u16 player_fired_sequence(void) {
    u16 ax;
    u16 di;
    u16 si;
    u32 l_4;
    u16 l_6;
    u16 l_8;
    u16 l_A;
    u16 l_C;
    u16 l_E;
    u16 l_10;
    u32 l_14;
    u32 t1;
    u32 t3;
    u32 t6;
    u32 t7;
    u32 t8;
    u32 t10;
    u32 t11;
    u16 t15;
    u16 t27;
    u16 t64;
    u16 t65;
    FUNC_TICK(373);
    l_C = 0x3F;
    game_clock_pause(0 /* unused ax */);
    if (g_fired_player(0) != g_player) {
        t1 = str_format(g_text(301), W2(players(g_fired_player(0)).name));
        l_14 = t1;
        ui_warning_str(LO16(t1), HI16(t1));
        mem_free(LO16(l_14), HI16(l_14));
        for (;;) {
            LOOP_TICK(8);
            game_frame();
            if (g_click == 0) {
                ax = 0;
            } else {
                ax = 1;
            }
            if (ax != 0) {
                continue;
            } else {
                break;
            }
        }
        for (;;) {
            LOOP_TICK(8);
            game_frame();
            if (g_click != 0) {
                ax = 0;
            } else {
                ax = 1;
            }
            if (ax != 0) {
                continue;
            } else {
                break;
            }
        }
        room_escape();
    } else {
        ui_status_text(G16(0x8764), G16(0x8766), 0xB4);
        tv_show_picture(0xDC);
        if (g_tvfam_state != 0) {
            g_tvfam_state = 1;
        }
        ui_status_text(0x11DD, DSEG, 0x7F);
    }
    ui_status_text(0x11E0, DSEG, 0xB4);
    G32(0xFC) = res_cache_unpack(2, 1);
    t3 = res_cache_unpack(0, 0);
    g_building_bg = t3;
    cursor_hide();
    g_bld_scroll = (((g_fired_player(0) * 3) + 2) * 0x1E) + 0xFFC4;
    t6 = res_load0(0x1C);
    g_room_gfx1 = t6;
    t7 = res_load(0x1D);
    g_room_gfx2 = t7;
    t8 = res_load(g_fired_player(0) + 0x18);
    g_room_gfx3 = t8;
    if (g_fired_player(0) == g_player) {
        t10 = res_load(0x1E);
        g_actor_txt = t10;
        t11 = bitmap_alloc(0x128, 0x3D);
        g_room_gfx5 = t11;
    }
    clip_reset();
    g_clip_y1 = 4;
    g_clip_y2 = 0x71;
    building_draw_sides(1);
    fired_draw_background();
    screen_transition(2, 1, 4, 0 /* unused ax */);
    g_present_bld_only = 0;
    l_E = 1;
    l_A = 0xA5;
    l_6 = timer_read();
    while (((i16)g_bld_scroll) >= 0) {
        LOOP_TICK(92);
        if ((timer_read()) < ((u16)(l_6 + 3))) { seat_pass(); continue; }   /* seats: a wait point */
        l_6 = timer_read();
        fired_draw_background();
        draw_sprite(MK32(g_room_gfx2_hi, g_room_gfx2_lo + 2), 0xA6, l_C);
        t15 = col_to_px(l_E);
        draw_sprite(MK32(g_room_gfx3_hi, g_room_gfx3_lo + t15), l_A, 0x3E);
        present_frame();
        g_bld_scroll = g_bld_scroll - 3;
        l_C = l_C - 3;
        l_E = l_E + 1;
        if (((i16)l_E) > 8) {
            l_E = 1;
        }
        l_10 = 0;
        while (!(((i16)(G8((((g_fired_player(0) - 1) * 0xA) + 0x11A8) + (l_10 << 1)))) < ((i16)l_A) || ((i16)l_10) >= 5)) {
            LOOP_TICK(18);
            l_10 = l_10 + 1;
        }
        if (((i16)l_10) >= 5) continue;
        if ((G8((((g_fired_player(0) - 1) * 0xA) + 0x11A8) + (l_10 << 1))) == 0) continue;
        l_A = l_A - (G8(((((g_fired_player(0) - 1) * 0xA) + 0x11A8) + (l_10 << 1)) + 1));
    }
    g_bld_scroll = 0;
    l_8 = 0x3B;
    while (((i16)l_8) <= 93) {
        LOOP_TICK(33);
        if ((timer_read()) < ((u16)(l_6 + 3))) { seat_pass(); continue; }   /* seats: a wait point */
        l_6 = timer_read();
        fired_draw_background();
        t27 = col_to_px(l_E);
        draw_sprite(MK32(g_room_gfx3_hi, g_room_gfx3_lo + t27), l_A, l_8);
        present_frame();
        l_E = l_E + 1;
        if (((i16)l_E) > 8) {
            l_E = 1;
        }
        if (((i16)l_A) > 20) {
            l_A = l_A - 1;
        }
        l_8 = l_8 + 3;
    }
    draw_sprite(MK32(g_room_gfx2_hi, g_room_gfx2_lo + 0xA), 6, 0x53);
    present_frame();
    int mp_out = 0;
    if (g_fired_player(0) == g_player && seat_humans) {
        /* multiplayer: the human sees it is fired, as in the original -- then
         * its station leaves the game as a computer player's does, and the
         * game goes on for the others (instead of the title screen) */
        g_clip_on = 0;
        l_6 = timer_read();
        while ((timer_read()) < ((u16)(l_6 + 0x64))) seat_pass();
        draw_sprite(MK32(g_actor_txt_hi, g_actor_txt_lo + 2), 0xE, 0x1D);
        draw_sprite(MK32(g_actor_txt_hi, g_actor_txt_lo + 0xA), 0x54, 0x1D);
        draw_sprite(MK32(g_actor_txt_hi, g_actor_txt_lo + 0x12), 0x9F, 0x1D);
        draw_sprite(MK32(g_actor_txt_hi, g_actor_txt_lo + 0x1A), 0xEC, 0x1D);
        screen_transition(2, 1, 4, 0 /* unused ax */);
        g_click = 0;
        while (g_click == 0) game_frame();
        mp_out = 1;
    }
    if (g_fired_player(0) != g_player || mp_out) {
        players(g_fired_player(0)).out = 1;
        G32((u16)((g_fired_player(0) * 0x3C) + 0x6E29) + 0x14) = 0;
        si = 0;
        while (((i16)(g_player_ad_count(g_fired_player(0)))) > ((i16)si)) {
            LOOP_TICK(25);
            ads(contracts(g_fired_player(0), si).ad).holder = 0;
            si = si + 1;
        }
        g_player_ad_count(g_fired_player(0)) = 0;
        if (g_fired_player(0) == g_tv_channel) {
            g_tv_channel = g_player;
        }
        si = 1;
        while (((i16)si) <= 10) {
            LOOP_TICK(39);
            di = 0;
            while (((i16)((u16)(i8)(library_count(g_fired_player(0), si)))) > ((i16)di)) {
                LOOP_TICK(34);
                films(library_items(g_fired_player(0), si, di)).owner = 0;
                di = di + 1;
            }
            si = si + 1;
        }
        di = 0;
        while (((i16)((u16)(i8)(library_count(g_fired_player(0), 0xB)))) > ((i16)di)) {
            LOOP_TICK(33);
            series(library_items(g_fired_player(0), si, di)).owner = 0;
            di = di + 1;
        }
        players(g_fired_player(0)).betty_love = 0;
        si = 1;
        while (((i16)si) <= 12) {
            LOOP_TICK(26);
            l_4 = studios_at(si);
            if (((u16)((u16)(i8)(AT(studio, l_4).renter))) == g_fired_player(0)) {
                AT(studio, l_4).script = 0;
                AT(studio, l_4).running = 0;
                AT(studio, l_4).renter = 0xFF;
                studio_give_back(si);
            }
            si = si + 1;
        }
        shoplists(0x1A, (g_fired_player(0) * 2) + 4).script = 0;
        fig_target_door(g_fired_player(0) - 1) = 0xFFFF;
        elevator_queue_remove(g_fired_player(0) - 1);
        g_clip_on = 0;
        ui_draw_player_icons();
        g_fired_player(0) = 0;
        room_draw_background(0x69);
        screen_transition(1, 1, 4, 0 /* unused ax */);
        if (mp_out) mp_seat_out();       /* this seat now only watches (runtime/seat.c) */
    } else {
        g_clip_on = 0;
        l_6 = timer_read();
        for (;;) {
            LOOP_TICK(5);
            if ((timer_read()) < ((u16)(l_6 + 0x64))) {
                seat_pass();             /* seats: a wait point (runtime/seat.h) */
                continue;
            } else {
                break;
            }
        }
        draw_sprite(MK32(g_actor_txt_hi, g_actor_txt_lo + 2), 0xE, 0x1D);
        draw_sprite(MK32(g_actor_txt_hi, g_actor_txt_lo + 0xA), 0x54, 0x1D);
        draw_sprite(MK32(g_actor_txt_hi, g_actor_txt_lo + 0x12), 0x9F, 0x1D);
        draw_sprite(MK32(g_actor_txt_hi, g_actor_txt_lo + 0x1A), 0xEC, 0x1D);
        screen_transition(2, 1, 4, 0 /* unused ax */);
        g_click = 0;
        while (g_click == 0) {
            LOOP_TICK(3);
            game_frame();
        }
        mem_free_ptr(HPTR(DSP(0x110)));
        mem_free_ptr(HPTR(DSP(0x114)));
        rtl_snd_room_music_stop(0x3E8, 0);
        game_load(0x2B, 0x2B);
        g_game_mode = 0;
        g_room = ROOM_BUILDING;
        g_room_stack_top = 0;
        cursor_hide();
        ui_status_text(G16(0x8768), G16(0x876A), 0xB4);
        panel_restore_strip();
        ui_draw_player_icons();
    }
    mem_free_ptr(HPTR(DSP(0x10C)));
    mem_free_ptr(HPTR(DSP(0x108)));
    t64 = mem_free_ptr(HPTR(DSP(0x104)));
    g_fired_player(0) = 0;
    t65 = cursor_show(t64);
    return game_clock_resume(t65);
}

/* 193D:2C8A  draw the elevator-shaft background tiles used during the fired animation */
void fired_draw_background(void) {
    u16 di;
    u16 si;
    u16 l_2;
    u16 l_4;
    u16 t4;
    u16 t5;
    u16 t6;
    u16 t7;
    u16 t11;
    u16 t12;
    u16 t13;
    u16 t14;
    u16 t18;
    u16 t19;
    u16 t20;
    u16 t21;
    FUNC_TICK(18);
    g_bld_full_redraw = 1;
    bld_draw_side_scenery();
    g_clip_y1 = 4;
    g_clip_y2 = 0x71;
    l_4 = 1;
    while (((i16)l_4) <= 5) {
        LOOP_TICK(104);
        l_2 = l_4 * 0x1E;
        if (((i16)g_bld_scroll) >= 30 || l_4 != 5) {
            di = 0;
        } else {
            di = 3;
        }
        t4 = (u16)((i32)(i16)g_bld_scroll % 30);
        t5 = g_screen_buf_hi;
        t6 = g_screen_buf_lo;
        t7 = col_to_px(di + 2);
        blit_opaque_to(MK32(g_room_gfx1_hi, g_room_gfx1_lo + t7), t6, t5, 0x38, (l_2 - (0x1E - t4)) + 0xFFDB);
        t11 = (u16)((i32)(i16)g_bld_scroll % 30);
        t12 = g_screen_buf_hi;
        t13 = g_screen_buf_lo;
        t14 = col_to_px(di + 3);
        blit_opaque_to(MK32(g_room_gfx1_hi, g_room_gfx1_lo + t14), t13, t12, 0xF0, (l_2 - (0x1E - t11)) + 0xFFDB);
        si = 1;
        while (((i16)si) <= 4) {
            LOOP_TICK(33);
            t18 = (u16)((i32)(i16)g_bld_scroll % 30);
            t19 = g_screen_buf_hi;
            t20 = g_screen_buf_lo;
            t21 = col_to_px(di + 1);
            blit_opaque_to(MK32(g_room_gfx1_hi, g_room_gfx1_lo + t21), t20, t19, (si * 0x28) + 0x28, (l_2 - (0x1E - t18)) + 0xFFDB);
            si = si + 1;
        }
        l_4 = l_4 + 1;
    }
    return;
}
