/* Timer: timer ticks shared with the sound driver.
 *
 * Original module 365D of MTV.EXE (Rainbow Arts, 1991).  Decompiled by
 * tools/decomp.py and hand-maintained since the freeze (git tag
 * generated-final).  Every function keeps its original address in the
 * comment above it; that address ties it to the machine-level twin in gen/
 * and to the difftest tables -- keep it when editing.
 */
#include "game.h"

/* 365D:0000  ? timer/sound-driver init stub; returns -1 */
void timer_init(void) {
    FUNC_TICK(3);
    return;
}

/* 365D:0006  g_timer_ticks = 0 */
void timer_reset(void) {
    FUNC_TICK(2);
    g_timer_ticks = 0;
    return;
}

/* 365D:000D  no-op stub */
void timer_stop(void) {
    FUNC_TICK(1);
    return;
}

/* 365D:000E  return g_timer_ticks (~50 Hz; used for frame pacing, random seeds, play-time clock) */
u16 timer_read(void) {
    u16 ax;
    FUNC_TICK(3);
    ax = g_timer_ticks;
    return ax;
}

/* 365D:0014  g_timer_ticks = n and hand the tick block 0x902C to the sound driver (371E entry 3) so its timer interrupt counts ticks */
u16 timer_start(u16 a0) {
    FUNC_TICK(11);
    g_timer_ticks = a0;
    return LO16(rtl_snd_drv_set_ticks_w(0x902C, DSEG));
}
