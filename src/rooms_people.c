/* Room input II: boss, new-game setup, supermarket, shop, Betty and presents.
 *
 * Original module 0AA4 of MTV.EXE (Rainbow Arts, 1991).  Decompiled by
 * tools/decomp.py and hand-maintained since the freeze (git tag
 * generated-final).  Every function keeps its original address in the
 * comment above it; that address ties it to the machine-level twin in gen/
 * and to the difftest tables -- keep it when editing.
 */
#include "game.h"
#include "seat.h"
#include "fixes.h"

/* ORIGINAL BUG (0AA4:000E, fix "boss-music"): on both ways into the end
 * sequence (fired in the morning talk, "I quit") the boss calls
 * snd_room_music_stop (35F8:058F) without pushing its two arguments (room
 * [bp+6], owner [bp+8]).  The callee reads the two words above its return
 * address instead: the DI and SI that 0AA4:000E's own prologue saved (push
 * si; push di), i.e. the DI and SI of whoever called the boss handler.
 * A click reaches it from the main loop (0054:0005) through ui_on_left_click
 * (05FF:000A), a right click or Esc through room_escape (0054:2741); neither
 * touches SI or DI, so they hold the main loop's: SI = 60, left over from its
 * start-up loop over the 60 figure sprite entries, and DI = 1, its "button
 * held" flag, set just before it dispatches the click.  The original calls
 * snd_room_music_stop(room 1, owner 60): it records room 1 as the last room
 * and stops the playing room sample only if the last room was 0.
 * The decompiled functions leave the CPU registers to the machine-level code,
 * so R.di / R.si still hold the main loop's DI / SI here (and, under the
 * difftest, the original's own registers).  Not reproduced: the other ways
 * in, where functions in between set SI/DI in the original -- the space key
 * while the boss's answer is shown (game_frame 0ED7:0004 sets DI = 0, the
 * hotkey handler 0054:085E SI = 0x20 before 05FF:000A) or room_escape called
 * by other code (e.g. the day end, 0054:20E0); there the original sees those
 * values.
 * Fixed: the arguments of entering a room, (g_room, g_room_owner), which
 * stops the room's sample. */
static void boss_music_stop(void) {
    if (fix_on(FIX_BOSS_MUSIC))
        rtl_snd_room_music_stop(g_room, g_room_owner);
    else
        rtl_snd_room_music_stop(R.di, R.si);
}

/* 0AA4:000E  boss (room 3). State 1 morning talk: greeting by image (<5 fired, <15, <75, else 'my dear'), complaints about cancelled hours / missed ads yesterday, Sammy result, verdict (<20 failure), Sammy announcement, warnings by image. State 2 menu: quit (image>=75 or Sammy winner -> boss begs; else fired), more money (image<10 thrown out; 10-49: $200,000 credit once, haggle for +$100k/+$200k; >=50: $400,000 credit, haggle +$200k/+$400k; Sammy winner gets full amount), repay credit ($100k/$200k/all, rounded down to $50k, limited by money, ledger cat 0x10) */
u16 room_boss_click(void) {
    u16 ax;
    u16 di;
    u16 dx;
    u16 s0;
    u16 s1;
    u16 si;
    u32 rv_;
    u16 t3;
    u16 t7;
    u16 t29;
    u32 t33;
    u32 t40;
    u16 t41;
    u16 t42;
    u32 t45;
    u16 t46;
    u16 t47;
    u32 t50;
    u16 t51;
    u32 t52;
    u32 t53;
    u32 t54;
    u32 t55;
    u32 t56;
    u32 t57;
    u32 t58;
    u16 t63;
    u32 t67;
    u32 t70;
    u32 t71;
    u16 t72;
    u32 t75;
    u16 t76;
    u8 fr_[28] = {0};                  /* bytes bp-28 .. bp-1 of the original frame */
    FUNC_TICK(830);
    FR16(fr_, 28, 0x1A) = 0;
    FR16(fr_, 28, 0x16) = players(g_player).image;
    struct_copy((fr_ + 28 - 0x10), HPTR(DSP(0x5F4)), 0x10);
    ax = 0x5F4;
    si = 0;
    if (g_room_mode == 2) {
        if (g_boss_menu != 0xFFFF) {
            if (g_hotspot != 0x24 || ((i16)g_boss_menu) <= 0) {
                if (g_hotspot != 0x25 || ((i16)g_boss_menu) <= 0) {
                    if (g_hotspot == 0x49) {
                        if (g_boss_menu != 0) {
                            if (g_boss_menu == 1) {
                                if (g_boss_choice != 1) {
                                    if (g_boss_choice == 2) {
                                        if (((i16)FR16(fr_, 28, 0x16)) >= 10) {
                                            if (((i16)FR16(fr_, 28, 0x16)) < 50) {
                                                if ((g_credit_lo | g_credit_hi) != 0) {
                                                    if (((i32)g_credit) <= (i32)credit_haggle_max()) {   /* the original: 200000; option --credit-line */
                                                        if (((i16)g_day) <= 1) {
L_021A:
                                                            g_boss_menu = 0xFFFF;
                                                            si = 0xC1;
                                                        } else {
                                                            G32(0x7A8E) = 0x10003;
                                                            si = 0xBA;
                                                        }
                                                    } else {
                                                        goto L_021A;
                                                    }
                                                } else {
                                                    g_boss_menu = 0xFFFF;
                                                    g_credit = 0x30D40;
                                                    G32((u16)((g_player * 0x17) + 0x8BDC) + 8) = (players(g_player).money) + 0x30D40;
                                                    ledger_add(LEDGER_SUNDRY_INCOME, 0x30D40);
                                                    ui_draw_money();
                                                    si = 0xB9;
                                                }
                                            } else if ((g_credit_lo | g_credit_hi) != 0) {
                                                if (((i32)g_credit) <= 400000) {
                                                    G32(0x7A8E) = 0x10004;
                                                    si = 0xC3;
                                                } else {
                                                    g_boss_menu = 0xFFFF;
                                                    si = 0xC1;
                                                }
                                            } else {
                                                g_boss_menu = 0xFFFF;
                                                g_credit = 0x61A80;
                                                G32((u16)((g_player * 0x17) + 0x8BDC) + 8) = (players(g_player).money) + 0x61A80;
                                                ledger_add(LEDGER_SUNDRY_INCOME, 0x61A80);
                                                ui_draw_money();
                                                si = 0xC2;
                                            }
                                        } else {
                                            g_boss_menu = 0xFFFF;
                                            si = 0xB8;
                                        }
                                    } else if (g_boss_choice == 3) {
                                        if ((g_credit_lo | g_credit_hi) != 0) {
                                            G32(0x7A8E) = 0x10005;
                                            si = 0x32;
                                        } else {
                                            G32(0x7A8E) = 0x10001;
                                            si = 0x2C;
                                        }
                                    }
                                } else if (((i16)(players(g_player).image)) >= 75 || !(g_sammy_winner == 0 || g_sammy_winner != g_player)) {
                                    G32(0x7A8E) = 0x10002;
                                    si = 0xB3;
                                } else {
                                    g_boss_menu = 0xFFFF;
                                    g_player_quit(0) = 1;
                                    si = 0xB2;
                                }
                            } else if (g_boss_menu != 2) {
                                if (g_boss_menu == 3) {
                                    if (g_boss_choice != 1) {
                                        if (g_boss_choice != 2) {
                                            if (g_boss_choice == 3) {
                                                g_boss_menu = 0xFFFF;
                                                si = 0xBF;
                                            }
                                        } else {
                                            g_boss_menu = 0xFFFF;
                                            g_credit = g_credit + 0x186A0;
                                            G32((u16)((g_player * 0x17) + 0x8BDC) + 8) = (players(g_player).money) + 0x186A0;
                                            ledger_add(LEDGER_SUNDRY_INCOME, 0x186A0);
                                            ui_draw_money();
                                            si = 0xC0;
                                        }
                                    } else if (g_sammy_winner != g_player) {
                                        g_boss_menu = 0xFFFF;
                                        si = 0xBF;
                                    } else {
                                        g_boss_menu = 0xFFFF;
                                        g_credit = g_credit + 0x30D40;
                                        ledger_add(LEDGER_SUNDRY_INCOME, 0x30D40);
                                        G32((u16)((g_player * 0x17) + 0x8BDC) + 8) = (players(g_player).money) + 0x30D40;
                                        ui_draw_money();
                                        si = 0xBE;
                                    }
                                } else if (g_boss_menu == 4) {
                                    if (g_boss_choice != 1) {
                                        if (g_boss_choice != 2) {
                                            if (g_boss_choice == 3) {
                                                g_boss_menu = 0xFFFF;
                                                si = 0xCA;
                                            }
                                        } else {
                                            g_boss_menu = 0xFFFF;
                                            g_credit = g_credit + 0x30D40;
                                            G32((u16)((g_player * 0x17) + 0x8BDC) + 8) = (players(g_player).money) + 0x30D40;
                                            ledger_add(LEDGER_SUNDRY_INCOME, 0x30D40);
                                            ui_draw_money();
                                            si = 0xC9;
                                        }
                                    } else if (g_sammy_winner != g_player) {
                                        g_boss_menu = 0xFFFF;
                                        si = 0xC8;
                                    } else {
                                        g_boss_menu = 0xFFFF;
                                        g_credit = g_credit + 0x61A80;
                                        G32((u16)((g_player * 0x17) + 0x8BDC) + 8) = (players(g_player).money) + 0x61A80;
                                        ledger_add(LEDGER_SUNDRY_INCOME, 0x61A80);
                                        ui_draw_money();
                                        si = 0xC7;
                                    }
                                } else if (g_boss_menu == 5) {
                                    FR16(fr_, 28, 0x12) = 0;
                                    FR16(fr_, 28, 0x14) = 0;
                                    if (g_boss_choice != 1) {
                                        if (g_boss_choice != 2) {
                                            t29 = g_credit_lo;
                                            FR16(fr_, 28, 0x12) = g_credit_hi;
                                            FR16(fr_, 28, 0x14) = t29;
                                        } else {
                                            FR16(fr_, 28, 0x12) = 3;
                                            FR16(fr_, 28, 0x14) = 0xD40;
                                        }
                                    } else {
                                        FR16(fr_, 28, 0x12) = 1;
                                        FR16(fr_, 28, 0x14) = 0x86A0;
                                    }
                                    s0 = 0;
                                    s1 = 0xC350;
                                    ax = pl_money_lo(g_player);
                                    if (((i32)(players(g_player).money)) > ((i32)FR32(fr_, 28, 0x14))) {
                                        ax = FR16(fr_, 28, 0x14);
                                        dx = FR16(fr_, 28, 0x12);
                                    } else {
                                        ax = pl_money_lo(g_player);
                                        dx = pl_money_hi(g_player);
                                    }
                                    /* ORIGINAL BUG (fix "credit-repay"): the amount is the smaller of
                                     * the choice and the cash -- negative cash gives a negative
                                     * repayment (the boss lends the debt, past every credit line), and
                                     * nothing caps it at the credit ($100k/$200k on a $50k credit: a
                                     * negative credit, which pays 5 % a day).  Fixed: also at most the
                                     * credit, never below 0 -- 0 is the original's "not enough money". */
                                    if (fix_on(FIX_CREDIT_REPAY)) {
                                        i32 a = (i32)MK32(dx, ax);
                                        if (a > (i32)g_credit) a = (i32)g_credit;
                                        if (a < 0) a = 0;
                                        ax = LO16((u32)a);
                                        dx = HI16((u32)a);
                                    }
                                    t33 = (u32)(((u32)((i32)MK32(dx, ax) / (i32)MK32(s0, s1))) * 0xC350);
                                    FR16(fr_, 28, 0x12) = HI16(t33);
                                    FR16(fr_, 28, 0x14) = LO16(t33);
                                    if ((FR16(fr_, 28, 0x14) | FR16(fr_, 28, 0x12)) != 0) {
                                        si = 0x3D;
                                    } else {
                                        si = 0x3C;
                                    }
                                    G32((u16)((g_player * 0x17) + 0x8BDC) + 8) = (players(g_player).money) - FR32(fr_, 28, 0x14);
                                    ledger_add(LEDGER_CREDIT_REPAID, FR32(fr_, 28, 0x14));
                                    g_credit = g_credit - FR32(fr_, 28, 0x14);
                                    g_boss_menu = 0xFFFF;
                                    ui_draw_money();
                                }
                            } else if (g_boss_choice != 1) {
                                if (g_boss_choice == 2) {
                                    g_boss_menu = 0xFFFF;
                                    si = 0xB7;
                                }
                            } else {
                                g_boss_menu = 0xFFFF;
                                g_player_quit(0) = 1;
                                si = 0xB6;
                            }
                        } else {
                            G32(0x7A8E) = 0x10001;
                            si = 0xFFFF;
                        }
                        boss_dialog_draw(2, si);
                        ax = present_frame();
                    }
                } else {
                    t7 = g_player_quit(g_boss_menu);
                    g_boss_choice = g_boss_choice + 1;
                    if (((i16)t7) < ((i16)g_boss_choice)) {
                        g_boss_choice = 1;
                    }
                    boss_dialog_draw(2, 0xFFFF);
                    ax = present_frame();
                }
            } else {
                g_boss_choice = g_boss_choice - 1;
                if (((i16)g_boss_choice) < 1) {
                    g_boss_choice = g_player_quit(g_boss_menu);
                }
                boss_dialog_draw(2, 0xFFFF);
                ax = present_frame();
            }
        } else {
            t3 = boss_dialog_draw(1, 0);
            g_boss_menu = 0xFFFE;
            ax = t3;
        }
    } else if (g_room_mode == 1) {
        if (g_day == 1) {
            if (g_boss_talk_step == 0) {
                t40 = text_dup(0x48);
                t41 = boss_say(0, LO16(t40), HI16(t40));
                g_boss_talk_step = 1;
                FR16(fr_, 28, 0x1A) = 1;
                ax = t41;
            }
            if (((u32)(MK32(FR16(fr_, 28, 0x1A), g_boss_talk_step))) == 1) {
                t42 = sammy_daily_update();
                ax = t42;
                if (t42 == 0) {
                    t45 = str_format(g_text(164), W2(G32(((g_sammy_category(0) + 0x153) << 2) + 0x8390)));
                    boss_say(2, LO16(t45), HI16(t45));
                    t46 = present_frame();
                    FR16(fr_, 28, 0x1A) = 1;
                    ax = t46;
                }
                g_boss_talk_step = 2;
            }
            if (((u32)(MK32(FR16(fr_, 28, 0x1A), g_boss_talk_step))) == 2) {
                boss_say(1, 0, 0);
                g_boss_talk_step = 0;
                g_room_mode = 0;
                t47 = room_escape();
                G16(0x7A32) = 2;
                ax = t47;
            }
        } else {
            if (g_boss_talk_step == 0) {
                if (((i16)FP16(g_cur_player_rec, 0x15)) >= 5) {
                    if (((i16)FP16(g_cur_player_rec, 0x15)) >= 15) {
                        if (((i16)FP16(g_cur_player_rec, 0x15)) >= 75) {
                            di = 0x9D;
                        } else {
                            di = 0x9C;
                        }
                    } else {
                        di = 0x9B;
                    }
                } else {
                    g_fired_player(0) = g_player;
                    di = 0x9A;
                }
                t50 = str_format(g_text(di), W2(FP32(g_cur_player_rec, 0)));
                t51 = boss_say(0, LO16(t50), HI16(t50));
                g_boss_talk_step = 1;
                FR16(fr_, 28, 0x1A) = 1;
                g_boss_complained = 0;
                ax = t51;
            }
            if (FR16(fr_, 28, 0x1A) == 0 && g_boss_talk_step == 1) {
                ax = g_fired_player(0);
                if (g_fired_player(0) != g_player) {
                    if (!(g_boss_cancelled_hours == 0 || g_fired_player(0) == g_player)) {
                        t52 = str_format(g_text(158), g_boss_cancelled_hours);
                        FR16(fr_, 28, 0xE) = HI16(t52);
                        FR16(fr_, 28, 0x10) = LO16(t52);
                        g_boss_complained = 1;
                    }
                    if (!(g_boss_missed_ads == 0 || g_fired_player(0) == g_player)) {
                        t53 = str_format(g_text(159), g_boss_missed_ads);
                        FR16(fr_, 28, 0xA) = HI16(t53);
                        FR16(fr_, 28, 0xC) = LO16(t53);
                        g_boss_complained = 1;
                    }
                    if (!(g_sammy_winner == 0 || g_sammy_winner == g_player || g_fired_player(0) == g_player)) {
                        t54 = text_dup(0xA0);
                        FR16(fr_, 28, 0x6) = HI16(t54);
                        FR16(fr_, 28, 0x8) = LO16(t54);
                        g_boss_complained = 1;
                    }
                    if (!(((i16)FP16(g_cur_player_rec, 0x15)) >= 20 || g_fired_player(0) == g_player || g_boss_complained != 0)) {
                        t55 = text_dup(0xA1);
                        FR16(fr_, 28, 0xE) = HI16(t55);
                        FR16(fr_, 28, 0x10) = LO16(t55);
                        g_boss_complained = 1;
                    }
                    if (!(g_boss_complained != 0 || g_fired_player(0) == g_player)) {
                        t56 = text_dup(0xA2);
                        FR16(fr_, 28, 0xE) = HI16(t56);
                        FR16(fr_, 28, 0x10) = LO16(t56);
                    }
                    if (!(g_sammy_winner != g_player || g_fired_player(0) == g_player)) {
                        t57 = text_dup(0xA3);
                        FR16(fr_, 28, 0x6) = HI16(t57);
                        FR16(fr_, 28, 0x8) = LO16(t57);
                    }
                    t58 = str_format(DSP(0x607) /* "#s#s#s" */, W2(FR32(fr_, 28, 0x10)), W2(FR32(fr_, 28, 0xC)), W2(FR32(fr_, 28, 0x8)));
                    boss_say(2, LO16(t58), HI16(t58));
                    FR16(fr_, 28, 0x18) = 0;
                    while (((i16)FR16(fr_, 28, 0x18)) < 3) {
                        LOOP_TICK(21);
                        if (((FRX16(fr_, 28, 0x10, (FR16(fr_, 28, 0x18) << 2) + 0)) | (FRX16(fr_, 28, 0x10, ((FR16(fr_, 28, 0x18) << 2) + 0) + 2))) != 0) {
                            mem_free_ptr((fr_ + 28 - 0x10 + (int)(i16)(0 + (FR16(fr_, 28, 0x18) << 2))));
                        }
                        FR16(fr_, 28, 0x18) = FR16(fr_, 28, 0x18) + 1;
                    }
                    t63 = present_frame();
                    FR16(fr_, 28, 0x1A) = 1;
                    ax = t63;
                }
                g_boss_talk_step = 2;
            }
            if (FR16(fr_, 28, 0x1A) == 0 && g_boss_talk_step == 2) {
                if (!((sammy_daily_update()) != 0 || g_fired_player(0) == g_player)) {
                    t67 = str_format(g_text(164), W2(G32(((g_sammy_category(0) + 0x153) << 2) + 0x8390)));
                    FR16(fr_, 28, 0xE) = HI16(t67);
                    FR16(fr_, 28, 0x10) = LO16(t67);
                }
                if (((i16)FP16(g_cur_player_rec, 0x15)) >= 20) {
                    if (((i16)FP16(g_cur_player_rec, 0x15)) >= 60) {
                        di = 0xA7;
                    } else {
                        di = 0xA6;
                    }
                } else {
                    di = 0xA5;
                }
                t70 = str_format(g_text(di), W2(FP32(g_cur_player_rec, 0)));
                FR16(fr_, 28, 0xA) = HI16(t70);
                FR16(fr_, 28, 0xC) = LO16(t70);
                t71 = str_format(DSP(0x60E) /* "#s#s" */, W2(FR32(fr_, 28, 0x10)), W2(FR32(fr_, 28, 0xC)));
                boss_say(2, LO16(t71), HI16(t71));
                present_frame();
                if ((FR16(fr_, 28, 0x10) | FR16(fr_, 28, 0xE)) != 0) {
                    mem_free_ptr((fr_ + 28 - 0x10));
                }
                t72 = mem_free_ptr((fr_ + 28 - 0xC));
                FR16(fr_, 28, 0x1A) = 1;
                g_boss_talk_step = 3;
                ax = t72;
            }
            if (((u32)(MK32(FR16(fr_, 28, 0x1A), g_boss_talk_step))) == 3) {
                if (((i16)FP16(g_cur_player_rec, 0x15)) >= 20) {
                    if (((i16)FP16(g_cur_player_rec, 0x15)) >= 75) {
                        di = 0xAA;
                    } else {
                        di = 0xA9;
                    }
                } else {
                    di = 0xA8;
                }
                t75 = str_format(g_text(di), W2(FP32(g_cur_player_rec, 0)));
                boss_say(2, LO16(t75), HI16(t75));
                t76 = present_frame();
                FR16(fr_, 28, 0x1A) = 1;
                g_boss_talk_step = 4;
                ax = t76;
            }
            if (((u32)(MK32(FR16(fr_, 28, 0x1A), g_boss_talk_step))) == 4) {
                boss_say(1, 0, 0);
                g_room_mode = 0;
                if (g_fired_player(0) == 0) {
                    ax = room_escape();
                } else {
                    boss_music_stop();
                    player_fired_sequence();
                    ax = g_fired_player(0);
                    if (g_fired_player(0) != g_player) {
                        ax = room_escape();
                    }
                }
                g_boss_talk_step = 0;
                G16(0x7A32) = 2;
            }
        }
    }
    if (g_player_quit(0) == 0 || g_boss_menu != 0xFFFE) {
        if (g_boss_menu == 0xFFFE) {
            g_room_mode = 0;
            ax = room_escape();
        }
    } else {
        g_player_quit(0) = 0;
        g_fired_player(0) = g_player;
        boss_music_stop();
        g_room_mode = 0;
        ax = player_fired_sequence();
    }
    rv_ = ax;
    return (u16)rv_;
}

/* 0AA4:0AC5  new-game setup (room 65): choose human player 1-3, edit station names, difficulty 1-3; start: computer players get $300,000 (easy/normal) or $750,000 (difficult) divided by (debug_level+1), figure speed 3; human gets $200,000 money + $200,000 credit, image 34, figure speed 4; 0x4B -> load game (room 58), 0x24 quit */
void room_newgame_click(void) {
    u16 ax;
    u16 di;
    u16 dx;
    u16 si;
    u16 l_2;
    u16 t1;
    FUNC_TICK(133);
    si = 0;
    if (g_hotspot == 4 || g_hotspot == 5 || g_hotspot == 6) {
        t1 = g_hotspot + 0xFFFD;
        g_player = g_hotspot + 0xFFFD;
        g_tv_channel = t1;
        g_player_figure = g_player - 1;
        newgame_draw_setup(0);
        present_frame();
        ui_draw_player_icons();
    } else if (g_hotspot == 7 || g_hotspot == 8 || g_hotspot == 9) {
        l_2 = 0;
        di = (u16)(g_hotspot + 0xFFFA);
        while (((i16)l_2) < 41) {
            LOOP_TICK(13);
            FP8(players(di).name, l_2) = 0;
            l_2 = l_2 + 1;
        }
        newgame_draw_setup(di);
        ui_input_text((di * 0x60) + 0xFFC0, 0x2E, 8, players(di).name);
        newgame_draw_setup(0);
        present_frame();
    } else if (((i16)g_hotspot) < 63 || ((i16)g_hotspot) > 65) {
        if (g_hotspot == 0x4A) {
            cursor_hide();
            if (seat_humans) {               /* multiplayer: the stations are the seats' (--humans) */
                g_player = (u16)seat_station[0];
                g_tv_channel = g_player;
            }
            l_2 = 1;
            while (((i16)l_2) <= 3) {
                LOOP_TICK(89);
                if (!mp_is_human(l_2)) {     /* l_2 != g_player (runtime/seat.h) */
                    g_ai_player(si) = (u8)l_2;
                    speed(l_2 - 1) = 3;
                    si = si + 1;
                    if (((i16)g_difficulty) > 2) {
                        ax = 0x71B0;
                        dx = 0xB;
                    } else {
                        ax = 0x93E0;
                        dx = 4;
                    }
                    players(l_2).money = MK32(dx, ax);
                    players(l_2).money = ((u32)(MK32(((i16)(g_debug_level + 1)) >> 0xF, g_debug_level + 1) * players(l_2).money));
                } else {
                    speed(l_2 - 1) = 4;
                    players(l_2).money = start_money_human();     /* the original: 0x30D40 ($200 000); option --start-money */
                    g_credit = 0x30D40;      /* seat 0's; the other humans' in seat_human_main */
                    players(l_2).image = 0x22;
                }
                l_2 = l_2 + 1;
            }
            while (seat_humans && ((i16)si) < 2) g_ai_player(si++) = 0;   /* an empty computer player's slot */
            g_player_figure = g_player - 1;
            players(g_player).image = 0x22;
            seat_clock_world = 1;            /* the new game: the world's clock */
            game_clock_stop(0 /* unused ax */);
            g_cursor_sprites(6) = g_clock_acc;
            game_clock_start();
            seat_clock_world = 0;
            newgame_starting_programs();
            ai_init_players();
            g_ai_visit_p1(0) = 1;
            g_ai_visit_rival(0) = 3;
            g_ai_visit_rival(1) = 0;
            g_ai_visit_rival(2) = 2;
            new_day();
            g_game_mode = 2;
        } else if (g_hotspot != 0x4B) {
            if (g_hotspot == 0x24) {
                g_room = 0x3E8;
            }
        } else if (!seat_humans) {   /* (multiplayer: no loading -- every machine has saved games of its own) */
            room_enter(ROOM_SAVE_ROOM);
        }
    } else {
        g_difficulty = g_hotspot + 0xFFC2;
        newgame_draw_setup(0);
        present_frame();
    }
    return;
}

/* 0AA4:0D65  supermarket (room 12): menu presents/equipment; presents shop: 10 presents for Betty (2x5, price in g_presents), carry max one (msg 0x46), return for refund; equipment: needs a shopping list from the studio (msg 0xEC), opens the first incomplete list in room 63 */
void room_supermarket_click(void) {
    int cc;
    u16 di;
    u16 si;
    u16 l_2;
    u16 t5;
    u16 t6;
    u16 t8;
    u16 t9;
    u16 t10;
    u16 t16;
    u16 t23;
    u16 t24;
    u16 t25;
    u16 t26;
    u16 t27;
    FUNC_TICK(179);
    if (g_room_mode != 0) {
        if (g_room_mode == 1) {
            if (g_hotspot != 0x2C) {
                if (g_hotspot == 0x2D) {
                    if (((i16)g_shoplist_count(g_player)) > 0) {
                        si = 0;
                        for (;;) {
                            LOOP_TICK(34);
                            cc = ((i16)g_shoplist_count(g_player)) > ((i16)si);
                            if (!(cc && (script_items_bought(shoplists(g_player, si).script, shoplists(g_player, si).episode)) != 0)) break;
                            si = si + 1;
                        }
                        if (((i16)g_shoplist_count(g_player)) <= ((i16)si)) {
                            si = 0;
                        }
                        G32(0x79E0) = MK32(1, si);
                        G32(0x79E4) = 0;
                        ui_supermarket_menu(1);
                        g_room_mode = 0;
                        room_enter(ROOM_SHOP);
                    } else {
                        room_anim_update(2, 0 /* unused ax */);
                        ui_warning(0xEC);
                    }
                }
            } else {
                g_room_mode = 3;
                ui_supermarket_menu(1);
                ui_gift_catalog(0);
                ui_draw_bag(0, 6, 0 /* unused ax */);
                present_frame();
            }
        } else if (g_room_mode == 3) {
            if (g_hotspot == 0x21 && g_player_present(g_player) != 0) {
                t5 = cursor_y();
                t6 = cursor_x();
                ui_drag_begin(0x20, 0x18, t6 + 0xFFF0, t5 + 0xFFF4, 0, 0, 0xB);
                di = (u16)(i8)g_player_present(g_player);
                if (((i16)((u16)(i8)g_player_present(g_player))) > 100) {
                    di = di - 0x64;
                }
                g_drag_item = (u16)(i8)g_player_present(g_player);
                t8 = g_drag_image_hi;
                t9 = g_drag_image_lo;
                t10 = col_to_px(di);
                bitmap_copy(g_room_gfx3_lo + t10, g_room_gfx3_hi, t9, t8);
                g_room_mode = 5;
                g_redraw = 1;
                g_player_present(g_player) = 0;
                ui_draw_bag(2, 6, 0 /* unused ax */);
                present_frame();
            } else {
                si = 0;
                while (((i16)si) < 2) {
                    LOOP_TICK(99);
                    l_2 = 1;
                    while (((i16)l_2) <= 5) {
                        LOOP_TICK(94);
                        if ((mouse_in_rect((l_2 * 0x28) + 0x38, (si * 0x24) + 0x10, (l_2 * 0x28) + 0x57, (si * 0x24) + 0x27)) != 0) {
                            t16 = money_check_afford(g_player, gifts((si * 5) + l_2).price);
                            di = (si * 5) + l_2;
                            if (t16 == 0) {
                                g_drag_item = di + 0x64;
                                MEM32((u16)(FPSEG(g_cur_player_rec)), (u16)(FPOFF(g_cur_player_rec)) + 8) = FP32(g_cur_player_rec, 8) - (gifts(di).price);
                                t23 = cursor_y();
                                t24 = cursor_x();
                                ui_drag_begin(0x20, 0x18, t24 + 0xFFF0, t23 + 0xFFF4, 0, 0, 0xB);
                                t25 = g_drag_image_hi;
                                t26 = g_drag_image_lo;
                                t27 = col_to_px(di);
                                bitmap_copy(g_room_gfx3_lo + t27, g_room_gfx3_hi, t26, t25);
                                g_room_mode = 4;
                                g_redraw = 1;
                                ui_draw_money();
                            }
                        }
                        l_2 = l_2 + 1;
                    }
                    si = si + 1;
                }
            }
        } else if (g_room_mode == 4 || g_room_mode == 5) {
            if (g_hotspot != 0x21) {
                if (g_hotspot == 0x35) {
                    present_return_to_shop();
                }
            } else if (g_player_present(g_player) != 0) {
                ui_warning(0x46);
            } else {
                present_store_in_bag();
            }
        }
    } else if (g_hotspot == 0x23) {
        g_room_mode = 6;
        room_anim_update(2, 0 /* unused ax */);
    }
    return;
}

/* 0AA4:10CC  production shopping (room 63): fill the current shopping list's 10 slots with actors (7 roles x 16 actors, cost = actor fee x episode multiplier) or props (scene types >=16, 3 quality levels); actors refuse by image: any needs image>=10, fee>5000 needs >=15, fee>10000 needs >=20 (msg 0x99); replacing an item refunds the old one; scroll lists; 0x23 choose another list */
void room_shop_click(void) {
    u16 di;
    u16 si;
    u32 l_4;
    u32 l_8;
    u32 l_C;
    u32 l_10;
    u32 l_14;
    u32 l_18;
    u16 l_1A;
    u16 l_1C;
    u16 l_1E;
    u16 t15;
    u16 t16;
    u16 t19;
    u16 t20;
    u16 t21;
    u16 t24;
    u16 t25;
    u32 t29;
    u32 t30;
    u16 t31;
    u16 t32;
    u16 t36;
    u16 t37;
    u16 t41;
    u16 t42;
    u16 t43;
    u16 t44;
    u16 t45;
    u16 t46;
    u16 t47;
    u16 t48;
    u16 t52;
    u16 t53;
    u16 t54;
    u16 t60;
    u16 t61;
    u16 t63;
    u16 t71;
    u16 t72;
    u16 t73;
    u16 t74;
    u16 t75;
    u16 t76;
    u16 t77;
    u16 t78;
    u16 t82;
    u16 t83;
    u16 t99;
    u16 t122;
    u16 t123;
    u16 t124;
    u16 t125;
    u16 t126;
    u16 t129;
    FUNC_TICK(267);
    l_14 = shoplists_at(g_player, g_shop_list);
    l_1E = AT(shoplist, l_14).script;
    l_1C = AT(shoplist, l_14).episode;
    l_18 = MK32(FPSEG(g_scripts), ((FPOFF(g_scripts) + (l_1E * 0x1B3)) + ((l_1C - 1) * 0x23)) + 8);
    if (!(g_room_mode != 2 || ((i16)(cursor_y())) <= 72 || ((i16)(cursor_y())) >= 120)) {
        g_room_mode = 0;
        shop_redraw();
    }
    if (!(g_hotspot != 0x2F || ((i16)g_shop_actor_scroll) <= 0)) {
        g_shop_actor_scroll = g_shop_actor_scroll - 5;
        if (((i16)g_shop_actor_scroll) < 0) {
            g_shop_actor_scroll = 0;
        }
        shop_redraw();
    }
    if (!(g_hotspot != 0x31 || ((i16)(G16((((g_shop_category - 1) * 0xC0) + 0x5013) + ((g_shop_actor_scroll + 6) * 0xC)))) <= 0)) {
        g_shop_actor_scroll = g_shop_actor_scroll + 5;
        while ((G16((((g_shop_category - 1) * 0xC0) + 0x5013) + ((g_shop_actor_scroll + 5) * 0xC))) == 0 || ((i16)g_shop_actor_scroll) > 11) {
            LOOP_TICK(18);
            g_shop_actor_scroll = g_shop_actor_scroll - 1;
        }
        shop_redraw();
    }
    if (!(g_hotspot != 0x30 || ((i16)g_shop_slot_scroll) <= 0)) {
        g_shop_slot_scroll = g_shop_slot_scroll - 3;
        if (((i16)g_shop_slot_scroll) < 0) {
            g_shop_slot_scroll = 0;
        }
        shop_redraw();
    }
    if (!(g_hotspot != 0x32 || (FP8(l_18, ((g_shop_slot_scroll + 4) * 3) + 1)) == 0)) {
        g_shop_slot_scroll = g_shop_slot_scroll + 3;
        while ((FP8(l_18, ((g_shop_slot_scroll + 3) * 3) + 1)) == 0 || ((i16)g_shop_slot_scroll) > 8) {
            LOOP_TICK(11);
            g_shop_slot_scroll = g_shop_slot_scroll - 1;
        }
        shop_redraw();
    }
    if (g_room_mode == 0) {
        if (((i16)(cursor_y())) >= 102 && ((i16)(cursor_y())) <= 111) {
            si = 1;
            while (((i16)si) <= 3) {
                LOOP_TICK(21);
                t15 = cursor_x();
                if (((i16)t15) < ((i16)((si * 0x58) + 0xFFD0))) {
                    si = si + 1;
                    continue;
                } else {
                    t16 = cursor_x();
                    if (((i16)t16) <= ((i16)((si * 0x58) + 0x26))) break;
                    si = si + 1;
                    continue;
                }
            }
            if (((i16)si) <= 3) {
                l_1A = si + g_shop_slot_scroll;
                l_8 = MK32(FPSEG(l_18), (FPOFF(l_18) + (l_1A * 3)) + 1);
                if (((i8)FP8(l_8, 0)) > 15) {
                    g_shop_category = (u16)(i8)FP8(l_8, 0);
                } else {
                    di = (u16)(i8)(FP8(g_scripts, ((l_1E * 0x1B3) + ((u16)(i8)FP8(l_8, 0))) + 0x1A3));
                    if (((u16)((u16)(i8)(FP8(g_scripts, ((l_1E * 0x1B3) + ((u16)(i8)FP8(l_8, 0))) + 0x1A3)))) != g_shop_category) {
                        g_shop_actor_scroll = 0;
                    }
                    g_shop_category = di;
                }
                castfit_pick(l_1A);   /* --cast-fit: which role the panel is about (cast_fit.c) */
                shop_redraw();
            }
        } else if (((i16)g_shop_category) <= 7 && ((i16)(cursor_y())) >= 7 && ((i16)(cursor_y())) <= 30) {
            si = 1;
            while (((i16)si) <= 5) {
                LOOP_TICK(126);
                t19 = cursor_x();
                if (((i16)t19) >= ((i16)((si * 0x24) + 0x54))) {
                    t20 = cursor_x();
                    if (((i16)t20) <= ((i16)((si * 0x24) + 0x73))) {
                        t21 = si + g_shop_actor_scroll;
                        g_drag_item = si + g_shop_actor_scroll;
                        g_selected_program = t21;
                        t24 = G16(((((g_shop_category - 1) * 0xC0) + 0x5013) + (g_selected_program * 0xC)) + 2);
                        t25 = G16(((((g_shop_category - 1) * 0xC0) + 0x5013) + (g_selected_program * 0xC)) + 4);
                        l_10 = G32(((((g_shop_category - 1) * 0xC0) + 0x5013) + (g_selected_program * 0xC)) + 2);
                        if ((money_check_afford(g_player, MK32(t25, t24))) == 0) {
                            if (((i32)l_10) > 10000) {
                                if (((i16)FP16(g_cur_player_rec, 0x15)) < 20) {
L_1403:
                                    t29 = actor_text(G16((((g_shop_category - 1) * 0xC0) + 0x5013) + (g_selected_program * 0xC)), 2);
                                    t30 = str_format(g_text(153), W2(t29));
                                    l_C = t30;
                                    ui_warning_str(LO16(t30), HI16(t30));
                                    mem_free(LO16(l_C), HI16(l_C));
                                } else {
L_13DE:
                                    if (((i32)l_10) > 5000) {
                                        if (((i16)FP16(g_cur_player_rec, 0x15)) < 15) {
                                            goto L_1403;
                                        } else {
L_13F8:
                                            if (((i16)FP16(g_cur_player_rec, 0x15)) >= 10) {
                                                t31 = cursor_y();
                                                t32 = cursor_x();
                                                ui_drag_begin(0x20, 0x18, t32, t31, 0, 0, 0xB);
                                                shop_draw_drag_icon();
                                                draw_sprite_to(MK32(g_room_gfx2_hi, g_room_gfx2_lo + 0x4A), g_drag_image_lo, g_drag_image_hi, 0, 0);
                                                g_room_mode = 1;
                                                g_redraw = 1;
                                            } else {
                                                goto L_1403;
                                            }
                                        }
                                    } else {
                                        goto L_13F8;
                                    }
                                }
                            } else {
                                goto L_13DE;
                            }
                        }
                    }
                }
                si = si + 1;
            }
        } else if (((i16)g_shop_category) >= 16 && ((i16)(cursor_y())) >= 23 && ((i16)(cursor_y())) <= 46) {
            si = 1;
            while (((i16)si) <= 3) {
                LOOP_TICK(108);
                t36 = cursor_x();
                if (((i16)t36) >= ((i16)((si * 0x47) + 0x14))) {
                    t37 = cursor_x();
                    if (((i16)t37) <= ((i16)((si * 0x47) + 0x33))) {
                        if (si != 1) {
                            if (si != 2) {
                                if (si == 3) {
                                    di = 2;
                                }
                            } else {
                                di = 1;
                            }
                        } else {
                            di = 3;
                        }
                        g_drag_item = si;
                        g_selected_program = si;
                        if ((money_check_afford(g_player, G32((((g_shop_category + 0xFFF1) << 4) + 0x6235) + (g_selected_program << 2)))) == 0) {
                            t41 = cursor_y();
                            t42 = cursor_x();
                            ui_drag_begin(0x20, 0x18, t42, t41, 0, 0, 0xB);
                            t43 = g_drag_image_hi;
                            t44 = g_drag_image_lo;
                            t45 = col_to_px(g_shop_category + 0xFFF1);
                            bitmap_put(MK32(g_room_gfx1_hi, g_room_gfx1_lo + t45), MK32(t43, t44), 0, 2);
                            t46 = g_drag_image_hi;
                            t47 = g_drag_image_lo;
                            t48 = col_to_px(di + 9);
                            draw_sprite_to(MK32(g_room_gfx2_hi, g_room_gfx2_lo + t48), t47, t46, 0, 0);
                            g_room_mode = 1;
                            g_redraw = 1;
                        }
                    }
                }
                si = si + 1;
            }
        }
        if (((i16)(cursor_y())) >= 78 && ((i16)(cursor_y())) <= 101) {
            si = 1;
            while (((i16)si) <= 3) {
                LOOP_TICK(261);
                l_1A = si + g_shop_slot_scroll;
                l_8 = MK32(FPSEG(l_18), (FPOFF(l_18) + (l_1A * 3)) + 1);
                l_4 = MK32(FPSEG(l_18), (FPOFF(l_18) + (l_1A * 3)) + 3);
                t52 = cursor_x();
                if (((i16)t52) >= ((i16)((si * 0x58) + 0xFFEC))) {
                    t53 = cursor_x();
                    if (((i16)t53) <= ((i16)((si * 0x58) + 0xB))) {
                        if (((i8)FP8(l_8, 0)) <= 15) {
                            di = (u16)(i8)(FP8(g_scripts, ((l_1E * 0x1B3) + ((u16)(i8)FP8(l_8, 0))) + 0x1A3));
                            if (((u16)((u16)(i8)(FP8(g_scripts, ((l_1E * 0x1B3) + ((u16)(i8)FP8(l_8, 0))) + 0x1A3)))) != g_shop_category) {
                                g_shop_actor_scroll = 0;
                            }
                            g_shop_category = di;
                            castfit_pick(l_1A);   /* --cast-fit: as above */
                            if (FP8(l_4, 0) != 0 && FP8(l_14, l_1A + 4) != FP8(l_4, 0)) {
                                t54 = (u16)(i8)FP8(l_4, 0);
                                g_drag_item = (u16)(i8)FP8(l_4, 0);
                                g_selected_program = t54;
                                FP8(l_4, 0) = FP8(l_14, l_1A + 4);
                                MEM32((u16)(FPSEG(g_cur_player_rec)), (u16)(FPOFF(g_cur_player_rec)) + 8) = FP32(g_cur_player_rec, 8) + ((u32)(MK32(((i16)((u16)(i8)FP8(l_18, 2))) >> 0xF, (u16)(i8)FP8(l_18, 2)) * G32(((((g_shop_category - 1) * 0xC0) + 0x5013) + (g_drag_item * 0xC)) + 2)));
                                shop_redraw();
                                t60 = cursor_y();
                                t61 = cursor_x();
                                ui_drag_begin(0x20, 0x18, t61, t60, 0, 0, 0xB);
                                shop_draw_drag_icon();
                                draw_sprite_to(MK32(g_room_gfx2_hi, g_room_gfx2_lo + 0x4A), g_drag_image_lo, g_drag_image_hi, 0, 0);
                                g_room_mode = 1;
                            } else {
                                shop_redraw();
                            }
                            g_redraw = 1;
                            ui_draw_money();
                        } else if (((i8)FP8(l_8, 0)) >= 16) {
                            g_shop_category = (u16)(i8)FP8(l_8, 0);
                            if (FP8(l_4, 0) != 0 && FP8(l_14, l_1A + 4) != FP8(l_4, 0)) {
                                t63 = (u16)(i8)FP8(l_4, 0);
                                g_drag_item = (u16)(i8)FP8(l_4, 0);
                                g_selected_program = t63;
                                FP8(l_4, 0) = FP8(l_14, l_1A + 4);
                                MEM32((u16)(FPSEG(g_cur_player_rec)), (u16)(FPOFF(g_cur_player_rec)) + 8) = FP32(g_cur_player_rec, 8) + ((u32)(MK32(((i16)((u16)(i8)FP8(l_18, 2))) >> 0xF, (u16)(i8)FP8(l_18, 2)) * G32((((g_shop_category + 0xFFF1) << 4) + 0x6235) + (g_drag_item << 2))));
                                shop_redraw();
                                t71 = cursor_y();
                                t72 = cursor_x();
                                ui_drag_begin(0x20, 0x18, t72, t71, 0, 0, 0xB);
                                t73 = g_drag_image_hi;
                                t74 = g_drag_image_lo;
                                t75 = col_to_px(g_shop_category + 0xFFF1);
                                bitmap_put(MK32(g_room_gfx1_hi, g_room_gfx1_lo + t75), MK32(t73, t74), 0, 2);
                                if (g_drag_item != 1) {
                                    if (g_drag_item != 2) {
                                        if (g_drag_item == 3) {
                                            di = 2;
                                        }
                                    } else {
                                        di = 1;
                                    }
                                } else {
                                    di = 3;
                                }
                                t76 = g_drag_image_hi;
                                t77 = g_drag_image_lo;
                                t78 = col_to_px(di + 9);
                                draw_sprite_to(MK32(g_room_gfx2_hi, g_room_gfx2_lo + t78), t77, t76, 0, 0);
                                g_room_mode = 1;
                            } else {
                                shop_redraw();
                            }
                            g_redraw = 1;
                            ui_draw_money();
                        }
                    }
                }
                si = si + 1;
            }
        }
        if (g_hotspot == 0x23) {
            ui_draw_shopping_lists();
            present_frame();
            g_room_mode = 2;
        }
    } else if (g_room_mode == 1) {
        if (((i16)(cursor_y())) >= 78 && ((i16)(cursor_y())) <= 101) {
            si = 1;
            while (((i16)si) <= 3) {
                LOOP_TICK(323);
                l_1A = si + g_shop_slot_scroll;
                l_8 = MK32(FPSEG(l_18), (FPOFF(l_18) + (l_1A * 3)) + 1);
                l_4 = MK32(FPSEG(l_18), (FPOFF(l_18) + (l_1A * 3)) + 3);
                t82 = cursor_x();
                if (((i16)t82) >= ((i16)((si * 0x58) + 0xFFEC))) {
                    t83 = cursor_x();
                    if (((i16)t83) <= ((i16)((si * 0x58) + 0xB))) {
                        if (((i16)g_shop_category) <= 7 && ((u16)((u16)(i8)(FP8(g_scripts, ((l_1E * 0x1B3) + ((u16)(i8)FP8(l_8, 0))) + 0x1A3)))) == g_shop_category) {
                            if (FP8(l_4, 0) == 0 || FP8(l_14, l_1A + 4) == FP8(l_4, 0)) {
                                FP8(l_4, 0) = g_drag_item_b;
                                MEM32((u16)(FPSEG(g_cur_player_rec)), (u16)(FPOFF(g_cur_player_rec)) + 8) = FP32(g_cur_player_rec, 8) - ((u32)(MK32(((i16)((u16)(i8)FP8(l_18, 2))) >> 0xF, (u16)(i8)FP8(l_18, 2)) * G32(((((g_shop_category - 1) * 0xC0) + 0x5013) + (g_drag_item * 0xC)) + 2)));
                                ui_drag_end();
                                g_room_mode = 0;
                            } else {
                                g_drag_film = (u16)(i8)FP8(l_4, 0);
                                MEM32((u16)(FPSEG(g_cur_player_rec)), (u16)(FPOFF(g_cur_player_rec)) + 8) = FP32(g_cur_player_rec, 8) + ((u32)(MK32(((i16)((u16)(i8)FP8(l_18, 2))) >> 0xF, (u16)(i8)FP8(l_18, 2)) * G32(((((g_shop_category - 1) * 0xC0) + 0x5013) + (g_drag_film * 0xC)) + 2)));
                                FP8(l_4, 0) = g_drag_item_b;
                                MEM32((u16)(FPSEG(g_cur_player_rec)), (u16)(FPOFF(g_cur_player_rec)) + 8) = FP32(g_cur_player_rec, 8) - ((u32)(MK32(((i16)((u16)(i8)FP8(l_18, 2))) >> 0xF, (u16)(i8)FP8(l_18, 2)) * G32(((((g_shop_category - 1) * 0xC0) + 0x5013) + (g_drag_item * 0xC)) + 2)));
                                t99 = g_drag_film;
                                g_drag_item = g_drag_film;
                                g_selected_program = t99;
                                shop_draw_drag_icon();
                                drag_image_draw(MK32(g_room_gfx2_hi, g_room_gfx2_lo + 0x4A));
                                g_redraw = 1;
                            }
                            shop_redraw();
                            ui_draw_money();
                        } else if (((i16)g_shop_category) >= 16 && ((u16)((u16)(i8)FP8(l_8, 0))) == g_shop_category) {
                            if (FP8(l_4, 0) == 0 || FP8(l_14, l_1A + 4) == FP8(l_4, 0)) {
                                FP8(l_4, 0) = g_drag_item_b;
                                MEM32((u16)(FPSEG(g_cur_player_rec)), (u16)(FPOFF(g_cur_player_rec)) + 8) = FP32(g_cur_player_rec, 8) - ((u32)(MK32(((i16)((u16)(i8)FP8(l_18, 2))) >> 0xF, (u16)(i8)FP8(l_18, 2)) * G32((((g_shop_category + 0xFFF1) << 4) + 0x6235) + (g_drag_item << 2))));
                                ui_drag_end();
                                g_room_mode = 0;
                            } else {
                                g_drag_film = (u16)(i8)FP8(l_4, 0);
                                MEM32((u16)(FPSEG(g_cur_player_rec)), (u16)(FPOFF(g_cur_player_rec)) + 8) = FP32(g_cur_player_rec, 8) + ((u32)(MK32(((i16)((u16)(i8)FP8(l_18, 2))) >> 0xF, (u16)(i8)FP8(l_18, 2)) * G32((((g_shop_category + 0xFFF1) << 4) + 0x6235) + (g_drag_film << 2))));
                                FP8(l_4, 0) = g_drag_item_b;
                                MEM32((u16)(FPSEG(g_cur_player_rec)), (u16)(FPOFF(g_cur_player_rec)) + 8) = FP32(g_cur_player_rec, 8) - ((u32)(MK32(((i16)((u16)(i8)FP8(l_18, 2))) >> 0xF, (u16)(i8)FP8(l_18, 2)) * G32((((g_shop_category + 0xFFF1) << 4) + 0x6235) + (g_drag_item << 2))));
                                t122 = g_drag_film;
                                g_drag_item = g_drag_film;
                                g_selected_program = t122;
                                if (g_drag_item != 1) {
                                    if (g_drag_item != 2) {
                                        if (g_drag_item == 3) {
                                            di = 2;
                                        }
                                    } else {
                                        di = 1;
                                    }
                                } else {
                                    di = 3;
                                }
                                t123 = g_drag_image_hi;
                                t124 = g_drag_image_lo;
                                t125 = col_to_px(g_shop_category + 0xFFF1);
                                bitmap_put(MK32(g_room_gfx1_hi, g_room_gfx1_lo + t125), MK32(t123, t124), 0, 2);
                                t126 = col_to_px(di + 9);
                                drag_image_draw(MK32(g_room_gfx2_hi, g_room_gfx2_lo + t126));
                                g_redraw = 1;
                            }
                            shop_redraw();
                            ui_draw_money();
                        }
                    }
                }
                si = si + 1;
            }
        }
    } else if (((u32)MK32(g_room_mode, g_hotspot)) == 0x20024) {
        t129 = (u16)((i32)(i16)(cursor_y() + 0xFFFA) / 12);
        di = t129;
        if (((i16)g_shoplist_count(g_player)) > ((i16)t129)) {
            g_shop_list = di;
            g_shop_slot_scroll = 0;
            ui_draw_actor_card();
            ui_draw_shopping_lists();
            present_frame();
        }
    }
    return;
}

/* 0AA4:1D27  supermarket: put the dragged present into the player's present slot (g_player_present), back to presents shop state */
u16 present_store_in_bag(void) {
    FUNC_TICK(13);
    g_player_present(g_player) = g_drag_item_b;
    ui_drag_end();
    g_room_mode = 3;
    ui_draw_bag(2, 6, 0 /* unused ax */);
    return present_frame();
}

/* 0AA4:1D53  Betty's room: put the dragged present back into the player's present slot, state 0 */
u16 present_store_in_bag_betty(void) {
    FUNC_TICK(13);
    g_player_present(g_player) = g_drag_item_b;
    ui_drag_end();
    g_room_mode = 0;
    ui_draw_bag(2, 7, 0 /* unused ax */);
    return present_frame();
}

/* 0AA4:1D7F  supermarket: drop present on counter; refund its price if it was bought now (id>100) */
u16 present_return_to_shop(void) {
    FUNC_TICK(19);
    if (((i16)g_drag_item) > 100) {
        MEM32((u16)(FPSEG(g_cur_player_rec)), (u16)(FPOFF(g_cur_player_rec)) + 8) = FP32(g_cur_player_rec, 8) + price_7(g_drag_item + 0xFF9C);
    }
    ui_drag_end();
    g_room_mode = 3;
    return ui_draw_money();
}

/* 0AA4:1DC0  Betty's office (room 14): talk (love<20 or already has key), else gives the master key (msg 0x15D); give present: plastic-surgery voucher (2) resets love to 0, honeymoon trip (10) wins the game if love>=100 (105E_447B) else refused, others add present love value, value then decays to 70% per gift, love capped at player image */
void room_betty_click(void) {
    u16 si;
    u32 t2;
    u16 t4;
    u16 t5;
    u16 t6;
    u16 t7;
    u16 t8;
    u32 t10;
    u32 t35;
    u32 t36;
    FUNC_TICK(231);
    si = 0;
    if (g_room_mode == 0) {
        if (g_hotspot == 0x36) {
            if (((i16)FP16(g_cur_player_rec, 0xC)) < 20 || (master_key(g_player, 0)) != 0) {
                betty_greeting(0);
                g_room_mode = 1;
            } else {
                t2 = text_dup(0x15D);
                betty_say(0, LO16(t2), HI16(t2));
                master_key(g_player, 1);
            }
        }
        if (g_hotspot == 0x21 && g_player_present(g_player) != 0) {
            g_drag_item = (u16)(i8)g_player_present(g_player);
            g_player_present(g_player) = 0;
            t4 = cursor_y();
            t5 = cursor_x();
            ui_drag_begin(0x20, 0x18, t5 + 0xFFF0, t4 + 0xFFF4, 0, 0, 0xB);
            t6 = g_drag_image_hi;
            t7 = g_drag_image_lo;
            t8 = col_to_px(g_drag_item);
            bitmap_copy(g_room_gfx_lo + t8, g_room_gfx_hi, t7, t6);
            ui_draw_bag(2, 7, 0 /* unused ax */);
            present_frame();
            g_room_mode = 2;
        }
    } else if (g_room_mode != 1) {
        if (g_room_mode == 2) {
            if (g_hotspot != 0x21) {
                if (g_hotspot == 0x36) {
                    if (g_drag_item != 0xA) {
                        ui_drag_end();
                        g_room_mode = 0;
                        if (g_drag_item != 2) {
                            FP16(g_cur_player_rec, 0xC) = FP16(g_cur_player_rec, 0xC) + ((u16)(i8)(gifts(g_drag_item).love));
                            gifts(g_drag_item).love = (u8)(u16)((u16)((i32)(i16)(((u16)(i8)(gifts(g_drag_item).love)) * 0x46) / 100));
                        } else {
                            FP16(g_cur_player_rec, 0xC) = 0;
                        }
                        gifts(g_drag_item).given = (gifts(g_drag_item).given) + 1;
                        if (((i16)FP16(g_cur_player_rec, 0xC)) > ((i16)FP16(g_cur_player_rec, 0x15))) {
                            FP16(g_cur_player_rec, 0xC) = FP16(g_cur_player_rec, 0x15);
                        }
                        ui_draw_bag(1, 7, 0 /* unused ax */);
                        ui_draw_betty_office();
                        present_frame();
                        ui_draw_betty_meter();
                        if (g_drag_item == 2) {
                            t36 = text_dup(0x108);
                            betty_say(0, LO16(t36), HI16(t36));
                        } else {
                            t35 = text_dup(0x107);
                            betty_say(0, LO16(t35), HI16(t35));
                        }
                    } else if (((i16)FP16(g_cur_player_rec, 0xC)) >= 100) {
                        ui_drag_end();
                        g_room_mode = 0;
                        si = 1;
                    } else {
                        t10 = text_dup(0x106);
                        betty_say(0, LO16(t10), HI16(t10));
                    }
                }
            } else {
                present_store_in_bag_betty();
            }
        }
    } else {
        betty_greeting(1);
        g_room_mode = 0;
    }
    if (si != 0) {
        ending_play();
    }
    return;
}

/* 0AA4:2054  draw the dragged actor's portrait (actor rec +0xA icon; >15 from second atlas) into the drag image */
void shop_draw_drag_icon(void) {
    u16 bx;
    u16 cx;
    u16 s0;
    u16 s1;
    u16 s2;
    u16 s3;
    u16 si;
    FUNC_TICK(45);
    s0 = 2;
    s1 = 0;
    s2 = g_drag_image_hi;
    s3 = g_drag_image_lo;
    si = G16(((((g_shop_category - 1) * 0xC0) + 0x5013) + (g_drag_item * 0xC)) + 0xA);
    if (((i16)(G16(((((g_shop_category - 1) * 0xC0) + 0x5013) + (g_drag_item * 0xC)) + 0xA))) > 15) {
        bx = g_room_gfx3_lo + col_to_px(si + 0xFFF1);
        cx = g_room_gfx3_hi;
    } else {
        bx = g_room_gfx_lo + col_to_px(si);
        cx = g_room_gfx_hi;
    }
    bitmap_put(MK32(cx, bx), MK32(s2, s3), s1, s0);
    return;
}

/* 0AA4:20C1  redraw shopping screen (26A7_3B04) and present */
u16 shop_redraw(void) {
    FUNC_TICK(3);
    ui_draw_actor_card();
    return present_frame();
}

/* 0AA4:20CC  draw a 0x1E-byte news record (a0) at x,y: colour by category, headline, '*** HH:00 today/old ** $ price ***' */
void news_draw_card(u32 a0, u16 a2, u16 a3) {
    u16 bx;
    u16 es;
    u32 l_4;
    u16 t1;
    u32 t2;
    u32 t3;
    u16 t4;
    u16 t5;
    u16 t6;
    u32 t7;
    FUNC_TICK(78);
    t1 = (u16)(i8)(G8(((u16)(i8)FP8(a0, 0x1C)) + 0x603));
    t2 = news_text(a0, FP16(a0, 0));
    l_4 = t2;
    t3 = str_format(DSP(0x613) /* "#*51s" */, W2(t2));
    ui_drag_begin(0x90, 0x22, a2, a3, LO16(t3), HI16(t3), t1);
    mem_free(LO16(l_4), HI16(l_4));
    if (FP16(a0, 0x1A) != g_day) {
        bx = FPOFF(g_text(268));
        es = FPSEG(g_text(268));
    } else {
        bx = FPOFF(g_text(259));
        es = FPSEG(g_text(259));
    }
    l_4 = MK32(es, bx);
    t4 = g_drag_image_hi;
    t5 = g_drag_image_lo;
    t6 = news_price((u16)(i8)FP8(a0, 0x1D));
    t7 = str_format(DSP(0x619) /* "*** #02d:00 #_5s ** $ #_5d ***" */, FP16(a0, 0x18), W2(l_4), t6);
    text_draw_to_free(2, 0x11, LO16(t7), HI16(t7), t5, t4);
    return;
}

/* 0AA4:2197  look up the 145-entry hotspot table (rect, room, room state or 0xFF, owner-only flag) for the cursor; returns hotspot id (0 = none); a0=1 allows owner-only hotspots in other players' rooms */
u16 ui_hotspot_at_cursor(u16 a0) {
    u16 ax;
    u16 di;
    u16 si;
    u32 l_4;
    FUNC_TICK(21);
    si = 0;
    di = 0;
    if ((di = speed4_hotspot()) != 0) {   /* --fourth-speed: the Control-Center's four speed buttons (fixes.c) */
        return di;
    }
    if ((di = adoffer_hotspot()) != 0) {  /* --ad-offers: the ad agency's desk around the rows of folders (fixes.c) */
        return di;
    }
    if ((di = filmoffer_hotspot()) != 0) {   /* --film-offers: the dealer's counter along the longer row (film_offers.c) */
        return di;
    }
    while (((i16)si) < 145) {
        LOOP_TICK(45);
        l_4 = hotspots_at(si);
        if ((mouse_in_rect(AT(hotspot, l_4).x1, FP16(l_4, 2), FP16(l_4, 4), FP16(l_4, 6))) == 0 || ((u16)((u16)(i8)(AT(hotspot, l_4).room))) != g_room || !(((u16)((u16)(i8)(AT(hotspot, l_4).state))) == g_room_mode || (AT(hotspot, l_4).state) == 0xFF) || !(g_room_owner == g_player || (AT(hotspot, l_4).owner_only) == 0 || a0 == 1)) {
            si = si + 1;
            continue;
        } else {
            goto L_2223;
        }
    }
    ax = di;
    return ax;
L_2223:
    di = (u16)(i8)(AT(hotspot, l_4).id);
    ax = di;
    return ax;
}

/* 0AA4:2243  next active transmitter slot (1..32, wrapping) of the viewed player at or after a0 (skips empty and state 2) */
u16 tx_next_slot(u16 a0) {
    u16 ax;
    int cc;
    u16 si;
    FUNC_TICK(13);
    si = a0;
    if (((i16)a0) > 32) {
        si = 1;
    }
    for (;;) {
        LOOP_TICK(31);
        if (((contracts(5, ((g_room_owner * 0x42) + (si * 2)) + 9).ad) | (G16((((g_room_owner * 0x1CE) + 0x1D09) + (si * 0xE)) + 6))) == 0) {
L_2254:
            cc = ((i16)(si + 1)) <= 32;
            si = si + 1;
            if (cc) continue;
            si = 1;
            continue;
        }
        if ((tx(g_room_owner, si).state) != 2) break;
        goto L_2254;
    }
    ax = si;
    return ax;
}

/* 0AA4:22AB  previous active transmitter slot (wrapping) of the viewed player at or before a0 */
u16 tx_prev_slot(u16 a0) {
    u16 ax;
    int cc;
    u16 si;
    FUNC_TICK(13);
    si = a0;
    if (((i16)a0) < 1) {
        si = 0x20;
    }
    for (;;) {
        LOOP_TICK(31);
        if (((contracts(5, ((g_room_owner * 0x42) + (si * 2)) + 9).ad) | (G16((((g_room_owner * 0x1CE) + 0x1D09) + (si * 0xE)) + 6))) == 0) {
L_22BC:
            cc = ((i16)(si - 1)) >= 1;
            si = si - 1;
            if (cc) continue;
            si = 0x20;
            continue;
        }
        if ((tx(g_room_owner, si).state) != 2) break;
        goto L_22BC;
    }
    ax = si;
    return ax;
}

/* 0AA4:2313  sell the dragged own film/series back to the dealer: money += price (1DA8_3F5B / 4035), ledger 0xB/0xC, owner cleared */
void film_sell_to_dealer(void) {
    u32 t1;
    u32 t4;
    u32 t11;
    u32 t14;
    FUNC_TICK(53);
    if (((i16)g_drag_film) >= 1000) {
        g_drag_film = g_drag_film - 0x3E8;
        t11 = series_price(g_drag_film);
        MEM32((u16)(FPSEG(g_cur_player_rec)), (u16)(FPOFF(g_cur_player_rec)) + 8) = FP32(g_cur_player_rec, 8) + t11;
        t14 = series_price(g_drag_film);
        ledger_add(LEDGER_SERIES_SOLD, t14);
        series(g_drag_film).owner = 0;
    } else {
        t1 = movie_price(g_drag_film);
        MEM32((u16)(FPSEG(g_cur_player_rec)), (u16)(FPOFF(g_cur_player_rec)) + 8) = FP32(g_cur_player_rec, 8) + t1;
        t4 = movie_price(g_drag_film);
        ledger_add(LEDGER_FILMS_SOLD, t4);
        films(g_drag_film).owner = 0;
    }
    ui_draw_money();
    g_room_mode = 0;
    return;
}

/* 0AA4:23B5  ad agency confirm: mark all taken offers as owned by the player (ad +0xF), clear their offer slots, refill offers (24B8_000C) */
u16 ad_agency_sign_contracts(void) {
    u16 ax;
    int cc;
    u16 si;
    FUNC_TICK(13);
    si = 1;
    while (((i16)si) <= (i16)g_nadoffers) {   /* 10 in the original (--ad-offers) */
        LOOP_TICK(40);
        cc = ((i8)(ad_offers(si).shelf)) >= 0;
        ax = DSEG;
        if (!(cc)) {
            ads(ad_offers(si).ad).holder = (u8)((((u8)((u8)(ax >> 8))) << 8) | G8(0x8BA8));
            mem_zero(ad_offers_at(si), 7);
        }
        si = si + 1;
    }
    ui_draw_bag(1, 3, 0 /* unused ax */);
    present_frame();
    return adagency_refill();
}

/* 0AA4:2439  screenplay agency: clear sold shelf slots and refill the shelf (24B8_02C1) */
void script_agency_finalize(void) {
    u16 si;
    FUNC_TICK(13);
    si = 1;
    while (((i16)si) <= 10) {
        LOOP_TICK(22);
        if (((i8)(script_offers(si).status)) < 0) {
            mem_zero(script_offers_at(si), 9);
        }
        si = si + 1;
    }
    ui_draw_bag(1, 4, 0 /* unused ax */);
    present_frame();
    scriptagency_refill();
    return;
}

/* 0AA4:2491  library: append dragged film to the bag */
u16 library_film_to_bag(void) {
    FUNC_TICK(15);
    g_bag(g_bag_count) = g_drag_film;
    g_bag_count = g_bag_count + 1;
    ui_drag_end();
    g_room_mode = 0;
    ui_draw_bag(2, 1, 0 /* unused ax */);
    return present_frame();
}

/* 0AA4:24C3  library: put dragged film/series (series id-1000) on the current genre shelf, count++ */
u16 library_film_to_shelf(void) {
    u16 ax;
    u32 l_4;
    FUNC_TICK(50);
    l_4 = library_at(g_room_owner);
    if (((i16)g_drag_film) >= 1000) {
        ax = g_drag_film + 0xFC18;
    } else {
        ax = g_drag_film;
    }
    FP16(l_4, (g_selected_genre << 3) + (((u16)(i8)FP8(l_4, g_selected_genre + 0x60)) << 1)) = ax;
    FP8(l_4, g_selected_genre + 0x60) = FP8(l_4, g_selected_genre + 0x60) + 1;
    /* ORIGINAL BUG (fix "library-owner"): taking the programme off the shelf
     * withdrew it like a sale (program_withdraw: owner 0, on the market);
     * filing it again does not give it back to its owner, so the dealer's
     * refill can offer it while it sits in this library.  Fixed: set the
     * owner, as bag_unload_to_library does for programmes from the case. */
    if (fix_on(FIX_LIBRARY_OWNER)) {
        if (((i16)g_drag_film) >= 1000) series(g_drag_film - 1000).owner = (u8)g_room_owner;
        else films(g_drag_film).owner = (u8)g_room_owner;
    }
    ui_drag_end();
    g_room_mode = 0;
    ui_draw_library_shelf(g_room_owner);
    return present_frame();
}

/* 0AA4:2546  film agency: append dragged film to the bag (strips the +10000 'fresh' marker) */
u16 filmagency_film_to_bag(void) {
    FUNC_TICK(18);
    if (((i16)g_drag_film) > 10000) {
        g_drag_film = g_drag_film - 0x2710;
    }
    g_bag(g_bag_count) = g_drag_film;
    g_bag_count = g_bag_count + 1;
    ui_drag_end();
    g_room_mode = 0;
    ui_draw_bag(2, 2, 0 /* unused ax */);
    return present_frame();
}

/* 0AA4:2586  film agency: give a freshly bought film back to the dealer: refund price (negative ledger entry cat 1/2), shelf slot available again */
u16 filmagency_return_film(u16 ax) {
    int cc;
    u16 si;
    u32 t6;
    u32 t10;
    u32 t13;
    u32 t17;
    FUNC_TICK(73);
    if (((i16)g_drag_film) > 10000) {
        g_drag_film = g_drag_film - 0x2710;
        si = 1;
        while (((i16)si) <= (i16)g_nfilmoffers) {   /* 10 in the original (--film-offers) */
            LOOP_TICK(13);
            cc = (dealer(si).program) == g_drag_film;
            ax = dealer(si).program;
            if (cc) {
                goto L_25BA;
            } else {
                si = si + 1;
                continue;
            }
        }
    }
    return ax;
L_25BA:
    if (((i16)g_drag_film) >= 1000) {
        g_drag_film = g_drag_film - 0x3E8;
        t13 = series_price(g_drag_film);
        MEM32((u16)(FPSEG(g_cur_player_rec)), (u16)(FPOFF(g_cur_player_rec)) + 8) = FP32(g_cur_player_rec, 8) + t13;
        t17 = -series_price(g_drag_film);
        ledger_add(LEDGER_SERIES_BOUGHT, t17);
    } else {
        t6 = movie_price(g_drag_film);
        MEM32((u16)(FPSEG(g_cur_player_rec)), (u16)(FPOFF(g_cur_player_rec)) + 8) = FP32(g_cur_player_rec, 8) + t6;
        t10 = -movie_price(g_drag_film);
        ledger_add(LEDGER_FILMS_BOUGHT, t10);
    }
    dealer(si).shelf = (dealer(si).shelf) + 0x64;
    ui_drag_end();
    g_room_mode = 0;
    ui_draw_dealer_shelf(1);
    present_frame();
    ax = ui_draw_money();
    return ax;
}

/* 0AA4:267A  ad agency: drop held ad back on its offer slot (unmark taken) */
u16 ad_return_offer(void) {
    u16 si;
    u16 t3;
    FUNC_TICK(17);
    si = 1;
    while ((ad_offers(si).ad) != g_drag_ad) {
        LOOP_TICK(13);
        if (((i16)si) > (i16)g_nadoffers) break;   /* 10 in the original (--ad-offers) */
        si = si + 1;
    }
    ui_drag_end();
    /* not found (never: the carried folder is an offer) the original writes past
     * offer 10 into the film dealer's slot 0 (DS:19ED, unused) -- kept, classic;
     * past more offers it would land in dealer slots 1.. (from DS:19F5), in use */
    if (!(g_nadoffers > 10 && (i16)si > (i16)g_nadoffers))
        ad_offers(si).shelf = (ad_offers(si).shelf) + 0x64;
    ui_draw_ad_offers();
    t3 = present_frame();
    g_room_mode = 0;
    return t3;
}

/* 0AA4:26CD  ad agency: add held ad to player's contracts (spots 0, deadline day = today + ad days), count++ */
u16 ad_take_contract(void) {
    u32 l_4;
    u16 t12;
    FUNC_TICK(54);
    l_4 = contracts_at(g_player, g_player_ad_count(g_player));
    AT(contract, l_4).ad = g_drag_ad;
    AT(contract, l_4).deadline = ((u16)(i8)(ads(g_drag_ad).days)) + g_day;
    AT(contract, l_4).aired = 0;
    g_player_ad_count(g_player) = g_player_ad_count(g_player) + 1;
    ui_drag_end();
    ui_draw_bag(2, 3, 0 /* unused ax */);
    t12 = present_frame();
    g_room_mode = 0;
    return t12;
}

/* 0AA4:2761  screenplay agency: return held script to its shelf slot with refund (negative ledger cat 4) */
u16 script_return_to_agency(void) {
    u16 si;
    u32 t6;
    u16 t7;
    FUNC_TICK(48);
    ui_drag_end();
    si = 1;
    while ((script_offers(si).script) != g_drag_item) {
        LOOP_TICK(11);
        si = si + 1;
    }
    script_offers(si).status = (script_offers(si).status) + 0x64;
    MEM32((u16)(FPSEG(g_cur_player_rec)), (u16)(FPOFF(g_cur_player_rec)) + 8) = FP32(g_cur_player_rec, 8) + (script_offers(si).price);
    t6 = -(script_offers(si).price);
    ledger_add(LEDGER_SCRIPTS, t6);
    ui_draw_script_offers();
    ui_draw_money();
    t7 = present_frame();
    g_room_mode = 0;
    return t7;
}

/* 0AA4:2807  screenplay agency: add held script to player's script list (+2 aux byte, episode counter 1) */
u16 script_to_bag(void) {
    u32 l_4;
    u16 t8;
    FUNC_TICK(45);
    l_4 = owned_scripts_at(g_player, g_scripts_owned_count(g_player));
    AT(owned_script, l_4).script = g_drag_item;
    AT(owned_script, l_4).aux = g_drag_aux_b;
    AT(owned_script, l_4).episode = 1;
    g_scripts_owned_count(g_player) = g_scripts_owned_count(g_player) + 1;
    ui_drag_end();
    ui_draw_bag(2, 4, 0 /* unused ax */);
    t8 = present_frame();
    g_room_mode = 0;
    return t8;
}

/* 0AA4:2884  studio: put held script back into player's script list with its episode counter */
u16 script_to_bag_from_studio(void) {
    u32 l_4;
    FUNC_TICK(42);
    l_4 = owned_scripts_at(g_player, g_scripts_owned_count(g_player));
    AT(owned_script, l_4).script = g_drag_item;
    AT(owned_script, l_4).episode = g_drag_aux;
    g_scripts_owned_count(g_player) = g_scripts_owned_count(g_player) + 1;
    ui_drag_end();
    ui_draw_bag(2, 5, 0 /* unused ax */);
    return present_frame();
}

/* 0AA4:28F2  put held script into the current studio (script id, episode = aux, progress = aux-1) */
u16 studio_insert_script(void) {
    u32 l_4;
    FUNC_TICK(33);
    l_4 = studios_at(g_cur_studio);
    AT(studio, l_4).script = g_drag_item;
    AT(studio, l_4).episode = g_drag_aux_b;
    AT(studio, l_4).episode_last = g_drag_aux_b - 1;
    ui_drag_end();
    ui_draw_studio();
    ui_draw_bag(2, 5, 0 /* unused ax */);
    return present_frame();
}

/* 0AA4:2950  take the script out of the current studio into the hand (cancels production state 222A_0206) */
void studio_take_script(void) {
    u32 l_4;
    u16 t4;
    u16 t5;
    FUNC_TICK(64);
    l_4 = studios_at(g_cur_studio);
    g_drag_item = AT(studio, l_4).script;
    g_drag_aux = (u16)(i8)(AT(studio, l_4).episode);
    if (g_drag_aux == 0) {
        g_drag_aux = 1;
    }
    studio_cancel_production(g_cur_studio);
    t4 = cursor_y();
    t5 = cursor_x();
    ui_drag_begin(0x10, 0x1D, t5 + 0xFFFC, t4 + 0xFFF1, 0, 0, 0xB);
    drag_image_draw(MK32(g_room_gfx_hi, g_room_gfx_lo + 0xA));
    AT(studio, l_4).script = 0;
    clip_reset();
    bitmap_put_screen(g_room_gfx3, 0xF8, 0x53);
    present_frame();
    return;
}

/* 0AA4:2A04  when dropping a script on an occupied studio: pick up the studio's old script (it is replaced later) */
void studio_swap_script(void) {
    u32 l_4;
    u16 t2;
    u16 t3;
    FUNC_TICK(65);
    l_4 = studios_at(g_cur_studio);
    if ((AT(studio, l_4).script) == 0) {
        g_swap_script = 0;
    } else {
        g_swap_script = AT(studio, l_4).script;
        G16(0x8F07) = (u16)(i8)(AT(studio, l_4).episode);
        studio_cancel_production(g_cur_studio);
    }
    studio_insert_script();
    if (g_swap_script != 0) {
        g_drag_item = g_swap_script;
        g_drag_aux = G16(0x8F07);
        t2 = cursor_y();
        t3 = cursor_x();
        ui_drag_begin(0x10, 0x1D, t3 + 0xFFFC, t2 + 0xFFF1, 0, 0, 0xB);
        drag_image_draw(MK32(g_room_gfx_hi, g_room_gfx_lo + 0xA));
        g_room_mode = 2;
    } else {
        g_room_mode = 0;
    }
    return;
}

/* 0AA4:2AB7  animate a 0<->30 slider variable (*a0) in steps of 2 with redraw (TV cover/blind; used with g_tv_off) */
void ui_animate_toggle(u32 a0, u16 a2, u16 a3) {
    u16 di;
    u16 si;
    FUNC_TICK(19);
    if (FP16(a0, 0) != 0) {
        si = 0;
        di = 0xFFFE;
    } else {
        si = 0x1E;
        di = 2;
    }
    cursor_hide();
    while (FP16(a0, 0) != si) {
        LOOP_TICK(28);
        FP16(a0, 0) = FP16(a0, 0) + di;
        options_draw_switch(FP16(a0, 0));
        blit_transparent(g_actor_txt, g_screen_buf, a2, a3, 0x20, 0, 0x20, 0xA, 0 /* unused ax */);
        present_frame();
    }
    cursor_show(0 /* unused ax */);
    return;
}

/* 0AA4:2B31  after TV on/off: play silence (sound 0xDC) when off, else the jingle of the program running on the watched channel (0054_3738) */
void tv_update_sound(void) {
    FUNC_TICK(13);
    if (((i16)(clock_hour())) >= 1) {
        if (g_tv_off != 0) {
            tv_show_picture(0xDC);
        } else {
            tv_update_picture();
        }
    }
    return;
}

/* 0AA4:2B55  transmitter map: after selling/scrapping move to next transmitter, redraw, recompute coverage */
u16 txmap_after_sale(void) {
    u16 t1;
    FUNC_TICK(28);
    g_redraw = 1;
    t1 = tx_next_slot(g_selected_index + 1);
    g_selected_index = t1;
    transmitter_sale_dialog(1, DSP(0x8F0D));
    g_room_mode = 3;
    ui_draw_transmitter_map(g_selected_index);
    transmitters_recalc(g_player);
    present_frame();
    return ui_draw_money();
}
