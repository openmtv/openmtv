/* End of day: daily settlement: ledger, transmitters, shopping lists.
 *
 * Original module 222A of MTV.EXE (Rainbow Arts, 1991).  Decompiled by
 * tools/decomp.py and hand-maintained since the freeze (git tag
 * generated-final).  Every function keeps its original address in the
 * comment above it; that address ties it to the machine-level twin in gen/
 * and to the difftest tables -- keep it when editing.
 */
#include "game.h"

/* 222A:0001  copy today's 0x54-byte finance ledger to yesterday, clear it, opening balance = player's money */
void money_ledger_new_day(void) {
    FUNC_TICK(29);
    far_memcpy(HPTR(DSP(0x7492)), HPTR(DSP(0x71F1)), 0x54);
    mem_zero(DSP(0x71F1), 0x54);
    g_ledger_today = players(g_player).money;
    return;
}

/* 222A:004A  shift per-player daily stats to 'yesterday', push today's average of 7 samples into the 7-day viewer history (0x6DA9), clear today's stats */
void rating_stats_new_day(void) {
    u16 di;
    u16 si;
    u16 l_2;
    u16 l_4;
    u16 t9;
    u16 t15;
    FUNC_TICK(59);
    di = 1;
    while (((i16)di) <= 3) {
        LOOP_TICK(36);
        far_memcpy(HPTR(MK32(DSEG, (di * 0x1E) + 0x1C8A)), HPTR(owned_scripts_at(0xB, (di * 6) + 9)), 0xE);
        mem_zero(owned_scripts_at(0xB, (di * 6) + 9), 0xE);
        di = di + 1;
    }
    far_memcpy(HPTR(DSP(0x78BE)), HPTR(DSP(0x74EA)), 0x80);
    far_memcpy(HPTR(DSP(0x793E)), HPTR(DSP(0x76B0)), 0x80);
    far_memcpy(HPTR(DSP(0x5573)), HPTR(DSP(0x5553)), 0x20);
    di = 1;
    while (((i16)di) <= 3) {
        LOOP_TICK(98);
        si = 1;
        while (((i16)si) <= 6) {
            LOOP_TICK(32);
            G32(((di << 5) + 0x6DA9) + (si << 2)) = G32(((di << 5) + 0x6DA9) + ((si + 1) << 2));
            si = si + 1;
        }
        l_2 = 0;
        l_4 = 0;
        si = 1;
        while (((i16)si) <= 7) {
            LOOP_TICK(17);
            t9 = l_4;
            l_4 = l_4 + (G16(((di << 5) + 0x74EA) + (si << 2)));
            l_2 = (l_2 + G16((((di << 5) + 0x74EA) + (si << 2)) + 2)) + ((u32)t9 + G16(((di << 5) + 0x74EA) + (si << 2)) > 0xFFFF);
            si = si + 1;
        }
        G32((di << 5) + 0x6DC5) = ((u32)((i32)MK32(l_2, l_4) / (i32)7));
        l_2 = 0;
        l_4 = 0;
        si = 1;
        while (((i16)si) <= 7) {
            LOOP_TICK(17);
            t15 = l_4;
            l_4 = l_4 + (G16(((di << 5) + 0x6DA9) + (si << 2)));
            l_2 = (l_2 + G16((((di << 5) + 0x6DA9) + (si << 2)) + 2)) + ((u32)t15 + G16(((di << 5) + 0x6DA9) + (si << 2)) > 0xFFFF);
            si = si + 1;
        }
        di = di + 1;
    }
    mem_zero(DSP(0x74EA), 0x80);
    mem_zero(DSP(0x76B0), 0x80);
    return;
}

/* 222A:0206  clear the script/episode of studio a0 and delete the renter's shopping lists for that script */
void studio_cancel_production(u16 a0) {
    u16 di;
    u16 si;
    u32 l_4;
    u16 l_6;
    u16 t1;
    FUNC_TICK(38);
    l_4 = studios_at(a0);
    if ((AT(studio, l_4).script) != 0) {
        t1 = (u16)(i8)(AT(studio, l_4).renter);
        l_6 = AT(studio, l_4).script;
        AT(studio, l_4).script = 0;
        AT(studio, l_4).episode_last = 0;
        AT(studio, l_4).episode = 1;
        di = t1;
        if (((i16)l_6) > 0) {
            si = 0;
            while (((i16)g_shoplist_count(di)) > ((i16)si)) {
                LOOP_TICK(25);
                if ((shoplists(di, si).script) == l_6) {
                    shoplist_delete(di, si);
                    si = si - 1;
                }
                si = si + 1;
            }
        }
    }
    return;
}

/* 222A:02A1  settle transmitter changes of player a0: scrapped (state 2) removed, newly bought (state 1) paid now (ledger cat 3), then recompute coverage */
u16 tx_commit_day(u16 a0) {
    u16 di;
    u16 si;
    FUNC_TICK(24);
    tx(a0, 0).state = 0;
    si = 1;
    di = a0;
    while (((i16)si) <= 32) {
        LOOP_TICK(98);
        if ((tx(di, si).state) != 2) {
            if ((tx(di, si).state) == 1) {
                players(di).money = (players(di).money) - (tx(di, si).fee);
                ledger_add(LEDGER_TRANSMITTERS, tx(di, si).fee);
            }
        } else {
            tx(di, si).reach = 0;
        }
        tx(di, si).state = 0;
        si = si + 1;
    }
    return transmitters_recalc(di);
}

/* 222A:03D1  ? zero the first shopping list record of player a0 (unused) */
void shoplist_clear_first(u16 a0) {
    FUNC_TICK(14);
    mem_zero(shoplists_at(a0, 0), 0xF);
    return;
}

/* 222A:03EE  remove bag slot a0 (shift down, count--) */
u16 bag_remove(u16 a0, u16 ax) {
    u16 si;
    FUNC_TICK(9);
    si = a0;
    while (((i16)si) < ((i16)g_bag_count)) {
        LOOP_TICK(18);
        ax = far_memcpy(HPTR(MK32(DSEG, (si << 1) + 0x2483)), HPTR(MK32(DSEG, ((si + 1) << 1) + 0x2483)), 2);
        si = si + 1;
    }
    g_bag_count = g_bag_count - 1;
    return ax;
}

/* 222A:0424  book production purchases of player a0 into the ledger (cat 7, cost = multiplier x actor fee / prop price) for items changed since last booking; book a present bought today (cat 6) */
u16 shoplists_book_costs(u16 a0) {
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
    u16 l_14;
    u16 t2;
    u16 t4;
    FUNC_TICK(41);
    l_12 = 0;
    for (;;) {
        LOOP_TICK(160);
        ax = g_shoplist_count(a0);
        if (((i16)g_shoplist_count(a0)) <= ((i16)l_12)) break;
        l_8 = shoplists_at(a0, l_12);
        l_14 = AT(shoplist, l_8).script;
        t2 = AT(shoplist, l_8).episode;
        l_4 = screenplays_at(l_14);
        si = 1;
        di = t2;
        while (((i16)si) <= 10) {
            LOOP_TICK(127);
            if (a0 == g_player) {
                l_10 = (u16)(i8)(FP8(l_4, (((di - 1) * 0x23) + (si * 3)) + 0xB));
                cc = (u16)((u16)(i8)(FP8(l_4, (((di - 1) * 0x23) + (si * 3)) + 0xB))) != ((u16)((u16)(i8)FP8(l_8, si + 4)));
                if (cc && l_10 != 0) {
                    t4 = (u16)(i8)(FP8(l_4, (((di - 1) * 0x23) + (si * 3)) + 9));
                    l_E = (u16)(i8)(FP8(l_4, (((di - 1) * 0x23) + (si * 3)) + 9));
                    if (((i16)t4) > 15) {
                        l_C = ((u32)(MK32(((i16)((u16)(i8)(FP8(l_4, ((di - 1) * 0x23) + 0xA)))) >> 0xF, (u16)(i8)(FP8(l_4, ((di - 1) * 0x23) + 0xA))) * G32((((l_E + 0xFFF1) << 4) + 0x6235) + (l_10 << 2))));
                    } else {
                        l_C = ((u32)(MK32(((i16)((u16)(i8)(FP8(l_4, ((di - 1) * 0x23) + 0xA)))) >> 0xF, (u16)(i8)(FP8(l_4, ((di - 1) * 0x23) + 0xA))) * G32(((((((u16)(i8)FP8(l_4, l_E + 0x1A3)) - 1) * 0xC0) + 0x5013) + (l_10 * 0xC)) + 2)));
                    }
                    ledger_add(LEDGER_CAST, l_C);
                }
            }
            FP8(l_8, si + 4) = FP8(l_4, (((di - 1) * 0x23) + (si * 3)) + 0xB);
            si = si + 1;
        }
        l_12 = l_12 + 1;
    }
    if (((i8)g_player_present(a0)) > 100) {
        g_player_present(a0) = g_player_present(a0) - 0x64;
        ax = a0;
        if (a0 == g_player) {
            ax = ledger_add(LEDGER_SUNDRY, gifts((u16)(i8)g_player_present(a0)).price);
        }
    }
    return ax;
}

/* 222A:061E  move films from the bag into the player's library shelves (owner set, max 4 per genre); special recurring live films 0x1D and 0xBB start their countdown events */
u16 bag_unload_to_library(void) {
    u16 ax;
    int cc;
    u16 di;
    u16 si;
    u32 l_4;
    u16 l_6;
    u16 t16;
    u16 t22;
    FUNC_TICK(23);
    l_4 = library_at(g_player);
    ax = DSEG;
    si = 0;
    while (((i16)si) < ((i16)g_bag_count)) {
        LOOP_TICK(95);
        if (((i16)g_bag(si)) >= 1000) {
            series_owner(g_bag(si) + 0xFC18) = (u8)((((u8)((u8)(ax >> 8))) << 8) | G8(0x8BA8));
            di = 0xB;
        } else {
            films(g_bag(si)).owner = (u8)((((u8)((u8)(ax >> 8))) << 8) | G8(0x8BA8));
            cc = g_bag(si) != 0x1D;
            di = (u16)(i8)(films(g_bag(si)).genre);
            if (cc) {
                if (g_bag(si) == 0xBB) {
                    g_live187_left = 5;
                    live187_reschedule();
                }
            } else {
                g_live29_left = 3;
                live29_reschedule();
            }
        }
        t16 = (u16)(i8)FP8(l_4, di + 0x60);
        l_6 = (u16)(i8)FP8(l_4, di + 0x60);
        ax = t16;
        if (((i16)t16) < 4) {
            t22 = g_bag(si);
            FP16(l_4, (di << 3) + (l_6 << 1)) = g_bag(si);
            cc = di != 0xB;
            ax = t22;
            if (!(cc)) {
                FP16(l_4, (di << 3) + (l_6 << 1)) = (FP16(l_4, (di << 3) + (l_6 << 1))) - 0x3E8;
                ax = l_6 << 1;
            }
            FP8(l_4, di + 0x60) = FP8(l_4, di + 0x60) + 1;
            ax = bag_remove(si, ax);
            si = si - 1;
        }
        si = si + 1;
    }
    return ax;
}
