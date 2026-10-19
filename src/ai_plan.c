/* Computer players III: schedule gaps, ad plan, budget distribution, initialisation.
 *
 * Original module 348B of MTV.EXE (Rainbow Arts, 1991).  Decompiled by
 * tools/decomp.py and hand-maintained since the freeze (git tag
 * generated-final).  Every function keeps its original address in the
 * comment above it; that address ties it to the machine-level twin in gen/
 * and to the difftest tables -- keep it when editing.
 */
#include "game.h"
#include "seat.h"
#include "fixes.h"

/* 348B:0003  count unscheduled 1-block films and series in the AI's library (?series check reads count bytes, likely original bug) */
u16 ai_count_short_programs(void) {
    u16 ax;
    u32 l_4;
    u16 l_6;
    u8 l_7;
    u8 l_8;
    u16 l_A;
    u16 l_C;
    u16 t5;
    FUNC_TICK(28);
    l_8 = 0;
    l_7 = g_ai_player((u16)(i8)g_ai_cur);
    l_4 = library_at((u16)(i8)l_7);
    l_C = 1;
    while (((i16)l_C) <= 10) {
        LOOP_TICK(42);
        l_A = 0;
        while (((i16)((u16)(i8)FP8(l_4, l_C + 0x60))) > ((i16)l_A)) {
            LOOP_TICK(37);
            t5 = FP16(l_4, (l_C << 3) + (l_A << 1));
            l_6 = FP16(l_4, (l_C << 3) + (l_A << 1));
            if (!((films(t5).blocks) != 1 || ((u16)(((i16)((films(l_6).flags2_16) << 0xE)) >> 0xF)) != 0)) {
                l_8 = l_8 + 1;
            }
            l_A = l_A + 1;
        }
        l_C = l_C + 1;
    }
    l_C = 0;
    for (;;) {
        LOOP_TICK(38);
        ax = (u16)(i8)FP8(l_4, 0x6B);
        if (((i16)((u16)(i8)FP8(l_4, 0x6B))) <= ((i16)l_C)) break;
        l_6 = FP16(l_4, (l_C << 1) + 0x58);
        if (!((FP16(l_4, (l_C << 1) + 0x60)) == 0 || ((u16)(((i16)((series(l_6).flags2_16) << 0xE)) >> 0xF)) != 0 || (series(l_6).blocks) != 1)) {
            l_8 = l_8 + 1;
        }
        l_C = l_C + 1;
    }
    ax = (((u8)((u8)(ax >> 8))) << 8) | l_8;
    return ax;
}

/* 348B:0121  (buf, minlen): write the start slots of empty schedule gaps >= minlen blocks (from the next hour) into buf, return count */
u16 ai_find_schedule_gaps(u8 *a0, u16 a2) {
    u16 ax;
    u8 l_1;
    u8 l_2;
    u16 l_4;
    u16 l_6;
    u16 l_8;
    u16 l_A;
    u16 l_C;
    FUNC_TICK(28);
    if (((i16)g_minute) <= 5) {
        ax = clock_hour();
    } else {
        ax = clock_hour() + 1;
    }
    l_4 = ax;
    l_2 = 0;
    l_1 = g_ai_player((u16)(i8)g_ai_cur);
    if (l_4 == 0) {
        l_4 = 1;
    }
    l_C = l_4;
    ax = l_4;
    while (((i16)l_C) <= 14) {
        LOOP_TICK(59);
        ax = l_C << 1;
        if ((schedule_program((u16)(i8)l_1, l_C)) == 0) {
            if (((i16)l_C) > 7) {
                ax = 7;
            } else {
                ax = 8;
            }
            l_6 = ax;
            l_A = l_C;
            l_8 = 0;
            while (!((schedule_program((u16)(i8)l_1, l_A)) != 0 || l_A == l_6 || ((i16)l_A) > 14)) {
                LOOP_TICK(20);
                l_A = l_A + 1;
                l_8 = l_8 + 1;
            }
            ax = (u16)(i8)((u8)a2);
            if (((i16)((u16)(i8)((u8)a2))) <= ((i16)l_8)) {
                HP8(a0, (u16)(i8)l_2) = (u8)((((u8)((u8)(ax >> 8))) << 8) | ((u8)((u8)l_C)));
                l_2 = l_2 + 1;
                ax = (((u8)((u8)(ax >> 8))) << 8) | ((u8)((u8)l_C));
            }
        }
        l_C = l_C + 1;
    }
    ax = (((u8)((u8)(ax >> 8))) << 8) | l_2;
    return ax;
}

/* 348B:0203  1 if every AI ad contract is fully booked (flag bit2) or booked for today and tomorrow (bits 0,1) */
u16 ai_contracts_all_booked(void) {
    u16 ax;
    u32 l_4;
    u16 l_6;
    u8 l_7;
    u8 l_8;
    u8 l_9;
    FUNC_TICK(22);
    l_9 = 1;
    l_8 = g_ai_player((u16)(i8)g_ai_cur);
    l_6 = g_player_ad_count((u16)(i8)l_8);
    l_7 = 0;
    for (;;) {
        LOOP_TICK(45);
        ax = (u16)(i8)l_7;
        if (((i16)((u16)(i8)l_7)) >= ((i16)l_6)) break;
        l_4 = ai_ad_plan_at((u16)(i8)l_8, (u16)(i8)l_7);
        if ((((u16)(((i16)(FP16(l_4, 6) << 0xF)) >> 0xF)) == 0 || ((u16)(((i16)(FP16(l_4, 6) << 0xE)) >> 0xF)) == 0) && ((u16)(((i16)(FP16(l_4, 6) << 0xD)) >> 0xF)) == 0) {
            l_9 = 0;
        }
        l_7 = l_7 + 1;
    }
    ax = (((u8)((u8)(ax >> 8))) << 8) | l_9;
    return ax;
}

/* 348B:02A3  for unfinished contracts compute the average minimum audience and an OR of target-genre bitmasks into g_ai_ad_req[player] */
void ai_ad_requirements(void) {
    u16 ax;
    u8 *el;
    u16 t6;
    u16 t13;
    u16 t14;
    u16 t15;
    u32 t22;
    u32 t32;
    u8 fr_[52] = {0};                  /* bytes bp-52 .. bp-1 of the original frame */
    FUNC_TICK(79);
    FR8(fr_, 52, 0x32) = g_ai_player((u16)(i8)g_ai_cur);
    FR16(fr_, 52, 0x2A) = g_player_ad_count((u16)(i8)FR8(fr_, 52, 0x32));
    FR16(fr_, 52, 0x26) = 0;
    FR16(fr_, 52, 0x28) = 0;
    FR16(fr_, 52, 0x22) = 0;
    FR16(fr_, 52, 0x24) = 0;
    FR16(fr_, 52, 0xA) = 0;
    if (FR16(fr_, 52, 0x2A) != 0) {
        FR16(fr_, 52, 0x30) = 0;
        while (((i16)FR16(fr_, 52, 0x30)) < ((i16)FR16(fr_, 52, 0x2A))) {
            LOOP_TICK(116);
            el = (fr_ + 52 - 0x20 + (int)(i16)((FR16(fr_, 52, 0x30) << 1) + 0));   /* the array element: a far pointer in the frame (FR32(fr_, 52, 0x8)) in the original */
            t6 = contracts((u16)(i8)FR8(fr_, 52, 0x32), FR16(fr_, 52, 0x30)).ad;
            FR16(fr_, 52, 0x2C) = contracts((u16)(i8)FR8(fr_, 52, 0x32), FR16(fr_, 52, 0x30)).ad;
            FR16(fr_, 52, 0x2) = FPSEG(ads_at(t6));
            FR16(fr_, 52, 0x4) = FPOFF(ads_at(t6));
            if (FR16(fr_, 52, 0x2C) != 0 && ((i8)(ai_ad_plan((u16)(i8)FR8(fr_, 52, 0x32), FR16(fr_, 52, 0x30)).booked_total)) < ((i8)(FP8(FR32(fr_, 52, 0x4), 9)))) {
                t13 = G16(((((u16)(i8)(FP8(FR32(fr_, 52, 0x4), 0xA))) - 1) << 2) + 0xAA);
                t14 = FR16(fr_, 52, 0x24);
                t15 = G16(((((u16)(i8)(FP8(FR32(fr_, 52, 0x4), 0xA))) - 1) << 2) + 0xA8);
                FR16(fr_, 52, 0x24) = FR16(fr_, 52, 0x24) + (G16(((((u16)(i8)(FP8(FR32(fr_, 52, 0x4), 0xA))) - 1) << 2) + 0xA8));
                FR16(fr_, 52, 0x22) = (FR16(fr_, 52, 0x22) + t13) + ((u32)t14 + t15 > 0xFFFF);
                HP16(el, 0) = 0;
                FR16(fr_, 52, 0x2E) = 1;
                while (((i16)FR16(fr_, 52, 0x2E)) <= 3) {
                    LOOP_TICK(29);
                    if ((FP8(FR32(fr_, 52, 0x4), FR16(fr_, 52, 0x2E) + 0xB)) != 0) {
                        if (((i8)(FP8(FR32(fr_, 52, 0x4), FR16(fr_, 52, 0x2E) + 0xB))) >= 0) {
                            ax = (((u8)((u8)(ax >> 8))) << 8) | 0;
                        } else {
                            ax = (((u8)((u8)(ax >> 8))) << 8) | 1;
                        }
                        FR8(fr_, 52, 0x31) = (u8)ax;
                    }
                    t22 = rtl_abs((u16)(i8)(FP8(FR32(fr_, 52, 0x4), FR16(fr_, 52, 0x2E) + 0xB)));
                    bit_set(el, ((u16)(i8)((u8)LO16(t22))) - 1);
                    FR16(fr_, 52, 0x2E) = FR16(fr_, 52, 0x2E) + 1;
                }
                if ((HP16(el, 0)) == 0) {
                    HP16(el, 0) = 0xFFFF;
                } else {
                    if (FR8(fr_, 52, 0x31) == 0) {
                        ax = HP16(el, 0);
                    } else {
                        ax = ~(HP16(el, 0));
                    }
                    HP16(el, 0) = ax;
                }
            } else if (FR16(fr_, 52, 0x2C) != 0) {
                HP16(el, 0) = 0;
            }
            FR16(fr_, 52, 0x30) = FR16(fr_, 52, 0x30) + 1;
        }
        FR16(fr_, 52, 0x2E) = 0;
        while (((i16)FR16(fr_, 52, 0x2E)) < ((i16)FR16(fr_, 52, 0x2A))) {
            LOOP_TICK(10);
            FR16(fr_, 52, 0xA) = FR16(fr_, 52, 0xA) | (FRX16(fr_, 52, 0x20, (FR16(fr_, 52, 0x2E) << 1) + 0));
            FR16(fr_, 52, 0x2E) = FR16(fr_, 52, 0x2E) + 1;
        }
        if (FR16(fr_, 52, 0x2A) == 0) {
            FR16(fr_, 52, 0x26) = 0;
            FR16(fr_, 52, 0x28) = 0;
        } else {
            t32 = (u32)((i32)FR32(fr_, 52, 0x24) / (i32)MK32(((i16)FR16(fr_, 52, 0x2A)) >> 0xF, FR16(fr_, 52, 0x2A)));
            FR16(fr_, 52, 0x26) = HI16(t32);
            FR16(fr_, 52, 0x28) = LO16(t32);
        }
    } else {
        FR16(fr_, 52, 0x26) = 0;
        FR16(fr_, 52, 0x28) = 0;
        FR16(fr_, 52, 0xA) = 0xFFFF;
    }
    G32((u16)((((u16)(i8)FR8(fr_, 52, 0x32)) * 6) + 0x7764) + 2) = FR32(fr_, 52, 0x28);
    g_ai_ad_req(((u16)(i8)FR8(fr_, 52, 0x32)) * 3) = FR16(fr_, 52, 0xA);
    return;
}

/* 348B:04CD  (player, amount i32, contract): split income into the 7 AI budget pots by their percentages; nothing if contract >= 0 is not yet fulfilled */
void ai_budget_distribute(u16 a0, u16 a1, u16 a2, u16 a3) {
    u32 l_4;
    u8 l_5;
    FUNC_TICK(58);
    l_4 = ai_at((u16)(i8)((u8)a0));
    if (!(((i16)a3) < 0 || ((i8)((u8)((((u8)((u8)(((u16)(a3 * 7)) >> 8))) << 8) | (contracts((u16)(i8)((u8)a0), a3).aired)))) >= ((i8)(ads(contracts((u16)(i8)((u8)a0), a3).ad).spots)))) {
        a2 = 0;
        a1 = 0;
    }
    l_5 = 0;
    while (((i8)l_5) <= 6) {
        LOOP_TICK(32);
        FP32(l_4, ((u16)(i8)l_5) << 2) = (FP32(l_4, ((u16)(i8)l_5) << 2)) + ((u32)((i32)((u32)(MK32(((i16)((u16)(i8)(FP8(l_4, ((u16)(i8)l_5) + 0x1C)))) >> 0xF, (u16)(i8)(FP8(l_4, ((u16)(i8)l_5) + 0x1C))) * MK32(a2, a1))) / (i32)0x64));
        l_5 = l_5 + 1;
    }
    return;
}

/* 348B:059F  game start for both AIs: clear pots/percentages, strategy 3, film budget = money*4/5, transmitter budget = 49500, money = money/5 - 49500, then distribute */
void ai_init_players(void) {
    u32 l_4;
    u32 l_8;
    u16 l_A;
    u16 l_C;
    u16 l_E;
    u32 t11;
    u32 t13;
    FUNC_TICK(9);
    l_E = 0;
    while (((i16)l_E) <= 1) {
        LOOP_TICK(117);
        if (g_ai_player(l_E) == 0) { l_E = l_E + 1; continue; }   /* multiplayer: a human's slot */
        l_A = (u16)(i8)g_ai_player(l_E);
        l_8 = ai_at(l_A);
        l_4 = players_at(l_A);
        l_C = 0;
        while (((i16)l_C) <= 6) {
            LOOP_TICK(9);
            FP8(l_8, l_C + 0x1C) = 0;
            FP8(l_8, l_C + 0x23) = 1;
            l_C = l_C + 1;
        }
        g_ai_studio(l_A * 3) = 0;
        g_ai_studio((l_A * 3) + 1) = 0;
        l_C = 0;
        while (((i16)l_C) <= 3) {
            LOOP_TICK(16);
            G32((((l_A * 0x3C) + 0x6E29) + (l_C << 2)) + 0x2C) = 0;
            l_C = l_C + 1;
        }
        g_ai_period(l_E) = 0;
        AT(ai_budget, l_8).image_rank = 3;
        t11 = (u32)((i32)((2 >= 32 ? 0u : (u32)AT(player, l_4).money << 2)) / (i32)5);
        AT(ai_budget, l_8).films = t11;
        AT(ai_budget, l_8).transmitters = 0xC15C;
        t13 = ((u32)((i32)AT(player, l_4).money / (i32)5)) + 0xFFFF3EA4;
        AT(player, l_4).money = t13;
        ai_plan_budget(l_A);
        ai_budget_distribute(l_A, FP16(l_4, 8), FP16(l_4, 0xA), 0xFFFF);
        l_E = l_E + 1;
    }
    return;
}

/* 348B:070C  (hour, contract, add(1)/remove(0), player): adjust booked counters today/tomorrow/total of g_ai_ad_plan and update flags (bit0 today>=plan, bit1 tomorrow>=plan, bit2 total>=required spots) */
void ad_plan_book(u16 a0, u16 a1, u16 a2, u16 a3) {
    u16 ax;
    u32 l_4;
    u8 l_5;
    FUNC_TICK(86);
    if (((u8)((u8)a2)) == 0) {
        ax = (((u8)((u8)(ax >> 8))) << 8) | 0xFF;
    } else {
        ax = (((u8)((u8)(ax >> 8))) << 8) | 1;
    }
    l_5 = (u8)ax;
    l_4 = ai_ad_plan_at((u16)(i8)((u8)a3), a1);
    if (((i16)a0) > 7) {
        AT(ai_ad, l_4).booked_tomorrow = (AT(ai_ad, l_4).booked_tomorrow) + l_5;
    } else {
        AT(ai_ad, l_4).booked_today = (AT(ai_ad, l_4).booked_today) + l_5;
    }
    AT(ai_ad, l_4).booked_total = (AT(ai_ad, l_4).booked_total) + l_5;
    if (((i8)(AT(ai_ad, l_4).booked_today)) < ((i8)(AT(ai_ad, l_4).today))) {
        FP16(l_4, 6) = FP16(l_4, 6) & 0xFFFE;
    } else {
        FP16(l_4, 6) = FP16(l_4, 6) | 1;
    }
    if (((i8)(AT(ai_ad, l_4).booked_tomorrow)) < ((i8)(AT(ai_ad, l_4).tomorrow))) {
        FP16(l_4, 6) = FP16(l_4, 6) & 0xFFFD;
    } else {
        FP16(l_4, 6) = FP16(l_4, 6) | 2;
    }
    if (((i8)(ads(contracts((u16)(i8)((u8)a3), a1).ad).spots)) > ((i8)(AT(ai_ad, l_4).booked_total))) {
        FP16(l_4, 6) = FP16(l_4, 6) & 0xFFFB;
    } else {
        FP16(l_4, 6) = FP16(l_4, 6) | 4;
    }
    return;
}

/* 348B:07FF  init the AI booking plan of contract slot a0 (spots per day from AI heuristics, counters cleared) */
void ai_ad_plan_init(u16 a0) {
    u32 l_4;
    u8 l_5;
    u16 t1;
    u16 t2;
    FUNC_TICK(55);
    l_5 = g_ai_player((u16)(i8)g_ai_cur);
    l_4 = ai_ad_plan_at((u16)(i8)l_5, a0);
    t1 = contract_spots_per_day(a0, (u16)(i8)l_5);
    AT(ai_ad, l_4).today = (u8)t1;
    t2 = contract_spots_per_day(a0, (u16)(i8)l_5);
    AT(ai_ad, l_4).tomorrow = (u8)t2;
    AT(ai_ad, l_4).booked_today = 0;
    AT(ai_ad, l_4).booked_tomorrow = 0;
    AT(ai_ad, l_4).booked_total = 0;
    FP16(l_4, 6) = FP16(l_4, 6) & 0xFFFE;
    FP16(l_4, 6) = FP16(l_4, 6) & 0xFFFD;
    FP16(l_4, 6) = FP16(l_4, 6) & 0xFFFB;
    return;
}

/* 348B:0895  average quality of a player's library films (+series rating); ?loops count+1 entries per genre */
u16 ai_library_avg_quality(u16 a0) {
    u16 ax;
    u16 l_2;
    u16 l_4;
    u16 l_6;
    u32 l_A;
    u8 l_B;
    u8 l_C;
    u8 l_D;
    u16 t14;
    FUNC_TICK(31);
    l_A = library_at((u16)(i8)((u8)a0));
    l_4 = 0;
    l_2 = 0;
    l_D = 0;
    /* ORIGINAL BUG (fix "ai-lib-quality", 348B:0918 / 348B:0961 jge): the
     * slot loops run to count inclusive, and over genre 0 too: 12 phantom
     * entries per call (sum / (n + 12)), the slot past a full genre is the
     * next genre's first programme, past a full series shelf the count
     * bytes.  ai_plan_budget's "worst quality" flag then follows library
     * size.  Fixed: slots 0..count-1 (genre 0 is always empty). */
    int past = fix_on(FIX_AI_LIB_QUALITY) ? 0 : 1;   /* classic: slot count too */
    while (((i8)l_D) <= 11) {
        LOOP_TICK(65);
        if (((i8)l_D) > 10) {
            l_C = 0;
            while (((i8)(FP8(l_A, ((u16)(i8)l_D) + 0x60))) + past > ((i8)l_C)) {
                LOOP_TICK(26);
                l_6 = FP16(l_A, (((u16)(i8)l_D) << 3) + (((u16)(i8)l_C) << 1));
                l_4 = l_4 + series_avg_topicality(l_6);
                l_2 = l_2 + 1;
                l_C = l_C + 1;
            }
        } else {
            l_C = 0;
            while (((i8)(FP8(l_A, ((u16)(i8)l_D) + 0x60))) + past > ((i8)l_C)) {
                LOOP_TICK(30);
                l_6 = FP16(l_A, (((u16)(i8)l_D) << 3) + (((u16)(i8)l_C) << 1));
                l_4 = l_4 + ((u16)(i8)(films(l_6).topicality));
                l_2 = l_2 + 1;
                l_C = l_C + 1;
            }
        }
        l_D = l_D + 1;
    }
    if (l_2 == 0) {
        l_2 = 1;
    }
    t14 = (u16)((i32)(i16)l_4 / (i16)l_2);
    l_B = (u8)t14;
    ax = (((u8)((u8)(t14 >> 8))) << 8) | l_B;
    return ax;
}

/* 348B:098D  pick the free studio with the best size/rent ratio under 10% of the production budget; ORIGINAL BUG (fix "ai-studio"): passes the loop counter (13) instead of the chosen studio to studio_request_rent */
void ai_rent_studio(void) {
    u16 ax;
    u32 l_4;
    u16 l_6;
    u32 l_A;
    u32 l_E;
    u32 l_12;
    u32 l_16;
    u8 l_17;
    u8 l_18;
    FUNC_TICK(58);
    l_18 = g_ai_player((u16)(i8)g_ai_cur);
    l_16 = 0;
    l_12 = 0;
    l_E = 0;
    l_A = 0;
    l_6 = 0;
    l_A = ((u32)((i32)ai((u16)(i8)l_18).studio_avg / (i32)0xA));
    l_17 = 1;
    while (((i8)l_17) <= 12) {
        LOOP_TICK(47);
        l_4 = studios_at((u16)(i8)l_17);
        if ((AT(studio, l_4).renter) == 0) {
            l_E = AT(studio, l_4).rent;
            l_16 = ((u32)((i32)l_E / (i32)MK32(((i16)((u16)(i8)(AT(studio, l_4).size))) >> 0xF, (u16)(i8)(AT(studio, l_4).size))));
            ax = LO16(l_E);
            if (((i32)l_E) <= ((i32)l_A)) {
                ax = LO16(l_16);
                if (((i32)l_16) > ((i32)l_12)) {
                    l_12 = l_16;
                    l_6 = (u16)(i8)l_17;
                }
            }
        }
        l_17 = l_17 + 1;
    }
    if (l_6 != 0) {
        /* ORIGINAL BUG (fix "ai-studio"): l_17 is the loop counter, 13 after
         * the loop; "studio 13" is g_prop_prices (DS:6235), so the computer
         * player pays three times a "rent" read from the props price table,
         * raises it by half there and never gets a studio.  Fixed: the
         * studio it chose. */
        studio_request_rent((u16)(i8)l_18, fix_on(FIX_AI_STUDIO) ? l_6 : (u16)(i8)l_17, (u16)(i8)l_18);
    }
    ai_next_goal();
    return;
}

/* 348B:0AAF  buy the best-rated screenplay offer (rating >= threshold, price <= 30% of production budget, genre not over-full) into the AI's script list; pays, marks offer sold, refills the agency */
void ai_buy_script(void) {
    u16 ax;
    u32 l_4;
    u32 l_8;
    u16 l_A;
    u16 l_C;
    u32 l_10;
    u32 l_14;
    u8 l_15;
    u8 l_16;
    u8 l_17;
    u8 l_18;
    u8 l_19;
    u8 l_1A;
    u16 t13;
    FUNC_TICK(199);
    l_1A = g_ai_player((u16)(i8)g_ai_cur);
    l_18 = 0;
    l_17 = 0;
    l_16 = 0;
    l_10 = owned_scripts_at((u16)(i8)l_1A, g_scripts_owned_count((u16)(i8)l_1A));
    l_C = 0;
    l_A = 0;
    l_8 = 0;
    l_8 = ((u32)((i32)((u32)(ai((u16)(i8)l_1A).production * 3)) / (i32)0xA));
    l_19 = 1;
    while (((i8)l_19) <= 12) {
        LOOP_TICK(27);
        l_4 = studios_at((u16)(i8)l_19);
        if (!((AT(studio, l_4).renter) != l_1A || (AT(studio, l_4).running) == 0 || ((i8)(AT(studio, l_4).size)) <= ((i8)l_16))) {
            l_16 = AT(studio, l_4).size;
        }
        l_19 = l_19 + 1;
    }
    l_19 = 1;
    while (((i8)l_19) <= 5) {
        LOOP_TICK(81);
        l_14 = script_offers_at((u16)(i8)l_19);
        if ((AT(script_offer, l_14).status) == 0) {
            l_C = AT(script_offer, l_14).script;
            if (((u16)(((i16)((screenplays(l_C).flags_16) << 0xF)) >> 0xF)) == 0) {
                ax = (((u8)((u8)(((u16)((screenplays(l_C).target) * 0x17)) >> 8))) << 8) | (films(screenplays(l_C).target).genre);
            } else {
                ax = (((u8)((u8)(ax >> 8))) << 8) | 0xB;
            }
            l_15 = (u8)ax;
            ax = FP16(l_14, 4);
            if (((i32)(AT(script_offer, l_14).price)) <= ((i32)l_8)) {
                if (((i8)(library_count((u16)(i8)l_1A, (u16)(i8)l_15))) <= 3) {
                    l_18 = (u8)script_avg_quality(l_C);
                    if (!(((i8)l_18) < ((i8)l_17) || ((i8)(g_ai_script_quality((u16)(i8)l_1A))) > ((i8)l_18))) {
                        l_17 = l_18;
                        l_A = (u16)(i8)l_19;
                    }
                }
            }
        }
        l_19 = l_19 + 1;
    }
    if (l_A != 0) {
        AT(owned_script, l_10).script = script_offers(l_A).script;
        AT(owned_script, l_10).aux = script_offers(l_A).aux;
        AT(owned_script, l_10).episode = 1;
        g_scripts_owned_count((u16)(i8)l_1A) = (g_scripts_owned_count((u16)(i8)l_1A)) + 1;
        players((u16)(i8)l_1A).money = (players((u16)(i8)l_1A).money) - (script_offers(l_A).price);
        ai((u16)(i8)l_1A).production = (ai((u16)(i8)l_1A).production) - (script_offers(l_A).price);
        t13 = ((((u16)(i8)l_1A) * 0x3C) + 0x6E29) + (((u16)(i8)(g_ai_period((u16)(i8)g_ai_cur))) << 2);
        G32(t13 + 0x2C) = (G32((((((u16)(i8)l_1A) * 0x3C) + 0x6E29) + (((u16)(i8)(g_ai_period((u16)(i8)g_ai_cur))) << 2)) + 0x2C)) - (script_offers(l_A).price);
        ai_avg_studio_budget((u16)(i8)l_1A);
        script_offers(l_A).status = (script_offers(l_A).status) - 0x64;
        l_19 = 1;
        while (((i8)l_19) <= 10) {
            LOOP_TICK(24);
            if (((i8)(script_offers((u16)(i8)l_19).status)) < 0) {
                mem_zero(script_offers_at((u16)(i8)l_19), 9);
            }
            l_19 = l_19 + 1;
        }
        scriptagency_refill();
    }
    ai_next_goal();
    return;
}

/* 348B:0DF7  start production in the AI's studio if not running (films only if not already in the library) */
void ai_start_production(void) {
    u16 ax;
    u16 l_2;
    u16 l_4;
    u32 l_8;
    u16 l_A;
    u8 l_B;
    FUNC_TICK(110);
    l_B = g_ai_player((u16)(i8)g_ai_cur);
    l_A = (u16)(i8)(g_ai_studio(((u16)(i8)l_B) * 3));
    l_8 = screenplays_at(studios(l_A).script);
    l_4 = studios(l_A).script;
    if (((u16)(((i16)((AT(screenplay, l_8).flags_16) << 0xF)) >> 0xF)) != 0) {
        ax = ((u16)(i8)(series(AT(screenplay, l_8).target).genre)) + 0xA;
    } else {
        ax = (u16)(i8)(films(AT(screenplay, l_8).target).genre);
    }
    l_2 = ax;
    if (!((studios(l_A).running) != 0 || !(((u16)(((i16)((AT(screenplay, l_8).flags_16) << 0xF)) >> 0xF)) != 0 || (library_contains((u16)(i8)l_B, l_2, AT(screenplay, l_8).target)) == 0))) {
        studio_start_production((u16)(i8)(g_ai_studio(((u16)(i8)l_B) * 3)));
    }
    ai_next_goal();
    return;
}

/* 348B:0F05  put a waiting script into an own empty studio (if size fits) or continue the current one, then issue the next shopping list */
void ai_next_shopping_list(void) {
    u32 l_4;
    u16 l_6;
    u16 l_8;
    u16 l_A;
    u8 l_B;
    u8 l_C;
    u16 t3;
    FUNC_TICK(55);
    l_C = g_ai_player((u16)(i8)g_ai_cur);
    l_B = 0;
    l_A = 0;
    l_8 = 0;
    if ((g_scripts_owned_count((u16)(i8)l_C)) != 0) {
        l_8 = G16(((((u16)(i8)l_C) * 0x32) + 0x1A45) + (((g_scripts_owned_count((u16)(i8)l_C)) - 1) * 5));
    }
    l_6 = 1;
    while (!(((i16)l_6) > 12 || l_B != 0)) {
        LOOP_TICK(95);
        l_4 = studios_at(l_6);
        if ((AT(studio, l_4).renter) == l_C) {
            if ((AT(studio, l_4).end_day) != 0 || (AT(studio, l_4).script) == 0) {
                if ((AT(studio, l_4).script) == 0 && l_8 != 0) {
                    t3 = script_count_scenes(l_8);
                    if (((i16)t3) <= ((i16)((u16)(i8)(AT(studio, l_4).size)))) {
                        l_A = l_6;
                        AT(studio, l_4).script = l_8;
                        AT(studio, l_4).episode = G8((((((u16)(i8)l_C) * 0x32) + 0x1A45) + (((g_scripts_owned_count((u16)(i8)l_C)) - 1) * 5)) + 3);
                        AT(studio, l_4).episode_last = (AT(studio, l_4).episode) - 1;
                        AT(studio, l_4).end_day = 0;
                        script_list_delete((u16)(i8)l_C, (g_scripts_owned_count((u16)(i8)l_C)) - 1);
                        l_B = 1;
                    }
                }
            } else {
                l_A = l_6;
                l_8 = AT(studio, l_4).script;
                l_B = 1;
            }
        }
        l_6 = l_6 + 1;
    }
    if (l_B == 1) {
        studio_new_shopping_list(l_A);
    }
    ai_next_goal();
    return;
}

/* 348B:1087  for each cast slot of the current shopping list spend (weight% of 60% of the production budget): best-rated affordable actor of the role's category, or most expensive affordable scene/equipment level (1..3) */
void ai_buy_cast(void) {
    u16 ax;
    u16 bx;
    u16 es;
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
    u32 l_20;
    u8 l_21;
    u8 l_22;
    u8 l_23;
    u8 l_24;
    u8 l_25;
    u8 l_26;
    u16 t6;
    u32 t12;
    u16 t13;
    u32 t14;
    u16 t17;
    u32 t18;
    u16 t21;
    u16 t24;
    u32 t32;
    u32 t35;
    u16 t40;
    FUNC_TICK(151);
    l_26 = g_ai_player((u16)(i8)g_ai_cur);
    l_20 = screenplays_at(studios((u16)(i8)(g_ai_studio(((u16)(i8)l_26) * 3))).script);
    l_1A = 0;
    l_16 = 0;
    l_14 = 0;
    l_12 = 0;
    l_4 = 0;
    l_C = ((u32)((i32)((u32)(ai((u16)(i8)l_26).production * 0x3C)) / (i32)0x64));
    if (((u16)(((i16)((AT(screenplay, l_20).flags_16) << 0xF)) >> 0xF)) == 0) {
        l_1C = ((u16)(i8)(studios((u16)(i8)(g_ai_studio(((u16)(i8)l_26) * 3))).episode)) - 1;
    } else {
        l_1C = ((u16)(i8)(studios((u16)(i8)(g_ai_studio(((u16)(i8)l_26) * 3))).episode)) - 1;
    }
    l_10 = MK32(FPSEG(l_20), (FPOFF(l_20) + (l_1C * 0x23)) + 8);
    l_25 = 1;
    while (((i8)l_25) <= 10) {
        LOOP_TICK(20);
        if ((FP8(l_10, (((u16)(i8)l_25) * 3) + 1)) != 0) {
            l_1A = l_1A + ((u16)(i8)(FP8(l_10, (((u16)(i8)l_25) * 3) + 2)));
        }
        l_25 = l_25 + 1;
    }
    if (l_1A == 0) {
        l_1A = 1;
    }
    l_25 = 1;
    while (((i8)l_25) <= 10) {
        LOOP_TICK(457);
        t6 = (((u8)((u8)(((u16)(((u16)(i8)l_25) * 3)) >> 8))) << 8) | (FP8(l_10, (((u16)(i8)l_25) * 3) + 1));
        l_24 = FP8(l_10, (((u16)(i8)l_25) * 3) + 1);
        if (((u8)((u8)t6)) != 0) {
            l_23 = FP8(l_20, ((u16)(i8)l_24) + 0x1A4);
            if (((i8)l_24) <= 15 && l_23 != 0) {
                l_18 = (u16)((u16)((i32)(i16)(((u16)(i8)(FP8(l_10, (((u16)(i8)l_25) * 3) + 2))) * 0x64) / (i16)l_1A));
                t12 = (u32)((i32)((u32)(MK32(((i16)l_18) >> 0xF, l_18) * l_C)) / (i32)0x64);
                l_8 = t12;
                ax = LO16(t12);
                if (((i8)l_23) < 8) {
                    l_22 = 0;
                    while (((i8)l_22) <= 15) {
                        LOOP_TICK(87);
                        t13 = G16(((((((u16)(i8)l_23) - 1) * 0xC0) + 0x5013) + (((u16)(i8)l_22) * 0xC)) + 4);
                        ax = ((u16)(i8)l_22) * 0xC;
                        bx = (((((u16)(i8)l_23) - 1) * 0xC0) + 0x5013) + (((u16)(i8)l_22) * 0xC);
                        es = DSEG;
                        if (((i16)(G16(((((((u16)(i8)l_23) - 1) * 0xC0) + 0x5013) + (((u16)(i8)l_22) * 0xC)) + 4))) >= 0 && (((i16)t13) > 0 || MEM16(es, bx + 2) > 0)) {
                            t14 = (u32)(MK32(((i16)((u16)(i8)FP8(l_10, 2))) >> 0xF, (u16)(i8)FP8(l_10, 2)) * G32(((((((u16)(i8)l_23) - 1) * 0xC0) + 0x5013) + (((u16)(i8)l_22) * 0xC)) + 2));
                            l_4 = t14;
                            ax = LO16(t14);
                            if (((i32)t14) <= ((i32)l_8)) {
                                t17 = script_role_match_pct(studios((u16)(i8)(g_ai_studio(((u16)(i8)l_26) * 3))).script, l_1C + 1, (u16)(i8)l_25, (u16)(i8)l_24);
                                l_16 = t17;
                                ax = t17;
                                if (((i16)t17) > ((i16)l_14)) {
                                    l_14 = l_16;
                                    l_12 = (u16)(i8)l_22;
                                    ax = (u16)(i8)l_22;
                                }
                            }
                        }
                        l_22 = l_22 + 1;
                    }
                } else {
                    l_22 = 0;
                    while (((i8)l_22) <= 15) {
                        LOOP_TICK(92);
                        l_21 = 0;
                        while (((i8)l_21) <= 6) {
                            LOOP_TICK(87);
                            ax = (G16((((((u16)(i8)l_21) * 0xC0) + 0x5013) + (((u16)(i8)l_22) * 0xC)) + 2)) | (G16((((((u16)(i8)l_21) * 0xC0) + 0x5013) + (((u16)(i8)l_22) * 0xC)) + 4));
                            if (((G16((((((u16)(i8)l_21) * 0xC0) + 0x5013) + (((u16)(i8)l_22) * 0xC)) + 2)) | (G16((((((u16)(i8)l_21) * 0xC0) + 0x5013) + (((u16)(i8)l_22) * 0xC)) + 4))) != 0 && l_21 != 4) {
                                t18 = (u32)(MK32(((i16)((u16)(i8)FP8(l_10, 2))) >> 0xF, (u16)(i8)FP8(l_10, 2)) * actors((u16)(i8)l_21, (u16)(i8)l_22).fee);
                                l_4 = t18;
                                ax = LO16(t18);
                                if (((i32)t18) <= ((i32)l_8)) {
                                    t21 = script_role_match_pct(studios((u16)(i8)(g_ai_studio(((u16)(i8)l_26) * 3))).script, l_1C + 1, (u16)(i8)l_25, (u16)(i8)l_24);
                                    l_16 = t21;
                                    ax = t21;
                                    if (((i16)t21) > ((i16)l_14)) {
                                        l_14 = l_16;
                                        l_12 = (u16)(i8)l_22;
                                        l_23 = l_21;
                                        ax = (((u8)((u8)(((u16)((u16)(i8)l_22)) >> 8))) << 8) | l_21;
                                    }
                                }
                            }
                            l_21 = l_21 + 1;
                        }
                        l_22 = l_22 + 1;
                    }
                }
                if (l_14 != 0) {
                    FP8(l_10, (((u16)(i8)l_25) * 3) + 3) = (u8)((((u8)((u8)(ax >> 8))) << 8) | ((u8)((u8)l_12)));
                    players((u16)(i8)l_26).money = (players((u16)(i8)l_26).money) - l_4;
                    ai((u16)(i8)l_26).production = (ai((u16)(i8)l_26).production) - l_4;
                    t24 = ((((u16)(i8)l_26) * 0x3C) + 0x6E29) + (((u16)(i8)(g_ai_period((u16)(i8)g_ai_cur))) << 2);
                    G32(t24 + 0x2C) = (G32((((((u16)(i8)l_26) * 0x3C) + 0x6E29) + (((u16)(i8)(g_ai_period((u16)(i8)g_ai_cur))) << 2)) + 0x2C)) - l_4;
                    ai_avg_studio_budget((u16)(i8)l_26);
                }
                l_14 = 0;
                l_12 = 0;
            } else {
                l_18 = (u16)((u16)((i32)(i16)(((u16)(i8)(FP8(l_10, (((u16)(i8)l_25) * 3) + 2))) * 0x64) / (i16)l_1A));
                t32 = (u32)((i32)((u32)(MK32(((i16)l_18) >> 0xF, l_18) * l_C)) / (i32)0x64);
                l_8 = t32;
                l_22 = 1;
                ax = LO16(t32);
                while (((i8)l_22) <= 3) {
                    LOOP_TICK(35);
                    t35 = (u32)(MK32(((i16)((u16)(i8)FP8(l_10, 2))) >> 0xF, (u16)(i8)FP8(l_10, 2)) * G32((((((u16)(i8)l_24) + 0xFFF1) << 4) + 0x6235) + (((u16)(i8)l_22) << 2)));
                    l_4 = t35;
                    ax = LO16(t35);
                    if (((i32)t35) <= ((i32)l_8)) {
                        l_12 = (u16)(i8)l_22;
                        ax = (u16)(i8)l_22;
                    }
                    l_22 = l_22 + 1;
                }
                if (l_12 != 0) {
                    FP8(l_10, (((u16)(i8)l_25) * 3) + 3) = (u8)((((u8)((u8)(ax >> 8))) << 8) | ((u8)((u8)l_12)));
                    players((u16)(i8)l_26).money = (players((u16)(i8)l_26).money) - l_4;
                    ai((u16)(i8)l_26).production = (ai((u16)(i8)l_26).production) - l_4;
                    t40 = ((((u16)(i8)l_26) * 0x3C) + 0x6E29) + (((u16)(i8)(g_ai_period((u16)(i8)g_ai_cur))) << 2);
                    G32(t40 + 0x2C) = (G32((((((u16)(i8)l_26) * 0x3C) + 0x6E29) + (((u16)(i8)(g_ai_period((u16)(i8)g_ai_cur))) << 2)) + 0x2C)) - l_4;
                    ai_avg_studio_budget((u16)(i8)l_26);
                }
                l_12 = 0;
            }
        }
        l_25 = l_25 + 1;
    }
    ai_next_goal();
    return;
}
