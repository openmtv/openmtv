/* Computer players I: state machine, schedule, ads, purchases, budget, transmitter search.
 *
 * Original module 2DD9 of MTV.EXE (Rainbow Arts, 1991).  Decompiled by
 * tools/decomp.py and hand-maintained since the freeze (git tag
 * generated-final).  Every function keeps its original address in the
 * comment above it; that address ties it to the machine-level twin in gen/
 * and to the difftest tables -- keep it when editing.
 */
#include "game.h"
#include "seat.h"
#include "fixes.h"
#include "ai_improve.h"

/* ORIGINAL BUG (2DD9:1147, fix "ai-odd-range"): ai_fill_program_slot reads its
 * half-day range [bp-12h]/[bp-11h] before writing it (see there).  Those two
 * bytes are the stack word 40 bytes below the SP at which ai_tick (2DD9:000C)
 * calls its state functions; the original's last write to it is, within the
 * same ai_tick:
 *   - door_check_entry (1DA8:3643) called by ai_ready_at_target (2DD9:3985):
 *     its prologue saves the caller's DI there, and ai_ready_at_target only
 *     calls it with DI = 1 -- and must have called it, or ai_tick would not
 *     act (ai_office_book_spots runs only when ai_ready_at_target returns 1);
 *   - then, if ai_place_live_programs (2DD9:4719, from ai_office_book_spots)
 *     places a live film, its far call of sched_mark_scheduled pushes the
 *     return segment there (CS = load segment + 2DD9).
 * The other calls in between (the rest of ai_ready_at_target; clock_hour,
 * sched_mark_scheduled and ai_contracts_all_booked from ai_office_book_spots)
 * stay at least 4 bytes above it, so the word is tracked here.  Not covered:
 * an interrupt handler pushing that deep in between (not seen in any measured
 * run: --stackchk names door_check_entry, or the live-film call, as the last
 * writer every time). */
static u16 ai_stale_range = 1;

/* 2DD9:000C  run one step of the current computer player (if not out): when ai_ready_at_target, act for its state (0 office: book contract spots, 1 ad agency: take contracts, 2 movie agency: plan/buy/auction rebid/sell in 4 sub-steps, 3 news room: buy news, 4 office: transmitter purchase or schedule tuning, 7 production errand, 8 office: ai_plan_programs x15, 9 buy gift, 10 give gift, 11 swap signs, others nothing) then ai_next_goal; finally switch to the other AI */
void ai_tick(void) {
    u16 ax;
    u16 di;
    u16 si;
    u8 l_1;
    u8 l_2;
    u16 t9;
    FUNC_TICK(283);
    if (g_ai_player((u16)(i8)g_ai_cur) == 0) {   /* multiplayer: a human's slot; the one computer */
        ai_switch();                             /* player still steps every second call */
        return;
    }
    l_2 = g_ai_player((u16)(i8)g_ai_cur);
    di = (u16)(i8)(g_ai_state((u16)(i8)g_ai_cur));
    if ((players((u16)(i8)l_2).out) == 0) ai_improve_step((u16)(i8)l_2);   /* --ai-improve: watching the human (ai_improve.c) */
    if ((players((u16)(i8)l_2).out) == 0 && ((u8)((u8)(ai_ready_at_target()))) == 1) {
        ax = di + 1;
        if (((u16)(di + 1)) <= 0xD) {
            switch (ax) {
            case 0:
                ai_next_goal();
                break;
            case 1:
                ai_office_book_spots();
                break;
            case 2:
                ai_take_contracts();
                ai_next_goal();
                break;
            case 3:
                ax = g_ai_step((u16)(i8)g_ai_cur);
                if ((g_ai_step((u16)(i8)g_ai_cur)) <= 3) {
                    switch (ax) {
                    case 0:
                        ai_plan_film_purchases();
                        ai_auction_rebid();
                        break;
                    case 1:
                        ai_execute_purchases();
                        dealer_refill();
                        break;
                    case 2:
                        break;
                    case 3:
                        if (((i8)((u8)(library_total_blocks((u16)(i8)l_2, (u16)(i8)l_2)))) < 14) break;
                        ai_sell_weak_programs();
                        break;
                    default:
                        cpu_halt("bad switch index %u in 2DD9:000C", ax);
                    }
                }
                g_ai_step((u16)(i8)g_ai_cur) = (g_ai_step((u16)(i8)g_ai_cur)) + 1;
                if (((i16)(g_ai_step((u16)(i8)g_ai_cur))) <= 3) break;
                g_ai_step((u16)(i8)g_ai_cur) = 0;
                ai_next_goal();
                break;
            case 4:
                ai_buy_news();
                ai_next_goal();
                break;
            case 5:
                t9 = ai_want_transmitter();
                l_1 = (u8)t9;
                if (((u8)((u8)t9)) == 0) {
                    ai_upgrade_ads();
                    ai_replace_weak_programs();
                    ai_fill_ad_slots();
                    ai_optimize_ads();
                    ai_next_goal();
                    g_ai_tx_busy((u16)(i8)l_2) = 0;
                    break;
                } else if (l_1 != 2) {
                    g_ai_tx_busy((u16)(i8)l_2) = 1;
                    if (((i16)(g_ai_cursor((u16)(i8)g_ai_cur))) > 32) {
                        ai_scan_transmitter_site();
                        break;
                    } else {
                        if (g_ai_cursor((u16)(i8)g_ai_cur) == 0) ai_improve_rebuild((u16)(i8)l_2);   /* (its follow-up search starts anew) */
                        ai_rebuild_coverage_step();
                        g_ai_cursor((u16)(i8)g_ai_cur) = (g_ai_cursor((u16)(i8)g_ai_cur)) + 1;
                        break;
                    }
                } else {
                    g_ai_cursor((u16)(i8)g_ai_cur) = 0;
                    g_ai_tx_busy((u16)(i8)l_2) = 1;
                    break;
                }
                break;
            case 6:
                ai_next_goal();
                break;
            case 7:
                ai_next_goal();
                break;
            case 8:
                ai_production_status((u16)(i8)l_2);
                si = (u16)(i8)(g_ai_studio((((u16)(i8)l_2) * 3) + 2));
                if (((u16)((u16)(i8)(g_ai_studio((((u16)(i8)l_2) * 3) + 2)))) == 0) {
                    ai_next_goal();
                    break;
                } else if (si != 1) {
                    if (si == 2) {
                        ai_buy_script();
                        break;
                    } else if (si != 3) {
                        if (si != 4) {
                            if (si != 5) {
                                if (si != 6) break;
                                ai_start_production();
                                break;
                            } else {
                                ai_buy_cast();
                                break;
                            }
                        } else {
                            ai_next_shopping_list();
                            break;
                        }
                    } else {
                        ai_buy_script();
                        break;
                    }
                } else {
                    ai_rent_studio();
                    break;
                }
                break;
            case 9:
                ai_plan_programs();
                g_ai_step((u16)(i8)g_ai_cur) = (g_ai_step((u16)(i8)g_ai_cur)) + 1;
                if (((i16)(g_ai_step((u16)(i8)g_ai_cur))) <= 14) break;
                g_ai_step((u16)(i8)g_ai_cur) = 0;
                ai_next_goal();
                break;
            case 0xA:
                ai_buy_betty_gift();
                ai_next_goal();
                break;
            case 0xB:
                ai_give_betty_gift();
                ai_next_goal();
                break;
            case 0xC:
                ai_swap_signs_sabotage();
                ai_next_goal();
                break;
            case 0xD:
                ai_next_goal();
                break;
            default:
                cpu_halt("bad switch index %u in 2DD9:000C", ax);
            }
        }
    } else if (!((players((u16)(i8)l_2).out) != 0 || di != 5 || (shoplists(0x1A, (((u16)(i8)l_2) * 2) + 4).script) != (fig_target_floor(((u16)(i8)l_2) - 1)) || (figures(g_player_figure).floor) != (shoplists(0x1A, (((u16)(i8)l_2) * 2) + 4).script))) {
        ai_next_goal();
    }
    ai_switch();
    return;
}

/* 2DD9:0349  from the next hour: a scheduled film (not live) whose topicality < player average quality, whose genre the AI's ads exclude, or whose estimated viewers < AI's min audience is replaced (once per call) by an unscheduled library film of the same length with topicality > average and allowed genre; its ad spots in those hours are removed (aborts if a spot belongs to a fully booked contract) */
void ai_replace_weak_programs(void) {
    u16 ax;
    u16 di;
    u16 si;
    u32 l_4;
    u32 l_8;
    u32 l_C;
    u32 l_10;
    u32 l_14;
    u8 l_15;
    u8 l_16;
    u16 l_18;
    u16 l_1A;
    u16 l_1C;
    u16 l_1E;
    u16 l_20;
    u16 l_22;
    u16 l_24;
    u16 l_26;
    u16 t10;
    u16 t11;
    u32 t17;
    u16 t24;
    FUNC_TICK(67);
    if (((i16)g_minute) <= 5) {
        ax = clock_hour();
    } else {
        ax = clock_hour() + 1;
    }
    l_1E = ax;
    l_16 = g_ai_player((u16)(i8)g_ai_cur);
    l_15 = 1;
    l_10 = schedule_at((u16)(i8)l_16);
    l_4 = library_at((u16)(i8)l_16);
    l_26 = l_1E;
    while (((i16)l_26) <= 14) {
        LOOP_TICK(308);
        l_1C = FP16(l_10, l_26 << 1);
        l_C = films_at(l_1C);
        if (l_1C != 0x270F) {
            l_24 = l_26;
        } else {
            l_24 = l_26;
            while (FP16(l_10, l_24 << 1) == 0x270F) {
                LOOP_TICK(7);
                l_24 = l_24 - 1;
            }
            l_1C = FP16(l_10, l_24 << 1);
        }
        if (l_1C != 0 && ((i16)l_1C) < 1000 && ((i16)l_24) > ((i16)l_1E) && (films(l_1C).genre) != GENRE_LIVE) {
            l_14 = ai_estimate_audience(l_26, 0, 1);
            if (((i16)((u16)(i8)(AT(film, l_C).topicality))) < ((i16)(g_avg_quality((u16)(i8)l_16))) || ((u8)((u8)(bit_test(g_ai_ad_req(((u16)(i8)l_16) * 3), ((u16)(i8)(AT(film, l_C).genre)) - 1)))) == 0) {
L_04D0:
                if (l_15 == 1) {
                    l_22 = 1;
                    while (((i16)l_22) <= 10) {
                        LOOP_TICK(184);
                        l_20 = 0;
                        while (((i16)((u16)(i8)FP8(l_4, l_22 + 0x60))) > ((i16)l_20)) {
                            LOOP_TICK(179);
                            l_1A = FP16(l_4, (l_22 << 3) + (l_20 << 1));
                            l_8 = films_at(l_1A);
                            t17 = ai_estimate_audience(l_26, l_1A, 0);
                            ax = LO16(t17);
                            if (((i32)t17) < ((i32)l_14)) {
                                if (((u8)((u8)(bit_test(g_ai_ad_req(((u16)(i8)l_16) * 3), ((u16)(i8)(AT(film, l_C).genre)) - 1)))) != 0) {
                                    l_15 = 0;
                                }
                            }
                            if (l_1A != 0) {
                                if (((i16)((u16)(i8)(AT(film, l_8).topicality))) > ((i16)(g_avg_quality((u16)(i8)l_16))) && ((u16)(((i16)((AT(film, l_8).flags2_16) << 0xE)) >> 0xF)) == 0 && (AT(film, l_8).blocks) == (AT(film, l_C).blocks) && l_15 == 1 && ((u8)((u8)(bit_test(g_ai_ad_req(((u16)(i8)l_16) * 3), ((u16)(i8)(AT(film, l_8).genre)) - 1)))) != 0) {
                                    di = l_24;
                                    while (((i16)(((u16)(i8)(AT(film, l_C).blocks)) + l_24)) > ((i16)di)) {
                                        LOOP_TICK(77);
                                        t24 = FP16(l_10, (di << 1) + 0x3C);
                                        l_18 = FP16(l_10, (di << 1) + 0x3C);
                                        if (t24 != 0) {
                                            si = 0;
                                            while ((contracts((u16)(i8)l_16, si).ad) != l_18) {
                                                LOOP_TICK(23);
                                                if (((i16)(g_player_ad_count((u16)(i8)l_16))) <= ((i16)si)) break;
                                                si = si + 1;
                                            }
                                            if (((i16)si) < 10) {
                                                if (((u16)(((i16)((G16((((((u16)(i8)l_16) * 0x4D) + 0x4EDF) + (si * 7)) + 6)) << 0xD)) >> 0xF)) != 0) {
                                                    l_15 = 0;
                                                } else {
                                                    FP16(l_10, (di << 1) + 0x3C) = 0;
                                                    ad_plan_book(di, si, 0, (u16)(i8)l_16);
                                                }
                                            }
                                        }
                                        di = di + 1;
                                    }
                                    if (l_15 != 1) {
                                        l_20 = l_20 + 1;
                                        continue;
                                    } else {
                                        goto L_06AF;
                                    }
                                } else {
                                    l_20 = l_20 + 1;
                                    continue;
                                }
                            } else {
                                l_20 = l_20 + 1;
                                continue;
                            }
                        }
                        l_22 = l_22 + 1;
                    }
                }
            } else {
                t10 = g_ai_ad_req((((u16)(i8)l_16) * 3) + 2);
                t11 = HI16(l_14);
                ax = g_ai_ad_req((((u16)(i8)l_16) * 3) + 1);
                if (((i16)(g_ai_ad_req((((u16)(i8)l_16) * 3) + 2))) >= ((i16)HI16(l_14)) && (((i16)t10) > ((i16)t11) || ax > LO16(l_14))) goto L_04D0;
            }
        }
L_06F9:
        l_24 = 0;
        l_26 = l_26 + 1;
    }
    return;
L_06AF:
    sched_remove_program((u16)(i8)l_16, l_26);
    sched_place_program(l_20, l_22, l_24, (u16)(i8)l_16);
    goto L_06F9;
}

/* 2DD9:0710  from the next hour (:55 rule) every booked ad spot whose minimum audience exceeds the estimated viewers is removed; otherwise it is swapped for ai_find_better_ad if one exists */
void ai_upgrade_ads(void) {
    u16 ax;
    u16 bx;
    u16 di;
    u16 si;
    u8 l_1;
    u16 l_4;
    u16 l_6;
    u16 l_8;
    u16 t9;
    u16 t10;
    u32 t11;
    u16 t15;
    FUNC_TICK(34);
    if (((i16)g_minute) <= 55) {
        ax = clock_hour();
    } else {
        ax = clock_hour() + 1;
    }
    l_8 = ax;
    l_1 = g_ai_player((u16)(i8)g_ai_cur);
    if (l_8 == 0) {
        l_8 = 1;
    }
    di = l_8;
    while (((i16)di) <= 14) {
        LOOP_TICK(158);
        l_6 = schedule_ad((u16)(i8)l_1, di);
        if (l_6 != 0) {
            if ((schedule_program((u16)(i8)l_1, di)) != 0) {
                si = 0;
                while ((contracts((u16)(i8)l_1, si).ad) != l_6) {
                    LOOP_TICK(23);
                    if (((i16)(g_player_ad_count((u16)(i8)l_1))) <= ((i16)si)) break;
                    si = si + 1;
                }
                if (((i16)(g_player_ad_count((u16)(i8)l_1))) > ((i16)si)) {
                    t9 = G16(((((u16)(i8)(ads(contracts((u16)(i8)l_1, si).ad).min_class)) - 1) << 2) + 0xAA);
                    t10 = G16(((((u16)(i8)(ads(contracts((u16)(i8)l_1, si).ad).min_class)) - 1) << 2) + 0xA8);
                    t11 = ai_estimate_audience(di, 0, 1);
                    ax = LO16(t11);
                    bx = t10;
                    if (((i32)MK32(t9, t10)) > ((i32)t11)) {
                        schedule_ad((u16)(i8)l_1, di) = 0;
                        ad_plan_book(di, si, 0, (u16)(i8)l_1);
                    } else {
                        t15 = ai_find_better_ad(si, di);
                        l_4 = t15;
                        if (t15 != 0) {
                            ad_plan_book(di, si, 0, (u16)(i8)l_1);
                            sched_place_ad(l_4, di, (u16)(i8)l_1);
                            ad_plan_book(di, l_4, 1, (u16)(i8)l_1);
                        }
                    }
                }
            }
        }
        di = di + 1;
    }
    return;
}

/* 2DD9:08D1  (contract idx, hour) if the contract is over-booked in that half (today/tomorrow) and not complete: the not-complete contract with the highest minimum audience still <= estimated viewers and genre-compatible (stops at one still needing spots); returns its idx or 0 */
u16 ai_find_better_ad(u16 a0, u16 a1) {
    u16 ax;
    u16 di;
    u16 si;
    u32 l_4;
    u32 l_8;
    u32 l_C;
    u32 l_10;
    u32 l_14;
    u8 l_15;
    u8 l_16;
    FUNC_TICK(116);
    l_16 = g_ai_player((u16)(i8)g_ai_cur);
    if (((i16)a1) > 7) {
        ax = (((u8)((u8)(ax >> 8))) << 8) | 1;
    } else {
        ax = (((u8)((u8)(ax >> 8))) << 8) | 0;
    }
    l_15 = (u8)ax;
    l_14 = 0;
    l_10 = 0;
    l_C = ai_estimate_audience(a1, 0, 1);
    l_8 = G32(((((u16)(i8)(ads(contracts((u16)(i8)l_16, a0).ad).min_class)) - 1) << 2) + 0xA8);
    l_4 = ai_ad_plan_at((u16)(i8)l_16, a0);
    di = 0;
    if (((i8)((u8)((((u8)((u8)(((u16)((u16)(i8)l_15)) >> 8))) << 8) | (FP8(l_4, (u16)(i8)l_15))))) < ((i8)(FP8(l_4, ((u16)(i8)l_15) + 2))) && ((u16)(((i16)(FP16(l_4, 6) << 0xD)) >> 0xF)) == 0) {
        si = 0;
        while (((i16)(g_player_ad_count((u16)(i8)l_16))) > ((i16)si)) {
            LOOP_TICK(118);
            l_4 = ai_ad_plan_at((u16)(i8)l_16, si);
            l_10 = G32(((((u16)(i8)(ads(contracts((u16)(i8)l_16, si).ad).min_class)) - 1) << 2) + 0xA8);
            ax = LO16(l_10);
            if (((i16)HI16(l_10)) >= ((i16)HI16(l_8)) && (((i16)HI16(l_10)) > ((i16)HI16(l_8)) || ax > LO16(l_8))) {
                ax = LO16(l_10);
                if (((i16)HI16(l_10)) >= ((i16)HI16(l_14)) && (((i16)HI16(l_10)) > ((i16)HI16(l_14)) || ax > LO16(l_14))) {
                    ax = LO16(l_10);
                    if (((i16)HI16(l_10)) <= ((i16)HI16(l_C)) && (((i16)HI16(l_10)) < ((i16)HI16(l_C)) || ax < LO16(l_C)) && !(((u16)(((i16)(FP16(l_4, 6) << 0xD)) >> 0xF)) != 0 || ((u8)((u8)(ad_genre_allowed(a1, contracts((u16)(i8)l_16, si).ad, 0)))) != 1)) {
                        l_14 = l_10;
                        di = si;
                        if (((i8)((u8)((((u8)((u8)(((u16)((u16)(i8)l_15)) >> 8))) << 8) | (FP8(l_4, (u16)(i8)l_15))))) > ((i8)(FP8(l_4, ((u16)(i8)l_15) + 2)))) break;
                        si = si + 1;
                        continue;
                    } else {
                        si = si + 1;
                        continue;
                    }
                } else {
                    si = si + 1;
                    continue;
                }
            } else {
                si = si + 1;
                continue;
            }
        }
    }
    if (di != a0) {
        ax = di;
    } else {
        ax = 0;
    }
    return ax;
}

/* 2DD9:0B13  from the next hour: every slot with a program but no ad gets the genre-compatible contract with the highest minimum audience <= estimated viewers (preferring contracts still short of their daily plan) */
void ai_fill_ad_slots(void) {
    u16 ax;
    int cc;
    u16 di;
    u16 si;
    u32 l_4;
    u32 l_8;
    u32 l_C;
    u32 l_10;
    u8 l_11;
    u8 l_12;
    u8 l_13;
    u8 l_14;
    u8 l_15;
    u8 l_16;
    u8 l_17;
    u8 l_18;
    u16 t1;
    u16 t4;
    u16 t8;
    u16 t11;
    u16 t16;
    FUNC_TICK(57);
    l_18 = g_ai_player((u16)(i8)g_ai_cur);
    if (((i16)g_minute) <= 55) {
        ax = clock_hour();
    } else {
        t1 = clock_hour();
        ax = (((u8)((u8)(t1 >> 8))) << 8) | ((u8)(((u8)t1) + 1));
    }
    l_17 = (u8)ax;
    l_13 = 0;
    l_12 = 0;
    l_11 = 0;
    t4 = g_player_ad_count((u16)(i8)l_18);
    l_C = 0;
    l_4 = schedule_at((u16)(i8)l_18);
    di = t4;
    if (l_17 == 0) {
        l_17 = 1;
    }
    l_15 = l_17;
    while (((i8)l_15) <= 14) {
        LOOP_TICK(225);
        if (((i8)l_15) <= 7) {
            ax = (((u8)((u8)(ax >> 8))) << 8) | 0;
        } else {
            ax = (((u8)((u8)(ax >> 8))) << 8) | 1;
        }
        l_14 = (u8)ax;
        l_10 = ai_estimate_audience((u16)(i8)l_15, 0, 1);
        if ((FP16(l_4, (((u16)(i8)l_15) << 1) + 0x3C)) == 0) {
            if ((FP16(l_4, ((u16)(i8)l_15) << 1)) != 0) {
                l_16 = 0;
                for (;;) {
                    LOOP_TICK(157);
                    cc = ((i16)((u16)(i8)l_16)) >= ((i16)di);
                    if (cc || l_12 != 0) break;
                    t8 = contracts((u16)(i8)l_18, (u16)(i8)l_16).ad;
                    l_8 = ai_ad_plan_at((u16)(i8)l_18, (u16)(i8)l_16);
                    si = t8;
                    if (((u16)(((i16)(FP16(l_8, 6) << 0xF)) >> 0xF)) != 0 || ((u16)(((i16)(FP16(l_8, 6) << 0xE)) >> 0xF)) != 0) {
L_0C89:
                        if (((u8)((u8)(ad_genre_allowed((u16)(i8)l_15, si, 0)))) == 1) {
                            t16 = G16(((((u16)(i8)(ads(si).min_class)) - 1) << 2) + 0xAA);
                            ax = G16(((((u16)(i8)(ads(si).min_class)) - 1) << 2) + 0xA8);
                            if (((i16)(G16(((((u16)(i8)(ads(si).min_class)) - 1) << 2) + 0xAA))) <= ((i16)HI16(l_10)) && (((i16)t16) < ((i16)HI16(l_10)) || ax < LO16(l_10)) && ((u16)(((i16)(FP16(l_8, 6) << 0xD)) >> 0xF)) == 0) {
                                l_C = G32(((((u16)(i8)(ads(contracts((u16)(i8)l_18, (u16)(i8)l_16).ad).min_class)) - 1) << 2) + 0xA8);
                                l_13 = l_16;
                                l_11 = 1;
                                if (((i8)((u8)((((u8)((u8)(((u16)((u16)(i8)l_14)) >> 8))) << 8) | (FP8(l_8, (u16)(i8)l_14))))) > ((i8)(FP8(l_8, ((u16)(i8)l_14) + 2)))) {
                                    l_12 = 1;
                                }
                            }
                        }
                    } else {
                        t11 = G16(((((u16)(i8)(ads(si).min_class)) - 1) << 2) + 0xAA);
                        cc = ((i16)(G16(((((u16)(i8)(ads(si).min_class)) - 1) << 2) + 0xAA))) >= ((i16)HI16(l_C));
                        ax = G16(((((u16)(i8)(ads(si).min_class)) - 1) << 2) + 0xA8);
                        if (cc && (t11 != HI16(l_C) || ax >= LO16(l_C))) goto L_0C89;
                    }
                    l_16 = l_16 + 1;
                }
                if (l_11 == 1) {
                    sched_place_ad((u16)(i8)l_13, (u16)(i8)l_15, (u16)(i8)l_18);
                    ai_ad_requirements();
                    ad_plan_book((u16)(i8)l_15, (u16)(i8)l_13, 1, (u16)(i8)l_18);
                }
                l_12 = 0;
                l_11 = 0;
                l_13 = 0;
                l_C = 0;
            }
        }
        l_15 = l_15 + 1;
    }
    return;
}

/* 2DD9:0DD9  one empty hour per call (cursor g_ai_cursor from the current hour to 14): pick the library program (genre allowed by the AI mask, est. viewers >= AI minimum, fits the free block, no live, no X-rated before 21h, one series episode per day) maximising g_genre_audience[genre][hour]*topicality; prefers topicality >= player average; limits odd-length programs per half day; done at 14 -> ai_next_goal. ?estimate is called with the category instead of the hour (original bug) */
void ai_fill_program_slot(void) {
    u16 ax;
    u16 bx;
    u16 di;
    u16 es;
    u16 s0;
    u16 si;
    u16 t1;
    u16 t6;
    u16 t44;
    u16 t49;
    u32 t50;
    u16 t51;
    u16 t52;
    u8 l_1C = 0;
    u8 l_1B = 0;
    u8 l_1A = 0;
    u8 l_19 = 0;
    u8 l_18 = 0;
    u8 l_17 = 0;
    u8 l_16 = 0;
    u8 l_15 = 0;
    u8 l_14 = 0;
    u8 l_13 = 0;
    u8 l_12 = 0;
    u8 l_11 = 0;
    u8 l_10 = 0;
    u8 l_F = 0;
    u8 l_E = 0;
    u8 l_D = 0;
    u8 l_C = 0;
    u8 l_B = 0;
    u8 l_A = 0;
    u8 l_9 = 0;
    u8 l_8 = 0;
    u8 l_7 = 0;
    u8 best_genre = 0;                   /* fix "ai-fill-genre": l_8 of the best (l_7 is its category) */
    u8 l_6 = 0;
    u8 l_5 = 0;
    u16 l_4 = 0;
    u16 l_2 = 0;
    FUNC_TICK(183);
    l_1C = g_ai_player((u16)(i8)g_ai_cur);
    if (((i16)g_minute) <= 5) {
        ax = clock_hour();
    } else {
        t1 = clock_hour();
        ax = (((u8)((u8)(t1 >> 8))) << 8) | ((u8)(((u8)t1) + 1));
    }
    l_1A = (u8)ax;
    l_E = 0;
    l_C = 0;
    l_B = 0;
    l_A = 0;
    l_9 = 0;
    l_8 = 0;
    l_7 = 0;
    l_6 = 0;
    l_5 = 0;
    l_4 = 0;
    l_2 = 0;
    if (l_1A == 0) {
        l_1A = 1;
    }
    if (((i16)(g_ai_cursor((u16)(i8)g_ai_cur))) < ((i16)((u16)(i8)l_1A))) {
        g_ai_cursor((u16)(i8)g_ai_cur) = (u16)(i8)l_1A;
    }
    t6 = (((u8)((u8)(((u16)((u16)(i8)g_ai_cur)) >> 8))) << 8) | (G8((((u16)(i8)g_ai_cur) << 1) + 0x24EB));
    l_19 = G8((((u16)(i8)g_ai_cur) << 1) + 0x24EB);
    if (((i8)((u8)t6)) <= 14) {
        if (((i8)l_19) < 8) {
            ax = (((u8)((u8)(ax >> 8))) << 8) | 0;
        } else {
            ax = (((u8)((u8)(ax >> 8))) << 8) | 1;
        }
        l_10 = (u8)ax;
        if (((i8)l_19) < 8) {
            ax = (((u8)((u8)(ax >> 8))) << 8) | l_19;
        } else {
            ax = (((u8)((u8)(ax >> 8))) << 8) | ((u8)(l_19 + 0xF9));
        }
        l_1B = (u8)ax;
        if ((schedule_program((u16)(i8)l_1C, (u16)(i8)l_19)) == 0) {
            if (l_19 == l_1A) {
                l_5 = 1;
            }
            if (((i8)l_19) > 7) {
                ax = (((u8)((u8)(ax >> 8))) << 8) | 7;
            } else {
                ax = (((u8)((u8)(ax >> 8))) << 8) | 8;
            }
            l_13 = (u8)ax;
            l_18 = l_19;
            l_15 = 0;
            while (!((schedule_program((u16)(i8)l_1C, (u16)(i8)l_18)) != 0 || l_18 == l_13 || ((i8)l_18) > 14)) {
                LOOP_TICK(21);
                l_18 = l_18 + 1;
                l_15 = l_15 + 1;
            }
            /* ORIGINAL BUG (2DD9:1147, 2DD9:1197; fix "ai-odd-range"): the
             * half-day range 0x12..0x11 is only set when the series category
             * (11) comes up, but the odd-length check of the films (categories
             * 1..10, visited first) already uses it, so the original reads what
             * an earlier call left on the stack: almost always the DI = 1 that
             * door_check_entry saved there in this ai_tick, i.e. the range
             * 1..0, and the scan of the half day for other free hours never
             * runs (after a live film was placed: the bytes of a return
             * segment).  ai_stale_range (top of this file) tracks that word.
             * Fixed: the range the series branch sets, the half day of the
             * hour.  names/FINDINGS.md */
            if (fix_on(FIX_AI_ODD_RANGE)) {
                l_12 = ((i8)l_19) < 8 ? 1 : 8;
                l_11 = ((i8)l_19) < 8 ? 7 : 0xE;
            } else {
                l_12 = (u8)ai_stale_range;
                l_11 = (u8)(ai_stale_range >> 8);
            }
            l_17 = 1;
            while (((i8)l_17) <= 11) {
                LOOP_TICK(482);
                l_16 = 0;
                while (((i8)(library_count((u16)(i8)l_1C, (u16)(i8)l_17))) > ((i8)l_16)) {
                    LOOP_TICK(477);
                    si = library_items((u16)(i8)l_1C, (u16)(i8)l_17, (u16)(i8)l_16);
                    if ((library_items((u16)(i8)l_1C, (u16)(i8)l_17, (u16)(i8)l_16)) != 0) {
                        if (l_17 == 0xB) {
                            di = 0;
                            if (((i8)l_19) < 8) {
                                ax = (((u8)((u8)(ax >> 8))) << 8) | 1;
                            } else {
                                ax = (((u8)((u8)(ax >> 8))) << 8) | 8;
                            }
                            l_12 = (u8)ax;
                            if (((i8)l_19) < 8) {
                                ax = (((u8)((u8)(ax >> 8))) << 8) | 7;
                            } else {
                                ax = (((u8)((u8)(ax >> 8))) << 8) | 0xE;
                            }
                            l_11 = (u8)ax;
                            l_14 = l_12;
                            while (((i8)l_14) <= ((i8)l_11)) {
                                LOOP_TICK(22);
                                if ((schedule_program((u16)(i8)l_1C, (u16)(i8)l_14)) == ((u16)(si + 0x3E8))) {
                                    di = 1;
                                }
                                l_14 = l_14 + 1;
                            }
                            l_C = (u8)series_episode_at((u16)(i8)l_1C, si, (u16)(i8)l_19);
                            l_B = series(si).blocks;
                            l_A = FP8(g_series, ((si * 0x39) + (((u16)(i8)l_C) * 3)) + 1);
                            l_8 = (series(si).genre) + 0xA;
                        } else {
                            if (l_5 == 0) {
                                di = ((i16)((films(si).flags2_16) << 0xE)) >> 0xF;
                                if (((i8)l_1B) < 4) {
                                    if (di == 0) {
                                        di = ((i16)((films(si).flags2_16) << 0xC)) >> 0xF;
                                    }
                                }
                            } else {
                                di = 0;
                            }
                            l_B = films(si).blocks;
                            l_A = films(si).topicality;
                            l_8 = l_17;
                        }
                    }
                    if (((i8)l_B) <= ((i8)l_15)) {
                        if ((u16)((u16)((i32)(i16)((u16)(i8)l_B) % 2)) != 0) {
                            if (di == 0) {
                                if (!((g_ai_odd_today((((u16)(i8)l_10) * 4) + ((u16)(i8)l_1C))) != 0 || l_17 == 0xB)) {
                                    if (((i8)(G8((((u16)(i8)l_1C) << 1) + 0x24FF))) > ((i8)l_A)) {
                                        di = 1;
                                    }
                                }
                                if ((u16)((u16)((i32)(i16)((u16)(i8)(g_ai_odd_today((((u16)(i8)l_10) * 4) + ((u16)(i8)l_1C)))) % 2)) != 0) {
L_1133:
                                    if (((i8)(G8((((u16)(i8)l_1C) << 1) + 0x24FF))) > ((i8)l_A)) {
                                        di = 1;
                                    }
                                    l_D = l_12;
                                    while (((i8)l_D) <= ((i8)l_11)) {
                                        LOOP_TICK(34);
                                        if (!((schedule_program((u16)(i8)l_1C, (u16)(i8)l_D)) != 0 || !(((i8)l_D) < ((i8)l_19) || ((i16)((((u16)(i8)l_19) + ((u16)(i8)l_B)) - 1)) < ((i16)((u16)(i8)l_D))))) {
                                            l_E = 1;
                                        }
                                        l_D = l_D + 1;
                                    }
                                    if (l_E == 1) {
                                        if (((i8)l_B) <= 1) {
                                            ax = (((u8)((u8)(ax >> 8))) << 8) | 3;
                                        } else {
                                            ax = (((u8)((u8)(ax >> 8))) << 8) | 2;
                                        }
                                        l_F = (u8)ax;
                                        t49 = ai_count_short_programs();
                                        if (((i8)((u8)t49)) <= ((i8)l_F)) {
                                            di = 1;
                                        }
                                    }
                                } else {
                                    t44 = l_15;
                                    if ((u16)((u16)((i32)(i16)((u16)(i8)((((-t44) - (-t44)) - (t44 != 0)) + 1)) % 2)) != 0) goto L_1133;
                                }
                            }
                        }
                    }
                    if (((i8)l_B) <= ((i8)l_15)) {
                        if (di == 0) {
                            if (si != 0) {
                                if (((i16)si) > 1000) {
L_11F3:
                                    if (l_4 == 0) {
                                        l_4 = si;
                                        l_2 = (u16)(i8)l_16;
                                        if (l_17 != 0xB) {
                                            ax = (((u8)((u8)(((u16)(l_4 * 0x17)) >> 8))) << 8) | (films(l_4).topicality);
                                        } else {
                                            ax = (((u8)((u8)(((u16)(((u16)(i8)l_C) * 3)) >> 8))) << 8) | (FP8(g_series, ((l_4 * 0x39) + (((u16)(i8)l_C) * 3)) + 1));
                                        }
                                        l_9 = (u8)ax;
                                        if (l_17 != 0xB) {
                                            ax = (((u8)((u8)(((u16)(l_4 * 0x17)) >> 8))) << 8) | (films(l_4).blocks);
                                        } else {
                                            ax = (((u8)((u8)(((u16)(l_4 * 0x39)) >> 8))) << 8) | (series(l_4).blocks);
                                        }
                                        l_6 = (u8)ax;
                                        l_7 = l_17;
                                        best_genre = l_8;
                                    }
                                    s0 = 1;
                                    if (l_17 != 0xB) {
                                        ax = si;
                                    } else {
                                        ax = si + 0x3E8;
                                    }
                                    /* ORIGINAL BUG (2DD9:1289 mov al,[bp-23]): the hour argument is the category
                                     * instead of the hour being filled ([bp-25]). */
                                    t50 = ai_estimate_audience(fix_on(FIX_AI_FILL_HOUR) ? (u16)(i8)l_19 : (u16)(i8)l_17, ax, s0);
                                    t51 = HI16(t50);
                                    t52 = g_ai_ad_req((((u16)(i8)l_1C) * 3) + 2);
                                    ax = LO16(t50);
                                    bx = (((u16)(i8)l_1C) * 6) + 0x7764;
                                    es = DSEG;
                                    /* ORIGINAL BUG (fix "ai-fill-genre", 2DD9:12C1 / 2DD9:1316 mov al,[bp-7]):
                                     * the contracts' genre mask is tested with the best's category
                                     * instead of the candidate's genre ([bp-8], as every sibling does),
                                     * and the best's score reads g_genre_audience with its category:
                                     * a series best always gets row 11 (family series).  Fixed: the
                                     * candidate's genre; the best's genre (series genre + 10). */
                                    u8 mask_genre = fix_on(FIX_AI_FILL_GENRE) ? l_8 : l_7;
                                    u8 best_row = fix_on(FIX_AI_FILL_GENRE) ? best_genre : l_7;
                                    if (((i16)HI16(t50)) >= ((i16)(g_ai_ad_req((((u16)(i8)l_1C) * 3) + 2))) && (((i16)t51) > ((i16)t52) || ax > MEM16(es, bx + 2))) {
                                        if (((u8)((u8)(bit_test(g_ai_ad_req(((u16)(i8)l_1C) * 3), ((u16)(i8)mask_genre) - 1)))) != 0) {
                                            if (((i16)((G8(((((u16)(i8)l_8) << 3) + 0x7160) + ((u16)(i8)l_1B))) * ((u16)(i8)l_A))) >= ((i16)((G8(((((u16)(i8)best_row) << 3) + 0x7160) + ((u16)(i8)l_1B))) * ((u16)(i8)l_9)))) {
                                                l_4 = si;
                                                l_2 = (u16)(i8)l_16;
                                                if (l_17 != 0xB) {
                                                    ax = (((u8)((u8)(((u16)(l_4 * 0x17)) >> 8))) << 8) | (films(l_4).topicality);
                                                } else {
                                                    ax = (((u8)((u8)(((u16)(((u16)(i8)l_C) * 3)) >> 8))) << 8) | (FP8(g_series, ((l_4 * 0x39) + (((u16)(i8)l_C) * 3)) + 1));
                                                }
                                                l_9 = (u8)ax;
                                                if (l_17 != 0xB) {
                                                    ax = (((u8)((u8)(((u16)(l_4 * 0x17)) >> 8))) << 8) | (films(l_4).blocks);
                                                } else {
                                                    ax = (((u8)((u8)(((u16)(l_4 * 0x39)) >> 8))) << 8) | (series(l_4).blocks);
                                                }
                                                l_6 = (u8)ax;
                                                l_7 = l_17;
                                                best_genre = l_8;
                                                if (l_6 == l_15) goto L_13FE;
                                                l_16 = l_16 + 1;
                                                continue;
                                            } else {
                                                l_16 = l_16 + 1;
                                                continue;
                                            }
                                        } else {
                                            l_16 = l_16 + 1;
                                            continue;
                                        }
                                    } else {
                                        l_16 = l_16 + 1;
                                        continue;
                                    }
                                } else if ((films(si).genre) != GENRE_LIVE) {
                                    goto L_11F3;
                                } else {
                                    l_16 = l_16 + 1;
                                    continue;
                                }
                            } else {
                                l_16 = l_16 + 1;
                                continue;
                            }
                        } else {
                            l_16 = l_16 + 1;
                            continue;
                        }
                    } else {
                        l_16 = l_16 + 1;
                        continue;
                    }
                }
                l_17 = l_17 + 1;
            }
L_13FE:
            if (l_4 != 0) {
                sched_place_program(l_2, (u16)(i8)l_7, (u16)(i8)l_19, (u16)(i8)l_1C);
                if ((u16)((u16)((i32)(i16)((u16)(i8)l_6) % 2)) != 0) {
                    g_ai_odd_today((((u16)(i8)l_10) * 4) + ((u16)(i8)l_1C)) = (g_ai_odd_today((((u16)(i8)l_10) * 4) + ((u16)(i8)l_1C))) + 1;
                }
                l_4 = 0;
                l_2 = 0;
                if (((i8)g_ai_debug_step) > 0) {
                    for (;;) {
                        LOOP_TICK(3);
                        if (LO16((rtl_key_read())) == 0xFFFF) {
                            continue;
                        } else {
                            break;
                        }
                    }
                }
            }
        }
    }
    g_ai_cursor((u16)(i8)g_ai_cur) = (g_ai_cursor((u16)(i8)g_ai_cur)) + 1;
    if (((i16)(g_ai_cursor((u16)(i8)g_ai_cur))) > 14) {
        g_ai_cursor((u16)(i8)g_ai_cur) = 0;
        ai_next_goal();
    }
    return;
}

/* 2DD9:1498  state 0 (own office): refresh scheduled flags, place live programs; if contracts are not all booked: for contract g_ai_step with a minimum audience, book programs (ai_book_programs_for_contract) when booked < planned spots; otherwise/afterwards fill empty slots (ai_fill_program_slot) */
void ai_office_book_spots(void) {
    u16 ax;
    u16 bx;
    u16 di;
    u16 si;
    u32 l_4;
    u16 l_6;
    u16 l_8;
    u8 l_9;
    u8 l_A;
    u16 t10;
    u16 t11;
    u16 t15;
    u16 t16;
    FUNC_TICK(162);
    l_A = g_ai_player((u16)(i8)g_ai_cur);
    l_9 = 0;
    l_8 = g_player_ad_count((u16)(i8)l_A);
    if (((i16)g_minute) <= 5) {
        ax = clock_hour();
    } else {
        ax = clock_hour() + 1;
    }
    l_6 = ax;
    sched_mark_scheduled();
    ai_place_live_programs();
    if (((u8)((u8)(ai_contracts_all_booked()))) == 0) {
        if ((schedule_program((u16)(i8)l_A, l_6)) != 0) {
            if ((g_ai_cursor((u16)(i8)g_ai_cur)) == 0) {
                if (((i16)(g_ai_step((u16)(i8)g_ai_cur))) < ((i16)l_8)) {
                    t10 = g_ai_step((u16)(i8)g_ai_cur);
                    t11 = contracts((u16)(i8)l_A, g_ai_step((u16)(i8)g_ai_cur)).ad;
                    l_4 = ai_ad_plan_at((u16)(i8)l_A, g_ai_step((u16)(i8)g_ai_cur));
                    si = t10;
                    di = t11;
                    if (t11 != 0) {
                        bx = (((u16)(i8)(ads(di).min_class)) - 1) << 2;
                        if (((i32)(G32(((((u16)(i8)(ads(di).min_class)) - 1) << 2) + 0xA8))) > 0) {
                            t15 = (((u8)((u8)(((u16)((u16)(i8)(AT(ai_ad, l_4).booked_tomorrow))) >> 8))) << 8) | ((u8)((AT(ai_ad, l_4).tomorrow) + (AT(ai_ad, l_4).today)));
                            t16 = ((u16)(i8)(AT(ai_ad, l_4).booked_today)) + ((u16)(i8)(AT(ai_ad, l_4).booked_tomorrow));
                            l_9 = (AT(ai_ad, l_4).tomorrow) + (AT(ai_ad, l_4).today);
                            if (((i16)t16) < ((i16)((u16)(i8)((u8)t15)))) {
                                ai_book_programs_for_contract(si, (u16)(i8)l_9);
                            }
                        }
                    }
                }
                if (((i16)((g_ai_step((u16)(i8)g_ai_cur)) + 1)) >= ((i16)l_8)) {
                    g_ai_step((u16)(i8)g_ai_cur) = 0;
                    ai_next_goal();
                } else {
                    g_ai_step((u16)(i8)g_ai_cur) = (g_ai_step((u16)(i8)g_ai_cur)) + 1;
                }
            } else {
L_1620:
                ai_fill_program_slot();
            }
        } else {
            goto L_1620;
        }
    } else {
        goto L_1620;
    }
    return;
}

/* 2DD9:162A  (contract idx, spots) for each library category whose last program suits the ad's genre rule, place unscheduled programs (no live, no X-rated before 21h) into empty gaps whose estimated viewers reach the ad's minimum and book the ad after each of their hours, until spots are covered; sets g_ai_no_program if nothing was found. ?series category check adds (char)1000 = -24 to the id (original bug) */
void ai_book_programs_for_contract(u16 a0, u16 a1) {
    u16 ax;
    u16 bx;
    int cc;
    u16 di;
    u16 si;
    u32 t21;
    u16 t25;
    u8 fr_[30] = {0};                  /* bytes bp-30 .. bp-1 of the original frame */
    FUNC_TICK(60);
    FR8(fr_, 30, 0x1A) = 0;
    FR8(fr_, 30, 0x19) = g_ai_player((u16)(i8)g_ai_cur);
    FR8(fr_, 30, 0x18) = 0;
    FR8(fr_, 30, 0x17) = 0;
    FR8(fr_, 30, 0x16) = 0;
    FR8(fr_, 30, 0x5) = 0;
    FR16(fr_, 30, 0x2) = contracts((u16)(i8)FR8(fr_, 30, 0x19), a0).ad;
    FR8(fr_, 30, 0x15) = (u8)a1;
    si = 0;
    di = a0;
    if (FR16(fr_, 30, 0x2) != 0) {
        FR8(fr_, 30, 0x1C) = 1;
        while (((i8)FR8(fr_, 30, 0x1C)) <= 11) {
            LOOP_TICK(302);
            FR16(fr_, 30, 0x4) = (u16)(i8)(library_count((u16)(i8)FR8(fr_, 30, 0x19), (u16)(i8)FR8(fr_, 30, 0x1C)));
            if (FR8(fr_, 30, 0x1C) == 0xB) {
                FR8(fr_, 30, 0x16) = 0xE8;
            }
            /* ORIGINAL BUG (2DD9:16B7 mov byte [bp-22],0E8h): a series is id+1000; the
             * original adds the char constant (char)1000 = -24 instead. */
            if (((u8)((u8)(ad_genre_allowed((G16((((((u16)(i8)FR8(fr_, 30, 0x19)) * 0x6C) + 0x6F19) + (((u16)(i8)FR8(fr_, 30, 0x1C)) << 3)) + ((FR16(fr_, 30, 0x4) - 1) << 1))) + ((fix_on(FIX_AI_SERIES_RULE) && FR8(fr_, 30, 0x1C) == 0xB) ? 1000 : (u16)(i8)FR8(fr_, 30, 0x16)), FR16(fr_, 30, 0x2), 1)))) == 1) {
                FR8(fr_, 30, 0x1B) = 0;
                while (((i16)((u16)(i8)FR8(fr_, 30, 0x1B))) < ((i16)FR16(fr_, 30, 0x4))) {
                    LOOP_TICK(248);
                    si = library_items((u16)(i8)FR8(fr_, 30, 0x19), (u16)(i8)FR8(fr_, 30, 0x1C), (u16)(i8)FR8(fr_, 30, 0x1B));
                    if ((library_items((u16)(i8)FR8(fr_, 30, 0x19), (u16)(i8)FR8(fr_, 30, 0x1C), (u16)(i8)FR8(fr_, 30, 0x1B))) != 0) {
                        if (FR8(fr_, 30, 0x1C) != 0xB) {
                            FR8(fr_, 30, 0x18) = (u8)(((i16)((films(si).flags2_16) << 0xE)) >> 0xF);
                            FR8(fr_, 30, 0x17) = films(si).blocks;
                        } else {
                            FR8(fr_, 30, 0x18) = (u8)(((i16)((series(si).flags2_16) << 0xE)) >> 0xF);
                            FR8(fr_, 30, 0x17) = series(si).blocks;
                            si = si + 0x3E8;
                        }
                        cc = ((u16)(((i16)((G16((((((u16)(i8)FR8(fr_, 30, 0x19)) * 0x4D) + 0x4EDF) + (di * 7)) + 6)) << 0xD)) >> 0xF)) == 0;
                        if (cc && FR8(fr_, 30, 0x18) == 0) {
                            FR8(fr_, 30, 0x5) = (u8)ai_find_schedule_gaps((fr_ + 30 - 0x14), (u16)(i8)FR8(fr_, 30, 0x17));
                            if (FR8(fr_, 30, 0x5) != 0) {
                                FR8(fr_, 30, 0x1A) = 0;
                                while (((i16)((u16)(i8)FR8(fr_, 30, 0x1A))) <= ((i16)(((u16)(i8)FR8(fr_, 30, 0x5)) - 1))) {
                                    LOOP_TICK(85);
                                    t21 = ai_estimate_audience((u16)(i8)(FRX8(fr_, 30, 0x14, ((u16)(i8)FR8(fr_, 30, 0x1A)) + 0)), si, 1);
                                    t25 = G16(((((u16)(i8)(ads(contracts((u16)(i8)FR8(fr_, 30, 0x19), di).ad).min_class)) - 1) << 2) + 0xAA);
                                    cc = ((i16)HI16(t21)) >= ((i16)(G16(((((u16)(i8)(ads(contracts((u16)(i8)FR8(fr_, 30, 0x19), di).ad).min_class)) - 1) << 2) + 0xAA)));
                                    ax = LO16(t21);
                                    bx = (((u16)(i8)(ads(contracts((u16)(i8)FR8(fr_, 30, 0x19), di).ad).min_class)) - 1) << 2;
                                    if (cc && (HI16(t21) != t25 || ax >= G16(bx + 0xA8))) {
                                        if (((i16)si) >= 1000) {
                                            goto L_18BE;
                                        } else {
                                            cc = ((u16)(((i16)((films(si).flags2_16) << 0xC)) >> 0xF)) == 0;
                                            if (cc || ((i8)(FRX8(fr_, 30, 0x14, ((u16)(i8)FR8(fr_, 30, 0x1A)) + 0))) >= 4) {
                                                if ((films(si).genre) != GENRE_LIVE) {
                                                    goto L_18BE;
                                                } else {
                                                    FR8(fr_, 30, 0x1A) = FR8(fr_, 30, 0x1A) + 1;
                                                    continue;
                                                }
                                            } else {
                                                FR8(fr_, 30, 0x1A) = FR8(fr_, 30, 0x1A) + 1;
                                                continue;
                                            }
                                        }
                                    } else {
                                        FR8(fr_, 30, 0x1A) = FR8(fr_, 30, 0x1A) + 1;
                                        continue;
                                    }
                                }
L_1951:
                                FRX8(fr_, 30, 0x14, ((u16)(i8)FR8(fr_, 30, 0x1A)) + 0) = 0;
                                FR8(fr_, 30, 0x1B) = FR8(fr_, 30, 0x1B) + 1;
                                continue;
                            } else {
                                goto L_1951;
                            }
                        } else {
                            goto L_1951;
                        }
                    } else {
                        goto L_1951;
                    }
                }
                FR8(fr_, 30, 0x1C) = FR8(fr_, 30, 0x1C) + 1;
                continue;
            } else {
                FR8(fr_, 30, 0x1C) = FR8(fr_, 30, 0x1C) + 1;
                continue;
            }
        }
L_197B:
        if (si == 0) {
            g_ai_no_program = 1;
        }
    } else {
        goto L_197B;
    }
    FR8(fr_, 30, 0x15) = (u8)a1;
    return;
L_18BE:
    FR8(fr_, 30, 0x15) = FR8(fr_, 30, 0x15) - FR8(fr_, 30, 0x17);
    sched_place_program((u16)(i8)FR8(fr_, 30, 0x1B), (u16)(i8)FR8(fr_, 30, 0x1C), (u16)(i8)(FRX8(fr_, 30, 0x14, ((u16)(i8)FR8(fr_, 30, 0x1A)) + 0)), (u16)(i8)FR8(fr_, 30, 0x19));
    if (si != 0) {
        ai_book_contract_spots(di, (u16)(i8)(FRX8(fr_, 30, 0x14, ((u16)(i8)FR8(fr_, 30, 0x1A)) + 0)), (((u16)(i8)(FRX8(fr_, 30, 0x14, ((u16)(i8)FR8(fr_, 30, 0x1A)) + 0))) + ((u16)(i8)FR8(fr_, 30, 0x17))) - 1);
    }
    if (((i8)FR8(fr_, 30, 0x15)) > 0) {
        FRX8(fr_, 30, 0x14, ((u16)(i8)FR8(fr_, 30, 0x1A)) + 0) = 0;
        goto L_1951;
    }
    FR8(fr_, 30, 0x15) = (u8)a1;
    return;
}

/* 2DD9:1990  (contract idx, from hour, to hour) put the contract's ad after every scheduled hour in the range while the contract is not fully booked */
void ai_book_contract_spots(u16 a0, u16 a1, u16 a2) {
    u16 si;
    u8 l_1;
    u8 l_2;
    FUNC_TICK(19);
    l_1 = g_ai_player((u16)(i8)g_ai_cur);
    l_2 = (u8)a1;
    si = a0;
    while (((i8)l_2) <= ((i8)((u8)a2))) {
        LOOP_TICK(57);
        if ((schedule_program((u16)(i8)l_1, (u16)(i8)l_2)) != 0) {
            if (((u16)(((i16)((G16((((((u16)(i8)l_1) * 0x4D) + 0x4EDF) + (si * 7)) + 6)) << 0xD)) >> 0xF)) == 0) {
                sched_place_ad(si, (u16)(i8)l_2, (u16)(i8)l_1);
                ad_plan_book((u16)(i8)l_2, si, 1, (u16)(i8)l_1);
            }
        }
        l_2 = l_2 + 1;
    }
    ai_ad_requirements();
    return;
}

/* 2DD9:1A3F  build the AI shopping plan (up to g_ai_budget +0x2A entries): from the 10 dealer offers (then auction lots via ai_plan_auction_bid) pick the best score = blocks*topicality*20/(price/1000) with topicality >= player average quality, < 4 programs in that genre, not yet planned, price <= film budget; the budget is reserved immediately */
void ai_plan_film_purchases(void) {
    u16 ax;
    int cc;
    u16 di;
    u16 dx;
    u16 si;
    u16 t4;
    u32 t16;
    u32 t17;
    u32 t18;
    u8 fr_[44] = {0};                  /* bytes bp-44 .. bp-1 of the original frame */
    FUNC_TICK(35);
    FR8(fr_, 44, 0x29) = 1;
    FR8(fr_, 44, 0x28) = 0;
    FR8(fr_, 44, 0x27) = g_ai_player((u16)(i8)g_ai_cur);
    FR16(fr_, 44, 0x18) = 0;
    FR16(fr_, 44, 0x16) = g_avg_quality((u16)(i8)FR8(fr_, 44, 0x27));
    FR16(fr_, 44, 0xE) = 0;
    di = 1;
    while (((i16)di) <= 11) {
        LOOP_TICK(14);
        FRX8(fr_, 44, 0x24, 0 + di) = library_count((u16)(i8)FR8(fr_, 44, 0x27), di);
        di = di + 1;
    }
    while (!(((i16)((u16)(i8)(ai((u16)(i8)FR8(fr_, 44, 0x27)).max_deals))) <= ((i16)FR16(fr_, 44, 0x18)) || FR8(fr_, 44, 0x28) != 0)) {
        LOOP_TICK(307);
        t4 = ((((u16)(i8)FR8(fr_, 44, 0x27)) * 0x4D) + 0x2512) + (FR16(fr_, 44, 0x18) * 0xB);
        FR16(fr_, 44, 0x2) = DSEG;
        FR16(fr_, 44, 0x4) = t4;
        FR16(fr_, 44, 0x10) = 0;
        di = 1;
        while (((i16)di) <= (i16)g_nfilmoffers) {   /* 10 in the original (--film-offers) */
            LOOP_TICK(189);
            FR16(fr_, 44, 0xE) = 0;
            FR8(fr_, 44, 0x29) = 1;
            if ((dealer(di).program) != 0) {
                cc = ((i16)(dealer(di).program)) >= 1000;
                si = dealer(di).program;
                if (cc) {
                    ax = 0xB;
                } else {
                    ax = (u16)(i8)(films(si).genre);
                }
                FR16(fr_, 44, 0x12) = ax;
                if (((i16)si) >= 1000) {
                    ax = series_avg_topicality(si);
                } else {
                    ax = (u16)(i8)(films(si).topicality);
                }
                FR16(fr_, 44, 0x14) = ax;
                if (((i16)si) >= 1000) {
                    t17 = series_price(si + 0xFC18);
                    ax = LO16(t17);
                    dx = HI16(t17);
                } else {
                    t16 = movie_price(si);
                    ax = LO16(t16);
                    dx = HI16(t16);
                }
                FR16(fr_, 44, 0xA) = dx;
                FR16(fr_, 44, 0xC) = ax;
                if (((i16)si) >= 1000) {
                    ax = ((u16)(i8)series_episodes(si + 0xFC18)) * ((u16)(i8)series_blocks(si + 0xFC18));
                } else {
                    ax = (((u8)((u8)(((u16)(si * 0x17)) >> 8))) << 8) | (films(si).blocks);
                }
                FR8(fr_, 44, 0x26) = (u8)ax;
                t18 = (u32)((i32)FR32(fr_, 44, 0xC) / (i32)0x3E8);
                FR16(fr_, 44, 0x6) = HI16(t18);
                FR16(fr_, 44, 0x8) = LO16(t18);
                if ((FR16(fr_, 44, 0x8) | FR16(fr_, 44, 0x6)) == 0) {
                    FR16(fr_, 44, 0x6) = 0;
                    FR16(fr_, 44, 0x8) = 1;
                }
                FR16(fr_, 44, 0xE) = LO16(((u32)((i32)MK32(((i16)((((u16)(i8)FR8(fr_, 44, 0x26)) * FR16(fr_, 44, 0x14)) * 0x14)) >> 0xF, (((u16)(i8)FR8(fr_, 44, 0x26)) * FR16(fr_, 44, 0x14)) * 0x14) / (i32)FR32(fr_, 44, 0x8))));
                if (((i16)FR16(fr_, 44, 0x14)) < ((i16)FR16(fr_, 44, 0x16)) || ((i8)(FRX8(fr_, 44, 0x24, 0 + FR16(fr_, 44, 0x12)))) > 3) {
                    FR8(fr_, 44, 0x29) = 0;
                }
                if (((i16)FR16(fr_, 44, 0x18)) > 0) {
                    FR8(fr_, 44, 0x25) = 0;
                    while (((i16)((u16)(i8)FR8(fr_, 44, 0x25))) < ((i16)FR16(fr_, 44, 0x18))) {
                        LOOP_TICK(22);
                        if ((ai_buy_plan((u16)(i8)FR8(fr_, 44, 0x27), (u16)(i8)FR8(fr_, 44, 0x25)).program) == si) {
                            FR8(fr_, 44, 0x29) = 0;
                        }
                        FR8(fr_, 44, 0x25) = FR8(fr_, 44, 0x25) + 1;
                    }
                }
                if (!(FR8(fr_, 44, 0x29) != 1 || ((i16)(FP16(FR32(fr_, 44, 0x4), 2))) >= ((i16)FR16(fr_, 44, 0xE)))) {
                    ax = aibudget_films_lo((u16)(i8)FR8(fr_, 44, 0x27));
                    if (((i32)(ai((u16)(i8)FR8(fr_, 44, 0x27)).films)) >= ((i32)FR32(fr_, 44, 0xC))) {
                        FP16(FR32(fr_, 44, 0x4), 4) = si;
                        FP16(FR32(fr_, 44, 0x4), 0) = di;
                        MEM32((u16)(FPSEG(FR32(fr_, 44, 0x4))), (u16)(FPOFF(FR32(fr_, 44, 0x4))) + 6) = FR32(fr_, 44, 0xC);
                        FP16(FR32(fr_, 44, 0x4), 2) = FR16(fr_, 44, 0xE);
                        FP8(FR32(fr_, 44, 0x4), 0xA) = 0;
                        FR16(fr_, 44, 0x10) = FR16(fr_, 44, 0x12);
                    }
                }
            }
            di = di + 1;
        }
        ai_plan_auction_bid(FR16(fr_, 44, 0x18));
        if ((FP16(FR32(fr_, 44, 0x4), 4)) == 0 || !(((i16)FR16(fr_, 44, 0x18)) <= 0 || (G16((((((u16)(i8)FR8(fr_, 44, 0x27)) * 0x4D) + 0x2512) + ((FR16(fr_, 44, 0x18) - 1) * 0xB)) + 4)) != (FP16(FR32(fr_, 44, 0x4), 4)))) {
            FR8(fr_, 44, 0x28) = 1;
            g_ai_buy_last((u16)(i8)FR8(fr_, 44, 0x27)) = FR16(fr_, 44, 0x18) - 1;
            continue;
        } else {
            if ((FP8(FR32(fr_, 44, 0x4), 0xA)) != 0) {
                G32((u16)((((u16)(i8)FR8(fr_, 44, 0x27)) * 0x3C) + 0x6E29) + 8) = (ai((u16)(i8)FR8(fr_, 44, 0x27)).films) - (FP32(FR32(fr_, 44, 0x4), 6));
            } else {
                G32((u16)((((u16)(i8)FR8(fr_, 44, 0x27)) * 0x3C) + 0x6E29) + 8) = (ai((u16)(i8)FR8(fr_, 44, 0x27)).films) - (FP32(FR32(fr_, 44, 0x4), 6));
            }
            FRX8(fr_, 44, 0x24, 0 + FR16(fr_, 44, 0x10)) = (FRX8(fr_, 44, 0x24, 0 + FR16(fr_, 44, 0x10))) + 1;
            g_ai_buy_last((u16)(i8)FR8(fr_, 44, 0x27)) = FR16(fr_, 44, 0x18);
            FR16(fr_, 44, 0x18) = FR16(fr_, 44, 0x18) + 1;
            continue;
        }
    }
    return;
}

/* 2DD9:1DDA  carry out the shopping plan: dealer items are paid (cash), removed from the shelf and added to the library genre list (max 4); auction items get a bid (bid += increment, refunding the other AI if it was the high bidder) and are remembered in the AI bid list; plan cleared */
void ai_execute_purchases(void) {
    u16 di;
    u16 si;
    u32 l_4;
    u32 l_8;
    u16 l_A;
    u16 l_C;
    u16 l_E;
    u16 l_10;
    u16 t3;
    u32 t10;
    FUNC_TICK(40);
    l_E = 0;
    l_C = (u16)(i8)(g_ai_player((u16)(i8)g_ai_cur));
    l_A = (u16)(i8)(g_ai_player(1 - ((u16)(i8)g_ai_cur)));
    if (seat_humans) l_A = (u16)mp_third(l_C);     /* multiplayer: the other computer player -> the third station */
    l_10 = 0;
    while (((i16)g_ai_buy_last(l_C)) >= ((i16)l_10)) {
        LOOP_TICK(235);
        if (((i16)g_ai_buy_last(l_C)) < 0) break;
        l_8 = ai_buy_plan_at(l_C, l_10);
        if ((AT(ai_buy, l_8).program) != 0) {
            si = AT(ai_buy, l_8).slot;
            if ((AT(ai_buy, l_8).auction) != 0) {
                l_4 = auction_at(si);
                if (fix_on(FIX_AI_BIDS)) {
                    /* one entry a lot: a lot already in the list keeps its entry, the reserve
                     * added (10 lots, 11 places); no place: no bid, the reserve back */
                    u16 base = (u16)((l_C * 0x42) + 0x756A), n = g_ai_bid_count(l_C), k = 0;
                    while (k < n && G16(base + k * 6) != si) k++;
                    if (k < n) {
                        G32(base + k * 6 + 2) = G32(base + k * 6 + 2) + AT(ai_buy, l_8).price;
                    } else if (n < AI_BIDS_MAX) {
                        G32(base + n * 6 + 2) = AT(ai_buy, l_8).price;
                        G16(base + n * 6) = si;
                        g_ai_bid_count(l_C) = n + 1;
                    } else {
                        ai(l_C).films = (ai(l_C).films) + (AT(ai_buy, l_8).price);
                        goto L_planned;
                    }
                } else {
                    t3 = ((l_C * 0x42) + 0x756A) + (g_ai_bid_count(l_C) * 6);
                    G32(t3 + 2) = AT(ai_buy, l_8).price;
                    G16(((l_C * 0x42) + 0x756A) + (g_ai_bid_count(l_C) * 6)) = si;
                }
                if (((u16)((u16)(i8)(AT(lot, l_4).bidder))) == l_A) {
                    ai(l_A).films = (ai(l_A).films) + (AT(ai_buy, l_8).price);
                }
                AT(lot, l_4).bidder = (u8)l_C;
                t10 = auction_increment(si);
                AT(lot, l_4).bid = (AT(lot, l_4).bid) + t10;
                if (!fix_on(FIX_AI_BIDS)) g_ai_bid_count(l_C) = g_ai_bid_count(l_C) + 1;
            } else {
                dealer(si).program = 0;
                players(l_C).money = (players(l_C).money) - (AT(ai_buy, l_8).price);
                dealer(si).shelf = (dealer(si).shelf) - 0x64;
                if (((i16)(AT(ai_buy, l_8).program)) >= 1000) {
                    di = 0xB;
                } else {
                    di = (u16)(i8)(films(AT(ai_buy, l_8).program).genre);
                }
                if (((i16)(AT(ai_buy, l_8).program)) > 1000) {
                    AT(ai_buy, l_8).program = (AT(ai_buy, l_8).program) - 0x3E8;
                }
                if (((i8)library_count(l_C, di)) < 4) {
                    library_items(l_C, di, (u16)(i8)library_count(l_C, di)) = AT(ai_buy, l_8).program;
                    library_count(l_C, di) = library_count(l_C, di) + 1;
                    l_E = 1;
                }
            }
L_planned:
            AT(ai_buy, l_8).program = 0;
            AT(ai_buy, l_8).slot = 0;
            AT(ai_buy, l_8).price = 0;
            AT(ai_buy, l_8).score = 0;
            AT(ai_buy, l_8).auction = 0;
        }
        l_10 = l_10 + 1;
    }
    g_ai_buy_last(l_C) = 0;
    if (!(g_room != ROOM_LIBRARY || g_room_owner != l_C || l_E == 0)) {
        library_refresh();
    }
    return;
}

/* 2DD9:20BA  (plan idx) compare auction lots (not already led by this or the other AI) at bid+increment with the same score as ai_plan_film_purchases (genre must have < 3 programs) and put a better one into the plan entry as an auction item */
void ai_plan_auction_bid(u16 a0) {
    u16 ax;
    int cc;
    u16 di;
    u16 si;
    u32 t5;
    u32 t6;
    u32 t7;
    u16 t9;
    u8 fr_[40] = {0};                  /* bytes bp-40 .. bp-1 of the original frame */
    FUNC_TICK(41);
    FR8(fr_, 40, 0x25) = 1;
    FR8(fr_, 40, 0x24) = g_ai_player((u16)(i8)g_ai_cur);
    FR16(fr_, 40, 0x14) = g_avg_quality((u16)(i8)FR8(fr_, 40, 0x24));
    FR16(fr_, 40, 0xE) = 0;
    if (seat_humans) {                   /* multiplayer: the other computer player -> the third station */
        ax = (((u8)((u8)(ax >> 8))) << 8) | (u8)mp_third((u16)(i8)FR8(fr_, 40, 0x24));
    } else if (g_ai_cur == 0) {
        ax = (((u8)((u8)(ax >> 8))) << 8) | g_ai_player(1);
    } else {
        ax = (((u8)((u8)(ax >> 8))) << 8) | g_ai_player(0);
    }
    FR8(fr_, 40, 0x23) = (u8)ax;
    di = 1;
    while (((i16)di) <= 11) {
        LOOP_TICK(14);
        FRX8(fr_, 40, 0x20, 0 + di) = library_count((u16)(i8)FR8(fr_, 40, 0x24), di);
        di = di + 1;
    }
    di = 1;
    while (((i16)di) <= 10) {
        LOOP_TICK(220);
        FR16(fr_, 40, 0xE) = 0;
        FR8(fr_, 40, 0x25) = 1;
        cc = (auction(di).program) != 0;
        si = auction(di).program;
        if (cc) {
            if ((auction(di).bidder) != FR8(fr_, 40, 0x24)) {
                if (((i16)si) >= 1000) {
                    ax = 0xB;
                } else {
                    ax = (u16)(i8)(films(si).genre);
                }
                FR16(fr_, 40, 0x10) = ax;
                if (((i16)si) >= 1000) {
                    ax = series_avg_topicality(si);
                } else {
                    ax = (u16)(i8)(films(si).topicality);
                }
                FR16(fr_, 40, 0x12) = ax;
                t5 = auction_increment(di);
                t6 = t5 + (auction(di).bid);
                FR16(fr_, 40, 0xA) = HI16(t6);
                FR16(fr_, 40, 0xC) = LO16(t6);
                if (((i16)si) >= 1000) {
                    ax = ((u16)(i8)series_episodes(si + 0xFC18)) * ((u16)(i8)series_blocks(si + 0xFC18));
                } else {
                    ax = (((u8)((u8)(((u16)(si * 0x17)) >> 8))) << 8) | (films(si).blocks);
                }
                FR8(fr_, 40, 0x22) = (u8)ax;
                t7 = (u32)((i32)FR32(fr_, 40, 0xC) / (i32)0x3E8);
                FR16(fr_, 40, 0x6) = HI16(t7);
                FR16(fr_, 40, 0x8) = LO16(t7);
                if ((FR16(fr_, 40, 0x8) | FR16(fr_, 40, 0x6)) == 0) {
                    FR16(fr_, 40, 0x6) = 0;
                    FR16(fr_, 40, 0x8) = 1;
                }
                FR16(fr_, 40, 0xE) = LO16(((u32)((i32)MK32(((i16)((((u16)(i8)FR8(fr_, 40, 0x22)) * FR16(fr_, 40, 0x12)) * 0x14)) >> 0xF, (((u16)(i8)FR8(fr_, 40, 0x22)) * FR16(fr_, 40, 0x12)) * 0x14) / (i32)FR32(fr_, 40, 0x8))));
                if (((i16)FR16(fr_, 40, 0x12)) < ((i16)FR16(fr_, 40, 0x14)) || ((i8)(FRX8(fr_, 40, 0x20, 0 + FR16(fr_, 40, 0x10)))) >= 3) {
                    FR8(fr_, 40, 0x25) = 0;
                }
                if ((auction(di).bidder) == FR8(fr_, 40, 0x23)) {
                    FR8(fr_, 40, 0x25) = 0;
                }
                if (((i16)a0) > 0) {
                    FR8(fr_, 40, 0x21) = 0;
                    while (((i16)((u16)(i8)FR8(fr_, 40, 0x21))) < ((i16)a0)) {
                        LOOP_TICK(22);
                        if ((ai_buy_plan((u16)(i8)FR8(fr_, 40, 0x24), (u16)(i8)FR8(fr_, 40, 0x21)).program) == si) {
                            FR8(fr_, 40, 0x25) = 0;
                        }
                        FR8(fr_, 40, 0x21) = FR8(fr_, 40, 0x21) + 1;
                    }
                }
                if (FR8(fr_, 40, 0x25) == 1) {
                    t9 = ((((u16)(i8)FR8(fr_, 40, 0x24)) * 0x4D) + 0x2512) + (a0 * 0xB);
                    FR16(fr_, 40, 0x2) = DSEG;
                    FR16(fr_, 40, 0x4) = t9;
                    if (((i16)(FP16(FR32(fr_, 40, 0x4), 2))) < ((i16)FR16(fr_, 40, 0xE))) {
                        ax = aibudget_films_lo((u16)(i8)FR8(fr_, 40, 0x24));
                        if (((i32)(ai((u16)(i8)FR8(fr_, 40, 0x24)).films)) >= ((i32)FR32(fr_, 40, 0xC))) {
                            FP16(FR32(fr_, 40, 0x4), 4) = si;
                            FP16(FR32(fr_, 40, 0x4), 0) = di;
                            MEM32((u16)(FPSEG(FR32(fr_, 40, 0x4))), (u16)(FPOFF(FR32(fr_, 40, 0x4))) + 6) = FR32(fr_, 40, 0xC);
                            FP16(FR32(fr_, 40, 0x4), 2) = FR16(fr_, 40, 0xE);
                            FP8(FR32(fr_, 40, 0x4), 0xA) = 1;
                        }
                    }
                }
            }
        }
        di = di + 1;
    }
    return;
}

/* 2DD9:2386  if image >= 10: for each AI bid now led by the human: value = topicality*hours*20/(bid/1000) (series: episodes*blocks/2); if value > 2 and increment < film budget, outbid (bid += increment, budget charged), else give up (reserved money back to the film budget, bid removed) */
void ai_auction_rebid(void) {
    u16 ax;
    u16 bx;
    u16 di;
    u16 es;
    u16 si;
    u32 l_4;
    u16 l_6;
    u16 l_8;
    u16 l_A;
    u16 l_C;
    u16 l_E;
    u8 l_F;
    u32 t10;
    u16 t12;
    u32 t13;
    u32 t14;
    u32 t15;
    FUNC_TICK(33);
    l_F = g_ai_player((u16)(i8)g_ai_cur);
    if (((i16)(players((u16)(i8)l_F).image)) >= 10) {
        l_E = 0;
        while (((i16)(g_ai_bid_count((u16)(i8)l_F))) > ((i16)l_E)) {
            LOOP_TICK(328);
            l_C = G16(((((u16)(i8)l_F) * 0x42) + 0x756A) + (l_E * 6));
            l_4 = auction_at(l_C);
            if (mp_is_human((u16)(i8)(AT(lot, l_4).bidder))) {   /* == g_player (multiplayer: any human outbid it) */
                di = AT(lot, l_4).program;
                if (((i16)(AT(lot, l_4).program)) >= 1000) {
                    ax = series_avg_topicality(di);
                } else {
                    ax = (u16)(i8)(films(di).topicality);
                }
                l_6 = ax;
                if (((i16)di) >= 1000) {
                    ax = (u16)((u16)((i32)(i16)(((u16)(i8)series_episodes(di + 0xFC18)) * ((u16)(i8)series_blocks(di + 0xFC18))) / 2));
                } else {
                    ax = (u16)(i8)(films(di).blocks);
                }
                l_8 = ax;
                l_A = (u16)((u16)((i32)(i16)((l_6 * l_8) * 0x14) / (i16)LO16(((u32)((i32)AT(lot, l_4).bid / (i32)0x3E8)))));
                t10 = auction_increment(l_C);
                t12 = aibudget_films_hi((u16)(i8)l_F);
                ax = LO16(t10);
                bx = (((u16)(i8)l_F) * 0x3C) + 0x6E29;
                es = DSEG;
                if (((i16)HI16(t10)) <= ((i16)(aibudget_films_hi((u16)(i8)l_F))) && (((i16)HI16(t10)) < ((i16)t12) || ax < MEM16(es, bx + 8)) && ((i16)l_A) > 2) {
                    AT(lot, l_4).bidder = l_F;
                    t13 = auction_increment(l_C);
                    AT(lot, l_4).bid = (AT(lot, l_4).bid) + t13;
                    t14 = auction_increment(l_C);
                    ai((u16)(i8)l_F).films = (ai((u16)(i8)l_F).films) - t14;
                    t15 = auction_increment(l_C);
                    G32((((((u16)(i8)l_F) * 0x42) + 0x756A) + (l_E * 6)) + 2) = (G32((((((u16)(i8)l_F) * 0x42) + 0x756A) + (l_E * 6)) + 2)) + t15;
                } else {
                    ai((u16)(i8)l_F).films = (ai((u16)(i8)l_F).films) + (G32((((((u16)(i8)l_F) * 0x42) + 0x756A) + (l_E * 6)) + 2));
                    si = l_E;
                    while (((i16)((g_ai_bid_count((u16)(i8)l_F)) - 1)) > ((i16)si)) {
                        LOOP_TICK(71);
                        G16(((((u16)(i8)l_F) * 0x42) + 0x756A) + (si * 6)) = G16(((((u16)(i8)l_F) * 0x42) + 0x756A) + ((si + 1) * 6));
                        G32((((((u16)(i8)l_F) * 0x42) + 0x756A) + (si * 6)) + 2) = G32((((((u16)(i8)l_F) * 0x42) + 0x756A) + ((si + 1) * 6)) + 2);
                        si = si + 1;
                    }
                    g_ai_bid_count((u16)(i8)l_F) = (g_ai_bid_count((u16)(i8)l_F)) - 1;
                    G16(((((u16)(i8)l_F) * 0x42) + 0x756A) + ((g_ai_bid_count((u16)(i8)l_F)) * 6)) = 0;
                    G32((u16)(((((u16)(i8)l_F) * 0x42) + 0x756A) + ((g_ai_bid_count((u16)(i8)l_F)) * 6)) + 2) = 0;
                }
            }
            l_E = l_E + 1;
        }
    }
    return;
}

/* 2DD9:26EA  (player) daily ranking vs. the other AI and the human: image rank (+0x2B, also max deals = 2*rank), Betty love rank (gift priority), library quality/size need (film priority), reach share >28%/<20% (transmitter priority), rating rank (news/production priority, Sammy-dependent); pot percentages = base weights {5,28,14,18,35,0} * priority, normalised to 100 */
void ai_plan_budget(u16 a0) {
    u16 ax;
    u16 bx;
    u16 di;
    u16 si;
    u32 t12;
    u16 t23;
    u16 t28;
    u8 fr_[36] = {0};                  /* bytes bp-36 .. bp-1 of the original frame */
    FUNC_TICK(412);
    if (g_ai_cur == 0) {
        bx = 1;
    } else {
        bx = 0;
    }
    /* ORIGINAL BUG (fix "ai-rival"): the rival is the computer player that
     * is not g_ai_cur, but ai_new_day plans both with the same g_ai_cur: for
     * one of them the rival is itself (image rank never 1, reach twice) */
    if (fix_on(FIX_AI_RIVAL)) {
        bx = ((u16)(i8)g_ai_player(0)) == ((u16)(i8)((u8)a0)) ? 1 : 0;
    }
    FR8(fr_, 36, 0x22) = g_ai_player(bx);
    if (seat_humans) FR8(fr_, 36, 0x22) = (u8)mp_third(a0);   /* multiplayer: the rival is the third station */
    FR8(fr_, 36, 0x21) = G8(0x8BA8);
    FR8(fr_, 36, 0x20) = (u8)ai_library_avg_quality((u16)(i8)((u8)a0));
    FR8(fr_, 36, 0x1F) = (u8)ai_library_avg_quality((u16)(i8)FR8(fr_, 36, 0x22));
    FR8(fr_, 36, 0x1E) = (u8)ai_library_avg_quality((u16)(i8)FR8(fr_, 36, 0x21));
    FR8(fr_, 36, 0x1D) = (u8)ai_eval_yesterday(0, (u16)(i8)((u8)a0));
    FR8(fr_, 36, 0x1C) = (u8)ai_eval_yesterday(0, (u16)(i8)FR8(fr_, 36, 0x22));
    FR8(fr_, 36, 0x1B) = (u8)ai_eval_yesterday(0, (u16)(i8)FR8(fr_, 36, 0x21));
    FR8(fr_, 36, 0x1A) = 0;
    FR8(fr_, 36, 0x19) = 0;
    FR16(fr_, 36, 0x18) = 0;
    FR16(fr_, 36, 0x16) = 0;
    FR8(fr_, 36, 0xD) = 0;
    FR16(fr_, 36, 0x6) = DSEG;
    FR16(fr_, 36, 0x8) = (((u16)(i8)((u8)a0)) * 0x3C) + 0x6E29;
    FR16(fr_, 36, 0x2) = DSEG;
    FR16(fr_, 36, 0x4) = (((u16)(i8)((u8)a0)) * 0x17) + 0x8BDC;
    di = 0;
    if (((i16)(FP16(FR32(fr_, 36, 0x4), 0x15))) <= ((i16)(players((u16)(i8)FR8(fr_, 36, 0x22)).image)) || ((i16)(FP16(FR32(fr_, 36, 0x4), 0x15))) <= ((i16)(players((u16)(i8)FR8(fr_, 36, 0x21)).image))) {
        if (((i16)(FP16(FR32(fr_, 36, 0x4), 0x15))) > ((i16)(players((u16)(i8)FR8(fr_, 36, 0x22)).image)) || ((i16)(FP16(FR32(fr_, 36, 0x4), 0x15))) > ((i16)(players((u16)(i8)FR8(fr_, 36, 0x21)).image))) {
            FP8(FR32(fr_, 36, 0x8), 0x2B) = 2;
        } else {
            FP8(FR32(fr_, 36, 0x8), 0x2B) = 3;
        }
    } else {
        FP8(FR32(fr_, 36, 0x8), 0x2B) = 1;
    }
    FP8(FR32(fr_, 36, 0x8), 0x1C) = (u8)((3 - ((u16)(i8)(FP8(FR32(fr_, 36, 0x8), 0x2B)))) * 0xA);
    if (g_difficulty == 3) {
        FP8(FR32(fr_, 36, 0x8), 0x1C) = 0;
    }
    if (((i8)FR8(fr_, 36, 0x1D)) <= ((i8)FR8(fr_, 36, 0x1C)) || ((i8)FR8(fr_, 36, 0x1D)) <= ((i8)FR8(fr_, 36, 0x1B))) {
        if (((i8)FR8(fr_, 36, 0x1D)) > ((i8)FR8(fr_, 36, 0x1C)) || ((i8)FR8(fr_, 36, 0x1D)) > ((i8)FR8(fr_, 36, 0x1B))) {
            FR8(fr_, 36, 0xD) = 2;
        } else {
            FR8(fr_, 36, 0xD) = 3;
        }
    } else {
        FR8(fr_, 36, 0xD) = 1;
    }
    if (((i16)(FP16(FR32(fr_, 36, 0x4), 0xC))) < ((i16)(players((u16)(i8)FR8(fr_, 36, 0x22)).betty_love)) || ((i16)(FP16(FR32(fr_, 36, 0x4), 0xC))) < ((i16)(players((u16)(i8)FR8(fr_, 36, 0x21)).betty_love))) {
        if (((i16)(FP16(FR32(fr_, 36, 0x4), 0xC))) >= ((i16)(players((u16)(i8)FR8(fr_, 36, 0x22)).betty_love)) || ((i16)(FP16(FR32(fr_, 36, 0x4), 0xC))) >= ((i16)(players((u16)(i8)FR8(fr_, 36, 0x21)).betty_love))) {
            FP8(FR32(fr_, 36, 0x8), 0x24) = 2;
        } else {
            FP8(FR32(fr_, 36, 0x8), 0x24) = 3;
        }
    } else {
        FP8(FR32(fr_, 36, 0x8), 0x24) = 1;
    }
    if (!(((i8)FR8(fr_, 36, 0x20)) > ((i8)FR8(fr_, 36, 0x1F)) || ((i8)FR8(fr_, 36, 0x20)) > ((i8)FR8(fr_, 36, 0x1E)))) {
        FR8(fr_, 36, 0x1A) = 1;
    }
    if (((i8)((u8)(library_total_blocks((u16)(i8)((u8)a0), (u16)(i8)((u8)a0))))) < 21) {
        FR8(fr_, 36, 0x19) = 1;
    }
    FP8(FR32(fr_, 36, 0x8), 0x25) = (FR8(fr_, 36, 0x1A) + FR8(fr_, 36, 0x19)) + 1;
    t12 = ((tx((u16)(i8)((u8)a0), 0).reach) + (tx((u16)(i8)FR8(fr_, 36, 0x22), 0).reach)) + (tx((u16)(i8)FR8(fr_, 36, 0x21), 0).reach);
    FR16(fr_, 36, 0xA) = HI16(t12);
    FR16(fr_, 36, 0xC) = LO16(t12);
    FR16(fr_, 36, 0x16) = LO16(((u32)((i32)((u32)(tx((u16)(i8)((u8)a0), 0).reach * 0x64)) / (i32)MK32(FR16(fr_, 36, 0xA), FR16(fr_, 36, 0xC)))));
    if (((i16)FR16(fr_, 36, 0x16)) <= 28) {
        if (((i16)FR16(fr_, 36, 0x16)) >= 20) {
            FP8(FR32(fr_, 36, 0x8), 0x28) = 2;
        } else {
            FP8(FR32(fr_, 36, 0x8), 0x28) = 3;
        }
    } else {
        FP8(FR32(fr_, 36, 0x8), 0x28) = 1;
    }
    if (g_sammy_category(0) != 3) {
        if (((i8)FR8(fr_, 36, 0xD)) <= 1) {
            ax = (((u8)((u8)(ax >> 8))) << 8) | FR8(fr_, 36, 0xD);
        } else {
            ax = (((u8)((u8)(ax >> 8))) << 8) | ((u8)(FR8(fr_, 36, 0xD) - 1));
        }
        FP8(FR32(fr_, 36, 0x8), 0x27) = (u8)ax;
    } else if (((i16)(sammy_daily_update())) >= 2) {
        if (((i8)FR8(fr_, 36, 0xD)) <= 1) {
            ax = (((u8)((u8)(ax >> 8))) << 8) | FR8(fr_, 36, 0xD);
        } else {
            ax = (((u8)((u8)(ax >> 8))) << 8) | ((u8)(FR8(fr_, 36, 0xD) - 1));
        }
        FP8(FR32(fr_, 36, 0x8), 0x27) = (u8)ax;
    } else {
        FP8(FR32(fr_, 36, 0x8), 0x27) = FP8(FR32(fr_, 36, 0x8), 0x2B);
    }
    if (g_sammy_category(0) != 1) {
        if (((i8)FR8(fr_, 36, 0xD)) <= 1) {
            ax = (((u8)((u8)(ax >> 8))) << 8) | FR8(fr_, 36, 0xD);
        } else {
            ax = (((u8)((u8)(ax >> 8))) << 8) | ((u8)(FR8(fr_, 36, 0xD) - 1));
        }
        FP8(FR32(fr_, 36, 0x8), 0x26) = (u8)ax;
    } else if (((i16)(sammy_daily_update())) >= 2) {
        if (((i8)FR8(fr_, 36, 0xD)) <= 1) {
            ax = (((u8)((u8)(ax >> 8))) << 8) | FR8(fr_, 36, 0xD);
        } else {
            ax = (((u8)((u8)(ax >> 8))) << 8) | ((u8)(FR8(fr_, 36, 0xD) - 1));
        }
        FP8(FR32(fr_, 36, 0x8), 0x26) = (u8)ax;
    } else {
        FP8(FR32(fr_, 36, 0x8), 0x26) = FP8(FR32(fr_, 36, 0x8), 0x2B);
    }
    FP8(FR32(fr_, 36, 0x8), 0x29) = 1;
    FR16(fr_, 36, 0x18) = ((u16)(i8)(FP8(FR32(fr_, 36, 0x8), 0x1C))) + 0x64;
    si = 1;
    while (((i16)si) <= 6) {
        LOOP_TICK(28);
        t23 = (u16)((i32)(i16)(((u16)(i8)g_ai_pot_weights(si)) * 0x64) / (i16)FR16(fr_, 36, 0x18));
        FRX8(fr_, 36, 0x14, 0 + si) = (u8)t23;
        FP8(FR32(fr_, 36, 0x8), si + 0x1C) = (u8)(((u16)(i8)(FP8(FR32(fr_, 36, 0x8), si + 0x23))) * ((u16)(i8)(FRX8(fr_, 36, 0x14, 0 + si))));
        t28 = di + ((u16)(i8)(FP8(FR32(fr_, 36, 0x8), si + 0x1C)));
        si = (u16)(si + 1);
        di = t28;
    }
    si = 1;
    while (((i16)si) <= 6) {
        LOOP_TICK(14);
        FP8(FR32(fr_, 36, 0x8), si + 0x1C) = (u8)(u16)((u16)((i32)(i16)(((u16)(i8)(FP8(FR32(fr_, 36, 0x8), si + 0x1C))) * 0x64) / (i16)di));
        si = si + 1;
    }
    FP8(FR32(fr_, 36, 0x8), 0x2A) = (FP8(FR32(fr_, 36, 0x8), 0x2B)) << 1;
    return;
}

/* 2DD9:2B8D  (mode, player) mode 0: average rating % of yesterday's 7 hours = viewers*100/(reach*2464); mode 1: average quality of yesterday's 7 scheduled programs (film quality, series episode quality); day 1: 10 / 15 */
u16 ai_eval_yesterday(u16 a0, u16 a1) {
    u16 ax;
    u16 di;
    u16 si;
    u16 l_2;
    u16 l_4;
    u16 l_6;
    u16 l_8;
    FUNC_TICK(42);
    l_2 = 0;
    if (g_day != 1) {
        ax = (u16)(i8)((u8)a0);
        if (((u16)((u16)(i8)((u8)a0))) == 0) {
            l_8 = 1;
            while (((i16)l_8) <= 7) {
                LOOP_TICK(39);
                if (fix_on(FIX_AI_RATING)) {      /* the product in 64 bits (the original's 32 overflow above 21.5M viewers) */
                    u32 t = tx((u16)(i8)((u8)a1), 0).reach * 0x9A0;
                    l_2 = l_2 + LO16(t ? (u32)((u64)G32(((((u16)(i8)((u8)a1)) << 5) + 0x78BE) + (l_8 << 2)) * 0x64 / t) : 0);
                } else
                l_2 = l_2 + LO16(((u32)((i32)((u32)(G32(((((u16)(i8)((u8)a1)) << 5) + 0x78BE) + (l_8 << 2)) * 0x64)) / (i32)((u32)(tx((u16)(i8)((u8)a1), 0).reach * 0x9A0)))));
                l_8 = l_8 + 1;
            }
        } else if (ax == 1) {
            l_6 = 1;
            while (((i16)l_6) <= 7) {
                LOOP_TICK(82);
                if ((G16(((((u16)(i8)((u8)a1)) * 0x5A) + 0x3939) + (l_6 << 1))) != 0) {
                    si = l_6;
                    while ((G16(((((u16)(i8)((u8)a1)) * 0x5A) + 0x3939) + (si << 1))) == 0x270F) {
                        LOOP_TICK(16);
                        if (((i16)si) < 1) break;
                        si = si - 1;
                    }
                    di = G16(((((u16)(i8)((u8)a1)) * 0x5A) + 0x3939) + (si << 1));
                    if (((i16)(G16(((((u16)(i8)((u8)a1)) * 0x5A) + 0x3939) + (si << 1)))) >= 1000) {
                        l_4 = series_episode_at((u16)(i8)((u8)a1), di + 0xFC18, l_6);
                        l_2 = l_2 + ((u16)(i8)(FP8(g_series, ((di + 0xFC18) * 0x39) + (l_4 * 3))));
                    } else {
                        l_2 = l_2 + ((u16)(i8)(films(di).quality));
                    }
                }
                l_6 = l_6 + 1;
            }
        }
        l_2 = (u16)((u16)((i32)(i16)l_2 / 7));
    } else {
        if (((u8)((u8)a0)) == 0) {
            ax = 0xA;
        } else {
            ax = 0xF;
        }
        l_2 = ax;
    }
    ax = l_2;
    return ax;
}

/* 2DD9:2D26  (genre, idx) sell a library item to the dealer at its current value (cash and film budget += value), program_withdraw, owner = 0 */
void ai_sell_program(u16 a0, u16 a1) {
    u16 di;
    u8 l_1;
    u16 t5;
    u32 t6;
    u32 t7;
    u16 t14;
    u32 t15;
    u32 t16;
    FUNC_TICK(148);
    l_1 = g_ai_player((u16)(i8)g_ai_cur);
    di = a0;
    if (a0 == 0xB) {
        t5 = library_items((u16)(i8)l_1, di, a1);
        t6 = series_price(library_items((u16)(i8)l_1, di, a1));
        players((u16)(i8)l_1).money = (players((u16)(i8)l_1).money) + t6;
        t7 = series_price(t5);
        ai((u16)(i8)l_1).films = (ai((u16)(i8)l_1).films) + t7;
        program_withdraw(t5 + 0x3E8);
        series(t5).owner = 0;
    } else {
        t14 = library_items((u16)(i8)l_1, di, a1);
        t15 = movie_price(library_items((u16)(i8)l_1, di, a1));
        players((u16)(i8)l_1).money = (players((u16)(i8)l_1).money) + t15;
        t16 = movie_price(t14);
        ai((u16)(i8)l_1).films = (ai((u16)(i8)l_1).films) + t16;
        program_withdraw(t14);
        films(t14).owner = 0;
    }
    return;
}

/* 2DD9:2E72  sell unscheduled, priced films (not live/show) whose quality + 3 < the player's average quality (stops once 1-hour films are needed for odd gaps); with more than 3 series sell the unscheduled one with the lowest average topicality */
void ai_sell_weak_programs(void) {
    int cc;
    u16 di;
    u16 si;
    u32 l_4;
    u8 l_5;
    u8 l_6;
    u16 l_8;
    u16 l_A;
    u16 l_C;
    u16 l_E;
    FUNC_TICK(50);
    l_6 = g_ai_player((u16)(i8)g_ai_cur);
    l_5 = 1;
    l_4 = library_at((u16)(i8)l_6);
    l_E = 1;
    while (((i16)l_E) <= 10) {
        LOOP_TICK(89);
        if (l_E != 8 && l_E != 7) {
            di = 0;
            while (((i16)((u16)(i8)FP8(l_4, l_E + 0x60))) > ((i16)di)) {
                LOOP_TICK(80);
                cc = (FP16(l_4, (l_E << 3) + (di << 1))) != 0;
                si = FP16(l_4, (l_E << 3) + (di << 1));
                if (cc) {
                    cc = (films(si).blocks) != 1;
                    if (!(cc || !(((i8)(g_ai_odd_today((u16)(i8)l_6))) > 1 || ((i8)(g_ai_odd_today(((u16)(i8)l_6) + 4))) > 1))) {
                        l_5 = 0;
                    }
                    cc = ((u16)(((i16)((films(si).flags2_16) << 0xE)) >> 0xF)) != 0;
                    if (!(cc || l_5 != 1)) {
                        if (((films(si).price_lo) | (films(si).price_hi)) != 0) {
                            if (((i16)(((u16)(i8)(films(si).quality)) + 3)) < ((i16)(g_avg_quality((u16)(i8)l_6)))) {
                                ai_sell_program(l_E, di);
                            }
                        }
                    }
                }
                di = di + 1;
            }
        }
        l_E = l_E + 1;
    }
    if (((i8)FP8(l_4, 0x6B)) > 3) {
        l_A = 0x64;
        l_8 = 0xFFFF;
        l_E = 0;
        while (((i16)((u16)(i8)FP8(l_4, 0x6B))) > ((i16)l_E)) {
            LOOP_TICK(69);
            si = FP16(l_4, (l_E << 1) + 0x58);
            if (((u16)(((i16)((series(FP16(l_4, (l_E << 1) + 0x58)).flags2_16) << 0xE)) >> 0xF)) == 0) {
                l_C = 0;
                di = 1;
                while (((i16)((u16)(i8)(series(si).episodes))) >= ((i16)di)) {
                    LOOP_TICK(22);
                    l_C = l_C + ((u16)(i8)(FP8(g_series, ((si * 0x39) + (di * 3)) + 1)));
                    di = di + 1;
                }
                l_C = (u16)((u16)((i32)(i16)l_C / (i16)((u16)(i8)(series(si).episodes))));
                if (((i16)l_C) < ((i16)l_A)) {
                    l_A = l_C;
                    l_8 = l_E;
                }
            }
            l_E = l_E + 1;
        }
        if (((i16)l_8) >= 0) {
            ai_sell_program(0xB, l_8);
        }
    }
    return;
}

/* 2DD9:3085  (id, +1000 allowed) average episode topicality of a series; ?sums episodes 2..n+1 (off by one in the original) */
u16 series_avg_topicality(u16 a0) {
    u16 ax;
    u16 di;
    u16 si;
    u16 l_2;
    u16 l_4;
    FUNC_TICK(42);
    l_4 = 0;
    di = a0;
    if (((i16)a0) > 1000) {
        di = di - 0x3E8;
    }
    if ((series(di).episodes) == 0) {
        ax = 0;
    } else {
        si = 1;
        while (((i16)((u16)(i8)(series(di).episodes))) >= ((i16)si)) {
            LOOP_TICK(22);
            /* ORIGINAL BUG (2DD9:30B8): inc si before the read -> episodes 2..n+1 */
            l_4 = l_4 + ((u16)(i8)(FP8(g_series, ((di * 0x39) + ((fix_on(FIX_SERIES_AVG) ? si : si + 1) * 3)) + 1)));
            si = si + 1;
        }
        l_2 = (u16)((u16)((i32)(i16)l_4 / (i16)((u16)(i8)(series(di).episodes))));
        ax = l_2;
    }
    return ax;
}

/* 2DD9:311D  ad agency: up to g_ai_budget +0x2A times take the best-paying offer that ai_can_take_contract accepts and whose minimum audience <= reach*2464*avg rating/100; contract deadline = today + days, booking plan initialised, offer removed; agency refilled */
void ai_take_contracts(void) {
    u16 ax;
    int cc;
    u16 di;
    u16 si;
    u32 l_4;
    u32 l_8;
    u32 l_C;
    u16 l_E;
    u16 l_10;
    u16 l_12;
    u8 l_13;
    u8 l_14;
    u8 l_15;
    u16 t3;
    u16 t11;
    FUNC_TICK(48);
    l_15 = g_ai_player((u16)(i8)g_ai_cur);
    l_14 = 0;
    l_13 = 0;
    l_12 = g_player_ad_count((u16)(i8)l_15);
    l_10 = 0;
    l_E = 0;
    l_C = 0;
    l_8 = 0;
    l_4 = 0;
    adagency_refill();
    while (((i8)(ai((u16)(i8)l_15).max_deals)) > ((i8)l_13)) {
        LOOP_TICK(208);
        t3 = g_player_ad_count((u16)(i8)l_15);
        l_12 = g_player_ad_count((u16)(i8)l_15);
        ax = t3;
        di = 1;
        while (((i16)di) <= (i16)g_nadoffers) {   /* 10 in the original (--ad-offers) */
            LOOP_TICK(99);
            cc = (ad_offers(di).ad) != 0;
            ax = ad_offers(di).ad;
            si = ad_offers(di).ad;
            if (cc) {
                l_8 = G32(((((u16)(i8)(ads(si).min_class)) - 1) << 2) + 0xA8);
                l_4 = ((u32)(((u32)((i32)((u32)(tx((u16)(i8)l_15, 0).reach * MK32(((i16)(g_avg_rating((u16)(i8)l_15))) >> 0xF, g_avg_rating((u16)(i8)l_15)))) / (i32)0x64)) * 0x9A0));
                t11 = ai_can_take_contract(si);
                ax = t11;
                if (((u8)((u8)t11)) == 1) {
                    ax = LO16(l_8);
                    if (((i32)l_8) <= ((i32)l_4)) {
                        cc = ((i32)(ads(si).fee)) > ((i32)l_C);
                        ax = ad_fee_lo(si);
                        if (cc) {
                            l_10 = si;
                            l_E = di;
                            l_C = ads(l_10).fee;
                            ax = (u16)(ad_fee_lo(l_10));
                        }
                    }
                }
            } else {
                l_14 = 1;
            }
            di = di + 1;
        }
        if (l_10 != 0) {
            ads(l_10).holder = (u8)((((u8)((u8)(ax >> 8))) << 8) | l_15);
            contracts((u16)(i8)l_15, l_12).ad = l_10;
            contracts((u16)(i8)l_15, l_12).deadline = ((u16)(i8)(ads(l_10).days)) + g_day;
            ai_ad_plan_init(l_12);
            g_player_ad_count((u16)(i8)l_15) = (g_player_ad_count((u16)(i8)l_15)) + 1;
            ad_offers(l_E).ad = 0;
            ad_offers(l_E).shelf = (ad_offers(l_E).shelf) - 0x64;
            l_14 = 1;
        }
        l_10 = 0;
        l_E = 0;
        l_C = 0;
        l_8 = 0;
        l_4 = 0;
        l_13 = l_13 + 1;
    }
    if (l_14 == 1) {
        adagency_refill();
    }
    return;
}

/* 2DD9:33CB  (ad) 1 if today's planned spots + new spots/day < 6, next hour + all outstanding spots <= 14, fewer than 8 contracts, the ad has no required genre (only exclusions) and its minimum audience as % of reach <= the AI's image */
u16 ai_can_take_contract(u16 a0) {
    u16 ax;
    u16 di;
    u16 si;
    u32 l_4;
    u16 l_6;
    u16 l_8;
    u8 l_9;
    u8 l_A;
    u8 l_B;
    u16 t3;
    u16 t15;
    FUNC_TICK(103);
    l_B = 0;
    l_A = 0;
    l_9 = g_ai_player((u16)(i8)g_ai_cur);
    l_8 = 0;
    l_6 = clock_hour_round05();
    l_4 = ads_at(a0);
    si = 0;
    di = 0;
    while (((i16)(g_player_ad_count((u16)(i8)l_9))) > ((i16)si)) {
        LOOP_TICK(62);
        t3 = di + ((u16)(i8)(ai_ad_plan((u16)(i8)l_9, si).today));
        l_6 = l_6 + (((u16)(i8)(ads(contracts((u16)(i8)l_9, si).ad).spots)) - ((u16)(i8)(contracts((u16)(i8)l_9, si).aired)));
        si = si + 1;
        di = t3;
    }
    if ((AT(ad, l_4).days) == 0) {
        l_8 = 1;
    } else {
        l_8 = (u16)((u16)((i32)(i16)((u16)(i8)(AT(ad, l_4).spots)) / (i16)((u16)(i8)(AT(ad, l_4).days))));
    }
    if (((i16)(di + l_8)) >= 6 || ((i16)l_6) > 14) {
L_34F4:
        l_B = 0;
    } else if (((i16)(g_player_ad_count((u16)(i8)l_9))) >= 8) {
        goto L_34F4;
    } else {
        l_B = 1;
    }
    if (l_B == 1) {
        l_B = 0;
        si = 1;
        while (((i16)si) <= 3) {
            LOOP_TICK(8);
            if (((i8)FP8(l_4, si + 0xB)) <= 0) {
                l_A = l_A + 1;
            }
            si = si + 1;
        }
        if (l_A == 3) {
            l_B = 1;
        }
    }
    t15 = viewers_to_pct((u16)(i8)l_9, G32(((((u16)(i8)(AT(ad, l_4).min_class)) - 1) << 2) + 0xA8));
    ax = t15;
    if (((i16)t15) > ((i16)(players((u16)(i8)l_9).image))) {
        l_B = 0;
    }
    ax = (((u8)((u8)(ax >> 8))) << 8) | l_B;
    return ax;
}

/* 2DD9:3576  (player) total length in hour blocks of all library programs (films + series episode length); AI sells only when >= 14 */
u16 library_total_blocks(u16 a0, u16 ax) {
    u16 si;
    u8 l_1;
    u8 l_2;
    u8 l_3;
    u8 l_4;
    u16 t1;
    FUNC_TICK(15);
    l_2 = 0;
    l_1 = 0;
    l_4 = 1;
    while (((i8)l_4) <= 11) {
        LOOP_TICK(58);
        t1 = (((u8)((u8)(((u16)((u16)(i8)l_4)) >> 8))) << 8) | (library_count((u16)(i8)((u8)a0), (u16)(i8)l_4));
        l_1 = library_count((u16)(i8)((u8)a0), (u16)(i8)l_4);
        l_3 = 0;
        ax = t1;
        for (;;) {
            LOOP_TICK(40);
            ax = (((u8)((u8)(ax >> 8))) << 8) | l_3;
            if (((i8)l_3) >= ((i8)l_1)) break;
            si = library_items((u16)(i8)((u8)a0), (u16)(i8)l_4, (u16)(i8)l_3);
            if (((i8)l_4) > 10) {
                l_2 = l_2 + (series(si).blocks);
                ax = (u16)((((u8)((u8)(((u16)(si * 0x39)) >> 8))) << 8) | (series(si).blocks));
            } else {
                l_2 = l_2 + (films(si).blocks);
                ax = (u16)((((u8)((u8)(((u16)(si * 0x17)) >> 8))) << 8) | (films(si).blocks));
            }
            l_3 = l_3 + 1;
        }
        l_4 = l_4 + 1;
    }
    ax = (((u8)((u8)(ax >> 8))) << 8) | l_2;
    return ax;
}

/* 2DD9:362D  (genre) ?total blocks of the AI's programs of one genre (series: genre-10); result is lost, no callers */
void library_genre_blocks(u16 a0) {
    u16 ax;
    u16 si;
    u8 l_1;
    u32 l_6;
    u8 l_7;
    u8 l_8;
    FUNC_TICK(63);
    l_8 = 0;
    l_7 = g_ai_player((u16)(i8)g_ai_cur);
    l_6 = library_at((u16)(i8)l_7);
    if ((FP8(l_6, ((u16)(i8)((u8)a0)) + 0x60)) == 0) {
        ax = (((u8)((u8)(ax >> 8))) << 8) | 0;
    } else {
        ax = (((u8)((u8)(((u16)((u16)(i8)((u8)a0))) >> 8))) << 8) | ((u8)((FP8(l_6, ((u16)(i8)((u8)a0)) + 0x60)) - 1));
    }
    l_1 = (u8)ax;
    if (((i8)((u8)a0)) >= 11) {
        si = 0;
        while (((i16)((u16)(i8)FP8(l_6, 0x6B))) > ((i16)si)) {
            LOOP_TICK(35);
            if (((u16)((u16)(i8)(series(FP16(l_6, (si << 1) + 0x58)).genre))) == ((u16)(((u16)(i8)((u8)a0)) + 0xFFF6))) {
                l_8 = l_8 + (series(FP16(l_6, (si << 1) + 0x58)).blocks);
            }
            si = si + 1;
        }
    } else if ((FP16(l_6, (((u16)(i8)((u8)a0)) << 3) + (((u16)(i8)l_1) << 1))) == 0) {
        l_8 = 0;
    } else {
        si = 0;
        while (((i16)((u16)(i8)(FP8(l_6, ((u16)(i8)((u8)a0)) + 0x60)))) > ((i16)si)) {
            LOOP_TICK(26);
            l_8 = l_8 + (films(FP16(l_6, (((u16)(i8)((u8)a0)) << 3) + (si << 1))).blocks);
            si = si + 1;
        }
    }
    return;
}

/* 2DD9:3754  (contract idx, player) remaining spots / remaining days (deadline - today + 1, min 1); 0 when fully booked */
u16 contract_spots_per_day(u16 a0, u16 a1) {
    u16 ax;
    u16 di;
    u16 si;
    u32 l_4;
    u16 l_6;
    u8 l_7;
    u16 t6;
    FUNC_TICK(88);
    l_6 = contracts((u16)(i8)((u8)a1), a0).ad;
    l_4 = ai_ad_plan_at((u16)(i8)((u8)a1), a0);
    ax = ((i16)(FP16(l_4, 6) << 0xD)) >> 0xF;
    di = a0;
    if (((u16)(((i16)(FP16(l_4, 6) << 0xD)) >> 0xF)) != 0) {
        l_7 = 0;
    } else {
        si = ((contracts((u16)(i8)((u8)a1), di).deadline) - g_day) + 1;
        if (((u16)(((contracts((u16)(i8)((u8)a1), di).deadline) - g_day) + 1)) == 0) {
            si = 1;
        }
        t6 = (u16)((i32)(i16)(((u16)(i8)(ads(l_6).spots)) - ((u16)(i8)(AT(ai_ad, l_4).booked_total))) / (i16)si);
        l_7 = (u8)t6;
        ax = t6;
    }
    ax = (((u8)((u8)(ax >> 8))) << 8) | l_7;
    return ax;
}

/* 2DD9:381E  (hour or program, ad, is_program) 1 if the program's genre (series 10+genre) passes the ad's 3 genre bytes: a negative entry excludes |genre|, positive entries require one of them; 0 for no ad */
u16 ad_genre_allowed(u16 a0, u16 a1, u16 a2) {
    u16 ax;
    int cc;
    u16 di;
    u16 si;
    u32 l_4;
    u8 l_5;
    u8 l_6;
    u16 l_8;
    u16 t3;
    u32 t4;
    FUNC_TICK(89);
    l_6 = 1;
    l_5 = g_ai_player((u16)(i8)g_ai_cur);
    l_4 = ads_at(a1);
    if (((u8)((u8)a2)) == 0) {
        si = a0;
        while ((schedule_program((u16)(i8)l_5, si)) == 0x270F) {
            LOOP_TICK(16);
            if (((i16)si) <= 0) break;
            si = si - 1;
        }
        t3 = schedule_program((u16)(i8)l_5, si);
        l_8 = schedule_program((u16)(i8)l_5, si);
        ax = t3;
    } else {
        l_8 = a0;
        ax = a0;
    }
    if (a1 != 0) {
        if (l_8 == 0) {
            di = 0;
        } else if (((i16)l_8) >= 1000) {
            ax = (u16)(i8)series_genre(l_8 + 0xFC18);
            di = ((u16)(i8)series_genre(l_8 + 0xFC18)) + 0xA;
        } else {
            ax = (u16)(i8)(films(l_8).genre);
            di = (u16)(i8)(films(l_8).genre);
        }
        si = 1;
        while (((i16)si) <= 3) {
            LOOP_TICK(18);
            if (((i8)FP8(l_4, si + 0xB)) < 0) {
                t4 = rtl_abs((u16)(i8)FP8(l_4, si + 0xB));
                cc = LO16(t4) != di;
                ax = LO16(t4);
                if (!(cc)) {
                    l_6 = 0;
                }
            }
            si = si + 1;
        }
        if (l_6 != 0) {
            l_6 = 2;
            si = 1;
            while (((i16)si) <= 3) {
                LOOP_TICK(10);
                cc = ((i8)FP8(l_4, si + 0xB)) <= 0;
                if (!(cc || l_6 != 2)) {
                    l_6 = 0;
                }
                si = si + 1;
            }
            si = 1;
            while (((i16)si) <= 3) {
                LOOP_TICK(10);
                cc = ((u16)((u16)(i8)FP8(l_4, si + 0xB))) != di;
                ax = (u16)(i8)FP8(l_4, si + 0xB);
                if (!(cc)) {
                    l_6 = 1;
                }
                si = si + 1;
            }
            if (l_6 == 2) {
                l_6 = 1;
            }
        }
    } else {
        l_6 = 0;
    }
    ax = (((u8)((u8)(ax >> 8))) << 8) | l_6;
    return ax;
}

/* 2DD9:3985  1 when the AI figure stands at its target door, no other figure is there, the door is enterable and it waited 300 ticks (or was already flagged). If the human is caught in the AI's own room he pays g_catch_fine (doubles each time, max 200000) with message 0x12A; other occupants make the AI choose a new goal */
u16 ai_ready_at_target(void) {
    u16 ax;
    int cc;
    u16 di;
    u16 si;
    u32 rv_;
    u16 t1;
    u16 t11;
    u16 t16;
    u32 t23;
    u16 t25;
    u16 t27;
    u8 fr_[14] = {0};                  /* bytes bp-14 .. bp-1 of the original frame */
    FUNC_TICK(136);
    FR8(fr_, 14, 0xC) = 0;
    FR8(fr_, 14, 0xB) = g_ai_player((u16)(i8)g_ai_cur);
    t1 = ((((u16)(i8)FR8(fr_, 14, 0xB)) - 1) * 0x1E) + 0x7B42;
    FR16(fr_, 14, 0x6) = DSEG;
    FR16(fr_, 14, 0x8) = t1;
    game_clock_get((fr_ + 14 - 0xA));
    if (g_catch_busy != 0 || (FP16(FR32(fr_, 14, 0x8), 0x10)) != 0 || (FP16(FR32(fr_, 14, 0x8), 6)) != (FP16(FR32(fr_, 14, 0x8), 2))) {
L_39F5:
        ax = (u16)(i8)g_ai_cur;
        if ((g_ai_wait_start((u16)(i8)g_ai_cur)) != 0) {
L_3A07:
            si = 0;
            di = 1;
            while (((i16)si) <= 2) {
                LOOP_TICK(104);
                FR16(fr_, 14, 0x2) = DSEG;
                FR16(fr_, 14, 0x4) = (si * 0x1E) + 0x7B42;
                cc = ((u16)(((u16)(i8)FR8(fr_, 14, 0xB)) - 1)) != si;
                ax = ((u16)(i8)FR8(fr_, 14, 0xB)) - 1;
                if (cc && (FP16(FR32(fr_, 14, 0x4), 0x10)) == 0) {
                    ax = FP16(FR32(fr_, 14, 0x4), 0x12);
                    if ((FP16(FR32(fr_, 14, 0x4), 0x12)) == (FP16(FR32(fr_, 14, 0x8), 0x12))) {
                        ax = FP16(FR32(fr_, 14, 0x4), 6);
                        if ((FP16(FR32(fr_, 14, 0x4), 6)) == (FP16(FR32(fr_, 14, 0x8), 6))) {
                            if (((u16)(g_player - 1)) == si) {
                                if ((floors_door_owner(FP16(FR32(fr_, 14, 0x4), 6), FP16(FR32(fr_, 14, 0x4), 0x12))) == FR8(fr_, 14, 0xB)) {
                                    g_caught_figure = (u16)(i8)FR8(fr_, 14, 0xB);
                                    g_status_msg = 0x12A;
                                    g_status_amount = g_catch_fine;
                                    G32((u16)((g_player * 0x17) + 0x8BDC) + 8) = (players(g_player).money) - g_catch_fine;
                                    ledger_add(LEDGER_CREDIT_REPAID, g_catch_fine);
                                    ui_draw_money();
                                    t11 = g_catch_fine_lo << 1;
                                    g_catch_fine = MK32((g_catch_fine_hi << 1) | (u16)(g_catch_fine_lo >> 0xF), g_catch_fine_lo << 1);
                                    ax = t11;
                                    if (((i32)g_catch_fine) > 200000) {
                                        g_catch_fine = 0x30D40;
                                    }
                                    g_catch_busy = 1;
                                    while (g_room != ROOM_BUILDING) {
                                        LOOP_TICK(3);
                                        ax = room_escape();
                                    }
                                    g_catch_busy = 0;
                                } else {
                                    ax = ai_next_goal();
                                    di = 0;
                                }
                            } else if (seat_humans && mp_is_human(si + 1) &&
                                       (floors_door_owner(FP16(FR32(fr_, 14, 0x4), 6), FP16(FR32(fr_, 14, 0x4), 0x12))) == FR8(fr_, 14, 0xB)) {
                                /* multiplayer: another seat's human in this computer player's room:
                                 * caught -- it pays and is thrown out in its seat; the computer
                                 * player waits at the door meanwhile */
                                mp_catch_human(si + 1, (u16)(i8)FR8(fr_, 14, 0xB));
                                di = 0;
                            } else {
                                ax = ai_next_goal();
                                di = 0;
                            }
                        }
                    }
                }
                si = si + 1;
            }
            if (di == 1) {
                ai_stale_range = di;    /* saved by door_check_entry's prologue (ai_stale_range above) */
                t16 = door_check_entry(((u16)(i8)FR8(fr_, 14, 0xB)) - 1, 0);
                ax = t16;
                if (t16 != 0) {
                    ax = ai_next_goal();
                    di = 0;
                }
            }
            if (di == 1) {
                ax = (u16)(i8)g_ai_cur;
                if ((g_ai_arrived((u16)(i8)g_ai_cur)) != 0) {
                    FR8(fr_, 14, 0xC) = 1;
                } else {
                    if ((g_ai_wait_start((u16)(i8)g_ai_cur)) == 0) {
                        g_ai_wait_start((u16)(i8)g_ai_cur) = FR16(fr_, 14, 0xA);
                    }
                    t23 = rtl_abs(FR16(fr_, 14, 0xA) - (g_ai_wait_start((u16)(i8)g_ai_cur)));
                    ax = LO16(t23);
                    if (((i16)LO16(t23)) >= 300) {
                        FR8(fr_, 14, 0xC) = 1;
                        g_ai_wait_start((u16)(i8)g_ai_cur) = 0;
                        t25 = (u16)(i8)g_ai_cur;
                        g_ai_arrived((u16)(i8)g_ai_cur) = 1;
                        ax = t25;
                    }
                }
            }
        } else {
            t27 = (u16)(i8)g_ai_cur;
            g_ai_wait_start((u16)(i8)g_ai_cur) = 0;
            ax = t27;
        }
    } else {
        ax = FP16(FR32(fr_, 14, 0x8), 4);
        if ((FP16(FR32(fr_, 14, 0x8), 4)) == (FP16(FR32(fr_, 14, 0x8), 0))) {
            goto L_3A07;
        } else {
            goto L_39F5;
        }
    }
    ax = (((u8)((u8)(ax >> 8))) << 8) | FR8(fr_, 14, 0xC);
    rv_ = ax;
    return (u16)rv_;
}

/* 2DD9:3C0D  advance the AI state 0..12 and walk to its room: 0/4/8 own office, 1 ad agency, 2 movie agency, 3 own news room, 5/6 news room/office/library of player 1 / of the other AI (every 3rd time, if Betty love >= 20), 7 production errand (skipped without task), 8 skipped on Easy, 9 supermarket (gift budget >= 30000), 10 Betty (with gift), 11 info board (day event 1..3, no blocked door), 12 porter (10%) */
u16 ai_next_goal(void) {
    u16 ax;
    u16 bx;
    int cc;
    u16 es;
    u16 si;
    u32 l_4;
    u8 l_5;
    u8 l_6;
    u16 t20;
    u16 t41;
    u16 t50;
    FUNC_TICK(525);
    l_4 = MK32(DSEG, ((u16)(i8)g_ai_cur) + 0x24E5);
    si = (u16)(i8)(g_ai_player((u16)(i8)g_ai_cur));
    if (((i8)FP8(l_4, 0)) < 12) {
        FP8(l_4, 0) = FP8(l_4, 0) + 1;
        ai_production_status(si);
        if (FP8(l_4, 0) == 7) {
            if (((i8)(g_ai_studio((si * 3) + 2))) <= 0) {
                FP8(l_4, 0) = 8;
            }
        }
        if (!(FP8(l_4, 0) != 8 || g_difficulty != 1)) {
            FP8(l_4, 0) = 9;
        }
        if (FP8(l_4, 0) == 9) {
            cc = ((i32)(ai(si).gifts)) < 30000;
            bx = (si * 0x3C) + 0x6E29;
            es = DSEG;
            if (cc) {
                FP8(l_4, 0) = 0xA;
            }
        }
        if (!(FP8(l_4, 0) != 0xA || (g_ai_pending_gift((u16)(i8)g_ai_cur)) != 0)) {
            FP8(l_4, 0) = 0xB;
        }
        if (!(FP8(l_4, 0) != 0xB || !(g_npc_type == 0 || g_npc_type == 4 || g_npc_target_floor != 0))) {
            FP8(l_4, 0) = 0xC;
        }
        if (!(FP8(l_4, 0) != 0xC || (random_n(0xA, 1)) == 1)) {
            FP8(l_4, 0) = 0;
        }
    } else {
        FP8(l_4, 0) = 0;
    }
    g_ai_arrived((u16)(i8)g_ai_cur) = 0;
    ax = (u16)(i8)FP8(l_4, 0);
    if (((u16)((u16)(i8)FP8(l_4, 0))) <= 0xC) {
        switch (ax) {
        case 0:
            ai_walk_to(si, 1, si, 0);
            break;
        case 1:
            ai_walk_to(si, 0xB, 0, 0);
            break;
        case 2:
            ai_walk_to(si, 8, 0, 0);
            break;
        case 3:
            ai_walk_to(si, 2, si, 0);
            break;
        case 4:
            ai_walk_to(si, 1, si, 0);
            break;
        case 5:
            if ((u16)((u16)((i32)(i16)((u16)(i8)(g_ai_visit_p1(((u16)(i8)g_ai_cur) * 2))) % 3)) == 0) {
                if (((i16)(players(si).betty_love)) >= 20) {
                    l_5 = (u8)(u16)((u16)((i32)(i16)((u16)(i8)(g_ai_visit_p1(((u16)(i8)g_ai_cur) * 2))) / 3));
                    ax = (u16)(i8)l_5;
                    /* ORIGINAL BUG (fix "ai-visit"): the rooms of player 1, who is
                     * the human only on station 1; otherwise a computer player
                     * visits its own rooms or the other one's, never the human */
                    u16 victim = fix_on(FIX_AI_VISIT) ? g_player : 1;
                    if (((u16)((u16)(i8)l_5)) == 0) {
                        ai_walk_to(si, 2, victim, 0);
                    } else if (ax == 1) {
                        ai_walk_to(si, 1, victim, 0);
                    } else if (ax == 2) {
                        ai_walk_to(si, 4, victim, 0);
                    }
                }
            }
            t20 = (((u8)((u8)(DSEG >> 8))) << 8) | (g_ai_visit_p1(((u16)(i8)g_ai_cur) * 2));
            g_ai_visit_p1(((u16)(i8)g_ai_cur) * 2) = (g_ai_visit_p1(((u16)(i8)g_ai_cur) * 2)) + 1;
            if (((i8)((u8)t20)) <= 9) break;
            g_ai_visit_p1(((u16)(i8)g_ai_cur) * 2) = 0;
            break;
        case 6:
            if ((u16)((u16)((i32)(i16)((u16)(i8)(g_ai_visit_rival(((u16)(i8)g_ai_cur) * 2))) % 3)) == 0) {
                if (((i16)(players(si).betty_love)) >= 20) {
                    l_5 = (u8)(u16)((u16)((i32)(i16)((u16)(i8)(g_ai_visit_rival(((u16)(i8)g_ai_cur) * 2))) / 3));
                    if (g_ai_cur == 0) {
                        ax = (((u8)((u8)(ax >> 8))) << 8) | 1;
                    } else {
                        ax = (((u8)((u8)(ax >> 8))) << 8) | 0;
                    }
                    l_6 = (u8)ax;
                    ax = (u16)(i8)l_5;
                    u16 rival = seat_humans ? (u16)mp_third(si) : (u16)(i8)(g_ai_player((u16)(i8)l_6));   /* (multiplayer: the third station) */
                    if (((u16)((u16)(i8)l_5)) == 0) {
                        ai_walk_to(si, 2, rival, 0);
                    } else if (ax == 1) {
                        ai_walk_to(si, 1, rival, 0);
                    } else if (ax == 2) {
                        ai_walk_to(si, 4, (u16)(i8)(g_ai_player((u16)(i8)l_6)), 0);
                    }
                }
            }
            t41 = (((u8)((u8)(DSEG >> 8))) << 8) | (g_ai_visit_rival(((u16)(i8)g_ai_cur) * 2));
            g_ai_visit_rival(((u16)(i8)g_ai_cur) * 2) = (g_ai_visit_rival(((u16)(i8)g_ai_cur) * 2)) + 1;
            if (((i8)((u8)t41)) <= 9) break;
            g_ai_visit_rival(((u16)(i8)g_ai_cur) * 2) = 0;
            break;
        case 7:
            ai_production_status(si);
            cc = ((u16)(((u16)(i8)(g_ai_studio((si * 3) + 2))) - 1)) <= 5;
            ax = ((u16)(i8)(g_ai_studio((si * 3) + 2))) - 1;
            if (!(cc)) break;
            switch (ax) {
            case 0:
                ai_walk_to(si, 0xA, 0, 0);
                break;
            case 1:
                ai_walk_to(si, 9, 0, 0);
                break;
            case 2:
                ai_walk_to(si, 9, 0, 0);
                break;
            case 3:
                ai_walk_to(si, 1, si, 0);
                break;
            case 4:
                ai_walk_to(si, 0xC, 0, 0);
                break;
            case 5:
                ai_walk_to(si, 5, si, (u16)(i8)g_ai_studio(si * 3));
                break;
            default:
                cpu_halt("bad switch index %u in 2DD9:3C0D", ax);
            }
            break;
        case 8:
            ai_walk_to(si, 1, si, 0);
            break;
        case 9:
            ai_walk_to(si, 0xC, 0, 0);
            break;
        case 0xA:
            ai_walk_to(si, 0xE, 0, 0);
            break;
        case 0xB:
            ai_walk_to(si, 0xD, 0, 0);
            break;
        case 0xC:
            ai_walk_to(si, 0x40, 0, 0);
            break;
        default:
            cpu_halt("bad switch index %u in 2DD9:3C0D", ax);
        }
    }
    g_ai_step((u16)(i8)g_ai_cur) = 0;
    t50 = (u16)(i8)g_ai_cur;
    g_ai_cursor((u16)(i8)g_ai_cur) = 0;
    return t50;
}

/* 2DD9:4115  toggle g_ai_cur between the two computer players */
void ai_switch(void) {
    u16 ax;
    FUNC_TICK(7);
    if (g_ai_cur == 0) {
        ax = (((u8)((u8)(ax >> 8))) << 8) | 1;
    } else {
        ax = (((u8)((u8)(ax >> 8))) << 8) | 0;
    }
    g_ai_cur = (u8)ax;
    return;
}

/* 2DD9:4126  (player, room type, owner, studio) set the AI figure's target floor/door (x = 0x99 elevator when on another floor); sets the arrived flag if already there */
void ai_walk_to(u16 a0, u16 a1, u16 a2, u16 a3) {
    u16 ax;
    int cc;
    u16 di;
    u16 si;
    u8 fr_[10] = {0};                  /* bytes bp-10 .. bp-1 of the original frame */
    FUNC_TICK(84);
    FR16(fr_, 10, 0x2) = DSEG;
    FR16(fr_, 10, 0x4) = ((a0 - 1) * 0x1E) + 0x7B42;
    building_find_room(a1, a2, a3, (fr_ + 10 - 0x8), (fr_ + 10 - 0x6));
    di = a0;
    if ((FP16(FR32(fr_, 10, 0x4), 6)) != FR16(fr_, 10, 0x8) || (FP16(FR32(fr_, 10, 0x4), 0x12)) != FR16(fr_, 10, 0x6)) {
        FP16(FR32(fr_, 10, 0x4), 0x14) = FP16(FR32(fr_, 10, 0x4), 0x12);
        FP16(FR32(fr_, 10, 0x4), 6) = FR16(fr_, 10, 0x8);
        FP16(FR32(fr_, 10, 0x4), 0x12) = FR16(fr_, 10, 0x6);
        if ((FP16(FR32(fr_, 10, 0x4), 6)) != (FP16(FR32(fr_, 10, 0x4), 2))) {
            ax = 0x99;
        } else {
            ax = (floors_door_x(FR16(fr_, 10, 0x8), FR16(fr_, 10, 0x6))) + 8;
        }
        FP16(FR32(fr_, 10, 0x4), 4) = ax;
    } else {
        si = 0;
        /* ORIGINAL BUG (fix "ai-arrived"): "the walker is a computer player"
         * looks at AI index 0 only, so only the first one may act at once
         * when its next goal is the door it already stands at */
        if (fix_on(FIX_AI_ARRIVED)) {
            si = (u16)(i8)g_ai_cur;
        }
        cc = ((i16)si) <= 1;
        if (cc && ((u16)((u16)(i8)g_ai_player(si))) == di) {
            g_ai_arrived((u16)(i8)g_ai_cur) = 1;
        }
    }
    return;
}

/* 2DD9:41FF  (player) g_ai_studio[player]: status 5 studio production lacks shopping-list items (supermarket), 6 production ready to start (studio), else 1 screenplays but no studio (estate agent), 2/3 no screenplay (screenplay agency), 4 screenplay and studio (office) */
void ai_production_status(u16 a0) {
    u16 di;
    u16 si;
    u32 l_4;
    u32 l_8;
    u16 l_A;
    FUNC_TICK(80);
    l_4 = MK32(DSEG, (((u16)(i8)((u8)a0)) * 3) + 0x264A);
    FP8(l_4, 2) = 0xFF;
    l_A = 1;
    si = 0;
    di = 0;
    while (((i16)l_A) <= 12) {
        LOOP_TICK(46);
        l_8 = studios_at(l_A);
        if ((AT(studio, l_8).renter) == ((u8)((u8)a0))) {
            if ((AT(studio, l_8).script) != 0) {
                if ((script_items_bought(AT(studio, l_8).script, (u16)(i8)(AT(studio, l_8).episode))) != 1) {
                    FP8(l_4, 2) = 5;
                    FP8(l_4, 0) = (u8)l_A;
                } else if (!((AT(studio, l_8).end_day) != 0 || (AT(studio, l_8).running) != 0)) {
                    FP8(l_4, 2) = 6;
                    FP8(l_4, 0) = (u8)l_A;
                }
            } else {
                si = si + 1;
            }
        }
        l_A = l_A + 1;
    }
    if (FP8(l_4, 2) == 0xFF) {
        if ((g_scripts_owned_count((u16)(i8)((u8)a0))) != 0) {
            di = g_scripts_owned_count((u16)(i8)((u8)a0));
        }
        if (di == 0) {
L_42FB:
            if (((u32)MK32(di, si)) == 0) {
                FP8(l_4, 2) = 2;
            } else if (di != 0) {
L_432C:
                if (di != 0) {
                    if (si == 0) {
                        FP8(l_4, 2) = 1;
                        FP8(l_4, 1) = ((u8)di) - ((u8)si);
                    }
                }
            } else if (si == 0) {
                goto L_432C;
            } else {
                FP8(l_4, 2) = 3;
                FP8(l_4, 1) = ((u8)si) - ((u8)di);
            }
        } else if (si == 0) {
            goto L_42FB;
        } else {
            FP8(l_4, 2) = 4;
        }
    }
    return;
}

/* 2DD9:434F  (screenplay) base quality (+8 of episode block), averaged over all episodes for series screenplays */
u16 script_avg_quality(u16 a0) {
    u16 ax;
    u16 si;
    u32 l_4;
    u16 l_6;
    u8 l_7;
    u16 t2;
    u16 t10;
    FUNC_TICK(43);
    l_6 = 0;
    l_4 = screenplays_at(a0);
    if (((u16)(((i16)((AT(screenplay, l_4).flags_16) << 0xF)) >> 0xF)) != 0) {
        l_7 = 0;
        si = script_episode_count(a0);
        while (((i16)((u16)(i8)l_7)) < ((i16)si)) {
            LOOP_TICK(14);
            l_6 = l_6 + ((u16)(i8)(FP8(l_4, (((u16)(i8)l_7) * 0x23) + 8)));
            l_7 = l_7 + 1;
        }
        t10 = (u16)((i32)(i16)((u16)(i8)((u8)l_6)) / (i16)si);
        l_6 = t10;
        ax = t10;
    } else {
        t2 = (u16)(i8)(AT(screenplay, l_4).quality);
        l_6 = (u16)(i8)(AT(screenplay, l_4).quality);
        ax = t2;
    }
    ax = (((u8)((u8)(ax >> 8))) << 8) | ((u8)((u8)l_6));
    return ax;
}

/* 2DD9:43D6  (player) average script_avg_quality of the player's owned screenplays */
u16 scripts_owned_avg_quality(u16 a0) {
    u16 ax;
    u16 l_2;
    u8 l_3;
    u16 t7;
    FUNC_TICK(30);
    l_2 = 0;
    ax = (u16)(i8)((u8)a0);
    if ((g_scripts_owned_count((u16)(i8)((u8)a0))) == 0) {
        l_2 = 0;
    } else {
        l_3 = 0;
        while (((i16)(g_scripts_owned_count((u16)(i8)((u8)a0)))) > ((i16)((u16)(i8)l_3))) {
            LOOP_TICK(32);
            l_2 = l_2 + ((u16)(i8)((u8)script_avg_quality(owned_scripts((u16)(i8)((u8)a0), (u16)(i8)l_3).script)));
            l_3 = l_3 + 1;
        }
        t7 = (u16)((i32)(i16)l_2 / (i16)(g_scripts_owned_count((u16)(i8)((u8)a0))));
        l_2 = (u16)(i8)((u8)t7);
        ax = (u16)(i8)((u8)t7);
    }
    ax = (((u8)((u8)(ax >> 8))) << 8) | ((u8)((u8)l_2));
    return ax;
}

/* 2DD9:4460  reset both AIs: states -1, budget history, yesterday's avg rating/quality, odd-block counters, transmitter search, daily spot plans (spots per remaining day), ai_plan_budget, screenplay quality, news agencies (cat 1 if image >= 5, 2 if >= 10 and news budget > 20000, 3 if >= 15 and > 40000) */
void ai_new_day(void) {
    u16 si;
    u32 l_4;
    u32 l_8;
    u32 l_C;
    u32 l_10;
    u32 l_14;
    u8 l_15;
    u8 l_16;
    u8 l_17;
    u16 t1;
    u16 t2;
    u16 t3;
    u16 t6;
    u16 t8;
    u16 t14;
    u16 t15;
    u16 t17;
    FUNC_TICK(21);
    g_ai_state(0) = 0xFF;
    g_ai_state(1) = 0xFF;
    l_15 = 0;
    while (((i8)l_15) <= 1) {
        LOOP_TICK(247);
        if (g_ai_player((u16)(i8)l_15) == 0) { l_15 = l_15 + 1; continue; }   /* multiplayer: a human's slot */
        l_17 = g_ai_player((u16)(i8)l_15);
        l_C = ai_at((u16)(i8)l_17);
        l_8 = players_at((u16)(i8)l_17);
        t1 = (((u8)((u8)(((u16)((u16)(i8)l_15)) >> 8))) << 8) | (g_ai_period((u16)(i8)l_15));
        t2 = FP16(l_C, 0x12);
        t3 = FP16(l_C, 0x10);
        g_ai_period((u16)(i8)l_15) = (g_ai_period((u16)(i8)l_15)) + 1;
        FP32(l_C, (((u16)(i8)((u8)t1)) << 2) + 0x2C) = MK32(t2, t3);
        if (((i8)(g_ai_period((u16)(i8)l_15))) > 3) {
            g_ai_period((u16)(i8)l_15) = 1;
        }
        ai_avg_studio_budget((u16)(i8)l_17);
        t6 = ai_eval_yesterday(0, (u16)(i8)l_17);
        g_avg_rating((u16)(i8)l_17) = t6;
        t8 = ai_eval_yesterday(1, (u16)(i8)l_17);
        g_avg_quality((u16)(i8)l_17) = t8;
        g_ai_tx_busy((u16)(i8)l_17) = 0;
        g_ai_odd_today((u16)(i8)l_17) = (u8)((((u8)((u8)(((u16)((u16)(i8)l_17)) >> 8))) << 8) | (g_ai_odd_today(((u16)(i8)l_17) + 4)));
        g_ai_odd_today(((u16)(i8)l_17) + 4) = 0;
        l_14 = MK32(DSEG, (((u16)(i8)l_17) << 4) + 0x2443);
        FP32(l_14, 8) = 0;
        FP32(l_14, 0xC) = 0;
        FP16(l_14, 0) = 0;
        FP16(l_14, 2) = 0;
        FP16(l_14, 4) = 0;
        FP16(l_14, 6) = 0;
        l_16 = 0;
        while (((i16)(g_player_ad_count((u16)(i8)l_17))) > ((i16)((u16)(i8)l_16))) {
            LOOP_TICK(59);
            l_10 = ai_ad_plan_at((u16)(i8)l_17, (u16)(i8)l_16);
            AT(ai_ad, l_10).booked_today = AT(ai_ad, l_10).booked_tomorrow;
            AT(ai_ad, l_10).booked_tomorrow = 0;
            FP16(l_10, 6) = FP16(l_10, 6) & 0xFFFE;
            FP16(l_10, 6) = FP16(l_10, 6) & 0xFFFD;
            t14 = contract_spots_per_day((u16)(i8)l_16, (u16)(i8)l_17);
            AT(ai_ad, l_10).today = (u8)t14;
            t15 = contract_spots_per_day((u16)(i8)l_16, (u16)(i8)l_17);
            AT(ai_ad, l_10).tomorrow = (u8)t15;
            l_16 = l_16 + 1;
        }
        ai_plan_budget((u16)(i8)l_17);
        t17 = scripts_owned_avg_quality((u16)(i8)l_17);
        g_ai_script_quality((u16)(i8)l_17) = (u8)t17;
        si = 1;
        while (((i16)si) <= 3) {
            LOOP_TICK(6);
            FP8(l_8, si + 0x10) = 0;
            si = si + 1;
        }
        l_4 = AT(ai_budget, l_C).news;
        if (((i16)(AT(player, l_8).image)) >= 5) {
            AT(player, l_8).news_sub = 1;
        }
        if (((i16)(AT(player, l_8).image)) >= 10) {
            if (((i32)l_4) > 20000) {
                FP8(l_8, 0x12) = 1;
            }
        }
        if (((i16)(AT(player, l_8).image)) >= 15) {
            if (((i32)l_4) > 40000) {
                FP8(l_8, 0x13) = 1;
            }
        }
        l_15 = l_15 + 1;
    }
    ai_ad_requirements();
    return;
}

/* 2DD9:4719  put each unscheduled live film (genre 7) of the AI into its fixed slot when its day is today or tomorrow (clearing what is there) */
void ai_place_live_programs(void) {
    u16 di;
    u16 si;
    u32 l_4;
    u16 l_6;
    u16 l_8;
    u16 l_A;
    u8 l_B;
    u8 l_C;
    u16 t2;
    FUNC_TICK(36);
    l_C = g_ai_player((u16)(i8)g_ai_cur);
    l_B = 0;
    l_8 = (u16)(i8)(library_count((u16)(i8)l_C, 7));
    if (l_8 != 0) {
        l_A = 0;
        while (((i16)l_A) < ((i16)l_8)) {
            LOOP_TICK(95);
            t2 = library_items((u16)(i8)l_C, 7, l_A);
            l_6 = library_items((u16)(i8)l_C, 7, l_A);
            l_4 = films_at(t2);
            if (((u16)(((i16)((AT(film, l_4).flags2_16) << 0xE)) >> 0xF)) == 0) {
                di = 0;
                if ((AT(film, l_4).live_day) != g_day) {
                    if ((AT(film, l_4).live_day) == ((u16)(g_day + 1))) {
                        di = (AT(film, l_4).live_slot) + 7;
                    }
                } else {
                    di = AT(film, l_4).live_slot;
                }
                if (di != 0) {
                    if ((schedule_program((u16)(i8)l_C, di)) == 0x270F) {
                        si = di;
                        while ((schedule_program((u16)(i8)l_C, si)) == 0x270F) {
                            LOOP_TICK(14);
                            si = si - 1;
                        }
                        sched_clear_slot((u16)(i8)l_C, si);
                    }
                    sched_place_program(l_A, 7, di, (u16)(i8)l_C);
                    ai_stale_range = SEGV(0x2DD9);   /* return segment of this far call (ai_stale_range above) */
                    sched_mark_scheduled();
                }
            }
            l_A = l_A + 1;
        }
    }
    return;
}

/* 2DD9:4862  scan 3 map points per call (grid step 3, 150 x 90): best = largest new reach whose fee <= transmitter budget; when the scan ends, buy it in a free slot 3..32 (cash and budget charged, coverage stamped and recalculated), then ai_next_goal */
void ai_scan_transmitter_site(void) {
    u16 ax;
    u16 bx;
    int cc;
    u16 es;
    u16 si;
    u32 l_4;
    u32 l_8;
    u32 l_C;
    u32 l_10;
    u32 l_14;
    u32 l_18;
    u32 l_1C;
    u8 l_1D;
    u8 l_1E;
    u16 t2;
    u32 t6;
    u32 t17;
    FUNC_TICK(180);
    l_1E = g_ai_player((u16)(i8)g_ai_cur);
    if (ai_improve_scan((u16)(i8)l_1E)) return;   /* --ai-improve: a follow-up purchase (ai_improve.c) */
    l_1D = 0;
    l_14 = MK32(DSEG, (((u16)(i8)l_1E) << 4) + 0x2443);
    l_10 = l_14;
    l_C = MK32(FPSEG(l_14), FPOFF(l_14) + 2);
    l_8 = MK32(FPSEG(l_14), FPOFF(l_14) + 0xC);
    si = 0;
    for (;;) {
        LOOP_TICK(87);
        cc = ((i16)si) >= 3;
        if (cc || l_1D != 0) break;
        l_18 = transmitter_new_reach(FP16(l_10, 0), FP16(l_C, 0));
        t2 = FP16(l_8, 2);
        ax = FP16(l_8, 0);
        if (((i16)FP16(l_8, 2)) <= ((i16)HI16(l_18)) && (((i16)t2) < ((i16)HI16(l_18)) || ax < LO16(l_18))) {
            t6 = (u32)((i32)((u32)(transmitter_reach(FP16(l_10, 0), FP16(l_C, 0)) * 0x268)) / (i32)0xA);
            l_1C = transmitter_fee_bracket(LO16(t6), HI16(t6));
            if (((i32)l_1C) >= 50000) {
                ax = aibudget_transmitters_lo((u16)(i8)l_1E);
                if (((i32)(ai((u16)(i8)l_1E).transmitters)) >= ((i32)l_1C)) {
                    FP32(l_14, 8) = l_1C;
                    FP32(l_14, 0xC) = l_18;
                    FP16(l_14, 4) = FP16(l_10, 0);
                    FP16(l_14, 6) = FP16(l_C, 0);
                }
            }
        }
        FP16(l_10, 0) = FP16(l_10, 0) + 3;
        if (((i16)FP16(l_10, 0)) < 150) {
            si = si + 1;
            continue;
        } else {
            FP16(l_10, 0) = 0;
            FP16(l_C, 0) = FP16(l_C, 0) + 3;
            if (((i16)FP16(l_C, 0)) < 90) {
                si = si + 1;
                continue;
            } else {
                goto L_49C4;
            }
        }
    }
L_49D9:
    if (l_1D == 1) {
        g_ai_tx_busy((u16)(i8)l_1E) = 0;
        si = 3;
        while (((i16)si) <= 32) {
            LOOP_TICK(17);
            cc = ((i32)(tx((u16)(i8)l_1E, si).reach)) > 0;
            bx = ((((u16)(i8)l_1E) * 0x1CE) + 0x1D09) + (si * 0xE);
            es = DSEG;
            if (!(cc)) break;
            si = si + 1;
        }
        l_4 = tx_at((u16)(i8)l_1E, si);
        AT(transmitter, l_4).reach = FP32(l_14, 0xC);
        t17 = round_to(FP32(l_14, 8), 0x3E8);
        AT(transmitter, l_4).fee = t17;
        AT(transmitter, l_4).x = FP16(l_14, 4);
        AT(transmitter, l_4).y = FP16(l_14, 6);
        AT(transmitter, l_4).state = 1;
        players((u16)(i8)l_1E).money = (players((u16)(i8)l_1E).money) - (AT(transmitter, l_4).fee);
        coverage_stamp(si, (u16)(i8)l_1E, g_population_map, g_coverage_map);
        transmitters_recalc((u16)(i8)l_1E);
        FP32(l_14, 8) = 0;
        FP32(l_14, 0xC) = 0;
        FP16(l_14, 0) = 0;
        FP16(l_14, 2) = 0;
        FP16(l_14, 4) = 0;
        FP16(l_14, 6) = 0;
        ai((u16)(i8)l_1E).transmitters = (ai((u16)(i8)l_1E).transmitters) - (tx((u16)(i8)l_1E, si).fee);
        ai_next_goal();
    }
    return;
L_49C4:
    l_1D = 1;
    goto L_49D9;
}

/* 2DD9:4B84  1 if transmitter budget > 50000, time slot >= 18 and the other AI is not buying; 2 if the human is on the transmitter map (room 0x19); else 0 */
u16 ai_want_transmitter(void) {
    u16 ax;
    u16 bx;
    u16 es;
    u8 l_1;
    u8 l_2;
    u8 l_3;
    FUNC_TICK(48);
    l_3 = g_ai_player((u16)(i8)g_ai_cur);
    if (g_ai_cur == 0) {
        ax = (((u8)((u8)(ax >> 8))) << 8) | 1;
    } else {
        ax = (((u8)((u8)(ax >> 8))) << 8) | 0;
    }
    l_2 = (u8)ax;
    l_1 = 1;
    bx = (((u16)(i8)l_3) * 0x3C) + 0x6E29;
    es = DSEG;
    if (((i32)(ai((u16)(i8)l_3).transmitters)) > 50000) {
        if (((i16)g_time_slot) < 18) {
L_4BD6:
            l_1 = 0;
        }
    } else {
        goto L_4BD6;
    }
    if (ai_improve_wants((u16)(i8)l_3)) l_1 = 1;   /* --ai-improve: a follow-up purchase is due -- the pot and the hour aside */
    ax = (u16)(i8)(g_ai_player((u16)(i8)l_2));
    if (seat_humans) ax = (u16)mp_third((u16)(i8)l_3);   /* multiplayer: the third station */
    if ((g_ai_tx_busy(seat_humans ? ax : (u16)(i8)(g_ai_player((u16)(i8)l_2)))) != 0) {
        l_1 = 0;
    }
    if (g_room == ROOM_TX_MAP) {
        l_1 = 2;
    }
    ax = (((u8)((u8)(ax >> 8))) << 8) | l_1;
    return ax;
}

/* 2DD9:4C06  rebuild the AI's coverage map one transmitter per call (step 0 clears it; slots 1/2 copy the west/east half of the population map, others stamp a circle); at step 32 sum the west/east coverage */
void ai_rebuild_coverage_step(void) {
    u16 ax;
    u16 di;
    u16 si;
    u16 l_2;
    u8 l_3;
    u32 t10;
    u32 t11;
    FUNC_TICK(113);
    l_3 = g_ai_player((u16)(i8)g_ai_cur);
    l_2 = g_ai_cursor((u16)(i8)g_ai_cur);
    if (l_2 == 0) {
        mem_zero(g_coverage_map, 0x11E2);
    }
    if (((contracts(5, ((((u16)(i8)l_3) * 0x42) + (l_2 * 2)) + 9).ad) | (G16((((((u16)(i8)l_3) * 0x1CE) + 0x1D09) + (l_2 * 0xE)) + 6))) != 0 && (tx((u16)(i8)l_3, l_2).state) != 2) {
        ax = l_2;
        if (l_2 == 1) {
            di = 0;
            while (((i16)di) < 108) {
                LOOP_TICK(26);
                si = 0;
                while (((i16)si) < 21) {
                    LOOP_TICK(21);
                    FP16(g_coverage_map, ((di * 0x2A) + si) << 1) = FP16(g_population_map, ((di * 0x2A) + si) << 1);
                    si = si + 1;
                }
                di = di + 1;
            }
        } else if (ax == 2) {
            di = 0;
            while (((i16)di) < 108) {
                LOOP_TICK(26);
                si = 0x15;
                while (((i16)si) < 42) {
                    LOOP_TICK(21);
                    FP16(g_coverage_map, ((di * 0x2A) + si) << 1) = FP16(g_population_map, ((di * 0x2A) + si) << 1);
                    si = si + 1;
                }
                di = di + 1;
            }
        } else {
            coverage_stamp(l_2, (u16)(i8)l_3, g_population_map, g_coverage_map);
        }
    }
    if (l_2 == 0x20) {
        t10 = map_sum_population(0, 0, 0x54, 0x6D, g_coverage_map);
        g_coverage_west = t10;
        t11 = map_sum_population(0x54, 0, 0xA8, 0x6D, g_coverage_map);
        g_coverage_east = t11;
    }
    return;
}

/* 2DD9:4D9F  (player) g_ai_budget +0x2C = average of the 3 daily production budgets (+0x30..+0x38) */
void ai_avg_studio_budget(u16 a0) {
    u16 si;
    u16 l_2;
    u16 l_4;
    u16 t4;
    FUNC_TICK(33);
    l_2 = 0;
    l_4 = 0;
    si = 1;
    while (((i16)si) <= 3) {
        LOOP_TICK(19);
        t4 = l_4;
        l_4 = l_4 + (G16((((((u16)(i8)((u8)a0)) * 0x3C) + 0x6E29) + (si << 2)) + 0x2C));
        l_2 = (l_2 + G16((((((u16)(i8)((u8)a0)) * 0x3C) + 0x6E29) + (si << 2)) + 0x2E)) + ((u32)t4 + G16((((((u16)(i8)((u8)a0)) * 0x3C) + 0x6E29) + (si << 2)) + 0x2C) > 0xFFFF);
        si = si + 1;
    }
    ai((u16)(i8)((u8)a0)).studio_avg = ((u32)((i32)MK32(l_2, l_4) / (i32)3));
    return;
}
