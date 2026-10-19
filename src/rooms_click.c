/* Room input I: left-click dispatcher and the click handlers of film agency, library, planner, building, room board, office, news room, transmitter map, ad and screenplay agencies, estate agent, studio, save room, porter.
 *
 * Original module 05FF of MTV.EXE (Rainbow Arts, 1991).  Decompiled by
 * tools/decomp.py and hand-maintained since the freeze (git tag
 * generated-final).  Every function keeps its original address in the
 * comment above it; that address ties it to the machine-level twin in gen/
 * and to the difftest tables -- keep it when editing.
 */
#include "game.h"
#include "seat.h"
#include "fixes.h"

/* 05FF:000A  left-click dispatcher: g_cur_player_rec=&players[g_player], g_hotspot=hotspot_at_cursor(0); y<120 -> per-room handler by g_room (0 building,1 office,2 news,3 boss,4 library,5 studio,8 film agency,9 script agency,10/62 estate agent,11 ad agency,12 supermarket,13/61 room board,14 Betty,25 transmitter map,31 planner,58 load/save,63 shop,64 porter,65 new-game setup); y>=120 -> bottom panel (TV sound knob, channel buttons, pause room -5); rooms -1/-3/0x42 = click closes */
void ui_on_left_click(void) {
    u16 ax;
    int cc;
    u16 si;
    u16 t4;
    u16 t7;
    u16 t9;
    u16 t10;
    u16 t17;
    u16 t19;
    FUNC_TICK(264);
    g_cur_player_rec = players_at(g_player);
    t4 = ui_hotspot_at_cursor(0);
    g_hotspot = t4;
    if (g_room != 0xFFFB) {
        if (((u32)MK32(g_room, g_pause_request)) == 0x420000) {
            game_clock_resume(0 /* unused ax */);
            room_escape();
        } else if ((g_room == 0xFFFF || g_room == 0xFFFD) && g_pause_request == 0) {
            room_escape();
        } else if (((i16)(cursor_y())) < 120 && g_pause_request == 0) {
            t7 = g_room;
            ax = g_room;
            if (g_room != ROOM_SUPERMARKET) {
                if (((i16)t7) > 12) {
                    t9 = ax;
                    if (ax == 0x3D) {
                        room_roomboard_goto_click();
                    } else if (((i16)t9) > 61) {
                        cc = ((u16)(ax - 0x3E)) <= 3;
                        ax = ax - 0x3E;
                        if (cc) {
                            switch (ax) {
                            case 0:
L_0197:
                                room_estateagent_click();
                                break;
                            case 1:
                                room_shop_click();
                                break;
                            case 2:
                                room_porter_click();
                                break;
                            case 3:
                                room_newgame_click();
                                break;
                            default:
                                cpu_halt("bad switch index %u in 05FF:000A", ax);
                            }
                        }
                    } else {
                        t10 = ax;
                        if (ax != 0x19) {
                            if (((i16)t10) > 25) {
                                if (ax == 0x1F) {
                                    room_planner_click();
                                } else if (ax == 0x3A) {
                                    room_saveload_click();
                                }
                            } else if (ax == 0xD) {
                                room_roomboard_click();
                            } else if (ax == 0xE) {
                                room_betty_click();
                            }
                        } else {
                            room_txmap_click();
                        }
                    }
                } else if (ax <= 0xB) {
                    switch (ax) {
                    case 0:
                        room_building_click();
                        break;
                    case 1:
                        room_office_click();
                        break;
                    case 2:
                        room_newsroom_click();
                        break;
                    case 3:
                        room_boss_click();
                        break;
                    case 4:
                        room_library_click();
                        break;
                    case 5:
                        room_studio_click();
                        break;
                    case 6:
                    case 7:
                        break;
                    case 8:
                        room_filmagency_click();
                        break;
                    case 9:
                        room_scriptagency_click();
                        break;
                    case 0xA:
                        goto L_0197;
                    case 0xB:
                        room_adagency_click();
                        break;
                    default:
                        cpu_halt("bad switch index %u in 05FF:000A", ax);
                    }
                }
            } else {
                room_supermarket_click();
            }
        } else if ((mouse_in_rect(0x17, 0xB9, 0x52, 0xC8)) != 0 && g_pause_request == 0) {
            if ((mouse_in_rect(0x17, 0xB9, 0x25, 0xC8)) == 0) {
                t17 = (u16)((i32)(i16)(cursor_x() + 0xFFDA) / 15);
                si = t17 + 1;
                if ((players(t17 + 1).out) == 0) {
                    g_tv_channel = si;
                    if (g_tv_off == 0) {
                        tv_update_picture();
                    }
                    g_now_title_stale = 1;
                    g_redraw = 1;
                }
            } else {
                if (((u32)MK32(g_room, g_room_mode)) == 0x1F000A) {
                    ui_animate_toggle(DSP(0x7A10), 0x80, 0x3C);
                } else {
                    g_tv_off = 0x1E - g_tv_off;
                }
                tv_update_sound();
            }
            ui_draw_player_icons();
        } else {
            t19 = mouse_in_rect(0x99, 0xB5, 0xC1, 0xBD);
            if (((u32)MK32(t19, g_pause_request)) != 0) {
                if (seat_humans && seat_paused_by_other()) {   /* multiplayer: one pause at a time -- a second */
                    g_pause_request = 0;                       /* one kept the shared clock (DS:72AE) again */
                } else if (g_room != 0xFFFB && g_clock_halt == 0) {
                    g_pause_request = 0;
                    room_enter(0xFFFB);
                    seat_clock_world = 1;    /* multiplayer: the pause is the world's (runtime/seat.h) */
                    game_clock_stop(0 /* unused ax */);
                    G32(0x72AE) = g_clock_acc;
                    game_clock_start();
                    seat_clock_world = 0;
                    cursor_hide();
                    bitmap_copy(G16(0x8E86), G16(0x8E88), g_cursor_sprites_lo(5), g_cursor_sprites_hi(5));
                    text_draw_center_to(0x15, 0xFFFA, G16(0x8814), G16(0x8816), g_cursor_sprites_lo(5), g_cursor_sprites_hi(5));
                    rtl_vga_put_bitmap_w(W2(g_cursor_sprites(5)), 0x13, 0xB4);
                    cursor_show(R.ax);
                }
            }
        }
    } else {
        g_pause_request = 0;
        seat_clock_world = 1;            /* multiplayer: the pause is the world's (runtime/seat.h) */
        game_clock_stop(0 /* unused ax */);
        g_clock_acc = G32(0x72AE);
        game_clock_start();
        seat_clock_world = 0;
        room_return();
        if (world_now()) {                   /* seats: the world in seat 0 */
            game_time_tick(1, 0 /* unused ax */);
        }
        g_redraw = 1;
    }
    return;
}

/* 05FF:0359  film agency (room 8): buy film/series from 10 dealer shelf slots (money -= price at once, ledger cat 1 film / 2 series, item id +10000 marks 'fresh, refundable'); drop on dealer (0x18) returns fresh item for refund else msg 0x67; drop on Mr. Jabble (0x23) sells own film for full price (ledger 0xB/0xC) and puts it into the 10-slot auction if flag bit6, else back to dealer stock; auction needs image>=10 (msg 0xCB); state 4 = bidding on auction slots (229E_0088) */
void room_filmagency_click(void) {
    u16 ax;
    int cc;
    u16 di;
    u16 dx;
    u16 s0;
    u16 s1;
    u16 si;
    u32 l_4;
    u32 l_8;
    u32 l_C;
    u16 l_E;
    u32 t14;
    u32 t15;
    u32 t18;
    u32 t19;
    u16 t21;
    u16 t24;
    u16 t28;
    u16 t29;
    u32 t36;
    u32 t37;
    u32 t38;
    u32 t39;
    u32 t42;
    u32 t43;
    FUNC_TICK(398);
    ax = g_room_mode;
    if (g_room_mode <= 5) {
        switch (ax) {
        case 0:
            if ((si = filmoffer_at_cursor()) != 0) {   /* --film-offers: the tapes left of slot 1 stand in front (film_offers.c) */
                l_C = filmshelf_rect_at(si);
                l_8 = dealer_at(si);
                goto L_03EA;
            }
            si = 1;
            while (((i16)si) <= 10) {
                LOOP_TICK(36);
                l_C = MK32(DSEG, (si << 3) + 0x32E);
                l_8 = dealer_at(si);
                if ((mouse_in_rect(FP16(l_C, 0), FP16(l_C, 2), FP16(l_C, 4), FP16(l_C, 6))) != 0 && ((i8)(AT(offer, l_8).shelf)) >= 0) {
                    goto L_03EA;
                } else {
                    si = si + 1;
                    continue;
                }
            }
L_04F9:
            if (g_hotspot == 0x21) {
                t21 = (u16)((i32)(i16)(cursor_x() + 0xFF20) / 9);
                t24 = (u16)((i32)(i16)(cursor_y() + 0xFFE4) / 30);
                l_E = t21 + (t24 * 5);
                /* ORIGINAL BUG (fix "case-corner"): the slot is not checked against
                 * the carried tapes, only for a non-zero word -- in the lower right
                 * corner of the case frame that is the human's programme score
                 * (g_program_score(1), DS:24A3), a film id to sell to Mr. Jabble
                 * again and again; bag_remove then counts the case below zero.
                 * Fixed: only the drawn tapes (case_slot_ok, fixes.c). */
                if ((G16(((t21 + (t24 * 5)) << 1) + 0x2483)) != 0 && (!fix_on(FIX_CASE_CORNER) || case_slot_ok(t21, l_E))) {
                    g_drag_film = g_bag(l_E);
                    bag_remove(l_E, 0 /* unused ax */);
                    t28 = cursor_y();
                    t29 = cursor_x();
                    ui_drag_begin(0x10, 0x1D, t29 + 0xFFFB, t28 + 0xFFEC, 0, 0, 0xB);
                    drag_image_draw(MK32(g_room_gfx1_hi, g_room_gfx1_lo + 2));
                    ui_draw_bag(2, 2, 0 /* unused ax */);
                    present_frame();
                    g_room_mode = 5;
                    si = 1;
                    while (((i16)si) <= (i16)g_nfilmoffers) {   /* 10 in the original (--film-offers) */
                        LOOP_TICK(13);
                        if ((dealer(si).program) != g_drag_film) {
                            si = si + 1;
                            continue;
                        } else {
                            goto L_05C5;
                        }
                    }
                } else {
L_05D5:
                    if (g_hotspot != 0x23) break;
                    if ((player_image_at_least(g_player, 0xA)) != 0) {
                        ui_draw_bag(1, 2, 0 /* unused ax */);
                        rtl_snd_room_music_stop(8, 0);
                        ui_draw_auction_board(0);
                        present_frame();
                        g_room_mode = 4;
                        break;
                    } else {
                        ui_warning(0xCB);
                        break;
                    }
                }
            } else {
                goto L_05D5;
            }
            break;
        case 1:
        case 5:
            if (g_hotspot != 0x21) {
                if (g_hotspot == 0x18 || !(g_hotspot != 0x23 || ((i16)g_drag_film) <= 10000)) {
                    if (((i16)g_drag_film) <= 10000) {
                        ui_warning(0x67);
                        break;
                    } else {
                        filmagency_return_film(0 /* unused ax */);
                        break;
                    }
                } else {
                    if (g_hotspot != 0x23) break;
                    ui_drag_end();
                    if ((((i16)g_drag_film) >= 1000 || ((u16)(((i16)((films(g_drag_film).flags_16) << 9)) >> 0xF)) == 0) && !(((i16)g_drag_film) > 1000 && ((u16)(((i16)(series_flags(g_drag_film + 0xFC18) << 9)) >> 0xF)) != 0)) {
                        film_sell_to_dealer();
                        break;
                    } else {
                        si = 1;
                        for (;;) {
                            LOOP_TICK(14);
                            l_4 = auction_at(si);
                            if ((auction(si).program) == 0) break;
                            if (((i16)si) > 10) break;
                            si = si + 1;
                        }
                        if (((i16)si) <= 10) {
                            AT(lot, l_4).program = g_drag_film;
                            AT(lot, l_4).seller = G8(0x8BA8);
                            AT(lot, l_4).bidder = 0;
                            AT(lot, l_4).idle_days = 0;
                            if (((i16)g_drag_film) >= 1000) {
                                t37 = series_price(g_drag_film + 0xFC18);
                                ax = LO16(t37);
                                dx = HI16(t37);
                            } else {
                                t36 = movie_price(g_drag_film);
                                ax = LO16(t36);
                                dx = HI16(t36);
                            }
                            AT(lot, l_4).bid = MK32(dx, ax);
                            if (((i16)g_drag_film) >= 1000) {
                                t39 = series_price(g_drag_film + 0xFC18);
                                ax = LO16(t39);
                                dx = HI16(t39);
                            } else {
                                t38 = movie_price(g_drag_film);
                                ax = LO16(t38);
                                dx = HI16(t38);
                            }
                            MEM32((u16)(FPSEG(g_cur_player_rec)), (u16)(FPOFF(g_cur_player_rec)) + 8) = FP32(g_cur_player_rec, 8) + MK32(dx, ax);
                            if (((i16)g_drag_film) >= 1000) {
                                t43 = series_price(g_drag_film + 0xFC18);
                                ax = LO16(t43);
                                dx = HI16(t43);
                            } else {
                                t42 = movie_price(g_drag_film);
                                ax = LO16(t42);
                                dx = HI16(t42);
                            }
                            s0 = dx;
                            s1 = ax;
                            if (((i16)g_drag_film) >= 1000) {
                                ax = 0xC;
                            } else {
                                ax = 0xB;
                            }
                            ledger_add(ax, MK32(s0, s1));
                            if (((i16)g_drag_film) >= 1000) {
                                series_owner(g_drag_film + 0xFC18) = 0;
                                series_flags(g_drag_film + 0xFC18) = series_flags(g_drag_film + 0xFC18) & 0xFFDF;
                            } else {
                                films(g_drag_film).owner = 0;
                                films(g_drag_film).flags_16 = (films(g_drag_film).flags_16) & 0xFFDF;
                            }
                            g_room_mode = 0;
                            ui_draw_money();
                            break;
                        } else {
                            film_sell_to_dealer();
                            break;
                        }
                    }
                }
            } else if (((i16)g_bag_count) >= 10) {
                ui_warning(0x66);
                break;
            } else {
                filmagency_film_to_bag();
                break;
            }
            break;
        case 2:
        case 3:
            break;
        case 4:
            si = 0;
            while (((i16)si) <= 1) {
                LOOP_TICK(66);
                di = 1;
                while (((i16)di) <= 5) {
                    LOOP_TICK(61);
                    if ((mouse_in_rect((si * 0x8E) + 0x14, (di * 0x13) + 0xFFF9, (si * 0x8E) + 0x9C, (di * 0x13) + 0xB)) != 0) {
                        cc = (auction((si * 5) + di).program) == 0;
                        if (!(cc || (auction_toggle_bid((si * 5) + di)) != 1)) {
                            ui_draw_auction_board(((si * 5) + di) + 0x64);
                            present_frame();
                            ui_draw_money();
                            g_redraw = 1;
                        }
                    }
                    di = di + 1;
                }
                si = si + 1;
            }
            break;
        default:
            cpu_halt("bad switch index %u in 05FF:0359", ax);
        }
    }
    return;
L_03EA:
    if ((money_check_afford(g_player, AT(offer, l_8).price)) == 0) {
        room_anim_update(2, 0 /* unused ax */);
        AT(offer, l_8).shelf = (AT(offer, l_8).shelf) - 0x64;
        g_drag_film = (AT(offer, l_8).program) + 0x2710;
        ui_drag_begin(0x10, 0x1D, FP16(l_C, 0), FP16(l_C, 2), 0, 0, 0xB);
        drag_image_draw(MK32(g_room_gfx1_hi, g_room_gfx1_lo + 2));
        ui_draw_dealer_shelf(1);
        present_frame();
        if (((i16)g_drag_film) >= 11000) {
            t15 = series_price(g_drag_film + 0xD508);
            ax = LO16(t15);
            dx = HI16(t15);
        } else {
            t14 = movie_price(g_drag_film + 0xD8F0);
            ax = LO16(t14);
            dx = HI16(t14);
        }
        MEM32((u16)(FPSEG(g_cur_player_rec)), (u16)(FPOFF(g_cur_player_rec)) + 8) = FP32(g_cur_player_rec, 8) - MK32(dx, ax);
        if (((i16)g_drag_film) >= 11000) {
            t19 = series_price(g_drag_film + 0xD508);
            ledger_add(LEDGER_SERIES_BOUGHT, t19);
        } else {
            t18 = movie_price(g_drag_film + 0xD8F0);
            ledger_add(LEDGER_FILMS_BOUGHT, t18);
        }
        g_room_mode = 1;
        ui_draw_money();
        goto L_04F9;
    }
    goto L_04F9;
L_05C5:
    g_drag_film = g_drag_film + 0x2710;
    return;
}

/* 05FF:090A  library/archive (room 4): 12 genres x 4 shelf slots per player; pick film from shelf (only if 229E_051F allows), from bag; drop on bag (max 10, msg 0x66) or shelf (max 4 per genre, msgs 0x64/0x65; genre 11 = series, id+1000); genre picker list 0x22/0x17 */
void room_library_click(void) {
    u16 ax;
    int cc;
    u16 si;
    u32 l_4;
    u32 l_8;
    u16 t3;
    u16 t22;
    u16 t27;
    u16 t31;
    u16 t43;
    u16 t48;
    u32 t51;
    u16 t52;
    FUNC_TICK(367);
    l_4 = library_at(g_room_owner);
    if (g_room_mode == 0) {
        if (((u32)MK32(g_hotspot, g_selected_genre)) == 0x1F000B) {
            cursor_hide();
            g_cursor_shape = 2;
            g_room_mode = 2;
            cursor_show(0 /* unused ax */);
        }
        if (g_selected_genre != 0 && g_hotspot == 0x20) {
            t3 = (u16)((i32)(i16)(cursor_x() + 0xFF60) / 9);
            si = t3;
            if (((i16)(FP16(l_4, (g_selected_genre << 3) + (t3 << 1)))) > 0) {
                cc = ((i16)((u16)(i8)FP8(l_4, g_selected_genre + 0x60))) > ((i16)si);
                if (cc && (library_check_can_sell(si, g_selected_genre)) == 1) {
                    g_selected_index = si;
                    g_drag_film = FP16(l_4, (g_selected_genre << 3) + (g_selected_index << 1));
                    if (g_selected_genre == 0xB) {
                        g_drag_film = g_drag_film + 0x3E8;
                    }
                    program_withdraw(g_drag_film);
                    /* ORIGINAL BUG (fix "case-owner"): program_withdraw takes the
                     * tape off the shelf like a sale -- owner 0, on the market --
                     * and so it stays while carried in the case: every dealer
                     * refill (leaving the film agency before the case is unloaded,
                     * the day change, a computer player's visit) can offer it.
                     * Fixed: it keeps its owner until it is sold (film_sell_to_dealer
                     * and the auction set owner 0 themselves). */
                    if (fix_on(FIX_CASE_OWNER)) {
                        if (((i16)g_drag_film) >= 1000) series(g_drag_film - 1000).owner = (u8)g_room_owner;
                        else films(g_drag_film).owner = (u8)g_room_owner;
                    }
                    ui_draw_library_shelf(g_room_owner);
                    present_frame();
                    ui_drag_begin(0x10, 0x1D, (g_selected_index * 9) + 0xA0, 0x30, 0, 0, 0xB);
                    drag_image_draw(MK32(g_room_gfx1_hi, g_room_gfx1_lo + 2));
                    g_room_mode = 3;
                }
            }
        }
        if (((u32)MK32(g_hotspot, g_genre_popup_open)) == 0x220000) {
            ui_genre_count_popup(0x12, 1, 0, 0 /* unused ax */);
            g_room_mode = 1;
            present_frame();
        }
        if (g_hotspot == 0x21) {
            t22 = (u16)((i32)(i16)(cursor_x() + 0xFFE8) / 9);
            si = t22;
            if (((i16)t22) < ((i16)g_bag_count)) {
                g_map_player = si;
                g_drag_film = g_bag(g_map_player);
                bag_remove(g_map_player, 0 /* unused ax */);
                if (((i16)g_drag_film) >= 1000) {
                    ax = 0xB;
                } else {
                    ax = (u16)(i8)(films(g_drag_film).genre);
                }
                g_selected_genre = ax;
                ui_draw_library_shelf(g_room_owner);
                ui_draw_bag(2, 1, 0 /* unused ax */);
                present_frame();
                ui_drag_begin(0x10, 0x1D, (g_map_player * 9) + 0x18, 0x2C, 0, 0, 0xB);
                drag_image_draw(MK32(g_room_gfx1_hi, g_room_gfx1_lo + 2));
                g_room_mode = 4;
            }
        }
    } else if (g_room_mode != 1) {
        if (g_room_mode == 2) {
            if ((mouse_in_rect(0xA0, 0x30, 0x140, 0x4D)) != 0) {
                t31 = (u16)((i32)(i16)(cursor_x() + 0xFF60) / 9);
                si = t31;
                if (((i16)(FP16(l_4, (g_selected_genre << 3) + (t31 << 1)))) > 0) {
                    g_selected_series = FP16(l_4, (g_selected_genre << 3) + (si << 1));
                    cursor_hide();
                    g_cursor_shape = 1;
                    cursor_show(0 /* unused ax */);
                    ui_series_info(0);
                }
            }
        } else if (g_room_mode == 3 || g_room_mode == 4) {
            if (g_hotspot != 0x21) {
                if (g_hotspot == 0x20) {
                    if (((i16)g_drag_film) < 1000) {
                        t43 = (u16)(i8)(films(g_drag_film).genre);
                        g_selected_genre = (u16)(i8)(films(g_drag_film).genre);
                        t48 = (u16)(i8)FP8(l_4, t43 + 0x60);
                        g_selected_index = (u16)(i8)FP8(l_4, t43 + 0x60);
                        if (((i16)t48) >= 4) {
                            t51 = str_format(g_text(100), W2(G32(((((u16)(i8)(films(g_drag_film).genre)) + 0x1A) << 2) + 0x8390)));
                            l_8 = t51;
                            ui_warning_str(LO16(t51), HI16(t51));
                            mem_free(LO16(l_8), HI16(l_8));
                        } else {
                            library_film_to_shelf();
                        }
                    } else {
                        g_selected_genre = 0xB;
                        t52 = (u16)(i8)FP8(l_4, 0x6B);
                        g_selected_index = (u16)(i8)FP8(l_4, 0x6B);
                        if (((i16)t52) >= 4) {
                            ui_warning(0x65);
                        } else {
                            library_film_to_shelf();
                        }
                    }
                }
            } else if (((i16)g_bag_count) >= 10) {
                ui_warning(0x66);
            } else {
                library_film_to_bag();
            }
        }
    } else if (g_hotspot == 0x17) {
        t27 = (u16)((i32)(i16)(cursor_y() + 0xFFF4) / 8);
        g_selected_genre = t27 + 1;
        ui_genre_count_popup(0, 0, 1, 0 /* unused ax */);
        g_room_mode = 0;
        ui_draw_library_shelf(g_room_owner);
        present_frame();
        g_genre_popup_open = 0;
    }
    return;
}

/* 05FF:0D0E  program planner / schedule (room 31): drag films/series/ads into hourly slots of sched table 0x4417 (9999 = continuation hour), scroll 0x79C6 (0/7), statistics graph (hotspot 0x33: picks axis scale 1..60 from max of today/yesterday/7-day viewer stats) */
void room_planner_click(void) {
    u16 ax;
    int cc;
    u16 di;
    u16 s0;
    u16 s1;
    u16 s2;
    u16 s3;
    u16 s4;
    u16 si;
    u32 l_4;
    u16 l_6;
    u16 l_8;
    u16 l_A;
    u16 t2;
    u32 t26;
    u16 t27;
    u16 t38;
    u32 t43;
    u32 t45;
    u32 t50;
    u16 t55;
    u16 t61;
    u16 t63;
    u16 t66;
    u32 t67;
    u16 t70;
    u32 t82;
    u16 t90;
    u32 t101;
    u32 t108;
    FUNC_TICK(1075);
    t2 = (u16)((i32)(i16)(cursor_y() + 0xFFFC) / 16);
    l_A = (t2 + g_planner_scroll(0)) + 1;
    l_8 = (((l_A - g_planner_scroll(0)) - 1) << 4) + 4;
    if (g_hotspot != 0xB || g_planner_scroll(0) == 0 || !(g_room_mode == 0 || g_room_mode == 4)) {
        if (((u32)(MK32(g_hotspot, g_planner_scroll(0)))) == 0xC0000) {
            if (g_room_mode == 0 || g_room_mode == 4) {
                g_planner_scroll(0) = 7;
                ui_draw_planner_rows(g_room_owner);
                present_frame();
            } else {
L_0DA8:
                if (g_room_mode == 0) {
                    if (g_hotspot != 0x37) {
                        if (g_hotspot == 0x38) {
                            g_chart_mode = 1;
                            finance_report_draw();
                            g_room_mode = 9;
                        }
                    } else if (((i8)g_message_pos) > 0) {
                        msg_show_next(g_room_owner);
                        g_room_mode = 8;
                    }
                    if (g_hotspot == 0xD) {
                        g_room_mode = 5;
                        ui_contract_popup(0x16, 1, 0, g_room_owner);
                        present_frame();
                    }
                    if (g_hotspot == 0xE) {
                        g_room_mode = 1;
                        ui_genre_count_popup(0x19, 1, 0, 0 /* unused ax */);
                    }
                    if (g_hotspot == 0x45) {
                        g_room_mode = 0xA;
                        options_draw();
                    }
                    if (g_hotspot == 0x33) {
                        g_room_mode = 7;
                        g_chart_mode = 1;
                        l_4 = 0;
                        di = 1;
                        while (((i16)di) <= 3) {
                            LOOP_TICK(101);
                            if ((players(di).out) == 0) {
                                si = 1;
                                while (((i16)si) <= 7) {
                                    LOOP_TICK(87);
                                    cc = (G32(((di << 5) + 0x74EA) + (si << 2))) > l_4;
                                    ax = G16(((di << 5) + 0x74EA) + (si << 2));
                                    if (cc) {
                                        l_4 = G32(((di << 5) + 0x74EA) + (si << 2));
                                    }
                                    cc = (G32(((di << 5) + 0x78BE) + (si << 2))) > l_4;
                                    ax = G16(((di << 5) + 0x78BE) + (si << 2));
                                    if (cc) {
                                        l_4 = G32(((di << 5) + 0x78BE) + (si << 2));
                                    }
                                    cc = (G32(((di << 5) + 0x6DA9) + (si << 2))) > l_4;
                                    ax = G16(((di << 5) + 0x6DA9) + (si << 2));
                                    if (cc) {
                                        l_4 = G32(((di << 5) + 0x6DA9) + (si << 2));
                                    }
                                    si = si + 1;
                                }
                            }
                            di = di + 1;
                        }
                        t26 = (u32)(l_4 + 0x3D08FF) / (u32)0x3D0900;
                        t27 = util_max(LO16(t26), HI16(t26), 1);
                        l_4 = MK32(((i16)t27) >> 0xF, t27);
                        if (l_4 < 5) {
                            g_chart_scale = LO16(l_4);
                        } else if (l_4 < 0xA) {
                            g_chart_scale = 0xA;
                        } else if (l_4 < 0x14) {
                            g_chart_scale = 0x14;
                        } else if (l_4 < 0x19) {
                            g_chart_scale = 0x19;
                        } else if (l_4 < 0x1E) {
                            g_chart_scale = 0x1E;
                        } else if (l_4 < 0x28) {
                            g_chart_scale = 0x28;
                        } else if (l_4 < 0x32) {
                            g_chart_scale = 0x32;
                        } else {
                            g_chart_scale = 0x3C;
                        }
                        if (g_chart_scale == 0) {
                            g_chart_scale = 1;
                        }
                        cursor_shape_push(3);
                        txt_film_cache_free(0);
                        txt_series_cache_free(0);
                        txt_film_cache_load(0x63, g_chart_player, 0x63);
                        txt_series_cache_load(0x63, g_chart_player, 0x63);
                        ui_draw_stats_chart();
                        cursor_shape_push(0);
                    }
                    if (g_hotspot == 0xA) {
                        clip_reset();
                        draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + 0x52), 0xC, 0x5D);
                        present_frame();
                        cursor_hide();
                        g_cursor_shape = 2;
                        g_room_mode = 4;
                        cursor_show(0 /* unused ax */);
                    }
                    if (g_hotspot == 0xF && (sched_slot_editable()) == 1) {
                        l_6 = 0;
                        for (;;) {
                            LOOP_TICK(16);
                            t38 = schedule_program(g_room_owner, l_A + l_6);
                            g_selected_program = schedule_program(g_room_owner, l_A + l_6);
                            if (t38 != 0x270F) break;
                            l_6 = l_6 - 1;
                        }
                        l_A = l_A + l_6;
                        l_8 = (((l_A - 1) - g_planner_scroll(0)) << 4) + 4;
                        archive_locate_selected();
                        if (g_selected_genre != 0) {
                            g_redraw = 1;
                            if (((i16)g_selected_program) >= 1000) {
                                t45 = program_title(schedule_program(g_room_owner, l_A));
                                ui_drag_begin(0x90, ((u16)(i8)series_blocks(g_selected_program + 0xFC18)) << 4, 0x48, l_8, LO16(t45), HI16(t45), 0);
                            } else {
                                t43 = sched_slot_title(l_A, g_room_owner);
                                ui_drag_begin(0x90, ((u16)(i8)(films(g_selected_program).blocks)) << 4, 0x48, l_8, LO16(t43), HI16(t43), 0);
                            }
                            sched_clear_slot_redraw(l_A);
                            g_room_mode = 3;
                        }
                    }
                    if (g_hotspot == 0x10 && (sched_slot_editable()) == 1) {
                        g_selected_program = schedule_ad(g_room_owner, l_A);
                        if (g_selected_program != 0) {
                            G16(0x79D8) = 0;
                            while ((contracts(g_room_owner, G16(0x79D8)).ad) != g_selected_program) {
                                LOOP_TICK(15);
                                G16(0x79D8) = G16(0x79D8) + 1;
                            }
                            t50 = str_format(DSP(0x5C2) /* "#*13s" */, W2(ads(g_selected_program).name));
                            ui_drag_begin(0x40, 0x10, 0xD8, l_8, LO16(t50), HI16(t50), 0);
                            schedule_ad(g_room_owner, l_A) = 0;
                            ui_draw_planner_rows(g_room_owner);
                            present_frame();
                            g_room_mode = 6;
                        }
                    }
                } else if (g_room_mode == 1) {
                    if (g_hotspot == 0x17) {
                        t55 = (u16)((i32)(i16)(cursor_y() + 0xFFF4) / 8);
                        g_selected_genre = t55 + 1;
                        g_room_mode = 2;
                        ui_library_genre_popup(0x13, 8, 0, floors_door_owner(figures(g_player_figure).floor, figures(g_player_figure).target_door));
                        present_frame();
                    }
                } else if (g_room_mode == 2) {
                    if (g_hotspot == 0x18) {
                        t61 = ((g_room_owner * 0x6C) + 0x6F19) + (g_selected_genre << 3);
                        t63 = (u16)((i32)(i16)(cursor_y() + 0xFFB8) / 10);
                        g_selected_index = t63;
                        t66 = G16(t61 + (t63 << 1));
                        g_selected_program = G16(t61 + (t63 << 1));
                        if (t66 != 0) {
                            l_8 = (g_selected_index * 0xA) + 0x48;
                            if (((i16)g_selected_program) < 1000) {
                                if (g_selected_genre != 8 || ((u16)(((i16)((films(g_selected_program).flags2_16) << 0xD)) >> 0xF)) == 0 || ((u16)(((i16)((films(g_selected_program).flags2_16) << 0xE)) >> 0xF)) == 0) {
                                    clip_reset();
                                    ui_library_genre_popup(0, 0, 1, g_room_owner);
                                    ui_genre_count_popup(0, 0, 1, 0 /* unused ax */);
                                    ui_draw_planner_rows(g_room_owner);
                                    present_frame();
                                    t67 = library_item_title(g_selected_genre, g_selected_index, g_room_owner);
                                    s0 = 0;
                                    s1 = HI16(t67);
                                    s2 = LO16(t67);
                                    s3 = l_8;
                                    s4 = 0x9C;
                                    if (((i16)g_selected_genre) > 10) {
                                        ax = (((u8)((u8)(((u16)(g_selected_program * 0x39)) >> 8))) << 8) | (series(g_selected_program).blocks);
                                    } else {
                                        ax = (((u8)((u8)(((u16)(g_selected_program * 0x17)) >> 8))) << 8) | (films(g_selected_program).blocks);
                                    }
                                    ui_drag_begin(0x90, ((u16)(i8)((u8)ax)) << 4, s4, s3, s2, s1, s0);
                                    g_room_mode = 3;
                                } else {
                                    ui_warning(0x2F);
                                }
                            }
                            g_redraw = 1;
                        }
                    }
                } else if (g_room_mode == 3) {
                    if (g_hotspot == 0xF && (sched_slot_editable()) == 1) {
                        l_6 = 0;
                        for (;;) {
                            LOOP_TICK(16);
                            t70 = schedule_program(g_room_owner, l_A + l_6);
                            g_selected_program = schedule_program(g_room_owner, l_A + l_6);
                            if (t70 != 0) break;
                            l_6 = l_6 + 1;
                        }
                        if (((i16)g_selected_genre) > 10 || ((i16)g_selected_genre) <= 0) {
                            if (g_selected_genre == 0xB) {
                                si = (u16)(i8)(series(library_items(g_room_owner, g_selected_genre, g_selected_index)).blocks);
                            }
                        } else {
                            si = (u16)(i8)(films(library_items(g_room_owner, g_selected_genre, g_selected_index)).blocks);
                        }
                        if (((i16)l_6) >= ((i16)si)) {
                            g_selected_program = 0;
                        }
                        if ((sched_place_program(g_selected_index, g_selected_genre, l_A, g_room_owner)) == 1) {
                            g_redraw = 1;
                            ui_drag_end();
                            ui_draw_planner_rows(g_room_owner);
                            present_frame();
                            archive_locate_selected();
                            if (g_selected_genre == 0) {
                                g_room_mode = 0;
                            } else {
                                t82 = library_item_title(g_selected_genre, g_selected_index, g_room_owner);
                                s0 = 0;
                                s1 = HI16(t82);
                                s2 = LO16(t82);
                                s3 = l_8;
                                s4 = 0x48;
                                if (((i16)g_selected_program) >= 1000) {
                                    ax = (((u8)((u8)(((u16)((g_selected_program + 0xFC18) * 0x39)) >> 8))) << 8) | series_blocks(g_selected_program + 0xFC18);
                                } else {
                                    ax = (((u8)((u8)(((u16)(g_selected_program * 0x17)) >> 8))) << 8) | (films(g_selected_program).blocks);
                                }
                                ui_drag_begin(0x90, ((u16)(i8)((u8)ax)) << 4, s4, s3, s2, s1, s0);
                                g_room_mode = 3;
                            }
                        }
                    }
                } else if (g_room_mode == 4) {
                    if (!(g_hotspot != 0xF || (sched_slot_editable()) != 1)) {
                        l_6 = l_A;
                        while (schedule_program(g_room_owner, l_6) == 0x270F) {
                            LOOP_TICK(13);
                            l_6 = l_6 - 1;
                        }
                        sched_clear_slot_redraw(l_6);
                    }
                    if (!(g_hotspot != 0x10 || (sched_slot_editable()) != 1)) {
                        schedule_ad(g_room_owner, l_A) = 0;
                        ui_draw_planner_rows(g_room_owner);
                        present_frame();
                    }
                } else if (g_room_mode == 5) {
                    if (g_hotspot == 0x19) {
                        t90 = (u16)((i32)(i16)(cursor_y() + 0xFFEE) / 10);
                        if ((contracts(g_room_owner, t90).ad) != 0) {
                            G16(0x79D8) = (u16)((u16)((i32)(i16)(cursor_y() + 0xFFEE) / 10));
                            g_selected_program = contracts(g_room_owner, G16(0x79D8)).ad;
                            l_8 = (G16(0x79D8) * 0xA) + 0x12;
                            clip_reset();
                            ui_contract_popup(0, 0, 1, 0);
                            ui_draw_planner_rows(g_room_owner);
                            present_frame();
                            t101 = str_format(DSP(0x5C8) /* "#*13s" */, W2(ads(g_selected_program).name));
                            ui_drag_begin(0x40, 0x10, 0xB0, l_8, LO16(t101), HI16(t101), 0);
                            g_room_mode = 6;
                        }
                    }
                } else if (g_room_mode == 6) {
                    if (g_hotspot == 0x10 && (sched_slot_editable()) == 1) {
                        g_selected_program = schedule_ad(g_room_owner, l_A);
                        if ((sched_place_ad(G16(0x79D8), l_A, g_room_owner)) == 1) {
                            g_redraw = 1;
                            ui_drag_end();
                            ui_draw_planner_rows(g_room_owner);
                            present_frame();
                            if (g_selected_program == 0) {
                                g_room_mode = 0;
                            } else {
                                G16(0x79D8) = 0;
                                while ((contracts(g_room_owner, G16(0x79D8)).ad) != g_selected_program) {
                                    LOOP_TICK(15);
                                    G16(0x79D8) = G16(0x79D8) + 1;
                                }
                                t108 = str_format(DSP(0x5CE) /* "#*13s" */, W2(ads(g_selected_program).name));
                                ui_drag_begin(0x40, 0x10, 0xD8, l_8, LO16(t108), HI16(t108), 0);
                            }
                        }
                    }
                } else if (g_room_mode == 7) {
                    if (g_chart_image == 0) {
                        if (((i16)g_hotspot) < 4 || ((i16)g_hotspot) > 6 || ((u16)(g_hotspot + 0xFFFD)) == g_chart_player || pl_out(g_hotspot + 0xFFFD) != 0) {
                            if (g_hotspot == 0x34) {
                                g_chart_mode = g_chart_mode + 1;
                                if (((i16)g_chart_mode) >= 3) {
                                    g_chart_mode = 0;
                                }
                                if (((u32)MK32(g_day, g_chart_mode)) == 0x10000) {
                                    g_chart_mode = 1;
                                }
                                ui_draw_stats_chart();
                            }
                        } else {
                            g_chart_player = g_hotspot + 0xFFFD;
                            ui_draw_stats_chart();
                        }
                    }
                    if (g_hotspot == 0x4C) {
                        g_chart_image = 1 - g_chart_image;
                        ui_draw_stats_chart();
                    }
                } else if (g_room_mode != 8) {
                    if (g_room_mode != 9) {
                        if (g_room_mode == 0xA) {
                            ax = g_hotspot - 0x3F;
                            if (((u16)(g_hotspot - 0x3F)) <= 9) {
                                switch (ax) {
                                case 3:                     /* 0x42: the fourth speed (--fourth-speed, fixes.c) */
                                    if (!speed4_on()) break;
                                    /* fall through */
                                case 0:
                                case 1:
                                case 2:
                                    seat_clock_world = 1;    /* multiplayer: the speed is the world's -- its time so far at the old speed (runtime/seat.h) */
                                    game_clock_stop(0 /* unused ax */);
                                    ax = g_hotspot;
                                    if (g_hotspot == 0x3F) {
                                        g_game_speed = 0x14;
                                    } else if (ax == 0x40) {
                                        g_game_speed = 0x1E;
                                    } else if (ax == 0x41) {
                                        g_game_speed = 0x3C;
                                    } else if (ax == 0x42) {
                                        g_game_speed = SPEED4;
                                    }
                                    game_clock_start();
                                    seat_clock_world = 0;
                                    mp_notice_speed(g_game_speed);   /* (multiplayer: the others' windows say it) */
                                    options_draw();
                                    present_frame();
                                    break;
                                case 4:
                                case 5:
                                case 6:
                                    break;
                                case 7:
                                    ui_animate_toggle(DSP(0x7A30), 0x80, 0x24);
                                    if (g_opt_tv_family != 0) {
                                        if (((i16)g_tvfam_state) <= 0) break;
                                        g_tvfam_state = 1;
                                        break;
                                    } else {
                                        if (((i16)g_minute) < 5 || ((i16)g_minute) >= 55 || ((i16)(clock_hour())) < 1) break;
                                        tv_family_update(1);
                                        g_tvfam_state = 2;
                                        break;
                                    }
                                    break;
                                case 8:
                                    ui_animate_toggle(DSP(0x7A10), 0x80, 0x3C);
                                    tv_update_sound();
                                    ui_draw_player_icons();
                                    break;
                                case 9:
                                    ui_animate_toggle(DSP(0x7A12), 0x80, 0x54);
                                    break;
                                default:
                                    cpu_halt("bad switch index %u in 05FF:0D0E", ax);
                                }
                            }
                        }
                    } else if (g_hotspot == 0x34) {
                        g_chart_mode = 1 - g_chart_mode;
                        if (g_day == 1) {
                            g_chart_mode = 1;
                        }
                        finance_report_draw();
                    }
                } else {
                    present_frame();
                    g_room_mode = 0;
                }
            }
        } else {
            goto L_0DA8;
        }
    } else {
        g_planner_scroll(0) = 0;
        ui_draw_planner_rows(g_room_owner);
        present_frame();
    }
    return;
}

/* 05FF:1B1D  building view (room 0): click on elevator (hotspot 0x1A) or on a floor (0x1B) sets the player figure's walk target: door under cursor (kinds 1-3 rooms, 6 info board, 10 porter) or plain x (max 236); other floor -> walk to elevator x 0x98 and queue final x in fig+8 */
void room_building_click(void) {
    u16 di;
    u16 si;
    u32 l_4;
    u32 l_8;
    u16 t2;
    u16 t7;
    u16 t9;
    u16 t14;
    u16 t16;
    u16 t17;
    u16 t18;
    u16 t19;
    u16 t22;
    u16 t23;
    FUNC_TICK(113);
    l_8 = figures_at(g_player_figure);
    if (((i16)(AT(figure, l_8).lift_phase)) <= 1 && ((i16)(AT(figure, l_8).motion)) <= 1) {
        if (g_hotspot == 0x1A) {
            t2 = cursor_y();
            t7 = (u16)((i32)(i16)(((0x71 - t2) + g_bld_scroll) + 0xFFFC) / 30);
            AT(figure, l_8).target_floor = t7 + 1;
            AT(figure, l_8).target_x = 0x99;
            AT(figure, l_8).target_door = 0xFFFE;
            g_status_msg = 0;
        }
        if (g_hotspot == 0x1B) {
            t9 = cursor_y();
            t14 = (u16)((i32)(i16)(((0x71 - t9) + g_bld_scroll) + 0xFFFC) / 30);
            l_4 = floors_at(t14 + 1);
            AT(figure, l_8).target_x = 0xFFFF;
            si = 0;
            di = t14 + 1;
            while (FP8(l_4, si + 0x14) != 0) {
                LOOP_TICK(53);
                t16 = FP8(l_4, si + 0x14);
                g_tmp_a = FP8(l_4, si + 0x14);
                if (!((((i16)t16) < 1 || ((i16)g_tmp_a) > 3) && !(g_tmp_a == 6 || g_tmp_a == 0xA))) {
                    t17 = cursor_x();
                    t18 = FP8(l_4, si);
                    g_tmp_a = FP8(l_4, si);
                    if (((i16)t17) >= ((i16)t18)) {
                        t19 = cursor_x();
                        if (((i16)t19) <= ((i16)(g_tmp_a + 0x10))) {
                            AT(figure, l_8).target_floor = di;
                            AT(figure, l_8).target_door = si;
                            if ((AT(figure, l_8).target_floor) != (AT(figure, l_8).floor)) {
                                AT(figure, l_8).target_x = 0x99;
                            } else {
                                AT(figure, l_8).target_x = FP8(l_4, si) + 4;
                                AT(figure, l_8).lift_phase = 0;
                            }
                            AT(figure, l_8).final_x = 0;
                            g_status_msg = 0;
                        }
                    }
                }
                si = si + 1;
            }
            if ((AT(figure, l_8).target_x) == 0xFFFF) {
                AT(figure, l_8).target_door = 0xFFFF;
                if ((AT(figure, l_8).floor) != di) {
                    if ((AT(figure, l_8).lift_phase) == 0) {
                        AT(figure, l_8).target_x = 0x98;
                    }
                    AT(figure, l_8).target_floor = di;
                    t23 = cursor_x();
                    AT(figure, l_8).final_x = t23;
                } else {
                    t22 = cursor_x();
                    AT(figure, l_8).target_x = t22;
                    if (((i16)(AT(figure, l_8).target_x)) >= 236) {
                        AT(figure, l_8).target_x = 0xEC;
                    }
                    AT(figure, l_8).target_floor = di;
                    AT(figure, l_8).final_x = 0;
                    AT(figure, l_8).lift_phase = 0;
                }
                g_status_msg = 0;
            }
        }
    }
    return;
}

/* 05FF:1D23  information board (room 13): 13x4 grid of room signs (g_roomboard); pick up a sign (cell byte+100 = taken) and drop it on another cell to swap them (sabotage: board no longer matches the doors); only signs with a name or studios can be moved */
void room_roomboard_click(void) {
    u16 ax;
    int cc;
    u16 di;
    u16 si;
    u16 t1;
    u16 t5;
    u16 t9;
    u16 t10;
    u16 t13;
    u16 t18;
    u16 t23;
    u16 t35;
    u32 t36;
    u16 t43;
    u16 t47;
    u16 t51;
    u16 t52;
    u16 t57;
    u16 t62;
    u16 t75;
    u16 t83;
    u16 t88;
    u32 t89;
    u16 l_8 = 0;
    u16 l_6 = 0;
    u16 l_4 = 0;
    u16 l_2 = 0;
    FUNC_TICK(44);
    ax = g_room_mode;
    if (g_room_mode == 0) {
        di = 1;
        while (((i16)di) <= 13) {
            LOOP_TICK(195);
            si = 0;
            while (((i16)si) < 4) {
                LOOP_TICK(187);
                if (((i16)si) < 2) {
                    ax = 0;
                } else {
                    ax = 9;
                }
                l_8 = ax;
                t1 = cursor_y();
                l_4 = 0x6E - (di << 3);
                if (((i16)t1) >= ((i16)(0x6E - (di << 3)))) {
                    t5 = cursor_y();
                    if (((i16)t5) <= ((i16)(0x75 - (di << 3)))) {
                        t9 = cursor_x();
                        t10 = ((si * 0x4B) + l_8) + 6;
                        l_2 = ((si * 0x4B) + l_8) + 6;
                        if (((i16)t9) >= ((i16)t10)) {
                            t13 = cursor_x();
                            if (((i16)t13) <= ((i16)(((si * 0x4B) + l_8) + 0x50))) {
                                t18 = (u16)(i8)(G8(((((di - 1) << 3) + 0x8A60) + (si << 1)) + 1));
                                g_tmp_a = (u16)(i8)(G8(((((di - 1) << 3) + 0x8A60) + (si << 1)) + 1));
                                if (((i16)t18) < 100) {
                                    t23 = (u16)(i8)(G8((((di - 1) << 3) + 0x8A60) + (si << 1)));
                                    g_tmp_b = (u16)(i8)(G8((((di - 1) << 3) + 0x8A60) + (si << 1)));
                                    if (floors_door_name(t23, g_tmp_a) != 0 || floors_door_room(g_tmp_b, g_tmp_a) == 5) {
                                        g_map_player = di - 1;
                                        g_selected_series = si;
                                        g_selected_index = (u16)(i8)(G8((((di - 1) << 3) + 0x8A60) + (si << 1)));
                                        g_selected_genre = (u16)(i8)(G8(((((di - 1) << 3) + 0x8A60) + (si << 1)) + 1));
                                        if (floors_door_owner(g_selected_index, g_selected_genre) != 0) {
                                            l_6 = floors_door_owner(g_selected_index, g_selected_genre) + 1;
                                        } else {
                                            l_6 = 5;
                                        }
                                        t35 = l_6;
                                        t36 = door_label_text(g_selected_index, g_selected_genre);
                                        ui_drag_begin(0x48, 8, l_2, l_4, LO16(t36), HI16(t36), t35);
                                        G8(((((di - 1) << 3) + 0x8A60) + (si << 1)) + 1) = (G8(((((di - 1) << 3) + 0x8A60) + (si << 1)) + 1)) + 0x64;
                                        ui_draw_info_board();
                                        present_frame();
                                        g_room_mode = 1;
                                    }
                                }
                            }
                        }
                    }
                }
                if ((g_drag_image_lo | g_drag_image_hi) != 0) break;
                si = si + 1;
            }
            if ((g_drag_image_lo | g_drag_image_hi) != 0) break;
            di = di + 1;
        }
    } else if (ax == 1) {
        di = 1;
        while (((i16)di) <= 13) {
            LOOP_TICK(232);
            si = 0;
            while (((i16)si) < 4) {
                LOOP_TICK(225);
                if (((i16)si) < 2) {
                    l_8 = 0;
                } else {
                    l_8 = 9;
                }
                t43 = cursor_y();
                if (((i16)t43) >= ((i16)(0x6E - (di << 3)))) {
                    t47 = cursor_y();
                    if (((i16)t47) <= ((i16)(0x75 - (di << 3)))) {
                        t51 = cursor_x();
                        if (((i16)t51) >= ((i16)(((si * 0x4B) + l_8) + 6))) {
                            t52 = cursor_x();
                            if (((i16)t52) <= ((i16)(((si * 0x4B) + l_8) + 0x50))) {
                                t57 = (u16)(i8)(G8((((di - 1) << 3) + 0x8A60) + (si << 1)));
                                g_tmp_a = (u16)(i8)(G8((((di - 1) << 3) + 0x8A60) + (si << 1)));
                                t62 = (u16)(i8)(G8(((((di - 1) << 3) + 0x8A60) + (si << 1)) + 1));
                                g_tmp_b = (u16)(i8)(G8(((((di - 1) << 3) + 0x8A60) + (si << 1)) + 1));
                                if (floors_door_name(t57, t62) != 0 || floors_door_room(g_tmp_a, g_tmp_b) == 5 || ((i16)g_tmp_b) >= 100) {
                                    cc = ((i8)(G8(((((di - 1) << 3) + 0x8A60) + (si << 1)) + 1))) < 100;
                                    ax = si << 1;
                                    if (cc) {
                                        g_map_player = (u16)(i8)(G8((((di - 1) << 3) + 0x8A60) + (si << 1)));
                                        g_selected_series = (u16)(i8)(G8(((((di - 1) << 3) + 0x8A60) + (si << 1)) + 1));
                                        ax = DSEG;
                                        if (floors_door_owner(g_map_player, g_selected_series) != 0) {
                                            t75 = floors_door_owner(g_map_player, g_selected_series) + 1;
                                            l_6 = floors_door_owner(g_map_player, g_selected_series) + 1;
                                            ax = t75;
                                        } else {
                                            l_6 = 5;
                                        }
                                    } else {
                                        g_map_player = 0;
                                    }
                                    t83 = (((u8)((u8)(ax >> 8))) << 8) | G8(0x79C8);
                                    G8((((di - 1) << 3) + 0x8A60) + (si << 1)) = (u8)((((u8)((u8)(ax >> 8))) << 8) | G8(0x79C8));
                                    G8(((((di - 1) << 3) + 0x8A60) + (si << 1)) + 1) = (u8)((((u8)((u8)(t83 >> 8))) << 8) | G8(0x79CA));
                                    ui_drag_end();
                                    if (g_map_player == 0) {
                                        g_room_mode = 0;
                                    } else {
                                        g_selected_index = g_map_player;
                                        g_selected_genre = g_selected_series;
                                        t88 = l_6;
                                        t89 = door_label_text(g_selected_index, g_selected_genre);
                                        ui_drag_begin(0x48, 8, 0xFFFF, 0, LO16(t89), HI16(t89), t88);
                                    }
                                    ui_draw_info_board();
                                    present_frame();
                                }
                            }
                        }
                    }
                }
                if (g_room_mode == 0) break;
                si = si + 1;
            }
            if (g_room_mode == 0) break;
            di = di + 1;
        }
    }
    return;
}

/* 05FF:21C0  information board in 'go' mode (room 61): clicking a board cell sends the player figure to the physical door at that row(floor)/column (x 0x50/0x74/0xB4/0xD8), whatever sign is displayed there, then leaves the board */
void room_roomboard_goto_click(void) {
    u16 ax;
    int cc;
    u16 di;
    u16 si;
    u16 t1;
    u16 t4;
    u16 t8;
    u16 t12;
    u16 t13;
    u16 t18;
    u16 t25;
    u16 l_A = 0;
    u16 l_8 = 0;
    u16 l_6 = 0;
    u8 fr_[4] = {0};                  /* bytes bp-4 .. bp-1 of the original frame */
    FUNC_TICK(38);
    t1 = (g_player_figure * 0x1E) + 0x7B42;
    FR16(fr_, 4, 0x2) = DSEG;
    FR16(fr_, 4, 0x4) = t1;
    di = 1;
    while (((i16)di) <= 13) {
        LOOP_TICK(168);
        l_A = 0;
        while (((i16)l_A) < 4) {
            LOOP_TICK(161);
            if (((i16)l_A) < 2) {
                ax = 0;
            } else {
                ax = 9;
            }
            l_8 = ax;
            t4 = cursor_y();
            if (((i16)t4) >= ((i16)(0x6E - (di << 3)))) {
                t8 = cursor_y();
                if (((i16)t8) <= ((i16)(0x75 - (di << 3)))) {
                    t12 = cursor_x();
                    if (((i16)t12) >= ((i16)(((l_A * 0x4B) + l_8) + 6))) {
                        t13 = cursor_x();
                        if (((i16)t13) <= ((i16)(((l_A * 0x4B) + l_8) + 0x50))) {
                            t18 = (u16)(i8)(G8((((di - 1) << 3) + 0x8A60) + (l_A << 1)));
                            g_tmp_a = (u16)(i8)(G8((((di - 1) << 3) + 0x8A60) + (l_A << 1)));
                            t25 = (u16)(i8)(G8(((((di - 1) << 3) + 0x8A60) + (l_A << 1)) + 1));
                            g_tmp_b = (u16)(i8)(G8(((((di - 1) << 3) + 0x8A60) + (l_A << 1)) + 1));
                            if (floors_door_name(t18, t25) != 0 || floors_door_room(g_tmp_a, g_tmp_b) == 5) {
L_22FC:
                                FP16(FR32(fr_, 4, 0x4), 6) = di;
                                if (((u32)(MK32(di, l_A))) == 0x10003) {
                                    si = 3;
                                } else if (((u32)(MK32(di, l_A))) == 0x10000) {
                                    si = 0;
                                } else {
                                    ax = l_A;
                                    if (l_A <= 3) {
                                        switch (ax) {
                                        case 0:
                                            l_6 = 0x50;
                                            break;
                                        case 1:
                                            l_6 = 0x74;
                                            break;
                                        case 2:
                                            l_6 = 0xB4;
                                            break;
                                        case 3:
                                            l_6 = 0xD8;
                                            break;
                                        default:
                                            cpu_halt("bad switch index %u in 05FF:21C0", ax);
                                        }
                                    }
                                    si = 0;
                                    while (((i16)si) < 4) {
                                        LOOP_TICK(14);
                                        if (floors_door_x(di, si) == l_6) break;
                                        si = si + 1;
                                    }
                                }
                                FP16(FR32(fr_, 4, 0x4), 0x12) = si;
                                FP16(FR32(fr_, 4, 0x4), 4) = floors_door_x(di, si) + 4;
                                room_return();
                            } else if (((u32)(MK32(di, l_A))) != 0x10003) {
                                cc = di == 1;
                                if (cc && l_A == 0) goto L_22FC;
                            } else {
                                goto L_22FC;
                            }
                        }
                    }
                }
            }
            if (g_room == ROOM_BUILDING) break;
            l_A = l_A + 1;
        }
        if (g_room == ROOM_BUILDING) break;
        di = di + 1;
    }
    return;
}

/* 05FF:23D8  player's office (room 1): wall map 0x1C -> room 25 transmitter map, 0x1D -> room 31 planner, 0x1E -> room 58 load/save; 0x36 letter/mail if flagged; 0x45-0x4A ? TV/VCR transport buttons (g_office_vcr_mode 1..5, 0x49 enters view state 2); 0x3E speeds up; 0x44 leave */
void room_office_click(void) {
    u16 ax;
    int cc;
    u16 t1;
    u16 t2;
    FUNC_TICK(155);
    if (g_room_mode != 1) {
        t1 = g_hotspot;
        ax = g_hotspot;
        if (g_hotspot != 0x45) {
            if (((i16)t1) > 69) {
                cc = ((u16)(ax - 0x46)) <= 4;
                ax = ax - 0x46;
                if (cc) {
                    switch (ax) {
                    case 0:
                        g_madris_input = 1;
                        draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + 0x92), 0xCA, 0x59);
                        present_frame();
                        break;
                    case 1:
                        g_madris_input = 3;
                        draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + 0x9A), 0xBD, 0x65);
                        present_frame();
                        break;
                    case 2:
                        g_madris_input = 4;
                        draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + 0xA2), 0xCA, 0x65);
                        present_frame();
                        break;
                    case 3:
                        if (G16(0x7A9E) == 0) break;
                        g_madris_state = 0;
                        tetris_draw(0, 0 /* unused ax */);
                        present_frame();
                        g_room_mode = 2;
                        break;
                    case 4:
                        g_madris_input = 5;
                        break;
                    default:
                        cpu_halt("bad switch index %u in 05FF:23D8", ax);
                    }
                }
            } else {
                t2 = ax;
                if (ax != 0x36) {
                    if (((i16)t2) > 54) {
                        if (ax != 0x3E) {
                            if (ax == 0x44) {
                                room_escape();
                            }
                        } else {
                            g_madris_delay = g_madris_delay - 1;
                            if (((i16)g_madris_delay) < 1) {
                                g_madris_delay = 1;
                            }
                        }
                    } else if (ax == 0x1C) {
                        room_enter(ROOM_TX_MAP);
                    } else if (ax == 0x1D) {
                        room_enter(ROOM_PLANNER);
                    } else if (ax == 0x1E) {
                        if (seat_humans) {   /* multiplayer: no safe -- every machine has saved games of its own, */
                        } else {             /* a load would part the games (docs/multiplayer.md, stage 7) */
                        draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + 2), 0x3F, 0xC);
                        present_frame();
                        room_enter(ROOM_SAVE_ROOM);
                        }
                    }
                } else if (g_betty_letter(g_room_owner) != 0) {
                    ui_player_event_text(g_room_owner);
                    g_room_mode = 1;
                }
            }
        } else {
            g_madris_input = 2;
            draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + 0x8A), 0xBD, 0x59);
            present_frame();
        }
    } else {
        present_frame();
        g_room_mode = 0;
    }
    return;
}

/* 05FF:25A4  news studio (room 2): subscribe/cancel the 3 news agencies (1 Political $1000 needs image>=5, 2 Show-business $10000 image>=10, 3 Sundry $10000 image>=15, else msg 0xCC; ledger cat 0xA); browse news by category with scroll; drag a news item (0x1E-byte record) into one of 3 broadcast slots, paying its price (1DA8_3F48 of price class) when taken from the pool; slots can be swapped */
void room_newsroom_click(void) {
    u16 ax;
    int cc;
    u16 di;
    u16 si;
    u32 t8;
    u16 t15;
    u16 t18;
    u16 t19;
    u16 t21;
    u16 t23;
    u16 t25;
    u16 t28;
    u16 t32;
    u16 t38;
    u8 fr_[42] = {0};                  /* bytes bp-42 .. bp-1 of the original frame */
    FUNC_TICK(566);
    if (g_room_mode == 0) {
        if (g_room_owner == g_player) {
            si = 0;
            if (g_hotspot != 0x3F) {
                if (g_hotspot != 0x40) {
                    if (g_hotspot == 0x41) {
                        si = 3;
                    }
                } else {
                    si = 2;
                }
            } else {
                si = 1;
            }
            if (si != 0) {
                if (FP8(g_cur_player_rec, si + 0x10) != 0) {
                    FR16(fr_, 42, 0x6) = 0;
                    FR16(fr_, 42, 0x8) = 0;
                    FR16(fr_, 42, 0x28) = 0;
                } else {
                    cc = si == 1;
                    ax = si;
                    if (cc) {
                        FR16(fr_, 42, 0x6) = 0;
                        FR16(fr_, 42, 0x8) = 0x3E8;
                        FR16(fr_, 42, 0x28) = 5;
                    } else if (ax == 2) {
                        FR16(fr_, 42, 0x6) = 0;
                        FR16(fr_, 42, 0x8) = 0x2710;
                        FR16(fr_, 42, 0x28) = 0xA;
                    } else if (ax == 3) {
                        FR16(fr_, 42, 0x6) = 0;
                        FR16(fr_, 42, 0x8) = 0x2710;
                        FR16(fr_, 42, 0x28) = 0xF;
                    }
                }
                if ((money_check_afford(g_player, FR32(fr_, 42, 0x8))) == 0) {
                    if ((player_image_at_least(g_player, FR16(fr_, 42, 0x28))) == 0) {
                        t8 = str_format(g_text(204), W2(G32(((si + 0x121) << 2) + 0x8390)));
                        FR16(fr_, 42, 0x2) = HI16(t8);
                        FR16(fr_, 42, 0x4) = LO16(t8);
                        ui_warning_str(LO16(t8), HI16(t8));
                        mem_free(FR16(fr_, 42, 0x4), FR16(fr_, 42, 0x2));
                    } else {
                        news_toggle_subscription(g_room_owner, si);
                        if (FP8(g_cur_player_rec, si + 0x10) != 0) {
                            MEM32((u16)(FPSEG(g_cur_player_rec)), (u16)(FPOFF(g_cur_player_rec)) + 8) = FP32(g_cur_player_rec, 8) - FR32(fr_, 42, 0x8);
                            ledger_add(LEDGER_NEWS, FR32(fr_, 42, 0x8));
                        }
                        ui_draw_news_agencies(g_room_owner);
                        present_frame();
                        ui_draw_money();
                    }
                }
            }
        }
        if (g_hotspot == 0x42 || !(g_hotspot != 0x3E || g_room_owner != g_player)) {
            ui_draw_news_slots(g_room_owner, 0);
            if (g_room_owner == g_player) {
                di = 1;
                for (;;) {
                    LOOP_TICK(9);
                    cc = ((i16)di) > 3;
                    if (cc || (news_count_category(di)) != 0) break;
                    di = di + 1;
                }
                if (((i16)di) <= 3) {
                    g_news_category = di;
                }
                g_news_scroll = 0;
                ui_draw_news_inbox(0);
            }
            present_frame();
            g_room_mode = 1;
        }
    } else if (g_room_mode == 1) {
        if (!(g_hotspot != 0x3F || (news_count_category(1)) == 0 || g_news_category == 1)) {
            g_news_category = 1;
            ui_draw_news_inbox(2);
            present_frame();
        }
        if (!(g_hotspot != 0x40 || (news_count_category(2)) == 0 || g_news_category == 2)) {
            G32(0x7A70) = 0x20000;
            ui_draw_news_inbox(2);
            present_frame();
        }
        if (!(g_hotspot != 0x41 || (news_count_category(3)) == 0 || g_news_category == 3)) {
            G32(0x7A70) = 0x30000;
            ui_draw_news_inbox(2);
            present_frame();
        }
        if (!(g_hotspot != 0x2F || ((i16)g_news_scroll) <= 0)) {
            g_news_scroll = g_news_scroll - 2;
            if (((i16)g_news_scroll) < 0) {
                g_news_scroll = 0;
            }
            ui_draw_news_inbox(2);
            present_frame();
        }
        if (g_hotspot == 0x31) {
            t15 = news_count_category(g_news_category);
            if (((i16)t15) > ((i16)(g_news_scroll + 2))) {
                g_news_scroll = g_news_scroll + 2;
                t18 = news_count_category(g_news_category);
                if (((i16)t18) < ((i16)(g_news_scroll + 2))) {
                    t19 = news_count_category(g_news_category);
                    g_news_scroll = t19 + 0xFFFE;
                }
                ui_draw_news_inbox(2);
                present_frame();
            }
        }
        if (g_hotspot == 0x44 && g_room_owner == g_player) {
            t21 = (u16)((i32)(i16)(cursor_y() + 0xFFD7) / 34);
            si = 0xFFFF;
            di = (t21 + g_news_scroll) + 1;
            for (;;) {
                LOOP_TICK(11);
                cc = di != 0;
                di = di - 1;
                if (!(cc)) break;
                t23 = news_find_category(g_news_category, si);
                si = t23;
                if (t23 != 0xFFFF) {
                    continue;
                } else {
                    break;
                }
            }
            if (((i16)si) >= 0) {
                g_drag_news_new = 1;
                far_memcpy(HPTR(DSP(0x6147)), HPTR(inbox_at(g_player, si)), 0x1E);
                news_inbox_delete(g_player, si);
                ui_draw_news_inbox(2);
                present_frame();
                t25 = (u16)((i32)(i16)(cursor_y() + 0xFFD7) / 34);
                news_draw_card(DSP(0x6147), 0xC, (t25 * 0x22) + 0x29);
                g_room_mode = 2;
            }
        }
        if (g_hotspot == 0x43 && g_room_owner == g_player) {
            t28 = (u16)((i32)(i16)(cursor_y() + 0xFFF9) / 34);
            si = t28 + 1;
            if ((news_pool(((g_room_owner * 4) + t28) + 0x39).node) != 0) {
                far_memcpy(HPTR(DSP(0x6147)), HPTR(news_pool_at(((g_room_owner * 4) + si) + 0x38)), 0x1E);
                news_draw_card(DSP(0x6147), 0xA4, ((si - 1) * 0x1A) + 0x12);
                news_pool(((g_room_owner * 4) + si) + 0x38).node = 0;
                ui_draw_news_slots(g_room_owner, 2);
                ui_draw_news_inbox(2);
                present_frame();
                g_room_mode = 2;
                g_drag_news_new = 0;
                g_news_placed = 0;
            }
        }
    } else if (g_room_mode == 2 && g_hotspot == 0x43 && g_room_owner == g_player) {
        t32 = news_price((u16)(i8)G8(0x6164));
        FR16(fr_, 42, 0x6) = ((i16)t32) >> 0xF;
        FR16(fr_, 42, 0x8) = t32;
        if (g_drag_news_new == 0 || (money_check_afford(g_player, FR32(fr_, 42, 0x8))) == 0) {
            if (g_drag_news_new != 0) {
                MEM32((u16)(FPSEG(g_cur_player_rec)), (u16)(FPOFF(g_cur_player_rec)) + 8) = FP32(g_cur_player_rec, 8) - FR32(fr_, 42, 0x8);
                ledger_add(LEDGER_NEWS, FR32(fr_, 42, 0x8));
                ui_draw_money();
            }
            t38 = (u16)((i32)(i16)(cursor_y() + 0xFFF9) / 34);
            si = t38 + 1;
            if ((news_pool(((g_room_owner * 4) + t38) + 0x39).node) == 0) {
                FR16(fr_, 42, 0x26) = 0;
            } else {
                far_memcpy((fr_ + 42 - 0x26), HPTR(news_pool_at(((g_room_owner * 4) + si) + 0x38)), 0x1E);
            }
            far_memcpy(HPTR(news_pool_at(((g_room_owner * 4) + si) + 0x38)), HPTR(DSP(0x6147)), 0x1E);
            ui_drag_end();
            if (FR16(fr_, 42, 0x26) == 0) {
                g_room_mode = 1;
            } else {
                far_memcpy(HPTR(DSP(0x6147)), (fr_ + 42 - 0x26), 0x1E);
                news_draw_card(DSP(0x6147), 0x9C, ((si - 1) * 0x1A) + 0x12);
            }
            ui_draw_news_slots(g_room_owner, 2);
            ui_draw_news_inbox(2);
            present_frame();
            g_redraw = 1;
            g_news_placed = 1;
            g_drag_news_new = 0;
        }
    }
    return;
}

/* 05FF:2C02  transmitter map (room 25): buy a transmitter at map x,y (reach=1DA8_2036, price=f(reach*616/10), max 30 transmitters, msg 0x62), satellites (slots 1,2) toggle; view other players' maps (0x4-6) / stats (0x7-9); browse own transmitters (0x12/0x13) and sell one (0x14) to an opponent bid (32BA_000A, income cat 0xD) or scrap it (0x15); last transmitter cannot be sold (msg 0x63) */
void room_txmap_click(void) {
    u16 ax;
    u16 bx;
    int cc;
    u16 di;
    u16 es;
    u16 si;
    u32 l_4;
    u16 t2;
    u16 t3;
    u16 t4;
    u16 t7;
    u32 t9;
    u16 t10;
    u16 t11;
    u32 t12;
    u16 t15;
    u16 t16;
    u32 t19;
    u32 t20;
    u16 t22;
    u16 t23;
    u16 t24;
    u32 t25;
    u16 t26;
    u16 t27;
    u32 t30;
    u32 t31;
    u16 t32;
    u16 t33;
    u16 t37;
    u16 t38;
    u16 t39;
    u16 t41;
    u32 t43;
    u32 t46;
    u16 t49;
    u16 t53;
    u16 t55;
    FUNC_TICK(620);
    ax = g_room_mode;
    if (g_room_mode <= 4) {
        switch (ax) {
        case 0:
            if (g_hotspot == 0x16) {
                si = 3;
                while (((contracts(5, ((g_room_owner * 0x42) + (si * 2)) + 9).ad) | (G16((((g_room_owner * 0x1CE) + 0x1D09) + (si * 0xE)) + 6))) != 0) {
                    LOOP_TICK(17);
                    if (((i16)si) > 32) break;
                    si = si + 1;
                }
                if (((i16)si) <= 32) {
                    if (g_map_player != g_room_owner) {
                        g_map_player = g_room_owner;
                        ui_draw_transmitter_map(0);
                        present_frame();
                        transmitters_recalc(g_player);
                    }
                    t2 = cursor_y();
                    t3 = cursor_x();
                    ui_drag_begin(0x10, 0x10, t3, t2, 0, 0, 0xB);
                    t4 = col_to_px(g_map_player + 2);
                    drag_image_draw(MK32(g_room_gfx1_hi, g_room_gfx1_lo + t4));
                    G32(0x79EC) = 0x40030;
                    G32(0x79F0) = 0x6100C8;
                    g_room_mode = 1;
                    break;
                } else {
                    ui_warning(0x62);
                    break;
                }
            } else if (g_hotspot == 0x11) {
                g_room_mode = 3;
                g_map_player = g_room_owner;
                si = 1;
                while (((i16)si) <= 32) {
                    LOOP_TICK(29);
                    cc = ((i32)(tx(g_map_player, si).reach)) > 0;
                    bx = ((g_map_player * 0x1CE) + 0x1D09) + (si * 0xE);
                    es = DSEG;
                    if (cc) {
                        if ((tx(g_map_player, si).state) != 2) break;
                        si = si + 1;
                        continue;
                    } else {
                        si = si + 1;
                        continue;
                    }
                }
                if (((i16)si) <= 32) {
                    g_selected_index = si;
                }
                ui_draw_transmitter_map(g_selected_index);
                present_frame();
                break;
            } else if (((i16)g_hotspot) < 4 || ((i16)g_hotspot) > 6) {
                if (((i16)g_hotspot) < 7 || ((i16)g_hotspot) > 9) {
                    if (!(g_hotspot == 1 || g_hotspot == 2)) break;
                    t7 = ((g_room_owner * 0x1CE) + 0x1D09) + (g_hotspot * 0xE);
                    l_4 = tx_at(g_room_owner, g_hotspot);
                    if (!((G16(t7 + 4) | G16(t7 + 6)) != 0 || (AT(transmitter, l_4).state) == 2 || (money_check_afford(g_room_owner, AT(transmitter, l_4).fee)) == 0)) break;
                    if ((FP16(l_4, 4) | FP16(l_4, 6)) != 0) {
                        if ((AT(transmitter, l_4).state) != 1) {
                            if ((AT(transmitter, l_4).state) != 2) {
                                if ((AT(transmitter, l_4).state) == 0) {
                                    AT(transmitter, l_4).state = 2;
                                }
                            } else {
                                AT(transmitter, l_4).state = 0;
                            }
                        } else {
                            AT(transmitter, l_4).reach = 0;
                            AT(transmitter, l_4).state = 0;
                        }
                    } else {
                        t9 = map_sum_population((g_hotspot - 1) * 0x54, 0, ((g_hotspot - 1) * 0x54) + 0x53, 0x6D, g_population_map);
                        AT(transmitter, l_4).reach = t9;
                        AT(transmitter, l_4).state = 1;
                    }
                    g_map_player = g_player;
                    transmitters_recalc(g_player);
                    ui_draw_transmitter_map(0);
                    present_frame();
                    ui_draw_money();
                    break;
                } else {
                    si = g_hotspot + 0xFFFA;
                    if (pl_out(g_hotspot + 0xFFFA) != 0) break;
                    g_map_player = si;
                    ui_draw_transmitter_map(0);
                    present_frame();
                    ui_transmitter_info(g_map_player, 0, 0 /* unused ax */);
                    g_room_mode = 2;
                    break;
                }
            } else {
                si = g_hotspot + 0xFFFD;
                if (pl_out(g_hotspot + 0xFFFD) != 0) break;
                g_map_player = si;
                ui_draw_transmitter_map(0);
                present_frame();
                transmitters_recalc(g_map_player);
                break;
            }
            break;
        case 1:
            if (g_hotspot != 3) break;
            t10 = cursor_y();
            t11 = cursor_x();
            t12 = transmitter_new_reach(t11 + 0xFFD0, t10 + 0xFFFC);
            ax = LO16(t12);
            if (!(((i16)HI16(t12)) >= 0 && (((i16)HI16(t12)) > 0 || ax != 0))) break;
            t15 = cursor_y();
            t16 = cursor_x();
            t19 = (u32)((i32)((u32)(transmitter_new_reach(t16 + 0xFFD0, t15 + 0xFFFC) * 0x268)) / (i32)0xA);
            t20 = round_to(t19, 0x3E8);
            /* ORIGINAL BUG (fix "tx-price"): the purchase is checked against the new
             * viewers x 61.6, but a transmitter costs its price class -- all viewers
             * in the circle x 61.6, up to the next of 50 000/100 000/250 000/500 000
             * (set as .fee below) -- which the tooltip shows, the money display
             * subtracts and the day's settlement charges: a player could buy a
             * transmitter he cannot pay for.  Fixed: check the price class. */
            if (fix_on(FIX_TX_PRICE)) {
                t20 = (u32)((i32)((u32)(transmitter_reach(t16 + 0xFFD0, t15 + 0xFFFC) * 0x268)) / (i32)0xA);
                t20 = transmitter_fee_bracket(LO16(t20), HI16(t20));
            }
            if ((money_check_afford(g_room_owner, t20)) != 0) break;
            si = 3;
            for (;;) {
                LOOP_TICK(22);
                t22 = ((g_room_owner * 0x1CE) + 0x1D09) + (si * 0xE);
                l_4 = tx_at(g_room_owner, si);
                if ((G16(t22 + 4) | G16(t22 + 6)) == 0) break;
                if (((i16)si) > 32) break;
                si = si + 1;
            }
            if (((i16)si) > 32) break;
            t23 = cursor_y();
            t24 = cursor_x();
            t25 = transmitter_new_reach(t24 + 0xFFD0, t23 + 0xFFFC);
            AT(transmitter, l_4).reach = t25;
            t26 = cursor_y();
            t27 = cursor_x();
            t30 = (u32)((i32)((u32)(transmitter_reach(t27 + 0xFFD0, t26 + 0xFFFC) * 0x268)) / (i32)0xA);
            t31 = transmitter_fee_bracket(LO16(t30), HI16(t30));
            AT(transmitter, l_4).fee = t31;
            t32 = cursor_x();
            AT(transmitter, l_4).x = t32 + 0xFFD0;
            t33 = cursor_y();
            AT(transmitter, l_4).y = t33 + 0xFFFC;
            AT(transmitter, l_4).state = 1;
            g_room_mode = 0;
            ui_drag_end();
            ui_draw_transmitter_map(0);
            present_frame();
            ui_draw_money();
            transmitters_recalc(g_player);
            break;
        case 2:
            if (((i16)g_hotspot) < 7 || ((i16)g_hotspot) > 9) break;
            g_map_player = g_hotspot + 0xFFFA;
            ui_transmitter_info(g_map_player, 1, 0 /* unused ax */);
            g_room_mode = 0;
            break;
        case 3:
            if (g_hotspot != 0x12) {
                if (g_hotspot != 0x13) {
                    if (g_hotspot != 0x14) break;
                    t39 = tx_next_slot(g_selected_index + 1);
                    if (t39 != g_selected_index) {
                        l_4 = tx_at(g_room_owner, g_selected_index);
                        if ((AT(transmitter, l_4).state) != 1) {
                            l_4 = tx_at(g_player, g_selected_index);
                            t43 = ai_transmitter_bid(0, AT(transmitter, l_4).x, AT(transmitter, l_4).y);
                            g_tx_bids(0) = t43;
                            t46 = ai_transmitter_bid(1, AT(transmitter, l_4).x, AT(transmitter, l_4).y);
                            g_tx_bids(1) = t46;
                            transmitter_sale_dialog(0, DSP(0x8F0D));
                            present_frame();
                            g_room_mode = 4;
                            break;
                        } else {
                            AT(transmitter, l_4).reach = 0;
                            AT(transmitter, l_4).state = 0;
                            t41 = tx_next_slot(g_selected_index + 1);
                            g_selected_index = t41;
                            g_redraw = 1;
                            ui_draw_transmitter_map(g_selected_index);
                            transmitters_recalc(g_player);
                            present_frame();
                            ui_draw_money();
                            break;
                        }
                    } else {
                        ui_warning(0x63);
                        break;
                    }
                } else {
                    t38 = tx_next_slot(g_selected_index + 1);
                    g_selected_index = t38;
                    g_redraw = 1;
                    ui_draw_transmitter_map(g_selected_index);
                    present_frame();
                    break;
                }
            } else {
                t37 = tx_prev_slot(g_selected_index - 1);
                g_selected_index = t37;
                g_redraw = 1;
                ui_draw_transmitter_map(g_selected_index);
                present_frame();
                break;
            }
            break;
        case 4:
            if (g_hotspot == 4 || g_hotspot == 5) {
                t49 = G16(((g_hotspot + 0xFFFC) << 2) + 0x8F0F);
                bx = (g_hotspot + 0xFFFC) << 2;
                di = g_hotspot + 0xFFFC;
                if (!(((i16)(G16(((g_hotspot + 0xFFFC) << 2) + 0x8F0F))) >= 0 && (((i16)t49) > 0 || G16(bx + 0x8F0D) > 0))) break;
                l_4 = tx_at(g_player, g_selected_index);
                t53 = FPOFF(g_cur_player_rec);
                t55 = FPSEG(g_cur_player_rec);
                MEM32(t55, t53 + 8) = FP32(g_cur_player_rec, 8) + g_tx_bids(di);
                ledger_add(LEDGER_SUNDRY_INCOME, g_tx_bids(di));
                si = 3;
                while (((contracts(5, ((((u16)(i8)g_ai_player(di)) * 0x42) + (si * 2)) + 9).ad) | (G16((((((u16)(i8)g_ai_player(di)) * 0x1CE) + 0x1D09) + (si * 0xE)) + 6))) != 0) {
                    LOOP_TICK(16);
                    si = si + 1;
                }
                far_memcpy(HPTR(tx_at((u16)(i8)g_ai_player(di), si)), HPTR(l_4), 0xE);
                AT(transmitter, l_4).reach = 0;
                AT(transmitter, l_4).state = 2;
                txmap_after_sale();
                break;
            } else {
                if (g_hotspot != 0x15) break;
                l_4 = tx_at(g_player, g_selected_index);
                AT(transmitter, l_4).state = 2;
                AT(transmitter, l_4).reach = 0;
                txmap_after_sale();
                break;
            }
            break;
        default:
            cpu_halt("bad switch index %u in 05FF:2C02", ax);
        }
    }
    return;
}

/* 05FF:33F6  advertising agency (room 11): take an ad offer (10 slots) if image >= required (1DA8_2682 from ad image class; X-rated ads need image>=20; else msg 0xCD); give back an ad taken today (else msg 0x69 'already signed'); drop on bag signs it (max 10 contracts, msg 0x66); 0x23 = confirm/leave (ad_agency_sign_contracts) */
void room_adagency_click(void) {
    int cc;
    u16 di;
    u16 si;
    u32 l_4;
    u32 l_8;
    u32 l_C;
    u32 l_10;
    u16 l_12;
    u16 l_14;
    u16 t1;
    u32 t18;
    u16 t23;
    u16 t24;
    u16 t28;
    u16 t31;
    u16 t36;
    u16 t37;
    FUNC_TICK(381);
    if (g_room_mode == 0) {
        if (g_nadoffers > 10) {          /* --ad-offers: the rows of folders (fixes.c) */
            si = adoffer_at((i16)cursor_x(), (i16)cursor_y());
        } else {
            t1 = mouse_in_rect(0x6F, 0x4C, 0x84, 0x6A);
            si = 0;
            if (t1 == 0) {
                if ((mouse_in_rect(0x83, 0x3D, 0x9F, 0x5B)) == 0) {
                    if ((mouse_in_rect(0x9D, 0x2B, 0xB2, 0x49)) != 0) {
                        si = (u16)((u16)((i32)(i16)(cursor_x() + 0xFF6A) / 8)) + 7;
                    }
                } else {
                    si = (u16)((u16)((i32)(i16)(cursor_x() + 0xFF84) / 8)) + 3;
                }
            } else {
                si = (u16)((u16)((i32)(i16)(cursor_x() + 0xFF98) / 8));
            }
        }
        if (((i8)(ad_offers(si).shelf)) < 0) {
L_34C7:
            si = 0;
        } else {
            if ((ad_offers(si).ad) == 0) goto L_34C7;
        }
        if (((i16)si) > 0) {
            l_12 = viewers_to_pct(g_player, G32(((((u16)(i8)(ads(ad_offers(si).ad).min_class)) - 1) << 2) + 0xA8));
            cc = (ads(ad_offers(si).ad).genre_rule) == 0;
            if ((cc || (player_image_at_least(g_player, 0x14)) != 0) && (player_image_at_least(g_player, l_12)) != 0) {
                g_drag_ad = ad_offers(si).ad;
                l_10 = ads_at(g_drag_ad);
                l_8 = tx(g_player, 0).reach;
                ad_offers(si).shelf = (ad_offers(si).shelf) - 0x64;
                t23 = cursor_y();
                t24 = cursor_x();
                ui_drag_begin(0x10, 0x1E, t24 + 0xFFFC, t23 + 0xFFF1, 0, 0, 0xB);
                drag_image_draw(MK32(g_room_gfx1_hi, g_room_gfx1_lo + 0xA));
                ui_draw_ad_offers();
                present_frame();
                g_room_mode = 1;
            } else {
                t18 = str_format(g_text(205), W2(ads(ad_offers(si).ad).name));
                l_4 = t18;
                ui_warning_str(LO16(t18), HI16(t18));
                mem_free(LO16(l_4), HI16(l_4));
            }
        } else if ((mouse_in_rect(0xE4, 0x20, 0x111, 0x57)) != 0) {
            t28 = (u16)((i32)(i16)(cursor_x() + 0xFF1C) / 9);
            t31 = (u16)((i32)(i16)(cursor_y() + 0xFFE0) / 32);
            l_14 = t28 + (t31 * 5);
            si = contracts(g_player, t28 + (t31 * 5)).ad;
            if ((contracts(g_player, t28 + (t31 * 5)).ad) != 0) {
                di = 1;
                while ((ad_offers(di).ad) != si) {
                    LOOP_TICK(13);
                    if (((i16)di) > (i16)g_nadoffers) break;   /* 10 in the original (--ad-offers) */
                    di = di + 1;
                }
                if (((i16)di) <= (i16)g_nadoffers) {
                    l_C = contracts_at(g_player, l_14);
                    g_drag_ad = AT(contract, l_C).ad;
                    contract_delete(g_player, l_14);
                    t36 = cursor_y();
                    t37 = cursor_x();
                    ui_drag_begin(0x10, 0x1E, t37 + 0xFFFC, t36 + 0xFFF1, 0, 0, 0xB);
                    drag_image_draw(MK32(g_room_gfx1_hi, g_room_gfx1_lo + 0xA));
                    ui_draw_bag(2, 3, 0 /* unused ax */);
                    present_frame();
                    g_room_mode = 2;
                } else {
                    ui_warning(0x69);
                }
            } else {
L_37B5:
                if (g_hotspot == 0x23) {
                    ad_agency_sign_contracts();
                }
            }
        } else {
            goto L_37B5;
        }
    } else if (g_room_mode == 1 || g_room_mode == 2) {
        if (g_hotspot != 0x26) {
            if (g_hotspot == 0x21) {
                if (((i16)g_player_ad_count(g_player)) >= 10) {
                    ui_warning(0x66);
                } else {
                    ad_take_contract();
                }
            }
        } else {
            ad_return_offer();
        }
    }
    return;
}

/* 05FF:380B  screenplay agency (room 9): buy one of 5 screenplays (money -= price, ledger cat 4), return a script bought today (refund) else msg 0x2A 'No returns'; drop on bag (max 5 scripts); 0x44 finalize purchases and leave */
void room_scriptagency_click(void) {
    int cc;
    u16 di;
    u16 si;
    u32 l_4;
    u16 t3;
    u16 t5;
    u16 t10;
    u16 t12;
    FUNC_TICK(193);
    if (((u32)MK32(g_hotspot, g_room_mode)) == 0x440000) {
        script_agency_finalize();
        room_escape();
    } else if (g_room_mode == 0) {
        si = 1;
        while (((i16)si) <= 5) {
            LOOP_TICK(44);
            if ((mouse_in_rect((si * 7) + 0x63, 0x23, (si * 7) + 0x69, 0x40 - si)) != 0) {
                l_4 = script_offers_at(si);
                cc = ((i8)(script_offers(si).status)) >= 0;
                if (cc && (AT(script_offer, l_4).script) != 0 && (money_check_afford(g_player, AT(script_offer, l_4).price)) == 0) {
                    goto L_38B7;
                } else {
                    si = si + 1;
                    continue;
                }
            } else {
                si = si + 1;
                continue;
            }
        }
L_395C:
        if (g_hotspot == 0x21) {
            t10 = (u16)((i32)(i16)(cursor_x() + 0xFFE6) / 9);
            t12 = owned_scripts(g_player, t10).script;
            g_drag_item = owned_scripts(g_player, t10).script;
            di = t10;
            if (((i16)t12) >= 0) {
                si = 1;
                while ((script_offers(si).script) != g_drag_item) {
                    LOOP_TICK(13);
                    if (((i16)si) > 5) break;
                    si = si + 1;
                }
                if (((i16)si) <= 5) {
                    g_drag_aux = (u16)(i8)(owned_scripts(g_player, di).aux);
                    ui_drag_begin(0x10, 0x1D, (di * 9) + 0x1A, 0x41, 0, 0, 0xB);
                    drag_image_draw(MK32(g_room_gfx1_hi, g_room_gfx1_lo + 0x2A));
                    script_list_delete(g_player, di);
                    ui_draw_bag(2, 4, 0 /* unused ax */);
                    present_frame();
                    g_room_mode = 2;
                } else {
                    ui_warning(0x2A);
                }
            }
        }
        if (g_hotspot == 0x23) {
            g_room_mode = 3;
        }
    } else if (g_room_mode == 1 || g_room_mode == 2) {
        if (g_hotspot != 0x26) {
            if (g_hotspot == 0x21) {
                if (((i16)g_scripts_owned_count(g_player)) >= 5) {
                    ui_warning(0x66);
                } else {
                    script_to_bag();
                }
            }
        } else {
            script_return_to_agency();
        }
    }
    return;
L_38B7:
    g_drag_item = AT(script_offer, l_4).script;
    g_drag_aux = (u16)(i8)(AT(script_offer, l_4).aux);
    ui_drag_begin(0x10, 0x1D, (si * 7) + 0x63, 0x23, 0, 0, 0xB);
    drag_image_draw(MK32(g_room_gfx1_hi, g_room_gfx1_lo + 0x2A));
    t3 = FPOFF(g_cur_player_rec);
    t5 = FPSEG(g_cur_player_rec);
    MEM32(t5, t3 + 8) = FP32(g_cur_player_rec, 8) - (AT(script_offer, l_4).price);
    ledger_add(LEDGER_SCRIPTS, AT(script_offer, l_4).price);
    AT(script_offer, l_4).status = (AT(script_offer, l_4).status) - 0x64;
    ui_draw_script_offers();
    ui_draw_money();
    present_frame();
    g_room_mode = 1;
    goto L_395C;
}

/* 05FF:3AB7  real-estate agent (rooms 10 and 62): 0x27 opens studio plan (room 62); click a studio: own -> offer to give it up (229E_104A), free -> rent it for 3/2 of base rent per day (229E_100C); updates g_estate_wealth (0..10) from total studio rents for the room decoration */
void room_estateagent_click(void) {
    u16 di;
    u16 si;
    u16 t2;
    u32 t5;
    u16 t6;
    u32 t8;
    u32 t9;
    u16 t10;
    u16 t11;
    u16 t12;
    u16 t13;
    u16 t14;
    u32 t22;
    u8 fr_[14] = {0};                  /* bytes bp-14 .. bp-1 of the original frame */
    FUNC_TICK(153);
    FR16(fr_, 14, 0xA) = 0;
    FR16(fr_, 14, 0xC) = 0;
    if (g_room != ROOM_ESTATE_AGENT) {
        if (g_room == ROOM_ESTATE_PLAN) {
            if (g_room_mode == 0) {
                si = 1;
                while (((i16)si) <= 13) {
                    LOOP_TICK(123);
                    di = 0;
                    for (;;) {
                        LOOP_TICK(118);
                        t10 = floors_door_kind(si, di);
                        g_tmp_a = floors_door_kind(si, di);
                        if (((i16)t10) < 1 || ((i16)g_tmp_a) > 3) break;
                        door_board_pos(si, di, (fr_ + 14 - 0x8), (fr_ + 14 - 0x6));
                        if ((mouse_in_rect(FR16(fr_, 14, 0x8), FR16(fr_, 14, 0x6), FR16(fr_, 14, 0x8) + 9, FR16(fr_, 14, 0x6) + 0xA)) != 0) {
                            g_estate_studio = floors_door_studio(si, di);
                            t2 = (g_estate_studio << 4) + 0x6165;
                            FR16(fr_, 14, 0x2) = DSEG;
                            FR16(fr_, 14, 0x4) = t2;
                            if (g_estate_studio != 0 && ((i8)(FP8(FR32(fr_, 14, 0x4), 0xF))) <= 0) {
                                if (((u16)((u16)(i8)(FP8(FR32(fr_, 14, 0x4), 3)))) != g_player) {
                                    if ((FP8(FR32(fr_, 14, 0x4), 3)) == 0xFF) {
                                        t6 = (u16)(i8)(FP8(FR32(fr_, 14, 0x4), 2));
                                        t8 = (u32)((i32)((u32)(FP32(FR32(fr_, 14, 0x4), 6) * 3)) / (i32)2);
                                        t9 = str_format(g_text(118), W2(t8), t6);
                                        ui_yesno_box(LO16(t9), HI16(t9), 0);
                                        present_frame();
                                        g_room_mode = 2;
                                    }
                                } else {
                                    t5 = str_format(g_text(116), (u16)(i8)(FP8(FR32(fr_, 14, 0x4), 2)));
                                    ui_yesno_box(LO16(t5), HI16(t5), 0);
                                    present_frame();
                                    g_room_mode = 1;
                                }
                            }
                        }
                        di = di + 1;
                    }
                    si = si + 1;
                }
            } else if (g_room_mode == 1) {
                if (g_hotspot != 0x24) {
                    if (g_hotspot == 0x25) {
                        ui_yesno_box(0, 0, 1);
                        present_frame();
                        g_room_mode = 0;
                    }
                } else {
                    ui_yesno_box(0, 0, 1);
                    studio_give_back(g_estate_studio);
                    ui_draw_building_plan();
                    present_frame();
                    if ((studios(g_estate_studio).reserved) == 0xFF) {
                        /* ORIGINAL BUG: text 0x77 is the door sign "Psychiatrist"
                         * ("Psychiaterbuero"); the studio is only released when
                         * its production ends: say when (text 242 as in 05FF:3DDF) */
                        if (fix_on(FIX_ESTATE_TEXT)) {
                            u16 dd = studios(g_estate_studio).end_day - g_day;
                            u32 tx_ = str_format(g_text(242), W2(g_text(dd == 0 ? 206 : dd == 1 ? 207 : 274)), studios(g_estate_studio).end_hour);
                            ui_warning_str(LO16(tx_), HI16(tx_));
                            mem_free(LO16(tx_), HI16(tx_));
                        } else {
                            ui_warning(0x77);
                        }
                    }
                    g_room_mode = 0;
                }
            } else if (g_room_mode == 2) {
                if (g_hotspot != 0x24) {
                    if (g_hotspot == 0x25) {
                        ui_yesno_box(0, 0, 1);
                        present_frame();
                        g_room_mode = 0;
                    }
                } else {
                    t11 = ui_yesno_box(0, 0, 1);
                    studio_request_rent(g_player, g_estate_studio, t11);
                    ui_draw_building_plan();
                    present_frame();
                    ui_draw_money();
                    if (((u16)((u16)(i8)(studios(g_estate_studio).reserved))) == g_player) {
                        /* ORIGINAL BUG: text 0x78 is the door sign "Bio-control";
                         * queued behind a running production: text 113 (the hover
                         * text for exactly this state) */
                        if (fix_on(FIX_ESTATE_TEXT)) {
                            u32 tx_ = str_format(g_text(113), (u16)(i8)(studios(g_estate_studio).size));
                            ui_message_str(LO16(tx_), HI16(tx_));   /* too long for "Warning:~#s" */
                            mem_free(LO16(tx_), HI16(tx_));
                        } else {
                            ui_warning(0x78);
                        }
                    }
                    g_room_mode = 0;
                }
            }
            si = 1;
            while (((i16)si) <= 12) {
                LOOP_TICK(13);
                t12 = rent_hi(si);
                t13 = FR16(fr_, 14, 0xC);
                t14 = rent_lo(si);
                FR16(fr_, 14, 0xC) = FR16(fr_, 14, 0xC) + rent_lo(si);
                FR16(fr_, 14, 0xA) = (FR16(fr_, 14, 0xA) + t12) + ((u32)t13 + t14 > 0xFFFF);
                si = si + 1;
            }
            t22 = (u32)((i32)((FR32(fr_, 14, 0xC) - g_estate_base_rent) + 1) / (i32)((u32)((i32)g_estate_base_rent / (i32)0xA)));
            g_estate_wealth = LO16(t22) + 1;
            if (((i16)(LO16(t22) + 1)) > 10) {
                g_estate_wealth = 0xA;
            }
        }
    } else if (g_hotspot == 0x27) {
        room_enter(ROOM_ESTATE_PLAN);
    }
    return;
}

/* 05FF:3DDF  production studio (room 5): insert screenplay from bag (studio size check vs 1DA8_2293, msg 0x3E), take it out, throw away (0xA); menu: new shopping list (229E_09AF), production info = quality forecast (actors fit*weight + props quality, scaled x125/100 -> >80 'good', >60 'neat', >40 'poor', else 'catastrophe', plus which part is worse), start production (needs script, all items bought 1DA8_24EA, then 229E_0B98) with end day/hour message */
void room_studio_click(void) {
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
    u16 l_12;
    u32 l_16;
    u32 l_1A;
    u32 l_1E;
    u16 l_20;
    u16 t3;
    u16 t5;
    u16 t15;
    u16 t19;
    u32 t58;
    FUNC_TICK(512);
    l_1E = studios_at(g_cur_studio);
    l_E = 0;
    l_C = 0;
    l_A = 0;
    di = 0;
    if (g_room_mode == 0) {
        if (g_hotspot != 0x28 || (AT(studio, l_1E).script) == 0) {
            if (g_hotspot == 0x21) {
                t3 = (u16)((i32)(i16)(cursor_x() + 0xFF06) / 9);
                l_20 = t3;
                t5 = owned_scripts(g_player, t3).script;
                g_drag_item = owned_scripts(g_player, t3).script;
                if (t5 != 0) {
                    g_drag_aux = owned_scripts(g_player, l_20).episode;
                    if (g_drag_aux == 0) {
                        g_drag_aux = 1;
                    }
                    ui_drag_begin(0x10, 0x1D, (l_20 * 9) + 0xFA, 0x19, 0, 0, 0xB);
                    drag_image_draw(MK32(g_room_gfx_hi, g_room_gfx_lo + 0xA));
                    script_list_delete(g_player, l_20);
                    ui_draw_bag(2, 5, 0 /* unused ax */);
                    present_frame();
                    g_room_mode = 1;
                } else {
L_3F16:
                    if (!(g_hotspot != 0x29 || (AT(studio, l_1E).script) == 0)) {
                        studio_take_script();
                        g_room_mode = 2;
                    }
                }
            } else {
                goto L_3F16;
            }
        } else {
            ui_studio_menu(0);
            g_room_mode = 3;
        }
    } else if (g_room_mode == 1 || g_room_mode == 2) {
        if (g_hotspot != 0xA) {
            if (g_hotspot != 0x21) {
                if (g_hotspot == 0x29) {
                    t15 = script_count_scenes(g_drag_item);
                    if (((i16)t15) > ((i16)((u16)(i8)(AT(studio, l_1E).size)))) {
                        ui_warning(0x3E);
                    } else {
                        studio_swap_script();
                    }
                }
            } else {
                script_to_bag_from_studio();
                g_room_mode = 0;
            }
        } else {
            screenplays(g_drag_item).state = 0x63;
            ui_drag_end();
            g_room_mode = 0;
        }
    } else if (g_room_mode == 3) {
        if (g_hotspot != 0x2A) {
            if (g_hotspot == 0x4B) {
                ui_box(0, 0xB, 3, 0x1F, 9, 2, 0 /* unused ax */);
                l_12 = studios(g_cur_studio).script;
                l_10 = (u16)(i8)(studios(g_cur_studio).episode);
                if (((i8)((u8)((((u8)((u8)(DSEG >> 8))) << 8) | (studios(g_cur_studio).episode)))) <= ((i8)(studios(g_cur_studio).episode_last))) {
                    l_4 = MK32(FPSEG(g_scripts), ((FPOFF(g_scripts) + (l_12 * 0x1B3)) + ((l_10 - 1) * 0x23)) + 8);
                    si = 1;
                    while (((i16)si) <= 10) {
                        LOOP_TICK(123);
                        if ((FP8(l_4, (si * 3) + 1)) == 0) {
L_40CC:
                            if (((i8)(FP8(l_4, (si * 3) + 1))) <= 0) {
L_4149:
                                if (((i8)(FP8(l_4, (si * 3) + 1))) >= 16) {
                                    l_A = l_A + util_min((u16)(i8)(FP8(l_4, (si * 3) + 2)), (u16)(i8)(G8((((((u16)(i8)(FP8(l_4, (si * 3) + 1))) + 0xFFF1) << 2) + 0x268E) + ((u16)(i8)(FP8(l_4, (si * 3) + 3))))));
                                    l_E = l_E + ((u16)(i8)(FP8(l_4, (si * 3) + 2)));
                                }
                            } else if (((i8)(FP8(l_4, (si * 3) + 1))) > 15) {
                                goto L_4149;
                            } else {
                                t19 = script_role_match_pct(l_12, l_10, si, (u16)(i8)(FP8(l_4, (si * 3) + 3)));
                                l_C = l_C + (u16)((u16)((i32)(i16)(t19 * ((u16)(i8)(FP8(l_4, (si * 3) + 2)))) / 100));
                                di = di + ((u16)(i8)(FP8(l_4, (si * 3) + 2)));
                            }
                            si = si + 1;
                            continue;
                        } else {
                            if ((FP8(l_4, (si * 3) + 3)) == 0) break;
                            goto L_40CC;
                        }
                    }
                    if (((i16)si) > 10) {
                        if (((u16)(di + l_E)) == 0) {
                            ax = 0;
                        } else {
                            ax = (u16)((u16)((i32)(i16)(((u16)(i8)FP8(l_4, 0)) * (l_C + l_A)) / (i16)(di + l_E)));
                        }
                        l_8 = (u16)((u16)((i32)(i16)(ax * 0x7D) / 100));
                        l_8 = (u16)((u16)((i32)(i16)(l_8 * 0x64) / (i16)((u16)(i8)FP8(l_4, 0))));
                        if (((i16)l_8) <= 80) {
                            if (((i16)l_8) <= 60) {
                                if (((i16)l_8) <= 40) {
                                    l_6 = 0x164;
                                } else {
                                    l_6 = 0x163;
                                }
                            } else {
                                l_6 = 0x162;
                            }
                        } else {
                            l_6 = 0x161;
                        }
                        if (l_6 != 0x161) {
                            if (((i16)(u16)((u16)((i32)(i16)(l_C * 0x64) / (i16)di))) <= ((i16)(u16)((u16)((i32)(i16)(l_A * 0x64) / (i16)l_E)))) {
                                l_16 = str_format(g_text(357), W2(g_text(359)));
                            } else {
                                l_16 = str_format(g_text(357), W2(g_text(358)));
                            }
                        } else {
                            l_16 = text_dup(0x168);
                        }
                        l_1A = str_format(DSP(0x5D4) /* "#s~#s" */, W2(g_text(l_6)), W2(l_16));
                        mem_free(LO16(l_16), HI16(l_16));
                    } else {
                        l_1A = text_dup(0x160);
                    }
                } else {
                    l_1A = text_dup(0x160);
                }
                text_draw_ex_free(0x60, 0x18, LO16(l_1A), HI16(l_1A), 0x13, 1, g_screen_buf_lo, g_screen_buf_hi);
                present_frame();
                g_room_mode = 4;
            } else if (g_hotspot == 0x2B) {
                if ((AT(studio, l_1E).script) != 0) {
                    if (((i8)(AT(studio, l_1E).episode_last)) >= ((i8)(AT(studio, l_1E).episode))) {
                        if ((script_items_bought(AT(studio, l_1E).script, (u16)(i8)(AT(studio, l_1E).episode))) != 0) {
                            if ((studio_start_production(g_cur_studio)) == 1) {
                                ui_studio_menu(1);
                                if (((u16)((AT(studio, l_1E).end_day) - g_day)) != 0) {
                                    if (((u16)((AT(studio, l_1E).end_day) - g_day)) != 1) {
                                        l_16 = g_text(274);
                                    } else {
                                        l_16 = g_text(207);
                                    }
                                } else {
                                    l_16 = g_text(206);
                                }
                                t58 = str_format(g_text(242), W2(l_16), AT(studio, l_1E).end_hour);
                                l_1A = t58;
                                ui_message_str(LO16(t58), HI16(t58));
                                mem_free(LO16(l_1A), HI16(l_1A));
                                g_room = 0xFFFC;
                                g_room_mode = 0;
                                room_anim_update(2, 0 /* unused ax */);
                            }
                        } else {
                            ui_warning(0xF1);
                        }
                    } else {
                        ui_warning(0xF3);
                    }
                } else {
                    ui_warning(0x42);
                }
            } else if (!(g_hotspot != 0x24 || ((i8)(AT(studio, l_1E).episode_last)) < ((i8)(AT(studio, l_1E).episode)))) {
                si = g_shoplist_count(g_player);
                while (((i16)si) >= 0) {
                    LOOP_TICK(18);
                    if ((shoplists(g_player, si).script) != (AT(studio, l_1E).script)) {
                        si = si - 1;
                        continue;
                    } else {
                        goto L_44A3;
                    }
                }
                AT(studio, l_1E).episode_last = (AT(studio, l_1E).episode_last) - 1;
                ui_studio_menu(2);
            }
        } else if ((studio_new_shopping_list(g_cur_studio)) == 1) {
            ui_studio_menu(2);
        }
    }
    return;
L_44A3:
    shoplist_delete(g_player, si);
    AT(studio, l_1E).episode_last = (AT(studio, l_1E).episode_last) - 1;
    ui_studio_menu(2);
    return;
}

/* 05FF:44CF  busy cursor, redraw, reload the 10 save-slot names from resource 0xD1 (0054_3FC1) and redraw the slot list */
void saveload_reload_names(void) {
    FUNC_TICK(14);
    cursor_shape_push(3);
    game_clock_start();
    savegame_names_load();
    save_slots_draw();
    cursor_shape_push(0);
    return;
}

/* 05FF:44F4  load/save room (room 58): 10 slots (2x5); save = text entry of name, write names to res 0xD1 and game to res 0xD2+slot (229E_200D), 'Disk full!' on g_save_error; load = 229E_1386(0xD2+slot) and re-init time/room/music; creates the save index res 0xD1 (0x190 bytes) if missing */
void room_saveload_click(void) {
    u16 di;
    u16 si;
    u32 t17;
    u8 fr_[8] = {0};                  /* bytes bp-8 .. bp-1 of the original frame */
    FUNC_TICK(126);
    if (g_room_mode != 1) {
        if (g_room_mode != 2) {
            if (g_room_mode == 3) {
                di = 0;
                while (((i16)di) < 2) {
                    LOOP_TICK(126);
                    si = 1;
                    while (((i16)si) <= 5) {
                        LOOP_TICK(121);
                        if ((mouse_in_rect((di * 0x76) + 0x40, (si * 0x11) + 8, (di * 0x76) + 0x8E, (si * 0x11) + 0xF)) != 0) {
                            draw_sprite(MK32(g_room_gfx1_hi, g_room_gfx1_lo + 0x22), (di * 0x76) + 0x40, (si * 0x11) + 8);
                            game_clock_stop(0 /* unused ax */);
                            if ((ui_input_text((di * 0x76) + 0x40, (si * 0x11) + 8, 0xA, G32(((((di * 5) + si) - 1) << 2) + 0x8BB2))) == 0) {
                                if (LO16((rtl_strlen_w(W2(G32(((((di * 5) + si) - 1) << 2) + 0x8BB2))))) == 0) {
L_46EF:
                                    saveload_reload_names();
                                } else {
                                    cursor_shape_push(3);
                                    game_clock_start();
                                    save_slots_draw();
                                    savegame_names_store();
                                    game_save(((di * 5) + si) + 0xD1, ((di * 5) + si) + 0xD1);
                                    cursor_shape_push(0);
                                    if (g_save_error == 0) {
                                        g_room_mode = 0;
                                        room_escape();
                                    } else {
                                        saveload_reload_names();
                                        if (savegame_save_refused()) savegame_refused_warning(1);   /* too large a mod (actions.c) */
                                        else ui_warning(0x1AB);
                                    }
                                }
                            } else {
                                goto L_46EF;
                            }
                        }
                        si = si + 1;
                    }
                    di = di + 1;
                }
            } else if (g_room_mode == 4) {
                di = 0;
                while (((i16)di) < 2) {
                    LOOP_TICK(167);
                    si = 1;
                    while (((i16)si) <= 5) {
                        LOOP_TICK(162);
                        if ((mouse_in_rect((di * 0x76) + 0x40, (si * 0x11) + 8, (di * 0x76) + 0x8E, (si * 0x11) + 0xF)) != 0) {
                            if (LO16((rtl_strlen_w(W2(G32(((((di * 5) + si) - 1) << 2) + 0x8BB2))))) != 0) {
                                cursor_shape_push(3);
                                if (g_tvfam_state != 0) {
                                    g_tvfam_state = 1;
                                    while (g_tvfam_state != 0) {
                                        LOOP_TICK(3);
                                        game_frame();
                                    }
                                }
                                game_load(((di * 5) + si) + 0xD1, ((di * 5) + si) + 0xD1);
                                if (savegame_load_refused()) {   /* more films, series or ads than this game has: not loaded (actions.c) */
                                    cursor_shape_push(0);
                                    savegame_refused_warning(0);
                                    return;
                                }
                                g_player_figure = g_player - 1;
                                g_room_owner = floors_door_owner(figures(g_player_figure).floor, figures(g_player_figure).target_door);
                                game_clock_update((fr_ + 8 - 0x6));
                                if (g_game_mode == 1) {
                                    g_room_mode = 0;
                                    G32(0x8A4A) = 0x10000;
                                    g_room_stack_top = 2;
                                    room_escape();
                                    g_game_mode = 3;
                                    g_clock_halt = 0;
                                }
                                t17 = str_format(DSP(0x5DA) /* "#02d:#02d" */, W2(G32(0x71EA)));
                                FR16(fr_, 8, 0x2) = HI16(t17);
                                FR16(fr_, 8, 0x4) = LO16(t17);
                                ui_status_text(FR16(fr_, 8, 0x4), FR16(fr_, 8, 0x2), 0xB4);
                                mem_free(FR16(fr_, 8, 0x4), FR16(fr_, 8, 0x2));
                                g_now_title_stale = 1;
                                if (!(g_opt_tv_family != 0 || ((i16)(clock_hour())) < 1)) {
                                    if (((i16)g_minute) < 5 || ((i16)g_minute) >= 55) {
                                        tv_family_update(2);
                                    } else {
                                        tv_family_update(1);
                                    }
                                }
                                tv_update_picture();
                                figures_load_sprites();
                                ui_draw_money();
                                ui_draw_viewers();
                                ui_draw_betty_meter();
                                panel_restore_strip();
                                ui_draw_player_icons();
                                cursor_shape_push(0);
                            }
                        }
                        si = si + 1;
                    }
                    di = di + 1;
                }
            } else if (g_room_mode == 5) {
                if ((res_exists(0xD1, 0xD1)) != 0xFFFF) {
                    save_insert_disk_box(1);
                    save_overlay_box(0);
                    g_room_mode = 2;
                } else {
                    save_insert_disk_box(1);
                    save_not_found_box(0);
                    g_room_mode = 1;
                }
            }
        } else if (g_hotspot != 0x39) {
            if (g_hotspot == 0x3A) {
                g_room_mode = 4;
                save_overlay_box(1);
            }
        } else {
            g_room_mode = 3;
            save_overlay_box(1);
        }
    } else if (g_hotspot != 0x25) {
        if (g_hotspot == 0x24) {
            cursor_shape_push(3);
            R.ax = 0xD1;
            rtl_res_create(0xD1, 0x190, 0);
            savegame_names_store();
            cursor_shape_push(0);
            save_not_found_box(1);
            if (g_game_mode != 1) {
                g_room_mode = 2;
                save_overlay_box(0);
            } else {
                g_room_mode = 4;
            }
        }
    } else {
        room_escape();
    }
    return;
}

/* 05FF:4957  porter (room 64): gossip bubble: bailiff/impound news from g_porter_event (type 3: victim self -> text 0x13A, other player -> 0x13B, 8 = film agency -> 0x13C), otherwise 0x13D 'boring day'; second click closes */
void room_porter_click(void) {
    u32 t1;
    u32 t2;
    u32 t4;
    u32 t5;
    u16 l_4 = 0;
    u16 l_2 = 0;
    FUNC_TICK(83);
    if (g_room_mode == 0) {
        if (g_npc_type == 3) {
            if (g_npc_target_player != g_player) {
                if (((i16)g_npc_target_player) > 3) {
                    if (g_npc_target_player == 8) {
                        t4 = str_format(g_text(316), W2(FP32(g_cur_player_rec, 0)));
                        l_2 = HI16(t4);
                        l_4 = LO16(t4);
                    }
                } else {
                    t2 = str_format(g_text(315), W2(FP32(g_cur_player_rec, 0)), W2(players(g_npc_target_player).name));
                    l_2 = HI16(t2);
                    l_4 = LO16(t2);
                }
            } else {
                t1 = str_format(g_text(314), W2(FP32(g_cur_player_rec, 0)));
                l_2 = HI16(t1);
                l_4 = LO16(t1);
            }
        } else {
            t5 = str_format(g_text(317), W2(FP32(g_cur_player_rec, 0)));
            l_2 = HI16(t5);
            l_4 = LO16(t5);
        }
        gossip_say(0, l_4, l_2);
        g_room_mode = 1;
    } else {
        gossip_say(1, 0, 0);
        g_room_mode = 0;
    }
    return;
}
