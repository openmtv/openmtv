/* Game rules: rule helpers: schedule, audience shares, screenplays, topicality, studios, library, intruder events.
 *
 * Original module 1DA8 of MTV.EXE (Rainbow Arts, 1991).  Decompiled by
 * tools/decomp.py and hand-maintained since the freeze (git tag
 * generated-final).  Every function keeps its original address in the
 * comment above it; that address ties it to the machine-level twin in gen/
 * and to the difftest tables -- keep it when editing.
 */
#include "game.h"
#include "seat.h"
#include "fixes.h"
#include "ai_improve.h"

/* 1DA8:0006  number of screenplays (0x715C records 1..50) with status +4 == 0 and flag bit1 (in pool) - used by the screenplay agency refresh */
u16 script_count_available(void) {
    u16 ax;
    u16 di;
    u16 si;
    FUNC_TICK(10);
    si = 1;
    di = 0;
    while (((i16)si) <= 50) {
        LOOP_TICK(23);
        if ((screenplays(si).state) == 0) {
            if (((u16)(((i16)((screenplays(si).flags_16) << 0xE)) >> 0xF)) != 0) {
                di = di + 1;
            }
        }
        si = si + 1;
    }
    ax = di;
    return ax;
}

/* 1DA8:004E  clear the "scheduled" flag (movie +7 bit1, series +0x26 bit1) of all programs, then set it for every program in each player's schedule from the current hour (clock_hour_round55) to slot 14 */
void sched_mark_scheduled(void) {
    int cc;
    u16 di;
    u16 si;
    u16 l_2;
    FUNC_TICK(18);
    l_2 = 1;
    while (((i16)l_2) <= (i16)g_nfilms) {
        LOOP_TICK(9);
        films(l_2).flags2_16 = (films(l_2).flags2_16) & 0xFFFD;
        l_2 = l_2 + 1;
    }
    l_2 = 1;
    while (((i16)l_2) <= (i16)g_nseries) {
        LOOP_TICK(9);
        series(l_2).flags2_16 = (series(l_2).flags2_16) & 0xFFFD;
        l_2 = l_2 + 1;
    }
    l_2 = 1;
    while (((i16)l_2) <= 3) {
        LOOP_TICK(55);
        di = clock_hour_round55();
        while (schedule_program(l_2, di) == 0x270F) {
            LOOP_TICK(13);
            di = di - 1;
        }
        while (((i16)di) <= 14) {
            LOOP_TICK(36);
            cc = ((i16)schedule_program(l_2, di)) <= 0;
            si = schedule_program(l_2, di);
            if (cc) {
L_010C:
                if (((i16)si) > 1000) {
                    if (((i16)si) < 2000) {
                        series_flags2(si + 0xFC18) = series_flags2(si + 0xFC18) | 2;
                    }
                }
            } else if (((i16)si) >= 1000) {
                goto L_010C;
            } else {
                films(si).flags2_16 = (films(si).flags2_16) | 2;
            }
            di = di + 1;
        }
        l_2 = l_2 + 1;
    }
    return;
}

/* 1DA8:0146  sum of news scores G8(0x3F4B+p) of players still in the game, capped at 95 (% of audience watching the news) */
u16 news_total_share(void) {
    u16 ax;
    u16 di;
    u16 si;
    FUNC_TICK(13);
    si = 1;
    di = 0;
    while (((i16)si) <= 3) {
        LOOP_TICK(17);
        if ((players(si).out) != 0) {
            ax = 0;
        } else {
            ax = (u16)(i8)g_news_score(si);
        }
        si = si + 1;
        di = di + ax;
    }
    if (((i16)di) > 95) {
        di = 0x5F;
    }
    ax = di;
    return ax;
}

/* 1DA8:0187  sum of program scores G16(0x24A1+2p) of players still in the game, capped at 95 (% of audience watching TV) */
u16 program_total_share(void) {
    u16 ax;
    u16 di;
    u16 si;
    FUNC_TICK(13);
    si = 1;
    di = 0;
    while (((i16)si) <= 3) {
        LOOP_TICK(18);
        if ((players(si).out) != 0) {
            ax = 0;
        } else {
            ax = g_program_score(si);
        }
        si = si + 1;
        di = di + ax;
    }
    if (((i16)di) > 95) {
        di = 0x5F;
    }
    ax = di;
    return ax;
}

/* 1DA8:01CB  episodes a screenplay produces: series flag (+5 bit0) -> episode count (+0x22) of its target series, else 1 */
u16 script_episode_count(u16 a0) {
    u16 ax;
    FUNC_TICK(31);
    if (((u16)(((i16)((screenplays(a0).flags_16) << 0xF)) >> 0xF)) != 0) {
        ax = (u16)(i8)(series(screenplays(a0).target).episodes);
    } else {
        ax = 1;
    }
    return ax;
}

/* 1DA8:0217  every 2nd day: movie topicality (+3) rises to the next multiple of max(+1)/5 (20% steps); series episode topicality rises to the next multiple of max/20 (5% steps) */
void programs_recover_topicality(void) {
    u16 di;
    u16 si;
    u32 l_4;
    u32 l_8;
    u32 l_C;
    u16 l_E;
    u16 t8;
    u16 t9;
    u16 t13;
    u16 t18;
    u16 t19;
    FUNC_TICK(22);
    if ((u16)((u16)((i32)(i16)g_day % 2)) == 0) {
        l_E = 1;
        while (((i16)l_E) <= (i16)g_nfilms) {
            LOOP_TICK(44);
            l_C = films_at(l_E);
            if (!(((i8)(AT(film, l_C).topicality)) >= ((i8)(AT(film, l_C).quality)) || ((i8)(AT(film, l_C).quality)) <= 0)) {
                di = 1;
                for (;;) {
                    LOOP_TICK(15);
                    t8 = (u16)(i8)(AT(film, l_C).topicality);
                    t9 = (u16)((i32)(i16)(((u16)(i8)(AT(film, l_C).quality)) * di) / 5);
                    if (((i16)t8) < ((i16)t9)) break;
                    di = di + 1;
                }
                t13 = (u16)((i32)(i16)(((u16)(i8)(AT(film, l_C).quality)) * di) / 5);
                AT(film, l_C).topicality = (u8)t13;
            }
            l_E = l_E + 1;
        }
        l_E = 1;
        while (((i16)l_E) <= (i16)g_nseries) {
            LOOP_TICK(60);
            l_8 = series_at(l_E);
            di = 1;
            while (((i16)((u16)(i8)(series(l_E).episodes))) >= ((i16)di)) {
                LOOP_TICK(48);
                l_4 = MK32(FPSEG(l_8), FPOFF(l_8) + (di * 3));
                if (((i8)FP8(l_4, 1)) < ((i8)FP8(l_4, 0))) {
                    si = 1;
                    for (;;) {
                        LOOP_TICK(15);
                        t18 = (u16)(i8)FP8(l_4, 1);
                        t19 = (u16)((i32)(i16)(((u16)(i8)FP8(l_4, 0)) * si) / 20);
                        if (((i16)t18) < ((i16)t19)) break;
                        si = si + 1;
                    }
                    FP8(l_4, 1) = (u8)(u16)((u16)((i32)(i16)(((u16)(i8)FP8(l_4, 0)) * si) / 20));
                }
                di = di + 1;
            }
            l_E = l_E + 1;
        }
    }
    return;
}

/* 1DA8:034E  for each of the 12 studios whose production finishes today at/after this hour: message 0x10E ("Studio No. %d reports ... completed"), movie/episode becomes available (+6|=0x20) and goes to the owner's archive at price 0, series episode marked produced; whole production done -> script status 99 and studio freed; refreshes planner/library if open */
void studio_check_finished(void) {
    u16 ax;
    u16 di;
    u16 si;
    u32 l_4;
    u32 l_8;
    u32 l_C;
    u16 l_E;
    u16 l_10;
    u16 l_12;
    u16 l_14;
    u16 t1;
    u32 t2;
    u16 t3;
    u16 t12;
    FUNC_TICK(21);
    di = 1;
    while (((i16)di) <= 12) {
        LOOP_TICK(337);
        l_C = studios_at(di);
        if ((AT(studio, l_C).end_day) == g_day) {
            t1 = clock_hour();
            if (((i16)(t1 + 0x11)) >= ((i16)(AT(studio, l_C).end_hour))) {
                AT(studio, l_C).end_day = 0;
                t2 = (MK32(((i16)((u16)(i8)(AT(studio, l_C).episode))) >> 0xF, (u16)(i8)(AT(studio, l_C).episode))) + 0xFFFFFFFF;
                msg_post(0x10E, (u16)(i8)(AT(studio, l_C).renter), MK32(((i16)di) >> 0xF, di), MK32(((i16)(AT(studio, l_C).script)) >> 0xF, AT(studio, l_C).script), t2, MK32(((i16)(AT(studio, l_C).end_hour)) >> 0xF, AT(studio, l_C).end_hour), 0);
                t3 = AT(studio, l_C).script;
                l_14 = AT(studio, l_C).script;
                l_8 = screenplays_at(t3);
                l_12 = AT(screenplay, l_8).target;
                l_E = l_12;
                if (((u16)(((i16)((AT(screenplay, l_8).flags_16) << 0xF)) >> 0xF)) != 0) {
                    l_E = l_E + 0x3E8;
                }
                if (!((((u16)(((i16)((AT(screenplay, l_8).flags_16) << 0xF)) >> 0xF)) == 0 || (AT(studio, l_C).episode) != 2) && ((u16)(((i16)((AT(screenplay, l_8).flags_16) << 0xF)) >> 0xF)) != 0)) {
                    prog_give_to_player(l_E, (u16)(i8)(AT(studio, l_C).renter), 0);
                }
                if (((u16)(((i16)((AT(screenplay, l_8).flags_16) << 0xF)) >> 0xF)) != 0) {
                    if (((i16)l_E) > 1000) {
                        t12 = (u16)((i32)(i16)l_E % 1000);
                        l_10 = t12;
                        l_4 = series_at(t12);
                        AT(series, l_4).flags_16 = (AT(series, l_4).flags_16) | 0x20;
                        FP16(l_4, ((((u16)(i8)(AT(studio, l_C).episode)) - 1) * 3) + 2) = (FP16(l_4, ((((u16)(i8)(AT(studio, l_C).episode)) - 1) * 3) + 2)) | 1;
                        if (!((AT(series, l_4).next_ep) != SGENRE_FAMILY || ((i8)(AT(studio, l_C).episode)) <= 2)) {
                            si = 1;
                            while (((i16)((u16)(i8)(AT(studio, l_C).episode))) > ((i16)si)) {
                                LOOP_TICK(21);
                                if (((u8)((u8)((((u8)((u8)(((u16)(si * 3)) >> 8))) << 8) | FP8(l_4, si * 3)))) == (FP8(l_4, (si * 3) + 1))) break;
                                si = si + 1;
                            }
                            if (((i16)((u16)(i8)(AT(studio, l_C).episode))) > ((i16)si)) {
                                AT(series, l_4).next_ep = si;
                            }
                        }
                    } else {
                        films(l_E).flags_16 = (films(l_E).flags_16) | 0x20;
                        if (((u16)(((i16)((AT(screenplay, l_8).flags_16) << 0xD)) >> 0xF)) != 0) {
                            films(l_E).flags2_16 = (films(l_E).flags2_16) | 4;
                        }
                    }
                }
                ax = ((i16)((AT(screenplay, l_8).flags_16) << 0xF)) >> 0xF;
                if (!((((u16)(((i16)((AT(screenplay, l_8).flags_16) << 0xF)) >> 0xF)) == 0 || ((i8)((u8)((((u8)((u8)(ax >> 8))) << 8) | (AT(studio, l_C).episode)))) <= ((i8)(series(AT(screenplay, l_8).target).episodes))) && l_14 != 0x18)) {
                    AT(screenplay, l_8).state = 0x63;
                    studio_cancel_production(di);
                }
                if (((u16)(((i16)((AT(screenplay, l_8).flags_16) << 0xF)) >> 0xF)) != 0 || ((u16)(((i16)((AT(screenplay, l_8).flags_16) << 0xD)) >> 0xF)) == 0) {
                    AT(studio, l_C).running = 0;
                    if ((AT(studio, l_C).reserved) != 0xFF) {
                        if (((i8)(AT(studio, l_C).reserved)) > 0) {
                            studio_take_over(di, (u16)(i8)(AT(studio, l_C).reserved));
                        }
                    } else {
                        studio_release(di);
                    }
                }
                if (g_room == ROOM_PLANNER) {
                    if (((u16)((u16)(i8)(studios(di).renter))) == g_room_owner) {
                        cursor_shape_push(3);
                        txt_film_cache_free(0);
                        txt_series_cache_free(0);
                        txt_film_cache_load(0x1F, g_room_owner, 0x1F);
                        txt_series_cache_load(0x1F, g_room_owner, 0x1F);
                        cursor_shape_push(0);
                        if (g_room_mode == 2) {
                            ui_library_genre_popup(0x13, 8, 1, 0);
                            ui_library_genre_popup(0x13, 8, 0, g_room_owner);
                            present_frame();
                        }
                    } else {
L_06B6:
                        if (g_room == ROOM_LIBRARY) {
                            if (((u16)((u16)(i8)(studios(di).renter))) == g_room_owner) {
                                library_refresh();
                            }
                        }
                    }
                } else {
                    goto L_06B6;
                }
            }
        }
        di = di + 1;
    }
    return;
}

/* 1DA8:06EA  ?(player, id, genre; genre 0 = derive from id) counts archive entries of that genre + auction lots held by the player + running productions of that genre; >3 shows a "category full" message and returns -1, else 0 (max 4 programs per genre) */
void archive_genre_full(u16 player, u16 id, u16 genre) {
    u16 di;
    u16 si;
    u16 l_6 = 0;                         /* an auction lot's programme id ... */
    u16 l_8 = 0;                         /* ... and its genre */
    u32 l_4;                             /* far pointer to a studio's production record */
    FUNC_TICK(49);
    if (genre == 0) {
        prog_id_genre((u8 *)&id, (u8 *)&genre);   /* (the original's argument slots; no code calls this function) */
    }
    si = 1;
    di = (u16)(i8)(library_count(player, genre));
    while (((i16)si) <= 10) {
        LOOP_TICK(35);
        if (((u16)((u16)(i8)(auction(si).bidder))) == player) {
            l_6 = auction(si).program;
            prog_id_genre((u8 *)&l_6, (u8 *)&l_8);
            if (l_8 == genre) {
                di = di + 1;
            }
        }
        si = si + 1;
    }
    si = 1;
    while (((i16)si) <= 12) {
        LOOP_TICK(68);
        l_4 = MK32(DSEG, (si << 4) + 0x6165);
        if (((u16)((u16)(i8)(FP8(l_4, 3)))) == player && (FP16(l_4, 0)) != 0) {
            if (((u16)(((i16)((screenplays(FP16(l_4, 0)).flags_16) << 0xF)) >> 0xF)) == 0 || (FP8(l_4, 4)) != 1 || genre != 0xB) {
                if (!(((u16)(((i16)((screenplays(FP16(l_4, 0)).flags_16) << 0xF)) >> 0xF)) != 0 || ((u16)((u16)(i8)(films(screenplays(FP16(l_4, 0)).target).genre))) != genre)) {
                    di = di + 1;
                }
            } else {
                di = di + 1;
            }
        }
        si = si + 1;
    }
    if (((i16)di) > 3) {
        ui_warning(0x58);
    }
    return;
}

/* 1DA8:085D  split a program id: id <= 1000 -> genre = movie genre byte; else id -= 1000 and genre = 11 (series) */
void prog_id_genre(u8 *a0, u8 *a2) {
    FUNC_TICK(22);
    if (((i16)HP16(a0, 0)) <= 1000) {
        HP16(a2, 0) = (u16)(i8)(films(HP16(a0, 0)).genre);
    } else {
        HP16(a0, 0) = HP16(a0, 0) - 0x3E8;
        HP16(a2, 0) = 0xB;
    }
    return;
}

/* 1DA8:0899  ?n of the "n/N" label for the ad planned (id+3000) in schedule slot: starts from the contract's spot counter (+4) or the ad's required spots, minus later planned spots of the same ad */
u16 ad_spot_number(u16 a0, u16 a1) {
    u16 ax;
    u16 di;
    u16 si;
    u16 l_2;
    u16 l_4;
    u16 t2;
    u16 t5;
    FUNC_TICK(53);
    t2 = schedule_ad(a0, a1) + 0xF448;
    l_2 = (u16)(i8)(ad_spots(schedule_ad(a0, a1) + 0xF448));
    si = 0;
    di = t2;
    while (((i16)g_player_ad_count(a0)) > ((i16)si)) {
        LOOP_TICK(20);
        if ((contracts(a0, si).ad) != di) {
            si = si + 1;
            continue;
        } else {
            goto L_0901;
        }
    }
L_0934:
    si = 7;
    while (((i16)(a1 + 1)) <= ((i16)si)) {
        LOOP_TICK(37);
        t5 = schedule_ad(a0, si);
        l_4 = schedule_ad(a0, si);
        if (((u16)(t5 + 0xF448)) != di) {
            if (((u16)(l_4 + 0xF830)) == di) {
                l_2 = ((u16)(i8)(ads(di).spots)) - 1;
            }
        } else {
            l_2 = l_2 - 1;
        }
        si = si - 1;
    }
    ax = l_2;
    return ax;
L_0901:
    l_2 = (u16)(i8)(contracts(a0, si).aired);
    goto L_0934;
}

/* 1DA8:099C  for every player contract whose deadline day (+5) is today: pay the ad's penalty (ledger 5), message 0x10F (liquidation damages) or 0x110 (cancelled) for the human, remove the ad from all ad slots and delete the contract */
void ad_check_deadlines(void) {
    u16 di;
    u16 si;
    u32 l_4;
    u16 l_6;
    FUNC_TICK(17);
    si = 1;
    while (((i16)si) <= 3) {
        LOOP_TICK(165);
        l_6 = 0;
        while (((i16)g_player_ad_count(si)) > ((i16)l_6)) {
            LOOP_TICK(160);
            l_4 = contracts_at(si, l_6);
            if ((AT(contract, l_4).deadline) == g_day) {
                players(si).money = (players(si).money) - (ads(AT(contract, l_4).ad).penalty);
                int as_human = mp_enter_player(si);   /* multiplayer: the ledger of that human */
                if (si == g_player) {
                    ledger_add(LEDGER_DAMAGES, ads(AT(contract, l_4).ad).penalty);
                    if (((ad_penalty_lo(AT(contract, l_4).ad)) | (ad_penalty_hi(AT(contract, l_4).ad))) == 0) {
                        msg_post(0x110, si, MK32(((i16)(AT(contract, l_4).ad)) >> 0xF, AT(contract, l_4).ad), 0, 0, 0, 0);
                    } else {
                        msg_post(0x10F, si, MK32(((i16)(AT(contract, l_4).ad)) >> 0xF, AT(contract, l_4).ad), ads(AT(contract, l_4).ad).penalty, 0, 0, 0);
                    }
                }
                if (as_human) mp_leave();
                di = 1;
                while (((i16)di) < 15) {
                    LOOP_TICK(28);
                    if (schedule_ad(si, di) == (AT(contract, l_4).ad)) {
                        schedule_ad(si, di) = 0;
                    }
                    di = di + 1;
                }
                contract_delete(si, l_6);
                l_6 = l_6 - 1;
            }
            l_6 = l_6 + 1;
        }
        si = si + 1;
    }
    return;
}

/* 1DA8:0B53  daily check of movie 58 (Betty's cultural program): owner who aired it in slots 1..7 gets letter 0x29 (love letter); 3 days without airing -> movie taken away and put into the auction, Betty love (+0xC) halved, letter 0x111; unowned -> put into the auction */
void betty_culture_check(void) {
    u16 di;
    u16 si;
    FUNC_TICK(89);
    si = 0;
    while (((i16)si) < 3) {
        LOOP_TICK(7);
        g_betty_letter(si) = 0;
        si = si + 1;
    }
    if ((films(0x3A).owner) != 0) {
        if (((i16)g_day) > 0) {
            si = 1;
            di = (u16)(i8)(films(0x3A).owner);
            while (((i16)si) <= 7) {
                LOOP_TICK(15);
                if (schedule_program(di, si) == 0x3A) break;
                si = si + 1;
            }
            if (((i16)si) > 7) {
                g_culture_missed_days = g_culture_missed_days + 1;
                if (((i16)g_culture_missed_days) >= 3) {
                    program_withdraw(0x3A);
                    si = 1;
                    while (((i16)si) <= 10) {
                        LOOP_TICK(12);
                        if ((auction(si).program) != 0) {
                            si = si + 1;
                            continue;
                        } else {
                            goto L_0C33;
                        }
                    }
L_0C4C:
                    if (((i16)di) > 0) {
                        if (((i16)di) <= 3) {
                            g_betty_letter(di) = 0x111;
                            players(di).betty_love = ((i16)(players(di).betty_love)) >> 1;
                            ui_draw_betty_meter();
                        }
                    }
                    g_culture_missed_days = 0;
                }
            } else {
                g_betty_letter(di) = 0x29;
                g_culture_missed_days = 0;
            }
        }
    } else {
        si = 1;
        while (((i16)si) <= 10) {
            LOOP_TICK(12);
            if ((auction(si).program) != 0) {
                si = si + 1;
                continue;
            } else {
                goto L_0B94;
            }
        }
    }
    return;
L_0B94:
    auction_add_lot(si, 0x3A, 0xFFFF);
    g_culture_missed_days = 0;
    return;
L_0C33:
    auction_add_lot(si, 0x3A, 0xFFFF);
    goto L_0C4C;
}

/* 1DA8:0CB3  (slot, id, kind) put a movie (kind 0/-1) or series (other, id+1000) into auction slot: current value as price, lot code 4..10 random (or owner) also stored as the program's owner, clears "available" flag; live movies get a new date: */
void auction_add_lot(u16 a0, u16 a1, u16 a2) {
    u16 di;
    u16 si;
    u32 l_4;
    u32 l_8;
    u32 l_C;
    u32 t2;
    u16 t4;
    u16 t11;
    u16 t13;
    u32 t19;
    u16 t21;
    FUNC_TICK(152);
    l_4 = auction_at(a0);
    AT(lot, l_4).program = a1;
    si = a1;
    di = a2;
    if (a2 == 0) {
L_0CEB:
        l_C = films_at(si);
        t2 = movie_price(si);
        AT(lot, l_4).bid = t2;
        if ((AT(film, l_C).owner) == 0 || di == 0xFFFF) {
            t4 = random_n(7, si);
            AT(lot, l_4).seller = ((u8)t4) + 3;
        } else {
            AT(lot, l_4).seller = AT(film, l_C).owner;
        }
        AT(film, l_C).owner = AT(lot, l_4).seller;
        AT(film, l_C).flags_16 = (AT(film, l_C).flags_16) & 0xFFDF;
        if (si != 0x1D) {
            if (si != 0xBB) {
                if ((AT(film, l_C).genre) == GENRE_LIVE) {
                    t11 = random_n(8 - ((u16)(i8)(AT(film, l_C).blocks)), 3);
                    AT(film, l_C).live_slot = t11;
                    t13 = random_n(3, si);
                    AT(film, l_C).live_day = (t13 + g_day) + 3;
                }
            } else {
                AT(film, l_C).live_slot = GENRE_EPIC;
                AT(film, l_C).live_day = g_day + 4;
                g_live187_left = 5;
            }
        } else {
            AT(film, l_C).live_slot = GENRE_COMEDY;
            AT(film, l_C).live_day = g_day + 4;
            g_live29_left = 3;
        }
    } else if (di == 0xFFFF) {
        goto L_0CEB;
    } else {
        AT(lot, l_4).program = (AT(lot, l_4).program) + 0x3E8;
        l_8 = series_at(si);
        t19 = series_price(si);
        AT(lot, l_4).bid = t19;
        if ((AT(series, l_8).owner) != 0) {
            AT(lot, l_4).seller = AT(series, l_8).owner;
        } else {
            t21 = random_n(7, si);
            AT(lot, l_4).seller = (u8)t21;
        }
        AT(series, l_8).owner = AT(lot, l_4).seller;
        AT(series, l_8).flags_16 = (AT(series, l_8).flags_16) & 0xFFDF;
    }
    AT(lot, l_4).idle_days = 0;
    AT(lot, l_4).bidder = 0;
    return;
}

/* 1DA8:0E74  if the player is the human: store message (text id, 5 dword args, day, hour) in the message list 0x60BF[G8(0x7247)] and notify */
void msg_post(u16 a0, u16 a1, u32 a2, u32 a4, u32 a6, u32 a8, u32 a10) {
    u32 l_4;
    if (seat_humans && a1 != g_player && mp_enter_player(a1)) {   /* multiplayer: into that human's office */
        msg_post(a0, a1, a2, a4, a6, a8, a10);
        mp_leave();
        return;
    }
    FUNC_TICK(59);
    l_4 = messages_at((u16)(i8)g_message_pos);
    if (a1 == g_player) {
        AT(message, l_4).text_id = a0;
        FP32(l_4, 2) = a2;
        FP32(l_4, 6) = a4;
        FP32(l_4, 0xA) = a6;
        FP32(l_4, 0xE) = a8;
        FP32(l_4, 0x12) = a10;
        FP16(l_4, 0x16) = g_day;
        FP16(l_4, 0x18) = g_time_slot;
        msg_count_new(a1);
        planner_refresh();
    }
    return;
}

/* 1DA8:0F5B  daily interest on the human's credit: money -= credit/20 (5% per day), ledger 0xF */
void pay_credit_interest(void) {
    u32 t5;
    FUNC_TICK(37);
    if ((g_credit_lo | g_credit_hi) != 0) {
        G32((u16)((g_player * 0x17) + 0x8BDC) + 8) = (players(g_player).money) - ((u32)((i32)g_credit / (i32)0x14));
        t5 = (u32)((i32)g_credit / (i32)0x14);
        ledger_add(LEDGER_INTEREST, t5);
    }
    return;
}

/* 1DA8:0FB9  per player: rent of every owned studio (ledger 8), transmitter fees = sum of transmitter fee values / 5 (ledger 9; AI pays from its budget), news agency subscriptions 1000 (agency 1) / 10000 (agencies 2,3) per day (ledger 0xA) */
void pay_daily_costs(void) {
    u16 ax;
    int cc;
    u16 si;
    u32 l_4;
    u32 l_8;
    u32 l_C;
    u32 l_10;
    u16 l_12;
    u32 t10;
    FUNC_TICK(15);
    l_12 = 1;
    while (((i16)l_12) <= 3) {
        LOOP_TICK(212);
        int as_human = mp_enter_player(l_12);   /* multiplayer: another seat's human pays as a human (runtime/seat.h) */
        l_4 = MK32(DSEG, (l_12 * 0x17) + 0x8BE4);
        si = 1;
        while (((i16)si) <= 12) {
            LOOP_TICK(49);
            l_8 = studios_at(si);
            if (((u16)((u16)(i8)(AT(studio, l_8).renter))) == l_12) {
                FP32(l_4, 0) = FP32(l_4, 0) - (AT(studio, l_8).rent);
                if (l_12 != g_player) {
                    ai(l_12).production = (ai(l_12).production) - (AT(studio, l_8).rent);
                } else {
                    ledger_add(LEDGER_STUDIO_RENT, AT(studio, l_8).rent);
                }
            }
            si = si + 1;
        }
        l_10 = ((u32)((i32)tx(l_12, 0).fee / (i32)5));
        FP32(l_4, 0) = FP32(l_4, 0) - l_10;
        if (l_12 != g_player) {
            /* --ai-improve: the follow-up transmitters' fees not from the pots for films, news
             * and productions -- only their half the transmitter pot pays (ai_improve.c) */
            u32 follow = ai_improve_on() ? (u32)((i32)ai_improve_follow_fees(l_12) / (i32)5) : 0;
            if (ai_improve_on()) l_10 = (u32)((i32)(tx(l_12, 0).fee - ai_improve_follow_fees(l_12)) / (i32)5);
            if ((G8((l_12 * 0x3C) + 0x6E4E)) != 1) {
                l_C = MK32(((i16)HI16(l_10)) >> 1, (LO16(l_10) >> 1) | ((u16)(HI16(l_10) & 1) << 0xF));
                l_10 = MK32(((i16)HI16(l_10)) >> 1, (LO16(l_10) >> 1) | ((u16)(HI16(l_10) & 1) << 0xF));
            } else {
                l_C = l_10;
                l_10 = 0;
            }
            ai(l_12).transmitters = (ai(l_12).transmitters) - l_10 - (u32)((i32)follow / (i32)2);
            t10 = (u32)(l_C * 0xFFFFFFFF);
            ai_budget_distribute((u16)(i8)((u8)l_12), LO16(t10), HI16(t10), 0xFFFF);
        } else {
            ledger_add(LEDGER_TX_FEES, l_10);
        }
        si = 1;
        while (((i16)si) <= 3) {
            LOOP_TICK(58);
            l_10 = 0;
            if ((G8((((l_12 * 0x17) + 0x8BDC) + si) + 0x10)) != 0) {
                cc = si == 1;
                ax = si;
                if (cc) {
                    l_10 = l_10 + 0x3E8;
                } else {
                    cc = ax == 2;
                    if (cc || ax == 3) {
                        l_10 = l_10 + 0x2710;
                    }
                }
                FP32(l_4, 0) = FP32(l_4, 0) - l_10;
                if (l_12 != g_player) {
                    ai(l_12).news = (ai(l_12).news) - l_10;
                } else {
                    ledger_add(LEDGER_NEWS, l_10);
                }
            }
            si = si + 1;
        }
        if (as_human) mp_leave();
        l_12 = l_12 + 1;
    }
    return;
}

/* 1DA8:1227  end of day: untaken ad offers released, screenplay offers expire after 2 days unsold, film dealer offers returned to the pool (flag 0x20) */
void market_day_cleanup(void) {
    u16 si;
    u32 l_4;
    u32 l_8;
    u32 l_C;
    FUNC_TICK(14);
    si = 1;
    while (((i16)si) <= (i16)g_nadoffers) {   /* 10 in the original (--ad-offers) */
        LOOP_TICK(29);
        l_C = ad_offers_at(si);
        if (!((AT(ad_offer, l_C).shelf) != 0 || (AT(ad_offer, l_C).ad) == 0)) {
            ads(AT(ad_offer, l_C).ad).holder = 0;
            AT(ad_offer, l_C).ad = 0;
        }
        si = si + 1;
    }
    si = 1;
    while (((i16)si) <= 5) {
        LOOP_TICK(32);
        l_8 = script_offers_at(si);
        if (!((AT(script_offer, l_8).status) != 0 || (AT(script_offer, l_8).script) == 0)) {
            FP8(l_8, 3) = FP8(l_8, 3) + 1;
            if (((i8)FP8(l_8, 3)) >= 2) {
                screenplays(AT(script_offer, l_8).script).state = 0;
                AT(script_offer, l_8).script = 0;
            }
        }
        si = si + 1;
    }
    si = 1;
    while (((i16)si) <= (i16)g_nfilmoffers) {   /* 10 in the original (--film-offers) */
        LOOP_TICK(36);
        l_4 = dealer_at(si);
        if ((AT(offer, l_4).program) != 0) {
            if (((i16)(AT(offer, l_4).program)) >= 1000) {
                series_flags((AT(offer, l_4).program) + 0xFC18) = (series_flags((AT(offer, l_4).program) + 0xFC18)) | 0x20;
            } else {
                films(AT(offer, l_4).program).flags_16 = (films(AT(offer, l_4).program).flags_16) | 0x20;
            }
            AT(offer, l_4).program = 0;
        }
        si = si + 1;
    }
    return;
}

/* 1DA8:135D  for each player's live programs (archive genre 7) dated today: movie 29/187 get rescheduled (recurring), others are lost with message 0x113 "You missed the %s live" */
void live_check_missed(void) {
    int cc;
    u16 di;
    u16 si;
    u16 l_2;
    FUNC_TICK(14);
    di = 1;
    while (((i16)di) <= 3) {
        LOOP_TICK(90);
        l_2 = 0;
        while (((i16)((u16)(i8)library_count(di, 7))) > ((i16)l_2)) {
            LOOP_TICK(85);
            cc = library_items(di, 7, l_2) == 0x1D;
            si = library_items(di, 7, l_2);
            if (cc) {
L_13DC:
                if (si != 0x1D) {
L_13FF:
                    cc = si != 0xBB;
                    if (!(cc || (films(si).live_day) != g_day)) {
                        live187_reschedule();
                    }
                } else if ((films(si).live_day) != g_day) {
                    goto L_13FF;
                } else {
                    live29_reschedule();
                }
            } else if (si == 0xBB) {
                goto L_13DC;
            } else if ((films(si).live_day) != g_day) {
                goto L_13DC;
            } else {
                program_withdraw(si);
                msg_post(0x113, di, MK32(((i16)si) >> 0xF, si), 0, 0, 0, 0);
            }
            l_2 = l_2 + 1;
        }
        di = di + 1;
    }
    return;
}

/* 1DA8:1451  pick today's 15 actors: slot i takes actor i of a random group (0..5, not 4) of the actor pool 0x5013, names unique; copied to 0x5493 */
void actors_daily_offer(void) {
    int cc;
    u16 di;
    u16 si;
    u16 t1;
    u16 l_4 = 0;
    u16 l_2 = 0;
    FUNC_TICK(14);
    si = 1;
    while (((i16)si) <= 15) {
        LOOP_TICK(85);
        l_2 = 0;
        while (l_2 == 0) {
            LOOP_TICK(55);
            l_2 = 1;
            for (;;) {
                LOOP_TICK(22);
                t1 = random_n(6, si);
                l_4 = t1 - 1;
                cc = ((G16(((((t1 - 1) * 0xC0) + 0x5013) + (si * 0xC)) + 2)) | (G16(((((t1 - 1) * 0xC0) + 0x5013) + (si * 0xC)) + 4))) == 0;
                if (cc || l_4 == 4) {
                    continue;
                } else {
                    break;
                }
            }
            di = 1;
            while (((i16)di) < ((i16)si)) {
                LOOP_TICK(27);
                if ((actors(l_4, si).name) == (actors(6, di).name)) {
                    l_2 = 0;
                }
                di = di + 1;
            }
        }
        far_memcpy(HPTR(actors_at(6, si)), HPTR(actors_at(l_4, si)), 0xC);
        si = si + 1;
    }
    return;
}

/* Starting pools (an extension, see newgame_starting_programs): is one of the
 * marked films (or series) still free? */
static int start_pool_free(const u16 *pool, u16 n, int film_pool) {
    for (u16 k = 0; k < n; k++)
        if ((film_pool ? films(pool[k]).owner : series(pool[k]).owner) == 0) return 1;
    return 0;
}

/* Does one of the human's programmes fit the first evening from slot `slot`
 * on (what the draw loop of newgame_starting_programs accepts)?  With the
 * original data always; a mod's starting programmes may all be too long for
 * the last slots -- then the rest of the evening stays empty, where the
 * original would draw for ever (an extension, docs/CHANGES.md).  Plain C, no
 * game time. */
static int start_slot_fits(u16 slot) {
    for (u16 k = 1; k <= g_nfilms; k++)
        if ((u16)(i8)films(k).owner == g_player && (i16)((u16)(i8)films(k).blocks + slot - 1) <= 7 && !(films(k).flags2 & 2))
            return 1;
    for (u16 k = 1; k <= g_nseries; k++)
        if ((u16)(i8)series(k).owner == g_player && (i16)((u16)(i8)series(k).blocks + slot - 1) <= 7)
            return 1;
    return 0;
}

/* 1DA8:1538  new game: every player gets one free series (random of 18..20) and 3 free genre-6 movies with flag 0x20; the human's first evening (slots 1..7) is filled with random owned programs */
/* the human's first evening (slots 1..7) filled with random owned programs -- the end of
 * newgame_starting_programs, for g_player; *seed is its draw counter (l_12) */
static void start_evening_fill(u16 *seed) {
    u16 di;
    u16 si;
    u32 l_4;
    u32 l_8;
    u32 l_C;
    u16 l_12 = *seed;
    u16 l_14;
    u16 t7;
    u16 t8;
    u16 t10;
    u16 t20;
    l_4 = schedule_at(g_player);
    l_14 = 1;
    while (((i16)l_14) <= 7) {
        LOOP_TICK(150);
        if (!start_slot_fits(l_14)) break;   /* the original draws for ever (an extension) */
        t7 = l_12;
        l_12 = l_12 + 1;
        t8 = random_n(g_nfilms + g_nseries, t7);   /* films and series: 0xFA (200 + 50) in the original */
        si = t8;
        if (((i16)t8) <= (i16)g_nfilms) {
            l_C = films_at(si);
            if (((u16)((u16)(i8)(AT(film, l_C).owner))) != g_player) continue;
            t10 = (((u16)(i8)(AT(film, l_C).blocks)) + l_14) - 1;
            if (((i16)(t10 - ((u16)((u16)((i32)(i16)(l_14 - 1) / 7)) * 7))) > 7 || ((u16)(((i16)((AT(film, l_C).flags2_16) << 0xE)) >> 0xF)) != 0) continue;
            FP16(l_4, l_14 << 1) = si;
            di = l_14 + 1;
            while (((i16)((((u16)(i8)(AT(film, l_C).blocks)) + l_14) - 1)) >= ((i16)di)) {
                LOOP_TICK(13);
                FP16(l_4, di << 1) = 0x270F;
                di = di + 1;
            }
            l_14 = l_14 + ((u16)(i8)(AT(film, l_C).blocks));
            AT(film, l_C).flags2_16 = (AT(film, l_C).flags2_16) | 2;
            continue;
        } else {
            l_8 = MK32(FPSEG(g_series), FPOFF(g_series) + ((si - g_nfilms) * 0x39));
            si = si - g_nfilms;
            if (((u16)((u16)(i8)FP8(l_8, 0x24))) != g_player) continue;
            t20 = (((u16)(i8)FP8(l_8, 0x23)) + l_14) - 1;
            if (((i16)(t20 - ((u16)((u16)((i32)(i16)(l_14 - 1) / 7)) * 7))) > 7) continue;
            FP16(l_4, l_14 << 1) = si + 0x3E8;
            if (FP16(l_8, 0x31) == 0) {
                FP16(l_8, 0x31) = 1;
            }
            di = l_14 + 1;
            while (((i16)((((u16)(i8)FP8(l_8, 0x23)) + l_14) - 1)) >= ((i16)di)) {
                LOOP_TICK(25);
                FP16(l_4, di << 1) = 0x270F;
                if (((i16)l_14) <= 7) {
                    FP16(l_4, (di << 1) + 0x1E) = series_episode_at(g_player, si, di);
                }
                di = di + 1;
            }
            l_14 = l_14 + ((u16)(i8)FP8(l_8, 0x23));
            continue;
        }
    }
    *seed = l_12;
}

void newgame_starting_programs(void) {
    u16 di;
    u16 si;
    u32 l_4;
    u32 l_8;
    u32 l_C;
    u16 l_E;
    u16 l_10;
    u16 l_12;
    u16 l_14;
    u16 l_16;
    u16 t1;
    u16 t2;
    u16 t3;
    u16 t7;
    u16 t8;
    u16 t10;
    u16 t20;
    /* Starting pools (an extension, docs/CHANGES.md): films and series a mod marks with
     * flags2 bit 4 (unused by the original) come first; while none of them is free any
     * more, the original pools fill up -- the series 18-20 and the cultural programmes on
     * the market.  The original data marks none, so the original rule runs unchanged;
     * plain C counting, no game time. */
    u16 spool[SERIES_MAX + 1], fpool[FILMS_MAX + 1], ns = 0, nf = 0;
    FUNC_TICK(35);
    for (u16 k = 1; k <= g_nseries; k++) if (series(k).flags2 & 0x10) spool[ns++] = k;
    for (u16 k = 1; k <= g_nfilms; k++) if (films(k).flags2 & 0x10) fpool[nf++] = k;
    l_12 = 1;
    l_16 = 1;
    while (((i16)l_16) <= 3) {
        LOOP_TICK(77);
        for (;;) {
            LOOP_TICK(15);
            if (start_pool_free(spool, ns, 0)) {   /* the mod's series pool, while one is free */
                t1 = random_n(ns, l_12);
                si = spool[t1 - 1];
                if ((series(si).owner) == 0) break;
                l_12 = l_12 + 1;
                continue;
            }
            t1 = random_n(3, l_12);
            si = t1 + 0x11;
            if ((series(t1 + 0x11).owner) == 0) break;
            l_12 = l_12 + 1;
        }
        prog_give_to_player(si + 0x3E8, l_16, 0);
        l_E = 0;
        l_10 = 1;
        while (!(((i16)l_E) >= 0 && (((i16)l_E) > 0 || l_10 > 3))) {
            LOOP_TICK(47);
            for (;;) {
                LOOP_TICK(33);
                if (start_pool_free(fpool, nf, 1)) {   /* the mod's film pool (any genre), while one is free */
                    t2 = random_n(nf, l_12);
                    si = fpool[t2 - 1];
                    if ((films(si).owner) == 0) break;
                    l_12 = l_12 + 1;
                    continue;
                }
                t2 = random_n(g_nfilms, l_12);
                si = t2;
                if ((films(t2).owner) != 0) {
                    l_12 = l_12 + 1;
                    continue;
                }
                if ((films(si).genre) != GENRE_CULTURE) {
                    l_12 = l_12 + 1;
                    continue;
                }
                if (((u16)(((i16)((films(si).flags_16) << 0xA)) >> 0xF)) != 0) break;
                l_12 = l_12 + 1;
            }
            prog_give_to_player(si, l_16, 0);
            t3 = l_10;
            l_10 = l_10 + 1;
            l_E = l_E + ((u32)t3 + 1 > 0xFFFF);
        }
        l_16 = l_16 + 1;
    }
    start_evening_fill(&l_12);
    if (seat_humans) {   /* multiplayer: every human's first evening, not only seat 0's (the
                          * others' stations are no computer player's: nobody else fills them) */
        u16 me = g_player;
        for (u16 p = 1; p <= 3; p++) {
            if (p == me || !mp_is_human(p)) continue;
            g_player = p;
            start_evening_fill(&l_12);
            g_player = me;
        }
    }
    return;
}

/* 1DA8:17E3  population inside a transmitter circle at map x,y (population map AND circle mask, weighted) */
u32 transmitter_reach(u16 a0, u16 a1) {
    u16 ax;
    int cc;
    u16 dx;
    u16 si;
    u32 l_4;
    u32 l_8;
    u32 l_C;
    u32 l_10;
    u32 l_14;
    u16 l_16;
    u16 l_18;
    FUNC_TICK(56);
    l_18 = (a0 & 7) << 1;
    l_16 = 0x10 - l_18;
    l_14 = mem_calloc(0x40, 1);
    l_C = l_14;
    l_8 = DSP(0x11F4);
    l_10 = MK32(FPSEG(g_population_map), FPOFF(g_population_map) + (((a1 * 0x15) + (((i16)(((i16)(((i16)a0) >> 1)) >> 1)) >> 1)) << 1));
    si = 0;
    for (;;) {
        LOOP_TICK(35);
        cc = ((i16)si) < 16;
        si = si + 1;
        if (!(cc)) break;
        FP16(l_C, 0) = ((FP16(l_10, 0) >> ((u8)((u8)l_18))) | (FP16(l_10, 2) << ((u8)((u8)l_16)))) & FP16(l_8, 0);
        l_8 = MK32(HI16(l_8), LO16(l_8) + 2);
        l_C = MK32(HI16(l_C), LO16(l_C) + 2);
        FP16(l_C, 0) = ((FP16(l_10, 2) >> ((u8)((u8)l_18))) | (FP16(l_10, 4) << ((u8)((u8)l_16)))) & FP16(l_8, 0);
        l_8 = MK32(HI16(l_8), LO16(l_8) + 2);
        l_C = MK32(HI16(l_C), LO16(l_C) + 2);
        l_10 = MK32(HI16(l_10), LO16(l_10) + 0x2A);
    }
    l_4 = block_population(l_14);
    mem_free(LO16(l_14), HI16(l_14));
    ax = LO16(l_4);
    dx = HI16(l_4);
    return MK32(dx, ax);
}

/* 1DA8:18E0  density weight (table 0x1274: 0/1/10/100) of the 2-bit map pixel at x,y */
u16 map_density_at(u16 a0, u16 a1, u32 a2) {
    u16 ax;
    FUNC_TICK(24);
    ax = G16(((((FP16(a2, ((a1 * 0x15) + (((i16)(((i16)(((i16)a0) >> 1)) >> 1)) >> 1)) << 1)) >> ((u8)((((u8)a0) & 7) << 1))) & 3) << 1) + 0x1274);
    return ax;
}

/* 1DA8:1918  sum of density weights (0x127C: 0/1/10/100 per pixel) over a map rectangle, x step 8 px */
u32 map_sum_population(u16 a0, u16 a1, u16 a2, u16 a3, u32 a4) {
    u16 ax;
    u16 di;
    u16 dx;
    u32 l_4;
    u16 l_6;
    u16 l_8;
    FUNC_TICK(20);
    l_4 = 0;
    l_8 = a1;
    while (l_8 < a3) {
        LOOP_TICK(71);
        l_6 = l_8 * 0x15;
        di = a0;
        while (di < a2) {
            LOOP_TICK(61);
            l_4 = l_4 + (MK32(((i16)((((((((g_density_weight((FP16(a4, ((((di >> 1) >> 1) >> 1) + l_6) << 1)) & 3)) + (g_density_weight((((FP16(a4, ((((di >> 1) >> 1) >> 1) + l_6) << 1)) >> 1) >> 1) & 3))) + (g_density_weight(((FP16(a4, ((((di >> 1) >> 1) >> 1) + l_6) << 1)) >> 4) & 3))) + (g_density_weight(((FP16(a4, ((((di >> 1) >> 1) >> 1) + l_6) << 1)) >> 6) & 3))) + (g_density_weight(((FP16(a4, ((((di >> 1) >> 1) >> 1) + l_6) << 1)) >> 8) & 3))) + (g_density_weight(((FP16(a4, ((((di >> 1) >> 1) >> 1) + l_6) << 1)) >> 0xA) & 3))) + (g_density_weight(((FP16(a4, ((((di >> 1) >> 1) >> 1) + l_6) << 1)) >> 0xC) & 3))) + (g_density_weight(((FP16(a4, ((((di >> 1) >> 1) >> 1) + l_6) << 1)) >> 0xE) & 3)))) >> 0xF, (((((((g_density_weight((FP16(a4, ((((di >> 1) >> 1) >> 1) + l_6) << 1)) & 3)) + (g_density_weight((((FP16(a4, ((((di >> 1) >> 1) >> 1) + l_6) << 1)) >> 1) >> 1) & 3))) + (g_density_weight(((FP16(a4, ((((di >> 1) >> 1) >> 1) + l_6) << 1)) >> 4) & 3))) + (g_density_weight(((FP16(a4, ((((di >> 1) >> 1) >> 1) + l_6) << 1)) >> 6) & 3))) + (g_density_weight(((FP16(a4, ((((di >> 1) >> 1) >> 1) + l_6) << 1)) >> 8) & 3))) + (g_density_weight(((FP16(a4, ((((di >> 1) >> 1) >> 1) + l_6) << 1)) >> 0xA) & 3))) + (g_density_weight(((FP16(a4, ((((di >> 1) >> 1) >> 1) + l_6) << 1)) >> 0xC) & 3))) + (g_density_weight(((FP16(a4, ((((di >> 1) >> 1) >> 1) + l_6) << 1)) >> 0xE) & 3))));
            di = di + 8;
        }
        l_8 = l_8 + 1;
    }
    ax = LO16(l_4);
    dx = HI16(l_4);
    return MK32(dx, ax);
}

/* 1DA8:1A03  rebuild player's coverage map, store covered population (all map) at +4 and total transmitter fee values at +0 of the player's transmitter block; returns fees */
u16 transmitters_recalc(u16 a0) {
    u16 di;
    u16 si;
    u32 l_4;
    u32 l_8;
    u32 t1;
    FUNC_TICK(49);
    l_8 = 0;
    l_4 = tx_at(a0, 0);
    coverage_build(a0);
    t1 = map_sum_population(0, 0, 0xA7, 0x6C, g_coverage_map);
    AT(transmitter, l_4).reach = t1;
    si = 1;
    di = a0;
    while (((i16)si) <= 32) {
        LOOP_TICK(32);
        if (((contracts(5, ((di * 0x42) + (si * 2)) + 9).ad) | (G16((((di * 0x1CE) + 0x1D09) + (si * 0xE)) + 6))) != 0) {
            l_8 = l_8 + (tx(di, si).fee);
        }
        si = si + 1;
    }
    AT(transmitter, l_4).fee = l_8;
    return LO16(l_8);
}

/* 1DA8:1ACB  clear coverage map 0x8BAA and OR in every active transmitter of the player: slot 1 = whole west half, slot 2 = whole east half (satellites?), others = circle; stores covered population per half in 0x7AD2/0x7AD6 */
void coverage_build(u16 a0) {
    u16 ax;
    u16 di;
    u16 si;
    u16 l_2;
    u16 l_4;
    u32 t19;
    u32 t20;
    FUNC_TICK(55);
    mem_zero(g_coverage_map, 0x11E2);
    l_4 = 1;
    while (((i16)l_4) <= 32) {
        LOOP_TICK(137);
        if (((contracts(5, ((a0 * 0x42) + (l_4 * 2)) + 9).ad) | (G16((((a0 * 0x1CE) + 0x1D09) + (l_4 * 0xE)) + 6))) != 0 && (tx(a0, l_4).state) != 2) {
            ax = l_4;
            if (l_4 == 1) {
                l_2 = 0;
                while (((i16)l_2) < 108) {
                    LOOP_TICK(45);
                    si = 0;
                    di = l_2 * 0x15;
                    while (((i16)si) < 10) {
                        LOOP_TICK(15);
                        FP16(g_coverage_map, (di + si) << 1) = FP16(g_population_map, (di + si) << 1);
                        si = si + 1;
                    }
                    FP16(g_coverage_map, (di + 0xA) << 1) = (u16)(i8)((FP8(g_coverage_map, (di + 0xA) << 1)) | ((FP8(g_population_map, (di + 0xA) << 1)) & 0));
                    l_2 = l_2 + 1;
                }
            } else if (ax != 2) {
                coverage_stamp(l_4, a0, g_population_map, g_coverage_map);
            } else {
                l_2 = 0;
                while (((i16)l_2) < 108) {
                    LOOP_TICK(44);
                    si = 0xB;
                    di = l_2 * 0x15;
                    while (((i16)si) < 21) {
                        LOOP_TICK(15);
                        FP16(g_coverage_map, (di + si) << 1) = FP16(g_population_map, (di + si) << 1);
                        si = si + 1;
                    }
                    FP16(g_coverage_map, (di + 0xA) << 1) = (FP16(g_coverage_map, (di + 0xA) << 1)) | ((FP16(g_population_map, (di + 0xA) << 1)) & 0xFF);
                    l_2 = l_2 + 1;
                }
            }
        }
        l_4 = l_4 + 1;
    }
    t19 = map_sum_population(0, 0, 0x54, 0x6D, g_coverage_map);
    g_coverage_west = t19;
    t20 = map_sum_population(0x54, 0, 0xA8, 0x6D, g_coverage_map);
    g_coverage_east = t20;
    return;
}

/* 1DA8:1CC2  (floor, door, &x, &y) position of a door on the building directory board: y = (14-floor)*15+9, x from door x (0x50/0x74/0xB4/0xD8 -> 0x26/0x3D/0x6A/0x81); floors <= 7 in the right column */
void door_board_pos(u16 a0, u16 a1, u8 *a2, u8 *a4) {
    u16 ax;
    u16 si;
    u16 t1;
    FUNC_TICK(53);
    HP16(a4, 0) = ((0xE - a0) * 0xF) + 9;
    t1 = floors_door_x(a0, a1);
    ax = floors_door_x(a0, a1);
    si = a0;
    if (floors_door_x(a0, a1) == 0xB4) {
        HP16(a2, 0) = 0x6A;
    } else if (((i16)t1) > 180) {
        if (ax == 0xD8) {
            HP16(a2, 0) = 0x81;
        }
    } else if (ax == 0x50) {
        HP16(a2, 0) = 0x26;
    } else if (ax == 0x74) {
        HP16(a2, 0) = 0x3D;
    }
    if (((i16)si) <= 7) {
        HP16(a4, 0) = HP16(a4, 0) - 0x69;
        HP16(a2, 0) = HP16(a2, 0) + 0xA0;
    }
    return;
}

/* 1DA8:1D4E  new string with a door's label: text +0x28 (or +0x32) of the floor table, combined "%s %s" with the owner's player name (room type 1) or station name */
u32 door_label_text(u16 a0, u16 a1) {
    u16 ax;
    u16 di;
    u16 dx;
    u16 si;
    u32 l_4;
    u32 l_8;
    u32 t3;
    u32 t6;
    u32 t9;
    u32 t12;
    FUNC_TICK(144);
    l_4 = floors_at(a0);
    si = a1;
    di = FP8(l_4, a1 + 0x46);
    if (FP8(l_4, a1 + 0x32) != 0) {
        if (di == 0) {
            t6 = str_format(g_text(FP8(l_4, si + 0x32)));
            ax = LO16(t6);
            dx = HI16(t6);
        } else {
            if (FP8(l_4, si + 0x1E) != 1) {
                ax = pl_station_lo(di);
                dx = pl_station_hi(di);
            } else {
                ax = pl_name_lo(di);
                dx = pl_name_hi(di);
            }
            t3 = str_format(DSP(0x12B8) /* "#s #s" */, W2(g_text(FP8(l_4, si + 0x32))), ax, dx);
            ax = LO16(t3);
            dx = HI16(t3);
        }
        l_8 = MK32(dx, ax);
    } else {
        if (di == 0) {
            t12 = str_format(g_text(FP8(l_4, si + 0x28)));
            ax = LO16(t12);
            dx = HI16(t12);
        } else {
            if (FP8(l_4, si + 0x1E) != 1) {
                ax = pl_station_lo(di);
                dx = pl_station_hi(di);
            } else {
                ax = pl_name_lo(di);
                dx = pl_name_hi(di);
            }
            t9 = str_format(DSP(0x12BE) /* "#s #s" */, W2(g_text(FP8(l_4, si + 0x28))), ax, dx);
            ax = LO16(t9);
            dx = HI16(t9);
        }
        l_8 = MK32(dx, ax);
    }
    ax = LO16(l_8);
    dx = HI16(l_8);
    return MK32(dx, ax);
}

/* 1DA8:1EC6  current clock hour from the time slot counter */
u16 clock_hour(void) {
    u16 ax;
    FUNC_TICK(9);
    if (((i16)g_time_slot) < 17) {
        ax = g_time_slot + 7;
    } else {
        ax = g_time_slot + 0xFFEF;
    }
    return ax;
}

/* 1DA8:1EDE  clock_hour, +1 once minute >= 5 (news part of the hour is over) */
u16 clock_hour_round05(void) {
    u16 ax;
    FUNC_TICK(10);
    if (((i16)g_minute) < 5) {
        ax = clock_hour();
    } else {
        ax = clock_hour() + 1;
    }
    return ax;
}

/* 1DA8:1EF3  clock_hour, +1 once minute >= 55 (ad break started) - first schedule slot still open */
u16 clock_hour_round55(void) {
    u16 ax;
    FUNC_TICK(10);
    if (((i16)g_minute) < 55) {
        ax = clock_hour();
    } else {
        ax = clock_hour() + 1;
    }
    return ax;
}

/* 1DA8:1F08  OR the 32x16 transmitter circle mask (0x11F4) of a player's transmitter slot onto the coverage map, limited to populated map pixels */
void coverage_stamp(u16 a0, u16 a1, u32 a2, u32 a4) {
    u16 si;
    u32 l_4;
    u32 l_8;
    u32 l_C;
    u16 l_E;
    u16 l_10;
    u16 l_12;
    u16 t1;
    FUNC_TICK(72);
    t1 = tx(a1, a0).x;
    l_12 = tx(a1, a0).y;
    l_10 = (t1 & 7) << 1;
    l_E = 0x10 - l_10;
    l_8 = MK32(FPSEG(a2), FPOFF(a2) + (((l_12 * 0x15) + (((i16)(((i16)(((i16)t1) >> 1)) >> 1)) >> 1)) << 1));
    l_C = MK32(FPSEG(a4), FPOFF(a4) + (((l_12 * 0x15) + (((i16)(((i16)(((i16)t1) >> 1)) >> 1)) >> 1)) << 1));
    l_4 = DSP(0x11F4);
    si = 0;
    while (((i16)si) < 16) {
        LOOP_TICK(42);
        FP16(l_C, 0) = ((FP16(l_4, 0) << ((u8)((u8)l_10))) & FP16(l_8, 0)) | FP16(l_C, 0);
        FP16(l_C, 2) = (((FP16(l_4, 0) >> ((u8)((u8)l_E))) | (FP16(l_4, 2) << ((u8)((u8)l_10)))) & FP16(l_8, 2)) | FP16(l_C, 2);
        FP16(l_C, 4) = (FP16(l_8, 4) & (FP16(l_4, 2) >> ((u8)((u8)l_E)))) | FP16(l_C, 4);
        l_4 = MK32(HI16(l_4), LO16(l_4) + 4);
        l_C = MK32(HI16(l_C), LO16(l_C) + 0x2A);
        l_8 = MK32(HI16(l_8), LO16(l_8) + 0x2A);
        si = si + 1;
    }
    return;
}

/* 1DA8:2036  extra population a transmitter at x,y would add to the current coverage (covered-after minus covered-before inside the circle) */
u32 transmitter_new_reach(u16 a0, u16 a1) {
    u16 ax;
    int cc;
    u16 di;
    u16 dx;
    u16 si;
    u32 l_4;
    u32 l_8;
    u32 l_C;
    u32 l_10;
    u32 l_14;
    u32 l_18;
    u16 l_1A;
    u16 l_1C;
    u32 t64;
    FUNC_TICK(87);
    l_1C = (a0 & 7) << 1;
    l_1A = 0x10 - l_1C;
    l_10 = mem_calloc(0x40, 1);
    l_8 = l_10;
    l_C = MK32(FPSEG(g_coverage_map), FPOFF(g_coverage_map) + (((a1 * 0x15) + (((i16)(((i16)(((i16)a0) >> 1)) >> 1)) >> 1)) << 1));
    l_4 = DSP(0x11F4);
    si = 0;
    di = a0;
    for (;;) {
        LOOP_TICK(30);
        cc = ((i16)si) < 16;
        si = si + 1;
        if (!(cc)) break;
        FP16(l_8, 0) = (FP16(l_C, 0) >> ((u8)((u8)l_1C))) | (FP16(l_C, 2) << ((u8)((u8)l_1A)));
        l_C = MK32(HI16(l_C), LO16(l_C) + 2);
        l_8 = MK32(HI16(l_8), LO16(l_8) + 2);
        FP16(l_8, 0) = (FP16(l_C, 0) >> ((u8)((u8)l_1C))) | (FP16(l_C, 2) << ((u8)((u8)l_1A)));
        l_8 = MK32(HI16(l_8), LO16(l_8) + 2);
        l_C = MK32(HI16(l_C), LO16(l_C) + 0x28);
    }
    l_18 = block_population(l_10);
    l_8 = l_10;
    l_4 = DSP(0x11F4);
    l_C = MK32(FPSEG(g_population_map), FPOFF(g_population_map) + (((a1 * 0x15) + (((i16)(((i16)(((i16)di) >> 1)) >> 1)) >> 1)) << 1));
    si = 0;
    for (;;) {
        LOOP_TICK(37);
        cc = ((i16)si) < 16;
        si = si + 1;
        if (!(cc)) break;
        FP16(l_8, 0) = FP16(l_8, 0) | (((FP16(l_C, 0) >> ((u8)((u8)l_1C))) | (FP16(l_C, 2) << ((u8)((u8)l_1A)))) & FP16(l_4, 0));
        l_4 = MK32(HI16(l_4), LO16(l_4) + 2);
        l_8 = MK32(HI16(l_8), LO16(l_8) + 2);
        FP16(l_8, 0) = (((FP16(l_C, 2) >> ((u8)((u8)l_1C))) | (FP16(l_C, 4) << ((u8)((u8)l_1A)))) & FP16(l_4, 0)) | FP16(l_8, 0);
        l_4 = MK32(HI16(l_4), LO16(l_4) + 2);
        l_8 = MK32(HI16(l_8), LO16(l_8) + 2);
        l_C = MK32(HI16(l_C), LO16(l_C) + 0x2A);
    }
    l_14 = block_population(l_10);
    mem_free(LO16(l_10), HI16(l_10));
    t64 = l_14 - l_18;
    ax = LO16(t64);
    dx = HI16(t64);
    return MK32(dx, ax);
}

/* 1DA8:21E1  sum of density weights (0x1284) in a 16-row x 2-word (32x16 px) map block */
u32 block_population(u32 a0) {
    u16 ax;
    u16 dx;
    u16 si;
    u16 l_2;
    FUNC_TICK(17);
    l_2 = 0;
    si = 0;
    while (((i16)si) < 32) {
        LOOP_TICK(55);
        l_2 = l_2 + ((((((((g_density_weight((FP16(a0, si << 1) & 3) + 4)) + (g_density_weight(((((i16)(((i16)FP16(a0, si << 1)) >> 1)) >> 1) & 3) + 4))) + (g_density_weight(((((i16)FP16(a0, si << 1)) >> 4) & 3) + 4))) + (g_density_weight(((((i16)FP16(a0, si << 1)) >> 6) & 3) + 4))) + (g_density_weight(((((i16)FP16(a0, si << 1)) >> 8) & 3) + 4))) + (g_density_weight(((((i16)FP16(a0, si << 1)) >> 0xA) & 3) + 4))) + (g_density_weight(((((i16)FP16(a0, si << 1)) >> 0xC) & 3) + 4))) + (g_density_weight(((((i16)FP16(a0, si << 1)) >> 0xE) & 3) + 4)));
        si = si + 1;
    }
    ax = l_2;
    dx = ((i16)l_2) >> 0xF;
    return MK32(dx, ax);
}

/* 1DA8:2293  number of entries with item id 17..41 (scene/set items) in a screenplay's first-episode item list */
u16 script_count_scenes(u16 a0) {
    u16 ax;
    u16 di;
    u16 si;
    u16 l_2;
    FUNC_TICK(16);
    l_2 = 0;
    si = 1;
    di = a0;
    while (((i16)si) <= 10) {
        LOOP_TICK(26);
        if (((i8)(FP8(g_scripts, ((di * 0x1B3) + (si * 3)) + 9))) >= 17) {
            if (((i8)(FP8(g_scripts, ((di * 0x1B3) + (si * 3)) + 9))) <= 41) {
                l_2 = l_2 + 1;
            }
        }
        si = si + 1;
    }
    ax = l_2;
    return ax;
}

/* 1DA8:22F6  (id, player, price) set owner, money -= price (ledger 1 movie / 2 series), file it into the archive genre list if < 4 there; else human: into the bag (max 10), AI: purchase undone */
void prog_give_to_player(u16 a0, u16 a1, u32 a2) {
    u16 ax;
    u16 di;
    u16 si;
    u32 l_4;
    u16 l_6;
    u16 t3;
    u16 t16;
    if (seat_humans && a1 != g_player && mp_enter_player(a1)) {   /* multiplayer: into that human's bag, its ledger */
        prog_give_to_player(a0, a1, a2);
        mp_leave();
        return;
    }
    FUNC_TICK(191);
    l_4 = library_at(a1);
    ax = DSEG;
    si = a0;
    if (((i16)a0) < 1000) {
        films(si).owner = (u8)((((u8)((u8)(ax >> 8))) << 8) | ((u8)((u8)a1)));
        players(a1).money = (players(a1).money) - a2;
        t3 = (u16)(i8)(FP8(l_4, ((u16)(i8)(films(si).genre)) + 0x60));
        l_6 = (u16)(i8)(FP8(l_4, ((u16)(i8)(films(si).genre)) + 0x60));
        di = (u16)((u16)(i8)(films(si).genre));
        if (((i16)t3) >= 4) {
            if (a1 != g_player || ((i16)g_bag_count) >= 10) {
                films(si).owner = 0;
                if ((films(si).genre) == GENRE_SHOW) {
                    films(si).flags_16 = (films(si).flags_16) & 0xFFDF;
                }
            } else {
                g_bag(g_bag_count) = si;
                g_bag_count = g_bag_count + 1;
            }
        } else {
            FP16(l_4, (di << 3) + (l_6 << 1)) = si;
            FP8(l_4, di + 0x60) = FP8(l_4, di + 0x60) + 1;
        }
        if (a1 == g_player) {
            ledger_add(LEDGER_FILMS_BOUGHT, a2);
        }
    } else {
        series_owner(si - 0x3E8) = (u8)((((u8)((u8)(ax >> 8))) << 8) | ((u8)((u8)a1)));
        players(a1).money = (players(a1).money) - a2;
        t16 = (u16)(i8)FP8(l_4, 0x6B);
        l_6 = (u16)(i8)FP8(l_4, 0x6B);
        si = si - 0x3E8;
        di = 0xB;
        if (((i16)t16) >= 4) {
            if (a1 != g_player || ((i16)g_bag_count) >= 10) {
                series(si).owner = 0;
            } else {
                g_bag(g_bag_count) = si + 0x3E8;
                g_bag_count = g_bag_count + 1;
            }
        } else {
            FP16(l_4, (di << 3) + (l_6 << 1)) = si;
            FP8(l_4, di + 0x60) = FP8(l_4, di + 0x60) + 1;
        }
        if (a1 == g_player) {
            ledger_add(LEDGER_SERIES_BOUGHT, a2);
        }
    }
    return;
}

/* 1DA8:24EA  1 if every item of the episode's shopping list (+0xB) of the screenplay is bought */
u16 script_items_bought(u16 a0, u16 a1) {
    u16 ax;
    u16 di;
    u16 si;
    u16 l_2;
    FUNC_TICK(16);
    l_2 = 1;
    si = 1;
    di = a0;
    while ((FP8(g_scripts, (((di * 0x1B3) + ((a1 - 1) * 0x23)) + (si * 3)) + 9)) != 0) {
        LOOP_TICK(34);
        if ((FP8(g_scripts, (((di * 0x1B3) + ((a1 - 1) * 0x23)) + (si * 3)) + 0xB)) == 0) {
            l_2 = 0;
        }
        si = si + 1;
    }
    ax = l_2;
    return ax;
}

/* 1DA8:2560  (script, ep, item, actor) how well actor fits a role: sum(min(role requirement, actor attribute)) / sum(requirements) * 100 over attributes 0,2,3 */
u16 script_role_match_pct(u16 a0, u16 a1, u16 a2, u16 a3) {
    u16 ax;
    u16 di;
    u16 si;
    u16 l_2;
    u16 l_4;
    u16 l_6;
    FUNC_TICK(51);
    l_4 = 0;
    l_2 = 0;
    l_6 = (u16)(i8)(FP8(g_scripts, ((a0 * 0x1B3) + (((u16)(i8)(FP8(g_scripts, (((a0 * 0x1B3) + ((a1 - 1) * 0x23)) + (a2 * 3)) + 9))) - 1)) + 0x1A4));
    si = 0;
    di = (u16)(((u16)(i8)(FP8(g_scripts, (((a0 * 0x1B3) + ((a1 - 1) * 0x23)) + (a2 * 3)) + 9))) - 1);
    while (((i16)si) < 4) {
        LOOP_TICK(50);
        if (si != 1) {
            l_4 = l_4 + util_min((u16)(i8)(FP8(g_scripts, (((a0 * 0x1B3) + (di << 2)) + si) + 0x168)), (u16)(i8)(G8((((((l_6 - 1) * 0xC0) + 0x5013) + (a3 * 0xC)) + si) + 6)));
            l_2 = l_2 + ((u16)(i8)(FP8(g_scripts, (((a0 * 0x1B3) + (di << 2)) + si) + 0x168)));
        }
        si = si + 1;
    }
    if (l_2 == 0) {
        ax = 0;
    } else {
        ax = (u16)((u16)((i32)(i16)(l_4 * 0x64) / (i16)l_2));
    }
    return ax;
}

/* 1DA8:2653  round a long to a multiple: ((v + m/2) / m) * m */
u32 round_to(u32 a0, u32 a2) {
    u16 ax;
    u16 dx;
    u32 t4;
    FUNC_TICK(19);
    t4 = (u32)(((u32)((i32)((MK32(((i16)HI16(a2)) >> 1, (LO16(a2) >> 1) | ((u16)(HI16(a2) & 1) << 0xF))) + a0) / (i32)a2)) * a2);
    ax = LO16(t4);
    dx = HI16(t4);
    return MK32(dx, ax);
}

/* 1DA8:2682  (player, viewers) viewers as % of the player's reach: (v + T/2) / T with T = reach * 2464 / 100 */
u16 viewers_to_pct(u16 a0, u32 a1) {
    u16 ax;
    FUNC_TICK(50);
    ax = LO16(((u32)((i32)(((u32)((i32)((u32)(tx(a0, 0).reach * 0x9A0)) / (i32)0xC8)) + a1) / (i32)((u32)((i32)((u32)(tx(a0, 0).reach * 0x9A0)) / (i32)0x64)))));
    return ax;
}

/* 1DA8:26FC  live movie 29 recurs: up to 3 more times dated day+3 hour 4, then removed from its owner */
void live29_reschedule(void) {
    u16 t1;
    FUNC_TICK(18);
    t1 = g_live29_left;
    g_live29_left = g_live29_left - 1;
    if (((i16)t1) <= 0) {
        program_withdraw(0x1D);
        g_live29_left = 3;
    } else {
        films(0x1D).live_day = g_day + 3;
        films(0x1D).live_slot = 4;
    }
    return;
}

/* 1DA8:2735  live movie 187 recurs: up to 5 more times dated day+2 hour 3, then removed from its owner */
void live187_reschedule(void) {
    u16 t1;
    FUNC_TICK(19);
    t1 = g_live187_left;
    g_live187_left = g_live187_left - 1;
    if (((i16)t1) <= 0) {
        program_withdraw(0xBB);
        g_live187_left = 5;
    } else {
        films(0xBB).live_day = g_day + 2;
        films(0xBB).live_slot = 3;
    }
    return;
}

/* 1DA8:276D  choose today's intruder: human with money < -100000 -> bailiff for that player; else from day 3 a 1/3 chance each of terrorist 1, terrorist 2 (only if that event is enabled) or bailiff for the movie agency; sets up the NPC figure 0x7BD8 */
void npc_event_daily(void) {
    u16 ax;
    u16 bx;
    int cc;
    u16 es;
    u16 s0;
    u16 si;
    u32 l_4;
    u16 t2;
    FUNC_TICK(130);
    g_npc_target_player = 8;
    bx = (g_player * 0x17) + 0x8BDC;
    si = 0;
    es = DSEG;
    if (seat_humans) {                   /* multiplayer: the most indebted human (src/seatctx.c) */
        int debtor = mp_bailiff_target();
        if (debtor) { g_npc_target_player = (u16)debtor; si = 3; }
    } else if (((i32)(players(g_player).money)) < -100000) {
        g_npc_target_player = g_player;
        si = 3;
    }
    cc = si != 0;
    if (!(cc || ((i16)g_day) < 3)) {
        t2 = random_n(9, 1);
        si = t2 + 0xFFFA;
        if (((i16)(t2 + 0xFFFA)) > 0) {
            if (((i16)si) > 2) {
                ax = 0;
            } else {
                ax = 1;
            }
            s0 = ax;
            if (g_fired_player(si) != 0) {
                ax = 0;
            } else {
                ax = 1;
            }
            if ((s0 & ax) != 0) {
                si = 0;
            }
        }
    }
    if (((i16)si) > 0) {
        if (g_fired_player(si) != 0) {
            g_fired_player(si) = 2;
            if (((i16)si) <= 2) {
                news_add_event(0xA6, 0x10, si);
            }
            l_4 = DSP(0x7BD8);
            FP16(l_4, 0x18) = 2;
            FP16(l_4, 0x10) = 0;
            FP16(l_4, 4) = 0x48;
            FP16(l_4, 0) = 0x48;
            FP16(l_4, 6) = 1;
            FP16(l_4, 2) = 1;
            FP16(l_4, 8) = 0;
            FP16(l_4, 0x12) = 0xFFFF;
            FP16(l_4, 0xE) = 0;
            g_npc_type = si;
        }
        g_guardian_state = 0;
    } else {
        l_4 = DSP(0x7BD8);
        FP16(l_4, 0x18) = 2;
        FP16(l_4, 0x10) = 0;
        FP16(l_4, 4) = 0x48;
        FP16(l_4, 0) = 0x48;
        FP16(l_4, 6) = 1;
        FP16(l_4, 2) = 1;
        FP16(l_4, 8) = 0;
        FP16(l_4, 0x12) = 0xFFFF;
        FP16(l_4, 0xE) = 0;
        g_guardian_state = 1;
        g_npc_type = 4;
    }
    G32(0x7A64) = 0;
    return;
}

/* 1DA8:2905  per-frame state machine of the intruder (terrorist/bailiff/moral guardian): start walking at a random moment, pick target door of the right room type and owner, walk, plant bomb / act (npc_event_strike), bomb timer and door-closed states */
void npc_event_update(void) {
    u16 ax;
    u16 bx;
    u16 di;
    u16 si;
    u32 t4;
    u16 t12;
    u16 t19;
    u16 t27;
    u16 l_10 = 0;
    u16 l_E = 0;
    u16 l_C = 0;
    u16 l_A = 0;
    u16 l_8 = 0;
    u16 l_6 = 0;
    u8 fr_[20] = {0};                  /* bytes bp-20 .. bp-1 of the original frame */
    FUNC_TICK(259);
    if (((i16)g_npc_type) >= 4) {
        bx = 0x7A88;
    } else {
        bx = ((g_npc_type + 0x47) << 1) + 0x79C6;
    }
    FR16(fr_, 20, 0x12) = DSEG;
    FR16(fr_, 20, 0x14) = bx;
    FR16(fr_, 20, 0x2) = DSEG;
    FR16(fr_, 20, 0x4) = 0x7BD8;
    if ((FP16(FR32(fr_, 20, 0x14), 0)) != 0) {
        if ((FP16(FR32(fr_, 20, 0x14), 0)) == 2 && ((i16)(clock_hour())) >= 1 && mp_nobody_watches()) {   /* g_room != 0 || g_bld_scroll > 30 (multiplayer: no human looks) */
            if (((i16)(clock_hour())) > 5) {
L_299B:
                if (((i16)(clock_hour())) > 5 || g_npc_type == 4) {
L_29AE:
                    FP16(FR32(fr_, 20, 0x14), 0) = 3;
                    ax = g_npc_type - 1;
                    if (((u16)(g_npc_type - 1)) <= 3) {
                        switch (ax) {
                        case 0:
                            l_6 = 0x13;
                            l_8 = 0;
                            break;
                        case 1:
                            l_6 = 0x12;
                            l_8 = 0;
                            break;
                        case 2:
                            if (((i16)g_npc_target_player) > 3) {
                                l_6 = g_npc_target_player;
                                l_8 = 0;
                                break;
                            } else {
                                l_6 = 1;
                                l_8 = g_npc_target_player;
                                break;
                            }
                            break;
                        case 3:
                            l_6 = 4;
                            l_8 = g_npc_target_player;
                            break;
                        default:
                            cpu_halt("bad switch index %u in 1DA8:2905", ax);
                        }
                    }
                    l_10 = 0;
                    while (((i16)l_10) <= 12) {
                        LOOP_TICK(95);
                        di = 0;
                        while (((i16)di) < 4) {
                            LOOP_TICK(90);
                            t12 = (u16)(i8)(G8(((l_10 << 3) + 0x8A60) + (di << 1)));
                            l_C = (u16)(i8)(G8(((l_10 << 3) + 0x8A60) + (di << 1)));
                            t19 = (u16)(i8)(G8((((l_10 << 3) + 0x8A60) + (di << 1)) + 1));
                            l_A = (u16)(i8)(G8((((l_10 << 3) + 0x8A60) + (di << 1)) + 1));
                            if (floors_door_room(t12, t19) == l_6 && ((floors_door_owner(l_C, l_A)) == l_8 || l_8 == 0)) {
                                FP16(FR32(fr_, 20, 0x4), 0x10) = 1;
                                FP16(FR32(fr_, 20, 0x4), 6) = l_10 + 1;
                                l_E = 0xFFFF;
                                si = 0;
                                while (((i16)si) <= ((i16)di)) {
                                    LOOP_TICK(16);
                                    if ((G8(((l_10 << 3) + 0x8A60) + (si << 1))) != 0) {
                                        l_E = l_E + 1;
                                    }
                                    si = si + 1;
                                }
                                FP16(FR32(fr_, 20, 0x4), 0x12) = l_E;
                                FP16(FR32(fr_, 20, 0x4), 4) = 0x99;
                                FP16(FR32(fr_, 20, 0x4), 0x16) = 0;
                            }
                            di = di + 1;
                        }
                        l_10 = l_10 + 1;
                    }
                }
            } else {
                t4 = (u32)((i32)0x278D0 / (i32)MK32(((i16)g_game_speed) >> 0xF, g_game_speed));
                if ((random_n(LO16(t4), 1)) == 1) {
                    goto L_29AE;
                } else {
                    goto L_299B;
                }
            }
        } else if ((FP16(FR32(fr_, 20, 0x14), 0)) == 3 && (FP16(FR32(fr_, 20, 0x4), 0x10)) == 0) {
            t27 = door_check_entry(5, 1);
            ax = t27;
            if (t27 == 0) {
                FP16(FR32(fr_, 20, 0x14), 0) = 4;
            } else if (ax == 1) {
                FP16(FR32(fr_, 20, 0x14), 0) = (FP16(FR32(fr_, 20, 0x4), 0x12)) + 0x3E8;
                FP16(FR32(fr_, 20, 0x4), 0x10) = 2;
                FP16(FR32(fr_, 20, 0x4), 0x12) = 0xFFFF;
            } else if (ax == 2) {
                FP16(FR32(fr_, 20, 0x14), 0) = 0x22;
            }
            if (((i16)(FP16(FR32(fr_, 20, 0x14), 0))) < 1000) {
                FP16(FR32(fr_, 20, 0x14), 0) = 4;
            }
        } else if (((i16)(FP16(FR32(fr_, 20, 0x14), 0))) < 4 || ((i16)(FP16(FR32(fr_, 20, 0x14), 0))) > 32) {
            if ((FP16(FR32(fr_, 20, 0x14), 0)) == 0x21) {
                g_npc_target_door = FP16(FR32(fr_, 20, 0x4), 0x12);
                g_npc_target_floor = FP16(FR32(fr_, 20, 0x4), 2);
                g_bomb_timer = 0;
                if (((i16)g_npc_type) > 2) {
                    if (g_npc_type != 3) {
                        if (g_npc_type == 4) {
                            npc_event_strike();
                        }
                    } else {
                        npc_event_strike();
                    }
                } else {
                    g_bomb_timer = 1;
                    g_door_block = 1;
                }
                FP16(FR32(fr_, 20, 0x4), 0x12) = 0xFFFF;
                FP16(FR32(fr_, 20, 0x4), 8) = 0x48;
                FP16(FR32(fr_, 20, 0x4), 6) = 1;
                FP16(FR32(fr_, 20, 0x4), 4) = 0x99;
                FP16(FR32(fr_, 20, 0x14), 0) = 0x22;
            } else if ((FP16(FR32(fr_, 20, 0x14), 0)) == 0x22 && (FP16(FR32(fr_, 20, 0x4), 4)) == (FP16(FR32(fr_, 20, 0x4), 0)) && !((FP16(FR32(fr_, 20, 0x4), 2)) != (FP16(FR32(fr_, 20, 0x4), 6)) || (FP16(FR32(fr_, 20, 0x4), 0x16)) != 0) && (FP16(FR32(fr_, 20, 0x4), 0xE)) == 0) {
                FP16(FR32(fr_, 20, 0x4), 0x10) = 0;
                if (((i16)g_npc_type) > 2) {
                    g_fired_player(3) = 1;
                } else {
                    g_fired_player(g_npc_type) = 1;
                }
                if (g_terror_retire_idx != 0) {
                    g_planner_scroll(g_terror_retire_idx) = 0;
                }
            } else if (((i16)(FP16(FR32(fr_, 20, 0x14), 0))) >= 1000) {
                FP16(FR32(fr_, 20, 0x4), 0x12) = (u16)((u16)((i32)(i16)(FP16(FR32(fr_, 20, 0x14), 0)) % 1000));
                if ((door_check_entry(4, 1)) != 0) {
                    FP16(FR32(fr_, 20, 0x4), 0x12) = 0xFFFF;
                } else {
                    FP16(FR32(fr_, 20, 0x14), 0) = 3;
                }
            }
        } else {
            FP16(FR32(fr_, 20, 0x14), 0) = (FP16(FR32(fr_, 20, 0x14), 0)) + 1;
        }
        if (g_bomb_timer != 0) {
            g_bomb_timer = g_bomb_timer + 1;
            if (g_bomb_timer == 0x64) {
                npc_event_strike();
                g_bomb_timer = 0x3E8;
                g_door_block = 2;
            }
            if (g_bomb_timer == 0x41A) {
                g_bomb_timer = 0;
                g_door_block = 3;
            }
        }
    }
    return;
}

/* Part of 1DA8:2D3B: how many films the intruder can pick from.
 * ORIGINAL BUG (fix "intruder"): the original counts genres 2..12 (12 is
 * already the first byte of the next player's library), while the pick in
 * npc_event_strike draws genres 1..10 only.  A library holding nothing but
 * series (11) therefore hangs the game in an endless pick loop -- seen with
 * the bailiff and with a terrorist -- and one holding nothing but love films
 * (1) counts as empty.  Fixed: count exactly the genres the pick can draw. */
static u16 intruder_film_count(u16 player) {
    u16 n = 0, g;
    int fixed = fix_on(FIX_INTRUDER);
    for (g = 1; g <= (fixed ? 10 : 11); g++) {
        LOOP_TICK(14);
        n += (u16)(i8)library_count(player, fixed ? g : g + 1);
    }
    return n;
}

/* 1DA8:2D3B  intruder acts at target door: consulate -> room destroyed; studio -> production lost (0x133/0x136); library -> random movie destroyed/seized; news room -> news wiped (bailiff); office -> seizure; moral guardian confiscates the airing movie (0x15B); terrorist damage costs the owner 50000 (0x135) */
void npc_event_strike(void) {
    u16 ax;
    int cc;
    u16 di;
    u16 s0;
    u16 s1;
    u16 s2;
    u16 s3;
    u16 s4;
    u16 s5;
    u16 s6;
    u16 s7;
    u16 s8;
    u16 s9;
    u16 si;
    u32 l_4;
    u16 l_6;
    u16 l_8;
    u16 l_A;
    u16 l_C;
    u16 l_E;
    u16 s10;
    u16 t1;
    u16 t25;
    u16 t29;
    u16 t30;
    u16 t31;
    u16 t38;
    u16 t39;
    u16 t40;
    FUNC_TICK(369);
    l_E = floors_door_room(g_npc_target_floor, g_npc_target_door);
    l_C = floors_door_studio(g_npc_target_floor, g_npc_target_door);
    t1 = floors_door_owner(g_npc_target_floor, g_npc_target_door);
    g_terror_retire_idx = 0;
    si = t1;
    if (((i16)l_E) > 15 && ((i16)l_E) < 25) {
        if (((i16)g_npc_type) <= 2) {
            if (((u32)MK32(g_npc_type, l_E)) != 0x10013) {
                if (((u32)MK32(g_npc_type, l_E)) == 0x20012) {
L_2DD8:
                    news_add_event(0xA7, 0x10, g_npc_type);
                    g_terror_retire_idx = 1;
                } else {
                    news_add_event(0xA8, 0x10, g_npc_type);
                    g_terror_retire_idx = 0;
                }
            } else {
                goto L_2DD8;
            }
        }
        g_terror_retire_idx = 0;
        if (l_E == 0x13) {
            g_terror_retire_idx = 0x48;
        }
        if (l_E == 0x12) {
            g_terror_retire_idx = 0x49;
        }
        l_4 = floors_at(g_npc_target_floor);
        FP8(l_4, g_npc_target_door + 0x1E) = 0x36;
        FP8(l_4, g_npc_target_door + 0x28) = 0x89;
        FP8(l_4, g_npc_target_door + 0x32) = 0x8A;
        FP8(l_4, g_npc_target_door + 0x46) = 0;
        studios(l_C).renter = 0xFF;
    } else if (((i16)g_npc_type) <= 2) {
        news_add_event(0xA8, 0x10, g_npc_type);
        g_terror_retire_idx = 0;
    }
    if (l_E == 5) {
        studio_cancel_production(l_C);
        s0 = 0;
        s1 = 0;
        s2 = 0;
        s3 = 0;
        s4 = 0;
        s5 = 0;
        s6 = 0;
        s7 = 0;
        s8 = ((i16)g_npc_target_floor) >> 0xF;
        s9 = g_npc_target_floor;
        s10 = si;
        if (((i16)g_npc_type) > 2) {
            ax = 0x136;
        } else {
            ax = 0x133;
        }
        msg_post(ax, s10, MK32(s8, s9), MK32(s6, s7), MK32(s4, s5), MK32(s2, s3), MK32(s0, s1));
        l_4 = floors_at(g_npc_target_floor);
        FP8(l_4, g_npc_target_door + 0x1E) = 0x36;
        FP8(l_4, g_npc_target_door + 0x28) = 0x89;
        FP8(l_4, g_npc_target_door + 0x32) = 0x8A;
        FP8(l_4, g_npc_target_door + 0x46) = 0;
        studios(l_C).renter = 0xFF;
        studios(l_C).script = 0;
        studios(l_C).episode = 0;
    } else if (((u32)MK32(l_E, g_npc_type)) == 0x80003) {
        g_door_block = 4;
    } else if (((u32)MK32(l_E, g_npc_type)) == 0x20003) {
        g_player_news_count(si) = 0;
        mem_zero(news_pool_at((si * 4) + 0x39), 0x5A);
        news_pool((si * 4) + 0x39).node = 0xA9;
        g_news_score(si) = 0;
    } else if (l_E == 4) {
        if (g_npc_type != 4) {
            l_6 = intruder_film_count(si);
            if (l_6 != 0) {
                for (;;) {
                    LOOP_TICK(29);
                    t29 = random_n(0xA, l_E);
                    t30 = (u16)(i8)library_count(si, t29);
                    t31 = random_n(4, l_E);
                    l_A = t31;
                    di = t29;
                    if (((i16)t30) < ((i16)t31)) {
                        continue;
                    } else {
                        break;
                    }
                }
                s0 = si;
                if (((i16)g_npc_type) > 2) {
                    ax = 0x138;
                } else {
                    ax = 0x134;
                }
                movie_destroy(G16((((si * 0x6C) + 0x6F19) + (di << 3)) + ((l_A - 1) << 1)), ax, s0);
            }
        } else {
            t25 = clock_hour();
            l_8 = sched_program_at(si, t25);
            movie_destroy(l_8, 0x15B, si);
        }
    } else if (l_E == 1) {
        int as_human = mp_enter_player(si);     /* multiplayer: the papers of the human it hits */
        if (si == g_player) {
            g_office_papers = 0;
        }
        if (as_human) mp_leave();
        if (g_npc_type == 3) {
            l_6 = intruder_film_count(si);
            if (l_6 != 0) {
                for (;;) {
                    LOOP_TICK(29);
                    t38 = random_n(0xA, l_E);
                    t39 = (u16)(i8)library_count(si, t38);
                    t40 = random_n(4, l_E);
                    l_A = t40;
                    di = t38;
                    if (((i16)t39) < ((i16)t40)) {
                        continue;
                    } else {
                        break;
                    }
                }
                movie_destroy(G16((((si * 0x6C) + 0x6F19) + (di << 3)) + ((l_A - 1) << 1)), 0x138, si);
            }
        }
    }
    cc = si == 0;
    if (!(cc || ((i16)g_npc_type) > 2)) {
        u32 dmg = terror_damage();              /* option --terror-damage: the original's $50 000; 0 nothing, no message (src/fixes.c) */
        if (dmg != 0) {
            players(si).money = (players(si).money) - dmg;
            int as_human = mp_enter_player(si); /* multiplayer: the ledger of the human it hits */
            if (si == g_player) {
                ledger_add(LEDGER_CREDIT_REPAID, dmg);
                ui_draw_money();
            }
            if (as_human) mp_leave();
            msg_post(0x135, si, 0, 0, 0, 0, 0);
        }
    }
    return;
}

/* 1DA8:3202  (movie, msg, player) remove movie from owner and from the schedule (stops it if on air), message msg; bailiff (type 3) credits half its value to the owner (ledger 0xB) */
void movie_destroy(u16 a0, u16 a1, u16 a2) {
    u16 di;
    u16 si;
    u16 t3;
    u32 t7;
    u32 t9;
    if (seat_humans && a2 != g_player && mp_enter_player(a2)) {   /* multiplayer: as the human it hits */
        movie_destroy(a0, a1, a2);
        mp_leave();
        return;
    }
    FUNC_TICK(104);
    program_withdraw(a0);
    si = a2;
    di = clock_hour();
    while (((i16)di) < 15) {
        LOOP_TICK(73);
        if (schedule_program(si, di) == a0) {
            sched_clear_slot(si, di);
            t3 = clock_hour();
            if (t3 == di) {
                ratings(si).share = 0;
                ratings(si).viewers = 0;
                G32(((si << 5) + 0x74EA) + (di << 2)) = 0;
                G16(((si * 0x1E) + 0x1C88) + ((di + 7) << 1)) = 0;
                if (si == g_tv_channel) {
                    g_now_title_stale = 1;
                    g_redraw = 1;
                    tv_update_picture();
                }
                if (si == g_player) {
                    g_tvfam_state = 1;
                }
            }
        }
        di = di + 1;
    }
    if (((i16)g_npc_type) > 3) {
        msg_post(a1, si, MK32(((i16)a0) >> 0xF, a0), 0, 0, 0, 0);
    } else {
        msg_post(a1, si, 0, 0, 0, 0, 0);
        if (g_npc_type == 3) {
            t7 = movie_price(a0);
            players(si).money = (players(si).money) + (MK32(((i16)HI16(t7)) >> 1, (LO16(t7) >> 1) | ((u16)(HI16(t7) & 1) << 0xF)));
            if (si == g_player) {
                t9 = movie_price(a0);
                ledger_add(LEDGER_FILMS_SOLD, MK32(((i16)HI16(t9)) >> 1, (LO16(t9) >> 1) | ((u16)(HI16(t9) & 1) << 0xF)));
            }
        }
    }
    if (((u32)MK32(g_room, si)) == ((u32)MK32(0x1F, g_room_owner))) {
        planner_refresh();
    }
    return;
}

/* 1DA8:33A6  far pointer to a movie's title from the loaded text table 0x63E5 (49 entries), NULL if absent */
u32 movie_title(u16 a0) {
    u16 ax;
    u16 di;
    u16 dx;
    u16 si;
    FUNC_TICK(26);
    si = 0;
    di = a0;
    while (film(si) != di) {
        LOOP_TICK(13);
        if (((i16)si) >= 49) break;
        si = si + 1;
    }
    if (((i16)si) >= 49) {
        u32 more = film_txt_more_text(a0, 0);   /* fix "title-cache" (text_cache.c): slots 49.. */
        ax = LO16(more);
        dx = HI16(more);
    } else {
        ax = title_lo(si);
        dx = title_hi(si);
    }
    return MK32(dx, ax);
}

/* 1DA8:33FA  far pointer to a movie's description text (table 0x63E5 +6) */
u32 movie_description(u16 a0) {
    u16 ax;
    u16 di;
    u16 dx;
    u16 si;
    FUNC_TICK(26);
    si = 0;
    di = a0;
    while (film(si) != di) {
        LOOP_TICK(13);
        if (((i16)si) >= 49) break;
        si = si + 1;
    }
    if (((i16)si) >= 49) {
        u32 more = film_txt_more_text(a0, 1);   /* fix "title-cache" (text_cache.c): slots 49.. */
        ax = LO16(more);
        dx = HI16(more);
    } else {
        ax = desc_lo(si);
        dx = desc_hi(si);
    }
    return MK32(dx, ax);
}

/* 1DA8:344E  far pointer to a series title (table 0x65CF, 15 entries) */
u32 series_title(u16 a0) {
    u16 ax;
    u16 di;
    u16 dx;
    u16 si;
    FUNC_TICK(26);
    si = 0;
    di = a0;
    while ((inbox(0xD, (si * 3) + 0xF).quality) != di) {
        LOOP_TICK(13);
        if (((i16)si) >= 15) break;
        si = si + 1;
    }
    if (((i16)si) >= 15) {
        ax = 0;
        dx = 0;
    } else {
        ax = inbox(0xD, (si * 3) + 0xF).hour;
        dx = inbox(0xD, (si * 3) + 0xF).day;
    }
    return MK32(dx, ax);
}

/* 1DA8:34A2  far pointer to a series description (table 0x65CF +6) */
u32 series_description(u16 a0) {
    u16 ax;
    u16 di;
    u16 dx;
    u16 si;
    FUNC_TICK(26);
    si = 0;
    di = a0;
    while ((inbox(0xD, (si * 3) + 0xF).quality) != di) {
        LOOP_TICK(13);
        if (((i16)si) >= 15) break;
        si = si + 1;
    }
    if (((i16)si) >= 15) {
        ax = 0;
        dx = 0;
    } else {
        ax = schedule_ad(si + 0x5F, 0xE);
        dx = inbox(0xD, (si * 3) + 0x10).headline;
    }
    return MK32(dx, ax);
}

/* 1DA8:34F6  (series, ep) far pointer to an episode title (table 0x65CF +0xA) */
u32 series_episode_title(u16 a0, u16 a1) {
    u16 ax;
    u16 di;
    u16 dx;
    u16 si;
    FUNC_TICK(31);
    si = 0;
    di = a0;
    while ((inbox(0xD, (si * 3) + 0xF).quality) != di) {
        LOOP_TICK(13);
        if (((i16)si) >= 15) break;
        si = si + 1;
    }
    if (((i16)si) >= 15) {
        ax = 0;
        dx = 0;
    } else {
        ax = schedule_ad(si + 0x5F, (a1 * 2) + 0xE);
        dx = schedule_program(si + 0x60, a1 * 2);
    }
    return MK32(dx, ax);
}

/* 1DA8:3554  (series, ep) far pointer to an episode description (table 0x65CF +0x32) */
u32 series_episode_description(u16 a0, u16 a1) {
    u16 ax;
    u16 di;
    u16 dx;
    u16 si;
    FUNC_TICK(31);
    si = 0;
    di = a0;
    while ((inbox(0xD, (si * 3) + 0xF).quality) != di) {
        LOOP_TICK(13);
        if (((i16)si) >= 15) break;
        si = si + 1;
    }
    if (((i16)si) >= 15) {
        ax = 0;
        dx = 0;
    } else {
        ax = schedule_episode(si + 0x60, (a1 * 2) + 4);
        dx = schedule_episode(si + 0x60, (a1 * 2) + 5);
    }
    return MK32(dx, ax);
}

/* 1DA8:35B2  far pointer to a screenplay title (table 0x6B15, 10 entries) */
u32 script_title(u16 a0) {
    u16 ax;
    u16 di;
    u16 dx;
    u16 si;
    FUNC_TICK(21);
    si = 0;
    di = a0;
    while ((G16((si * 0x42) + 0x6B15)) != di) {
        LOOP_TICK(11);
        si = si + 1;
    }
    ax = G16((si * 0x42) + 0x6B17);
    dx = G16((si * 0x42) + 0x6B19);
    return MK32(dx, ax);
}

/* 1DA8:35F6  (script, role) far pointer to a role name of a screenplay (table 0x6B15 +6) */
u32 script_role_name(u16 a0, u16 a1) {
    u16 ax;
    u16 di;
    u16 dx;
    u16 si;
    FUNC_TICK(25);
    si = 0;
    di = a0;
    while ((G16((si * 0x42) + 0x6B15)) != di) {
        LOOP_TICK(11);
        si = si + 1;
    }
    ax = G16((((si * 0x42) + 0x6B15) + (a1 << 2)) + 6);
    dx = G16((((si * 0x42) + 0x6B15) + (a1 << 2)) + 8);
    return MK32(dx, ax);
}

/* 1DA8:3643  (figure, npc) why a figure may not enter its door: bomb alarm texts 0x130..0x137, screenplay being written 0xF4, someone inside 0x12B, catching a rival in your own room 0x12E pays you a bounty that doubles each time (max 200000), empty room 0x12F, dead player's room 0x49; 0 = may enter */
u16 door_check_entry(u16 a0, u16 a1) {
    u16 ax;
    u16 bx;
    int cc;
    u16 di;
    u16 si;
    u32 l_4;
    u16 l_6;
    u16 l_8;
    u16 t11;
    FUNC_TICK(166);
    l_8 = figures(a0).floor;
    l_6 = figures(a0).target_door;
    l_4 = floors_at(l_8);
    di = 0;
    if (((u32)MK32(0, a1)) == 0) {
        if (!(g_door_block == 0 || l_8 != g_npc_target_floor || l_6 != g_npc_target_door)) {
            if (a1 != 1) {
                ax = g_door_block - 1;
                if (((u16)(g_door_block - 1)) <= 3) {
                    switch (ax) {
                    case 0:
                        di = 0x130;
                        break;
                    case 1:
                        di = 0x131;
                        break;
                    case 2:
                        di = 0x132;
                        break;
                    case 3:
                        di = 0x137;
                        break;
                    default:
                        cpu_halt("bad switch index %u in 1DA8:3643", ax);
                    }
                }
            } else {
                di = 2;
            }
        }
    }
    cc = di != 0;
    if (!(cc || FP8(l_4, l_6 + 0x6E) <= 0 || (studios(FP8(l_4, l_6 + 0x6E)).running) == 0)) {
        if (a1 != 0) {
            di = 2;
        } else {
            di = 0xF4;
        }
    }
    if (di == 0 && knock_opt && seat_humans && mp_knock_held(a0, l_8, l_6))
        di = a1 != 0 ? 1 : 0x12B;        /* multiplayer, --knock: the room waits for the one who knocked */
    if (di == 0) {
        si = 0;
        while (((i16)si) < 6) {
            LOOP_TICK(128);
            if (si != a0) {
                if ((players(si + 1).out) == 0) {
                    if ((figures(si).kind) == 2) {
                        if ((figures(si).task) == 0) {
                            if ((figures(si).target_door) == l_6) {
                                if ((figures(si).target_floor) == l_8) {
                                    if (a1 == 0) {
                                        if (FP8(l_4, l_6 + 0x46) == ((u16)(a0 + 1))) {
                                            di = 0x12E;
                                            /* ORIGINAL BUG (fix "catch-bounty"): only figure 0 gets the
                                             * bounty -- the human only if they play station 1.  The
                                             * caller with a1 = 0 is the human's door check (main loop),
                                             * so a0 is always the human's figure here. */
                                            if (a0 == (fix_on(FIX_CATCH_BOUNTY) ? g_player_figure : 0)) {
                                                g_caught_figure = si + 1;
                                                g_status_amount = g_catch_bounty(si);
                                                G32((u16)((g_player * 0x17) + 0x8BDC) + 8) = (players(g_player).money) + g_status_amount;
                                                ledger_add(LEDGER_SUNDRY_INCOME, g_status_amount);
                                                ui_draw_money();
                                                t11 = g_catch_bounty_lo(si) >> 0xF;
                                                g_catch_bounty(si) = MK32((g_catch_bounty_hi(si) << 1) | t11, g_catch_bounty_lo(si) << 1);
                                                cc = ((i32)g_catch_bounty(si)) > 200000;
                                                bx = (si + 0xC) << 2;
                                                if (cc) {
                                                    g_catch_bounty(si) = 0x30D40;
                                                }
                                            }
                                            if (seat_humans && mp_is_human(si + 1) && si + 1 != g_player)
                                                mp_catch_human(si + 1, a0 + 1);   /* multiplayer: it is thrown out, with a fine */
                                        } else {
                                            di = 0x12B;
                                            if (knock_opt && seat_humans)   /* multiplayer, --knock: a human inside hears it */
                                                mp_knock(a0 + 1, si + 1, l_8, l_6, FP8(l_4, l_6 + 0x1E));
                                        }
                                    } else {
                                        di = 1;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            si = si + 1;
        }
    }
    cc = di != 0;
    if (!(cc || FP8(l_4, l_6 + 0x1E) != 3 || FP8(l_4, l_6 + 0x46) == ((u16)(a0 + 1)))) {
        if (a1 != 0) {
            di = 0;
        } else {
            di = 1;
        }
    }
    cc = di != 0;
    if (!(cc || FP8(l_4, l_6 + 0x1E) != 0x36)) {
        if (a1 != 0) {
            di = 2;
        } else {
            di = 0x12F;
        }
    }
    if (di == 0) {
        si = FP8(l_4, l_6 + 0x46);
        if (!(FP8(l_4, l_6 + 0x46) == 0 || (players(si).out) == 0)) {
            if (a1 != 0) {
                di = 2;
            } else {
                di = 0x49;
            }
        }
    }
    ax = di;
    return ax;
}

/* 1DA8:395A  rank players by this hour's share (0x24AD); ties -> everybody 1st; ranks stored in 0x5553[p*8+hour]; image deltas +1 / 0 / -1 for 1st/2nd/3rd applied via image_apply_deltas */
void rating_rank_hour(u16 ax) {
    int cc;
    u16 di;
    u16 si;
    u16 t1;
    u16 t13;
    u16 t21;
    u16 t24;
    u16 t26;
    u16 t28;
    u16 t29;
    u16 l_6 = 0;
    u16 l_4 = 0;
    u16 l_2 = 0;
    u8 fr_[8] = {0};                  /* bytes bp-8 .. bp-1 of the original frame */
    FUNC_TICK(54);
    l_4 = 0;
    FR16(fr_, 8, 0x8) = 1;
    while (((i16)FR16(fr_, 8, 0x8)) <= 3) {
        LOOP_TICK(13);
        ax = DSEG;
        if ((players(FR16(fr_, 8, 0x8)).out) == 0) {
            l_4 = l_4 + 1;
        }
        FR16(fr_, 8, 0x8) = FR16(fr_, 8, 0x8) + 1;
    }
    if (l_4 != 1) {
        FR16(fr_, 8, 0x8) = 1;
        while (((i16)FR16(fr_, 8, 0x8)) <= 3) {
            LOOP_TICK(25);
            if ((G16((FR16(fr_, 8, 0x8) * 0xA) + 0x24B5)) == 0) {
                ax = ratings(FR16(fr_, 8, 0x8)).share;
            } else {
                ax = 0;
            }
            g_rank_share(FR16(fr_, 8, 0x8)) = ax;
            FR16(fr_, 8, 0x8) = FR16(fr_, 8, 0x8) + 1;
        }
        l_6 = 0;
        FR16(fr_, 8, 0x8) = 1;
        while (((i16)FR16(fr_, 8, 0x8)) <= 3) {
            LOOP_TICK(49);
            si = 1;
            while (((i16)si) <= 3) {
                LOOP_TICK(32);
                cc = FR16(fr_, 8, 0x8) == si;
                if (!(cc || (players(FR16(fr_, 8, 0x8)).out) != 0)) {
                    if ((players(si).out) == 0) {
                        if ((g_rank_share(FR16(fr_, 8, 0x8))) == g_rank_share(si)) {
                            l_6 = 1;
                        }
                    }
                }
                si = si + 1;
            }
            ax = DSEG;
            if ((players(FR16(fr_, 8, 0x8)).out) != 0) {
                g_rank_share(FR16(fr_, 8, 0x8)) = 0xFFFF;
            }
            FR16(fr_, 8, 0x8) = FR16(fr_, 8, 0x8) + 1;
        }
        if (l_6 == 0) {
            si = 0;
            while (((i16)si) < 3) {
                LOOP_TICK(45);
                if (((u32)(MK32(l_4, si))) == 0x20001) {
                    FR16(fr_, 8, 0x8) = 1;
                    while ((players(FR16(fr_, 8, 0x8)).out) == 0) {
                        LOOP_TICK(10);
                        FR16(fr_, 8, 0x8) = FR16(fr_, 8, 0x8) + 1;
                    }
                    t21 = FR16(fr_, 8, 0x8);
                    g_rank_order(si) = FR16(fr_, 8, 0x8);
                    ax = t21;
                } else {
                    FR16(fr_, 8, 0x8) = 1;
                    di = 0xFFFF;
                    while (((i16)FR16(fr_, 8, 0x8)) <= 3) {
                        LOOP_TICK(13);
                        if (((i16)(g_rank_share(FR16(fr_, 8, 0x8)))) > ((i16)di)) {
                            t24 = g_rank_share(FR16(fr_, 8, 0x8));
                            l_2 = FR16(fr_, 8, 0x8);
                            di = t24;
                        }
                        FR16(fr_, 8, 0x8) = FR16(fr_, 8, 0x8) + 1;
                    }
                    t26 = l_2;
                    g_rank_order(si) = l_2;
                    g_rank_share(l_2) = 0xFFFE;
                    ax = t26;
                }
                si = si + 1;
            }
            FR16(fr_, 8, 0x8) = 0;
            while (((i16)FR16(fr_, 8, 0x8)) < 3) {
                LOOP_TICK(21);
                t28 = (((u8)((u8)(ax >> 8))) << 8) | FR8(fr_, 8, 0x8);
                t29 = clock_hour();
                G8((((g_rank_order(FR16(fr_, 8, 0x8))) << 3) + 0x5553) + t29) = (u8)t28;
                FR16(fr_, 8, 0x8) = FR16(fr_, 8, 0x8) + 1;
                ax = t28;
            }
            FR16(fr_, 8, 0x8) = 0;
            while (((i16)FR16(fr_, 8, 0x8)) < 3) {
                LOOP_TICK(10);
                g_image_delta(g_rank_order(FR16(fr_, 8, 0x8))) = 1 - FR16(fr_, 8, 0x8);
                FR16(fr_, 8, 0x8) = FR16(fr_, 8, 0x8) + 1;
            }
        } else {
            FR16(fr_, 8, 0x8) = 0;
            while (((i16)FR16(fr_, 8, 0x8)) < 3) {
                LOOP_TICK(24);
                g_rank_order(FR16(fr_, 8, 0x8)) = 0;
                t13 = clock_hour();
                G8((((FR16(fr_, 8, 0x8) + 1) << 3) + 0x5553) + t13) = 1;
                g_image_delta(FR16(fr_, 8, 0x8) + 1) = 0;
                FR16(fr_, 8, 0x8) = FR16(fr_, 8, 0x8) + 1;
            }
        }
        image_apply_deltas(HPTR(DSP(0x8FB4)));
    } else {
        t1 = clock_hour();
        G8(((g_player << 3) + 0x5553) + t1) = 1;
    }
    return;
}

/* 1DA8:3BE8  apply int deltas[4] to player images (+0x15) zero-sum: gains and losses clamped to 0..100 and balanced (min), Betty love (+0xC) capped at image, rounding remainder to the leader so images total 100 */
void image_apply_deltas(u8 *a0) {
    u16 ax;
    u16 di;
    u16 si;
    u16 l_2;
    u16 l_4;
    u16 l_6;
    u16 l_8;
    u16 l_A;
    u16 t12;
    u16 t27;
    FUNC_TICK(65);
    l_A = 0;
    l_8 = 0;
    l_6 = 0;
    l_4 = 0;
    si = 1;
    di = 0;
    while (((i16)si) <= 3) {
        LOOP_TICK(13);
        if ((players(si).out) == 0) {
            l_4 = l_4 + 1;
        }
        si = si + 1;
    }
    if (l_4 != 1) {
        si = 1;
        while (((i16)si) <= 3) {
            LOOP_TICK(97);
            if (((i16)HP16(a0, si << 1)) >= 0) {
                if (((i16)HP16(a0, si << 1)) > 0) {
                    if ((players(si).out) == 0) {
                        if (((i16)((players(si).image) + HP16(a0, si << 1))) <= 100) {
                            ax = HP16(a0, si << 1);
                        } else {
                            ax = 0x64 - (players(si).image);
                        }
                        l_6 = l_6 + 1;
                        di = di + ax;
                    }
                }
            } else if ((players(si).out) == 0) {
                if (((i32)((i16)(players(si).image)) + (i32)((i16)HP16(a0, si << 1))) >= 0) {
                    ax = HP16(a0, si << 1);
                } else {
                    ax = -(u16)(players(si).image);
                }
                l_A = l_A + ax;
                l_8 = l_8 + 1;
            }
            si = si + 1;
        }
        if (((u16)(-l_A)) != di) {
            t12 = util_min(di, -l_A);
            l_A = -t12;
            si = 1;
            di = t12;
            while (((i16)si) <= 3) {
                LOOP_TICK(32);
                if (((i16)HP16(a0, si << 1)) >= 0) {
                    if (((i16)HP16(a0, si << 1)) > 0) {
                        HP16(a0, si << 1) = (u16)((u16)((i32)(i16)di / (i16)l_6));
                    }
                } else {
                    HP16(a0, si << 1) = (u16)((u16)((i32)(i16)l_A / (i16)l_8));
                }
                si = si + 1;
            }
        }
        si = 1;
        while (((i16)si) <= 3) {
            LOOP_TICK(89);
            players(si).image = (players(si).image) + HP16(a0, si << 1);
            if (((i16)(players(si).image)) < 0) {
                players(si).image = 0;
            }
            if (((i16)(players(si).image)) > 100) {
                players(si).image = 0x64;
            }
            if (((i16)(players(si).betty_love)) > ((i16)(players(si).image))) {
                players(si).betty_love = players(si).image;
            }
            si = si + 1;
        }
        t27 = (G16(0x8C08) + G16(0x8C1F)) + G16(0x8C36);
        l_2 = (G16(0x8C08) + G16(0x8C1F)) + G16(0x8C36);
        if (t27 != 0x64) {
            l_6 = 0;
            si = 1;
            di = 0;
            while (((i16)si) <= 3) {
                LOOP_TICK(23);
                if (((i16)(players(si).image)) > ((i16)l_6)) {
                    l_6 = players(si).image;
                    di = si;
                }
                si = si + 1;
            }
            players(di).image = (players(di).image) + (0x64 - l_2);
        }
    }
    return;
}

/* 1DA8:3F0D  smallest of {50000, 100000, 250000, 500000} greater than the value (else 500000) - daily fee class of a transmitter from population*61.6 */
u32 transmitter_fee_bracket(u16 a0, u16 a1) {
    u16 ax;
    int cc;
    u16 dx;
    u16 si;
    FUNC_TICK(17);
    si = 0;
    for (;;) {
        LOOP_TICK(10);
        cc = ((i32)g_transmitter_fees(si)) <= ((i32)MK32(a1, a0));
        ax = g_transmitter_fees_lo(si);
        if (!(cc)) break;
        if (((i16)si) >= 3) break;
        si = si + 1;
    }
    ax = g_transmitter_fees_lo(si);
    dx = g_transmitter_fees_hi(si);
    return MK32(dx, ax);
}

/* 1DA8:3F48  price of a news item by class: {1000, 2000, 3000, 4000, 6000, 8000}[class] */
u16 news_price(u16 a0) {
    u16 ax;
    FUNC_TICK(10);
    ax = G16((((u16)(i8)((u8)a0)) << 1) + 0x129C);
    return ax;
}

/* 1DA8:3F5B  current movie value: price/2 + price*topicality/(2*max topicality), rounded to 1000 */
u32 movie_price(u16 a0) {
    u16 ax;
    u16 dx;
    u16 s0;
    u16 s1;
    u16 si;
    u32 t7;
    u32 t8;
    FUNC_TICK(94);
    s0 = 0;
    s1 = 0x3E8;
    si = a0;
    if (((films(a0).price_lo) | (films(a0).price_hi)) == 0) {
L_3F94:
        ax = films(si).price_lo;
        dx = films(si).price_hi;
    } else if ((films(si).quality) != 0) {
        t7 = ((u32)((i32)((u32)(MK32(((i16)((u16)(i8)(films(si).topicality))) >> 0xF, (u16)(i8)(films(si).topicality)) * films(si).price)) / (i32)MK32((u16)(((i16)(((u16)(i8)(films(si).quality)) << 1)) >> 0xF), (u16)(((u16)(i8)(films(si).quality)) << 1)))) + ((u32)((i32)films(si).price / (i32)2));
        ax = LO16(t7);
        dx = HI16(t7);
    } else {
        goto L_3F94;
    }
    t8 = round_to(MK32(dx, ax), MK32(s0, s1));
    ax = LO16(t8);
    dx = HI16(t8);
    return MK32(dx, ax);
}

/* 1DA8:4035  series value: n produced episodes * episode price (+0x2B), half fixed + half scaled by sum(topicality)/sum(max), rounded to 1000 */
u32 series_price(u16 a0) {
    u16 ax;
    u16 di;
    u16 dx;
    u16 s0;
    u16 s1;
    u16 si;
    u32 l_4;
    u32 l_8;
    u16 l_A;
    u32 t10;
    u32 t11;
    FUNC_TICK(82);
    l_A = 0;
    si = 1;
    di = 0;
    while (((i16)si) <= 10) {
        LOOP_TICK(31);
        l_4 = MK32(g_series_hi, (g_series_lo + (a0 * 0x39)) + (si * 3));
        if (((u16)(((i16)(MEM16((u16)(g_series_hi), (u16)((g_series_lo + (a0 * 0x39)) + (si * 3)) + 2) << 0xF)) >> 0xF)) == 0) break;
        l_A = l_A + ((u16)(i8)FP8(l_4, 1));
        si = si + 1;
        di = di + ((u16)(i8)FP8(l_4, 0));
    }
    l_8 = ((u32)(MK32(((i16)(si - 1)) >> 0xF, si - 1) * FP32(g_series, (a0 * 0x39) + 0x2B)));
    s0 = 0;
    s1 = 0x3E8;
    if ((LO16(l_8) | HI16(l_8)) != 0) {
        ax = 0;
    } else {
        ax = 1;
    }
    if (ax != 0) {
L_40DD:
        ax = 0;
        dx = 0;
    } else if (di != 0) {
        t10 = ((u32)((i32)((u32)(MK32(((i16)l_A) >> 0xF, l_A) * l_8)) / (i32)MK32(((i16)(di << 1)) >> 0xF, di << 1))) + ((u32)((i32)l_8 / (i32)2));
        ax = LO16(t10);
        dx = HI16(t10);
    } else {
        goto L_40DD;
    }
    t11 = round_to(MK32(dx, ax), MK32(s0, s1));
    ax = LO16(t11);
    dx = HI16(t11);
    return MK32(dx, ax);
}

/* 1DA8:412F  new string with a viewer count in millions: mode 1 "%d,%d" (0.1 M), mode 2 "%d,%02d" (0.01 M), mode 3 = 1 if >= 1,000,000 else 2 */
u32 format_millions(u32 a0, u16 a2) {
    u16 ax;
    u16 di;
    u16 dx;
    u32 rv_;
    u32 t2;
    u32 t3;
    u32 t7;
    u32 t8;
    u32 t9;
    u32 t13;
    u16 l_6 = 0;
    u16 l_4 = 0;
    u16 l_2 = 0;
    FUNC_TICK(127);
    di = a2;
    if (a2 == 3) {
        if (((i32)a0) < 1000000) {
            di = 2;
        } else {
            di = 1;
        }
    }
    if (di != 1) {
        if (di == 2) {
            t8 = round_to(a0, 0x2710);
            a0 = MK32(HI16(t8), LO16(a0));
            a0 = MK32(HI16(a0), LO16(t8));
            t9 = (u32)((i32)a0 / (i32)0xF4240);
            l_2 = LO16(((u32)((i32)(a0 - ((u32)(MK32(((i16)LO16(t9)) >> 0xF, LO16(t9)) * 0xF4240))) / (i32)0x2710)));
            t13 = str_format(DSP(0x12CA) /* "#d,#+02d" */, LO16(t9), l_2);
            l_4 = HI16(t13);
            l_6 = LO16(t13);
        }
    } else {
        t2 = round_to(a0, 0x186A0);
        a0 = MK32(HI16(t2), LO16(a0));
        a0 = MK32(HI16(a0), LO16(t2));
        t3 = (u32)((i32)a0 / (i32)0xF4240);
        l_2 = LO16(((u32)((i32)(a0 - ((u32)(MK32(((i16)LO16(t3)) >> 0xF, LO16(t3)) * 0xF4240))) / (i32)0x186A0)));
        t7 = str_format(DSP(0x12C4) /* "#d,#d" */, LO16(t3), l_2);
        l_4 = HI16(t7);
        l_6 = LO16(t7);
    }
    ax = l_6;
    dx = l_4;
    rv_ = MK32(dx, ax);
    return rv_;
}

/* 1DA8:4258  (ad, group) 1 if the group satisfies the ad's target condition (+0xB..+0xD: negative = excluded group, positive = required groups, none = any) */
u16 ad_target_group_ok(u16 a0, u16 a1) {
    u16 ax;
    int cc;
    u16 di;
    u16 si;
    u32 l_4;
    FUNC_TICK(33);
    l_4 = FPADD(ads_at(a0), 0xB);        /* the ad's genre rule bytes */
    si = 1;
    di = 1;
    while (((i16)si) <= 3) {
        LOOP_TICK(13);
        cc = ((i8)FP8(l_4, si)) >= 0;
        if (!(cc || ((u16)(-(u16)((u16)(i8)FP8(l_4, si)))) != a1)) {
            di = 0;
        }
        si = si + 1;
    }
    if (di != 0) {
        si = 1;
        di = 2;
        while (((i16)si) <= 3) {
            LOOP_TICK(15);
            if (((i8)FP8(l_4, si)) > 0) {
                if (di == 2) {
                    di = 0;
                }
            }
            if (((u16)((u16)(i8)FP8(l_4, si))) == a1) {
                di = 1;
            }
            si = si + 1;
        }
        if (di == 2) {
            di = 1;
        }
    }
    ax = di;
    return ax;
}

/* 1DA8:42E5  number of the human's news items (0x457F) with category (+0x1C) == c */
u16 news_count_category(u16 a0) {
    u16 ax;
    u16 di;
    u16 si;
    FUNC_TICK(13);
    si = 0;
    di = 0;
    while (((i16)g_player_news_count(g_player)) > ((i16)si)) {
        LOOP_TICK(22);
        if (((u16)((u16)(i8)(inbox(g_player, si).category))) == a0) {
            di = di + 1;
        }
        si = si + 1;
    }
    ax = di;
    return ax;
}

/* 1DA8:432D  next news index after start with category c, -1 if none */
u16 news_find_category(u16 a0, u16 a1) {
    u16 ax;
    u16 si;
    FUNC_TICK(18);
    si = a1 + 1;
    while (((i16)g_player_news_count(g_player)) > ((i16)si)) {
        LOOP_TICK(20);
        if ((inbox(g_player, si).category) == ((u8)((u8)a0))) break;
        si = si + 1;
    }
    if (((i16)g_player_news_count(g_player)) <= ((i16)si)) {
        ax = 0xFFFF;
    } else {
        ax = si;
    }
    return ax;
}

/* 1DA8:4384  -1 if player's image (+0x15) >= n, else 0 */
u16 player_image_at_least(u16 a0, u16 a1) {
    u16 ax;
    FUNC_TICK(18);
    if (((i16)(players(a0).image)) < ((i16)a1)) {
        ax = 0;
    } else {
        ax = 0xFFFF;
    }
    return ax;
}

/* 1DA8:43AD  news agency 1..3 cancels a player's subscription when image < {0,5,10,15}[agency] (message 0x152) */
void news_agency_image_check(void) {
    u16 di;
    u16 si;
    u32 l_4;
    FUNC_TICK(13);
    di = 1;
    while (((i16)di) <= 3) {
        LOOP_TICK(54);
        l_4 = players_at(di);
        si = 1;
        while (((i16)si) <= 3) {
            LOOP_TICK(40);
            if (FP8(l_4, si + 0x10) != 0) {
                if (((i16)(AT(player, l_4).image)) < ((i16)(G16((si << 1) + 0x12A8)))) {
                    FP8(l_4, si + 0x10) = 0;
                    if (mp_is_human(di)) {   /* di == g_player (multiplayer: every human, msg_post finds it) */
                        msg_post(0x152, di, MK32(((i16)si) >> 0xF, si), 0, 0, 0, 0);
                    }
                }
            }
            si = si + 1;
        }
        di = di + 1;
    }
    return;
}

/* 1DA8:4439  (player, op) Betty's master key bit set 0x7A86: op 1 = give, 2 = take back, 0 = query; returns bit */
u16 master_key(u16 a0, u16 a1) {
    u16 ax;
    FUNC_TICK(33);
    if (a1 != 1) {
        if (a1 == 2) {
            bit_clear(DSP(0x7A86), (u16)(i8)((u8)a0));
        }
    } else {
        bit_set(HPTR(DSP(0x7A86)), (u16)(i8)((u8)a0));
    }
    ax = (u16)(i8)((u8)bit_test(g_master_key, (u16)(i8)((u8)a0)));
    return ax;
}

/* 1DA8:4481  Sammy award: every 3 days a random category (only 1..2 before day 5); 2 days later the leader wins (image + {8,4,6,6}[cat], others lose all/half of it), tie -> 1-day extension or no award if nobody scored; returns day%3 */
u16 sammy_daily_update(void) {
    u16 ax;
    u16 di;
    u16 s0;
    u16 si;
    u32 rv_;
    u16 t4;
    u16 t5;
    u16 t14;
    u8 fr_[12] = {0};                  /* bytes bp-12 .. bp-1 of the original frame */
    FUNC_TICK(177);
    si = 1;
    di = 0;
    while (((i16)si) < 4) {
        LOOP_TICK(13);
        if ((players(si).out) == 0) {
            di = di + 1;
        }
        si = si + 1;
    }
    t4 = (u16)((i32)(i16)g_sammy_day % 3);
    if (((u32)(MK32(t4, g_sammy_category(0)))) == 0) {
        s0 = g_day;
        if (((i16)g_day) >= 5) {
            ax = 4;
        } else {
            ax = 2;
        }
        t5 = random_n(ax, s0);
        g_sammy_category(0) = t5;
        si = 1;
        while (((i16)si) < 4) {
            LOOP_TICK(6);
            g_sammy_category(si) = 0;
            si = si + 1;
        }
    } else if ((u16)((u16)((i32)(i16)g_sammy_day % 3)) == 2 && g_sammy_category(0) != 0) {
        if ((sammy_leader()) != 0) {
            t14 = sammy_leader();
            FR16(fr_, 12, 0xA) = t14;
            g_sammy_winner = t14;
            si = 1;
            while (((i16)si) < 4) {
                LOOP_TICK(43);
                if (si == FR16(fr_, 12, 0xA)) {
                    if (((i16)di) > 1) {
                        FRX16(fr_, 12, 0x8, (si << 1) + 0) = G16(((g_sammy_category(0) - 1) << 1) + 0x12B0);
                    }
                    g_sammy_category(si) = 1;
                } else {
                    g_sammy_category(si) = 0;
                    if (((i16)di) > 1) {
                        if (di != 2) {
                            ax = -(u16)(((i16)(G16(((g_sammy_category(0) - 1) << 1) + 0x12B0))) >> 1);
                        } else {
                            ax = -G16(((g_sammy_category(0) - 1) << 1) + 0x12B0);
                        }
                        FRX16(fr_, 12, 0x8, (si << 1) + 0) = ax;
                    }
                }
                si = si + 1;
            }
            image_apply_deltas((fr_ + 12 - 0x8));
            sammy_pay(FR16(fr_, 12, 0xA), g_sammy_category(0));    /* option --sammy-prize (src/fixes.c) */
            for (si = 1; si < 4; si++) {            /* multiplayer: every human hears it (alone: g_player) */
                if (!mp_is_human(si)) continue;
                if (g_sammy_category(si) != 1) {
                    msg_post(0x158, si, MK32(((i16)g_sammy_category(0)) >> 0xF, g_sammy_category(0)), MK32(((i16)FR16(fr_, 12, 0xA)) >> 0xF, FR16(fr_, 12, 0xA)), 0, 0, 0);
                } else {
                    msg_post(0x159, si, MK32(((i16)g_sammy_category(0)) >> 0xF, g_sammy_category(0)), sammy_prize(g_sammy_category(0)), 0, 0, 0);
                }
            }
            g_sammy_category(0) = 0;
        } else if ((sammy_score(1)) != 0 || (sammy_score(2)) != 0) {
            g_sammy_day = g_sammy_day - 1;
            for (si = 1; si < 4; si++)
                if (mp_is_human(si)) msg_post(0x15A, si, 0, 0, 0, 0, 0);
        } else {
            for (si = 1; si < 4; si++)
                if (mp_is_human(si)) msg_post(0x2B, si, MK32(((i16)g_sammy_category(0)) >> 0xF, g_sammy_category(0)), 0, 0, 0, 0);
            g_sammy_category(0) = 0;
        }
    }
    ax = (u16)((u16)((i32)(i16)g_sammy_day % 3));
    rv_ = ax;
    return (u16)rv_;
}

/* 1DA8:4694  player's Sammy score: category points + Betty love/2 (news, series), + love (culture), + love/3 (shows); 0 for dead players */
u16 sammy_score(u16 a0) {
    u16 ax;
    u16 di;
    u16 si;
    u16 t9;
    FUNC_TICK(76);
    si = a0;
    di = 0;
    if ((players(a0).out) == 0) {
        if (g_sammy_category(si) != 0) {
            ax = g_sammy_category(0) - 1;
            if (((u16)(g_sammy_category(0) - 1)) <= 3) {
                switch (ax) {
                case 0:
                case 3:
                    di = g_sammy_category(si) + (((i16)(players(si).betty_love)) >> 1);
                    break;
                case 1:
                    di = g_sammy_category(si) + (players(si).betty_love);
                    break;
                case 2:
                    t9 = g_sammy_category(si);
                    di = t9 + (u16)((u16)((i32)(i16)(players(si).betty_love) / 3));
                    break;
                default:
                    cpu_halt("bad switch index %u in 1DA8:4694", ax);
                }
            }
        }
    }
    ax = di;
    return ax;
}

/* 1DA8:4752  player with the highest Sammy score, 0 on a tie */
u16 sammy_leader(void) {
    u16 ax;
    u16 di;
    u16 si;
    u16 l_2;
    u16 l_4;
    u16 t1;
    u16 t3;
    FUNC_TICK(22);
    l_4 = 0;
    l_2 = 0;
    si = 1;
    di = 0;
    while (((i16)si) < 4) {
        LOOP_TICK(26);
        if ((players(si).out) == 0) {
            t1 = sammy_score(si);
            if (((i16)t1) > ((i16)di)) {
                l_4 = si;
                di = sammy_score(si);
            }
        }
        si = si + 1;
    }
    si = 1;
    while (((i16)si) < 4) {
        LOOP_TICK(20);
        if ((players(si).out) == 0) {
            t3 = sammy_score(si);
            if (t3 == di) {
                l_2 = l_2 + 1;
            }
        }
        si = si + 1;
    }
    if (((i16)l_2) > 1) {
        l_4 = 0;
    }
    ax = l_4;
    return ax;
}

/* 1DA8:47E7  player's Sammy score as % of all players' scores */
u16 sammy_score_pct(u16 a0) {
    u16 ax;
    u16 di;
    u16 si;
    u16 t1;
    FUNC_TICK(25);
    si = 1;
    di = 0;
    while (((i16)si) < 4) {
        LOOP_TICK(9);
        t1 = sammy_score(si);
        si = si + 1;
        di = di + t1;
    }
    if (di == 0) {
        ax = 0;
    } else {
        ax = (u16)((u16)((i32)(i16)(sammy_score(a0) * 0x64) / (i16)di));
    }
    return ax;
}
