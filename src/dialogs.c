/* Dialogs: shopping lists, Betty, boss dialog, Madris, save slots, language prompt.
 *
 * Original module 2B74 of MTV.EXE (Rainbow Arts, 1991).  Decompiled by
 * tools/decomp.py and hand-maintained since the freeze (git tag
 * generated-final).  Every function keeps its original address in the
 * comment above it; that address ties it to the machine-level twin in gen/
 * and to the difftest tables -- keep it when editing.
 */
#include "game.h"
#include "fixes.h"

/* 2B74:0006  supermarket: draw the player's shopping-list numbers (complete/incomplete icon) and the title of the selected list (" */
void ui_draw_shopping_lists(void) {
    u16 ax;
    u16 di;
    u16 s0;
    u16 s1;
    u16 si;
    u32 l_4;
    u16 l_6;
    u16 l_8;
    u16 l_A;
    u16 t1;
    u16 t3;
    u16 t4;
    u32 t5;
    u16 t7;
    u32 t8;
    u32 t9;
    u32 t10;
    u32 t12;
    u32 t13;
    u32 t14;
    FUNC_TICK(174);
    l_A = shoplists(g_player, g_shop_list).script;
    t1 = shoplists(g_player, g_shop_list).episode;
    l_8 = screenplays(l_A).target;
    l_6 = script_items_bought(l_A, t1);
    si = 0;
    di = t1;
    while (((i16)g_shoplist_count(g_player)) > ((i16)si)) {
        LOOP_TICK(73);
        t3 = script_items_bought(shoplists(g_player, si).script, shoplists(g_player, si).episode);
        s0 = (si * 0xC) + 6;
        s1 = 0x35;
        if (t3 == 0) {
            ax = 0x13;
        } else {
            ax = 0x14;
        }
        t4 = col_to_px(ax);
        draw_sprite(MK32(g_room_gfx2_hi, g_room_gfx2_lo + t4), s1, s0);
        t5 = str_format(DSP(0x14F5), si + 1);
        text_draw_free(0x37, si * 0xC, LO16(t5), HI16(t5));
        si = si + 1;
    }
    ui_frame_px(0x3C, 3, 0x132, 0x3E, g_screen_buf, 1);
    s0 = (g_shop_list * 0xC) + 6;
    s1 = 0x35;
    if (l_6 == 0) {
        ax = 0x13;
    } else {
        ax = 0x14;
    }
    t7 = col_to_px(ax);
    draw_sprite(MK32(g_room_gfx2_hi, g_room_gfx2_lo + t7), s1, s0);
    t8 = str_format(DSP(0x14F8), g_shop_list + 1);
    text_draw_free(0x37, g_shop_list * 0xC, LO16(t8), HI16(t8));
    if (((u16)(((i16)((screenplays(l_A).flags_16) << 0xF)) >> 0xF)) != 0) {
        t12 = series_episode_description(l_8, di);
        t13 = series_episode_title(l_8, di);
        t14 = series_title(l_8);
        l_4 = str_format(g_text(374), W2(t14), di, W2(t13), W2(t12));
    } else {
        t9 = script_title(l_A);
        t10 = movie_title(l_8);
        l_4 = str_format(g_text(373), W2(t10), W2(t9), di);
    }
    text_draw_wrap_free(0x46, 3, LO16(l_4), HI16(l_4), 0x1D, 1);
    return;
}

/* 2B74:0267  (x, y, mode): mode 0 saves background and shows the 11 genre names with the library count per genre (5 = category full); mode 1 restores */
u16 ui_genre_count_popup(u16 a0, u16 a1, u16 a2, u16 ax) {
    int cc;
    u16 di;
    u16 si;
    u16 l_2;
    u16 t3;
    u32 t7;
    u16 t8;
    u16 t9;
    FUNC_TICK(79);
    si = 1;
    while (((i16)si) <= 11) {
        LOOP_TICK(55);
        g_lib_genre_count(si) = (u16)(i8)library_count(g_room_owner, si);
        ax = g_player;
        if (g_player == g_room_owner) {
            di = 0;
            while (((i16)di) < ((i16)g_bag_count)) {
                LOOP_TICK(30);
                if (((i16)si) > 10) {
L_02CE:
                    if (si != 0xB) {
                        di = di + 1;
                        continue;
                    } else if (((i16)g_bag(di)) <= 1000) {
                        di = di + 1;
                        continue;
                    } else {
L_02DF:
                        if (g_lib_genre_count(si) != 4) {
                            di = di + 1;
                            continue;
                        } else {
                            goto L_02EA;
                        }
                    }
                } else {
                    t3 = g_bag(di);
                    l_2 = g_bag(di);
                    ax = t3;
                    if (((i16)t3) >= 1000) {
                        goto L_02CE;
                    } else {
                        cc = ((u16)((u16)(i8)(films(l_2).genre))) == si;
                        ax = (u16)(i8)(films(l_2).genre);
                        if (cc) {
                            goto L_02DF;
                        } else {
                            goto L_02CE;
                        }
                    }
                }
            }
        }
L_02FD:
        si = si + 1;
    }
    clip_reset();
    if (a2 == 0) {
        t7 = str_format(DSP(0x14FB) /* "#v #s~#v #s~#v #s~#v #s~#v #s~#v #s~#v #s~#v #s~#v #s~#v ..." */, g_lib_genre_count(1), W2(g_text(27)), g_lib_genre_count(2), W2(g_text(28)), g_lib_genre_count(3), W2(g_text(29)), g_lib_genre_count(4), W2(g_text(30)), g_lib_genre_count(5), W2(g_text(31)), g_lib_genre_count(6), W2(g_text(32)), g_lib_genre_count(7), W2(g_text(33)), g_lib_genre_count(8), W2(g_text(34)), g_lib_genre_count(9), W2(g_text(35)), g_lib_genre_count(10), W2(g_text(36)), g_lib_genre_count(11), W2(g_text(37)));
        t8 = popup_open(a0, a1, a0 + 0xB, a1 + 0xB, LO16(t7), HI16(t7), 1);
        g_genre_popup_bg = t8;
        g_genre_popup_open = 1;
        ax = t8;
    } else if (a2 == 1) {
        t9 = popup_close(g_genre_popup_bg, ax);
        g_genre_popup_open = 0;
        ax = t9;
    }
    return ax;
L_02EA:
    g_lib_genre_count(si) = 5;
    goto L_02FD;
}

/* 2B74:03EF  (mode, str): Betty speech bubble box; mode 0 draws text, presents and waits for input; mode 1 closes */
u16 betty_say(u16 a0, u16 a1, u16 a2) {
    u16 ax;
    FUNC_TICK(37);
    ax = ui_box(a0, 0x15, 2, 0x26, 0xD, 1, 0x15);
    if (a0 == 0) {
        text_draw_wrap_free(0xB0, 0x10, a1, a2, 0xF, 1);
        present_frame();
        ax = room_enter(0xFFFA);
    }
    return ax;
}

/* 2B74:0447  (mode, text_id): boss office dialog: greeting by boss rating (<10 "You dare to show your face", <75 "Get to the point", else friendly), or reply/option text from g_boss_reply_table for the current page; -1 = repeat last */
u16 boss_dialog_draw(u16 a0, u16 a1) {
    u16 ax;
    u16 di;
    u16 si;
    u16 l_2;
    u16 l_4;
    u32 t5;
    u16 t6;
    u32 t12;
    FUNC_TICK(203);
    l_4 = players(g_player).image;
    if (a1 == 0xFFFF) {
        a1 = g_boss_last_text;
    }
    if (a0 == 0) {
        g_boss_greeted = 0;
    }
    ui_box(a0, 0x13, 1, 0x26, 8, 1, 0 /* unused ax */);
    l_2 = a0;
    ax = a0;
    if (g_boss_menu == 0xFFFF) {
        l_2 = 1;
    }
    if (g_boss_greeted == 0) {
        ax = ui_box(l_2, 0x13, 9, 0x26, 0xD, 2, 0x13);
        if (l_2 == 1) {
            g_boss_greeted = 1;
        }
    }
    if (a0 != 1) {
        if (((i16)g_boss_menu) > 0) {
            draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + 0x2A), 0x9C, 0x4C);
            ax = draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + 0x32), 0x9C, 0x5C);
        }
        if (g_boss_menu == 0) {
            text_draw_wrap(0xB0, 0x4C, G16(0x863C), G16(0x863E), 0xF, 1);
            if (((i16)l_4) >= 10) {
                if (((i16)l_4) >= 75) {
                    di = 0xAE;
                } else {
                    di = 0xAD;
                }
            } else {
                di = 0xAC;
            }
            t5 = str_format(g_text(di), W2(players(g_player).name));
            t6 = text_draw_wrap_free(0x9C, 0xC, LO16(t5), HI16(t5), 0x12, 1);
            g_boss_last_text = di;
            ax = t6;
        }
        if (((i16)g_boss_menu) > 0) {
            si = 0;
            while (((i16)si) < 15) {
                LOOP_TICK(23);
                if (g_boss_replies(si * 3) != g_boss_menu) {
                    si = si + 1;
                    continue;
                } else if ((g_boss_replies((si * 3) + 1)) != g_boss_choice) {
                    si = si + 1;
                    continue;
                } else {
                    goto L_05FE;
                }
            }
L_061B:
            ax = text_draw_wrap(0xB0, 0x4C, g_text_lo(di), g_text_hi(di), 0xF, 1);
        }
        if (g_boss_menu != 0) {
            t12 = str_format(g_text(a1), W2(players(g_player).name));
            text_draw_wrap_free(0x9C, 0xC, LO16(t12), HI16(t12), 0x12, 1);
            g_boss_last_text = a1;
            ax = a1;
        }
    }
    return ax;
L_05FE:
    di = g_boss_replies((si * 3) + 2);
    goto L_061B;
}

/* 2B74:069E  (mode): hidden Tetris on the office computer: draw 20x10 grid, next piece, score/level/lines/high score; game-over animation resets the grid and picks a random next piece (1..7); mode 1 restores background */
u16 tetris_draw(u16 a0, u16 ax) {
    int cc;
    u16 di;
    u16 si;
    u8 l_1;
    u16 t1;
    u32 t3;
    u32 t4;
    u32 t5;
    u32 t6;
    u16 t9;
    u16 t10;
    u16 t14;
    u16 t15;
    u16 t21;
    FUNC_TICK(210);
    clip_reset();
    if (a0 == 0 || a0 == 2) {
        if (a0 == 0) {
            t1 = popup_open(9, 0, 0x1F, 0xE, 0, 0, 3);
            g_tetris_bg = t1;
        }
        draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + 0x52), 0x67, 4);
        g_clip_y1 = 8;
        g_clip_y2 = 0x6E;
        t3 = str_format(DSP(0x153D) /* "#04d" */, g_madris_lines);
        text_draw_free(0x6C, 0x31, LO16(t3), HI16(t3));
        t4 = str_format(DSP(0x1542) /* "#_4d" */, 9 - g_madris_delay);
        text_draw_free(0x6C, 0x1D, LO16(t4), HI16(t4));
        t5 = str_format(DSP(0x1547) /* "#04d" */, g_madris_score);
        text_draw_free(0x6C, 9, LO16(t5), HI16(t5));
        t6 = str_format(DSP(0x154C) /* "#04d" */, g_madris_hiscore);
        ax = text_draw_free(0x6C, 0x45, LO16(t6), HI16(t6));
        di = 0;
        while (((i16)di) < 20) {
            LOOP_TICK(34);
            si = 0;
            while (((i16)si) < 10) {
                LOOP_TICK(29);
                cc = (G8(si + ((di * 0xA) + 0x8AD4))) == 0;
                ax = DSEG;
                if (!(cc)) {
                    ax = draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + 0x82), (si * 5) + 0x88, (di * 5) + 9);
                }
                si = si + 1;
            }
            di = di + 1;
        }
        di = 0;
        while (((i16)di) < 20) {
            LOOP_TICK(43);
            si = 0;
            while (((i16)si) < 10) {
                LOOP_TICK(38);
                t9 = (((u8)((u8)(DSEG >> 8))) << 8) | (G8(si + ((di * 0xA) + 0x8AD4)));
                l_1 = G8(si + ((di * 0xA) + 0x8AD4));
                ax = t9;
                if (((u8)((u8)t9)) != 0) {
                    t10 = col_to_px(l_1 + 0xB);
                    ax = draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + t10), (si * 5) + 0x89, (di * 5) + 8);
                }
                si = si + 1;
            }
            di = di + 1;
        }
        di = 0;
        while (((i16)di) < 4) {
            LOOP_TICK(47);
            si = 0;
            while (((i16)si) < 4) {
                LOOP_TICK(42);
                t14 = (((u8)((u8)(((u16)(di << 2)) >> 8))) << 8) | (G8(si + ((((g_madris_next - 1) << 6) + 0x39C) + (di << 2))));
                l_1 = G8(si + ((((g_madris_next - 1) << 6) + 0x39C) + (di << 2)));
                ax = t14;
                if (((u8)((u8)t14)) != 0) {
                    t15 = col_to_px(l_1 + 0xB);
                    ax = draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + t15), (di * 5) + 0x6A, (si * 5) + 0x5A);
                }
                si = si + 1;
            }
            di = di + 1;
        }
        if (((i16)g_madris_state) >= 14) {
            if (((i16)g_madris_state) >= 28) {
                if (((i16)g_madris_state) < 42) {
                    ax = draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + 0xBA), 0x98, 0x1E);
                }
            } else {
                ax = draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + 0xB2), 0x8C, 0x1E);
            }
        } else {
            ax = draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + 0xAA), 0x8F, 0x1E);
        }
        if (((i16)g_madris_state) > 42) {
            draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + 0xC2), 0x8F, 0x1E);
            draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + 0xCA), 0x92, 0x32);
            g_madris_state = g_madris_state + 1;
            ax = g_madris_state;
            if (((i16)g_madris_state) > 56) {
                g_madris_state = 0;
                mem_zero(DSP(0x8AD4), 0xC8);
                G32(0x7A7E) = 8;
                G32(0x7A74) = 0;
                g_madris_rot = 0;
                g_madris_input = 0;
                g_madris_piece = 0;
                t21 = random_n(7, 1);
                g_madris_next = t21;
                ax = t21;
            }
        }
    } else if (a0 == 1) {
        ax = popup_close(g_tetris_bg, ax);
    }
    return ax;
}

/* 2B74:09F1  draw the 10 savegame slot names (2 columns x 5) in the safe */
u16 save_slots_draw(void) {
    u16 di;
    u16 si;
    FUNC_TICK(8);
    di = 0;
    while (((i16)di) < 2) {
        LOOP_TICK(46);
        si = 1;
        while (((i16)si) <= 5) {
            LOOP_TICK(41);
            draw_sprite(MK32(g_room_gfx1_hi, g_room_gfx1_lo + 0x22), (di * 0x76) + 0x40, (si * 0x11) + 8);
            text_draw((di * 0x76) + 0x40, si * 0x11, G16(((((di * 5) + si) - 1) << 2) + 0x8BB2), G16(((((di * 5) + si) - 1) << 2) + 0x8BB4));
            si = si + 1;
        }
        di = di + 1;
    }
    return present_frame();
}

/* 2B74:0A6D  (mode): "Please insert the disk ..." box */
u16 save_insert_disk_box(u16 a0) {
    u16 ax;
    FUNC_TICK(37);
    ax = ui_box(a0, 0xA, 5, 0x1D, 9, 2, 0xA);
    if (a0 != 1) {
        text_draw_full(0x54, 0x24, g_text(286), 0x12, 1, g_screen_buf_lo, g_screen_buf_hi, 0xFFFF);
        ax = present_frame();
    }
    return ax;
}

/* 2B74:0AC8  (mode, text_id): mode 0 shows a 160x40 popup with text a1 (disk swap prompts), mode 1 restores the background */
void ui_disk_prompt(u16 a0, u16 a1) {
    u32 t1;
    u32 t2;
    FUNC_TICK(101);
    if (a0 == 0) {
        t1 = bitmap_alloc(0xA0, 0x28);
        g_ui_popup_bmp = t1;
        t2 = bitmap_alloc(0xA0, 0x28);
        g_ui_popup_bg = t2;
        rtl_vga_get_bitmap_w(W2(g_ui_popup_bmp), 0xA, 0x28);
        bitmap_put(g_ui_popup_bmp, g_ui_popup_bg, 0, 0);
        ui_frame(0, 0, 0x13, 4, g_ui_popup_bmp_lo, g_ui_popup_bmp_hi, 2);
        text_draw_full(4, 4, g_text(a1), 0x12, 1, g_ui_popup_bmp_lo, g_ui_popup_bmp_hi, 0xFFFF);
        rtl_vga_put_bitmap_w(W2(g_ui_popup_bmp), 0xA, 0x28);
        mem_free(g_ui_popup_bmp_lo, g_ui_popup_bmp_hi);
    } else if (a0 == 1) {
        rtl_vga_put_bitmap_w(W2(g_ui_popup_bg), 0xA, 0x28);
        mem_free(g_ui_popup_bg_lo, g_ui_popup_bg_hi);
    }
    return;
}

/* 2B74:0BDD  (mode, str): 160x40 popup with the two language names (strings at +1 and +0x15 of resource 41); mode 1 restores background */
void lang_prompt_box(u16 a0, u32 a1) {
    u32 l_4;
    u32 t2;
    FUNC_TICK(110);
    if (a0 == 0) {
        l_4 = bitmap_alloc(0xA0, 0x28);
        t2 = bitmap_alloc(0xA0, 0x28);
        g_lang_prompt_bg = t2;
        rtl_vga_get_bitmap_w(W2(l_4), 0xA, 0x50);
        bitmap_put(l_4, g_lang_prompt_bg, 0, 0);
        ui_frame(0, 0, 0x13, 4, LO16(l_4), HI16(l_4), 2);
        text_draw_center_to(0x50, 4, LO16(a1) + 1, HI16(a1), LO16(l_4), HI16(l_4));
        text_draw_center_to(0x50, 0xC, LO16(a1) + 0x15, HI16(a1), LO16(l_4), HI16(l_4));
        rtl_vga_put_bitmap_w(W2(l_4), 0xA, 0x50);
        mem_free(LO16(l_4), HI16(l_4));
    } else if (a0 == 1) {
        rtl_vga_put_bitmap_w(W2(g_lang_prompt_bg), 0xA, 0x50);
        mem_free(g_lang_prompt_bg_lo, g_lang_prompt_bg_hi);
    }
    return;
}

/* 2B74:0D03  (mode, str): boss speech box, draws str at 156,12 */
u16 boss_say(u16 a0, u16 a1, u16 a2) {
    u16 ax;
    FUNC_TICK(31);
    ax = ui_box(a0, 0x13, 1, 0x26, 0xD, 2, 0x13);
    if (a0 != 1) {
        ax = text_draw_wrap_free(0x9C, 0xC, a1, a2, 0x12, 1);
    }
    return ax;
}

/* 2B74:0D49  (mode, str): right-side speech box (room 0x40 character: "Good afternoon, */
u16 gossip_say(u16 a0, u16 a1, u16 a2) {
    u16 ax;
    FUNC_TICK(33);
    ax = ui_box(a0, 0x1A, 2, 0x26, 0xC, 1, 0x1A);
    if (a0 != 1) {
        text_draw_wrap_free(0xD4, 0x14, a1, a2, 0xB, 1);
        ax = present_frame();
    }
    return ax;
}

/* 2B74:0D96  (mode, col, row, w, h, style) in 8-px cells: mode 0 save background on g_ui_box_stack + draw frame, 2 redraw frame only, 1 pop and restore background */
u16 ui_box(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5, u16 ax) {
    u16 si;
    u16 t1;
    FUNC_TICK(48);
    clip_reset();
    si = a0;
    if (a0 == 0) {
L_0DAB:
        if (si == 0) {
            t1 = popup_open(a1, a2, a3, a4, 0, 0, 3);
            g_ui_box_stack(g_ui_box_depth) = t1;
            g_ui_box_depth = g_ui_box_depth + 1;
        }
        ax = ui_frame(a1, a2, a3, a4, g_screen_buf_lo, g_screen_buf_hi, a5);
    } else if (si != 2) {
        if (si == 1) {
            g_ui_box_depth = g_ui_box_depth - 1;
            ax = popup_close(g_ui_box_stack(g_ui_box_depth), ax);
        }
    } else {
        goto L_0DAB;
    }
    return ax;
}

/* 2B74:0E17  (mode): small box with "You look for your junk yourself ..." */
void porter_say(u16 a0) {
    u32 t1;
    FUNC_TICK(37);
    ui_box(a0, 2, 2, 0x12, 7, 1, 0 /* unused ax */);
    if (a0 != 1) {
        t1 = text_dup(0x52);
        text_draw_wrap_free(0x14, 0x14, LO16(t1), HI16(t1), 0xD, 1);
    }
    return;
}

/* 2B74:0E63  options panel: speed buttons 20/30/60 (g_game_speed) and On/Off switches "TV family", "TV picture", "Music" */
void options_draw(void) {
    u16 si;
    u16 t4;
    u16 t5;
    u16 t6;
    u16 x0 = 0x58, x1 = 0xC8, b0 = 0x60, b1 = 0xC0;   /* the panel, its boxes */
    FUNC_TICK(231);
    if (speed4_on()) {
        speed4_panel(&x0, &x1, &b0, &b1);         /* --fourth-speed: room for four buttons (fixes.c) */
    }
    ui_tile_box(x0, 8, x1, 0x72, 0);
    draw_sprite(MK32(g_room_gfx3_hi, g_room_gfx3_lo + 0xA2), 0x61, 0xC);
    ui_tile_box(b0, 0x1A, b1, 0x30, 1);
    ui_tile_box(b0, 0x32, b1, 0x48, 1);
    ui_tile_box(b0, 0x4A, b1, 0x60, 1);
    g_tile_atlas = g_room_gfx3;
    g_tile_target = g_screen_buf;
    if (speed4_on()) {
        speed4_draw_buttons();
        goto L_texts;
    }
    if (g_game_speed != 0x14) {
        si = 0x19;
    } else {
        si = 0x18;
    }
    t4 = col_to_px(si);
    draw_atlas_tile(t4, 0x60, 0x62);
    if (g_game_speed != 0x1E) {
        si = 0x19;
    } else {
        si = 0x18;
    }
    t5 = col_to_px(si);
    draw_atlas_tile(t5, 0x82, 0x62);
    if (g_game_speed != 0x3C) {
        si = 0x19;
    } else {
        si = 0x18;
    }
    t6 = col_to_px(si);
    draw_atlas_tile(t6, 0xA4, 0x62);
    draw_atlas_tile(0xCA, 0x61, 0x63);
    draw_atlas_tile(0xD2, 0x83, 0x63);
    draw_atlas_tile(0xDA, 0xA5, 0x63);
L_texts:
    text_draw_center(0x90, 0x14, G16(0x84DC), G16(0x84DE));
    text_draw_center(0x90, 0x2C, G16(0x84E0), G16(0x84E2));
    text_draw_center(0x90, 0x44, G16(0x84F0), G16(0x84F2));
    options_draw_switch(g_opt_tv_family);
    blit_transparent(g_actor_txt, g_screen_buf, 0x80, 0x24, 0x20, 0, 0x20, 0xA, 0 /* unused ax */);
    options_draw_switch(g_tv_off);
    blit_transparent(g_actor_txt, g_screen_buf, 0x80, 0x3C, 0x20, 0, 0x20, 0xA, 0 /* unused ax */);
    options_draw_switch(g_opt_music);
    blit_transparent(g_actor_txt, g_screen_buf, 0x80, 0x54, 0x20, 0, 0x20, 0xA, 0 /* unused ax */);
    present_frame();
    return;
}

/* 2B74:10B8  draw an On/Off slider switch at offset a0 into the options switch bitmap */
void options_draw_switch(u16 a0) {
    FUNC_TICK(56);
    draw_sprite_to(MK32(g_room_gfx3_hi, g_room_gfx3_lo + 0xB2), g_actor_txt_lo, g_actor_txt_hi, 0x21 - a0, 1);
    text_draw_center_to(0x31 - a0, 0xFFFA, G16(0x84F8), G16(0x84FA), g_actor_txt_lo, g_actor_txt_hi);
    text_draw_center_to(0x4F - a0, 0xFFFA, G16(0x84FC), G16(0x84FE), g_actor_txt_lo, g_actor_txt_hi);
    draw_sprite_to(MK32(g_room_gfx3_hi, g_room_gfx3_lo + 0xAA), g_actor_txt_lo, g_actor_txt_hi, 0x20, 0);
    return;
}

/* 2B74:114F  ?(mode): "You want to cancel? Are you serious?" with yes/no buttons (no callers found) */
void confirm_cancel_box(u16 a0) {
    FUNC_TICK(58);
    ui_box(a0, 0x19, 1, 0x26, 0xC, 1, 0 /* unused ax */);
    if (a0 != 1) {
        text_draw_full(0xCC, 4, g_text(300), 0xC, 1, g_screen_buf_lo, g_screen_buf_hi, 0xFFFF);
        draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + 0x12), 0xE4, 0x50);
        draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + 0x1A), 0x10C, 0x50);
    }
    return;
}

/* 2B74:11D8  ?draw the 3-station indicator strip (selected player highlighted, eliminated players greyed; first icon depends on g_opt_tv_picture) */
void ui_draw_player_icons(void) {
    u16 ax;
    u16 s0;
    u16 s1;
    u16 s2;
    u16 s3;
    u16 si;
    u32 l_4;
    FUNC_TICK(58);
    l_4 = bitmap_alloc(0x48, 0xB);
    cursor_hide();
    rtl_vga_get_bitmap_w(W2(l_4), 2, 0xBC);
    s0 = 0;
    s1 = 7;
    s2 = HI16(l_4);
    s3 = LO16(l_4);
    if (g_tv_off != 0) {
        ax = 0x72;
    } else {
        ax = 0x6A;
    }
    draw_sprite_to(MK32(g_tvfam_atlas_hi, g_tvfam_atlas_lo + ax), s3, s2, s1, s0);
    si = 1;
    while (((i16)si) <= 3) {
        LOOP_TICK(35);
        s0 = 0;
        s1 = (si * 0xF) + 7;
        s2 = HI16(l_4);
        s3 = LO16(l_4);
        if ((players(si).out) == 0) {
            if (si != g_tv_channel) {
                ax = 0x72;
            } else {
                ax = 0x6A;
            }
        } else {
            ax = 0x7A;
        }
        draw_sprite_to(MK32(g_tvfam_atlas_hi, g_tvfam_atlas_lo + ax), s3, s2, s1, s0);
        si = si + 1;
    }
    rtl_vga_put_bitmap_w(W2(l_4), 2, 0xBC);
    mem_free(LO16(l_4), HI16(l_4));
    cursor_show(0 /* unused ax */);
    return;
}

/* 2B74:12CC  new-game screen: "Select a player", per station: station name, manager name (except the one being edited, a0), Computer/Player, difficulty Easy/Normal/Difficult (highlight g_difficulty), buttons New game/Load game/End */
u16 newgame_draw_setup(u16 a0) {
    u16 ax;
    u16 bx;
    int cc;
    u16 di;
    u16 si;
    u16 l_2;
    FUNC_TICK(107);
    ui_tile_box(4, 4, 0x13C, 0x70, 0);
    ui_tile_box(0x18, 8, 0x128, 0x14, 1);
    text_draw_center_to(0xA0, 2, G16(0x8948), G16(0x894A), g_screen_buf_lo, g_screen_buf_hi);
    di = 1;
    while (((i16)di) <= 3) {
        LOOP_TICK(153);
        l_2 = (u16)(i8)g_player_color(di);
        ui_tile_box((di * 0x60) + 0xFFB8, 0x1A, (di * 0x60) + 8, 0x48, 1);
        ui_tile_box((di * 0x60) + 0xFFBC, 0x1C, (di * 0x60) + 4, 0x28, l_2);
        ui_tile_box((di * 0x60) + 0xFFBC, 0x2C, (di * 0x60) + 4, 0x38, l_2);
        cc = g_player != di;
        si = (di * 0x60) + 0xFFB8;
        if (cc) {
            ax = 0;
        } else {
            ax = 6;
        }
        ui_tile_box(si + 4, 0x3A, si + 0x4C, 0x46, ax);
        text_draw(si + 8, 0x16, pl_station_lo(di), pl_station_hi(di));
        if (g_player != di) {
            bx = 0x14A;
        } else {
            bx = 0x14B;
        }
        text_draw(si + 8, 0x34, g_text_lo(bx), g_text_hi(bx));
        if (di != a0) {
            text_draw((di * 0x60) + 0xFFC0, 0x26, pl_name_lo(di), pl_name_hi(di));
        }
        if (g_difficulty != di) {
            ax = 0;
        } else {
            ax = 6;
        }
        ui_tile_box(si + 4, 0x4C, si + 0x4C, 0x58, ax);
        text_draw_center(si + 0x28, 0x46, G16(((di + 0x1A3) << 2) + 0x8390), G16(((di + 0x1A3) << 2) + 0x8392));
        di = di + 1;
    }
    ui_tile_box(0x18, 0x5E, 0x68, 0x6C, 4);
    text_draw_center(0x40, 0x59, G16(0x88C0), G16(0x88C2));
    ui_tile_box(0x78, 0x5E, 0xC8, 0x6C, 4);
    text_draw_center(0xA0, 0x59, G16(0x88C4), G16(0x88C6));
    ui_tile_box(0xD8, 0x5E, 0x128, 0x6C, 5);
    return text_draw_center(0x100, 0x59, G16(0x894C), G16(0x894E));
}

/* 2B74:154F  (mode, bids far): sell-transmitter dialog: "Transmitter: $" value, "Auction:" with each AI's offer ("Bid: $"), "Scrap:"/"Throw away" */
void transmitter_sale_dialog(u16 a0, u32 a1) {
    u16 bx;
    int cc;
    u16 di;
    u16 es;
    u16 si;
    u16 t1;
    u32 t2;
    u16 t5;
    u32 t11;
    FUNC_TICK(150);
    di = a0;
    if (a0 == 0) {
L_1563:
        if (di == 0) {
            t1 = popup_open(0xA, 3, 0x1D, 0xC, 0, 0, 3);
            g_tx_dialog_bg = t1;
        }
        ui_frame(0xA, 3, 0x1D, 0xC, g_screen_buf_lo, g_screen_buf_hi, 2);
        t2 = str_format(g_text(368), W2(tx(g_player, g_selected_index).fee));
        text_draw_wrap_free(0x58, 0x18, LO16(t2), HI16(t2), 0x12, 1);
        ui_band(0xA, 0x28, 0x13, 0xA, 3);
        text_draw(0x58, 0x21, G16(0x8954), G16(0x8956));
        si = 0;
        while (((i16)si) < 2) {
            LOOP_TICK(79);
            cc = FP32(a1, si << 2) == 0xFFFFFFFF;
            bx = FPOFF(a1) + (si << 2);
            es = FPSEG(a1);
            if (!(cc)) {
                t5 = col_to_px(((u16)(i8)g_ai_player(si)) + 0x12);
                draw_sprite(MK32(g_room_gfx1_hi, g_room_gfx1_lo + t5), 0x58, (si * 0xB) + 0x33);
                text_draw(0x5A, (si * 0xB) + 0x2D, pl_station_lo((u16)(i8)g_ai_player(si)), pl_station_hi((u16)(i8)g_ai_player(si)));
                cc = ((i32)FP32(a1, si << 2)) > 0;
                bx = FPOFF(a1) + (si << 2);
                es = FPSEG(a1);
                if (cc) {
                    t11 = str_format(g_text(370), W2(FP32(a1, si << 2)));
                    text_draw_free(0x80, (si * 0xB) + 0x2D, LO16(t11), HI16(t11));
                }
            }
            si = si + 1;
        }
        ui_band(0xA, 0x49, 0x13, 0xA, 3);
        text_draw(0x5A, 0x42, G16(0x895C), G16(0x895E));
        draw_sprite(MK32(g_room_gfx1_hi, g_room_gfx1_lo + 0xAA), 0x58, 0x54);
        text_draw(0x5A, 0x4D, G16(0x8960), G16(0x8962));
    } else if (di == 2) {
        goto L_1563;
    } else if (di == 1) {
        popup_close(g_tx_dialog_bg, 0 /* unused ax */);
    }
    return;
}

/* 2B74:1791  Betty's greeting "Hello <name>, ...": text by Betty love (0 / <30 / >=30) or special text if player out flag set */
u16 betty_greeting(u16 a0) {
    u16 ax;
    u16 si;
    u32 t4;
    FUNC_TICK(90);
    ax = ui_box(a0, 0x15, 2, 0x26, 0xD, 1, 0x15);
    if (a0 != 1) {
        if ((players(g_player).betty_love) != 0) {
            if (((i16)(players(g_player).betty_love)) >= 30) {
                si = 0x19;
            } else {
                si = 0x18;
            }
        } else {
            si = 0x17;
        }
        if ((players(g_player).out) != 0) {
            si = 0x105;
        }
        t4 = str_format(DSP(0x1551) /* "#s #s, #s" */, W2(g_text(22)), W2(players(g_player).name), W2(g_text(si)));
        text_draw_wrap_free(0xB0, 0x10, LO16(t4), HI16(t4), 0xF, 1);
        ax = present_frame();
    }
    return ax;
}

/* 2B74:1875  "An important message for */
void msg_show_next(u16 a0) {
    u16 ax;
    u16 bx;
    u16 di;
    u16 dx;
    u16 es;
    u16 s0;
    u16 s1;
    u16 s2;
    u16 s3;
    u16 si;
    u32 l_4;
    u32 l_8;
    u32 l_C;
    u32 l_10;
    u32 l_14;
    u32 l_18;
    u32 t1;
    u32 t2;
    u32 t44;
    FUNC_TICK(494);
    ui_frame(5, 2, 0x22, 0xC, g_screen_buf_lo, g_screen_buf_hi, 1);
    t1 = str_format(g_text(364), W2(players(a0).name));
    text_draw_wrap_free(0x2C, 0xC, LO16(t1), HI16(t1), 0x13, 1);
    l_4 = DSP(0x60BF);
    di = a0;
    if (FP16(l_4, 0) != 0x113) {
        if (FP16(l_4, 0) == 0x10E) {
            if (((u16)(((i16)((screenplays(FP16(l_4, 6)).flags_16) << 0xF)) >> 0xF)) != 0) {
                txt_series_cache_load(0, screenplays(FP16(l_4, 6)).target, 0);
                l_14 = series_title(screenplays(FP16(l_4, 6)).target);
                l_10 = series_episode_title(screenplays(FP16(l_4, 6)).target, FP16(l_4, 0xA));
                l_C = g_text(365);
            } else {
                txt_film_cache_load(0, screenplays(FP16(l_4, 6)).target, 0);
                l_14 = movie_title(screenplays(FP16(l_4, 6)).target);
                l_C = DSP(0x155B);
                l_10 = DSP(0x155C);
            }
            l_8 = str_format(DSP(0x155D) /* "#s #s #s" */, W2(l_14), W2(l_C), W2(l_10));
            if (((u16)(((i16)((screenplays(FP16(l_4, 6)).flags_16) << 0xF)) >> 0xF)) != 0) {
                txt_series_cache_free(0xFFFF);
            } else {
                txt_film_cache_free(0xFFFF);
            }
            l_18 = str_format(g_text(270), FP16(l_4, 2), W2(l_8), FP16(l_4, 0xE));
            mem_free(LO16(l_8), HI16(l_8));
        } else if (FP16(l_4, 0) == 0x10F || FP16(l_4, 0) == 0x110) {
            l_18 = str_format(g_text(FP16(l_4, 0)), W2(ads(FP16(l_4, 2)).name), W2(FP32(l_4, 6)));
        } else if (FP16(l_4, 0) == 0x133 || FP16(l_4, 0) == 0x136) {
            l_18 = str_format(g_text(FP16(l_4, 0)), FP16(l_4, 2));
        } else if (FP16(l_4, 0) != 0x152) {
            if (FP16(l_4, 0) != 0x158) {
                if (FP16(l_4, 0) != 0x159) {
                    if (FP16(l_4, 0) != 0x2B) {
                        if (FP16(l_4, 0) == 0x15F) {
                            bx = FPOFF(l_4);
                            es = FPSEG(l_4);
                            if (((i32)FP32(l_4, 0xA)) <= 10) {
                                ax = G16(((FP16(l_4, 0xA) + 0x1A) << 2) + 0x8390);
                                dx = G16(((FP16(l_4, 0xA) + 0x1A) << 2) + 0x8392);
                            } else {
                                ax = G16(((FP16(l_4, 0xA) + 0x11A) << 2) + 0x8390);
                                dx = G16(((FP16(l_4, 0xA) + 0x11A) << 2) + 0x8392);
                            }
                            s0 = dx;
                            s1 = ax;
                            bx = FPOFF(l_4);
                            es = FPSEG(l_4);
                            if (((i32)FP32(l_4, 6)) <= 10) {
                                ax = G16(((FP16(l_4, 6) + 0x1A) << 2) + 0x8390);
                                dx = G16(((FP16(l_4, 6) + 0x1A) << 2) + 0x8392);
                            } else {
                                ax = G16(((FP16(l_4, 6) + 0x11A) << 2) + 0x8390);
                                dx = G16(((FP16(l_4, 6) + 0x11A) << 2) + 0x8392);
                            }
                            s2 = dx;
                            s3 = ax;
                            bx = FPOFF(l_4);
                            es = FPSEG(l_4);
                            if (((i32)FP32(l_4, 2)) <= 10) {
                                ax = G16(((FP16(l_4, 2) + 0x1A) << 2) + 0x8390);
                                dx = G16(((FP16(l_4, 2) + 0x1A) << 2) + 0x8392);
                            } else {
                                ax = G16(((FP16(l_4, 2) + 0x11A) << 2) + 0x8390);
                                dx = G16(((FP16(l_4, 2) + 0x11A) << 2) + 0x8392);
                            }
                            l_18 = str_format(g_text(351), ax, dx, s3, s2, s1, s0);
                        } else if (FP16(l_4, 0) != 0x15B) {
                            l_18 = text_dup(FP16(l_4, 0));
                            if (FP16(l_4, 0) == 0x135) terror_text(l_18);   /* option --terror-damage: its amount (src/fixes.c) */
                        } else {
                            txt_film_cache_load(0, FP16(l_4, 2), 0);
                            t44 = movie_title(FP16(l_4, 2));
                            l_18 = str_format(g_text(347), W2(players(di).name), W2(t44));
                            txt_film_cache_free(0xFFFF);
                        }
                    } else {
                        l_18 = str_format(g_text(43), W2(G32(((FP16(l_4, 2) + 0x153) << 2) + 0x8390)));
                    }
                } else {
                    l_18 = str_format(g_text(345), W2(players(di).name), W2(G32(((FP16(l_4, 2) + 0x153) << 2) + 0x8390)));
                    sammy_prize_text(l_18, FP32(l_4, 6));       /* option --sammy-prize: the money it came with (src/fixes.c) */
                }
            } else {
                l_18 = str_format(g_text(344), W2(G32(((FP16(l_4, 2) + 0x153) << 2) + 0x8390)), W2(players(FP16(l_4, 6)).name));
            }
        } else {
            l_18 = str_format(g_text(338), W2(G32(((FP16(l_4, 2) + 0x121) << 2) + 0x8390)));
        }
    } else {
        txt_film_cache_load(0, FP16(l_4, 2), 0);
        t2 = movie_title(FP16(l_4, 2));
        l_18 = str_format(g_text(275), W2(t2));
        txt_film_cache_free(0xFFFF);
    }
    text_draw_wrap_free(0x2C, 0x1C, LO16(l_18), HI16(l_18), 0x1D, 1);
    si = 0;
    while (((i16)si) <= 3) {
        LOOP_TICK(20);
        far_memcpy(HPTR(messages_at(si)), HPTR(messages_at(si + 1)), 0x1A);
        si = si + 1;
    }
    g_message_pos = g_message_pos - 1;
    present_frame();
    return;
}

/* 2B74:1E56  ?show a framed text box with per-player event text id g_player_event_text[a0] formatted with the player's name */
void ui_player_event_text(u16 a0) {
    u32 l_4;
    u32 t5;
    FUNC_TICK(83);
    l_4 = bitmap_alloc(0xF0, 0x58);
    rtl_vga_get_bitmap_w(W2(l_4), 5, 0x10);
    ui_frame(0, 0, 0x1D, 0xA, LO16(l_4), HI16(l_4), 1);
    t5 = str_format(g_text(g_betty_letter(a0)), W2(players(a0).name));
    text_draw_ex_free(4, 4, LO16(t5), HI16(t5), 0x1D, 1, LO16(l_4), HI16(l_4));
    cursor_hide();
    rtl_vga_put_bitmap_w(W2(l_4), 5, 0x10);
    cursor_show(R.ax);
    mem_free(LO16(l_4), HI16(l_4));
    return;
}

/* 2B74:1F33  draw the balance sheet (today/yesterday ledger): Expenses (films bought incl. pending auction bids, transmitters, screenplays, actors/scene, liquidation damages, studio rent, news, transmitter fees, sundry), Income (films sold, ads, sundry), interest, previous/current balance */
void finance_report_draw(void) {
    u16 di;
    u16 si;
    u16 l_2;
    u16 l_4;
    u16 l_6;
    u16 l_8;
    u16 l_A;
    u16 l_C;
    u32 l_10;
    u16 t5;
    u32 t7;
    u32 t9;
    u32 t10;
    u16 t14;
    u32 t16;
    u16 t17;
    u32 t19;
    u32 t20;
    u32 t21;
    u16 t25;
    u32 t27;
    u32 t28;
    u32 t29;
    u32 t32;
    u32 t33;
    FUNC_TICK(400);
    l_A = 0;
    l_C = 0;
    l_6 = 0;
    l_8 = 0;
    l_2 = 0;
    l_4 = 0;
    G32(0x386) = 0x40001;
    G32(0x38A) = 0x13B0004;
    g_clip_y2 = 0x74;
    si = 0;
    while (((i16)si) < 113) {
        LOOP_TICK(43);
        di = 1;
        while (((i16)di) < 39) {
            LOOP_TICK(16);
            draw_sprite(MK32(g_room_gfx3_hi, g_room_gfx3_lo + 0xA), di << 3, si);
            di = di + 1;
        }
        draw_sprite(MK32(g_room_gfx3_hi, g_room_gfx3_lo + 2), 4, si);
        draw_sprite(MK32(g_room_gfx3_hi, g_room_gfx3_lo + 2), 0x138, si);
        si = si + 8;
    }
    clip_reset();
    if (g_chart_mode != 0) {
        l_10 = DSP(0x71F1);
        si = 0;
        while (((i16)si) <= 10) {
            LOOP_TICK(34);
            if ((auction(si).program) != 0) {
                if (((u16)((u16)(i8)(auction(si).bidder))) == g_player) {
                    t5 = l_4;
                    l_4 = l_4 + lot_bid_lo(si);
                    l_2 = (l_2 + (u16)(script_offers(si + 0x2F).script)) + ((u32)t5 + lot_bid_lo(si) > 0xFFFF);
                }
            }
            si = si + 1;
        }
    } else {
        l_10 = DSP(0x7492);
    }
    ui_frame_px(8, 2, 0x98, 0x60, g_screen_buf, 1);
    text_draw(0xC, 0xFFFE, G16(0x87E0), G16(0x87E2));
    ui_draw_hline(0xC, 0x9C, 0xE);
    text_draw(0xC, 8, G16(0x87E4), G16(0x87E6));
    t7 = FP32(l_10, 0x1C) + FP32(l_10, 0x44);
    t9 = (FP32(l_10, 8) + FP32(l_10, 0xC)) + MK32(l_2, l_4);
    t10 = str_format(DSP(0x1566) /* "#_8l $~#_8l $~#_8l $~#_8l $~#_8l $~#_8l $~#_8l $~#_8l $~#..." */, W2(t9), W2(FP32(l_10, 0x10)), W2(FP32(l_10, 0x14)), W2(FP32(l_10, 0x20)), W2(FP32(l_10, 0x18)), W2(FP32(l_10, 0x24)), W2(FP32(l_10, 0x2C)), W2(FP32(l_10, 0x28)), W2(t7));
    text_draw_free(0x6C, 8, LO16(t10), HI16(t10));
    ui_draw_hline(0xC, 0x9C, 0x58);
    text_draw(0xC, 0x52, G16(0x87F4), G16(0x87F6));
    si = 1;
    while (((i16)si) <= 10) {
        LOOP_TICK(12);
        t14 = l_C;
        l_C = l_C + (FP16(l_10, (si << 2) + 4));
        l_A = (l_A + FP16(l_10, (si << 2) + 6)) + ((u32)t14 + FP16(l_10, (si << 2) + 4) > 0xFFFF);
        si = si + 1;
    }
    t16 = FP32(l_10, 0x44) + MK32(l_2, l_4);
    t17 = l_C;
    l_C = l_C + LO16(t16);
    l_A = (l_A + HI16(t16)) + ((u32)t17 + LO16(t16) > 0xFFFF);
    t19 = str_format(DSP(0x15A5) /* "#_8l $" */, l_C, l_A);
    text_draw_free(0x6C, 0x52, LO16(t19), HI16(t19));
    ui_frame_px(0xA0, 2, 0x130, 0x2C, g_screen_buf, 1);
    ui_frame_px(0xA0, 0x32, 0x130, 0x60, g_screen_buf, 1);
    text_draw(0xA4, 0xFFFE, G16(0x87C4), G16(0x87C6));
    ui_draw_hline(0xA4, 0x134, 0xE);
    text_draw(0xA4, 8, G16(0x87EC), G16(0x87EE));
    t20 = FP32(l_10, 0x30) + FP32(l_10, 0x34);
    t21 = str_format(DSP(0x15AC) /* "#_8l $~#_8l $~#_8l $" */, W2(t20), W2(FP32(l_10, 0x3C)), W2(FP32(l_10, 0x38)));
    text_draw_free(0x104, 8, LO16(t21), HI16(t21));
    ui_draw_hline(0xA4, 0x134, 0x27);
    text_draw(0xA4, 0x21, G16(0x87F4), G16(0x87F6));
    si = 0xB;
    while (((i16)si) <= 14) {
        LOOP_TICK(12);
        t25 = l_8;
        l_8 = l_8 + (FP16(l_10, (si << 2) + 4));
        l_6 = (l_6 + FP16(l_10, (si << 2) + 6)) + ((u32)t25 + FP16(l_10, (si << 2) + 4) > 0xFFFF);
        si = si + 1;
    }
    t27 = str_format(DSP(0x15C1) /* "#_8l $" */, l_8, l_6);
    text_draw_free(0x104, 0x21, LO16(t27), HI16(t27));
    text_draw(0xA4, 0x2E, G16(0x87F0), G16(0x87F2));
    ui_draw_hline(0xA4, 0x134, 0x3E);
    text_draw(0xA4, 0x38, G16(0x87F8), G16(0x87FA));
    ui_draw_hline(0xA4, 0x134, 0x58);
    text_draw(0xA4, 0x52, G16(0x87FC), G16(0x87FE));
    t28 = str_format(DSP(0x15C8) /* "#_8l $" */, W2(FP32(l_10, 0)));
    text_draw_free(0x104, 0x2E, LO16(t28), HI16(t28));
    t29 = str_format(DSP(0x15CF) /* "#_8l $~#_8l $~#_8l $" */, l_8, l_6, l_C, l_A, W2(FP32(l_10, 0x40)));
    text_draw_free(0x104, 0x38, LO16(t29), HI16(t29));
    t32 = ((FP32(l_10, 0) + MK32(l_6, l_8)) - MK32(l_A, l_C)) - FP32(l_10, 0x40);
    t33 = str_format(DSP(0x15E4) /* "#_8l $" */, W2(t32));
    text_draw_free(0x104, 0x52, LO16(t33), HI16(t33));
    draw_sprite(MK32(g_room_gfx3_hi, g_room_gfx3_lo + 0x2A), 0x88, 0x68);
    text_draw(0x8F, 0x62, G16(((g_chart_mode + 0x102) << 2) + 0x8390), G16(((g_chart_mode + 0x102) << 2) + 0x8392));
    present_frame();
    return;
}

/* 2B74:2496  draw a horizontal line of 8-px tiles from x a0 to a1 at y a2 */
void ui_draw_hline(u16 a0, u16 a1, u16 a2) {
    u16 di;
    u16 si;
    FUNC_TICK(22);
    si = a0;
    di = a1;
    while (((i16)(di + 0xFFF8)) >= ((i16)si)) {
        LOOP_TICK(14);
        draw_sprite(MK32(g_room_gfx3_hi, g_room_gfx3_lo + 0x9A), si, a2);
        si = si + 8;
    }
    draw_sprite(MK32(g_room_gfx3_hi, g_room_gfx3_lo + 0x9A), di + 0xFFF8, a2);
    return;
}

/* 2B74:24E8  (mode): "Saved game not found. Quit?" with two buttons */
u16 save_not_found_box(u16 a0) {
    u16 ax;
    FUNC_TICK(60);
    ax = ui_box(a0, 0xA, 3, 0x1D, 0xB, 1, 0xA);
    if (a0 != 1) {
        text_draw_full(0x54, 0x14, g_text(426), 0x12, 1, g_screen_buf_lo, g_screen_buf_hi, 0xFFFF);
        draw_sprite(MK32(g_room_gfx1_hi, g_room_gfx1_lo + 0x12), 0x80, 0x48);
        draw_sprite(MK32(g_room_gfx1_hi, g_room_gfx1_lo + 0x1A), 0xB8, 0x48);
        ax = present_frame();
    }
    return ax;
}

/* 2B74:2578  (mode): load/save selection overlay box with two buttons */
u16 save_overlay_box(u16 a0) {
    u16 ax;
    FUNC_TICK(61);
    ax = ui_box(a0, 0xA, 3, 0x1D, 0xB, 2, 0xA);
    if (a0 != 1) {
        text_draw_full(0x54, 0x14, g_text(285), 0x12, 1, g_screen_buf_lo, g_screen_buf_hi, 0xFFFF);
        draw_sprite(MK32(g_room_gfx1_hi, g_room_gfx1_lo + 2), 0x7B, 0x40);
        draw_sprite(MK32(g_room_gfx1_hi, g_room_gfx1_lo + 0xA), 0xB5, 0x40);
        ax = present_frame();
    }
    return ax;
}
