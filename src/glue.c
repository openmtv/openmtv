/* Machine-level entry points of the decompiled functions.
 *
 * The machine-level code (gen/), the interpreter and the differential tester
 * call a function through f_SSSS_OOOO(void): it reads the arguments the
 * original caller pushed on the emulated stack and the registers it set,
 * calls the C function and puts the result into R.ax / R.dx.  When you change
 * a function's C signature, change its wrapper here (and the conv_* tables in
 * game_table.c) to match.
 */
#include "game.h"

void f_0054_0005(void) {
    R.ax = game_main(rw(R.ss, (u16)(R.sp + 4)), rd(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_0054_085E(void) {
    hotkey_handler(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_0054_0A0B(void) {
    game_load_init_data(R.ax);
    R.sp += 4;
}

void f_0054_0CDA(void) {
    initdata_read(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)));
    R.sp += 4;
}

void f_0054_0D03(void) {
    R.ax = room_return();
    R.sp += 4;
}

void f_0054_0E5F(void) {
    R.ax = room_enter(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_0054_0F79(void) {
    R.ax = room_load_assets(rw(R.ss, (u16)(R.sp + 4)), R.ax);
    R.sp += 4;
}

void f_0054_1DB7(void) {
    R.ax = room_free_assets();
    R.sp += 4;
}

void f_0054_1EEC(void) {
    R.ax = room_draw_background(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_0054_1F6F(void) {
    new_day();
    R.sp += 4;
}

void f_0054_20E0(void) {
    R.ax = game_time_tick(rw(R.ss, (u16)(R.sp + 4)), R.ax);
    R.sp += 4;
}

void f_0054_2425(void) {
    nop_stub();
    R.sp += 4;
}

void f_0054_242A(void) {
    archive_locate_selected();
    R.sp += 4;
}

void f_0054_24E0(void) {
    sched_clear_slot_redraw(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_0054_2505(void) {
    sched_clear_slot(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_0054_256E(void) {
    R.ax = sched_slot_editable();
    R.sp += 4;
}

void f_0054_2632(void) {
    build_door_index();
    R.sp += 4;
}

void f_0054_2741(void) {
    R.ax = room_escape();
    R.sp += 4;
}

void f_0054_32B6(void) {
    day_end_go_home();
    R.sp += 4;
}

void f_0054_32FC(void) {
    player_walk_out();
    R.sp += 4;
}

void f_0054_33B3(void) {
    day_start_to_boss();
    R.sp += 4;
}

void f_0054_3481(void) {
    R.ax = screen_transition(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), R.ax);
    R.sp += 4;
}

void f_0054_34BD(void) {
    R.ax = screen_wipe(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)), rw(R.ss, (u16)(R.sp + 12)), rw(R.ss, (u16)(R.sp + 14)), rw(R.ss, (u16)(R.sp + 16)), rw(R.ss, (u16)(R.sp + 18)));
    R.sp += 4;
}

void f_0054_3701(void) {
    clock_show_start();
    R.sp += 4;
}

void f_0054_3738(void) {
    tv_update_picture();
    R.sp += 4;
}

void f_0054_3855(void) {
    tv_show_picture(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_0054_39CF(void) {
    tv_family_update(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_0054_3F89(void) {
    tv_family_stop();
    R.sp += 4;
}

void f_0054_3F96(void) {
    R.ax = savegame_names_alloc(R.ax);
    R.sp += 4;
}

void f_0054_3FC1(void) {
    savegame_names_load();
    R.sp += 4;
}

void f_0054_4021(void) {
    savegame_names_store();
    R.sp += 4;
}

void f_0054_40A6(void) {
    u32 r = tv_family_load_sprite(rw(R.ss, (u16)(R.sp + 4)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_0054_4135(void) {
    planner_refresh();
    R.sp += 4;
}

void f_0054_41E2(void) {
    library_refresh();
    R.sp += 4;
}

void f_0054_4233(void) {
    R.ax = choose_language();
    R.sp += 4;
}

void f_048C_0007(void) {
    R.ax = txt_news_cache_load(rw(R.ss, (u16)(R.sp + 4)), R.ax);
    R.sp += 4;
}

void f_048C_0147(void) {
    txt_news_cache_load_one(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_048C_020F(void) {
    R.ax = txt_news_cache_free(R.ax);
    R.sp += 4;
}

void f_048C_025B(void) {
    R.ax = txt_film_cache_load(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), R.ax);
    R.sp += 4;
}

void f_048C_094A(void) {
    R.ax = txt_film_cache_find(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_048C_098F(void) {
    txt_film_cache_load_one(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_048C_0AB5(void) {
    txt_film_cache_free(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_048C_0AFC(void) {
    txt_film_cache_free_one(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_048C_0B69(void) {
    R.ax = txt_series_cache_load(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), R.ax);
    R.sp += 4;
}

void f_048C_11B6(void) {
    R.ax = txt_series_cache_find(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_048C_11FB(void) {
    txt_series_cache_load_one(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_048C_131B(void) {
    R.ax = txt_series_cache_free(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_048C_1362(void) {
    txt_series_cache_free_one(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_048C_13C6(void) {
    R.ax = txt_script_cache_load(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), R.ax);
    R.sp += 4;
}

void f_048C_15A2(void) {
    txt_script_cache_load_one(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_048C_16C8(void) {
    R.ax = txt_script_cache_free_all(R.ax);
    R.sp += 4;
}

void f_048C_16EB(void) {
    R.ax = txt_script_cache_free_one(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_05FF_000A(void) {
    ui_on_left_click();
    R.sp += 4;
}

void f_05FF_0359(void) {
    room_filmagency_click();
    R.sp += 4;
}

void f_05FF_090A(void) {
    room_library_click();
    R.sp += 4;
}

void f_05FF_0D0E(void) {
    room_planner_click();
    R.sp += 4;
}

void f_05FF_1B1D(void) {
    room_building_click();
    R.sp += 4;
}

void f_05FF_1D23(void) {
    room_roomboard_click();
    R.sp += 4;
}

void f_05FF_21C0(void) {
    room_roomboard_goto_click();
    R.sp += 4;
}

void f_05FF_23D8(void) {
    room_office_click();
    R.sp += 4;
}

void f_05FF_25A4(void) {
    room_newsroom_click();
    R.sp += 4;
}

void f_05FF_2C02(void) {
    room_txmap_click();
    R.sp += 4;
}

void f_05FF_33F6(void) {
    room_adagency_click();
    R.sp += 4;
}

void f_05FF_380B(void) {
    room_scriptagency_click();
    R.sp += 4;
}

void f_05FF_3AB7(void) {
    room_estateagent_click();
    R.sp += 4;
}

void f_05FF_3DDF(void) {
    room_studio_click();
    R.sp += 4;
}

void f_05FF_44CF(void) {
    saveload_reload_names();
    R.sp += 4;
}

void f_05FF_44F4(void) {
    room_saveload_click();
    R.sp += 4;
}

void f_05FF_4957(void) {
    room_porter_click();
    R.sp += 4;
}

void f_0AA4_000E(void) {
    R.ax = room_boss_click();
    R.sp += 4;
}

void f_0AA4_0AC5(void) {
    room_newgame_click();
    R.sp += 4;
}

void f_0AA4_0D65(void) {
    room_supermarket_click();
    R.sp += 4;
}

void f_0AA4_10CC(void) {
    room_shop_click();
    R.sp += 4;
}

void f_0AA4_1D27(void) {
    R.ax = present_store_in_bag();
    R.sp += 4;
}

void f_0AA4_1D53(void) {
    R.ax = present_store_in_bag_betty();
    R.sp += 4;
}

void f_0AA4_1D7F(void) {
    R.ax = present_return_to_shop();
    R.sp += 4;
}

void f_0AA4_1DC0(void) {
    room_betty_click();
    R.sp += 4;
}

void f_0AA4_2054(void) {
    shop_draw_drag_icon();
    R.sp += 4;
}

void f_0AA4_20C1(void) {
    R.ax = shop_redraw();
    R.sp += 4;
}

void f_0AA4_20CC(void) {
    news_draw_card(rd(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)));
    R.sp += 4;
}

void f_0AA4_2197(void) {
    R.ax = ui_hotspot_at_cursor(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_0AA4_2243(void) {
    R.ax = tx_next_slot(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_0AA4_22AB(void) {
    R.ax = tx_prev_slot(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_0AA4_2313(void) {
    film_sell_to_dealer();
    R.sp += 4;
}

void f_0AA4_23B5(void) {
    R.ax = ad_agency_sign_contracts();
    R.sp += 4;
}

void f_0AA4_2439(void) {
    script_agency_finalize();
    R.sp += 4;
}

void f_0AA4_2491(void) {
    R.ax = library_film_to_bag();
    R.sp += 4;
}

void f_0AA4_24C3(void) {
    R.ax = library_film_to_shelf();
    R.sp += 4;
}

void f_0AA4_2546(void) {
    R.ax = filmagency_film_to_bag();
    R.sp += 4;
}

void f_0AA4_2586(void) {
    R.ax = filmagency_return_film(R.ax);
    R.sp += 4;
}

void f_0AA4_267A(void) {
    R.ax = ad_return_offer();
    R.sp += 4;
}

void f_0AA4_26CD(void) {
    R.ax = ad_take_contract();
    R.sp += 4;
}

void f_0AA4_2761(void) {
    R.ax = script_return_to_agency();
    R.sp += 4;
}

void f_0AA4_2807(void) {
    R.ax = script_to_bag();
    R.sp += 4;
}

void f_0AA4_2884(void) {
    R.ax = script_to_bag_from_studio();
    R.sp += 4;
}

void f_0AA4_28F2(void) {
    R.ax = studio_insert_script();
    R.sp += 4;
}

void f_0AA4_2950(void) {
    studio_take_script();
    R.sp += 4;
}

void f_0AA4_2A04(void) {
    studio_swap_script();
    R.sp += 4;
}

void f_0AA4_2AB7(void) {
    ui_animate_toggle(rd(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)));
    R.sp += 4;
}

void f_0AA4_2B31(void) {
    tv_update_sound();
    R.sp += 4;
}

void f_0AA4_2B55(void) {
    R.ax = txmap_after_sale();
    R.sp += 4;
}

void f_0D5E_0001(void) {
    R.ax = res_init();
    R.sp += 4;
}

void f_0D5E_0035(void) {
    R.ax = res_disk_mode();
    R.sp += 4;
}

void f_0D5E_0050(void) {
    R.ax = disk_access_begin(R.ax);
    R.sp += 4;
}

void f_0D5E_0066(void) {
    R.ax = disk_access_end(R.ax);
    R.sp += 4;
}

void f_0D5E_007C(void) {
    R.ax = res_files_open(R.ax);
    R.sp += 4;
}

void f_0D5E_008B(void) {
    res_files_close(R.ax);
    R.sp += 4;
}

void f_0D5E_009F(void) {
    debug_nop();
    R.sp += 4;
}

void f_0D5E_00A0(void) {
    u32 r = res_load_raw(rw(R.ss, (u16)(R.sp + 4)), R.ax);
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_0D5E_00FA(void) {
    R.ax = res_read_raw(rw(R.ss, (u16)(R.sp + 4)), HPTR(MK32(rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 6)))), R.ax);
    R.sp += 4;
}

void f_0D5E_011B(void) {
    u32 r = res_load0(rw(R.ss, (u16)(R.sp + 4)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_0D5E_01A1(void) {
    u32 r = res_load(rw(R.ss, (u16)(R.sp + 4)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_0D5E_0229(void) {
    R.ax = random_n(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_0D5E_025C(void) {
    game_clock_reset();
    R.sp += 4;
}

void f_0D5E_0280(void) {
    R.ax = game_clock_stop(R.ax);
    R.sp += 4;
}

void f_0D5E_02B1(void) {
    R.ax = game_clock_start();
    R.sp += 4;
}

void f_0D5E_02D6(void) {
    R.ax = game_clock_get(HPTR(rd(R.ss, (u16)(R.sp + 4))));
    R.sp += 4;
}

void f_0D5E_02E4(void) {
    game_clock_update(HPTR(rd(R.ss, (u16)(R.sp + 4))));
    R.sp += 4;
}

void f_0D5E_03AD(void) {
    sprite_bank_fixup(rd(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_0D5E_0416(void) {
    R.ax = mem_free_ptr(HPTR(rd(R.ss, (u16)(R.sp + 4))));
    R.sp += 4;
}

void f_0D5E_043A(void) {
    R.ax = figure_sprite_stride(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_0D5E_048D(void) {
    R.ax = res_query_wait(rw(R.ss, (u16)(R.sp + 4)), HPTR(MK32(rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 6)))), rw(R.ss, (u16)(R.sp + 10)));
    R.sp += 4;
}

void f_0D5E_050F(void) {
    res_cache_packed(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)));
    R.sp += 4;
}

void f_0D5E_0558(void) {
    u32 r = res_cache_unpack(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_0D5E_05BC(void) {
    figures_load_sprites();
    R.sp += 4;
}

void f_0D5E_0664(void) {
    figure_load_sprite_slot(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_0D5E_06FB(void) {
    u32 r = mem_calloc(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_0D5E_0729(void) {
    R.ax = mem_free(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_0D5E_073B(void) {
    R.ax = game_clock_pause(R.ax);
    R.sp += 4;
}

void f_0D5E_0773(void) {
    R.ax = game_clock_resume(R.ax);
    R.sp += 4;
}

void f_0D5E_0795(void) {
    u32 r = text_dup(rw(R.ss, (u16)(R.sp + 4)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_0DDD_0001(void) {
    R.ax = text_draw(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)));
    R.sp += 4;
}

void f_0DDD_002B(void) {
    text_draw_right_free(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)), rw(R.ss, (u16)(R.sp + 12)), rw(R.ss, (u16)(R.sp + 14)));
    R.sp += 4;
}

void f_0DDD_0071(void) {
    R.ax = text_draw_free(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)));
    R.sp += 4;
}

void f_0DDD_00A8(void) {
    text_draw_to(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)), rw(R.ss, (u16)(R.sp + 12)), rw(R.ss, (u16)(R.sp + 14)));
    R.sp += 4;
}

void f_0DDD_00D0(void) {
    R.ax = text_draw_wrap(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)), rw(R.ss, (u16)(R.sp + 12)), rw(R.ss, (u16)(R.sp + 14)));
    R.sp += 4;
}

void f_0DDD_00FA(void) {
    text_draw_to_free(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)), rw(R.ss, (u16)(R.sp + 12)), rw(R.ss, (u16)(R.sp + 14)));
    R.sp += 4;
}

void f_0DDD_012F(void) {
    text_draw_center_free(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)), rw(R.ss, (u16)(R.sp + 12)), rw(R.ss, (u16)(R.sp + 14)));
    R.sp += 4;
}

void f_0DDD_017B(void) {
    R.ax = text_draw_center(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)));
    R.sp += 4;
}

void f_0DDD_01BC(void) {
    text_draw_center_to(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)), rw(R.ss, (u16)(R.sp + 12)), rw(R.ss, (u16)(R.sp + 14)));
    R.sp += 4;
}

void f_0DDD_01FB(void) {
    text_draw_center_ex(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)), rw(R.ss, (u16)(R.sp + 12)), rw(R.ss, (u16)(R.sp + 14)), rw(R.ss, (u16)(R.sp + 16)), rw(R.ss, (u16)(R.sp + 18)), rw(R.ss, (u16)(R.sp + 20)));
    R.sp += 4;
}

void f_0DDD_0239(void) {
    text_draw_ex_free(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)), rw(R.ss, (u16)(R.sp + 12)), rw(R.ss, (u16)(R.sp + 14)), rw(R.ss, (u16)(R.sp + 16)), rw(R.ss, (u16)(R.sp + 18)));
    R.sp += 4;
}

void f_0DDD_026E(void) {
    text_draw_full_free(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)), rw(R.ss, (u16)(R.sp + 12)), rw(R.ss, (u16)(R.sp + 14)), rw(R.ss, (u16)(R.sp + 16)), rw(R.ss, (u16)(R.sp + 18)), rw(R.ss, (u16)(R.sp + 20)));
    R.sp += 4;
}

void f_0DDD_02A2(void) {
    R.ax = text_draw_wrap_free(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)), rw(R.ss, (u16)(R.sp + 12)), rw(R.ss, (u16)(R.sp + 14)));
    R.sp += 4;
}

void f_0DDD_02D9(void) {
    R.ax = text_draw_full(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rd(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 12)), rw(R.ss, (u16)(R.sp + 14)), rw(R.ss, (u16)(R.sp + 16)), rw(R.ss, (u16)(R.sp + 18)), rw(R.ss, (u16)(R.sp + 20)));
    R.sp += 4;
}

void f_0DDD_048D(void) {
    R.ax = text_count_lines(rd(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)));
    R.sp += 4;
}

void f_0E35_0000(void) {
    /* variable arguments: the formatter reads the words the machine-level
     * caller pushed after the format pointer, as many as the format asks for */
    u32 r = str_format_at(rd(R.ss, (u16)(R.sp + 4)), R.ss, (u16)(R.sp + 8));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_0E35_04C0(void) {
    R.ax = str_digit_count(rd(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

/* fmt_put_int/fmt_put_long take the value and the width by far pointer (the
 * original: into str_format's stack frame); the C versions work on them in
 * place in emulated memory */
void f_0E35_0504(void) {
    fmt_put_int(rd(R.ss, (u16)(R.sp + 4)), &FP16(rd(R.ss, (u16)(R.sp + 8)), 0), &FP16(rd(R.ss, (u16)(R.sp + 12)), 0), rd(R.ss, (u16)(R.sp + 16)));
    R.sp += 4;
}

void f_0E35_060D(void) {
    fmt_put_long(rd(R.ss, (u16)(R.sp + 4)), &FP32(rd(R.ss, (u16)(R.sp + 8)), 0), &FP16(rd(R.ss, (u16)(R.sp + 12)), 0), rd(R.ss, (u16)(R.sp + 16)));
    R.sp += 4;
}

void f_0E35_0730(void) {
    str_to_long(rd(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_0E35_079A(void) {
    str_patch(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)), rw(R.ss, (u16)(R.sp + 12)), rd(R.ss, (u16)(R.sp + 14)));
    R.sp += 4;
}

void f_0E35_0873(void) {
    mem_zero(rd(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 8)));
    R.sp += 4;
}

void f_0E35_088E(void) {
    R.ax = far_memcpy(HPTR(rd(R.ss, (u16)(R.sp + 4))), HPTR(rd(R.ss, (u16)(R.sp + 8))), rw(R.ss, (u16)(R.sp + 12)));
    R.sp += 4;
}

void f_0E35_08B1(void) {
    R.ax = far_strlen(rd(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_0E35_08D6(void) {
    mem_fill_char(rd(R.ss, (u16)(R.sp + 4)), rd(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 12)));
    R.sp += 4;
}

void f_0E35_0901(void) {
    R.ax = text_fit_chars(rd(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 8)));
    R.sp += 4;
}

void f_0E35_0989(void) {
    R.ax = text_width_px(rd(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_0E35_09F7(void) {
    str_is_prefix(rd(R.ss, (u16)(R.sp + 4)), rd(R.ss, (u16)(R.sp + 8)));
    R.sp += 4;
}

void f_0ED7_0004(void) {
    R.ax = game_frame();
    R.sp += 4;
}

void f_0ED7_08B8(void) {
    R.ax = present_frame();
    R.sp += 4;
}

void f_0ED7_0A8C(void) {
    panel_show_wipe();
    R.sp += 4;
}

void f_0ED7_0BBE(void) {
    mouse_bounds_full();
    R.sp += 4;
}

void f_0ED7_0BD7(void) {
    R.ax = ui_input_text(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rd(R.ss, (u16)(R.sp + 10)));
    R.sp += 4;
}

void f_0ED7_0DE2(void) {
    R.ax = cursor_shape_push(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_0ED7_0E2E(void) {
    madris_step(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_0ED7_1752(void) {
    ui_band(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)), rw(R.ss, (u16)(R.sp + 12)));
    R.sp += 4;
}

void f_0ED7_1775(void) {
    ui_band_to(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)), rw(R.ss, (u16)(R.sp + 12)), rw(R.ss, (u16)(R.sp + 14)), rw(R.ss, (u16)(R.sp + 16)));
    R.sp += 4;
}

void f_105E_000E(void) {
    R.ax = cursor_show(R.ax);
    R.sp += 4;
}

void f_105E_01AE(void) {
    R.ax = cursor_hide();
    R.sp += 4;
}

void f_105E_0203(void) {
    mouse_ease_step(rd(R.ss, (u16)(R.sp + 4)), rd(R.ss, (u16)(R.sp + 8)));
    R.sp += 4;
}

void f_105E_0266(void) {
    text_draw_2(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rd(R.ss, (u16)(R.sp + 8)), rd(R.ss, (u16)(R.sp + 12)), rw(R.ss, (u16)(R.sp + 16)));
    R.sp += 4;
}

void f_105E_0386(void) {
    R.ax = ui_frame(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)), rw(R.ss, (u16)(R.sp + 12)), rw(R.ss, (u16)(R.sp + 14)), rw(R.ss, (u16)(R.sp + 16)));
    R.sp += 4;
}

void f_105E_03DA(void) {
    R.ax = ui_frame_px(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)), rd(R.ss, (u16)(R.sp + 12)), rw(R.ss, (u16)(R.sp + 16)));
    R.sp += 4;
}

void f_105E_04DB(void) {
    R.ax = ui_frame_tile(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)));
    R.sp += 4;
}

void f_105E_0526(void) {
    ui_text_box(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)), rw(R.ss, (u16)(R.sp + 12)), rw(R.ss, (u16)(R.sp + 14)), rw(R.ss, (u16)(R.sp + 16)));
    R.sp += 4;
}

void f_105E_058C(void) {
    R.ax = popup_open(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)), rw(R.ss, (u16)(R.sp + 12)), rw(R.ss, (u16)(R.sp + 14)), rw(R.ss, (u16)(R.sp + 16)));
    R.sp += 4;
}

void f_105E_069A(void) {
    ui_message_window(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)), rw(R.ss, (u16)(R.sp + 12)), rw(R.ss, (u16)(R.sp + 14)));
    R.sp += 4;
}

void f_105E_06EA(void) {
    R.ax = popup_close(rw(R.ss, (u16)(R.sp + 4)), R.ax);
    R.sp += 4;
}

void f_105E_0778(void) {
    popup_restore(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_105E_07D9(void) {
    room_draw_border();
    R.sp += 4;
}

void f_105E_08F1(void) {
    room_draw_border_alt();
    R.sp += 4;
}

void f_105E_0A31(void) {
    R.ax = col_to_px(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_105E_0A44(void) {
    R.ax = figure_sprite_ofs(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_105E_0A72(void) {
    drag_image_draw(rd(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_105E_0AAC(void) {
    draw_sprite_to(rd(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)), rw(R.ss, (u16)(R.sp + 12)), rw(R.ss, (u16)(R.sp + 14)));
    R.sp += 4;
}

void f_105E_0AE4(void) {
    blit_opaque_to(rd(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)), rw(R.ss, (u16)(R.sp + 12)), rw(R.ss, (u16)(R.sp + 14)));
    R.sp += 4;
}

void f_105E_0B1C(void) {
    R.ax = blit_opaque(rd(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)));
    R.sp += 4;
}

void f_105E_0B56(void) {
    R.ax = grab_screen(rd(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)));
    R.sp += 4;
}

void f_105E_0B90(void) {
    tile_draw_opaque(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)));
    R.sp += 4;
}

void f_105E_0BD9(void) {
    R.ax = draw_atlas_tile(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)));
    R.sp += 4;
}

void f_105E_0C22(void) {
    sprite_draw_target(rd(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)));
    R.sp += 4;
}

void f_105E_0C5C(void) {
    tile_draw_at_x(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_105E_0CA6(void) {
    tile_draw_at_y(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_105E_0CF0(void) {
    tile_draw_fixed(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_105E_0D3B(void) {
    R.ax = draw_sprite(rd(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)));
    R.sp += 4;
}

void f_105E_0D75(void) {
    R.ax = bitmap_put_screen(rd(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)));
    R.sp += 4;
}

void f_105E_0DB1(void) {
    R.ax = blit_transparent(rd(R.ss, (u16)(R.sp + 4)), rd(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 12)), rw(R.ss, (u16)(R.sp + 14)), rw(R.ss, (u16)(R.sp + 16)), rw(R.ss, (u16)(R.sp + 18)), rw(R.ss, (u16)(R.sp + 20)), rw(R.ss, (u16)(R.sp + 22)), R.ax);
    R.sp += 4;
}

void f_105E_0ED3(void) {
    bitmap_put(rd(R.ss, (u16)(R.sp + 4)), rd(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 12)), rw(R.ss, (u16)(R.sp + 14)));
    R.sp += 4;
}

void f_105E_0F0E(void) {
    bitmap_copy(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)));
    R.sp += 4;
}

void f_105E_0F2B(void) {
    R.ax = blit_opaque_rect(rd(R.ss, (u16)(R.sp + 4)), rd(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 12)), rw(R.ss, (u16)(R.sp + 14)), rw(R.ss, (u16)(R.sp + 16)), rw(R.ss, (u16)(R.sp + 18)), rw(R.ss, (u16)(R.sp + 20)), rw(R.ss, (u16)(R.sp + 22)), R.ax);
    R.sp += 4;
}

void f_105E_1041(void) {
    elevator_queue_add(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_105E_1078(void) {
    elevator_queue_remove(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_105E_10CA(void) {
    floor_pick_door(rw(R.ss, (u16)(R.sp + 4)), HPTR(rd(R.ss, (u16)(R.sp + 6))), rw(R.ss, (u16)(R.sp + 10)));
    R.sp += 4;
}

void f_105E_114D(void) {
    R.ax = floor_door_count(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_105E_1196(void) {
    clip_cursor_cell();
    R.sp += 4;
}

void f_105E_11B5(void) {
    clip_reset();
    R.sp += 4;
}

void f_105E_11D4(void) {
    clip_panel();
    R.sp += 4;
}

void f_105E_11DF(void) {
    R.ax = cursor_x();
    R.sp += 4;
}

void f_105E_1213(void) {
    R.ax = cursor_y();
    R.sp += 4;
}

void f_105E_1247(void) {
    figure_unqueue_elevator(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_105E_1287(void) {
    building_draw_sides(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_105E_1366(void) {
    door_draw(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)));
    R.sp += 4;
}

void f_105E_14CA(void) {
    panel_build();
    R.sp += 4;
}

void f_105E_15B6(void) {
    figure_draw(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_105E_16EE(void) {
    R.ax = input_poll();
    R.sp += 4;
}

void f_105E_17AB(void) {
    mouse_poll();
    R.sp += 4;
}

void f_105E_17D1(void) {
    R.ax = room_anim_update(rw(R.ss, (u16)(R.sp + 4)), R.ax);
    R.sp += 4;
}

void f_105E_2DD2(void) {
    anim_div_tick(rw(R.ss, (u16)(R.sp + 4)), rd(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_105E_2DEF(void) {
    R.ax = room_anim_blit(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_105E_3002(void) {
    window_create(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)));
    R.sp += 4;
}

void f_105E_3247(void) {
    window_show(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_105E_3280(void) {
    tvfam_update();
    R.sp += 4;
}

void f_105E_32ED(void) {
    u32 r = tvfam_render();
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_105E_3B14(void) {
    tvfam_draw_room();
    R.sp += 4;
}

void f_105E_3BAC(void) {
    tvfam_draw_viewer0();
    R.sp += 4;
}

void f_105E_3CE8(void) {
    tvfam_draw_viewer1();
    R.sp += 4;
}

void f_105E_3D79(void) {
    tvfam_draw_viewer2();
    R.sp += 4;
}

void f_105E_3E3B(void) {
    tvfam_draw_viewer3();
    R.sp += 4;
}

void f_105E_3EB5(void) {
    tvfam_draw_viewer4();
    R.sp += 4;
}

void f_105E_3F58(void) {
    tvfam_tick();
    R.sp += 4;
}

void f_105E_3F64(void) {
    scriptagency_draw_scripts();
    R.sp += 4;
}

void f_105E_3FB7(void) {
    office_draw_paper_pile();
    R.sp += 4;
}

void f_105E_40B0(void) {
    office_draw_letter();
    R.sp += 4;
}

void f_105E_4107(void) {
    dealer_draw_pose();
    R.sp += 4;
}

void f_105E_415C(void) {
    estate_draw_wealth();
    R.sp += 4;
}

void f_105E_434D(void) {
    R.ax = cursor_over_rect(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)));
    R.sp += 4;
}

void f_105E_43D8(void) {
    tvfam_idle_anim(rd(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)), rw(R.ss, (u16)(R.sp + 12)), rw(R.ss, (u16)(R.sp + 14)));
    R.sp += 4;
}

void f_105E_4447(void) {
    R.ax = mouse_in_rect(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)));
    R.sp += 4;
}

void f_105E_447B(void) {
    ending_play();
    R.sp += 4;
}

void f_105E_47DF(void) {
    panel_restore_strip();
    R.sp += 4;
}

void f_14E1_000E(void) {
    tooltip_update();
    R.sp += 4;
}

void f_14E1_16BB(void) {
    tooltip_show(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_14E1_3406(void) {
    status_window_draw();
    R.sp += 4;
}

void f_14E1_348B(void) {
    money_box_show();
    R.sp += 4;
}

void f_14E1_34CF(void) {
    day_box_show();
    R.sp += 4;
}

void f_14E1_3502(void) {
    channel_status_draw(rd(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_14E1_3B5B(void) {
    tvinfo_fetch_titles();
    R.sp += 4;
}

void f_14E1_3D66(void) {
    ad_audience_text(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_14E1_3E49(void) {
    ad_genre_list(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_14E1_4013(void) {
    tooltip_text_window(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_14E1_403D(void) {
    tooltip_band(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)));
    R.sp += 4;
}

void f_14E1_4061(void) {
    tooltip_render(rd(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_14E1_4161(void) {
    tooltip_present();
    R.sp += 4;
}

void f_14E1_4172(void) {
    tooltip_film(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_193D_000F(void) {
    bld_render_floors(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_193D_077D(void) {
    bld_blit_view();
    R.sp += 4;
}

void f_193D_07BB(void) {
    bld_draw_elevator(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_193D_0AAF(void) {
    bld_draw_side_scenery();
    R.sp += 4;
}

void f_193D_0C1C(void) {
    bld_update_frame();
    R.sp += 4;
}

void f_193D_0D62(void) {
    fig_animate();
    R.sp += 4;
}

void f_193D_12FA(void) {
    fig_think();
    R.sp += 4;
}

void f_193D_1B63(void) {
    elevator_update();
    R.sp += 4;
}

void f_193D_1DF9(void) {
    bld_draw_overlays();
    R.sp += 4;
}

void f_193D_2424(void) {
    u32 r = door_label_text_2(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_193D_257A(void) {
    R.ax = player_fired_sequence();
    R.sp += 4;
}

void f_193D_2C8A(void) {
    fired_draw_background();
    R.sp += 4;
}

void f_1C18_000C(void) {
    data_load_tables();
    R.sp += 4;
}

void f_1C18_02C6(void) {
    strpool_add(rw(R.ss, (u16)(R.sp + 4)), rd(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_1C18_0313(void) {
    script_init_episodes();
    R.sp += 4;
}

void f_1C18_03A7(void) {
    R.ax = ledger_add(rw(R.ss, (u16)(R.sp + 4)), rd(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_1C18_03C1(void) {
    news_tick(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_1C18_044F(void) {
    R.ax = news_count_cat1();
    R.sp += 4;
}

void f_1C18_0490(void) {
    news_emit(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_1C18_069A(void) {
    news_add_event(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)));
    R.sp += 4;
}

void f_1C18_0755(void) {
    R.ax = news_advance_story(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_1C18_08A2(void) {
    news_start_stories();
    R.sp += 4;
}

void f_1C18_0B21(void) {
    R.ax = news_variant_in_use(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_1C18_0BCB(void) {
    news_rate_player(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_1C18_0C1B(void) {
    auction_commit_bids();
    R.sp += 4;
}

void f_1C18_0C73(void) {
    msg_count_new(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_1C18_0CCD(void) {
    sched_end_of_hour(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_1C18_0E49(void) {
    sched_new_day();
    R.sp += 4;
}

void f_1C18_0F51(void) {
    news_distribute();
    R.sp += 4;
}

void f_1C18_1022(void) {
    news_age(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_1C18_1116(void) {
    R.ax = series_episode_at(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)));
    R.sp += 4;
}

void f_1C18_11F8(void) {
    u32 r = money_available(rw(R.ss, (u16)(R.sp + 4)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_1C18_12F2(void) {
    program_withdraw(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_1C18_147E(void) {
    news_inbox_drop_oldest(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_1C18_14F0(void) {
    news_inbox_delete(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_1C18_1552(void) {
    script_list_delete(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_1C18_15C5(void) {
    shoplist_delete(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_1C18_1627(void) {
    contract_delete(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_1C18_172F(void) {
    series_wear_episode(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_1C18_1794(void) {
    film_apply_wear(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_1C18_17E7(void) {
    u32 r = auction_increment(rw(R.ss, (u16)(R.sp + 4)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_1DA8_0006(void) {
    R.ax = script_count_available();
    R.sp += 4;
}

void f_1DA8_004E(void) {
    sched_mark_scheduled();
    R.sp += 4;
}

void f_1DA8_0146(void) {
    R.ax = news_total_share();
    R.sp += 4;
}

void f_1DA8_0187(void) {
    R.ax = program_total_share();
    R.sp += 4;
}

void f_1DA8_01CB(void) {
    R.ax = script_episode_count(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_1DA8_0217(void) {
    programs_recover_topicality();
    R.sp += 4;
}

void f_1DA8_034E(void) {
    studio_check_finished();
    R.sp += 4;
}

void f_1DA8_06EA(void) {
    archive_genre_full(WARG16(0), WARG16(1), WARG16(2));
    R.sp += 4;
}

void f_1DA8_085D(void) {
    prog_id_genre(HPTR(rd(R.ss, (u16)(R.sp + 4))), HPTR(rd(R.ss, (u16)(R.sp + 8))));
    R.sp += 4;
}

void f_1DA8_0899(void) {
    R.ax = ad_spot_number(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_1DA8_099C(void) {
    ad_check_deadlines();
    R.sp += 4;
}

void f_1DA8_0B53(void) {
    betty_culture_check();
    R.sp += 4;
}

void f_1DA8_0CB3(void) {
    auction_add_lot(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)));
    R.sp += 4;
}

void f_1DA8_0E74(void) {
    msg_post(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rd(R.ss, (u16)(R.sp + 8)), rd(R.ss, (u16)(R.sp + 12)), rd(R.ss, (u16)(R.sp + 16)), rd(R.ss, (u16)(R.sp + 20)), rd(R.ss, (u16)(R.sp + 24)));
    R.sp += 4;
}

void f_1DA8_0F5B(void) {
    pay_credit_interest();
    R.sp += 4;
}

void f_1DA8_0FB9(void) {
    pay_daily_costs();
    R.sp += 4;
}

void f_1DA8_1227(void) {
    market_day_cleanup();
    R.sp += 4;
}

void f_1DA8_135D(void) {
    live_check_missed();
    R.sp += 4;
}

void f_1DA8_1451(void) {
    actors_daily_offer();
    R.sp += 4;
}

void f_1DA8_1538(void) {
    newgame_starting_programs();
    R.sp += 4;
}

void f_1DA8_17E3(void) {
    u32 r = transmitter_reach(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_1DA8_18E0(void) {
    R.ax = map_density_at(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rd(R.ss, (u16)(R.sp + 8)));
    R.sp += 4;
}

void f_1DA8_1918(void) {
    u32 r = map_sum_population(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)), rd(R.ss, (u16)(R.sp + 12)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_1DA8_1A03(void) {
    R.ax = transmitters_recalc(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_1DA8_1ACB(void) {
    coverage_build(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_1DA8_1CC2(void) {
    door_board_pos(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), HPTR(rd(R.ss, (u16)(R.sp + 8))), HPTR(rd(R.ss, (u16)(R.sp + 12))));
    R.sp += 4;
}

void f_1DA8_1D4E(void) {
    u32 r = door_label_text(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_1DA8_1EC6(void) {
    R.ax = clock_hour();
    R.sp += 4;
}

void f_1DA8_1EDE(void) {
    R.ax = clock_hour_round05();
    R.sp += 4;
}

void f_1DA8_1EF3(void) {
    R.ax = clock_hour_round55();
    R.sp += 4;
}

void f_1DA8_1F08(void) {
    coverage_stamp(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rd(R.ss, (u16)(R.sp + 8)), rd(R.ss, (u16)(R.sp + 12)));
    R.sp += 4;
}

void f_1DA8_2036(void) {
    u32 r = transmitter_new_reach(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_1DA8_21E1(void) {
    u32 r = block_population(rd(R.ss, (u16)(R.sp + 4)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_1DA8_2293(void) {
    R.ax = script_count_scenes(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_1DA8_22F6(void) {
    prog_give_to_player(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rd(R.ss, (u16)(R.sp + 8)));
    R.sp += 4;
}

void f_1DA8_24EA(void) {
    R.ax = script_items_bought(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_1DA8_2560(void) {
    R.ax = script_role_match_pct(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)));
    R.sp += 4;
}

void f_1DA8_2653(void) {
    u32 r = round_to(rd(R.ss, (u16)(R.sp + 4)), rd(R.ss, (u16)(R.sp + 8)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_1DA8_2682(void) {
    R.ax = viewers_to_pct(rw(R.ss, (u16)(R.sp + 4)), rd(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_1DA8_26FC(void) {
    live29_reschedule();
    R.sp += 4;
}

void f_1DA8_2735(void) {
    live187_reschedule();
    R.sp += 4;
}

void f_1DA8_276D(void) {
    npc_event_daily();
    R.sp += 4;
}

void f_1DA8_2905(void) {
    npc_event_update();
    R.sp += 4;
}

void f_1DA8_2D3B(void) {
    npc_event_strike();
    R.sp += 4;
}

void f_1DA8_3202(void) {
    movie_destroy(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)));
    R.sp += 4;
}

void f_1DA8_33A6(void) {
    u32 r = movie_title(rw(R.ss, (u16)(R.sp + 4)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_1DA8_33FA(void) {
    u32 r = movie_description(rw(R.ss, (u16)(R.sp + 4)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_1DA8_344E(void) {
    u32 r = series_title(rw(R.ss, (u16)(R.sp + 4)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_1DA8_34A2(void) {
    u32 r = series_description(rw(R.ss, (u16)(R.sp + 4)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_1DA8_34F6(void) {
    u32 r = series_episode_title(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_1DA8_3554(void) {
    u32 r = series_episode_description(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_1DA8_35B2(void) {
    u32 r = script_title(rw(R.ss, (u16)(R.sp + 4)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_1DA8_35F6(void) {
    u32 r = script_role_name(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_1DA8_3643(void) {
    R.ax = door_check_entry(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_1DA8_395A(void) {
    rating_rank_hour(R.ax);
    R.sp += 4;
}

void f_1DA8_3BE8(void) {
    image_apply_deltas(HPTR(rd(R.ss, (u16)(R.sp + 4))));
    R.sp += 4;
}

void f_1DA8_3F0D(void) {
    u32 r = transmitter_fee_bracket(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_1DA8_3F48(void) {
    R.ax = news_price(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_1DA8_3F5B(void) {
    u32 r = movie_price(rw(R.ss, (u16)(R.sp + 4)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_1DA8_4035(void) {
    u32 r = series_price(rw(R.ss, (u16)(R.sp + 4)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_1DA8_412F(void) {
    u32 r = format_millions(rd(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 8)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_1DA8_4258(void) {
    R.ax = ad_target_group_ok(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_1DA8_42E5(void) {
    R.ax = news_count_category(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_1DA8_432D(void) {
    R.ax = news_find_category(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_1DA8_4384(void) {
    R.ax = player_image_at_least(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_1DA8_43AD(void) {
    news_agency_image_check();
    R.sp += 4;
}

void f_1DA8_4439(void) {
    R.ax = master_key(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_1DA8_4481(void) {
    R.ax = sammy_daily_update();
    R.sp += 4;
}

void f_1DA8_4694(void) {
    R.ax = sammy_score(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_1DA8_4752(void) {
    R.ax = sammy_leader();
    R.sp += 4;
}

void f_1DA8_47E7(void) {
    R.ax = sammy_score_pct(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_222A_0001(void) {
    money_ledger_new_day();
    R.sp += 4;
}

void f_222A_004A(void) {
    rating_stats_new_day();
    R.sp += 4;
}

void f_222A_0206(void) {
    studio_cancel_production(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_222A_02A1(void) {
    R.ax = tx_commit_day(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_222A_03D1(void) {
    shoplist_clear_first(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_222A_03EE(void) {
    R.ax = bag_remove(rw(R.ss, (u16)(R.sp + 4)), R.ax);
    R.sp += 4;
}

void f_222A_0424(void) {
    R.ax = shoplists_book_costs(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_222A_061E(void) {
    R.ax = bag_unload_to_library();
    R.sp += 4;
}

void f_229E_0007(void) {
    news_slot_take_first(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_229E_004A(void) {
    news_toggle_subscription(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_229E_0088(void) {
    R.ax = auction_toggle_bid(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_229E_01BB(void) {
    R.ax = sched_place_ad(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)));
    R.sp += 4;
}

void f_229E_0225(void) {
    R.ax = sched_place_program(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)));
    R.sp += 4;
}

void f_229E_0465(void) {
    sched_clear_slot_2(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_229E_04A3(void) {
    sched_remove_program(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_229E_051F(void) {
    R.ax = library_check_can_sell(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_229E_0828(void) {
    R.ax = time_hour_tick();
    R.sp += 4;
}

void f_229E_0948(void) {
    R.ax = money_check_afford(rw(R.ss, (u16)(R.sp + 4)), rd(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_229E_09AF(void) {
    R.ax = studio_new_shopping_list(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_229E_0B98(void) {
    R.ax = studio_start_production(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_229E_100C(void) {
    studio_request_rent(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), R.ax);
    R.sp += 4;
}

void f_229E_104A(void) {
    studio_give_back(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_229E_1080(void) {
    studio_take_over(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_229E_1264(void) {
    studio_release(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_229E_1386(void) {
    game_load(rw(R.ss, (u16)(R.sp + 4)), R.ax);
    R.sp += 4;
}

void f_229E_139A(void) {
    game_save_load(rw(R.ss, (u16)(R.sp + 4)), R.ax);
    R.sp += 4;
}

void f_229E_200D(void) {
    game_save(rw(R.ss, (u16)(R.sp + 4)), R.ax);
    R.sp += 4;
}

void f_229E_2020(void) {
    R.ax = savegame_io(HPTR(MK32(rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 4)))), rw(R.ss, (u16)(R.sp + 8)), R.ax);
    R.sp += 4;
}

void f_229E_2140(void) {
    R.ax = library_contains(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)));
    R.sp += 4;
}

void f_24B8_000C(void) {
    R.ax = adagency_refill();
    R.sp += 4;
}

void f_24B8_02C1(void) {
    scriptagency_refill();
    R.sp += 4;
}

void f_24B8_04B9(void) {
    dealer_refill();
    R.sp += 4;
}

void f_24B8_0905(void) {
    auction_daily();
    R.sp += 4;
}

void f_24B8_0C4A(void) {
    broadcast_phase_eval(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_24B8_1B5C(void) {
    trend_pick_new();
    R.sp += 4;
}

void f_24B8_1BFD(void) {
    R.ax = sched_program_at(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_24B8_1C4C(void) {
    u32 r = actor_text(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_24B8_1C8C(void) {
    daily_boss_update();
    R.sp += 4;
}

void f_26A7_000D(void) {
    ui_warning_str(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_26A7_005D(void) {
    ui_warning(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_26A7_0077(void) {
    ui_message_str(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_26A7_0097(void) {
    R.ax = ui_message_box(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)));
    R.sp += 4;
}

void f_26A7_00E4(void) {
    R.ax = ui_draw_money();
    R.sp += 4;
}

void f_26A7_013C(void) {
    ui_status_text(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)));
    R.sp += 4;
}

void f_26A7_01DF(void) {
    ui_draw_betty_meter();
    R.sp += 4;
}

void f_26A7_02A2(void) {
    R.ax = ui_draw_planner(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_26A7_03C6(void) {
    R.ax = ui_draw_planner_rows(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_26A7_0723(void) {
    ui_draw_planner_grid(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_26A7_08B0(void) {
    u32 r = sched_slot_title(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_26A7_0A02(void) {
    u32 r = library_item_title(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_26A7_0A49(void) {
    u32 r = program_title(rw(R.ss, (u16)(R.sp + 4)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_26A7_0AC0(void) {
    R.ax = ui_library_genre_popup(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)));
    R.sp += 4;
}

void f_26A7_0C7B(void) {
    R.ax = ui_draw_transmitter_map(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_26A7_0FE5(void) {
    ui_draw_dealer_shelf(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_26A7_10B9(void) {
    R.ax = ui_draw_bag(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), R.ax);
    R.sp += 4;
}

void f_26A7_14FB(void) {
    ui_draw_ad_offers();
    R.sp += 4;
}

void f_26A7_1613(void) {
    ui_draw_library_shelf(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_26A7_1742(void) {
    R.ax = ui_series_info(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_26A7_19E9(void) {
    ui_tile_box(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)), rw(R.ss, (u16)(R.sp + 12)));
    R.sp += 4;
}

void f_26A7_1A0C(void) {
    ui_tile_box_to(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)), rw(R.ss, (u16)(R.sp + 12)), rd(R.ss, (u16)(R.sp + 14)));
    R.sp += 4;
}

void f_26A7_1B87(void) {
    ui_drag_begin(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)), rw(R.ss, (u16)(R.sp + 12)), rw(R.ss, (u16)(R.sp + 14)), rw(R.ss, (u16)(R.sp + 16)));
    R.sp += 4;
}

void f_26A7_1DF8(void) {
    R.ax = ui_drag_end();
    R.sp += 4;
}

void f_26A7_1E94(void) {
    ui_contract_popup(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)));
    R.sp += 4;
}

void f_26A7_2047(void) {
    R.ax = ui_draw_info_board();
    R.sp += 4;
}

void f_26A7_22BD(void) {
    R.ax = ui_draw_news_agencies(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_26A7_2363(void) {
    ui_draw_news_slots(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_26A7_25CD(void) {
    ui_draw_news_inbox(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_26A7_292F(void) {
    u32 r = news_text(rd(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 8)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_26A7_2B9D(void) {
    R.ax = ui_transmitter_info(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), R.ax);
    R.sp += 4;
}

void f_26A7_2F27(void) {
    ui_draw_auction_board(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_26A7_31C3(void) {
    ui_draw_script_offers();
    R.sp += 4;
}

void f_26A7_32CA(void) {
    R.ax = ui_draw_building_plan();
    R.sp += 4;
}

void f_26A7_35D8(void) {
    R.ax = ui_yesno_box(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)));
    R.sp += 4;
}

void f_26A7_3654(void) {
    ui_draw_viewers();
    R.sp += 4;
}

void f_26A7_36C8(void) {
    ui_draw_studio();
    R.sp += 4;
}

void f_26A7_3749(void) {
    ui_draw_studio_items();
    R.sp += 4;
}

void f_26A7_38EA(void) {
    R.ax = ui_studio_menu(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_26A7_3A31(void) {
    R.ax = ui_supermarket_menu(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_26A7_3B04(void) {
    ui_draw_actor_card();
    R.sp += 4;
}

void f_26A7_42B2(void) {
    ui_gift_catalog(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_26A7_43AD(void) {
    ui_draw_betty_office();
    R.sp += 4;
}

void f_26A7_4611(void) {
    ui_draw_stats_chart();
    R.sp += 4;
}

void f_2B74_0006(void) {
    ui_draw_shopping_lists();
    R.sp += 4;
}

void f_2B74_0267(void) {
    R.ax = ui_genre_count_popup(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), R.ax);
    R.sp += 4;
}

void f_2B74_03EF(void) {
    R.ax = betty_say(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)));
    R.sp += 4;
}

void f_2B74_0447(void) {
    R.ax = boss_dialog_draw(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_2B74_069E(void) {
    R.ax = tetris_draw(rw(R.ss, (u16)(R.sp + 4)), R.ax);
    R.sp += 4;
}

void f_2B74_09F1(void) {
    R.ax = save_slots_draw();
    R.sp += 4;
}

void f_2B74_0A6D(void) {
    R.ax = save_insert_disk_box(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_2B74_0AC8(void) {
    ui_disk_prompt(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_2B74_0BDD(void) {
    lang_prompt_box(rw(R.ss, (u16)(R.sp + 4)), rd(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_2B74_0D03(void) {
    R.ax = boss_say(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)));
    R.sp += 4;
}

void f_2B74_0D49(void) {
    R.ax = gossip_say(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)));
    R.sp += 4;
}

void f_2B74_0D96(void) {
    R.ax = ui_box(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)), rw(R.ss, (u16)(R.sp + 12)), rw(R.ss, (u16)(R.sp + 14)), R.ax);
    R.sp += 4;
}

void f_2B74_0E17(void) {
    porter_say(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_2B74_0E63(void) {
    options_draw();
    R.sp += 4;
}

void f_2B74_10B8(void) {
    options_draw_switch(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_2B74_114F(void) {
    confirm_cancel_box(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_2B74_11D8(void) {
    ui_draw_player_icons();
    R.sp += 4;
}

void f_2B74_12CC(void) {
    R.ax = newgame_draw_setup(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_2B74_154F(void) {
    transmitter_sale_dialog(rw(R.ss, (u16)(R.sp + 4)), rd(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_2B74_1791(void) {
    R.ax = betty_greeting(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_2B74_1875(void) {
    msg_show_next(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_2B74_1E56(void) {
    ui_player_event_text(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_2B74_1F33(void) {
    finance_report_draw();
    R.sp += 4;
}

void f_2B74_2496(void) {
    ui_draw_hline(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)));
    R.sp += 4;
}

void f_2B74_24E8(void) {
    R.ax = save_not_found_box(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_2B74_2578(void) {
    R.ax = save_overlay_box(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_2DD4_0007(void) {
    R.ax = util_min(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_2DD4_001E(void) {
    R.ax = util_max(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)));
    R.sp += 4;
}

void f_2DD4_0035(void) {
    u32 r = util_lmax(rd(R.ss, (u16)(R.sp + 4)), rd(R.ss, (u16)(R.sp + 8)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_2DD9_000C(void) {
    ai_tick();
    R.sp += 4;
}

void f_2DD9_0349(void) {
    ai_replace_weak_programs();
    R.sp += 4;
}

void f_2DD9_0710(void) {
    ai_upgrade_ads();
    R.sp += 4;
}

void f_2DD9_08D1(void) {
    R.ax = ai_find_better_ad(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_2DD9_0B13(void) {
    ai_fill_ad_slots();
    R.sp += 4;
}

void f_2DD9_0DD9(void) {
    ai_fill_program_slot();
    R.sp += 4;
}

void f_2DD9_1498(void) {
    ai_office_book_spots();
    R.sp += 4;
}

void f_2DD9_162A(void) {
    ai_book_programs_for_contract(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_2DD9_1990(void) {
    ai_book_contract_spots(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)));
    R.sp += 4;
}

void f_2DD9_1A3F(void) {
    ai_plan_film_purchases();
    R.sp += 4;
}

void f_2DD9_1DDA(void) {
    ai_execute_purchases();
    R.sp += 4;
}

void f_2DD9_20BA(void) {
    ai_plan_auction_bid(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_2DD9_2386(void) {
    ai_auction_rebid();
    R.sp += 4;
}

void f_2DD9_26EA(void) {
    ai_plan_budget(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_2DD9_2B8D(void) {
    R.ax = ai_eval_yesterday(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_2DD9_2D26(void) {
    ai_sell_program(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_2DD9_2E72(void) {
    ai_sell_weak_programs();
    R.sp += 4;
}

void f_2DD9_3085(void) {
    R.ax = series_avg_topicality(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_2DD9_311D(void) {
    ai_take_contracts();
    R.sp += 4;
}

void f_2DD9_33CB(void) {
    R.ax = ai_can_take_contract(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_2DD9_3576(void) {
    R.ax = library_total_blocks(rw(R.ss, (u16)(R.sp + 4)), R.ax);
    R.sp += 4;
}

void f_2DD9_362D(void) {
    library_genre_blocks(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_2DD9_3754(void) {
    R.ax = contract_spots_per_day(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_2DD9_381E(void) {
    R.ax = ad_genre_allowed(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)));
    R.sp += 4;
}

void f_2DD9_3985(void) {
    R.ax = ai_ready_at_target();
    R.sp += 4;
}

void f_2DD9_3C0D(void) {
    R.ax = ai_next_goal();
    R.sp += 4;
}

void f_2DD9_4115(void) {
    ai_switch();
    R.sp += 4;
}

void f_2DD9_4126(void) {
    ai_walk_to(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)));
    R.sp += 4;
}

void f_2DD9_41FF(void) {
    ai_production_status(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_2DD9_434F(void) {
    R.ax = script_avg_quality(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_2DD9_43D6(void) {
    R.ax = scripts_owned_avg_quality(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_2DD9_4460(void) {
    ai_new_day();
    R.sp += 4;
}

void f_2DD9_4719(void) {
    ai_place_live_programs();
    R.sp += 4;
}

void f_2DD9_4862(void) {
    ai_scan_transmitter_site();
    R.sp += 4;
}

void f_2DD9_4B84(void) {
    R.ax = ai_want_transmitter();
    R.sp += 4;
}

void f_2DD9_4C06(void) {
    ai_rebuild_coverage_step();
    R.sp += 4;
}

void f_2DD9_4D9F(void) {
    ai_avg_studio_budget(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_32BA_000A(void) {
    u32 r = ai_transmitter_bid(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_32BA_0124(void) {
    ai_buy_news();
    R.sp += 4;
}

void f_32BA_036D(void) {
    R.ax = ai_news_good_enough(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_32BA_03BC(void) {
    u32 r = ai_estimate_audience(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_32BA_0ABA(void) {
    R.ax = ai_news_leadin(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_32BA_0AFD(void) {
    ai_optimize_ads();
    R.sp += 4;
}

void f_32BA_0FB2(void) {
    ai_book_ad_spots(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)), rw(R.ss, (u16)(R.sp + 12)), rw(R.ss, (u16)(R.sp + 14)));
    R.sp += 4;
}

void f_32BA_11AE(void) {
    ai_buy_betty_gift();
    R.sp += 4;
}

void f_32BA_1331(void) {
    ai_give_betty_gift();
    R.sp += 4;
}

void f_32BA_1432(void) {
    ai_plan_programs();
    R.sp += 4;
}

void f_32BA_1800(void) {
    ai_swap_signs_sabotage();
    R.sp += 4;
}

void f_32BA_1B17(void) {
    event_find_target_sign(HPTR(rd(R.ss, (u16)(R.sp + 4))), HPTR(rd(R.ss, (u16)(R.sp + 8))));
    R.sp += 4;
}

void f_32BA_1C6B(void) {
    building_find_room(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), HPTR(rd(R.ss, (u16)(R.sp + 10))), HPTR(rd(R.ss, (u16)(R.sp + 14))));
    R.sp += 4;
}

void f_348B_0003(void) {
    R.ax = ai_count_short_programs();
    R.sp += 4;
}

void f_348B_0121(void) {
    R.ax = ai_find_schedule_gaps(HPTR(rd(R.ss, (u16)(R.sp + 4))), rw(R.ss, (u16)(R.sp + 8)));
    R.sp += 4;
}

void f_348B_0203(void) {
    R.ax = ai_contracts_all_booked();
    R.sp += 4;
}

void f_348B_02A3(void) {
    ai_ad_requirements();
    R.sp += 4;
}

void f_348B_04CD(void) {
    ai_budget_distribute(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)));
    R.sp += 4;
}

void f_348B_059F(void) {
    ai_init_players();
    R.sp += 4;
}

void f_348B_070C(void) {
    ad_plan_book(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)));
    R.sp += 4;
}

void f_348B_07FF(void) {
    ai_ad_plan_init(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_348B_0895(void) {
    R.ax = ai_library_avg_quality(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_348B_098D(void) {
    ai_rent_studio();
    R.sp += 4;
}

void f_348B_0AAF(void) {
    ai_buy_script();
    R.sp += 4;
}

void f_348B_0DF7(void) {
    ai_start_production();
    R.sp += 4;
}

void f_348B_0F05(void) {
    ai_next_shopping_list();
    R.sp += 4;
}

void f_348B_1087(void) {
    ai_buy_cast();
    R.sp += 4;
}

void f_35F1_000A(void) {
    bit_set(HPTR(rd(R.ss, (u16)(R.sp + 4))), rw(R.ss, (u16)(R.sp + 8)));
    R.sp += 4;
}

void f_35F1_0028(void) {
    bit_clear(rd(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 8)));
    R.sp += 4;
}

void f_35F1_0048(void) {
    R.ax = bit_test(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_35F8_0003(void) {
    R.ax = snd_init();
    R.sp += 4;
}

void f_35F8_0330(void) {
    snd_free_sample();
    R.sp += 4;
}

void f_35F8_0341(void) {
    R.ax = snd_play_sample();
    R.sp += 4;
}

void f_35F8_035C(void) {
    snd_stop_sample();
    R.sp += 4;
}

void f_35F8_037C(void) {
    R.ax = snd_room_music_start(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), R.ax);
    R.sp += 4;
}

void f_35F8_0636(void) {
    u32 r = ptr_align_para(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_365D_0000(void) {
    timer_init();
    R.sp += 4;
}

void f_365D_0006(void) {
    timer_reset();
    R.sp += 4;
}

void f_365D_000D(void) {
    timer_stop();
    R.sp += 4;
}

void f_365D_000E(void) {
    R.ax = timer_read();
    R.sp += 4;
}

void f_365D_0014(void) {
    R.ax = timer_start(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_365F_000B(void) {
    u32 r = res_load_lzw(rw(R.ss, (u16)(R.sp + 4)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_365F_00A5(void) {
    intro_scene_load();
    R.sp += 4;
}

void f_365F_0229(void) {
    intro_decode_frame();
    R.sp += 4;
}

void f_365F_0244(void) {
    intro_scene_step();
    R.sp += 4;
}

void f_365F_03A3(void) {
    intro_play_scene();
    R.sp += 4;
}

void f_365F_03F2(void) {
    intro_play();
    R.sp += 4;
}

void f_37B4_0002(void) {
    R.ax = huff_read_byte();
    R.sp += 4;
}

void f_37B4_0019(void) {
    huff_put_pixel(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_37B4_013D(void) {
    R.ax = huff_read_bit();
    R.sp += 4;
}

void f_37B4_0166(void) {
    huff_read_tree(rd(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_37B4_01FE(void) {
    huff_decode();
    R.sp += 4;
}

void f_37B4_04AC(void) {
    huff_unpack_bank(rd(R.ss, (u16)(R.sp + 4)), rd(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 12)));
    R.sp += 4;
}

void f_37B4_05DD(void) {
    huff_unpack_bank_alloc(rd(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_382C_000E(void) {
    u32 r = bitmap_alloc(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_3833_0001(void) {
    u32 r = bitmap_alloc2(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_3839_0004(void) {
    R.ax = bank_mem_size(rd(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_3848_0002(void) {
    res_bank_sizes(rw(R.ss, (u16)(R.sp + 4)), HPTR(rd(R.ss, (u16)(R.sp + 6))), HPTR(rd(R.ss, (u16)(R.sp + 10))), rw(R.ss, (u16)(R.sp + 14)));
    R.sp += 4;
}

void f_385D_0001(void) {
    R.ax = res_init_2(rd(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_385D_0037(void) {
    res_select_drive(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_385D_003E(void) {
    res_flush();
    R.sp += 4;
}

void f_385D_0043(void) {
    res_close_all(R.ax);
    R.sp += 4;
}

void f_385D_0200(void) {
    R.ax = res_list_present(rd(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_385D_0241(void) {
    R.ax = res_exists(rw(R.ss, (u16)(R.sp + 4)), R.ax);
    R.sp += 4;
}

void f_385D_029A(void) {
    R.ax = res_read(rw(R.ss, (u16)(R.sp + 4)), HPTR(MK32(rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 6)))), R.ax);
    R.sp += 4;
}

void f_385D_0336(void) {
    R.ax = res_write(rw(R.ss, (u16)(R.sp + 4)), HPTR(MK32(rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 6)))), rw(R.ss, (u16)(R.sp + 10)), R.ax);
    R.sp += 4;
}

void f_385D_03C3(void) {
    res_read_part(rw(R.ss, (u16)(R.sp + 4)), HPTR(MK32(rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 6)))), rw(R.ss, (u16)(R.sp + 10)), rw(R.ss, (u16)(R.sp + 12)), R.ax);
    R.sp += 4;
}

void f_385D_045B(void) {
    res_write_part(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 8)), rw(R.ss, (u16)(R.sp + 10)), rw(R.ss, (u16)(R.sp + 12)), R.ax);
    R.sp += 4;
}

void f_38C6_0087(void) {
    u32 r = heap_allocate_partial_block(rd(R.ss, (u16)(R.sp + 4)), rd(R.ss, (u16)(R.sp + 8)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_38C6_0137(void) {
    u32 r = heap_create_block(rd(R.ss, (u16)(R.sp + 4)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_38C6_01A5(void) {
    u32 r = heap_create_first_block(rd(R.ss, (u16)(R.sp + 4)));
    R.ax = LO16(r); R.dx = HI16(r);
    R.sp += 4;
}

void f_3985_000D(void) {
    R.ax = rtl_free(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

void f_3985_001F(void) {
    heap_insert_free_block(rd(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_3985_00BE(void) {
    R.ax = heap_join_free_blocks(rd(R.ss, (u16)(R.sp + 4)), rd(R.ss, (u16)(R.sp + 8)));
    R.sp += 4;
}

void f_3985_0136(void) {
    R.ax = heap_free_last_block();
    R.sp += 4;
}

void f_3985_021B(void) {
    R.ax = heap_free_inner_block(rd(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

void f_3985_02E7(void) {
    R.ax = rtl_farfree(rd(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}
