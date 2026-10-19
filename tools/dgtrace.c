/* Who touches which game variable, from where -- the inventory for the
 * multiplayer plan (docs/multiplayer-inventar.md, stage 0).
 *
 * Linked only into build/mtv-dec-trace (make build/mtv-dec-trace): src/ is
 * compiled there with -DDG_TRACE, so every access to the data segment
 * through dg_addr() (src/dec.h: G8/G16/G32, far pointers into DGROUP) calls
 * dg_trace(), and with -finstrument-functions, so a shadow stack knows the
 * decompiled function making it and the parts of the frame it runs under.
 *
 * Context of an access: "world" when one of the world roots (computer
 * players, intruder, elevator, figures, the game clock) is on the stack --
 * the outermost of them names it -- else the innermost of the seat roots
 * (input, clicks, rooms, drawing), else "main" (start-up, game_main's own
 * statements).  The world roots run once per frame for everybody; the seat
 * roots are what every human does in their own view.
 *
 * DG_WATCH=OFF prints who changes the word at DS:OFF (stderr, "[watch]"
 * lines; resolve the addresses with nm against "[watch] base").
 *
 * DG_TRACE=FILE writes at exit: "base ADDR" (the run-time address of
 * dg_trace, to resolve the others with nm) and one line per (offset,
 * function, context): "OFFSET FNADDR CTX COUNT", offsets in hex. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"

#define NOI __attribute__((no_instrument_function))

typedef struct { const char *name; void *fn; int world; } Root;
static Root roots[] = {
    /* the world: once per frame, for every station */
    {"ai_tick", (void *)ai_tick, 1}, {"npc_event_update", (void *)npc_event_update, 1},
    {"elevator_update", (void *)elevator_update, 1}, {"fig_think", (void *)fig_think, 1},
    {"fig_animate", (void *)fig_animate, 1}, {"game_time_tick", (void *)game_time_tick, 1},
    /* a seat: what one human does and sees */
    {"game_frame", (void *)game_frame, 0}, {"input_poll", (void *)input_poll, 0},
    {"mouse_poll", (void *)mouse_poll, 0}, {"ui_on_left_click", (void *)ui_on_left_click, 0},
    {"room_escape", (void *)room_escape, 0}, {"room_enter", (void *)room_enter, 0},
    {"room_return", (void *)room_return, 0}, {"hotkey_handler", (void *)hotkey_handler, 0},
    {"present_frame", (void *)present_frame, 0}, {"room_anim_update", (void *)room_anim_update, 0},
    {"tvfam_tick", (void *)tvfam_tick, 0}, {"madris_step", (void *)madris_step, 0},
    {"tetris_draw", (void *)tetris_draw, 0}, {"tooltip_update", (void *)tooltip_update, 0},
    {"bld_render_floors", (void *)bld_render_floors, 0}, {"bld_draw_side_scenery", (void *)bld_draw_side_scenery, 0},
    {"bld_blit_view", (void *)bld_blit_view, 0}, {"bld_draw_overlays", (void *)bld_draw_overlays, 0},
    {"door_check_entry", (void *)door_check_entry, 0}, {"player_walk_out", (void *)player_walk_out, 0},
    {"day_start_to_boss", (void *)day_start_to_boss, 0}, {"player_fired_sequence", (void *)player_fired_sequence, 0},
    {"ending_play", (void *)ending_play, 0}, {"ui_input_text", (void *)ui_input_text, 0},
    {"screen_wipe", (void *)screen_wipe, 0},
};
#define NROOTS ((int)(sizeof roots / sizeof roots[0]))

/* the shadow stack: function, and the root index it is (-1: none) */
#define DEPTH 4096
static void *st_fn[DEPTH];
static signed char st_root[DEPTH];
static int depth, overflow;
static int world_depth;              /* number of world roots on the stack */

static NOI int root_of(void *fn) {
    for (int i = 0; i < NROOTS; i++) if (roots[i].fn == fn) return i;
    return -1;
}

void __cyg_profile_func_enter(void *fn, void *site) NOI;
void __cyg_profile_func_exit(void *fn, void *site) NOI;
void __cyg_profile_func_enter(void *fn, void *site) {
    (void)site;
    if (depth >= DEPTH) { overflow++; depth++; return; }
    int r = root_of(fn);
    st_fn[depth] = fn; st_root[depth] = (signed char)r;
    if (r >= 0 && roots[r].world) world_depth++;
    depth++;
}
void __cyg_profile_func_exit(void *fn, void *site) {
    (void)fn; (void)site;
    if (depth == 0) return;
    depth--;
    if (depth >= DEPTH) return;
    int r = st_root[depth];
    if (r >= 0 && roots[r].world) world_depth--;
}

/* context: 1 + root index, 0 = main */
static NOI int context(void) {
    int n = depth < DEPTH ? depth : DEPTH;
    if (world_depth > 0) {
        for (int i = 0; i < n; i++) if (st_root[i] >= 0 && roots[st_root[i]].world) return 1 + st_root[i];
    }
    for (int i = n - 1; i >= 0; i--) if (st_root[i] >= 0 && !roots[st_root[i]].world) return 1 + st_root[i];
    return 0;
}

/* (offset, function, context) -> count, open addressing */
typedef struct { void *fn; u32 key; u64 n; } Slot;
#define SLOTS (1u << 20)
static Slot *tab;
static unsigned used;
static const char *out_path;

static NOI void dump(void) {
    FILE *f = fopen(out_path, "w");
    if (!f) { perror(out_path); return; }
    fprintf(f, "base %p\n", (void *)dg_trace);
    fprintf(f, "roots");
    for (int i = 0; i < NROOTS; i++) fprintf(f, " %s%s", roots[i].world ? "W:" : "S:", roots[i].name);
    fprintf(f, "\noverflow %d\n", overflow);
    for (unsigned i = 0; i < SLOTS; i++)
        if (tab[i].n) fprintf(f, "%04X %p %u %llu\n", tab[i].key >> 8, tab[i].fn, tab[i].key & 0xFF, (unsigned long long)tab[i].n);
    fclose(f);
}

/* DG_WATCH=OFF (hex): report every change of the word at DS:OFF, with the
 * frame and the shadow stack of the access before it -- who wrote it (a
 * write through a host pointer shows as the function that made the pointer).
 * Found the corrupted news count of the "Trendy" bug (names/FINDINGS.md). */
extern u8 g_dgroup[];
extern int dg_active;
u64 frame_no(void);
static long watch_off = -1;
static u16 watch_last;
static void *watch_stack[64];
static int watch_depth;
static NOI void watch_check(void) {
    if (watch_off < 0 || !dg_active) return;
    u16 v = (u16)(g_dgroup[watch_off] | g_dgroup[watch_off + 1] << 8);
    if (v != watch_last) {
        fprintf(stderr, "[watch] DS:%04lX %u -> %u at frame %llu, after:", watch_off, watch_last, v, (unsigned long long)frame_no());
        for (int i = watch_depth - 1; i >= 0 && i >= watch_depth - 12; i--) fprintf(stderr, " %p", watch_stack[i]);
        fprintf(stderr, "\n");
        watch_last = v;
    }
    watch_depth = depth < 64 ? depth : 64;
    for (int i = 0; i < watch_depth; i++) watch_stack[i] = st_fn[i];
}

void dg_trace(u16 o) NOI;
void dg_trace(u16 o) {
    static int init;
    watch_check();
    if (!init) {
        init = 1;
        out_path = getenv("DG_TRACE");
        if (getenv("DG_WATCH")) { watch_off = strtol(getenv("DG_WATCH"), NULL, 16); fprintf(stderr, "[watch] base %p\n", (void *)dg_trace); }
        if (out_path) { tab = calloc(SLOTS, sizeof *tab); atexit(dump); }
    }
    if (!tab || o >= DG_END) return;
    void *fn = depth > 0 && depth <= DEPTH ? st_fn[depth - 1] : NULL;
    u32 key = (u32)o << 8 | (u32)context();
    u32 h = (key * 2654435761u) ^ (u32)((uintptr_t)fn >> 4) * 40503u;
    for (unsigned i = h & (SLOTS - 1);; i = (i + 1) & (SLOTS - 1)) {
        if (!tab[i].n) {
            if (used + 1 >= SLOTS / 2) return;           /* full: drop (reported as missing counts) */
            tab[i].fn = fn; tab[i].key = key; tab[i].n = 1; used++;
            return;
        }
        if (tab[i].key == key && tab[i].fn == fn) { tab[i].n++; return; }
    }
}
