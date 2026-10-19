/* News and schedule: content table loading, the news engine, hour and day transitions of the schedule, money available, auction increment.
 *
 * Original module 1C18 of MTV.EXE (Rainbow Arts, 1991).  Decompiled by
 * tools/decomp.py and hand-maintained since the freeze (git tag
 * generated-final).  Every function keeps its original address in the
 * comment above it; that address ties it to the machine-level twin in gen/
 * and to the difftest tables -- keep it when editing.
 */
#include "game.h"
#include "seat.h"

/* A table resource with more bytes than the records the game has room for (films,
 * series, ads: state.c table_count; the others: the original's fixed counts) -- a
 * hand-made one: more records than FILMS_MAX, or a size that is no multiple of a
 * record -- is read only as far as its block holds (an extension: the original data,
 * exactly that size, reads whole with res_read, in the original's time) */
static void table_read(u16 res, u8 *buf, u16 n, u16 rec) {
    u16 want = (u16)((n + 1) * rec);
    if (FP16(g_res_index, (res << 3) + 2) > want) res_read_part(res, buf, 0, want, res);
    else res_read(res, buf, res);
}

/* 1C18:000C  load films (res 0xC9 -> g_movies), series (0xCA), screenplays (0xCB), ads (0xCC -> 0x3F4F), news chain nodes (0xCD -> 0x2731), news topics (0xCE -> 0x5593), news templates (0x30 -> 0x3AA1); then the language strings (ad names/clients, 10 phrases per news topic) into a string pool; then script_init_episodes */
void data_load_tables(void) {
    u16 l_2;
    u16 l_4;
    u16 l_6;
    u32 l_A;
    u16 t1;
    u16 t2;
    u32 t3;
    u16 t4;
    u16 t5;
    u32 t6;
    u16 t18;
    u16 t19;
    u32 t20;
    u16 t29;
    FUNC_TICK(164);
    t1 = res_exists(G16((g_language << 4) + 0x56C), 1);
    t2 = res_exists(G16((g_language << 4) + 0x568), t1);
    t3 = mem_calloc(t1 + t2, 1);
    g_strpool = t3;
    disk_access_begin(LO16(t3));
    table_read(0xC9, HPTR(g_movies), g_nfilms, 0x17);
    table_read(0xCA, HPTR(g_series), g_nseries, 0x39);
    table_read(0xCB, HPTR(g_scripts), 50, 0x1B3);
    if (g_nads <= 50) {
        table_read(0xCC, HPTR(MK32(DSEG, 0x3F4F)), 50, 0x18);
    } else {                             /* a mod's ads 51.. into the host array (state.c, an extension) */
        res_read_part(0xCC, HPTR(MK32(DSEG, 0x3F4F)), 0, 51 * 0x18, 0xCC);
        res_read_part(0xCC, g_ads_extra, 51 * 0x18, (g_nads - 50) * 0x18, 0xCC);
    }
    table_read(0xCD, HPTR(MK32(DSEG, 0x2731)), 400, 0xA);
    table_read(0xCE, HPTR(MK32(DSEG, 0x5593)), 50, 0x38);
    table_read(0x30, HPTR(MK32(DSEG, 0x3AA1)), 50, 0xE);
    g_initdata_pos = 0;
    t4 = G16((g_language << 4) + 0x568);
    l_6 = G16((g_language << 4) + 0x568);
    t5 = res_exists(t4, t4);
    t6 = mem_calloc(t5, 1);
    g_initdata = t6;
    res_read(l_6, HPTR(g_initdata), LO16(t6));
    l_A = MK32(FPSEG(g_initdata), FPOFF(g_initdata) + g_initdata_pos);
    g_initdata_pos = g_initdata_pos + (g_nads + 1) * 2;   /* the advertisers' lengths: 0x66 for 50 ads */
    l_4 = 1;
    while (((i16)l_4) <= (i16)g_nads) {
        LOOP_TICK(22);
        if (FP16(l_A, l_4 << 1) != 0) {
            strpool_add(FP16(l_A, l_4 << 1), ads_at(l_4));
        }
        l_4 = l_4 + 1;
    }
    l_A = MK32(FPSEG(g_initdata), FPOFF(g_initdata) + g_initdata_pos);
    g_initdata_pos = g_initdata_pos + (g_nads + 1) * 2;   /* the clients' lengths */
    l_4 = 1;
    while (((i16)l_4) <= (i16)g_nads) {
        LOOP_TICK(24);
        if (FP16(l_A, l_4 << 1) != 0) {
            strpool_add(FP16(l_A, l_4 << 1), FPADD(ads_at(l_4), 4));
        }
        l_4 = l_4 + 1;
    }
    mem_free(g_initdata_lo, g_initdata_hi);
    t18 = G16((g_language << 4) + 0x56C);
    l_6 = G16((g_language << 4) + 0x56C);
    t19 = res_exists(t18, t18);
    t20 = mem_calloc(t19, 1);
    g_initdata = t20;
    g_initdata_pos = 0;
    res_read(l_6, HPTR(g_initdata), LO16(t20));
    l_A = g_initdata;
    g_initdata_pos = g_initdata_pos + 0x462;
    l_4 = 1;
    while (((i16)l_4) <= 50) {
        LOOP_TICK(38);
        l_2 = 1;
        while (((i16)l_2) <= 10) {
            LOOP_TICK(33);
            if ((FP16(l_A, ((l_4 * 0xA) + l_2) << 1)) != 0) {
                strpool_add(FP16(l_A, ((l_4 * 0xA) + l_2) << 1), MK32(DSEG, ((l_4 * 0x38) + 0x5593) + (l_2 << 2)));
            }
            l_2 = l_2 + 1;
        }
        l_4 = l_4 + 1;
    }
    t29 = mem_free(g_initdata_lo, g_initdata_hi);
    disk_access_end(t29);
    script_init_episodes();
    return;
}

/* 1C18:02C6  (len, far **dst) copy len bytes from the init-data buffer (g_initdata + g_initdata_pos) into the string pool, store the far pointer in *dst, advance both positions (+1 for the NUL) */
void strpool_add(u16 a0, u32 a1) {
    FUNC_TICK(28);
    far_memcpy(HPTR(MK32(g_strpool_hi, g_strpool_lo + g_strpool_pos)), HPTR(MK32(g_initdata_hi, g_initdata_lo + g_initdata_pos)), a0);
    g_initdata_pos = g_initdata_pos + a0;
    FP32(a1, 0) = MK32(g_strpool_hi, g_strpool_lo + g_strpool_pos);
    g_strpool_pos = g_strpool_pos + (a0 + 1);
    return;
}

/* 1C18:0313  for single (non-series, +5 bit0 clear) screenplays with flag +5 bit2: copy episode block 0 (0x23 bytes at +8) to episodes 1..9 */
void script_init_episodes(void) {
    u32 l_4;
    u16 l_6;
    u16 l_8;
    FUNC_TICK(9);
    l_8 = 1;
    while (((i16)l_8) <= 50) {
        LOOP_TICK(49);
        l_4 = screenplays_at(l_8);
        if (!(((u16)(((i16)((AT(screenplay, l_4).flags_16) << 0xF)) >> 0xF)) != 0 || ((u16)(((i16)((AT(screenplay, l_4).flags_16) << 0xD)) >> 0xF)) == 0)) {
            l_6 = 1;
            while (((i16)l_6) < 10) {
                LOOP_TICK(21);
                far_memcpy(HPTR(MK32(HI16(l_4), (LO16(l_4) + (l_6 * 0x23)) + 8)), HPTR(MK32(HI16(l_4), LO16(l_4) + 8)), 0x23);
                l_6 = l_6 + 1;
            }
        }
        l_8 = l_8 + 1;
    }
    return;
}

/* 1C18:03A7  (category, amount lo, hi) g_ledger[category] += amount (today's finance statistics of the human); returns lo */
u16 ledger_add(u16 a0, u32 a1) {
    FUNC_TICK(11);
    g_ledger(a0) = g_ledger(a0) + a1;
    return LO16(a1);
}

/* 1C18:03C1  (hours, min_cat1) age the 7 running news stories by hours, emit every story that is due (news_advance_story(0)), start new stories, then force stories of category 1 until at least min_cat1 category-1 items exist today; finally news_distribute. Called hourly with (1,0), at day start with (5,3) */
void news_tick(u16 a0, u16 a1) {
    u16 l_2;
    u16 l_4;
    FUNC_TICK(13);
    l_4 = 0;
    while (((i16)l_4) < 7) {
        LOOP_TICK(23);
        if ((stories(l_4).template) != 0) {
            stories(l_4).countdown = (stories(l_4).countdown) - a0;
        }
        l_4 = l_4 + 1;
    }
    for (;;) {
        LOOP_TICK(7);
        l_2 = news_advance_story(0);
        if (l_2 != 0xFFFF) {
            continue;
        } else {
            break;
        }
    }
    news_start_stories();
    l_2 = 0;
    while (!(((i16)(news_count_cat1())) >= ((i16)a1) || l_2 == 0xFFFF)) {
        LOOP_TICK(10);
        l_2 = news_advance_story(0x3E8);
    }
    news_start_stories();
    news_distribute();
    return;
}

/* 1C18:044F  number of today's news items (g_news_pool) with category 1 */
u16 news_count_cat1(void) {
    u16 ax;
    u16 l_2;
    u16 l_4;
    FUNC_TICK(11);
    l_2 = 0;
    l_4 = 0;
    while (((i16)l_4) < ((i16)g_news_pool_count)) {
        LOOP_TICK(14);
        if ((news_pool(l_4).category) == NEWS_POLITICS) {
            l_2 = l_2 + 1;
        }
        l_4 = l_4 + 1;
    }
    ax = l_2;
    return ax;
}

/* 1C18:0490  (story slot) append a news item to today's pool (20 max, oldest dropped): node, 5 (topic, phrase) parts, value = node value * (30 + 70*avg phrase value/10) / 100, price class = number of thresholds {15,30,40,50,60,100} below the value, category, day, hour */
void news_emit(u16 a0) {
    u16 ax;
    u32 l_4;
    u32 l_8;
    u32 l_C;
    u32 l_10;
    u16 l_12;
    u16 l_14;
    u16 l_16;
    FUNC_TICK(104);
    if (((i16)g_news_pool_count) > 19) {
        l_16 = 1;
        while (((i16)l_16) <= 19) {
            LOOP_TICK(20);
            far_memcpy(HPTR(MK32(DSEG, ((l_16 - 1) * 0x1E) + 0x36DB)), HPTR(news_pool_at(l_16)), 0x1E);
            l_16 = l_16 + 1;
        }
        g_news_pool_count = g_news_pool_count - 1;
    }
    l_10 = news_pool_at(g_news_pool_count);
    l_C = stories_at(a0);
    AT(news_item, l_10).node = AT(story, l_C).node;
    l_4 = MK32(DSEG, ((AT(story, l_C).node) * 0xA) + 0x2731);
    l_8 = MK32(DSEG, ((AT(story, l_C).template) * 0xE) + 0x3AA1);
    l_14 = 0;
    l_12 = 0;
    l_16 = 0;
    while (((i16)l_16) < 5) {
        LOOP_TICK(66);
        FP16(l_10, (l_16 << 2) + 2) = (u16)((u16)((i32)(i16)FP16(l_8, l_16 << 1) % 1000));
        FP16(l_10, (l_16 << 2) + 4) = FP16(l_C, (l_16 << 1) + 6);
        if (FP16(l_8, l_16 << 1) != 0) {
            if (((i16)FP16(l_8, l_16 << 1)) < 1000) {
                l_14 = l_14 + ((u16)(i8)(G8((((FP16(l_8, l_16 << 1) * 0x38) + 0x5593) + (FP16(l_C, (l_16 << 1) + 6))) + 0x2C)));
                l_12 = l_12 + 0xA;
            }
        }
        l_16 = l_16 + 1;
    }
    if (l_12 == 0) {
        ax = (u16)(i8)FP8(l_4, 8);
    } else {
        ax = (u16)((u16)((i32)(i16)((u16)((u16)(i8)FP8(l_4, 8)) * ((u16)((u16)((i32)(i16)(l_14 * 0x46) / (i16)l_12)) + 0x1E)) / 100));
    }
    AT(news_item, l_10).value = ax;
    l_16 = 0;
    while (((i16)(AT(news_item, l_10).value)) > ((i16)((u16)(i8)g_news_price_steps(l_16)))) {
        LOOP_TICK(10);
        l_16 = l_16 + 1;
    }
    AT(news_item, l_10).price_class = (u8)l_16;
    AT(news_item, l_10).category = stories(a0).category;
    AT(news_item, l_10).day = g_day;
    AT(news_item, l_10).hour = g_time_slot;
    g_news_pool_count = g_news_pool_count + 1;
    return;
}

/* 1C18:069A  (node, topic, variant) append a special news item for a game event (e.g. node 0xA6..0xA8 about a player) with the node's base value, category 1 */
void news_add_event(u16 a0, u16 a1, u16 a2) {
    u32 l_4;
    u16 l_6;
    FUNC_TICK(49);
    if (((i16)g_news_pool_count) > 19) {
        l_6 = 1;
        while (((i16)l_6) <= 19) {
            LOOP_TICK(20);
            far_memcpy(HPTR(MK32(DSEG, ((l_6 - 1) * 0x1E) + 0x36DB)), HPTR(news_pool_at(l_6)), 0x1E);
            l_6 = l_6 + 1;
        }
        g_news_pool_count = g_news_pool_count - 1;
    }
    l_4 = news_pool_at(g_news_pool_count);
    AT(news_item, l_4).node = a0;
    AT(news_item, l_4).value = (u16)(i8)(G8((a0 * 0xA) + 0x2739));
    AT(news_item, l_4).parts = a1;
    FP16(l_4, 4) = a2;
    AT(news_item, l_4).category = 1;
    AT(news_item, l_4).day = g_day;
    AT(news_item, l_4).hour = g_time_slot;
    g_news_pool_count = g_news_pool_count + 1;
    return;
}

/* 1C18:0755  (limit) pick the running story with the smallest countdown <= limit (limit != 0: only category 1), emit its news, move it to one of the node's 4 random successors (9999 = story ends, slot freed); countdown += node delay (= delay when forced). Returns slot or -1 */
u16 news_advance_story(u16 a0) {
    u16 ax;
    u32 l_4;
    u16 l_6;
    u16 l_8;
    u16 l_A;
    u16 l_C;
    u16 t1;
    FUNC_TICK(69);
    l_C = 0xFFFF;
    l_A = a0 + 1;
    l_8 = 0;
    while (((i16)l_8) < 7) {
        LOOP_TICK(33);
        l_4 = stories_at(l_8);
        if (!((AT(story, l_4).template) == 0 || ((i16)(AT(story, l_4).countdown)) > ((i16)a0) || ((i16)(AT(story, l_4).countdown)) >= ((i16)l_A) || !(a0 == 0 || (AT(story, l_4).category) == 1))) {
            l_A = AT(story, l_4).countdown;
            l_C = l_8;
        }
        l_8 = l_8 + 1;
    }
    if (l_C != 0xFFFF) {
        news_emit(l_C);
        l_4 = stories_at(l_C);
        l_6 = 0;
        while (l_6 == 0) {
            LOOP_TICK(23);
            t1 = random_n(4, l_8);
            l_6 = G16((((AT(story, l_4).node) * 0xA) + 0x2731) + ((t1 - 1) << 1));
        }
        if (l_6 == 0x270F) {
            AT(story, l_4).last_template = AT(story, l_4).template;
            AT(story, l_4).template = 0;
        } else {
            AT(story, l_4).node = l_6;
            if (a0 == 0) {
                AT(story, l_4).countdown = (AT(story, l_4).countdown) + ((u16)(i8)(G8((l_6 * 0xA) + 0x273A)));
            } else {
                AT(story, l_4).countdown = (u16)(i8)(G8((l_6 * 0xA) + 0x273A));
            }
        }
    }
    ax = l_C;
    return ax;
}

/* 1C18:08A2  fill free story slots: random unused template (start node > 0) of the slot's category, re-enabling all (+1000) when none is left; node = start, countdown 3, phrase variant per part random 1..10 (non-empty, not used by another running story) or same as previous part */
void news_start_stories(void) {
    u16 ax;
    u32 l_4;
    u32 l_8;
    u16 l_A;
    u16 l_C;
    u16 l_E;
    u16 t12;
    u16 t14;
    u16 t24;
    FUNC_TICK(17);
    l_E = 0;
    while (((i16)l_E) < 7) {
        LOOP_TICK(219);
        l_8 = stories_at(l_E);
        if ((AT(story, l_8).template) == 0) {
            l_C = 1;
            while (((i16)l_C) <= 50 && (((i16)(tx(0x10, l_C + 0xD).state)) <= 0 || (G8((l_C * 0xE) + 0x3AAE)) != (AT(story, l_8).category))) {
                LOOP_TICK(23);
                l_C = l_C + 1;
            }
            if (((i16)l_C) > 50) {
                l_C = 1;
                while (((i16)l_C) <= 50) {
                    LOOP_TICK(22);
                    if ((G8((l_C * 0xE) + 0x3AAE)) == (AT(story, l_8).category)) {
                        tx(0x10, l_C + 0xD).state = (tx(0x10, l_C + 0xD).state) + 0x3E8;
                    }
                    l_C = l_C + 1;
                }
            }
            l_A = 0;
            while (l_A == 0) {
                LOOP_TICK(33);
                l_A = random_n(0x32, l_E);
                if (((i16)(tx(0x10, l_A + 0xD).state)) < 1) {
                    l_A = 0;
                }
                if (l_A == 0 || (G8((l_A * 0xE) + 0x3AAE)) == (AT(story, l_8).category)) continue;
                l_A = 0;
            }
            l_4 = MK32(DSEG, (l_A * 0xE) + 0x3AA1);
            AT(story, l_8).template = l_A;
            AT(story, l_8).node = FP16(l_4, 0xA);
            FP16(l_4, 0xA) = FP16(l_4, 0xA) - 0x3E8;
            AT(story, l_8).countdown = 3;
            l_C = 0;
            while (((i16)l_C) < 5) {
                LOOP_TICK(96);
                if (FP16(l_4, l_C << 1) != 0) {
                    FP16(l_8, (l_C << 1) + 6) = 0;
                    t12 = (u16)((i32)(i16)FP16(l_4, l_C << 1) / 1000);
                    ax = t12;
                    if (t12 == 0) {
L_0A54:
                        l_A = 0;
                        while (l_A == 0) {
                            LOOP_TICK(54);
                            t14 = random_n(0xA, l_C);
                            l_A = t14;
                            if ((G8(((((u16)((u16)((i32)(i16)FP16(l_4, l_C << 1) % 1000)) * 0x38) + 0x5593) + t14) + 0x2C)) != 0) {
                                t24 = (u16)((i32)(i16)FP16(l_4, l_C << 1) % 1000);
                                if ((news_variant_in_use(t24, l_A)) == 0) {
                                    FP16(l_8, (l_C << 1) + 6) = l_A;
                                    continue;
                                } else {
                                    l_A = 0;
                                    continue;
                                }
                            } else {
                                l_A = 0;
                                continue;
                            }
                        }
                    } else if (ax != 1) {
                        if (ax == 2) goto L_0A54;
                    } else {
                        FP16(l_8, (l_C << 1) + 6) = FP16(l_8, ((l_C - 1) << 1) + 6);
                    }
                }
                l_C = l_C + 1;
            }
        }
        l_E = l_E + 1;
    }
    return;
}

/* 1C18:0B21  (topic, variant) 1 if a running story uses that phrase variant for that topic */
u16 news_variant_in_use(u16 a0, u16 a1) {
    u16 ax;
    u32 l_4;
    u32 l_8;
    u16 l_A;
    u16 l_C;
    u16 l_E;
    u16 l_10;
    FUNC_TICK(12);
    l_A = 0;
    l_10 = 1;
    while (((i16)l_10) < 8) {
        LOOP_TICK(53);
        l_8 = stories_at(l_10);
        l_C = stories(l_10).template;
        l_4 = MK32(DSEG, (l_C * 0xE) + 0x3AA1);
        l_E = 0;
        while (((i16)l_E) < 5) {
            LOOP_TICK(21);
            if ((u16)((u16)((i32)(i16)FP16(l_4, l_E << 1) % 1000)) == a0) {
                if ((FP16(l_8, (l_E << 1) + 6)) == a1) {
                    l_A = 1;
                }
            }
            l_E = l_E + 1;
        }
        l_10 = l_10 + 1;
    }
    ax = l_A;
    return ax;
}

/* 1C18:0BCB  (player) g_news_score[player] = average value of the player's 3 news broadcast slots */
void news_rate_player(u16 a0) {
    u16 l_2;
    u16 l_4;
    FUNC_TICK(15);
    l_2 = 0;
    l_4 = 1;
    while (((i16)l_4) <= 3) {
        LOOP_TICK(16);
        l_2 = l_2 + (news_pool(((a0 * 4) + l_4) + 0x38).value);
        l_4 = l_4 + 1;
    }
    g_news_score(a0) = (u8)(u16)((u16)((i32)(i16)l_2 / 3));
    return;
}

/* 1C18:0C1B  on leaving the movie agency: auction lots without bidder (< 0) become the human's bids (bid += auction_increment) */
void auction_commit_bids(void) {
    u32 l_4;
    u16 l_6;
    u32 t1;
    FUNC_TICK(8);
    l_6 = 1;
    while (((i16)l_6) <= 10) {
        LOOP_TICK(24);
        l_4 = auction_at(l_6);
        if (((i8)(AT(lot, l_4).bidder)) < 0) {
            AT(lot, l_4).bidder = G8(0x8BA8);
            t1 = auction_increment(l_6);
            AT(lot, l_4).bid = (AT(lot, l_4).bid) + t1;
        }
        l_6 = l_6 + 1;
    }
    return;
}

/* 1C18:0C73  (player) if it is the human: one more message in the office message list (max 4, oldest scrolled out of 0x60BF) */
void msg_count_new(u16 a0) {
    u16 l_2;
    if (seat_humans && a0 != g_player && mp_enter_player(a0)) {   /* multiplayer: that human's office */
        msg_count_new(a0);
        mp_leave();
        return;
    }
    FUNC_TICK(15);
    if (a0 == g_player) {
        g_message_pos = g_message_pos + 1;
        if (((i8)g_message_pos) > 4) {
            l_2 = 1;
            while (((i16)l_2) <= 4) {
                LOOP_TICK(20);
                far_memcpy(HPTR(MK32(DSEG, ((l_2 - 1) * 0x1A) + 0x60BF)), HPTR(messages_at(l_2)), 0x1A);
                l_2 = l_2 + 1;
            }
            g_message_pos = 4;
        }
    }
    return;
}

/* 1C18:0CCD  (unused arg) for every player whose program ends in the current hour: series -> record the aired episode in the slot (+0x1E), wear it and advance the next episode (wrap to 1 after the last existing one, max 10); film -> film_apply_wear */
void sched_end_of_hour(u16 a0) {
    u32 l_4;
    u32 l_8;
    u16 l_A;
    u16 l_C;
    u16 l_E;
    u16 l_10;
    u16 t3;
    u16 t4;
    u16 t7;
    FUNC_TICK(13);
    l_10 = 1;
    while (((i16)l_10) <= 3) {
        LOOP_TICK(125);
        l_4 = schedule_at(l_10);
        l_C = clock_hour();
        for (;;) {
            LOOP_TICK(15);
            t3 = schedule_program(l_10, l_C);
            l_E = schedule_program(l_10, l_C);
            if (t3 != 0x270F) break;
            l_C = l_C - 1;
        }
        if (((i16)l_E) > 1000 && ((i16)l_E) < 2000) {
            t4 = clock_hour();
            if (t4 == ((u16)((((u16)(i8)series_blocks(l_E + 0xFC18)) + l_C) - 1))) {
                l_8 = MK32(FPSEG(g_series), FPOFF(g_series) + ((l_E + 0xFC18) * 0x39));
                if (FP16(l_8, 0x31) == 0) {
                    FP16(l_8, 0x31) = 1;
                }
                FP16(l_4, (l_C << 1) + 0x1E) = FP16(l_8, 0x31);
                series_wear_episode(l_E + 0xFC18, FP16(l_8, 0x31));
                FP16(l_8, 0x31) = FP16(l_8, 0x31) + 1;
                l_A = 1;
                while (((u16)(((i16)((FP16(l_8, ((l_A + 1) * 3) + 2)) << 0xF)) >> 0xF)) != 0) {
                    LOOP_TICK(14);
                    l_A = l_A + 1;
                }
                if (((i16)l_A) > 10) {
                    l_A = 0xA;
                }
                if (((i16)FP16(l_8, 0x31)) > ((i16)l_A)) {
                    FP16(l_8, 0x31) = 1;
                }
            } else {
L_0E01:
                if (!(((i16)l_E) <= 0 || ((i16)l_E) >= 1000)) {
                    t7 = clock_hour();
                    if (t7 == ((u16)((((u16)(i8)(films(l_E).blocks)) + l_C) - 1))) {
                        film_apply_wear(l_E);
                    }
                }
            }
        } else {
            goto L_0E01;
        }
        l_10 = l_10 + 1;
    }
    return;
}

/* 1C18:0E49  copy all schedules to g_schedule_prev, move tomorrow's slots 8..14 (programs, episodes, ads) to today's 1..7 and clear 8..14; reset hour counter and door block */
void sched_new_day(void) {
    u32 l_4;
    u16 l_6;
    u16 l_8;
    FUNC_TICK(24);
    G8(0x71F0) = 0;
    g_last_hour = 0;
    far_memcpy(HPTR(DSP(0x3939)), HPTR(DSP(0x4417)), 0x168);
    l_8 = 1;
    while (((i16)l_8) <= 3) {
        LOOP_TICK(68);
        l_4 = schedule_at(l_8);
        l_6 = 1;
        while (((i16)l_6) <= 7) {
            LOOP_TICK(54);
            FP16(l_4, l_6 << 1) = FP16(l_4, (l_6 + 7) << 1);
            FP16(l_4, (l_6 << 1) + 0x1E) = FP16(l_4, ((l_6 + 7) << 1) + 0x1E);
            FP16(l_4, (l_6 << 1) + 0x3C) = FP16(l_4, ((l_6 + 7) << 1) + 0x3C);
            FP16(l_4, (l_6 + 7) << 1) = 0;
            FP16(l_4, ((l_6 + 7) << 1) + 0x1E) = 0;
            FP16(l_4, ((l_6 + 7) << 1) + 0x3C) = 0;
            l_6 = l_6 + 1;
        }
        l_8 = l_8 + 1;
    }
    g_boss_talk_step = 0;
    g_door_block = 0;
    return;
}

/* 1C18:0F51  copy today's news items into the inbox (20 max, oldest dropped) of every player subscribed to the item's category; empty today's pool */
void news_distribute(void) {
    u16 l_2;
    u16 l_4;
    FUNC_TICK(14);
    if (((i16)g_news_pool_count) > 0) {
        l_4 = 1;
        while (((i16)l_4) <= 3) {
            LOOP_TICK(69);
            l_2 = 0;
            while (((i16)l_2) < ((i16)g_news_pool_count)) {
                LOOP_TICK(64);
                if ((G8((((l_4 * 0x17) + 0x8BDC) + ((u16)(i8)(news_pool(l_2).category))) + 0x10)) != 0) {
                    if (((i16)g_player_news_count(l_4)) >= 20) {
                        news_inbox_drop_oldest(l_4);
                    }
                    far_memcpy(HPTR(inbox_at(l_4, g_player_news_count(l_4))), HPTR(news_pool_at(l_2)), 0x1E);
                    g_player_news_count(l_4) = g_player_news_count(l_4) + 1;
                }
                l_2 = l_2 + 1;
            }
            l_4 = l_4 + 1;
        }
        g_news_pool_count = 0;
    }
    return;
}

/* 1C18:1022  (pct_inbox, pct_slots) news values in all inboxes *= pct_inbox/100 (price class recomputed), in the 3 broadcast slots *= pct_slots/100; hourly (85,85), at day start (0,0) */
void news_age(u16 a0, u16 a1) {
    u32 l_4;
    u16 l_6;
    u16 l_8;
    u16 l_A;
    FUNC_TICK(10);
    l_A = 1;
    while (((i16)l_A) <= 3) {
        LOOP_TICK(79);
        l_6 = 0;
        while (((i16)g_player_news_count(l_A)) >= ((i16)l_6)) {
            LOOP_TICK(45);
            l_4 = inbox_at(l_A, l_6);
            if (FP16(l_4, 0x16) != 0) {
                FP16(l_4, 0x16) = (u16)((u16)((i32)(i16)(FP16(l_4, 0x16) * a0) / 100));
                l_8 = 0;
                while (((i16)FP16(l_4, 0x16)) > ((i16)((u16)(i8)G8(l_8 + 0x11EE)))) {
                    LOOP_TICK(10);
                    l_8 = l_8 + 1;
                }
                FP8(l_4, 0x1D) = (u8)l_8;
            }
            l_6 = l_6 + 1;
        }
        l_6 = 1;
        while (((i16)l_6) <= 3) {
            LOOP_TICK(27);
            l_4 = news_pool_at(((l_A * 4) + l_6) + 0x38);
            if (FP16(l_4, 0x16) != 0) {
                FP16(l_4, 0x16) = (u16)((u16)((i32)(i16)(FP16(l_4, 0x16) * a1) / 100));
            }
            l_6 = l_6 + 1;
        }
        l_A = l_A + 1;
    }
    return;
}

/* 1C18:1116  (player, series, hour) episode that airs at hour: next episode (+0x31) plus earlier same-day slots of this series without recorded episode, wrapped by the number of existing episodes */
u16 series_episode_at(u16 a0, u16 a1, u16 a2) {
    u16 ax;
    u32 l_4;
    u16 l_6;
    u16 l_8;
    u16 l_A;
    FUNC_TICK(33);
    l_6 = 0;
    l_4 = series_at(a1);
    l_8 = 1;
    while (!(((i16)l_8) > 10 || ((u16)(((i16)((FP16(l_4, (l_8 * 3) + 2)) << 0xF)) >> 0xF)) == 0)) {
        LOOP_TICK(15);
        l_8 = l_8 + 1;
    }
    l_6 = l_8 - 1;
    if ((AT(series, l_4).next_ep) == 0) {
        AT(series, l_4).next_ep = SGENRE_FAMILY;
    }
    l_A = (AT(series, l_4).next_ep) - 1;
    l_8 = 1;
    while (((i16)l_8) <= ((i16)a2)) {
        LOOP_TICK(32);
        if (schedule_program(a0, l_8) == ((u16)(a1 + 0x3E8))) {
            if (schedule_episode(a0, l_8) == 0) {
                l_A = l_A + 1;
            }
        }
        l_8 = l_8 + 1;
    }
    while (((i16)l_A) > ((i16)l_6)) {
        LOOP_TICK(5);
        l_A = l_A - l_6;
    }
    ax = l_A;
    return ax;
}

/* 1C18:11F8  (player) cash minus unpaid new transmitters (state 1) minus the player's own auction bids; for the human also minus bid+increment of lots without bidder */
u32 money_available(u16 a0) {
    u16 ax;
    u16 dx;
    u32 l_4;
    u32 l_8;
    u16 l_A;
    u32 l_E;
    u32 t4;
    FUNC_TICK(25);
    l_E = players(a0).money;
    l_A = 1;
    while (((i16)l_A) < 32) {
        LOOP_TICK(24);
        l_8 = tx_at(a0, l_A);
        if ((AT(transmitter, l_8).state) == 1) {
            l_E = l_E - (AT(transmitter, l_8).fee);
        }
        l_A = l_A + 1;
    }
    l_A = 1;
    while (((i16)l_A) <= 10) {
        LOOP_TICK(40);
        l_4 = auction_at(l_A);
        if ((AT(lot, l_4).program) != 0) {
            if (((u16)((u16)(i8)(AT(lot, l_4).bidder))) != a0) {
                if (!(((i8)(AT(lot, l_4).bidder)) >= 0 || a0 != g_player)) {
                    t4 = auction_increment(l_A);
                    l_E = l_E - (t4 + (AT(lot, l_4).bid));
                }
            } else {
                l_E = l_E - (AT(lot, l_4).bid);
            }
        }
        l_A = l_A + 1;
    }
    ax = LO16(l_E);
    dx = HI16(l_E);
    return MK32(dx, ax);
}

/* 1C18:12F2  (id) remove film/series (id >= 1000) from every player's library; owner = 0 and flags |= 0x20 (back on the market); live films (genre 7) keep owner/flags */
void program_withdraw(u16 a0) {
    u16 l_2;
    u16 l_4;
    u16 l_6;
    u16 l_8;
    FUNC_TICK(55);
    if (((i16)a0) >= 1000) {
        a0 = a0 - 0x3E8;
        series(a0).owner = 0;
        series(a0).flags_16 = (series(a0).flags_16) | 0x20;
        l_4 = 0xB;
    } else {
        if ((films(a0).genre) != GENRE_LIVE) {
            films(a0).owner = 0;
            films(a0).flags_16 = (films(a0).flags_16) | 0x20;
        }
        l_4 = (u16)(i8)(films(a0).genre);
    }
    l_8 = 1;
    while (((i16)l_8) <= 3) {
        LOOP_TICK(96);
        l_6 = 0;
        while (((i16)((u16)(i8)library_count(l_8, l_4))) > ((i16)l_6)) {
            LOOP_TICK(91);
            if (library_items(l_8, l_4, l_6) == a0) {
                l_2 = l_6;
                while (((i16)((u16)(i8)library_count(l_8, l_4))) > ((i16)l_2)) {
                    LOOP_TICK(48);
                    library_items(l_8, l_4, l_2) = library_items(l_8, l_4, l_2 + 1);
                    l_2 = l_2 + 1;
                }
                library_count(l_8, l_4) = library_count(l_8, l_4) - 1;
            }
            l_6 = l_6 + 1;
        }
        l_8 = l_8 + 1;
    }
    return;
}

/* 1C18:147E  (player) delete inbox item 0 */
void news_inbox_drop_oldest(u16 a0) {
    u16 l_2;
    FUNC_TICK(11);
    l_2 = 1;
    while (((i16)g_player_news_count(a0)) > ((i16)l_2)) {
        LOOP_TICK(41);
        far_memcpy(HPTR(MK32(DSEG, ((a0 * 0x258) + 0x457F) + ((l_2 - 1) * 0x1E))), HPTR(inbox_at(a0, l_2)), 0x1E);
        l_2 = l_2 + 1;
    }
    g_player_news_count(a0) = g_player_news_count(a0) - 1;
    return;
}

/* 1C18:14F0  (player, idx) delete inbox item idx */
void news_inbox_delete(u16 a0, u16 a1) {
    FUNC_TICK(47);
    far_memcpy(HPTR(inbox_at(a0, a1)), HPTR(inbox_at(a0, a1 + 1)), (g_player_news_count(a0) - a1) * 0x1E);
    g_player_news_count(a0) = g_player_news_count(a0) - 1;
    return;
}

/* 1C18:1552  (player, idx) delete owned screenplay idx (5-byte entries, count g_scripts_owned_count) */
void script_list_delete(u16 a0, u16 a1) {
    u16 l_2;
    FUNC_TICK(12);
    l_2 = a1;
    while (((i16)g_scripts_owned_count(a0)) >= ((i16)l_2)) {
        LOOP_TICK(41);
        far_memcpy(HPTR(owned_scripts_at(a0, l_2)), HPTR(owned_scripts_at(a0, l_2 + 1)), 5);
        l_2 = l_2 + 1;
    }
    g_scripts_owned_count(a0) = g_scripts_owned_count(a0) - 1;
    return;
}

/* 1C18:15C5  (player, idx) delete shopping list idx (0xF-byte entries at 0x734C) */
void shoplist_delete(u16 a0, u16 a1) {
    FUNC_TICK(47);
    far_memcpy(HPTR(shoplists_at(a0, a1)), HPTR(shoplists_at(a0, a1 + 1)), (g_shoplist_count(a0) - a1) * 0xF);
    g_shoplist_count(a0) = g_shoplist_count(a0) - 1;
    return;
}

/* 1C18:1627  (player, idx) delete ad contract idx (and, for a computer player, its g_ai_ad_plan entry) */
void contract_delete(u16 a0, u16 a1) {
    u16 l_2;
    FUNC_TICK(36);
    if (g_player_ad_count(a0) != 0) {
        l_2 = a1;
        while (((i16)g_player_ad_count(a0)) > ((i16)l_2)) {
            LOOP_TICK(84);
            far_memcpy(HPTR(contracts_at(a0, l_2)), HPTR(contracts_at(a0, l_2 + 1)), 7);
            if (((u16)((u16)(i8)g_ai_player(0))) == a0 || ((u16)((u16)(i8)g_ai_player(1))) == a0) {
                far_memcpy(HPTR(ai_ad_plan_at(a0, l_2)), HPTR(ai_ad_plan_at(a0, l_2 + 1)), 7);
            }
            l_2 = l_2 + 1;
        }
        contracts(a0, g_player_ad_count(a0)).ad = 0;
        g_player_ad_count(a0) = g_player_ad_count(a0) - 1;
    }
    return;
}

/* 1C18:172F  (series, ep) episode topicality -= quality * wear(+0x33) / 20, min 1 (after airing) */
void series_wear_episode(u16 a0, u16 a1) {
    u32 l_4;
    u32 l_8;
    u16 t4;
    FUNC_TICK(40);
    l_8 = series_at(a0);
    l_4 = MK32(FPSEG(l_8), FPOFF(l_8) + (a1 * 3));
    t4 = (u16)((i32)(i16)(((u16)(i8)(AT(series, l_8).wear_rate)) * ((u16)(i8)FP8(l_4, 0))) / 20);
    FP8(l_4, 1) = FP8(l_4, 1) - ((u8)t4);
    if (((i8)FP8(l_4, 1)) < 1) {
        FP8(l_4, 1) = 1;
    }
    return;
}

/* 1C18:1794  (film) topicality(+3) -= quality(+1) * wear(+2) / 5, min 1 (after airing) */
void film_apply_wear(u16 a0) {
    u32 l_4;
    u16 t4;
    FUNC_TICK(33);
    l_4 = films_at(a0);
    t4 = (u16)((i32)(i16)(((u16)(i8)(AT(film, l_4).quality)) * ((u16)(i8)(AT(film, l_4).wear_rate))) / 5);
    AT(film, l_4).topicality = (AT(film, l_4).topicality) - ((u8)t4);
    if (((i8)(AT(film, l_4).topicality)) < GENRE_LOVE) {
        AT(film, l_4).topicality = GENRE_LOVE;
    }
    return;
}

/* 1C18:17E7  (lot) minimum raise: 10% of the current bid rounded down to its leading digit (100000s / 10000s / 1000s / 100s / 10s) */
u32 auction_increment(u16 a0) {
    u16 ax;
    u16 dx;
    u32 l_4;
    u32 t4;
    u32 t7;
    u32 t10;
    u32 t13;
    u32 t16;
    FUNC_TICK(105);
    l_4 = ((u32)((i32)auction(a0).bid / (i32)0xA));
    if (((i32)l_4) > 100000) {
        t4 = (u32)(((u32)((i32)l_4 / (i32)0x186A0)) * 0x186A0);
        ax = LO16(t4);
        dx = HI16(t4);
    } else if (((i32)l_4) > 10000) {
        t7 = (u32)(((u32)((i32)l_4 / (i32)0x2710)) * 0x2710);
        ax = LO16(t7);
        dx = HI16(t7);
    } else if (((i32)l_4) > 1000) {
        t10 = (u32)(((u32)((i32)l_4 / (i32)0x3E8)) * 0x3E8);
        ax = LO16(t10);
        dx = HI16(t10);
    } else if (((i32)l_4) > 100) {
        t13 = (u32)(((u32)((i32)l_4 / (i32)0x64)) * 0x64);
        ax = LO16(t13);
        dx = HI16(t13);
    } else if (((i32)l_4) > 10) {
        t16 = (u32)(((u32)((i32)l_4 / (i32)0xA)) * 0xA);
        ax = LO16(t16);
        dx = HI16(t16);
    } else {
        ax = LO16(l_4);
        dx = HI16(l_4);
    }
    return MK32(dx, ax);
}
