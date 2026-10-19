/* Seats: one human each, every seat a coroutine (coro.h) on the main
 * thread, run in a fixed order by the scheduler (docs/multiplayer.md,
 * stage 1).  Off by default: without --seats the game runs as it always
 * did, with no scheduler at all.
 *
 * A round: every seat runs once, up to its next seat_yield() -- the end of
 * its game_frame.  The world parts (computer players, intruder, elevator
 * and figures, the game clock: ai_tick, npc_event_update, elevator_update,
 * fig_think, fig_animate, game_time_tick) run in seat 0, wherever its code
 * calls them (game_frame, bld_update_frame during a room change, ...); the
 * other seats skip them (world_now()).  With one seat that is the game's
 * own order: --seats 1 plays the same game, bit for bit
 * (tools/seattest.py).  With --seats 2 a second seat joins as a spectator
 * (stage 2): it shows the building with its own screen, data (the seat
 * block, src/seatctx.c), devices and far heap (the seat window), and seat
 * 0's game stays the single-player game, bit for bit.  What the world draws
 * for seat 0 -- the camera, the figures, the doors, the clock and the panel
 * -- the other seats draw for themselves (seat_fig_*, seat_req_door,
 * seat_status_tick).  Input per seat: the script's "N seat S ..." lines
 * (host_seat_input), applied as seat S is about to run; a spectator takes
 * only motion.  Seat requests (seat_request): the day's end; wait points
 * (seat_pass) where seat 0 waits for the retrace or the timer, so the
 * others run through its wipes and walks.  Still to come with real second
 * humans: the morning at the boss, being caught, the info board's time
 * limit. */
#ifndef MTV_SEAT_H
#define MTV_SEAT_H
#include <stdint.h>

#define SEATS_MAX 3
/* the seat window: a far heap of its own for every seat but 0 (src/seatctx.c),
 * in memory the game does not use: B000..E7FF, 224 KB (A000 is the screen,
 * E800 a mod's ads 51.., F000 the BIOS; the text screen at B800 is cleared
 * only when a text mode is set, the BIOS font the runtime names at C000 is
 * never asked for).  96 KB (D000..) were too little for a fired human's
 * sequence */
#define SEAT_HEAP_SEG 0xB000
#define SEAT_HEAP_END 0xE800
void bios_input_clear(void);           /* runtime/bios.c: a new seat's input starts empty */
extern int seats_n;              /* 0: no scheduler (the default); 1..SEATS_MAX: --seats N */
extern int seat_cur;             /* index of the running seat, -1 outside any */
extern int seat_humans;          /* 0, or with --humans the number of human seats (seat i plays seat_station[i]) */
extern int seat_station[SEATS_MAX];
extern int seat_out[SEATS_MAX];  /* the human of that seat is out of the game (fired): it only watches */
extern int seat_paused[SEATS_MAX];   /* that seat's human paused the game (the pause room, -5) */
int seat_paused_by_other(void);  /* another human paused: the world stands, this seat waits (--humans) */
extern int seat_clock_world;     /* the world's clock is being worked (game_time_tick, the start, the pause) */
/* game_clock_* do nothing: another seat's turn, or with --humans any seat's
 * own doings (room changes, dialogs, loading) -- the clock is the world's */
static inline int mp_clock_skip(void) { return (seat_cur > 0 || (seat_humans && seat_cur == 0)) && !seat_clock_world; }
void mp_seat_out(void);          /* the running seat's human is out; when all are, the game ends */
extern uint32_t seat_heap_peak[SEATS_MAX];   /* bytes of the seat window a seat's heap reached (SEAT_STATS=1) */

void seats_run(void (*entry)(void));   /* run the program as seat 0 under the scheduler */
void seat_yield(void);                 /* the running seat's frame is done (no-op without seats) */
/* a wait point (a busy wait for the retrace or the timer): seat 0 lets the
 * others run once a new video frame has begun, another seat always passes
 * (its time stands while it runs) */
void seat_pass(void);
void seat_disk_pass(void);
void seat_frame_timer_fresh(void);     /* src/seatctx.c: seat 0 about to run -- its frame timer after another seat restarted the timer */             /* seat 0 loads (disk access): a turn for the others each frame */
int seat_disk_busy(void);              /* src/system.c: seat 0 loads or changes rooms */
extern int seat_disk_depth[], seat_room_depth[];   /* src/system.c: per seat, disk accesses and room changes it is in */
int world_now(void);                   /* 1 if the running seat runs the world parts (seat 0; always without seats) */
uint64_t seat_rounds(void);            /* rounds completed */
/* fn(arg) with seat i's state in memory -- its screen, palette, data -- and
 * the running seat's back afterwards, exactly; 0 if seat i is not playing */
int seat_view(int i, void (*fn)(const char *), const char *arg);
int seat_in_memory(void);              /* whose state is in memory: the running seat's, or seat_view's */
/* runtime/host.c: seat i's script lines that are due, applied as it is about
 * to run (its keyboard and mouse in memory); a spectator takes only motion */
void host_seat_input(int seat, int spectator);

/* the game's side (src/seatctx.c; weak defaults in runtime/clock_none.c) */
unsigned seat_game_size(void);
unsigned seat_game_save(unsigned char *buf, unsigned cap);
void seat_game_load(const unsigned char *buf);
int seat_game_join_ok(void);
void seat_spectator_main(void);
void seat_human_main(void);            /* a human seat: its station's first morning, then the main loop */
void seat_game_ptrcheck(void);         /* SEAT_PTRCHECK=1: shared pointers into a seat's heap (src/seatctx.c) */
void seat_game_ptrcheck_before(void);
int mp_is_human(int player);           /* the player (1..3) is a human: == g_player without --humans */
/* run a piece of the world as the human of station p (another seat's): its
 * seat block, heap, screen and buffers swapped in until mp_leave(), where
 * g_player is p -- the human's branch, its ledger, its office.  Returns 1 if
 * it switched (then call mp_leave), 0 if p is no other seat's human.  No
 * seat yields meanwhile. */
int mp_enter_player(int p);
void mp_leave(void);
void mp_for_other_humans(void (*fn)(void));   /* fn for every other seat's human, as that human */
void mp_daily_boss_update(void);                /* daily_boss_update for every human (src/seatctx.c) */
void mp_office_new_day(void);                   /* the office's new day of a human (another pile of paper) */
int mp_third(int player);
int mp_bailiff_target(void);           /* the most indebted human below -$100 000, or 0 (only g_player without --humans) */
int mp_leading_human(void);            /* the human with the highest image (g_player without --humans) */
int mp_nobody_watches(void);           /* no human looks at the building's lower floors (the intruder may start) */              /* the station that is neither the player nor g_player: in the
                                          single-player game the other computer player */
void seat_game_turn(int seat);         /* a seat but 0 is about to run, its state loaded, the world frozen */
int seat_sound_seat(void);             /* the seat this machine hears: the network game's, --view-seat, else 0 */
int seat_joined(int i);
const unsigned char *seat_window_of(int i);   /* seat i's window, where another seat's is in memory (else NULL) */
/* a seat's view of the world (src/seatctx.c): fig_animate notes what it
 * draws, the other seats draw it; their clock and money */
void seat_fig_begin(void);
void seat_fig_note(unsigned fig, unsigned anim_frame);
void seat_figures_draw(void);
void seat_status_tick(void);
void seat_req_door(unsigned floor, unsigned door, unsigned mode);
void seat_doors_draw(void);
/* seat requests: the world (seat 0) gives them, every other seat carries
 * them out at the start of its next frame (seat_requests_run) */
enum { SEAT_REQ_DAY_END = 1, SEAT_REQ_DAY_DARK, SEAT_REQ_DAY_START, SEAT_REQ_CAUGHT };
int mp_catch_human(int player, int by);   /* that human is thrown out with a fine (its seat); 1 if it is a seat's */
void mp_requests_seat0(void);          /* seat 0's requests at the top of game_main's loop (--humans) */
/* knocking (--knock, src/seatctx.c): a human finds a human in the film dealer's, the ad agency
 * or the supermarket (door_check_entry) and knocks; the room kept for the knocker afterwards */
void mp_knock(int knocker, int inside, unsigned floor, unsigned door, unsigned room);
int mp_knock_held(unsigned fig, unsigned floor, unsigned door);   /* 1: the room waits for another */
void mp_knock_status_text(u32 buf);       /* the status line's 0x12B for a knocker: the minutes left */
void seat_request(int kind);
void seat_requests_run(void);
void mp_wait_humans_home(void);        /* the day's end waits for every human seat's figure */
#endif
