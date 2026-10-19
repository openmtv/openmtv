/* Screens: planner, transmitter map, auction board, information board, news room screens.
 *
 * Original module 26A7 of MTV.EXE (Rainbow Arts, 1991).  Decompiled by
 * tools/decomp.py and hand-maintained since the freeze (git tag
 * generated-final).  Every function keeps its original address in the
 * comment above it; that address ties it to the machine-level twin in gen/
 * and to the difftest tables -- keep it when editing.
 */
#include "game.h"
#include "hr.h"
#include "fixes.h"

/* 26A7:000D  (far str) show "Warning: <str>" (text 89) in a modal message box (screen -1) */
void ui_warning_str(u16 a0, u16 a1) {
    u32 l_4;
    FUNC_TICK(29);
    l_4 = str_format(g_text(89), a0, a1);
    room_enter(0xFFFF);
    ui_message_box(0, LO16(l_4), HI16(l_4));
    mem_free(LO16(l_4), HI16(l_4));
    return;
}

/* 26A7:005D  (text id) ui_warning_str(g_text[id]) */
void ui_warning(u16 a0) {
    FUNC_TICK(12);
    ui_warning_str(g_text_lo(a0), g_text_hi(a0));
    return;
}

/* 26A7:0077  (far str) show a modal message box (screen -3) */
void ui_message_str(u16 a0, u16 a1) {
    FUNC_TICK(14);
    room_enter(0xFFFD);
    ui_message_box(0, a0, a1);
    return;
}

/* 26A7:0097  (mode, far str) open (0/2) or close (1) the message window and draw the text */
u16 ui_message_box(u16 a0, u16 a1, u16 a2) {
    FUNC_TICK(33);
    ui_box(a0, 0xA, 3, 0x1E, 8, 1, 0 /* unused ax */);
    if (a0 != 1) {
        text_draw_wrap(0x54, 0x1C, a1, a2, 0x13, 1);
    }
    return present_frame();
}

/* 26A7:00E4  print money_available(human)/1000 into the status panel */
u16 ui_draw_money(void) {
    u32 l_4;
    u32 t2;
    FUNC_TICK(36);
    t2 = (u32)((i32)money_available(g_player) / (i32)0x3E8);
    l_4 = str_format(DSP(0x1346) /* "#_6l" */, W2(t2));
    ui_status_text(LO16(l_4), HI16(l_4), 0x7F);
    return mem_free(LO16(l_4), HI16(l_4));
}

/* 26A7:013C  (far str, y) print a text into the status panel bitmap and blit it to the screen (hides the cursor if it overlaps) */
void ui_status_text(u16 a0, u16 a1, u16 a2) {
    int cc;
    u16 di;
    u16 si;
    FUNC_TICK(55);
    clip_reset();
    bitmap_copy(G16(0x8E86), G16(0x8E88), g_cursor_sprites_lo(5), g_cursor_sprites_hi(5));
    text_draw_center_to(0x15, 0xFFFA, a0, a1, g_cursor_sprites_lo(5), g_cursor_sprites_hi(5));
    si = 0;
    di = a2;
    if (((i16)(a2 + 0xFFF7)) <= ((i16)g_mouse_y)) {
        cc = ((i16)(di + 0xA)) < ((i16)g_mouse_y);
        if (!(cc || ((i16)g_mouse_y) < 136 || ((i16)g_mouse_x) > 200 || g_cursor_shown != 1)) {
            cursor_hide();
            si = 1;
        }
    }
    if (g_tvfam_frozen == 0) {
        rtl_vga_put_bitmap_w(W2(g_cursor_sprites(5)), 0x13, di);
    }
    if (si != 0) {
        cursor_show(0 /* unused ax */);
    }
    return;
}

/* 26A7:01DF  46-segment bar of the human's Betty love (percent) in the status panel */
void ui_draw_betty_meter(void) {
    u16 ax;
    u16 s0;
    u16 s1;
    u16 s2;
    u16 s3;
    u16 si;
    u32 l_4;
    u16 t2;
    u16 t5;
    FUNC_TICK(39);
    clip_panel();
    l_4 = bitmap_alloc(0x30, 2);
    rtl_vga_get_bitmap_w(W2(l_4), 0x10, 0xAB);
    si = 1;
    while (((i16)si) <= 46) {
        LOOP_TICK(40);
        t2 = players(g_player).betty_love;
        s0 = 0;
        s1 = si;
        s2 = HI16(l_4);
        s3 = LO16(l_4);
        if (((i16)t2) < ((i16)(u16)((u16)((i32)(i16)(si * 0x64) / 46)))) {
            ax = 0xD;
        } else {
            ax = 0xC;
        }
        t5 = col_to_px(ax);
        draw_sprite_to(MK32(g_tvfam_atlas_hi, g_tvfam_atlas_lo + t5), s3, s2, s1, s0);
        si = si + 1;
    }
    hr_betty_meter(l_4, players(g_player).betty_love, g_tvfam_atlas);   /* --filter hr: 184 steps (src/hr.c) */
    rtl_vga_put_bitmap_w(W2(l_4), 0x10, 0xAB);
    mem_free(LO16(l_4), HI16(l_4));
    return;
}

/* 26A7:02A2  (player) draw the program planner screen (frame, buttons, message icon) and its rows */
u16 ui_draw_planner(u16 a0) {
    u16 di;
    u16 si;
    u16 t3;
    FUNC_TICK(64);
    room_draw_border_alt();
    G32(0x386) = 0x40001;
    G32(0x38A) = 0x13B0004;
    g_clip_y2 = 0x74;
    g_tile_target = g_screen_buf;
    g_tile_atlas = g_room_gfx;
    di = 0;
    while (((i16)di) < 113) {
        LOOP_TICK(26);
        g_tile_fixed_y = di;
        si = 8;
        while (((i16)si) < 312) {
            LOOP_TICK(8);
            tile_draw_at_x(0x3A, si);
            si = si + 8;
        }
        tile_draw_at_x(0x32, 4);
        tile_draw_at_x(0x32, 0x138);
        di = di + 8;
    }
    di = 1;
    while (((i16)di) <= 6) {
        LOOP_TICK(17);
        t3 = col_to_px(di);
        draw_atlas_tile(t3, 0x128, (di * 0x11) + 0xFFF7);
        di = di + 1;
    }
    if (!(((i8)g_message_pos) <= 0 || g_room_owner != g_player)) {
        draw_atlas_tile(0x2BA, 0x128, 0x5D);
    }
    ui_tile_box(8, 8, 0x20, 0x72, 1);
    g_tile_target = g_screen_buf;
    draw_atlas_tile(0x102, 0xC, 0x5D);
    return ui_draw_planner_rows(a0);
}

/* 26A7:03C6  (player) the 7 visible hours: "HH:00", program title, ad name with "spot n/total", "OK" (aired) or "---" (failed) */
u16 ui_draw_planner_rows(u16 a0) {
    u16 ax;
    u16 di;
    u16 si;
    u32 rv_;
    u32 t12;
    u16 t13;
    u16 t14;
    u32 t15;
    u32 t23;
    u32 t24;
    u32 t25;
    u32 t27;
    u16 l_10 = 0;
    u16 l_E = 0;
    u16 l_C = 0;
    u16 l_A = 0;
    u16 l_8 = 0;
    u16 l_6 = 0;
    u16 l_4 = 0;
    u16 l_2 = 0;
    FUNC_TICK(58);
    clip_reset();
    ui_draw_planner_grid(a0);
    g_tile_atlas = g_room_gfx;
    g_tile_target = g_screen_buf;
    draw_atlas_tile(0x42, 0xC, 0xC);
    draw_atlas_tile(0x4A, 0xC, 0x1E);
    if (g_planner_scroll(0) != 0) {
        ax = 0x1E;
    } else {
        ax = 0xC;
    }
    draw_atlas_tile(0x52, 0xC, ax);
    g_font = 1;
    l_10 = g_planner_scroll(0) + 1;
    for (;;) {
        LOOP_TICK(264);
        ax = g_planner_scroll(0) + 7;
        if (((i16)(g_planner_scroll(0) + 7)) < ((i16)l_10)) break;
        l_8 = ((l_10 - g_planner_scroll(0)) << 4) + 0xFFEE;
        l_E = l_10;
        if (((i16)l_E) > 7) {
            l_E = l_E - 7;
        }
        l_E = l_E + 0x11;
        if (((i16)l_E) >= 24) {
            l_E = l_E - 0x18;
        }
        t12 = str_format(DSP(0x134B) /* "#02d:00" */, l_E);
        text_draw_wrap_free(0x2A, l_8, LO16(t12), HI16(t12), 0, 2);
        t13 = g_screen_buf_hi;
        t14 = g_screen_buf_lo;
        t15 = sched_slot_title(l_10, a0);
        text_draw_full_free(0x4A, l_8, LO16(t15), HI16(t15), 0x11, 1, t14, t13, 2);
        si = schedule_ad(g_room_owner, l_10);
        if ((schedule_ad(g_room_owner, l_10)) != 0) {
            if (((i16)si) < 1000) {
                di = 0;
                while (((i16)g_player_ad_count(g_room_owner)) > ((i16)di)) {
                    LOOP_TICK(20);
                    if ((contracts(g_room_owner, di).ad) != si) {
                        di = di + 1;
                        continue;
                    } else {
                        goto L_052C;
                    }
                }
L_0560:
                if (((i16)g_player_ad_count(g_room_owner)) <= ((i16)di)) {
                    l_6 = (u16)(i8)(ads(si).spots);
                }
                l_A = 1;
                l_C = clock_hour_round55();
                while (((i16)l_C) < ((i16)l_10)) {
                    LOOP_TICK(18);
                    if ((schedule_ad(g_room_owner, l_C)) == si) {
                        l_A = l_A + 1;
                    }
                    l_C = l_C + 1;
                }
                t23 = str_format(DSP(0x1353) /* "#*7s~#d/#d" */, W2(ads(si).name), l_6 + l_A, (u16)(i8)(ads(si).spots));
                l_2 = HI16(t23);
                l_4 = LO16(t23);
            } else if (((i16)si) >= 2000) {
                if (((i16)si) >= 3000) {
                    l_A = ad_spot_number(g_room_owner, l_10);
                    t27 = str_format(DSP(0x1367) /* "#*7s~#d/#d" */, W2(ad_name(si + 0xF448)), l_A, (u16)(i8)ad_spots(si + 0xF448));
                    l_2 = HI16(t27);
                    l_4 = LO16(t27);
                } else {
                    t25 = str_format(g_text(284), W2(ad_name(si + 0xF830)));
                    l_2 = HI16(t25);
                    l_4 = LO16(t25);
                }
            } else {
                t24 = str_format(DSP(0x135E) /* "#*7s~---" */, W2(ad_name(si + 0xFC18)));
                l_2 = HI16(t24);
                l_4 = LO16(t24);
            }
            text_draw_full_free(0xDA, l_8, l_4, l_2, 8, 0, g_screen_buf_lo, g_screen_buf_hi, 2);
        }
        l_10 = l_10 + 1;
    }
    g_font = 0;
    rv_ = ax;
    return (u16)rv_;
L_052C:
    l_6 = (u16)(i8)(contracts(g_room_owner, di).aired);
    goto L_0560;
}

/* 26A7:0723  (player) row boxes for time, program (width = blocks*16) and ad, styled by past/current/future hour */
void ui_draw_planner_grid(u16 a0) {
    int cc;
    u16 di;
    u16 si;
    u16 t2;
    u16 t5;
    u16 t8;
    u16 l_6 = 0;
    u16 l_4 = 0;
    u16 l_2 = 0;
    FUNC_TICK(14);
    si = 1;
    while (((i16)si) <= 7) {
        LOOP_TICK(139);
        l_6 = ((si - 1) << 4) + 4;
        cc = ((i16)clock_hour()) < ((i16)si);
        if (cc || g_planner_scroll(0) != 0) {
            di = 0;
        } else {
            di = 2;
        }
        ui_tile_box(0x28, l_6, 0x48, l_6 + 0x10, di);
        t2 = clock_hour();
        if (((i16)t2) > ((i16)si)) {
L_0790:
            if (g_planner_scroll(0) != 0) {
L_079C:
                di = 0;
            } else {
                di = 2;
            }
        } else {
            cc = clock_hour() != si;
            if ((cc || ((i16)g_minute) < 5) && g_clock_halt == 0) {
                goto L_079C;
            } else {
                goto L_0790;
            }
        }
        t5 = schedule_program(a0, si + g_planner_scroll(0));
        l_2 = schedule_program(a0, si + g_planner_scroll(0));
        if (((i16)t5) <= 0 || ((i16)l_2) > 2000) {
            if (l_2 == 0) {
                l_4 = 0x10;
                di = di + 1;
            }
        } else if (((i16)l_2) >= 1000) {
            l_4 = ((u16)(i8)(series_blocks(l_2 + 0xFC18))) << 4;
        } else {
            l_4 = ((u16)(i8)(films(l_2).blocks)) << 4;
        }
        if (l_2 != 0x270F) {
            ui_tile_box(0x48, l_6, 0xD8, l_6 + l_4, di);
        }
        t8 = clock_hour();
        if (((i16)t8) > ((i16)si)) {
L_0854:
            if (g_planner_scroll(0) != 0) {
L_0860:
                di = 0;
            } else {
                di = 2;
            }
        } else {
            cc = clock_hour() != si;
            if (cc || ((i16)g_minute) < 55) {
                goto L_0860;
            } else {
                goto L_0854;
            }
        }
        if ((schedule_ad(a0, si + g_planner_scroll(0))) == 0) {
            di = di + 1;
        }
        ui_tile_box(0xD8, l_6, 0x118, l_6 + 0x10, di);
        si = si + 1;
    }
    return;
}

/* 26A7:08B0  (hour, player) allocated title string of a schedule slot: film title, series "title: episode no: episode title", empty for 0/9999, "Fehler aufgetreten!" otherwise */
u32 sched_slot_title(u16 a0, u16 a1) {
    u16 ax;
    int cc;
    u16 di;
    u16 dx;
    u16 si;
    u32 l_4;
    u32 l_8;
    u32 t3;
    u16 t6;
    u32 t7;
    u16 t8;
    u32 t9;
    u32 t12;
    u16 t14;
    u32 t15;
    FUNC_TICK(141);
    l_4 = schedule_at(a1);
    si = FP16(l_4, a0 << 1);
    di = a0;
    if (FP16(l_4, a0 << 1) != 0) {
        if (((i16)si) >= 1000) {
            if (((i16)si) < 2000) {
                cc = (FP16(l_4, (di << 1) + 0x1E)) != 0;
                si = si - 0x3E8;
                if (cc) {
                    t12 = series_episode_title(si, FP16(l_4, (di << 1) + 0x1E));
                    t14 = FP16(l_4, (di << 1) + 0x1E);
                    t15 = series_title(si);
                    l_8 = str_format(DSP(0x1389) /* "#*17s:~#d:#*15s" */, W2(t15), t14, W2(t12));
                } else {
                    t6 = series_episode_at(a1, si, di);
                    t7 = series_episode_title(si, t6);
                    t8 = series_episode_at(a1, si, di);
                    t9 = series_title(si);
                    l_8 = str_format(DSP(0x1379) /* "#*17s:~#d:#*15s" */, W2(t9), t8, W2(t7));
                }
            } else if (si != 0x270F) {
                l_8 = str_format(DSP(0x139A) /* "Fehler aufgetreten!" */);
            } else {
                l_8 = str_format(DSP(0x1399));
            }
        } else {
            t3 = movie_title(si);
            l_8 = str_format(DSP(0x1373) /* "#*34s" */, W2(t3));
        }
    } else {
        l_8 = str_format(DSP(0x1372));
    }
    ax = LO16(l_8);
    dx = HI16(l_8);
    return MK32(dx, ax);
}

/* 26A7:0A02  (genre, idx, player) program_title of a library entry (genre 11 = series) */
u32 library_item_title(u16 a0, u16 a1, u16 a2) {
    u16 ax;
    u16 dx;
    u16 s0;
    u32 t5;
    FUNC_TICK(32);
    s0 = library_items(a2, a0, a1);
    if (a0 != 0xB) {
        ax = 0;
    } else {
        ax = 0x3E8;
    }
    t5 = program_title(s0 + ax);
    ax = LO16(t5);
    dx = HI16(t5);
    return MK32(dx, ax);
}

/* 26A7:0A49  (id) allocated title of film / series (id >= 1000), "(Empty)" for 0 */
u32 program_title(u16 a0) {
    u16 ax;
    u16 dx;
    u16 si;
    u32 l_4;
    u32 t2;
    u32 t4;
    FUNC_TICK(51);
    si = a0;
    if (a0 != 0) {
        if (((i16)si) >= 1000) {
            t4 = series_title(si - 0x3E8);
            l_4 = str_format(g_text(93), W2(t4));
        } else {
            t2 = movie_title(si);
            l_4 = str_format(DSP(0x13AE), W2(t2));
        }
    } else {
        l_4 = text_dup(0x5C);
    }
    ax = LO16(l_4);
    dx = HI16(l_4);
    return MK32(dx, ax);
}

/* 26A7:0AC0  (x, y, mode, player) mode 0 save background and list the programs of the selected genre, mode 1 restore */
u16 ui_library_genre_popup(u16 a0, u16 a1, u16 a2, u16 a3) {
    u16 ax;
    int cc;
    u16 di;
    u16 si;
    u16 l_2;
    u16 l_4;
    u32 l_8;
    u32 t16;
    u32 t24;
    u32 t25;
    FUNC_TICK(94);
    l_4 = a0 << 3;
    clip_reset();
    ax = a0 << 3;
    di = a1 << 3;
    if (a2 == 0) {
        G16(0x8FD2) = popup_open(a0, a1, a0 + 0x14, a1 + 6, 0, 0, 3);
        ui_tile_box(l_4, di, (a0 + 0x14) << 3, ((a1 + 6) << 3) + 2, 0);
        g_font = 1;
        t16 = str_format(DSP(0x13B1) /* "#s:" */, W2(G32(((g_selected_genre + 0x1A) << 2) + 0x8390)));
        text_draw_free(l_4 + 4, di + 0xFFFA, LO16(t16), HI16(t16));
        si = 0;
        for (;;) {
            LOOP_TICK(87);
            cc = ((i16)((u16)(i8)library_count(a3, g_selected_genre))) <= ((i16)si);
            ax = (u16)(i8)library_count(a3, g_selected_genre);
            if (cc) break;
            l_2 = si * 0xA;
            cc = library_items(a3, g_selected_genre, si) != 0;
            ax = si << 1;
            if (!(cc)) break;
            ui_tile_box(l_4 + 4, (di + l_2) + 8, ((a0 + 0x14) << 3) + 0xFFFC, (di + l_2) + 0x12, 0);
            t24 = library_item_title(g_selected_genre, si, a3);
            l_8 = t24;
            t25 = str_format(DSP(0x13B5) /* "#*18s" */, W2(t24));
            text_draw_free(l_4 + 5, (di + l_2) + 3, LO16(t25), HI16(t25));
            mem_free(LO16(l_8), HI16(l_8));
            si = si + 1;
        }
        g_font = 0;
    } else if (a2 == 1) {
        ax = popup_close(G16(0x8FD2), ax);
    }
    return ax;
}

/* 26A7:0C7B  (sel) transmitter map screen: player legend, buy/sell buttons for the own map, transmitters of g_map_player (sel 0 = all, else only that one) */
u16 ui_draw_transmitter_map(u16 a0) {
    u16 ax;
    int cc;
    u16 di;
    u16 si;
    u16 t3;
    u16 t8;
    u16 t9;
    u16 t10;
    FUNC_TICK(179);
    clip_reset();
    g_clip_y2 = 0x71;
    g_tile_atlas = g_room_gfx1;
    g_tile_target = g_screen_buf;
    si = 4;
    while (((i16)si) < 113) {
        LOOP_TICK(14);
        di = 4;
        while (((i16)di) < 40) {
            LOOP_TICK(9);
            draw_atlas_tile(0x62, di, si);
            di = di + 8;
        }
        si = si + 8;
    }
    bitmap_put_screen(MK32(g_room_gfx_hi, g_room_gfx_lo + 2), 0x28, 4);
    si = 4;
    while (((i16)si) < 113) {
        LOOP_TICK(14);
        di = 0xD8;
        while (((i16)di) < 312) {
            LOOP_TICK(9);
            draw_atlas_tile(0x62, di, si);
            di = di + 8;
        }
        si = si + 8;
    }
    si = 4;
    while (((i16)si) < 113) {
        LOOP_TICK(10);
        draw_atlas_tile(0x62, 0x134, si);
        si = si + 8;
    }
    draw_atlas_tile(0x32, 5, 0x46);
    draw_atlas_tile(0x2A, 0xDC, 0x28);
    si = 1;
    while (((i16)si) <= 3) {
        LOOP_TICK(46);
        if ((players(si).out) == 0) {
            t3 = col_to_px(si + 7);
            draw_atlas_tile(t3, 0x100, (si * 0xD) + 0x37);
            text_draw(0x102, (si * 0xD) + 0x31, pl_name_lo(si), pl_name_hi(si));
        }
        si = si + 1;
    }
    if (((i16)g_map_player) > 0) {
        draw_atlas_tile(0x6A, 0x100, (g_map_player * 0xD) + 0x37);
    }
    ax = g_room_owner;
    if (g_room_owner == g_player) {
        draw_atlas_tile(0x52, 0x108, 0x17);
        draw_atlas_tile(0x52, 0x108, 0x24);
        text_draw(0x10A, 0x11, G16(0x8874), G16(0x8876));
        ax = text_draw(0x10A, 0x1E, G16(0x88DC), G16(0x88DE));
        if (g_room_mode == 3) {
            ax = draw_atlas_tile(0x5A, 0x108, 0x31);
        }
    }
    if (g_map_player != 0) {
        if (!(((contracts(6, (g_map_player * 0x42) + 1).ad) | (G16((g_map_player * 0x1CE) + 0x1D1D))) == 0 || (tx(g_map_player, 1).state) == 2 || !(a0 == 0 || a0 == 1))) {
            draw_atlas_tile(0xA, 5, 0x46);
        }
        ax = (contracts(6, (g_map_player * 0x42) + 3).ad) | (G16((g_map_player * 0x1CE) + 0x1D2B));
        if (((contracts(6, (g_map_player * 0x42) + 3).ad) | (G16((g_map_player * 0x1CE) + 0x1D2B))) != 0) {
            ax = DSEG;
            if (!((tx(g_map_player, 2).state) == 2 || !(a0 == 0 || a0 == 2))) {
                ax = draw_atlas_tile(2, 0xDC, 0x28);
            }
        }
        si = 3;
        while (((i16)si) <= 32) {
            LOOP_TICK(73);
            cc = ((contracts(5, ((g_map_player * 0x42) + (si * 2)) + 9).ad) | (G16((((g_map_player * 0x1CE) + 0x1D09) + (si * 0xE)) + 6))) != 0;
            ax = (contracts(5, ((g_map_player * 0x42) + (si * 2)) + 9).ad) | (G16((((g_map_player * 0x1CE) + 0x1D09) + (si * 0xE)) + 6));
            if (cc) {
                cc = (tx(g_map_player, si).state) == 2;
                ax = si * 0xE;
                if (!(cc)) {
                    if (a0 == 0) {
L_0F7C:
                        t8 = (tx(g_map_player, si).y) + 4;
                        t9 = (tx(g_map_player, si).x) + 0x30;
                        t10 = col_to_px(g_map_player + 2);
                        ax = draw_atlas_tile(t10, t9, t8);
                    } else {
                        cc = a0 != si;
                        ax = a0;
                        if (!(cc)) goto L_0F7C;
                    }
                }
            }
            si = si + 1;
        }
    }
    return ax;
}

/* 26A7:0FE5  (mode) movie agency: draw the tapes of the 10 offers still on the shelf (mode 0 saves the background first) */
void ui_draw_dealer_shelf(u16 a0) {
    u16 si;
    u32 t1;
    u16 t11;
    u16 t12;
    u16 t13;
    u16 bx, bw, bh;
    FUNC_TICK(40);
    clip_reset();
    filmoffers_shelf_bg(&bx, &bw, &bh);  /* --film-offers: a wider background for the longer row (film_offers.c) */
    if (a0 != 0) {
        cursor_hide();
        bitmap_put_screen(g_room_gfx7, bx, 0x3B);
        /* --film-offers: the wider background holds Mr. Jabble as he sat at the room's entry
         * (x 16..87, y 59..70): his last animation frame over it again */
        if (g_nfilmoffers > 10 && (g_room_gfx6_lo | g_room_gfx6_hi) != 0) blit_opaque(g_room_gfx6, 0x10, 0x12);
        cursor_show(0 /* unused ax */);
    } else {
        t1 = bitmap_alloc(bw, bh);
        g_room_gfx7 = t1;
        cursor_hide();
        grab_screen(g_room_gfx7, bx, 0x3B);
        cursor_show(0 /* unused ax */);
        filmoffers_prepare();            /* --film-offers: the new tapes, out of this room's bank (film_offers.c) */
    }
    si = 0xA;
    while (((i16)si) >= 1) {
        LOOP_TICK(39);
        if (((i8)(dealer(si).shelf)) >= 0) {
            t11 = g_filmshelf_rects((si * 4) + 1);
            t12 = g_filmshelf_rects(si * 4);
            t13 = col_to_px(si);
            draw_sprite(MK32(g_room_gfx3_hi, g_room_gfx3_lo + t13), t12, t11);
        }
        si = si - 1;
    }
    filmoffers_draw(g_screen_buf, 0, 0);   /* --film-offers: the tapes 11.. left of slot 1 (film_offers.c) */
    return;
}

/* 26A7:10B9  (mode, kind) case/bag window (geometry table 0x12E8): kind 1/2 carried programs, 3 ad contracts, 4 screenplays, 5 studio renter's screenplays, 6/7 Betty gift; mode 1 restores */
u16 ui_draw_bag(u16 a0, u16 a1, u16 ax) {
    int cc;
    u16 di;
    u16 si;
    u32 rv_;
    u16 t3;
    u16 t9;
    u16 t15;
    u16 t21;
    u16 t51;
    u16 t55;
    u16 t64;
    u16 t65;
    u16 t73;
    u16 t74;
    u16 t82;
    u16 t85;
    u16 t86;
    u16 t87;
    u16 t90;
    u16 t91;
    u16 t92;
    u16 l_12 = 0;
    u16 l_10 = 0;
    u16 l_E = 0;
    u16 l_C = 0;
    u16 l_A = 0;
    u16 l_8 = 0;
    u16 l_6 = 0;
    u16 l_4 = 0;
    u16 l_2 = 0;
    FUNC_TICK(228);
    if (((i16)a1) > 0) {
        t3 = (u16)(i8)(G8(((a1 - 1) << 2) + 0x12E8));
        l_12 = (u16)(i8)(G8(((a1 - 1) << 2) + 0x12E8));
        l_8 = t3 << 3;
        t9 = (u16)(i8)(G8(((a1 - 1) << 2) + 0x12E9));
        l_10 = (u16)(i8)(G8(((a1 - 1) << 2) + 0x12E9));
        l_6 = t9 << 3;
        t15 = (u16)(i8)(G8(((a1 - 1) << 2) + 0x12EA));
        l_E = (u16)(i8)(G8(((a1 - 1) << 2) + 0x12EA));
        l_4 = t15 << 3;
        t21 = (u16)(i8)(G8(((a1 - 1) << 2) + 0x12EB));
        l_C = (u16)(i8)(G8(((a1 - 1) << 2) + 0x12EB));
        l_2 = t21 << 3;
        ax = t21 << 3;
    }
    clip_reset();
    if (a0 == 0 || a0 == 2) {
        if (a0 == 0) {
            G16(0x8FD4) = popup_open(l_12, l_10, l_E, l_C, 0, 0, 3);
        }
        g_tile_atlas = g_room_gfx2;
        g_tile_target = g_screen_buf;
        draw_atlas_tile(2, l_8, l_6);
        draw_atlas_tile(0xA, l_4, l_6);
        draw_atlas_tile(0x12, l_8, l_2);
        draw_atlas_tile(0x1A, l_4, l_2);
        si = l_12 + 1;
        while (((i16)si) < ((i16)l_E)) {
            LOOP_TICK(23);
            draw_atlas_tile(0x22, si << 3, l_6);
            draw_atlas_tile(0x3A, si << 3, l_2);
            si = si + 1;
        }
        di = l_10 + 1;
        while (((i16)di) < ((i16)l_C)) {
            LOOP_TICK(23);
            draw_atlas_tile(0x32, l_8, di << 3);
            draw_atlas_tile(0x2A, l_4, di << 3);
            di = di + 1;
        }
        si = l_12 + 1;
        while (((i16)si) < ((i16)l_E)) {
            LOOP_TICK(23);
            di = l_10 + 1;
            while (((i16)di) < ((i16)l_C)) {
                LOOP_TICK(17);
                draw_atlas_tile(0x42, si << 3, di << 3);
                di = di + 1;
            }
            si = si + 1;
        }
        t51 = l_10 << 3;
        t55 = (u16)((i32)(i16)(l_E - l_12) / 2);
        draw_atlas_tile(0x4A, (t55 + l_12) << 3, t51);
        ax = a1 - 1;
        if (((u16)(a1 - 1)) <= 6) {
            switch (ax) {
            case 0:
                si = 0;
                while (((i16)si) < ((i16)g_bag_count)) {
                    LOOP_TICK(20);
                    ax = draw_sprite(MK32(g_room_gfx1_hi, g_room_gfx1_lo + 2), ((si * 9) + l_8) + 8, l_6 + 8);
                    si = si + 1;
                }
                break;
            case 1:
                si = 0;
                while (((i16)si) < ((i16)g_bag_count)) {
                    LOOP_TICK(35);
                    t64 = (((u16)((u16)((i32)(i16)si / 5)) * 0x1E) + l_6) + 8;
                    t65 = ((si * 9) + l_8) + 8;
                    ax = draw_sprite(MK32(g_room_gfx1_hi, g_room_gfx1_lo + 2), t65 - ((u16)((u16)((i32)(i16)si / 5)) * 0x2D), t64);
                    si = si + 1;
                }
                break;
            case 2:
                si = 0;
                for (;;) {
                    LOOP_TICK(37);
                    cc = ((i16)g_player_ad_count(g_player)) > ((i16)si);
                    ax = g_player_ad_count(g_player);
                    if (!(cc)) break;
                    t73 = (((u16)((u16)((i32)(i16)si / 5)) << 5) + l_6) + 8;
                    t74 = ((si * 9) + l_8) + 0xC;
                    draw_sprite(MK32(g_room_gfx1_hi, g_room_gfx1_lo + 0xA), t74 - ((u16)((u16)((i32)(i16)si / 5)) * 0x2D), t73);
                    si = si + 1;
                }
                break;
            case 3:
                si = 0;
                for (;;) {
                    LOOP_TICK(22);
                    cc = ((i16)g_scripts_owned_count(g_player)) > ((i16)si);
                    ax = g_scripts_owned_count(g_player);
                    if (!(cc)) break;
                    draw_sprite(MK32(g_room_gfx1_hi, g_room_gfx1_lo + 0x2A), ((si * 9) + l_8) + 0xA, l_6 + 9);
                    si = si + 1;
                }
                break;
            case 4:
                si = 0;
                for (;;) {
                    LOOP_TICK(30);
                    cc = ((i16)(g_scripts_owned_count((u16)(i8)(studios(g_cur_studio).renter)))) > ((i16)si);
                    ax = g_scripts_owned_count((u16)(i8)(studios(g_cur_studio).renter));
                    if (!(cc)) break;
                    draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + 0xA), ((si * 9) + l_8) + 0xA, l_6 + 9);
                    si = si + 1;
                }
                break;
            case 5:
                t82 = (u16)(i8)g_player_present(g_player);
                l_A = (u16)(i8)g_player_present(g_player);
                ax = t82;
                if (t82 == 0) break;
                if (((i16)l_A) > 100) {
                    l_A = l_A - 0x64;
                }
                t85 = l_6 + 8;
                t86 = l_8 + 8;
                t87 = col_to_px(l_A);
                ax = bitmap_put_screen(MK32(g_room_gfx3_hi, g_room_gfx3_lo + t87), t86, t85);
                break;
            case 6:
                if (g_player_present(g_player) == 0) break;
                t90 = l_6 + 8;
                t91 = l_8 + 8;
                t92 = col_to_px((u16)(i8)g_player_present(g_player));
                ax = bitmap_put_screen(MK32(g_room_gfx_hi, g_room_gfx_lo + t92), t91, t90);
                break;
            default:
                cpu_halt("bad switch index %u in 26A7:10B9", ax);
            }
        }
    } else if (a0 == 1) {
        ax = popup_close(G16(0x8FD4), ax);
    }
    rv_ = ax;
    return (u16)rv_;
}

/* 26A7:14FB  ad agency: draw the 10 contract folders still offered on 3 shelves */
void ui_draw_ad_offers(void) {
    u16 di;
    u16 si;
    u16 l_2;
    u16 l_4;
    u32 t1;
    u16 bx, bw;
    FUNC_TICK(47);
    clip_reset();
    adoffers_bg(&bx, &bw);               /* --ad-offers: the rows reach further left (fixes.c) */
    if ((g_actor_txt_lo | g_actor_txt_hi) != 0) {
        cursor_hide();
        bitmap_put_screen(g_actor_txt, bx, 0x2B);
        cursor_show(0 /* unused ax */);
    } else {
        t1 = bitmap_alloc(bw, 0x46);
        g_actor_txt = t1;
        cursor_hide();
        grab_screen(g_actor_txt, bx, 0x2B);
        cursor_show(0 /* unused ax */);
    }
    si = g_nadoffers;                    /* 10 in the original (--ad-offers) */
    while (((i16)si) >= 1) {
        LOOP_TICK(57);
        if (((i8)(ad_offers(si).shelf)) >= 0) {
            if ((ad_offers(si).ad) != 0) {
                if (g_nadoffers > 10) {      /* --ad-offers: a front and a back row (fixes.c) */
                    adoffer_pos(si, &di, &l_4);
                    l_2 = 0;
                } else if (((i16)si) < 8) {
                    if (((i16)si) < 4) {
                        l_4 = 0x4C;
                        l_2 = si - 1;
                        di = 0x6B;
                    } else {
                        l_4 = 0x3D;
                        l_2 = si + 0xFFFC;
                        di = 0x7F;
                    }
                } else {
                    l_4 = 0x2B;
                    l_2 = si + 0xFFF8;
                    di = 0x99;
                }
                draw_sprite(MK32(g_room_gfx1_hi, g_room_gfx1_lo + 2), (l_2 << 3) + di, l_4);
            }
        }
        si = si - 1;
    }
    return;
}

/* 26A7:1613  (player) library: title of the selected genre and one tape per program */
void ui_draw_library_shelf(u16 a0) {
    u16 ax;
    u16 di;
    u16 dx;
    u16 s0;
    u16 si;
    u32 t1;
    u32 t4;
    FUNC_TICK(73);
    clip_reset();
    di = a0;
    if ((g_actor_txt_lo | g_actor_txt_hi) != 0) {
        cursor_hide();
        bitmap_put_screen(g_actor_txt, 0xA0, 0x30);
        cursor_show(0 /* unused ax */);
    } else {
        t1 = bitmap_alloc(0x50, 0x38);
        g_actor_txt = t1;
        cursor_hide();
        grab_screen(g_actor_txt, 0xA0, 0x30);
        cursor_show(0 /* unused ax */);
    }
    s0 = 1;
    if (((i16)g_selected_genre) > 10) {
        ax = G16(0x8424);
        dx = G16(0x8426);
    } else {
        ax = G16(((g_selected_genre + 0x1A) << 2) + 0x8390);
        dx = G16(((g_selected_genre + 0x1A) << 2) + 0x8392);
    }
    t4 = str_format(MK32(dx, ax));
    ui_text_box(0x14, 0xA, 0x1D, 0xB, LO16(t4), HI16(t4), s0);
    si = 0;
    while (((i16)((u16)(i8)library_count(di, g_selected_genre))) > ((i16)si)) {
        LOOP_TICK(45);
        if (((i16)library_items(di, g_selected_genre, si)) > 0) {
            draw_sprite(MK32(g_room_gfx1_hi, g_room_gfx1_lo + 2), (si * 9) + 0xA0, 0x31);
        }
        si = si + 1;
    }
    return;
}

/* 26A7:1742  (mode) info window of series g_selected_series: episode list, title, description, blocks/genre/critics/press bars */
u16 ui_series_info(u16 a0) {
    u16 ax;
    int cc;
    u16 si;
    u32 rv_;
    u16 t1;
    u16 t2;
    u32 t8;
    u32 t9;
    u16 t13;
    u16 t14;
    u16 t15;
    u16 t16;
    u16 t17;
    u16 t18;
    u16 t19;
    u16 t20;
    u16 t21;
    u16 t22;
    u16 t23;
    u16 t24;
    u16 t25;
    u16 t26;
    u16 t27;
    u16 t28;
    u16 t29;
    u16 t30;
    u16 t31;
    u16 t32;
    u16 t33;
    u16 t34;
    u32 t35;
    u32 t36;
    u16 t37;
    u16 t38;
    u32 t39;
    u16 t46;
    u16 t47;
    u32 t48;
    u8 fr_[50] = {0};                  /* bytes bp-50 .. bp-1 of the original frame */
    FUNC_TICK(227);
    struct_copy((fr_ + 50 - 0x30), HPTR(DSP(0x1304)), 0x2C);
    t1 = g_selected_series * 0x39;
    t2 = FPOFF(g_series) + (g_selected_series * 0x39);
    FR16(fr_, 50, 0x2) = FPSEG(g_series);
    FR16(fr_, 50, 0x4) = t2;
    clip_reset();
    ax = t1;
    if (a0 == 0) {
        si = 1;
        while (((i16)((u16)(i8)(FP8(FR32(fr_, 50, 0x4), 0x22)))) >= ((i16)si)) {
            LOOP_TICK(25);
            t8 = series_episode_title(g_selected_series, si);
            t9 = str_format(DSP(0x13BB) /* "#_2d. #*15s" */, si, W2(t8));
            FRX32(fr_, 50, 0x30, (si << 2) + 0) = t9;
            si = si + 1;
        }
        G16(0x8FD6) = popup_open(1, 1, 0x26, 0xD, 0, 0, 3);
        ui_frame(1, 1, 0x26, 0xD, g_screen_buf_lo, g_screen_buf_hi, 1);
        ui_band(1, 0xB, 0x25, 9, 3);
        ui_band(1, 0x14, 0x12, 0x59, 1);
        t13 = g_screen_buf_hi;
        t14 = g_screen_buf_lo;
        t15 = FR16(fr_, 50, 0x6);
        t16 = FR16(fr_, 50, 0x8);
        t17 = FR16(fr_, 50, 0xA);
        t18 = FR16(fr_, 50, 0xC);
        t19 = FR16(fr_, 50, 0xE);
        t20 = FR16(fr_, 50, 0x10);
        t21 = FR16(fr_, 50, 0x12);
        t22 = FR16(fr_, 50, 0x14);
        t23 = FR16(fr_, 50, 0x16);
        t24 = FR16(fr_, 50, 0x18);
        t25 = FR16(fr_, 50, 0x1A);
        t26 = FR16(fr_, 50, 0x1C);
        t27 = FR16(fr_, 50, 0x1E);
        t28 = FR16(fr_, 50, 0x20);
        t29 = FR16(fr_, 50, 0x22);
        t30 = FR16(fr_, 50, 0x24);
        t31 = FR16(fr_, 50, 0x26);
        t32 = FR16(fr_, 50, 0x28);
        t33 = FR16(fr_, 50, 0x2A);
        t34 = FR16(fr_, 50, 0x2C);
        t35 = series_title(g_selected_series);
        t36 = str_format(DSP(0x13C7) /* "#*16s:~#s~#s~#s~#s~#s~#s~#s~#s~#s~#s" */, W2(t35), t34, t33, t32, t31, t30, t29, t28, t27, t26, t25, t24, t23, t22, t21, t20, t19, t18, t17, t16, t15);
        text_draw_to_free(0xC, 4, LO16(t36), HI16(t36), t14, t13);
        ui_band(0x13, 0x14, 0x13, 0x38, 2);
        ui_band(0x13, 0x4C, 0x13, 0x10, 1);
        ui_band(0x13, 0x5C, 0x13, 0x11, 2);
        t37 = g_screen_buf_hi;
        t38 = g_screen_buf_lo;
        t39 = series_description(g_selected_series);
        text_draw_full(0x9C, 0xC, t39, 0x12, 1, t38, t37, 7);
        t46 = g_screen_buf_hi;
        t47 = g_screen_buf_lo;
        t48 = str_format(g_text(361), (u16)(i8)(FP8(FR32(fr_, 50, 0x4), 0x23)), W2(G32(((((u16)(i8)(FP8(FR32(fr_, 50, 0x4), 0x21))) + 0x124) << 2) + 0x8390)), ((u16)(i8)(FP8(FR32(fr_, 50, 0x4), 0x35))) << 2, ((u16)(i8)(FP8(FR32(fr_, 50, 0x4), 0x36))) << 2);
        text_draw_to_free(0x9C, 0x44, LO16(t48), HI16(t48), t47, t46);
        ax = present_frame();
        si = 1;
        while (((i16)si) <= 10) {
            LOOP_TICK(20);
            cc = ((FRX16(fr_, 50, 0x30, (si << 2) + 0)) | (FRX16(fr_, 50, 0x30, ((si << 2) + 0) + 2))) == 0;
            ax = (FRX16(fr_, 50, 0x30, (si << 2) + 0)) | (FRX16(fr_, 50, 0x30, ((si << 2) + 0) + 2));
            if (!(cc)) {
                ax = mem_free(FRX16(fr_, 50, 0x30, (si << 2) + 0), FRX16(fr_, 50, 0x30, ((si << 2) + 0) + 2));
            }
            si = si + 1;
        }
    } else {
        ax = popup_close(G16(0x8FD6), ax);
    }
    rv_ = ax;
    return (u16)rv_;
}

/* 26A7:19E9  (x0, y0, x1, y1, style) ui_tile_box_to into the screen buffer */
void ui_tile_box(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4) {
    FUNC_TICK(13);
    ui_tile_box_to(a0, a1, a2, a3, a4, g_screen_buf);
    return;
}

/* 26A7:1A0C  (x0, y0, x1, y1, style, far dst) 9-slice frame from atlas tiles; base tile from the style table at 0x1330 */
void ui_tile_box_to(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u32 a5) {
    u16 di;
    u16 si;
    u16 l_2;
    u16 t1;
    u16 t3;
    u16 t4;
    u16 t5;
    u16 t8;
    u16 t9;
    u16 t10;
    u16 t11;
    u16 t12;
    u16 t13;
    FUNC_TICK(80);
    t1 = (u16)(i8)G8(a4 + 0x1330);
    g_clip_on = 0;
    g_tile_atlas = g_room_gfx;
    g_tile_target = a5;
    l_2 = a0 + 8;
    di = t1;
    while (((i16)(a2 + 0xFFF8)) > ((i16)l_2)) {
        LOOP_TICK(50);
        t3 = col_to_px(di + 1);
        draw_atlas_tile(t3, l_2, a1);
        t4 = col_to_px(di + 7);
        draw_atlas_tile(t4, l_2, a3 + 0xFFFE);
        si = a1 + 2;
        while (((i16)(a3 + 0xFFFE)) > ((i16)si)) {
            LOOP_TICK(17);
            t5 = col_to_px(di + 4);
            draw_atlas_tile(t5, l_2, si);
            si = si + 2;
        }
        l_2 = l_2 + 8;
    }
    t8 = col_to_px(di);
    draw_atlas_tile(t8, a0, a1);
    t9 = col_to_px(di + 2);
    draw_atlas_tile(t9, a2 + 0xFFF8, a1);
    t10 = col_to_px(di + 6);
    draw_atlas_tile(t10, a0, a3 + 0xFFFE);
    t11 = col_to_px(di + 8);
    draw_atlas_tile(t11, a2 + 0xFFF8, a3 + 0xFFFE);
    si = a1 + 2;
    while (((i16)(a3 + 0xFFFE)) > ((i16)si)) {
        LOOP_TICK(30);
        t12 = col_to_px(di + 3);
        draw_atlas_tile(t12, a0, si);
        t13 = col_to_px(di + 5);
        draw_atlas_tile(t13, a2 + 0xFFF8, si);
        si = si + 2;
    }
    return;
}

/* 26A7:1B87  (w, h, hot x, hot y, far label, style) create the floating image of a picked-up item (frame, tape icon or label), allocate save-under bitmaps, cursor = carrying */
void ui_drag_begin(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5, u16 a6) {
    u16 ax;
    u16 di;
    u16 s0;
    u16 s1;
    u16 s2;
    u16 s3;
    u16 s4;
    u16 s5;
    u16 s6;
    u16 s7;
    u16 si;
    u16 l_2;
    u32 t1;
    u16 t2;
    u16 t3;
    u16 t4;
    u16 t20;
    u32 t27;
    u32 t28;
    u32 t29;
    FUNC_TICK(239);
    si = a6;
    di = a0;
    if ((g_drag_image_lo | g_drag_image_hi) == 0) {
        cursor_hide();
        t1 = bitmap_alloc2(di, a1);
        g_drag_image = t1;
        if (si == 0) {
L_1BC2:
            ui_tile_box_to(0, 0, di, a1, 0, g_drag_image);
        } else {
            if (si == 0xA) goto L_1BC2;
        }
        if (((i16)si) < 21) {
L_1C0A:
            if (((i16)si) < 1) {
L_1C61:
                if (si == 0xB) {
                    rtl_bitmap_fill_w(W2(g_drag_image), 0, 0, ((i16)(((i16)(((i16)di) >> 1)) >> 1)) >> 1, a1, 0);
                }
            } else if (((i16)si) > 9) {
                goto L_1C61;
            } else {
                rtl_bitmap_fill_w(W2(g_drag_image), 0, 0, 9, 8, 0);
                t2 = g_drag_image_hi;
                t3 = g_drag_image_lo;
                t4 = col_to_px(si);
                draw_sprite_to(MK32(g_room_gfx_hi, g_room_gfx_lo + t4), t3, t2, 0, 0);
            }
        } else if (((i16)si) > 24) {
            goto L_1C0A;
        } else {
            ui_tile_box_to(0, 0, di, a1, si + 0xFFEF, g_drag_image);
        }
        if (((i16)si) < 0) {
L_1CE9:
            if (si == 0xA) {
L_1CF8:
                s0 = (u16)((u16)((i32)(i16)(a1 + 0xFFFE) / 8));
                s1 = (u16)(g_drag_image_hi);
                s2 = (u16)(g_drag_image_lo);
                s3 = 1;
                s4 = (u16)((u16)((i32)(i16)di / 8)) - 1;
                s5 = a5;
                s6 = a4;
                s7 = 0xFFF9;
                if (si != 0xA) {
                    ax = 2;
                } else {
                    ax = 1;
                }
                text_draw_full_free(ax, s7, s6, s5, s4, s3, s2, s1, s0);
            } else if (((i16)si) >= 21) {
                if (((i16)si) <= 24) goto L_1CF8;
            }
        } else if (((i16)si) > 9) {
            goto L_1CE9;
        } else {
            g_font = 1;
            if (si != 0) {
                ax = 2;
            } else {
                ax = 1;
            }
            l_2 = ax;
            text_draw_full_free(l_2, 0xFFFA, a4, a5, (u16)((u16)((i32)(i16)di / 8)) - 1, 1, (u16)(g_drag_image_lo), (u16)(g_drag_image_hi), (u16)((u16)((i32)(i16)(a1 + 0xFFFE) / 7)));
            g_font = 0;
        }
        g_drag_w = di;
        g_drag_h = a1;
        if (a2 != 0xFFFF) {
            t20 = util_min(g_mouse_x - a2, di + 0xFFF0);
            g_drag_hot_x = t20;
            g_drag_hot_y = g_mouse_y - a3;
        }
        G32(0x7B18) = g_cursor_comp;
        G32(0x7B1C) = g_cursor_bg;
        G32(0x7B20) = g_cursor_tmp;
        t27 = bitmap_alloc(di + 8, a1);
        g_cursor_comp = t27;
        t28 = bitmap_alloc(di + 8, a1);
        g_cursor_tmp = t28;
        t29 = bitmap_alloc(di + 8, a1);
        g_cursor_bg = t29;
        g_cursor_shape = 2;
        g_cursor_drawn_x = 0xFFFF;
        g_mouse_max_y = (0x78 - g_drag_h) + g_drag_hot_y;
    }
    return;
}

/* 26A7:1DF8  drop the carried image: restore background, free bitmaps, cursor shape 1 */
u16 ui_drag_end(void) {
    u16 ax;
    u16 t4;
    FUNC_TICK(44);
    ax = g_drag_image_lo | g_drag_image_hi;
    if ((g_drag_image_lo | g_drag_image_hi) != 0) {
        rtl_vga_put_bitmap_w(W2(g_cursor_bg), ((i16)(((i16)(((i16)g_cursor_drawn_x) >> 1)) >> 1)) >> 1, g_cursor_drawn_y);
        mem_free_ptr(HPTR(DSP(0x390)));
        mem_free(g_cursor_comp_lo, g_cursor_comp_hi);
        mem_free(g_cursor_bg_lo, g_cursor_bg_hi);
        t4 = mem_free(g_cursor_tmp_lo, g_cursor_tmp_hi);
        g_cursor_comp = G32(0x7B18);
        g_cursor_bg = G32(0x7B1C);
        g_cursor_tmp = G32(0x7B20);
        g_cursor_shape = 1;
        mouse_bounds_full();
        ax = cursor_show(t4);
    }
    return ax;
}

/* 26A7:1E94  (x, y, mode, player) "Adverts in stock:" window listing the player's 10 contract slots */
void ui_contract_popup(u16 a0, u16 a1, u16 a2, u16 a3) {
    u16 di;
    u16 si;
    u16 l_2;
    u16 l_4;
    u32 t17;
    FUNC_TICK(79);
    l_2 = a0 << 3;
    clip_reset();
    si = a1 << 3;
    if (a2 == 0) {
        G16(0x8FD8) = popup_open(a0, a1, a0 + 0x11, a1 + 0xD, 0, 0, 3);
        ui_tile_box(l_2, si, (a0 + 0x11) << 3, (a1 + 0xE) << 3, 0);
        text_draw(l_2 + 4, si + 0xFFF9, G16(0x850C), G16(0x850E));
        l_4 = 0;
        while (((i16)l_4) < 10) {
            LOOP_TICK(103);
            di = l_4 * 0xA;
            if ((contracts(a3, l_4).ad) != 0) {
                ui_tile_box(l_2 + 4, (si + di) + 0xA, ((a0 + 0x11) << 3) + 0xFFFC, (si + di) + 0x14, 0);
                t17 = str_format(DSP(0x13EC) /* "#*15s" */, W2(ads(contracts(a3, l_4).ad).name));
                text_draw_free(l_2 + 5, (si + di) + 3, LO16(t17), HI16(t17));
            } else {
                ui_tile_box(l_2 + 4, (si + di) + 0xA, ((a0 + 0x11) << 3) + 0xFFFC, (si + di) + 0x14, 1);
            }
            l_4 = l_4 + 1;
        }
    } else if (a2 == 1) {
        popup_close(G16(0x8FD8), 0 /* unused ax */);
    }
    return;
}

/* 26A7:2047  information board: 13 floors x 4 door signs (permuted by g_signs) coloured by owner */
u16 ui_draw_info_board(void) {
    u16 di;
    u16 si;
    u32 rv_;
    u16 t17;
    u16 t18;
    u16 t19;
    u32 t20;
    u16 t24;
    u16 l_8 = 0;
    u16 l_6 = 0;
    u16 l_4 = 0;
    u16 l_2 = 0;
    FUNC_TICK(70);
    rtl_bitmap_fill_w(W2(g_screen_buf), 0, 0, 0x28, 0x78, 0);
    room_draw_border();
    clip_reset();
    g_font = 1;
    l_8 = 1;
    while (((i16)l_8) <= 13) {
        LOOP_TICK(172);
        draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + 0x2A), 0x9A, (l_8 << 3) + 0xFFFD);
        l_6 = 0;
        while (((i16)l_6) <= 3) {
            LOOP_TICK(152);
            l_2 = 0;
            if (((i16)l_6) >= 2) {
                l_2 = 9;
            }
            si = (u16)(i8)(G8(((((l_8 - 1) << 3) + 0x8A60) + (l_6 << 1)) + 1));
            di = (u16)(i8)(G8((((l_8 - 1) << 3) + 0x8A60) + (l_6 << 1)));
            if (((i16)((u16)(i8)(G8(((((l_8 - 1) << 3) + 0x8A60) + (l_6 << 1)) + 1)))) < 100) {
                if (floors_door_room(di, si) != 0) {
                    if (floors_door_owner(di, si) != 0) {
                        if (floors_door_owner(di, si) > 0) {
                            l_4 = floors_door_owner(di, si) + 1;
                        }
                    } else {
                        l_4 = 5;
                    }
                } else {
                    l_4 = 1;
                }
            } else {
                l_4 = 7;
            }
            t17 = 0x6E - (l_8 << 3);
            t18 = ((l_6 * 0x4B) + l_2) + 6;
            t19 = col_to_px(l_4);
            draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + t19), t18, t17);
            if (((i16)si) < 100) {
                if (floors_door_name(di, si) != 0) {
                    if (floors_door_room(di, si) != 0) {
                        t20 = door_label_text(di, si);
                        text_draw_free(((l_6 * 0x4B) + l_2) + 8, 0x68 - (l_8 << 3), LO16(t20), HI16(t20));
                    }
                }
            }
            l_6 = l_6 + 1;
        }
        l_8 = l_8 + 1;
    }
    draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + 0x22), 0xF0, 0x66);
    text_draw(0xF2, 0x60, G16(0x8510), G16(0x8512));
    draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + 0x22), 6, 0x66);
    t24 = text_draw(8, 0x60, G16(0x85B0), G16(0x85B2));
    g_font = 0;
    rv_ = t24;
    return (u16)rv_;
}

/* 26A7:22BD  (player) news room: subscription lamps of the 3 news agencies and the inbox indicator */
u16 ui_draw_news_agencies(u16 a0) {
    u16 ax;
    int cc;
    u16 di;
    u16 s0;
    u16 s1;
    u16 si;
    u16 l_2;
    u16 t1;
    u16 t3;
    FUNC_TICK(35);
    clip_reset();
    si = 1;
    while (((i16)si) <= 3) {
        LOOP_TICK(34);
        cc = (G8((((a0 * 0x17) + 0x8BDC) + si) + 0x10)) != 0;
        di = (u16)(i8)G8(si + 0x1337);
        if (cc) {
            ax = 2;
        } else {
            ax = 1;
        }
        l_2 = ax;
        t1 = col_to_px(l_2);
        draw_sprite(MK32(g_room_gfx1_hi, g_room_gfx1_lo + t1), di, 0x5B);
        si = si + 1;
    }
    s0 = 0x5C;
    s1 = 0x50;
    if (((i16)g_player_news_count(a0)) <= 0) {
        ax = 4;
    } else {
        ax = 3;
    }
    t3 = col_to_px(ax);
    return draw_sprite(MK32(g_room_gfx1_hi, g_room_gfx1_lo + t3), s1, s0);
}

/* 26A7:2363  (player, mode) the 3 news broadcast slots: text, time, "Today"/"(Old)", price class */
void ui_draw_news_slots(u16 a0, u16 a1) {
    u16 bx;
    u16 di;
    u16 es;
    u16 si;
    u32 l_4;
    u16 t2;
    u16 t3;
    u32 t4;
    u32 t5;
    u16 t6;
    u16 t7;
    u16 t8;
    u32 t9;
    FUNC_TICK(58);
    clip_reset();
    di = a0;
    if (a1 == 0 || a1 == 2) {
        if (a1 == 0) {
            G16(0x8FDA) = popup_open(0x14, 0, 0x26, 0xE, 0, 0, 3);
        }
        ui_tile_box(0xA0, 4, 0x138, 0x70, 0);
        si = 1;
        while (((i16)si) <= 3) {
            LOOP_TICK(206);
            if ((news_pool(((di * 4) + si) + 0x38).node) != 0) {
                ui_tile_box(0xA4, (si * 0x22) + 0xFFE5, 0x134, (si * 0x22) + 7, (u16)(i8)(G8(((u16)(i8)(news_pool(((di * 4) + si) + 0x38).category)) + 0x133A)));
                t2 = g_screen_buf_hi;
                t3 = g_screen_buf_lo;
                t4 = news_text(news_pool_at(((di * 4) + si) + 0x38), news_pool(((di * 4) + si) + 0x38).node);
                l_4 = t4;
                t5 = str_format(DSP(0x13F2) /* "#*54s" */, W2(t4));
                text_draw_full_free(0xA6, (si * 0x22) + 0xFFDE, LO16(t5), HI16(t5), 0x11, 1, t3, t2, 3);
                mem_free(LO16(l_4), HI16(l_4));
                if ((news_pool(((di * 4) + si) + 0x38).day) != g_day) {
                    bx = FPOFF(g_text(268));
                    es = FPSEG(g_text(268));
                } else {
                    bx = FPOFF(g_text(259));
                    es = FPSEG(g_text(259));
                }
                l_4 = MK32(es, bx);
                t6 = g_screen_buf_hi;
                t7 = g_screen_buf_lo;
                t8 = news_price((u16)(i8)(news_pool(((di * 4) + si) + 0x38).price_class));
                t9 = str_format(DSP(0x13F8) /* "*** #02d:00 #_5s ** $ #_5d ***" */, news_pool(((di * 4) + si) + 0x38).hour, W2(l_4), t8);
                text_draw_full_free(0xA6, (si * 0x22) + 0xFFF6, LO16(t9), HI16(t9), 0x11, 0, t7, t6, 1);
            } else {
                ui_tile_box(0xA4, (si * 0x22) + 0xFFE5, 0x134, (si * 0x22) + 7, 1);
            }
            si = si + 1;
        }
    } else if (a1 == 1) {
        popup_close(G16(0x8FDA), 0 /* unused ax */);
    }
    return;
}

/* 26A7:25CD  (mode) news room ticker: inbox items of the selected category with paging */
void ui_draw_news_inbox(u16 a0) {
    u16 bx;
    u16 di;
    u16 es;
    u16 si;
    u32 l_4;
    u16 t7;
    u16 t8;
    u16 t10;
    u16 t11;
    u16 t12;
    u32 t13;
    u32 t14;
    u16 t15;
    u16 t16;
    u16 t17;
    u32 t18;
    FUNC_TICK(132);
    clip_reset();
    di = 0xFFFF;
    if (a0 == 0 || a0 == 2) {
        if (a0 == 0) {
            G16(0x8FDC) = popup_open(1, 0, 0x13, 0xE, 0, 0, 3);
        }
        ui_tile_box(8, 4, 0xA0, 0x70, 0);
        if ((news_count_category(1)) != 0 || (news_count_category(2)) != 0 || (news_count_category(3)) != 0) {
            while ((news_count_category(g_news_category)) == 0) {
                LOOP_TICK(11);
                g_news_category = g_news_category + 1;
                if (((i16)g_news_category) <= 3) continue;
                g_news_category = 1;
            }
            si = 1;
            while (((i16)si) < 4) {
                LOOP_TICK(29);
                if ((news_count_category(si)) != 0) {
                    t7 = col_to_px(si + 4);
                    draw_sprite(MK32(g_room_gfx1_hi, g_room_gfx1_lo + t7), (si * 0x18) + 0xFFF4, 8);
                }
                si = si + 1;
            }
            draw_sprite(MK32(g_room_gfx1_hi, g_room_gfx1_lo + 0x5A), (g_news_category * 0x18) + 0xFFF4, 8);
            if (g_news_scroll != 0) {
                draw_sprite(MK32(g_room_gfx1_hi, g_room_gfx1_lo + 0x3A), 0x64, 8);
            }
            t8 = news_count_category(g_news_category);
            if (((i16)t8) > ((i16)(g_news_scroll + 2))) {
                draw_sprite(MK32(g_room_gfx1_hi, g_room_gfx1_lo + 0x42), 0x78, 8);
            }
            si = 0;
            while (((i16)si) < ((i16)g_news_scroll)) {
                LOOP_TICK(8);
                si = si + 1;
                di = news_find_category(g_news_category, di);
            }
            si = 1;
            while (((i16)si) <= 2) {
                LOOP_TICK(180);
                t10 = news_find_category(g_news_category, di);
                di = t10;
                if (t10 == 0xFFFF) break;
                ui_tile_box(0xC, (si * 0x22) + 7, 0x9C, (si * 0x22) + 0x29, (u16)(i8)(G8(((u16)(i8)(inbox(g_player, di).category)) + 0x133D)));
                t11 = g_screen_buf_hi;
                t12 = g_screen_buf_lo;
                t13 = news_text(inbox_at(g_player, di), inbox(g_player, di).headline);
                l_4 = t13;
                t14 = str_format(DSP(0x1417) /* "#*54s" */, W2(t13));
                text_draw_full_free(0xE, si * 0x22, LO16(t14), HI16(t14), 0x11, 1, t12, t11, 3);
                mem_free(LO16(l_4), HI16(l_4));
                if ((inbox(g_player, di).day) != g_day) {
                    bx = FPOFF(g_text(268));
                    es = FPSEG(g_text(268));
                } else {
                    bx = FPOFF(g_text(259));
                    es = FPSEG(g_text(259));
                }
                l_4 = MK32(es, bx);
                t15 = g_screen_buf_hi;
                t16 = g_screen_buf_lo;
                t17 = news_price((u16)(i8)(inbox(g_player, di).price_class));
                t18 = str_format(DSP(0x141D) /* "*** #02d:00 #_5s ** $ #_5d ***" */, inbox(g_player, di).hour, W2(l_4), t17);
                text_draw_full_free(0xE, (si * 0x22) + 0x18, LO16(t18), HI16(t18), 0x11, 0, t16, t15, 1);
                si = si + 1;
            }
        }
    } else {
        popup_close(G16(0x8FDC), 0 /* unused ax */);
    }
    return;
}

/* 26A7:292F  (far news item, node) allocated news text: cached template of the node with its ' */
u32 news_text(u32 a0, u16 a2) {
    u16 ax;
    u16 di;
    u16 dx;
    u16 si;
    u32 rv_;
    u32 t1;
    u32 t2;
    u32 t3;
    u32 t37;
    u8 fr_[18] = {0};                  /* bytes bp-18 .. bp-1 of the original frame */
    FUNC_TICK(226);
    struct_copy((fr_ + 18 - 0x10), HPTR(DSP(0x1341)), 5);
    di = 0;
    while (((i16)di) < 20) {
        LOOP_TICK(13);
        if (g_news_txt_cache(di * 3) == a2) break;
        di = di + 1;
    }
    /* ORIGINAL BUG (fix "news-cache"): the cache holds 24 texts (20 inbox
     * items, the 3 broadcast slots, the dragged item) but only the first 20
     * are searched; a node not found there gets entry 20 (broadcast slot 1),
     * so with a full inbox slots 2 and 3 show slot 1's headline.  Fixed:
     * search all 24, same fallback when the node is not cached at all. */
    if (di == 20 && fix_on(FIX_NEWS_CACHE)) {
        for (u16 k = 20; k < 24; k++)
            if (g_news_txt_cache(k * 3) == a2) { di = k; break; }
    }
    t1 = rtl_strlen_w(W2(G32((di * 6) + 0x72BC)));
    t2 = mem_calloc(LO16(t1) + 1, 1);
    FR16(fr_, 18, 0x6) = HI16(t2);
    FR16(fr_, 18, 0x8) = LO16(t2);
    t3 = rtl_strlen_w(W2(G32((di * 6) + 0x72BC)));
    far_memcpy(HPTR(FR32(fr_, 18, 0x8)), HPTR(G32((di * 6) + 0x72BC)), LO16(t3) + 1);
    FR16(fr_, 18, 0xA) = 0;
    si = 0;
    while ((FP8(FR32(fr_, 18, 0x8), si)) != 0) {
        LOOP_TICK(17);
        if ((FP8(FR32(fr_, 18, 0x8), si)) == 0x23) {
            FRX8(fr_, 18, 0x10, 0 + FR16(fr_, 18, 0xA)) = (FP8(FR32(fr_, 18, 0x8), si + 1)) + 0xCF;
            FR16(fr_, 18, 0xA) = FR16(fr_, 18, 0xA) + 1;
            FP8(FR32(fr_, 18, 0x8), si + 1) = 0x73;
            si = si + 1;
        }
        si = si + 1;
    }
    t37 = str_format(FR32(fr_, 18, 0x8), W2(G32((((FP16(a0, (((u16)(i8)FR8(fr_, 18, 0x10)) << 2) + 2)) * 0x38) + 0x5593) + ((FP16(a0, (((u16)(i8)FR8(fr_, 18, 0x10)) << 2) + 4)) << 2))), W2(G32((((FP16(a0, (((u16)(i8)FR8(fr_, 18, 0xF)) << 2) + 2)) * 0x38) + 0x5593) + ((FP16(a0, (((u16)(i8)FR8(fr_, 18, 0xF)) << 2) + 4)) << 2))), W2(G32((((FP16(a0, (((u16)(i8)FR8(fr_, 18, 0xE)) << 2) + 2)) * 0x38) + 0x5593) + ((FP16(a0, (((u16)(i8)FR8(fr_, 18, 0xE)) << 2) + 4)) << 2))), W2(G32((((FP16(a0, (((u16)(i8)FR8(fr_, 18, 0xD)) << 2) + 2)) * 0x38) + 0x5593) + ((FP16(a0, (((u16)(i8)FR8(fr_, 18, 0xD)) << 2) + 4)) << 2))), W2(G32((((FP16(a0, (((u16)(i8)FR8(fr_, 18, 0xC)) << 2) + 2)) * 0x38) + 0x5593) + ((FP16(a0, (((u16)(i8)FR8(fr_, 18, 0xC)) << 2) + 4)) << 2))));
    FR16(fr_, 18, 0x2) = HI16(t37);
    FR16(fr_, 18, 0x4) = LO16(t37);
    mem_free(FR16(fr_, 18, 0x8), FR16(fr_, 18, 0x6));
    ax = FR16(fr_, 18, 0x4);
    dx = FR16(fr_, 18, 0x2);
    rv_ = MK32(dx, ax);
    return rv_;
}

/* 26A7:2B9D  (player, flag) recalculate coverage, redraw the map and show "Transmitters: n, Total viewers: x million" (reach*2464) */
u16 ui_transmitter_info(u16 a0, u16 a1, u16 ax) {
    u16 di;
    u16 si;
    u32 l_4;
    u32 l_8;
    u16 l_A;
    u16 l_C;
    u16 l_E;
    u16 l_10;
    u16 l_12;
    u16 t3;
    u32 t4;
    u16 t14;
    u16 t15;
    u32 t16;
    u32 t17;
    FUNC_TICK(253);
    l_12 = 0;
    if (a1 == 0) {
        game_clock_stop(0 /* unused ax */);
        di = 1;
        while (((i16)di) <= 32) {
            LOOP_TICK(31);
            if (((contracts(5, ((g_map_player * 0x42) + (di * 2)) + 9).ad) | (G16((((g_map_player * 0x1CE) + 0x1D09) + (di * 0xE)) + 6))) != 0) {
                if ((tx(g_map_player, di).state) != 2) {
                    l_12 = l_12 + 1;
                }
            }
            di = di + 1;
        }
        coverage_build(g_map_player);
        hr_txmap_begin(g_map_player);   /* --filter hr: the coverage's circles (src/hr.c) */
        cursor_hide();
        ui_draw_transmitter_map(0x21);
        l_C = 0;
        l_A = 0xA8;
        if (((contracts(6, (g_map_player * 0x42) + 1).ad) | (G16((g_map_player * 0x1CE) + 0x1D1D))) != 0) {
            l_C = 0x54;
        }
        if (((contracts(6, (g_map_player * 0x42) + 3).ad) | (G16((g_map_player * 0x1CE) + 0x1D2B))) != 0) {
            l_A = 0x54;
        }
        di = l_C;
        while (((i16)di) < ((i16)l_A)) {
            LOOP_TICK(59);
            l_10 = 0;
            while (((i16)l_10) <= 108) {
                LOOP_TICK(51);
                if ((map_density_at(di, l_10, g_population_map)) != 0) {
                    si = 0xF;
                } else {
                    si = 0;
                }
                l_E = map_density_at(di, l_10, g_coverage_map);
                if (l_E != 1) {
                    if (l_E != 0xA) {
                        if (l_E == 0x64) {
                            si = 0x12;
                        }
                    } else {
                        si = 0x11;
                    }
                } else {
                    si = 0x10;
                }
                if (si != 0) {
                    t3 = col_to_px(si);
                    draw_sprite(MK32(g_room_gfx1_hi, g_room_gfx1_lo + t3), di + 0x30, l_10 + 4);
                }
                l_10 = l_10 + 1;
            }
            hr_txmap_column(di);
            if ((di & 1) == 0) {
                present_frame();
            }
            di = di + 1;
        }
        present_frame();
        ui_frame(0x1B, 1, 0x26, 6, g_screen_buf_lo, g_screen_buf_hi, 1);
        ui_band(0x1B, 0xB, 0xB, 9, 3);
        ui_band(0x1B, 0x14, 0xB, 8, 2);
        ui_band(0x1B, 0x1C, 0xB, 8, 1);
        ui_band(0x1B, 0x24, 0xB, 8, 2);
        ui_band(0x1B, 0x2C, 0xB, 9, 1);
        t4 = map_sum_population(0, 0, 0xA7, 0x6C, g_coverage_map);
        G32((u16)((g_map_player * 0x1CE) + 0x1D09) + 4) = t4;
        G32((u16)((g_map_player * 0x1CE) + 0x1D09)) = ((u32)((i32)((u32)(tx(a0, 0).reach * 0x268)) / (i32)0xA));
        l_8 = ((u32)(tx(g_map_player, 0).reach * 0x9A0));
        t14 = g_screen_buf_hi;
        t15 = g_screen_buf_lo;
        t16 = format_millions(l_8, 3);
        l_4 = t16;
        t17 = str_format(g_text(97), W2(players(g_map_player).station), l_12, W2(t16));
        text_draw_to_free(0xDC, 4, LO16(t17), HI16(t17), t15, t14);
        mem_free(LO16(l_4), HI16(l_4));
        present_frame();
        cursor_show(0 /* unused ax */);
        ax = game_clock_start();
    } else if (a1 == 1) {
        ui_draw_transmitter_map(0);
        ax = present_frame();
    }
    return ax;
}

/* 26A7:2F27  (mode) 2 x 5 auction lots: title, current bid and bidder (mode >= 100: only lot mode-100; 1 restore) */
void ui_draw_auction_board(u16 a0) {
    u16 di;
    u16 si;
    u32 l_4;
    u16 l_6;
    u16 l_8;
    u16 l_A;
    u16 t2;
    u32 t3;
    u32 t4;
    u32 t5;
    u32 t6;
    u32 t7;
    u16 t9;
    u32 t11;
    FUNC_TICK(64);
    clip_reset();
    if (a0 == 0 || a0 == 2 || ((i16)a0) > 100) {
        if (a0 == 0) {
            G16(0x8FDE) = popup_open(2, 1, 0x25, 0xD, 0, 0, 3);
        }
        if (((i16)a0) < 100) {
            ui_tile_box(0x10, 8, 0x130, 0x70, 0);
        }
        di = 0;
        while (((i16)di) <= 1) {
            LOOP_TICK(216);
            si = 1;
            while (((i16)si) <= 5) {
                LOOP_TICK(211);
                if (((i16)a0) < 100) {
L_2FB9:
                    t2 = auction((di * 5) + si).program;
                    l_8 = auction((di * 5) + si).program;
                    if (t2 == 0) {
                        l_A = 1;
                    } else {
                        l_A = 0;
                    }
                    ui_tile_box((di * 0x8E) + 0x14, (si * 0x13) + 0xFFF9, (di * 0x8E) + 0x9C, (si * 0x13) + 0xB, l_A);
                    if (l_A == 0) {
                        draw_sprite(MK32(g_bld_sprites_hi, g_bld_sprites_lo + 0x62), (di * 0x8E) + 0x15, (si * 0x13) + 0xFFFA);
                        if (((i16)l_8) >= 1000) {
                            t5 = series_title(l_8 + 0xFC18);
                            t6 = str_format(DSP(0x1442) /* "#*14s" */, W2(t5));
                            text_draw_free((di * 0x8E) + 0x26, (si * 0x13) + 0xFFF2, LO16(t6), HI16(t6));
                        } else {
                            t3 = movie_title(l_8);
                            t4 = str_format(DSP(0x143C) /* "#*14s" */, W2(t3));
                            text_draw_free((di * 0x8E) + 0x26, (si * 0x13) + 0xFFF2, LO16(t4), HI16(t4));
                        }
                        t7 = auction_increment((di * 5) + si);
                        l_4 = (t7 + (auction((di * 5) + si).bid));
                        t9 = (u16)(i8)(auction((di * 5) + si).bidder);
                        l_6 = (u16)(i8)(auction((di * 5) + si).bidder);
                        if (((i16)t9) < 0) {
                            l_6 = g_player;
                            l_4 = l_4 + auction_increment((di * 5) + si);
                        }
                        t11 = str_format(DSP(0x1448) /* "#_7l #*8s" */, W2(l_4), W2(players(l_6).station));
                        text_draw_free((di * 0x8E) + 0x26, (si * 0x13) + 0xFFFA, LO16(t11), HI16(t11));
                    }
                } else {
                    if (((u16)((di * 5) + si)) == ((u16)(a0 + 0xFF9C))) goto L_2FB9;
                }
                si = si + 1;
            }
            di = di + 1;
        }
    } else if (a0 == 1) {
        popup_close(G16(0x8FDE), 0 /* unused ax */);
    }
    return;
}

/* 26A7:31C3  screenplay agency: draw the up to 5 screenplays on offer */
void ui_draw_script_offers(void) {
    u16 si;
    u32 t1;
    u16 t2;
    u16 t3;
    u16 t4;
    FUNC_TICK(39);
    clip_reset();
    if ((g_actor_txt_lo | g_actor_txt_hi) != 0) {
        cursor_hide();
        bitmap_put_screen(g_actor_txt, 0x58, 0x1F);
        cursor_show(0 /* unused ax */);
    } else {
        t1 = bitmap_alloc(0x40, 0x21);
        g_actor_txt = t1;
        cursor_hide();
        grab_screen(g_actor_txt, 0x58, 0x1F);
        cursor_show(0 /* unused ax */);
    }
    si = 5;
    while (((i16)si) >= 1) {
        LOOP_TICK(59);
        if (((i8)(script_offers(si).status)) >= 0) {
            if ((script_offers(si).status) != 0x63) {
                if ((script_offers(si).script) != 0) {
                    t2 = g_screen_buf_hi;
                    t3 = g_screen_buf_lo;
                    t4 = col_to_px(si);
                    blit_transparent(MK32(g_room_gfx1_hi, g_room_gfx1_lo + t4), MK32(t2, t3), (si * 7) + 0x54, 0x23, 0, 0, 0x18, 0x1E - si, 0 /* unused ax */);
                }
            }
        }
        si = si - 1;
    }
    return;
}

/* 26A7:32CA  estate agent: floor numbers and a door marker per room coloured by renter/owner/studio state */
u16 ui_draw_building_plan(void) {
    u16 ax;
    int cc;
    u16 di;
    u16 si;
    u32 rv_;
    u32 t2;
    u32 t3;
    u16 t11;
    u16 t12;
    u16 t13;
    u8 fr_[12] = {0};                  /* bytes bp-12 .. bp-1 of the original frame */
    FUNC_TICK(45);
    clip_reset();
    rtl_bitmap_fill_w(W2(g_screen_buf), 0, 0, 0x28, 0x78, 0);
    room_draw_border();
    ax = draw_sprite(MK32(g_room_gfx1_hi, g_room_gfx1_lo + 0x42), 0x19, 7);
    di = 0;
    while (((i16)di) <= 6) {
        LOOP_TICK(138);
        FR16(fr_, 12, 0x2) = (di * 0xF) + 7;
        if (di != 0) {
            draw_sprite(MK32(g_room_gfx1_hi, g_room_gfx1_lo + 0x4A), 8, FR16(fr_, 12, 0x2));
            t2 = str_format(DSP(0x1452) /* "#_2d" */, 0xE - di);
            text_draw_free(0xB, FR16(fr_, 12, 0x2) + 0xFFFB, LO16(t2), HI16(t2));
            si = 0x19;
            while (((i16)si) < 152) {
                LOOP_TICK(12);
                draw_sprite(MK32(g_room_gfx1_hi, g_room_gfx1_lo + 0x52), si, FR16(fr_, 12, 0x2));
                si = si + 8;
            }
            draw_sprite(MK32(g_room_gfx1_hi, g_room_gfx1_lo + 0x52), 0x90, FR16(fr_, 12, 0x2));
            draw_sprite(MK32(g_room_gfx1_hi, g_room_gfx1_lo + 0x3A), 0x54, FR16(fr_, 12, 0x2) + 2);
        }
        draw_sprite(MK32(g_room_gfx1_hi, g_room_gfx1_lo + 0x4A), 0xA8, FR16(fr_, 12, 0x2));
        t3 = str_format(DSP(0x1457) /* "#_2d" */, 7 - di);
        text_draw_free(0xAB, FR16(fr_, 12, 0x2) + 0xFFFB, LO16(t3), HI16(t3));
        si = 0xB9;
        while (((i16)si) < 312) {
            LOOP_TICK(12);
            draw_sprite(MK32(g_room_gfx1_hi, g_room_gfx1_lo + 0x52), si, FR16(fr_, 12, 0x2));
            si = si + 8;
        }
        draw_sprite(MK32(g_room_gfx1_hi, g_room_gfx1_lo + 0x52), 0x130, FR16(fr_, 12, 0x2));
        ax = draw_sprite(MK32(g_room_gfx1_hi, g_room_gfx1_lo + 0x3A), 0xF4, FR16(fr_, 12, 0x2) + 2);
        di = di + 1;
    }
    di = 1;
    while (((i16)di) <= 13) {
        LOOP_TICK(127);
        si = 0;
        for (;;) {
            LOOP_TICK(122);
            cc = floors_door_kind(di, si) < 1;
            ax = DSEG;
            if (cc) break;
            cc = floors_door_kind(di, si) > 3;
            ax = DSEG;
            if (cc) break;
            FR16(fr_, 12, 0xA) = 1;
            FR16(fr_, 12, 0x4) = floors_door_studio(di, si);
            if (FR16(fr_, 12, 0x4) != 0) {
L_34EC:
                if (((i8)(studios(FR16(fr_, 12, 0x4)).reserved)) <= 0) {
                    if (((i8)(studios(FR16(fr_, 12, 0x4)).renter)) < 1) {
                        if (((i16)FR16(fr_, 12, 0x4)) > 0) {
                            FR16(fr_, 12, 0xA) = 0xC;
                        }
                    } else {
                        FR16(fr_, 12, 0xA) = ((u16)(i8)(studios(FR16(fr_, 12, 0x4)).renter)) + 1;
                    }
                } else {
                    FR16(fr_, 12, 0xA) = ((u16)(i8)(studios(FR16(fr_, 12, 0x4)).reserved)) + 0xC;
                }
            } else if (floors_door_owner(di, si) < 1) {
                goto L_34EC;
            } else {
                FR16(fr_, 12, 0xA) = floors_door_owner(di, si) + 4;
            }
            door_board_pos(di, si, (fr_ + 12 - 0x8), (fr_ + 12 - 0x6));
            t11 = FR16(fr_, 12, 0x6);
            t12 = FR16(fr_, 12, 0x8);
            t13 = col_to_px(FR16(fr_, 12, 0xA));
            draw_sprite(MK32(g_room_gfx1_hi, g_room_gfx1_lo + t13), t12, t11);
            si = si + 1;
        }
        di = di + 1;
    }
    rv_ = ax;
    return (u16)rv_;
}

/* 26A7:35D8  (far str, mode) dialog with text and two answer buttons */
u16 ui_yesno_box(u16 a0, u16 a1, u16 a2) {
    u16 ax;
    FUNC_TICK(53);
    ax = ui_box(a2, 8, 3, 0x1F, 0xC, 1, 8);
    if (a2 != 1) {
        draw_sprite(MK32(g_room_gfx2_hi, g_room_gfx2_lo + 0xA), 0x75, 0x50);
        draw_sprite(MK32(g_room_gfx2_hi, g_room_gfx2_lo + 0x12), 0xBB, 0x50);
        ax = text_draw_wrap_free(0x44, 0x14, a0, a1, 0x17, 1);
    }
    return ax;
}

/* 26A7:3654  status panel: current viewers of the human (blank when the clock is halted or before the first hour) */
void ui_draw_viewers(void) {
    u16 ax;
    u16 dx;
    u32 l_4;
    u32 t2;
    u32 t3;
    FUNC_TICK(43);
    if (g_clock_halt != 0 || ((i16)(clock_hour())) <= 0) {
        t3 = str_format(DSP(0x145C));
        ax = LO16(t3);
        dx = HI16(t3);
    } else {
        t2 = format_millions(ratings(g_player).viewers, 3);
        ax = LO16(t2);
        dx = HI16(t2);
    }
    l_4 = MK32(dx, ax);
    ui_status_text(LO16(l_4), HI16(l_4), 0x93);
    mem_free(LO16(l_4), HI16(l_4));
    return;
}

/* 26A7:36C8  studio room background objects (script marker if a production is set up) */
void ui_draw_studio(void) {
    FUNC_TICK(52);
    draw_sprite(MK32(g_actor_txt_hi, g_actor_txt_lo + 0xA), 4, 0x4F);
    draw_sprite(MK32(g_actor_txt_hi, g_actor_txt_lo + 2), 4, 0x1E);
    bitmap_put_screen(g_room_gfx3, 0xF8, 0x53);
    if ((studios(g_cur_studio).script) != 0) {
        draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + 2), 0xF8, 0x53);
    }
    return;
}

/* 26A7:3749  icons of the bought cast/props of the studio's current episode, empty places for the rest */
void ui_draw_studio_items(void) {
    u16 ax;
    u16 di;
    u16 si;
    u16 l_2;
    u16 l_4;
    u16 l_6;
    u16 l_8;
    u16 l_A;
    u16 t1;
    u16 t2;
    u16 t3;
    u16 t4;
    u16 t5;
    u16 t6;
    u16 t7;
    FUNC_TICK(79);
    ax = (u16)(i8)(studios(g_cur_studio).size);
    if (((u16)((u16)(i8)(studios(g_cur_studio).size))) == 1) {
        si = 0x90;
    } else if (ax == 2) {
        si = 0x80;
    } else if (ax == 3) {
        si = 0x70;
    }
    l_2 = (u16)(i8)(studios(g_cur_studio).size);
    t1 = studios(g_cur_studio).script;
    l_6 = studios(g_cur_studio).script;
    if (t1 != 0) {
        t2 = (u16)(i8)(studios(g_cur_studio).episode);
        t3 = (u16)(i8)(studios(g_cur_studio).episode_last);
        l_4 = (u16)(i8)(studios(g_cur_studio).episode);
        if (((i16)t3) >= ((i16)t2)) {
            di = 1;
            while (((i16)di) < 10) {
                LOOP_TICK(77);
                t4 = (u16)(i8)(FP8(g_scripts, (((l_6 * 0x1B3) + ((l_4 - 1) * 0x23)) + (di * 3)) + 9));
                l_A = (u16)(i8)(FP8(g_scripts, (((l_6 * 0x1B3) + ((l_4 - 1) * 0x23)) + (di * 3)) + 9));
                if (((i16)t4) >= 17 && ((i16)l_A) <= 41) {
                    t5 = (u16)(i8)(FP8(g_scripts, (((l_6 * 0x1B3) + ((l_4 - 1) * 0x23)) + (di * 3)) + 0xB));
                    l_8 = (u16)(i8)(FP8(g_scripts, (((l_6 * 0x1B3) + ((l_4 - 1) * 0x23)) + (di * 3)) + 0xB));
                    if (t5 != 0) {
                        t6 = col_to_px(l_A + 0xFFF1);
                        bitmap_put_screen(MK32(g_room_gfx1_hi, g_room_gfx1_lo + t6), si, 0x32);
                        t7 = col_to_px(l_8 + 3);
                        draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + t7), si, 0x30);
                        l_2 = l_2 - 1;
                        si = si + 0x20;
                    }
                }
                di = di + 1;
            }
            while (((i16)l_2) > 0) {
                LOOP_TICK(14);
                draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + 0x12), si, 0x30);
                l_2 = l_2 - 1;
                si = si + 0x20;
            }
        }
    }
    return;
}

/* 26A7:38EA  (mode) studio assistant menu (text 141: new shopping list / production info / start) with episode markers */
u16 ui_studio_menu(u16 a0) {
    u16 ax;
    u16 s0;
    u16 si;
    u32 t7;
    FUNC_TICK(78);
    ax = ui_box(a0, 0xB, 3, 0x1F, 9, 1, 0xB);
    if (a0 != 1) {
        ui_frame(0xB, 3, 0x1F, 9, g_screen_buf_lo, g_screen_buf_hi, 1);
        ui_band(0xB, 0x1B, 0x14, 9, 3);
        si = 0;
        while (((i16)si) < 5) {
            LOOP_TICK(27);
            s0 = (u16)((u16)((i32)(i16)si % 2)) + 1;
            if (si == 4) {
                ax = 9;
            } else {
                ax = 8;
            }
            ui_band(0xB, (si << 3) + 0x24, 0x14, ax, s0);
            si = si + 1;
        }
        t7 = text_dup(0x8D);
        text_draw_wrap_free(0x60, 0x14, LO16(t7), HI16(t7), 0x13, 1);
        si = (u16)(i8)(studios(g_cur_studio).episode);
        while (((i16)((u16)(i8)(studios(g_cur_studio).episode_last))) >= ((i16)si)) {
            LOOP_TICK(36);
            draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + 0x3A), (si << 1) + 0xD8, ((si - ((u16)(i8)(studios(g_cur_studio).episode))) << 2) + 0x2C);
            si = si + 1;
        }
        ax = present_frame();
    }
    return ax;
}

/* 26A7:3A31  (mode) "What's your choice? Presents / Equipment" */
u16 ui_supermarket_menu(u16 a0) {
    u16 ax;
    u32 t2;
    u32 t3;
    FUNC_TICK(93);
    ax = ui_box(a0, 7, 2, 0x1A, 0xA, 1, 7);
    if (a0 != 1) {
        ui_frame(7, 2, 0x1A, 0xA, g_screen_buf_lo, g_screen_buf_hi, 1);
        t2 = text_dup(0xEA);
        text_draw_wrap_free(0x3C, 0xC, LO16(t2), HI16(t2), 0x13, 1);
        t3 = text_dup(0xEB);
        text_draw_wrap_free(0x50, 0x20, LO16(t3), HI16(t3), 0x10, 1);
        draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + 2), 0x3C, 0x24);
        draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + 0xA), 0x3C, 0x3C);
        ax = present_frame();
    }
    return ax;
}

/* 26A7:3B04  ?shopping list: actor of the selected role with fee and humor/appearance/skill bars */
void ui_draw_actor_card(void) {
    u16 bx;
    u16 cx;
    u16 di;
    u16 s0;
    u16 s1;
    u16 s2;
    u16 s3;
    u16 s4;
    u16 s5;
    u16 s6;
    u16 s7;
    u16 si;
    u32 l_4;
    u16 l_6;
    u16 l_8;
    u16 l_A;
    u16 l_C;
    u16 l_E;
    u16 l_10;
    u16 l_12;
    u16 l_14;
    u16 t12;
    u16 t20;
    u16 t28;
    u32 t30;
    u16 t31;
    u16 t32;
    u16 t33;
    u16 t34;
    u16 t37;
    u16 t38;
    u32 t39;
    u16 t42;
    u16 t43;
    u32 t44;
    u32 t45;
    u16 t50;
    u16 t51;
    u16 t56;
    u16 t57;
    u16 t58;
    u16 t59;
    FUNC_TICK(204);
    clip_reset();
    l_12 = shoplists(g_player, g_shop_list).script;
    l_10 = shoplists(g_player, g_shop_list).episode;
    ui_frame_px(0x34, 3, 0x132, 0x3E, g_screen_buf, 1);
    if (((i16)g_shop_category) >= 1 && ((i16)g_shop_category) <= 7) {
        if (((i16)g_shop_actor_scroll) > 0) {
            draw_sprite(MK32(g_room_gfx2_hi, g_room_gfx2_lo + 0x72), 0x38, 7);
        }
        if ((G16((((g_shop_category - 1) * 0xC0) + 0x5013) + ((g_shop_actor_scroll + 6) * 0xC))) != 0) {
            draw_sprite(MK32(g_room_gfx2_hi, g_room_gfx2_lo + 0x7A), 0x4C, 7);
        }
        text_draw(0x38, 0x10, G16(((g_shop_category + 0x91) << 2) + 0x8390), G16(((g_shop_category + 0x91) << 2) + 0x8392));
        text_draw(0x38, 0x22, G16(0x893C), G16(0x893E));
        si = 1;
        while (((i16)si) <= 5) {
            LOOP_TICK(117);
            l_6 = (si * 0x24) + 0x54;
            l_4 = MK32(DSEG, (((g_shop_category - 1) * 0xC0) + 0x5013) + ((si + g_shop_actor_scroll) * 0xC));
            l_8 = FP16(l_4, 0xA);
            s0 = 0x14;
            s1 = 0x20;
            s2 = 0;
            s3 = 0;
            s4 = 9;
            s5 = l_6;
            s6 = g_screen_buf_hi;
            s7 = g_screen_buf_lo;
            if (((i16)l_8) > 15) {
                bx = g_room_gfx3_lo + col_to_px(l_8 + 0xFFF1);
                cx = g_room_gfx3_hi;
            } else {
                bx = g_room_gfx_lo + col_to_px(l_8);
                cx = g_room_gfx_hi;
            }
            blit_opaque_rect(MK32(cx, bx), MK32(s6, s7), s5, s4, s3, s2, s1, s0, 0 /* unused ax */);
            draw_sprite(MK32(g_room_gfx2_hi, g_room_gfx2_lo + 0x4A), l_6, 7);
            t12 = (u16)((i32)(i16)(((u16)(i8)FP8(l_4, 9)) << 3) / 5);
            t20 = (u16)((i32)(i16)(((u16)(i8)FP8(l_4, 8)) << 3) / 5);
            t28 = (u16)((i32)(i16)(((u16)(i8)FP8(l_4, 6)) << 3) / 5);
            t30 = str_format(DSP(0x145E) /* "$ #l~#32f~#32f~#32f" */, W2(FP32(l_4, 2)), t28, t20, t12);
            text_draw_free(l_6, 0x1A, LO16(t30), HI16(t30));
            si = si + 1;
        }
    } else {
        si = 1;
        while (((i16)si) <= 3) {
            LOOP_TICK(124);
            l_6 = (si * 0x47) + 0x14;
            t31 = g_screen_buf_hi;
            t32 = g_screen_buf_lo;
            t33 = col_to_px(g_shop_category + 0xFFF1);
            blit_opaque_to(MK32(g_room_gfx1_hi, g_room_gfx1_lo + t33), t32, t31, l_6, 0x19);
            if (si != 1) {
                if (si != 2) {
                    if (si == 3) {
                        di = 2;
                    }
                } else {
                    di = 1;
                }
            } else {
                di = 3;
            }
            t34 = col_to_px(di + 9);
            draw_sprite(MK32(g_room_gfx2_hi, g_room_gfx2_lo + t34), l_6, 0x17);
            t37 = g_screen_buf_hi;
            t38 = g_screen_buf_lo;
            t39 = str_format(DSP(0x1472), W2(G32(((si + 0xEC) << 2) + 0x8390)));
            text_draw_center_free(l_6 + 0x10, 0x28, LO16(t39), HI16(t39), t38, t37);
            t42 = g_screen_buf_hi;
            t43 = g_screen_buf_lo;
            t44 = (u32)(MK32(((i16)((u16)(i8)(FP8(g_scripts, ((l_12 * 0x1B3) + ((l_10 - 1) * 0x23)) + 0xA)))) >> 0xF, (u16)(i8)(FP8(g_scripts, ((l_12 * 0x1B3) + ((l_10 - 1) * 0x23)) + 0xA))) * G32((((g_shop_category + 0xFFF1) << 4) + 0x6235) + (si << 2)));
            t45 = str_format(DSP(0x1475), W2(t44));
            text_draw_center_free(l_6 + 0x10, 0x30, LO16(t45), HI16(t45), t43, t42);
            si = si + 1;
        }
        draw_sprite(MK32(g_room_gfx2_hi, g_room_gfx2_lo + 0x82), 0x88, 9);
        text_draw_center_to(0xB5, 2, G16(((g_shop_category + 0xC0) << 2) + 0x8390), G16(((g_shop_category + 0xC0) << 2) + 0x8392), g_screen_buf_lo, g_screen_buf_hi);
    }
    si = 8;
    while (((i16)si) < 312) {
        LOOP_TICK(37);
        draw_sprite(MK32(g_room_gfx2_hi, g_room_gfx2_lo + 0x62), si, 0x45);
        l_14 = 0x4D;
        while (((i16)l_14) < 109) {
            LOOP_TICK(12);
            draw_sprite(MK32(g_room_gfx2_hi, g_room_gfx2_lo + 0x6A), si, l_14);
            l_14 = l_14 + 8;
        }
        draw_sprite(MK32(g_room_gfx2_hi, g_room_gfx2_lo + 0x6A), si, 0x69);
        si = si + 0x10;
    }
    if (((i16)g_shop_slot_scroll) > 0) {
        draw_sprite(MK32(g_room_gfx2_hi, g_room_gfx2_lo + 0x72), 0xC, 0x4D);
    }
    if (!((FP8(g_scripts, (((l_12 * 0x1B3) + ((l_10 - 1) * 0x23)) + ((g_shop_slot_scroll + 4) * 3)) + 9)) == 0 || ((i16)(g_shop_slot_scroll + 4)) > 10)) {
        draw_sprite(MK32(g_room_gfx2_hi, g_room_gfx2_lo + 0x7A), 0xC, 0x5E);
    }
    si = g_shop_slot_scroll + 1;
    while (((i16)(g_shop_slot_scroll + 3)) >= ((i16)si)) {
        LOOP_TICK(271);
        l_E = (u16)(i8)(FP8(g_scripts, (((l_12 * 0x1B3) + ((l_10 - 1) * 0x23)) + (si * 3)) + 9));
        if (l_E == 0) break;
        t50 = (u16)(i8)(FP8(g_scripts, (((l_12 * 0x1B3) + ((l_10 - 1) * 0x23)) + (si * 3)) + 0xB));
        l_A = ((si - g_shop_slot_scroll) * 0x58) + 0xFFD0;
        draw_sprite(MK32(g_room_gfx2_hi, g_room_gfx2_lo + 0x82), l_A, 0x66);
        di = t50;
        if (((i16)l_E) >= 1 && ((i16)l_E) <= 15) {
            l_C = (u16)(i8)(FP8(g_scripts, ((l_12 * 0x1B3) + l_E) + 0x1A3));
            if (di != 0) {
                l_8 = G16(((((l_C - 1) * 0xC0) + 0x5013) + (di * 0xC)) + 0xA);
                s0 = 0x50;
                s1 = l_A + 0x1C;
                s2 = g_screen_buf_hi;
                s3 = g_screen_buf_lo;
                if (((i16)l_8) > 15) {
                    bx = g_room_gfx3_lo + col_to_px(l_8 + 0xFFF1);
                    cx = g_room_gfx3_hi;
                } else {
                    bx = g_room_gfx_lo + col_to_px(l_8);
                    cx = g_room_gfx_hi;
                }
                blit_opaque_to(MK32(cx, bx), s3, s2, s1, s0);
            } else {
                t51 = col_to_px(l_C);
                draw_sprite(MK32(g_room_gfx2_hi, g_room_gfx2_lo + t51), l_A + 0x1C, 0x50);
            }
            draw_sprite(MK32(g_room_gfx2_hi, g_room_gfx2_lo + 0x4A), l_A + 0x1C, 0x4E);
            text_draw_center_to(l_A + 0x2D, 0x5F, G16(((l_C + 0x91) << 2) + 0x8390), G16(((l_C + 0x91) << 2) + 0x8392), g_screen_buf_lo, g_screen_buf_hi);
        } else if (((i16)l_E) >= 16) {
            if (((i16)di) <= 41) {
                if (di != 0) {
                    t56 = g_screen_buf_hi;
                    t57 = g_screen_buf_lo;
                    t58 = col_to_px(l_E + 0xFFF1);
                    blit_opaque_to(MK32(g_room_gfx1_hi, g_room_gfx1_lo + t58), t57, t56, l_A + 0x1C, 0x50);
                    if (di != 1) {
                        if (di != 2) {
                            if (di == 3) {
                                di = 2;
                            }
                        } else {
                            di = 1;
                        }
                    } else {
                        di = 3;
                    }
                    t59 = col_to_px(di + 9);
                    draw_sprite(MK32(g_room_gfx2_hi, g_room_gfx2_lo + t59), l_A + 0x1C, 0x4E);
                } else {
                    if (l_E != 0x10) {
                        draw_sprite(MK32(g_room_gfx2_hi, g_room_gfx2_lo + 0x42), l_A + 0x1C, 0x50);
                    } else {
                        draw_sprite(MK32(g_room_gfx2_hi, g_room_gfx2_lo + 0x3A), l_A + 0x1C, 0x50);
                    }
                    draw_sprite(MK32(g_room_gfx2_hi, g_room_gfx2_lo + 0x4A), l_A + 0x1C, 0x4E);
                }
                text_draw_center_to(l_A + 0x2D, 0x5F, G16(((l_E + 0xC0) << 2) + 0x8390), G16(((l_E + 0xC0) << 2) + 0x8392), g_screen_buf_lo, g_screen_buf_hi);
            }
        }
        si = si + 1;
    }
    castfit_draw();   /* --cast-fit: the row "Eignung" instead of the bars (cast_fit.c) */
    return;
}

/* 26A7:42B2  (mode) supermarket catalog: 10 Betty gifts with prices */
void ui_gift_catalog(u16 a0) {
    u16 di;
    u16 si;
    u16 t1;
    u16 t5;
    u16 t6;
    u32 t7;
    FUNC_TICK(37);
    ui_box(a0, 0xA, 0, 0x26, 0xB, 1, 0 /* unused ax */);
    if (a0 != 1) {
        text_draw(0x58, 0xFFFE, G16(0x87A0), G16(0x87A2));
        di = 0;
        while (((i16)di) < 2) {
            LOOP_TICK(67);
            si = 1;
            while (((i16)si) <= 5) {
                LOOP_TICK(62);
                t1 = col_to_px((di * 5) + si);
                bitmap_put_screen(MK32(g_room_gfx3_hi, g_room_gfx3_lo + t1), (si * 0x28) + 0x38, (di * 0x24) + 0x10);
                t5 = g_screen_buf_hi;
                t6 = g_screen_buf_lo;
                t7 = str_format(DSP(0x1478) /* "#_6l" */, W2(gifts((di * 5) + si).price));
                text_draw_center_free((si * 0x28) + 0x48, (di * 0x24) + 0x22, LO16(t7), HI16(t7), t6, t5);
                si = si + 1;
            }
            di = di + 1;
        }
        present_frame();
    }
    return;
}

/* 26A7:43AD  Betty's office: player portraits at the height of their Betty love; Sammy standings while a Sammy runs */
void ui_draw_betty_office(void) {
    u16 ax;
    int cc;
    u16 di;
    u16 si;
    u32 t1;
    u32 t2;
    u16 t5;
    u16 t6;
    u16 t7;
    u16 t11;
    u16 t12;
    u16 t13;
    u16 t14;
    u16 l_8 = 0;
    u16 l_6 = 0;
    u16 l_4 = 0;
    u16 l_2 = 0;
    FUNC_TICK(99);
    if ((g_room_gfx3_lo | g_room_gfx3_hi) != 0) {
        bitmap_put_screen(g_room_gfx3, 0xA8, 0xC);
        bitmap_put_screen(g_actor_txt, 0x110, 0xC);
    } else {
        t1 = bitmap_alloc(0x38, 0x4E);
        g_room_gfx3 = t1;
        t2 = bitmap_alloc(0x18, 0x4E);
        g_actor_txt = t2;
        grab_screen(g_room_gfx3, 0xA8, 0xC);
        grab_screen(g_actor_txt, 0x110, 0xC);
    }
    si = 1;
    while (((i16)si) <= 3) {
        LOOP_TICK(90);
        if ((players(si).out) == 0) {
            if ((players(si).betty_love) != 0) {
                l_6 = si;
            } else {
                l_6 = 4;
            }
            cc = si == 1;
            ax = si;
            if (cc) {
                l_8 = 0xAA;
            } else if (ax == 2) {
                l_8 = 0xC2;
            } else if (ax == 3) {
                l_8 = 0x110;
            }
            t5 = 0x3E - (((i16)(players(si).betty_love)) >> 1);
            t6 = l_8;
            t7 = col_to_px(l_6);
            draw_sprite(MK32(g_room_gfx1_hi, g_room_gfx1_lo + t7), t6, t5);
            if ((players(si).out) == 1) {
                draw_sprite(MK32(g_room_gfx1_hi, g_room_gfx1_lo + 0x22), l_8, 0x3E - (((i16)(players(si).betty_love)) >> 1));
            }
        }
        si = si + 1;
    }
    if (((i16)(sammy_daily_update())) <= 1) {
        draw_sprite(MK32(g_room_gfx1_hi, g_room_gfx1_lo + 0x2A), 0x48, 8);
        if ((sammy_daily_update()) == 1) {
            l_2 = 0x18;
            si = 1;
            while (((i16)si) < 4) {
                LOOP_TICK(41);
                if ((players(si).out) == 0) {
                    l_4 = 0x4A;
                    di = 0;
                    for (;;) {
                        LOOP_TICK(25);
                        t14 = sammy_score_pct(si);
                        if (((i16)(((i16)(((i16)t14) >> 1)) >> 1)) <= ((i16)di)) break;
                        l_4 = l_4 + 1;
                        t11 = l_2;
                        t12 = l_4;
                        t13 = col_to_px(si + 7);
                        draw_sprite(MK32(g_room_gfx1_hi, g_room_gfx1_lo + t13), t12, t11);
                        di = di + 1;
                    }
                    l_2 = l_2 + 6;
                }
                si = si + 1;
            }
        }
        draw_sprite(MK32(g_room_gfx1_hi, g_room_gfx1_lo + 0x32), 0x48, 8);
    }
    return;
}

/* 26A7:4611  statistics chart: viewers per hour (0 yesterday, 1 today, 2 last 7 days; programme and news bars, ad result marks) or the 3 players' image % bars */
void ui_draw_stats_chart(void) {
    u16 ax;
    u16 bx;
    u16 di;
    u16 s0;
    u16 s1;
    u16 si;
    u16 t8;
    u16 t9;
    u16 t12;
    u16 t18;
    u16 t21;
    u16 t24;
    u16 t30;
    u16 t34;
    u32 t41;
    u32 t45;
    u16 t46;
    u32 t49;
    u32 t53;
    u16 t54;
    u16 t56;
    u16 t58;
    u16 t59;
    u16 t60;
    u16 t61;
    u32 t62;
    u32 t63;
    u32 t68;
    u16 t71;
    u32 t72;
    u16 t73;
    u16 l_18 = 0;
    u16 l_16 = 0;
    u16 l_14 = 0;
    u16 l_12 = 0;
    u8 fr_[16] = {0};                  /* bytes bp-16 .. bp-1 of the original frame */
    FUNC_TICK(213);
    g_tile_atlas = g_room_gfx3;
    g_tile_target = g_screen_buf;
    G32(0x386) = 0x40001;
    G32(0x38A) = 0x13B0004;
    g_clip_y2 = 0x74;
    si = 0;
    while (((i16)si) < 113) {
        LOOP_TICK(32);
        l_18 = 1;
        while (((i16)l_18) < 39) {
            LOOP_TICK(13);
            draw_atlas_tile(0xA, l_18 << 3, si);
            l_18 = l_18 + 1;
        }
        draw_atlas_tile(2, 4, si);
        draw_atlas_tile(2, 0x138, si);
        si = si + 8;
    }
    draw_atlas_tile(0x2A, 0xC, 0x46);
    if (g_chart_image != 0) {
        bx = 0x151;
    } else {
        bx = 0x150;
    }
    text_draw(0xF, 0x40, g_text_lo(bx), g_text_hi(bx));
    if (g_chart_image == 0) {
        si = 1;
        while (((i16)si) <= 3) {
            LOOP_TICK(47);
            if ((players(si).out) == 0) {
                t8 = col_to_px(si + 2);
                draw_atlas_tile(t8, 0xC, (si * 0xE) + 0xFFFA);
                text_draw(0xF, (si * 0xE) + 0xFFF4, pl_station_lo(si), pl_station_hi(si));
            }
            si = si + 1;
        }
        if (g_chart_player != 0) {
            draw_atlas_tile(0x32, 0xC, (g_chart_player * 0xE) + 0xFFFA);
        }
    }
    if (g_chart_mode != 0) {
        if (g_chart_mode != 1) {
            if (g_chart_mode == 2) {
                t34 = (g_chart_player << 5) + 0x6DA9;
                FR16(fr_, 16, 0xA) = DSEG;
                FR16(fr_, 16, 0xC) = t34;
                l_16 = 7;
                l_14 = 0x120;
            }
        } else {
            t21 = (g_chart_player << 5) + 0x74EA;
            FR16(fr_, 16, 0xA) = DSEG;
            FR16(fr_, 16, 0xC) = t21;
            t24 = (g_chart_player << 5) + 0x76B0;
            FR16(fr_, 16, 0x6) = DSEG;
            FR16(fr_, 16, 0x8) = t24;
            t30 = (g_chart_player << 3) + 0x5553;
            FR16(fr_, 16, 0x2) = DSEG;
            FR16(fr_, 16, 0x4) = t30;
            l_16 = clock_hour();
            l_14 = 0x103;
        }
    } else {
        t9 = (g_chart_player << 5) + 0x78BE;
        FR16(fr_, 16, 0xA) = DSEG;
        FR16(fr_, 16, 0xC) = t9;
        t12 = (g_chart_player << 5) + 0x793E;
        FR16(fr_, 16, 0x6) = DSEG;
        FR16(fr_, 16, 0x8) = t12;
        t18 = (g_chart_player << 3) + 0x5573;
        FR16(fr_, 16, 0x2) = DSEG;
        FR16(fr_, 16, 0x4) = t18;
        l_16 = 7;
        l_14 = 0x102;
    }
    l_12 = l_16;
    if (g_chart_image == 0) {
        draw_atlas_tile(0x2A, 0xC, 0x38);
        text_draw(0xF, 0x32, g_text_lo(l_14), g_text_hi(l_14));
        si = 1;
        while (((i16)si) <= ((i16)l_16)) {
            LOOP_TICK(135);
            if (((i16)g_chart_mode) <= 1) {
                t41 = (u32)((i32)FP32(FR32(fr_, 16, 0x8), si << 2) / (i32)0x3E8);
                FR16(fr_, 16, 0xE) = HI16(t41);
                FR16(fr_, 16, 0x10) = LO16(t41);
                t45 = (u32)((i32)((u32)(FR32(fr_, 16, 0x10) * 0xF)) / (i32)((u32)(MK32(((i16)g_chart_scale) >> 0xF, g_chart_scale) * 0x3E8)));
                draw_atlas_tile(0x132, (si * 0x1E) + 0x41, 0x5B - LO16(t45));
                di = (0x5B - LO16(t45)) + 1;
                while (((i16)di) < 91) {
                    LOOP_TICK(19);
                    t46 = col_to_px(g_chart_player + 0x23);
                    draw_atlas_tile(t46, (si * 0x1E) + 0x41, di);
                    di = di + 1;
                }
                hr_chart_bar((si * 0x1E) + 0x41, 0x5B - LO16(t45), hr_chart_frac(FR32(fr_, 16, 0x10), g_chart_scale, LO16(t45)), MK32(g_tile_atlas_hi, g_tile_atlas_lo + 0x132));
            }
            if (((u32)MK32(si, g_chart_mode)) == ((u32)(MK32(l_16, 1)))) {
                if (((i16)g_minute) >= 5) {
L_4961:
                    t49 = (u32)((i32)FP32(FR32(fr_, 16, 0xC), si << 2) / (i32)0x3E8);
                    FR16(fr_, 16, 0xE) = HI16(t49);
                    FR16(fr_, 16, 0x10) = LO16(t49);
                    t53 = (u32)((i32)((u32)(FR32(fr_, 16, 0x10) * 0xF)) / (i32)((u32)(MK32(((i16)g_chart_scale) >> 0xF, g_chart_scale) * 0x3E8)));
                    draw_atlas_tile(0x152, (si * 0x1E) + 0x47, 0x5B - LO16(t53));
                    di = (0x5B - LO16(t53)) + 1;
                    while (((i16)di) < 91) {
                        LOOP_TICK(19);
                        t54 = col_to_px(g_chart_player + 0x27);
                        draw_atlas_tile(t54, (si * 0x1E) + 0x47, di);
                        di = di + 1;
                    }
                    hr_chart_bar((si * 0x1E) + 0x47, 0x5B - LO16(t53), hr_chart_frac(FR32(fr_, 16, 0x10), g_chart_scale, LO16(t53)), MK32(g_tile_atlas_hi, g_tile_atlas_lo + 0x152));
                }
            } else {
                goto L_4961;
            }
            si = si + 1;
        }
    } else {
        si = 1;
        while (((i16)si) <= 3) {
            LOOP_TICK(102);
            if ((players(si).out) == 0) {
                t56 = (u16)((u16)((players(si).image) * 0x4B) / 0x64);
                draw_atlas_tile(0xFA, (si * 0x3C) + 0x40, 0x59 - t56);
                di = (0x59 - t56) + 1;
                while (((i16)di) < 91) {
                    LOOP_TICK(19);
                    t58 = col_to_px(si + 0x1C);
                    draw_atlas_tile(t58, (si * 0x3C) + 0x40, di);
                    di = di + 1;
                }
                {   /* --filter hr: the image in thirds of a percent, a pixel 4/3 % (src/hr.c) */
                    int f = (int)((players(si).image) * 3) - 4 * (int)t56;
                    hr_chart_bar((si * 0x3C) + 0x40, 0x59 - t56, f, MK32(g_tile_atlas_hi, g_tile_atlas_lo + 0xFA));
                }
                t59 = col_to_px(si + 2);
                draw_atlas_tile(t59, (si * 0x3C) + 0x36, 0x63);
                t60 = g_screen_buf_hi;
                t61 = g_screen_buf_lo;
                t62 = str_format(DSP(0x147D) /* "#d%" */, players(si).image);
                text_draw_center_free((si * 0x3C) + 0x4E, 0x5D, LO16(t62), HI16(t62), t61, t60);
            }
            si = si + 1;
        }
    }
    draw_atlas_tile(0x4A, 0x4C, 0x5B);
    draw_atlas_tile(0x52, 0x5B, 8);
    if (g_chart_image != 0) {
        l_16 = 0x64;
        si = 0x10;
        while (((i16)si) <= 76) {
            LOOP_TICK(26);
            draw_atlas_tile(0x3A, 0x58, si);
            t68 = str_format(DSP(0x1486) /* "#_3d%" */, l_16);
            text_draw_free(0x45, si + 0xFFF5, LO16(t68), HI16(t68));
            l_16 = l_16 - 0x14;
            si = si + 0xF;
        }
    } else {
        l_16 = g_chart_scale * 5;
        si = 0x10;
        while (((i16)si) <= 76) {
            LOOP_TICK(27);
            draw_atlas_tile(0x3A, 0x58, si);
            t63 = str_format(DSP(0x1481) /* "#_3d" */, l_16);
            text_draw_free(0x45, si + 0xFFF5, LO16(t63), HI16(t63));
            l_16 = l_16 - g_chart_scale;
            si = si + 0xF;
        }
    }
    l_16 = 0x12;
    si = 0x5D;
    while (((i16)si) <= 273) {
        LOOP_TICK(48);
        s0 = 0x5B;
        s1 = si;
        if (g_chart_image != 0) {
            ax = 0x23;
        } else {
            ax = 9;
        }
        t71 = col_to_px(ax);
        draw_atlas_tile(t71, s1, s0);
        if (!(g_chart_mode == 2 || g_chart_image != 0)) {
            draw_atlas_tile(0x62, si, 0x5E);
            t72 = str_format(DSP(0x148C) /* "#02d" */, l_16);
            text_draw_free(si + 8, 0x58, LO16(t72), HI16(t72));
        }
        l_16 = l_16 + 1;
        if (l_16 == 0x18) {
            l_16 = 0;
        }
        si = si + 0x1E;
    }
    if (g_chart_image == 0) {
        si = 1;
        while (((i16)si) <= ((i16)l_12)) {
            LOOP_TICK(32);
            if (((i16)g_chart_mode) <= 1) {
                if (((u32)MK32(si, g_chart_mode)) == ((u32)(MK32(l_12, 1)))) {
                    if (((i16)g_minute) >= 5) {
L_4C85:
                        if ((FP8(FR32(fr_, 16, 0x4), si)) != 1) {
                            s0 = 0x66;
                            s1 = (si * 0x1E) + 0x46;
                            if ((FP8(FR32(fr_, 16, 0x4), si)) != 0) {
                                ax = 0x22;
                            } else {
                                ax = 0x21;
                            }
                            t73 = col_to_px(ax);
                            draw_atlas_tile(t73, s1, s0);
                        }
                    }
                } else {
                    goto L_4C85;
                }
            }
            si = si + 1;
        }
    }
    g_clip_on = 0;
    present_frame();
    return;
}
