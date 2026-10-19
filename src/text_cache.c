/* Text caches: film, series, screenplay and news texts loaded on demand from the language resources.
 *
 * Original module 048C of MTV.EXE (Rainbow Arts, 1991).  Decompiled by
 * tools/decomp.py and hand-maintained since the freeze (git tag
 * generated-final).  Every function keeps its original address in the
 * comment above it; that address ties it to the machine-level twin in gen/
 * and to the difftest tables -- keep it when editing.
 */
#include "game.h"
#include "fixes.h"

/* 048C:0007  load the texts of player a0's news inbox, broadcast slots (and the dragged item in the news room) into g_news_txt_cache */
u16 txt_news_cache_load(u16 a0, u16 ax) {
    u16 l_2;
    u16 l_4;
    u16 l_6;
    u32 t1;
    u16 t2;
    u16 t3;
    u16 t4;
    u16 t7;
    u16 t8;
    u16 t11;
    u16 t12;
    u16 t13;
    u16 t14;
    u16 t19;
    FUNC_TICK(58);
    l_4 = 0;
    disk_access_begin(ax);
    t1 = mem_calloc(0x322, 1);
    g_txt_index = t1;
    t2 = G16((g_language << 4) + 0x56A);
    t3 = g_txt_index_hi;
    t4 = g_txt_index_lo;
    g_txt_res = G16((g_language << 4) + 0x56A);
    res_read_part(t2, HPTR(MK32(t3, t4)), 0, 0x322, t2);
    l_6 = 0;
    while (((i16)g_player_news_count(a0)) > ((i16)l_6)) {
        LOOP_TICK(24);
        t7 = inbox(a0, l_6).headline;
        t8 = l_4;
        l_4 = l_4 + 1;
        txt_news_cache_load_one(t8, t7);
        l_6 = l_6 + 1;
    }
    l_6 = 1;
    while (((i16)l_6) <= 3) {
        LOOP_TICK(25);
        t11 = news_pool(((a0 * 4) + l_6) + 0x38).node;
        l_2 = news_pool(((a0 * 4) + l_6) + 0x38).node;
        if (t11 != 0) {
            t12 = l_4;
            l_4 = l_4 + 1;
            txt_news_cache_load_one(t12, l_2);
        }
        l_6 = l_6 + 1;
    }
    if (((u32)MK32(g_room, g_room_mode)) == 0x20002) {
        t13 = g_news_drag;
        t14 = l_4;
        l_4 = l_4 + 1;
        txt_news_cache_load_one(t14, t13);
    }
    l_6 = l_4;
    while (((i16)l_6) < 24) {
        LOOP_TICK(12);
        G32((l_6 * 6) + 0x72BC) = 0;
        l_6 = l_6 + 1;
    }
    t19 = mem_free(g_txt_index_lo, g_txt_index_hi);
    return disk_access_end(t19);
}

/* 048C:0147  (slot, news_id): read one news text from the language news-text resource into cache slot */
void txt_news_cache_load_one(u16 a0, u16 a1) {
    u16 l_2;
    FUNC_TICK(81);
    l_2 = FP16(g_txt_index, a1 << 1) - (FP16(g_txt_index, (a1 - 1) << 1));
    g_news_txt_cache(a0 * 3) = a1;
    if (l_2 == 0) {
        G32((a0 * 6) + 0x72BC) = 0;
    } else {
        G32((a0 * 6) + 0x72BC) = mem_calloc(l_2 + 1, 1);
        res_read_part(g_txt_res, HPTR(MK32(g_news_txt_cache((a0 * 3) + 2), g_news_txt_cache((a0 * 3) + 1))), FP16(g_txt_index, (a1 - 1) << 1), l_2, a0 * 6);
    }
    return;
}

/* 048C:020F  free all 24 news text cache entries */
u16 txt_news_cache_free(u16 ax) {
    u16 l_2;
    FUNC_TICK(8);
    l_2 = 0;
    while (((i16)l_2) < 24) {
        LOOP_TICK(25);
        ax = (g_news_txt_cache((l_2 * 3) + 1)) | (g_news_txt_cache((l_2 * 3) + 2));
        if (((g_news_txt_cache((l_2 * 3) + 1)) | (g_news_txt_cache((l_2 * 3) + 2))) != 0) {
            ax = mem_free_ptr(HPTR(MK32(DSEG, (l_2 * 6) + 0x72BC)));
        }
        l_2 = l_2 + 1;
    }
    return ax;
}

/* ORIGINAL BUG (fix "title-cache"): g_film_txt_cache has 49 slots, but the
 * own library (room 4) fills slots 3.. with the case's films and then every
 * archive film, with no bound: 3 + 10 + 40 = 53.  A film bought for a full
 * genre stays in the case, so 47 films in archive and case are reachable;
 * slots 49.. then lie on the series cache's slot 0 (DS:65CF): the last
 * archive films have no title or description, a series shows a film's
 * title and garbage episodes until slot 0 is loaded again, and the texts of
 * slots 49.. are never freed (txt_film_cache_free: 3..48).  The planner
 * (0x1F: 3 + 40 + films aired today) could do the same.  Fixed, slots 49..
 * are kept in film_txt_more, searched and freed with slots 3..48; a slot
 * past those is not loaded at all. */
FilmTxtMore film_txt_more[FILM_TXT_MORE];
static FilmTxtMore film_more_none;          /* a slot past those: never loaded, always free */

/* where slot lives with the fix: NULL in g_film_txt_cache (or classic) */
static FilmTxtMore *film_more(u16 slot) {
    if (((i16)slot) < 49 || !fix_on(FIX_TITLE_CACHE)) return NULL;
    if (slot - 49 >= FILM_TXT_MORE) { film_more_none.id = 0; return &film_more_none; }
    return &film_txt_more[slot - 49];
}

static FilmTxtMore *film_more_find(u16 film_id) {
    if (!film_id || !fix_on(FIX_TITLE_CACHE)) return NULL;
    for (int i = 0; i < FILM_TXT_MORE; i++)
        if (film_txt_more[i].id == film_id) return &film_txt_more[i];
    return NULL;
}

u32 film_txt_more_text(u16 film_id, int which) {
    FilmTxtMore *m = film_more_find(film_id);
    return m ? m->txt[which] : 0;
}

/* 048C:025B  (room, arg): load film titles/descriptions needed by a room into g_film_txt_cache (0: film arg into slot 0; 0x1F planner: library+aired schedule; 0x63/0x64: all stations' schedules; 4 library(+bag); 8 movie agency: bag, dealer, auction; 9 script agency; 5 studio; 0xC supermarket lists) */
u16 txt_film_cache_load(u16 a0, u16 a1, u16 ax) {
    u16 l_2;
    u16 l_4;
    u16 l_6;
    u16 l_8;
    u16 l_A;
    u32 t1;
    u16 t2;
    u16 t3;
    u16 t4;
    u16 t7;
    u16 t14;
    u16 t16;
    u16 t19;
    u16 t21;
    u16 t23;
    u16 t25;
    u16 t27;
    u16 t29;
    u16 t36;
    u16 t44;
    u16 t48;
    u16 t49;
    u16 t51;
    u16 t52;
    u16 t54;
    u16 t55;
    u16 t57;
    FUNC_TICK(178);
    disk_access_begin(ax);
    t1 = mem_calloc((g_nfilms + 1) * 2, 1);   /* the film texts' index: 0x192 for 200 films */
    g_txt_index = t1;
    t2 = G16((g_language << 4) + 0x562);
    t3 = g_txt_index_hi;
    t4 = g_txt_index_lo;
    g_txt_res = G16((g_language << 4) + 0x562);
    res_read_part(t2, HPTR(MK32(t3, t4)), 0, (g_nfilms + 1) * 2, t2);
    if (a0 != 0) {
        if (a0 == 0x1F) {
            l_6 = 3;
            l_A = 1;
            while (((i16)l_A) <= 10) {
                LOOP_TICK(40);
                l_8 = 0;
                while (((i16)((u16)(i8)library_count(a1, l_A))) > ((i16)l_8)) {
                    LOOP_TICK(35);
                    t7 = l_6;
                    l_6 = l_6 + 1;
                    txt_film_cache_load_one(library_items(a1, l_A, l_8), t7);
                    l_8 = l_8 + 1;
                }
                l_A = l_A + 1;
            }
            l_A = 1;
            while (((i16)(clock_hour())) >= ((i16)l_A)) {
                LOOP_TICK(32);
                t14 = schedule_program(a1, l_A);
                l_2 = schedule_program(a1, l_A);
                if (!(t14 == 0 || ((i16)l_2) >= 1000 || (txt_film_cache_find(l_2)) != 0)) {
                    t16 = l_6;
                    l_6 = l_6 + 1;
                    txt_film_cache_load_one(l_2, t16);
                }
                l_A = l_A + 1;
            }
        } else if (a0 == 0x63 || a0 == 0x64) {
            if (a0 != 0x63) {
                l_6 = 3;
                while ((film_more(l_6) ? film_more(l_6)->id : film(l_6)) != 0) {
                    LOOP_TICK(10);
                    l_6 = l_6 + 1;
                }
            } else {
                l_6 = 3;
            }
            if (((i16)g_day) > 1) {
                l_8 = 1;
                while (((i16)l_8) <= 3) {
                    LOOP_TICK(36);
                    l_A = 1;
                    while (((i16)l_A) <= 7) {
                        LOOP_TICK(31);
                        t19 = G16(((l_8 * 0x5A) + 0x3939) + (l_A << 1));
                        l_2 = G16(((l_8 * 0x5A) + 0x3939) + (l_A << 1));
                        if (!(t19 == 0 || ((i16)l_2) >= 1000 || (txt_film_cache_find(l_2)) != 0)) {
                            t21 = l_6;
                            l_6 = l_6 + 1;
                            txt_film_cache_load_one(l_2, t21);
                        }
                        l_A = l_A + 1;
                    }
                    l_8 = l_8 + 1;
                }
            }
            l_8 = 1;
            while (((i16)l_8) <= 3) {
                LOOP_TICK(36);
                l_A = 1;
                while (((i16)l_A) <= 7) {
                    LOOP_TICK(31);
                    t23 = schedule_program(l_8, l_A);
                    l_2 = schedule_program(l_8, l_A);
                    if (!(t23 == 0 || ((i16)l_2) >= 1000 || (txt_film_cache_find(l_2)) != 0)) {
                        t25 = l_6;
                        l_6 = l_6 + 1;
                        txt_film_cache_load_one(l_2, t25);
                    }
                    l_A = l_A + 1;
                }
                l_8 = l_8 + 1;
            }
        } else if (a0 == 4) {
            l_6 = 3;
            if (a1 == g_player) {
                l_A = 0;
                while (((i16)l_A) < ((i16)g_bag_count)) {
                    LOOP_TICK(17);
                    if (((i16)g_bag(l_A)) < 1000) {
                        t27 = l_6;
                        l_6 = l_6 + 1;
                        txt_film_cache_load_one(g_bag(l_A), t27);
                    }
                    l_A = l_A + 1;
                }
            }
            l_A = 1;
            while (((i16)l_A) <= 10) {
                LOOP_TICK(40);
                l_8 = 0;
                while (((i16)((u16)(i8)library_count(a1, l_A))) > ((i16)l_8)) {
                    LOOP_TICK(35);
                    t29 = l_6;
                    l_6 = l_6 + 1;
                    txt_film_cache_load_one(library_items(a1, l_A, l_8), t29);
                    l_8 = l_8 + 1;
                }
                l_A = l_A + 1;
            }
        } else if (a0 == 8) {
            l_6 = 3;
            l_A = 0;
            while (((i16)l_A) < ((i16)g_bag_count)) {
                LOOP_TICK(17);
                if (((i16)g_bag(l_A)) < 1000) {
                    t36 = l_6;
                    l_6 = l_6 + 1;
                    txt_film_cache_load_one(g_bag(l_A), t36);
                }
                l_A = l_A + 1;
            }
            l_A = 1;
            while (((i16)l_A) <= (i16)g_nfilmoffers) {   /* 10 in the original (--film-offers) */
                LOOP_TICK(35);
                if ((dealer(l_A).program) != 0) {
                    if (((i16)(dealer(l_A).program)) < 1000) {
                        t44 = l_6;
                        l_6 = l_6 + 1;
                        txt_film_cache_load_one(dealer(l_A).program, t44);
                    }
                }
                l_A = l_A + 1;
            }
            l_A = 1;
            while (((i16)l_A) <= 10) {
                LOOP_TICK(35);
                if (!((auction(l_A).program) == 0 || ((i16)(auction(l_A).program)) >= 1000)) {
                    t48 = l_6;
                    l_6 = l_6 + 1;
                    txt_film_cache_load_one(auction(l_A).program, t48);
                }
                l_A = l_A + 1;
            }
        } else if (a0 == 9) {
            l_6 = 3;
            l_A = 0;
            while (((i16)g_scripts_owned_count(g_player)) > ((i16)l_A)) {
                LOOP_TICK(52);
                if (((u16)(((i16)((screenplays(owned_scripts(g_player, l_A).script).flags_16) << 0xF)) >> 0xF)) == 0) {
                    t49 = l_6;
                    l_6 = l_6 + 1;
                    txt_film_cache_load_one(screenplays(owned_scripts(g_player, l_A).script).target, t49);
                }
                l_A = l_A + 1;
            }
            l_A = 1;
            while (((i16)l_A) <= 5) {
                LOOP_TICK(50);
                if (!((script_offers(l_A).script) == 0 || ((u16)(((i16)((screenplays(script_offers(l_A).script).flags_16) << 0xF)) >> 0xF)) != 0)) {
                    t51 = l_6;
                    l_6 = l_6 + 1;
                    txt_film_cache_load_one(screenplays(script_offers(l_A).script).target, t51);
                }
                l_A = l_A + 1;
            }
        } else if (a0 == 5) {
            l_6 = 3;
            if (((u16)((u16)(i8)(studios(a1).renter))) == g_player) {
                l_A = 0;
                while (((i16)g_scripts_owned_count(g_player)) > ((i16)l_A)) {
                    LOOP_TICK(52);
                    if (((u16)(((i16)((screenplays(owned_scripts(g_player, l_A).script).flags_16) << 0xF)) >> 0xF)) == 0) {
                        t52 = l_6;
                        l_6 = l_6 + 1;
                        txt_film_cache_load_one(screenplays(owned_scripts(g_player, l_A).script).target, t52);
                    }
                    l_A = l_A + 1;
                }
            }
            if (!((studios(a1).script) == 0 || ((u16)(((i16)((screenplays(studios(a1).script).flags_16) << 0xF)) >> 0xF)) != 0)) {
                t54 = l_6;
                l_6 = l_6 + 1;
                txt_film_cache_load_one(screenplays(studios(a1).script).target, t54);
            }
        } else if (a0 == 0xC) {
            l_6 = 3;
            l_A = 0;
            while (((i16)g_shoplist_count(g_player)) > ((i16)l_A)) {
                LOOP_TICK(43);
                l_4 = shoplists(g_player, l_A).script;
                if (((u16)(((i16)((screenplays(l_4).flags_16) << 0xF)) >> 0xF)) == 0) {
                    t55 = l_6;
                    l_6 = l_6 + 1;
                    txt_film_cache_load_one(screenplays(l_4).target, t55);
                }
                l_A = l_A + 1;
            }
        }
    } else {
        txt_film_cache_load_one(a1, 0);
    }
    t57 = mem_free(g_txt_index_lo, g_txt_index_hi);
    return disk_access_end(t57);
}

/* 048C:094A  0xFFFF if film id a0 is already in the film text cache */
u16 txt_film_cache_find(u16 a0) {
    u16 ax;
    u16 l_2;
    FUNC_TICK(15);
    l_2 = 0;
    while (((i16)l_2) < 49 && film(l_2) != a0) {
        LOOP_TICK(13);
        l_2 = l_2 + 1;
    }
    if (((i16)l_2) >= 49) {
        ax = 0;
        if (film_more_find(a0)) ax = 0xFFFF;   /* fix "title-cache" (above) */
    } else {
        ax = 0xFFFF;
    }
    return ax;
}

/* 048C:098F  (film_id, slot): read title and description (2 length-prefixed strings) of a film into a cache slot */
void txt_film_cache_load_one(u16 a0, u16 a1) {
    u32 l_4;
    u32 l_8;
    u32 l_C;
    u16 l_E;
    u16 l_10;
    u32 t3;
    FilmTxtMore *more = film_more(a1);     /* fix "title-cache" (above): slots 49.. */
    FUNC_TICK(71);
    if (more == &film_more_none) return;
    l_10 = FP16(g_txt_index, a0 << 1) - (FP16(g_txt_index, (a0 - 1) << 1));
    if (more) more->id = a0; else film(a1) = a0;
    t3 = mem_calloc(l_10 + 1, 1);
    l_8 = t3;
    l_C = t3;
    res_read_part(g_txt_res, HPTR(l_C), FP16(g_txt_index, (a0 - 1) << 1), l_10, (a0 - 1) << 1);
    if (a0 == 200) {                     /* data fix "film-200" (fixes.c): the damaged texts restored */
        static u8 fixed[160];
        int n = fix_film200_text(HPTR(l_C), l_10, fixed, sizeof fixed);
        if (n) {
            mem_free(LO16(l_C), HI16(l_C));
            l_C = l_8 = mem_calloc((u16)(n + 1), 1);
            far_memcpy(HPTR(l_C), fixed, (u16)n);
        }
    }
    l_4 = MK32(DSEG, (a1 * 0xA) + 0x63E7);
    l_E = 0;
    while (((i16)l_E) < 2) {
        LOOP_TICK(44);
        u32 *txt = more ? &more->txt[l_E] : &FP32(l_4, 0);
        if (FP8(l_8, 0) == 0) {
            *txt = 0;
            l_8 = MK32(HI16(l_8), LO16(l_8) + 1);
        } else {
            *txt = mem_calloc(FP8(l_8, 0) + 1, 1);
            far_memcpy(HPTR(*txt), HPTR(MK32(HI16(l_8), LO16(l_8) + 1)), FP8(l_8, 0));
            l_8 = MK32(HI16(l_8), LO16(l_8) + (FP8(l_8, 0) + 1));
        }
        l_4 = MK32(HI16(l_4), LO16(l_4) + 4);
        l_E = l_E + 1;
    }
    mem_free(LO16(l_C), HI16(l_C));
    return;
}

/* 048C:0AB5  free film text cache slots 3..48 (a0 = 0, room contents) or 0..2 (a0 != 0) */
void txt_film_cache_free(u16 a0) {
    u16 ax;
    u16 l_2;
    u16 l_4;
    u16 l_6;
    FUNC_TICK(21);
    if (a0 == 0) {
        ax = 3;
    } else {
        ax = 0;
    }
    l_6 = ax;
    if (a0 == 0) {
        ax = 0x30;
    } else {
        ax = 2;
    }
    l_4 = ax;
    l_2 = l_6;
    while (((i16)l_2) <= ((i16)l_4)) {
        LOOP_TICK(7);
        txt_film_cache_free_one(l_2);
        l_2 = l_2 + 1;
    }
    if (a0 == 0 && fix_on(FIX_TITLE_CACHE)) {   /* fix "title-cache" (above): slots 49.. with 3..48 */
        for (int i = 0; i < FILM_TXT_MORE; i++)
            if (film_txt_more[i].id || film_txt_more[i].txt[0] || film_txt_more[i].txt[1]) txt_film_cache_free_one(49 + i);
    }
    return;
}

/* 048C:0AFC  free both strings of film text cache slot a0 */
void txt_film_cache_free_one(u16 a0) {
    u32 l_4;
    FilmTxtMore *more = film_more(a0);     /* fix "title-cache" (above): slots 49.. */
    FUNC_TICK(44);
    if (more) {
        more->id = 0;
        for (int k = 0; k < 2; k++)
            if (more->txt[k]) mem_free_ptr((u8 *)&more->txt[k]);
        return;
    }
    film(a0) = 0;
    l_4 = MK32(DSEG, (a0 * 0xA) + 0x63E7);
    if ((FP16(l_4, 0) | FP16(l_4, 2)) != 0) {
        mem_free_ptr(HPTR(l_4));
    }
    l_4 = MK32(HI16(l_4), LO16(l_4) + 4);
    if ((FP16(l_4, 0) | FP16(l_4, 2)) != 0) {
        mem_free_ptr(HPTR(l_4));
    }
    return;
}

/* 048C:0B69  (room, arg): same as txt_film_cache_load for series texts (title, description, episode titles) */
u16 txt_series_cache_load(u16 a0, u16 a1, u16 ax) {
    u16 l_2;
    u16 l_4;
    u16 l_6;
    u16 l_8;
    u16 l_A;
    u16 l_C;
    u32 t1;
    u16 t2;
    u16 t3;
    u16 t4;
    u16 t7;
    u16 t11;
    u16 t13;
    u16 t16;
    u16 t18;
    u16 t20;
    u16 t22;
    u16 t24;
    u16 t25;
    u16 t26;
    u16 t30;
    u16 t35;
    u16 t36;
    u16 t37;
    u16 t38;
    u16 t39;
    u16 t40;
    u16 t42;
    u16 t43;
    u16 t44;
    u16 t45;
    u16 t47;
    u16 t48;
    u16 t49;
    u16 t51;
    FUNC_TICK(165);
    disk_access_begin(ax);
    t1 = mem_calloc((g_nseries + 1) * 2, 1);   /* the series texts' index: 0x66 for 50 series */
    g_txt_index = t1;
    t2 = G16((g_language << 4) + 0x564);
    t3 = g_txt_index_hi;
    t4 = g_txt_index_lo;
    g_txt_res = G16((g_language << 4) + 0x564);
    res_read_part(t2, HPTR(MK32(t3, t4)), 0, (g_nseries + 1) * 2, t2);
    if (a0 != 0) {
        if (a0 == 0x1F) {
            l_8 = 3;
            l_A = 0;
            while (((i16)((u16)(i8)library_count(a1, 0xB))) > ((i16)l_A)) {
                LOOP_TICK(29);
                t7 = l_8;
                l_8 = l_8 + 1;
                txt_series_cache_load_one(library_items(a1, 0xB, l_A), t7);
                l_A = l_A + 1;
            }
            l_C = 1;
            while (((i16)(clock_hour())) >= ((i16)l_C)) {
                LOOP_TICK(36);
                t11 = schedule_program(a1, l_C);
                l_4 = schedule_program(a1, l_C);
                if (!(((i16)t11) <= 1000 || ((i16)l_4) >= 2000 || (txt_series_cache_find(l_4 + 0xFC18)) != 0)) {
                    t13 = l_8;
                    l_8 = l_8 + 1;
                    txt_series_cache_load_one(l_4 + 0xFC18, t13);
                }
                l_C = l_C + 1;
            }
        } else if (a0 == 0x63 || a0 == 0x64) {
            if (a0 != 0x63) {
                l_8 = 3;
                while ((inbox(0xD, (l_8 * 3) + 0xF).quality) != 0) {
                    LOOP_TICK(10);
                    l_8 = l_8 + 1;
                }
            } else {
                l_8 = 3;
            }
            if (((i16)g_day) > 1) {
                l_A = 1;
                while (((i16)l_A) <= 3) {
                    LOOP_TICK(42);
                    l_C = 1;
                    while (((i16)l_C) <= 7) {
                        LOOP_TICK(37);
                        t16 = G16(((l_A * 0x5A) + 0x3939) + (l_C << 1));
                        l_4 = G16(((l_A * 0x5A) + 0x3939) + (l_C << 1));
                        if (!(((i16)t16) <= 1000 || ((i16)l_4) >= 2000 || (txt_series_cache_find(l_4 + 0xFC18)) != 0 || ((i16)l_8) >= 15)) {
                            t18 = l_8;
                            l_8 = l_8 + 1;
                            txt_series_cache_load_one(l_4 + 0xFC18, t18);
                        }
                        l_C = l_C + 1;
                    }
                    l_A = l_A + 1;
                }
            }
            l_A = 1;
            while (((i16)l_A) <= 3) {
                LOOP_TICK(42);
                l_C = 1;
                while (((i16)l_C) <= 7) {
                    LOOP_TICK(37);
                    t20 = schedule_program(l_A, l_C);
                    l_4 = schedule_program(l_A, l_C);
                    if (!(((i16)t20) <= 1000 || ((i16)l_4) >= 2000 || (txt_series_cache_find(l_4 + 0xFC18)) != 0 || ((i16)l_8) >= 15)) {
                        t22 = l_8;
                        l_8 = l_8 + 1;
                        txt_series_cache_load_one(l_4 + 0xFC18, t22);
                    }
                    l_C = l_C + 1;
                }
                l_A = l_A + 1;
            }
        } else if (a0 == 4) {
            l_8 = 3;
            if (a1 == g_player) {
                l_C = 0;
                while (((i16)l_C) < ((i16)g_bag_count)) {
                    LOOP_TICK(19);
                    t24 = g_bag(l_C);
                    l_4 = g_bag(l_C);
                    if (((i16)t24) > 1000) {
                        t25 = l_8;
                        l_8 = l_8 + 1;
                        txt_series_cache_load_one(l_4 + 0xFC18, t25);
                    }
                    l_C = l_C + 1;
                }
            }
            l_A = 0;
            while (((i16)((u16)(i8)library_count(a1, 0xB))) > ((i16)l_A)) {
                LOOP_TICK(29);
                t26 = l_8;
                l_8 = l_8 + 1;
                txt_series_cache_load_one(library_items(a1, 0xB, l_A), t26);
                l_A = l_A + 1;
            }
        } else if (a0 == 8) {
            l_8 = 3;
            l_C = 0;
            while (((i16)l_C) < ((i16)g_bag_count)) {
                LOOP_TICK(19);
                if (((i16)g_bag(l_C)) > 1000) {
                    t30 = l_8;
                    l_8 = l_8 + 1;
                    txt_series_cache_load_one(g_bag(l_C) + 0xFC18, t30);
                }
                l_C = l_C + 1;
            }
            l_C = 1;
            while (((i16)l_C) <= (i16)g_nfilmoffers) {   /* 10 in the original (--film-offers) */
                LOOP_TICK(25);
                if (g_nfilmoffers > 10 && ((i16)l_8) >= 15) break;   /* --film-offers: the cache has 15 places (the original does not check here) */
                t35 = dealer(l_C).program;
                l_2 = dealer(l_C).program;
                if (!(((i16)t35) <= 1000 || ((i16)l_2) >= 2000)) {
                    t36 = l_8;
                    l_8 = l_8 + 1;
                    txt_series_cache_load_one(l_2 + 0xFC18, t36);
                }
                l_C = l_C + 1;
            }
            l_C = 1;
            while (((i16)l_C) <= 10) {
                LOOP_TICK(25);
                if (g_nfilmoffers > 10 && ((i16)l_8) >= 15) break;   /* --film-offers: as above */
                t37 = auction(l_C).program;
                l_2 = auction(l_C).program;
                if (!(((i16)t37) <= 1000 || ((i16)l_2) >= 2000)) {
                    t38 = l_8;
                    l_8 = l_8 + 1;
                    txt_series_cache_load_one(l_2 + 0xFC18, t38);
                }
                l_C = l_C + 1;
            }
        } else if (a0 == 9) {
            l_8 = 3;
            l_C = 0;
            while (((i16)g_scripts_owned_count(g_player)) > ((i16)l_C)) {
                LOOP_TICK(42);
                t39 = owned_scripts(g_player, l_C).script;
                l_2 = owned_scripts(g_player, l_C).script;
                if (((u16)(((i16)((screenplays(t39).flags_16) << 0xF)) >> 0xF)) != 0) {
                    t40 = l_8;
                    l_8 = l_8 + 1;
                    txt_series_cache_load_one(screenplays(l_2).target, t40);
                }
                l_C = l_C + 1;
            }
            l_C = 1;
            while (((i16)l_C) <= 5) {
                LOOP_TICK(38);
                t42 = script_offers(l_C).script;
                l_2 = script_offers(l_C).script;
                if (!(t42 == 0 || ((u16)(((i16)((screenplays(l_2).flags_16) << 0xF)) >> 0xF)) == 0)) {
                    t43 = l_8;
                    l_8 = l_8 + 1;
                    txt_series_cache_load_one(screenplays(l_2).target, t43);
                }
                l_C = l_C + 1;
            }
        } else if (a0 == 5) {
            l_8 = 3;
            if (((u16)((u16)(i8)(studios(a1).renter))) == g_player) {
                l_C = 0;
                while (((i16)g_scripts_owned_count(g_player)) > ((i16)l_C)) {
                    LOOP_TICK(42);
                    t44 = owned_scripts(g_player, l_C).script;
                    l_2 = owned_scripts(g_player, l_C).script;
                    if (((u16)(((i16)((screenplays(t44).flags_16) << 0xF)) >> 0xF)) != 0) {
                        t45 = l_8;
                        l_8 = l_8 + 1;
                        txt_series_cache_load_one(screenplays(l_2).target, t45);
                    }
                    l_C = l_C + 1;
                }
            }
            t47 = studios(a1).script;
            l_2 = studios(a1).script;
            if (!(t47 == 0 || ((u16)(((i16)((screenplays(l_2).flags_16) << 0xF)) >> 0xF)) == 0)) {
                t48 = l_8;
                l_8 = l_8 + 1;
                txt_series_cache_load_one(screenplays(l_2).target, t48);
            }
        } else if (a0 == 0xC) {
            l_8 = 3;
            l_C = 0;
            while (((i16)g_shoplist_count(g_player)) > ((i16)l_C)) {
                LOOP_TICK(43);
                l_6 = shoplists(g_player, l_C).script;
                if (((u16)(((i16)((screenplays(l_6).flags_16) << 0xF)) >> 0xF)) != 0) {
                    t49 = l_8;
                    l_8 = l_8 + 1;
                    txt_series_cache_load_one(screenplays(l_6).target, t49);
                }
                l_C = l_C + 1;
            }
        }
    } else {
        txt_series_cache_load_one(a1, 0);
    }
    t51 = mem_free(g_txt_index_lo, g_txt_index_hi);
    return disk_access_end(t51);
}

/* 048C:11B6  0xFFFF if series id a0 is already in the series text cache */
u16 txt_series_cache_find(u16 a0) {
    u16 ax;
    u16 l_2;
    FUNC_TICK(15);
    l_2 = 0;
    while (((i16)l_2) < 15 && (inbox(0xD, (l_2 * 3) + 0xF).quality) != a0) {
        LOOP_TICK(13);
        l_2 = l_2 + 1;
    }
    if (((i16)l_2) >= 15) {
        ax = 0;
    } else {
        ax = 0xFFFF;
    }
    return ax;
}

/* 048C:11FB  (series_id, slot): read the 22 strings of a series into a cache slot */
void txt_series_cache_load_one(u16 a0, u16 a1) {
    u32 l_4;
    u32 l_8;
    u32 l_C;
    u32 l_10;
    u16 l_12;
    u16 l_14;
    u32 t7;
    FUNC_TICK(66);
    l_4 = MK32(DSEG, (a1 * 0x5A) + 0x65CF);
    l_14 = FP16(g_txt_index, a0 << 1) - (FP16(g_txt_index, (a0 - 1) << 1));
    FP16(l_4, 0) = a0;
    t7 = mem_calloc(l_14 + 1, 1);
    l_C = t7;
    l_10 = t7;
    res_read_part(g_txt_res, HPTR(l_10), FP16(g_txt_index, (a0 - 1) << 1), l_14, (a0 - 1) << 1);
    l_8 = MK32(FPSEG(l_4), FPOFF(l_4) + 2);
    l_12 = 0;
    while (l_12 < 0x16) {
        LOOP_TICK(44);
        if (FP8(l_C, 0) == 0) {
            FP32(l_8, 0) = 0;
            l_C = MK32(HI16(l_C), LO16(l_C) + 1);
        } else {
            FP32(l_8, 0) = mem_calloc(FP8(l_C, 0) + 1, 1);
            far_memcpy(HPTR(FP32(l_8, 0)), HPTR(MK32(HI16(l_C), LO16(l_C) + 1)), FP8(l_C, 0));
            l_C = MK32(HI16(l_C), LO16(l_C) + (FP8(l_C, 0) + 1));
        }
        l_8 = MK32(HI16(l_8), LO16(l_8) + 4);
        l_12 = l_12 + 1;
    }
    mem_free(LO16(l_10), HI16(l_10));
    return;
}

/* 048C:131B  free series text cache slots 3..14 (a0 = 0) or 0..2 */
u16 txt_series_cache_free(u16 a0) {
    u16 ax;
    u16 l_2;
    u16 l_4;
    u16 l_6;
    FUNC_TICK(21);
    if (a0 != 0) {
        ax = 0;
    } else {
        ax = 3;
    }
    l_6 = ax;
    if (a0 != 0) {
        ax = 2;
    } else {
        ax = 0xE;
    }
    l_4 = ax;
    l_2 = l_6;
    for (;;) {
        LOOP_TICK(7);
        ax = l_2;
        if (((i16)l_2) > ((i16)l_4)) break;
        txt_series_cache_free_one(l_2);
        l_2 = l_2 + 1;
    }
    return ax;
}

/* 048C:1362  free the 22 strings of series text cache slot a0 */
void txt_series_cache_free_one(u16 a0) {
    u32 l_4;
    u16 l_6;
    FUNC_TICK(27);
    inbox(0xD, (a0 * 3) + 0xF).quality = 0;
    l_4 = MK32(DSEG, (a0 * 0x5A) + 0x65D1);
    l_6 = 0;
    while (((i16)l_6) < 22) {
        LOOP_TICK(13);
        if ((FP16(l_4, 0) | FP16(l_4, 2)) != 0) {
            mem_free_ptr(HPTR(l_4));
        }
        l_4 = MK32(HI16(l_4), LO16(l_4) + 4);
        l_6 = l_6 + 1;
    }
    return;
}

/* 048C:13C6  (room, arg): load screenplay texts for room 9 (script agency: script 0x22, own scripts, offers), 5 (studio) or 0xC (supermarket lists) */
u16 txt_script_cache_load(u16 a0, u16 a1, u16 ax) {
    u16 l_2;
    u16 l_4;
    u16 l_6;
    u32 t1;
    u16 t2;
    u16 t3;
    u16 t4;
    u16 t7;
    u16 t8;
    u16 t11;
    u16 t12;
    u16 t13;
    u16 t16;
    u16 t17;
    u16 t18;
    u16 t21;
    FUNC_TICK(88);
    disk_access_begin(ax);
    t1 = mem_calloc(0x33, 2);
    g_txt_index = t1;
    t2 = G16((g_language << 4) + 0x566);
    t3 = g_txt_index_hi;
    t4 = g_txt_index_lo;
    g_txt_res = G16((g_language << 4) + 0x566);
    res_read_part(t2, HPTR(MK32(t3, t4)), 0, 0x66, t2);
    if (a0 == 9) {
        l_4 = 0;
        t7 = l_4;
        l_4 = l_4 + 1;
        txt_script_cache_load_one(0x22, t7);
        l_6 = 0;
        while (((i16)g_scripts_owned_count(g_player)) > ((i16)l_6)) {
            LOOP_TICK(24);
            t8 = l_4;
            l_4 = l_4 + 1;
            txt_script_cache_load_one(owned_scripts(g_player, l_6).script, t8);
            l_6 = l_6 + 1;
        }
        l_6 = 1;
        while (((i16)l_6) <= 5) {
            LOOP_TICK(21);
            t11 = script_offers(l_6).script;
            l_2 = script_offers(l_6).script;
            if (t11 != 0) {
                t12 = l_4;
                l_4 = l_4 + 1;
                txt_script_cache_load_one(l_2, t12);
            }
            l_6 = l_6 + 1;
        }
    } else if (a0 == 5) {
        l_4 = 0;
        if (g_room_owner == g_player) {
            l_6 = 0;
            while (((i16)g_scripts_owned_count(g_player)) > ((i16)l_6)) {
                LOOP_TICK(24);
                t13 = l_4;
                l_4 = l_4 + 1;
                txt_script_cache_load_one(owned_scripts(g_player, l_6).script, t13);
                l_6 = l_6 + 1;
            }
        }
        t16 = studios(a1).script;
        l_2 = studios(a1).script;
        if (t16 != 0) {
            t17 = l_4;
            l_4 = l_4 + 1;
            txt_script_cache_load_one(l_2, t17);
        }
    } else if (a0 == 0xC) {
        l_4 = 0;
        l_6 = 0;
        while (((i16)g_shoplist_count(g_player)) > ((i16)l_6)) {
            LOOP_TICK(24);
            t18 = l_4;
            l_4 = l_4 + 1;
            txt_script_cache_load_one(shoplists(g_player, l_6).script, t18);
            l_6 = l_6 + 1;
        }
    }
    t21 = mem_free(g_txt_index_lo, g_txt_index_hi);
    return disk_access_end(t21);
}

/* 048C:15A2  (prod_id, slot): read the 16 strings of a screenplay into g_script_txt_cache */
void txt_script_cache_load_one(u16 a0, u16 a1) {
    u32 l_4;
    u32 l_8;
    u32 l_C;
    u16 l_E;
    u16 l_10;
    u32 t3;
    FUNC_TICK(71);
    l_10 = FP16(g_txt_index, a0 << 1) - (FP16(g_txt_index, (a0 - 1) << 1));
    G16((a1 * 0x42) + 0x6B15) = a0;
    l_4 = MK32(DSEG, (a1 * 0x42) + 0x6B17);
    t3 = mem_calloc(l_10 + 1, 1);
    l_8 = t3;
    l_C = t3;
    res_read_part(g_txt_res, HPTR(l_C), FP16(g_txt_index, (a0 - 1) << 1), l_10, (a0 - 1) << 1);
    l_E = 0;
    while (((i16)l_E) < 16) {
        LOOP_TICK(44);
        if (FP8(l_8, 0) == 0) {
            FP32(l_4, 0) = 0;
            l_8 = MK32(HI16(l_8), LO16(l_8) + 1);
        } else {
            FP32(l_4, 0) = mem_calloc(FP8(l_8, 0) + 1, 1);
            far_memcpy(HPTR(FP32(l_4, 0)), HPTR(MK32(HI16(l_8), LO16(l_8) + 1)), FP8(l_8, 0));
            l_8 = MK32(HI16(l_8), LO16(l_8) + (FP8(l_8, 0) + 1));
        }
        l_4 = MK32(HI16(l_4), LO16(l_4) + 4);
        l_E = l_E + 1;
    }
    mem_free(LO16(l_C), HI16(l_C));
    return;
}

/* 048C:16C8  free all 10 screenplay text cache entries */
u16 txt_script_cache_free_all(u16 ax) {
    u16 l_2;
    u16 t1;
    FUNC_TICK(8);
    l_2 = 0;
    while (((i16)l_2) < 10) {
        LOOP_TICK(6);
        t1 = txt_script_cache_free_one(l_2);
        l_2 = l_2 + 1;
        ax = t1;
    }
    return ax;
}

/* 048C:16EB  free the 16 strings of screenplay text cache slot a0 */
u16 txt_script_cache_free_one(u16 a0) {
    u16 ax;
    u16 l_2;
    u32 l_6;
    FUNC_TICK(19);
    l_6 = MK32(DSEG, (a0 * 0x42) + 0x6B17);
    l_2 = 0;
    ax = a0 * 0x42;
    while (((i16)l_2) < 16) {
        LOOP_TICK(13);
        ax = FP16(l_6, 0) | FP16(l_6, 2);
        if ((FP16(l_6, 0) | FP16(l_6, 2)) != 0) {
            ax = mem_free_ptr(HPTR(l_6));
        }
        l_6 = MK32(HI16(l_6), LO16(l_6) + 4);
        l_2 = l_2 + 1;
    }
    return ax;
}
