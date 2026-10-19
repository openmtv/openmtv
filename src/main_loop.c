/* Main loop: main loop, entering and leaving rooms, room assets, the game clock tick, hotkeys, TV picture.
 *
 * Original module 0054 of MTV.EXE (Rainbow Arts, 1991).  Decompiled by
 * tools/decomp.py and hand-maintained since the freeze (git tag
 * generated-final).  Every function keeps its original address in the
 * comment above it; that address ties it to the machine-level twin in gen/
 * and to the difftest tables -- keep it when editing.
 */
#include "game.h"
#include "seat.h"
#include "mpsave.h"
#include "fixes.h"

/* main's calls the way the machine code made them: the argument words (in
 * push order) and the return address on the emulated stack while the callee
 * runs, so callees' frames -- and the stack garbage a few of them read --
 * stay where they were; framed_pop() after the call */
static void framed(u16 ip, int n, ...) {
    va_list ap;
    va_start(ap, n);
    for (int i = 0; i < n; i++) push16((u16)va_arg(ap, int));
    va_end(ap);
    push16(SEGV(0x0054)); push16(ip);
}
static void framed_pop(int n) { R.sp = (u16)(R.sp + 4 + 2 * n); }

/* 0054:0005  game entry: parse the arguments (d/e: language, '*'s: debug
 * level), set up video, keyboard, sound, mouse and timer, load the resident
 * resources, play the intro, then run the main loop until the game ends
 * (g_room 0x3E8).  Exact time like the run-time library (rtl.h): it was
 * machine-level code until Phase 3 */
u16 game_main(u16 argc, u32 argv) {
    u16 ax, bp_;
    u32 r;
    RTL_ENTRY();
    state_activate();                    /* the game's state in host memory from here (state.c) */
    push16(R.bp); R.bp = R.sp; bp_ = R.sp;
    R.sp -= 8;
    push16(R.si); push16(R.di);
    R.di = 0;                            /* SI and DI live in the registers, as in the
                                          * machine code: the boss room reads them (rooms_people.c) */
    g_language = 0xFFFF;
    T += 9;
    if ((i16)argc > 1) {
        u8 c = FP8(FP32(argv, 4), 0);
        T += 4;
        if (c == 'd' || (T += 4, c == 'D')) { g_language = 0; T += 1; }
        T += 4;
        if (c == 'e' || (T += 4, c == 'E')) { g_language = 1; T += 1; }
        LOC32(4) = FP32(argv, 8);
        R.si = 0;
        T += 6;
        for (;;) {                       /* debug level: leading '*'s of the 2nd argument */
            T += 3;
            if (FP8(LOC32(4), R.si) != '*') break;
            POLL();
            R.si++;
            T += 1;
        }
        T += 2;
        if ((i16)R.si > 10) { ax = 10; T += 1; } else { ax = R.si; T += 2; }
        g_debug_level = ax;
        T += 1;
    }
    T += 6;
    framed(0x0093, 3, 0xC8, DSEG, 0x79C6); mem_zero(DSP(0x79C6), 0xC8); framed_pop(3);
    g_elevator_speed = 3;
    g_frame_divisor = 0xC;
    G16(0x7A28) = 1;
    T += 9;
    rtl_atexit(1, SEGV(0x3859));         /* keyboard */
    T += 6;
    rtl_atexit(0xD, SEGV(0x365D));       /* timer */
    T += 6;
    rtl_atexit(0, SEGV(0x3741));         /* video mode */
    T += 6;
    rtl_atexit(0x376, SEGV(0x35F8));     /* sound */
    T += 2;
    rtl_video_detect();
    T += 1;
    rtl_video_mode13();
    T += 5;
    rtl_video_segs();
    T += 2;
    rtl_kbd_detect();
    T += 1;
    rtl_kbd_flush();
    T += 1;
    framed(0x010F, 0); ax = res_init(); framed_pop(0);
    T += 1;
    framed(0x0114, 0); ax = disk_access_end(ax); framed_pop(0);
    T += 1;
    framed(0x0119, 0); disk_access_begin(ax); framed_pop(0);
    T += 1;
    framed(0x011E, 0); ax = snd_init(); framed_pop(0);
    T += 1;
    framed(0x0123, 0); disk_access_end(ax); framed_pop(0);
    T += 1;
    rtl_mouse_detect();
    T += 1;
    rtl_mouse_setup();
    T += 1;
    framed(0x0132, 0); timer_init(); framed_pop(0);
    T += 3;
    framed(0x013B, 1, 3); g_font_2 = res_load(3); framed_pop(1);
    T += 7;
    framed(0x014D, 1, 2); g_ui_atlas = res_load(2); framed_pop(1);
    g_cursor_sprites(1) = MK32(FPSEG(g_ui_atlas), FPOFF(g_ui_atlas) + 2);
    g_cursor_sprites(2) = MK32(FPSEG(g_ui_atlas), FPOFF(g_ui_atlas) + 0xA);
    g_cursor_sprites(3) = MK32(FPSEG(g_ui_atlas), FPOFF(g_ui_atlas) + 0x12);
    g_frame_tiles1 = MK32(FPSEG(g_ui_atlas), FPOFF(g_ui_atlas) + 0x1A);
    g_frame_tiles2 = MK32(FPSEG(g_ui_atlas), FPOFF(g_ui_atlas) + 0x62);
    T += 28;
    framed(0x01A9, 1, 0x12B); G32(0x8AD0) = res_load_raw(0x12B, 0x12B); framed_pop(1);
    T += 7;
    rtl_palette_set(G16(0x8AD0), G16(0x8AD2));
    T += 4;
    framed(0x01CF, 2, G16(0x8AD2), G16(0x8AD0)); ax = mem_free(G16(0x8AD0), G16(0x8AD2)); framed_pop(2);
    T += 3;
    if (g_language == 0xFFFF) {
        T += 1;
        framed(0x01DE, 0); ax = choose_language(); framed_pop(0);
    }
    T += 1;
    framed(0x01E3, 0); game_load_init_data(ax); framed_pop(0);
    g_player = 1;
    T += 4;
    framed(0x01F2, 1, 0x12C); r = res_exists(0x12C, 0x12C); framed_pop(1);
    T += 4;
    if (new_game_opt) timer_start(0);   /* --new-game, no intro: the tick block to the sound driver, as the intro hands it */
    else if ((u16)r != 0xFFFF) {         /* the intro, if its resource is there */
        T += 5;
        framed(0x0205, 2, 0, 0x1F4); snd_room_music_start(0x1F4, 0, 0x1F4); framed_pop(2);
        T += 2;
        framed(0x020D, 0); intro_play(); framed_pop(0);
        T += 5;
        framed(0x0219, 2, 0, 0x1F4); rtl_snd_room_music_stop(0x1F4, 0); framed_pop(2);
        T += 1;
    }
    T += 5;
    tables_count_init();                 /* 200 films and 50 series, or a mod's counts (state.c) */
    framed(0x0229, 2, 0x17, g_nfilms + 1); g_movies = mem_calloc(g_nfilms + 1, 0x17); framed_pop(2);
    T += 8;
    framed(0x0240, 2, 0x39, g_nseries + 1); g_series = mem_calloc(g_nseries + 1, 0x39); framed_pop(2);
    T += 8;
    framed(0x0257, 2, 0x1B3, 0x33); g_scripts = mem_calloc(0x33, 0x1B3); framed_pop(2);
    state_activate();                    /* the tables and the managers' names in host memory (state.c) */
    T += 4;
    framed(0x0266, 0); data_load_tables(); framed_pop(0);
    T += 3;
    framed(0x026F, 1, 1); G32(0x8AD0) = res_load_raw(1, 1); framed_pop(1);
    T += 7;
    rtl_palette_set(G16(0x8AD0), G16(0x8AD2));
    T += 4;
    framed(0x0295, 2, G16(0x8AD2), G16(0x8AD0)); ax = mem_free(G16(0x8AD0), G16(0x8AD2)); framed_pop(2);
    T += 2;
    framed(0x029D, 0); disk_access_begin(ax); framed_pop(0);
    T += 3;
    framed(0x02A6, 1, 6); g_bld_tiles = res_load(6); framed_pop(1);
    T += 7;
    framed(0x02B8, 1, 0xB); g_bld_sprites = res_load(0xB); framed_pop(1);
    T += 7;
    framed(0x02CA, 1, 0xF); g_tvfam_atlas = res_load(0xF); framed_pop(1);
    T += 7;
    framed(0x02DC, 1, 4); g_roomframe_tiles = res_load0(4); framed_pop(1);
    T += 7;
    framed(0x02EE, 1, 7); g_band_tiles = res_load0(7); framed_pop(1);
    T += 7;
    framed(0x0300, 1, 0xD); g_bld_side_tiles = res_load(0xD); framed_pop(1);
    T += 7;
    framed(0x0312, 1, 0x10); G32(0x837E) = res_load(0x10); framed_pop(1);
    openmtv_sign(G32(0x837E));       /* (src/openmtv_sign.c: OpenMTV's sign, untimed) */
    T += 7;
    framed(0x0328, 2, HI16(g_roomframe_tiles), LO16(g_roomframe_tiles));
    ax = bank_mem_size(g_roomframe_tiles);
    framed_pop(2);
    g_figure_sprite_cols = (u16)((u16)((u16)(ax + 0xFF9E) << 3) / 0x1A8);
    T += 11;
    if ((i16)g_figure_sprite_cols < 8) { g_figure_sprite_cols_adj = 1; T += 2; }
    else { g_figure_sprite_cols_adj = 0; T += 1; }
    T += 7;
    framed(0x0364, 3, 1, 2, 0xC); res_cache_packed(0xC, 2, 1); framed_pop(3);
    T += 8;
    framed(0x0376, 3, 0, 0, 5); res_cache_packed(5, 0, 0); framed_pop(3);
    T += 8;
    framed(0x0389, 3, 0, 1, 0xE); res_cache_packed(0xE, 1, 0); framed_pop(3);
    T += 8;
    framed(0x039C, 3, 0, 0xB, 0xEE); res_cache_packed(0xEE, 0xB, 0); framed_pop(3);
    T += 8;
    framed(0x03AF, 3, 0, 0xA, 0xED); res_cache_packed(0xED, 0xA, 0); framed_pop(3);
    T += 8;
    framed(0x03C2, 3, 0, 9, 0xEC); res_cache_packed(0xEC, 9, 0); framed_pop(3);
    T += 8;
    framed(0x03D5, 3, 0, 0xE, 0xDC); res_cache_packed(0xDC, 0xE, 0); framed_pop(3);
    T += 6;
    framed(0x03E5, 2, 1, 0x130); G16(0x8ACE) = figure_sprite_stride(0x130, 1); framed_pop(2);
    T += 12;
    {
        u16 n = (u16)((u16)((G16(0x8ACE) + 8) * 0x3C) + 2);
        framed(0x0402, 2, 1, n); g_figure_sprites = mem_calloc(n, 1); framed_pop(2);
    }
    FP16(g_figure_sprites, 0) = 0x3C;    /* 60 figure sprites: headers, then the images */
    LOC32(4) = MK32(FPSEG(g_figure_sprites), FPOFF(g_figure_sprites) + 0x1E2);
    R.si = 0;
    T += 11;
    for (;;) {
        T += 2;
        if ((i16)R.si >= 60) break;
        POLL();
        FP16(g_figure_sprites, (u16)((R.si << 3) + 2)) = 2;
        FP16(g_figure_sprites, (u16)((R.si << 3) + 4)) = 0x13;
        FP32(g_figure_sprites, (u16)((R.si << 3) + 6)) = LOC32(4);
        T += 35;
        framed(0x047E, 2, 1, 0x130); ax = figure_sprite_stride(0x130, 1); framed_pop(2);
        LOC16(4) = (u16)(LOC16(4) + ax);
        R.si++;
        T += 3;
    }
    T += 1;
    framed(0x048F, 0); figures_load_sprites(); framed_pop(0);
    T += 3;
    framed(0x0498, 1, 0xA4); g_population_map = res_load_raw(0xA4, 0xA4); framed_pop(1);
    T += 5;
    framed(0x04A6, 0); disk_access_end(LO16(g_population_map)); framed_pop(0);
    T += 3;
    framed(0x04AF, 1, 0x2B); game_load(0x2B, 0x2B); framed_pop(1);
    fix_film200_record();                /* data fix "film-200": on the market (fixes.c) */
    fix_live_aid_record();               /* data fix "live-aid": 2 hours long (fixes.c) */
    fix_video_conn_record();             /* data fix "video-connection": its roles' requirements (fixes.c) */
    g_mouse_x = 0xA0;
    G16(0x7BF8) = 0x64;
    g_bld_scroll = 0;
    g_elevator_floor = 1;
    g_room = ROOM_BUILDING;
    G16(0x8E7C) = 2;
    g_player_figure = g_player - 1;
    g_font = 0;
    g_cursor_shape = 1;
    T += 14;
    framed(0x04ED, 0); mouse_bounds_full(); framed_pop(0);
    T += 5;
    framed(0x04FA, 2, 1, 0x11E2); g_coverage_map = mem_calloc(0x11E2, 1); framed_pop(2);
    state_activate();                    /* the coverage map too */
    g_time_slot = 0x11;
    T += 7;
    framed(0x0513, 1, 9); g_panel_sprites = res_load0(9); framed_pop(1);
    T += 7;
    framed(0x0525, 1, 0xA); g_panel_atlas = res_load(0xA); framed_pop(1);
    T += 9;
    framed(0x053B, 2, 0x50, 0x140); g_panel_bmp = bitmap_alloc(0x140, 0x50); framed_pop(2);
    T += 4;
    framed(0x054A, 0); panel_build(); framed_pop(0);
    T += 3;
    framed(0x0557, 2, HI16(g_panel_atlas), LO16(g_panel_atlas)); mem_free(LO16(g_panel_atlas), HI16(g_panel_atlas)); framed_pop(2);
    T += 4;
    framed(0x0567, 2, g_panel_sprites_hi, g_panel_sprites_lo); mem_free(g_panel_sprites_lo, g_panel_sprites_hi); framed_pop(2);
    T += 2;
    framed(0x056F, 0); panel_show_wipe(); framed_pop(0);
    T += 3;
    framed(0x057C, 2, g_panel_bmp_hi, g_panel_bmp_lo); mem_free(g_panel_bmp_lo, g_panel_bmp_hi); framed_pop(2);
    T += 2;
    framed(0x0584, 0); ui_draw_player_icons(); framed_pop(0);
    T += 5;
    framed(0x0591, 2, 0x64, 0x78); g_tv_family_gfx = bitmap_alloc(0x78, 0x64); framed_pop(2);
    T += 8;
    framed(0x05A8, 2, 0xB, 0x18); g_cursor_comp = bitmap_alloc(0x18, 0xB); framed_pop(2);
    T += 8;
    framed(0x05BF, 2, 0xB, 0x18); g_cursor_bg = bitmap_alloc(0x18, 0xB); framed_pop(2);
    T += 8;
    framed(0x05D6, 2, 0xB, 0x18); g_cursor_tmp = bitmap_alloc(0x18, 0xB); framed_pop(2);
    T += 8;
    framed(0x05ED, 2, 0xB, 0x30); g_cursor_sprites(5) = bitmap_alloc(0x30, 0xB); framed_pop(2);
    T += 8;
    framed(0x0604, 2, 0xB, 0x30); G32(0x8E86) = bitmap_alloc(0x30, 0xB); framed_pop(2);
    T += 8;
    framed(0x061B, 2, 0x3A, 0x68); g_tvfam_bg = bitmap_alloc(0x68, 0x3A); framed_pop(2);
    T += 8;
    framed(0x0632, 2, 0x78, 0x140); g_screen_buf = bitmap_alloc(0x140, 0x78); framed_pop(2);
    T += 8;
    framed(0x0649, 2, 0x38, 0x88); g_status_win_bmp = bitmap_alloc(0x88, 0x38); framed_pop(2);
    T += 8;
    framed(0x0660, 2, 0x96, 0xD0); g_building_buf = bitmap_alloc(0xD0, 0x96); framed_pop(2);
    T += 10;
    rtl_vga_get_bitmap_w(W2(G32(0x8E86)), 0x13, 0x7F);
    T += 2;
    framed(0x0687, 0); clock_show_start(); framed_pop(0);
    g_cursor_shown = 0;
    T += 2;
    for (;;) {                           /* the main loop: one game frame per pass */
        if (seat_humans) mp_requests_seat0();   /* multiplayer: what another seat asked of seat 0 (caught) */
        T += 2;
        if (g_room == 0x3E8) break;
        T += 1;
        POLL();
        T += 2;
        if (g_game_mode == 0) {          /* title / new-game screen */
            g_clock_halt = 2;
            T += 4;
            framed(0x06A6, 1, ROOM_NEW_GAME); ax = room_enter(ROOM_NEW_GAME); framed_pop(1);
            T += 3;
            framed(0x06AD, 0); cursor_show(ax); framed_pop(0);
            g_game_mode = 1;
            T += 2;
            if (new_game_opt && mp_load_opt) {   /* --mp-load: a saved network game, its morning (src/mpsave.c) */
                new_game_opt = 0;
                mpsave_load_start();
            } else if (new_game_opt) {       /* --new-game: the setup's "Start" at once, its defaults (a network game: every machine alike) */
                new_game_opt = 0;            /* (once: after the game, the setup is the setup) */
                if (difficulty_opt) g_difficulty = (u16)difficulty_opt;
                for (u16 p = 1; p <= 3; p++) {   /* --names: as typed on the setup screen (its hot spots 7..9) */
                    const char *nm = new_game_names[p - 1];
                    if (!nm[0]) continue;
                    for (u16 i = 0; i < 41; i++) FP8(players(p).name, i) = 0;
                    for (u16 i = 0; nm[i]; i++) FP8(players(p).name, i) = (u8)nm[i];
                }
                g_clock_acc = g_clock_acc + (u32)seed_opt;   /* --seed: time spent on the setup screen (the start's randomness) */
                g_hotspot = 0x4A;
                room_newgame_click();
                /* the human's player record, as a click sets it (ui_on_left_click): without one the
                 * host's boss read the image from a null record the next morning -- and fired them */
                g_cur_player_rec = players_at(g_player);
            }
        } else if (T += 2, g_game_mode == 2) {   /* a new game started: the first morning */
            g_clock_halt = 1;
            g_bld_full_redraw = 1;
            T += 3;
            framed(0x06CD, 0); room_return(); framed_pop(0);
            T += 1;
            framed(0x06D2, 0); ui_draw_money(); framed_pop(0);
            T += 1;
            framed(0x06D7, 0); ui_draw_viewers(); framed_pop(0);
            if (mp_loaded) {                 /* a saved game: Betty's meter and the stations' icons as it has them (the setup's Load draws them) */
                ui_draw_betty_meter();
                ui_draw_player_icons();
            }
            G16(0x7A2C) = !mp_loaded;        /* the first morning's building (not a saved game's morning) */
            T += 2;
            if (!(seat_humans && seat_out[0])) {   /* (a saved game's host who was fired: watching, no boss) */
                framed(0x06E2, 0); day_start_to_boss(); framed_pop(0);
            }
            G16(0x7A2C) = 0;
            T += 2;
            seat_clock_world = 1;            /* the start: the world's clock */
            framed(0x06ED, 0); game_clock_stop(0 /* AX unused */); framed_pop(0);
            T += 1;
            framed(0x06F2, 0); game_clock_reset(); framed_pop(0);
            seat_clock_world = 0;
            g_clock_halt = 0;
            T += 2;
            framed(0x06FD, 0); cursor_show(0 /* AX unused */); framed_pop(0);
            g_game_mode = 3;
            T += 1;
        }
        T += 1;
        framed(0x0708, 0); ax = game_frame(); framed_pop(0);
        if (seat_humans && seat_paused_by_other()) { ax = 0; g_click = 0; }   /* another human paused: wait */
        LOC16(8) = ax;
        T += 3;
        if (ax != 0) {                   /* a door was clicked: enter that room */
            T += 1 + 4;
            framed(0x07CE, 2, 0, g_player_figure); ax = door_check_entry(g_player_figure, 0); framed_pop(2);
            g_status_msg = ax;
            T += 4;
            if (ax != 0) {
                figures(g_player_figure).task = 2;
                figures(g_player_figure).target_door = 0xFFFF;
                T += 17;
            } else {
                T += 2;
                framed(0x080E, 1, LOC16(8)); room_enter(LOC16(8)); framed_pop(1);
                T += 2;
            }
            continue;
        }
        T += 2;
        if (g_room == ROOM_BUILDING && (T += 9, figures(g_player_figure).motion == 3) &&
            (T += 9, figures(g_player_figure).target_door == 0xFFFE)) {
            T += 3;
            framed(0x0754, 1, ROOM_INFO_BOARD_GO); room_enter(ROOM_INFO_BOARD_GO); framed_pop(1);
            T += 2;
        }
        T += 2;
        if (g_room == ROOM_INFO_BOARD_GO) {
            T += 4;
            framed(0x0767, 2, R.ss, (u16)(bp_ - 6)); game_clock_get(LOCP(6)); framed_pop(2);
            T += 5;
            if ((i16)(G16(0x7A8C) + 0x384) < (i16)LOC16(6)) {   /* 15 minutes there: leave */
                T += 1;
                for (;;) {
                    T += 2;
                    if ((i16)g_room <= ROOM_BUILDING) break;
                    POLL();
                    T += 1;
                    framed(0x077C, 0); room_escape(); framed_pop(0);
                }
            }
        }
        T += 2;
        if (g_click != 0 && (T += 2, R.di == 0)) {   /* one action per press */
            R.di = 1;
            T += 3;
            if (g_click == 2) {
                T += 2;
                if (g_room != ROOM_BUILDING) {
                    T += 1;
                    framed(0x07A4, 0); room_escape(); framed_pop(0);
                }
                T += 1;
            } else {
                T += 2;
                if (g_click == 1) {
                    T += 1;
                    framed(0x07B2, 0); ui_on_left_click(); framed_pop(0);
                }
            }
        }
        T += 2;
        if (g_click == 0 && (T += 2, R.di != 0)) { R.di = 0; T += 1; }
        T += 1;
    }
    T += 1;
    framed(0x0823, 0); cursor_hide(); framed_pop(0);
    T += 13;
    rtl_bitmap_fill_w(W2(g_screen_buf), 0, 0, 0x28, 0x78, 0);
    T += 8;
    framed(0x0852, 3, 4, 2, 1); ax = screen_transition(1, 2, 4, 1); framed_pop(3);
    R.di = pop16(); R.si = pop16();
    R.sp = R.bp; R.bp = pop16();
    T += 6;
    return ax;
}

/* 0054:085E  keyboard hotkeys: in the office sub-screen select options (0x7A82) with button highlight; key 0x2F toggles the human figure's alien look (0x7A34); key 0x20 opens the menu (05FF:000A) */
void hotkey_handler(u16 a0) {
    int cc;
    u16 si;
    FUNC_TICK(139);
    si = a0;
    if (((u32)MK32(a0, g_room)) == 0x220001 && g_room_mode == 2) {
        g_madris_input = 2;
        draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + 0x8A), 0xBD, 0x59);
        present_frame();
    } else {
        cc = ((u32)MK32(si, g_room)) == 0x230001;
        if (cc && g_room_mode == 2) {
            g_madris_input = 1;
            draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + 0x92), 0xCA, 0x59);
            present_frame();
        } else {
            cc = ((u32)MK32(si, g_room)) == 0x3D0001;
            if (cc && g_room_mode == 2) {
                g_madris_input = 3;
                draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + 0x9A), 0xBD, 0x65);
                present_frame();
            } else {
                cc = ((u32)MK32(si, g_room)) == 0x280001;
                if (cc && g_room_mode == 2) {
                    g_madris_input = 4;
                    draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + 0xA2), 0xCA, 0x65);
                    present_frame();
                } else {
                    cc = ((u32)MK32(si, g_room)) == 1;
                    if (cc && g_room_mode == 2) {
                        g_madris_delay = g_madris_delay - 1;
                        if (((i16)g_madris_delay) < 1) {
                            g_madris_delay = 1;
                        }
                    } else {
                        cc = ((u32)MK32(si, g_room)) == 0x1F0001;
                        if (cc && g_room_mode == 2) {
                            g_madris_input = 5;
                        } else if (si != 0x2F) {
                            if (((u32)MK32(si, g_clock_halt)) == 0x200000) {
                                g_pause_request = 1;
                                ui_on_left_click();
                            }
                        } else if (g_alien_mode != 0) {
                            figures(g_player_figure).speed = 4;
                            g_alien_mode = 0;
                            figures_load_sprites();
                        } else {
                            figures(g_player_figure).speed = 2;
                            g_alien_mode = 1;
                            figures_load_sprites();
                        }
                    }
                }
            }
        }
    }
    return;
}

/* 0054:0A0B  load resource 0xD0 into the start tables (hour factors, studios, players, actors, figures, floors, transmitters ...), load the 430 texts of the language, default player/station names, clear market lists, game speed 30 */
void game_load_init_data(u16 ax) {
    u16 di;
    u16 si;
    u16 l_2;
    u32 t1;
    u16 t2;
    u16 t3;
    u32 t4;
    u32 t11;
    FUNC_TICK(243);
    disk_access_begin(ax);
    t1 = mem_calloc(0x1A90, 1);
    g_initdata = t1;
    g_initdata_pos = 0;
    res_read(0xD0, HPTR(g_initdata), 0xD0);
    initdata_read(0x1D00, DSEG, 8);
    initdata_read(0x6165, DSEG, 0xD0);
    initdata_read(0x8BDC, DSEG, 0x5C);
    initdata_read(0x5013, DSEG, 0x540);
    initdata_read(0x6235, DSEG, 0x180);
    initdata_read(0x268E, DSEG, 0x60);
    initdata_read(0x1D09, DSEG, 0x738);
    initdata_read(0x724A, DSEG, 0x58);
    initdata_read(0x7B42, DSEG, 0xB4);
    initdata_read(0x7C82, DSEG, 0x6AC);
    initdata_read(0x7160, DSEG, 0x80);
    mem_free(g_initdata_lo, g_initdata_hi);
    t2 = G16((g_language << 4) + 0x55E);
    l_2 = G16((g_language << 4) + 0x55E);
    t3 = res_exists(t2, t2);
    t4 = mem_calloc(t3, 1);
    g_text_buf = t4;
    ax = res_read(l_2, HPTR(g_text_buf), LO16(t4));
    si = 0;
    di = 0;
    while (((i16)di) < 430) {
        LOOP_TICK(20);
        g_text(di) = MK32(g_text_buf_hi, g_text_buf_lo + si);
        t11 = rtl_strlen_w(g_text_buf_lo + si, g_text_buf_hi);
        ax = LO16(t11) + 1;
        si = si + (LO16(t11) + 1);
        di = di + 1;
    }
    disk_access_end(ax);
    G32(0x8BF3) = mem_calloc(0x29, 1);
    far_memcpy(HPTR(G32(0x8BF3)), HPTR(g_text(63)), 7);
    G32(0x8BF7) = g_text(71);
    G32(0x8C0A) = mem_calloc(0x29, 1);
    far_memcpy(HPTR(G32(0x8C0A)), HPTR(g_text(67)), 6);
    G32(0x8C0E) = g_text(76);
    G32(0x8C21) = mem_calloc(0x29, 1);
    far_memcpy(HPTR(G32(0x8C21)), HPTR(g_text(69)), 0xA);
    G32(0x8C25) = g_text(145);
    mem_zero(DSP(0x19A0), 0x4D);
    memset(g_ad_offers_extra, 0, (AD_OFFERS_MAX - 10) * 0x7);   /* --ad-offers: the offers past 10 */
    filmoffers_clear(11);                /* --film-offers: the dealer's slots past 10 */
    mem_zero(DSP(0x1B70), 0x118);
    mem_zero(DSP(0x76A8), 8);
    mem_zero(DSP(0x72BA), 0x90);
    mem_zero(DSP(0x63E5), 0x1EA);
    memset(film_txt_more, 0, sizeof film_txt_more);   /* fix "title-cache": the slots past 48 */
    mem_zero(DSP(0x65CF), 0x546);
    mem_zero(DSP(0x6B15), 0x294);
    g_game_speed = 0x1E;
    return;
}

/* 0054:0CDA  copy the next n bytes of the init-data blob (0x8FB0, cursor 0x11E4) to dst */
void initdata_read(u16 a0, u16 a1, u16 a2) {
    FUNC_TICK(16);
    far_memcpy(HPTR(MK32(a1, a0)), HPTR(MK32(g_initdata_hi, g_initdata_lo + g_initdata_pos)), a2);
    g_initdata_pos = g_initdata_pos + a2;
    return;
}

/* fast room changes (runtime.h fast_rooms): a room change's wipe without its delay per step
 * (1 tick: ~55 ms, 20 steps a wipe, two in a change) -- two of a room change's three
 * seconds.  With several humans the world runs in seat 0's frames and stood still for
 * everyone while the host's wipe went on; alone the player waited (the game's clock stands
 * in a room change).  Without the option the original's pace. */
static u16 fast_wipe_delay(u16 d) { return fast_rooms() ? 0 : d; }

/* 0054:0D03  leave the current room: pop the previous room from the room stack, free/load room graphics, wipe; back in the building reload building backgrounds (day/night by hour >= 21:00) */
u16 room_return(void) {
    seat_room_depth[seat_cur > 0 ? seat_cur : 0]++;   /* (seats: seat 0's room change lets the others run, runtime/seat.c) */
    u16 ax;
    u16 s0;
    u16 si;
    u16 t2;
    u16 t3;
    u16 t4;
    u16 t5;
    u32 t9;
    u32 t10;
    u16 t12;
    u32 t13;
    u16 t14;
    u16 t15;
    FUNC_TICK(111);
    game_clock_pause(0 /* unused ax */);
    cursor_hide();
    g_tip_visible = 0;
    t2 = g_room_stack(g_room_stack_top);
    t3 = g_room;
    g_room = g_room_stack(g_room_stack_top);
    g_room_stack_top = g_room_stack_top - 1;
    ax = t2;
    si = t3;
    if (((i16)t3) <= 0) {
        if (((i16)si) < 0) {
            ax = present_frame();
        }
    } else {
        room_anim_update(2, 0 /* unused ax */);
        rtl_snd_room_music_stop(g_room, g_room_owner);
        t4 = room_free_assets();
        t5 = snd_room_music_start(g_room, g_room_owner, t4);
        ax = room_load_assets(g_room, t5);
        if (g_room != ROOM_BUILDING) {
            ax = screen_transition(1, fast_wipe_delay(1), 4, 1);
        }
    }
    if (g_room == ROOM_BUILDING && ((i16)si) > 0) {
        G16(0x7A8A) = 1;
        game_clock_stop(0 /* unused ax */);
        t9 = bitmap_alloc(0x88, 0x38);
        g_status_win_bmp = t9;
        t10 = bitmap_alloc(0xD0, 0x96);
        g_building_buf = t10;
        G32(0xFC) = res_cache_unpack(2, 1);
        t12 = clock_hour();
        s0 = 0;
        if (((i16)t12) >= 4) {
            ax = 1;
        } else {
            ax = 0;
        }
        t13 = res_cache_unpack(ax, s0);
        g_building_bg = t13;
        game_clock_start();
        g_bld_full_redraw = 1;
        g_redraw = 1;
        bld_render_floors(g_bld_scroll);
        bld_update_frame();
        screen_transition(1, fast_wipe_delay(1), 4, 0 /* unused ax */);
        t14 = game_frame();
        G16(0x7A8A) = 0;
        ax = t14;
    }
    t15 = cursor_show(ax);
    seat_room_depth[seat_cur > 0 ? seat_cur : 0]--;
    return game_clock_resume(t15);
}

/* 0054:0E5F  push the current room and enter room n: owner = owner of the human figure's door, load room graphics, wipe transition */
u16 room_enter(u16 a0) {
    seat_room_depth[seat_cur > 0 ? seat_cur : 0]++;   /* (seats: seat 0's room change lets the others run, runtime/seat.c) */
    u16 ax;
    u16 t12;
    FUNC_TICK(94);
    game_clock_pause(0 /* unused ax */);
    cursor_hide();
    g_tip_visible = 0;
    g_room_stack_top = g_room_stack_top + 1;
    g_room_stack(g_room_stack_top) = g_room;
    g_room = a0;
    ax = a0;
    if (((i16)g_room) > ROOM_BUILDING) {
        g_room_owner = floors_door_owner(figures(g_player_figure).floor, figures(g_player_figure).target_door);
        rtl_snd_room_music_stop(g_room, g_room_owner);
        ax = room_free_assets();
        if (g_room != ROOM_FILM_AGENCY) {
            ax = snd_room_music_start(g_room, g_room_owner, ax);
        }
        ax = room_load_assets(g_room, ax);
        if (g_room == ROOM_FILM_AGENCY) {
            ax = snd_room_music_start(g_room, g_room_owner, ax);
        }
        g_redraw = 1;
        if (g_room != ROOM_SHOP) {
            ax = screen_transition(2, fast_wipe_delay(1), 4, 2);
        }
        if (g_room == ROOM_INFO_BOARD_GO) {
            ax = game_clock_get(HPTR(DSP(0x7A8C)));
        }
        g_room_anim_dirty = 0;
    }
    t12 = cursor_show(ax);
    seat_room_depth[seat_cur > 0 ? seat_cur : 0]--;
    return game_clock_resume(t12);
}

/* 0054:0F79  per-room setup on entry: draw background halves, load sprite resources into 0x100..0x124, room-specific init (office, library, studio, planner, transmitter map, save room ...) */
u16 room_load_assets(u16 a0, u16 ax) {
    int cc;
    u32 t2;
    u32 t3;
    u32 t4;
    u32 t5;
    u32 t6;
    u32 t7;
    u32 t8;
    u32 t10;
    u32 t11;
    u32 t12;
    u32 t14;
    u32 t15;
    u32 t16;
    u32 t17;
    u32 t19;
    u32 t20;
    u32 t21;
    u32 t22;
    u32 t23;
    u32 t24;
    u16 t26;
    u32 t28;
    u32 t29;
    u32 t30;
    u32 t31;
    u32 t32;
    u32 t33;
    u32 t35;
    u32 t37;
    u32 t38;
    u32 t39;
    u32 t46;
    u32 t48;
    u32 t49;
    u32 t50;
    u32 t51;
    u32 t52;
    u32 t53;
    u32 t54;
    u32 t56;
    u32 t57;
    u32 t58;
    u32 t59;
    u32 t60;
    u32 t61;
    u32 t62;
    u32 t63;
    u32 t64;
    u32 t65;
    u32 t66;
    u32 t67;
    u32 t68;
    u32 t69;
    u32 t70;
    u32 t71;
    u16 t82;
    u16 t84;
    u16 t85;
    u32 t88;
    u32 t89;
    u32 t91;
    u32 t92;
    u32 t95;
    u32 t96;
    u32 t99;
    u32 t102;
    u32 t103;
    u16 t104;
    u32 t105;
    u32 t106;
    u32 t107;
    u32 t108;
    u16 t110;
    u32 t111;
    u32 t115;
    u32 t116;
    u32 t118;
    u32 t119;
    u32 t121;
    u32 t122;
    u16 t124;
    u16 t125;
    u16 t126;
    u16 t128;
    u16 t129;
    u16 t131;
    FUNC_TICK(1203);
    g_bg_half_bmp = 0;
    disk_access_begin(ax);
    ax = a0 - 1;
    if (((u16)(a0 - 1)) <= 0x40) {
        switch (ax) {
        case 0:
            room_draw_background(0x68);
            t3 = res_load(0xB5);
            g_room_gfx = t3;
            ax = LO16(t3);
            break;
        case 1:
            room_draw_background(0x6E);
            t10 = res_load(0x96);
            g_room_gfx = t10;
            t11 = res_load(0xA2);
            g_room_gfx1 = t11;
            t12 = res_load(0xBA);
            g_room_gfx2 = t12;
            ax = txt_news_cache_load(g_room_owner, LO16(t12));
            break;
        case 2:
            room_draw_background(0x69);
            t14 = res_load(0xB6);
            g_room_gfx = t14;
            ax = LO16(t14);
            break;
        case 3:
            room_draw_background(0x65);
            t15 = res_load(0x9D);
            g_room_gfx1 = t15;
            t16 = res_load(0x9A);
            g_room_gfx2 = t16;
            t17 = res_load(0xA6);
            g_room_gfx3 = t17;
            txt_film_cache_load(4, g_room_owner, 4);
            ax = txt_series_cache_load(4, g_room_owner, 4);
            break;
        case 4:
            g_cur_studio = floors_door_studio(figures(g_player_figure).floor, figures(g_player_figure).target_door);
            clip_reset();
            room_draw_background(((u16)(i8)(studios(g_cur_studio).size)) + 0x70);
            t46 = res_load(0xAA);
            g_room_gfx = t46;
            t48 = res_load0(0xA8);
            g_room_gfx1 = t48;
            ui_draw_studio_items();
            mem_free(g_room_gfx1_lo, g_room_gfx1_hi);
            t49 = res_load(0x70);
            g_room_gfx1 = t49;
            draw_sprite(MK32(g_room_gfx1_hi, g_room_gfx1_lo + 2), 4, 4);
            mem_free(g_room_gfx1_lo, g_room_gfx1_hi);
            t50 = res_load(0x3E);
            g_room_gfx1 = t50;
            draw_sprite(MK32(g_room_gfx1_hi, g_room_gfx1_lo + 2), 0xA0, 4);
            mem_free_ptr(HPTR(DSP(0x104)));
            t51 = res_load(0xC0);
            g_actor_txt = t51;
            t52 = res_load(0x9A);
            g_room_gfx2 = t52;
            t53 = bitmap_alloc(0x10, 0x1D);
            g_room_gfx3 = t53;
            t54 = res_load(0x9D);
            g_room_gfx7 = t54;
            txt_film_cache_load(5, g_cur_studio, 5);
            txt_series_cache_load(5, g_cur_studio, 5);
            txt_script_cache_load(5, g_cur_studio, 5);
            ax = grab_screen(g_room_gfx3, 0xF8, 0x53);
            break;
        case 5:
        case 6:
        case 0xE:
        case 0x19:
        case 0x1A:
        case 0x1B:
        case 0x1C:
        case 0x1D:
        case 0x1F:
        case 0x20:
        case 0x21:
        case 0x22:
        case 0x23:
        case 0x24:
        case 0x25:
        case 0x26:
        case 0x27:
        case 0x28:
        case 0x29:
        case 0x2A:
        case 0x2B:
        case 0x2C:
        case 0x2D:
        case 0x2E:
        case 0x2F:
        case 0x30:
        case 0x31:
        case 0x32:
        case 0x33:
        case 0x34:
        case 0x35:
        case 0x36:
        case 0x37:
        case 0x38:
        case 0x3A:
        case 0x3B:
            break;
        case 7:
            room_draw_background(0x6A);
            t26 = clock_hour();
            ax = t26;
            if (t26 <= 7) {
                switch (ax) {
                case 0:
                case 1:
                    g_dealer_pose = 0xBC;
                    break;
                case 2:
                case 3:
                    g_dealer_pose = 0xBD;
                    break;
                case 4:
                case 5:
                    g_dealer_pose = 0xBE;
                    break;
                case 6:
                case 7:
                    g_dealer_pose = 0xBF;
                    break;
                default:
                    cpu_halt("bad switch index %u in 0054:0F79", ax);
                }
            }
            t28 = res_load(g_dealer_pose);
            g_actor_txt = t28;
            dealer_draw_pose();
            mem_free(g_actor_txt_lo, g_actor_txt_hi);
            t29 = res_load(0xBB);
            g_actor_txt = t29;
            t30 = res_load(0x96);
            g_room_gfx = t30;
            t31 = res_load(0x9B);
            g_room_gfx3 = t31;
            t32 = res_load(0x9D);
            g_room_gfx1 = t32;
            t33 = res_load(0x9A);
            g_room_gfx2 = t33;
            txt_film_cache_load(8, 0, 8);
            ax = txt_series_cache_load(8, 0, 8);
            break;
        case 8:
            room_draw_background(0x67);
            t22 = res_load(0xA5);
            g_room_gfx1 = t22;
            t23 = res_load(0x9A);
            g_room_gfx2 = t23;
            t24 = res_load(0xB9);
            g_room_gfx3 = t24;
            txt_film_cache_load(9, 0, 9);
            txt_series_cache_load(9, 0, 9);
            ax = txt_script_cache_load(9, 0, 9);
            break;
        case 9:
            room_draw_background(0x6C);
            t35 = res_load(0xC1);
            g_room_gfx = t35;
            ax = g_estate_wealth;
            if (g_estate_wealth <= 0xA) {
                switch (ax) {
                case 0:
                case 1:
                case 2:
                    g_estate_portrait = 0xC2;
                    break;
                case 3:
                case 4:
                    g_estate_portrait = 0xC3;
                    break;
                case 5:
                case 6:
                case 7:
                    g_estate_portrait = 0xC4;
                    break;
                case 8:
                case 9:
                case 0xA:
                    g_estate_portrait = 0xC6;
                    break;
                default:
                    cpu_halt("bad switch index %u in 0054:0F79", ax);
                }
            }
            t37 = res_load(g_estate_portrait);
            g_room_gfx1 = t37;
            ax = LO16(t37);
            break;
        case 0xA:
            room_draw_background(0x6F);
            t66 = res_load(0x9C);
            g_room_gfx1 = t66;
            t67 = res_load(0x9A);
            g_room_gfx2 = t67;
            ax = LO16(t67);
            break;
        case 0xB:
            room_draw_background(0x6D);
            t56 = res_load0(0xAB);
            g_room_gfx = t56;
            t57 = res_load(0xAE);
            g_room_gfx1 = t57;
            t58 = res_load(0x9A);
            g_room_gfx2 = t58;
            t59 = res_load0(0x97);
            g_room_gfx3 = t59;
            t60 = res_load(0xC5);
            g_actor_txt = t60;
            ax = LO16(t60);
            break;
        case 0xC:
        case 0x3C:
            t70 = res_load(0xA1);
            g_room_gfx = t70;
            t71 = res_load(0x98);
            g_room_gfx2 = t71;
            ax = LO16(t71);
            break;
        case 0xD:
            room_draw_background(0x66);
            t19 = res_load0(0x97);
            g_room_gfx = t19;
            t20 = res_load(0x9F);
            g_room_gfx1 = t20;
            t21 = res_load(0x9A);
            g_room_gfx2 = t21;
            ax = LO16(t21);
            break;
        case 0xF:
            ax = room_draw_background(0x78);
            break;
        case 0x10:
            ax = room_draw_background(0x77);
            break;
        case 0x11:
            ax = room_draw_background(0x7E);
            break;
        case 0x12:
            ax = room_draw_background(0x7F);
            break;
        case 0x13:
            ax = room_draw_background(0x7B);
            break;
        case 0x14:
            ax = room_draw_background(0x7A);
            break;
        case 0x15:
            ax = room_draw_background(0x75);
            break;
        case 0x16:
            ax = room_draw_background(0x76);
            break;
        case 0x17:
            ax = room_draw_background(0x79);
            break;
        case 0x18:
            t68 = res_load0(0xA3);
            g_room_gfx = t68;
            t69 = res_load(0x99);
            g_room_gfx1 = t69;
            ax = LO16(t69);
            break;
        case 0x1E:
            t5 = res_load(0x96);
            g_room_gfx = t5;
            t6 = res_load(0x9E);
            g_room_gfx1 = t6;
            t7 = res_load(0x98);
            g_room_gfx2 = t7;
            t8 = res_load(0xAF);
            g_room_gfx3 = t8;
            txt_film_cache_load(0x1F, g_room_owner, 0x1F);
            ax = txt_series_cache_load(0x1F, g_room_owner, 0x1F);
            break;
        case 0x39:
            room_draw_background(0x74);
            t4 = res_load(0xB6);
            g_room_gfx1 = t4;
            ax = LO16(t4);
            break;
        case 0x3D:
            t38 = res_load(0xA0);
            g_room_gfx1 = t38;
            t39 = res_load(0x9D);
            g_room_gfx2 = t39;
            ax = LO16(t39);
            break;
        case 0x3E:
            t61 = res_load0(0xA7);
            g_room_gfx = t61;
            t62 = res_load0(0xAC);
            g_room_gfx3 = t62;
            t63 = res_load0(0xA8);
            g_room_gfx1 = t63;
            t64 = res_load(0xA9);
            g_room_gfx2 = t64;
            txt_film_cache_load(0xC, 0, 0xC);
            txt_series_cache_load(0xC, 0, 0xC);
            txt_script_cache_load(0xC, 0, 0xC);
            t65 = res_load_raw(0x2A, 0x2A);
            g_actor_txt = t65;
            ax = LO16(t65);
            break;
        case 0x3F:
            ax = room_draw_background(0x7C);
            break;
        case 0x40:
            t2 = res_load(0x96);
            g_room_gfx = t2;
            ax = LO16(t2);
            break;
        default:
            cpu_halt("bad switch index %u in 0054:0F79", ax);
        }
    }
    disk_access_end(ax);
    t82 = g_room;
    ax = g_room;
    if (g_room != ROOM_SUPERMARKET) {
        if (((i16)t82) > 12) {
            t84 = ax;
            if (ax != 0x3A) {
                if (((i16)t84) > 58) {
                    cc = ((u16)(ax - 0x3D)) <= 4;
                    ax = ax - 0x3D;
                    if (cc) {
                        switch (ax) {
                        case 0:
L_1A0C:
                            ax = ui_draw_info_board();
                            break;
                        case 1:
                            ax = ui_draw_building_plan();
                            break;
                        case 2:
                            castfit_room_enter();   /* --cast-fit: the room's picture, before the panel covers it (cast_fit.c) */
                            ui_draw_actor_card();
                            ui_draw_shopping_lists();
                            t124 = present_frame();
                            g_room_mode = 2;
                            ax = t124;
                            break;
                        case 3:
                            break;
                        case 4:
                            rtl_bitmap_fill_w(W2(g_screen_buf), 0, 0, 0x28, 0x78, 0);
                            room_draw_border();
                            ax = newgame_draw_setup(0);
                            break;
                        default:
                            cpu_halt("bad switch index %u in 0054:0F79", ax);
                        }
                    }
                } else {
                    t85 = ax;
                    if (ax != 0x19) {
                        if (((i16)t85) > 25) {
                            if (ax == 0x1F) {
                                t88 = bitmap_alloc(0x60, 0xA);
                                g_actor_txt = t88;
                                t89 = mem_calloc(series_top_window_buf(0x3A88), 1);   /* --series-topicality: room for the taller series card (fixes.c) */
                                g_window_buf = t89;
                                g_room_mode = 0;
                                g_chart_mode = 0;
                                g_chart_player = g_player;
                                g_chart_image = 0;
                                g_planner_scroll(0) = 0;
                                ax = ui_draw_planner(g_room_owner);
                            }
                        } else if (ax != 0xD) {
                            if (ax == 0xE) {
                                ui_draw_betty_office();
                                if (g_player_present(g_player) != 0) {
                                    ui_draw_bag(0, 7, 0 /* unused ax */);
                                }
                                ax = DSEG;
                                if (((i16)(players(g_player).betty_love)) < 20) {
                                    t110 = master_key(g_player, 0);
                                    ax = t110;
                                    if (t110 != 0) {
                                        t111 = text_dup(0x15E);
                                        betty_say(0, LO16(t111), HI16(t111));
                                        ax = master_key(g_player, 2);
                                    }
                                }
                            }
                        } else {
                            goto L_1A0C;
                        }
                    } else {
                        g_map_player = g_player;
                        coverage_build(g_player);
                        ax = ui_draw_transmitter_map(0);
                    }
                }
            } else {
                t125 = savegame_names_alloc(ax);
                disk_access_begin(t125);
                t126 = res_exists(0xD1, 0xD1);
                ax = t126;
                if (t126 != 0xFFFF) {
                    disk_access_end(ax);
                    savegame_names_load();
                    ax = save_slots_draw();
                    if (g_game_mode != 1) {
                        t131 = save_overlay_box(0);
                        g_room_mode = 2;
                        ax = t131;
                    } else {
                        g_room_mode = 4;
                    }
                } else {
                    disk_access_end(ax);
                    if ((res_disk_mode()) != 1) {
                        t129 = save_not_found_box(0);
                        g_room_mode = 1;
                        ax = t129;
                    } else {
                        t128 = save_insert_disk_box(0);
                        g_room_mode = 5;
                        ax = t128;
                    }
                }
                ax = game_clock_pause(ax);
            }
        } else {
            cc = ((u16)(ax - 1)) <= 0xA;
            ax = ax - 1;
            if (cc) {
                switch (ax) {
                case 0:
                    if (g_room_owner == g_player) {
                        office_draw_paper_pile();
                    }
                    office_draw_letter();
                    ax = g_room_owner;
                    if (g_room_owner != g_player || ((i16)g_office_papers) < 1) break;
                    t91 = bitmap_alloc(0x28, 0x10);
                    g_room_gfx5 = t91;
                    t92 = bitmap_alloc(0x28, 0x10);
                    g_room_gfx6 = t92;
                    ax = grab_screen(g_room_gfx5, 0x78, 0x45);
                    break;
                case 1:
                    t102 = bitmap_alloc(0x18, 0xA);
                    g_room_gfx3 = t102;
                    t103 = bitmap_alloc(0x18, 0xA);
                    g_actor_txt = t103;
                    grab_screen(g_room_gfx3, 0xE8, 0x31);
                    t104 = ui_draw_news_agencies(g_room_owner);
                    g_news_placed = 0;
                    ax = t104;
                    break;
                case 2:
                    g_boss_menu = 0;
                    g_room_mode = G16(0x7A32);
                    if (g_room_mode != 1) {
                        ax = boss_dialog_draw(0, 0);
                        break;
                    } else {
                        ax = room_boss_click();
                        break;
                    }
                    break;
                case 3:
                    g_selected_genre = 0;
                    g_room_mode = 0;
                    sched_mark_scheduled();
                    if (g_room_owner == g_player) {
                        ui_draw_bag(0, 1, 0 /* unused ax */);
                    }
                    t99 = bitmap_alloc(0x38, 0x2D);
                    g_room_gfx5 = t99;
                    ax = grab_screen(g_room_gfx5, 0xE8, 0xC);
                    break;
                case 4:
                    t118 = bitmap_alloc(0x50, 0x53);
                    g_room_gfx5 = t118;
                    t119 = bitmap_alloc(0x50, 0x53);
                    g_room_gfx6 = t119;
                    grab_screen(g_room_gfx5, 0, 0x1E);
                    ui_draw_studio();
                    ax = g_room_owner;
                    if (g_room_owner != g_player) break;
                    ax = ui_draw_bag(0, 5, 0);
                    break;
                case 5:
                case 6:
                    break;
                case 7:
                    t95 = bitmap_alloc(0x48, 0x35);
                    g_room_gfx5 = t95;
                    t96 = bitmap_alloc(0x48, 0x35);
                    g_room_gfx6 = t96;
                    grab_screen(g_room_gfx5, 0x10, 0x12);
                    ui_draw_dealer_shelf(0);
                    ax = ui_draw_bag(0, 2, 0);
                    break;
                case 8:
                    t105 = bitmap_alloc(0x30, 0x28);
                    g_room_gfx5 = t105;
                    t106 = bitmap_alloc(0x30, 0x28);
                    g_room_gfx6 = t106;
                    t107 = bitmap_alloc(0x18, 0xC);
                    g_room_gfx7 = t107;
                    t108 = bitmap_alloc(0x20, 0x14);
                    g_room_gfx8 = t108;
                    grab_screen(g_room_gfx5, 0xD0, 0x20);
                    draw_sprite(MK32(g_room_gfx3_hi, g_room_gfx3_lo + 0x12), 0xE8, 0x3C);
                    draw_sprite(MK32(g_room_gfx3_hi, g_room_gfx3_lo + 0x1A), 0xE8, 0x32);
                    scriptagency_draw_scripts();
                    ui_draw_script_offers();
                    ax = ui_draw_bag(0, 4, 0);
                    break;
                case 9:
                    estate_draw_wealth();
                    t115 = bitmap_alloc(0x20, 0xA);
                    g_room_gfx2 = t115;
                    grab_screen(g_room_gfx2, 0xF8, 0x28);
                    t116 = bitmap_alloc(0x20, 0xA);
                    g_room_gfx3 = t116;
                    ax = LO16(t116);
                    break;
                case 0xA:
                    ui_draw_ad_offers();
                    ax = ui_draw_bag(0, 3, 0);
                    break;
                default:
                    cpu_halt("bad switch index %u in 0054:0F79", ax);
                }
            }
        }
    } else {
        t121 = bitmap_alloc(0x30, 0x5E);
        g_room_gfx5 = t121;
        t122 = bitmap_alloc(0x30, 0x5E);
        g_room_gfx6 = t122;
        grab_screen(g_room_gfx5, 0, 0x14);
        draw_sprite(MK32(g_actor_txt_hi, g_actor_txt_lo + 2), 0xC, 0x4A);
        ax = draw_sprite(MK32(g_actor_txt_hi, g_actor_txt_lo + 0xA), 6, 0x15);
    }
    return ax;
}

/* 0054:1DB7  free all room graphic buffers (0xF4..0x124, 0x8E8A, 0x8EDA) */
u16 room_free_assets(void) {
    u16 ax;
    FUNC_TICK(113);
    if ((g_building_buf_lo | g_building_buf_hi) != 0) {
        mem_free_ptr(HPTR(DSP(0x8EDA)));
    }
    if ((g_status_win_bmp_lo | g_status_win_bmp_hi) != 0) {
        mem_free_ptr(HPTR(DSP(0x8E8A)));
    }
    if ((G16(0xFC) | G16(0xFE)) != 0) {
        mem_free_ptr(HPTR(DSP(0xFC)));
    }
    if ((g_building_bg_lo | g_building_bg_hi) != 0) {
        mem_free_ptr(HPTR(DSP(0xF4)));
    }
    if ((g_room_gfx_lo | g_room_gfx_hi) != 0) {
        mem_free_ptr(HPTR(DSP(0x100)));
    }
    if ((g_room_gfx1_lo | g_room_gfx1_hi) != 0) {
        mem_free_ptr(HPTR(DSP(0x104)));
    }
    if ((g_room_gfx2_lo | g_room_gfx2_hi) != 0) {
        mem_free_ptr(HPTR(DSP(0x108)));
    }
    if ((g_room_gfx3_lo | g_room_gfx3_hi) != 0) {
        mem_free_ptr(HPTR(DSP(0x10C)));
    }
    if ((g_actor_txt_lo | g_actor_txt_hi) != 0) {
        mem_free_ptr(HPTR(DSP(0x110)));
    }
    if ((g_room_gfx5_lo | g_room_gfx5_hi) != 0) {
        mem_free_ptr(HPTR(DSP(0x114)));
    }
    if ((g_room_gfx6_lo | g_room_gfx6_hi) != 0) {
        mem_free_ptr(HPTR(DSP(0x118)));
    }
    if ((g_room_gfx7_lo | g_room_gfx7_hi) != 0) {
        mem_free_ptr(HPTR(DSP(0x11C)));
    }
    if ((g_room_gfx8_lo | g_room_gfx8_hi) != 0) {
        mem_free_ptr(HPTR(DSP(0x120)));
    }
    ax = g_window_buf_lo | g_window_buf_hi;
    if ((g_window_buf_lo | g_window_buf_hi) != 0) {
        ax = mem_free_ptr(HPTR(DSP(0x124)));
    }
    return ax;
}

/* 0054:1EEC  draw a room background: resource n at x=0 and n-50 at x=160 */
u16 room_draw_background(u16 a0) {
    u32 t1;
    u32 t2;
    FUNC_TICK(50);
    clip_reset();
    t1 = res_load0(a0 + 0xFFCE);
    g_bg_half_bmp = t1;
    bitmap_put_screen(MK32(g_bg_half_bmp_hi, g_bg_half_bmp_lo + 2), 0xA0, 4);
    mem_free(g_bg_half_bmp_lo, g_bg_half_bmp_hi);
    t2 = res_load0(a0);
    g_bg_half_bmp = t2;
    bitmap_put_screen(MK32(g_bg_half_bmp_hi, g_bg_half_bmp_lo + 2), 0, 4);
    return mem_free(g_bg_half_bmp_lo, g_bg_half_bmp_hi);
}

/* 0054:1F6F  daily processing: culture check, auction, Sammy, agency/image checks, ad deadlines, market cleanup, missed lives, credit interest, day++, offers, actors, intruder roll, topicality recovery, daily costs, reset figures */
void new_day(void) {
    u16 si;
    u32 l_4;
    FUNC_TICK(59);
    betty_culture_check();
    auction_daily();
    g_sammy_day = g_sammy_day + 1;
    g_sammy_winner = 0;
    sammy_daily_update();
    if (((i16)g_day) > 0) {
        news_agency_image_check();
        ad_check_deadlines();
        market_day_cleanup();
        live_check_missed();
        pay_credit_interest();
        mp_for_other_humans(pay_credit_interest);   /* multiplayer: every human's credit (src/seatctx.c) */
        rating_stats_new_day();
        daily_boss_update();
        sched_new_day();
    }
    money_ledger_new_day();
    mp_for_other_humans(money_ledger_new_day);      /* multiplayer: every human's ledger */
    g_day = g_day + 1;
    dealer_refill();
    adagency_refill();
    scriptagency_refill();
    actors_daily_offer();
    npc_event_daily();
    news_age(0, 0);
    news_tick(5, 3);
    programs_recover_topicality();
    if (((i16)g_day) > 1) {
        pay_daily_costs();
    }
    build_door_index();
    ai_new_day();
    trend_pick_new();
    G16(0x7A32) = 1;
    g_office_papers = g_office_papers + 1;
    mp_for_other_humans(mp_office_new_day);         /* multiplayer: every human's office */
    si = 0;
    while (((i16)si) < 3) {
        LOOP_TICK(44);
        if ((players(si + 1).out) == 0) {
            l_4 = figures_at(si);
            AT(figure, l_4).x = 0x48;
            AT(figure, l_4).floor = 1;
            AT(figure, l_4).task = 1;
            AT(figure, l_4).target_floor = (si * 3) + 5;
            AT(figure, l_4).target_door = 2;
            AT(figure, l_4).target_x = 0x99;
            AT(figure, l_4).motion = 1;
            AT(figure, l_4).lift_phase = 0;
            AT(figure, l_4).anim = 1;
        }
        si = si + 1;
    }
    figures_load_sprites();
    G32(0x128) = 0;
    g_elevator_floor = 1;
    g_elevator_calls = 0;
    return;
}

/* 0054:20E0  advance the game clock; at 01:00 (28800 s after 17:00) end the day (go home, new_day, studios, wipe); hourly broadcast processing; redraw clock and refresh room views at :00/:05/:55 */
u16 game_time_tick(u16 a0, u16 ax) {
    u16 si;
    u32 rv_;
    u32 t3;
    u16 t4;
    u8 fr_[8] = {0};                  /* bytes bp-8 .. bp-1 of the original frame */
    int clock_was = seat_clock_world;     /* the world's clock (runtime/seat.h) */
    seat_clock_world = 1;
    FUNC_TICK(253);
    si = a0;
    if (g_room == 0xFFFB || g_clock_paused != 0) {
        if (si != 0) {
L_20FF:
            game_clock_update((fr_ + 8 - 0x6));
            if (((i16)FR16(fr_, 8, 0x6)) >= 28800) {
                seat_request(SEAT_REQ_DAY_END);          /* seats: the others too (src/seatctx.c) */
                g_status_msg = 0;
                tv_show_picture(0xDC);
                g_clock_halt = 1;
                ui_status_text(G16(0x8764), G16(0x8766), 0xB4);
                ui_draw_viewers();
                cursor_hide();
                if (g_room != ROOM_BUILDING) {
                    for (;;) {
                        LOOP_TICK(8);
                        room_escape();
                        if (g_room == ROOM_BUILDING) {
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
                }
                day_end_go_home();
                g_time_slot = 0x11;
                new_day();
                studio_check_finished();
                mpsave_night();              /* multiplayer: every human home, the new day made -- the save (src/mpsave.c) */
                seat_request(SEAT_REQ_DAY_START);
                bld_update_frame();
                screen_transition(2, 2, 4, 0 /* unused ax */);
                clock_show_start();
                ui_draw_viewers();
                ui_draw_money();
                if (!(seat_humans && seat_out[0]))   /* (multiplayer: an out host watches, it has no boss) */
                    day_start_to_boss();
                game_clock_stop(0 /* unused ax */);
                game_clock_reset();
                game_clock_get((fr_ + 8 - 0x6));
                g_clock_halt = 0;
                if (((u32)MK32(g_cursor_shown, g_cursor_shape)) != 1) {
                    g_cursor_shown = 0;
                    g_cursor_shape = 1;
                }
                cursor_show(0 /* unused ax */);
                si = 1;
            }
            if ((time_hour_tick()) != 0) {
                game_clock_pause(0 /* unused ax */);
                if (!(((i16)g_minute) < 55 || ((i16)(clock_hour())) < 1 || g_opt_tv_family != 0)) {
                    tv_family_stop();
                }
                g_redraw = 1;
                g_now_title_stale = 1;
                if (g_tv_off == 0) {
                    tvinfo_fetch_titles();
                }
                tv_update_picture();
                if (!(((i16)g_minute) < 5 || ((i16)g_minute) >= 55 || g_opt_tv_family != 0)) {
                    tv_family_update(0);
                }
                ui_draw_money();
                game_clock_resume(0 /* unused ax */);
            }
            if (g_minute != G16(0x59C) || si == 1) {
                t3 = str_format(DSP(0x5B7) /* "#02d:#02d" */, W2(G32(0x71EA)));
                FR16(fr_, 8, 0x2) = HI16(t3);
                FR16(fr_, 8, 0x4) = LO16(t3);
                ui_status_text(FR16(fr_, 8, 0x4), FR16(fr_, 8, 0x2), 0xB4);
                mem_free(FR16(fr_, 8, 0x4), FR16(fr_, 8, 0x2));
            }
            G16(0x59C) = g_minute;
            ax = g_minute;
            room_minute_update();
        }
    } else {
        goto L_20FF;
    }
    seat_clock_world = clock_was;
    rv_ = ax;
    return (u16)rv_;
}

/* the rooms' updates on a new minute -- the end of game_time_tick (0054:20E6),
 * no time of its own: the planner at :00, :05, :55 (and its chart, the
 * contract popup), the ad agency's bag from :55, the news room at :00.  A
 * seat's: every seat runs it for its own room (seat_status_tick); DS:059E,
 * the minute it last ran, is the seat's. */
void room_minute_update(void) {
    if (g_minute != G16(0x59E)) {
        if (g_room == ROOM_PLANNER) {
            if ((g_room_mode == 0 || g_room_mode == 3 || g_room_mode == 4 || g_room_mode == 6) && (g_minute == 0 || g_minute == 5 || g_minute == 0x37)) {
                planner_refresh();
            }
            if (((u32)MK32(g_room_mode, g_minute)) == 0x70005 && g_chart_mode == 1) {
                cursor_shape_push(3);
                txt_film_cache_free(0);
                txt_series_cache_free(0);
                txt_film_cache_load(0x64, g_chart_player, 0x64);
                txt_series_cache_load(0x64, g_chart_player, 0x64);
                cursor_shape_push(0);
                ui_draw_stats_chart();
                g_redraw = 1;
            } else if (((u32)MK32(g_room_mode, g_minute)) == 0x70000 && g_chart_mode == 1) {
                ui_draw_stats_chart();
                g_redraw = 1;
            }
            if (!(g_room_mode != 5 || ((i16)g_minute) < 55)) {
                ui_contract_popup(0x16, 1, 2, g_room_owner);
            }
        }
        if (!(g_room != ROOM_AD_AGENCY || ((i16)g_minute) < 55)) {
            ui_draw_bag(2, 3, 0 /* unused ax */);
            g_redraw = 1;
        }
        if (((u32)MK32(g_room, g_minute)) == 0x20000) {
            if ((g_room_mode == 1 || g_room_mode == 2) && g_room_owner == g_player) {
                ui_draw_news_inbox(2);
                present_frame();
            } else if (g_room_mode == 0) {
                ui_draw_news_agencies(g_room_owner);
                present_frame();
            }
        }
        G16(0x59E) = g_minute;
    }
}

/* 0054:2425  empty function */
void nop_stub(void) {
    FUNC_TICK(4);
    return;
}

/* 0054:242A  find the selected program 0x79D6 in the room owner's archive: genre -> 0x79CA, position -> 0x79C8 (0 if none) */
void archive_locate_selected(void) {
    u16 di;
    u16 si;
    FUNC_TICK(21);
    if (((i16)g_selected_program) > 0 && ((i16)g_selected_program) < 2000) {
        if (((i16)g_selected_program) > 1000) {
            si = 0;
            while (((i16)si) < 4) {
                LOOP_TICK(20);
                if (library_items(g_room_owner, 0xB, si) == ((u16)(g_selected_program + 0xFC18))) {
                    G32(0x79C8) = MK32(0xB, si);
                }
                si = si + 1;
            }
        } else {
            di = 1;
            while (((i16)di) <= 10) {
                LOOP_TICK(28);
                si = 0;
                while (((i16)si) < 4) {
                    LOOP_TICK(23);
                    if (library_items(g_room_owner, di, si) == g_selected_program) {
                        G32(0x79C8) = MK32(di, si);
                    }
                    si = si + 1;
                }
                di = di + 1;
            }
        }
    } else {
        G32(0x79C8) = 0;
    }
    return;
}

/* 0054:24E0  clear a schedule slot of the room owner and redraw the planner */
void sched_clear_slot_redraw(u16 a0) {
    FUNC_TICK(13);
    sched_clear_slot(g_room_owner, a0);
    ui_draw_planner_rows(g_room_owner);
    present_frame();
    return;
}

/* 0054:2505  clear schedule slot of a player and the following continuation slots (9999) */
void sched_clear_slot(u16 a0, u16 a1) {
    u16 di;
    u16 si;
    FUNC_TICK(23);
    schedule_program(a0, a1) = 0;
    si = a1 + 1;
    di = a0;
    while (schedule_program(di, si) == 0x270F) {
        LOOP_TICK(24);
        schedule_program(di, si) = 0;
        si = si + 1;
    }
    return;
}

/* 0054:256E  1 if the schedule cell under the mouse may still be changed: programs until minute 5 of their start hour, ads until minute 55 */
u16 sched_slot_editable(void) {
    u16 ax;
    int cc;
    u16 di;
    u16 si;
    FUNC_TICK(56);
    di = (u16)((u16)((i32)(i16)(cursor_y() + 0xFFFC) / 16)) + 1;
    if (g_planner_scroll(0) != 7) {
        if (((i16)(cursor_x())) <= 216 || ((i16)(cursor_x())) > 279) {
            if (((i16)(cursor_x())) < 56 || ((i16)(cursor_x())) > 215) {
                ax = 1;
            } else {
                si = di;
                while (schedule_program(g_room_owner, si) == 0x270F) {
                    LOOP_TICK(13);
                    si = si - 1;
                }
                cc = ((i16)clock_hour()) < ((i16)si);
                if (cc || !((clock_hour()) != si || ((i16)g_minute) >= 5)) {
                    ax = 1;
                } else {
                    ax = 0;
                }
            }
        } else {
            cc = ((i16)clock_hour()) < ((i16)di);
            if (cc || !((clock_hour()) != di || ((i16)g_minute) >= 55)) {
                ax = 1;
            } else {
                ax = 0;
            }
        }
    } else {
        ax = 1;
    }
    return ax;
}

/* 0054:2632  build 0x8A60: for floors 1..13 the doors with kind 1..3, as (floor, door) pairs ordered by door x */
void build_door_index(void) {
    u16 ax;
    u16 si;
    u16 l_2;
    u16 l_4;
    u16 t1;
    FUNC_TICK(24);
    mem_zero(DSP(0x8A60), 0x68);
    l_4 = 1;
    while (((i16)l_4) <= 13) {
        LOOP_TICK(86);
        l_2 = 0;
        while (((i16)l_2) < 4) {
            LOOP_TICK(81);
            if (!(floors_door_kind(l_4, l_2) >= 1 && floors_door_kind(l_4, l_2) <= 3)) break;
            t1 = floors_door_x(l_4, l_2);
            ax = floors_door_x(l_4, l_2);
            if (floors_door_x(l_4, l_2) == 0xB4) {
                si = 2;
            } else if (((i16)t1) > 180) {
                if (ax == 0xD8) {
                    si = 3;
                }
            } else if (ax == 0x50) {
                si = 0;
            } else if (ax == 0x74) {
                si = 1;
            }
            G8((((l_4 - 1) << 3) + 0x8A60) + (si << 1)) = (u8)((((u8)((u8)(ax >> 8))) << 8) | ((u8)((u8)l_4)));
            G8(((((l_4 - 1) << 3) + 0x8A60) + (si << 1)) + 1) = (u8)((((u8)((u8)(((u16)((((u8)((u8)(ax >> 8))) << 8) | ((u8)((u8)l_4)))) >> 8))) << 8) | ((u8)((u8)l_2)));
            l_2 = l_2 + 1;
        }
        l_4 = l_4 + 1;
    }
    return;
}

/* 0054:2741  back out one level: close the room's sub-screen (0x8BA4 -> 0) or leave the room (room_return) */
u16 room_escape(void) {
    u16 ax;
    int cc;
    u16 di;
    u16 si;
    u32 l_4;
    u16 t4;
    u16 t5;
    u16 t6;
    u16 t12;
    u16 t17;
    u16 t19;
    u16 t23;
    u16 t26;
    u16 t33;
    u16 t44;
    u16 t49;
    u16 t50;
    u16 t51;
    u16 t52;
    u16 t53;
    u16 t54;
    u16 t57;
    u16 t58;
    u16 t59;
    u16 t60;
    u16 t65;
    u16 t66;
    u16 t67;
    u16 t68;
    u16 t69;
    u16 t72;
    u16 t73;
    u16 t74;
    u16 t75;
    u16 t76;
    u16 t96;
    u16 t97;
    u16 t98;
    u16 t99;
    u16 t100;
    u16 t101;
    u16 t102;
    u16 t107;
    u16 t108;
    FUNC_TICK(922);
    l_4 = figures_at(g_player_figure);
    ax = DSEG;
    if (!(g_room == ROOM_BUILDING || g_room == ROOM_NEW_GAME)) {
        if (g_room != 0xFFFB) {
            if (g_room == ROOM_SAVE_ROOM) {
                if (g_room_mode != 1) {
                    if (g_room_mode != 2) {
                        if (g_room_mode != 5) {
                            if (((u32)MK32(g_game_mode, g_room_mode)) == 0x10004) {
                                g_room_mode = 0;
                            }
                        } else {
                            t6 = save_insert_disk_box(1);
                            g_room_mode = 0;
                            ax = t6;
                        }
                    } else {
                        t5 = save_overlay_box(1);
                        g_room_mode = 0;
                        ax = t5;
                    }
                } else {
                    t4 = save_not_found_box(1);
                    g_room_mode = 0;
                    ax = t4;
                }
            }
            if (g_room_mode == 0 || ((i16)g_room) < ROOM_BUILDING) {
                if (((i16)g_room) >= ROOM_BUILDING) {
                    ax = room_anim_update(2, 2);
                }
                if (g_room == 0xFFFC) {
                    ui_message_box(1, 0, 0);
                    ax = room_return();
                }
                if (g_room != 0xFFFD) {
                    if (g_room != 0xFFFA) {
                        if (g_room != ROOM_PLANNER) {
                            if (g_room != ROOM_INFO_BOARD_GO) {
                                if (g_room != ROOM_FILM_AGENCY) {
                                    if (g_room != ROOM_TX_MAP) {
                                        if (g_room != ROOM_AD_AGENCY) {
                                            if (g_room != ROOM_SCRIPT_AGENCY) {
                                                if (g_room != ROOM_STUDIO) {
                                                    if (g_room != ROOM_LIBRARY) {
                                                        if (g_room != ROOM_SUPERMARKET) {
                                                            if (g_room != ROOM_SHOP) {
                                                                if (g_room != ROOM_BETTY || g_player_present(g_player) == 0) {
                                                                    if (g_room == ROOM_NEWS) {
                                                                        txt_news_cache_free(0 /* unused ax */);
                                                                        news_rate_player(g_room_owner);
                                                                        ax = g_room_owner;
                                                                        if (!(g_room_owner != g_player || g_news_placed == 0)) {
                                                                            ui_box(0, 2, 2, 0x10, 7, 1, 0 /* unused ax */);
                                                                            text_draw_full(0x14, 0x14, g_text(247), 0xD, 1, g_screen_buf_lo, g_screen_buf_hi, 0xFFFF);
                                                                            present_frame();
                                                                            t26 = room_enter(0xFFFD);
                                                                            G16(0x57E) = 1;
                                                                            ax = t26;
                                                                        }
                                                                    } else if (g_room != ROOM_SAVE_ROOM) {
                                                                        if (g_room == 0xFFFF) {
                                                                            ax = ui_message_box(1, 0, 0);
                                                                        }
                                                                    } else {
                                                                        di = 0;
                                                                        while (((i16)di) < 10) {
                                                                            LOOP_TICK(10);
                                                                            ax = mem_free(g_save_names_lo(di), g_save_names_hi(di));
                                                                            di = di + 1;
                                                                        }
                                                                        ax = game_clock_resume(ax);
                                                                    }
                                                                } else {
                                                                    ax = ui_draw_bag(1, 7, 1);
                                                                }
                                                            } else {
                                                                txt_film_cache_free(0);
                                                                t23 = txt_series_cache_free(0);
                                                                ax = txt_script_cache_free_all(t23);
                                                            }
                                                        } else {
                                                            ax = shoplists_book_costs(g_player);
                                                        }
                                                    } else {
                                                        if (g_room_owner == g_player) {
                                                            ui_draw_bag(1, 1, 0 /* unused ax */);
                                                        }
                                                        txt_film_cache_free(0);
                                                        ax = txt_series_cache_free(0);
                                                    }
                                                } else {
                                                    if (g_room_owner == g_player) {
                                                        ui_draw_bag(1, 5, 0 /* unused ax */);
                                                    }
                                                    txt_film_cache_free(0);
                                                    t19 = txt_series_cache_free(0);
                                                    ax = txt_script_cache_free_all(t19);
                                                }
                                            } else {
                                                script_agency_finalize();
                                                txt_film_cache_free(0);
                                                t17 = txt_series_cache_free(0);
                                                ax = txt_script_cache_free_all(t17);
                                            }
                                        } else {
                                            ax = ad_agency_sign_contracts();
                                        }
                                    } else {
                                        ax = tx_commit_day(g_player);
                                    }
                                } else {
                                    ui_draw_bag(1, 2, 0 /* unused ax */);
                                    auction_commit_bids();
                                    dealer_refill();
                                    txt_film_cache_free(0);
                                    ax = txt_series_cache_free(0);
                                    if (((i16)g_bag_count) > 0) {
                                        ax = bag_unload_to_library();
                                    }
                                }
                            } else {
                                AT(figure, l_4).target_door = 0xFFFF;
                                t12 = AT(figure, l_4).floor;
                                AT(figure, l_4).target_floor = AT(figure, l_4).floor;
                                ax = t12;
                            }
                        } else {
                            clip_reset();
                            rtl_bitmap_fill_w(W2(g_screen_buf), 0, 0, 0x28, 0x78, 0);
                            room_draw_border();
                            txt_film_cache_free(0);
                            ax = txt_series_cache_free(0);
                        }
                    } else {
                        ax = betty_say(1, 0, 0);
                    }
                } else {
                    ui_message_box(1, 0, 0);
                    ax = room_return();
                }
                if (G16(0x57E) != 0) {
                    G16(0x57E) = 0;
                } else {
                    ax = room_return();
                    if (g_room == ROOM_BUILDING) {
                        AT(figure, l_4).target_door = 0xFFFF;
                        t33 = AT(figure, l_4).target_x;
                        AT(figure, l_4).final_x = AT(figure, l_4).target_x;
                        AT(figure, l_4).anim = 0;
                        ax = t33;
                    }
                }
                if (g_game_mode != 1) {
                    ax = ui_draw_money();
                }
            } else if (g_room != ROOM_SAVE_ROOM) {
                if (g_room == ROOM_INFO_BOARD) {
                    if (g_room_mode == 1) {
                        di = 0;
                        while (((i16)di) <= 12) {
                            LOOP_TICK(48);
                            si = 0;
                            while (((i16)si) <= 4) {
                                LOOP_TICK(43);
                                cc = ((i8)(G8((((di << 3) + 0x8A60) + (si << 1)) + 1))) < 100;
                                ax = si << 1;
                                if (!(cc)) {
                                    t44 = (((u8)((u8)(ax >> 8))) << 8) | G8(0x79CA);
                                    G8((((di << 3) + 0x8A60) + (si << 1)) + 1) = (u8)((((u8)((u8)(ax >> 8))) << 8) | G8(0x79CA));
                                    G8(((di << 3) + 0x8A60) + (si << 1)) = (u8)((((u8)((u8)(t44 >> 8))) << 8) | G8(0x79CC));
                                }
                                si = si + 1;
                            }
                            di = di + 1;
                        }
                        ui_drag_end();
                        ui_draw_info_board();
                        t49 = present_frame();
                        g_room_mode = 0;
                        ax = t49;
                    }
                } else if (g_room != ROOM_OFFICE) {
                    if (g_room == ROOM_PLANNER) {
                        if (g_room_mode != 1) {
                            if (g_room_mode != 2) {
                                if (g_room_mode != 3) {
                                    if (g_room_mode != 4) {
                                        if (g_room_mode != 5) {
                                            if (g_room_mode != 6) {
                                                if (g_room_mode == 7 || g_room_mode == 9 || g_room_mode == 0xA) {
                                                    if (g_room_mode == 7) {
                                                        cursor_shape_push(3);
                                                        txt_film_cache_free(0);
                                                        txt_series_cache_free(0);
                                                        txt_film_cache_load(0x1F, g_room_owner, 0x1F);
                                                        txt_series_cache_load(0x1F, g_room_owner, 0x1F);
                                                        cursor_shape_push(0);
                                                    }
                                                    ui_draw_planner(g_room_owner);
                                                    t59 = present_frame();
                                                    g_room_mode = 0;
                                                    ax = t59;
                                                } else if (g_room_mode == 8) {
                                                    ui_draw_planner(g_room_owner);
                                                    t60 = present_frame();
                                                    g_room_mode = 0;
                                                    ax = t60;
                                                }
                                            } else {
                                                t58 = ui_drag_end();
                                                g_room_mode = 0;
                                                ax = t58;
                                            }
                                        } else {
                                            ui_contract_popup(0, 0, 1, 0);
                                            t57 = ui_draw_planner_rows(g_room_owner);
                                            g_room_mode = 0;
                                            ax = t57;
                                        }
                                    } else {
                                        clip_reset();
                                        draw_sprite(MK32(g_room_gfx_hi, g_room_gfx_lo + 0x102), 0xC, 0x5D);
                                        g_room_mode = 0;
                                        cursor_hide();
                                        g_cursor_shape = 1;
                                        cursor_show(0 /* unused ax */);
                                        ax = present_frame();
                                    }
                                } else {
                                    t54 = ui_drag_end();
                                    g_room_mode = 0;
                                    ax = t54;
                                }
                            } else {
                                t53 = ui_library_genre_popup(0, 0, 1, 0);
                                g_room_mode = 1;
                                ax = t53;
                            }
                        } else {
                            ui_genre_count_popup(0, 0, 1, 0 /* unused ax */);
                            ui_draw_planner_rows(g_room_owner);
                            t52 = present_frame();
                            g_room_mode = 0;
                            ax = t52;
                        }
                    } else if (g_room == ROOM_BOSS) {
                        if (g_room_mode != 1) {
                            if (g_room_mode == 2) {
                                if (((i16)g_boss_menu) <= 1) {
                                    if (g_boss_menu != 0xFFFF) {
                                        boss_dialog_draw(1, 0);
                                        g_room_mode = 0;
                                        ax = room_return();
                                        if (g_room == ROOM_BUILDING) {
                                            AT(figure, l_4).target_door = 0xFFFF;
                                            t65 = AT(figure, l_4).target_x;
                                            AT(figure, l_4).final_x = AT(figure, l_4).target_x;
                                            AT(figure, l_4).anim = 0;
                                            ax = t65;
                                        }
                                    } else {
                                        ax = room_boss_click();
                                    }
                                } else {
                                    G32(0x7A8E) = 0x10001;
                                    boss_dialog_draw(2, 0xFFFF);
                                    ax = present_frame();
                                }
                            }
                        } else {
                            g_boss_menu = 0;
                            ax = room_boss_click();
                        }
                    } else if (g_room == ROOM_LIBRARY) {
                        if (g_room_mode != 1) {
                            if (g_room_mode != 2) {
                                if (g_room_mode != 3) {
                                    if (g_room_mode == 4) {
                                        ax = library_film_to_bag();
                                    }
                                } else {
                                    ax = library_film_to_shelf();
                                }
                            } else if (g_cursor_shape != 2) {
                                t69 = ui_series_info(1);
                                g_selected_series = 0;
                                g_room_mode = 0;
                                ax = t69;
                            } else {
                                t67 = cursor_hide();
                                g_cursor_shape = 1;
                                t68 = cursor_show(t67);
                                g_room_mode = 0;
                                ax = t68;
                            }
                        } else {
                            t66 = ui_genre_count_popup(0, 0, 1, 0);
                            g_room_mode = 0;
                            ax = t66;
                        }
                    } else if (g_room != ROOM_NEWS) {
                        if (g_room != ROOM_PORTER) {
                            if (g_room == ROOM_TX_MAP) {
                                if (g_room_mode != 1) {
                                    if (g_room_mode != 2) {
                                        if (g_room_mode != 3) {
                                            if (g_room_mode == 4) {
                                                G32((u16)(((g_player * 0x1CE) + 0x1D09) + (g_selected_index * 0xE)) + 4) = 0;
                                                tx(g_player, g_selected_index).state = 2;
                                                ax = txmap_after_sale();
                                            }
                                        } else {
                                            g_room_mode = 0;
                                            ui_draw_transmitter_map(0);
                                            ax = present_frame();
                                        }
                                    } else {
                                        t76 = ui_transmitter_info(g_map_player, 1, 1);
                                        g_room_mode = 0;
                                        ax = t76;
                                    }
                                } else {
                                    ui_drag_end();
                                    t75 = present_frame();
                                    g_room_mode = 0;
                                    ax = t75;
                                }
                            } else if (g_room != ROOM_FILM_AGENCY) {
                                if (g_room != ROOM_AD_AGENCY) {
                                    if (g_room != ROOM_SCRIPT_AGENCY) {
                                        if (g_room != ROOM_ESTATE_PLAN) {
                                            if (g_room != ROOM_STUDIO) {
                                                if (g_room != ROOM_SUPERMARKET) {
                                                    if (g_room != ROOM_SHOP) {
                                                        if (g_room == ROOM_BETTY) {
                                                            if (g_room_mode != 1) {
                                                                if (g_room_mode == 2) {
                                                                    t108 = present_store_in_bag_betty();
                                                                    g_room_mode = 0;
                                                                    ax = t108;
                                                                }
                                                            } else {
                                                                t107 = betty_greeting(1);
                                                                g_room_mode = 0;
                                                                ax = t107;
                                                            }
                                                        }
                                                    } else if (g_room_mode != 1) {
                                                        if (g_room_mode == 2) {
                                                            g_room_mode = 0;
                                                            ax = shop_redraw();
                                                        }
                                                    } else {
                                                        ui_drag_end();
                                                        g_room_mode = 0;
                                                        ax = present_frame();
                                                    }
                                                } else if (g_room_mode != 1) {
                                                    if (g_room_mode != 3) {
                                                        if (g_room_mode != 4) {
                                                            if (g_room_mode == 5) {
                                                                ax = present_store_in_bag();
                                                            }
                                                        } else {
                                                            ax = present_return_to_shop();
                                                        }
                                                    } else {
                                                        ui_gift_catalog(1);
                                                        t102 = ui_draw_bag(1, 6, 1);
                                                        g_room_mode = 0;
                                                        ax = t102;
                                                    }
                                                } else {
                                                    t101 = ui_supermarket_menu(1);
                                                    g_room_mode = 0;
                                                    ax = t101;
                                                }
                                            } else if (g_room_mode != 1) {
                                                if (g_room_mode != 2) {
                                                    if (g_room_mode != 3) {
                                                        if (g_room_mode == 4) {
                                                            t100 = ui_box(1, 0, 0, 0, 0, 0, 1);
                                                            g_room_mode = 3;
                                                            ax = t100;
                                                        }
                                                    } else {
                                                        t99 = ui_studio_menu(1);
                                                        g_room_mode = 0;
                                                        ax = t99;
                                                    }
                                                } else {
                                                    t98 = studio_insert_script();
                                                    g_room_mode = 0;
                                                    ax = t98;
                                                }
                                            } else {
                                                t97 = script_to_bag_from_studio();
                                                g_room_mode = 0;
                                                ax = t97;
                                            }
                                        } else if (g_room_mode == 1 || g_room_mode == 2) {
                                            t96 = ui_yesno_box(0, 0, 1);
                                            g_room_mode = 0;
                                            ax = t96;
                                        }
                                    } else if (g_room_mode != 1) {
                                        if (g_room_mode != 2) {
                                            if (g_room_mode == 4) {
                                                porter_say(1);
                                                g_room_mode = 0;
                                                ax = present_frame();
                                            }
                                        } else {
                                            ax = script_to_bag();
                                        }
                                    } else {
                                        ax = script_return_to_agency();
                                    }
                                } else if (g_room_mode != 1) {
                                    if (g_room_mode == 2) {
                                        ax = ad_take_contract();
                                    }
                                } else {
                                    ax = ad_return_offer();
                                }
                            } else if (g_room_mode != 1) {
                                if (g_room_mode != 4) {
                                    if (g_room_mode == 5) {
                                        ax = filmagency_film_to_bag();
                                    }
                                } else {
                                    ui_draw_auction_board(1);
                                    ui_draw_bag(0, 2, 0 /* unused ax */);
                                    present_frame();
                                    g_room_mode = 0;
                                    cursor_shape_push(3);
                                    snd_room_music_start(g_room, 0, 0 /* unused ax */);
                                    ax = cursor_shape_push(0);
                                }
                            } else {
                                ax = filmagency_return_film(ax);
                            }
                        } else if (g_room_mode == 1) {
                            t74 = gossip_say(1, 0, 0);
                            g_room_mode = 0;
                            ax = t74;
                        }
                    } else if (g_room_mode != 1) {
                        if (g_room_mode == 2) {
                            t73 = ui_drag_end();
                            g_room_mode = 1;
                            g_redraw = 1;
                            ax = t73;
                        }
                    } else {
                        ui_draw_news_slots(g_room_owner, 1);
                        ui_draw_news_inbox(1);
                        ui_draw_news_agencies(g_room_owner);
                        t72 = present_frame();
                        g_room_mode = 0;
                        ax = t72;
                    }
                } else if (g_room_mode != 1) {
                    if (g_room_mode == 2) {
                        t51 = tetris_draw(1, 1);
                        g_room_mode = 0;
                        ax = t51;
                    }
                } else {
                    t50 = present_frame();
                    g_room_mode = 0;
                    ax = t50;
                }
            } else if (g_room_mode == 3 || g_room_mode == 4) {
                g_room_mode = 2;
                ax = save_overlay_box(0);
            }
        } else {
            game_clock_stop(0 /* unused ax */);
            g_clock_acc = G32(0x72AE);
            game_clock_start();
            room_return();
            if (world_now()) {               /* seats: the world in seat 0 */
                ax = game_time_tick(1, 1);
            }
        }
    }
    return ax;
}

/* 0054:32B6  leave all rooms, walk the human to the exit, wipe, clock to 17:00 */
void day_end_go_home(void) {
    FUNC_TICK(26);
    if (!(seat_humans && seat_out[0]))   /* multiplayer: an out host's figure moves no more -- the walk would wait for ever */
        player_walk_out();
    mp_wait_humans_home();                       /* multiplayer: and the other humans (src/seatctx.c) */
    seat_request(SEAT_REQ_DAY_DARK);             /* seats: the others too (src/seatctx.c) */
    rtl_bitmap_fill_w(W2(g_screen_buf), 0, 0, 0x28, 0x78, 0);
    screen_transition(1, 2, 4, 0 /* unused ax */);
    room_draw_border();
    g_time_slot = 0x11;
    return;
}

/* 0054:32FC  leave rooms, then walk the human figure to floor 1 and the exit x and wait until it arrives */
void player_walk_out(void) {
    u16 ax;
    u32 l_4;
    FUNC_TICK(38);
    l_4 = figures_at(g_player_figure);
    if (g_room != ROOM_BUILDING) {
        for (;;) {
            LOOP_TICK(9);
            room_escape();
            if (g_room == ROOM_BUILDING) {
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
    }
    AT(figure, l_4).target_floor = 1;
    AT(figure, l_4).target_door = 0xFFFF;
    if (!((AT(figure, l_4).motion) == 3 || (AT(figure, l_4).lift_phase) != 0)) {
        AT(figure, l_4).target_x = 0x99;
        AT(figure, l_4).motion = 1;
    }
    while ((AT(figure, l_4).floor) != 1 || (AT(figure, l_4).motion) != 0) {
        LOOP_TICK(7);
        game_frame();
    }
    AT(figure, l_4).target_x = 0x48;
    AT(figure, l_4).motion = 1;
    while ((AT(figure, l_4).motion) != 0) {
        LOOP_TICK(4);
        game_frame();
    }
    return;
}

/* 0054:33B3  walk the human figure to its floor (3p+2), enter the boss room (3), reset other figures to their targets */
void day_start_to_boss(void) {
    u16 si;
    u32 l_4;
    u32 l_8;
    FUNC_TICK(40);
    l_8 = figures_at(g_player_figure);
    AT(figure, l_8).target_door = 2;
    AT(figure, l_8).target_floor = (g_player * 3) + 2;
    AT(figure, l_8).target_x = 0x99;
    AT(figure, l_8).motion = 1;
    while ((AT(figure, l_8).task) != 0) {
        LOOP_TICK(4);
        game_frame();
    }
    room_enter(ROOM_BOSS);
    si = 0;
    while (((i16)si) <= 2 && seat_cur <= 0) {   /* (multiplayer: at seat 0's arrival only -- another human's
                                                 * later one would put the computer players back again) */
        LOOP_TICK(28);
        if (si != g_player_figure && !mp_is_human(si + 1)) {   /* (multiplayer: not another human's) */
            l_4 = figures_at(si);
            AT(figure, l_4).x = AT(figure, l_4).target_x;
            AT(figure, l_4).floor = AT(figure, l_4).target_floor;
            AT(figure, l_4).motion = 0;
            AT(figure, l_4).lift_phase = 0;
            AT(figure, l_4).task = 0;
            if (seat_humans) elevator_queue_remove(si);   /* multiplayer: only their calls go */
        }
        si = si + 1;
    }
    if (!seat_humans) g_elevator_calls = 0;   /* (multiplayer: another human may be waiting for the cabin) */
    return;
}

/* 0054:3481  if mode == 4: wipe the 320x120 building view from the back buffer (dir, delay) */
u16 screen_transition(u16 a0, u16 a1, u16 a2, u16 ax) {
    FUNC_TICK(24);
    clip_reset();
    if (a2 == 4) {
        clip_reset();
        ax = screen_wipe(g_screen_buf_lo, g_screen_buf_hi, 0, 0, 0x140, 0x78, a0, a1);
    }
    return ax;
}

/* 0054:34BD  box wipe effect copying a w x h area from a buffer to the screen in shrinking/growing frames (dir 1 = outward), delay per step */
u16 screen_wipe(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5, u16 a6, u16 a7) {
    u16 ax;
    u16 si;
    u32 l_4;
    u16 l_6;
    u16 l_8;
    u16 l_A;
    u16 l_C;
    u16 l_E;
    u16 l_10;
    u16 l_12;
    u16 l_14;
    u16 l_16;
    u16 t12;
    FUNC_TICK(51);
    l_10 = (u16)((u16)((i32)(i16)a5 / (i16)(((i16)(((i16)(((i16)a4) >> 1)) >> 1)) >> 1)));
    l_A = ((i16)(((i16)(((i16)a2) >> 1)) >> 1)) >> 1;
    l_8 = ((i16)(((i16)(((i16)a4) >> 1)) >> 1)) >> 1;
    if (a6 != 1) {
        l_E = (((i16)a4) >> 4) - 1;
        l_C = 0xFFFF;
        l_6 = 0xFFFF;
    } else {
        l_E = 0;
        l_C = ((i16)a4) >> 4;
        l_6 = 1;
    }
    t12 = timer_read();
    l_16 = t12 + a7;
    ax = t12 + a7;
    si = l_E;
    while (si != l_C) {
        LOOP_TICK(183);
        for (;;) {
            LOOP_TICK(3);
            if ((timer_read()) < l_16) {
                seat_pass();                     /* seats: a wait point (runtime/seat.h) */
                continue;
            } else {
                break;
            }
        }
        l_16 = timer_read() + a7;
        l_14 = l_8 - si;
        l_12 = a5 - (si * l_10);
        l_4 = bitmap_alloc((l_14 - si) << 3, l_10);
        blit_opaque_rect(MK32(a1, a0), l_4, 0, 0, si << 3, si * l_10, (l_14 - si) << 3, l_10, 0 /* unused ax */);
        rtl_vga_put_bitmap_w(W2(l_4), si + l_A, (si * l_10) + a3);
        blit_opaque_rect(MK32(a1, a0), l_4, 0, 0, si << 3, (a5 - l_10) - (si * l_10), (l_14 - si) << 3, l_10, 0 /* unused ax */);
        rtl_vga_put_bitmap_w(W2(l_4), si + l_A, ((a5 - l_10) - (si * l_10)) + a3);
        mem_free(LO16(l_4), HI16(l_4));
        l_4 = bitmap_alloc(8, l_12 - (si * l_10));
        blit_opaque_rect(MK32(a1, a0), l_4, 0, 0, si << 3, si * l_10, 8, l_12 - (si * l_10), 0 /* unused ax */);
        rtl_vga_put_bitmap_w(W2(l_4), si + l_A, (si * l_10) + a3);
        blit_opaque_rect(MK32(a1, a0), l_4, 0, 0, ((l_8 - 1) - si) << 3, si * l_10, 8, l_12 - (si * l_10), 0 /* unused ax */);
        rtl_vga_put_bitmap_w(W2(l_4), ((l_8 - 1) - si) + l_A, (si * l_10) + a3);
        ax = mem_free(LO16(l_4), HI16(l_4));
        si = si + l_6;
    }
    return ax;
}

/* 0054:3701  show text 0xF6 "Start" in the clock field */
void clock_show_start(void) {
    u32 l_4;
    u32 t1;
    FUNC_TICK(23);
    t1 = text_dup(0xF6);
    l_4 = t1;
    ui_status_text(LO16(t1), HI16(t1), 0xB4);
    mem_free(LO16(l_4), HI16(l_4));
    return;
}

/* 0054:3738  office TV: picture for what channel 0x79FE airs now: test pattern before 18:00, news (:00-:05), ads (:55) or empty, program picture (+0xC / +0x2F) or genre default */
void tv_update_picture(void) {
    u16 di;
    u16 si;
    u16 t2;
    u16 t4;
    u16 t5;
    FUNC_TICK(107);
    if (((i16)(clock_hour())) >= 1) {
        if (((i16)g_minute) < 0 || ((i16)g_minute) >= 5) {
            if (((i16)g_minute) < 55) {
                t4 = clock_hour();
                t5 = sched_program_at(g_tv_channel, t4);
                di = t5;
                if (t5 != 0) {
                    if (((i16)di) >= 1000) {
                        if (jingle_2(di + 0xFC18) == 0) {
                            si = ((u16)(i8)series_genre(di + 0xFC18)) + 0xE6;
                        } else {
                            si = jingle_2(di + 0xFC18);
                        }
                    } else if ((films(di).jingle) == 0) {
                        si = ((u16)(i8)(films(di).genre)) + 0xDC;
                    } else {
                        si = films(di).jingle;
                    }
                } else {
                    si = 0xEE;
                }
            } else {
                t2 = clock_hour();
                if (schedule_ad(g_tv_channel, t2) == 0) {
                    si = 0xEE;
                } else {
                    si = 0xED;
                }
            }
        } else {
            si = 0xEC;
        }
    } else {
        si = 0xDC;
    }
    if (si != 0) {
        tv_show_picture(si);
    }
    return;
}

/* 0054:3855  draw TV picture resource (from cache 0x5A0 or loaded) into the TV window, channel digit on the empty picture, skip if unchanged */
void tv_show_picture(u16 a0) {
    u16 ax;
    int cc;
    u16 di;
    u16 s0;
    u16 si;
    u32 l_4;
    u16 t3;
    u32 t6;
    u16 t8;
    u16 t9;
    FUNC_TICK(133);
    di = a0;
    if (g_tv_off == 0) {
L_3870:
        if (di != g_tv_picture) {
L_387F:
            si = 0;
            for (;;) {
                LOOP_TICK(22);
                if (((i16)si) >= 4) {
                    ax = 0;
                } else {
                    ax = 1;
                }
                s0 = ax;
                if (g_tv_cache(si * 2) == di) {
                    ax = 0;
                } else {
                    ax = 1;
                }
                if ((s0 & ax) == 0) break;
                si = si + 1;
            }
            if (((i16)si) >= 4) {
                disk_access_begin(ax);
                cursor_shape_push(3);
                l_4 = res_load0(di);
                t8 = cursor_shape_push(0);
                disk_access_end(t8);
            } else {
                t3 = cursor_shape_push(3);
                disk_access_begin(t3);
                t6 = res_cache_unpack(g_tv_cache((si * 2) + 1), 0);
                l_4 = t6;
                disk_access_end(LO16(t6));
                cursor_shape_push(0);
            }
            clip_panel();
            draw_sprite_to(MK32(g_tvfam_atlas_hi, g_tvfam_atlas_lo + 2), LO16(l_4) + 2, HI16(l_4), 5, 5);
            cc = di != 0xEE;
            if (!(cc || ((i16)g_tv_channel) <= 1)) {
                t9 = col_to_px(g_tv_channel);
                bitmap_put(MK32(HI16(l_4), LO16(l_4) + t9), MK32(HI16(l_4), LO16(l_4) + 2), 0x10, 0x1B);
            }
            cursor_hide();
            rtl_vga_put_bitmap_w(LO16(l_4) + 2, HI16(l_4), 1, 0x7C);
            mem_free(LO16(l_4), HI16(l_4));
            cursor_show(0 /* unused ax */);
            clip_reset();
        } else {
            if (di == 0xEE) goto L_387F;
        }
    } else {
        if (di == 0xDC) goto L_3870;
    }
    g_tv_picture = di;
    return;
}

/* 0054:39CF  "TV family" in front of the TV: which of 5 members watch from the program's flags (+6 bits), the child leaves after 23:00, moods from the program's topicality 0x7A14 and genre; loads their sprites */
void tv_family_update(u16 a0) {
    int cc;
    u16 di;
    u16 si;
    u16 t1;
    u16 t3;
    u16 t5;
    u16 t7;
    u16 t14;
    u32 t27;
    u32 t31;
    u32 t37;
    u32 t43;
    u32 t49;
    u16 l_14 = 0;
    u16 l_12 = 0;
    u16 l_10 = 0;
    u16 l_E = 0;
    u8 fr_[12] = {0};                  /* bytes bp-12 .. bp-1 of the original frame */
    FUNC_TICK(199);
    l_E = 0;
    if (g_opt_tv_family == 0) {
        cursor_shape_push(3);
        t1 = clock_hour();
        if (((u32)(MK32(schedule_program(g_player, t1), a0))) == 0x270F0000) {
            di = 0;
        } else {
            di = 1;
        }
        t3 = clock_hour();
        if (schedule_program(g_player, t3) != 0) {
            t5 = clock_hour();
            l_14 = sched_program_at(g_player, t5);
            if (((i16)l_14) >= 1000) {
                l_14 = l_14 - 0x3E8;
                t14 = FPOFF(g_series) + (l_14 * 0x39);
                FR16(fr_, 12, 0x6) = FPSEG(g_series);
                FR16(fr_, 12, 0x8) = t14;
                g_tv_family(0) = (u8)(((i16)((FP16(FR32(fr_, 12, 0x8), 0x25)) << 0xE)) >> 0xF);
                g_tv_family(1) = (u8)(((i16)((FP16(FR32(fr_, 12, 0x8), 0x25)) << 0xF)) >> 0xF);
                g_tv_family(2) = (u8)(((i16)((FP16(FR32(fr_, 12, 0x8), 0x25)) << 0xD)) >> 0xF);
                g_tv_family(3) = (u8)(((i16)((FP16(FR32(fr_, 12, 0x8), 0x25)) << 0xB)) >> 0xF);
                g_tv_family(4) = (u8)(((i16)((FP16(FR32(fr_, 12, 0x8), 0x25)) << 0xC)) >> 0xF);
                l_12 = ((u16)(i8)(FP8(FR32(fr_, 12, 0x8), 0x21))) + 0xA;
            } else {
                t7 = FPOFF(g_movies) + (l_14 * 0x17);
                FR16(fr_, 12, 0x2) = FPSEG(g_movies);
                FR16(fr_, 12, 0x4) = t7;
                g_tv_family(0) = (u8)(((i16)((FP16(FR32(fr_, 12, 0x4), 6)) << 0xE)) >> 0xF);
                g_tv_family(1) = (u8)(((i16)((FP16(FR32(fr_, 12, 0x4), 6)) << 0xF)) >> 0xF);
                g_tv_family(2) = (u8)(((i16)((FP16(FR32(fr_, 12, 0x4), 6)) << 0xD)) >> 0xF);
                g_tv_family(3) = (u8)(((i16)((FP16(FR32(fr_, 12, 0x4), 6)) << 0xB)) >> 0xF);
                g_tv_family(4) = (u8)(((i16)((FP16(FR32(fr_, 12, 0x4), 6)) << 0xC)) >> 0xF);
                l_12 = (u16)(i8)(FP8(FR32(fr_, 12, 0x4), 0));
            }
            if (((i16)(clock_hour())) >= 6) {
                g_tv_family(4) = 0;
            }
        } else {
            mem_zero(DSP(0x5B2), 5);
            di = 0;
        }
        if (g_tv_family(1) == 0) {
            l_10 = 0xD;
        } else {
            G16(0x8342) = 6;
            l_10 = 0x22;
        }
        if (di != 0) {
            g_tv_family_gfx_pos = 0;
        }
        tvfam_draw_room();
        si = 0;
        while (((i16)si) <= 4) {
            LOOP_TICK(275);
            cc = g_tv_family(si) != 0;
            if (cc && ((i16)l_E) < 3) {
                if (si != 1) {
                    G16((si << 4) + 0x8332) = l_10;
                }
                if (si == 0) {
                    FR16(fr_, 12, 0xA) = DSEG;
                    FR16(fr_, 12, 0xC) = 0x832E;
                    l_10 = l_10 + 0x1A;
                    FP16(FR32(fr_, 12, 0xC), 8) = 1;
                    if ((random_n(5, si)) == 1) {
                        FP16(FR32(fr_, 12, 0xC), 0xA) = (FP16(FR32(fr_, 12, 0xC), 0xA)) + 1;
                    }
                    if (((i16)(FP16(FR32(fr_, 12, 0xC), 0xA))) > 7) {
                        FP16(FR32(fr_, 12, 0xC), 0xA) = 1;
                    }
                    FP16(FR32(fr_, 12, 0xC), 0xC) = 1;
                    if (((i16)g_tv_program_quality) > 10) {
                        if (((i16)g_tv_program_quality) > 20) {
                            if (((i16)g_tv_program_quality) > 30) {
                                if (l_12 != 4) {
                                    if (l_12 != 5) {
                                        FP16(FR32(fr_, 12, 0xC), 6) = 3;
                                    } else {
                                        FP16(FR32(fr_, 12, 0xC), 6) = 2;
                                    }
                                } else {
                                    FP16(FR32(fr_, 12, 0xC), 6) = 6;
                                }
                            } else {
                                FP16(FR32(fr_, 12, 0xC), 6) = 0;
                            }
                        } else {
                            FP16(FR32(fr_, 12, 0xC), 6) = 5;
                        }
                    } else {
                        FP16(FR32(fr_, 12, 0xC), 6) = 4;
                    }
                    if (di != 0) {
                        t27 = tv_family_load_sprite(0xB0);
                        MEM32((u16)(FPSEG(FR32(fr_, 12, 0xC))), (u16)(FPOFF(FR32(fr_, 12, 0xC)))) = t27;
                    }
                    tvfam_draw_viewer0();
                } else if (si == 1) {
                    FR16(fr_, 12, 0xA) = DSEG;
                    FR16(fr_, 12, 0xC) = 0x833E;
                    if (((i16)(clock_hour())) < 5 || ((i16)g_tv_program_quality) > 30) {
                        FP16(FR32(fr_, 12, 0xC), 8) = 1;
                        FP16(FR32(fr_, 12, 0xC), 0xA) = 1;
                        if (((i16)g_tv_program_quality) > 10) {
                            if (((i16)g_tv_program_quality) > 20) {
                                if (((i16)g_tv_program_quality) > 30) {
                                    FP16(FR32(fr_, 12, 0xC), 6) = 5;
                                } else {
                                    FP16(FR32(fr_, 12, 0xC), 6) = 1;
                                }
                            } else {
                                FP16(FR32(fr_, 12, 0xC), 6) = 2;
                            }
                        } else {
                            FP16(FR32(fr_, 12, 0xC), 6) = 4;
                        }
                    } else if ((FP16(FR32(fr_, 12, 0xC), 8)) != 2) {
                        FP16(FR32(fr_, 12, 0xC), 8) = 2;
                        FP16(FR32(fr_, 12, 0xC), 0xA) = 1;
                    }
                    if (di != 0) {
                        t31 = tv_family_load_sprite(0xB4);
                        MEM32((u16)(FPSEG(FR32(fr_, 12, 0xC))), (u16)(FPOFF(FR32(fr_, 12, 0xC)))) = t31;
                    }
                    tvfam_draw_viewer1();
                } else if (si == 2) {
                    FR16(fr_, 12, 0xA) = DSEG;
                    FR16(fr_, 12, 0xC) = 0x834E;
                    l_10 = l_10 + 0x1A;
                    if (((i16)(clock_hour())) < 4 || ((i16)g_tv_program_quality) > 5) {
                        if (((i16)g_tv_program_quality) > 7) {
                            FP16(FR32(fr_, 12, 0xC), 8) = 1;
                            if (((i16)g_tv_program_quality) > 10) {
                                if (((i16)g_tv_program_quality) > 20) {
                                    if (((i16)g_tv_program_quality) > 30) {
                                        if (l_12 != 4) {
                                            FP16(FR32(fr_, 12, 0xC), 6) = 0xE;
                                        } else {
                                            FP16(FR32(fr_, 12, 0xC), 6) = 0xB;
                                        }
                                    } else {
                                        FP16(FR32(fr_, 12, 0xC), 6) = 9;
                                    }
                                } else {
                                    FP16(FR32(fr_, 12, 0xC), 6) = 0xD;
                                }
                            } else {
                                FP16(FR32(fr_, 12, 0xC), 6) = 0xC;
                            }
                        } else {
                            FP16(FR32(fr_, 12, 0xC), 8) = 2;
                        }
                    } else {
                        FP16(FR32(fr_, 12, 0xC), 8) = 3;
                    }
                    FP16(FR32(fr_, 12, 0xC), 0xA) = 0;
                    if (di != 0) {
                        t37 = tv_family_load_sprite(0xB1);
                        MEM32((u16)(FPSEG(FR32(fr_, 12, 0xC))), (u16)(FPOFF(FR32(fr_, 12, 0xC)))) = t37;
                    }
                    tvfam_draw_viewer2();
                } else if (si == 3) {
                    FR16(fr_, 12, 0xA) = DSEG;
                    FR16(fr_, 12, 0xC) = 0x835E;
                    l_10 = l_10 + 0x19;
                    FP16(FR32(fr_, 12, 0xC), 8) = 1;
                    if (((i16)g_tv_program_quality) > 5) {
                        if (((i16)g_tv_program_quality) > 10) {
                            if (((i16)g_tv_program_quality) > 20) {
                                if (((i16)g_tv_program_quality) > 30) {
                                    FP16(FR32(fr_, 12, 0xC), 6) = 9;
                                } else {
                                    FP16(FR32(fr_, 12, 0xC), 6) = 8;
                                }
                            } else {
                                FP16(FR32(fr_, 12, 0xC), 6) = 5;
                            }
                        } else {
                            FP16(FR32(fr_, 12, 0xC), 6) = 6;
                        }
                    } else {
                        FP16(FR32(fr_, 12, 0xC), 6) = 7;
                    }
                    if (di != 0) {
                        t43 = tv_family_load_sprite(0xB2);
                        MEM32((u16)(FPSEG(FR32(fr_, 12, 0xC))), (u16)(FPOFF(FR32(fr_, 12, 0xC)))) = t43;
                    }
                    tvfam_draw_viewer3();
                } else if (si == 4) {
                    FR16(fr_, 12, 0xA) = DSEG;
                    FR16(fr_, 12, 0xC) = 0x836E;
                    l_10 = l_10 + 0x19;
                    FP16(FR32(fr_, 12, 0xC), 0xA) = 0;
                    if (((i16)g_tv_program_quality) <= 20) {
                        FP16(FR32(fr_, 12, 0xC), 8) = 1;
                    } else {
                        FP16(FR32(fr_, 12, 0xC), 8) = 2;
                        FP16(FR32(fr_, 12, 0xC), 0xC) = 0;
                    }
                    if (((i16)g_tv_program_quality) > 10) {
                        if (((i16)g_tv_program_quality) > 20) {
                            FP16(FR32(fr_, 12, 0xC), 6) = 0xD;
                        } else {
                            FP16(FR32(fr_, 12, 0xC), 6) = 0;
                        }
                    } else {
                        FP16(FR32(fr_, 12, 0xC), 6) = 0xC;
                    }
                    if (di != 0) {
                        t49 = tv_family_load_sprite(0xB3);
                        MEM32((u16)(FPSEG(FR32(fr_, 12, 0xC))), (u16)(FPOFF(FR32(fr_, 12, 0xC)))) = t49;
                    }
                    tvfam_draw_viewer4();
                }
                l_E = l_E + 1;
            } else {
                G16((si << 4) + 0x8336) = 0;
            }
            si = si + 1;
        }
        if (a0 != 2) {
            G32(0x79F4) = 0x20007;
        }
        cursor_shape_push(0);
    }
    return;
}

/* 0054:3F89  ?reset TV family animation state (0x79F4 = 0, 0x79F6 = 1) */
void tv_family_stop(void) {
    FUNC_TICK(3);
    G32(0x79F4) = 0x10000;
    return;
}

/* 0054:3F96  allocate 10 save-slot name buffers (0x8BB2) */
u16 savegame_names_alloc(u16 ax) {
    u16 si;
    u32 t1;
    FUNC_TICK(5);
    si = 0;
    while (((i16)si) < 10) {
        LOOP_TICK(14);
        t1 = mem_calloc(0x29, 1);
        g_save_names(si) = t1;
        ax = LO16(t1);
        si = si + 1;
    }
    return ax;
}

/* 0054:3FC1  read the 10 save-slot names (40 bytes each) from resource 0xD1 */
void savegame_names_load(void) {
    u16 si;
    u32 l_4;
    FUNC_TICK(21);
    l_4 = res_load_raw(0xD1, 0xD1);
    si = 0;
    while (((i16)si) < 10) {
        LOOP_TICK(20);
        far_memcpy(HPTR(g_save_names(si)), HPTR(MK32(HI16(l_4), LO16(l_4) + (si * 0x28))), 0x28);
        si = si + 1;
    }
    mem_free(LO16(l_4), HI16(l_4));
    return;
}

/* 0054:4021  write the 10 save-slot names back to resource 0xD1 */
void savegame_names_store(void) {
    u16 ax;
    u16 si;
    u32 l_4;
    u32 t1;
    u16 t5;
    FUNC_TICK(32);
    t1 = mem_calloc(0x190, 1);
    l_4 = t1;
    ax = LO16(t1);
    si = 0;
    while (((i16)si) < 10) {
        LOOP_TICK(20);
        ax = far_memcpy(HPTR(MK32(HI16(l_4), LO16(l_4) + (si * 0x28))), HPTR(g_save_names(si)), 0x28);
        si = si + 1;
    }
    disk_access_begin(ax);
    t5 = res_write(0xD1, HPTR(l_4), 0x190, 0xD1);
    disk_access_end(t5);
    mem_free(LO16(l_4), HI16(l_4));
    return;
}

/* 0054:40A6  load a sprite resource into the TV family buffer 0x8C50 at 0x8C5A, fix pointers, advance; returns its address */
u32 tv_family_load_sprite(u16 a0) {
    u16 ax;
    u16 dx;
    u32 l_4;
    u16 t2;
    u16 t5;
    FUNC_TICK(49);
    l_4 = res_load(a0);
    t2 = bank_mem_size(l_4);
    far_memcpy(HPTR(MK32(g_tv_family_gfx_hi, g_tv_family_gfx_lo + g_tv_family_gfx_pos)), HPTR(l_4), t2);
    sprite_bank_fixup(MK32(g_tv_family_gfx_hi, g_tv_family_gfx_lo + g_tv_family_gfx_pos));
    mem_free(LO16(l_4), HI16(l_4));
    l_4 = MK32(FPSEG(g_tv_family_gfx), FPOFF(g_tv_family_gfx) + g_tv_family_gfx_pos);
    t5 = bank_mem_size(l_4);
    g_tv_family_gfx_pos = g_tv_family_gfx_pos + t5;
    ax = LO16(l_4);
    dx = HI16(l_4);
    return MK32(dx, ax);
}

/* 0054:4135  redraw the programme planner if it is open (room 0x1F) */
void planner_refresh(void) {
    FUNC_TICK(62);
    if (g_room == ROOM_PLANNER) {
        if (g_room_mode == 0 || g_room_mode == 3 || g_room_mode == 4 || g_room_mode == 6) {
            ui_draw_planner(g_room_owner);
            g_redraw = 1;
            present_frame();
        } else if (g_room_mode != 2) {
            if (g_room_mode == 5) {
                ui_contract_popup(0, 0, 1, g_room_owner);
                ui_contract_popup(0x16, 1, 0, g_room_owner);
                present_frame();
            }
        } else {
            ui_library_genre_popup(0, 0, 1, 0);
            ui_library_genre_popup(0x13, 8, 0, g_room_owner);
        }
    }
    return;
}

/* 0054:41E2  redraw the library lists if the library is open (room 4) */
void library_refresh(void) {
    u16 t1;
    u16 t2;
    FUNC_TICK(31);
    if (g_room == ROOM_LIBRARY) {
        cursor_shape_push(3);
        txt_film_cache_free(0);
        t1 = txt_series_cache_free(0);
        t2 = txt_film_cache_load(g_room, g_room_owner, t1);
        txt_series_cache_load(g_room, g_room_owner, t2);
        cursor_shape_push(0);
    }
    return;
}

/* 0054:4233  language prompt (only when no command-line argument) */
u16 choose_language(void) {
    u16 ax;
    int cc;
    u16 si;
    u32 rv_;
    u32 t1;
    u32 t2;
    u32 t3;
    u32 t4;
    u32 names;
    u8 fr_[42] = {0};                  /* bytes bp-42 .. bp-1 of the original frame */
    FUNC_TICK(96);
    res_read_raw(0x29, (fr_ + 42 - 0x28), 0x29);
    names = host_handle((fr_ + 42 - 0x28), 0x28);   /* the prompt takes the language names by far pointer */
    lang_prompt_box(0, names);
    host_handle_release(names);
    t1 = bitmap_alloc(0x18, 0xB);
    g_cursor_comp = t1;
    t2 = bitmap_alloc(0x18, 0xB);
    g_cursor_bg = t2;
    t3 = bitmap_alloc(0x18, 0xB);
    g_cursor_tmp = t3;
    g_room = 0xFFFB;
    g_cursor_shown = 0;
    g_cursor_shape = 1;
    G32(0x7BF6) = 0x6400A0;
    mouse_bounds_full();
    cursor_show(0 /* unused ax */);
    g_click = 0;
    for (;;) {
        LOOP_TICK(20);
        t4 = rtl_key_read();
        si = LO16(t4);
        if (LO16(t4) == ((u16)((u16)(i8)FR8(fr_, 42, 0x28)))) break;
        cc = ((u16)((u16)(i8)FR8(fr_, 42, 0x14))) == si;
        if (cc || !(g_click == 0 || ((i16)g_mouse_y) < 92 || ((i16)g_mouse_y) > 108)) break;
        game_frame();
    }
    if (g_click == 0 || ((i16)g_mouse_y) < 92 || ((i16)g_mouse_y) > 108) {
        if (((u16)((u16)(i8)FR8(fr_, 42, 0x28))) != si) {
            ax = 1;
        } else {
            ax = 0;
        }
        g_language = ax;
    } else {
        g_language = (u16)((u16)((i32)(i16)(g_mouse_y + 0xFFA4) / 8));
    }
    cursor_hide();
    lang_prompt_box(1, 0);
    mem_free(g_cursor_comp_lo, g_cursor_comp_hi);
    mem_free(g_cursor_bg_lo, g_cursor_bg_hi);
    rv_ = mem_free(g_cursor_tmp_lo, g_cursor_tmp_hi);
    return (u16)rv_;
}
