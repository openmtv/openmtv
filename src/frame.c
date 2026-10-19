/* Frame: per-frame update and presentation, text input, Madris step.
 *
 * Original module 0ED7 of MTV.EXE (Rainbow Arts, 1991).  Decompiled by
 * tools/decomp.py and hand-maintained since the freeze (git tag
 * generated-final).  Every function keeps its original address in the
 * comment above it; that address ties it to the machine-level twin in gen/
 * and to the difftest tables -- keep it when editing.
 */
#include "game.h"
#include "seat.h"
#include "fixes.h"

/* 0ED7:0004  one frame of the main loop: input_poll -> g_click; ease cursor toward pending mouse motion (instant while dragging), clamp to g_mouse_min/max, redraw cursor or drag image flicker-free on VGA; every 50/g_game_speed timer ticks: building (193D:0C1C + present + tvfam_tick) or room (room_anim_update, Madris in office screen 2, tvfam_tick) then tooltip_update; returns room id of the door the idle player figure stands at (building only), else 0 */
u16 game_frame(void) {
    u16 ax;
    u16 di;
    u32 rv_;
    u16 t1;
    u16 t84;
    u16 t85;
    u16 t86;
    u16 t87;
    u16 t184;
    u16 t186;
    u16 t187;
    u16 t188;
    u16 t196;
    u16 t197;
    u16 t199;
    u16 t200;
    u16 t201;
    u16 t203;
    u8 fr_[22] = {0};                  /* bytes bp-22 .. bp-1 of the original frame */
    FUNC_TICK(191);
    if (seat_cur > 0) seat_requests_run();   /* seats: what the world asked of this seat */
    t1 = input_poll();
    g_click = t1;
    g_mouse_click = 0;
    di = 0;
    arrow_refresh();                   /* --mouse system (fixes.c) */
    drag_refresh();
    if ((figures(g_player_figure).motion) != 0x45) {
        if (((u32)MK32(g_mouse_pending_dx, g_mouse_pending_dy)) == 0) {
            if (g_cursor_drawn_x == 0xFFFF) {
                for (;;) {
L_06CC:
                    LOOP_TICK(584);
                    if (g_mouse_pending_dx == 0 && g_mouse_pending_dy == 0 && g_cursor_drawn_x != 0xFFFF) break;
                    if ((g_drag_image_lo | g_drag_image_hi) != 0) {
                        g_mouse_x = g_mouse_x + g_mouse_pending_dx;
                        g_mouse_pending_dx = 0;
                    } else {
                        mouse_ease_step(DSP(0x7A48), DSP(0x7BF6));
                    }
                    if (((i16)g_mouse_x) > ((i16)g_mouse_max_x)) {
                        g_mouse_x = g_mouse_max_x;
                    }
                    if (((i16)g_mouse_x) < ((i16)g_mouse_min_x)) {
                        g_mouse_x = g_mouse_min_x;
                    }
                    if ((g_drag_image_lo | g_drag_image_hi) != 0) {
                        g_mouse_y = g_mouse_y + g_mouse_pending_dy;
                        g_mouse_pending_dy = 0;
                    } else {
                        mouse_ease_step(DSP(0x7A4A), DSP(0x7BF8));
                    }
                    if (((i16)g_mouse_y) > ((i16)g_mouse_max_y)) {
                        g_mouse_y = g_mouse_max_y;
                    }
                    if (((i16)g_mouse_y) < ((i16)g_mouse_min_y)) {
                        g_mouse_y = g_mouse_min_y;
                    }
                    if (g_cursor_shown == 1) {
                        rtl_vga_get_bitmap_w(W2(g_cursor_comp), ((i16)(((i16)(((i16)g_mouse_x) >> 1)) >> 1)) >> 1, g_mouse_y);
                        if (((i16)(((i16)(((i16)(((i16)g_mouse_x) >> 1)) >> 1)) >> 1)) >= ((i16)((((i16)(((i16)(((i16)g_cursor_drawn_x) >> 1)) >> 1)) >> 1) + 3))) {
L_0157:
                            bitmap_copy(g_cursor_comp_lo, g_cursor_comp_hi, g_cursor_tmp_lo, g_cursor_tmp_hi);
                            arrow_cover(g_cursor_comp, 0);
                            blit_transparent(g_cursor_sprites(g_cursor_shape), g_cursor_comp, g_mouse_x & 7, 0, 0, 0, 0x10, 0xA, 0 /* unused ax */);
                            arrow_uncover();
                        } else if (((i16)(((i16)(((i16)(((i16)g_cursor_drawn_x) >> 1)) >> 1)) >> 1)) >= ((i16)((((i16)(((i16)(((i16)g_mouse_x) >> 1)) >> 1)) >> 1) + 3)) || ((i16)(g_cursor_drawn_y + 0xB)) <= ((i16)g_mouse_y) || ((i16)(g_mouse_y + 0xB)) <= ((i16)g_cursor_drawn_y)) {
                            goto L_0157;
                        } else {
                            if (((i16)(((i16)(((i16)(((i16)g_cursor_drawn_x) >> 1)) >> 1)) >> 1)) <= ((i16)(((i16)(((i16)(((i16)g_mouse_x) >> 1)) >> 1)) >> 1))) {
                                FR16(fr_, 22, 0x4) = 0;
                                FR16(fr_, 22, 0x10) = (((i16)(((i16)(((i16)g_mouse_x) >> 1)) >> 1)) >> 1) - (((i16)(((i16)(((i16)g_cursor_drawn_x) >> 1)) >> 1)) >> 1);
                                FR16(fr_, 22, 0xC) = 3 - FR16(fr_, 22, 0x10);
                            } else {
                                FR16(fr_, 22, 0x4) = (((i16)(((i16)(((i16)g_cursor_drawn_x) >> 1)) >> 1)) >> 1) - (((i16)(((i16)(((i16)g_mouse_x) >> 1)) >> 1)) >> 1);
                                FR16(fr_, 22, 0x10) = 0;
                                FR16(fr_, 22, 0xC) = 3 - FR16(fr_, 22, 0x4);
                            }
                            if (((i16)g_cursor_drawn_y) <= ((i16)g_mouse_y)) {
                                FR16(fr_, 22, 0x2) = 0;
                                FR16(fr_, 22, 0xE) = g_mouse_y - g_cursor_drawn_y;
                                FR16(fr_, 22, 0xA) = 0xB - FR16(fr_, 22, 0xE);
                            } else {
                                FR16(fr_, 22, 0x2) = g_cursor_drawn_y - g_mouse_y;
                                FR16(fr_, 22, 0xE) = 0;
                                FR16(fr_, 22, 0xA) = 0xB - FR16(fr_, 22, 0x2);
                            }
                            blit_opaque_rect(g_cursor_bg, g_cursor_comp, FR16(fr_, 22, 0x4) << 3, FR16(fr_, 22, 0x2), FR16(fr_, 22, 0x10) << 3, FR16(fr_, 22, 0xE), FR16(fr_, 22, 0xC) << 3, FR16(fr_, 22, 0xA), 0 /* unused ax */);
                            bitmap_copy(g_cursor_comp_lo, g_cursor_comp_hi, g_cursor_tmp_lo, g_cursor_tmp_hi);
                            arrow_cover(g_cursor_comp, 0);
                            draw_sprite_to(g_cursor_sprites(g_cursor_shape), g_cursor_comp_lo, g_cursor_comp_hi, g_mouse_x & 7, 0);
                            arrow_uncover();
                            t84 = FPOFF(g_cursor_comp);
                            FR16(fr_, 22, 0x12) = FPSEG(g_cursor_comp);
                            FR16(fr_, 22, 0x14) = t84;
                            t85 = FPOFF(g_cursor_bg);
                            FR16(fr_, 22, 0x6) = FPSEG(g_cursor_bg);
                            FR16(fr_, 22, 0x8) = t85;
                            t86 = FR16(fr_, 22, 0x4);
                            FR16(fr_, 22, 0x4) = FR16(fr_, 22, 0x10);
                            FR16(fr_, 22, 0x10) = t86;
                            t87 = FR16(fr_, 22, 0x2);
                            FR16(fr_, 22, 0x2) = FR16(fr_, 22, 0xE);
                            FR16(fr_, 22, 0xE) = t87;
                            rtl_blit_opaque_aligned((fr_ + 22 - 0x14));
                        }
                        rtl_vga_put_bitmap_w(W2(g_cursor_bg), ((i16)(((i16)(((i16)g_cursor_drawn_x) >> 1)) >> 1)) >> 1, g_cursor_drawn_y);
                        rtl_vga_put_bitmap_w(W2(g_cursor_comp), ((i16)(((i16)(((i16)g_mouse_x) >> 1)) >> 1)) >> 1, g_mouse_y);
                        bitmap_copy(g_cursor_tmp_lo, g_cursor_tmp_hi, g_cursor_bg_lo, g_cursor_bg_hi);
                        g_cursor_drawn_x = g_mouse_x;
                        g_cursor_drawn_y = g_mouse_y;
                        continue;
                    } else {
                        if (g_cursor_shape != 2) continue;
                        g_drag_draw_x = g_mouse_x - g_drag_hot_x;
                        g_drag_draw_y = g_mouse_y - g_drag_hot_y;
                        if (((i16)g_drag_draw_x) >= 0) {
                            if (((i16)(g_drag_draw_x + g_drag_w)) > 319) {
                                g_drag_draw_x = 0x13F - g_drag_w;
                            }
                        } else {
                            g_drag_draw_x = 0;
                        }
                        if (((i16)g_drag_draw_y) >= 0) {
                            if (((i16)(g_drag_draw_y + g_drag_h)) > 120) {
                                g_drag_draw_y = 0x78 - g_drag_h;
                            }
                        } else {
                            g_drag_draw_y = 0;
                        }
                        if (!(g_drag_draw_x != g_cursor_drawn_x || g_drag_draw_y != g_cursor_drawn_y)) continue;
                        rtl_vga_get_bitmap_w(W2(g_cursor_comp), ((i16)(((i16)(((i16)g_drag_draw_x) >> 1)) >> 1)) >> 1, g_drag_draw_y);
                        if (((i16)((((i16)(((i16)(((i16)g_cursor_drawn_x) >> 1)) >> 1)) >> 1) + (((i16)(((i16)(((i16)(g_drag_w + 0xF)) >> 1)) >> 1)) >> 1))) <= ((i16)(((i16)(((i16)(((i16)g_drag_draw_x) >> 1)) >> 1)) >> 1))) {
L_046F:
                            blit_opaque_rect(g_cursor_comp, g_cursor_tmp, 0, 0, 0, 0, g_drag_w + 8, g_drag_h, 0 /* unused ax */);
                            drag_cover(g_cursor_comp);   /* (a network game: the window draws it at the pointer, fixes.c) */
                            blit_transparent(g_drag_image, g_cursor_comp, g_drag_draw_x & 7, 0, 0, 0, g_drag_w, g_drag_h, 0 /* unused ax */);
                            drag_uncover();
                        } else if (((i16)((((i16)(((i16)(((i16)g_drag_draw_x) >> 1)) >> 1)) >> 1) + (((i16)(((i16)(((i16)(g_drag_w + 0xF)) >> 1)) >> 1)) >> 1))) <= ((i16)(((i16)(((i16)(((i16)g_cursor_drawn_x) >> 1)) >> 1)) >> 1)) || ((i16)(g_cursor_drawn_y + g_drag_h)) <= ((i16)g_drag_draw_y) || ((i16)(g_drag_draw_y + g_drag_h)) <= ((i16)g_cursor_drawn_y) || g_cursor_drawn_x == 0xFFFF) {
                            goto L_046F;
                        } else {
                            if (((i16)g_cursor_drawn_x) <= ((i16)g_drag_draw_x)) {
                                FR16(fr_, 22, 0x4) = 0;
                                FR16(fr_, 22, 0x10) = (((i16)(((i16)(((i16)g_drag_draw_x) >> 1)) >> 1)) >> 1) - (((i16)(((i16)(((i16)g_cursor_drawn_x) >> 1)) >> 1)) >> 1);
                                FR16(fr_, 22, 0xC) = ((((i16)(((i16)(((i16)(g_drag_w + 7)) >> 1)) >> 1)) >> 1) - FR16(fr_, 22, 0x10)) + 1;
                            } else {
                                FR16(fr_, 22, 0x4) = (((i16)(((i16)(((i16)g_cursor_drawn_x) >> 1)) >> 1)) >> 1) - (((i16)(((i16)(((i16)g_drag_draw_x) >> 1)) >> 1)) >> 1);
                                FR16(fr_, 22, 0x10) = 0;
                                FR16(fr_, 22, 0xC) = ((((i16)(((i16)(((i16)(g_drag_w + 7)) >> 1)) >> 1)) >> 1) - FR16(fr_, 22, 0x4)) + 1;
                            }
                            if (((i16)g_cursor_drawn_y) <= ((i16)g_drag_draw_y)) {
                                FR16(fr_, 22, 0x2) = 0;
                                FR16(fr_, 22, 0xE) = g_drag_draw_y - g_cursor_drawn_y;
                                FR16(fr_, 22, 0xA) = g_drag_h - FR16(fr_, 22, 0xE);
                            } else {
                                FR16(fr_, 22, 0x2) = g_cursor_drawn_y - g_drag_draw_y;
                                FR16(fr_, 22, 0xE) = 0;
                                FR16(fr_, 22, 0xA) = g_drag_h - FR16(fr_, 22, 0x2);
                            }
                            blit_opaque_rect(g_cursor_bg, g_cursor_comp, FR16(fr_, 22, 0x4) << 3, FR16(fr_, 22, 0x2), FR16(fr_, 22, 0x10) << 3, FR16(fr_, 22, 0xE), FR16(fr_, 22, 0xC) << 3, FR16(fr_, 22, 0xA), 0 /* unused ax */);
                            blit_opaque_rect(g_cursor_comp, g_cursor_tmp, 0, 0, 0, 0, g_drag_w + 8, g_drag_h, 0 /* unused ax */);
                            drag_cover(g_cursor_comp);   /* (a network game: the window draws it at the pointer, fixes.c) */
                            blit_transparent(g_drag_image, g_cursor_comp, g_drag_draw_x & 7, 0, 0, 0, g_drag_w, g_drag_h, 0 /* unused ax */);
                            drag_uncover();
                            t184 = FPOFF(g_cursor_comp);
                            FR16(fr_, 22, 0x12) = FPSEG(g_cursor_comp);
                            FR16(fr_, 22, 0x14) = t184;
                            t186 = FPOFF(g_cursor_bg);
                            FR16(fr_, 22, 0x6) = FPSEG(g_cursor_bg);
                            FR16(fr_, 22, 0x8) = t186;
                            t187 = FR16(fr_, 22, 0x4);
                            FR16(fr_, 22, 0x4) = FR16(fr_, 22, 0x10);
                            FR16(fr_, 22, 0x10) = t187;
                            t188 = FR16(fr_, 22, 0x2);
                            FR16(fr_, 22, 0x2) = FR16(fr_, 22, 0xE);
                            FR16(fr_, 22, 0xE) = t188;
                            rtl_blit_opaque_aligned((fr_ + 22 - 0x14));
                        }
                        if (g_cursor_drawn_x != 0xFFFF) {
                            rtl_vga_put_bitmap_w(W2(g_cursor_bg), ((i16)(((i16)(((i16)g_cursor_drawn_x) >> 1)) >> 1)) >> 1, g_cursor_drawn_y);
                        }
                        rtl_vga_put_bitmap_w(W2(g_cursor_comp), ((i16)(((i16)(((i16)g_drag_draw_x) >> 1)) >> 1)) >> 1, g_drag_draw_y);
                        blit_opaque_rect(g_cursor_tmp, g_cursor_bg, 0, 0, 0, 0, g_drag_w + 8, g_drag_h, 0 /* unused ax */);
                        g_cursor_drawn_x = g_drag_draw_x;
                        g_cursor_drawn_y = g_drag_draw_y;
                        continue;
                    }
                }
            }
        } else {
            goto L_06CC;
        }
    }
    if (g_room != 0xFFFB) {
        if (g_clock_halt == 0) {
            if (world_now()) {               /* seats: the world in seat 0 (runtime/seat.h) */
                ai_tick();
            }
        }
        if (g_room == ROOM_BUILDING) {
            t196 = timer_read();
            t197 = (u16)((i32)50 / (i16)g_frame_divisor);
            if (t196 >= ((u16)(t197 + g_frame_timer))) {
                t199 = timer_read();
                g_frame_timer = t199;
                if (g_npc_type != 0 && world_now()) {
                    npc_event_update();
                }
                g_present_bld_only = 0;
                bld_update_frame();
                if (g_present_bld_only != 0x270F) {
                    g_present_bld_only = 1;
                }
                present_frame();
                g_present_bld_only = 0;
                tvfam_tick();
                if ((figures(g_player_figure).task) == 0) {
                    di = floors_door_room(figures(g_player_figure).floor, figures(g_player_figure).target_door);
                }
            }
        } else {
            t200 = timer_read();
            t201 = (u16)((i32)50 / (i16)g_frame_divisor);
            if (t200 >= ((u16)(t201 + g_frame_timer))) {
                t203 = timer_read();
                g_frame_timer = t203;
                if (g_npc_type != 0 && world_now()) {
                    npc_event_update();
                }
                bld_update_frame();
                room_anim_update(1, 0 /* unused ax */);
                tvfam_tick();
                if (g_room == ROOM_OFFICE && ((u32)MK32(g_room_mode, g_tip_visible)) == 0x20000) {
                    if (((i16)g_madris_state) < 42) {
                        g_madris_state = g_madris_state + 1;
                    }
                    g_madris_tick = g_madris_tick + 1;
                    if (((i16)g_madris_tick) < ((i16)g_madris_delay)) {
                        if (g_madris_input != 5) {
                            if (g_madris_input != 0) {
                                madris_step(0);
                            }
                        } else {
                            madris_step(1);
                        }
                    } else {
                        g_madris_tick = 0;
                        madris_step(1);
                    }
                    tetris_draw(2, 0 /* unused ax */);
                    present_frame();
                }
            }
            tooltip_update();
        }
    }
    if (!(g_clock_halt != 0 || g_room == 0xFFFB || ((u32)(MK32(G16(0x7A8A), g_clock_paused))) != 0)) {
        if (world_now()) {
            game_time_tick(0, 0 /* unused ax */);
        } else {
            seat_status_tick();              /* another seat: its clock and money (src/seatctx.c) */
        }
    }
    ax = di;
    rv_ = ax;
    if (seat_humans && seat_cur == 0) seat_paused[0] = g_room == 0xFFFB;   /* (a human's pause stops the world) */
    seat_yield();                        /* seats: this seat's frame is done */
    return (u16)rv_;
}

/* 0ED7:08B8  wait for retrace, draw cursor, show the back buffer */
u16 present_frame(void) {
    u16 si;
    u32 rv_;
    u16 t3;
    u16 t5;
    u8 fr_[18] = {0};                  /* bytes bp-18 .. bp-1 of the original frame */
    FUNC_TICK(146);
    rtl_wait_retrace();
    if (g_room == ROOM_BUILDING || ((u32)MK32(g_room, g_room_mode)) == 0x10002) {
        if (!(((i16)g_mouse_y) >= 120 || g_cursor_shown != 1)) {
            clip_reset();
            blit_opaque_rect(g_screen_buf, g_cursor_tmp, 0, 0, g_mouse_x & 0xFFF8, g_mouse_y, 0x18, 0xB, 0 /* unused ax */);
            arrow_cover(g_screen_buf, (i16)g_mouse_y);
            draw_sprite(g_cursor_sprites(g_cursor_shape), g_mouse_x, g_mouse_y);
            arrow_uncover();
            g_clip_on = 0;
        }
    } else {
        cursor_hide();
    }
    if (g_present_bld_only != 1) {
        rtl_vga_put_bitmap_w(W2(g_screen_buf), 0, 0);
    } else {
        t3 = FPOFF(g_screen_buf);
        FR16(fr_, 18, 0xE) = FPSEG(g_screen_buf);
        FR16(fr_, 18, 0x10) = t3;
        FR16(fr_, 18, 0x2) = 4;
        FR16(fr_, 18, 0xA) = 4;
        FR16(fr_, 18, 0x6) = 0x6D;
        if (g_bld_scroll != G16(0xD28) || G16(0x7A2C) == 1) {
            FR16(fr_, 18, 0x4) = 0;
            FR16(fr_, 18, 0xC) = 0;
            FR16(fr_, 18, 0x8) = 0x28;
            rtl_vga_put_rect((fr_ + 18 - 0x10));
        } else {
            FR16(fr_, 18, 0x4) = 7;
            FR16(fr_, 18, 0xC) = 7;
            FR16(fr_, 18, 0x8) = 0x1A;
            rtl_vga_put_rect((fr_ + 18 - 0x10));
        }
    }
    if (g_room == ROOM_BUILDING || (g_room == ROOM_OFFICE && g_room_mode == 2)) {
        if (((i16)g_mouse_y) < 120 && g_cursor_shown == 1) {
            clip_reset();
            bitmap_put_screen(g_cursor_tmp, g_mouse_x & 0xFFF8, g_mouse_y);
            g_clip_on = 0;
            if (((i16)g_mouse_y) < 110) {
                si = 0xB;
            } else {
                si = 0x78 - g_mouse_y;
            }
            blit_opaque_rect(g_screen_buf, g_cursor_bg, 0, 0, g_mouse_x & 0xFFF8, g_mouse_y, 0x18, si, 0 /* unused ax */);
            bitmap_copy(g_cursor_bg_lo, g_cursor_bg_hi, g_cursor_comp_lo, g_cursor_comp_hi);
        }
    } else {
        cursor_show(0 /* unused ax */);
    }
    t5 = g_bld_scroll;
    G16(0xD28) = g_bld_scroll;
    rv_ = t5;
    return (u16)rv_;
}

/* 0ED7:0A8C  copy bottom panel bitmap (320x80) to VGA y=120 with box-wipe transition (0054:34BD), compositing the mouse cursor when it is over the panel */
void panel_show_wipe(void) {
    u16 si;
    FUNC_TICK(107);
    if (!(((i16)g_mouse_y) <= 109 || g_cursor_shown != 1)) {
        clip_cursor_cell();
        blit_opaque_rect(g_panel_bmp, g_cursor_tmp, 0, 0, g_mouse_x & 0xFFF8, g_mouse_y + 0xFF88, 0x18, 0xB, 0 /* unused ax */);
        clip_panel();
        arrow_cover(g_screen_buf, (i16)(g_mouse_y + 0xFF88));
        draw_sprite(g_cursor_sprites(g_cursor_shape), g_mouse_x, g_mouse_y + 0xFF88);
        arrow_uncover();
    }
    screen_wipe(g_panel_bmp_lo, g_panel_bmp_hi, 0, 0x78, 0x140, 0x50, 0, 1);
    rtl_vga_put_bitmap_w(W2(g_panel_bmp), 0, 0x78);
    if (((i16)g_mouse_y) > 109 && g_cursor_shown == 1) {
        clip_panel();
        bitmap_put(g_cursor_tmp, g_panel_bmp, g_mouse_x & 0xFFF8, g_mouse_y + 0xFF88);
        si = 0xB;
        if (((i16)g_mouse_y) < 120) {
            si = 0x78 - g_mouse_y;
        }
        clip_cursor_cell();
        blit_opaque_rect(g_screen_buf, g_cursor_bg, 0, si, g_mouse_x & 0xFFF8, (g_mouse_y + 0xFF88) - si, 0x18, si, 0 /* unused ax */);
    }
    return;
}

/* 0ED7:0BBE  mouse limits = x 0..303, y 0..189 */
void mouse_bounds_full(void) {
    FUNC_TICK(5);
    G32(0x79EC) = 0;
    G32(0x79F0) = 0xBD012F;
    return;
}

/* 0ED7:0BD7  text entry field (x, y, width_cols, buf): letters (first upper case, rest lower), digits, space, backspace; redraws with cursor glyph (0xD35); returns 0 on Enter, -1 on Esc */
u16 ui_input_text(u16 a0, u16 a1, u16 a2, u32 a3) {
    u16 ax;
    u16 di;
    u16 si;
    u16 l_2;
    u32 l_6;
    u32 l_A;
    u16 t11;
    u16 t12;
    u16 t13;
    u16 t14;
    u32 t18;
    u32 t24;
    FUNC_TICK(65);
    cursor_hide();
    l_6 = bitmap_alloc((a2 + 2) << 3, 8);
    blit_opaque_rect(g_screen_buf, l_6, 0, 0, a0 & 0xFFF8, a1, (a2 + 2) << 3, 8, 0 /* unused ax */);
    l_A = DSP(0xD35);
    si = a0;
    di = a1;
    for (;;) {
        LOOP_TICK(136);
        t24 = rtl_key_read();
        l_2 = LO16(t24);
        if (LO16(t24) == 0x1F || l_2 == 0x14) break;
        blit_opaque_rect(l_6, g_screen_buf, si & 0xFFF8, di, 0, 0, (a2 + 2) << 3, 8, 0 /* unused ax */);
        text_draw_2(si, di, a3, g_screen_buf, 0);
        t11 = g_screen_buf_hi;
        t12 = g_screen_buf_lo;
        t13 = text_width_px(a3);
        text_draw_2(t13 + si, di, l_A, MK32(t11, t12), 0);
        present_frame();
        t14 = text_width_px(a3);
        ax = ((i16)(((i16)(((i16)t14) >> 1)) >> 1)) >> 1;
        if (((i16)(((i16)(((i16)(((i16)t14) >> 1)) >> 1)) >> 1)) < ((i16)(a2 - 1))) {
            if (((i16)l_2) < 38 || ((i16)l_2) > 63) {
                if (((i16)l_2) < 10 || ((i16)l_2) > 19) {
                    if (l_2 == 0x20) {
                        FP8(a3, LO16(rtl_strlen_w(W2(a3)))) = (u8)((((u8)((u8)(ax >> 8))) << 8) | ((u8)((u8)l_2)));
                    }
                } else {
                    FP8(a3, LO16(rtl_strlen_w(W2(a3)))) = (u8)((((u8)((u8)(ax >> 8))) << 8) | ((u8)(((u8)l_2) + 0x26)));
                }
            } else {
                t18 = rtl_strlen_w(W2(a3));
                ax = LO16(t18);
                if (LO16(t18) == 0) {
                    ax = (((u8)((u8)(ax >> 8))) << 8) | ((u8)(((u8)l_2) + 0x1B));
                } else {
                    ax = (((u8)((u8)(ax >> 8))) << 8) | ((u8)(((u8)l_2) + 0x3B));
                }
                FP8(a3, LO16(rtl_strlen_w(W2(a3)))) = (u8)ax;
            }
        }
        if (l_2 != 0x21 || LO16((rtl_strlen_w(W2(a3)))) == 0) continue;
        MEM8(FPSEG(a3), (FPOFF(a3) + LO16(rtl_strlen_w(W2(a3)))) - 1) = 0;
    }
    mem_free(LO16(l_6), HI16(l_6));
    cursor_show(0 /* unused ax */);
    if (l_2 != 0x1F) {
        ax = 0xFFFF;
    } else {
        ax = 0;
    }
    return ax;
}

/* 0ED7:0DE2  shape != 0: push g_cursor_shape and switch to shape (3 = busy, used while loading texts); 0: pop previous shape; ignored while dragging */
u16 cursor_shape_push(u16 a0) {
    u16 ax;
    u16 t2;
    FUNC_TICK(26);
    ax = g_drag_image_lo | g_drag_image_hi;
    if ((g_drag_image_lo | g_drag_image_hi) == 0) {
        cursor_hide();
        if (a0 != 0) {
            g_cursor_shape_stack(g_cursor_shape_sp) = g_cursor_shape;
            g_cursor_shape_sp = g_cursor_shape_sp + 1;
            g_cursor_shape = a0;
            ax = a0;
        } else {
            g_cursor_shape_sp = g_cursor_shape_sp - 1;
            t2 = g_cursor_shape_stack(g_cursor_shape_sp);
            g_cursor_shape = g_cursor_shape_stack(g_cursor_shape_sp);
            ax = t2;
        }
        ax = cursor_show(ax);
    }
    return ax;
}

/* 0ED7:0E2E  "Madris" Tetris clone on the office computer (res 0xB5 sprite 10): spawn next of 7 pieces (random_n(7)), apply input (1/2 rotate, 3/4 move, 5 drop) with collision tests on the 10x20 board, fall by arg rows; on landing score += piece_points[O10 S8 Z8 T6 J6 L6 I4] * (9 - delay), score >= 9999 or blocks in top row = game over, full lines removed, every 10 lines delay-- (min 1); keeps hiscore */
void madris_step(u16 a0) {
    u16 ax;
    u16 di;
    u16 si;
    u8 l_1;
    u16 l_4;
    u16 l_6;
    u16 l_8;
    u16 l_A;
    u16 t1;
    u16 t36;
    u16 t42;
    u16 t45;
    FUNC_TICK(186);
    if (g_madris_state == 0x2A) {
        if (g_madris_piece == 0) {
            g_madris_piece = g_madris_next;
            g_madris_rot = 0;
            t1 = random_n(7, 1);
            g_madris_next = t1;
            di = 3;
            while (((i16)di) >= 0) {
                LOOP_TICK(29);
                l_1 = 1;
                si = 0;
                while (((i16)si) < 4) {
                    LOOP_TICK(21);
                    if ((G8(di + (((((g_madris_piece - 1) << 6) + 0x39C) + (g_madris_rot << 4)) + (si << 2)))) != 0) {
                        l_1 = 0;
                    }
                    si = si + 1;
                }
                if (l_1 == 0) break;
                di = di - 1;
            }
            G32(0x8F2C) = MK32(0 - di, 3);
            g_madris_drawn_x = 0x3E7;
        }
        if (g_madris_drawn_x != 0x3E7) {
            di = g_madris_drawn_x;
            while (((i16)(g_madris_drawn_x + 4)) > ((i16)di)) {
                LOOP_TICK(49);
                si = g_madris_drawn_y;
                while (((i16)(g_madris_drawn_y + 4)) > ((i16)si)) {
                    LOOP_TICK(42);
                    if ((G8((((((g_madris_drawn_piece - 1) << 6) + 0x39C) + (g_madris_drawn_rot << 4)) + ((di - g_madris_drawn_x) << 2)) + (si - g_madris_drawn_y))) != 0) {
                        if (((i16)si) >= 0) {
                            if (((i16)si) < 20) {
                                if (((i16)di) >= 0) {
                                    if (((i16)di) < 10) {
                                        G8(di + ((si * 0xA) + 0x8AD4)) = 0;
                                    }
                                }
                            }
                        }
                    }
                    si = si + 1;
                }
                di = di + 1;
            }
        }
        if (g_madris_input == 1) {
            l_6 = g_madris_rot;
            l_6 = l_6 - 1;
            if (((i32)((i16)l_6) - 1) < 0) {
                l_6 = 3;
            }
            l_1 = 0;
            if (((i16)g_madris_x) < 0) {
                di = g_madris_x;
                while (((i16)di) < 0) {
                    LOOP_TICK(27);
                    si = 0;
                    while (((i16)si) < 4) {
                        LOOP_TICK(22);
                        if ((G8(si + (((((g_madris_piece - 1) << 6) + 0x39C) + (l_6 << 4)) + ((di - g_madris_x) << 2)))) != 0) {
                            l_1 = 1;
                        }
                        si = si + 1;
                    }
                    di = di + 1;
                }
            }
            if (((i16)g_madris_x) > 6) {
                di = g_madris_x + 3;
                while (((i16)di) > 9) {
                    LOOP_TICK(27);
                    si = 0;
                    while (((i16)si) < 4) {
                        LOOP_TICK(22);
                        if ((G8(si + (((((g_madris_piece - 1) << 6) + 0x39C) + (l_6 << 4)) + ((di - g_madris_x) << 2)))) != 0) {
                            l_1 = 1;
                        }
                        si = si + 1;
                    }
                    di = di - 1;
                }
            }
            di = g_madris_x;
            while (((i16)(g_madris_x + 4)) > ((i16)di)) {
                LOOP_TICK(51);
                si = g_madris_y;
                while (((i16)(g_madris_y + 4)) > ((i16)si)) {
                    LOOP_TICK(44);
                    if (((i16)di) >= 0) {
                        if (((i16)di) < 10) {
                            if (((i16)si) >= 0) {
                                if (((i16)si) < 20) {
                                    if ((G8((((((g_madris_piece - 1) << 6) + 0x39C) + (l_6 << 4)) + ((di - g_madris_x) << 2)) + (si - g_madris_y))) != 0) {
                                        if ((G8(di + ((si * 0xA) + 0x8AD4))) != 0) {
                                            l_1 = 1;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    si = si + 1;
                }
                di = di + 1;
            }
            if (l_1 == 0) {
                g_madris_rot = l_6;
            }
        } else if (g_madris_input == 2) {
            l_6 = g_madris_rot;
            l_6 = l_6 + 1;
            if (((i16)l_6) > 3) {
                l_6 = 0;
            }
            l_1 = 0;
            if (((i16)g_madris_x) < 0) {
                di = g_madris_x;
                while (((i16)di) < 0) {
                    LOOP_TICK(27);
                    si = 0;
                    while (((i16)si) < 4) {
                        LOOP_TICK(22);
                        if ((G8(si + (((((g_madris_piece - 1) << 6) + 0x39C) + (l_6 << 4)) + ((di - g_madris_x) << 2)))) != 0) {
                            l_1 = 1;
                        }
                        si = si + 1;
                    }
                    di = di + 1;
                }
            }
            if (((i16)g_madris_x) > 6) {
                di = g_madris_x + 3;
                while (((i16)di) > 9) {
                    LOOP_TICK(27);
                    si = 0;
                    while (((i16)si) < 4) {
                        LOOP_TICK(22);
                        if ((G8(si + (((((g_madris_piece - 1) << 6) + 0x39C) + (l_6 << 4)) + ((di - g_madris_x) << 2)))) != 0) {
                            l_1 = 1;
                        }
                        si = si + 1;
                    }
                    di = di - 1;
                }
            }
            di = g_madris_x;
            while (((i16)(g_madris_x + 4)) > ((i16)di)) {
                LOOP_TICK(51);
                si = g_madris_y;
                while (((i16)(g_madris_y + 4)) > ((i16)si)) {
                    LOOP_TICK(44);
                    if (((i16)di) >= 0) {
                        if (((i16)di) < 10) {
                            if (((i16)si) >= 0) {
                                if (((i16)si) < 20) {
                                    if ((G8((((((g_madris_piece - 1) << 6) + 0x39C) + (l_6 << 4)) + ((di - g_madris_x) << 2)) + (si - g_madris_y))) != 0) {
                                        if ((G8(di + ((si * 0xA) + 0x8AD4))) != 0) {
                                            l_1 = 1;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    si = si + 1;
                }
                di = di + 1;
            }
            if (l_1 == 0) {
                g_madris_rot = l_6;
            }
        } else if (g_madris_input == 3) {
            l_4 = g_madris_x;
            l_4 = l_4 - 1;
            l_1 = 0;
            if (((i16)l_4) < 0) {
                di = l_4;
                while (((i16)di) < 0) {
                    LOOP_TICK(27);
                    si = 0;
                    while (((i16)si) < 4) {
                        LOOP_TICK(22);
                        if ((G8(si + (((((g_madris_piece - 1) << 6) + 0x39C) + (g_madris_rot << 4)) + ((di - l_4) << 2)))) != 0) {
                            l_1 = 1;
                        }
                        si = si + 1;
                    }
                    di = di + 1;
                }
            }
            di = l_4;
            while (((i16)(l_4 + 4)) > ((i16)di)) {
                LOOP_TICK(51);
                si = g_madris_y;
                while (((i16)(g_madris_y + 4)) > ((i16)si)) {
                    LOOP_TICK(44);
                    if (((i16)di) >= 0) {
                        if (((i16)di) < 10) {
                            if (((i16)si) >= 0) {
                                if (((i16)si) < 20) {
                                    if ((G8((((((g_madris_piece - 1) << 6) + 0x39C) + (g_madris_rot << 4)) + ((di - l_4) << 2)) + (si - g_madris_y))) != 0) {
                                        if ((G8(di + ((si * 0xA) + 0x8AD4))) != 0) {
                                            l_1 = 1;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    si = si + 1;
                }
                di = di + 1;
            }
            if (l_1 == 0) {
                g_madris_x = l_4;
            }
        } else if (g_madris_input == 4) {
            l_4 = g_madris_x;
            l_4 = l_4 + 1;
            l_1 = 0;
            if (((i16)l_4) > 6) {
                di = l_4 + 3;
                while (((i16)di) > 9) {
                    LOOP_TICK(27);
                    si = 0;
                    while (((i16)si) < 4) {
                        LOOP_TICK(22);
                        if ((G8(si + (((((g_madris_piece - 1) << 6) + 0x39C) + (g_madris_rot << 4)) + ((di - l_4) << 2)))) != 0) {
                            l_1 = 1;
                        }
                        si = si + 1;
                    }
                    di = di - 1;
                }
            }
            di = l_4;
            while (((i16)(l_4 + 4)) > ((i16)di)) {
                LOOP_TICK(51);
                si = g_madris_y;
                while (((i16)(g_madris_y + 4)) > ((i16)si)) {
                    LOOP_TICK(44);
                    if (((i16)di) >= 0) {
                        if (((i16)di) < 10) {
                            if (((i16)si) >= 0) {
                                if (((i16)si) < 20) {
                                    if ((G8((((((g_madris_piece - 1) << 6) + 0x39C) + (g_madris_rot << 4)) + ((di - l_4) << 2)) + (si - g_madris_y))) != 0) {
                                        if ((G8(di + ((si * 0xA) + 0x8AD4))) != 0) {
                                            l_1 = 1;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    si = si + 1;
                }
                di = di + 1;
            }
            if (l_1 == 0) {
                g_madris_x = l_4;
            }
        }
        if (g_madris_input != 5) {
            g_madris_input = 0;
        }
        l_8 = 0;
        if (((i16)g_madris_y) > 16) {
            si = g_madris_y;
            while (((i16)(g_madris_y + 4)) > ((i16)si)) {
                LOOP_TICK(40);
                if (((i16)si) > 19) {
                    l_1 = 1;
                    di = g_madris_x;
                    while (((i16)(g_madris_x + 4)) > ((i16)di)) {
                        LOOP_TICK(27);
                        if ((G8((((((g_madris_piece - 1) << 6) + 0x39C) + (g_madris_rot << 4)) + ((di - g_madris_x) << 2)) + (si - g_madris_y))) != 0) {
                            l_1 = 0;
                        }
                        di = di + 1;
                    }
                    if (l_1 == 0) {
                        l_8 = 1;
                    }
                }
                si = si + 1;
            }
        }
        di = g_madris_x;
        while (((i16)(g_madris_x + 4)) > ((i16)di)) {
            LOOP_TICK(51);
            si = g_madris_y;
            while (((i16)(g_madris_y + 4)) > ((i16)si)) {
                LOOP_TICK(44);
                if (((i16)si) >= 0) {
                    if (((i16)si) < 20) {
                        if (((i16)di) >= 0) {
                            if (((i16)di) < 10) {
                                if ((G8((((((g_madris_piece - 1) << 6) + 0x39C) + (g_madris_rot << 4)) + ((di - g_madris_x) << 2)) + (si - g_madris_y))) != 0) {
                                    if ((G8(di + ((si * 0xA) + 0x8AD4))) != 0) {
                                        l_8 = 1;
                                    }
                                }
                            }
                        }
                    }
                }
                si = si + 1;
            }
            di = di + 1;
        }
        if (l_8 != 0) {
            g_madris_input = 0;
            g_madris_score = g_madris_score + (((u16)(i8)g_madris_points(g_madris_piece)) * (9 - g_madris_delay));
            if (((i16)g_madris_score) > 9999) {
                G32(0x7A74) = 0x270F002B;
            }
            if (((i16)g_madris_score) > ((i16)g_madris_hiscore)) {
                g_madris_hiscore = g_madris_score;
            }
            g_madris_piece = 0;
            di = g_madris_drawn_x;
            while (((i16)(g_madris_drawn_x + 4)) > ((i16)di)) {
                LOOP_TICK(54);
                si = g_madris_drawn_y;
                while (((i16)(g_madris_drawn_y + 4)) > ((i16)si)) {
                    LOOP_TICK(47);
                    t36 = (((u8)((u8)(((u16)(si - g_madris_drawn_y)) >> 8))) << 8) | (G8((((((g_madris_drawn_piece - 1) << 6) + 0x39C) + (g_madris_drawn_rot << 4)) + ((di - g_madris_drawn_x) << 2)) + (si - g_madris_drawn_y)));
                    l_1 = G8((((((g_madris_drawn_piece - 1) << 6) + 0x39C) + (g_madris_drawn_rot << 4)) + ((di - g_madris_drawn_x) << 2)) + (si - g_madris_drawn_y));
                    ax = t36;
                    if (((u8)((u8)t36)) != 0) {
                        if (((i16)si) >= 0) {
                            if (((i16)si) < 20) {
                                if (((i16)di) >= 0) {
                                    if (((i16)di) < 10) {
                                        G8(di + ((si * 0xA) + 0x8AD4)) = (u8)((((u8)((u8)(ax >> 8))) << 8) | l_1);
                                    }
                                }
                            }
                        }
                    }
                    si = si + 1;
                }
                di = di + 1;
            }
            si = 0x13;
            while (((i16)si) >= 0) {
                LOOP_TICK(79);
                l_1 = 0xA;
                di = 0;
                while (((i16)di) < 10) {
                    LOOP_TICK(13);
                    if ((G8(di + ((si * 0xA) + 0x8AD4))) != 0) {
                        l_1 = l_1 - 1;
                    }
                    di = di + 1;
                }
                if (l_1 == 0) {
                    l_A = si;
                    while (((i16)l_A) > 0) {
                        LOOP_TICK(27);
                        di = 0;
                        while (((i16)di) < 10) {
                            LOOP_TICK(22);
                            G8(di + ((l_A * 0xA) + 0x8AD4)) = (u8)((((u8)((u8)(DSEG >> 8))) << 8) | (G8(di + (((l_A - 1) * 0xA) + 0x8AD4))));
                            di = di + 1;
                        }
                        l_A = l_A - 1;
                    }
                    di = 0;
                    while (((i16)di) < 10) {
                        LOOP_TICK(4);
                        g_madris_board(di) = 0;
                        di = di + 1;
                    }
                    g_madris_lines = g_madris_lines + 1;
                    t42 = (u16)((i32)(i16)g_madris_lines % 10);
                    si = si + 1;
                    if (!(t42 != 0 || ((i16)g_madris_delay) <= 1)) {
                        g_madris_delay = g_madris_delay - 1;
                    }
                }
                l_1 = 0;
                di = 0;
                while (((i16)di) < 10) {
                    LOOP_TICK(6);
                    if (g_madris_board(di) != 0) {
                        l_1 = 1;
                    }
                    di = di + 1;
                }
                if (l_1 == 1) {
                    g_madris_state = 0x2B;
                }
                si = si - 1;
            }
        } else {
            di = g_madris_x;
            while (((i16)(g_madris_x + 4)) > ((i16)di)) {
                LOOP_TICK(54);
                si = g_madris_y;
                while (((i16)(g_madris_y + 4)) > ((i16)si)) {
                    LOOP_TICK(47);
                    t45 = (((u8)((u8)(((u16)(si - g_madris_y)) >> 8))) << 8) | (G8((((((g_madris_piece - 1) << 6) + 0x39C) + (g_madris_rot << 4)) + ((di - g_madris_x) << 2)) + (si - g_madris_y)));
                    l_1 = G8((((((g_madris_piece - 1) << 6) + 0x39C) + (g_madris_rot << 4)) + ((di - g_madris_x) << 2)) + (si - g_madris_y));
                    ax = t45;
                    if (((u8)((u8)t45)) != 0) {
                        if (((i16)si) >= 0) {
                            if (((i16)si) < 20) {
                                if (((i16)di) >= 0) {
                                    if (((i16)di) < 10) {
                                        G8(di + ((si * 0xA) + 0x8AD4)) = (u8)((((u8)((u8)(ax >> 8))) << 8) | l_1);
                                    }
                                }
                            }
                        }
                    }
                    si = si + 1;
                }
                di = di + 1;
            }
            g_madris_drawn_piece = g_madris_piece;
            g_madris_drawn_rot = g_madris_rot;
            g_madris_drawn_x = g_madris_x;
            g_madris_drawn_y = g_madris_y;
            g_madris_y = g_madris_y + a0;
        }
    }
    return;
}

/* 0ED7:1752  ui_band_to into the screen buffer */
void ui_band(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4) {
    FUNC_TICK(13);
    ui_band_to(a0, a1, a2, a3, a4, g_screen_buf_lo, g_screen_buf_hi);
    return;
}

/* 0ED7:1775  draw a horizontal band (col, y, width_cols, height_px, style, dst): end caps from res 2 atlas (sprite style*3+0x13..) and filler tiles from res 7, repeated in 8px rows; used for info-card row backgrounds */
void ui_band_to(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5, u16 a6) {
    u16 di;
    u16 si;
    u16 l_2;
    u16 t4;
    u16 t11;
    u16 t18;
    FUNC_TICK(19);
    l_2 = (a4 * 3) + 0x13;
    di = a3 - 8;
    while (di != 0xFFFF) {
        LOOP_TICK(94);
        t4 = col_to_px(l_2 + 2);
        draw_sprite_to(MK32(g_ui_atlas_hi, g_ui_atlas_lo + t4), a5, a6, (a0 << 3) + 2, a1);
        t11 = col_to_px(l_2);
        draw_sprite_to(MK32(g_ui_atlas_hi, g_ui_atlas_lo + t11), a5, a6, (a0 << 3) + (a2 << 3), a1);
        si = 1;
        while (((i16)si) < ((i16)a2)) {
            LOOP_TICK(29);
            t18 = col_to_px(a4 + 0xE);
            bitmap_put(MK32(g_band_tiles_hi, g_band_tiles_lo + t18), MK32(a6, a5), (a0 << 3) + (si << 3), a1);
            si = si + 1;
        }
        if (((i16)di) < 8) {
            if (((i16)di) <= 0) {
                if (di != 0) continue;
                di = 0xFFFF;
                continue;
            } else {
                a1 = a1 + di;
                di = 0;
                continue;
            }
        } else {
            a1 = a1 + 8;
            di = di - 8;
            continue;
        }
    }
    return;
}
