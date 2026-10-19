/* Logical clock: no counted entries unless build/clock_entries.c is linked
 * (see clock.c); the same file lists the run-time library functions the
 * difftest checks for exact time (difftest.c). */
#include <stdio.h>
#include "runtime.h"
__attribute__((weak)) const u32 clock_entry_table[] = { 0 };
__attribute__((weak)) const int clock_entry_count = 0;
__attribute__((weak)) const u32 rtl_exact_keys[] = { 0 };
__attribute__((weak)) const int rtl_exact_count = 0;
/* the decompiled functions (src/game_table.c): none without src/ */
__attribute__((weak)) const u32 dec_keys[1] = { 0 };
__attribute__((weak)) const int dec_count = 0;
__attribute__((weak)) const native_entry src_native_table[1] = { { 0, 0 } };
/* the game's cursor (DS:7BF6/7BF8) + its pending motion (DS:7A48/7A4A), in
 * the emulated memory; src/state.c reads it where the decompiled build keeps it */
__attribute__((weak)) void game_cursor(int *x, int *y) {
    u32 ds = (u32)(LOADSEG + 0x39D4) << 4;
    *x = (s16)(M[ds + 0x7BF6] | M[ds + 0x7BF7] << 8) + (s16)(M[ds + 0x7A48] | M[ds + 0x7A49] << 8);
    *y = (s16)(M[ds + 0x7BF8] | M[ds + 0x7BF9] << 8) + (s16)(M[ds + 0x7A4A] | M[ds + 0x7A4B] << 8);
}
/* the system arrow needs the decompiled build (src/fixes.c) */
__attribute__((weak)) int sys_arrow_state(void) { return SYS_ARROW_OFF; }
__attribute__((weak)) const int src_native_count = 0;
/* no built-in program image (build/exe_image.c, decompiled build only):
 * load the EXE */
__attribute__((weak)) const struct builtin_image *builtin_image = 0;
/* the seats' game state (src/seatctx.c): none without src/ */
__attribute__((weak)) unsigned seat_game_size(void) { return 0; }
__attribute__((weak)) unsigned seat_game_save(unsigned char *buf, unsigned cap) { (void)buf; (void)cap; return 1; }
__attribute__((weak)) void seat_game_load(const unsigned char *buf) { (void)buf; }
__attribute__((weak)) int seat_game_join_ok(void) { return 0; }
__attribute__((weak)) void seat_spectator_main(void) {}
__attribute__((weak)) void seat_human_main(void) {}
__attribute__((weak)) void seat_game_ptrcheck(void) {}
__attribute__((weak)) void seat_game_ptrcheck_before(void) {}
__attribute__((weak)) void seat_game_turn(int seat) { (void)seat; }
__attribute__((weak)) void seat_frame_timer_fresh(void) {}
/* --filter hr needs the decompiled build (src/hr.c) */
__attribute__((weak)) int hr_init(void) { return 0; }
__attribute__((weak)) int hr_ki_toggle(void) { return -1; }
/* --make-ki too (src/ki_make.c) */
__attribute__((weak)) int ki_make_main(int argc, char **argv) {
    (void)argc; (void)argv;
    fprintf(stderr, "--make-ki: only the decompiled build (build/mtv-dec) makes AI pictures\n");
    return 2;
}
__attribute__((weak)) int hr_render(u32 *out, const u32 *pal, u8 *bad) { (void)out; (void)pal; (void)bad; return -1; }
__attribute__((weak)) void hr_seat_save(int seat) { (void)seat; }
__attribute__((weak)) void hr_seat_load(int seat) { (void)seat; }
__attribute__((weak)) int sys_drag_image(unsigned char *pix, int cap, int *w, int *h, int *hx, int *hy, int lim[4]) {
    (void)pix; (void)cap; (void)w; (void)h; (void)hx; (void)hy; (void)lim; return 0;   /* (no carried item without src/) */
}
__attribute__((weak)) int seat_disk_busy(void) { return 0; }
/* a seat's own sound (src/rtl_snd.c): none without src/ */
__attribute__((weak)) void seat_sound_frame(void) {}
/* game state in host memory (src/state.c): nothing to sync without src/ */
__attribute__((weak)) void state_activate(void) {}
__attribute__((weak)) void state_export(void) {}
__attribute__((weak)) void state_drop(void) {}
__attribute__((weak)) void state_reload(void) {}
/* a network game's savegame (src/mpsave.c): without src/ none is one */
__attribute__((weak)) int mpsave_check(const u8 *data, u32 size, char *why, unsigned whylen) {
    (void)data; (void)size;
    snprintf(why, whylen, "only the decompiled build (build/mtv-dec) loads one");
    return 0;
}
/* a script's "txbuy" day line (host.c): without src/ nothing to buy */
__attribute__((weak)) int game_test_txbuy(int p, long maxfee) { (void)p; (void)maxfee; return 0; }
