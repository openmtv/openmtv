/* Computer players II: audience estimate, news buying, Betty gifts, transmitter bids.
 *
 * Original module 32BA of MTV.EXE (Rainbow Arts, 1991).  Decompiled by
 * tools/decomp.py and hand-maintained since the freeze (git tag
 * generated-final).  Every function keeps its original address in the
 * comment above it; that address ties it to the machine-level twin in gen/
 * and to the difftest tables -- keep it when editing.
 */
#include "game.h"
#include "seat.h"

/* 32BA:000A  (ai_idx, value i32): offer of an AI for a transmitter the human sells: -1 if no free transmitter slot or transmitter budget too small, else derived value * random pct (table 0x15F8)/100 */
u32 ai_transmitter_bid(u16 a0, u16 a1, u16 a2) {
    u16 ax;
    u16 di;
    u16 dx;
    u16 si;
    u32 rv_;
    u16 t1;
    u32 t4;
    u32 t5;
    u16 t6;
    u32 t8;
    u8 fr_[14] = {0};                  /* bytes bp-14 .. bp-1 of the original frame */
    if (g_ai_player(a0) == 0) return 0xFFFFFFFF;   /* multiplayer: a human's slot bids nothing */
    FUNC_TICK(96);
    struct_copy((fr_ + 14 - 0xC), HPTR(DSP(0x15F8)), 4);
    t1 = (u16)(i8)g_ai_player(a0);
    coverage_build((u16)(i8)g_ai_player(a0));
    t4 = (u32)((i32)((u32)(transmitter_new_reach(a1, a2) * 0x268)) / (i32)0xA);
    t5 = transmitter_fee_bracket(LO16(t4), HI16(t4));
    FR16(fr_, 14, 0x6) = HI16(t5);
    FR16(fr_, 14, 0x8) = LO16(t5);
    si = 3;
    di = t1;
    while (((contracts(5, ((di * 0x42) + (si * 2)) + 9).ad) | (G16((((di * 0x1CE) + 0x1D09) + (si * 0xE)) + 6))) != 0) {
        LOOP_TICK(17);
        if (((i16)si) >= 33) break;
        si = si + 1;
    }
    if (((i16)si) <= 32) {
        t6 = random_n(4, 1);
        t8 = (u32)((i32)((u32)(MK32(((i16)((u16)(i8)(FRX8(fr_, 14, 0xD, t6 + 0)))) >> 0xF, (u16)(i8)(FRX8(fr_, 14, 0xD, t6 + 0))) * FR32(fr_, 14, 0x8))) / (i32)0x64);
        FR16(fr_, 14, 0x2) = HI16(t8);
        FR16(fr_, 14, 0x4) = LO16(t8);
        ax = aibudget_transmitters_lo(di);
        if (((i32)(ai(di).transmitters)) < ((i32)FR32(fr_, 14, 0x4))) {
            FR16(fr_, 14, 0x2) = 0xFFFF;
            FR16(fr_, 14, 0x4) = 0xFFFF;
        }
    } else {
        FR16(fr_, 14, 0x2) = 0xFFFF;
        FR16(fr_, 14, 0x4) = 0xFFFF;
    }
    ax = FR16(fr_, 14, 0x4);
    dx = FR16(fr_, 14, 0x2);
    rv_ = MK32(dx, ax);
    return rv_;
}

/* 32BA:0124  AI news purchase: while news budget > 0, average slot quality below strategy threshold and < 3 swaps: replace the weakest of the 3 news slots by the best affordable inbox item (pays price), then recompute news quality */
void ai_buy_news(void) {
    u16 ax;
    int cc;
    u16 di;
    u16 si;
    u32 l_4;
    u16 l_6;
    u16 l_8;
    u16 l_A;
    u16 l_C;
    u16 l_E;
    u16 l_10;
    u16 t1;
    u16 t3;
    u16 t7;
    u16 t8;
    u16 t9;
    FUNC_TICK(47);
    t1 = (u16)(i8)(g_ai_player((u16)(i8)g_ai_cur));
    l_10 = 0;
    l_6 = g_ai_news_threshold((u16)(i8)(ai(t1).image_rank));
    di = t1;
    for (;;) {
        LOOP_TICK(196);
        t8 = actors(0x28, (di * 5) + 2).icon;
        t9 = actors(0x28, (di * 5) + 3).name;
        l_4 = ai(di).news;
        ax = t8;
        if (((i32)MK32(t9, t8)) <= 0) break;
        if ((ai_news_good_enough(di, l_6)) != 0) break;
        cc = ((i16)g_player_news_count(di)) <= 0;
        if (cc || ((i16)l_10) >= 3) break;
        l_C = 0;
        l_E = 0x96;
        si = 1;
        while (((i16)si) < 4) {
            LOOP_TICK(31);
            if (((i16)(news_pool(((di * 4) + si) + 0x38).value)) < ((i16)l_E)) {
                l_C = si;
                l_E = news_pool(((di * 4) + si) + 0x38).value;
            }
            si = si + 1;
        }
        l_8 = 0xFFFF;
        l_A = 0;
        si = 0;
        while (((i16)g_player_news_count(di)) > ((i16)si)) {
            LOOP_TICK(54);
            t3 = news_price((u16)(i8)(inbox(di, si).price_class));
            ax = t3;
            if (((i32)(MK32(((i16)t3) >> 0xF, t3))) < ((i32)l_4)) {
                if (((i16)(inbox(di, si).quality)) > ((i16)l_A)) {
                    l_8 = si;
                    l_A = inbox(di, si).quality;
                }
            }
            si = si + 1;
        }
        if (!(l_C != 0 && ((i16)l_8) >= 0 && ((i16)l_A) > ((i16)l_E))) break;
        t7 = news_price((u16)(i8)(inbox(di, l_8).price_class));
        ai(di).news = (ai(di).news) - (MK32(((i16)t7) >> 0xF, t7));
        far_memcpy(HPTR(news_pool_at(((di * 4) + l_C) + 0x38)), HPTR(inbox_at(di, l_8)), 0x1E);
        news_inbox_delete(di, l_8);
        l_10 = l_10 + 1;
    }
    news_rate_player(di);
    return;
}

/* 32BA:036D  (player, threshold): 1 if average quality of the 3 broadcast news slots >= threshold */
u16 ai_news_good_enough(u16 a0, u16 a1) {
    u16 ax;
    u16 di;
    u16 si;
    u16 t2;
    FUNC_TICK(22);
    si = 1;
    di = 0;
    while (((i16)si) < 4) {
        LOOP_TICK(16);
        t2 = di + (news_pool(((a0 * 4) + si) + 0x38).value);
        si = (u16)(si + 1);
        di = t2;
    }
    if (((i16)(u16)((u16)((i32)(i16)di / 3))) < ((i16)a1)) {
        ax = 0;
    } else {
        ax = 1;
    }
    return ax;
}

/* 32BA:03BC  (hour, program, mode) audience model: per station attract = film topicality (series: episode quality / repeats so far today); x trend bonus (g_trends mult/10 if genre in AI's known trends); stations sharing a genre split the genre audience g_genre_audience[g][h]; attract = attract*g_genre_audience/100 + news lead-in/5; total capped at 100%; viewers = share*reach/100*2464. mode 0 share%, 1 viewers, 2 rank (1..3); X-rated film counts 0 if hour%7<4; uses measured g_ratings for the running hour */
u32 ai_estimate_audience(u16 a0, u16 a1, u16 a2) {
    u16 ax;
    int cc;
    u16 di;
    u16 dx;
    u16 si;
    u32 rv_;
    u16 t29;
    u16 t65;
    u16 t95;
    u16 t98;
    u16 t101;
    u8 fr_[88] = {0};                  /* bytes bp-88 .. bp-1 of the original frame */
    FUNC_TICK(159);
    if (((i16)a0) > 7) {
        ax = 8;
    } else {
        ax = 1;
    }
    FR16(fr_, 88, 0x42) = ax;
    if (((i16)a0) > 7) {
        ax = a0 + 0xFFF9;
    } else {
        ax = a0;
    }
    FR16(fr_, 88, 0x40) = ax;
    FR8(fr_, 88, 0x2D) = g_ai_player((u16)(i8)g_ai_cur);
    if (((u32)MK32((clock_hour()), a1)) == ((u32)MK32(a0, 0))) {
        if (((i16)g_minute) < 5) {
L_044D:
            si = 1;
            while (((i16)si) <= 3) {
                LOOP_TICK(236);
                cc = ((u16)((u16)(i8)FR8(fr_, 88, 0x2D))) != si;
                if (cc || a1 == 0) {
                    di = a0;
                    while (schedule_program(si, di) == 0x270F) {
                        LOOP_TICK(13);
                        di = di - 1;
                    }
                    FR16(fr_, 88, 0x4E) = schedule_program(si, di);
                } else {
                    FR16(fr_, 88, 0x4E) = a1;
                }
                if (FR16(fr_, 88, 0x4E) != 0) {
                    if (((i16)FR16(fr_, 88, 0x4E)) > 0 && ((i16)FR16(fr_, 88, 0x4E)) < 2000) {
                        if (((i16)FR16(fr_, 88, 0x4E)) < 1000) {
                            FRX16(fr_, 88, 0x3E, (si << 1) + 0) = (u16)(i8)(films(FR16(fr_, 88, 0x4E)).topicality);
                            FRX16(fr_, 88, 0x36, (si << 1) + 0) = (u16)(i8)(films(FR16(fr_, 88, 0x4E)).genre);
                            cc = ((u16)((u16)(i8)FR8(fr_, 88, 0x2D))) != si;
                            if (!(cc || ((u16)(((i16)((films(FR16(fr_, 88, 0x4E)).flags2_16) << 0xC)) >> 0xF)) == 0)) {
                                if (((i16)(u16)((u16)((i32)(i16)a0 % 7))) < 4) {
                                    FRX16(fr_, 88, 0x3E, (si << 1) + 0) = 0;
                                }
                            }
                        } else {
                            FR16(fr_, 88, 0x50) = FR16(fr_, 88, 0x4E) + 0xFC18;
                            FRX16(fr_, 88, 0x3E, (si << 1) + 0) = (u16)(i8)(FP8(g_series, ((FR16(fr_, 88, 0x50) * 0x39) + ((series(FR16(fr_, 88, 0x50)).next_ep) * 3)) + 1));
                            FRX16(fr_, 88, 0x36, (si << 1) + 0) = ((u16)(i8)(series(FR16(fr_, 88, 0x50)).genre)) + 0xA;
                            if (FR16(fr_, 88, 0x4E) != a1) {
                                ax = 0;
                            } else {
                                ax = 1;
                            }
                            FR16(fr_, 88, 0x4C) = ax;
                            di = a0;
                            while (((i16)di) >= ((i16)FR16(fr_, 88, 0x42))) {
                                LOOP_TICK(17);
                                if (schedule_program(si, di) == FR16(fr_, 88, 0x4E)) {
                                    FR16(fr_, 88, 0x4C) = FR16(fr_, 88, 0x4C) + 1;
                                }
                                di = di - 1;
                            }
                            if (FR16(fr_, 88, 0x4C) == 0) {
                                FR16(fr_, 88, 0x4C) = 1;
                            }
                            FRX16(fr_, 88, 0x3E, (si << 1) + 0) = (u16)((u16)((i32)(i16)(FRX16(fr_, 88, 0x3E, (si << 1) + 0)) / (i16)FR16(fr_, 88, 0x4C)));
                        }
                    }
                } else {
                    FRX16(fr_, 88, 0x3E, (si << 1) + 0) = 0;
                    FRX16(fr_, 88, 0x36, (si << 1) + 0) = 0;
                }
                di = 0;
                while (((i16)di) < 3) {
                    LOOP_TICK(43);
                    if ((g_ai_trends((((u16)(i8)g_ai_cur) * 3) + di)) == (FRX16(fr_, 88, 0x36, (si << 1) + 0))) {
                        t29 = (u16)((u16)((FRX16(fr_, 88, 0x3E, (si << 1) + 0)) * (g_trends((di * 2) + 1))) / 0xA);
                        FRX16(fr_, 88, 0x3E, (si << 1) + 0) = t29;
                    }
                    di = di + 1;
                }
                si = si + 1;
            }
            si = 1;
            while (((i16)si) <= 3) {
                LOOP_TICK(146);
                if ((FRX16(fr_, 88, 0x36, (si << 1) + 0)) != 0) {
                    FR16(fr_, 88, 0x52) = 0;
                    di = 1;
                    while (((i16)di) <= 3) {
                        LOOP_TICK(20);
                        if ((FRX16(fr_, 88, 0x36, (si << 1) + 0)) == (FRX16(fr_, 88, 0x36, (di << 1) + 0))) {
                            FR16(fr_, 88, 0x52) = FR16(fr_, 88, 0x52) + (FRX16(fr_, 88, 0x3E, (di << 1) + 0));
                        }
                        di = di + 1;
                    }
                    if (((i16)(G8((((FRX16(fr_, 88, 0x36, (si << 1) + 0)) << 3) + 0x7160) + FR16(fr_, 88, 0x40)))) < ((i16)FR16(fr_, 88, 0x52))) {
                        FRX16(fr_, 88, 0x3E, (si << 1) + 0) = (u16)((u16)((i32)(i16)((G8((((FRX16(fr_, 88, 0x36, (si << 1) + 0)) << 3) + 0x7160) + FR16(fr_, 88, 0x40))) * (FRX16(fr_, 88, 0x3E, (si << 1) + 0))) / (i16)FR16(fr_, 88, 0x52)));
                    }
                    FRX16(fr_, 88, 0x3E, (si << 1) + 0) = (u16)((u16)((i32)(i16)((FRX16(fr_, 88, 0x3E, (si << 1) + 0)) * (G8((((FRX16(fr_, 88, 0x36, (si << 1) + 0)) << 3) + 0x7160) + FR16(fr_, 88, 0x40)))) / 100));
                    /* ORIGINAL BUG (32BA:07EC): the news lead-in is only computed
                     * for today's hours (a0 <= 7) but added (/5) for every hour;
                     * for tomorrow's the original adds a fifth of the last word
                     * anything wrote at [bp-4Ah+2*si].  Measured on the scenarios:
                     * mostly this function's own locals from an earlier call (from
                     * the same or another caller, at another depth: ai_tick runs
                     * from game_frame, which is called at many depths), but also
                     * temporaries of Borland helpers (LXMUL@/LDIV@, farmalloc) and
                     * of the assembler blitters (377A, 3762, 3786, 37A2) that the
                     * drawing code ran at that depth in between -- e.g. a pushed DS
                     * (0x3B84, +3047).  So the value depends on what was drawn and
                     * computed since, at which depth; the decompiled build's
                     * emulated stack holds none of it (no pushed arguments, return
                     * addresses, saved registers or helper frames), and tracking it
                     * would mean reproducing every function's stack writes.  Not
                     * reproduced: no lead-in, in every mode.  names/FINDINGS.md */
                    FRX16(fr_, 88, 0x4A, (si << 1) + 0) = 0;
                    if (((i16)a0) <= 7) {
                        if ((FRX16(fr_, 88, 0x3E, (si << 1) + 0)) != 0) {
                            t65 = ai_news_leadin(si, a0);
                            FRX16(fr_, 88, 0x4A, (si << 1) + 0) = t65;
                        }
                    }
                    if ((FRX16(fr_, 88, 0x3E, (si << 1) + 0)) != 0) {
                        FRX16(fr_, 88, 0x3E, (si << 1) + 0) = (FRX16(fr_, 88, 0x3E, (si << 1) + 0)) + (u16)((u16)((i32)(i16)(FRX16(fr_, 88, 0x4A, (si << 1) + 0)) / 5));
                    }
                } else {
                    FRX16(fr_, 88, 0x3E, (si << 1) + 0) = 0;
                }
                si = si + 1;
            }
            FR16(fr_, 88, 0x54) = 0;
            FR16(fr_, 88, 0x52) = 0;
            si = 1;
            while (((i16)si) <= 3) {
                LOOP_TICK(15);
                FR16(fr_, 88, 0x54) = FR16(fr_, 88, 0x54) + (FRX16(fr_, 88, 0x3E, (si << 1) + 0));
                FR16(fr_, 88, 0x52) = FR16(fr_, 88, 0x52) + (FRX16(fr_, 88, 0x3E, (si << 1) + 0));
                si = si + 1;
            }
            if (((i16)FR16(fr_, 88, 0x54)) > 100) {
                FR16(fr_, 88, 0x54) = 0x64;
            }
            si = 1;
            while (((i16)si) <= 3) {
                LOOP_TICK(92);
                if (FR16(fr_, 88, 0x52) != 0) {
                    FRX16(fr_, 88, 0x2C, (si * 0xA) + 0) = (u16)((u16)((i32)(i16)((FRX16(fr_, 88, 0x3E, (si << 1) + 0)) * FR16(fr_, 88, 0x54)) / (i16)FR16(fr_, 88, 0x52)));
                    FRX32(fr_, 88, 0x2C, ((si * 0xA) + 0) + 2) = ((u32)(((u32)((i32)((u32)(tx(si, 0).reach * MK32(((i16)(FRX16(fr_, 88, 0x2C, (si * 0xA) + 0))) >> 0xF, FRX16(fr_, 88, 0x2C, (si * 0xA) + 0)))) / (i32)0x64)) * 0x9A0));
                } else {
                    FRX16(fr_, 88, 0x2C, (si * 0xA) + 0) = 0;
                    FRX32(fr_, 88, 0x2C, ((si * 0xA) + 0) + 2) = 0;
                }
                si = si + 1;
            }
            si = 1;
            while (((i16)si) <= 3) {
                LOOP_TICK(23);
                FRX16(fr_, 88, 0x2C, ((si * 0xA) + 0) + 6) = FRX16(fr_, 88, 0x2C, (si * 0xA) + 0);
                si = si + 1;
            }
            t95 = FRX16(fr_, 88, 0x2C, ((((u16)(i8)FR8(fr_, 88, 0x2D)) * 0xA) + 0) + 6);
            FR16(fr_, 88, 0x2) = ((i16)(FRX16(fr_, 88, 0x2C, ((((u16)(i8)FR8(fr_, 88, 0x2D)) * 0xA) + 0) + 6))) >> 0xF;
            FR16(fr_, 88, 0x4) = t95;
            if (((u8)((u8)a2)) != 1) {
                if (((u8)((u8)a2)) == 2) {
                    FR16(fr_, 88, 0x56) = 1;
                    di = 0;
                    if (((u32)(MK32(FR16(fr_, 88, 0x22), FR16(fr_, 88, 0x18)))) == 0 && FR16(fr_, 88, 0xE) == 0) {
                        FR16(fr_, 88, 0x56) = 3;
                    } else {
                        si = 1;
                        while (((i16)si) <= 3) {
                            LOOP_TICK(49);
                            if ((players(si).out) == 0) {
                                if (((u16)((u16)(i8)FR8(fr_, 88, 0x2D))) != si) {
                                    if (((i16)(FRX16(fr_, 88, 0x2C, (si * 0xA) + 0))) >= ((i16)(FRX16(fr_, 88, 0x2C, (((u16)(i8)FR8(fr_, 88, 0x2D)) * 0xA) + 0)))) {
                                        FR16(fr_, 88, 0x56) = FR16(fr_, 88, 0x56) + 1;
                                    }
                                }
                            }
                            if ((players(si).out) != 0) {
                                di = di + 1;
                            }
                            si = si + 1;
                        }
                        if (((u32)(MK32(FR16(fr_, 88, 0x56), di))) == 0x20001) {
                            FR16(fr_, 88, 0x56) = 3;
                        }
                    }
                    t101 = FR16(fr_, 88, 0x56);
                    FR16(fr_, 88, 0x2) = ((i16)FR16(fr_, 88, 0x56)) >> 0xF;
                    FR16(fr_, 88, 0x4) = t101;
                }
            } else {
                t98 = FRX16(fr_, 88, 0x2C, ((((u16)(i8)FR8(fr_, 88, 0x2D)) * 0xA) + 0) + 2);
                FR16(fr_, 88, 0x2) = FRX16(fr_, 88, 0x2C, ((((u16)(i8)FR8(fr_, 88, 0x2D)) * 0xA) + 0) + 4);
                FR16(fr_, 88, 0x4) = t98;
            }
            if (a1 == 0) {
                FR16(fr_, 88, 0x4E) = schedule_program((u16)(i8)FR8(fr_, 88, 0x2D), a0);
            } else {
                FR16(fr_, 88, 0x4E) = a1;
            }
            ax = FR16(fr_, 88, 0x4);
            dx = FR16(fr_, 88, 0x2);
        } else if (((u8)((u8)a2)) != 0) {
            ax = viewers_lo((u16)(i8)FR8(fr_, 88, 0x2D));
            dx = owned_scripts(0x35, (((u16)(i8)FR8(fr_, 88, 0x2D)) * 2) + 3).episode;
        } else {
            ax = ratings((u16)(i8)FR8(fr_, 88, 0x2D)).share;
            dx = ((i16)(ratings((u16)(i8)FR8(fr_, 88, 0x2D)).share)) >> 0xF;
        }
    } else {
        goto L_044D;
    }
    rv_ = MK32(dx, ax);
    return rv_;
}

/* 32BA:0ABA  (station, hour): news quality decayed by 15% per hour from now to hour, times g_hour_audience[hour]/100 */
u16 ai_news_leadin(u16 a0, u16 a1) {
    u16 ax;
    u16 di;
    u16 si;
    FUNC_TICK(23);
    si = (u16)((u16)(i8)g_news_score(a0));
    di = clock_hour();
    while (((i16)di) < ((i16)a1)) {
        LOOP_TICK(10);
        si = (u16)((u16)((i32)(i16)(si * 0x55) / 100));
        di = di + 1;
    }
    ax = (u16)((u16)((i32)(i16)(((u16)(i8)g_hour_audience(a1)) * si) / 100));
    return ax;
}

/* 32BA:0AFD  ?AI ad schedule pass: from the next hour, pull later ads forward into empty ad slots and swap ads so that each slot's estimated viewers meet the ad's minimum audience */
void ai_optimize_ads(void) {
    u16 ax;
    u16 bx;
    u16 di;
    u16 s0;
    u16 s1;
    u16 si;
    u16 t1;
    u16 t2;
    u32 t14;
    u16 t30;
    u16 t42;
    u32 t43;
    u16 t51;
    u16 t52;
    u16 t59;
    u16 t71;
    u16 l_24 = 0;
    u16 l_22 = 0;
    u16 l_20 = 0;
    u16 l_1E = 0;
    u16 l_1C = 0;
    u16 l_1A = 0;
    u16 l_18 = 0;
    u16 l_16 = 0;
    u16 l_14 = 0;
    u16 l_12 = 0;
    u8 fr_[16] = {0};                  /* bytes bp-16 .. bp-1 of the original frame */
    FUNC_TICK(52);
    t1 = (((u16)(i8)(g_ai_player((u16)(i8)g_ai_cur))) * 0x5A) + 0x4417;
    t2 = (u16)(i8)(g_ai_player((u16)(i8)g_ai_cur));
    FR16(fr_, 16, 0x2) = DSEG;
    FR16(fr_, 16, 0x4) = t1;
    l_24 = clock_hour_round55();
    si = t2;
    while (((i16)l_24) < 15) {
        LOOP_TICK(348);
        if ((FP16(FR32(fr_, 16, 0x4), (l_24 << 1) + 0x3C)) == 0) {
            l_12 = 0;
            l_22 = l_24 + 1;
            for (;;) {
                LOOP_TICK(58);
                t30 = l_12;
                s0 = (((-t30) - (-t30)) - (t30 != 0)) + 1;
                if (((i16)l_22) >= 15) {
                    ax = 0;
                } else {
                    ax = 1;
                }
                if ((s0 & ax) == 0) {
                    goto L_0ED9;
                } else {
                    l_12 = FP16(FR32(fr_, 16, 0x4), (l_22 << 1) + 0x3C);
                    if (((u8)((u8)(ad_genre_allowed(l_24, l_12, 0)))) == 0) {
L_0BBE:
                        l_12 = 0;
                    } else {
                        t14 = ai_estimate_audience(l_24, 0, 1);
                        ax = LO16(t14);
                        bx = (((u16)(i8)(ads(l_12).min_class)) - 1) << 2;
                        if (((i32)t14) < ((i32)(G32(((((u16)(i8)(ads(l_12).min_class)) - 1) << 2) + 0xA8)))) goto L_0BBE;
                    }
                    if (l_12 != 0) break;
                    l_22 = l_22 + 1;
                    continue;
                }
            }
            di = 0;
            while (((i16)g_player_ad_count(si)) > ((i16)di)) {
                LOOP_TICK(20);
                if ((contracts(si, di).ad) == l_12) break;
                di = di + 1;
            }
            ad_plan_book(l_22, di, 0, si);
            ad_plan_book(l_24, di, 1, si);
            FP16(FR32(fr_, 16, 0x4), (l_24 << 1) + 0x3C) = l_12;
            FP16(FR32(fr_, 16, 0x4), (l_22 << 1) + 0x3C) = 0;
        } else {
            if (((i16)l_24) > 7) {
                ax = 1;
            } else {
                ax = 0;
            }
            l_1E = ax;
            l_12 = FP16(FR32(fr_, 16, 0x4), (l_24 << 1) + 0x3C);
            l_20 = 0;
            while (((i16)g_player_ad_count(si)) > ((i16)l_20)) {
                LOOP_TICK(20);
                if ((contracts(si, l_20).ad) == l_12) break;
                l_20 = l_20 + 1;
            }
            if (((i16)g_player_ad_count(si)) > ((i16)l_20)) {
                l_1C = 0xE;
                if (((i8)((u8)((((u8)((u8)(((u16)(l_20 * 7)) >> 8))) << 8) | (G8((((si * 0x4D) + 0x4EDF) + (l_20 * 7)) + l_1E))))) < ((i8)(G8(((((si * 0x4D) + 0x4EDF) + (l_20 * 7)) + l_1E) + 2)))) {
                    l_1C = (l_1E * 7) + 7;
                }
                l_1A = 0;
                t42 = G16(((((u16)(i8)(ads(l_12).min_class)) - 1) << 2) + 0xA8);
                FR16(fr_, 16, 0xE) = G16(((((u16)(i8)(ads(l_12).min_class)) - 1) << 2) + 0xAA);
                FR16(fr_, 16, 0x10) = t42;
                t43 = ai_estimate_audience(l_24, 0, 1);
                FR16(fr_, 16, 0xA) = HI16(t43);
                FR16(fr_, 16, 0xC) = LO16(t43);
                l_22 = l_24;
                while (((i16)l_22) <= ((i16)l_1C)) {
                    LOOP_TICK(59);
                    l_16 = FP16(FR32(fr_, 16, 0x4), (l_22 << 1) + 0x3C);
                    if (l_16 != 0) {
                        t51 = G16(((((u16)(i8)(ads(l_16).min_class)) - 1) << 2) + 0xA8);
                        t52 = G16(((((u16)(i8)(ads(l_16).min_class)) - 1) << 2) + 0xAA);
                        FR16(fr_, 16, 0x6) = G16(((((u16)(i8)(ads(l_16).min_class)) - 1) << 2) + 0xAA);
                        FR16(fr_, 16, 0x8) = t51;
                        ax = t51;
                        if (((i32)MK32(t52, t51)) < ((i32)FR32(fr_, 16, 0xC))) {
                            if (((u8)((u8)(ad_genre_allowed(l_24, l_16, 0)))) != 0) {
                                ax = FR16(fr_, 16, 0x8);
                                if (((i32)FR32(fr_, 16, 0x8)) > ((i32)FR32(fr_, 16, 0x10))) {
                                    if (((u8)((u8)(ad_genre_allowed(l_22, l_12, 0)))) != 0) {
                                        l_1A = l_16;
                                        t59 = FR16(fr_, 16, 0x8);
                                        FR16(fr_, 16, 0xE) = FR16(fr_, 16, 0x6);
                                        FR16(fr_, 16, 0x10) = t59;
                                        l_18 = l_22;
                                    }
                                }
                            }
                        }
                    }
                    l_22 = l_22 + 1;
                }
                if (l_1A != 0) {
                    ad_plan_book(l_24, l_20, 0, si);
                    ad_plan_book(l_18, l_20, 1, si);
                    di = 0;
                    while (((i16)g_player_ad_count(si)) > ((i16)di)) {
                        LOOP_TICK(20);
                        if ((contracts(si, di).ad) == l_1A) break;
                        di = di + 1;
                    }
                    ad_plan_book(l_18, di, 0, si);
                    ad_plan_book(l_24, di, 1, si);
                    FP16(FR32(fr_, 16, 0x4), (l_18 << 1) + 0x3C) = l_12;
                    FP16(FR32(fr_, 16, 0x4), (l_24 << 1) + 0x3C) = l_1A;
                }
            }
        }
L_0ED9:
        l_24 = l_24 + 1;
    }
    l_24 = 0;
    while (((i16)g_player_ad_count(si)) > ((i16)l_24)) {
        LOOP_TICK(72);
        t71 = contracts(si, l_24).deadline;
        l_14 = contracts(si, l_24).deadline;
        if (((i16)t71) <= ((i16)(g_day + 1))) {
            if (((u16)(((i16)((G16((((si * 0x4D) + 0x4EDF) + (l_24 * 7)) + 6)) << 0xD)) >> 0xF)) == 0) {
                l_22 = 1;
                while (((i16)l_22) <= 2) {
                    LOOP_TICK(29);
                    s0 = l_24;
                    s1 = l_22;
                    if (l_14 != g_day) {
                        ax = 0xE;
                    } else {
                        ax = 7;
                    }
                    ai_book_ad_spots(si, contracts(si, l_24).ad, 1, ax, s1, s0);
                    l_22 = l_22 + 1;
                }
            }
        }
        l_24 = l_24 + 1;
    }
    return;
}

/* 32BA:0FB2  (player, ad, from, to, mode, contract): book remaining spots (ad spots - booked) into hour slots whose estimated viewers exceed the ad's minimum; mode 1 empty slots only, mode 2 may displace another contract's spot */
void ai_book_ad_spots(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5) {
    u16 ax;
    u16 di;
    u16 si;
    u32 l_4;
    u32 l_8;
    u16 l_A;
    u16 l_C;
    u16 l_E;
    u32 t5;
    u16 t10;
    u32 t11;
    FUNC_TICK(74);
    l_A = ((u16)(i8)(ads(a1).spots)) - ((u16)(i8)(ai_ad_plan(a0, a5).booked_total));
    l_8 = schedule_at(a0);
    l_4 = G32(((((u16)(i8)(ads(a1).min_class)) - 1) << 2) + 0xA8);
    di = a2;
    while (((i16)di) <= ((i16)a3)) {
        LOOP_TICK(133);
        if (((i16)l_A) < 1) break;
        l_E = 0;
        if (a4 == 1) {
            if ((FP16(l_8, (di << 1) + 0x3C)) == 0) {
                t5 = ai_estimate_audience(di, 0, 1);
                ax = LO16(t5);
                if (((i32)t5) > ((i32)l_4)) {
                    if (((u8)((u8)(ad_genre_allowed(di, a1, 0)))) != 0) {
                        l_E = 1;
                    }
                }
            }
        }
        if (a4 == 2) {
            t10 = FP16(l_8, (di << 1) + 0x3C);
            l_C = FP16(l_8, (di << 1) + 0x3C);
            if (t10 != 0) {
                t11 = ai_estimate_audience(di, 0, 1);
                ax = LO16(t11);
                if (((i16)HI16(t11)) >= ((i16)HI16(l_4)) && (((i16)HI16(t11)) > ((i16)HI16(l_4)) || ax > LO16(l_4)) && ((u8)((u8)(ad_genre_allowed(di, a1, 0)))) != 0) {
                    si = 0;
                    while ((contracts(a0, si).ad) != l_C) {
                        LOOP_TICK(20);
                        if (((i16)g_player_ad_count(a0)) <= ((i16)si)) break;
                        si = si + 1;
                    }
                    if (((i16)g_player_ad_count(a0)) > ((i16)si)) {
                        if (((u16)(((i16)((G16((((a0 * 0x4D) + 0x4EDF) + (si * 7)) + 6)) << 0xD)) >> 0xF)) == 0) {
                            l_E = 1;
                            ad_plan_book(di, si, 0, a0);
                        }
                    }
                }
            }
        }
        if (l_E != 0) {
            FP16(l_8, (di << 1) + 0x3C) = a1;
            ad_plan_book(di, a5, 1, a0);
            l_A = l_A - 1;
        }
        di = di + 1;
    }
    return;
}

/* 32BA:11AE  if gift budget > 30000 pick the gift (3..9) with the highest price per love point that is affordable, pay it, remember it in g_ai_pending_gift */
void ai_buy_betty_gift(void) {
    u16 ax;
    u16 bx;
    int cc;
    u16 di;
    u16 es;
    u16 si;
    u16 l_2;
    u16 l_4;
    u16 t1;
    u16 t5;
    u16 t6;
    u32 t16;
    u16 t29;
    FUNC_TICK(72);
    l_4 = (u16)(i8)(g_ai_player((u16)(i8)g_ai_cur));
    t1 = aibudget_gifts_hi(l_4);
    bx = (l_4 * 0x3C) + 0x6E29;
    di = 0;
    es = DSEG;
    if (((i16)aibudget_gifts_hi(l_4)) >= 0 && (((i16)t1) > 0 || MEM16(es, bx + 4) > 0x7530)) {
        l_2 = 0;
        si = 9;
        while (((i16)si) > 2) {
            LOOP_TICK(91);
            t5 = g_presents((si * 4) + 2);
            t6 = aibudget_gifts_hi(l_4);
            cc = ((i16)(g_presents((si * 4) + 2))) <= ((i16)aibudget_gifts_hi(l_4));
            ax = g_presents((si * 4) + 1);
            bx = (l_4 * 0x3C) + 0x6E29;
            es = DSEG;
            if (cc && (((i16)t5) < ((i16)t6) || ax < MEM16(es, bx + 4))) {
                if ((gifts(si).love) != 0) {
                    t16 = (u32)((i32)gifts(si).price / (i32)MK32(((i16)((u16)(i8)(gifts(si).love))) >> 0xF, (u16)(i8)(gifts(si).love)));
                    cc = ((i32)t16) > ((i32)(MK32(((i16)di) >> 0xF, di)));
                    ax = di;
                    bx = LO16(t16);
                    if (cc) {
                        l_2 = si;
                        di = LO16(((u32)((i32)gifts(si).price / (i32)MK32(((i16)((u16)(i8)(gifts(si).love))) >> 0xF, (u16)(i8)(gifts(si).love)))));
                    }
                }
            }
            si = si - 1;
        }
        if (l_2 != 0) {
            t29 = g_presents((l_2 * 4) + 1);
            ai(l_4).gifts = (ai(l_4).gifts) - (gifts(l_2).price);
            g_ai_pending_gift((u16)(i8)g_ai_cur) = (u8)((((u8)((u8)(t29 >> 8))) << 8) | ((u8)((u8)l_2)));
        }
    }
    return;
}

/* 32BA:1331  give the pending gift: Betty love += gift points (capped at the player's boss rating); gift points shrink to 70% and use count++ */
void ai_give_betty_gift(void) {
    u16 di;
    u16 si;
    FUNC_TICK(112);
    si = (u16)(i8)(g_ai_player((u16)(i8)g_ai_cur));
    di = (u16)(i8)(g_ai_pending_gift((u16)(i8)g_ai_cur));
    if (((u16)((u16)(i8)(g_ai_pending_gift((u16)(i8)g_ai_cur)))) != 0) {
        players(si).betty_love = (players(si).betty_love) + ((u16)(i8)(gifts(di).love));
        gifts(di).love = (u8)(u16)((u16)((i32)(i16)(((u16)(i8)(gifts(di).love)) * 0x46) / 100));
        gifts(di).given = (gifts(di).given) + 1;
        g_ai_pending_gift((u16)(i8)g_ai_cur) = 0;
        if (((i16)(players(si).betty_love)) > ((i16)(players(si).image))) {
            players(si).betty_love = players(si).image;
        }
    }
    return;
}

/* 32BA:1432  AI program planning from its start hour: blocks whose estimated rank is worse than the pass threshold get the library film/series of matching length (no live, no X-rated before 21h, no scheduled series) with the best rank; passes run for difficulty 2..g_difficulty (thresholds rank 2, 1) so on Easy the AI does no re-planning here */
void ai_plan_programs(void) {
    u16 ax;
    u16 bx;
    int cc;
    u16 di;
    u16 s0;
    u16 si;
    u16 t1;
    u16 t6;
    u32 t9;
    u16 t28;
    u32 t34;
    u16 l_2C = 0;
    u16 l_2A = 0;
    u16 l_28 = 0;
    u16 l_26 = 0;
    u16 l_24 = 0;
    u16 l_22 = 0;
    u16 l_20 = 0;
    u16 l_1E = 0;
    u16 l_1C = 0;
    u16 l_1A = 0;
    u16 l_18 = 0;
    u16 l_16 = 0;
    u16 l_14 = 0;
    u16 l_12 = 0;
    u16 l_10 = 0;
    u16 l_E = 0;
    u16 l_C = 0;
    u16 l_A = 0;
    u16 l_8 = 0;
    u16 l_6 = 0;
    u8 fr_[4] = {0};                  /* bytes bp-4 .. bp-1 of the original frame */
    FUNC_TICK(58);
    l_2C = (u16)(i8)(g_ai_player((u16)(i8)g_ai_cur));
    t1 = (l_2C * 0x5A) + 0x4417;
    FR16(fr_, 4, 0x2) = DSEG;
    FR16(fr_, 4, 0x4) = t1;
    l_2A = g_ai_step((u16)(i8)g_ai_cur);
    t6 = clock_hour_round05();
    if (((i16)t6) <= ((i16)l_2A)) {
        l_A = 2;
        l_22 = 2;
        while (((i16)l_22) <= ((i16)g_difficulty)) {
            LOOP_TICK(321);
            if (((i16)l_2A) > 7) {
                ax = 0xE;
            } else {
                ax = 7;
            }
            l_8 = ax;
            si = FP16(FR32(fr_, 4, 0x4), l_2A << 1);
            if ((FP16(FR32(fr_, 4, 0x4), l_2A << 1)) != 0x270F) {
                l_14 = 1;
                for (;;) {
                    LOOP_TICK(20);
                    s0 = FP16(FR32(fr_, 4, 0x4), (l_2A + l_14) << 1);
                    if (si == 0) {
                        ax = 0;
                    } else {
                        ax = 0x270F;
                    }
                    if (s0 != ax || ((i16)(l_2A + l_14)) > ((i16)l_8)) break;
                    l_14 = l_14 + 1;
                }
                l_10 = 0;
                di = l_2A;
                while (((i16)(l_2A + l_14)) > ((i16)di)) {
                    LOOP_TICK(22);
                    t9 = ai_estimate_audience(di, 0, 2);
                    ax = l_A;
                    bx = LO16(t9);
                    if (((i32)t9) <= ((i32)(MK32(((i16)l_A) >> 0xF, l_A)))) {
                        l_10 = l_10 + 1;
                    }
                    di = di + 1;
                }
                if (((i16)l_10) < ((i16)l_14)) {
                    l_20 = 0;
                    l_C = 0;
                    if (si != 0) {
                        if (((i16)si) >= 1000) {
                            l_E = (u16)(i8)series_blocks(si + 0xFC18);
                        } else {
                            l_E = (u16)(i8)(films(si).blocks);
                        }
                    } else {
                        di = l_2A;
                        while ((FP16(FR32(fr_, 4, 0x4), l_2A << 1)) == 0) {
                            LOOP_TICK(9);
                            if (((i16)di) > ((i16)l_8)) break;
                            di = di + 1;
                        }
                        l_E = (di - l_2A) + 1;
                        l_C = 1;
                    }
                    di = 1;
                    while (((i16)di) <= 11) {
                        LOOP_TICK(200);
                        l_28 = 0;
                        while (((i16)((u16)(i8)(library_count(l_2C, di)))) > ((i16)l_28)) {
                            LOOP_TICK(195);
                            cc = ((i16)di) > 10;
                            si = library_items(l_2C, di, l_28);
                            if (cc) {
L_15EF:
                                if (di == 0xB) {
                                    if (((u16)(((i16)((series(si).flags2_16) << 0xE)) >> 0xF)) == 0) {
L_1617:
                                        if (di != 7) {
                                            if (di == 0xB) {
L_1652:
                                                if (((i16)di) > 10) {
                                                    ax = ((u16)(i8)(series(si).genre)) + 0xA;
                                                } else {
                                                    ax = (u16)(i8)(films(si).genre);
                                                }
                                                l_1A = ax;
                                                if (((i16)di) > 10) {
                                                    ax = (((u8)((u8)(((u16)(si * 0x39)) >> 8))) << 8) | (series(si).blocks);
                                                } else {
                                                    ax = (((u8)((u8)(((u16)(si * 0x17)) >> 8))) << 8) | (films(si).blocks);
                                                }
                                                l_1C = (u16)(i8)((u8)ax);
                                                if (!(((l_C == 0 || ((i16)l_1C) > ((i16)l_E) || ((i16)l_1C) >= 4) && !(l_C == 0 && l_1C == l_E)) || !(l_C == 0 && l_1C == l_E))) {
                                                    l_6 = 1;
                                                    l_26 = l_2A;
                                                    while (((i16)(l_2A + l_1C)) > ((i16)l_26)) {
                                                        LOOP_TICK(13);
                                                        t28 = FP16(FR32(fr_, 4, 0x4), (l_26 << 1) + 0x3C);
                                                        l_1E = FP16(FR32(fr_, 4, 0x4), (l_26 << 1) + 0x3C);
                                                        if (t28 == 0) {
                                                            l_26 = l_26 + 1;
                                                            continue;
                                                        } else {
                                                            goto L_170D;
                                                        }
                                                    }
L_1734:
                                                    if (l_6 != 0) {
                                                        if (di == 0xB) {
                                                            si = si + 0x3E8;
                                                        }
                                                        l_12 = 0;
                                                        l_24 = l_2A;
                                                        while (((i16)(l_2A + l_14)) > ((i16)l_24)) {
                                                            LOOP_TICK(21);
                                                            t34 = ai_estimate_audience(l_24, si, 2);
                                                            ax = l_A;
                                                            bx = LO16(t34);
                                                            if (((i32)t34) <= ((i32)(MK32(((i16)l_A) >> 0xF, l_A)))) {
                                                                l_12 = l_12 + 1;
                                                            }
                                                            l_24 = l_24 + 1;
                                                        }
                                                        if (((i16)l_12) > ((i16)l_10)) {
                                                            l_20 = si;
                                                            l_10 = l_12;
                                                            l_18 = di;
                                                            l_16 = l_28;
                                                        }
                                                    }
                                                }
                                            } else if (((u16)(((i16)((films(si).flags2_16) << 0xC)) >> 0xF)) == 0) {
                                                goto L_1652;
                                            } else {
                                                if (((i16)(u16)((u16)((i32)(i16)l_2A % 7))) > 3) goto L_1652;
                                            }
                                        }
                                    }
                                }
                            } else if (((u16)(((i16)((films(si).flags2_16) << 0xE)) >> 0xF)) == 0) {
                                goto L_1617;
                            } else {
                                goto L_15EF;
                            }
                            l_28 = l_28 + 1;
                        }
                        di = di + 1;
                    }
                    if (l_20 != 0) {
                        sched_place_program(l_16, l_18, l_2A, l_2C);
                        sched_mark_scheduled();
                    }
                }
            }
            l_A = l_A - 1;
            l_22 = l_22 + 1;
        }
    }
    return;
L_170D:
    if ((ad_target_group_ok(l_1E, l_1A)) == 0) {
        l_6 = 0;
        goto L_1734;
    }
    goto L_1734;
}

/* 32BA:1800  AI sabotage: pick a victim room (50% random public room 0x10..0x18, 20% human's library, 20% human's studio with production, 10% human's news room) and swap its door sign with the sign of today's event target (bomb/bailiff) so the event hits the victim; a missing target swaps with garbage in the original (see ORIGINAL BUG below) */
void ai_swap_signs_sabotage(void) {
    u16 ax;
    u16 di;
    u16 si;
    u16 t1;
    u16 t48;
    u16 t57;
    u8 fr_[24] = {0};                  /* bytes bp-24 .. bp-1 of the original frame */
    FUNC_TICK(211);
    u16 victim = (u16)mp_leading_human();   /* g_player (multiplayer: the leading human) */
    FR16(fr_, 24, 0x16) = (u16)(i8)(g_ai_player((u16)(i8)g_ai_cur));
    FR16(fr_, 24, 0x14) = 0;
    t1 = random_n(0xA, 1);
    ax = t1 - 1;
    if (((u16)(t1 - 1)) <= 9) {
        switch (ax) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
            FR16(fr_, 24, 0x4) = 0;
            for (;;) {
                LOOP_TICK(27);
                FR16(fr_, 24, 0xE) = random_n(9, 1) + 0xF;
                FR16(fr_, 24, 0x14) = 0;
                building_find_room(FR16(fr_, 24, 0xE), 0, 0, (fr_ + 24 - 0x14), (fr_ + 24 - 0x12));
                FR16(fr_, 24, 0x4) = FR16(fr_, 24, 0x4) + 1;
                if (FR16(fr_, 24, 0x14) != 0 || ((i16)FR16(fr_, 24, 0x4)) >= 20) {
                    break;
                } else {
                    continue;
                }
            }
            break;
        case 5:
        case 6:
            building_find_room(4, victim, 0, (fr_ + 24 - 0x14), (fr_ + 24 - 0x12));
            break;
        case 7:
        case 8:
            FR16(fr_, 24, 0x14) = 0;
            si = 1;
            while (((i16)si) <= 12) {
                LOOP_TICK(33);
                if (((u16)((u16)(i8)(studios(si).renter))) == victim) {
                    if ((studios(si).script) != 0) {
                        building_find_room(5, victim, si, (fr_ + 24 - 0x14), (fr_ + 24 - 0x12));
                    }
                }
                si = si + 1;
            }
            if (FR16(fr_, 24, 0x14) != 0) break;
            si = 1;
            while (((i16)si) <= 12) {
                LOOP_TICK(25);
                if (((u16)((u16)(i8)(studios(si).renter))) == victim) {
                    building_find_room(5, victim, si, (fr_ + 24 - 0x14), (fr_ + 24 - 0x12));
                }
                si = si + 1;
            }
            break;
        case 9:
            building_find_room(2, victim, 0, (fr_ + 24 - 0x14), (fr_ + 24 - 0x12));
            break;
        default:
            cpu_halt("bad switch index %u in 32BA:1800", ax);
        }
    }
    if (FR16(fr_, 24, 0x14) != 0) {
        si = 0;
        di = 0xFFFF;
        while (((i16)di) < ((i16)FR16(fr_, 24, 0x12))) {
            LOOP_TICK(17);
            if ((G8((((FR16(fr_, 24, 0x14) - 1) << 3) + 0x8A60) + (si << 1))) != 0) {
                di = di + 1;
            }
            si = si + 1;
        }
        FR16(fr_, 24, 0xA) = si - 1;
        /* ORIGINAL BUG (32BA:1800): event_find_target_sign writes floor and slot
         * only when a door with the event's target room exists.  When it does
         * not -- e.g. a terrorist from a consulate whose door has become a
         * rented studio or was bombed -- the swap below runs with the
         * never-written locals bp-10/bp-C and writes a (floor, door) pair to a
         * random address.  Seen: the film dealer's door turned into a "studio",
         * a computer player then walked to floor 0 and the elevator waited for
         * it for good.  Defined here: no target, no swap.  names/FINDINGS.md */
        FR16(fr_, 24, 0x10) = 0;
        FR16(fr_, 24, 0xC) = 0;
        event_find_target_sign((fr_ + 24 - 0x10), (fr_ + 24 - 0xC));
        si = 0;
        di = 0xFFFF;
        while (((i16)di) < ((i16)FR16(fr_, 24, 0xC))) {
            LOOP_TICK(17);
            if ((G8((((FR16(fr_, 24, 0x10) - 1) << 3) + 0x8A60) + (si << 1))) != 0) {
                di = di + 1;
            }
            si = si + 1;
        }
        FR16(fr_, 24, 0x2) = si - 1;
        if (FR16(fr_, 24, 0x10) != 0 && (FR16(fr_, 24, 0x10) != FR16(fr_, 24, 0x14) || FR16(fr_, 24, 0x2) != FR16(fr_, 24, 0xA))) {
            FR16(fr_, 24, 0x6) = (u16)(i8)(G8((((FR16(fr_, 24, 0x14) - 1) << 3) + 0x8A60) + (FR16(fr_, 24, 0xA) << 1)));
            FR16(fr_, 24, 0x8) = (u16)(i8)(G8(((((FR16(fr_, 24, 0x14) - 1) << 3) + 0x8A60) + (FR16(fr_, 24, 0xA) << 1)) + 1));
            G8((((FR16(fr_, 24, 0x14) - 1) << 3) + 0x8A60) + (FR16(fr_, 24, 0xA) << 1)) = (u8)((((u8)((u8)(((u16)(FR16(fr_, 24, 0x2) << 1)) >> 8))) << 8) | (G8((((FR16(fr_, 24, 0x10) - 1) << 3) + 0x8A60) + (FR16(fr_, 24, 0x2) << 1))));
            t48 = (((u8)((u8)(((u16)(FR16(fr_, 24, 0x2) << 1)) >> 8))) << 8) | (G8(((((FR16(fr_, 24, 0x10) - 1) << 3) + 0x8A60) + (FR16(fr_, 24, 0x2) << 1)) + 1));
            G8(((((FR16(fr_, 24, 0x14) - 1) << 3) + 0x8A60) + (FR16(fr_, 24, 0xA) << 1)) + 1) = (u8)((((u8)((u8)(((u16)(FR16(fr_, 24, 0x2) << 1)) >> 8))) << 8) | (G8(((((FR16(fr_, 24, 0x10) - 1) << 3) + 0x8A60) + (FR16(fr_, 24, 0x2) << 1)) + 1)));
            t57 = (((u8)((u8)(t48 >> 8))) << 8) | FR8(fr_, 24, 0x6);
            G8((((FR16(fr_, 24, 0x10) - 1) << 3) + 0x8A60) + (FR16(fr_, 24, 0x2) << 1)) = (u8)((((u8)((u8)(t48 >> 8))) << 8) | FR8(fr_, 24, 0x6));
            G8(((((FR16(fr_, 24, 0x10) - 1) << 3) + 0x8A60) + (FR16(fr_, 24, 0x2) << 1)) + 1) = (u8)((((u8)((u8)(t57 >> 8))) << 8) | FR8(fr_, 24, 0x8));
        }
    }
    return;
}

/* 32BA:1B17  locate the door-sign position (floor, index) of the room targeted by today's event (g_event_type/g_event_target) */
void event_find_target_sign(u8 *a0, u8 *a2) {
    u16 ax;
    u16 di;
    u16 si;
    u16 t6;
    u16 t13;
    u16 l_C = 0;
    u16 l_A = 0;
    u16 l_8 = 0;
    u16 l_6 = 0;
    u16 l_4 = 0;
    u16 l_2 = 0;
    FUNC_TICK(41);
    ax = g_npc_type - 1;
    if (((u16)(g_npc_type - 1)) <= 3) {
        switch (ax) {
        case 0:
            l_C = 0x13;
            l_A = 0;
            break;
        case 1:
            l_C = 0x12;
            l_A = 0;
            break;
        case 2:
            if (((i16)g_npc_target_player) > 3) {
                l_C = g_npc_target_player;
                l_A = 0;
                break;
            } else {
                l_C = 1;
                l_A = g_npc_target_player;
                break;
            }
            break;
        case 3:
            l_C = 4;
            l_A = g_npc_target_player;
            break;
        default:
            cpu_halt("bad switch index %u in 32BA:1B17", ax);
        }
    }
    l_4 = 0;
    while (((i16)l_4) <= 12) {
        LOOP_TICK(89);
        di = 0;
        while (((i16)di) < 4) {
            LOOP_TICK(84);
            t6 = (u16)(i8)(G8(((l_4 << 3) + 0x8A60) + (di << 1)));
            l_8 = (u16)(i8)(G8(((l_4 << 3) + 0x8A60) + (di << 1)));
            t13 = (u16)(i8)(G8((((l_4 << 3) + 0x8A60) + (di << 1)) + 1));
            l_6 = (u16)(i8)(G8((((l_4 << 3) + 0x8A60) + (di << 1)) + 1));
            if (!(floors_door_room(t6, t13) != l_C || !((floors_door_owner(l_8, l_6)) == l_A || l_A == 0))) {
                l_2 = 0xFFFF;
                si = 0;
                while (((i16)si) <= ((i16)di)) {
                    LOOP_TICK(16);
                    if ((G8(((l_4 << 3) + 0x8A60) + (si << 1))) != 0) {
                        l_2 = l_2 + 1;
                    }
                    si = si + 1;
                }
                HP16(a0, 0) = l_4 + 1;
                HP16(a2, 0) = l_2;
            }
            di = di + 1;
        }
        l_4 = l_4 + 1;
    }
    return;
}

/* 32BA:1C6B  (type, owner, param, *floor, *door): find a door in g_floors with room type, owner (0 = any) and param/studio no. (0 = any) */
void building_find_room(u16 a0, u16 a1, u16 a2, u8 *a3, u8 *a5) {
    u16 di;
    u16 si;
    FUNC_TICK(16);
    HP16(a3, 0) = 0;
    HP16(a5, 0) = 0;
    di = 1;
    while (((i16)di) <= 13) {
        LOOP_TICK(52);
        si = 0;
        while (((i16)si) < 4) {
            LOOP_TICK(47);
            if (floors_door_room(di, si) == a0) {
                if (a1 == 0) {
L_1CCD:
                    if (a2 == 0) {
L_1CF1:
                        HP16(a3, 0) = di;
                        HP16(a5, 0) = si;
                    } else {
                        if (floors_door_studio(di, si) == a2) goto L_1CF1;
                    }
                } else {
                    if (floors_door_owner(di, si) == a1) goto L_1CCD;
                }
            }
            si = si + 1;
        }
        di = di + 1;
    }
    return;
}
