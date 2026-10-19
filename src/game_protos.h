/* Prototypes of the game functions, by module. */
#ifndef GAME_PROTOS_H
#define GAME_PROTOS_H

/* main_loop.c */
u16 game_main(u16 argc, u32 argv);
void hotkey_handler(u16 a0);
void game_load_init_data(u16 ax);
void initdata_read(u16 a0, u16 a1, u16 a2);
u16 room_return(void);
u16 room_enter(u16 a0);
u16 room_load_assets(u16 a0, u16 ax);
u16 room_free_assets(void);
u16 room_draw_background(u16 a0);
void new_day(void);
u16 game_time_tick(u16 a0, u16 ax);
void room_minute_update(void);
void nop_stub(void);
void archive_locate_selected(void);
void sched_clear_slot_redraw(u16 a0);
void sched_clear_slot(u16 a0, u16 a1);
u16 sched_slot_editable(void);
void build_door_index(void);
u16 room_escape(void);
void day_end_go_home(void);
void player_walk_out(void);
void day_start_to_boss(void);
u16 screen_transition(u16 a0, u16 a1, u16 a2, u16 ax);
u16 screen_wipe(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5, u16 a6, u16 a7);
void clock_show_start(void);
void tv_update_picture(void);
void tv_show_picture(u16 a0);
void tv_family_update(u16 a0);
void tv_family_stop(void);
u16 savegame_names_alloc(u16 ax);
void savegame_names_load(void);
void savegame_names_store(void);
u32 tv_family_load_sprite(u16 a0);
void planner_refresh(void);
void library_refresh(void);
u16 choose_language(void);

/* text_cache.c */
u16 txt_news_cache_load(u16 a0, u16 ax);
void txt_news_cache_load_one(u16 a0, u16 a1);
u16 txt_news_cache_free(u16 ax);
u16 txt_film_cache_load(u16 a0, u16 a1, u16 ax);
u16 txt_film_cache_find(u16 a0);
void txt_film_cache_load_one(u16 a0, u16 a1);
void txt_film_cache_free(u16 a0);
void txt_film_cache_free_one(u16 a0);
u16 txt_series_cache_load(u16 a0, u16 a1, u16 ax);
u16 txt_series_cache_find(u16 a0);
void txt_series_cache_load_one(u16 a0, u16 a1);
u16 txt_series_cache_free(u16 a0);
void txt_series_cache_free_one(u16 a0);
u16 txt_script_cache_load(u16 a0, u16 a1, u16 ax);
void txt_script_cache_load_one(u16 a0, u16 a1);
u16 txt_script_cache_free_all(u16 ax);
u16 txt_script_cache_free_one(u16 a0);

/* rooms_click.c */
void ui_on_left_click(void);
void room_filmagency_click(void);
void room_library_click(void);
void room_planner_click(void);
void room_building_click(void);
void room_roomboard_click(void);
void room_roomboard_goto_click(void);
void room_office_click(void);
void room_newsroom_click(void);
void room_txmap_click(void);
void room_adagency_click(void);
void room_scriptagency_click(void);
void room_estateagent_click(void);
void room_studio_click(void);
void saveload_reload_names(void);
void room_saveload_click(void);
void room_porter_click(void);

/* rooms_people.c */
u16 room_boss_click(void);
void room_newgame_click(void);
void room_supermarket_click(void);
void room_shop_click(void);
u16 present_store_in_bag(void);
u16 present_store_in_bag_betty(void);
u16 present_return_to_shop(void);
void room_betty_click(void);
void shop_draw_drag_icon(void);
u16 shop_redraw(void);
void news_draw_card(u32 a0, u16 a2, u16 a3);
u16 ui_hotspot_at_cursor(u16 a0);
u16 tx_next_slot(u16 a0);
u16 tx_prev_slot(u16 a0);
void film_sell_to_dealer(void);
u16 ad_agency_sign_contracts(void);
void script_agency_finalize(void);
u16 library_film_to_bag(void);
u16 library_film_to_shelf(void);
u16 filmagency_film_to_bag(void);
u16 filmagency_return_film(u16 ax);
u16 ad_return_offer(void);
u16 ad_take_contract(void);
u16 script_return_to_agency(void);
u16 script_to_bag(void);
u16 script_to_bag_from_studio(void);
u16 studio_insert_script(void);
void studio_take_script(void);
void studio_swap_script(void);
void ui_animate_toggle(u32 a0, u16 a2, u16 a3);
void tv_update_sound(void);
u16 txmap_after_sale(void);

/* system.c */
u16 res_init(void);
u16 res_disk_mode(void);
u16 disk_access_begin(u16 ax);
u16 disk_access_end(u16 ax);
u16 res_files_open(u16 ax);
void res_files_close(u16 ax);
void debug_nop(void);
u32 res_load_raw(u16 a0, u16 ax);
u16 res_read_raw(u16 a0, u8 *buf, u16 ax);
u32 res_load0(u16 a0);
u32 res_load(u16 a0);
u16 random_n(u16 a0, u16 a1);
void game_clock_reset(void);
u16 game_clock_stop(u16 ax);
u16 game_clock_start(void);
u16 game_clock_get(u8 *a0);
void game_clock_update(u8 *a0);
void sprite_bank_fixup(u32 a0);
u16 mem_free_ptr(u8 *a0);
u16 figure_sprite_stride(u16 a0, u16 a1);
u16 res_query_wait(u16 a0, u8 *buf, u16 a3);
void res_cache_packed(u16 a0, u16 a1, u16 a2);
u32 res_cache_unpack(u16 a0, u16 a1);
void figures_load_sprites(void);
void figure_load_sprite_slot(u16 a0, u16 a1);
u32 mem_calloc(u16 a0, u16 a1);
u16 mem_free(u16 a0, u16 a1);
u16 game_clock_pause(u16 ax);
u16 game_clock_resume(u16 ax);
u32 text_dup(u16 a0);

/* text_draw.c */
u16 text_draw(u16 a0, u16 a1, u16 a2, u16 a3);
void text_draw_right_free(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5);
u16 text_draw_free(u16 a0, u16 a1, u16 a2, u16 a3);
void text_draw_to(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5);
u16 text_draw_wrap(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5);
void text_draw_to_free(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5);
void text_draw_center_free(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5);
u16 text_draw_center(u16 a0, u16 a1, u16 a2, u16 a3);
void text_draw_center_to(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5);
void text_draw_center_ex(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5, u16 a6, u16 a7, u16 a8);
void text_draw_ex_free(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5, u16 a6, u16 a7);
void text_draw_full_free(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5, u16 a6, u16 a7, u16 a8);
u16 text_draw_wrap_free(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5);
u16 text_draw_full(u16 a0, u16 a1, u32 a2, u16 a4, u16 a5, u16 a6, u16 a7, u16 a8);
u16 text_count_lines(u32 a0, u16 a2, u16 a3);

/* strings.c */
/* str_format(fmt, words...): the game's sprintf ('#' codes, strings.c) into a
 * new 1000-byte buffer.  The arguments are 16-bit words as the original
 * pushed them: a long or a far pointer as W2(x); the macro counts them (a
 * 0 appended to the list keeps it standard C when there are none). */
#define STR_FORMAT_FMT(fmt, ...) (fmt)
#define STR_FORMAT_WORDS(fmt, ...) __VA_ARGS__
#define str_format(...) str_format_words(STR_FORMAT_FMT(__VA_ARGS__, 0), \
        (const u16[]){STR_FORMAT_WORDS(__VA_ARGS__, 0)}, \
        (u16)(sizeof((const u16[]){STR_FORMAT_WORDS(__VA_ARGS__, 0)}) / sizeof(u16) - 1))
u32 str_format_words(u32 fmt, const u16 *args, u16 n);
u32 str_format_at(u32 fmt, u16 seg, u16 off);
u16 str_digit_count(u32 a0);
void fmt_put_int(u32 out, u16 *value, u16 *width, u32 padc);
void fmt_put_long(u32 out, u32 *value, u16 *width, u32 padc);
void str_to_long(u32 a0);
void str_patch(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u32 a5);
void mem_zero(u32 a0, u16 a2);
u16 far_memcpy(u8 *a0, const u8 *a2, u16 a4);
u16 far_strlen(u32 a0);
void mem_fill_char(u32 a0, u32 a2, u16 a4);
u16 text_fit_chars(u32 a0, u16 a2);
u16 text_width_px(u32 a0);
void str_is_prefix(u32 a0, u32 a2);

/* frame.c */
u16 game_frame(void);
u16 present_frame(void);
void panel_show_wipe(void);
void mouse_bounds_full(void);
u16 ui_input_text(u16 a0, u16 a1, u16 a2, u32 a3);
u16 cursor_shape_push(u16 a0);
void madris_step(u16 a0);
void ui_band(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4);
void ui_band_to(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5, u16 a6);

/* ui.c */
u16 cursor_show(u16 ax);
u16 cursor_hide(void);
void mouse_ease_step(u32 a0, u32 a2);
void text_draw_2(u16 a0, u16 a1, u32 a2, u32 a4, u16 a6);
u16 ui_frame(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5, u16 a6);
u16 ui_frame_px(u16 a0, u16 a1, u16 a2, u16 a3, u32 a4, u16 a6);
u16 ui_frame_tile(u16 a0, u16 a1, u16 a2);
void ui_text_box(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5, u16 a6);
u16 popup_open(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5, u16 a6);
void ui_message_window(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5);
u16 popup_close(u16 a0, u16 ax);
void popup_restore(u16 a0);
void room_draw_border(void);
void room_draw_border_alt(void);
u16 col_to_px(u16 a0);
u16 figure_sprite_ofs(u16 a0, u16 a1);
void drag_image_draw(u32 a0);
void draw_sprite_to(u32 a0, u16 a2, u16 a3, u16 a4, u16 a5);
void blit_opaque_to(u32 a0, u16 a2, u16 a3, u16 a4, u16 a5);
u16 blit_opaque(u32 a0, u16 a2, u16 a3);
u16 grab_screen(u32 a0, u16 a2, u16 a3);
void tile_draw_opaque(u16 a0, u16 a1, u16 a2);
u16 draw_atlas_tile(u16 a0, u16 a1, u16 a2);
void sprite_draw_target(u32 a0, u16 a2, u16 a3);
void tile_draw_at_x(u16 a0, u16 a1);
void tile_draw_at_y(u16 a0, u16 a1);
void tile_draw_fixed(u16 a0, u16 a1);
u16 draw_sprite(u32 a0, u16 a2, u16 a3);
u16 bitmap_put_screen(u32 a0, u16 a2, u16 a3);
u16 blit_transparent(u32 a0, u32 a2, u16 a4, u16 a5, u16 a6, u16 a7, u16 a8, u16 a9, u16 ax);
void bitmap_put(u32 a0, u32 a2, u16 a4, u16 a5);
void bitmap_copy(u16 a0, u16 a1, u16 a2, u16 a3);
u16 blit_opaque_rect(u32 a0, u32 a2, u16 a4, u16 a5, u16 a6, u16 a7, u16 a8, u16 a9, u16 ax);
void elevator_queue_add(u16 a0);
void elevator_queue_remove(u16 a0);
void floor_pick_door(u16 a0, u8 *a1, u16 a3);
u16 floor_door_count(u16 a0);
void clip_cursor_cell(void);
void clip_reset(void);
void clip_panel(void);
u16 cursor_x(void);
u16 cursor_y(void);
void figure_unqueue_elevator(u16 a0);
void building_draw_sides(u16 a0);
void door_draw(u16 a0, u16 a1, u16 a2);
void panel_build(void);
void figure_draw(u16 a0, u16 a1);
u16 input_poll(void);
void mouse_poll(void);
u16 room_anim_update(u16 a0, u16 ax);
void anim_div_tick(u16 a0, u32 a1);
u16 room_anim_blit(u16 a0);
void window_create(u16 a0, u16 a1, u16 a2, u16 a3);
void window_show(u16 a0, u16 a1);
void tvfam_update(void);
u32 tvfam_render(void);
void tvfam_draw_room(void);
void tvfam_draw_viewer0(void);
void tvfam_draw_viewer1(void);
void tvfam_draw_viewer2(void);
void tvfam_draw_viewer3(void);
void tvfam_draw_viewer4(void);
void tvfam_tick(void);
void scriptagency_draw_scripts(void);
void office_draw_paper_pile(void);
void office_draw_letter(void);
void dealer_draw_pose(void);
void estate_draw_wealth(void);
u16 cursor_over_rect(u16 a0, u16 a1, u16 a2, u16 a3);
void tvfam_idle_anim(u32 a0, u16 a2, u16 a3, u16 a4, u16 a5);
u16 mouse_in_rect(u16 a0, u16 a1, u16 a2, u16 a3);
void ending_play(void);
void panel_restore_strip(void);

/* status_panel.c */
void tooltip_update(void);
void tooltip_show(u16 a0);
void status_window_draw(void);
void money_box_show(void);
void day_box_show(void);
void channel_status_draw(u32 a0);
void tvinfo_fetch_titles(void);
void ad_audience_text(u16 a0);
void ad_genre_list(u16 a0, u16 a1);
void tooltip_text_window(u16 a0, u16 a1);
void tooltip_band(u16 a0, u16 a1, u16 a2);
void tooltip_render(u32 a0);
void tooltip_present(void);
void tooltip_film(u16 a0);

/* building.c */
void bld_render_floors(u16 a0);
void bld_blit_view(void);
void bld_draw_elevator(u16 a0);
void bld_draw_side_scenery(void);
void bld_update_frame(void);
void fig_animate(void);
void fig_think(void);
void elevator_update(void);
void bld_camera_follow(void);
void bld_draw_overlays(void);
u32 door_label_text_2(u16 a0, u16 a1);
u16 player_fired_sequence(void);
void fired_draw_background(void);

/* news.c */
void data_load_tables(void);
void strpool_add(u16 a0, u32 a1);
void script_init_episodes(void);
u16 ledger_add(u16 a0, u32 a1);
void news_tick(u16 a0, u16 a1);
u16 news_count_cat1(void);
void news_emit(u16 a0);
void news_add_event(u16 a0, u16 a1, u16 a2);
u16 news_advance_story(u16 a0);
void news_start_stories(void);
u16 news_variant_in_use(u16 a0, u16 a1);
void news_rate_player(u16 a0);
void auction_commit_bids(void);
void msg_count_new(u16 a0);
void sched_end_of_hour(u16 a0);
void sched_new_day(void);
void news_distribute(void);
void news_age(u16 a0, u16 a1);
u16 series_episode_at(u16 a0, u16 a1, u16 a2);
u32 money_available(u16 a0);
void program_withdraw(u16 a0);
void news_inbox_drop_oldest(u16 a0);
void news_inbox_delete(u16 a0, u16 a1);
void script_list_delete(u16 a0, u16 a1);
void shoplist_delete(u16 a0, u16 a1);
void contract_delete(u16 a0, u16 a1);
void series_wear_episode(u16 a0, u16 a1);
void film_apply_wear(u16 a0);
u32 auction_increment(u16 a0);

/* rules.c */
u16 script_count_available(void);
void sched_mark_scheduled(void);
u16 news_total_share(void);
u16 program_total_share(void);
u16 script_episode_count(u16 a0);
void programs_recover_topicality(void);
void studio_check_finished(void);
void archive_genre_full(u16 player, u16 id, u16 genre);
void prog_id_genre(u8 *a0, u8 *a2);
u16 ad_spot_number(u16 a0, u16 a1);
void ad_check_deadlines(void);
void betty_culture_check(void);
void auction_add_lot(u16 a0, u16 a1, u16 a2);
void msg_post(u16 a0, u16 a1, u32 a2, u32 a4, u32 a6, u32 a8, u32 a10);
void pay_credit_interest(void);
void pay_daily_costs(void);
void market_day_cleanup(void);
void live_check_missed(void);
void actors_daily_offer(void);
void newgame_starting_programs(void);
u32 transmitter_reach(u16 a0, u16 a1);
u16 map_density_at(u16 a0, u16 a1, u32 a2);
u32 map_sum_population(u16 a0, u16 a1, u16 a2, u16 a3, u32 a4);
u16 transmitters_recalc(u16 a0);
void coverage_build(u16 a0);
void door_board_pos(u16 a0, u16 a1, u8 *a2, u8 *a4);
u32 door_label_text(u16 a0, u16 a1);
u16 clock_hour(void);
u16 clock_hour_round05(void);
u16 clock_hour_round55(void);
void coverage_stamp(u16 a0, u16 a1, u32 a2, u32 a4);
u32 transmitter_new_reach(u16 a0, u16 a1);
u32 block_population(u32 a0);
u16 script_count_scenes(u16 a0);
void prog_give_to_player(u16 a0, u16 a1, u32 a2);
u16 script_items_bought(u16 a0, u16 a1);
u16 script_role_match_pct(u16 a0, u16 a1, u16 a2, u16 a3);
u32 round_to(u32 a0, u32 a2);
u16 viewers_to_pct(u16 a0, u32 a1);
void live29_reschedule(void);
void live187_reschedule(void);
void npc_event_daily(void);
void npc_event_update(void);
void npc_event_strike(void);
void movie_destroy(u16 a0, u16 a1, u16 a2);
u32 movie_title(u16 a0);
u32 movie_description(u16 a0);
u32 series_title(u16 a0);
u32 series_description(u16 a0);
u32 series_episode_title(u16 a0, u16 a1);
u32 series_episode_description(u16 a0, u16 a1);
u32 script_title(u16 a0);
u32 script_role_name(u16 a0, u16 a1);
u16 door_check_entry(u16 a0, u16 a1);
void rating_rank_hour(u16 ax);
void image_apply_deltas(u8 *a0);
u32 transmitter_fee_bracket(u16 a0, u16 a1);
u16 news_price(u16 a0);
u32 movie_price(u16 a0);
u32 series_price(u16 a0);
u32 format_millions(u32 a0, u16 a2);
u16 ad_target_group_ok(u16 a0, u16 a1);
u16 news_count_category(u16 a0);
u16 news_find_category(u16 a0, u16 a1);
u16 player_image_at_least(u16 a0, u16 a1);
void news_agency_image_check(void);
u16 master_key(u16 a0, u16 a1);
u16 sammy_daily_update(void);
u16 sammy_score(u16 a0);
u16 sammy_leader(void);
u16 sammy_score_pct(u16 a0);

/* settlement.c */
void money_ledger_new_day(void);
void rating_stats_new_day(void);
void studio_cancel_production(u16 a0);
u16 tx_commit_day(u16 a0);
void shoplist_clear_first(u16 a0);
u16 bag_remove(u16 a0, u16 ax);
u16 shoplists_book_costs(u16 a0);
u16 bag_unload_to_library(void);

/* actions.c */
void news_slot_take_first(u16 a0, u16 a1);
void news_toggle_subscription(u16 a0, u16 a1);
u16 auction_toggle_bid(u16 a0);
u16 sched_place_ad(u16 a0, u16 a1, u16 a2);
u16 sched_place_program(u16 a0, u16 a1, u16 a2, u16 a3);
void sched_clear_slot_2(u16 a0, u16 a1);
void sched_remove_program(u16 a0, u16 a1);
u16 library_check_can_sell(u16 a0, u16 a1);
u16 time_hour_tick(void);
u16 money_check_afford(u16 a0, u32 a1);
u16 studio_new_shopping_list(u16 a0);
u16 studio_start_production(u16 a0);
void studio_request_rent(u16 a0, u16 a1, u16 ax);
void studio_give_back(u16 a0);
void studio_take_over(u16 a0, u16 a1);
void studio_release(u16 a0);
void game_load(u16 a0, u16 ax);
void game_save_load(u16 a0, u16 ax);
void game_save(u16 a0, u16 ax);
int savegame_load_refused(void);         /* the last game_load loaded nothing: the savegame has more films, series or ads than this game */
int savegame_save_refused(void);         /* the last game_save wrote nothing: the savegame would not fit in 64 KB */
void savegame_refused_warning(int saving);   /* the warning box that says why (saving: 1, loading: 0) */
u16 savegame_io(u8 *buf, u16 a2, u16 ax);
u16 library_contains(u16 a0, u16 a1, u16 a2);

/* market.c */
u16 adagency_refill(void);
void scriptagency_refill(void);
void dealer_refill(void);
void auction_daily(void);
void broadcast_phase_eval(u16 a0);
void trend_pick_new(void);
u16 sched_program_at(u16 a0, u16 a1);
u32 actor_text(u16 a0, u16 a1);
void daily_boss_update(void);

/* screens.c */
void ui_warning_str(u16 a0, u16 a1);
void ui_warning(u16 a0);
void ui_message_str(u16 a0, u16 a1);
u16 ui_message_box(u16 a0, u16 a1, u16 a2);
u16 ui_draw_money(void);
void ui_status_text(u16 a0, u16 a1, u16 a2);
void ui_draw_betty_meter(void);
u16 ui_draw_planner(u16 a0);
u16 ui_draw_planner_rows(u16 a0);
void ui_draw_planner_grid(u16 a0);
u32 sched_slot_title(u16 a0, u16 a1);
u32 library_item_title(u16 a0, u16 a1, u16 a2);
u32 program_title(u16 a0);
u16 ui_library_genre_popup(u16 a0, u16 a1, u16 a2, u16 a3);
u16 ui_draw_transmitter_map(u16 a0);
void ui_draw_dealer_shelf(u16 a0);
u16 ui_draw_bag(u16 a0, u16 a1, u16 ax);
void ui_draw_ad_offers(void);
void ui_draw_library_shelf(u16 a0);
u16 ui_series_info(u16 a0);
void ui_tile_box(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4);
void ui_tile_box_to(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u32 a5);
void ui_drag_begin(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5, u16 a6);
u16 ui_drag_end(void);
void ui_contract_popup(u16 a0, u16 a1, u16 a2, u16 a3);
u16 ui_draw_info_board(void);
u16 ui_draw_news_agencies(u16 a0);
void ui_draw_news_slots(u16 a0, u16 a1);
void ui_draw_news_inbox(u16 a0);
u32 news_text(u32 a0, u16 a2);
u16 ui_transmitter_info(u16 a0, u16 a1, u16 ax);
void ui_draw_auction_board(u16 a0);
void ui_draw_script_offers(void);
u16 ui_draw_building_plan(void);
u16 ui_yesno_box(u16 a0, u16 a1, u16 a2);
void ui_draw_viewers(void);
void ui_draw_studio(void);
void ui_draw_studio_items(void);
u16 ui_studio_menu(u16 a0);
u16 ui_supermarket_menu(u16 a0);
void ui_draw_actor_card(void);
void ui_gift_catalog(u16 a0);
void ui_draw_betty_office(void);
void ui_draw_stats_chart(void);

/* dialogs.c */
void ui_draw_shopping_lists(void);
u16 ui_genre_count_popup(u16 a0, u16 a1, u16 a2, u16 ax);
u16 betty_say(u16 a0, u16 a1, u16 a2);
u16 boss_dialog_draw(u16 a0, u16 a1);
u16 tetris_draw(u16 a0, u16 ax);
u16 save_slots_draw(void);
u16 save_insert_disk_box(u16 a0);
void ui_disk_prompt(u16 a0, u16 a1);
void lang_prompt_box(u16 a0, u32 a1);
u16 boss_say(u16 a0, u16 a1, u16 a2);
u16 gossip_say(u16 a0, u16 a1, u16 a2);
u16 ui_box(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5, u16 ax);
void porter_say(u16 a0);
void options_draw(void);
void options_draw_switch(u16 a0);
void confirm_cancel_box(u16 a0);
void ui_draw_player_icons(void);
u16 newgame_draw_setup(u16 a0);
void transmitter_sale_dialog(u16 a0, u32 a1);
u16 betty_greeting(u16 a0);
void msg_show_next(u16 a0);
void ui_player_event_text(u16 a0);
void finance_report_draw(void);
void ui_draw_hline(u16 a0, u16 a1, u16 a2);
u16 save_not_found_box(u16 a0);
u16 save_overlay_box(u16 a0);

/* util.c */
u16 util_min(u16 a0, u16 a1);
u16 util_max(u16 a0, u16 a1, u16 a2);
u32 util_lmax(u32 a0, u32 a2);

/* ai.c */
void ai_tick(void);
void ai_replace_weak_programs(void);
void ai_upgrade_ads(void);
u16 ai_find_better_ad(u16 a0, u16 a1);
void ai_fill_ad_slots(void);
void ai_fill_program_slot(void);
void ai_office_book_spots(void);
void ai_book_programs_for_contract(u16 a0, u16 a1);
void ai_book_contract_spots(u16 a0, u16 a1, u16 a2);
void ai_plan_film_purchases(void);
void ai_execute_purchases(void);
void ai_plan_auction_bid(u16 a0);
void ai_auction_rebid(void);
void ai_plan_budget(u16 a0);
u16 ai_eval_yesterday(u16 a0, u16 a1);
void ai_sell_program(u16 a0, u16 a1);
void ai_sell_weak_programs(void);
u16 series_avg_topicality(u16 a0);
void ai_take_contracts(void);
u16 ai_can_take_contract(u16 a0);
u16 library_total_blocks(u16 a0, u16 ax);
void library_genre_blocks(u16 a0);
u16 contract_spots_per_day(u16 a0, u16 a1);
u16 ad_genre_allowed(u16 a0, u16 a1, u16 a2);
u16 ai_ready_at_target(void);
u16 ai_next_goal(void);
void ai_switch(void);
void ai_walk_to(u16 a0, u16 a1, u16 a2, u16 a3);
void ai_production_status(u16 a0);
u16 script_avg_quality(u16 a0);
u16 scripts_owned_avg_quality(u16 a0);
void ai_new_day(void);
void ai_place_live_programs(void);
void ai_scan_transmitter_site(void);
u16 ai_want_transmitter(void);
void ai_rebuild_coverage_step(void);
void ai_avg_studio_budget(u16 a0);

/* ai_estimate.c */
u32 ai_transmitter_bid(u16 a0, u16 a1, u16 a2);
void ai_buy_news(void);
u16 ai_news_good_enough(u16 a0, u16 a1);
u32 ai_estimate_audience(u16 a0, u16 a1, u16 a2);
u16 ai_news_leadin(u16 a0, u16 a1);
void ai_optimize_ads(void);
void ai_book_ad_spots(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5);
void ai_buy_betty_gift(void);
void ai_give_betty_gift(void);
void ai_plan_programs(void);
void ai_swap_signs_sabotage(void);
void event_find_target_sign(u8 *a0, u8 *a2);
void building_find_room(u16 a0, u16 a1, u16 a2, u8 *a3, u8 *a5);

/* ai_plan.c */
u16 ai_count_short_programs(void);
u16 ai_find_schedule_gaps(u8 *a0, u16 a2);
u16 ai_contracts_all_booked(void);
void ai_ad_requirements(void);
void ai_budget_distribute(u16 a0, u16 a1, u16 a2, u16 a3);
void ai_init_players(void);
void ad_plan_book(u16 a0, u16 a1, u16 a2, u16 a3);
void ai_ad_plan_init(u16 a0);
u16 ai_library_avg_quality(u16 a0);
void ai_rent_studio(void);
void ai_buy_script(void);
void ai_start_production(void);
void ai_next_shopping_list(void);
void ai_buy_cast(void);

/* bits.c */
void bit_set(u8 *a0, u16 a2);
void bit_clear(u32 a0, u16 a2);
u16 bit_test(u16 a0, u16 a1);

/* sound.c */
u16 snd_init(void);
void snd_free_sample(void);
u16 snd_play_sample(void);
void snd_stop_sample(void);
u16 snd_room_music_start(u16 a0, u16 a1, u16 ax);
u32 ptr_align_para(u16 a0, u16 a1);

/* timer.c */
void timer_init(void);
void timer_reset(void);
void timer_stop(void);
u16 timer_read(void);
u16 timer_start(u16 a0);

/* intro.c */
u32 res_load_lzw(u16 a0);
void intro_scene_load(void);
void intro_decode_frame(void);
void intro_scene_step(void);
void intro_play_scene(void);
void intro_play(void);

/* huffman.c */
u16 huff_read_byte(void);
void huff_put_pixel(u16 a0);
u16 huff_read_bit(void);
void huff_read_tree(u32 a0);
void huff_decode(void);
void huff_unpack_bank(u32 a0, u32 a2, u16 a4);
void huff_unpack_bank_alloc(u32 a0);

/* bitmap.c */
u32 bitmap_alloc(u16 a0, u16 a1);
u32 bitmap_alloc2(u16 a0, u16 a1);
u16 bank_mem_size(u32 a0);
void res_bank_sizes(u16 a0, u8 *a1, u8 *a3, u16 a5);

/* resfile.c */
u16 res_init_2(u32 a0);
void res_select_drive(u16 a0);
void res_flush(void);
void res_close_all(u16 ax);
u16 res_list_present(u32 a0);
u16 res_exists(u16 a0, u16 ax);
u16 res_read(u16 a0, u8 *buf, u16 ax);
u16 res_write(u16 a0, const u8 *buf, u16 a3, u16 ax);
void res_read_part(u16 a0, u8 *buf, u16 a3, u16 a4, u16 ax);
void res_write_part(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 ax);

/* heap.c */
u32 heap_allocate_partial_block(u32 a0, u32 a2);
u32 heap_create_block(u32 a0);
u32 heap_create_first_block(u32 a0);
u16 rtl_free(u16 a0, u16 a1);
void heap_insert_free_block(u32 a0);
u16 heap_join_free_blocks(u32 a0, u32 a2);
u16 heap_free_last_block(void);
u16 heap_free_inner_block(u32 a0);
u16 rtl_farfree(u32 a0);

/* state.c */
#define FILMS_MAX 999                    /* film ids < 1000 (1000 + n: series n) */
#define SERIES_MAX 999                   /* programme ids 1000 + n < 2000 */
extern u16 g_nfilms, g_nseries, g_nads;
extern u8 g_ads_extra[];
void tables_count_init(void);
void state_activate(void);
void state_export(void);
void state_block_freed(u32 fp);
void state_drop(void);
void state_reload(void);
u32 host_handle(void *p, u16 n);
void host_handle_release(u32 fp);
#endif
