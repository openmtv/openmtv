#ifndef MTV_RUNTIME_H
#define MTV_RUNTIME_H
#include <stdint.h>
#include <stdio.h>
#include "cpu.h"

typedef uint64_t u64;

/* ---- memory map ---- */
#define PSP_SEG      0x01A0
#define LOADSEG_C    0x01B0          /* PSP + 0x10: where the EXE image goes */
#define ENV_SEG      0x0180
#define BIOS_SEG     0xF000          /* IVT stubs: vector n -> F000:n */
#define MEM_TOP_SEG  0xA000

/* ---- the program image without the EXE ----
 * What the decompiled build needs of MTV.EXE is data: the initial data
 * segment (from DGROUP to the end of the load module) with the relocations
 * inside it, and the header's start values.  tools/exeimage.py writes them
 * into build/exe_image.c at build time; runtime/main.c loads them instead
 * of the EXE unless a mode runs the original's machine code (--interp,
 * --difftest, --stackchk) or --exe names one.  Without an EXE the code
 * segments stay empty: the decompiled build does not read them, except for
 * the 202 bytes of segment 371E that code_init (src/rtl_snd.c) writes. */
struct builtin_image {
    u32 image_bytes;                 /* size of the load module */
    u32 data_off, data_len;          /* the bytes kept: image offset and length */
    const u8 *data;                  /* as in the EXE (not relocated) */
    u32 nrel;                        /* relocations inside them (offsets from data_off) */
    const u32 *rel;
    u32 nrel_exe;                    /* relocations of the whole EXE (for the [load] line) */
    u16 ss, sp, cs, ip;              /* start values, relative to the load segment */
    void (*code_init)(void);         /* writes what is still needed of the code segments */
    const char *sha256;              /* of the EXE it was made from */
};
extern const struct builtin_image *builtin_image;   /* NULL: none in this build */
extern int exe_code_loaded;          /* 0: the code segments are empty (runtime/dispatch.c) */

/* ---- virtual time ----
 * T counts executed 8086 instructions.  All timing (PIT, retrace, BIOS
 * ticks) derives from it, which makes a run fully deterministic for a given
 * input script.  In real-time mode the host sleeps whenever T runs ahead of
 * the wall clock. */
extern u64 T;
extern u64 next_event_T;
extern double insns_per_us;          /* virtual CPU speed */

/* ---- logical clock (--logical-time) ----
 * Instruction time differs between the engines: the decompiled build only
 * approximates the original's instruction counts.  The logical clock counts
 * what every engine does alike if the program logic is the same: entries
 * into the decompiled game functions (the gen prologue, FUNC_TICK in src/,
 * calls the interpreter runs), I/O operations (port accesses, BIOS/DOS
 * services) and the runtime's own waits (lt_extra).  With --logical-time,
 * T is set from them wherever time is observed (poll points, I/O), so the
 * original build and the native decompiled build play the same game. */
extern int logical_time;
extern u64 clock_entries_n, clock_io_n, clock_extra;
extern u8 *clock_map;                /* image-relative linear entries counted */
#define CLOCK_PER_ENTRY 25
#define CLOCK_PER_IO    10
#define LOGICAL_SYNC() do { if (logical_time) T = clock_entries_n * CLOCK_PER_ENTRY + clock_io_n * CLOCK_PER_IO + clock_extra; } while (0)
extern int clock_trace;               /* CLOCK_TRACE=F0-F1: log entries and I/O (tools/clockdiff.py) */
void clock_trace_line(const char *fmt, ...);
static inline void clock_entry(u16 relseg, u16 off) {
    u32 r = ((u32)relseg << 4) + off;
    if (r < 0x50000 && ((clock_map[r >> 3] >> (r & 7)) & 1)) {
        clock_entries_n++;
        if (clock_trace) clock_trace_line("E %04X:%04X", relseg, off);
    }
}
/* an I/O operation: port (what < 0x10000) or BIOS/DOS service 0x10000 | int */
static inline void clock_io(u32 what) {
    clock_io_n++; LOGICAL_SYNC();
    if (clock_trace) clock_trace_line(what < 0x10000 ? "I port %04X" : "I int %02X", what & 0xFFFF);
}
/* the runtime lets time pass (polling costs, waits) */
static inline void time_advance(u64 n) { if (logical_time) { clock_extra += n; LOGICAL_SYNC(); } else T += n; }
static inline void time_skip_to(u64 t) {
    if (logical_time) { LOGICAL_SYNC(); if (t > T) clock_extra += t - T; LOGICAL_SYNC(); }
    else if (t > T) T = t;
}
void clock_init(void);
void runtime_events(void);
void runtime_halt_wait(void);
/* game state kept in host structures by the decompiled build (src/state.c):
 * the original layout in the emulated memory, written back / read in */
void state_activate(void);
int native_decompiled(u16 seg, u16 off);   /* src/ C, not lifted machine code */
void state_export(void);
void state_drop(void);
void state_reload(void);
void runtime_sched(void);            /* recompute next_event_T */

/* ---- recompiled code ---- */
typedef void (*native_fn)(void);
typedef struct { u32 key; native_fn fn; } native_entry;   /* key = relseg<<16 | off */
extern const native_entry native_table[];
extern const int native_count;
native_fn native_lookup(u16 seg, u16 off);
native_fn native_lookup_any(u16 seg, u16 off);
extern int difftest_on, difftest_active;
extern const char *fix_spec;                    /* --fixes[=LIST] (src/fixes.c); NULL = classic */
int fix_init(void);                             /* 1 if the build has fixes (src/fixes.c) */
extern long start_money_opt;                    /* --start-money / [game] start_money: the human's cash in a new game; -1 = the original's (src/fixes.c) */
#define START_MONEY_MAX 100000000L               /* $100 million: the status panel shows 6 digits of thousands */
extern long credit_line_opt;                    /* --credit-line / [game] credit_line: the boss's credit line for image 10..49 (+$100 000 a day from day 2); -1 = the original's (src/fixes.c), CREDIT_LINE_ORIGINAL chosen */
#define CREDIT_LINE_MIN 300000L                  /* the original's: $200 000 at the start + $100 000 from day 2 */
#define CREDIT_LINE_MAX 100000000L
#define CREDIT_LINE_DEFAULT 700000L              /* the settings file's and the launcher's default */
#define CREDIT_LINE_ORIGINAL 0L                  /* --credit-line=original: the original's line, chosen -- and fix credit-repay off, also in a network game */
extern long terror_damage_opt;                  /* --terror-damage / [game] terror_damage: a terrorist's bomb costs the room's owner this, 0 nothing (and no message); -1 = the original's $50 000 (src/fixes.c) */
#define TERROR_DAMAGE_ORIGINAL 50000L
extern long sammy_prize_opt[4];                 /* --sammy-prize / [game] sammy_news, sammy_culture, sammy_shows, sammy_series: the Sammy winner's prize money by category; -1 = none, as in the original (src/fixes.c) */
#define SAMMY_PRIZE_OPT(i) (sammy_prize_opt[i] < 0 ? 0L : sammy_prize_opt[i])   /* category i + 1's, 0 = none */
#define MONEY_OPT_MAX 100000000L                 /* the amounts' highest (as START_MONEY_MAX) */
extern int fourth_speed_opt;                    /* --fourth-speed / [game] fourth_speed: a fourth, fastest speed in the Control-Center (src/fixes.c) */
extern int fast_rooms_opt;                      /* --fast-rooms / [game] fast_rooms: room changes without the original's waits */
extern int series_top_opt;                      /* --series-topicality / [game] series_topicality: the series card shows the topicality of all episodes (src/fixes.c) */
extern int ai_improve_opt;                      /* --ai-improve / [game] ai_improve: 0 off, 1 normal, 2 strong -- computer players follow the human's transmitters (src/ai_improve.c) */
extern int ad_offers_opt;                       /* --ad-offers / [game] ad_offers: the ad agency's offers, 10..17; 0 = the original's 10 (src/fixes.c) */
#define AD_OFFERS_OPT_MIN 10                     /* the original's (its desk: three stacks of 3/4/3) */
#define AD_OFFERS_OPT_MAX 17                     /* a front row of 13 and a back row of 4 */
#define AD_OFFERS_OPT() (ad_offers_opt > AD_OFFERS_OPT_MIN ? ad_offers_opt : AD_OFFERS_OPT_MIN)   /* the number chosen, 10 = the original's */
#define AD_OFFERS_SEG 0xEE00                     /* offers 11.. in the decompiled build's host memory (src/state.c); the state's hash covers them */
extern int film_offers_opt;                     /* --film-offers / [game] film_offers: the film dealer's offers, 10..17; 0 = the original's 10 (src/film_offers.c) */
#define FILM_OFFERS_OPT_MIN 10                   /* the original's (its counter: a row of 10 tapes) */
#define FILM_OFFERS_OPT_MAX 17                   /* 7 more tapes to the left */
#define FILM_OFFERS_OPT() (film_offers_opt > FILM_OFFERS_OPT_MIN ? film_offers_opt : FILM_OFFERS_OPT_MIN)   /* the number chosen, 10 = the original's */
#define FILM_OFFERS_SEG 0xEE10                   /* the dealer's slots 11.. in host memory (src/state.c); the state's hash covers them */
#define FILM_RECTS_SEG 0xEE20                    /* their shelf rectangles (src/film_offers.c) */
extern int seat_humans;                         /* runtime/seat.h: with --humans the number of human seats, else 0 */
/* a room change without the original's waits -- the wipe's delay per step, the sprite
 * decoder's time, the sound driver's AdLib accesses at a tenth (src/main_loop.c,
 * src/huffman.c, hw.c): always with several humans (the world runs in the host's frames
 * and stood still for everyone), alone the player's choice; never under --difftest */
static inline int fast_rooms(void) { return seat_humans || (fast_rooms_opt && !difftest_on); }
extern int new_game_opt;                        /* --new-game: no intro, the setup's Start at once (src/main_loop.c; a network game's start) */
extern int difficulty_opt;                      /* --difficulty N (1..3) for --new-game; 0: the setup's */
extern char new_game_names[3][9];               /* --names A,B,C: the managers of stations 1..3 for --new-game ("": the game's) */
extern int notice_german;                       /* the game in German (--args d): the window's notices too */
void host_notice(const char *utf8, double seconds);   /* a notice over the picture (a network game's news), "[notice]" on stderr */
void host_notice_hold(double seconds);          /* keep the window (and the notice) a moment: before a network game's exit */
void mp_notice_speed(int speed);                /* a human set the game speed: the others see who and which (host.c) */
extern int knock_opt;                           /* --knock=N: a human knocking at an occupied film dealer, ad agency or supermarket
                                                   gives the human inside N game minutes, then it is out (src/seatctx.c); 0 off */
#define KNOCK_OPT_MAX 120
extern long seed_opt;                           /* --seed N: --new-game starts as if N ticks were spent on the setup screen (randomness) */
extern const char *mp_save_dir;                 /* --mp-save DIR: a network game saves each night there (src/mpsave.c) */
extern int mp_load_opt;                         /* --mp-load FILE: a network game starts from that savegame (src/mpsave.c) */
extern const u8 *mp_load_data;                  /*   its bytes */
extern u32 mp_load_size;
extern u64 mp_files_hash;                       /* the game files' hash a network game compares (dos_game_files_hash), 0 if not made */
int mpsave_check(const u8 *data, u32 size, char *why, unsigned whylen);   /* a network game's savegame? (src/mpsave.c) */
extern int stackchk_on, stackchk_hits;          /* --stackchk (interp.c) */
extern int stk_who_on;                          /* --stackchk: note the last writer of each byte */
void scrub_popped(int n);                       /* clear the n bytes just popped */
extern u16 stackchk_hit_cs, stackchk_hit_ip;
extern u8 stk_dead[];
extern u32 difftest_port_seq;
int difftest_call(u16 seg, u16 off, native_fn f, int far);
void difftest_abort(const char *why);
void difftest_summary(void);
/* Device state outside the 1 MB of memory (PIC/PIT, VGA, mouse, DTA, OPL):
 * the difftest saves it with memory and registers and rewinds it.  Each
 * module lists its variables; STATE_BLOCK adds one. */
typedef struct { void *p; unsigned n; } StateBlock;
#define STATE_BLOCK(b, n, v) ((b)[(n)].p = (void *)&(v), (b)[(n)].n = sizeof(v), (n)++)
int hw_state_blocks(StateBlock *b);
int hw_seat_blocks(StateBlock *b);
int hw_world_blocks(StateBlock *b);
extern int seat_frozen;              /* runtime/seat.c: a seat other than 0 runs, no events, no interrupts */
extern int seats_n;                  /* runtime/seat.c: the seats (0: none) */
void seat_sound_frame(void);          /* src/rtl_snd.c: a frame of the world's: the sound of the seat this machine plays */
void seat_disk_pass(void);            /* runtime/seat.c: seat 0 changes rooms -- a turn for the others (each frame) */
extern int interp_depth;             /* runtime/dispatch.c: the interpreter runs (the emulated memory is the game state's) */
int vga_state_blocks(StateBlock *b);
int vga_seat_blocks(StateBlock *b);
int bios_state_blocks(StateBlock *b);
int dos_state_blocks(StateBlock *b);
int adlib_state_blocks(StateBlock *b);
extern int use_native;               /* 0 = interpret everything */

void string_op_ex(u8 op, int rep, u16 src_seg);

/* ---- interpreter ---- */
void interp_run(int far);
void interp_step(void);
void interp_call(u16 seg, u16 off, int far);
extern u64 interp_insns;

/* ---- BIOS / DOS ---- */
static inline int bios_is_stub(u16 seg, u16 off) { (void)off; return seg == BIOS_SEG; }
void bios_dispatch(u8 n);
void bios_stub_iret(u8 n);          /* run stub n, then IRET with correct flags */
void bios_init(void);
void int10(void); void int16(void); void int1a(void); void int33(void);
void int21(void);
void dos_init(const char *gamedir, const char *progname);
int  bios_key_available(void);
void bios_key_push(u16 scan_ascii);

/* ---- hardware ---- */
void hw_init(void);
void pic_raise(int irq);
extern u16 pit_reload[3];
extern u8 vga_dac[256][3];
extern int vga_mode;
u64 frame_no(void);

/* ---- sound ---- */
void adlib_init(void);
u8 adlib_read(u16 port);
void adlib_write(u16 port, u8 v);
void adlib_flush(void);
void adlib_wav_open(const char *path);
void adlib_select(int k);          /* register writes to chip k (0: the game's, 1: a seat's own sound) */
void adlib_seat_start(void);       /* chip 1 from the game's, and heard from now on */
void adlib_shift(int64_t d);       /* chip 1 hears its writes d after T */
extern int adlib_enabled;

/* ---- host ---- */
void host_init(int headless, int scale);
enum { DISPLAY_SHARP, DISPLAY_NEAREST, DISPLAY_LINEAR, DISPLAY_MMPX, DISPLAY_HR_PIXEL };   /* (hr, with smoothed pictures, went 2026-10-18: "hr" reads as hr-pixel) */
#define DISPLAY_IS_HR(f) ((f) == DISPLAY_HR_PIXEL)
extern int display_fullscreen, display_square, display_filter, display_integer;
extern int cast_fit_opt;                          /* --cast-fit / [display] cast_fit: the actors' fit for the role in the shop (src/cast_fit.c; decompiled build) */
extern int mouse_free;                            /* --mouse free or system (host.c) */
extern int mouse_system;                          /* --mouse system: the system's arrow stands for the game's (decompiled build) */
extern int sys_ptr_x, sys_ptr_y;                  /* the game pixel under the system pointer (-1: not seen yet;
                                                     a network game: where the game has taken it to, net_inputs) */
int host_shown_seat(void);                        /* the seat whose screen this machine shows (a network game: its own) */
void game_cursor(int *x, int *y);   /* where the game's cursor is heading: its position + its pending motion */
enum { SYS_ARROW_OFF, SYS_ARROW_ON, SYS_ARROW_HIDDEN, SYS_ARROW_DRAG };
int sys_drag_image(unsigned char *pix, int cap, int *w, int *h, int *hx, int *hy, int lim[4]);   /* the carried item (SYS_ARROW_DRAG), the mouse's limits */
int sys_arrow_state(void);          /* --mouse system: does the system pointer show the game's arrow now (src/fixes.c) */
/* the player's settings file (settings.c; --settings) */
extern int settings_active;
int settings_load(const char *file);              /* NULL or "": the default path */
const char *settings_get(const char *section, const char *key);
int settings_bool(const char *v);                 /* 1, 0, or -1 */
void settings_set(const char *section, const char *key, const char *value);
FILE *settings_fopen(const char *path, const char *mode);   /* a path in the ANSI code page or UTF-8 (Windows) */
void host_present(void);
void mmpx_render(const u32 *rgb, u32 *out);       /* --filter mmpx (mmpx.c): 320x200 -> 1280x800 */
void mmpx_scale4(const u32 *src, int w, int h, int wrap, u32 *out);   /* any w x h -> 4w x 4h (wrap: a tile) */
/* --filter hr-pixel (src/hr.c; weak stand-ins in clock_none.c for the
 * machine-level build): the 4x cells of every byte -- pictures and figures
 * as their pixels; hr_render draws the screen's, bad[i] = 1 where a
 * pixel's cell is not valid (returns how many; -1: no cells) */
int hr_init(void);
extern const char *ai_pictures_path;    /* --ai-pictures / [display] ai_pictures: a picture set (docs/ki-bilder.md) */
int ki_make_main(int argc, char **argv);   /* --make-ki (src/ki_make.c, docs/ki-bilder.md) */
int hr_ki_toggle(void);                 /* Ctrl+K: the AI pictures off / on (1); -1 without a set */
int hr_render(u32 *out, const u32 *pal, u8 *bad);
void hr_seat_save(int seat);                     /* a seat's cells, saved and loaded with it (seat.c) */
void hr_seat_load(int seat);
extern void (*hr_frame_hook)(void);
void host_poll(void);
void host_sleep_until_T(u64 t);
extern int host_headless;
extern int realtime;

/* ---- mouse (driver-level state, in INT 33h units) ---- */
extern int mouse_x, mouse_y, mouse_buttons;
extern int mouse_minx, mouse_maxx, mouse_miny, mouse_maxy;
void mouse_move_rel(int dx_px, int dy_px);
void mouse_pending(int *dx_px, int *dy_px);       /* motion the game has not read yet */

/* ---- debugging ---- */
extern int trace_calls;
void dump_state(const char *why);

#endif
