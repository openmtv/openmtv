/* System: resource loading, disk switching, random_n, the game clock.
 *
 * Original module 0D5E of MTV.EXE (Rainbow Arts, 1991).  Decompiled by
 * tools/decomp.py and hand-maintained since the freeze (git tag
 * generated-final).  Every function keeps its original address in the
 * comment above it; that address ties it to the machine-level twin in gen/
 * and to the difftest tables -- keep it when editing.
 */
#include "game.h"
#include "hrs.h"
#include "hr.h"
#include "seat.h"
#include "fixes.h"

/* 0D5E:0001  init the resource manager, open the RSC files, list the present resource ids into 0x8C7C */
u16 res_init(void) {
    u16 t1;
    FUNC_TICK(18);
    t1 = res_init_2(DSP(0x8EB0));
    R.ax = t1;
    rtl_res_open_all();
    g_res_open_count = g_res_open_count + 1;
    debug_nop();
    res_select_drive(g_cursor_sprites_hi(7));
    return res_list_present(DSP(0x8C7C));
}

/* 0D5E:0035  1 if resource 500 exists (all data available), else 2 (second disk needed) */
u16 res_disk_mode(void) {
    u16 ax;
    FUNC_TICK(12);
    if ((res_exists(0x1F4, 0x1F4)) != 0xFFFF) {
        ax = 1;
    } else {
        ax = 2;
    }
    return ax;
}

/* 0D5E:0050  nested: on first entry stop the game clock and open the resource files */
u16 disk_access_begin(u16 ax) {
    u16 t1;
    seat_disk_depth[seat_cur > 0 ? seat_cur : 0]++;
    FUNC_TICK(6);
    g_disk_idle = g_disk_idle - 1;
    if (g_disk_idle == 0) {
        /* seats: the clock is the world's; while another seat loads, the
         * world stands anyway (runtime/seat.h) */
        t1 = seat_cur > 0 ? ax : game_clock_stop(ax);
        ax = res_files_open(t1);
    }
    return ax;
}

/* seats: seat 0 loads or changes rooms (runtime/seat.c seat_disk_pass): its disk
 * accesses and room changes counted per seat (g_disk_idle is shared: another seat
 * may be in the middle of a load of its own when it passed) */
int seat_disk_depth[SEATS_MAX], seat_room_depth[SEATS_MAX];
int seat_disk_busy(void) { return seat_disk_depth[0] > 0 || seat_room_depth[0] > 0; }

/* 0D5E:0066  nested: on last exit close the resource files and restart the game clock */
u16 disk_access_end(u16 ax) {
    if (seat_disk_depth[seat_cur > 0 ? seat_cur : 0] > 0) seat_disk_depth[seat_cur > 0 ? seat_cur : 0]--;
    FUNC_TICK(6);
    g_disk_idle = g_disk_idle + 1;
    if (g_disk_idle == 1) {
        res_files_close(ax);
        if (seat_cur <= 0) ax = game_clock_start();
    }
    return ax;
}

/* 0D5E:007C  open the RSC files */
u16 res_files_open(u16 ax) {
    u32 t1;
    FUNC_TICK(4);
    R.ax = ax;
    t1 = rtl_res_open_all();
    g_res_open_count = g_res_open_count + 1;
    debug_nop();
    return LO16(t1);
}

/* 0D5E:008B  close the RSC files */
void res_files_close(u16 ax) {
    FUNC_TICK(5);
    res_flush();
    res_close_all(ax);
    g_res_open_count = g_res_open_count - 1;
    debug_nop();
    return;
}

/* 0D5E:009F  empty hook */
void debug_nop(void) {
    FUNC_TICK(1);
    return;
}

/* 0D5E:00A0  allocate and read a resource without decompressing */
u32 res_load_raw(u16 a0, u16 ax) {
    u16 dx;
    u32 rv_;
    u16 t1;
    u32 t2;
    u16 t3;
    u8 fr_[12] = {0};                  /* bytes bp-12 .. bp-1 of the original frame */
    FUNC_TICK(36);
    disk_access_begin(ax);
    t1 = res_query_wait(a0, (fr_ + 12 - 0x2), 0);
    FR16(fr_, 12, 0x8) = ((i16)t1) >> 0xF;
    FR16(fr_, 12, 0xA) = t1;
    t2 = mem_calloc(FR16(fr_, 12, 0xA), 1);
    FR16(fr_, 12, 0x4) = HI16(t2);
    FR16(fr_, 12, 0x6) = LO16(t2);
    t3 = res_read_raw(a0, HPTR(MK32(FR16(fr_, 12, 0x4), FR16(fr_, 12, 0x6))), LO16(t2));
    disk_access_end(t3);
    ax = FR16(fr_, 12, 0x6);
    dx = FR16(fr_, 12, 0x4);
    rv_ = MK32(dx, ax);
    return rv_;
}

/* 0D5E:00FA  read a resource's raw bytes into buf (inside disk_access_begin/end) */
u16 res_read_raw(u16 a0, u8 *buf, u16 ax) {
    u16 t1;
    u16 t2;
    FUNC_TICK(15);
    HRS(hrs_res_read(a0, buf));
    hr_res_read(a0, buf);
    t1 = disk_access_begin(ax);
    t2 = res_read(a0, buf, t1);
    return disk_access_end(t2);
}

/* 0D5E:011B  load + decompress a resource (like res_load, mode flag 0) */
u32 res_load0(u16 a0) {
    u16 ax;
    u16 dx;
    u32 rv_;
    u16 t1;
    u32 t2;
    u32 t3;
    u8 fr_[12] = {0};                  /* bytes bp-12 .. bp-1 of the original frame */
    FUNC_TICK(52);
    t1 = res_query_wait(a0, (fr_ + 12 - 0xA), 0);
    t2 = mem_calloc(FR16(fr_, 12, 0xA), 1);
    FR16(fr_, 12, 0x2) = HI16(t2);
    FR16(fr_, 12, 0x4) = LO16(t2);
    t3 = mem_calloc(t1, 1);
    FR16(fr_, 12, 0x6) = HI16(t3);
    FR16(fr_, 12, 0x8) = LO16(t3);
    res_read_raw(a0, HPTR(MK32(FR16(fr_, 12, 0x6), FR16(fr_, 12, 0x8))), LO16(t3));
    huff_unpack_bank(FR32(fr_, 12, 0x8), FR32(fr_, 12, 0x4), 0);
    mem_free(FR16(fr_, 12, 0x8), FR16(fr_, 12, 0x6));
    ax = FR16(fr_, 12, 0x4);
    dx = FR16(fr_, 12, 0x2);
    rv_ = MK32(dx, ax);
    return rv_;
}

/* 0D5E:01A1  load resource by id and decompress it into a new buffer */
u32 res_load(u16 a0) {
    u16 ax;
    u16 dx;
    u32 rv_;
    u16 t1;
    u32 t2;
    u32 t3;
    u8 fr_[12] = {0};                  /* bytes bp-12 .. bp-1 of the original frame */
    FUNC_TICK(52);
    t1 = res_query_wait(a0, (fr_ + 12 - 0xA), 1);
    t2 = mem_calloc(FR16(fr_, 12, 0xA), 1);
    FR16(fr_, 12, 0x2) = HI16(t2);
    FR16(fr_, 12, 0x4) = LO16(t2);
    t3 = mem_calloc(t1, 1);
    FR16(fr_, 12, 0x6) = HI16(t3);
    FR16(fr_, 12, 0x8) = LO16(t3);
    res_read_raw(a0, HPTR(MK32(FR16(fr_, 12, 0x6), FR16(fr_, 12, 0x8))), LO16(t3));
    huff_unpack_bank(FR32(fr_, 12, 0x8), FR32(fr_, 12, 0x4), 1);
    mem_free(FR16(fr_, 12, 0x8), FR16(fr_, 12, 0x6));
    ax = FR16(fr_, 12, 0x4);
    dx = FR16(fr_, 12, 0x2);
    rv_ = MK32(dx, ax);
    return rv_;
}

/* 0D5E:0229  srand(...) then rand() % n + 1  (n = 0 divides by zero) */
u16 random_n(u16 a0, u16 a1) {
    u16 ax;
    u16 t1;
    u32 t2;
    u16 r;
    FUNC_TICK(22);
    t1 = timer_read();
    /* several humans: a count of the calls goes into the seed, the same on every machine.
     * In another seat's turn the world's time stands (runtime/seat.c), the timer with it --
     * every call drew the same number, and a loop that draws until it finds a free one (the
     * ad agency's refill once a human took an ad, the script agency's) never ended: the
     * game hung on every machine.  In seat 0 the same loop (the computer player's refill)
     * waited for the timer's next tick for each new number -- up to a second in which the
     * world stood for everyone.  Alone the original's draws. */
    if (seat_humans) { static u16 calls; t1 = (u16)(t1 + ++calls * 7); }
    t2 = MK32(0, t1 + a1) + g_cursor_sprites(6);
    rtl_srand(LO16(t2));                 /* the original pushes the high word too; srand reads one */
    r = LO16(rtl_rand());
    /* ORIGINAL BUG (fix "random0"): n = 0 -- e.g. a random door on a floor
     * without doors -- stops the game with "Divide error".  Fixed: 1. */
    if (a0 == 0 && fix_on(FIX_RANDOM0)) return 1;
    ax = (u16)((u16)((i32)(i16)r % (i16)a0)) + 1;
    return ax;
}

/* 0D5E:025C  restart the PIT timer and zero the accumulated clock ticks */
void game_clock_reset(void) {
    if (mp_clock_skip()) return;            /* seats: the clock is the world's (runtime/seat.h) */
    FUNC_TICK(12);
    timer_reset();
    timer_start(0);
    g_clock_acc = 0;
    g_timer_run_count = g_timer_run_count + 1;
    debug_nop();
    /* multiplayer: the timer starts at 0 again (the morning, at the boss), but the frame
     * timer -- the animation's last tick -- is left as it was (game_clock_start would set
     * it): the building, the elevator and the figures would wait until the timer caught up
     * with it, for seconds or minutes, while the first human sits with the boss and the
     * others wait for the cabin.  Alone it goes unseen: the boss room is still, and leaving
     * it restarts the clock. */
    if (seat_humans) g_frame_timer = (u16)-(u16)((i32)50 / (i16)g_frame_divisor);
    return;
}

/* 0D5E:0280  unless paused add elapsed timer ticks * game speed to the clock accumulator */
u16 game_clock_stop(u16 ax) {
    u16 t1;
    u32 t2;
    if (mp_clock_skip()) return ax;         /* seats: the clock is the world's (runtime/seat.h) */
    FUNC_TICK(18);
    if (g_clock_paused == 0) {
        t1 = timer_read();
        t2 = (u32)(MK32(((i16)g_game_speed) >> 0xF, g_game_speed) * MK32(0, t1));
        g_clock_acc = g_clock_acc + t2;
        ax = LO16(t2);
    }
    timer_stop();
    g_timer_run_count = g_timer_run_count - 1;
    debug_nop();
    return ax;
}

/* 0D5E:02B1  restart the timer; frame timer 0x6141 = -(50 / frame divisor 0x7A50) */
u16 game_clock_start(void) {
    u16 t3;
    if (mp_clock_skip()) return g_frame_timer;   /* seats: the clock is the world's (runtime/seat.h) */
    FUNC_TICK(15);
    timer_reset();
    timer_start(0);
    t3 = (u16)((u16)((i32)50 / (i16)g_frame_divisor));
    g_frame_timer = -t3;
    g_timer_run_count = g_timer_run_count + 1;
    debug_nop();
    return -t3;
}

/* 0D5E:02D6  *out = cached game seconds since 17:00 */
u16 game_clock_get(u8 *a0) {
    u16 t1;
    FUNC_TICK(7);
    t1 = g_clock_seconds;
    HP16(a0, 0) = g_clock_seconds;
    return t1;
}

/* 0D5E:02E4  game seconds = (accumulated + elapsed*speed) / 50; sets hour (17 + h, wrapped), minute 0x71EC, second 0x71EE */
void game_clock_update(u8 *a0) {
    u16 ax;
    u16 di;
    u16 si;
    u16 l_2;
    u16 t2;
    u16 t12;
    u16 t16;
    FUNC_TICK(81);
    if (g_clock_paused != 0) {
        ax = 0;
    } else {
        ax = timer_read();
    }
    g_clock_timer = ax;
    if (g_clock_timer >= 0xEA60) {
        game_clock_stop(0 /* unused ax */);
        game_clock_start();
        t2 = timer_read();
        g_clock_timer = t2;
    }
    if (speed4_fine_clock()) {   /* --fourth-speed at its speed: whole seconds (fixes.c) */
        HP16(a0, 0) = LO16((g_clock_acc + (u32)g_clock_timer * g_game_speed) / 0x32);
    } else {
        HP16(a0, 0) = LO16(((u32)g_clock_acc / (u32)0x32)) + ((u16)((u16)(g_clock_timer / 0x32)) * g_game_speed);
    }
    t12 = (u16)((i32)(i16)HP16(a0, 0) / 3600);
    t16 = (u16)((i32)(i16)HP16(a0, 0) / 60);
    l_2 = (HP16(a0, 0) - ((t16 - (t12 * 0x3C)) * 0x3C)) - (t12 * 0xE10);
    si = t12 + 0x11;
    di = t16 - (t12 * 0x3C);
    if (((i16)(t12 + 0x11)) >= 24) {
        si = si - 0x18;
    }
    g_clock_seconds = HP16(a0, 0);
    G32(0x71EA) = MK32(di, si);
    g_second = l_2;
    return;
}

/* 0D5E:03AD  set each sprite's data pointer in a sprite bank (count, then w/h/ptr entries; data size w*h*8) */
void sprite_bank_fixup(u32 a0) {
    u16 di;
    u16 si;
    u32 l_4;
    u32 l_8;
    FUNC_TICK(29);
    l_8 = MK32(FPSEG(a0), (FPOFF(a0) + (FP16(a0, 0) << 3)) + 2);
    l_4 = MK32(FPSEG(a0), FPOFF(a0) + 2);
    si = 1;
    di = FP16(a0, 0);
    while (((i16)si) <= ((i16)di)) {
        LOOP_TICK(17);
        FP32(l_4, 4) = l_8;
        l_8 = MK32(HI16(l_8), LO16(l_8) + ((FP16(l_4, 2) * FP16(l_4, 0)) << 3));
        l_4 = MK32(HI16(l_4), LO16(l_4) + 8);
        si = si + 1;
    }
    return;
}

/* 0D5E:0416  free *pp and set it to NULL */
u16 mem_free_ptr(u8 *a0) {
    u16 t1;
    FUNC_TICK(12);
    t1 = mem_free(HP16(a0, 0), HP16(a0, 2));
    HP32(a0, 0) = 0;
    return t1;
}

/* 0D5E:043A  ?n * G16(0x838E) / 8 (+ G16(0x838A) when a1) - size of a figure sprite set in the figure buffer */
u16 figure_sprite_stride(u16 a0, u16 a1) {
    u16 ax;
    FUNC_TICK(41);
    if (a1 != 0) {
        ax = LO16(((u32)((i32)((u32)(MK32(((i16)(g_figure_sprite_cols + g_figure_sprite_cols_adj)) >> 0xF, g_figure_sprite_cols + g_figure_sprite_cols_adj) * MK32(((i16)a0) >> 0xF, a0))) / (i32)8)));
    } else {
        ax = LO16(((u32)((i32)((u32)(MK32(((i16)g_figure_sprite_cols) >> 0xF, g_figure_sprite_cols) * MK32(((i16)a0) >> 0xF, a0))) / (i32)8)));
    }
    return ax;
}

/* 0D5E:048D  resource packed size (return) and unpacked size (*out); missing -> "insert disk" message and wait for a click, retry */
u16 res_query_wait(u16 a0, u8 *buf, u16 a3) {
    u16 ax;
    u16 di;
    u16 si;
    u32 rv_;
    u8 fr_[4] = {0};                  /* bytes bp-4 .. bp-1 of the original frame */
    FUNC_TICK(14);
    di = a0;
    for (;;) {
        LOOP_TICK(41);
        res_bank_sizes(di, (fr_ + 4 - 0x2), buf, a3);
        if (FR16(fr_, 4, 0x2) != 0xFFFF) {
            ax = 0;
        } else {
            ax = 1;
        }
        if (ax == 0) break;
        if (((i16)di) <= 300) {
            si = 0x11F;
        } else {
            si = 0x1AD;
        }
        ui_disk_prompt(0, si);
        for (;;) {
            LOOP_TICK(8);
            game_frame();
            if (g_click != 0) {
                ax = 0;
            } else {
                ax = 1;
            }
            if (ax != 0) {
                continue;
            } else {
                break;
            }
        }
        ui_disk_prompt(1, 0);
    }
    ax = FR16(fr_, 4, 0x2);
    rv_ = ax;
    return (u16)rv_;
}

/* 0D5E:050F  keep a resource packed in cache slot (0x7C00, 6 bytes: ptr + unpacked size) */
void res_cache_packed(u16 a0, u16 a1, u16 a2) {
    u16 t1;
    FUNC_TICK(34);
    t1 = res_query_wait(a0, HPTR(MK32(DSEG, (a1 * 6) + 0x7C04)), a2);
    G32((a1 * 6) + 0x7C00) = res_load_raw(a0, t1);
    return;
}

/* 0D5E:0558  decompress a cached resource into a new buffer */
u32 res_cache_unpack(u16 a0, u16 a1) {
    u16 ax;
    u16 dx;
    u32 l_4;
    FUNC_TICK(37);
    l_4 = mem_calloc(G16((a0 * 6) + 0x7C04), 1);
    huff_unpack_bank(G32((a0 * 6) + 0x7C00), l_4, a1);
    ax = LO16(l_4);
    dx = HI16(l_4);
    return MK32(dx, ax);
}

/* 0D5E:05BC  load walking-figure sprite sets: players 17..19 (alien 24 for the human in alien mode), 20, 21, intruder 23 (terrorist) or 22 (bailiff / guardian) */
void figures_load_sprites(void) {
    int cc;
    u16 si;
    FUNC_TICK(38);
    si = 0;
    while (((i16)si) < 3) {
        LOOP_TICK(26);
        cc = si != g_player_figure;
        if (cc || (figures(g_player_figure).speed) != 2) {
            figure_load_sprite_slot(si + 0x11, si);
        } else {
            figure_load_sprite_slot(0x18, si);
        }
        si = si + 1;
    }
    figure_load_sprite_slot(0x14, 3);
    figure_load_sprite_slot(0x15, 4);
    if (((i16)g_npc_type) <= 0 || ((i16)g_npc_type) > 2) {
        if (g_npc_type == 3 || g_npc_type == 4) {
            figure_load_sprite_slot(0x16, 5);
        }
    } else {
        figure_load_sprite_slot(0x17, 5);
    }
    return;
}

/* 0D5E:0664  load sprite set res into figure slot of buffer 0xF8 unless already there (0xD0E) */
void figure_load_sprite_slot(u16 a0, u16 a1) {
    u16 di;
    u16 si;
    u32 l_4;
    u32 l_8;
    u16 t1;
    u16 t5;
    FUNC_TICK(65);
    t1 = figure_sprite_stride(0x130, 1);
    l_8 = MK32(FPSEG(g_figure_sprites), (FPOFF(g_figure_sprites) + ((t1 * a1) * 0xA)) + 0x1E2);
    si = a0;
    di = a1;
    if (g_figure_sprite_res(a1) != a0) {
        l_4 = res_load(si);
        t5 = figure_sprite_stride(0x130, 1);
        far_memcpy(HPTR(l_8), HPTR(MK32(HI16(l_4), LO16(l_4) + 0x52)), t5 * 0xA);
        mem_free(LO16(l_4), HI16(l_4));
        g_figure_sprite_res(di) = si;
    }
    return;
}

/* 0D5E:06FB  farcalloc(count, size) */
u32 mem_calloc(u16 a0, u16 a1) {
    u16 ax;
    u16 dx;
    u32 l_4;
    FUNC_TICK(21);
    l_4 = rtl_farcalloc(a0, a1);
    ax = LO16(l_4);
    dx = HI16(l_4);
    return MK32(dx, ax);
}

/* 0D5E:0729  farfree(ptr) */
u16 mem_free(u16 a0, u16 a1) {
    FUNC_TICK(8);
    return rtl_farfree(MK32(a1, a0));
}

/* 0D5E:073B  nested pause: first call folds elapsed time in and sets 0x7A92 */
u16 game_clock_pause(u16 ax) {
    u16 t1;
    u16 t3;
    if (mp_clock_skip()) return ax;         /* seats: the clock is the world's (runtime/seat.h) */
    FUNC_TICK(21);
    g_clock_pause_depth = g_clock_pause_depth + 1;
    if (g_clock_pause_depth == 1) {
        t1 = timer_read();
        g_clock_acc = g_clock_acc + ((u32)(MK32(((i16)g_game_speed) >> 0xF, g_game_speed) * MK32(0, t1)));
        t3 = timer_start(0);
        g_clock_paused = 1;
        ax = t3;
    }
    return ax;
}

/* 0D5E:0773  nested resume: last call clears 0x7A92 and restarts the timer */
u16 game_clock_resume(u16 ax) {
    u16 t1;
    if (mp_clock_skip()) return ax;         /* seats: the clock is the world's (runtime/seat.h) */
    FUNC_TICK(11);
    g_clock_pause_depth = g_clock_pause_depth - 1;
    if (g_clock_pause_depth == 0) {
        g_clock_paused = 0;
        t1 = timer_start(0);
        g_frame_timer = 0;
        ax = t1;
    }
    return ax;
}

/* 0D5E:0795  new 500-byte buffer holding a copy of text n of the language table */
u32 text_dup(u16 a0) {
    u16 ax;
    u16 dx;
    u32 l_4;
    u32 t4;
    FUNC_TICK(35);
    l_4 = mem_calloc(0x1F4, 1);
    t4 = rtl_strlen_w(W2(g_text(a0)));
    far_memcpy(HPTR(l_4), HPTR(g_text(a0)), LO16(t4));
    ax = LO16(l_4);
    dx = HI16(l_4);
    return MK32(dx, ax);
}
