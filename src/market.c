/* Market: daily refill of ad agency, screenplay agency, dealer and auction; trends; boss rating.
 *
 * Original module 24B8 of MTV.EXE (Rainbow Arts, 1991).  Decompiled by
 * tools/decomp.py and hand-maintained since the freeze (git tag
 * generated-final).  Every function keeps its original address in the
 * comment above it; that address ties it to the machine-level twin in gen/
 * and to the difftest tables -- keep it when editing.
 */
#include "game.h"
#include "seat.h"
#include "fixes.h"

/* 24B8:000C  refill the ad agency's 10 offers: ad states recomputed (offered/contracted), only ads whose minimum audience class <= 40% of the largest station reach, random pick if > 5 candidates */
u16 adagency_refill(void) {
    u16 ax;
    u32 rv_;
    u32 t1;
    u32 t3;
    u32 t5;
    u16 t11;
    u16 t17;
    u16 t18;
    u16 t23;
    u16 t24;
    u16 t27;
    u16 t28;
    u16 t31;
    u16 l_14 = 0;
    u16 l_12 = 0;
    u16 l_10 = 0;
    u16 l_E = 0;
    u8 fr_[22] = {0};                  /* bytes bp-22 .. bp-1 of the original frame */
    FUNC_TICK(56);
    l_10 = 0;
    t1 = util_lmax(G32(0x20A9), G32(0x2277));
    t3 = (u32)(util_lmax(G32(0x1EDB), t1) * 0x9A0);
    FR16(fr_, 22, 0xA) = HI16(t3);
    FR16(fr_, 22, 0xC) = LO16(t3);
    t5 = (u32)((i32)MK32((FR16(fr_, 22, 0xA) << 1) | (u16)(FR16(fr_, 22, 0xC) >> 0xF), FR16(fr_, 22, 0xC) << 1) / (i32)5);
    FR16(fr_, 22, 0xA) = HI16(t5);
    FR16(fr_, 22, 0xC) = LO16(t5);
    l_12 = 1;
    for (;;) {
        LOOP_TICK(11);
        ax = G16(((l_12 - 1) << 2) + 0xA8);
        if (((i32)(G32(((l_12 - 1) << 2) + 0xA8))) >= ((i32)FR32(fr_, 22, 0xC))) break;
        if (((i16)l_12) > 10) break;
        l_12 = l_12 + 1;
    }
    l_12 = l_12 - 1;
    FR16(fr_, 22, 0x16) = 1;
    while (((i16)FR16(fr_, 22, 0x16)) <= (i16)g_nadoffers) {   /* 10 in the original (--ad-offers) */
        LOOP_TICK(27);
        ax = FR16(fr_, 22, 0x16) * 7;
        if ((ad_offers(FR16(fr_, 22, 0x16)).ad) != 0) {
            t11 = (ad_offers(FR16(fr_, 22, 0x16)).ad) * 0x18;
            ads(ad_offers(FR16(fr_, 22, 0x16)).ad).holder = 0x68;
            ax = t11;
        }
        FR16(fr_, 22, 0x16) = FR16(fr_, 22, 0x16) + 1;
    }
    FR16(fr_, 22, 0x16) = 1;
    while (((i16)FR16(fr_, 22, 0x16)) <= 3) {
        LOOP_TICK(34);
        l_14 = 0;
        for (;;) {
            LOOP_TICK(29);
            ax = g_player_ad_count(FR16(fr_, 22, 0x16));
            if (((i16)(g_player_ad_count(FR16(fr_, 22, 0x16)))) <= ((i16)l_14)) break;
            ads(contracts(FR16(fr_, 22, 0x16), l_14).ad).holder = (u8)((((u8)((u8)(ax >> 8))) << 8) | ((u8)(FR8(fr_, 22, 0x16) + 0x64)));
            l_14 = l_14 + 1;
        }
        FR16(fr_, 22, 0x16) = FR16(fr_, 22, 0x16) + 1;
    }
    FR16(fr_, 22, 0x16) = 1;
    while (((i16)FR16(fr_, 22, 0x16)) <= (i16)g_nads) {
        LOOP_TICK(33);
        t17 = FR16(fr_, 22, 0x16) * 0x18;
        t18 = FPOFF(ads_at(FR16(fr_, 22, 0x16)));   /* (id * 0x18) + 0x3F4F for 1..50 */
        FR16(fr_, 22, 0x2) = FPSEG(ads_at(FR16(fr_, 22, 0x16)));
        FR16(fr_, 22, 0x4) = t18;
        ax = t17;
        if (((i8)(FP8(FR32(fr_, 22, 0x4), 0xF))) <= 100) {
            FP8(FR32(fr_, 22, 0x4), 0xF) = 0;
        } else {
            FP8(FR32(fr_, 22, 0x4), 0xF) = (FP8(FR32(fr_, 22, 0x4), 0xF)) - 0x64;
        }
        if ((FP8(FR32(fr_, 22, 0x4), 0xF)) == 0) {
            ax = (FP16(FR32(fr_, 22, 0x4), 0x10)) | (FP16(FR32(fr_, 22, 0x4), 0x12));
            if (((FP16(FR32(fr_, 22, 0x4), 0x10)) | (FP16(FR32(fr_, 22, 0x4), 0x12))) != 0) {
                ax = (u16)(i8)(FP8(FR32(fr_, 22, 0x4), 0xA));
                if (((i16)((u16)(i8)(FP8(FR32(fr_, 22, 0x4), 0xA)))) <= ((i16)l_12)) {
                    l_10 = l_10 + 1;
                }
            }
        }
        FR16(fr_, 22, 0x16) = FR16(fr_, 22, 0x16) + 1;
    }
    FR16(fr_, 22, 0x16) = 1;
    while (((i16)FR16(fr_, 22, 0x16)) <= (i16)g_nadoffers) {   /* 10 in the original (--ad-offers) */
        LOOP_TICK(87);
        t23 = FR16(fr_, 22, 0x16) * 7;
        t24 = FPOFF(ad_offers_at(FR16(fr_, 22, 0x16)));   /* (i * 7) + 0x19A0 for 1..10 */
        FR16(fr_, 22, 0x6) = FPSEG(ad_offers_at(FR16(fr_, 22, 0x16)));
        FR16(fr_, 22, 0x8) = t24;
        ax = t23;
        if ((FP16(FR32(fr_, 22, 0x8), 0)) == 0 && l_10 != 0) {
            if (((i16)l_10) <= 5) {
                l_14 = 1;
                while (((i16)l_14) <= (i16)g_nads) {
                    LOOP_TICK(23);
                    t28 = FPOFF(ads_at(l_14));
                    FR16(fr_, 22, 0x2) = FPSEG(ads_at(l_14));
                    FR16(fr_, 22, 0x4) = t28;
                    /* without a fee: out of the game (the count above and the random pick
                     * below skip it; this pick did not -- the original data has none) */
                    if (FP8(ads_at(l_14), 0xF) != 0 || ((i16)((u16)(i8)(FP8(FR32(fr_, 22, 0x4), 0xA)))) > ((i16)l_12) || ((FP16(FR32(fr_, 22, 0x4), 0)) | (FP16(FR32(fr_, 22, 0x4), 2))) == 0
                        || ((FP16(FR32(fr_, 22, 0x4), 0x10)) | (FP16(FR32(fr_, 22, 0x4), 0x12))) == 0) {
                        l_14 = l_14 + 1;
                        continue;
                    } else {
                        goto L_027C;
                    }
                }
            } else {
                for (;;) {
                    LOOP_TICK(30);
                    t27 = random_n(g_nads, FR16(fr_, 22, 0x16));   /* 0x32 in the original */
                    l_E = t27;
                    FR16(fr_, 22, 0x2) = FPSEG(ads_at(t27));
                    FR16(fr_, 22, 0x4) = FPOFF(ads_at(t27));
                    if ((ads(t27).holder) != 0 || ((FP16(FR32(fr_, 22, 0x4), 0x10)) | (FP16(FR32(fr_, 22, 0x4), 0x12))) == 0 || ((i16)((u16)(i8)(FP8(FR32(fr_, 22, 0x4), 0xA)))) > ((i16)l_12) || ((FP16(FR32(fr_, 22, 0x4), 0)) | (FP16(FR32(fr_, 22, 0x4), 2))) == 0) {
                        continue;
                    } else {
                        break;
                    }
                }
            }
L_028D:
            t31 = l_E;
            FP16(FR32(fr_, 22, 0x8), 0) = l_E;
            FP8(FR32(fr_, 22, 0x8), 2) = 0;
            FP8(FR32(fr_, 22, 0x8), 3) = 0;
            FP8(FR32(fr_, 22, 0x4), 0xF) = 4;
            l_10 = l_10 - 1;
            ax = t31;
        }
        FR16(fr_, 22, 0x16) = FR16(fr_, 22, 0x16) + 1;
    }
    rv_ = ax;
    return (u16)rv_;
L_027C:
    l_E = l_14;
    goto L_028D;
}

/* 24B8:02C1  recycle used screenplays and refill the 5 screenplay-agency offers with random available ones (screenplay */
void scriptagency_refill(void) {
    u32 l_4;
    u32 l_8;
    u16 l_A;
    u16 l_C;
    u16 l_E;
    u16 l_10;
    u16 l_12;
    u16 t10;
    FUNC_TICK(46);
    l_12 = 1;
    while (((i16)l_12) <= 50) {
        LOOP_TICK(64);
        l_4 = screenplays_at(l_12);
        if (FP8(l_4, 4) == 0x63 && ((u16)(((i16)(FP16(l_4, 5) << 0xE)) >> 0xF)) != 0 && !(((u16)(((i16)(FP16(l_4, 5) << 0xF)) >> 0xF)) != 0 || ((u16)(((i16)((films(FP16(l_4, 6)).flags2_16) << 0xD)) >> 0xF)) == 0)) {
            FP8(l_4, 4) = 0;
            l_C = 0;
            while (((i16)l_C) < 10) {
                LOOP_TICK(18);
                l_A = 0;
                while (((i16)l_A) < 11) {
                    LOOP_TICK(13);
                    FP8(l_4, ((l_C * 0x23) + (l_A * 3)) + 0xB) = 0;
                    l_A = l_A + 1;
                }
                l_C = l_C + 1;
            }
        }
        l_12 = l_12 + 1;
    }
    l_4 = MK32(FPSEG(g_scripts), FPOFF(g_scripts) + 0x28C8);
    if (FP8(l_4, 4) == 0) {
        l_8 = DSP(0x1973);
        FP16(l_8, 0) = 0x18;
        FP8(l_8, 3) = 0;
        FP32(l_8, 4) = FP32(l_4, 0);
        FP8(l_8, 8) = (u8)script_count_scenes(0x18);
        FP8(l_4, 4) = 4;
    }
    l_E = script_count_available();
    l_12 = 1;
    while (((i16)l_12) <= 5) {
        LOOP_TICK(61);
        l_8 = script_offers_at(l_12);
        if (((i16)l_E) <= 0) break;
        if (FP16(l_8, 0) == 0) {
            for (;;) {
                LOOP_TICK(25);
                t10 = random_n(0x32, l_12);
                l_10 = t10;
                l_4 = MK32(g_scripts_hi, g_scripts_lo + (t10 * 0x1B3));
                if (MEM8((u16)(g_scripts_hi), (u16)(g_scripts_lo + (t10 * 0x1B3)) + 4) != 0 || ((u16)(((i16)(FP16(l_4, 5) << 0xE)) >> 0xF)) == 0) {
                    continue;
                } else {
                    break;
                }
            }
            FP16(l_8, 0) = l_10;
            FP8(l_8, 3) = 0;
            FP32(l_8, 4) = FP32(l_4, 0);
            FP8(l_4, 4) = 4;
            FP8(l_8, 8) = (u8)script_count_scenes(l_10);
        }
        l_E = l_E - 1;
        l_12 = l_12 + 1;
    }
    return;
}

/* 24B8:04B9  refill the movie dealer's 10 slots with random available films/series (non-auction), 3..9 days on offer; while no station reaches 12M viewers up to 3 offers are forced to be cheap (<= $60000); live films get a date 2..4 days ahead and a slot */
void dealer_refill(void) {
    u16 ax;
    u32 l_4;
    u32 l_8;
    u32 l_C;
    u16 l_E;
    u16 l_10;
    u16 l_12;
    u16 l_14;
    u16 l_16;
    u32 t1;
    u32 t4;
    u16 t15;
    u32 t20;
    u16 t21;
    u16 t22;
    u32 t27;
    u32 t30;
    u16 t31;
    u16 t32;
    u16 t35;
    u16 t36;
    u16 t38;
    FUNC_TICK(52);
    l_12 = 0;
    l_16 = 1;
    while (((i16)l_16) <= 3) {
        LOOP_TICK(17);
        t1 = (u32)(tx(l_16, 0).reach * 0x9A0);
        ax = LO16(t1);
        if (((i32)t1) > 12000000) break;
        l_16 = l_16 + 1;
    }
    if (((i16)l_16) > 3) {
        l_14 = 0;
        l_16 = 1;
        while (((i16)l_16) <= (i16)g_nfilms) {
            LOOP_TICK(42);
            l_8 = films_at(l_16);
            if (!(((u16)(((i16)((AT(film, l_8).flags_16) << 9)) >> 0xF)) != 0 || !((AT(film, l_8).owner) == 0 || ((i8)(AT(film, l_8).owner)) > GENRE_EPIC) || ((u16)(((i16)((AT(film, l_8).flags_16) << 0xA)) >> 0xF)) == 0)) {
                t4 = movie_price(l_16);
                ax = LO16(t4);
                if (((i32)t4) <= 60000) {
                    if (((AT(film, l_8).price_lo) | (AT(film, l_8).price_hi)) != 0) {
                        l_14 = l_14 + 1;
                    }
                }
            }
            l_16 = l_16 + 1;
        }
        if (((i16)l_14) > 3) {
            l_14 = 3;
        }
    } else {
        l_14 = 0;
    }
    l_10 = 0;
    l_16 = 1;
    while (((i16)l_16) <= (i16)g_nfilms) {
        LOOP_TICK(37);
        l_8 = films_at(l_16);
        if (!(((u16)(((i16)((AT(film, l_8).flags_16) << 9)) >> 0xF)) != 0 || !((AT(film, l_8).owner) == 0 || ((i8)(AT(film, l_8).owner)) > GENRE_EPIC) || ((u16)(((i16)((AT(film, l_8).flags_16) << 0xA)) >> 0xF)) == 0 || ((AT(film, l_8).price_lo) | (AT(film, l_8).price_hi)) == 0)) {
            l_10 = l_10 + 1;
        }
        l_16 = l_16 + 1;
    }
    l_16 = 1;
    while (((i16)l_16) <= (i16)g_nseries) {
        LOOP_TICK(33);
        l_4 = series_at(l_16);
        if (!(((u16)(((i16)((AT(series, l_4).flags_16) << 9)) >> 0xF)) != 0 || !((AT(series, l_4).owner) == 0 || ((i8)(AT(series, l_4).owner)) > SGENRE_WESTERN) || ((u16)(((i16)((AT(series, l_4).flags_16) << 0xA)) >> 0xF)) == 0)) {
            l_10 = l_10 + 1;
        }
        l_16 = l_16 + 1;
    }
    l_16 = 1;
    while (((i16)l_16) <= (i16)g_nfilmoffers) {   /* 10 in the original (--film-offers) */
        LOOP_TICK(17);
        l_C = dealer_at(l_16);
        if (((i8)(AT(offer, l_C).shelf)) < 0) {
            AT(offer, l_C).program = 0;
        }
        l_16 = l_16 + 1;
    }
    l_16 = 1;
    while (((i16)l_16) <= (i16)g_nfilmoffers) {   /* 10 in the original (--film-offers) */
        LOOP_TICK(188);
        l_C = dealer_at(l_16);
        if ((AT(offer, l_C).program) == 0 && ((i16)l_10) > 0) {
            l_10 = l_10 - 1;
            l_E = 0;
            while (l_E == 0) {
                LOOP_TICK(168);
                t15 = l_12;
                l_12 = l_12 + 1;
                l_E = random_n(g_nfilms + g_nseries, t15);   /* films and series: 0xFA (200 + 50) in the original */
                if (((i16)l_E) > (i16)g_nfilms) {
                    l_E = l_E - g_nfilms;
                    l_4 = series_at(l_E);
                    if (((u16)(((i16)((AT(series, l_4).flags_16) << 9)) >> 0xF)) == 0 && ((AT(series, l_4).owner) == 0 || ((i8)(AT(series, l_4).owner)) > SGENRE_WESTERN) && ((u16)(((i16)((AT(series, l_4).flags_16) << 0xA)) >> 0xF)) != 0) {
                        AT(offer, l_C).program = l_E + 0x3E8;
                        t20 = series_price(l_E);
                        AT(offer, l_C).price = t20;
                        FP8(l_C, 6) = 0;
                        t21 = l_12;
                        l_12 = l_12 + 1;
                        t22 = random_n(7, t21);
                        AT(offer, l_C).shelf = ((u8)t22) + 3;
                        AT(series, l_4).flags_16 = (AT(series, l_4).flags_16) & 0xFFDF;
                        AT(series, l_4).next_ep = SGENRE_FAMILY;
                        continue;
                    } else {
                        l_E = 0;
                        continue;
                    }
                } else {
                    l_8 = films_at(l_E);
                    if (((u16)(((i16)((AT(film, l_8).flags_16) << 9)) >> 0xF)) == 0 && ((AT(film, l_8).owner) == 0 || ((i8)(AT(film, l_8).owner)) > GENRE_EPIC) && ((u16)(((i16)((AT(film, l_8).flags_16) << 0xA)) >> 0xF)) != 0 && ((AT(film, l_8).price_lo) | (AT(film, l_8).price_hi)) != 0) {
                        if (((i16)l_14) <= 0) {
L_0852:
                            l_14 = l_14 - 1;
                            AT(offer, l_C).program = l_E;
                            t30 = movie_price(l_E);
                            AT(offer, l_C).price = t30;
                            FP8(l_C, 6) = 0;
                            t31 = l_12;
                            l_12 = l_12 + 1;
                            t32 = random_n(7, t31);
                            AT(offer, l_C).shelf = ((u8)t32) + 3;
                            AT(film, l_8).flags_16 = (AT(film, l_8).flags_16) & 0xFFDF;
                            if ((AT(film, l_8).genre) != GENRE_LIVE) continue;
                            t35 = l_12;
                            l_12 = l_12 + 1;
                            t36 = random_n(8 - ((u16)(i8)(AT(film, l_8).blocks)), t35);
                            AT(film, l_8).live_slot = t36;
                            t38 = random_n(3, 3);
                            AT(film, l_8).live_day = (t38 + g_day) + 1;
                            continue;
                        } else {
                            t27 = movie_price(l_E);
                            ax = LO16(t27);
                            if (((i32)t27) > 60000) {
                                l_E = 0;
                                continue;
                            } else {
                                goto L_0852;
                            }
                        }
                    } else {
                        l_E = 0;
                        continue;
                    }
                }
            }
        }
        l_16 = l_16 + 1;
    }
    return;
}

/* 24B8:0905  daily auction update: sold items transferred to the high bidder, unsold items return to the market after 3 days, empty slots 1..8 refilled from the auction pool */
void auction_daily(void) {
    u32 l_4;
    u32 l_8;
    u32 l_C;
    u16 l_E;
    u16 l_10;
    u16 l_12;
    u16 l_14;
    FUNC_TICK(21);
    l_10 = 0;
    l_14 = 1;
    while (((i16)l_14) <= (i16)g_nfilms) {
        LOOP_TICK(37);
        l_4 = films_at(l_14);
        if (!(((u16)(((i16)((AT(film, l_4).flags_16) << 9)) >> 0xF)) == 0 || !((AT(film, l_4).owner) == 0 || ((i8)(AT(film, l_4).owner)) > GENRE_EPIC) || ((u16)(((i16)((AT(film, l_4).flags_16) << 0xA)) >> 0xF)) == 0 || ((AT(film, l_4).price_lo) | (AT(film, l_4).price_hi)) == 0)) {
            l_10 = l_10 + 1;
        }
        l_14 = l_14 + 1;
    }
    l_14 = 1;
    while (((i16)l_14) <= (i16)g_nseries) {
        LOOP_TICK(33);
        l_8 = series_at(l_14);
        if (!(((u16)(((i16)((AT(series, l_8).flags_16) << 9)) >> 0xF)) == 0 || !((AT(series, l_8).owner) == 0 || ((i8)(AT(series, l_8).owner)) > SGENRE_WESTERN) || ((u16)(((i16)((AT(series, l_8).flags_16) << 0xA)) >> 0xF)) == 0)) {
            l_10 = l_10 + 1;
        }
        l_14 = l_14 + 1;
    }
    l_14 = 1;
    while (((i16)l_14) <= 10) {
        LOOP_TICK(193);
        l_C = auction_at(l_14);
        if ((AT(lot, l_C).program) != 0) {
            if ((AT(lot, l_C).bidder) == 0) {
                AT(lot, l_C).idle_days = (AT(lot, l_C).idle_days) + 1;
                if (((i8)(AT(lot, l_C).idle_days)) >= 3) {
                    l_12 = AT(lot, l_C).program;
                    if (l_12 == 0 || ((i16)l_12) >= 1000) {
                        l_12 = l_12 - 0x3E8;
                        series(l_12).owner = 0;
                        series(l_12).flags_16 = (series(l_12).flags_16) | 0x20;
                    } else {
                        films(l_12).owner = 0;
                        films(l_12).flags_16 = (films(l_12).flags_16) | 0x20;
                    }
                    AT(lot, l_C).program = 0;
                }
            } else {
                if (((i16)(AT(lot, l_C).program)) <= 1000) {
                    films(AT(lot, l_C).program).flags_16 = (films(AT(lot, l_C).program).flags_16) | 0x20;
                } else {
                    series_flags((AT(lot, l_C).program) + 0xFC18) = (series_flags((AT(lot, l_C).program) + 0xFC18)) | 0x20;
                }
                prog_give_to_player(AT(lot, l_C).program, (u16)(i8)(AT(lot, l_C).bidder), AT(lot, l_C).bid);
                AT(lot, l_C).program = 0;
            }
        }
        if (((i16)l_14) <= 8 && (AT(lot, l_C).program) == 0 && ((i16)l_10) > 0) {
            l_10 = l_10 - 1;
            l_E = 0;
            while (l_E == 0) {
                LOOP_TICK(92);
                l_E = random_n(g_nfilms + g_nseries, l_14);   /* films and series: 0xFA (200 + 50) in the original */
                if (((i16)l_E) > (i16)g_nfilms) {
                    l_E = l_E - g_nfilms;
                    l_8 = series_at(l_E);
                    if (((u16)(((i16)((AT(series, l_8).flags_16) << 9)) >> 0xF)) == 0 || !((AT(series, l_8).owner) == 0 || ((i8)(AT(series, l_8).owner)) > SGENRE_WESTERN) || ((u16)(((i16)((AT(series, l_8).flags_16) << 0xA)) >> 0xF)) == 0) {
                        l_E = 0;
                        continue;
                    } else {
                        AT(series, l_8).next_ep = SGENRE_FAMILY;
                        auction_add_lot(l_14, l_E, 1);
                        continue;
                    }
                } else {
                    l_4 = films_at(l_E);
                    if (((u16)(((i16)((AT(film, l_4).flags_16) << 9)) >> 0xF)) == 0 || !((AT(film, l_4).owner) == 0 || ((i8)(AT(film, l_4).owner)) > GENRE_EPIC) || ((u16)(((i16)((AT(film, l_4).flags_16) << 0xA)) >> 0xF)) == 0 || ((AT(film, l_4).price_lo) | (AT(film, l_4).price_hi)) == 0) {
                        l_E = 0;
                        continue;
                    } else {
                        auction_add_lot(l_14, l_E, 0);
                        continue;
                    }
                }
            }
        }
        l_14 = l_14 + 1;
    }
    return;
}

/* 24B8:0C4A  (phase 1 news / 2 program / 3 ads): hourly broadcast evaluation for all stations (news quality, ratings into g_ratings, Sammy points, ad contracts, studio hand-overs, messages) */
void broadcast_phase_eval(u16 a0) {
    u16 ax;
    u16 bx;
    u16 t9;
    u16 t14;
    u16 t46;
    u16 t107;
    u16 t121;
    u16 t132;
    u16 t150;
    u16 t154;
    u16 t163;
    u32 t169;
    u16 t175;
    u16 t179;
    u16 t182;
    u16 t200;
    u16 t208;
    u16 t214;
    u16 t223;
    u16 t227;
    u16 t232;
    u32 t236;
    u16 t243;
    u8 fr_[58] = {0};                  /* bytes bp-58 .. bp-1 of the original frame */
    u16 bp_ = (u16)(R.sp - 2);           /* the original frame's address, for AX values */
    FUNC_TICK(135);
    struct_copy((fr_ + 58 - 0x20), HPTR(DSP(0x12D6)), 8);
    FR16(fr_, 58, 0x14) = clock_hour();
    if (a0 == 1) {
        FR16(fr_, 58, 0x34) = 1;
        while (((i16)FR16(fr_, 58, 0x34)) <= 3) {
            LOOP_TICK(6);
            news_rate_player(FR16(fr_, 58, 0x34));
            FR16(fr_, 58, 0x34) = FR16(fr_, 58, 0x34) + 1;
        }
        FR16(fr_, 58, 0x38) = news_total_share();
        FR16(fr_, 58, 0x36) = 0;
        FR16(fr_, 58, 0x34) = 1;
        while (((i16)FR16(fr_, 58, 0x34)) <= 3) {
            LOOP_TICK(16);
            if ((players(FR16(fr_, 58, 0x34)).out) == 0) {
                FR16(fr_, 58, 0x36) = FR16(fr_, 58, 0x36) + ((u16)(i8)(g_news_score(FR16(fr_, 58, 0x34))));
            }
            FR16(fr_, 58, 0x34) = FR16(fr_, 58, 0x34) + 1;
        }
        FR16(fr_, 58, 0x34) = 1;
        while (((i16)FR16(fr_, 58, 0x34)) <= 3) {
            LOOP_TICK(109);
            if ((players(FR16(fr_, 58, 0x34)).out) == 0 && FR16(fr_, 58, 0x36) != 0) {
                t9 = (u16)((i32)(i16)(((u16)(i8)(g_news_score(FR16(fr_, 58, 0x34)))) * ((u16)(i8)(g_hour_audience(FR16(fr_, 58, 0x14))))) / 100);
                g_news_score(FR16(fr_, 58, 0x34)) = (u8)t9;
                t14 = (u16)((i32)(i16)(((u16)(i8)(g_news_score(FR16(fr_, 58, 0x34)))) * FR16(fr_, 58, 0x38)) / (i16)FR16(fr_, 58, 0x36));
                ratings(FR16(fr_, 58, 0x34)).share = t14;
                G32((u16)((FR16(fr_, 58, 0x34) * 0xA) + 0x24AD) + 2) = ((u32)(((u32)((i32)((u32)(tx(FR16(fr_, 58, 0x34), 0).reach * MK32(((i16)(ratings(FR16(fr_, 58, 0x34)).share)) >> 0xF, ratings(FR16(fr_, 58, 0x34)).share))) / (i32)0x64)) * 0x9A0));
            } else {
                ratings(FR16(fr_, 58, 0x34)).share = 0;
                G32((u16)((FR16(fr_, 58, 0x34) * 0xA) + 0x24AD) + 2) = 0;
            }
            FR16(fr_, 58, 0x34) = FR16(fr_, 58, 0x34) + 1;
        }
        FR16(fr_, 58, 0x34) = 1;
        while (((i16)FR16(fr_, 58, 0x34)) <= 3) {
            LOOP_TICK(71);
            if ((players(FR16(fr_, 58, 0x34)).out) == 0) {
                owned_scripts(0x35, (FR16(fr_, 58, 0x34) * 2) + 4).script = viewers_lo(FR16(fr_, 58, 0x34));
                G32((u16)(((FR16(fr_, 58, 0x34) << 5) + 0x76B0) + (FR16(fr_, 58, 0x14) << 2))) = ratings(FR16(fr_, 58, 0x34)).viewers;
                if (!(g_sammy_category(0) != 1 || (sammy_daily_update()) != 1)) {
                    g_sammy_category(FR16(fr_, 58, 0x34)) = (g_sammy_category(FR16(fr_, 58, 0x34))) + (ratings(FR16(fr_, 58, 0x34)).share);
                }
            }
            FR16(fr_, 58, 0x34) = FR16(fr_, 58, 0x34) + 1;
        }
    } else if (a0 == 2) {
        FR16(fr_, 58, 0x34) = 1;
        while (((i16)FR16(fr_, 58, 0x34)) <= 3) {
            LOOP_TICK(379);
            t46 = (FR16(fr_, 58, 0x34) << 1) + 0x24A1;
            FR16(fr_, 58, 0x24) = DSEG;
            FR16(fr_, 58, 0x26) = t46;
            if ((players(FR16(fr_, 58, 0x34)).out) == 0) {
                FRX16(fr_, 58, 0x20, (FR16(fr_, 58, 0x34) << 1) + 0) = 0;
                if ((schedule_program(FR16(fr_, 58, 0x34), FR16(fr_, 58, 0x14))) != 0) {
                    FR16(fr_, 58, 0x32) = FR16(fr_, 58, 0x14);
                    FR16(fr_, 58, 0x16) = 0;
                    for (;;) {
                        LOOP_TICK(19);
                        FR16(fr_, 58, 0x16) = FR16(fr_, 58, 0x16) + 1;
                        if ((schedule_program(FR16(fr_, 58, 0x34), FR16(fr_, 58, 0x32))) != 0x270F) {
                            ax = 0;
                        } else {
                            ax = 1;
                        }
                        if (ax == 0) break;
                        FR16(fr_, 58, 0x32) = FR16(fr_, 58, 0x32) - 1;
                    }
                    FR16(fr_, 58, 0x2A) = schedule_program(FR16(fr_, 58, 0x34), FR16(fr_, 58, 0x32));
                    if (((i16)FR16(fr_, 58, 0x2A)) < 1000) {
                        FP16(FR32(fr_, 58, 0x26), 0) = (u16)(i8)(films(FR16(fr_, 58, 0x2A)).topicality);
                        if (mp_is_human(FR16(fr_, 58, 0x34))) {   /* == g_player (multiplayer: that human's TV) */
                            int as_human = mp_enter_player(FR16(fr_, 58, 0x34));
                            g_tv_program_quality = FP16(FR32(fr_, 58, 0x26), 0);
                            if (as_human) mp_leave();
                        }
                        G8(FR16(fr_, 58, 0x34) + 0x71E6) = films(FR16(fr_, 58, 0x2A)).genre;
                        if ((((u16)(((i16)((films(FR16(fr_, 58, 0x2A)).flags2_16) << 0xD)) >> 0xF)) != 0 || (films(FR16(fr_, 58, 0x2A)).genre) == GENRE_LIVE) && ((u16)((u16)(i8)(films(FR16(fr_, 58, 0x2A)).blocks))) == FR16(fr_, 58, 0x16)) {
                            if (FR16(fr_, 58, 0x2A) != 0x1D) {
                                if (FR16(fr_, 58, 0x2A) != 0xBB) {
                                    FRX16(fr_, 58, 0x20, (FR16(fr_, 58, 0x34) << 1) + 0) = FR16(fr_, 58, 0x2A);
                                } else {
                                    live187_reschedule();
                                }
                            } else {
                                live29_reschedule();
                            }
                        }
                        if (FR16(fr_, 58, 0x2A) == 0x3A) {
                            FRX16(fr_, 58, 0x20, (FR16(fr_, 58, 0x34) << 1) + 0) = 0x3A;
                        }
                        if (((u16)(((i16)((films(FR16(fr_, 58, 0x2A)).flags2_16) << 0xC)) >> 0xF)) == 0 || ((i16)FR16(fr_, 58, 0x14)) >= 4) {
                            G16((FR16(fr_, 58, 0x34) * 0xA) + 0x24B5) = 0;
                        } else {
                            G16((FR16(fr_, 58, 0x34) * 0xA) + 0x24B5) = 1;
                            if (g_guardian_state == 1) {
                                g_guardian_state = 2;
                                g_npc_target_player = FR16(fr_, 58, 0x34);
                            }
                        }
                    } else {
                        FR16(fr_, 58, 0x12) = FR16(fr_, 58, 0x2A) + 0xFC18;
                        G8(FR16(fr_, 58, 0x34) + 0x71E6) = (series(FR16(fr_, 58, 0x12)).genre) + 0xA;
                        if ((series(FR16(fr_, 58, 0x12)).next_ep) == 0) {
                            series(FR16(fr_, 58, 0x12)).next_ep = 1;
                        }
                        schedule_episode(FR16(fr_, 58, 0x34), FR16(fr_, 58, 0x14)) = series(FR16(fr_, 58, 0x12)).next_ep;
                        FP16(FR32(fr_, 58, 0x26), 0) = (u16)(i8)(FP8(g_series, ((FR16(fr_, 58, 0x12) * 0x39) + ((series(FR16(fr_, 58, 0x12)).next_ep) * 3)) + 1));
                        if (mp_is_human(FR16(fr_, 58, 0x34))) {   /* == g_player (multiplayer: that human's TV) */
                            int as_human = mp_enter_player(FR16(fr_, 58, 0x34));
                            g_tv_program_quality = FP16(FR32(fr_, 58, 0x26), 0);
                            if (as_human) mp_leave();
                        }
                        FR16(fr_, 58, 0x18) = 0;
                        FR16(fr_, 58, 0x32) = FR16(fr_, 58, 0x14);
                        while (((i16)FR16(fr_, 58, 0x32)) > 0) {
                            LOOP_TICK(17);
                            if ((schedule_program(FR16(fr_, 58, 0x34), FR16(fr_, 58, 0x32))) == FR16(fr_, 58, 0x2A)) {
                                FR16(fr_, 58, 0x18) = FR16(fr_, 58, 0x18) + 1;
                            }
                            FR16(fr_, 58, 0x32) = FR16(fr_, 58, 0x32) - 1;
                        }
                        FP16(FR32(fr_, 58, 0x26), 0) = (u16)((u16)((i32)(i16)(FP16(FR32(fr_, 58, 0x26), 0)) / (i16)FR16(fr_, 58, 0x18)));
                    }
                } else {
                    FP16(FR32(fr_, 58, 0x26), 0) = 0;
                }
            }
            FR16(fr_, 58, 0x2C) = 0;
            FR16(fr_, 58, 0x32) = 0;
            while (((i16)FR16(fr_, 58, 0x32)) < 3) {
                LOOP_TICK(53);
                if ((g_trends(FR16(fr_, 58, 0x32) * 2)) == ((u16)((u16)(i8)(G8(FR16(fr_, 58, 0x34) + 0x71E6))))) {
                    FR16(fr_, 58, 0x2C) = 0xFFFF;
                    t107 = (u16)((u16)((g_trends((FR16(fr_, 58, 0x32) * 2) + 1)) * (FP16(FR32(fr_, 58, 0x26), 0))) / 0xA);
                    FP16(FR32(fr_, 58, 0x26), 0) = t107;
                    FR16(fr_, 58, 0x30) = 0;
                    while (((i16)FR16(fr_, 58, 0x30)) < 2) {
                        LOOP_TICK(19);
                        g_ai_trends((FR16(fr_, 58, 0x30) * 3) + FR16(fr_, 58, 0x32)) = (u16)(i8)(G8(FR16(fr_, 58, 0x34) + 0x71E6));
                        FR16(fr_, 58, 0x30) = FR16(fr_, 58, 0x30) + 1;
                    }
                }
                FR16(fr_, 58, 0x32) = FR16(fr_, 58, 0x32) + 1;
            }
            if (FR16(fr_, 58, 0x2C) == 0) {
                FR16(fr_, 58, 0x30) = 0;
                while (((i16)FR16(fr_, 58, 0x30)) < 2) {
                    LOOP_TICK(37);
                    FR16(fr_, 58, 0x32) = 0;
                    while (((i16)FR16(fr_, 58, 0x32)) < 3) {
                        LOOP_TICK(32);
                        if ((g_ai_trends((FR16(fr_, 58, 0x30) * 3) + FR16(fr_, 58, 0x32))) == ((u16)((u16)(i8)(G8(FR16(fr_, 58, 0x34) + 0x71E6))))) {
                            g_ai_trends((FR16(fr_, 58, 0x30) * 3) + FR16(fr_, 58, 0x32)) = 0;
                        }
                        FR16(fr_, 58, 0x32) = FR16(fr_, 58, 0x32) + 1;
                    }
                    FR16(fr_, 58, 0x30) = FR16(fr_, 58, 0x30) + 1;
                }
            }
            FR16(fr_, 58, 0x34) = FR16(fr_, 58, 0x34) + 1;
        }
        FR16(fr_, 58, 0x34) = 1;
        while (((i16)FR16(fr_, 58, 0x34)) <= 3) {
            LOOP_TICK(88);
            t121 = (FR16(fr_, 58, 0x34) << 1) + 0x24A1;
            FR16(fr_, 58, 0x24) = DSEG;
            FR16(fr_, 58, 0x26) = t121;
            if ((players(FR16(fr_, 58, 0x34)).out) == 0 && (G8(FR16(fr_, 58, 0x34) + 0x71E6)) != 0) {
                FR16(fr_, 58, 0x36) = 0;
                FR16(fr_, 58, 0x32) = 1;
                while (((i16)FR16(fr_, 58, 0x32)) <= 3) {
                    LOOP_TICK(21);
                    if (!((players(FR16(fr_, 58, 0x32)).out) != 0 || (G8(FR16(fr_, 58, 0x34) + 0x71E6)) != (G8(FR16(fr_, 58, 0x32) + 0x71E6)))) {
                        FR16(fr_, 58, 0x36) = FR16(fr_, 58, 0x36) + (g_program_score(FR16(fr_, 58, 0x32)));
                    }
                    FR16(fr_, 58, 0x32) = FR16(fr_, 58, 0x32) + 1;
                }
                t132 = G8(((((u16)(i8)(G8(FR16(fr_, 58, 0x34) + 0x71E6))) << 3) + 0x7160) + FR16(fr_, 58, 0x14));
                FR16(fr_, 58, 0x28) = G8(((((u16)(i8)(G8(FR16(fr_, 58, 0x34) + 0x71E6))) << 3) + 0x7160) + FR16(fr_, 58, 0x14));
                if (((i16)t132) < ((i16)FR16(fr_, 58, 0x36))) {
                    FP16(FR32(fr_, 58, 0x26), 0) = (u16)((u16)((i32)(i16)((FP16(FR32(fr_, 58, 0x26), 0)) * FR16(fr_, 58, 0x28)) / (i16)FR16(fr_, 58, 0x36)));
                }
                FP16(FR32(fr_, 58, 0x26), 0) = (u16)((u16)((i32)(i16)((FP16(FR32(fr_, 58, 0x26), 0)) * FR16(fr_, 58, 0x28)) / 100));
                if ((FP16(FR32(fr_, 58, 0x26), 0)) != 0) {
                    t150 = (u16)((i32)(i16)((u16)(i8)(g_news_score(FR16(fr_, 58, 0x34)))) / 5);
                    FP16(FR32(fr_, 58, 0x26), 0) = (FP16(FR32(fr_, 58, 0x26), 0)) + t150;
                }
            }
            FR16(fr_, 58, 0x34) = FR16(fr_, 58, 0x34) + 1;
        }
        t154 = program_total_share();
        FR16(fr_, 58, 0x38) = t154;
        FR16(fr_, 58, 0x36) = 0;
        FR16(fr_, 58, 0x34) = 1;
        ax = t154;
        while (((i16)FR16(fr_, 58, 0x34)) <= 3) {
            LOOP_TICK(18);
            if ((players(FR16(fr_, 58, 0x34)).out) != 0) {
                ax = 0;
            } else {
                ax = g_program_score(FR16(fr_, 58, 0x34));
            }
            FR16(fr_, 58, 0x36) = FR16(fr_, 58, 0x36) + ax;
            FR16(fr_, 58, 0x34) = FR16(fr_, 58, 0x34) + 1;
        }
        FR16(fr_, 58, 0x34) = 1;
        while (((i16)FR16(fr_, 58, 0x34)) <= 3) {
            LOOP_TICK(85);
            if (FR16(fr_, 58, 0x36) != 0) {
                t163 = (u16)((i32)(i16)((g_program_score(FR16(fr_, 58, 0x34))) * FR16(fr_, 58, 0x38)) / (i16)FR16(fr_, 58, 0x36));
                ratings(FR16(fr_, 58, 0x34)).share = t163;
                t169 = (u32)(((u32)((i32)((u32)(tx(FR16(fr_, 58, 0x34), 0).reach * MK32(((i16)(ratings(FR16(fr_, 58, 0x34)).share)) >> 0xF, ratings(FR16(fr_, 58, 0x34)).share))) / (i32)0x64)) * 0x9A0);
                G32((u16)((FR16(fr_, 58, 0x34) * 0xA) + 0x24AD) + 2) = t169;
                ax = LO16(t169);
            } else {
                ratings(FR16(fr_, 58, 0x34)).share = 0;
                t175 = FR16(fr_, 58, 0x34) * 0xA;
                G32((u16)((FR16(fr_, 58, 0x34) * 0xA) + 0x24AD) + 2) = 0;
                ax = t175;
            }
            FR16(fr_, 58, 0x34) = FR16(fr_, 58, 0x34) + 1;
        }
        FR16(fr_, 58, 0x34) = 1;
        while (((i16)FR16(fr_, 58, 0x34)) <= 3) {
            LOOP_TICK(32);
            t179 = ratings(FR16(fr_, 58, 0x34)).share;
            owned_scripts(0x35, (FR16(fr_, 58, 0x34) * 2) + 4).script = ratings(FR16(fr_, 58, 0x34)).share;
            ax = t179;
            if (!(g_sammy_category(0) != 2 || (G8(FR16(fr_, 58, 0x34) + 0x71E6)) != 6)) {
                t182 = sammy_daily_update();
                ax = t182;
                if (t182 == 1) {
                    g_sammy_category(FR16(fr_, 58, 0x34)) = (g_sammy_category(FR16(fr_, 58, 0x34))) + 0xA;
                }
            }
            FR16(fr_, 58, 0x34) = FR16(fr_, 58, 0x34) + 1;
        }
        FR16(fr_, 58, 0x34) = 1;
        while (((i16)FR16(fr_, 58, 0x34)) <= 3) {
            LOOP_TICK(108);
            if ((schedule_program(FR16(fr_, 58, 0x34), FR16(fr_, 58, 0x14))) == 0x3A) {
                players(FR16(fr_, 58, 0x34)).betty_love = (players(FR16(fr_, 58, 0x34)).betty_love) + 5;
                if (((i16)(players(FR16(fr_, 58, 0x34)).betty_love)) > ((i16)(players(FR16(fr_, 58, 0x34)).image))) {
                    players(FR16(fr_, 58, 0x34)).betty_love = players(FR16(fr_, 58, 0x34)).image;
                }
                ui_draw_betty_meter();
            }
            G32((u16)(((FR16(fr_, 58, 0x34) << 5) + 0x74EA) + (FR16(fr_, 58, 0x14) << 2))) = ratings(FR16(fr_, 58, 0x34)).viewers;
            t200 = ratings(FR16(fr_, 58, 0x34)).share;
            G16(((FR16(fr_, 58, 0x34) * 0x1E) + 0x1C88) + ((FR16(fr_, 58, 0x14) + 7) << 1)) = ratings(FR16(fr_, 58, 0x34)).share;
            FR16(fr_, 58, 0x34) = FR16(fr_, 58, 0x34) + 1;
            ax = t200;
        }
        FR16(fr_, 58, 0x34) = 1;
        while (((i16)FR16(fr_, 58, 0x34)) <= 3) {
            LOOP_TICK(106);
            ax = bp_ - 0x20;
            if ((FRX16(fr_, 58, 0x20, (FR16(fr_, 58, 0x34) << 1) + 0)) != 0) {
                program_withdraw(FRX16(fr_, 58, 0x20, (FR16(fr_, 58, 0x34) << 1) + 0));
                FR16(fr_, 58, 0x32) = 1;
                while (((i16)FR16(fr_, 58, 0x32)) <= 12) {
                    LOOP_TICK(55);
                    t208 = (FR16(fr_, 58, 0x32) << 4) + 0x6165;
                    FR16(fr_, 58, 0x6) = DSEG;
                    FR16(fr_, 58, 0x8) = t208;
                    if ((FP8(FR32(fr_, 58, 0x8), 0xE)) != 0) {
                        if ((FRX16(fr_, 58, 0x20, (FR16(fr_, 58, 0x34) << 1) + 0)) == (screenplays(FP16(FR32(fr_, 58, 0x8), 0)).target)) {
L_16DC:
                            FP8(FR32(fr_, 58, 0x8), 0xE) = 0;
                            if ((FP8(FR32(fr_, 58, 0x8), 0xF)) != 0xFF) {
                                if (((i8)(FP8(FR32(fr_, 58, 0x8), 0xF))) > 0) {
                                    /* ORIGINAL BUG (fix "studio-takeover", 24B8:170C push [bp-34h]):
                                     * the studio argument is the player loop index (1..3), not the
                                     * studio loop index [bp-32h] (as 1DA8:034E passes it): the queued
                                     * renter gets studio 1, 2 or 3 -- a public room's door -- and
                                     * the studio he queued for stays free and reserved. */
                                    studio_take_over(fix_on(FIX_STUDIO_TAKEOVER) ? FR16(fr_, 58, 0x32) : FR16(fr_, 58, 0x34),
                                                     (u16)(i8)(FP8(FR32(fr_, 58, 0x8), 0xF)));
                                }
                            } else {
                                studio_release(FR16(fr_, 58, 0x32));
                            }
                        } else {
                            /* ORIGINAL BUG (fix "trendy-studios", 24B8:16D5 cmp 8Ah): a live show
                             * lets its own studio go when it has aired (the match above), but
                             * film 138 "Trendy" matches every running studio, whoever airs it:
                             * a deferred give-up or a queued renter takes a studio mid-production
                             * (the production is lost, the end day stays and later "finishes"
                             * screenplay 0 for renter 0xFF).  Trendy's own studio is the match
                             * above: screenplay 24 makes film 138. */
                            if ((FRX16(fr_, 58, 0x20, (FR16(fr_, 58, 0x34) << 1) + 0)) == 0x8A && !fix_on(FIX_TRENDY_STUDIOS)) goto L_16DC;
                        }
                    }
                    FR16(fr_, 58, 0x32) = FR16(fr_, 58, 0x32) + 1;
                }
                ax = bp_ - 0x20;
                if ((FRX16(fr_, 58, 0x20, (FR16(fr_, 58, 0x34) << 1) + 0)) == 0x8A) {
                    t214 = FR16(fr_, 58, 0x34);
                    g_trendy_player = FR16(fr_, 58, 0x34);
                    ax = t214;
                }
                if (g_room == ROOM_PLANNER) {
                    ax = g_room_owner;
                    if (((u32)MK32(g_room_owner, g_room_mode)) == ((u32)(MK32(FR16(fr_, 58, 0x34), 2)))) {
                        ui_library_genre_popup(0x13, 8, 1, 0);
                        ui_library_genre_popup(0x13, 8, 0, g_room_owner);
                        ax = present_frame();
                    }
                }
            }
            FR16(fr_, 58, 0x34) = FR16(fr_, 58, 0x34) + 1;
        }
        rating_rank_hour(ax);
    } else if (a0 == 3) {
        if (g_trendy_player != 0) {
            if (!mp_is_human(g_trendy_player)) {   /* != g_player (multiplayer: no human) */
                FR16(fr_, 58, 0x34) = 0;
                while (((i16)FR16(fr_, 58, 0x34)) < 2 && ((u16)((u16)(i8)(g_ai_player(FR16(fr_, 58, 0x34))))) != g_trendy_player) {
                    LOOP_TICK(8);
                    FR16(fr_, 58, 0x34) = FR16(fr_, 58, 0x34) + 1;
                }
                g_trendy_player = (u16)(i8)(g_ai_player(FR16(fr_, 58, 0x34)));
                /* ORIGINAL BUG (fix "ai-trendy"): g_ai_trends is indexed by the
                 * computer player's index (0/1), found just above, but written
                 * by its player number: player 2 writes g_ai_odd_today, player 3
                 * also g_ai_pending_gift (a Betty gift nobody bought) */
                u16 trend_ai = fix_on(FIX_AI_TRENDY) ? FR16(fr_, 58, 0x34) : g_trendy_player;
                FR16(fr_, 58, 0x34) = 0;
                while (((i16)FR16(fr_, 58, 0x34)) < 3) {
                    LOOP_TICK(23);
                    g_ai_trends((trend_ai * 3) + FR16(fr_, 58, 0x34)) = g_trends(FR16(fr_, 58, 0x34) * 2);
                    FR16(fr_, 58, 0x34) = FR16(fr_, 58, 0x34) + 1;
                }
            } else {
                msg_post(0x15F, g_trendy_player, MK32(0, g_trends(2)), MK32(0, g_trends(4)), MK32(0, g_trends(0)), 0, 0);
            }
            g_trendy_player = 0;
        }
        FR16(fr_, 58, 0x34) = 1;
        while (((i16)FR16(fr_, 58, 0x34)) <= 3) {
            LOOP_TICK(263);
            t223 = (FR16(fr_, 58, 0x34) * 0x5A) + 0x4417;
            FR16(fr_, 58, 0xE) = DSEG;
            FR16(fr_, 58, 0x10) = t223;
            FR16(fr_, 58, 0x2C) = 0;
            if ((players(FR16(fr_, 58, 0x34)).out) == 0) {
                t227 = FP16(FR32(fr_, 58, 0x10), (FR16(fr_, 58, 0x14) << 1) + 0x3C);
                FR16(fr_, 58, 0x22) = FP16(FR32(fr_, 58, 0x10), (FR16(fr_, 58, 0x14) << 1) + 0x3C);
                if (t227 != 0) {
                    t232 = FPOFF(ads_at(FR16(fr_, 58, 0x22)));
                    FR16(fr_, 58, 0x2) = FPSEG(ads_at(FR16(fr_, 58, 0x22)));
                    FR16(fr_, 58, 0x4) = t232;
                    FR16(fr_, 58, 0x2E) = 0;
                    while (((i16)(g_player_ad_count(FR16(fr_, 58, 0x34)))) > ((i16)FR16(fr_, 58, 0x2E)) && (contracts(FR16(fr_, 58, 0x34), FR16(fr_, 58, 0x2E)).ad) != FR16(fr_, 58, 0x22)) {
                        LOOP_TICK(20);
                        FR16(fr_, 58, 0x2E) = FR16(fr_, 58, 0x2E) + 1;
                    }
                    if ((FP8(FR32(fr_, 58, 0x4), 0xA)) == 1) {
L_1948:
                        if ((FP8(FR32(fr_, 58, 0x4), 0xA)) == 1) {
                            if ((FP16(FR32(fr_, 58, 0x10), FR16(fr_, 58, 0x14) << 1)) != 0) {
L_1962:
                                FR16(fr_, 58, 0x2C) = 1;
                            }
                        }
                    } else {
                        t236 = round_to(ratings(FR16(fr_, 58, 0x34)).viewers, 0x186A0);
                        ax = LO16(t236);
                        bx = (((u16)(i8)(FP8(FR32(fr_, 58, 0x4), 0xA))) - 1) << 2;
                        if (((i32)t236) < ((i32)(G32(((((u16)(i8)(FP8(FR32(fr_, 58, 0x4), 0xA))) - 1) << 2) + 0xA8)))) {
                            goto L_1948;
                        } else {
                            goto L_1962;
                        }
                    }
                    if (FR16(fr_, 58, 0x2C) == 1) {
                        FR16(fr_, 58, 0x2C) = ad_target_group_ok(FR16(fr_, 58, 0x22), (u16)(i8)(G8(FR16(fr_, 58, 0x34) + 0x71E6)));
                    }
                    if (FR16(fr_, 58, 0x2C) == 1) {
                        t243 = ((FR16(fr_, 58, 0x34) * 0x46) + 0x1B70) + (FR16(fr_, 58, 0x2E) * 7);
                        FR16(fr_, 58, 0xA) = DSEG;
                        FR16(fr_, 58, 0xC) = t243;
                        FP8(FR32(fr_, 58, 0xC), 4) = (FP8(FR32(fr_, 58, 0xC), 4)) + 1;
                        if (((i8)(FP8(FR32(fr_, 58, 0xC), 4))) >= ((i8)(FP8(FR32(fr_, 58, 0x4), 9)))) {
                            G32((u16)((FR16(fr_, 58, 0x34) * 0x17) + 0x8BDC) + 8) = (players(FR16(fr_, 58, 0x34)).money) + (FP32(FR32(fr_, 58, 0x4), 0x10));
                            int as_human = mp_enter_player(FR16(fr_, 58, 0x34));   /* multiplayer: as that human: its ledger, its planner */
                            if (FR16(fr_, 58, 0x34) != g_player) {
                                ai_budget_distribute((u16)(i8)FR8(fr_, 58, 0x34), FP16(FR32(fr_, 58, 0x4), 0x10), FP16(FR32(fr_, 58, 0x4), 0x12), FR16(fr_, 58, 0x2E));
                            } else {
                                ledger_add(LEDGER_AD_INCOME, FP32(FR32(fr_, 58, 0x4), 0x10));
                            }
                            ui_draw_money();
                            FP16(FR32(fr_, 58, 0x10), (FR16(fr_, 58, 0x14) << 1) + 0x3C) = (FP16(FR32(fr_, 58, 0x10), (FR16(fr_, 58, 0x14) << 1) + 0x3C)) + 0x7D0;
                            FR16(fr_, 58, 0x12) = FR16(fr_, 58, 0x14) + 1;
                            while (((i16)FR16(fr_, 58, 0x12)) <= 14) {
                                LOOP_TICK(16);
                                if ((FP16(FR32(fr_, 58, 0x10), (FR16(fr_, 58, 0x12) << 1) + 0x3C)) == (FP16(FR32(fr_, 58, 0xC), 0))) {
                                    FP16(FR32(fr_, 58, 0x10), (FR16(fr_, 58, 0x12) << 1) + 0x3C) = 0;
                                }
                                FR16(fr_, 58, 0x12) = FR16(fr_, 58, 0x12) + 1;
                            }
                            FP8(FR32(fr_, 58, 0x4), 0xF) = 0;
                            contract_delete(FR16(fr_, 58, 0x34), FR16(fr_, 58, 0x2E));
                            if (((u32)(MK32(FR16(fr_, 58, 0x34), g_room))) == ((u32)MK32(g_player, 0x1F)) && g_room_mode == 6) {
                                if (FR16(fr_, 58, 0x22) == g_selected_program) {
                                    ui_drag_end();
                                    g_room_mode = 0;
                                }
                            }
                            if (((u32)(MK32(FR16(fr_, 58, 0x34), g_room))) == ((u32)MK32(g_room_owner, 0x1F)) && g_room_mode == 5) {
                                ui_contract_popup(0x16, 1, 1, g_room_owner);
                                ui_contract_popup(0x16, 1, 0, g_room_owner);
                                present_frame();
                            }
                            if (as_human) mp_leave();
                        } else {
                            FP16(FR32(fr_, 58, 0x10), (FR16(fr_, 58, 0x14) << 1) + 0x3C) = (FP16(FR32(fr_, 58, 0x10), (FR16(fr_, 58, 0x14) << 1) + 0x3C)) + 0xBB8;
                        }
                    } else {
                        FP16(FR32(fr_, 58, 0x10), (FR16(fr_, 58, 0x14) << 1) + 0x3C) = (FP16(FR32(fr_, 58, 0x10), (FR16(fr_, 58, 0x14) << 1) + 0x3C)) + 0x3E8;
                        if (((u16)((u16)(i8)g_ai_player(0))) == FR16(fr_, 58, 0x34) || ((u16)((u16)(i8)g_ai_player(1))) == FR16(fr_, 58, 0x34)) {
                            ad_plan_book(FR16(fr_, 58, 0x14), FR16(fr_, 58, 0x2E), 0, FR16(fr_, 58, 0x34));
                        }
                    }
                }
            }
            FR16(fr_, 58, 0x34) = FR16(fr_, 58, 0x34) + 1;
        }
    }
    return;
}

/* 24B8:1B5C  shift the trend history and pick a new random trend genre 1..15 (not 10, 15 or one of the last three) */
void trend_pick_new(void) {
    u16 l_6 = 0;
    u16 l_4 = 0;
    u16 l_2 = 0;
    FUNC_TICK(12);
    l_2 = 0xFFFF;
    l_6 = 0;
    while (((i16)l_6) < 3) {
        LOOP_TICK(20);
        g_trends(l_6 * 2) = g_trends((l_6 * 2) + 2);
        l_6 = l_6 + 1;
    }
    while (l_2 != 0) {
        LOOP_TICK(30);
        l_2 = 0;
        l_4 = random_n(0xF, l_2);
        l_6 = 0;
        while (((i16)l_6) < 3) {
            LOOP_TICK(13);
            if ((g_trends(l_6 * 2)) == l_4) {
                l_2 = 0xFFFF;
            }
            l_6 = l_6 + 1;
        }
        if (!(l_4 == 0xA || l_4 == 0xF)) continue;
        l_2 = 0xFFFF;
    }
    g_trends(6) = l_4;
    return;
}

/* 24B8:1BFD  (player, hour): program id covering hour slot (walks back over 9999 continuation blocks) */
u16 sched_program_at(u16 a0, u16 a1) {
    u16 ax;
    u16 l_2;
    FUNC_TICK(21);
    l_2 = a1;
    while (schedule_program(a0, l_2) == 0x270F) {
        LOOP_TICK(13);
        l_2 = l_2 - 1;
    }
    ax = schedule_program(a0, l_2);
    return ax;
}

/* 24B8:1C4C  (actor_id, n): far ptr to actor string n (2 = name, 1 = second string) from the actor text bank, "<not set>" if missing */
u32 actor_text(u16 a0, u16 a1) {
    u16 ax;
    u16 dx;
    u16 l_2;
    FUNC_TICK(26);
    l_2 = MEM16(FPSEG(g_actor_txt), (FPOFF(g_actor_txt) + ((a0 + 0xFF68) << 2)) - (a1 << 1));
    if (l_2 == 0) {
        ax = 0x12DE;
        dx = DSEG;
    } else {
        ax = g_actor_txt_lo + l_2;
        dx = g_actor_txt_hi;
    }
    return MK32(dx, ax);
}

/* 24B8:1C8C  count empty program/ad slots today; weakest AI (lowest boss rating, days 2..39) gets +100000 budget; a player with boss rating < 5 is fired (g_fired_player); a fired AI's rating is given to the others (human half if 3 active, else all) */
void daily_boss_update(void) {
    u16 l_2;
    u16 l_4;
    u16 l_6;
    u16 l_8;
    u16 l_A;
    u16 t3;
    if (seat_humans) { mp_daily_boss_update(); return; }   /* multiplayer: for every human (src/seatctx.c) */
    FUNC_TICK(133);
    G32(0x7A02) = 0;
    l_A = 1;
    while (((i16)l_A) <= 7) {
        LOOP_TICK(29);
        if (schedule_program(g_player, l_A) == 0) {
            g_boss_cancelled_hours = g_boss_cancelled_hours + 1;
        }
        if (schedule_ad(g_player, l_A) == 0) {
            g_boss_missed_ads = g_boss_missed_ads + 1;
        }
        l_A = l_A + 1;
    }
    l_8 = 0x64;
    l_6 = 0;
    l_A = 1;
    while (((i16)l_A) <= 3) {
        LOOP_TICK(24);
        if (((i16)(players(l_A).image)) <= ((i16)l_8)) {
            l_8 = players(l_A).image;
            l_6 = l_A;
        }
        l_A = l_A + 1;
    }
    if (!(l_6 == 0 || l_6 == g_player || ((i16)g_day) <= 1 || ((i16)g_day) >= 40)) {
        ai_budget_distribute(l_6, 0x86A0, 1, 0xFFFF);
    }
    g_fired_player(0) = 0;
    l_A = 1;
    while (((i16)l_A) <= 3) {
        LOOP_TICK(23);
        if (!(((i16)(players(l_A).image)) >= 5 || (players(l_A).out) != 0)) {
            g_fired_player(0) = l_A;
        }
        l_A = l_A + 1;
    }
    if (((i16)(players(g_player).image)) < 5) {
        g_fired_player(0) = g_player;
    }
    if (g_fired_player(0) != 0 && g_fired_player(0) != g_player && ((i16)(players(g_fired_player(0)).image)) > 0) {
        l_4 = 0;
        l_A = 1;
        while (((i16)l_A) <= 3) {
            LOOP_TICK(13);
            if ((players(l_A).out) == 0) {
                l_4 = l_4 + 1;
            }
            l_A = l_A + 1;
        }
        if (l_4 == 3) {
            t3 = players(g_fired_player(0)).image;
            l_2 = players(g_fired_player(0)).image;
            players(g_player).image = (players(g_player).image) + (((i16)t3) >> 1);
            l_A = 1;
            while (((i16)l_A) <= 3) {
                LOOP_TICK(9);
                if (l_A == g_fired_player(0) || l_A == g_player) {
                    l_A = l_A + 1;
                    continue;
                } else {
                    goto L_1E8C;
                }
            }
        } else {
            players(g_player).image = (players(g_player).image) + (players(g_fired_player(0)).image);
        }
        players(g_fired_player(0)).image = 0;
    }
    return;
L_1E8C:
    players(l_A).image = (players(l_A).image) + (l_2 - (((i16)l_2) >> 1));
    players(g_fired_player(0)).image = 0;
    return;
}
