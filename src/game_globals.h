/* Named globals of the data segment (DGROUP) and named fields of records
 * without a struct.  Started from names/symbols.txt; hand-maintained since
 * the freeze. */
#ifndef GAME_GLOBALS_H
#define GAME_GLOBALS_H

/* named fields of record arrays: name(i) = field of record i */
#define genre(i) G16((u16)((i) * 0x4 + 0x94))   /* trend genre */
#define mult(i) G16((u16)((i) * 0x4 + 0x96))   /* audience multiplier x10 */
#define page(i) G16((u16)((i) * 0x6 + 0x1492))   /* boss dialog page */
#define option(i) G16((u16)((i) * 0x6 + 0x1494))   /* option number */
#define text(i) G16((u16)((i) * 0x6 + 0x1496))   /* reply text id */
#define price_2_hi(i) G16((u16)((i) * 0x9 + 0x1970))
#define lot_bid_lo(i) G16((u16)((i) * 0x9 + 0x1B0F))
#define viewers_lo(i) G16((u16)((i) * 0xA + 0x24AF))
#define series_genre(i) FP8(G32(0x2646), (u16)((i) * 0x39 + 0x21))   /* 1 family, 2 SF, 3 western, 4 whodunit, 5 sundry (ad/trend genre = 10 + value) */
#define series_episodes(i) FP8(G32(0x2646), (u16)((i) * 0x39 + 0x22))   /* number of episodes; episode e at e*3: +0 quality, +1 topicality, +2 bit0 exists */
#define series_blocks(i) FP8(G32(0x2646), (u16)((i) * 0x39 + 0x23))   /* episode length in hour blocks */
#define series_owner(i) FP8(G32(0x2646), (u16)((i) * 0x39 + 0x24))   /* owning player */
#define series_flags(i) FP16(G32(0x2646), (u16)((i) * 0x39 + 0x25))   /* bit5 on the market */
#define series_flags2(i) FP16(G32(0x2646), (u16)((i) * 0x39 + 0x26))   /* bit1 scheduled from now on */
#define price_5_lo(i) FP16(G32(0x2646), (u16)((i) * 0x39 + 0x27))
#define price_5_hi(i) FP16(G32(0x2646), (u16)((i) * 0x39 + 0x29))
#define jingle_2(i) FP16(G32(0x2646), (u16)((i) * 0x39 + 0x2F))   /* sound id */
#define ad_name(i) FP32(ads_at(i), 0)   /* advertiser name (far text) */
#define ad_name_lo(i) FP16(ads_at(i), 0)
#define ad_name_hi(i) FP16(ads_at(i), 2)
#define ad_spots(i) FP8(ads_at(i), 9)   /* spots required */
#define ad_fee_lo(i) FP16(ads_at(i), 0x10)
#define ad_fee_hi(i) FP16(ads_at(i), 0x12)
#define ad_penalty_lo(i) FP16(ads_at(i), 0x14)
#define ad_penalty_hi(i) FP16(ads_at(i), 0x16)
#define rent_lo(i) G16((u16)((i) * 0x10 + 0x616B))
#define rent_hi(i) G16((u16)((i) * 0x10 + 0x616D))
#define film(i) G16((u16)((i) * 0xA + 0x63E5))   /* cached film id */
#define title_lo(i) G16((u16)((i) * 0xA + 0x63E7))
#define title_hi(i) G16((u16)((i) * 0xA + 0x63E9))
#define desc_lo(i) G16((u16)((i) * 0xA + 0x63EB))
#define desc_hi(i) G16((u16)((i) * 0xA + 0x63ED))
#define aibudget_gifts_hi(i) G16((u16)((i) * 0x3C + 0x6E2F))
#define aibudget_films_lo(i) G16((u16)((i) * 0x3C + 0x6E31))
#define aibudget_films_hi(i) G16((u16)((i) * 0x3C + 0x6E33))
#define aibudget_transmitters_lo(i) G16((u16)((i) * 0x3C + 0x6E3D))
#define price_3_lo(i) G16((u16)((i) * 0x8 + 0x724C))
#define price_3_hi(i) G16((u16)((i) * 0x8 + 0x724E))
#define price_7(i) G32((u16)((i) * 0x8 + 0x724C))   /* gift price */
#define fig_target_floor(i) G16((u16)((i) * 0x1E + 0x7B48))   /* walk target floor */
#define fig_target_door(i) G16((u16)((i) * 0x1E + 0x7B54))   /* target door */
#define speed(i) G16((u16)((i) * 0x1E + 0x7B5C))   /* px per step (human 4, computer 3) */
#define pl_name_lo(i) G16((u16)((i) * 0x17 + 0x8BDC))
#define pl_name_hi(i) G16((u16)((i) * 0x17 + 0x8BDE))
#define pl_station_lo(i) G16((u16)((i) * 0x17 + 0x8BE0))
#define pl_station_hi(i) G16((u16)((i) * 0x17 + 0x8BE2))
#define pl_money_lo(i) G16((u16)((i) * 0x17 + 0x8BE4))
#define pl_money_hi(i) G16((u16)((i) * 0x17 + 0x8BE6))
#define pl_out(i) G8((u16)((i) * 0x17 + 0x8BF0))   /* player out of the game */

/* named DGROUP globals */
#define g_trends(i)              G16((u16)(0x0094 + (i) * 2))   /* array; audience trends [i*4]: +0 genre (1..15, series 10+g), +2 viewer multiplier x10 (13, 15, 13, 10, 10) */
#define g_ad_min_viewers         G32(0x00A8)   /* [class-1] minimum viewers per ad audience class: 0, 250k, 500k, 1M, 2.5M, 5M, 10M, 25M, 50M, 100M */
#define g_ai_player(i)           G8((u16)(0x00F2 + (i) * 1))   /* array; [ai 0/1] player index of the two computer players */
#define g_building_bg            G32(0x00F4)   /* building background (day or night) */
#define g_figure_sprites         G32(0x00F8)   /* figure sprite buffer */
#define g_room_gfx               G32(0x0100)   /* room graphics pointers 0x100..0x124 */
#define g_room_gfx1              G32(0x0104)   /* room graphics pointer slot 1 (meaning depends on the room) */
#define g_room_gfx2              G32(0x0108)   /* room graphics pointer slot 2 */
#define g_room_gfx3              G32(0x010C)   /* room graphics pointer slot 3 */
#define g_actor_txt              G32(0x0110)   /* actor text bank (offset table + strings); also used as a bitmap by options_draw */
#define g_room_gfx5              G32(0x0114)   /* room graphics pointer slot 5 (slot 4 = g_actor_txt) */
#define g_room_gfx6              G32(0x0118)   /* room graphics pointer slot 6 */
#define g_room_gfx7              G32(0x011C)   /* room graphics pointer slot 7 */
#define g_room_gfx8              G32(0x0120)   /* room graphics pointer slot 8 */
#define g_window_buf             G32(0x0124)   /* preallocated window bitmap used by window_create instead of allocating (null = allocate) */
#define g_elevator_offset        G16(0x0128)   /* cabin pixel offset between floors */
#define g_elevator_door          G16(0x012A)   /* door counter 0..2 (2 = open) */
#define g_clock_halt             G16(0x012C)   /* nonzero: game clock and AI stopped */
#define g_font_map(i)            G8((u16)(0x012E + (i) * 1))   /* array; 256-byte table char -> glyph index (0 = not in font) */
#define g_glyph_width(i)         G8((u16)(0x022E + (i) * 1))   /* array; font glyph widths by char */
#define g_filmshelf_rects(i)     G16((u16)(0x032E + (i) * 2))   /* array; film agency shelf slot rects (x1,y1,x2,y2), slots 1..10 */
#define g_clip_on                G16(0x0386)   /* clipping enabled */
#define g_clip_x1                G16(0x0388)   /* clip rect left (stored but the blitters clip only vertically) */
#define g_clip_y1                G16(0x038A)   /* clip rect top (used by blit_transparent/blit_opaque_rect) */
#define g_clip_x2                G16(0x038C)   /* clip rect right (not used by the blitters) */
#define g_clip_y2                G16(0x038E)   /* clip rect bottom (exclusive) */
#define g_drag_image             G32(0x0390)   /* drag cursor bitmap (non-null while something is carried) */
#define g_drag_w                 G16(0x0394)   /* width of g_drag_image in px */
#define g_drag_h                 G16(0x0396)   /* height of g_drag_image */
#define g_drag_hot_x             G16(0x0398)   /* hotspot x: drag image drawn at mouse_x - hot_x */
#define g_drag_hot_y             G16(0x039A)   /* hotspot y */
#define g_madris_shapes          G8(0x039C)   /* 7 pieces x 4 rotations x 4x4 cells */
#define g_language               G16(0x055C)   /* 0 = German, 1 = English, 0xFFFF = ask */
#define g_debug_level            G16(0x0580)   /* number of '*' characters in the 2nd argument */
#define g_tv_cache(i)            G16((u16)(0x05A0 + (i) * 2))   /* array; 4 x (picture resource, cache slot) */
#define g_tv_picture             G16(0x05B0)   /* last TV picture shown */
#define g_tv_family(i)           G8((u16)(0x05B2 + (i) * 1))   /* array; 5 flags: which TV family members watch */
#define g_boss_complained        G16(0x05E6)   /* boss already complained in this speech */
#define g_player_quit(i)         G16((u16)(0x05E8 + (i) * 2))   /* array; set when the player quit / was fired by the boss */
#define g_hotspots               G16(0x0638)   /* 145 hotspot records of 12 bytes */
#define g_disk_idle              G16(0x0D04)   /* nesting counter of disk_access_begin/end (1 = idle) */
#define g_res_open_count         G16(0x0D06)   /* debug counter of resource file opens */
#define g_timer_run_count        G16(0x0D08)   /* debug counter: game-clock timer starts minus stops (game_clock_reset/start ++, game_clock_stop --; never read) */
#define g_clock_pause_depth      G16(0x0D0A)   /* nesting of game_clock_pause */
#define g_clock_seconds          G16(0x0D0C)   /* game seconds since 17:00 (cached) */
#define g_figure_sprite_res(i)   G16((u16)(0x0D0E + (i) * 2))   /* array; sprite resource loaded in each figure slot */
#define g_madris_tick            G16(0x0D26)   /* frames since last fall step */
#define g_cursor_shape_sp        G16(0x0D2A)   /* depth of g_cursor_shape_stack */
#define g_madris_points(i)       G8((u16)(0x0D2B + (i) * 1))   /* array; points per piece (index 1..7: 10,8,8,6,6,6,4) */
#define g_madris_drawn_x         G16(0x0D33)   /* column of the piece stamped last frame (999 = none) */
#define g_key_dir                G16(0x0D38)   /* last arrow key direction (1 up 2 down 3 left 4 right) */
#define g_key_speed              G16(0x0D3A)   /* keyboard cursor speed 2..16 */
#define g_room_anim_frame        G16(0x0D3C)   /* room animation frame counter */
#define g_room_anim_sub          G16(0x0D3E)   /* room animation helper next to g_room_anim_frame: random overlay variant kept between re-rolls (studio, film agency, room -3 from studio), loop count in the library (> 10 -> frame 18); 0 on reset */
#define g_news_anim_pick         G16(0x0D42)   /* room -3 newsroom cut-in animation: random overlay sprite 0..6, re-rolled every 4th frame and never the same twice; 0xFFFF after reset */
#define g_sa_fly1_bg             G32(0x0D44)   /* screenplay agency animation: saved screen under flying object 1 (24x12, frames 812..830, tumbling sprites 10..17, ?thrown paper); 3 records {bitmap, x, y} at 0xD44 freed by room_anim_update(2) */
#define g_sa_fly1_x              G16(0x0D48)   /* x (multiple of 8) where g_sa_fly1_bg was grabbed */
#define g_sa_fly1_y              G16(0x0D4A)   /* y where g_sa_fly1_bg was grabbed */
#define g_sa_fly2_bg             G32(0x0D4C)   /* screenplay agency animation: saved screen under flying object 2 (32x20, frames 824..841, sprites 18..22, ?thrown screenplay); on landing a hidden script sprite is made visible */
#define g_sa_fly2_x              G16(0x0D50)   /* x (multiple of 8) where g_sa_fly2_bg was grabbed */
#define g_sa_fly2_y              G16(0x0D52)   /* y where g_sa_fly2_bg was grabbed */
#define g_anim_div               G16(0x0D9C)   /* counter of anim_div_tick */
#define g_ending_anim(i)         G8((u16)(0x0E69 + (i) * 1))   /* array; ending animation script: 6 x {frame, sprite, x, y} */
#define g_tip_id                 G16(0x0E82)   /* id of the tooltip currently shown (0 = none) */
#define g_tip_shown_col          G16(0x0E84)   /* column of the shown tooltip */
#define g_tip_shown_row          G16(0x0E86)   /* row of the shown tooltip */
#define g_tip_room               G16(0x0E88)   /* room in which the tooltip state was reset */
#define g_tip_room_state         G16(0x0E8A)   /* room state in which the tooltip state was reset */
#define g_daylight               G16(0x117E)   /* building scenery variant: 1 = day (clock_hour < 4, skyline res 5), 2 = night (res 14) */
#define g_bld_drawn_top          G16(0x11A4)   /* top floor of the last render */
#define g_bld_drawn_scroll       G16(0x11A6)   /* scroll of the last side-scenery render */
#define g_initdata_pos           G16(0x11E4)   /* read position in the init-data blob */
#define g_strpool_pos            G16(0x11E6)   /* used bytes of the string pool */
#define g_news_price_steps(i)    G8((u16)(0x11E8 + (i) * 1))   /* array; value thresholds of the news price classes {15,30,40,50,60,100} (second copy at 0x11EE) */
#define g_transmitter_mask       G16(0x11F4)   /* 32x16 px circle mask of a transmitter */
#define g_density_weight(i)      G16((u16)(0x127C + (i) * 2))   /* array; population per density level {0,1,10,100} */
#define g_transmitter_fees(i)    G32((u16)(0x128C + (i) * 4))   /* array; fee brackets {50000,100000,250000,500000} */
#define g_sammy_image_bonus      G16(0x12B0)   /* image bonus per Sammy category {8,4,6,6} */
#define g_trendy_player          G16(0x12D4)   /* player whose film 138 "Trendy" (Im Trend) ended this hour; in the ad phase an AI copies the current g_trends into g_ai_trends, the human gets message 0x15F; then cleared (24B8:0C4A) */
#define g_boss_replies(i)        G16((u16)(0x1492 + (i) * 2))   /* array; 15 x (page, option, text id) reply table */
#define g_boss_last_text         G16(0x14EC)   /* last boss text id */
#define g_boss_greeted           G16(0x14EE)   /* boss greeting already drawn */
#define g_ui_box_depth           G16(0x14F0)   /* depth of g_ui_box_stack */
#define g_player_color(i)        G8((u16)(0x14F1 + (i) * 1))   /* array; [player] frame colour on the setup screen */
#define g_ai_debug_step          G8(0x15EC)   /* >0: wait for a key after every AI program placement (debug) */
#define g_ai_no_program          G8(0x15ED)   /* set when the AI found no program to book for a contract */
#define g_ai_pot_weights(i)      G8((u16)(0x15EE + (i) * 1))   /* array; [1..6] base budget weights: gifts 5, films 28, news 14, production 18, transmitters 35, other 0 */
#define g_catch_busy             G16(0x15F5)   /* a catch scene is running (blocks re-entry while the room is left) */
#define g_ai_bid_pct             G8(0x15F8)   /* 4 percentages for AI transmitter bids */
#define g_ai_news_threshold(i)   G16((u16)(0x15FC + (i) * 2))   /* array; [strategy] news quality the AI aims for */
#define g_snd_digi               G16(0x1604)   /* digital sound available */
#define g_snd_sample_res         G16(0x1606)   /* resource id of the loaded room sample (0 none) */
#define g_snd_room_on            G16(0x1608)   /* last room passed to snd_room_music_start */
#define g_snd_room_off           G16(0x160A)   /* last room passed to snd_room_music_stop */
#define g_intro_bg               G32(0x160C)   /* intro scene background sprite bank */
#define g_intro_clip1            G32(0x1610)   /* LZW-decoded delta animation clip 1 of the scene */
#define g_intro_clip2            G32(0x1614)   /* clip 2 */
#define g_intro_clip3            G32(0x1618)   /* clip 3 */
#define g_intro_skip(i)          G16((u16)(0x161C + (i) * 2))   /* array; set when the intro is interrupted */
#define g_intro_scenes           G16(0x161E)   /* 14 x {bg res, clip1 res, clip2 res, clip3 res} */
#define g_intro_clip_slot        G16(0x168E)   /* next clip slot 1..3 */
#define g_mouse_rel_x            G16(0x16A4)   /* mouse motion since last poll (int 33h position - 200, recentred) */
#define g_mouse_rel_y            G16(0x16A6)   /* mouse motion since last poll */
#define g_vga_seg                G16(0x16A8)   /* video segment (0xA000) used by the screen blit asm */
#define g_heap_first             G32(0x1824)   /* Borland far heap first block */
#define g_heap_last              G32(0x1828)   /* Borland far heap last block */
#define g_heap_rover             G32(0x182C)   /* Borland far heap free-list rover */
#define g_script_offers_2        G8(0x196A)   /* screenplay agency offers 1..5 (9 bytes each) */
#define g_script_offers          G16(0x196A)   /* screenplay agency offers [9], 1..5: +0 screenplay, +2 taken, +3 days, +4 u32 price, +8 scenes */
#define g_ad_offers_2            G8(0x19A0)   /* ad agency offers 1..10 (7 bytes each) */
#define g_ad_offers              G16(0x19A0)   /* ad agency offers [7], 1..10: +0 ad id, +2 taken */
#define g_dealer                 G8(0x19ED)   /* movie dealer slots 1..10 (8 bytes each) */
#define g_film_offers            G16(0x19ED)   /* ?film dealer offers [8], 1..10: +0 id, returned to the pool at day end */
#define g_scripts_owned          G16(0x1A45)   /* [player*0x32] 10 owned screenplays of 5 bytes */
#define g_auction_2              G8(0x1B0D)   /* auction slots 1..10 (9 bytes each) */
#define g_auction                G16(0x1B0D)   /* auction lots [9], 1..10: +0 id, +2 u32 bid, +6 bidder, +7 lot code, +8 days listed */
#define g_player_ads             G16(0x1B70)   /* signed contracts per player [0x46] = 10 x 7: +0 ad id, +4 spot counter, +5 deadline day */
#define g_hour_audience(i)       G8((u16)(0x1D00 + (i) * 1))   /* array; % of the population watching per hour slot (8) */
#define g_transmitters_2         G16(0x1D09)   /* per-player transmitters (stride 0x1CE, slots 1..32 of 0xE bytes) */
#define g_transmitters           G32(0x1D09)   /* per player [0x1CE]: +0 u32 total fees, +4 u32 reach; slots 1..32 [0xE]: +0 u32 fee, +4 u32 reach, +8 x, +0xA y, +0xC state */
#define g_culture_missed_days    G16(0x2441)   /* days in a row movie 58 was not aired */
#define g_ai_tx_scan             G16(0x2443)   /* [player*16] transmitter search: +0 x, +2 y, +4 best x, +6 best y, +8 u32 best fee, +0xC u32 best new reach */
#define g_bag(i)                 G16((u16)(0x2483 + (i) * 2))   /* array; programs carried in the bag (10) */
#define g_program_score(i)       G16((u16)(0x24A1 + (i) * 2))   /* array; program attractiveness per player this hour */
#define g_movies                 G32(0x24A9)   /* far ptr to movie table [0x17], 1..200: +0 genre, +1 max topicality, +3 topicality, +4 blocks, +5 owner, +6 flags (bits0-4 audience, 0x20 available), +8 u32 price, +0xC picture, +0xF live day, +0x11 live hour */
#define g_rating                 G16(0x24AD)   /* per player [10]: +0 audience share %, +8 no-program flag */
#define g_ai_state(i)            G8((u16)(0x24E5 + (i) * 1))   /* array; [ai] current goal state 0..12 (-1 = choose) */
#define g_ai_step(i)             G16((u16)(0x24E7 + (i) * 2))   /* array; [ai] sub-step within a state (contract index, movie agency phase 0..3, planning hour 0..14) */
#define g_ai_cursor(i)           G16((u16)(0x24EB + (i) * 2))   /* array; [ai] hour cursor of ai_fill_program_slot / step of ai_rebuild_coverage_step */
#define g_ai_wait_start(i)       G16((u16)(0x24EF + (i) * 2))   /* array; [ai] tick when the AI started waiting at its door (waits 300 ticks) */
#define g_ai_visit_p1(i)         G8((u16)(0x24F3 + (i) * 1))   /* array; [ai*2] counter 0..9 for visits to player 1's rooms (state 5) */
#define g_ai_visit_rival(i)      G8((u16)(0x24F4 + (i) * 1))   /* array; [ai*2] counter 0..9 for visits to the other AI's rooms (state 6) */
#define g_avg_rating(i)          G16((u16)(0x24F7 + (i) * 2))   /* array; [player] yesterday's average rating % (ai_eval_yesterday 0) */
#define g_avg_quality(i)         G16((u16)(0x24FF + (i) * 2))   /* array; [player] yesterday's average program quality (ai_eval_yesterday 1), AI buying/selling threshold */
#define g_ai_cur                 G8(0x2507)   /* index (0/1) of the AI being processed */
#define g_ai_arrived(i)          G8((u16)(0x2508 + (i) * 1))   /* array; [ai] AI is at its target and may act */
#define g_ai_tx_busy(i)          G8((u16)(0x250A + (i) * 1))   /* array; [player] AI is searching/buying a transmitter (blocks the other AI) */
#define g_ai_script_quality(i)   G8((u16)(0x250E + (i) * 1))   /* array; [player] average quality of the owned screenplays */
#define g_ai_buy_plan            G16(0x2512)   /* [player*0x4D] 7 purchase plan entries of 0xB bytes (see rec 2512) */
#define g_series                 G32(0x2646)   /* far ptr to series table [0x39], 1..50: episodes [3](max, cur, flags), +0x21 genre, +0x22 episodes, +0x23 blocks, +0x24 owner, +0x25 flags, +0x2B u32 episode price, +0x2F picture, +0x31 next episode */
#define g_ai_studio(i)           G8((u16)(0x264A + (i) * 1))   /* array; [player*3] +0 studio index, +1 count, +2 production status 1..6 (ai_production_status) */
#define g_equip_quality          G8(0x268E)   /* scene/equipment quality per (type-15)*4 + level */
#define g_broadcast_phase        G8(0x2730)   /* phase within the hour: 0 none, 1 news, 2 program, 3 ads done */
#define g_news_nodes             G16(0x2731)   /* news chain nodes [n*0xA]: +0 4 successor nodes (9999 = end), +8 value, +9 delay in hours */
#define g_news_pool              G16(0x36DB)   /* today's news items, 20 x 0x1E (count g_news_pool_count) */
#define g_schedule_prev          G16(0x3939)   /* [player*0x5A] yesterday's schedule (copy of g_schedule) */
#define g_news_templates         G16(0x3AA1)   /* 50 news templates [t*0xE]: +0 5 parts (topic + 1000*mode: 1 = same phrase as previous), +0xA start node (+1000 when used), +0xD category */
#define g_news_slots_2           G8(0x3D6B)   /* per-player news broadcast slots 1..3 (stride 0x78, 0x1E-byte records) */
#define g_news_slots             G16(0x3D6B)   /* [player*0x78] news broadcast slots 1..3 of 0x1E bytes */
#define g_news_score(i)          G8((u16)(0x3F4B + (i) * 1))   /* array; news attractiveness per player this hour */
#define g_ads_2                  G8(0x3F4F)   /* static array of 0x18-byte ad (advertiser) records (1..50) */
#define g_ads                    G32(0x3F4F)   /* ad table [0x18]: +0 far name, +9 spots, +0xA viewer class, +0xB..0xD target groups, +0xF taken, +0x14 u32 penalty */
#define g_schedule               G16(0x4417)   /* schedule per player [0x5A]: u16 program[15] (movie, series+1000, 9999 = continued), +0x1E episode[15], +0x3C ad[15] (+3000 planned, +2000 aired) */
#define g_player_news            G8(0x457F)   /* news items per player [0x258] = 20 x 0x1E: +0x1C category, +0x1D price class */
#define g_news_inbox             G16(0x457F)   /* [player*0x258] 20 news items of 0x1E bytes */
#define g_ai_ad_plan             G8(0x4EDF)   /* [player*0x4D] 11 x 7-byte AI booking counters per contract (see rec 4EDF) */
#define g_actor_pool             G16(0x5013)   /* actor pool, groups [0xC0] x 16 actors [12]: +0 name, +2 u32 fee, +6.. attributes */
#define g_actor_offers           G16(0x5493)   /* today's 15 actors [12] */
#define g_hour_rank              G8(0x5553)   /* per player [8]: rank for each hour of the day */
#define g_news_topics            G32(0x5593)   /* 50 news topics [t*0x38]: +4*j far text of phrase j (1..10), +0x2C+j phrase value */
#define g_txt_index              G32(0x60BB)   /* far ptr to the loaded string index of a text resource */
#define g_messages               G16(0x60BF)   /* message list [0x1A]: text id, 5 dword args, day, hour */
#define g_frame_timer            G16(0x6141)   /* timer value of the last animation frame */
#define g_clock_acc              G32(0x6143)   /* accumulated timer ticks * speed of the day (seconds = acc / 50) */
#define g_drag_news              G8(0x6147)   /* copy (0x1E bytes) of the dragged news record */
#define g_news_drag              G16(0x6147)   /* news item being dragged (0x1E bytes) */
#define g_studios_2              G8(0x6165)   /* static array of 16-byte studio records (1..12) */
#define g_studios                G16(0x6165)   /* 12 studios [16 bytes]: +0 screenplay, +2 size, +3 owner, +4 episode, +6 u32 rent, +0xA finish day, +0xC finish hour, +0xE writing */
#define g_prop_prices            G32(0x6235)   /* props: 24 scene types x 4 quality levels, u32 price */
#define g_film_txt_cache         G16(0x63E5)   /* 49 x 10 bytes: film id, title, description */
#define g_series_txt_cache       G16(0x65CF)   /* 15 x 0x5A bytes: series id + 22 strings */
#define g_script_txt_cache       G16(0x6B15)   /* 10 x 0x42 bytes: screenplay id + 16 strings */
#define g_viewer_history         G32(0x6DA9)   /* per-player 7-day history of average viewers (stride 0x20, u32) */
#define g_ai_budget              G32(0x6E29)   /* AI player records [0x3C]: +0xC news, +0x10 studio, +0x14 transmitter budget */
#define g_archive                G16(0x6F19)   /* library per player [0x6C]: u16 id[genre 1..11][4], +0x60+genre u8 count */
#define g_news_stories           G16(0x70C9)   /* 7 running news stories [s*0x15]: +0 template, +2 last template, +4 node, +6 5 phrase variants, +0x12 countdown, +0x14 category */
#define g_scripts                G32(0x715C)   /* far ptr to screenplay/production table [0x1B3], 1..50: +4 status (99 done), +5 flags (bit0 series), +6 target program, +9 item lists per episode [0x23], +0x168 role requirements, +0x1A4 role groups */
#define g_genre_audience         G8(0x7160)   /* [genre*8 + hour 1..7] audience % per genre and hour of the evening */
#define g_day                    G16(0x71E4)   /* day counter (incremented in new_day) */
#define g_time_slot              G16(0x71EA)   /* hour index of the broadcast day */
#define g_minute                 G16(0x71EC)   /* game clock minute */
#define g_second                 G16(0x71EE)   /* game clock second */
#define g_ledger_today           G32(0x71F1)   /* today's finance ledger (0x54 bytes; [0] opening balance, then categories) */
#define g_ledger(i)              G32((u16)(0x71F5 + (i) * 4))   /* array; today's money per ledger category (u32 each) */
#define g_ai_period(i)           G8((u16)(0x7245 + (i) * 1))   /* array; [ai] 1..3 index of today's production budget history slot */
#define g_message_pos            G8(0x7247)   /* current index in the message list */
#define g_news_pool_count        G16(0x7248)   /* number of items in g_news_pool */
#define g_presents(i)            G16((u16)(0x724A + (i) * 2))   /* array; Betty gifts 1..10 of 8 bytes: +2 u32 price, +6 love points (x0.7 per use), +7 times given */
#define g_gifts                  G32(0x724C)   /* Betty gifts, 8 bytes each (price, love points, times given) */
#define g_game_speed             G16(0x72A2)   /* game seconds per timer tick unit (30) */
#define g_clock_timer            G16(0x72A4)   /* raw timer reading at the last clock update */
#define g_player_news_count(i)   G16((u16)(0x72A6 + (i) * 2))   /* array; news items per player */
#define g_ai_buy_last(i)         G16((u16)(0x72B2 + (i) * 2))   /* array; [player] index of the last purchase plan entry (-1 none) */
#define g_news_txt_cache(i)      G16((u16)(0x72BA + (i) * 2))   /* array; 24 x 6 bytes: news id, far text */
#define g_bag_count              G16(0x734A)   /* number of programs in the bag */
#define g_shoplists              G16(0x734C)   /* [player*0x4B] 5 shopping lists of 0xF bytes */
#define g_shoplist_count(i)      G16((u16)(0x7480 + (i) * 2))   /* array; [player] */
#define g_sammy_category(i)      G16((u16)(0x7488 + (i) * 2))   /* array; current Sammy category 1..4 (0 = none); words at +2p = player points */
#define g_sammy_day              G16(0x7490)   /* Sammy cycle day counter (award every 3 days) */
#define g_ledger_yesterday       G32(0x7492)   /* yesterday's finance ledger */
#define g_player_present(i)      G8((u16)(0x74E6 + (i) * 1))   /* array; per-player present carried (id, +100 if bought today) */
#define g_hour_viewers           G32(0x74EA)   /* per player [32]: viewers per hour */
#define g_ai_bids                G16(0x756A)   /* [player*0x42] AI auction bids, 6-byte entries: +0 lot, +2 u32 money reserved */
#define g_scripts_owned_count(i) G16((u16)(0x7672 + (i) * 2))   /* array; [player] */
#define g_last_hour              G16(0x767A)   /* last hour processed by the hourly tick */
#define g_player_ad_count(i)     G16((u16)(0x76A8 + (i) * 2))   /* array; number of contracts per player */
#define g_hour_newsviewers       G32(0x76B0)   /* [player*0x20 + hour*4] ?news-show viewers per hour today */
#define g_ai_bid_count(i)        G16((u16)(0x7730 + (i) * 2))   /* array; [player] entries in g_ai_bids */
#define g_ai_ad_req(i)           G16((u16)(0x7764 + (i) * 2))   /* array; [player*6] +0 genre mask (bit g-1) the AI's ads allow, +2 u32 minimum viewers of its ads */
#define g_now_title(i)           G8((u16)(0x777C + (i) * 1))   /* array; title of the program now on g_tv_channel */
#define g_now_episode(i)         G8((u16)(0x7812 + (i) * 1))   /* array; episode title of the series now on g_tv_channel */
#define g_ai_trends(i)           G16((u16)(0x78A8 + (i) * 2))   /* array; [ai*6] 3 genres the AI believes are trendy */
#define g_ai_odd_today(i)        G8((u16)(0x78B4 + (i) * 1))   /* array; [player] odd-length programs placed today (+4: tomorrow) */
#define g_ai_pending_gift(i)     G8((u16)(0x78BC + (i) * 1))   /* array; [ai] gift bought for Betty, not yet given */
#define g_hour_viewers_yesterday G32(0x78BE)   /* [player*0x20 + hour*4] program viewers per hour yesterday */
#define g_hour_newsviewers_yesterday G32(0x793E)   /* [player*0x20 + hour*4] ?news-show viewers per hour yesterday */
#define g_panel_atlas            G32(0x79BE)   /* res 10: bottom panel parts */
#define g_tvfam_atlas            G32(0x79C2)   /* res 15: TV-family sofa / shutter tiles */
#define g_planner_scroll(i)      G16((u16)(0x79C6 + (i) * 2))   /* array; planner hour offset (0 today, 7 tomorrow) */
#define g_selected_index         G16(0x79C8)   /* archive position / list index of the selection */
#define g_selected_genre         G16(0x79CA)   /* archive genre of the selected program */
#define g_map_player             G16(0x79CC)   /* player whose transmitters are shown on the map */
#define g_redraw                 G16(0x79CE)   /* request redraw / refresh flag */
#define g_genre_popup_open       G16(0x79D0)   /* genre count popup visible */
#define g_cursor_shape           G16(0x79D2)   /* cursor shape index (2 = carrying an item, 3 = busy) */
#define g_selected_series        G16(0x79D4)   /* series shown by ui_series_info */
#define g_selected_program       G16(0x79D6)   /* program picked up in the planner */
#define g_cur_studio             G16(0x79DE)   /* studio index of the visited studio */
#define g_shop_list              G16(0x79E0)   /* shopping list index being filled */
#define g_shop_category          G16(0x79E2)   /* actor role 1..7 or scene type >=16 shown in the shop */
#define g_shop_actor_scroll      G16(0x79E4)   /* actor list scroll (step 5) */
#define g_shop_slot_scroll       G16(0x79E6)   /* shopping list slot scroll (step 3) */
#define g_status_msg             G16(0x79EA)   /* text id of the pending status message (0x12A = caught in a rival's room) */
#define g_mouse_min_x            G16(0x79EC)   /* mouse limits */
#define g_mouse_min_y            G16(0x79EE)   /* mouse limits */
#define g_mouse_max_x            G16(0x79F0)   /* mouse limits */
#define g_mouse_max_y            G16(0x79F2)   /* mouse limits */
#define g_tvfam_shutter          G16(0x79F4)   /* number of shutter bars covering the view (0..6) */
#define g_tvfam_state            G16(0x79F6)   /* TV-family view: 0 off, 1 switching off, 2 switching on, 3 on */
#define g_chart_mode             G16(0x79F8)   /* statistics chart: 0 yesterday, 1 today, 2 last 7 days */
#define g_chart_player           G16(0x79FA)   /* player shown in the statistics chart */
#define g_chart_scale            G16(0x79FC)   /* viewer axis step of the chart */
#define g_tv_channel             G16(0x79FE)   /* channel shown on the office TV */
#define g_boss_cancelled_hours   G16(0x7A02)   /* hours of cancelled programs yesterday (boss complains) */
#define g_boss_missed_ads        G16(0x7A04)   /* commercials not run yesterday (boss complains) */
#define g_sammy_winner           G16(0x7A06)   /* player who won a Sammy today */
#define g_boss_talk_step         G16(0x7A0E)   /* step of the boss's morning speech */
#define g_tv_off                 G16(0x7A10)   /* TV cover position 0 (on) .. 30 (off) */
#define g_opt_music              G16(0x7A12)   /* options switch "Music" (0 = on; room jingles skipped otherwise) */
#define g_tv_program_quality     G16(0x7A14)   /* topicality of the human's current program (TV family mood) */
#define g_caught_figure          G16(0x7A16)   /* figure caught in the human's room (+1) */
#define g_tip_visible            G16(0x7A18)   /* a tooltip is on screen */
#define g_office_papers          G16(0x7A24)   /* papers piling up in the player's office (+1 per day, cleared by an event in 1DA8:2D3B) */
#define g_dealer_pose            G16(0x7A26)   /* res id 0xBC..0xBF of the film dealer's pose by hour */
#define g_estate_wealth          G16(0x7A2E)   /* estate agent's room decoration level 0..10 from total studio rent */
#define g_opt_tv_family          G16(0x7A30)   /* options switch "TV family" */
#define g_alien_mode             G16(0x7A34)   /* human figure drawn as alien (hotkey) */
#define g_room_owner             G16(0x7A36)   /* player owning the room being visited */
#define g_room_anim_dirty        G16(0x7A38)   /* set by room_anim_update when something was redrawn */
#define g_betty_letter(i)        G16((u16)(0x7A3A + (i) * 2))   /* array; per player: pending letter text id (0x29 love letter, 0x111 angry) */
#define g_player_letter          G16(0x7A3C)   /* pending letter text id per player (array at 7A3A+2*p, p=1..3), shown in the office (2B74_1E56) */
#define g_live29_left            G16(0x7A42)   /* remaining reruns of live movie 29 */
#define g_present_bld_only       G16(0x7A44)   /* present_frame mode: 1 = copy only the building view to VGA (set by game_frame around building frames), 0 = full screen; 9999 set by bld_render_floors on the day/night switch forces a full copy */
#define g_mouse_pending_dx       G16(0x7A48)   /* accumulated mouse/keyboard motion not yet applied */
#define g_mouse_pending_dy       G16(0x7A4A)   /* accumulated vertical motion not yet applied */
#define g_mouse_click            G16(0x7A4C)   /* latched button state (1 left, 2 right), consumed by game_frame */
#define g_elevator_speed         G16(0x7A4E)   /* cabin speed in px per tick (also scenery parallax divisor) */
#define g_frame_divisor          G16(0x7A50)   /* animation frame interval = 50 / this ticks */
#define g_estate_portrait        G16(0x7A52)   /* estate agent portrait index 0..3 from g_estate_wealth (also res id while loading) */
#define g_fired_player(i)        G16((u16)(0x7A54 + (i) * 2))   /* array; player to be thrown out */
#define g_npc_type               G16(0x7A5C)   /* intruder: 1/2 terrorist (from either Duban consulate), 3 bailiff, 4 moral guardian */
#define g_npc_target_player      G16(0x7A5E)   /* player the intruder goes after (8 = movie agency) */
#define g_now_title_stale        G16(0x7A60)   /* tvinfo_fetch_titles must reload the titles */
#define g_bomb_timer             G16(0x7A62)   /* frames since the bomb was planted */
#define g_npc_target_door        G16(0x7A64)   /* door the intruder acts at */
#define g_npc_target_floor       G16(0x7A66)   /* floor the intruder acts at */
#define g_door_block             G16(0x7A68)   /* door message: 1 bomb alarm, 2 crazy, 3 work in progress, 4 closed due to bereavement */
#define g_terror_retire_idx      G16(0x7A6A)   /* terrorist event retired once the intruder has left: word index from 0x79C6 into the intruder event flags 0x7A54+2*type (0x48 = terrorist 1 after consulate room 0x13 was bombed, 0x49 = terrorist 2 after room 0x12); 0 = none */
#define g_tvfam_frozen           G16(0x7A6C)   /* skip TV-family redraw */
#define g_chart_image            G16(0x7A6E)   /* statistics chart shows image % instead of viewers */
#define g_news_scroll            G16(0x7A70)   /* news list scroll offset */
#define g_news_category          G16(0x7A72)   /* news category shown in the inbox (1..3) */
#define g_madris_state           G16(0x7A74)   /* Madris: < 42 start countdown, 42 playing, 43 game over */
#define g_madris_score           G16(0x7A76)   /* Madris score (max 9999) */
#define g_madris_piece           G16(0x7A78)   /* falling piece 1..7 (0 = spawn next) */
#define g_madris_next            G16(0x7A7A)   /* next piece */
#define g_madris_rot             G16(0x7A7C)   /* rotation 0..3 */
#define g_madris_delay           G16(0x7A7E)   /* frames per fall step (level = 9 - delay; -1 every 10 lines, min 1) */
#define g_madris_lines           G16(0x7A80)   /* cleared lines */
#define g_madris_input           G16(0x7A82)   /* pending input: 1/2 rotate, 3 left, 4 right, 5 drop */
#define g_madris_hiscore         G16(0x7A84)   /* Madris high score */
#define g_master_key             G16(0x7A86)   /* bit set of players holding Betty's master key */
#define g_guardian_state         G16(0x7A88)   /* moral guardian (intruder 4) event: 0 not today, 1 waiting, 2 triggered when a station airs an X-rated film (flags2 bit3) before 21:00 (target = that player); same 0/1/2 states as the flags 0x7A56..0x7A5A */
#define g_boss_menu              G16(0x7A8E)   /* boss dialog menu level (-1 close, -2 done) */
#define g_boss_choice            G16(0x7A90)   /* selected option in the boss dialog */
#define g_clock_paused           G16(0x7A92)   /* game clock paused */
#define g_difficulty             G16(0x7A94)   /* 1 easy, 2 normal, 3 difficult */
#define g_news_placed            G16(0x7A96)   /* 1 after the human dropped a news item into a broadcast slot (0 on entering the news room / taking one out); leaving the news room then shows the room -3 newsroom cut-in */
#define g_save_error             G16(0x7A98)   /* disk write error during save */
#define g_pause_request          G16(0x7A9A)   /* set by the pause key: next click opens pause screen (room -5) */
#define g_live187_left           G16(0x7A9C)   /* remaining reruns of live movie 187 */
#define g_coverage_west          G32(0x7AD2)   /* covered population, west half of the map */
#define g_coverage_east          G32(0x7AD6)   /* covered population, east half of the map */
#define g_estate_base_rent       G32(0x7ADA)   /* reference total studio rent for g_estate_wealth */
#define g_credit                 G32(0x7ADE)   /* credit taken by the human player */
#define g_catch_fine             G32(0x7AE2)   /* money the human pays when caught in an AI's room (doubles, max 200000) */
#define g_catch_bounty(i)        G32((u16)(0x7AE6 + (i) * 4))   /* array; per figure: money paid when caught in another player's room (doubles, max 200000) */
#define g_status_amount          G32(0x7AF2)   /* money shown in the status message */
#define g_credit_2               G32(0x7AFE)   /* bank credit of the human player */
#define g_frame_tiles1           G32(0x7B0A)   /* window frame tileset (style 1) */
#define g_frame_tiles2           G32(0x7B0E)   /* window frame tileset (other styles) */
#define g_player_figure          G16(0x7B12)   /* figure index of the human player */
#define g_click                  G16(0x7B14)   /* click of the current frame (result of input_poll) */
#define g_cursor_drawn_x         G16(0x7B26)   /* x of the cursor currently on VGA (-1 = none) */
#define g_popup_col(i)           G16((u16)(0x7B28 + (i) * 2))   /* array; popup slot column (array) */
#define g_bld_sprites            G32(0x7B32)   /* res 11: building sprite bank (elevator cabin/door parts, street-level overlays at the sides, door tooltip icons; also used by 26A7) */
#define g_cursor_drawn_y         G16(0x7B36)   /* y of the cursor currently on VGA */
#define g_popup_row(i)           G16((u16)(0x7B38 + (i) * 2))   /* array; popup slot row (array) */
#define g_figures                G16(0x7B42)   /* walking figures [0x1E], 0..2 players, 5 intruder: +0 x, +2 floor, +4 target x, +6 target floor, +0xE walking, +0x10 entering, +0x12 target door, +0x1A look */
#define g_mouse_x                G16(0x7BF6)
#define g_mouse_y                G16(0x7BF8)
#define g_tvfam_bg               G32(0x7BFC)   /* TV-family background bitmap (sofa + seated viewers) */
#define g_res_cache              G32(0x7C00)   /* packed resource cache [6]: far ptr + u16 unpacked size */
#define g_game_mode              G16(0x7C78)   /* 1 title/new-game screen, 2 new game started, 3 running after load */
#define g_population_map         G32(0x7C7E)   /* far ptr, 168x108 map of 2-bit population density */
#define g_floors                 G8(0x7C82)   /* floor table, 14 floors x 0x7A: door x[10], +0x14 kind, +0x1E room type, +0x28 name text, +0x32 "of" text, +0x46 owner, +0x6E studio index */
#define g_tvfam_viewers          G32(0x832E)   /* 5 viewer records x 16 bytes {atlas, x, mood, state, counter, counter2, idle timer} */
#define g_tvfam_v2_state         G16(0x8356)   /* TV-family viewer 2 (rec 0x834E) +8 state: 1 watching, 2 bored (quality <= 7), 3 ?dozing (from 21:00 with quality <= 5) */
#define g_tvfam_v2_counter       G16(0x8358)   /* TV-family viewer 2 (rec 0x834E) +0xA animation counter (wraps at 100 / 135 / 285 by state) */
#define g_bld_side_tiles         G32(0x8382)   /* res 13: building side strips */
#define g_font_2                 G32(0x8386)   /* main font glyph bank (res 3) */
#define g_figure_sprite_cols_adj G16(0x838A)   /* ?1 when g_figure_sprite_cols < 8 (set at startup from the figure sprite bank size); added to the column factor by figure_sprite_stride(n, 1) */
#define g_room                   G16(0x838C)   /* current room / screen (0 = building, 1 office, 2 news, 3 boss, 4 library, 5 studio, 8 movie agency, 0x19 transmitter map, 0x1F planner, 0x3A save room) */
#define g_figure_sprite_cols     G16(0x838E)   /* ?figure sprite set width factor (from sprite bank size) */
#define g_text(i)                G32((u16)(0x8390 + (i) * 4))   /* array; far pointers to the 430 texts of the language */
#define g_room_stack(i)          G16((u16)(0x8A48 + (i) * 2))   /* array; rooms entered before the current one */
#define g_bld_top_floor          G16(0x8A5C)   /* floor shown at the top of the view (scroll/30+5) */
#define g_door_index             G8(0x8A60)   /* 13 floors x 4 doors x (floor, door) */
#define g_strpool                G32(0x8AC8)   /* string pool of ad names and news phrases */
#define g_cursor_shown           G16(0x8ACC)   /* cursor show counter: cursor_show ++, cursor_hide --; arrow drawn when 1 (with drag image at 0) */
#define g_madris_board(i)        G8((u16)(0x8AD4 + (i) * 1))   /* array; 10 x 20 board (row * 10 + col), nonzero = block colour */
#define g_roomframe_tiles        G32(0x8B9C)   /* room view frame tiles (res 4) */
#define g_band_tiles             G32(0x8BA0)   /* res 7: band filler tiles */
#define g_room_mode              G16(0x8BA4)   /* sub-screen / dialog state inside the current room */
#define g_font                   G16(0x8BA6)   /* font selector (line height 8 / 7) */
#define g_player                 G16(0x8BA8)   /* human player index (1..3) */
#define g_coverage_map           G32(0x8BAA)   /* far ptr, coverage map of the player being computed */
#define g_text_buf               G32(0x8BAE)   /* buffer holding the text resource */
#define g_save_names(i)          G32((u16)(0x8BB2 + (i) * 4))   /* array; 10 save-slot name buffers */
#define g_elevator_floor         G16(0x8BDA)   /* floor of the elevator cabin */
#define g_players                G8(0x8BDC)   /* player records 4 x 0x17 (index 0 unused, see rec 8BDC) */
#define g_player_name            G32(0x8BDC)   /* player records [p*0x17], p = 1..3: +0 far name */
#define g_player_station         G32(0x8BE0)   /* player +4: far station name (Mad-TV, Fun-TV, Sun-TV) */
#define g_player_money           G32(0x8BE4)   /* player +8: money (signed) */
#define g_player_betty_love      G16(0x8BE8)   /* ?player +0xC: Betty's affection (halved when her program is dropped, capped at image, < 20 takes the master key) */
#define g_player_news_agency     G8(0x8BED)   /* player +0x11..0x13: news agency 1..3 subscribed */
#define g_player_out             G8(0x8BF0)   /* player +0x14: player is out of the game */
#define g_player_image           G16(0x8BF1)   /* player +0x15: image / market share in %, all players sum to 100 */
#define g_popup_w(i)             G16((u16)(0x8C3A + (i) * 2))   /* array; popup slot width in cells (array) */
#define g_popup_h(i)             G16((u16)(0x8C44 + (i) * 2))   /* array; popup slot height in cells (array) */
#define g_bld_scroll             G16(0x8C4E)   /* building view vertical scroll (0..321) */
#define g_tv_family_gfx          G32(0x8C50)   /* TV family sprite buffer */
#define g_elevator_calls         G16(0x8C54)   /* number of entries in g_elevator_queue */
#define g_bld_tiles              G32(0x8C56)   /* res 6: building tiles (doors of 3 types open/closed) */
#define g_tv_family_gfx_pos      G16(0x8C5A)   /* fill position in the TV family buffer */
#define g_ui_atlas               G32(0x8C5C)   /* res 2: cursor shapes, band end caps */
#define g_popup_used(i)          G16((u16)(0x8C60 + (i) * 2))   /* array; popup slot in use (array) */
#define g_bld_full_redraw        G16(0x8C6A)   /* force full building redraw */
#define g_elevator_queue(i)      G16((u16)(0x8C6C + (i) * 2))   /* array; list of figures waiting for the elevator (length g_elevator_calls) */
#define g_panel_sprites          G32(0x8C78)   /* res 9: the two halves of the bottom panel */
#define g_res_present            G16(0x8C7C)   /* count + list of resource ids present */
#define g_bg_half_bmp            G32(0x8E7E)   /* far ptr to the room background half just loaded by room_draw_background (res n-50 / n), freed right after drawing; zeroed by the room setup */
#define g_screen_buf             G32(0x8E82)   /* back buffer for drawing */
#define g_status_win_bmp         G32(0x8E8A)   /* 136x56 channel status window bitmap (status_window_draw) */
#define g_room_stack_top         G16(0x8E8E)   /* room stack depth */
#define g_cursor_comp            G32(0x8E90)   /* background + cursor composite put on VGA */
#define g_cursor_sprites(i)      G32((u16)(0x8E94 + (i) * 4))   /* array; far ptrs to the cursor shape sprites (by g_cursor_shape) */
#define g_res_drives             G16(0x8EB0)   /* resource drive info {count, current} */
#define g_panel_bmp              G32(0x8ED2)   /* bottom panel bitmap 320x80 */
#define g_cursor_tmp             G32(0x8ED6)   /* scratch background copy for cursor moves */
#define g_building_buf           G32(0x8EDA)   /* 208x150 building bitmap */
#define g_cursor_bg              G32(0x8EDE)   /* saved VGA background under the cursor */
#define g_popup_bmp(i)           G32((u16)(0x8EE2 + (i) * 4))   /* array; popup save buffers, 4 bytes per slot; slot 0 = current window/tooltip bitmap */
#define g_txt_res                G16(0x8EF8)   /* resource id of the text resource being cached */
#define g_drag_news_new          G16(0x8EFA)   /* dragged news comes from the pool and must be paid */
#define g_drag_ad                G16(0x8EFC)   /* dragged ad id */
#define g_hotspot                G16(0x8F03)   /* hotspot id under cursor at the last click (ui_hotspot_at_cursor) */
#define g_drag_aux_b             G8(0x8F05)   /* low byte of g_drag_aux, stored into byte fields (owned_script.aux, studio.episode / episode_last) */
#define g_drag_aux               G16(0x8F05)   /* extra value of the dragged item (episodes / byte) */
#define g_drag_film              G16(0x8F09)   /* dragged film id (<1000 film, >=1000 series+1000, +10000 = bought now) */
#define g_drag_item_b            G8(0x8F0B)   /* low byte of g_drag_item, stored into byte fields (shopping-list slot, g_player_present) */
#define g_drag_item              G16(0x8F0B)   /* dragged generic item (script, present, actor, prop, transmitter...) */
#define g_tx_bids(i)             G32((u16)(0x8F0D + (i) * 4))   /* array; 2 opponents' bids for the transmitter being sold */
#define g_estate_studio          G16(0x8F15)   /* studio selected at the estate agent */
#define g_swap_script            G16(0x8F17)   /* studio_swap_script: script that was in the studio, put into the hand after the dropped one is inserted (0 = studio was empty; its episode goes via 0x8F07) */
#define g_cur_player_rec         G32(0x8F19)   /* far ptr to players[g_player] */
#define g_drag_draw_x            G16(0x8F1E)   /* top-left x where the drag image is drawn */
#define g_drag_draw_y            G16(0x8F20)   /* top-left y where the drag image is drawn */
#define g_cursor_shape_stack(i)  G16((u16)(0x8F22 + (i) * 2))   /* array; stack of saved cursor shapes */
#define g_madris_x               G16(0x8F2C)   /* piece column */
#define g_madris_y               G16(0x8F2E)   /* piece row */
#define g_madris_drawn_y         G16(0x8F30)   /* row of the piece stamped last frame */
#define g_madris_drawn_piece     G16(0x8F32)   /* piece stamped last frame */
#define g_madris_drawn_rot       G16(0x8F34)   /* rotation stamped last frame */
#define g_glyph_blit             G32(0x8F36)   /* 14-byte param block for the 377A glyph blitter {tile, xoff, dst, col, row} */
#define g_tile_fixed_x           G16(0x8F44)   /* x for tile_draw_at_y */
#define g_tile_fixed_y           G16(0x8F46)   /* y for tile_draw_at_x */
#define g_tile_fixed_n           G16(0x8F48)   /* tile for tile_draw_fixed */
#define g_tile_target            G32(0x8F4A)   /* destination bitmap of the atlas tile helpers */
#define g_tile_atlas             G32(0x8F4E)   /* current tile/glyph atlas */
#define g_frame_target           G32(0x8F52)   /* bitmap ui_frame draws into */
#define g_frame_tileset          G32(0x8F56)   /* tileset used by ui_frame */
#define g_tip_viewers            G32(0x8F5E)   /* viewer number for the tooltip: viewers of the hovered schedule/chart hour (g_hour_viewers[_yesterday]), transmitter reach*2464; screenplay card reuses it for the block count */
#define g_tip_viewers2           G32(0x8F62)   /* second number for the tooltip: share % of the hovered chart hour (from 0x1C88, sign-extended), reach before the transmitter (gain = g_tip_viewers - this), min viewers of the ad running now (rating overview, 0 = none) */
#define g_tip_j                  G16(0x8F66)   /* 14E1 second counter/index: inner loop (column, door), contract index of the hovered ad, start hour of the programme block under the cursor / on air (channel_status_draw, tvinfo_fetch_titles); role card: episode byte +0xA multiplying the actor fee */
#define g_tip_col                G16(0x8F68)   /* tooltip window column */
#define g_tip_pos                G32(0x8F68)   /* g_tip_col (low) and g_tip_row (high) set together as one dword (MK32(row, col)) */
#define g_tip_row                G16(0x8F6A)   /* tooltip window row */
#define g_tip_text(i)            G32((u16)(0x8F6C + (i) * 4))   /* array; tooltip text slots (far ptrs, 4 bytes each, referenced by layout digits) */
#define g_tip_text1              G32(0x8F70)   /* tooltip text slot 1 (= g_tip_text[1]): description/genre/viewers string etc.; ad_genre_list part 1 */
#define g_tip_text2              G32(0x8F74)   /* tooltip text slot 2 (= g_tip_text[2]): genre/client/episode-count string etc.; ad_genre_list part 2 */
#define g_tip_text3              G32(0x8F78)   /* tooltip text slot 3 (= g_tip_text[3]): critics/fee/penalty or script title string; ad_genre_list part 3 */
#define g_tip_text4              G32(0x8F7C)   /* tooltip text slot 4 (= g_tip_text[4]): episodes/spots/deadline string; ad_genre_list result "A, B or C" */
#define g_tip_text5              G32(0x8F80)   /* tooltip text slot 5 (= g_tip_text[5]): X-rated flag text (series) / live date line (film) */
#define g_tip_text6              G32(0x8F84)   /* tooltip text slot 6 (= g_tip_text[6]): film length "n blocks" / live part x of y */
#define g_tip_plain              G32(0x8F88)   /* template/text for kind 5 tooltips */
#define g_tip_block              G16(0x8F8C)   /* block number (hour - start + 1) of the programme in the hovered chart hour (schedule-slot tooltip "block x of y"); reused as gift index (kind 16) and role index (kind 11) */
#define g_tip_episode            G16(0x8F8E)   /* episode number of the series in the hovered chart hour; reused as prop quality icon (kind 12: contract 1->3, 2->1, 3->2) */
#define g_tip_prev_block         G16(0x8F90)   /* g_tip_block of the last drawn slot tooltip; a change forces a redraw */
#define g_tip_prev_episode       G16(0x8F92)   /* g_tip_episode of the last drawn slot tooltip; a change forces a redraw */
#define g_tip_genre              G16(0x8F94)   /* genre of the film/series in the schedule-slot tooltip (kinds 14/15) */
#define g_tip_shop_script        G16(0x8F96)   /* shop: script of the selected shopping list (shoplists(g_player, g_shop_list).script) for role/prop/actor tooltips */
#define g_tip_shop_episode       G16(0x8F98)   /* shop: episode of the selected shopping list (shoplists(...).episode) */
#define g_tip_cursor_hidden      G16(0x8F9A)   /* tooltip_show hid the cursor */
#define g_tip_replace            G16(0x8F9C)   /* force present before showing a new tooltip */
#define g_tip_k                  G16(0x8F9E)   /* 14E1 scratch index/id: planner hour, shelf/bag/library index, studio on the estate board, series id (kind 10), actor id, role group, genre on air (kind 18), programme on air (tvinfo_fetch_titles), next text slot in production cards */
#define g_tip_i                  G16(0x8FA0)   /* 14E1 loop counter i: chart hour 1..7, dealer shelf, auction/supermarket row, floor, shop slot, owned-script index, episode number (kinds 9/10), contract index */
#define g_tip_h                  G16(0x8FA6)   /* tooltip height in rows */
#define g_tip_contract           G16(0x8FA8)   /* contract slot index for ad tooltips (-1 none) */
#define g_tip_w                  G16(0x8FAA)   /* tooltip width in cells */
#define g_tmp_b                  G16(0x8FAC)   /* scratch global */
#define g_tmp_a                  G16(0x8FAE)   /* scratch global */
#define g_initdata               G32(0x8FB0)   /* init-data blob (resource 0xD0) while loading */
#define g_image_delta(i)         G16((u16)(0x8FB4 + (i) * 2))   /* array; int[4] image changes passed to image_apply_deltas */
#define g_rank_share(i)          G16((u16)(0x8FBC + (i) * 2))   /* array; int[4] shares being ranked */
#define g_rank_order(i)          G16((u16)(0x8FC4 + (i) * 2))   /* array; int[3] players by rank */
#define g_save_buf               G32(0x8FCA)   /* savegame buffer (40000 bytes) */
#define g_save_pos               G16(0x8FCE)   /* read/write position in g_save_buf */
#define g_save_mode              G16(0x8FD0)   /* 1 load, 2 save (savegame_io direction) */
#define g_genre_popup_bg         G16(0x8FE0)   /* saved background handle of ui_genre_count_popup */
#define g_lib_genre_count(i)     G16((u16)(0x8FE2 + (i) * 2))   /* array; [genre 1..11] library counts (5 = full) for ui_genre_count_popup */
#define g_tetris_bg              G16(0x8FFA)   /* saved background handle of tetris_draw */
#define g_ui_popup_bmp           G32(0x8FFC)   /* ui_disk_prompt work bitmap */
#define g_ui_popup_bg            G32(0x9000)   /* ui_disk_prompt saved background */
#define g_lang_prompt_bg         G32(0x9004)   /* lang_prompt_box saved background */
#define g_ui_box_stack(i)        G16((u16)(0x9008 + (i) * 2))   /* array; saved-background handles of nested ui_box calls */
#define g_tx_dialog_bg           G16(0x9012)   /* saved background of transmitter_sale_dialog */
#define g_snd_drv                G32(0x9014)   /* paragraph-aligned driver code */
#define g_snd_sample             G32(0x9018)   /* paragraph-aligned sample data */
#define g_snd_detect             G32(0x901C)   /* paragraph-aligned detector code */
#define g_snd_sample_mem         G32(0x9020)   /* sample allocation */
#define g_snd_drv_mem            G32(0x9024)   /* driver allocation */
#define g_snd_detect_mem         G32(0x9028)   /* detector allocation */
#define g_timer_block            G16(0x902C)   /* ? tick block handed to the sound driver (counter at +8 = 0x9034) */
#define g_timer_ticks            G16(0x9034)   /* timer tick counter (~50 Hz) */
#define g_intro_passes           G16(0x904C)   /* passes over all clips of the scene */
#define g_intro_row              G16(0x904E)   /* screen y of the intro frame */
#define g_intro_scene_done       G16(0x9050)   /* current scene finished */
#define g_intro_scene            G16(0x9052)   /* current intro scene 1..14 */
#define g_intro_frame            G16(0x9054)   /* frame index in the current clip (0 = start next clip) */
#define g_intro_frame_bmp        G32(0x9056)   /* 208x144 intro frame bitmap */
#define g_intro_clip             G32(0x905A)   /* current clip (byte 0 = frame count) */
#define g_intro_hold             G16(0x905E)   /* hold counter (1005 -> 1000 = 5 frame pause) */
#define g_intro_frame_ticks      G16(0x9060)   /* timer ticks per intro frame */
#define g_intro_anim_ptr         G32(0x9062)   /* read pointer into the current delta stream */
#define g_intro_clip_count       G16(0x9066)   /* number of clips in the scene (1..3) */
#define g_huff_dst               G32(0xA438)   /* current pixel write pointer */
#define g_huff_new_sprite        G16(0xA43C)   /* next byte starts a new sprite */
#define g_huff_child0            G32(0xA43E)   /* child-0 table, biased by -0x202 so it is indexed by node id */
#define g_huff_pixels            G16(0xA442)   /* pixels written (sum of w*h) */
#define g_huff_size_lo           G16(0xA444)   /* low word of the output size */
#define g_huff_h                 G16(0xA446)   /* current sprite height */
#define g_huff_bitcnt            G16(0xA448)   /* bits left in g_huff_bitbuf */
#define g_huff_ctx_root(i)       G16((u16)(0xA44A + (i) * 2))   /* array; 256 roots of the order-1 context trees (by previous byte) */
#define g_huff_w                 G16(0xA64A)   /* current sprite width in px */
#define g_huff_y                 G16(0xA64C)   /* current row */
#define g_huff_escape_done       G16(0xA650)   /* escape leaf of the current tree already assigned */
#define g_huff_remaining         G32(0xA652)   /* bytes still to decode (BE u32 from stream) */
#define g_huff_src_pos           G16(0xA656)   /* read position in g_huff_src */
#define g_huff_nodes             G16(0xA658)   /* node table size (BE u16 from stream) */
#define g_huff_bitbuf            G16(0xA65A)   /* current bit buffer */
#define g_huff_out               G32(0xA65C)   /* output sprite bank */
#define g_huff_wcols             G16(0xA660)   /* current sprite width in 8px columns */
#define g_huff_count             G16(0xA662)   /* number of sprites in the bank */
#define g_huff_src               G32(0xA664)   /* compressed bitstream (after the w,h table) */
#define g_huff_child0_mem        G32(0xA668)   /* unbiased child-0 allocation (freed) */
#define g_huff_out_ofs           G16(0xA66C)   /* offset of the next sprite's pixel data in the bank */
#define g_huff_hdr               G32(0xA66E)   /* start of compressed bank (u8 count, (w,h) pairs) */
#define g_huff_x                 G16(0xA672)   /* x in the current row */
#define g_huff_child1_mem        G32(0xA674)   /* unbiased child-1 allocation (freed) */
#define g_huff_next_node         G16(0xA678)   /* next internal node id (from 0x101) */
#define g_huff_child1            G32(0xA67E)   /* child-1 table, biased by -0x202 */
#define g_huff_root              G16(0xA682)   /* root of the order-0 tree */
#define g_huff_sprite            G16(0xA684)   /* index of the sprite being written */
#define g_res_index              G32(0xA686)   /* resource index: 600 x {u16 handle, u16 size, u32 offset} */
#define g_res_open               G16(0xA68A)   /* rsc files are open */
#define g_res_nfiles             G16(0xA68C)   /* number of open rsc files */
#define g_res_handles(i)         G16((u16)(0xA68E + (i) * 2))   /* array; handles of the open rsc files */
#define g_transmitter_fees_hi(i) G16((u16)(0x128E + (i) * 4))   /* high word of g_transmitter_fees(i) */
#define g_transmitter_fees_lo(i) G16((u16)(0x128C + (i) * 4))   /* low word of g_transmitter_fees(i) */
#define g_catch_bounty_hi(i)     G16((u16)(0x7AE8 + (i) * 4))   /* high word of g_catch_bounty(i) */
#define g_catch_bounty_lo(i)     G16((u16)(0x7AE6 + (i) * 4))   /* low word of g_catch_bounty(i) */
#define g_text_hi(i)             G16((u16)(0x8392 + (i) * 4))   /* high word of g_text(i) */
#define g_text_lo(i)             G16((u16)(0x8390 + (i) * 4))   /* low word of g_text(i) */
#define g_save_names_hi(i)       G16((u16)(0x8BB4 + (i) * 4))   /* high word of g_save_names(i) */
#define g_save_names_lo(i)       G16((u16)(0x8BB2 + (i) * 4))   /* low word of g_save_names(i) */
#define g_cursor_sprites_hi(i)   G16((u16)(0x8E96 + (i) * 4))   /* high word of g_cursor_sprites(i) */
#define g_cursor_sprites_lo(i)   G16((u16)(0x8E94 + (i) * 4))   /* low word of g_cursor_sprites(i) */
#define g_popup_bmp_hi(i)        G16((u16)(0x8EE4 + (i) * 4))   /* high word of g_popup_bmp(i) */
#define g_popup_bmp_lo(i)        G16((u16)(0x8EE2 + (i) * 4))   /* low word of g_popup_bmp(i) */
#define g_tx_bids_hi(i)          G16((u16)(0x8F0F + (i) * 4))   /* high word of g_tx_bids(i) */
#define g_tip_text_hi(i)         G16((u16)(0x8F6E + (i) * 4))   /* high word of g_tip_text(i) */
#define g_tip_text_lo(i)         G16((u16)(0x8F6C + (i) * 4))   /* low word of g_tip_text(i) */
#define g_building_bg_hi         G16(0x00F6)   /* high word of g_building_bg */
#define g_building_bg_lo         G16(0x00F4)   /* low word of g_building_bg */
#define g_figure_sprites_hi      G16(0x00FA)   /* high word of g_figure_sprites */
#define g_figure_sprites_lo      G16(0x00F8)   /* low word of g_figure_sprites */
#define g_room_gfx_hi            G16(0x0102)   /* high word of g_room_gfx */
#define g_room_gfx_lo            G16(0x0100)   /* low word of g_room_gfx */
#define g_room_gfx1_hi           G16(0x0106)   /* high word of g_room_gfx1 */
#define g_room_gfx1_lo           G16(0x0104)   /* low word of g_room_gfx1 */
#define g_room_gfx2_hi           G16(0x010A)   /* high word of g_room_gfx2 */
#define g_room_gfx2_lo           G16(0x0108)   /* low word of g_room_gfx2 */
#define g_room_gfx3_hi           G16(0x010E)   /* high word of g_room_gfx3 */
#define g_room_gfx3_lo           G16(0x010C)   /* low word of g_room_gfx3 */
#define g_actor_txt_hi           G16(0x0112)   /* high word of g_actor_txt */
#define g_actor_txt_lo           G16(0x0110)   /* low word of g_actor_txt */
#define g_room_gfx5_hi           G16(0x0116)   /* high word of g_room_gfx5 */
#define g_room_gfx5_lo           G16(0x0114)   /* low word of g_room_gfx5 */
#define g_room_gfx6_hi           G16(0x011A)   /* high word of g_room_gfx6 */
#define g_room_gfx6_lo           G16(0x0118)   /* low word of g_room_gfx6 */
#define g_room_gfx7_hi           G16(0x011E)   /* high word of g_room_gfx7 */
#define g_room_gfx7_lo           G16(0x011C)   /* low word of g_room_gfx7 */
#define g_room_gfx8_hi           G16(0x0122)   /* high word of g_room_gfx8 */
#define g_room_gfx8_lo           G16(0x0120)   /* low word of g_room_gfx8 */
#define g_window_buf_hi          G16(0x0126)   /* high word of g_window_buf */
#define g_window_buf_lo          G16(0x0124)   /* low word of g_window_buf */
#define g_drag_image_hi          G16(0x0392)   /* high word of g_drag_image */
#define g_drag_image_lo          G16(0x0390)   /* low word of g_drag_image */
#define g_sa_fly1_bg_hi          G16(0x0D46)   /* high word of g_sa_fly1_bg */
#define g_sa_fly1_bg_lo          G16(0x0D44)   /* low word of g_sa_fly1_bg */
#define g_sa_fly2_bg_hi          G16(0x0D4E)   /* high word of g_sa_fly2_bg */
#define g_sa_fly2_bg_lo          G16(0x0D4C)   /* low word of g_sa_fly2_bg */
#define g_intro_bg_hi            G16(0x160E)   /* high word of g_intro_bg */
#define g_intro_bg_lo            G16(0x160C)   /* low word of g_intro_bg */
#define g_intro_clip1_hi         G16(0x1612)   /* high word of g_intro_clip1 */
#define g_intro_clip1_lo         G16(0x1610)   /* low word of g_intro_clip1 */
#define g_intro_clip2_hi         G16(0x1616)   /* high word of g_intro_clip2 */
#define g_intro_clip2_lo         G16(0x1614)   /* low word of g_intro_clip2 */
#define g_intro_clip3_hi         G16(0x161A)   /* high word of g_intro_clip3 */
#define g_intro_clip3_lo         G16(0x1618)   /* low word of g_intro_clip3 */
#define g_heap_first_hi          G16(0x1826)   /* high word of g_heap_first */
#define g_heap_first_lo          G16(0x1824)   /* low word of g_heap_first */
#define g_heap_last_hi           G16(0x182A)   /* high word of g_heap_last */
#define g_heap_last_lo           G16(0x1828)   /* low word of g_heap_last */
#define g_heap_rover_hi          G16(0x182E)   /* high word of g_heap_rover */
#define g_heap_rover_lo          G16(0x182C)   /* low word of g_heap_rover */
#define g_movies_hi              G16(0x24AB)   /* high word of g_movies */
#define g_movies_lo              G16(0x24A9)   /* low word of g_movies */
#define g_series_hi              G16(0x2648)   /* high word of g_series */
#define g_series_lo              G16(0x2646)   /* low word of g_series */
#define g_txt_index_hi           G16(0x60BD)   /* high word of g_txt_index */
#define g_txt_index_lo           G16(0x60BB)   /* low word of g_txt_index */
#define g_clock_acc_lo           G16(0x6143)   /* low word of g_clock_acc */
#define g_scripts_hi             G16(0x715E)   /* high word of g_scripts */
#define g_scripts_lo             G16(0x715C)   /* low word of g_scripts */
#define g_tvfam_atlas_hi         G16(0x79C4)   /* high word of g_tvfam_atlas */
#define g_tvfam_atlas_lo         G16(0x79C2)   /* low word of g_tvfam_atlas */
#define g_credit_hi              G16(0x7AE0)   /* high word of g_credit */
#define g_credit_lo              G16(0x7ADE)   /* low word of g_credit */
#define g_catch_fine_hi          G16(0x7AE4)   /* high word of g_catch_fine */
#define g_catch_fine_lo          G16(0x7AE2)   /* low word of g_catch_fine */
#define g_status_amount_hi       G16(0x7AF4)   /* high word of g_status_amount */
#define g_bld_sprites_hi         G16(0x7B34)   /* high word of g_bld_sprites */
#define g_bld_sprites_lo         G16(0x7B32)   /* low word of g_bld_sprites */
#define g_tvfam_bg_hi            G16(0x7BFE)   /* high word of g_tvfam_bg */
#define g_tvfam_bg_lo            G16(0x7BFC)   /* low word of g_tvfam_bg */
#define g_bld_side_tiles_hi      G16(0x8384)   /* high word of g_bld_side_tiles */
#define g_bld_side_tiles_lo      G16(0x8382)   /* low word of g_bld_side_tiles */
#define g_strpool_hi             G16(0x8ACA)   /* high word of g_strpool */
#define g_strpool_lo             G16(0x8AC8)   /* low word of g_strpool */
#define g_band_tiles_hi          G16(0x8BA2)   /* high word of g_band_tiles */
#define g_band_tiles_lo          G16(0x8BA0)   /* low word of g_band_tiles */
#define g_text_buf_hi            G16(0x8BB0)   /* high word of g_text_buf */
#define g_text_buf_lo            G16(0x8BAE)   /* low word of g_text_buf */
#define g_tv_family_gfx_hi       G16(0x8C52)   /* high word of g_tv_family_gfx */
#define g_tv_family_gfx_lo       G16(0x8C50)   /* low word of g_tv_family_gfx */
#define g_bld_tiles_hi           G16(0x8C58)   /* high word of g_bld_tiles */
#define g_bld_tiles_lo           G16(0x8C56)   /* low word of g_bld_tiles */
#define g_ui_atlas_hi            G16(0x8C5E)   /* high word of g_ui_atlas */
#define g_ui_atlas_lo            G16(0x8C5C)   /* low word of g_ui_atlas */
#define g_panel_sprites_hi       G16(0x8C7A)   /* high word of g_panel_sprites */
#define g_panel_sprites_lo       G16(0x8C78)   /* low word of g_panel_sprites */
#define g_bg_half_bmp_hi         G16(0x8E80)   /* high word of g_bg_half_bmp */
#define g_bg_half_bmp_lo         G16(0x8E7E)   /* low word of g_bg_half_bmp */
#define g_screen_buf_hi          G16(0x8E84)   /* high word of g_screen_buf */
#define g_screen_buf_lo          G16(0x8E82)   /* low word of g_screen_buf */
#define g_status_win_bmp_hi      G16(0x8E8C)   /* high word of g_status_win_bmp */
#define g_status_win_bmp_lo      G16(0x8E8A)   /* low word of g_status_win_bmp */
#define g_cursor_comp_hi         G16(0x8E92)   /* high word of g_cursor_comp */
#define g_cursor_comp_lo         G16(0x8E90)   /* low word of g_cursor_comp */
#define g_panel_bmp_hi           G16(0x8ED4)   /* high word of g_panel_bmp */
#define g_panel_bmp_lo           G16(0x8ED2)   /* low word of g_panel_bmp */
#define g_cursor_tmp_hi          G16(0x8ED8)   /* high word of g_cursor_tmp */
#define g_cursor_tmp_lo          G16(0x8ED6)   /* low word of g_cursor_tmp */
#define g_building_buf_hi        G16(0x8EDC)   /* high word of g_building_buf */
#define g_building_buf_lo        G16(0x8EDA)   /* low word of g_building_buf */
#define g_cursor_bg_hi           G16(0x8EE0)   /* high word of g_cursor_bg */
#define g_cursor_bg_lo           G16(0x8EDE)   /* low word of g_cursor_bg */
#define g_tile_atlas_hi          G16(0x8F50)   /* high word of g_tile_atlas */
#define g_tile_atlas_lo          G16(0x8F4E)   /* low word of g_tile_atlas */
#define g_tip_viewers_lo         G16(0x8F5E)   /* low word of g_tip_viewers */
#define g_tip_viewers2_hi        G16(0x8F64)   /* high word of g_tip_viewers2 */
#define g_tip_viewers2_lo        G16(0x8F62)   /* low word of g_tip_viewers2 */
#define g_tip_text1_hi           G16(0x8F72)   /* high word of g_tip_text1 */
#define g_tip_text1_lo           G16(0x8F70)   /* low word of g_tip_text1 */
#define g_tip_text2_hi           G16(0x8F76)   /* high word of g_tip_text2 */
#define g_tip_text2_lo           G16(0x8F74)   /* low word of g_tip_text2 */
#define g_tip_text3_hi           G16(0x8F7A)   /* high word of g_tip_text3 */
#define g_tip_text3_lo           G16(0x8F78)   /* low word of g_tip_text3 */
#define g_tip_text4_hi           G16(0x8F7E)   /* high word of g_tip_text4 */
#define g_tip_text4_lo           G16(0x8F7C)   /* low word of g_tip_text4 */
#define g_tip_text5_hi           G16(0x8F82)   /* high word of g_tip_text5 */
#define g_tip_text5_lo           G16(0x8F80)   /* low word of g_tip_text5 */
#define g_tip_text6_hi           G16(0x8F86)   /* high word of g_tip_text6 */
#define g_tip_text6_lo           G16(0x8F84)   /* low word of g_tip_text6 */
#define g_tip_plain_hi           G16(0x8F8A)   /* high word of g_tip_plain */
#define g_tip_plain_lo           G16(0x8F88)   /* low word of g_tip_plain */
#define g_initdata_hi            G16(0x8FB2)   /* high word of g_initdata */
#define g_initdata_lo            G16(0x8FB0)   /* low word of g_initdata */
#define g_save_buf_hi            G16(0x8FCC)   /* high word of g_save_buf */
#define g_save_buf_lo            G16(0x8FCA)   /* low word of g_save_buf */
#define g_ui_popup_bmp_hi        G16(0x8FFE)   /* high word of g_ui_popup_bmp */
#define g_ui_popup_bmp_lo        G16(0x8FFC)   /* low word of g_ui_popup_bmp */
#define g_ui_popup_bg_hi         G16(0x9002)   /* high word of g_ui_popup_bg */
#define g_ui_popup_bg_lo         G16(0x9000)   /* low word of g_ui_popup_bg */
#define g_lang_prompt_bg_hi      G16(0x9006)   /* high word of g_lang_prompt_bg */
#define g_lang_prompt_bg_lo      G16(0x9004)   /* low word of g_lang_prompt_bg */
#define g_snd_drv_hi             G16(0x9016)   /* high word of g_snd_drv */
#define g_snd_drv_lo             G16(0x9014)   /* low word of g_snd_drv */
#define g_snd_detect_hi          G16(0x901E)   /* high word of g_snd_detect */
#define g_snd_detect_lo          G16(0x901C)   /* low word of g_snd_detect */
#define g_snd_sample_mem_hi      G16(0x9022)   /* high word of g_snd_sample_mem */
#define g_snd_sample_mem_lo      G16(0x9020)   /* low word of g_snd_sample_mem */
#define g_snd_drv_mem_hi         G16(0x9026)   /* high word of g_snd_drv_mem */
#define g_snd_drv_mem_lo         G16(0x9024)   /* low word of g_snd_drv_mem */
#define g_snd_detect_mem_hi      G16(0x902A)   /* high word of g_snd_detect_mem */
#define g_snd_detect_mem_lo      G16(0x9028)   /* low word of g_snd_detect_mem */
#define g_intro_frame_bmp_hi     G16(0x9058)   /* high word of g_intro_frame_bmp */
#define g_intro_frame_bmp_lo     G16(0x9056)   /* low word of g_intro_frame_bmp */
#define g_huff_child0_hi         G16(0xA440)   /* high word of g_huff_child0 */
#define g_huff_child0_lo         G16(0xA43E)   /* low word of g_huff_child0 */
#define g_huff_remaining_lo      G16(0xA652)   /* low word of g_huff_remaining */
#define g_huff_out_hi            G16(0xA65E)   /* high word of g_huff_out */
#define g_huff_out_lo            G16(0xA65C)   /* low word of g_huff_out */
#define g_huff_child0_mem_hi     G16(0xA66A)   /* high word of g_huff_child0_mem */
#define g_huff_child0_mem_lo     G16(0xA668)   /* low word of g_huff_child0_mem */
#define g_huff_child1_mem_hi     G16(0xA676)   /* high word of g_huff_child1_mem */
#define g_huff_child1_mem_lo     G16(0xA674)   /* low word of g_huff_child1_mem */
#define g_huff_child1_hi         G16(0xA680)   /* high word of g_huff_child1 */
#define g_huff_child1_lo         G16(0xA67E)   /* low word of g_huff_child1 */
#endif
