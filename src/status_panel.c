/* Status panel: status panel, hover info (tooltips), TV info, hourly broadcast evaluation.
 *
 * Original module 14E1 of MTV.EXE (Rainbow Arts, 1991).  Decompiled by
 * tools/decomp.py and hand-maintained since the freeze (git tag
 * generated-final).  Every function keeps its original address in the
 * comment above it; that address ties it to the machine-level twin in gen/
 * and to the difftest tables -- keep it when editing.
 */
#include "game.h"
#include "fixes.h"

/* 14E1:000E  hover info: by g_room/g_room_state/g_hotspot find the object under the mouse (panel money/rating/day/TV, film and script shelves, ads, contracts, schedule slots, studios on the estate board, actors/props, transmitters...), build info id (kind*1000 + index) and window rect/size, call tooltip_show */
void tooltip_update(void) {
    u16 ax;
    int cc;
    u16 di;
    u16 si;
    u32 l_4;
    u16 t1;
    u16 t2;
    u32 t9;
    u16 t11;
    u16 t16;
    u16 t17;
    u16 t22;
    u16 t34;
    u16 t40;
    u16 t53;
    u16 t58;
    u16 t64;
    u16 t68;
    u16 t72;
    u16 t76;
    u16 t79;
    u16 t83;
    u16 t85;
    u16 t90;
    u16 t91;
    u16 t93;
    u16 t117;
    u16 t118;
    u16 t141;
    u16 t142;
    u16 t145;
    u16 t160;
    u16 t163;
    u16 t166;
    u16 t168;
    u16 t175;
    u16 t181;
    u16 t187;
    u16 t190;
    u16 t193;
    u16 t196;
    u16 t199;
    u16 t201;
    u16 t209;
    u16 t212;
    u16 t214;
    u16 t218;
    u16 t222;
    u32 t229;
    u32 t230;
    u32 t232;
    u16 t234;
    u32 t236;
    u32 t237;
    u32 t239;
    u32 t240;
    u32 t242;
    u16 t244;
    u16 t247;
    u16 t249;
    u16 t252;
    u32 t258;
    u16 t263;
    u16 t267;
    u16 t270;
    u32 t275;
    u16 t287;
    u16 t288;
    u16 t289;
    u16 t292;
    u16 t295;
    u16 t300;
    u16 t303;
    u16 t310;
    u16 t311;
    u16 t312;
    FUNC_TICK(1045);
    g_tip_contract = 0;
    g_tip_pos = 0;
    g_tip_w = 0;
    g_tip_h = 0;
    G32(0x8F5A) = 0;
    G32(0x8F9E) = 0;
    g_tip_cursor_hidden = 0;
    g_tip_j = 0;
    G32(0x8F96) = 0;
    g_tip_replace = 0;
    t1 = ui_hotspot_at_cursor(1);
    g_hotspot = t1;
    t2 = cursor_y();
    si = 0;
    if ((((i16)t2) >= 120 || g_cursor_shown == 1 || (g_drag_image_lo | g_drag_image_hi) != 0) && g_clock_halt == 0) {
        if (g_tip_room != g_room || g_tip_room_state != g_room_mode) {
            g_tip_id = 0;
            g_tip_room = g_room;
            g_tip_room_state = g_room_mode;
            g_tip_visible = 0;
        }
        if (((i16)(cursor_y())) > 120 && g_cursor_shown == 1) {
            if (((i16)(cursor_x())) >= 120 || ((i16)(clock_hour())) < 1 || g_clock_halt != 0) {
                if ((mouse_in_rect(0x99, 0x80, 0xC2, 0x89)) == 0) {
                    if ((mouse_in_rect(0x99, 0x94, 0xC2, 0x9E)) == 0) {
                        if ((mouse_in_rect(0x99, 0xB5, 0xC2, 0xBE)) != 0) {
                            g_tip_plain = DSP(0xE90);
                            si = 0x1388;
                            if (((u32)MK32(0x1388, g_redraw)) != ((u32)MK32(g_tip_id, 0))) {
                                t9 = str_format(g_text(81), g_day);
                                g_tip_text(0) = t9;
                            }
                            g_tip_pos = 0xC0010;
                            g_tip_w = 6;
                            g_tip_h = 1;
                        }
                    } else {
                        g_tip_pos = 0x7000C;
                        g_tip_w = 0xE;
                        g_tip_h = 6;
                        si = 0x4650;
                    }
                } else {
                    g_tip_pos = 0x90011;
                    g_tip_w = 8;
                    g_tip_h = 4;
                    si = 0x4268;
                }
            } else {
                g_tip_pos = 0x60001;
                g_tip_w = 0xF;
                g_tip_h = 7;
                si = 0x1B58;
            }
        } else if (g_room == ROOM_PLANNER) {
            if (g_room_mode == 0 || g_room_mode == 4) {
                if (g_hotspot == 0xF) {
                    t11 = (u16)((i32)(i16)(cursor_y() + 0xFFFC) / 16);
                    g_tip_k = (t11 + g_planner_scroll(0)) + 1;
                    if (schedule_program(g_room_owner, g_tip_k) != 0) {
                        t16 = sched_program_at(g_room_owner, g_tip_k);
                        t17 = cursor_x();
                        si = t16;
                        if (((i16)t17) >= 160) {
                            ax = 1;
                        } else {
                            ax = 0x15;
                        }
                        g_tip_pos = MK32(1, ax);
                    }
                }
                if (g_hotspot == 0x10) {
                    t22 = (u16)((i32)(i16)(g_mouse_y + 0xFFFC) / 16);
                    g_tip_k = (t22 + g_planner_scroll(0)) + 1;
                    if (schedule_ad(g_room_owner, g_tip_k) != 0) {
                        t34 = (u16)((i32)(i16)schedule_ad(g_room_owner, g_tip_k) % 1000);
                        g_tip_contract = 0xFFFF;
                        g_tip_j = 0;
                        si = t34 + 0xFA0;
                        while (((i16)g_player_ad_count(g_room_owner)) > ((i16)g_tip_j)) {
                            LOOP_TICK(22);
                            if ((contracts(g_room_owner, g_tip_j).ad) != ((u16)(si + 0xF060))) {
                                g_tip_j = g_tip_j + 1;
                                continue;
                            } else {
                                goto L_031C;
                            }
                        }
L_0338:
                        g_tip_pos = 0x10002;
                    }
                }
            } else if (g_room_mode == 2) {
                if (g_hotspot == 0x18) {
                    if (((i16)g_selected_genre) > 10) {
                        t58 = (u16)((i32)(i16)(g_mouse_y + 0xFFB8) / 10);
                        if (((i16)t58) < ((i16)((u16)(i8)library_count(g_room_owner, g_selected_genre)))) {
                            t64 = ((g_room_owner * 0x6C) + 0x6F19) + (g_selected_genre << 3);
                            t68 = (u16)((i32)(i16)(g_mouse_y + 0xFFB8) / 10);
                            si = G16(t64 + (t68 << 1));
                            if ((G16(t64 + (t68 << 1))) != 0) {
                                g_tip_pos = 0x10001;
                                si = si + 0x3E8;
                            }
                        }
                    } else {
                        t40 = (u16)((i32)(i16)(g_mouse_y + 0xFFB8) / 10);
                        if (((i16)t40) < ((i16)((u16)(i8)library_count(g_room_owner, g_selected_genre)))) {
                            t53 = G16((u16)(((g_room_owner * 0x6C) + 0x6F19) + (g_selected_genre << 3)) + ((u16)((u16)((i32)(i16)(g_mouse_y + 0xFFB8) / 10)) << 1));
                            g_tip_pos = 0x10001;
                            si = t53;
                        }
                    }
                }
            } else if (g_room_mode != 5) {
                if (g_room_mode == 7 && ((i16)(cursor_y())) >= 94 && ((i16)(cursor_y())) <= 104 && g_chart_image == 0) {
                    g_tip_prev_block = g_tip_block;
                    g_tip_prev_episode = g_tip_episode;
                    g_tip_i = 1;
                    while (((i16)g_tip_i) <= 7) {
                        LOOP_TICK(184);
                        t90 = cursor_x();
                        if (((i16)t90) >= ((i16)((g_tip_i * 0x1E) + 0x3F))) {
                            t91 = cursor_x();
                            if (((i16)t91) <= ((i16)((g_tip_i * 0x1E) + 0x5A))) {
                                if (g_chart_mode == 0) {
                                    t93 = G16(((g_chart_player * 0x5A) + 0x3939) + (g_tip_i << 1));
                                    g_tip_j = g_tip_i;
                                    si = t93;
                                    if (t93 == 0x270F) {
                                        for (;;) {
                                            LOOP_TICK(15);
                                            si = G16(((g_chart_player * 0x5A) + 0x3939) + (g_tip_j << 1));
                                            if ((G16(((g_chart_player * 0x5A) + 0x3939) + (g_tip_j << 1))) != 0x270F) break;
                                            g_tip_j = g_tip_j - 1;
                                        }
                                    }
                                    g_tip_episode = G16((((g_chart_player * 0x5A) + 0x3939) + (g_tip_j << 1)) + 0x1E);
                                    g_tip_viewers = G32(((g_chart_player << 5) + 0x78BE) + (g_tip_i << 2));
                                    g_tip_viewers2 = MK32(((i16)(G16(((g_chart_player * 0x1E) + 0x1C88) + (g_tip_i << 1)))) >> 0xF, G16(((g_chart_player * 0x1E) + 0x1C88) + (g_tip_i << 1)));
                                } else if (g_chart_mode == 1) {
                                    t117 = clock_hour();
                                    if (((i16)t117) > ((i16)g_tip_i)) {
L_0623:
                                        g_tip_j = g_tip_i;
                                        for (;;) {
                                            LOOP_TICK(15);
                                            si = schedule_program(g_chart_player, g_tip_j);
                                            if (schedule_program(g_chart_player, g_tip_j) != 0x270F) break;
                                            g_tip_j = g_tip_j - 1;
                                        }
                                        g_tip_episode = schedule_episode(g_chart_player, g_tip_j);
                                        g_tip_viewers = G32(((g_chart_player << 5) + 0x74EA) + (g_tip_i << 2));
                                        g_tip_viewers2 = MK32(((i16)(G16(((g_chart_player * 0x1E) + 0x1C88) + ((g_tip_i + 7) << 1)))) >> 0xF, G16(((g_chart_player * 0x1E) + 0x1C88) + ((g_tip_i + 7) << 1)));
                                    } else {
                                        t118 = clock_hour();
                                        if (t118 == g_tip_i && ((i16)g_minute) >= 5) goto L_0623;
                                    }
                                }
                                if (((i16)si) > 0) {
                                    g_tip_block = (g_tip_i - g_tip_j) + 1;
                                    g_tip_pos = 0x1000C;
                                    cc = ((u16)(si + 0x36B0)) == g_tip_id;
                                    si = si + 0x36B0;
                                    if (cc) {
                                        if (g_tip_episode != g_tip_prev_episode || g_tip_block != g_tip_prev_block) {
                                            g_redraw = 1;
                                        }
                                    } else {
                                        g_tip_replace = 1;
                                        g_redraw = 1;
                                    }
                                }
                            }
                        }
                        g_tip_i = g_tip_i + 1;
                    }
                }
            } else if (g_hotspot == 0x19) {
                t72 = (g_room_owner * 0x46) + 0x1B70;
                t76 = (u16)((i32)(i16)(g_mouse_y + 0xFFEE) / 10);
                if ((G16(t72 + (t76 * 7))) != 0) {
                    t79 = (g_room_owner * 0x46) + 0x1B70;
                    t83 = (u16)((i32)(i16)(g_mouse_y + 0xFFEE) / 10);
                    g_tip_contract = t83;
                    t85 = (G16(t79 + (t83 * 7))) + 0xFA0;
                    g_tip_pos = 0x10002;
                    si = t85;
                }
            }
        } else if (g_room == ROOM_TX_MAP) {
            if (g_room_mode == 0) {
                if (g_hotspot != 1) {
                    if (g_hotspot == 2) {
                        si = 0x9C6;
                    }
                } else {
                    si = 0x9C5;
                }
                g_tip_pos = 0x4000C;
            }
            if (g_room_mode != 1) {
                if (g_room_mode == 3) {
                    t145 = g_selected_index + 0x7D0;
                    g_tip_pos = 0x9001D;
                    si = t145;
                }
            } else {
                g_tip_pos = 0x4001D;
                t141 = cursor_x();
                si = 0x7D0;
                if (t141 != G16(0xE8C)) {
L_077B:
                    g_redraw = 1;
                } else {
                    t142 = cursor_y();
                    if (t142 != G16(0xE8E)) goto L_077B;
                }
                G16(0xE8C) = cursor_x();
                G16(0xE8E) = cursor_y();
            }
        } else if (g_room == ROOM_FILM_AGENCY) {
            if (g_room_mode == 0) {
                if ((g_tip_i = filmoffer_at_cursor()) != 0) goto L_0826;   /* --film-offers: the tapes left of slot 1, in front (film_offers.c) */
                g_tip_i = 1;
                while (((i16)g_tip_i) <= 10) {
                    LOOP_TICK(33);
                    l_4 = MK32(DSEG, (g_tip_i << 3) + 0x32E);
                    if ((mouse_in_rect(FP16(l_4, 0), FP16(l_4, 2), FP16(l_4, 4), FP16(l_4, 6))) == 0) {
                        g_tip_i = g_tip_i + 1;
                        continue;
                    } else if (((i8)(dealer(g_tip_i).shelf)) < 0) {
                        g_tip_i = g_tip_i + 1;
                        continue;
                    } else {
                        goto L_0826;
                    }
                }
L_0863:
                if (g_hotspot == 0x21) {
                    t160 = (u16)((i32)(i16)(cursor_x() + 0xFF20) / 9);
                    t163 = (u16)((i32)(i16)(cursor_y() + 0xFFE4) / 30);
                    t166 = G16(((t160 + (t163 * 5)) << 1) + 0x2483);
                    g_tip_k = G16(((t160 + (t163 * 5)) << 1) + 0x2483);
                    /* fix "case-corner": no info on a slot past the carried tapes */
                    if (t166 != 0 && (!fix_on(FIX_CASE_CORNER) || case_slot_ok(t160, t160 + (t163 * 5)))) {
                        g_tip_pos = 0x10001;
                        si = g_tip_k;
                    }
                }
            } else if (g_room_mode == 4) {
                g_tip_i = 0;
                while (((i16)g_tip_i) <= 1) {
                    LOOP_TICK(62);
                    g_tip_j = 1;
                    while (((i16)g_tip_j) <= 5) {
                        LOOP_TICK(57);
                        if (!((mouse_in_rect((g_tip_i * 0x8E) + 0x14, (g_tip_j * 0x13) + 0xFFF9, (g_tip_i * 0x8E) + 0x9E, (g_tip_j * 0x13) + 0xB)) == 0 || (auction((g_tip_i * 5) + g_tip_j).program) == 0)) {
                            t168 = auction((g_tip_i * 5) + g_tip_j).program;
                            g_tip_row = 1;
                            si = t168;
                            if (g_tip_i != 0) {
                                ax = 1;
                            } else {
                                ax = 0x15;
                            }
                            g_tip_col = ax;
                        }
                        g_tip_j = g_tip_j + 1;
                    }
                    g_tip_i = g_tip_i + 1;
                }
            }
        } else if (g_room == ROOM_AD_AGENCY) {
            if (g_nadoffers > 10) {          /* --ad-offers: the rows of folders, the mouse as the original's tip takes it (fixes.c) */
                di = g_room_mode == 0 ? adoffer_at((i16)g_mouse_x + 1, (i16)g_mouse_y) : 0;
                si = ad_offers(di).ad;
            } else if (g_hotspot != 0x3B) {
                if (g_hotspot != 0x3C) {
                    if (g_hotspot == 0x3D) {
                        t187 = (u16)((i32)(i16)(g_mouse_x + 0xFF6B) / 8);
                        si = ad_offers(t187 + 7).ad;
                        di = t187 + 7;
                    }
                } else {
                    t181 = (u16)((i32)(i16)(g_mouse_x + 0xFF85) / 8);
                    si = ad_offers(t181 + 3).ad;
                    di = t181 + 3;
                }
            } else {
                t175 = (u16)((i32)(i16)(g_mouse_x + 0xFF99) / 8);
                si = ad_offers(t175).ad;
                di = t175;
            }
            cc = ((i8)(ad_offers(di).shelf)) < 0;
            if (cc || (ad_offers(di).ad) == 0) {
                si = 0;
            }
            if (((i16)si) <= 0) {
                if (((u32)MK32(g_hotspot, g_room_mode)) == 0x210000) {
                    t190 = (u16)((i32)(i16)(cursor_x() + 0xFF1C) / 9);
                    t193 = (u16)((i32)(i16)(cursor_y() + 0xFFE0) / 32);
                    si = contracts(g_player, t190 + (t193 * 5)).ad;
                    di = t190 + (t193 * 5);
                    if ((contracts(g_player, t190 + (t193 * 5)).ad) != 0) {
                        cc = ((i16)di) < 0;
                        if (cc || ((i16)di) >= 10) {
                            si = 0;
                        } else {
                            g_tip_pos = 0x10002;
                            si = si + 0xBB8;
                        }
                    }
                }
            } else {
                g_tip_pos = 0x10017;
                si = si + 0xBB8;
            }
        } else if (g_room == ROOM_LIBRARY) {
            if (((u32)MK32(g_room_mode, g_hotspot)) == 0x21) {
                t196 = (u16)((i32)(i16)(cursor_x() + 0xFFE8) / 9);
                g_tip_k = t196;
                if (((i16)t196) < ((i16)g_bag_count)) {
                    t199 = g_bag(g_tip_k);
                    g_tip_pos = 0x10015;
                    si = t199;
                }
            }
            if (g_room_mode == 0 && g_selected_genre != 0 && g_hotspot == 0x20) {
                t201 = (u16)((i32)(i16)(cursor_x() + 0xFF60) / 9);
                g_tip_k = t201;
                if (((i16)t201) < ((i16)((u16)(i8)library_count(g_room_owner, g_selected_genre)))) {
                    si = library_items(g_room_owner, g_selected_genre, g_tip_k);
                    if (!(library_items(g_room_owner, g_selected_genre, g_tip_k) == 0 || ((i16)g_selected_genre) <= 10)) {
                        si = si + 0x3E8;
                    }
                    g_tip_pos = 0x10001;
                }
            }
            if (!(g_room_mode != 2 || (mouse_in_rect(0x1C, 0x14, 0x9A, 0xC7)) == 0)) {
                t209 = (u16)((i32)(i16)(cursor_y() + 0xFFEC) / 8);
                if (((i16)t209) < ((i16)((u16)(i8)(series(g_selected_series).episodes)))) {
                    t212 = (u16)((i32)(i16)(cursor_y() + 0xFFEC) / 8);
                    t214 = t212 + ((g_selected_series + 0x3E8) * 0xA);
                    g_tip_pos = 0x20013;
                    si = t214;
                }
            }
        } else if (g_room == ROOM_SCRIPT_AGENCY) {
            if (g_room_mode == 0) {
                g_tip_i = 1;
                while (((i16)g_tip_i) <= 5) {
                    LOOP_TICK(40);
                    if ((mouse_in_rect((g_tip_i * 7) + 0x63, 0x23, (g_tip_i * 7) + 0x69, 0x40 - g_tip_i)) == 0 || ((i8)(script_offers(g_tip_i).status)) < 0 || (script_offers(g_tip_i).script) == 0) {
                        g_tip_i = g_tip_i + 1;
                        continue;
                    } else {
                        goto L_0C99;
                    }
                }
L_0CCE:
                if (g_hotspot == 0x21) {
                    t222 = (u16)((i32)(i16)(cursor_x() + 0xFFE6) / 9);
                    si = owned_scripts(g_player, t222).script;
                    if ((owned_scripts(g_player, t222).script) != 0) {
                        g_tip_pos = 0x2000F;
                        si = si + 0x1770;
                    }
                }
            }
        } else if (g_room == ROOM_ESTATE_PLAN && g_room_mode == 0) {
            g_tip_i = 1;
            while (((i16)g_tip_i) <= 13) {
                LOOP_TICK(255);
                g_tip_j = 0;
                while (!(floors_door_kind(g_tip_i, g_tip_j) < 1 || floors_door_kind(g_tip_i, g_tip_j) > 3)) {
                    LOOP_TICK(43);
                    door_board_pos(g_tip_i, g_tip_j, HPTR(DSP(0x8F5A)), HPTR(DSP(0x8F5C)));
                    if ((mouse_in_rect(G16(0x8F5A), G16(0x8F5C), G16(0x8F5A) + 9, G16(0x8F5C) + 0xA)) != 0) {
                        goto L_0D7F;
                    } else {
                        g_tip_j = g_tip_j + 1;
                        continue;
                    }
                }
L_1028:
                if (si != 0) break;
                g_tip_i = g_tip_i + 1;
            }
        } else if (g_room == ROOM_STUDIO) {
            if (g_room_mode == 0) {
                if (((u32)MK32(g_room_owner, g_hotspot)) == ((u32)MK32(g_player, 0x21))) {
                    t244 = (u16)((i32)(i16)(cursor_x() + 0xFF06) / 9);
                    if ((owned_scripts(g_player, t244).script) == 0) {
L_10D0:
                        if (!(g_hotspot != 0x29 || (studios(g_cur_studio).script) == 0)) {
                            t252 = (studios(g_cur_studio).script) + 0x2328;
                            g_tip_pos = 0x20002;
                            si = t252;
                        }
                    } else {
                        t247 = (u16)((i32)(i16)(cursor_x() + 0xFF06) / 9);
                        t249 = (owned_scripts(g_player, t247).script) + 0x1770;
                        g_tip_pos = 0x20002;
                        si = t249;
                    }
                } else {
                    goto L_10D0;
                }
            }
        } else if (g_room != ROOM_BETTY || ((i16)(sammy_daily_update())) > 1) {
            if (g_room == ROOM_SUPERMARKET && g_room_mode == 3) {
                g_tip_i = 0;
                while (((i16)g_tip_i) < 2) {
                    LOOP_TICK(59);
                    g_tip_j = 1;
                    while (((i16)g_tip_j) <= 5) {
                        LOOP_TICK(54);
                        if ((mouse_in_rect((g_tip_j * 0x28) + 0x38, (g_tip_i * 0x24) + 0x10, (g_tip_j * 0x28) + 0x57, (g_tip_i * 0x24) + 0x27)) != 0) {
                            t263 = ((g_tip_i * 5) + g_tip_j) + 0x3E80;
                            g_tip_col = ((i16)(((i16)(((i16)((g_tip_j * 0x28) + 0x20)) >> 1)) >> 1)) >> 1;
                            si = t263;
                            if (((i16)g_tip_col) > 30) {
                                g_tip_col = 0x1E;
                            }
                            g_tip_row = ((i16)(((i16)(((i16)((g_tip_i * 0x24) + 0x28)) >> 1)) >> 1)) >> 1;
                            g_tip_w = 9;
                            g_tip_h = 5;
                        }
                        g_tip_j = g_tip_j + 1;
                    }
                    g_tip_i = g_tip_i + 1;
                }
                if (!(g_hotspot != 0x21 || g_player_present(g_player) == 0)) {
                    t267 = ((u16)(i8)g_player_present(g_player)) + 0x3E80;
                    g_tip_pos = 0x20001;
                    g_tip_w = 9;
                    g_tip_h = 5;
                    si = t267;
                }
            } else if (g_room == ROOM_NEWS) {
                if (g_room_mode == 0 && ((i16)g_hotspot) > 62 && ((i16)g_hotspot) < 66) {
                    t270 = g_hotspot + 0x146B;
                    g_tip_plain = DSP(0xEBC);
                    si = t270;
                    if (t270 != g_tip_id || g_redraw == 1) {
                        if (g_hotspot != 0x3F) {
                            ax = 0x2710;
                        } else {
                            ax = 0x3E8;
                        }
                        t275 = str_format(DSP(0xEC4) /* "#s~$ #d" */, W2(G32(((g_hotspot + 0xE3) << 2) + 0x8390)), ax);
                        g_tip_text(0) = t275;
                    }
                    g_tip_pos = 0x60002;
                    g_tip_w = 9;
                    g_tip_h = 2;
                }
            } else if (g_room == ROOM_SHOP) {
                g_tip_shop_script = shoplists(g_player, g_shop_list).script;
                g_tip_shop_episode = shoplists(g_player, g_shop_list).episode;
                if (((i16)(cursor_y())) >= 78 && ((i16)(cursor_y())) <= 101) {
                    g_tip_i = 1;
                    while (((i16)g_tip_i) <= 3) {
                        LOOP_TICK(212);
                        t287 = cursor_x();
                        if (((i16)t287) >= ((i16)((g_tip_i * 0x58) + 0xFFEC))) {
                            t288 = cursor_x();
                            if (((i16)t288) <= ((i16)((g_tip_i * 0x58) + 0xB))) {
                                if (((i8)(FP8(g_scripts, (((g_tip_shop_script * 0x1B3) + ((g_tip_shop_episode - 1) * 0x23)) + ((g_tip_i + g_shop_slot_scroll) * 3)) + 9))) <= 15) {
                                    if (g_room_mode != 1) {
                                        if ((FP8(g_scripts, (((g_tip_shop_script * 0x1B3) + ((g_tip_shop_episode - 1) * 0x23)) + ((g_tip_i + g_shop_slot_scroll) * 3)) + 0xB)) != 0) {
                                            t295 = (g_tip_i + g_shop_slot_scroll) + 0x2AF8;
                                            g_tip_contract = (u16)(i8)(FP8(g_scripts, (((g_tip_shop_script * 0x1B3) + ((g_tip_shop_episode - 1) * 0x23)) + ((g_tip_i + g_shop_slot_scroll) * 3)) + 0xB));
                                            si = t295;
                                        } else {
                                            t292 = (g_tip_i + g_shop_slot_scroll) + 0x2AF8;
                                            g_tip_contract = 0;
                                            si = t292;
                                        }
                                    } else if (((u16)((u16)(i8)(FP8(g_scripts, ((g_tip_shop_script * 0x1B3) + ((u16)(i8)(FP8(g_scripts, (((g_tip_shop_script * 0x1B3) + ((g_tip_shop_episode - 1) * 0x23)) + ((g_tip_i + g_shop_slot_scroll) * 3)) + 9)))) + 0x1A3)))) == g_shop_category) {
                                        t289 = (g_tip_i + g_shop_slot_scroll) + 0x2AF8;
                                        g_tip_contract = g_selected_program;
                                        si = t289;
                                    }
                                    if (si != 0) {
                                        g_tip_pos = 0x10008;
                                        g_tip_w = 0x12;
                                        g_tip_h = 7;
                                    }
                                } else if (((i8)(FP8(g_scripts, (((g_tip_shop_script * 0x1B3) + ((g_tip_shop_episode - 1) * 0x23)) + ((g_tip_i + g_shop_slot_scroll) * 3)) + 9))) >= 16) {
                                    if (g_room_mode != 1) {
                                        if ((FP8(g_scripts, (((g_tip_shop_script * 0x1B3) + ((g_tip_shop_episode - 1) * 0x23)) + ((g_tip_i + g_shop_slot_scroll) * 3)) + 0xB)) != 0) {
                                            t303 = (g_tip_i + g_shop_slot_scroll) + 0x2EE0;
                                            g_tip_contract = (u16)(i8)(FP8(g_scripts, (((g_tip_shop_script * 0x1B3) + ((g_tip_shop_episode - 1) * 0x23)) + ((g_tip_i + g_shop_slot_scroll) * 3)) + 0xB));
                                            g_tip_pos = 0x10008;
                                            g_tip_w = 0xD;
                                            g_tip_h = 7;
                                            si = t303;
                                        }
                                    } else if (((u16)((u16)(i8)(FP8(g_scripts, (((g_tip_shop_script * 0x1B3) + ((g_tip_shop_episode - 1) * 0x23)) + ((g_tip_i + g_shop_slot_scroll) * 3)) + 9)))) == g_shop_category) {
                                        t300 = (g_tip_i + g_shop_slot_scroll) + 0x2EE0;
                                        g_tip_contract = g_selected_program;
                                        g_tip_pos = 0x10008;
                                        g_tip_w = 0xD;
                                        g_tip_h = 7;
                                        si = t300;
                                    }
                                }
                            }
                        }
                        g_tip_i = g_tip_i + 1;
                    }
                } else if (((i16)g_shop_category) <= 7 && ((i16)(cursor_y())) >= 7 && !(((i16)(cursor_y())) > 30 || g_room_mode != 0)) {
                    g_tip_i = 1;
                    while (((i16)g_tip_i) <= 5) {
                        LOOP_TICK(26);
                        t310 = cursor_x();
                        if (((i16)t310) >= ((i16)((g_tip_i * 0x24) + 0x54))) {
                            t311 = cursor_x();
                            if (((i16)t311) <= ((i16)((g_tip_i * 0x24) + 0x73))) {
                                t312 = (g_tip_i + g_shop_actor_scroll) + 0x32C8;
                                g_tip_pos = 0x10001;
                                si = t312;
                            }
                        }
                        g_tip_i = g_tip_i + 1;
                    }
                }
            }
        } else if (g_hotspot == 0x4A) {
            g_tip_plain = DSP(0xEB0);
            si = 0x1388;
            if (((u32)MK32(0x1388, g_redraw)) != ((u32)MK32(g_tip_id, 0))) {
                t258 = str_format(g_text(375), W2(G32(((g_sammy_category(0) + 0x153) << 2) + 0x8390)));
                g_tip_text(0) = t258;
            }
            g_tip_pos = 0x20015;
            g_tip_w = 0x11;
            g_tip_h = 3;
        }
        tooltip_show(si);
    }
    return;
L_031C:
    g_tip_contract = g_tip_j;
    goto L_0338;
L_0826:
    if (((i16)g_tip_i) > 8 && ((i16)g_tip_i) <= 10) {   /* the back right tapes: the tip on the left; slots 11.. stand left (--film-offers) */
        ax = 1;
    } else {
        ax = 0x15;
    }
    g_tip_pos = MK32(1, ax);
    si = dealer(g_tip_i).program;
    goto L_0863;
L_0C99:
    t218 = (script_offers(g_tip_i).script) + 0x1770;
    g_tip_pos = 0x20014;
    si = t218;
    goto L_0CCE;
L_0D7F:
    si = ((g_tip_i * 0xA) + g_tip_j) + 0x1388;
    if (((i16)g_tip_i) > 7) {
        g_tip_col = 0x16;
    } else {
        g_tip_col = 2;
    }
    g_tip_row = 1;
    g_tip_w = 0x10;
    g_tip_h = 7;
    g_tip_plain = DSP(0xE94);
    if (((u32)MK32(si, g_redraw)) != ((u32)MK32(g_tip_id, 0))) {
        g_tip_k = floors_door_studio(g_tip_i, g_tip_j);
        if (g_tip_k != 0) {
            if (((i8)(studios(g_tip_k).reserved)) > 0) {
                if (((u16)((u16)(i8)(studios(g_tip_k).reserved))) != g_player) {
                    t232 = str_format(g_text(114), W2(players((u16)(i8)(studios(g_tip_k).reserved)).station), (u16)(i8)(studios(g_tip_k).size));
                    g_tip_text(0) = t232;
                    goto L_1028;
                } else {
                    t230 = str_format(g_text(113), (u16)(i8)(studios(g_tip_k).size));
                    g_tip_text(0) = t230;
                    goto L_1028;
                }
            } else if ((studios(g_tip_k).renter) != 0xFF) {
                if ((studios(g_tip_k).renter) != 0) {
                    if (((u16)((u16)(i8)(studios(g_tip_k).renter))) != g_player) {
                        t242 = text_dup(0x6C);
                        g_tip_text(0) = t242;
                        goto L_1028;
                    } else {
                        t240 = str_format(g_text(111), W2(studios(g_tip_k).rent), (u16)(i8)(studios(g_tip_k).size));
                        g_tip_text(0) = t240;
                        goto L_1028;
                    }
                } else {
                    t239 = text_dup(0x6C);
                    g_tip_text(0) = t239;
                    goto L_1028;
                }
            } else {
                t234 = (u16)(i8)(studios(g_tip_k).size);
                t236 = (u32)((i32)((u32)(studios(g_tip_k).rent * 3)) / (i32)2);
                t237 = str_format(g_text(109), W2(t236), t234);
                g_tip_text(0) = t237;
                goto L_1028;
            }
        } else {
            t229 = text_dup(0x6C);
            g_tip_text(0) = t229;
            goto L_1028;
        }
    }
    goto L_1028;
}

/* 14E1:16BB  draw info card for id unless already shown; kind = id/1000: 0 film, 1 series, 2 transmitter (new viewers/price), 3 ad offer, 4 ad contract (bonus/penalty/min viewers, spots done/left until deadline), 5 plain text, 6 screenplay, 7 channel status, 8-9 production, 10 episode, 11 actor (humor/appearance/skill, price), 12 prop/scene, 13 role, 14/15 schedule film/series slot (block x of y, viewers, share), 16 price entry, 17 money/credit, 18 rating overview (max viewers, rating, next advert) */
void tooltip_show(u16 a0) {
    u16 ax;
    u16 bx;
    int cc;
    int sertop = 0;                 /* --series-topicality: this series card shows the bar (fixes.c) */
    u16 di;
    u16 dx;
    u16 es;
    u16 s0;
    u16 s1;
    u16 s2;
    u16 s3;
    u16 si;
    u16 l_2;
    u32 l_6;
    u32 l_A;
    u32 l_E;
    u32 l_12;
    u32 l_16;
    u32 l_1A;
    u32 l_1E;
    u16 t1;
    u16 t5;
    u32 t7;
    u32 t9;
    u32 t17;
    u32 t18;
    u32 t19;
    u32 t20;
    u16 t22;
    u16 t23;
    u32 t24;
    u32 t25;
    u16 t26;
    u16 t27;
    u32 t29;
    u16 t30;
    u16 t31;
    u32 t34;
    u32 t35;
    u32 t36;
    u32 t37;
    u16 t40;
    u16 t41;
    u32 t42;
    u32 t44;
    u32 t51;
    u16 t52;
    u16 t53;
    u32 t54;
    u32 t55;
    u32 t56;
    u16 t57;
    u16 t58;
    u32 t59;
    u32 t61;
    u16 t63;
    u16 t68;
    u32 t71;
    u32 t72;
    u32 t73;
    u32 t74;
    u32 t75;
    u16 t77;
    u32 t84;
    u32 t91;
    u32 t92;
    u32 t94;
    u32 t101;
    u32 t104;
    u32 t105;
    u16 t106;
    u16 t107;
    u32 t108;
    u16 t109;
    u16 t110;
    u32 t111;
    u32 t120;
    u32 t123;
    u32 t126;
    u32 t131;
    u32 t134;
    u32 t135;
    u32 t137;
    u32 t140;
    u16 t143;
    u16 t144;
    u32 t145;
    u32 t146;
    u32 t159;
    u32 t160;
    u32 t169;
    u16 t172;
    u16 t173;
    u32 t174;
    u32 t175;
    u32 t176;
    u32 t179;
    u16 t180;
    u16 t181;
    u32 t182;
    u32 t183;
    u16 t184;
    u16 t185;
    u32 t186;
    u32 t187;
    u16 t188;
    u16 t189;
    u32 t190;
    u16 t191;
    u16 t192;
    u16 t193;
    u16 t194;
    u16 t195;
    u16 t196;
    u16 t202;
    u16 t203;
    u16 t205;
    u16 t213;
    u16 t221;
    u32 t223;
    u16 t224;
    u16 t225;
    u16 t226;
    u16 t229;
    u16 t230;
    u16 t231;
    u16 t232;
    u16 t233;
    u16 t234;
    u16 t239;
    u16 t240;
    u32 t241;
    u32 t244;
    u32 t247;
    u32 t248;
    u32 t249;
    u16 t251;
    u32 t254;
    u16 t255;
    u32 t256;
    u32 t259;
    u16 t261;
    u32 t264;
    u32 t265;
    u32 t266;
    u16 t267;
    u32 t268;
    u32 t271;
    u32 t285;
    u16 t286;
    u16 t287;
    u32 t288;
    u32 t289;
    u16 t290;
    u16 t291;
    u16 t298;
    u16 t301;
    u16 t302;
    u16 t307;
    u16 t309;
    u16 t314;
    u16 t316;
    u16 t317;
    u16 t322;
    u16 t325;
    u16 t326;
    u32 t329;
    u32 t331;
    u32 t332;
    u16 t333;
    u16 t335;
    u32 t336;
    u32 t337;
    u16 t339;
    u32 t341;
    u32 t342;
    u16 t343;
    u16 t344;
    u16 t345;
    u16 t346;
    u16 t347;
    u16 t348;
    u32 t349;
    u32 t350;
    u32 t352;
    FUNC_TICK(2416);
    clip_reset();
    si = a0;
    if (a0 != 0) {
        if (!((g_tip_id == 0 || !(g_tip_row != g_tip_shown_row || g_tip_col != g_tip_shown_col)) && g_tip_replace == 0)) {
            present_frame();
            g_tip_visible = 0;
            g_tip_replace = 0;
        }
        cc = si != g_tip_id;
        if (cc || g_tip_row != g_tip_shown_row || g_tip_col != g_tip_shown_col || g_redraw == 1) {
            g_tip_visible = 1;
            g_redraw = 0;
            if (((u32)MK32(g_room, g_room_mode)) != 0x190001) {
                cursor_hide();
                g_tip_cursor_hidden = 1;
            }
            t1 = (u16)((i32)(i16)si / 1000);
            ax = t1;
            if (t1 <= 0x12) {
                switch (ax) {
                case 0:
                    tooltip_film(si);
                    break;
                case 1:
                    g_tip_w = 0x11;
                    g_tip_h = 0xC;
                    t5 = (u16)((i32)(i16)si % 1000);
                    l_1E = series_at(t5);
                    t7 = series_title(t5);
                    g_tip_text(0) = t7;
                    t9 = series_description(t5);
                    g_tip_text1 = t9;
                    g_tip_text2 = G32(((((u16)(i8)(AT(series, l_1E).genre)) + 0x124) << 2) + 0x8390);
                    t17 = str_format(g_text(376), ((u16)(i8)(AT(series, l_1E).critics)) << 2, ((u16)(i8)(AT(series, l_1E).press)) << 2);
                    g_tip_text3 = t17;
                    di = t5;
                    if ((AT(series, l_1E).next_ep) == 0) {
                        AT(series, l_1E).next_ep = SGENRE_FAMILY;
                    }
                    t18 = series_episode_title(di, AT(series, l_1E).next_ep);
                    t19 = str_format(g_text(377), (u16)(i8)(AT(series, l_1E).episodes), (u16)(i8)(AT(series, l_1E).blocks), AT(series, l_1E).next_ep, W2(t18));
                    g_tip_text4 = t19;
                    if (((u16)(((i16)((AT(series, l_1E).flags2_16) << 0xE)) >> 0xF)) == 0) {
                        bx = 0xECC;
                        es = DSEG;
                    } else {
                        bx = FPOFF(g_text(39));
                        es = FPSEG(g_text(39));
                    }
                    g_tip_text5 = MK32(es, bx);
                    t20 = series_title(di);
                    if ((text_count_lines(t20, 0x10, 1)) == 0) {
                        ax = 0xEFE;
                        dx = DSEG;
                    } else {
                        ax = 0xECE;
                        dx = DSEG;
                    }
                    sertop = series_top_on();
                    if (sertop) {   /* --series-topicality: the series' topicality on a band of its own (fixes.c) */
                        u32 layout;
                        u16 rows;
                        g_tip_text6 = str_format(g_text(417), series_top_bar(l_1E));
                        layout = series_top_layout(MK32(dx, ax), text_count_lines(g_tip_text4, 0x10, 1), &rows);
                        g_tip_h = rows;
                        tooltip_render(layout);
                        host_handle_release(layout);
                    } else {
                        tooltip_render(MK32(dx, ax));
                    }
                    t22 = g_popup_bmp_hi(0);
                    t23 = g_popup_bmp_lo(0);
                    t24 = series_price(di);
                    t25 = str_format(DSP(0xF2E) /* "$ #l" */, W2(t24));
                    text_draw_right_free(0x88, 0x3C, LO16(t25), HI16(t25), t23, t22);
                    tooltip_present();
                    mem_free(g_tip_text3_lo, g_tip_text3_hi);
                    mem_free(g_tip_text4_lo, g_tip_text4_hi);
                    if (sertop) mem_free(g_tip_text6_lo, g_tip_text6_hi);
                    break;
                case 2:
                    if (si == 0x7D0) {
                        g_tip_w = 0xA;
                        g_tip_h = 4;
                        t26 = cursor_y();
                        t27 = cursor_x();
                        t29 = (u32)(transmitter_new_reach(t27 + 0xFFD0, t26 + 0xFFFC) * 0x9A0);
                        g_tip_viewers = t29;
                        t30 = cursor_y();
                        t31 = cursor_x();
                        t34 = (u32)((i32)((u32)(transmitter_reach(t31 + 0xFFD0, t30 + 0xFFFC) * 0x268)) / (i32)0xA);
                        t35 = transmitter_fee_bracket(LO16(t34), HI16(t34));
                        t36 = format_millions(g_tip_viewers, 3);
                        g_tip_text1 = t36;
                        t37 = str_format(g_text(378), W2(t36), W2(t35));
                        g_tip_text(0) = t37;
                        mem_free(g_tip_text1_lo, g_tip_text1_hi);
                        tooltip_render(DSP(0xF33));
                    } else if (((i16)si) > 2000) {
                        if (((i16)si) <= 2500) {
                            g_tip_w = 0xA;
                            g_tip_h = 4;
                            g_tip_viewers = ((u32)(G32((((g_room_owner * 0x1CE) + 0x1D09) + ((si + 0xF830) * 0xE)) + 4) * 0x9A0));
                            t40 = G16((((g_room_owner * 0x1CE) + 0x1D09) + ((si + 0xF830) * 0xE)) + 2);
                            t41 = G16(((g_room_owner * 0x1CE) + 0x1D09) + ((si + 0xF830) * 0xE));
                            t42 = format_millions(g_tip_viewers, 3);
                            g_tip_text1 = t42;
                            t44 = str_format(g_text(379), W2(t42), t41, t40);
                            g_tip_text(0) = t44;
                            mem_free(g_tip_text1_lo, g_tip_text1_hi);
                            tooltip_render(DSP(0xF43));
                        } else {
L_1AD4:
                            if (((i16)si) > 2500) {
                                if (((i16)si) <= 2600) {
                                    g_tip_w = 0xA;
                                    g_tip_viewers = ((u32)(G32(((si + 0xF63B) << 2) + 0x7AB6) * 0x9A0));
                                    g_tip_viewers2 = ((u32)(G32(((si + 0xF642) << 2) + 0x7AB6) * 0x9A0));
                                    if (((G16((((g_map_player * 0x1CE) + 0x1D09) + ((si + 0xF63C) * 0xE)) + 4)) | (G16((((g_map_player * 0x1CE) + 0x1D09) + ((si + 0xF63C) * 0xE)) + 6))) == 0) {
                                        g_tip_h = 6;
                                        t51 = g_tip_viewers - g_tip_viewers2;
                                        t52 = G16((((g_room_owner * 0x1CE) + 0x1D09) + ((si + 0xF63C) * 0xE)) + 2);
                                        t53 = G16(((g_room_owner * 0x1CE) + 0x1D09) + ((si + 0xF63C) * 0xE));
                                        t54 = format_millions(t51, 3);
                                        g_tip_text2 = t54;
                                        t55 = format_millions(g_tip_viewers, 3);
                                        g_tip_text1 = t55;
                                        t56 = str_format(g_text(380), W2(t55), W2(t54), t53, t52);
                                        g_tip_text(0) = t56;
                                        mem_free(g_tip_text1_lo, g_tip_text1_hi);
                                        mem_free(g_tip_text2_lo, g_tip_text2_hi);
                                        tooltip_render(DSP(0xF53));
                                    } else {
                                        g_tip_h = 4;
                                        t57 = G16((((g_room_owner * 0x1CE) + 0x1D09) + ((si + 0xF63C) * 0xE)) + 2);
                                        t58 = G16(((g_room_owner * 0x1CE) + 0x1D09) + ((si + 0xF63C) * 0xE));
                                        t59 = format_millions(g_tip_viewers, 3);
                                        g_tip_text1 = t59;
                                        t61 = str_format(g_text(381), W2(t59), t58, t57);
                                        g_tip_text(0) = t61;
                                        mem_free(g_tip_text1_lo, g_tip_text1_hi);
                                        tooltip_render(DSP(0xF6B));
                                    }
                                }
                            }
                        }
                    } else {
                        goto L_1AD4;
                    }
                    tooltip_present();
                    mem_free(g_tip_text_lo(0), g_tip_text_hi(0));
                    break;
                case 3:
                case 4:
                    t63 = (u16)((i32)(i16)si % 1000);
                    l_16 = ads_at(t63);
                    ad_audience_text(t63);
                    g_tip_w = 0xF;
                    g_tip_h = 0xB;
                    g_tip_text1 = AT(ad, l_16).name;
                    g_tip_text2 = AT(ad, l_16).client;
                    s0 = G16(((((u16)(i8)(AT(ad, l_16).min_class)) - 1) << 2) + 0xAA);
                    s1 = G16(((((u16)(i8)(AT(ad, l_16).min_class)) - 1) << 2) + 0xA8);
                    if (g_room != ROOM_AD_AGENCY) {
                        ax = g_room_owner;
                    } else {
                        ax = g_player;
                    }
                    t68 = viewers_to_pct(ax, MK32(s0, s1));
                    t71 = format_millions(G32(((((u16)(i8)(AT(ad, l_16).min_class)) - 1) << 2) + 0xA8), 3);
                    g_tip_text4 = t71;
                    t72 = str_format(g_text(382), W2(AT(ad, l_16).fee), W2(AT(ad, l_16).penalty), W2(t71), t68);
                    g_tip_text3 = t72;
                    mem_free(g_tip_text4_lo, g_tip_text4_hi);
                    if (((i16)si) > 4000) {
                        if (((i16)g_tip_contract) >= 0) {
                            l_2 = (contracts(g_room_owner, g_tip_contract).deadline) - g_day;
                            s0 = l_2;
                            s1 = (u16)(i8)(AT(ad, l_16).spots);
                            s2 = ((u16)(i8)(AT(ad, l_16).spots)) - ((u16)(i8)(contracts(g_room_owner, g_tip_contract).aired));
                            if (l_2 != 0) {
                                if (l_2 != 1) {
                                    ax = G16(0x898C);
                                    dx = G16(0x898E);
                                } else {
                                    ax = G16(0x8A30);
                                    dx = G16(0x8A32);
                                }
                            } else {
                                ax = G16(0x8A2C);
                                dx = G16(0x8A2E);
                            }
                            t73 = str_format(MK32(dx, ax), s2, s1, s0);
                            g_tip_text4 = t73;
                        } else {
                            t74 = text_dup(0x180);
                            g_tip_text4 = t74;
                        }
                    } else {
                        t75 = str_format(g_text(385), (u16)(i8)(AT(ad, l_16).spots), (u16)(i8)(AT(ad, l_16).days));
                        g_tip_text4 = t75;
                    }
                    tooltip_render(DSP(0xF7B));
                    mem_free(g_tip_text_lo(0), g_tip_text_hi(0));
                    mem_free(g_tip_text3_lo, g_tip_text3_hi);
                    mem_free(g_tip_text4_lo, g_tip_text4_hi);
                    tooltip_present();
                    break;
                case 5:
                    tooltip_render(g_tip_plain);
                    tooltip_present();
                    mem_free(g_tip_text_lo(0), g_tip_text_hi(0));
                    break;
                case 6:
                    g_tip_w = 0x12;
                    g_tip_h = 8;
                    t77 = (u16)((i32)(i16)si % 1000);
                    l_12 = screenplays_at(t77);
                    g_tip_i = 0;
                    di = t77;
                    while (((i16)g_scripts_owned_count(g_room_owner)) > ((i16)g_tip_i)) {
                        LOOP_TICK(20);
                        if ((owned_scripts(g_room_owner, g_tip_i).script) == di) break;
                        g_tip_i = g_tip_i + 1;
                    }
                    l_E = owned_scripts_at(g_room_owner, g_tip_i);
                    if (((u16)(((i16)(FP16(l_12, 5) << 0xF)) >> 0xF)) == 0) {
                        l_1A = films_at(FP16(l_12, 6));
                        t84 = movie_title(FP16(l_12, 6));
                        g_tip_text(0) = t84;
                        g_tip_text1 = G32(((((u16)(i8)(AT(film, l_1A).genre)) + 0x1A) << 2) + 0x8390);
                        g_tip_viewers = MK32(((i16)((u16)(i8)(AT(film, l_1A).blocks))) >> 0xF, (u16)(i8)(AT(film, l_1A).blocks));
                        if (((u32)(MK32(((i16)((films(FP16(l_12, 6)).flags2_16) << 0xD)) >> 0xF, g_room))) == 5) {
                            t91 = str_format(g_text(386), (AT(owned_script, l_E).episode) - 1);
                            g_tip_text2 = t91;
                        } else {
                            t92 = str_format(DSP(0xFA7));
                            g_tip_text2 = t92;
                        }
                    } else {
                        l_1E = series_at(FP16(l_12, 6));
                        t94 = series_title(FP16(l_12, 6));
                        g_tip_text(0) = t94;
                        g_tip_text1 = G32(((((u16)(i8)(AT(series, l_1E).genre)) + 0x124) << 2) + 0x8390);
                        g_tip_viewers = MK32(((i16)((u16)(i8)(AT(series, l_1E).blocks))) >> 0xF, (u16)(i8)(AT(series, l_1E).blocks));
                        if (g_room != ROOM_STUDIO) {
                            t104 = str_format(g_text(388), (u16)(i8)(AT(series, l_1E).episodes));
                            g_tip_text2 = t104;
                        } else {
                            t101 = str_format(g_text(387), (AT(owned_script, l_E).episode) - 1, (u16)(i8)(series(FP16(l_12, 6)).episodes));
                            g_tip_text2 = t101;
                        }
                    }
                    t105 = script_title(di);
                    g_tip_text3 = t105;
                    t106 = g_tip_viewers_lo;
                    t107 = script_count_scenes(di);
                    t108 = str_format(g_text(389), t107, t106);
                    g_tip_text4 = t108;
                    tooltip_render(DSP(0xFA9));
                    t109 = g_popup_bmp_hi(0);
                    t110 = g_popup_bmp_lo(0);
                    t111 = str_format(DSP(0xFC9) /* "$ #l" */, W2(FP32(l_12, 0)));
                    text_draw_right_free(0x88, 0x24, LO16(t111), HI16(t111), t110, t109);
                    tooltip_present();
                    mem_free(g_tip_text2_lo, g_tip_text2_hi);
                    mem_free(g_tip_text4_lo, g_tip_text4_hi);
                    break;
                case 7:
                    if (g_now_title_stale == 1) {
                        tvinfo_fetch_titles();
                    }
                    g_tip_w = 0x10;
                    window_create(g_tip_col, g_tip_row, g_tip_col + 0x10, g_tip_row + 6);
                    channel_status_draw(g_popup_bmp(0));
                    tooltip_present();
                    break;
                case 8:
                    break;
                case 9:
                    g_tip_w = 0x14;
                    g_tip_h = 9;
                    l_A = studios_at(g_cur_studio);
                    l_12 = MK32(FPSEG(g_scripts), FPOFF(g_scripts) + ((si + 0xDCD8) * 0x1B3));
                    g_tip_i = 2;
                    while (((i16)g_tip_i) < 7) {
                        LOOP_TICK(8);
                        G32((u16)(g_tip_i << 2) + 0x8F6C) = 0;
                        g_tip_i = g_tip_i + 1;
                    }
                    if (((u16)(((i16)(FP16(l_12, 5) << 0xF)) >> 0xF)) == 0) {
                        l_1A = films_at(FP16(l_12, 6));
                        t120 = movie_title(FP16(l_12, 6));
                        g_tip_text(0) = t120;
                        if (((u16)(((i16)((AT(film, l_1A).flags2_16) << 0xD)) >> 0xF)) != 0) {
                            t123 = str_format(g_text(390), ((u16)(i8)(AT(studio, l_A).episode)) - 1);
                            g_tip_text1 = t123;
                            g_tip_k = 2;
                            g_tip_i = (u16)(i8)(AT(studio, l_A).episode);
                            while (((i16)((u16)(i8)(AT(studio, l_A).episode_last))) >= ((i16)g_tip_i)) {
                                LOOP_TICK(35);
                                if ((script_items_bought(AT(studio, l_A).script, g_tip_i)) != 1) {
                                    g_tip_plain = DSP(0xFCE);
                                } else {
                                    g_tip_plain = g_text(362);
                                }
                                t126 = str_format(DSP(0xFD4) /* "#s #01d #s~" */, W2(g_text(142)), g_tip_i, W2(g_tip_plain));
                                G32((u16)(g_tip_k << 2) + 0x8F6C) = t126;
                                g_tip_k = g_tip_k + 1;
                                g_tip_i = g_tip_i + 1;
                            }
                        } else {
                            t131 = str_format(DSP(0xFE0));
                            g_tip_text1 = t131;
                            if ((AT(studio, l_A).episode_last) != 1) {
                                t135 = text_dup(0x90);
                                g_tip_text2 = t135;
                            } else {
                                if ((script_items_bought(AT(studio, l_A).script, (u16)(i8)(AT(studio, l_A).episode))) != 1) {
                                    g_tip_plain = DSP(0xFE1);
                                } else {
                                    g_tip_plain = g_text(362);
                                }
                                t134 = str_format(g_text(143), W2(g_tip_plain));
                                g_tip_text2 = t134;
                            }
                        }
                    } else {
                        l_1E = series_at(FP16(l_12, 6));
                        t137 = series_title(FP16(l_12, 6));
                        g_tip_text(0) = t137;
                        t140 = str_format(g_text(391), ((u16)(i8)(AT(studio, l_A).episode)) - 1, (u16)(i8)(AT(series, l_1E).episodes));
                        g_tip_text1 = t140;
                        g_tip_k = 2;
                        g_tip_i = (u16)(i8)(AT(studio, l_A).episode);
                        while (((i16)((u16)(i8)(AT(studio, l_A).episode_last))) >= ((i16)g_tip_i)) {
                            LOOP_TICK(40);
                            if ((script_items_bought(AT(studio, l_A).script, g_tip_i)) != 1) {
                                g_tip_plain = DSP(0xFE7);
                            } else {
                                g_tip_plain = g_text(362);
                            }
                            t143 = g_tip_plain_hi;
                            t144 = g_tip_plain_lo;
                            t145 = series_episode_title(FP16(l_12, 6), g_tip_i);
                            t146 = str_format(DSP(0xFED) /* "#d. #*13s #s" */, g_tip_i, W2(t145), t144, t143);
                            G32((u16)(g_tip_k << 2) + 0x8F6C) = t146;
                            g_tip_k = g_tip_k + 1;
                            g_tip_i = g_tip_i + 1;
                        }
                    }
                    tooltip_render(DSP(0xFFA));
                    tooltip_present();
                    mem_free(g_tip_text1_lo, g_tip_text1_hi);
                    g_tip_i = 2;
                    while (((i16)g_tip_i) < 7) {
                        LOOP_TICK(16);
                        if ((g_tip_text_lo(g_tip_i) | g_tip_text_hi(g_tip_i)) != 0) {
                            mem_free(g_tip_text_lo(g_tip_i), g_tip_text_hi(g_tip_i));
                        }
                        g_tip_i = g_tip_i + 1;
                    }
                    break;
                case 0xA:
                    g_tip_w = 0x13;
                    g_tip_h = 6;
                    g_tip_k = (u16)((u16)((i32)(i16)si / 10)) + 0xFC18;
                    g_tip_i = ((si + 0xD8F0) - (g_tip_k * 0xA)) + 1;
                    t159 = series_episode_title(g_tip_k, g_tip_i);
                    g_tip_text(0) = t159;
                    t160 = series_episode_description(g_tip_k, g_tip_i);
                    g_tip_text1 = t160;
                    tooltip_render(DSP(0x101E));
                    tooltip_present();
                    break;
                case 0xB:
                    l_12 = screenplays_at(g_tip_shop_script);
                    window_create(g_tip_col, g_tip_row, g_tip_col + g_tip_w, g_tip_row + g_tip_h);
                    g_tip_k = (u16)(i8)(FP8(l_12, ((u16)(i8)(FP8(l_12, (((g_tip_shop_episode - 1) * 0x23) + ((si + 0xD508) * 3)) + 9))) + 0x1A3));
                    l_6 = MK32(DSEG, (((g_tip_k - 1) * 0xC0) + 0x5013) + (g_tip_contract * 0xC));
                    g_tip_j = (u16)(i8)(FP8(l_12, ((g_tip_shop_episode - 1) * 0x23) + 0xA));
                    g_tip_block = ((u16)(i8)(FP8(l_12, (((g_tip_shop_episode - 1) * 0x23) + ((si + 0xD508) * 3)) + 9))) - 1;
                    if (((u16)(((i16)(FP16(l_12, 5) << 0xF)) >> 0xF)) != 0) {
                        s0 = g_popup_bmp_hi(0);
                        s1 = g_popup_bmp_lo(0);
                        s2 = 1;
                        s3 = 0xC;
                        if (g_tip_contract != 0) {
                            t169 = actor_text(FP16(l_6, 0), 2);
                            ax = LO16(t169);
                            dx = HI16(t169);
                        } else {
                            ax = G16(0x89B0);
                            dx = G16(0x89B2);
                        }
                        t172 = G16(((g_tip_k + 0x91) << 2) + 0x8392);
                        t173 = G16(((g_tip_k + 0x91) << 2) + 0x8390);
                        t174 = script_role_name(g_tip_shop_script, g_tip_block);
                        t175 = str_format(DSP(0x1036) /* "#s~(#s):~#s" */, W2(t174), t173, t172, ax, dx);
                        text_draw_ex_free(8, 0xFFFC, LO16(t175), HI16(t175), s3, s2, s1, s0);
                    } else {
                        s0 = g_popup_bmp_hi(0);
                        s1 = g_popup_bmp_lo(0);
                        s2 = 1;
                        s3 = 7;
                        if (g_tip_contract != 0) {
                            t176 = actor_text(FP16(l_6, 0), 2);
                            ax = LO16(t176);
                            dx = HI16(t176);
                        } else {
                            ax = G16(0x89B0);
                            dx = G16(0x89B2);
                        }
                        t179 = str_format(DSP(0x1042) /* "#s:~#s" */, W2(G32(((g_tip_k + 0x91) << 2) + 0x8390)), ax, dx);
                        text_draw_ex_free(8, 0xFFFC, LO16(t179), HI16(t179), s3, s2, s1, s0);
                    }
                    if (((i16)g_tip_contract) > 0) {
                        if (g_room_mode != 0) {
                            t184 = g_popup_bmp_hi(0);
                            t185 = g_popup_bmp_lo(0);
                            t186 = (u32)(MK32(((i16)g_tip_j) >> 0xF, g_tip_j) * FP32(l_6, 2));
                            t187 = str_format(g_text(393), W2(t186));
                            text_draw_ex_free(8, 0x1D, LO16(t187), HI16(t187), 7, 1, t185, t184);
                        } else {
                            t180 = g_popup_bmp_hi(0);
                            t181 = g_popup_bmp_lo(0);
                            t182 = (u32)(MK32(((i16)g_tip_j) >> 0xF, g_tip_j) * FP32(l_6, 2));
                            t183 = str_format(g_text(393), W2(t182));
                            text_draw_ex_free(8, 0x1D, LO16(t183), HI16(t183), 7, 1, t181, t180);
                        }
                        text_draw_to(0x3D, 0x1D, G16(0x893C), G16(0x893E), g_popup_bmp_lo(0), g_popup_bmp_hi(0));
                    } else {
                        t188 = g_popup_bmp_hi(0);
                        t189 = g_popup_bmp_lo(0);
                        t190 = str_format(DSP(0x1049), W2(g_text(334)));
                        text_draw_ex_free(8, 0x1D, LO16(t190), HI16(t190), 0xF, 1, t189, t188);
                    }
                    if (((i16)g_tip_contract) > 0) {
                        if (((i16)FP16(l_6, 0xA)) > 15) {
                            t194 = g_popup_bmp_hi(0);
                            t195 = g_popup_bmp_lo(0);
                            t196 = col_to_px(FP16(l_6, 0xA) + 0xFFF1);
                            blit_opaque_rect(MK32(g_room_gfx3_hi, g_room_gfx3_lo + t196), MK32(t194, t195), 0x69, 6, 0, 0, 0x20, 0x14, 0 /* unused ax */);
                        } else {
                            t191 = g_popup_bmp_hi(0);
                            t192 = g_popup_bmp_lo(0);
                            t193 = col_to_px(FP16(l_6, 0xA));
                            blit_opaque_rect(MK32(g_room_gfx_hi, g_room_gfx_lo + t193), MK32(t191, t192), 0x69, 6, 0, 0, 0x20, 0x14, 0 /* unused ax */);
                        }
                        blit_transparent(MK32(g_room_gfx2_hi, g_room_gfx2_lo + 0x4A), g_popup_bmp(0), 0x69, 4, 0, 0, 0x20, 0x18, 0 /* unused ax */);
                        t202 = g_popup_bmp_hi(0);
                        t203 = g_popup_bmp_lo(0);
                        t205 = (u16)((i32)(i16)(((u16)(i8)FP8(l_6, 9)) << 3) / 5);
                        t213 = (u16)((i32)(i16)(((u16)(i8)FP8(l_6, 8)) << 3) / 5);
                        t221 = (u16)((i32)(i16)(((u16)(i8)FP8(l_6, 6)) << 3) / 5);
                        t223 = str_format(DSP(0x104C) /* "#32f~#32f~#32f" */, t221, t213, t205);
                        text_draw_to_free(0x69, 0x1D, LO16(t223), HI16(t223), t203, t202);
                    } else {
                        t224 = g_popup_bmp_hi(0);
                        t225 = g_popup_bmp_lo(0);
                        t226 = col_to_px(g_tip_k);
                        draw_sprite_to(MK32(g_room_gfx2_hi, g_room_gfx2_lo + t226), t225, t224, 0x69, 6);
                        draw_sprite_to(MK32(g_room_gfx2_hi, g_room_gfx2_lo + 0x4A), g_popup_bmp_lo(0), g_popup_bmp_hi(0), 0x69, 4);
                    }
                    tooltip_present();
                    break;
                case 0xC:
                    g_tip_k = (u16)(i8)(FP8(g_scripts, (((g_tip_shop_script * 0x1B3) + ((g_tip_shop_episode - 1) * 0x23)) + ((si + 0xD120) * 3)) + 9));
                    window_create(g_tip_col, g_tip_row, g_tip_col + g_tip_w, g_tip_row + g_tip_h);
                    t229 = g_popup_bmp_hi(0);
                    t230 = g_popup_bmp_lo(0);
                    t231 = col_to_px(g_tip_k + 0xFFF1);
                    blit_opaque_to(MK32(g_room_gfx1_hi, g_room_gfx1_lo + t231), t230, t229, 0x27, 6);
                    if (g_tip_contract != 1) {
                        if (g_tip_contract != 2) {
                            if (g_tip_contract == 3) {
                                g_tip_episode = 2;
                            }
                        } else {
                            g_tip_episode = 1;
                        }
                    } else {
                        g_tip_episode = 3;
                    }
                    t232 = g_popup_bmp_hi(0);
                    t233 = g_popup_bmp_lo(0);
                    t234 = col_to_px(g_tip_episode + 9);
                    draw_sprite_to(MK32(g_room_gfx2_hi, g_room_gfx2_lo + t234), t233, t232, 0x27, 4);
                    draw_sprite_to(MK32(g_room_gfx2_hi, g_room_gfx2_lo + 0x82), g_popup_bmp_lo(0), g_popup_bmp_hi(0), 0xC, 0x1C);
                    text_draw_center_to(0x38, 0x15, G16(((g_tip_k + 0xC0) << 2) + 0x8390), G16(((g_tip_k + 0xC0) << 2) + 0x8392), g_popup_bmp_lo(0), g_popup_bmp_hi(0));
                    t239 = g_popup_bmp_hi(0);
                    t240 = g_popup_bmp_lo(0);
                    t241 = (u32)(MK32(((i16)((u16)(i8)(FP8(g_scripts, ((g_tip_shop_script * 0x1B3) + ((g_tip_shop_episode - 1) * 0x23)) + 0xA)))) >> 0xF, (u16)(i8)(FP8(g_scripts, ((g_tip_shop_script * 0x1B3) + ((g_tip_shop_episode - 1) * 0x23)) + 0xA))) * G32((((g_tip_k + 0xFFF1) << 4) + 0x6235) + (g_tip_contract << 2)));
                    t244 = str_format(g_text(394), W2(G32(((g_tip_contract + 0xEC) << 2) + 0x8390)), W2(t241));
                    text_draw_to_free(0xE, 0x1F, LO16(t244), HI16(t244), t240, t239);
                    tooltip_present();
                    break;
                case 0xD:
                    g_tip_w = 0xC;
                    g_tip_h = 7;
                    g_tip_k = G16((((g_shop_category - 1) * 0xC0) + 0x5013) + ((si + 0xCD38) * 0xC));
                    t247 = actor_text(g_tip_k, 1);
                    t248 = actor_text(g_tip_k, 2);
                    t249 = str_format(DSP(0x105B) /* "#s (#s)" */, W2(t248), W2(t247));
                    g_tip_text(0) = t249;
                    tooltip_render(DSP(0x1063));
                    tooltip_present();
                    mem_free(g_tip_text_lo(0), g_tip_text_hi(0));
                    break;
                case 0xE:
                    t251 = (u16)((i32)(i16)si % 1000);
                    g_tip_w = 0x14;
                    g_tip_h = 4;
                    g_tip_genre = (u16)(i8)(films(t251).genre);
                    t254 = movie_title(t251);
                    g_tip_text(0) = t254;
                    t255 = g_tip_viewers2_lo;
                    t256 = format_millions(g_tip_viewers, 3);
                    g_tip_text2 = t256;
                    t259 = str_format(g_text(395), W2(G32(((g_tip_genre + 0x1A) << 2) + 0x8390)), g_tip_block, (u16)(i8)(films(t251).blocks), W2(t256), t255);
                    g_tip_text1 = t259;
                    mem_free(g_tip_text2_lo, g_tip_text2_hi);
                    tooltip_render(DSP(0x107F));
                    tooltip_present();
                    mem_free(g_tip_text1_lo, g_tip_text1_hi);
                    g_tip_prev_block = g_tip_block;
                    break;
                case 0xF:
                    t261 = (u16)((i32)(i16)si % 1000);
                    g_tip_w = 0x14;
                    g_tip_h = 6;
                    g_tip_genre = (u16)(i8)(series(t261).genre);
                    t264 = series_title(t261);
                    g_tip_text(0) = t264;
                    t265 = series_episode_title(t261, g_tip_episode);
                    t266 = str_format(DSP(0x108F) /* "#d. #s" */, g_tip_episode, W2(t265));
                    g_tip_text1 = t266;
                    t267 = g_tip_viewers2_lo;
                    t268 = format_millions(g_tip_viewers, 3);
                    g_tip_text3 = t268;
                    t271 = str_format(g_text(396), W2(G32(((g_tip_genre + 0x124) << 2) + 0x8390)), g_tip_block, (u16)(i8)(series(t261).blocks), W2(t268), t267);
                    g_tip_text2 = t271;
                    mem_free(g_tip_text3_lo, g_tip_text3_hi);
                    tooltip_render(DSP(0x1096));
                    tooltip_present();
                    mem_free(g_tip_text2_lo, g_tip_text2_hi);
                    mem_free(g_tip_text1_lo, g_tip_text1_hi);
                    g_tip_prev_episode = g_tip_episode;
                    g_tip_prev_block = g_tip_block;
                    break;
                case 0x10:
                    g_tip_block = si + 0xC180;
                    if (((i16)g_tip_block) > 100) {
                        g_tip_block = g_tip_block - 0x64;
                    }
                    t285 = str_format(g_text(397), W2(g_text(g_presents(g_tip_block * 4))), W2(gifts(g_tip_block).price), ((u16)(i8)(gifts(g_tip_block).given)) + 1);
                    tooltip_text_window(LO16(t285), HI16(t285));
                    break;
                case 0x11:
                    t286 = g_credit_hi;
                    t287 = g_credit_lo;
                    t288 = money_available(g_player);
                    t289 = str_format(g_text(398), W2(t288), t287, t286);
                    tooltip_text_window(LO16(t289), HI16(t289));
                    break;
                case 0x12:
                    t290 = clock_hour();
                    t291 = sched_program_at(g_player, t290);
                    g_tip_k = t291;
                    if (((i16)g_tip_k) <= 0 || ((i16)g_tip_k) >= 1000) {
                        if (((i16)g_tip_k) <= 1000) {
                            g_tip_k = 0;
                        } else {
                            g_tip_k = ((u16)(i8)(series(g_tip_k).genre)) + 0xA;
                        }
                    } else {
                        g_tip_k = (u16)(i8)(films(g_tip_k).genre);
                    }
                    g_tip_viewers2 = 0;
                    t298 = clock_hour();
                    if (schedule_ad(g_player, t298) != 0) {
                        if (((i16)g_minute) < 55) {
                            g_tip_i = 0;
                            for (;;) {
                                LOOP_TICK(29);
                                t301 = contracts(g_player, g_tip_i).ad;
                                t302 = clock_hour();
                                if (t301 == schedule_ad(g_player, t302)) break;
                                g_tip_i = g_tip_i + 1;
                            }
                            g_tip_viewers2 = G32(((((u16)(i8)(ads(contracts(g_player, g_tip_i).ad).min_class)) - 1) << 2) + 0xA8);
                        } else {
                            g_tip_viewers2 = 0;
                        }
                    }
                    window_create(g_tip_col, g_tip_row, g_tip_col + 0xE, g_tip_row + 6);
                    tooltip_band(3, 9, 2);
                    tooltip_band(0xC, 8, 1);
                    tooltip_band(0x14, 8, 2);
                    tooltip_band(0x1C, 8, 1);
                    tooltip_band(0x24, 8, 2);
                    t307 = clock_hour();
                    if (schedule_program(g_player, t307) != 0) {
                        t309 = clock_hour();
                        ax = G16(((g_player << 5) + 0x74EA) + (t309 << 2));
                        if (((i32)(G32(((g_player << 5) + 0x74EA) + (t309 << 2)))) < ((i32)g_tip_viewers2)) {
                            if (((i16)g_minute) >= 5) {
L_3146:
                                if (((i16)g_minute) >= 55 || ((i16)(clock_hour())) <= 0) {
L_315B:
                                    ax = 1;
                                } else {
                                    ax = 3;
                                }
                            } else {
L_30F1:
                                t314 = clock_hour();
                                if (schedule_ad(g_player, t314) == 0) {
                                    goto L_3146;
                                } else {
                                    t316 = g_tip_k;
                                    t317 = clock_hour();
                                    if ((ad_target_group_ok(schedule_ad(g_player, t317), t316)) != 0) {
                                        goto L_315B;
                                    } else {
                                        goto L_3146;
                                    }
                                }
                            }
                        } else {
                            goto L_30F1;
                        }
                    } else {
                        goto L_3146;
                    }
                    tooltip_band(0x2C, 9, ax);
                    if (((i16)(clock_hour())) > 0 && ((i16)g_minute) >= 5) {
                        t322 = clock_hour();
                        t325 = viewers_to_pct(g_player, G32(((g_player << 5) + 0x74EA) + (t322 << 2)));
                        t326 = clock_hour();
                        t329 = format_millions(G32(((g_player << 5) + 0x74EA) + (t326 << 2)), 2);
                        g_tip_text1 = t329;
                        t331 = str_format(g_text(399), W2(t329), t325);
                        g_tip_text(0) = t331;
                        mem_free(g_tip_text1_lo, g_tip_text1_hi);
                    } else {
                        t332 = text_dup(0x190);
                        g_tip_text(0) = t332;
                    }
                    if ((g_tip_viewers2_lo | g_tip_viewers2_hi) != 0) {
L_3257:
                        t335 = viewers_to_pct(g_player, g_tip_viewers2);
                        t336 = format_millions(g_tip_viewers2, 2);
                        g_tip_text2 = t336;
                        t337 = str_format(g_text(401), W2(t336), t335);
                        g_tip_text1 = t337;
                        mem_free(g_tip_text2_lo, g_tip_text2_hi);
                    } else {
                        t333 = clock_hour();
                        if (schedule_ad(g_player, t333) == 0) {
                            if (((i16)(clock_hour())) <= 0) {
L_32F3:
                                t342 = text_dup(0x190);
                                g_tip_text1 = t342;
                            } else {
                                t339 = clock_hour();
                                if (schedule_ad(g_player, t339) != 0) {
                                    goto L_32F3;
                                } else {
                                    t341 = text_dup(0x192);
                                    g_tip_text1 = t341;
                                }
                            }
                        } else {
                            goto L_3257;
                        }
                    }
                    t343 = g_popup_bmp_hi(0);
                    t344 = g_popup_bmp_lo(0);
                    t345 = g_tip_text1_hi;
                    t346 = g_tip_text1_lo;
                    t347 = g_tip_text_hi(0);
                    t348 = g_tip_text_lo(0);
                    t349 = (u32)(tx(g_player, 0).reach * 0x9A0);
                    t350 = format_millions(t349, 1);
                    g_tip_text2 = t350;
                    t352 = str_format(g_text(403), W2(t350), t348, t347, t346, t345);
                    text_draw_to_free(8, 0xFFFC, LO16(t352), HI16(t352), t344, t343);
                    mem_free(g_tip_text2_lo, g_tip_text2_hi);
                    tooltip_present();
                    mem_free(g_tip_text_lo(0), g_tip_text_hi(0));
                    mem_free(g_tip_text1_lo, g_tip_text1_hi);
                    break;
                default:
                    cpu_halt("bad switch index %u in 14E1:16BB", ax);
                }
            }
            if (g_tip_cursor_hidden == 1) {
                cursor_show(0 /* unused ax */);
            }
        }
        g_tip_id = si;
        g_tip_shown_col = g_tip_col;
        g_tip_shown_row = g_tip_row;
    } else if (g_tip_id != 0 || g_redraw == 1) {
        present_frame();
        G32(0xE82) = 0;
        g_tip_shown_row = 0;
        g_redraw = 0;
        g_tip_visible = 0;
    }
    return;
}

/* 14E1:3406  building view: when g_redraw rebuild the 136x56 channel status window (titles via tvinfo_fetch_titles if stale, frame, channel_status_draw) and draw it at 64,48 */
void status_window_draw(void) {
    FUNC_TICK(47);
    if (g_redraw != 0) {
        if (g_now_title_stale != 0) {
            tvinfo_fetch_titles();
        }
        g_redraw = 0;
        rtl_bitmap_fill_w(W2(g_status_win_bmp), 0, 0, 0x11, 0x38, 0);
        ui_frame(0, 0, 0x10, 6, g_status_win_bmp_lo, g_status_win_bmp_hi, 1);
        channel_status_draw(g_status_win_bmp);
    }
    draw_sprite(g_status_win_bmp, 0x40, 0x30);
    return;
}

/* 14E1:348B  text box "Money / Credit" (string 0x18E) for the visited room's owner at cells 17,9-25,13 */
void money_box_show(void) {
    u16 t1;
    u16 t2;
    u32 t3;
    u32 t4;
    FUNC_TICK(27);
    t1 = g_credit_hi;
    t2 = g_credit_lo;
    t3 = money_available(g_player);
    t4 = str_format(g_text(398), W2(t3), t2, t1);
    ui_text_box(0x11, 9, 0x19, 0xD, LO16(t4), HI16(t4), 1);
    return;
}

/* 14E1:34CF  text box " */
void day_box_show(void) {
    u32 t1;
    FUNC_TICK(20);
    t1 = str_format(g_text(81), g_day);
    ui_text_box(0x10, 0xC, 0x16, 0xD, LO16(t1), HI16(t1), 1);
    return;
}

/* 14E1:3502  draw status of channel g_tv_channel into a bitmap: channel name; minute 0-4 "News", 5-54 program (genre, title, block n, X-rated, or "Program cancelled"), 55-59 advert name with result of the hour's ad slot (1000+ad " */
void channel_status_draw(u32 a0) {
    u16 ax;
    int cc;
    u16 di;
    u16 dx;
    u16 s0;
    u16 s1;
    u16 si;
    u16 t1;
    u16 t3;
    u16 t4;
    u16 t9;
    u16 t11;
    u32 t12;
    u32 t13;
    u32 t16;
    u16 t17;
    u32 t18;
    u32 t19;
    u32 t23;
    u16 t25;
    u16 t26;
    u16 t29;
    u32 t38;
    u32 t41;
    u16 t42;
    u32 t51;
    u16 t54;
    u16 t58;
    u16 t59;
    u16 t63;
    u16 t65;
    u32 t66;
    u32 t67;
    u16 t68;
    u32 t69;
    u32 t70;
    u16 l_A = 0;
    u8 fr_[8] = {0};                  /* bytes bp-8 .. bp-1 of the original frame */
    FUNC_TICK(535);
    t1 = clock_hour();
    t3 = viewers_lo(g_tv_channel);
    t4 = schedule_ad(g_tv_channel, t1);
    FR16(fr_, 8, 0x2) = owned_scripts(0x35, (g_tv_channel * 2) + 3).episode;
    FR16(fr_, 8, 0x4) = t3;
    g_popup_bmp(0) = a0;
    g_tip_w = 0x10;
    tooltip_band(3, 9, 3);
    tooltip_band(0xC, 0x20, 2);
    tooltip_band(0x2C, 9, 1);
    text_draw_to(8, 0xFFFC, pl_station_lo(g_tv_channel), pl_station_hi(g_tv_channel), LO16(a0), HI16(a0));
    l_A = viewers_to_pct(g_tv_channel, FR32(fr_, 8, 0x4));
    t9 = clock_hour();
    g_tip_j = t9;
    si = t4;
    for (;;) {
        LOOP_TICK(15);
        di = schedule_program(g_tv_channel, g_tip_j);
        if (schedule_program(g_tv_channel, g_tip_j) != 0x270F) break;
        g_tip_j = g_tip_j - 1;
    }
    if (((i16)g_minute) >= 5) {
        if (((i16)g_minute) >= 55) {
L_3641:
            if (((i16)g_minute) < 55) {
L_366D:
                if (((i16)g_minute) < 55) {
                    if (((i16)di) < 1000) {
                        if (((i8)(films(di).blocks)) <= 1) {
                            t13 = str_format(DSP(0x10AE));
                            ax = LO16(t13);
                            dx = HI16(t13);
                        } else {
                            t11 = clock_hour();
                            t12 = str_format(g_text(406), (t11 - g_tip_j) + 1);
                            ax = LO16(t12);
                            dx = HI16(t12);
                        }
                        FR16(fr_, 8, 0x6) = dx;
                        FR16(fr_, 8, 0x8) = ax;
                        t16 = str_format(DSP(0x10B0) /* "#s~#s#s" */, W2(G32(((((u16)(i8)(films(di).genre)) + 0x1A) << 2) + 0x8390)), 0x777C, DSEG, W2(FR32(fr_, 8, 0x8)));
                        text_draw_ex_free(8, 5, LO16(t16), HI16(t16), 0xF, 1, LO16(a0), HI16(a0));
                        mem_free(FR16(fr_, 8, 0x8), FR16(fr_, 8, 0x6));
                        if (((u16)(((i16)((films(di).flags2_16) << 0xC)) >> 0xF)) != 0) {
                            text_draw_to(0x40, 0xFFFC, G16(0x8A1C), G16(0x8A1E), LO16(a0), HI16(a0));
                        }
                    } else {
                        cc = ((i8)series_blocks(di - 0x3E8)) <= 1;
                        di = di - 0x3E8;
                        if (cc) {
                            t19 = str_format(DSP(0x10B8));
                            ax = LO16(t19);
                            dx = HI16(t19);
                        } else {
                            t17 = clock_hour();
                            t18 = str_format(g_text(406), (t17 - g_tip_j) + 1);
                            ax = LO16(t18);
                            dx = HI16(t18);
                        }
                        FR16(fr_, 8, 0x6) = dx;
                        FR16(fr_, 8, 0x8) = ax;
                        t23 = str_format(DSP(0x10BA) /* "#s~#s~#d.#s#s" */, W2(G32(((((u16)(i8)(series(di).genre)) + 0x124) << 2) + 0x8390)), 0x777C, DSEG, schedule_episode(g_tv_channel, g_tip_j), 0x7812, DSEG, W2(FR32(fr_, 8, 0x8)));
                        text_draw_ex_free(8, 5, LO16(t23), HI16(t23), 0xF, 1, LO16(a0), HI16(a0));
                        mem_free(FR16(fr_, 8, 0x8), FR16(fr_, 8, 0x6));
                    }
                } else {
                    t25 = (u16)((i32)(i16)si % 1000);
                    text_draw_to(8, 5, ad_name_lo(t25), ad_name_hi(t25), LO16(a0), HI16(a0));
                    t26 = (u16)((i32)(i16)si / 1000);
                    ax = t26;
                    if (t26 <= 3) {
                        switch (ax) {
                        case 0:
                            str_format(DSP(0x10C8));
                            break;
                        case 1:
                            g_tip_j = 0;
                            while (((i16)g_player_ad_count(g_tv_channel)) > ((i16)g_tip_j)) {
                                LOOP_TICK(26);
                                t29 = contracts(g_tv_channel, g_tip_j).ad;
                                if (t29 == (u16)((u16)((i32)(i16)si % 1000))) break;
                                g_tip_j = g_tip_j + 1;
                            }
                            t38 = str_format(g_text(407), ((u16)(i8)(contracts(g_tv_channel, g_tip_j).aired)) + 1, (u16)(i8)(ads((u16)((u16)((i32)(i16)si % 1000))).spots));
                            FR16(fr_, 8, 0x6) = HI16(t38);
                            FR16(fr_, 8, 0x8) = LO16(t38);
                            break;
                        case 2:
                            t41 = text_dup(0x198);
                            FR16(fr_, 8, 0x6) = HI16(t41);
                            FR16(fr_, 8, 0x8) = LO16(t41);
                            break;
                        case 3:
                            g_tip_j = 0;
                            while (((i16)g_player_ad_count(g_tv_channel)) > ((i16)g_tip_j)) {
                                LOOP_TICK(26);
                                t42 = contracts(g_tv_channel, g_tip_j).ad;
                                if (t42 == (u16)((u16)((i32)(i16)si % 1000))) break;
                                g_tip_j = g_tip_j + 1;
                            }
                            t51 = str_format(g_text(409), (u16)(i8)(contracts(g_tv_channel, g_tip_j).aired), (u16)(i8)(ads((u16)((u16)((i32)(i16)si % 1000))).spots));
                            FR16(fr_, 8, 0x6) = HI16(t51);
                            FR16(fr_, 8, 0x8) = LO16(t51);
                            break;
                        default:
                            cpu_halt("bad switch index %u in 14E1:3502", ax);
                        }
                    }
                    text_draw_to(8, 0x15, FR16(fr_, 8, 0x8), FR16(fr_, 8, 0x6), LO16(a0), HI16(a0));
                    mem_free(FR16(fr_, 8, 0x8), FR16(fr_, 8, 0x6));
                }
            } else if (si != 0) {
                goto L_366D;
            } else {
L_364C:
                text_draw_to(8, 5, G16(0x89E4), G16(0x89E6), LO16(a0), HI16(a0));
            }
        } else if (di == 0) {
            goto L_364C;
        } else {
            goto L_3641;
        }
    } else {
        text_draw_to(8, 5, G16(0x89E0), G16(0x89E2), LO16(a0), HI16(a0));
    }
    if (((i16)g_minute) >= 5 && ((i16)g_minute) < 55) {
        FR16(fr_, 8, 0x6) = 0;
        FR16(fr_, 8, 0x8) = 0;
        t54 = clock_hour();
        if ((G8(((g_tv_channel << 3) + 0x5553) + t54)) != 0) {
            t59 = clock_hour();
            if ((G8(((g_tv_channel << 3) + 0x5553) + t59)) == 2) {
                t63 = FPOFF(g_text(411));
                FR16(fr_, 8, 0x6) = FPSEG(g_text(411));
                FR16(fr_, 8, 0x8) = t63;
            }
        } else {
            t58 = FPOFF(g_text(410));
            FR16(fr_, 8, 0x6) = FPSEG(g_text(410));
            FR16(fr_, 8, 0x8) = t58;
        }
        if ((FR16(fr_, 8, 0x8) | FR16(fr_, 8, 0x6)) != 0) {
            text_draw_to(0x68, 0xFFFC, FR16(fr_, 8, 0x8), FR16(fr_, 8, 0x6), LO16(a0), HI16(a0));
        }
    }
    s0 = HI16(a0);
    s1 = LO16(a0);
    if (((i32)FR32(fr_, 8, 0x4)) >= 1000000) {
        t65 = l_A;
        t66 = format_millions(FR32(fr_, 8, 0x4), 1);
        FR16(fr_, 8, 0x6) = HI16(t66);
        FR16(fr_, 8, 0x8) = LO16(t66);
        t67 = str_format(DSP(0x10CC) /* "**** #5s ***** (#_2d%) ****" */, W2(t66), t65);
        ax = LO16(t67);
        dx = HI16(t67);
    } else {
        t68 = l_A;
        t69 = format_millions(FR32(fr_, 8, 0x4), 2);
        FR16(fr_, 8, 0x6) = HI16(t69);
        FR16(fr_, 8, 0x8) = LO16(t69);
        t70 = str_format(DSP(0x10E8) /* "**** #6s **** (#_2d%) ****" */, W2(t69), t68);
        ax = LO16(t70);
        dx = HI16(t70);
    }
    text_draw_to_free(8, 0x25, ax, dx, s1, s0);
    mem_free(FR16(fr_, 8, 0x8), FR16(fr_, 8, 0x6));
    return;
}

/* 14E1:3B5B  find the program running now on g_tv_channel (walk back over 9999 continuation hours) and copy its title to 0x777C (series: series title + episode title to 0x7812), loading the text block with busy cursor if needed; clears 0x7A60 */
void tvinfo_fetch_titles(void) {
    u16 si;
    u32 l_4;
    u16 t1;
    u16 t3;
    u32 t8;
    u32 t9;
    u16 t10;
    u32 t11;
    u32 t12;
    u16 t13;
    u32 t16;
    u32 t17;
    u16 t18;
    u32 t19;
    u32 t20;
    u16 t21;
    u16 t24;
    u16 t25;
    FUNC_TICK(182);
    si = 0;
    if (((i16)g_minute) >= 5 && ((i16)g_minute) < 55) {
        t1 = clock_hour();
        g_tip_j = t1;
        for (;;) {
            LOOP_TICK(15);
            t3 = schedule_program(g_tv_channel, g_tip_j);
            g_tip_k = schedule_program(g_tv_channel, g_tip_j);
            if (t3 != 0x270F) break;
            g_tip_j = g_tip_j - 1;
        }
        if (((i16)g_tip_k) > 0 && ((i16)g_tip_k) < 1000) {
            t8 = movie_title(g_tip_k);
            if ((HI16(t8) | LO16(t8)) == 0) {
                cursor_shape_push(3);
                txt_film_cache_load(0, g_tip_k, 0);
                cursor_shape_push(0);
                si = 0xFFFF;
            }
            t9 = movie_title(g_tip_k);
            t10 = far_strlen(t9);
            t11 = movie_title(g_tip_k);
            far_memcpy(HPTR(DSP(0x777C)), HPTR(t11), t10);
            t12 = movie_title(g_tip_k);
            t13 = far_strlen(t12);
            g_now_title(t13) = 0;
            if (si != 0) {
                txt_film_cache_free(0xFFFF);
            }
        } else if (((i16)g_tip_k) > 1000) {
            g_tip_k = g_tip_k - 0x3E8;
            t16 = series_title(g_tip_k);
            if ((HI16(t16) | LO16(t16)) == 0) {
                cursor_shape_push(3);
                txt_series_cache_load(0, g_tip_k, 0);
                cursor_shape_push(0);
                si = 0xFFFF;
            }
            t17 = series_title(g_tip_k);
            t18 = far_strlen(t17);
            t19 = series_title(g_tip_k);
            far_memcpy(HPTR(DSP(0x777C)), HPTR(t19), t18);
            t20 = series_title(g_tip_k);
            t21 = far_strlen(t20);
            g_now_title(t21) = 0;
            l_4 = series_episode_title(g_tip_k, schedule_episode(g_tv_channel, g_tip_j));
            t24 = far_strlen(l_4);
            far_memcpy(HPTR(DSP(0x7812)), HPTR(l_4), t24);
            t25 = far_strlen(l_4);
            g_now_episode(t25) = 0;
            if (si != 0) {
                txt_series_cache_free(0xFFFF);
            }
        }
    }
    g_now_title_stale = 0;
    return;
}

/* 14E1:3D66  tooltip text for ad target genres: " */
void ad_audience_text(u16 a0) {
    u16 si;
    u32 t1;
    u32 t2;
    u32 t3;
    FUNC_TICK(77);
    si = a0;
    if ((ads(a0).genre_rule) != 0) {
        if (((i8)(ads(si).genre_rule)) <= 0) {
            if (((i8)(ads(si).genre_rule)) < 0) {
                ad_genre_list(si, 0xFFFF);
                t3 = str_format(g_text(77), W2(g_tip_text4));
                g_tip_text(0) = t3;
                mem_free(g_tip_text4_lo, g_tip_text4_hi);
            }
        } else {
            ad_genre_list(si, 1);
            t2 = str_format(g_text(78), W2(g_tip_text4));
            g_tip_text(0) = t2;
            mem_free(g_tip_text4_lo, g_tip_text4_hi);
        }
    } else {
        t1 = str_format(DSP(0x1103));
        g_tip_text(0) = t1;
    }
    return;
}

/* 14E1:3E49  build "A, B or C" list from the ad's 3 genre codes (+0xC..0xE), sign selects required/forbidden; codes > 10 use the series genre names */
void ad_genre_list(u16 a0, u16 a1) {
    u16 di;
    u16 si;
    u32 l_4;
    u32 t13;
    FUNC_TICK(41);
    si = 1;
    di = a0;
    while (((i16)si) <= 3) {
        LOOP_TICK(125);
        if ((FP8(ads_at(di), si + 0xB)) != 0) {
            if (((i16)(((u16)(i8)(FP8(ads_at(di), si + 0xB))) * a1)) > 10) {
                l_4 = G32((((((u16)(i8)(FP8(ads_at(di), si + 0xB))) * a1) + 0x11A) << 2) + 0x8390);
            } else {
                l_4 = G32((((((u16)(i8)(FP8(ads_at(di), si + 0xB))) * a1) + 0x1A) << 2) + 0x8390);
            }
            if (si != 1) {
L_3F2E:
                if (((i16)si) > 2) {
L_3F6B:
                    l_4 = str_format(l_4);
                } else if ((FP8(ads_at(di), si + 0xC)) == 0) {
                    goto L_3F6B;
                } else {
                    l_4 = str_format(g_text(80), W2(l_4));
                }
            } else if ((FP8(ads_at(di), 0xE)) == 0) {
                goto L_3F2E;
            } else {
                l_4 = str_format(DSP(0x1105) /* "#s, " */, W2(l_4));
            }
        } else {
            l_4 = str_format(DSP(0x110A));
        }
        g_tip_text(si) = l_4;
        si = si + 1;
    }
    t13 = str_format(DSP(0x110B) /* "#s#s#s" */, W2(g_tip_text1), W2(g_tip_text2), W2(g_tip_text3));
    g_tip_text4 = t13;
    mem_free(g_tip_text1_lo, g_tip_text1_hi);
    mem_free(g_tip_text2_lo, g_tip_text2_hi);
    mem_free(g_tip_text3_lo, g_tip_text3_hi);
    return;
}

/* 14E1:4013  ui_message_window at the tooltip rect with given text */
void tooltip_text_window(u16 a0, u16 a1) {
    FUNC_TICK(16);
    ui_message_window(g_tip_col, g_tip_row, g_tip_col + g_tip_w, g_tip_row + g_tip_h, a0, a1);
    return;
}

/* 14E1:403D  draw a style 1..3 band (y, height) across the tooltip window bitmap */
void tooltip_band(u16 a0, u16 a1, u16 a2) {
    FUNC_TICK(14);
    ui_band_to(0, a0, g_tip_w, a1, a2, g_popup_bmp_lo(0), g_popup_bmp_hi(0));
    return;
}

/* 14E1:4061  window_create at tooltip rect, then render a layout string of 4 chars per row: band style digit, text slot digit (0x8F6C[i]), align/colour digit; row heights 9 (first/last) or 8 */
void tooltip_render(u32 a0) {
    u16 di;
    u16 si;
    u8 l_1;
    u8 l_2;
    u16 l_4;
    FUNC_TICK(26);
    window_create(g_tip_col, g_tip_row, g_tip_col + g_tip_w, g_tip_row + g_tip_h);
    si = 0;
    di = 3;
    while (((i16)si) < ((i16)g_tip_h)) {
        LOOP_TICK(33);
        if (si == 0) {
L_409C:
            l_4 = 9;
        } else if (((u16)(g_tip_h - 1)) != si) {
            l_4 = 8;
        } else {
            goto L_409C;
        }
        l_2 = FP8(a0, si << 2) + 0xD0;
        if (!(((i8)l_2) <= 0 || ((i8)l_2) > 3)) {
            tooltip_band(di, l_4, (u16)(i8)l_2);
        }
        si = si + 1;
        di = di + l_4;
    }
    si = 0;
    di = 0xFFFC;
    while (((i16)si) < ((i16)g_tip_h)) {
        LOOP_TICK(46);
        l_2 = (FP8(a0, (si << 2) + 1)) + 0xD0;
        l_1 = (FP8(a0, (si << 2) + 2)) + 0xD0;
        if (!(((i8)l_2) < 0 || ((i8)l_2) >= 10)) {
            text_draw_full(8, di, g_tip_text((u16)(i8)l_2), g_tip_w - 1, 1, g_popup_bmp_lo(0), g_popup_bmp_hi(0), (u16)(i8)l_1);
        }
        si = si + 1;
        di = di + 8;
    }
    return;
}

/* 14E1:4161  window_show at the tooltip position */
void tooltip_present(void) {
    FUNC_TICK(5);
    window_show(g_tip_col, g_tip_row);
    return;
}

/* Film years from 2000 on (an extension, docs/CHANGES.md): the card byte +0x14
 * counts years since 1900, and the game's card templates print "19" and two
 * digits ("#s 19#02d"), so a year from 2000 on (byte 100..199) prints with "20"
 * in a copy of the template.  The original data has 39..91: its cards are
 * drawn as before.  Plain C, no game time. */
static u8 card_tpl20[160];
static u32 card_year_template(u32 tpl, u8 year) {
    const u8 *s = HPTR(tpl);
    u16 n = 0;
    if (year < 100 || year > 199) return tpl;
    while (s[n] && n < sizeof card_tpl20 - 1) { card_tpl20[n] = s[n]; n++; }
    card_tpl20[n] = 0;
    for (u16 k = 0; k + 2 < n; k++)
        if (card_tpl20[k] == '1' && card_tpl20[k + 1] == '9' && card_tpl20[k + 2] == '#') { card_tpl20[k] = '2'; card_tpl20[k + 1] = '0'; break; }
    return host_handle(card_tpl20, sizeof card_tpl20);
}
static u16 card_year_arg(u8 year) { return year >= 100 && year <= 199 ? (u16)(year - 100) : (u16)(i8)year; }

/* 14E1:4172  film info card: genre, title, country/year with critics/box-office bars (cultural: Betty/critics, shows: Betty/critics; live: part x of y, "in N days" or "today/tomorrow, HH:00" with hour = +0x11 + 17), topicality % = +3*40/+1, length in blocks (+4), price; "(Full)" in the film agency when the player already holds 4 films of that genre; "X rated!!" flag */
void tooltip_film(u16 a0) {
    u16 ax;
    u16 di;
    u16 dx;
    u16 s0;
    u16 s1;
    u16 s2;
    u16 si;
    u32 l_4;
    u32 t2;
    u32 t4;
    u32 t14;
    u32 t21;
    u32 t26;
    u32 t27;
    u32 t30;
    u32 t32;
    u32 t34;
    u32 t35;
    u32 t36;
    u32 t44;
    u32 t45;
    u32 t46;
    u32 t47;
    u16 t49;
    u16 t50;
    u32 t51;
    u32 t52;
    u16 t53;
    u16 t54;
    u32 t55;
    FUNC_TICK(401);
    g_tip_w = 0x11;
    g_tip_h = 0xC;
    l_4 = films_at(a0);
    t2 = movie_title(a0);
    g_tip_text(0) = t2;
    t4 = movie_description(a0);
    g_tip_text1 = t4;
    g_tip_text2 = G32(((((u16)(i8)(AT(film, l_4).genre)) + 0x1A) << 2) + 0x8390);
    si = a0;
    if (((i8)(AT(film, l_4).genre)) < GENRE_CULTURE || ((i8)(AT(film, l_4).genre)) > GENRE_SHOW) {
        u32 tpl = card_year_template(g_text(412), FP8(l_4, 0x14));   /* years from 2000 on (an extension) */
        t14 = str_format(tpl, W2(G32(((((u16)(i8)FP8(l_4, 0x13)) + 0x13E) << 2) + 0x8390)), card_year_arg(FP8(l_4, 0x14)), ((u16)(i8)FP8(l_4, 0x15)) << 2, ((u16)(i8)FP8(l_4, 0x16)) << 2);
        if (tpl != g_text(412)) host_handle_release(tpl);
        g_tip_text3 = t14;
    } else if ((AT(film, l_4).genre) != GENRE_CULTURE) {
        if ((AT(film, l_4).genre) != GENRE_SHOW) {
            t27 = str_format(DSP(0x1112));
            g_tip_text3 = t27;
        } else {
            t26 = str_format(g_text(414), ((u16)(i8)FP8(l_4, 0x13)) << 2, ((u16)(i8)FP8(l_4, 0x14)) << 2);
            g_tip_text3 = t26;
        }
    } else {
        u32 tpl = card_year_template(g_text(413), FP8(l_4, 0x14));   /* years from 2000 on (an extension) */
        t21 = str_format(tpl, W2(G32(((((u16)(i8)FP8(l_4, 0x13)) + 0x13E) << 2) + 0x8390)), card_year_arg(FP8(l_4, 0x14)), ((u16)(i8)FP8(l_4, 0x15)) << 2, ((u16)(i8)FP8(l_4, 0x16)) << 2);
        if (tpl != g_text(413)) host_handle_release(tpl);
        g_tip_text3 = t21;
    }
    g_tip_text4 = 0;
    if (((u16)(((i16)((AT(film, l_4).flags2_16) << 0xE)) >> 0xF)) != 0) {
        g_tip_text4 = g_text(39);
    }
    if (!(g_room != ROOM_FILM_AGENCY || ((i8)(library_count(g_player, (u16)(i8)(AT(film, l_4).genre)))) < 4)) {
        g_tip_text4 = g_text(40);
    }
    if ((AT(film, l_4).genre) == GENRE_LIVE && (AT(film, l_4).live_day) != 0) {
        if (si != 0x1D) {
            if (si != 0xBB) {
                t34 = str_format(DSP(0x1114));
                g_tip_text6 = t34;
            } else {
                t32 = str_format(g_text(415), 5 - g_live187_left, 5);
                g_tip_text6 = t32;
            }
        } else {
            t30 = str_format(g_text(415), 3 - g_live29_left, 3);
            g_tip_text6 = t30;
        }
        di = (AT(film, l_4).live_day) - g_day;
        if (((i16)((AT(film, l_4).live_day) - g_day)) >= 2) {
            t36 = str_format(g_text(416), (AT(film, l_4).live_day) - g_day, W2(g_tip_text6));
            g_tip_text5 = t36;
        } else {
            s0 = g_tip_text6_hi;
            s1 = g_tip_text6_lo;
            s2 = (AT(film, l_4).live_slot) + 0x11;
            if (di != 0) {
                ax = G16(0x86CC);
                dx = G16(0x86CE);
            } else {
                ax = G16(0x86C8);
                dx = G16(0x86CA);
            }
            t35 = str_format(g_text(425), ax, dx, s2, s1, s0);
            g_tip_text5 = t35;
        }
        mem_free(g_tip_text6_lo, g_tip_text6_hi);
    } else if ((AT(film, l_4).genre) == GENRE_LIVE || (AT(film, l_4).genre) == GENRE_SHOW) {
        t45 = str_format(DSP(0x1116));
        g_tip_text5 = t45;
    } else {
        if ((AT(film, l_4).quality) == 0) {
            ax = 0x28;
        } else {
            ax = (u16)((u16)((i32)(i16)(((u16)(i8)(AT(film, l_4).topicality)) * 0x28) / (i16)((u16)(i8)(AT(film, l_4).quality))));
        }
        t44 = str_format(g_text(417), ax);
        g_tip_text5 = t44;
    }
    t46 = str_format(g_text(418), (u16)(i8)(AT(film, l_4).blocks));
    g_tip_text6 = t46;
    t47 = movie_title(si);
    if (((i16)(text_count_lines(t47, 0x10, 1))) > 1) {
        ax = 0x1148;
        dx = DSEG;
    } else {
        ax = 0x1118;
        dx = DSEG;
    }
    tooltip_render(MK32(dx, ax));
    t49 = g_popup_bmp_hi(0);
    t50 = g_popup_bmp_lo(0);
    t51 = movie_price(si);
    t52 = str_format(DSP(0x1178) /* "$ #l" */, W2(t51));
    text_draw_right_free(0x88, 0x44, LO16(t52), HI16(t52), t50, t49);
    if (((u16)(((i16)((films(si).flags2_16) << 0xC)) >> 0xF)) != 0) {
        t53 = g_popup_bmp_hi(0);
        t54 = g_popup_bmp_lo(0);
        t55 = text_dup(0x1A3);
        text_draw_right_free(0x88, 0x4C, LO16(t55), HI16(t55), t54, t53);
    }
    tooltip_present();
    mem_free(g_tip_text3_lo, g_tip_text3_hi);
    mem_free(g_tip_text5_lo, g_tip_text5_hi);
    mem_free(g_tip_text6_lo, g_tip_text6_hi);
    return;
}
