/* Player actions: news slots, subscriptions, bids, schedule placement, hour tick, save and load.
 *
 * Original module 229E of MTV.EXE (Rainbow Arts, 1991).  Decompiled by
 * tools/decomp.py and hand-maintained since the freeze (git tag
 * generated-final).  Every function keeps its original address in the
 * comment above it; that address ties it to the machine-level twin in gen/
 * and to the difftest tables -- keep it when editing.
 */
#include <string.h>
#include "game.h"
#include "seat.h"
#include "mpsave.h"
#include "fixes.h"
#include "ai_improve.h"

/* 229E:0007  move the first item of a player's news inbox (g_news_inbox) into broadcast news slot a1 and remove it from the inbox */
void news_slot_take_first(u16 a0, u16 a1) {
    FUNC_TICK(32);
    far_memcpy(HPTR(news_pool_at(((a0 * 4) + a1) + 0x38)), HPTR(inbox_at(a0, 0)), 0x1E);
    news_inbox_drop_oldest(a0);
    return;
}

/* 229E:004A  toggle player a0's news-agency subscription flag for category a1 (player record +0x10+a1) */
void news_toggle_subscription(u16 a0, u16 a1) {
    u16 ax;
    FUNC_TICK(28);
    ax = a0 * 0x17;
    if ((G8((((a0 * 0x17) + 0x8BDC) + a1) + 0x10)) != 0) {
        ax = (((u8)((u8)(ax >> 8))) << 8) | 0;
    } else {
        ax = (((u8)((u8)(ax >> 8))) << 8) | 1;
    }
    G8((((a0 * 0x17) + 0x8BDC) + a1) + 0x10) = (u8)ax;
    return;
}

/* 229E:0088  click on auction slot a0: own high bid -> msg "No returns"; else if money >= bid+increment, refund previous AI bidder's film budget and mark bidder byte -100 (bid pending); clicking a pending bid undoes it; returns 1 if changed */
u16 auction_toggle_bid(u16 a0) {
    u16 ax;
    u32 l_4;
    u16 l_6;
    u32 t1;
    u32 t2;
    u16 t6;
    u16 t10;
    FUNC_TICK(111);
    l_6 = 0;
    l_4 = auction_at(a0);
    if (((i16)(AT(lot, l_4).program)) > 0) {
        if (((u16)((u16)(i8)(AT(lot, l_4).bidder))) != g_player) {
            if (((i8)(AT(lot, l_4).bidder)) < 0) {
L_0103:
                if (((i8)(AT(lot, l_4).bidder)) >= 0) {
                    if (((u16)((u16)(i8)(AT(lot, l_4).bidder))) != g_player) {
                        if (!(((i8)(AT(lot, l_4).bidder)) < 1 || ((i8)(AT(lot, l_4).bidder)) > 3)) {
                            t10 = (((u16)(i8)(AT(lot, l_4).bidder)) * 0x3C) + 0x6E29;
                            G32(t10 + 8) = (ai((u16)(i8)(AT(lot, l_4).bidder)).films) + (AT(lot, l_4).bid);
                        }
                        AT(lot, l_4).bidder = (AT(lot, l_4).bidder) - 0x64;
                    }
                } else {
                    AT(lot, l_4).bidder = (AT(lot, l_4).bidder) + 0x64;
                    if (!(((i8)(AT(lot, l_4).bidder)) < 1 || ((i8)(AT(lot, l_4).bidder)) > 3)) {
                        t6 = (((u16)(i8)(AT(lot, l_4).bidder)) * 0x3C) + 0x6E29;
                        G32(t6 + 8) = (ai((u16)(i8)(AT(lot, l_4).bidder)).films) - (AT(lot, l_4).bid);
                    }
                }
                l_6 = 1;
            } else {
                t1 = auction_increment(a0);
                t2 = t1 + (AT(lot, l_4).bid);
                if ((money_check_afford(g_player, t2)) == 0) goto L_0103;
            }
        } else {
            ui_warning(0x2A);
        }
    }
    ax = l_6;
    return ax;
}

/* 229E:01BB  put the ad of contract slot a0 (g_contracts[a2]) after hour slot a1 of player a2's schedule; 0 if contract slot empty */
u16 sched_place_ad(u16 a0, u16 a1, u16 a2) {
    u16 ax;
    FUNC_TICK(46);
    if ((contracts(a2, a0).ad) == 0) {
        ax = 0;
    } else {
        schedule_ad(a2, a1) = contracts(a2, a0).ad;
        ax = 1;
    }
    return ax;
}

/* sched_place_program's refusals: a message box for the human.  Fix "ai-warning": not
 * for a computer player (ai_place_live_programs tries a live film whose hour is over
 * at every office visit -- in the original each try opens another box on the human's
 * screen, never closed, until the popup's background save gets a null bitmap) */
static void sched_warning(u16 player, u16 text) {
    if (fix_on(FIX_AI_WARNING) && !mp_is_human((i16)player)) return;
    ui_warning(text);
}

/* 229E:0225  place library item (index a0, genre a1; 11 = series) at hour slot a2 for player a3. Rules: slot in the past -> "slot is over"; current hour after :05 -> "being shown"; live (genre 7) only on its day/slot; must end by slot 7/14 (1 AM); overlapping programs removed; continuation blocks = 9999; series stored as id+1000 */
u16 sched_place_program(u16 a0, u16 a1, u16 a2, u16 a3) {
    u16 ax;
    u16 l_2;
    u16 l_4;
    u32 l_8;
    u32 l_C;
    u16 l_E;
    u16 l_10;
    u16 l_12;
    u16 l_14;
    u16 l_16;
    u16 l_18;
    u16 t10;
    FUNC_TICK(162);
    l_8 = schedule_at(a3);
    l_4 = 0;
    if (FP16(l_8, a2 << 1) != 0x270F) {
        if (((i16)(clock_hour())) > ((i16)a2) || !((clock_hour()) != a2 || ((i16)g_minute) < 55)) {
            sched_warning(a3, 0x2D);
        } else if ((clock_hour()) != a2 || ((i16)g_minute) <= 5) {
            l_10 = 1;
            l_14 = library_items(a3, a1, a0);
            if (a1 != 0xB) {
                l_C = films_at(l_14);
                l_E = (u16)(i8)(AT(film, l_C).blocks);
                if ((AT(film, l_C).genre) == GENRE_LIVE) {
                    if (((i16)a2) > 7) {
                        ax = g_day + 1;
                    } else {
                        ax = g_day;
                    }
                    l_2 = ax;
                    t10 = AT(film, l_C).live_slot;
                    if (t10 != ((u16)((u16)((u16)((i32)(i16)(a2 - 1) % 7)) + 1)) || (AT(film, l_C).live_day) != l_2) {
                        l_10 = 0;
                    }
                }
            } else {
                l_E = (u16)(i8)(series(l_14).blocks);
            }
            if (((i16)a2) > 7) {
                l_12 = 7;
            } else {
                l_12 = 0;
            }
            if (((i16)((a2 + l_E) - 1)) <= ((i16)(l_12 + 7))) {
                if (l_10 == 1) {
                    l_18 = a2;
                    while (((i16)((a2 + l_E) - 1)) >= ((i16)l_18)) {
                        LOOP_TICK(39);
                        if (((i16)FP16(l_8, l_18 << 1)) > 0) {
                            if (((i16)FP16(l_8, l_18 << 1)) <= 2000) {
                                sched_clear_slot_2(a3, l_18);
                                l_16 = l_18 + 1;
                                while (FP16(l_8, l_16 << 1) == 0x270F) {
                                    LOOP_TICK(12);
                                    sched_clear_slot_2(a3, l_16);
                                    l_16 = l_16 + 1;
                                }
                            }
                        }
                        l_18 = l_18 + 1;
                    }
                    FP16(l_8, a2 << 1) = l_14;
                    FP16(l_8, (a2 << 1) + 0x1E) = 0;
                    if (a1 == 0xB) {
                        FP16(l_8, a2 << 1) = FP16(l_8, a2 << 1) + 0x3E8;
                    }
                    l_18 = a2 + 1;
                    while (((i16)((a2 + l_E) - 1)) >= ((i16)l_18)) {
                        LOOP_TICK(11);
                        FP16(l_8, l_18 << 1) = 0x270F;
                        l_18 = l_18 + 1;
                    }
                    l_4 = 1;
                }
            } else {
                sched_warning(a3, 0x31);
            }
        } else {
            sched_warning(a3, 0x2E);
        }
    }
    sched_mark_scheduled();
    ax = l_4;
    return ax;
}

/* 229E:0465  clear program word and +0x1E word of hour slot a1 for player a0 */
void sched_clear_slot_2(u16 a0, u16 a1) {
    FUNC_TICK(26);
    schedule_program(a0, a1) = 0;
    schedule_episode(a0, a1) = 0;
    return;
}

/* 229E:04A3  remove the whole multi-block program covering hour slot a1 (walks back/forward over 9999 continuation blocks) */
void sched_remove_program(u16 a0, u16 a1) {
    u16 l_2;
    u16 l_4;
    u16 l_6;
    FUNC_TICK(16);
    l_6 = a1;
    while (schedule_program(a0, l_6) == 0x270F) {
        LOOP_TICK(13);
        l_6 = l_6 - 1;
    }
    l_4 = l_6 + 1;
    while (schedule_program(a0, l_4) == 0x270F) {
        LOOP_TICK(13);
        l_4 = l_4 + 1;
    }
    l_2 = l_6;
    while (((i16)l_2) < ((i16)l_4)) {
        LOOP_TICK(10);
        sched_clear_slot_2(a0, l_2);
        l_2 = l_2 + 1;
    }
    return;
}

/* 229E:051F  can library item (index a0, genre a1) of the player be sold/given away? series: not in schedule, not in production; film: price != 0 and id != 58 ("not for sale"), not scheduled, not live, not in a studio production or own script list; shows the reason text; returns 1 if ok */
u16 library_check_can_sell(u16 a0, u16 a1) {
    u16 ax;
    u16 l_2;
    u16 l_4;
    u16 l_6;
    FUNC_TICK(122);
    l_4 = 1;
    l_6 = library_items(g_player, a1, a0);
    if (a1 == 0xB) {
        if (((u16)(((i16)((series(l_6).flags2_16) << 0xE)) >> 0xF)) != 0) {
            ui_warning(0x33);
            l_4 = 0;
        }
        if (l_4 == 1) {
            l_2 = 1;
            while (((i16)l_2) <= 12) {
                LOOP_TICK(36);
                if (!(((u16)(((i16)((screenplays(studios(l_2).script).flags_16) << 0xF)) >> 0xF)) == 0 || (screenplays(studios(l_2).script).target) != l_6)) {
                    l_4 = 0;
                }
                l_2 = l_2 + 1;
            }
            l_2 = 0;
            while (((i16)g_scripts_owned_count(g_player)) > ((i16)l_2)) {
                LOOP_TICK(49);
                if (!(((u16)(((i16)((screenplays(owned_scripts(g_player, l_2).script).flags_16) << 0xF)) >> 0xF)) == 0 || (screenplays(owned_scripts(g_player, l_2).script).target) != l_6)) {
                    l_4 = 0;
                }
                l_2 = l_2 + 1;
            }
            if (l_4 == 0) {
                ui_warning(0x35);
            }
        }
        l_6 = l_6 + 0x3E8;
    } else {
        if (((films(l_6).price_lo) | (films(l_6).price_hi)) == 0 || l_6 == 0x3A) {
            ui_warning(0x36);
            l_4 = 0;
        } else if (((u16)(((i16)((films(l_6).flags2_16) << 0xE)) >> 0xF)) == 0) {
            if ((films(l_6).genre) == GENRE_LIVE) {
                ui_warning(0x39);
                l_4 = 0;
            }
        } else {
            ui_warning(0x37);
            l_4 = 0;
        }
        if (l_4 == 1) {
            l_2 = 1;
            while (((i16)l_2) <= 12) {
                LOOP_TICK(36);
                if (!(((u16)(((i16)((screenplays(studios(l_2).script).flags_16) << 0xF)) >> 0xF)) != 0 || (screenplays(studios(l_2).script).target) != l_6)) {
                    l_4 = 0;
                }
                l_2 = l_2 + 1;
            }
            l_2 = 0;
            while (((i16)g_scripts_owned_count(g_player)) > ((i16)l_2)) {
                LOOP_TICK(49);
                if (!(((u16)(((i16)((screenplays(owned_scripts(g_player, l_2).script).flags_16) << 0xF)) >> 0xF)) != 0 || (screenplays(owned_scripts(g_player, l_2).script).target) != l_6)) {
                    l_4 = 0;
                }
                l_2 = l_2 + 1;
            }
            if (l_4 == 0) {
                ui_warning(0x35);
            }
        }
    }
    ax = l_4;
    return ax;
}

/* 229E:0828  per-frame clock check: on a new hour runs hourly updates (g_last_hour++); within the hour fires broadcast phases: :00 news (phase 1), :05 program (phase 2), :55 ad block (phase 3) via broadcast_phase_eval; returns 1 if something happened */
u16 time_hour_tick(void) {
    u16 ax;
    u16 l_2;
    u16 t1;
    u16 t2;
    u16 t3;
    FUNC_TICK(94);
    l_2 = 0;
    t1 = clock_hour();
    if (((i16)t1) > ((i16)g_last_hour)) {
        news_age(0x55, 0x55);
        news_tick(1, 0);
        if (g_room == ROOM_NEWS) {
            t2 = cursor_shape_push(3);
            t3 = txt_news_cache_free(t2);
            txt_news_cache_load(g_room_owner, t3);
            cursor_shape_push(0);
        }
        studio_check_finished();
        g_last_hour = g_last_hour + 1;
        l_2 = 1;
    }
    if (((i16)(clock_hour())) > 0) {
        if (!((((i16)g_minute) >= 55 || ((i8)g_broadcast_phase) < 3) && (((i16)g_minute) >= 5 || ((i8)g_broadcast_phase) < 2))) {
            g_broadcast_phase = 0;
        }
        if (((i16)g_minute) < 0 || ((i8)g_broadcast_phase) >= 1) {
            if (((i16)g_minute) < 5 || ((i8)g_broadcast_phase) >= 2) {
                if (!(((i16)g_minute) < 55 || ((i8)g_broadcast_phase) >= 3)) {
                    broadcast_phase_eval(3);
                    sched_mark_scheduled();
                    g_broadcast_phase = g_broadcast_phase + 1;
                    l_2 = 1;
                }
            } else {
                broadcast_phase_eval(2);
                ui_draw_viewers();
                sched_end_of_hour(g_player);
                g_broadcast_phase = g_broadcast_phase + 1;
                l_2 = 1;
            }
        } else {
            broadcast_phase_eval(1);
            ui_draw_viewers();
            g_broadcast_phase = g_broadcast_phase + 1;
            l_2 = 1;
        }
    }
    ax = l_2;
    return ax;
}

/* 229E:0948  (player, amount i32): returns 0 if player's available money >= amount, else shows "Not enough money!" and returns -1 (human: money minus pending transmitter costs) */
u16 money_check_afford(u16 a0, u32 a1) {
    u16 ax;
    u16 dx;
    u32 l_4;
    u32 t1;
    u32 t2;
    FUNC_TICK(41);
    if (a0 != g_player) {
        ax = pl_money_lo(a0);
        dx = pl_money_hi(a0);
    } else {
        t1 = money_available(a0);
        ax = LO16(t1);
        dx = HI16(t1);
    }
    l_4 = MK32(dx, ax);
    t2 = l_4 - a1;
    ax = LO16(t2);
    if (((i32)t2) >= 0) {
        ax = 0;
    } else {
        ui_warning(0x3B);
        ax = 0xFFFF;
    }
    return ax;
}

/* 229E:09AF  issue the next shopping list (episode) for the production in studio a0. Limits: max 5 lists per player ("too many shopping lists"), screenplay */
u16 studio_new_shopping_list(u16 a0) {
    u16 ax;
    u32 l_4;
    u32 l_8;
    u32 l_C;
    u16 l_E;
    u16 l_10;
    u16 l_12;
    u16 l_14;
    u16 l_16;
    u16 t4;
    FUNC_TICK(187);
    l_E = 0;
    l_C = studios_at(a0);
    l_16 = AT(studio, l_C).script;
    l_10 = (u16)(i8)(AT(studio, l_C).renter);
    l_8 = screenplays_at(l_16);
    l_4 = shoplists_at(l_10, g_shoplist_count(l_10));
    t4 = script_episode_count(l_16);
    if (((i16)t4) > ((i16)((u16)(i8)(AT(studio, l_C).episode_last))) || ((u16)(((i16)((AT(screenplay, l_8).flags_16) << 0xF)) >> 0xF)) == 0) {
        if (l_16 != 0x18 || (AT(studio, l_C).episode_last) != 1) {
            if (((i16)g_shoplist_count(l_10)) < 5) {
                if (((u16)(((i16)((AT(screenplay, l_8).flags_16) << 0xF)) >> 0xF)) != 0 || ((u16)(((i16)((AT(screenplay, l_8).flags_16) << 0xD)) >> 0xF)) == 0 || ((i8)(AT(studio, l_C).episode_last)) < 10) {
                    if (!(((u16)(((i16)((AT(screenplay, l_8).flags_16) << 0xF)) >> 0xF)) != 0 || ((u16)(((i16)((AT(screenplay, l_8).flags_16) << 0xD)) >> 0xF)) == 0)) {
                        films(AT(screenplay, l_8).target).flags2_16 = (films(AT(screenplay, l_8).target).flags2_16) | 4;
                    }
                    l_14 = ((u16)(i8)(AT(studio, l_C).episode_last)) + 1;
                    AT(shoplist, l_4).script = l_16;
                    AT(shoplist, l_4).episode = l_14;
                    g_shoplist_count(l_10) = g_shoplist_count(l_10) + 1;
                    l_12 = l_14;
                    if (((u16)(((i16)((AT(screenplay, l_8).flags_16) << 0xF)) >> 0xF)) != 0) {
                        ax = (((u8)((u8)(((u16)((AT(screenplay, l_8).target) * 0x39)) >> 8))) << 8) | (series(AT(screenplay, l_8).target).blocks);
                    } else {
                        ax = (((u8)((u8)(((u16)((AT(screenplay, l_8).target) * 0x17)) >> 8))) << 8) | (films(AT(screenplay, l_8).target).blocks);
                    }
                    FP8(l_8, ((l_12 - 1) * 0x23) + 0xA) = (u8)ax;
                    AT(studio, l_C).episode_last = (u8)l_14;
                    l_E = 1;
                } else {
                    ui_warning(0x14F);
                }
            } else {
                ui_warning(0x41);
            }
        } else {
            ui_warning(0x40);
        }
    }
    ax = l_E;
    return ax;
}

/* 229E:0B98  start shooting in studio a0. Rule: per cast slot k (10): actor roles score = actor_rating*weight/100, scene/equipment score = f(weight, equip quality table g_equip_quality); quality = base*sum(score)/sum(weight)*125/100; film price = price*quality/100 (0 if self-produced flag), film quality/topicality = quality; series: episode quality; adds quality to Sammy points if category 3/4 is running; end time = now + duration(+0x166, default 10); refuses if previous production of the title not yet shown */
u16 studio_start_production(u16 a0) {
    u16 ax;
    u16 dx;
    u16 s0;
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
    u16 l_26;
    u16 l_28;
    u16 t1;
    u16 t4;
    u32 t28;
    u16 t34;
    FUNC_TICK(274);
    l_28 = 1;
    l_10 = studios_at(a0);
    l_1C = (u16)(i8)(AT(studio, l_10).renter);
    l_22 = AT(studio, l_10).script;
    t1 = (u16)(i8)(AT(studio, l_10).episode);
    l_18 = (u16)(i8)(AT(studio, l_10).episode);
    l_16 = t1;
    l_4 = screenplays_at(l_22);
    if (((u16)(((i16)((AT(screenplay, l_4).flags_16) << 0xF)) >> 0xF)) != 0) {
        l_14 = MK32(FPSEG(l_4), (FPOFF(l_4) + ((l_18 - 1) * 0x23)) + 8);
        l_20 = 0xB;
    } else {
        l_14 = MK32(FPSEG(l_4), (FPOFF(l_4) + ((l_18 - 1) * 0x23)) + 8);
        l_20 = (u16)(i8)(films(AT(screenplay, l_4).target).genre);
    }
    if (!(((u16)(((i16)((AT(screenplay, l_4).flags_16) << 0xF)) >> 0xF)) != 0 || ((u16)(((i16)((AT(screenplay, l_4).flags_16) << 0xD)) >> 0xF)) == 0 || (library_contains(l_1C, l_20, AT(screenplay, l_4).target)) == 0)) {
        ui_warning(0x44);
        l_28 = 0;
    }
    if (l_28 == 1) {
        l_26 = 0;
        l_1E = 0;
        l_24 = 1;
        while (((i16)l_24) <= 10) {
            LOOP_TICK(110);
            if (((i8)(FP8(l_14, (l_24 * 3) + 1))) <= 0 || ((i8)(FP8(l_14, (l_24 * 3) + 1))) > 15) {
                if (((i8)(FP8(l_14, (l_24 * 3) + 1))) >= 16) {
                    l_26 = l_26 + util_min((u16)(i8)(FP8(l_14, (l_24 * 3) + 2)), (u16)(i8)(G8((((((u16)(i8)(FP8(l_14, (l_24 * 3) + 1))) + 0xFFF1) << 2) + 0x268E) + ((u16)(i8)(FP8(l_14, (l_24 * 3) + 3))))));
                    l_1E = l_1E + ((u16)(i8)(FP8(l_14, (l_24 * 3) + 2)));
                }
            } else {
                t4 = script_role_match_pct(l_22, l_18, l_24, (u16)(i8)(FP8(l_14, (l_24 * 3) + 3)));
                l_26 = l_26 + (u16)((u16)((i32)(i16)(t4 * ((u16)(i8)(FP8(l_14, (l_24 * 3) + 2)))) / 100));
                l_1E = l_1E + ((u16)(i8)(FP8(l_14, (l_24 * 3) + 2)));
            }
            l_24 = l_24 + 1;
        }
        if (l_1E == 0) {
            ax = 0;
        } else {
            ax = (u16)((u16)((i32)(i16)(((u16)(i8)FP8(l_14, 0)) * l_26) / (i16)l_1E));
        }
        l_26 = (u16)((u16)((i32)(i16)(ax * 0x7D) / 100));
        if (((u16)(((i16)((AT(screenplay, l_4).flags_16) << 0xF)) >> 0xF)) == 0) {
            l_1A = AT(screenplay, l_4).target;
            l_C = films_at(l_1A);
            l_18 = (u16)(i8)(AT(studio, l_10).episode);
            if (((u16)(((i16)((AT(film, l_C).flags2_16) << 0xD)) >> 0xF)) == 0) {
                if (FP8(l_14, 0) == 0) {
                    ax = 0;
                    dx = 0;
                } else {
                    t28 = (u32)((i32)((u32)(MK32(((i16)l_26) >> 0xF, l_26) * AT(film, l_C).price)) / (i32)0x64);
                    ax = LO16(t28);
                    dx = HI16(t28);
                }
                AT(film, l_C).price = MK32(dx, ax);
            } else {
                AT(film, l_C).price = 0;
            }
            AT(film, l_C).quality = (u8)l_26;
            AT(film, l_C).topicality = (u8)l_26;
            if (!(g_sammy_category(0) != 3 || (sammy_daily_update()) != 1)) {
                g_sammy_category(l_1C) = g_sammy_category(l_1C) + l_26;
            }
        } else {
            l_1A = AT(screenplay, l_4).target;
            l_8 = series_at(l_1A);
            t34 = (u16)(i8)(AT(studio, l_10).episode);
            l_18 = (u16)(i8)(AT(studio, l_10).episode);
            FP8(l_8, l_18 * 3) = (u8)((((u8)((u8)(t34 >> 8))) << 8) | ((u8)((u8)l_26)));
            FP8(l_8, (l_18 * 3) + 1) = (u8)((((u8)((u8)(((u16)((((u8)((u8)(t34 >> 8))) << 8) | ((u8)((u8)l_26)))) >> 8))) << 8) | ((u8)((u8)l_26)));
            if (!(g_sammy_category(0) != 4 || (sammy_daily_update()) != 1)) {
                g_sammy_category(l_1C) = g_sammy_category(l_1C) + l_26;
            }
        }
        AT(studio, l_10).episode = (AT(studio, l_10).episode) + 1;
        AT(studio, l_10).running = 1;
        AT(studio, l_10).end_day = g_day;
        s0 = clock_hour();
        if ((AT(screenplay, l_4).duration) != 0) {
            ax = AT(screenplay, l_4).duration;
        } else {
            ax = 0xA;
        }
        AT(studio, l_10).end_hour = (s0 + ax) + 0x10;
        while (((i16)(AT(studio, l_10).end_hour)) > 24) {
            LOOP_TICK(7);
            AT(studio, l_10).end_hour = (AT(studio, l_10).end_hour) - 0xC;
            AT(studio, l_10).end_day = (AT(studio, l_10).end_day) + 1;
        }
        l_24 = 0;
        while (((i16)g_shoplist_count(l_1C)) > ((i16)l_24)) {
            LOOP_TICK(34);
            if ((shoplists(l_1C, l_24).script) != l_22 || (shoplists(l_1C, l_24).episode) != l_16) {
                l_24 = l_24 + 1;
                continue;
            } else {
                goto L_0FE3;
            }
        }
    }
    ax = l_28;
    return ax;
L_0FE3:
    shoplist_delete(l_1C, l_24);
    ax = l_28;
    return ax;
}

/* 229E:100C  (player, studio): rent now (studio_take_over) if no production is running, else queue player as pending renter (+0xF) */
void studio_request_rent(u16 a0, u16 a1, u16 ax) {
    FUNC_TICK(27);
    if ((studios(a1).running) != 0) {
        studios(a1).reserved = (u8)((((u8)((u8)(ax >> 8))) << 8) | ((u8)((u8)a0)));
    } else {
        studio_take_over(a1, a0);
    }
    return;
}

/* 229E:104A  release studio a0 now, or mark it for release after the running production (+0xF = 0xFF) */
void studio_give_back(u16 a0) {
    FUNC_TICK(23);
    if ((studios(a0).running) != 0) {
        studios(a0).reserved = 0xFF;
    } else {
        studio_release(a0);
    }
    return;
}

/* 229E:1080  (studio, player): rent += rent/2, player pays 3x the new rent (human: ledger cat 8 "Studio rental"; AI: budget pots); sets owner, clears production, updates door signs ("Studio of") */
void studio_take_over(u16 a0, u16 a1) {
    u32 l_4;
    u32 l_8;
    u16 l_A;
    u16 l_C;
    u32 t1;
    u32 t2;
    u32 t3;
    u32 t4;
    u32 t5;
    u16 t8;
    FUNC_TICK(125);
    l_8 = studios_at(a0);
    t1 = (u32)((i32)AT(studio, l_8).rent / (i32)2);
    AT(studio, l_8).rent = (AT(studio, l_8).rent) + t1;
    t2 = (u32)(AT(studio, l_8).rent * 3);
    players(a1).money = (players(a1).money) - t2;
    if (!mp_is_human(a1)) {              /* a1 != g_player (multiplayer: no human; else no slot is found) */
        t4 = (u32)(AT(studio, l_8).rent * 3);
        ai(a1).production = (ai(a1).production) - t4;
        l_C = 0;
        while (((u16)((u16)(i8)g_ai_player(l_C))) != a1) {
            LOOP_TICK(6);
            l_C = l_C + 1;
        }
        t5 = (u32)(AT(studio, l_8).rent * 3);
        t8 = ((a1 * 0x3C) + 0x6E29) + (((u16)(i8)g_ai_period(l_C)) << 2);
        G32(t8 + 0x2C) = (G32((((a1 * 0x3C) + 0x6E29) + (((u16)(i8)g_ai_period(l_C)) << 2)) + 0x2C)) - t5;
        ai_avg_studio_budget(a1);
    } else {
        t3 = (u32)(AT(studio, l_8).rent * 3);
        ledger_add(LEDGER_STUDIO_RENT, t3);
    }
    AT(studio, l_8).renter = (u8)a1;
    AT(studio, l_8).script = 0;
    AT(studio, l_8).episode = 0;
    AT(studio, l_8).episode_last = 0;
    /* ORIGINAL BUG (fix "studio-reserved"): the pending byte (+0xF: a queued
     * renter 1..3, or 0xFF "give it up when the production ends") is set by
     * studio_request_rent / studio_give_back and acted on when a production
     * ends (1DA8:034E, 24B8:0C4A), but nothing clears it.  A leftover 0xFF
     * releases the studio from its next renter after each of his
     * productions; a leftover renter blocks the studio for good and takes it
     * over again (3x the rent, +50 %) after every episode.  Fixed: cleared
     * when the studio changes hands, here and in studio_release -- and the
     * end day too: studio_check_finished (1DA8:034E) finishes "the
     * production" of any studio whose end day comes, so a studio handed on
     * before its end (a bailiff or terrorist frees a studio mid-production
     * and leaves the end; a queued renter gets it when "Trendy" airs)
     * would finish screenplay 0 for its new renter. */
    if (fix_on(FIX_STUDIO_RESERVED)) { AT(studio, l_8).reserved = 0; AT(studio, l_8).end_day = 0; }
    l_C = 1;
    while (((i16)l_C) <= 13) {
        LOOP_TICK(42);
        l_A = 0;
        while (((i16)l_A) <= 4) {
            LOOP_TICK(37);
            l_4 = floors_at(l_C);
            if (FP8(l_4, l_A + 0x6E) == a0) {
                FP8(l_4, l_A + 0x46) = (u8)a1;
                FP8(l_4, l_A + 0x1E) = 5;
                FP8(l_4, l_A + 0x28) = 0x8B;
                FP8(l_4, l_A + 0x32) = 0x8C;
                FP8(l_4, l_A + 0x50) = 0xC;
                FP8(l_4, l_A + 0x5A) = 2;
            }
            l_A = l_A + 1;
        }
        l_C = l_C + 1;
    }
    return;
}

/* 229E:1264  return studio a0: unfinished production goes back to the owner's script list (if < 5), studio cleared (owner 0xFF), door signs set to "Free studio" */
void studio_release(u16 a0) {
    u32 l_4;
    u32 l_8;
    u32 l_C;
    u16 l_E;
    u16 l_10;
    u16 l_12;
    u16 l_14;
    u16 t1;
    u16 t3;
    u16 t4;
    FUNC_TICK(65);
    l_C = studios_at(a0);
    if ((AT(studio, l_C).script) != 0) {
        t1 = (u16)(i8)(AT(studio, l_C).renter);
        l_10 = (u16)(i8)(AT(studio, l_C).renter);
        t3 = g_scripts_owned_count(t1);
        l_E = g_scripts_owned_count(t1);
        if (((i16)t3) < 5) {
            l_4 = owned_scripts_at(l_10, l_E);
            AT(owned_script, l_4).script = AT(studio, l_C).script;
            AT(owned_script, l_4).episode = (u16)(i8)(AT(studio, l_C).episode);
            t4 = script_count_scenes(AT(studio, l_C).script);
            AT(owned_script, l_4).aux = (u8)t4;
        }
    }
    AT(studio, l_C).renter = 0xFF;
    AT(studio, l_C).script = 0;
    AT(studio, l_C).episode = 0;
    AT(studio, l_C).episode_last = 0;
    if (fix_on(FIX_STUDIO_RESERVED)) { AT(studio, l_C).reserved = 0; AT(studio, l_C).end_day = 0; }   /* fix "studio-reserved" (studio_take_over) */
    l_14 = 1;
    while (((i16)l_14) <= 13) {
        LOOP_TICK(35);
        l_8 = floors_at(l_14);
        l_12 = 0;
        while (((i16)l_12) < 4) {
            LOOP_TICK(21);
            if (FP8(l_8, l_12 + 0x6E) == a0) {
                FP8(l_8, l_12 + 0x46) = 0;
                FP8(l_8, l_12 + 0x1E) = 0x36;
                FP8(l_8, l_12 + 0x28) = 0x89;
                FP8(l_8, l_12 + 0x32) = 0x8A;
            }
            l_12 = l_12 + 1;
        }
        l_14 = l_14 + 1;
    }
    return;
}

/* 229E:1386  load savegame a0 (g_save_mode = 1) via game_save_load */
void game_load(u16 a0, u16 ax) {
    FUNC_TICK(8);
    g_save_mode = 1;
    game_save_load(a0, ax);
    speed4_after_load();             /* saved at the fourth speed, loaded without it (fixes.c) */
    return;
}

/* multiplayer (src/mpsave.c): game_load reads these bytes instead of a
 * slot's resource while mp_load_buf is set */
const u8 *mp_load_buf;
u16 mp_load_len;

/* Films 201.., series 51.. and ads 51.. of a mod with more than the
 * original's 200 films, 50 series and 50 ads (an extension,
 * docs/CHANGES.md): a savegame keeps the original's layout for the original
 * counts and appends the others after the managers' names: "MTVX", u16 3 (the
 * number of counts that follow), u16 films, u16 series, u16 ads, then the
 * films' 0x12-byte, the series' 0x29-byte and the ads' 1-byte records of
 * game_save_load (films: quality, topicality, owner, the four flag bits,
 * price, +0xE, live day and slot; series: the episodes, owner, flag bits,
 * price, next episode; ads: the holder).  A savegame without them -- the
 * start savegame, or one made with fewer -- leaves those entries as their
 * table resource has them, as in a new game; one made with more is not
 * loaded (savegame_too_many: its libraries, schedules, offers and contracts
 * hold ids past this game's tables).  The first version (films only, no
 * header) still loads.  Plain C, no game time: the original data never gets
 * here.
 *
 * With --ai-improve the extension has a fourth count -- the length of the
 * computer players' state (src/ai_improve.c) -- and that state after the
 * records, also without a mod ("MTVX", u16 4, 200, 50, 50, length).  A
 * savegame without it starts the state anew; a build that knows only three
 * counts skips it like the records of a bigger mod.
 *
 * With --ad-offers above 10 a fifth count, the ad agency's offers (the
 * fourth then 0 without --ai-improve), and offers 11.. (7 bytes each, as
 * the original's at DS:19A0) after the computer players' state.  A savegame
 * without them has none past 10; offers past the game's count are left out
 * (their ads are free again at the agency's next refill, adagency_refill
 * recounts every ad's holder). */
static void save_film_rec(u8 *f, u8 *b) {
    b[0] = f[1]; b[1] = f[3]; b[3] = f[5];
    b[4] = (u8)(((f[6] >> 5) & 1) | (((f[6] >> 7) & 1) << 1) | ((f[7] & 1) << 2) | (((f[7] >> 1) & 1) << 3));
    memcpy(b + 5, f + 8, 4);
    b[0xD] = f[0xE];
    memcpy(b + 0xE, f + 0xF, 4);
}
static void load_film_rec(u8 *f, const u8 *b) {
    f[1] = b[0]; f[3] = b[1]; f[5] = b[3];
    f[6] = (u8)((f[6] & ~0xA0) | ((b[4] & 1) << 5) | (((b[4] >> 1) & 1) << 7));
    f[7] = (u8)((f[7] & ~3) | ((b[4] >> 2) & 1) | (((b[4] >> 3) & 1) << 1));
    memcpy(f + 8, b + 5, 4);
    f[0xE] = b[0xD];
    memcpy(f + 0xF, b + 0xE, 4);
}
static void save_series_rec(u8 *r, u8 *b) {
    memcpy(b, r, 0x21);
    b[0x21] = r[0x24];
    b[0x22] = (u8)(((r[0x25] >> 5) & 1) | (((r[0x25] >> 7) & 1) << 1) | ((r[0x26] & 1) << 2) | (((r[0x26] >> 1) & 1) << 3));
    memcpy(b + 0x23, r + 0x27, 4);
    memcpy(b + 0x27, r + 0x31, 2);
}
static void load_series_rec(u8 *r, const u8 *b) {
    memcpy(r, b, 0x21);
    r[0x24] = b[0x21];
    r[0x25] = (u8)((r[0x25] & ~0xA0) | ((b[0x22] & 1) << 5) | (((b[0x22] >> 1) & 1) << 7));
    r[0x26] = (u8)((r[0x26] & ~3) | ((b[0x22] >> 2) & 1) | (((b[0x22] >> 3) & 1) << 1));
    memcpy(r + 0x27, b + 0x23, 4);
    memcpy(r + 0x31, b + 0x27, 2);
}
/* one table's extra records: saved, loaded (the first `saved`), or reset
 * from the resource (rlen bytes at i * stride + roff) */
static void savegame_extra_table(u16 first, u16 n, u16 saved, u16 res, u16 stride, u16 roff, u16 rlen, u16 size,
                                 u8 *(*rec)(u16), void (*put)(u8 *, u8 *), void (*get)(u8 *, const u8 *)) {
    u8 b[0x29];
    for (u16 i = first; i <= n; i++) {
        u8 *r = rec(i);
        memset(b, 0, sizeof b);
        if (g_save_mode != 1) { put(r, b); savegame_io(b, size, 0); }
        else if (i < first + saved) { savegame_io(b, size, 0); get(r, b); }
        else res_read_part(res, r, (u16)(i * stride + roff), rlen, 0);
    }
    if (g_save_mode == 1)
        for (u16 i = n + 1; i < first + saved; i++) savegame_io(b, size, 0);   /* more than this mod has */
}
static u8 *film_rec(u16 i) { return (u8 *)&films(i); }
static u8 *series_rec(u16 i) { return (u8 *)&series(i); }
static u8 *ad_holder(u16 i) { return &ads(i).holder; }
static void save_byte(u8 *r, u8 *b) { b[0] = r[0]; }
static void load_byte(u8 *r, const u8 *b) { r[0] = b[0]; }
/* the extension's head as this game writes it: "MTVX", the number of counts,
 * the counts -- films, series, ads, the computer players' state's length
 * (--ai-improve), the ad agency's offers (--ad-offers), the film dealer's
 * offers (--film-offers): as many as up to the last one this game needs
 * (3 .. MTVX_COUNTS; one between written as it is: 0, 10).  Another count
 * later: one more entry here, its data after the others.  Its length, 0 if
 * the game needs no extension */
#define MTVX_COUNTS 6
static u16 savegame_extra_head(u8 head[6 + 2 * MTVX_COUNTS]) {
    u16 cnt[MTVX_COUNTS] = { g_nfilms, g_nseries, g_nads, ai_improve_save_size(), g_nadoffers, g_nfilmoffers };
    int need[MTVX_COUNTS] = { g_nfilms > 200, g_nseries > 50, g_nads > 50, cnt[3] != 0, g_nadoffers > 10, g_nfilmoffers > 10 };
    u16 n = 0;
    for (int k = 0; k < MTVX_COUNTS; k++) if (need[k]) n = (u16)(k + 1);
    if (!n) return 0;
    if (n < 3) n = 3;
    memcpy(head, "MTVX", 4);
    head[4] = (u8)n; head[5] = 0;
    for (int k = 0; k < n; k++) { head[6 + 2 * k] = (u8)cnt[k]; head[7 + 2 * k] = (u8)(cnt[k] >> 8); }
    return (u16)(6 + 2 * n);
}
/* (32 bits: 999 films and 999 series take 51 K; game_save refuses what passes SAVEGAME_MAX) */
static u32 savegame_extra_size(void) {
    u8 head[6 + 2 * MTVX_COUNTS];
    u16 h = savegame_extra_head(head);
    if (!h) return 0;
    u32 recs = (g_nfilms > 200 ? (g_nfilms - 200) * 0x12u : 0) + (g_nseries > 50 ? (g_nseries - 50) * 0x29u : 0) + (g_nads > 50 ? g_nads - 50u : 0);
    return h + recs + ai_improve_save_size() + (g_nadoffers > 10 ? (g_nadoffers - 10) * 0x7u : 0)
           + (g_nfilmoffers > 10 ? (g_nfilmoffers - 10) * 0x8u : 0);
}
/* the film dealer's slots past 10 of a savegame being loaded (8 bytes each, as the
 * original's at DS:19ED, after the ad offers, at `at`; `saved` the savegame's count):
 * film_offers_restore takes as many as this game has -- slots it has and the savegame
 * not stay empty until the dealer's next refill --, film_offers_return_surplus gives the
 * offers past this game's slots back to the market, once the films' and series' records
 * are loaded (the records would set their flags again) */
static int film_offers_saved(u32 at, u16 saved, u16 size, u16 *fsaved) {
    *fsaved = saved > 10 ? (u16)(saved - 10) : 0;
    return *fsaved && at + *fsaved * 8u <= size;
}
static void film_offers_restore(u32 at, u16 saved, u16 size) {
    u16 fsaved, fgame = g_nfilmoffers > 10 ? (u16)(g_nfilmoffers - 10) : 0;
    filmoffers_clear(11);
    if (film_offers_saved(at, saved, size, &fsaved) && fgame)
        memcpy(g_dealer_extra, HPTR(MK32(g_save_buf_hi, (u16)(g_save_buf_lo + at))), (fsaved < fgame ? fsaved : fgame) * 8u);
}
static void film_offers_return_surplus(u32 at, u16 saved, u16 size) {
    u16 fsaved, fgame = g_nfilmoffers > 10 ? (u16)(g_nfilmoffers - 10) : 0;
    if (!film_offers_saved(at, saved, size, &fsaved)) return;
    for (u16 i = fgame; i < fsaved; i++) filmoffers_return_to_market(HPTR(MK32(g_save_buf_hi, (u16)(g_save_buf_lo + at + i * 8u))));
}
/* the original's savegame up to the managers' names, where the extension
 * begins (game_save_load: 37 605 bytes) */
static u16 savegame_base_size(void);
/* A savegame made with more films, series or ads than this game has -- a
 * pack rebuilt smaller, its build folder's savegames kept: the stations'
 * libraries, the schedules, the dealer's and the auction's offers and the
 * contracts hold ids past this game's tables (films(280) of 250 films lies
 * in the series table, ads_at_id past g_nads in other data).  game_save_load
 * loads nothing of it, the load and save room says why
 * (savegame_refused_warning).  The savegame (`size` bytes) is in the buffer;
 * plain C, no game time. */
static int load_refused;
/* A savegame's length is 16 bits (the resource index, g_save_pos), and the game
 * addresses it through one far pointer (the buffer's offset plus the position,
 * 16 bits: the offset of a normalized pointer is below 16): a mod whose
 * extension takes it past SAVEGAME_MAX -- with every option on, 999 films, 50
 * ads and more than 374 series -- is not saved (game_save; tools/content.py and
 * the editor refuse such a mod). */
#define SAVEGAME_MAX 0xFFF0u
static int save_refused;
static int savegame_too_many(u16 size) {
    static const u16 orig[3] = { 200, 50, 50 };
    u16 n[3] = { g_nfilms, g_nseries, g_nads };
    u16 base = savegame_base_size();
    if (!g_save_buf || size <= base) return 0;
    const u8 *x = HPTR(MK32(g_save_buf_hi, (u16)(g_save_buf_lo + base)));
    u16 left = (u16)(size - base);
    if (left < 6 || memcmp(x, "MTVX", 4))      /* the first version: films only */
        return left / 0x12 > (n[0] > 200 ? n[0] - 200 : 0);
    u16 cnt = (u16)(x[4] | x[5] << 8);
    for (u16 k = 0; k < cnt && k < 3 && 6u + 2 * k + 2 <= left; k++) {
        u16 v = (u16)(x[6 + 2 * k] | x[7 + 2 * k] << 8);
        if (v > orig[k] && v > n[k]) return 1;
    }
    return 0;
}
int savegame_load_refused(void) { return load_refused; }
int savegame_save_refused(void) { return save_refused; }
void savegame_refused_warning(int saving) {
    char t[120];
    if (saving)
        strcpy(t, g_language == 0 ? "Zu viele Filme, Serien oder Werbespots: der Spielstand passt nicht in 64 KB, nicht gespeichert."
                                  : "Too many films, series or ads: the savegame does not fit in 64 KB, not saved.");
    else
        strcpy(t, g_language == 0 ? "Dieser Spielstand hat mehr Filme, Serien oder Werbespots als dieses Spiel: nicht geladen."
                                  : "This savegame has more films, series or ads than this game: not loaded.");
    u32 h = host_handle(t, sizeof t);
    ui_warning_str(LO16(h), HI16(h));
    host_handle_release(h);
}

static void savegame_extras(u16 slot) {
    u8 head[6 + 2 * MTVX_COUNTS] = { 'M', 'T', 'V', 'X', 3, 0 };
    u16 x[3] = { 0, 0, 0 };              /* the extra films, series and ads in the savegame */
    static const u16 orig[3] = { 200, 50, 50 };
    u16 ai = 0;                          /* the computer players' state (--ai-improve) */
    u16 offers = 10;                     /* the ad agency's offers (--ad-offers) */
    u16 filmoffers = 10;                 /* the film dealer's offers (--film-offers) */
    u32 film_at = 0;                     /* (loading: where the dealer's slots past 10 are) */
    u16 size = 0;
    if (g_save_mode != 1) {
        u16 n[3] = { g_nfilms, g_nseries, g_nads }, h = savegame_extra_head(head);
        if (!h) return;
        ai = ai_improve_save_size();
        offers = g_nadoffers;
        filmoffers = g_nfilmoffers;
        for (int k = 0; k < 3; k++) x[k] = n[k] > orig[k] ? n[k] - orig[k] : 0;
        savegame_io(head, h, 0);
    } else {
        size = mp_load_buf ? mp_load_len : FP16(g_res_index, (slot << 3) + 2);
        u16 left = size > g_save_pos ? (u16)(size - g_save_pos) : 0;
        int has_ai = 0;
        if (left >= 6 && !memcmp(HPTR(MK32(g_save_buf_hi, g_save_buf_lo + g_save_pos)), "MTVX", 4)) {
            savegame_io(head, 6, 0);
            u16 cnt = (u16)(head[4] | head[5] << 8);
            for (u16 k = 0; k < cnt; k++) {
                u8 w[2];
                savegame_io(w, 2, 0);
                u16 v = (u16)(w[0] | w[1] << 8);
                if (k < 3) x[k] = v > orig[k] ? (u16)(v - orig[k]) : 0;
                if (k == 3) { ai = v; has_ai = 1; }
                if (k == 4) offers = v;
                if (k == 5) filmoffers = v;
            }
        } else {
            x[0] = left / 0x12;          /* the first version: films only */
        }
        /* the computer players' state, after the records (ai_improve_load: none starts it anew) */
        u32 at = (u32)g_save_pos + x[0] * 0x12u + x[1] * 0x29u + x[2];
        ai_improve_load(has_ai && at + ai <= size ? HPTR(MK32(g_save_buf_hi, (u16)(g_save_buf_lo + at))) : NULL, ai);
        /* the offers past 10, after that state: as many as the game has, none it lacks */
        memset(g_ad_offers_extra, 0, (AD_OFFERS_MAX - 10) * 0x7);
        if (offers > 10 && g_nadoffers > 10) {
            u32 o = at + (has_ai ? ai : 0);
            u16 take = (u16)((offers < g_nadoffers ? offers : g_nadoffers) - 10);
            if (o + take * 0x7u <= size) memcpy(g_ad_offers_extra, HPTR(MK32(g_save_buf_hi, (u16)(g_save_buf_lo + o))), take * 0x7u);
        }
        film_at = at + (has_ai ? ai : 0) + (offers > 10 ? (offers - 10) * 0x7u : 0);
        film_offers_restore(film_at, filmoffers, size);
        if (g_nfilms <= 200 && g_nseries <= 50 && g_nads <= 50) { film_offers_return_surplus(film_at, filmoffers, size); return; }
    }
    savegame_extra_table(201, g_nfilms, x[0], 0xC9, 0x17, 0, 0x17, 0x12, film_rec, save_film_rec, load_film_rec);
    savegame_extra_table(51, g_nseries, x[1], 0xCA, 0x39, 0, 0x39, 0x29, series_rec, save_series_rec, load_series_rec);
    savegame_extra_table(51, g_nads, x[2], 0xCC, 0x18, 0xF, 1, 1, ad_holder, save_byte, load_byte);
    if (g_save_mode != 1 && ai) {
        u8 b[256];
        ai_improve_save(b);
        savegame_io(b, ai, 0);
    }
    if (g_save_mode != 1 && offers > 10) savegame_io(g_ad_offers_extra, (u16)((offers - 10) * 0x7), 0);
    if (g_save_mode != 1 && filmoffers > 10) savegame_io(g_dealer_extra, (u16)((filmoffers - 10) * 0x8), 0);
    if (g_save_mode == 1) film_offers_return_surplus(film_at, filmoffers, size);
}

/* the data segment's blocks of a savegame, in their order: before the
 * players' records, and after them (game_save_load; mp_savegame_write
 * writes the same, src/mpsave.c) */
typedef struct { u16 off, len; } SaveBlock;
static const SaveBlock save_blocks_a[] = {
    {0x7B42, 0xB4}, {0x8C6C, 0xC}, {0x8C54, 0x2}, {0x7C82, 0x6AC},
};
static const SaveBlock save_blocks_b[] = {
    {0x79C6, 0xF0}, {0x7AB6, 0x50}, {0x8A60, 0x68}, {0x1D09, 0x738}, {0x6F19, 0x1B0},
    {0x4417, 0x168}, {0x3939, 0x168}, {0x24AD, 0x28}, {0x60BF, 0x82}, {0x7247, 0x1},
    {0x1B70, 0x118}, {0x1A45, 0xC8}, {0x76A8, 0x8}, {0x7672, 0x8}, {0x1C88, 0x78}, {0x74EA, 0x80},
    {0x78BE, 0x80}, {0x6DA9, 0x80}, {0x793E, 0x80}, {0x76B0, 0x80}, {0x5553, 0x20}, {0x5573, 0x20},
    {0x2483, 0x1E}, {0x71F1, 0x54}, {0x7492, 0x54}, {0x734A, 0x2}, {0x457F, 0x960}, {0x3D6B, 0x1E0},
    {0x36DB, 0x258}, {0x6147, 0x1E}, {0x3AA1, 0x2CA}, {0x7248, 0x2}, {0x3F4B, 0x4}, {0x6165, 0xD0},
    {0x1B0D, 0x63}, {0x19ED, 0x58}, {0x19A0, 0x4D}, {0x72A6, 0x8}, {0x196A, 0x36}, {0x734C, 0x12C},
    {0x7480, 0x8}, {0x74E6, 0x4}, {0x71E6, 0x4}, {0x2441, 0x2}, {0x767A, 0x2}, {0x71F0, 0x1},
    {0x2730, 0x1}, {0x72A2, 0x2}, {0x24A1, 0x8}, {0x71E4, 0x2}, {0x8BA8, 0x2}, {0x6143, 0x4},
    {0x15ED, 0x1}, {0xF0, 0x2}, {0x6E29, 0xF0}, {0x71E0, 0x4}, {0x24E5, 0x2D}, {0xF2, 0x2},
    {0x4EDF, 0x134}, {0x7478, 0x8}, {0x7764, 0x18}, {0x2512, 0x134}, {0x72B2, 0x8}, {0x7730, 0x8},
    {0x756A, 0x108}, {0x2443, 0x40}, {0x78B4, 0x8}, {0x7245, 0x4}, {0x264A, 0xC}, {0x70C9, 0x93},
    {0x8AD4, 0xC8}, {0x94, 0x14}, {0x78A8, 0xC}, {0x7488, 0xA}, {0x724A, 0x58}, {0x78BC, 0x2},
    {0x549F, 0xB4},
};
#define SAVE_BLOCKS(t) (int)(sizeof t / sizeof t[0])
static u16 savegame_base_size(void) {
    u16 n = 200 * 0x12 + 50 * 0x29 + 50 * 0x161 + 50 + 3 * 0xF + 3 * 0x28;
    for (int k = 0; k < SAVE_BLOCKS(save_blocks_a); k++) n += save_blocks_a[k].len;
    for (int k = 0; k < SAVE_BLOCKS(save_blocks_b); k++) n += save_blocks_b[k].len;
    return n;
}

/* 229E:139A  serialize/deserialize the whole game state (200 films, 50 series, 50 productions, 50 ad states, ~70 global tables) through savegame_io; direction from g_save_mode */
void game_save_load(u16 a0, u16 ax) {
    u16 t2;
    u16 t7;
    u16 t8;
    u16 t9;
    u16 t14;
    u16 t15;
    u16 t16;
    u16 t21;
    u16 t22;
    u16 t23;
    u16 t28;
    u16 t29;
    u16 t30;
    u16 t40;
    u16 t45;
    u16 t46;
    u16 t47;
    u16 t52;
    u16 t53;
    u16 t54;
    u16 t59;
    u16 t60;
    u16 t61;
    u16 t66;
    u16 t67;
    u16 t68;
    u16 t79;
    u16 t85;
    u16 t88;
    u16 t97;
    u16 t102;
    u16 t108;
    u16 t115;
    u16 t123;
    u16 t125;
    u16 t134;
    u16 t139;
    u16 t145;
    u16 t152;
    u16 t160;
    u8 fr_[430] = {0};                  /* bytes bp-430 .. bp-1 of the original frame */
    u16 bp_ = (u16)(R.sp - 2);           /* the original frame's address, for AX values */
    FUNC_TICK(641);
    disk_access_begin(ax);
    if (g_save_mode != 1) {
        savegame_io(HPTR(MK32(0, 0)), 0, 0 /* unused ax */);
    } else {
        savegame_io(HPTR(MK32(0, 0)), -(u16)(a0), 0 /* unused ax */);
        load_refused = savegame_too_many(mp_load_buf ? mp_load_len : FP16(g_res_index, (a0 << 3) + 2));
        if (load_refused) {              /* more films, series or ads than this game has: nothing loaded */
            ax = savegame_io(HPTR(MK32(0, 0)), 0, 0);
            disk_access_end(ax);
            return;
        }
    }
    if (g_save_mode == 1) {
        FR16(fr_, 430, 0x1AC) = 1;
        while (((i16)FR16(fr_, 430, 0x1AC)) <= 200) {
            LOOP_TICK(81);
            t2 = FPOFF(g_movies) + (FR16(fr_, 430, 0x1AC) * 0x17);
            FR16(fr_, 430, 0x1A6) = FPSEG(g_movies);
            FR16(fr_, 430, 0x1A8) = t2;
            savegame_io((fr_ + 430 - 0x1A0), 0x12, 0 /* unused ax */);
            FP8(FR32(fr_, 430, 0x1A8), 1) = FR8(fr_, 430, 0x1A0);
            FP8(FR32(fr_, 430, 0x1A8), 3) = FR8(fr_, 430, 0x19F);
            FP8(FR32(fr_, 430, 0x1A8), 5) = FR8(fr_, 430, 0x19D);
            t7 = ((i16)(FR16(fr_, 430, 0x19C) << 0xF)) >> 0xF;
            t8 = FPOFF(FR32(fr_, 430, 0x1A8));
            t9 = FPSEG(FR32(fr_, 430, 0x1A8));
            MEM16(t9, t8 + 6) = (FP16(FR32(fr_, 430, 0x1A8), 6)) & 0xFFDF;
            MEM16(t9, t8 + 6) = MEM16(t9, t8 + 6) | ((t7 & 1) << 5);
            t14 = ((i16)(FR16(fr_, 430, 0x19C) << 0xE)) >> 0xF;
            t15 = FPOFF(FR32(fr_, 430, 0x1A8));
            t16 = FPSEG(FR32(fr_, 430, 0x1A8));
            MEM16(t16, t15 + 6) = (FP16(FR32(fr_, 430, 0x1A8), 6)) & 0xFF7F;
            MEM16(t16, t15 + 6) = MEM16(t16, t15 + 6) | ((t14 & 1) << 7);
            t21 = ((i16)(FR16(fr_, 430, 0x19C) << 0xD)) >> 0xF;
            t22 = FPOFF(FR32(fr_, 430, 0x1A8));
            t23 = FPSEG(FR32(fr_, 430, 0x1A8));
            MEM16(t23, t22 + 7) = (FP16(FR32(fr_, 430, 0x1A8), 7)) & 0xFFFE;
            MEM16(t23, t22 + 7) = MEM16(t23, t22 + 7) | (t21 & 1);
            t28 = ((i16)(FR16(fr_, 430, 0x19C) << 0xC)) >> 0xF;
            t29 = FPOFF(FR32(fr_, 430, 0x1A8));
            t30 = FPSEG(FR32(fr_, 430, 0x1A8));
            MEM16(t30, t29 + 7) = (FP16(FR32(fr_, 430, 0x1A8), 7)) & 0xFFFD;
            MEM16(t30, t29 + 7) = MEM16(t30, t29 + 7) | ((t28 & 1) << 1);
            MEM32((u16)(FPSEG(FR32(fr_, 430, 0x1A8))), (u16)(FPOFF(FR32(fr_, 430, 0x1A8))) + 8) = FR32(fr_, 430, 0x19B);
            FP8(FR32(fr_, 430, 0x1A8), 0xE) = FR8(fr_, 430, 0x193);
            FP16(FR32(fr_, 430, 0x1A8), 0xF) = FR16(fr_, 430, 0x192);
            FP16(FR32(fr_, 430, 0x1A8), 0x11) = FR16(fr_, 430, 0x190);
            FR16(fr_, 430, 0x1AC) = FR16(fr_, 430, 0x1AC) + 1;
        }
        FR16(fr_, 430, 0x1AC) = 1;
        while (((i16)FR16(fr_, 430, 0x1AC)) <= 50) {
            LOOP_TICK(78);
            t40 = FPOFF(g_series) + (FR16(fr_, 430, 0x1AC) * 0x39);
            FR16(fr_, 430, 0x1A2) = FPSEG(g_series);
            FR16(fr_, 430, 0x1A4) = t40;
            savegame_io((fr_ + 430 - 0x18E), 0x29, 0 /* unused ax */);
            FP8(FR32(fr_, 430, 0x1A4), 0x24) = FR8(fr_, 430, 0x16D);
            t45 = ((i16)(FR16(fr_, 430, 0x16C) << 0xF)) >> 0xF;
            t46 = FPOFF(FR32(fr_, 430, 0x1A4));
            t47 = FPSEG(FR32(fr_, 430, 0x1A4));
            MEM16(t47, t46 + 0x25) = (FP16(FR32(fr_, 430, 0x1A4), 0x25)) & 0xFFDF;
            MEM16(t47, t46 + 0x25) = MEM16(t47, t46 + 0x25) | ((t45 & 1) << 5);
            t52 = ((i16)(FR16(fr_, 430, 0x16C) << 0xE)) >> 0xF;
            t53 = FPOFF(FR32(fr_, 430, 0x1A4));
            t54 = FPSEG(FR32(fr_, 430, 0x1A4));
            MEM16(t54, t53 + 0x25) = (FP16(FR32(fr_, 430, 0x1A4), 0x25)) & 0xFF7F;
            MEM16(t54, t53 + 0x25) = MEM16(t54, t53 + 0x25) | ((t52 & 1) << 7);
            t59 = ((i16)(FR16(fr_, 430, 0x16C) << 0xD)) >> 0xF;
            t60 = FPOFF(FR32(fr_, 430, 0x1A4));
            t61 = FPSEG(FR32(fr_, 430, 0x1A4));
            MEM16(t61, t60 + 0x26) = (FP16(FR32(fr_, 430, 0x1A4), 0x26)) & 0xFFFE;
            MEM16(t61, t60 + 0x26) = MEM16(t61, t60 + 0x26) | (t59 & 1);
            t66 = ((i16)(FR16(fr_, 430, 0x16C) << 0xC)) >> 0xF;
            t67 = FPOFF(FR32(fr_, 430, 0x1A4));
            t68 = FPSEG(FR32(fr_, 430, 0x1A4));
            MEM16(t68, t67 + 0x26) = (FP16(FR32(fr_, 430, 0x1A4), 0x26)) & 0xFFFD;
            MEM16(t68, t67 + 0x26) = MEM16(t68, t67 + 0x26) | ((t66 & 1) << 1);
            MEM32((u16)(FPSEG(FR32(fr_, 430, 0x1A4))), (u16)(FPOFF(FR32(fr_, 430, 0x1A4))) + 0x27) = FR32(fr_, 430, 0x16B);
            FP16(FR32(fr_, 430, 0x1A4), 0x31) = FR16(fr_, 430, 0x167);
            far_memcpy(HPTR(FR32(fr_, 430, 0x1A4)), (fr_ + 430 - 0x18E), 0x21);
            FR16(fr_, 430, 0x1AC) = FR16(fr_, 430, 0x1AC) + 1;
        }
        FR16(fr_, 430, 0x1AC) = 1;
        while (((i16)FR16(fr_, 430, 0x1AC)) <= 50) {
            LOOP_TICK(46);
            t79 = savegame_io((fr_ + 430 - 0x164), 0x161, bp_ - 0x164);
            screenplays(FR16(fr_, 430, 0x1AC)).state = (u8)((((u8)((u8)(t79 >> 8))) << 8) | FR8(fr_, 430, 0x164));
            FR16(fr_, 430, 0x1AA) = 0;
            while (((i16)FR16(fr_, 430, 0x1AA)) < 10) {
                LOOP_TICK(25);
                struct_copy(HPTR(MK32(FPSEG(g_scripts), ((FPOFF(g_scripts) + (FR16(fr_, 430, 0x1AC) * 0x1B3)) + (FR16(fr_, 430, 0x1AA) * 0x23)) + 8)), (fr_ + 430 - 0x163 + (int)(i16)((FR16(fr_, 430, 0x1AA) * 0x23) + 0)), 0x23);
                FR16(fr_, 430, 0x1AA) = FR16(fr_, 430, 0x1AA) + 1;
            }
            FR16(fr_, 430, 0x1AC) = FR16(fr_, 430, 0x1AC) + 1;
        }
        FR16(fr_, 430, 0x1AC) = 1;
        while (((i16)FR16(fr_, 430, 0x1AC)) <= 50) {
            LOOP_TICK(21);
            t85 = savegame_io((fr_ + 430 - 0x2), 1, bp_ - 2);
            ads(FR16(fr_, 430, 0x1AC)).holder = (u8)((((u8)((u8)(t85 >> 8))) << 8) | FR8(fr_, 430, 0x2));
            FR16(fr_, 430, 0x1AC) = FR16(fr_, 430, 0x1AC) + 1;
        }
    } else {
        FR16(fr_, 430, 0x1AC) = 1;
        while (((i16)FR16(fr_, 430, 0x1AC)) <= 200) {
            LOOP_TICK(86);
            t88 = FPOFF(g_movies) + (FR16(fr_, 430, 0x1AC) * 0x17);
            FR16(fr_, 430, 0x1A6) = FPSEG(g_movies);
            FR16(fr_, 430, 0x1A8) = t88;
            FR8(fr_, 430, 0x1A0) = films(FR16(fr_, 430, 0x1AC)).quality;
            FR8(fr_, 430, 0x19F) = FP8(FR32(fr_, 430, 0x1A8), 3);
            FR8(fr_, 430, 0x19D) = FP8(FR32(fr_, 430, 0x1A8), 5);
            t97 = ((i16)((FP16(FR32(fr_, 430, 0x1A8), 6)) << 0xA)) >> 0xF;
            FR16(fr_, 430, 0x19C) = FR16(fr_, 430, 0x19C) & 0xFFFE;
            FR16(fr_, 430, 0x19C) = FR16(fr_, 430, 0x19C) | (t97 & 1);
            t102 = ((i16)((FP16(FR32(fr_, 430, 0x1A8), 6)) << 8)) >> 0xF;
            FR16(fr_, 430, 0x19C) = FR16(fr_, 430, 0x19C) & 0xFFFD;
            FR16(fr_, 430, 0x19C) = FR16(fr_, 430, 0x19C) | ((t102 & 1) << 1);
            t108 = ((i16)((FP16(FR32(fr_, 430, 0x1A8), 7)) << 0xF)) >> 0xF;
            FR16(fr_, 430, 0x19C) = FR16(fr_, 430, 0x19C) & 0xFFFB;
            FR16(fr_, 430, 0x19C) = FR16(fr_, 430, 0x19C) | ((t108 & 1) << 2);
            t115 = ((i16)((FP16(FR32(fr_, 430, 0x1A8), 7)) << 0xE)) >> 0xF;
            FR16(fr_, 430, 0x19C) = FR16(fr_, 430, 0x19C) & 0xFFF7;
            FR16(fr_, 430, 0x19C) = FR16(fr_, 430, 0x19C) | ((t115 & 1) << 3);
            t123 = FP16(FR32(fr_, 430, 0x1A8), 8);
            FR16(fr_, 430, 0x199) = FP16(FR32(fr_, 430, 0x1A8), 0xA);
            FR16(fr_, 430, 0x19B) = t123;
            FR16(fr_, 430, 0x192) = FP16(FR32(fr_, 430, 0x1A8), 0xF);
            FR16(fr_, 430, 0x190) = FP16(FR32(fr_, 430, 0x1A8), 0x11);
            FR8(fr_, 430, 0x193) = FP8(FR32(fr_, 430, 0x1A8), 0xE);
            savegame_io((fr_ + 430 - 0x1A0), 0x12, 0 /* unused ax */);
            FR16(fr_, 430, 0x1AC) = FR16(fr_, 430, 0x1AC) + 1;
        }
        FR16(fr_, 430, 0x1AC) = 1;
        while (((i16)FR16(fr_, 430, 0x1AC)) <= 50) {
            LOOP_TICK(107);
            t125 = FPOFF(g_series) + (FR16(fr_, 430, 0x1AC) * 0x39);
            FR16(fr_, 430, 0x1A2) = FPSEG(g_series);
            FR16(fr_, 430, 0x1A4) = t125;
            FR8(fr_, 430, 0x16D) = series(FR16(fr_, 430, 0x1AC)).owner;
            t134 = ((i16)((series(FR16(fr_, 430, 0x1AC)).flags_16) << 0xA)) >> 0xF;
            FR16(fr_, 430, 0x16C) = FR16(fr_, 430, 0x16C) & 0xFFFE;
            FR16(fr_, 430, 0x16C) = FR16(fr_, 430, 0x16C) | (t134 & 1);
            t139 = ((i16)((series(FR16(fr_, 430, 0x1AC)).flags_16) << 8)) >> 0xF;
            FR16(fr_, 430, 0x16C) = FR16(fr_, 430, 0x16C) & 0xFFFD;
            FR16(fr_, 430, 0x16C) = FR16(fr_, 430, 0x16C) | ((t139 & 1) << 1);
            t145 = ((i16)((series(FR16(fr_, 430, 0x1AC)).flags2_16) << 0xF)) >> 0xF;
            FR16(fr_, 430, 0x16C) = FR16(fr_, 430, 0x16C) & 0xFFFB;
            FR16(fr_, 430, 0x16C) = FR16(fr_, 430, 0x16C) | ((t145 & 1) << 2);
            t152 = ((i16)((series(FR16(fr_, 430, 0x1AC)).flags2_16) << 0xE)) >> 0xF;
            FR16(fr_, 430, 0x16C) = FR16(fr_, 430, 0x16C) & 0xFFF7;
            FR16(fr_, 430, 0x16C) = FR16(fr_, 430, 0x16C) | ((t152 & 1) << 3);
            t160 = price_5_lo(FR16(fr_, 430, 0x1AC));
            FR16(fr_, 430, 0x169) = price_5_hi(FR16(fr_, 430, 0x1AC));
            FR16(fr_, 430, 0x16B) = t160;
            FR16(fr_, 430, 0x167) = series(FR16(fr_, 430, 0x1AC)).next_ep;
            far_memcpy((fr_ + 430 - 0x18E), HPTR(FR32(fr_, 430, 0x1A4)), 0x21);
            savegame_io((fr_ + 430 - 0x18E), 0x29, 0 /* unused ax */);
            FR16(fr_, 430, 0x1AC) = FR16(fr_, 430, 0x1AC) + 1;
        }
        FR16(fr_, 430, 0x1AC) = 1;
        while (((i16)FR16(fr_, 430, 0x1AC)) <= 50) {
            LOOP_TICK(44);
            FR8(fr_, 430, 0x164) = screenplays(FR16(fr_, 430, 0x1AC)).state;
            FR16(fr_, 430, 0x1AA) = 0;
            while (((i16)FR16(fr_, 430, 0x1AA)) < 10) {
                LOOP_TICK(25);
                struct_copy((fr_ + 430 - 0x163 + (int)(i16)((FR16(fr_, 430, 0x1AA) * 0x23) + 0)), HPTR(MK32(FPSEG(g_scripts), ((FPOFF(g_scripts) + (FR16(fr_, 430, 0x1AC) * 0x1B3)) + (FR16(fr_, 430, 0x1AA) * 0x23)) + 8)), 0x23);
                FR16(fr_, 430, 0x1AA) = FR16(fr_, 430, 0x1AA) + 1;
            }
            savegame_io((fr_ + 430 - 0x164), 0x161, 0 /* unused ax */);
            FR16(fr_, 430, 0x1AC) = FR16(fr_, 430, 0x1AC) + 1;
        }
        FR16(fr_, 430, 0x1AC) = 1;
        while (((i16)FR16(fr_, 430, 0x1AC)) <= 50) {
            LOOP_TICK(19);
            FR8(fr_, 430, 0x2) = ads(FR16(fr_, 430, 0x1AC)).holder;
            savegame_io((fr_ + 430 - 0x2), 1, 0 /* unused ax */);
            FR16(fr_, 430, 0x1AC) = FR16(fr_, 430, 0x1AC) + 1;
        }
    }
    for (int k = 0; k < SAVE_BLOCKS(save_blocks_a); k++)
        savegame_io(HPTR(MK32(DSEG, save_blocks_a[k].off)), save_blocks_a[k].len, 0 /* unused ax */);
    FR16(fr_, 430, 0x1AC) = 1;
    while (((i16)FR16(fr_, 430, 0x1AC)) <= 3) {
        LOOP_TICK(15);
        savegame_io(HPTR(MK32(DSEG, (FR16(fr_, 430, 0x1AC) * 0x17) + 0x8BE4)), 0xF, 0 /* unused ax */);
        FR16(fr_, 430, 0x1AC) = FR16(fr_, 430, 0x1AC) + 1;
    }
    for (int k = 0; k < SAVE_BLOCKS(save_blocks_b); k++)
        savegame_io(HPTR(MK32(DSEG, save_blocks_b[k].off)), save_blocks_b[k].len, 0 /* unused ax */);
    FR16(fr_, 430, 0x1AC) = 1;
    while (((i16)FR16(fr_, 430, 0x1AC)) <= 3) {
        LOOP_TICK(16);
        savegame_io(HPTR(MK32(pl_name_hi(FR16(fr_, 430, 0x1AC)), pl_name_lo(FR16(fr_, 430, 0x1AC)))), 0x28, 0 /* unused ax */);
        FR16(fr_, 430, 0x1AC) = FR16(fr_, 430, 0x1AC) + 1;
    }
    savegame_extras(a0);                 /* films 201.., series 51.., ads 51.. of a mod (an extension) */
    if (g_save_mode != 1) {
        ax = savegame_io(HPTR(MK32(0, 0)), -(u16)(a0), 0);
    } else {
        ax = savegame_io(HPTR(MK32(0, 0)), 0, 0);
    }
    disk_access_end(ax);
    return;
}

/* 229E:200D  save game to slot a0 (g_save_mode = 2) via game_save_load */
void game_save(u16 a0, u16 ax) {
    FUNC_TICK(9);
    save_refused = savegame_base_size() + savegame_extra_size() > SAVEGAME_MAX;
    if (save_refused) {                  /* a mod too large for a savegame: not saved (the room says why) */
        g_save_error = 1;
        return;
    }
    g_save_mode = 2;
    game_save_load(a0, ax);
    return;
}

/* Multiplayer (src/mpsave.c): the savegame game_save would write now, into
 * buf -- the same bytes, except the frame bytes the original never sets
 * (game_save_load leaves its stack there: here they are 0) -- in plain C:
 * no game time, no heap, no disk, nothing of the game changed (a network
 * game saves at night on every machine and must play on as if it had not).
 * The bytes written, or 0 if they do not fit in cap or a savegame's
 * 64 KB (its size is 16 bits). */
u32 mp_savegame_write(u8 *buf, u32 cap) {
    u32 n = 0;
    u8 b[0x161];
    if (cap > 0xFFFF) cap = 0xFFFF;
#define PUT(p, len) do { if (n + (u32)(len) > cap) return 0; memcpy(buf + n, (p), (len)); n += (len); } while (0)
    for (u16 i = 1; i <= 200; i++) { memset(b, 0, 0x12); save_film_rec(film_rec(i), b); PUT(b, 0x12); }
    for (u16 i = 1; i <= 50; i++) { memset(b, 0, 0x29); save_series_rec(series_rec(i), b); PUT(b, 0x29); }
    for (u16 i = 1; i <= 50; i++) {
        memset(b, 0, 0x161);             /* (its last two bytes: the original's frame) */
        b[0] = screenplays(i).state;
        for (u16 j = 0; j < 10; j++)
            memcpy(b + 1 + j * 0x23, HPTR(MK32(FPSEG(g_scripts), (u16)(FPOFF(g_scripts) + i * 0x1B3 + j * 0x23 + 8))), 0x23);
        PUT(b, 0x161);
    }
    for (u16 i = 1; i <= 50; i++) PUT(&ads(i).holder, 1);
    for (int k = 0; k < SAVE_BLOCKS(save_blocks_a); k++) PUT(HPTR(MK32(DSEG, save_blocks_a[k].off)), save_blocks_a[k].len);
    for (u16 p = 1; p <= 3; p++) PUT(HPTR(MK32(DSEG, (u16)(p * 0x17 + 0x8BE4))), 0xF);
    for (int k = 0; k < SAVE_BLOCKS(save_blocks_b); k++) PUT(HPTR(MK32(DSEG, save_blocks_b[k].off)), save_blocks_b[k].len);
    for (u16 p = 1; p <= 3; p++) PUT(HPTR(MK32(pl_name_hi(p), pl_name_lo(p))), 0x28);
    u8 head[6 + 2 * MTVX_COUNTS];
    u16 h = savegame_extra_head(head);
    if (h) {                             /* as savegame_extras writes them */
        u16 ai = ai_improve_save_size();
        PUT(head, h);
        for (u16 i = 201; i <= g_nfilms; i++) { memset(b, 0, 0x12); save_film_rec(film_rec(i), b); PUT(b, 0x12); }
        for (u16 i = 51; i <= g_nseries; i++) { memset(b, 0, 0x29); save_series_rec(series_rec(i), b); PUT(b, 0x29); }
        for (u16 i = 51; i <= g_nads; i++) PUT(ad_holder(i), 1);
        if (ai) { ai_improve_save(b); PUT(b, ai); }
        if (g_nadoffers > 10) PUT(g_ad_offers_extra, (g_nadoffers - 10) * 0x7);
        if (g_nfilmoffers > 10) PUT(g_dealer_extra, (g_nfilmoffers - 10) * 0x8);
    }
#undef PUT
    return n;
}

/* 229E:2020  (buf far, n): save mode: n=0 alloc 40000-byte buffer, n>0 append n bytes, n<0 write buffer as resource -n (sets g_save_error); load mode: n<0 load resource -n, n>0 copy next n bytes out, n=0 free */
u16 savegame_io(u8 *buf, u16 a2, u16 ax) {
    u16 bx;
    u32 t1;
    u16 t2;
    u32 t3;
    u16 t4;
    u16 t9;
    u32 t10;
    FUNC_TICK(100);
    if (g_save_mode == 2) {
        if (a2 != 0) {
            if (((i16)a2) >= 0) {
                far_memcpy(HPTR(MK32(g_save_buf_hi, g_save_buf_lo + g_save_pos)), buf, a2);
                g_save_pos = g_save_pos + a2;
                ax = a2;
            } else {
                t2 = a2;
                R.ax = -t2;
                t3 = rtl_res_create(-t2, g_save_pos, 0);
                ax = LO16(t3);
                bx = g_save_pos;
                if (t3 == ((u32)MK32(0, g_save_pos))) {
                    t4 = a2;
                    R.ax = -t4;
                    res_write(-t4, HPTR(g_save_buf), g_save_pos, R.ax);
                    g_save_error = 0;
                } else {
                    g_save_error = 1;
                }
                ax = mem_free(g_save_buf_lo, g_save_buf_hi);
            }
        } else {
            u32 len = 0x9C40 + savegame_extra_size();   /* + a mod's films 201.., series 51.., ads 51.. */
            t1 = mem_calloc((u16)(len <= 0xFFFF ? len : savegame_base_size() + savegame_extra_size()), 1);   /* (game_save: at most SAVEGAME_MAX) */
            g_save_buf = t1;
            g_save_pos = 0;
            ax = LO16(t1);
        }
    } else if (g_save_mode == 1) {
        if (((i16)a2) >= 0) {
            if (a2 != 0) {
                far_memcpy(buf, HPTR(MK32(g_save_buf_hi, g_save_buf_lo + g_save_pos)), a2);
                g_save_pos = g_save_pos + a2;
                ax = a2;
            } else {
                ax = mem_free(g_save_buf_lo, g_save_buf_hi);
            }
        } else {
            t9 = a2;
            if (mp_load_buf) {           /* multiplayer: a network game's savegame, from memory (src/mpsave.c) */
                t10 = mem_calloc(mp_load_len, 1);
                if (t10) memcpy(HPTR(t10), mp_load_buf, mp_load_len);
            } else {
                t10 = res_load_raw(-t9, -t9);
            }
            g_save_buf = t10;
            g_save_pos = 0;
            ax = LO16(t10);
        }
    }
    return ax;
}

/* 229E:2140  (player, genre, id): 0xFFFF if id is in the player's library for that genre, else 0 */
u16 library_contains(u16 a0, u16 a1, u16 a2) {
    u16 ax;
    u16 l_2;
    u16 l_4;
    FUNC_TICK(13);
    l_2 = 0;
    l_4 = 0;
    while (((i16)((u16)(i8)library_count(a0, a1))) > ((i16)l_4)) {
        LOOP_TICK(31);
        if (library_items(a0, a1, l_4) != a2) {
            l_4 = l_4 + 1;
            continue;
        } else {
            goto L_217C;
        }
    }
    ax = l_2;
    return ax;
L_217C:
    l_2 = 0xFFFF;
    ax = l_2;
    return ax;
}
