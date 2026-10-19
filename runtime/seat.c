/* Seats and their scheduler (seat.h). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "runtime.h"
#include "coro.h"
#include "seat.h"
#include "net.h"

int seats_n;
int seat_cur = -1;
int seat_humans;
int seat_station[SEATS_MAX];
int seat_out[SEATS_MAX];
int seat_paused[SEATS_MAX];
int seat_clock_world;

int seat_paused_by_other(void) {
    if (!seat_humans) return 0;
    for (int i = 0; i < seats_n; i++)
        if (i != seat_cur && seat_paused[i] && !seat_out[i]) return 1;
    return 0;
}
static int spectator[SEATS_MAX];         /* a seat without a station, or out of the game: it only watches */
int seat_frozen;
uint32_t seat_heap_peak[SEATS_MAX];

#define SEAT_STACK (8u << 20)            /* C stack of a seat: the game's deepest calls use far less */
#define VRAM_LIN 0xA0000u
#define VRAM_LEN 0x10000u
#define ESTACK_LIN 0x45F70u              /* the program's stack segment 45F7, 0000..1FFF */
#define WINDOW_LIN ((u32)SEAT_HEAP_SEG << 4)
#define WINDOW_LEN (((u32)SEAT_HEAP_END - SEAT_HEAP_SEG) << 4)
#define ESTACK_LEN 0x2000u

/* what a seat owns, saved while another seat runs */
typedef struct {
    u8 *game; u32 game_cap;              /* the game's seat block (src/seatctx.c) */
    u8 vram[VRAM_LEN];                   /* the screen */
    u8 estack[ESTACK_LEN];               /* the emulated stack: game_main's frames */
    u8 window[WINDOW_LEN];               /* the seat window: its own far heap (seats but 0) */
    CPU r;                               /* the registers */
    u8 *dev;                             /* VGA (with the planar memory), palette, keyboard and mouse */
} SeatCtx;

static coro *seat_coro[SEATS_MAX];
static SeatCtx *ctx[SEATS_MAX];
static int loaded;                       /* whose state is in memory */
static void (*seat_entry)(void);
static uint64_t round_no;
static int frame_end;                    /* seat 0 yielded at the end of its frame (not in a wait) */
static uint64_t turns[SEATS_MAX];        /* turns each seat had (SEAT_STATS=1 prints them at exit) */
static uint64_t gap_max[SEATS_MAX], gap_at[SEATS_MAX];   /* the most frames between two turns of a seat, and where */
static uint64_t world_last, world_gap, world_gap_at;      /* the same for the world: seat 0's frame ends */
static void seat_stats(void) {
    fprintf(stderr, "[seats] rounds %llu, turns:", (unsigned long long)round_no);
    for (int i = 0; i < seats_n; i++) fprintf(stderr, " %llu", (unsigned long long)turns[i]);
    fprintf(stderr, "\n");
    for (int i = 1; i < seats_n; i++)
        fprintf(stderr, "[seats] seat %d waited at most %llu frames for a turn (until frame %llu)\n", i,
                (unsigned long long)gap_max[i], (unsigned long long)gap_at[i]);
    fprintf(stderr, "[seats] the world stood at most %llu frames (until frame %llu)\n",
            (unsigned long long)world_gap, (unsigned long long)world_gap_at);
    for (int i = 1; i < seats_n; i++)
        fprintf(stderr, "[seats] seat %d's heap reached %u of %u bytes\n", i, (unsigned)seat_heap_peak[i], (unsigned)WINDOW_LEN);
}

uint64_t seat_rounds(void) { return round_no; }

extern int host_view_seat;
int seat_sound_seat(void) { return net_active ? net_seat : host_view_seat; }

void mp_seat_out(void) {
    if (seat_cur < 0 || seat_cur >= SEATS_MAX || seat_out[seat_cur]) return;
    seat_out[seat_cur] = 1;
    spectator[seat_cur] = 1;
    if (net_active && seat_cur == net_seat) net_send("OUT");   /* the server need not wait for this seat */
    fprintf(stderr, "[seats] seat %d (station %d) is out of the game, frame %llu\n", seat_cur, seat_station[seat_cur], (unsigned long long)frame_no());
    for (int i = 0; i < seats_n; i++)
        if (seat_station[i] && !seat_out[i]) return;
    fprintf(stderr, "[seats] every human is out: the game ends, frame %llu\n", (unsigned long long)frame_no());
    fflush(stdout);
    exit(0);
}

/* SEAT_CHECK=1: the leak check -- seat 0's memory as it left it must be
 * what it finds when it runs again; any byte another seat changed and the
 * switch did not put back is printed (the first few, with the frame) */
static u8 *check_mem;
static int check_on = -1, check_reports;
static void check_before(void) {
    if (check_on < 0) check_on = getenv("SEAT_CHECK") != NULL;
    if (!check_on) return;
    if (!check_mem && !(check_mem = malloc(0x100000))) { check_on = 0; return; }
    memcpy(check_mem, M, 0x100000);
}
static void check_after(void) {
    if (!check_on || !check_mem || check_reports > 20 || !memcmp(M, check_mem, 0x100000)) return;
    for (u32 a = 0; a < 0x100000; a++) {
        if (M[a] == check_mem[a]) continue;
        u32 e = a;
        while (e + 1 < 0x100000 && (M[e + 1] != check_mem[e + 1] || (e + 2 < 0x100000 && M[e + 2] != check_mem[e + 2]))) e++;
        fprintf(stderr, "[seat check] frame %llu: %05X..%05X changed by another seat (was %02X, now %02X)\n",
                (unsigned long long)frame_no(), a, e, check_mem[a], M[a]);
        if (++check_reports > 20) { fprintf(stderr, "[seat check] more ...\n"); return; }
        a = e;
    }
}

/* the device state a seat owns, as StateBlock lists */
static int dev_blocks(StateBlock *b) {
    int n = vga_seat_blocks(b);
    n += hw_seat_blocks(b + n);
    n += bios_state_blocks(b + n);
    return n;
}

static void ctx_save(int i) {
    SeatCtx *c = ctx[i];
    u32 need = seat_game_size();
    if (need > c->game_cap) { free(c->game); c->game = malloc(c->game_cap = need + 4096); }
    if (!c->game || !seat_game_save(c->game, c->game_cap)) { fprintf(stderr, "seat: cannot save seat %d\n", i); exit(1); }
    memcpy(c->vram, M + VRAM_LIN, VRAM_LEN);
    memcpy(c->estack, M + ESTACK_LIN, ESTACK_LEN);
    memcpy(c->window, M + WINDOW_LIN, WINDOW_LEN);
    c->r = R;
    hr_seat_save(i);                     /* --filter hr: the cells of all that (src/hr.c) */
    StateBlock b[64]; int n = dev_blocks(b); u32 k = 0;
    if (!c->dev) {
        u32 len = 0;
        for (int j = 0; j < n; j++) len += b[j].n;
        if (!(c->dev = malloc(len))) { fprintf(stderr, "seat: out of memory\n"); exit(1); }
    }
    for (int j = 0; j < n; j++) { memcpy(c->dev + k, b[j].p, b[j].n); k += b[j].n; }
}
static void ctx_load(int i) {
    SeatCtx *c = ctx[i];
    seat_game_load(c->game);
    memcpy(M + VRAM_LIN, c->vram, VRAM_LEN);
    memcpy(M + ESTACK_LIN, c->estack, ESTACK_LEN);
    memcpy(M + WINDOW_LIN, c->window, WINDOW_LEN);
    hr_seat_load(i);
    R = c->r;
    StateBlock b[64]; int n = dev_blocks(b); u32 k = 0;
    for (int j = 0; j < n; j++) { memcpy(b[j].p, c->dev + k, b[j].n); k += b[j].n; }
}

/* While a seat other than 0 runs, the world's time stands: no events, no
 * interrupts (seat_frozen), and time, timers, the sound chip and the file
 * positions are put back afterwards -- what it does takes no time. */
int dos_snapshot_pos(long *pos, int n);
void dos_restore_pos(const long *pos, int n);
static int world_blocks(StateBlock *b) {
    int n = hw_world_blocks(b);
    n += adlib_state_blocks(b + n);
    return n;
}
static struct { u64 T; u8 *dev; long fpos[64]; } world;
static void world_freeze(void) {
    StateBlock b[64]; int n = world_blocks(b); u32 k = 0;
    if (!world.dev) {
        u32 len = 0;
        for (int j = 0; j < n; j++) len += b[j].n;
        if (!(world.dev = malloc(len))) { fprintf(stderr, "seat: out of memory\n"); exit(1); }
    }
    world.T = T;
    for (int j = 0; j < n; j++) { memcpy(world.dev + k, b[j].p, b[j].n); k += b[j].n; }
    dos_snapshot_pos(world.fpos, 64);
    seat_frozen = 1;
}
static void world_thaw(void) {
    seat_frozen = 0;
    T = world.T;
    StateBlock b[64]; int n = world_blocks(b); u32 k = 0;
    for (int j = 0; j < n; j++) { memcpy(b[j].p, world.dev + k, b[j].n); k += b[j].n; }
    dos_restore_pos(world.fpos, 64);
}

static void seat_main(void *arg) {
    (void)arg;
    seat_entry();                        /* the program: start-up code, game_main */
}
static void spectator_main(void *arg) {
    (void)arg;
    seat_spectator_main();
}
static void human_main(void *arg) {
    (void)arg;
    seat_human_main();
}

/* seat 1 joins as a spectator: a copy of seat 0's view, its own input */
static void seat_join(int i) {
    ctx[i] = calloc(1, sizeof *ctx[i]);
    if (!ctx[i]) { fprintf(stderr, "seat: out of memory\n"); exit(1); }
    ctx_save(i);                         /* seat 0's state, loaded now, as the start */
    spectator[i] = !seat_station[i] || seat_out[i];   /* (out: a saved game's fired human, src/mpsave.c -- watching) */
    seat_coro[i] = coro_new(seat_station[i] ? human_main : spectator_main, NULL, SEAT_STACK);
    if (!seat_station[i])
        fprintf(stderr, "[seats] seat %d joins as a spectator in round %llu, frame %llu\n", i, (unsigned long long)round_no, (unsigned long long)frame_no());
    else
        fprintf(stderr, "[seats] seat %d joins as the human on station %d%s in round %llu, frame %llu\n", i, seat_station[i],
                seat_out[i] ? " (out of the game: watching)" : "", (unsigned long long)round_no, (unsigned long long)frame_no());
}

void seats_run(void (*entry)(void)) {
    seat_entry = entry;
    if (getenv("SEAT_STATS")) atexit(seat_stats);
    for (int i = 0; i < seats_n; i++) {
        ctx[i] = NULL;
    }
    ctx[0] = calloc(1, sizeof *ctx[0]);
    seat_coro[0] = coro_new(seat_main, NULL, SEAT_STACK);
    int fresh[SEATS_MAX] = {0};
    u64 last_frame[SEATS_MAX] = {0};
    for (;;) {
        int alive = 0;
        for (int i = 0; i < seats_n; i++) {
            if (!seat_coro[i] || coro_done(seat_coro[i])) continue;
            /* the other seats run once per video frame: seat 0's game_frame
             * runs many times a frame in some rooms (no retrace wait there) */
            if (i > 0) {
                if (frame_no() == last_frame[i]) { alive = 1; continue; }
                if (last_frame[i] && frame_no() - last_frame[i] > gap_max[i]) { gap_max[i] = frame_no() - last_frame[i]; gap_at[i] = frame_no(); }
                last_frame[i] = frame_no();
            }
            if (loaded != i) {
                if (loaded == 0) check_before();
                ctx_save(loaded); ctx_load(i);
                if (i == 0) check_after();
                loaded = i;
            }
            if (i > 0 && !fresh[i]) { bios_input_clear(); fresh[i] = 1; }
            if (i > 0) world_freeze();
            seat_cur = i;
            if (i == 0) seat_frame_timer_fresh();   /* (another seat restarted the world's timer: src/seatctx.c) */
            if (i > 0) { seat_game_turn(i); host_seat_input(i, spectator[i]); seat_game_ptrcheck_before(); }
            turns[i]++;
            coro_resume(seat_coro[i]);
            if (i > 0) seat_game_ptrcheck();
            seat_cur = -1;
            if (i > 0) world_thaw();
            alive |= !coro_done(seat_coro[i]);
        }
        round_no++;
        if (!alive) break;
        for (int i = 1; i < seats_n; i++)          /* a seat joins at the end of a frame of seat 0's */
            if (!seat_coro[i] && loaded == 0 && frame_end && seat_game_join_ok()) seat_join(i);
    }
}

/* the game's part of a seat's state only: its seat block (with the buffers'
 * contents), its screen, its heap window -- not the registers, the emulated
 * stack, the devices (mp_enter_player) */
static void game_save(int i) {
    SeatCtx *c = ctx[i];
    u32 need = seat_game_size();
    if (need > c->game_cap) { free(c->game); c->game = malloc(c->game_cap = need + 4096); }
    if (!c->game || !seat_game_save(c->game, c->game_cap)) { fprintf(stderr, "seat: cannot save seat %d\n", i); exit(1); }
    memcpy(c->vram, M + VRAM_LIN, VRAM_LEN);
    memcpy(c->window, M + WINDOW_LIN, WINDOW_LEN);
    hr_seat_save(i);
}
static void game_load(int i) {
    SeatCtx *c = ctx[i];
    seat_game_load(c->game);
    memcpy(M + VRAM_LIN, c->vram, VRAM_LEN);
    memcpy(M + WINDOW_LIN, c->window, WINDOW_LEN);
    hr_seat_load(i);
}
static int entered = -1, entered_from;   /* mp_enter_player: the seat whose game is in, and whose was */
int mp_enter_player(int p) {
    if (!seat_humans || entered >= 0 || seat_cur < 0) return 0;
    for (int i = 0; i < seats_n; i++) {
        if (seat_station[i] != p) continue;
        if (i == loaded || !ctx[i] || !seat_coro[i]) return 0;
        game_save(loaded);
        game_load(i);
        entered = i; entered_from = loaded;
        seat_cur = i;
        return 1;
    }
    return 0;
}
void mp_leave(void) {
    if (entered < 0) return;
    game_save(entered);
    game_load(entered_from);
    seat_cur = entered_from;
    entered = -1;
}

/* the seat window of seat i, where it is not the one in memory (NULL: it
 * is, or seat i has not joined) */
const unsigned char *seat_window_of(int i) {
    if (i < 0 || i >= seats_n || !ctx[i] || !seat_coro[i]) return NULL;
    if (i == (entered >= 0 ? entered : loaded)) return NULL;
    return ctx[i]->window;
}
int seat_joined(int i) { return i >= 0 && i < seats_n && ctx[i] && seat_coro[i]; }

static int viewing = -1;                  /* seat_view: whose state it put in memory */
int seat_view(int i, void (*fn)(const char *), const char *arg) {
    if (i < 0 || i >= seats_n || !ctx[i]) return 0;
    if (i != loaded) { ctx_save(loaded); ctx_load(i); }
    viewing = i;
    fn(arg);
    viewing = -1;
    if (i != loaded) ctx_load(loaded);
    return 1;
}
int seat_in_memory(void) { return viewing >= 0 ? viewing : loaded; }

void seat_yield(void) {
    if (!seats_n || seat_cur < 0 || entered >= 0) return;
    if (seat_cur == 0) {
        frame_end = 1;
        if (world_last && frame_no() - world_last > world_gap) { world_gap = frame_no() - world_last; world_gap_at = frame_no(); }
        /* SEAT_GAPS=1: every time the world stood more than 15 frames (seat 0 had no frame end) */
        if (getenv("SEAT_GAPS") && world_last && frame_no() - world_last > 15) fprintf(stderr, "[gap] %llu..%llu (%llu)\n", (unsigned long long)world_last, (unsigned long long)frame_no(), (unsigned long long)(frame_no() - world_last));
        world_last = frame_no();
    }
    coro_yield();
}

void seat_pass(void) {
    static u64 passed;
    if (seats_n < 2 || seat_cur < 0 || entered >= 0) return;
    if (seat_cur == 0) {
        if (frame_no() == passed) return;
        passed = frame_no();
        frame_end = 0;
    }
    coro_yield();
}

/* Seat 0 changes rooms -- loads the room's pictures and texts (disk_access_begin/
 * end), draws it, or the building on the way back (room_enter, room_return) --
 * for up to a second and a half of frames, and the other seats had no turn
 * all that time: their screens stood still while the host changed rooms (the
 * other way round nobody waits: a seat's own load takes the world no time).
 * Now at every frame of it (hw.c) the others have their turn as at a wait
 * point.  What they could touch of seat 0's half-done load is its own:
 * its seat block (the decoders' state, the text index), heap window, screen,
 * registers and stack, saved and put back with it; the resource files stay
 * open (the nesting counter is shared, a load of theirs inside it neither
 * opens nor closes them) and their positions are put back after a turn
 * (world_thaw).  Not in the interpreter (the machine-level build). */
void seat_disk_pass(void) {
    if (seats_n < 2 || seat_cur != 0 || entered >= 0 || interp_depth || !seat_disk_busy()) return;
    seat_pass();
}

int world_now(void) {
    return !seats_n || (seat_cur == 0 && !seat_paused_by_other());   /* (a human's pause stops the world) */
}
