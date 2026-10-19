/* Host platform: SDL2 window/input, or headless with a frame-indexed script.
 *
 * Script lines (headless or windowed), executed when the frame counter
 * reaches N:
 *   N key <name|ascii>       press+release a key (BIOS scan/ascii)
 *   N mouse <x> <y>          move the game cursor to absolute game pixels
 *   N move <dx> <dy>         relative mouse motion in game pixels
 *   N down <btn> / N up <btn> mouse button 1=left 2=right
 *   N click|rclick <x> <y>   home the cursor, move to x,y; press the left/right button
 *                            once the game has seen the cursor there, release >= 5
 *                            frames later
 *   N goto <x> <y>           the same move without a button
 *   N pointer <x> <y>        the system pointer (--mouse free/system) over game pixel x,y
 *   N hash                   print memory/VRAM hashes (scenario checkpoints) and the game-state
 *                            hash (STATE: data segment below the stack + far tables)
 *   N dump <file>            write the 1 MB of memory to a file
 *   N fuzz <argsfile> [reps] differential fuzzing from here (tools/fuzzargs.py)
 *   N kcode <hex>            push a raw BIOS key code (scan << 8 | ascii)
 *   N poke16 SEG:OFF VALUE   test harness only: write a word of game memory
 *   N peek8|peek16|peek32 SEG:OFF  test harness: print a byte/word/dword of game memory
 *   N call SEG:OFF[,ARG...]  test harness: call a game function, print AX DX
 *   N shot <file.png|.ppm> [seat]  save the 320x200 frame (of that seat's screen, --seats)
 *   N seat S <command>       --seats: input for seat S (key, kcode, mouse, move, pointer,
 *                            down, up, click, rclick, goto), its shot, its hash (HASH seat S
 *                            frame N vram H: its screen), its peek8/16/32 (PEEK seat S ...);
 *                            applied when seat S runs; a spectator takes only the motions
 *   N shothd <file.png>      save the 1280x800 picture of --filter mmpx (mmpx.c)
 *   N shothr <file.png>      save the 1280x800 picture of --filter hr-pixel (src/hr.c; needs it)
 *
 * Lines must be in frame order (a line for an earlier frame is an error).
 * --logical-time: time from the logical clock (runtime.h), for comparisons
 * between engines.
 *   N quit                   exit
 *
 * Live input (SDL) is collected between frames and applied at the next frame
 * boundary -- the same point where script lines are applied -- so a session
 * recorded with --record replays exactly with --script.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <SDL.h>
#include "runtime.h"
#include "platform.h"
#include "seat.h"
#include "net.h"

int host_headless;
int realtime = 1;
static SDL_Window *win;
static SDL_Renderer *ren;
static SDL_Texture *tex;
static SDL_AudioDeviceID audio_dev;
static int scale = 3;

/* Display (presentation only: game state, scripts and recordings do not
 * depend on it).  The 320x200 picture is drawn into `view`, a rectangle of
 * the window's output pixels: 4:3 like a VGA monitor (each pixel 1.2 times
 * as tall as wide) or square pixels (16:10), as large as fits, centred.
 * Filters: sharp (default) = nearest up to a whole multiple of the picture
 * (tex_up, a render target), then linear to the view -- even pixels at any
 * size; nearest; linear; mmpx = the picture 4 times its size by MMPX
 * (mmpx.c), then linear to the view; hr = drawn at 4 times the size from
 * the cells the decompiled build keeps (src/hr.c), MMPX where a pixel has
 * none, then linear to the view; hr-pixel = the same with pictures and
 * figures not smoothed, and plain 4x pixels where one has no cell.  --integer keeps the scale a whole
 * number. */
int display_fullscreen;               /* --fullscreen */
int display_square;                   /* --aspect square */
int display_filter = DISPLAY_SHARP;   /* --filter */
int display_integer;                  /* --integer */
int cast_fit_opt;                     /* --cast-fit: the actors' fit for the role in the shop (src/cast_fit.c) */
int mouse_free;                       /* --mouse free: the system pointer, not captured */
int mouse_system;                     /* --mouse system: free, and the system's arrow is the game's */
static int ptr_x = -1, ptr_y = -1;    /* free mouse: the pointer in window points (-1: not seen yet) */
static int ptr_moved;
int sys_ptr_x = -1, sys_ptr_y = -1;   /* and the game pixel under it */
static SDL_Texture *tex_up;           /* the picture at up_k times its size (sharp) */
static SDL_Texture *tex4;             /* the 4x picture (mmpx) */
static u32 *rgba4;                    /* and its pixels */
static int up_k;
static SDL_Rect view;                 /* where the picture goes, in output pixels */
static float view_wpt, view_hpt;      /* its size in window points (mouse motion) */
static float view_xpt, view_ypt;      /* and its corner */
static u32 rgba[320 * 200];
static u64 wall_start_ns;
/* the scripted pointer, one per seat (seat 0's is the game's without --seats):
 * where we think the game's cursor is, and a click on its way -- the cursor
 * homed, then moved at move_at, the button pressed at down, released at up */
typedef struct { int x, y; int to_x, to_y; u64 move_at; int btn; u64 down, up; } ScriptPointer;
static ScriptPointer spt[SEATS_MAX] = { { 160, 100 }, { 160, 100 }, { 160, 100 } };
#define game_x spt[0].x
#define game_y spt[0].y
void mouse_set_buttons(int b);
static int shot_requests;
char *script_path;

/* --record: live input as script lines */
static FILE *recf;
static u16 live_keys[64]; static int live_nkeys;
static int live_dx, live_dy, applied_buttons;
static int btn_queue[32], btn_n;     /* button states in arrival order: one change per frame, so short clicks survive */

void host_record_open(const char *path, const char *prog_args, int argc, char **argv) {
    recf = fopen(path, "w");
    if (!recf) { perror(path); exit(1); }
    fprintf(recf, "# command:");               /* (tools/replay.py: the same options, the window's left out) */
    for (int i = 0; i < argc; i++) fprintf(recf, strpbrk(argv[i], " \"") ? " '%s'" : " %s", argv[i]);   /* (shell words) */
    fprintf(recf, "\n");
    char money[40] = "";
    char credit[40] = "";
    char terror[40] = "";
    char sammy[128] = "";
    char offers[48] = "";
    if (AD_OFFERS_OPT() > AD_OFFERS_OPT_MIN) snprintf(offers, sizeof offers, " --ad-offers=%d", AD_OFFERS_OPT());
    if (FILM_OFFERS_OPT() > FILM_OFFERS_OPT_MIN)
        snprintf(offers + strlen(offers), sizeof offers - strlen(offers), " --film-offers=%d", FILM_OFFERS_OPT());
    if (start_money_opt >= 0) snprintf(money, sizeof money, " --start-money=%ld", start_money_opt);
    if (credit_line_opt == CREDIT_LINE_ORIGINAL) snprintf(credit, sizeof credit, " --credit-line=original");
    else if (credit_line_opt >= 0) snprintf(credit, sizeof credit, " --credit-line=%ld", credit_line_opt);
    if (terror_damage_opt >= 0) snprintf(terror, sizeof terror, " --terror-damage=%ld", terror_damage_opt);
    if (SAMMY_PRIZE_OPT(0) || SAMMY_PRIZE_OPT(1) || SAMMY_PRIZE_OPT(2) || SAMMY_PRIZE_OPT(3))
        snprintf(sammy, sizeof sammy, " --sammy-prize=%ld,%ld,%ld,%ld", SAMMY_PRIZE_OPT(0), SAMMY_PRIZE_OPT(1), SAMMY_PRIZE_OPT(2), SAMMY_PRIZE_OPT(3));
    fprintf(recf, "# recorded session; replay with: --script %s --args \"%s\"%s%s%s%s%s%s%s%s%s%s%s on fresh game files\n", path, prog_args ? prog_args : "",
            fix_spec ? " --fixes=" : "", fix_spec ? fix_spec : "", money, credit, terror, sammy, offers, fourth_speed_opt ? " --fourth-speed" : "",
            fast_rooms_opt ? " --fast-rooms" : "", ai_improve_opt == 2 ? " --ai-improve=strong" : ai_improve_opt ? " --ai-improve=normal" : "",
            series_top_opt ? " --series-topicality=yes" : " --series-topicality=no");   /* (explicit: the settings file says yes by default) */
    fflush(recf);
}

/* --dump-on: snapshot memory a fixed delay after a DGROUP word changes
 * (e.g. the day counter), for tools/stats.py */
static u32 dumpon_lin; static const char *dumpon_dir;
static int dumpon_prev = -1; static u64 dumpon_at; static u16 dumpon_val;
#define DUMPON_DELAY 140             /* frames (2 s): let the day/hour processing finish */

void host_dump_on(u32 lin, const char *dir) { dumpon_lin = lin; dumpon_dir = dir; }

static void dump_on_change(void) {
    u16 v = (u16)(M[dumpon_lin] | M[dumpon_lin + 1] << 8);
    if (dumpon_prev >= 0 && v != dumpon_prev) { dumpon_at = frame_no() + DUMPON_DELAY; dumpon_val = v; }
    dumpon_prev = v;
    if (dumpon_at && frame_no() >= dumpon_at) {
        char p[1200];
        snprintf(p, sizeof p, "%s/%05u.bin", dumpon_dir, dumpon_val);
        FILE *f = fopen(p, "wb");
        if (!f) { perror(p); exit(1); }
        fwrite(M, 1, 0xA0000, f);
        fclose(f);
        snprintf(p, sizeof p, "%s/index.txt", dumpon_dir);
        if ((f = fopen(p, "a"))) {
            fprintf(f, "%05u.bin value %u frame %llu T %llu loadseg %04X\n", dumpon_val, dumpon_val,
                    (unsigned long long)frame_no(), (unsigned long long)T, LOADSEG);
            fclose(f);
        }
        dumpon_at = 0;
    }
}

/* --clamp: test harness only -- keep a signed DGROUP word >= a minimum at
 * every frame (e.g. the idle human's image, so long AI runs are not ended by
 * the human being fired).  Changes the game; never use for equivalence. */
static u32 clamp_lin[8]; static s16 clamp_min[8]; static int nclamp;

void host_clamp(u32 lin, int min) { if (nclamp < 8) { clamp_lin[nclamp] = lin; clamp_min[nclamp++] = (s16)min; } }

static void apply_clamps(void) {
    for (int i = 0; i < nclamp; i++) {
        s16 v = (s16)(M[clamp_lin[i]] | M[clamp_lin[i] + 1] << 8);
        if (v < clamp_min[i]) { M[clamp_lin[i]] = (u8)clamp_min[i]; M[clamp_lin[i] + 1] = (u8)((u16)clamp_min[i] >> 8); }
    }
}

/* Free mouse: move the game's cursor to the game pixel under the system
 * pointer (the edge of the picture when the pointer is outside it), as
 * ordinary motion -- recordings and replays stay relative.  The game uses
 * the mouse as a relative device (it re-centres the driver after each read)
 * and eases its own cursor toward the motion it collected; so the target is
 * compared with where the game is heading: its cursor + its pending motion
 * (game_cursor) + what the driver holds for it.  Starting from that each
 * frame keeps the game's own clamps (e.g. the transmitter map) from piling
 * up motion. */
static void motion_to(int gx, int gy, int *dx, int *dy) {
    int cx, cy, px, py;
    game_cursor(&cx, &cy);
    mouse_pending(&px, &py);
    *dx = gx - cx - px;
    *dy = gy - cy - py;
}
static void pointer_motion(int gx, int gy, int *dx, int *dy) {
    sys_ptr_x = gx; sys_ptr_y = gy;
    motion_to(gx, gy, dx, dy);
}

static int free_mouse_pixel(int *gx, int *gy) {
    if (!mouse_free || ptr_x < 0 || !ptr_moved || view_wpt < 1) return 0;
    ptr_moved = 0;
    *gx = (int)((ptr_x - view_xpt) * 320.0f / view_wpt);
    *gy = (int)((ptr_y - view_ypt) * 200.0f / view_hpt);
    if (*gx < 0) *gx = 0;
    if (*gx > 319) *gx = 319;
    if (*gy < 0) *gy = 0;
    if (*gy > 199) *gy = 199;
    return 1;
}
/* -> 1 and the game pixel *gx, *gy where the free mouse moved to */
static int free_mouse_target(int *gx, int *gy) {
    if (!free_mouse_pixel(gx, gy)) return 0;
    pointer_motion(*gx, *gy, &live_dx, &live_dy);   /* (the free mouse has no relative motion) */
    return 1;
}

/* --mouse system: the system pointer is shown over the picture while it
 * stands for the game's arrow (sys_arrow_state, src/fixes.c), after this
 * frame's motion has gone to the game.  The game hides its cursor for a
 * moment where it draws under it: that keeps the pointer; hidden for longer
 * (a cut scene, a screen change) the pointer goes too. */
/* A network game, something carried: this machine draws the item over the picture where the
 * pointer is now, at every present (src/fixes.c carry_wanted) -- not the game: its cursor
 * follows the server's bundles, some frames late.  A system cursor of the item's picture
 * could not be: the systems refuse cursors much wider than 256 pixels (a programme in the
 * planner at the window's scale).  host_drag_ok tells the game that the item is drawn here:
 * only then it leaves its own out (headless, the tests: as if). */
int host_drag_ok;
static SDL_Texture *drag_tex;
static int drag_on, drag_tw, drag_th, drag_hx, drag_hy, drag_lim[4];
static int drag_overlay_set(void) {
    static u8 pix[320 * 120];
    static u32 key_seen;
    int w, h, hx, hy;
    if (!sys_drag_image(pix, sizeof pix, &w, &h, &hx, &hy, drag_lim)) return 0;
    u32 key = 2166136261u;                       /* the item and its size */
    for (int i = 0; i < w * h; i++) key = (key ^ pix[i]) * 16777619u;
    key = (key ^ (u32)(w << 16 | h)) * 16777619u;
    if (!drag_tex || key != key_seen) {
        static u8 idx[320 * 200], pal6[256][3];
        static u32 argb[320 * 120];
        plat_screen(idx, pal6);
        for (int i = 0; i < w * h; i++) {
            u8 v = pix[i];
            argb[i] = !v ? 0 : 0xFF000000u | (u32)(pal6[v][0] * 255 / 63) << 16 | (u32)(pal6[v][1] * 255 / 63) << 8 | (u32)(pal6[v][2] * 255 / 63);
        }
        if (!drag_tex || w != drag_tw || h != drag_th) {
            if (drag_tex) SDL_DestroyTexture(drag_tex);
            drag_tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
            if (!drag_tex) { fprintf(stderr, "[display] no texture for the carried item (%s)\n", SDL_GetError()); return 0; }
            SDL_SetTextureBlendMode(drag_tex, SDL_BLENDMODE_BLEND);
            SDL_SetTextureScaleMode(drag_tex, SDL_ScaleModeNearest);
            drag_tw = w; drag_th = h;
        }
        SDL_UpdateTexture(drag_tex, NULL, argb, w * 4);
        key_seen = key;
    }
    drag_hx = hx; drag_hy = hy;
    return 1;
}
/* the item over the picture where the game would draw it with its cursor at the pointer:
 * the cursor kept inside the mouse's limits (frame.c; a transmitter stays on the map), less
 * the hot spot, kept inside the room's 320x120, a game pixel as big as the picture's */
static int clamp_int(int v, int lo, int hi) { return v > hi ? hi : v < lo ? lo : v; }
static void draw_drag(void) {
    if (!drag_on || !drag_tex || view_wpt < 1 || view_hpt < 1) return;
    int x = clamp_int((int)((ptr_x - view_xpt) * 320.0f / view_wpt), 0, 319), y = clamp_int((int)((ptr_y - view_ypt) * 200.0f / view_hpt), 0, 199);
    x = (x > drag_lim[2] ? drag_lim[2] : x < drag_lim[0] ? drag_lim[0] : x) - drag_hx;   /* (max, then min: frame.c) */
    y = (y > drag_lim[3] ? drag_lim[3] : y < drag_lim[1] ? drag_lim[1] : y) - drag_hy;
    if (x > 319 - drag_tw) x = 319 - drag_tw;
    if (x < 0) x = 0;
    if (y > 120 - drag_th) y = 120 - drag_th;
    if (y < 0) y = 0;
    float gx = view.w / 320.0f, gy = view.h / 200.0f;          /* renderer pixels per game pixel */
    SDL_Rect dst = {view.x + (int)(x * gx), view.y + (int)(y * gy), (int)(drag_tw * gx + 0.5f), (int)(drag_th * gy + 0.5f)};
    SDL_RenderCopy(ren, drag_tex, NULL, &dst);
}

static void pointer_show(int show);
static void system_pointer(void) {           /* (in the shown seat's context: present_screen) */
    static int shown = -1, hidden_frames;
    if (!mouse_system || ptr_x < 0) return;
    int inside = ptr_x >= view_xpt && ptr_x < view_xpt + view_wpt && ptr_y >= view_ypt && ptr_y < view_ypt + view_hpt;
    int st = sys_arrow_state(), show;
    drag_on = st == SYS_ARROW_DRAG && drag_overlay_set();
    host_drag_ok = drag_on;
    if (drag_on) { if (shown != 0) { pointer_show(0); shown = 0; } return; }   /* (the item is the cursor) */
    if (st == SYS_ARROW_DRAG) st = SYS_ARROW_OFF;
    if (st == SYS_ARROW_HIDDEN) { if (hidden_frames < 10) hidden_frames++; show = shown > 0 && hidden_frames < 10; }
    else { show = st == SYS_ARROW_ON; hidden_frames = 0; }
    if (!inside) show = 1;
    if (show != shown) { pointer_show(show); shown = show; }
}

static u64 state_hash(void);
static void apply_live_input(void) {
    u64 f = frame_no();
    if (net_active) {                    /* a network game: this seat's input goes to the server (net.h) */
        char m[64];
        int gx, gy;
        if (free_mouse_pixel(&gx, &gy)) {  /* the free mouse: where the pointer is; each machine makes it */
            snprintf(m, sizeof m, "IN 0 pointer %d %d", gx, gy);   /* motion when the server's bundle brings it */
            net_send(m);
        }
        for (int i = 0; i < live_nkeys; i++) { snprintf(m, sizeof m, "IN 0 kcode %04X 0", live_keys[i]); net_send(m); }
        live_nkeys = 0;
        if (live_dx || live_dy) { snprintf(m, sizeof m, "IN 0 move %d %d", live_dx, live_dy); net_send(m); live_dx = live_dy = 0; }
        if (btn_n) {
            int want = btn_queue[0];
            memmove(btn_queue, btn_queue + 1, sizeof btn_queue[0] * --btn_n);
            for (int b = 1; b <= 2; b++)
                if ((want ^ applied_buttons) & b) { snprintf(m, sizeof m, "IN 0 %s %d 0", want & b ? "down" : "up", b); net_send(m); }
            applied_buttons = want;
        }
        return;
    }
    {   /* --record: every 1000 frames the game state as a replay's "hash" line prints it
         * (STATE), before this frame's input -- where a replay goes its own way */
        static u64 checked;
        if (recf && f / 1000 > checked) {
            checked = f / 1000;
            fprintf(recf, "# frame %llu state %016llx\n", (unsigned long long)f, (unsigned long long)state_hash());
        }
    }
    int gx, gy, freem = free_mouse_target(&gx, &gy);
    for (int i = 0; i < live_nkeys; i++) {
        bios_key_push(live_keys[i]);
        if (recf) fprintf(recf, "%llu kcode %04X\n", (unsigned long long)f, live_keys[i]);
    }
    live_nkeys = 0;
    if (freem && recf) fprintf(recf, "%llu pointer %d %d\n", (unsigned long long)f, gx, gy);   /* (a replay makes its motion the same way) */
    if (live_dx || live_dy) {
        mouse_move_rel(live_dx, live_dy);
        if (recf && !freem) fprintf(recf, "%llu move %d %d\n", (unsigned long long)f, live_dx, live_dy);
        live_dx = live_dy = 0;
    }
    if (btn_n) {
        int want = btn_queue[0];
        memmove(btn_queue, btn_queue + 1, sizeof btn_queue[0] * --btn_n);
        for (int b = 1; b <= 2; b++) {
            if (!((want ^ applied_buttons) & b)) continue;
            if (want & b) mouse_set_buttons(mouse_buttons | b);
            else mouse_set_buttons(mouse_buttons & ~b);
            if (recf) fprintf(recf, "%llu %s %d\n", (unsigned long long)f, want & b ? "down" : "up", b);
        }
        applied_buttons = want;
    }
    if (recf) fflush(recf);              /* (a crash keeps the input up to its frame) */
}

typedef struct { u64 frame; char cmd[16]; char a[128]; int x, y; int seat; } ScriptLine;
/* a script's day lines, "@DAY[.HOUR] txbuy PLAYER MAXFEE": at the first frame of game day DAY
 * from hour HOUR on (0 = 17:00 .. 7 = 00:00), in their order (test harness, tools/ai_run.py
 * --human-plan: a good player's transmitters, src/ai_improve.c game_test_txbuy; MAXFEE -1 / -2:
 * the west / east satellite) */
typedef struct { int day, hour; char cmd[16]; char a[128]; int y; } DayLine;
static DayLine *dscript; static int dscript_n, dscript_pos;
int game_test_txbuy(int p, long maxfee);
static ScriptLine *script; static int script_n, script_pos;
static int seat_pos[SEATS_MAX];            /* each seat's own place in the script (seats but 0) */
/* a network game: the inputs of every seat, as the server's bundles bring them (net.h) */
static ScriptLine *netq; static int netq_n, netq_cap;
static int netq_pos[SEATS_MAX];
int host_view_seat;                        /* --view-seat K: the screen shown (a network game: its seat) */
int host_shown_seat(void) { return net_active ? net_seat : host_view_seat; }
static void net_inputs(int seat, int spectator);

/* the wall clock for pacing (never the game's time) */
#ifdef _WIN32
/* Windows' C library has neither clock_gettime nor nanosleep: SDL's timers
 * (the sleeps in whole milliseconds) */
static u64 now_ns(void) {
    static u64 freq;
    if (!freq) freq = SDL_GetPerformanceFrequency();
    u64 c = SDL_GetPerformanceCounter();
    return c / freq * 1000000000ull + c % freq * 1000000000ull / freq;
}
static void sleep_ns(u64 ns) { SDL_Delay((Uint32)(ns / 1000000)); }
#else
static u64 now_ns(void) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u64)ts.tv_sec * 1000000000ull + ts.tv_nsec;
}
static void sleep_ns(u64 ns) {
    struct timespec ts = { (time_t)(ns / 1000000000ull), (long)(ns % 1000000000ull) };
    nanosleep(&ts, NULL);
}
#endif

static void load_script(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); exit(1); }
    char line[256];
    int lineno = 0;
    u64 last = 0;
    while (fgets(line, sizeof line, f)) {
        lineno++;
        if (line[0] == '#' || line[0] == '\n') continue;
        if (line[0] == '@') {
            DayLine d = { 0 };
            int n = sscanf(line, "@%d.%d %15s %127s %d", &d.day, &d.hour, d.cmd, d.a, &d.y);
            if (n < 2) { d.hour = 0; n = sscanf(line, "@%d %15s %127s %d", &d.day, d.cmd, d.a, &d.y) + 1; }
            if (n < 5 || strcmp(d.cmd, "txbuy") || d.hour < 0 || d.hour > 7 ||
                (dscript_n && (d.day < dscript[dscript_n - 1].day || (d.day == dscript[dscript_n - 1].day && d.hour < dscript[dscript_n - 1].hour)))) {
                fprintf(stderr, "%s:%d: day lines are @DAY[.HOUR] txbuy PLAYER MAXFEE, HOUR 0..7, in day order\n", path, lineno);
                exit(1);
            }
            dscript = realloc(dscript, sizeof *dscript * (dscript_n + 1));
            dscript[dscript_n++] = d;
            continue;
        }
        script = realloc(script, sizeof *script * (script_n + 1));
        ScriptLine *s = &script[script_n];
        memset(s, 0, sizeof *s);
        unsigned long long fr;
        int n = sscanf(line, "%llu %15s %127s %d", &fr, s->cmd, s->a, &s->y);
        if (n >= 3 && !strcmp(s->cmd, "seat")) {           /* N seat S command args */
            memset(s, 0, sizeof *s);
            n = sscanf(line, "%llu seat %d %15s %127s %d", &fr, &s->seat, s->cmd, s->a, &s->y);
            if (n < 3 || s->seat < 1 || s->seat >= SEATS_MAX) {
                fprintf(stderr, "%s:%d: seat lines are N seat S command, S = 1..%d\n", path, lineno, SEATS_MAX - 1);
                exit(1);
            }
            n -= 1;
        }
        if (n < 2) continue;
        if (fr < last) {
            /* it would run at the next frame due, silently out of place */
            fprintf(stderr, "%s:%d: frame %llu comes after frame %llu (script lines must be in frame order)\n",
                    path, lineno, fr, (unsigned long long)last);
            exit(1);
        }
        last = fr;
        s->frame = fr;
        s->x = atoi(s->a);
        script_n++;
    }
    fclose(f);
}

static void render_rgba(void) {
    static u8 idx[320 * 200], pal6[256][3];
    if (!plat_screen(idx, pal6)) { memset(rgba, 0, sizeof rgba); return; }
    u32 pal[256];
    for (int i = 0; i < 256; i++)
        pal[i] = 0xFF000000u | ((u32)(pal6[i][0] * 255 / 63) << 16) | ((u32)(pal6[i][1] * 255 / 63) << 8) | (u32)(pal6[i][2] * 255 / 63);
    for (int i = 0; i < 320 * 200; i++) rgba[i] = pal[idx[i]];
}

/* PNG with stored (uncompressed) deflate blocks: no zlib needed */
static u32 png_crc(u32 c, const u8 *p, size_t n) {
    static u32 tab[256];
    if (!tab[1])
        for (u32 i = 0; i < 256; i++) { u32 k = i; for (int j = 0; j < 8; j++) k = k & 1 ? 0xEDB88320u ^ (k >> 1) : k >> 1; tab[i] = k; }
    c = ~c;
    while (n--) c = tab[(c ^ *p++) & 0xFF] ^ (c >> 8);
    return ~c;
}
static void png_chunk(FILE *f, const char *type, const u8 *data, u32 n) {
    u8 hd[8] = { n >> 24, n >> 16, n >> 8, n, type[0], type[1], type[2], type[3] };
    fwrite(hd, 1, 8, f);
    if (n) fwrite(data, 1, n, f);
    u32 c = png_crc(png_crc(0, hd + 4, 4), data, n);
    u8 ce[4] = { c >> 24, c >> 16, c >> 8, c };
    fwrite(ce, 1, 4, f);
}
static void write_png(FILE *f, const u32 *px, u32 W, u32 H) {
    u32 ROW = 1 + 3 * W, RAW = ROW * H;
    u8 *raw = malloc(RAW), *z = malloc(RAW + RAW / 65535 * 5 + 5 + 6);
    for (u32 y = 0; y < H; y++) {
        u8 *r = raw + y * ROW;
        r[0] = 0;                                            /* filter: none */
        for (u32 x = 0; x < W; x++) { u32 c = px[y * W + x]; r[1 + 3 * x] = c >> 16; r[2 + 3 * x] = c >> 8; r[3 + 3 * x] = c; }
    }
    u32 n = 0, a = 1, b = 0;
    z[n++] = 0x78; z[n++] = 0x01;
    for (u32 o = 0; o < RAW; ) {
        u32 len = RAW - o > 65535 ? 65535 : RAW - o;
        z[n++] = o + len == RAW;                             /* BFINAL, BTYPE 00 */
        z[n++] = len; z[n++] = len >> 8; z[n++] = ~len; z[n++] = ~len >> 8;
        memcpy(z + n, raw + o, len); n += len; o += len;
    }
    for (u32 i = 0; i < RAW; i++) { a = (a + raw[i]) % 65521; b = (b + a) % 65521; }
    z[n++] = b >> 8; z[n++] = b; z[n++] = a >> 8; z[n++] = a;
    static const u8 sig[8] = { 0x89, 'P', 'N', 'G', 13, 10, 26, 10 };
    const u8 ihdr[13] = { 0, 0, W >> 8, W & 0xFF, 0, 0, H >> 8, H & 0xFF, 8, 2, 0, 0, 0 };
    fwrite(sig, 1, 8, f);
    png_chunk(f, "IHDR", ihdr, 13);
    png_chunk(f, "IDAT", z, n);
    png_chunk(f, "IEND", NULL, 0);
    free(raw); free(z);
}

static void save_png_or_ppm(const char *path) {
    render_rgba();
    const char *dot = strrchr(path, '.');
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return; }
    if (dot && !strcmp(dot, ".png")) write_png(f, rgba, 320, 200);
    else {
        fprintf(f, "P6\n320 200\n255\n");
        for (int i = 0; i < 320 * 200; i++) {
            u8 px[3] = { (u8)(rgba[i] >> 16), (u8)(rgba[i] >> 8), (u8)rgba[i] };
            fwrite(px, 1, 3, f);
        }
    }
    fclose(f);
    fprintf(stderr, "[shot] frame %llu -> %s\n", (unsigned long long)frame_no(), path);
    shot_requests++;
}

/* the 4x picture as it is shown with --filter mmpx */
static void render_mmpx(void) {
    if (!rgba4) rgba4 = malloc(sizeof *rgba4 * 1280 * 800);
    render_rgba();
    mmpx_render(rgba, rgba4);
}
/* ... with --filter hr-pixel: the cells; where a pixel has no valid cell (or there are no
 * cells), the pixel itself, 4 x 4 */
static void render_hr(void) {
    static u8 bad[320 * 200];
    if (!rgba4) rgba4 = malloc(sizeof *rgba4 * 1280 * 800);
    render_rgba();
    static u8 idx[320 * 200], pal6[256][3];
    u32 pal[256];
    if (!plat_screen(idx, pal6)) return;
    for (int i = 0; i < 256; i++)
        pal[i] = 0xFF000000u | ((u32)(pal6[i][0] * 255 / 63) << 16) | ((u32)(pal6[i][1] * 255 / 63) << 8) | (u32)(pal6[i][2] * 255 / 63);
    int n = hr_render(rgba4, pal, bad);
    if (!n) return;
    for (int i = 0; i < 320 * 200; i++)
        if (n < 0 || bad[i]) {
            size_t o = (size_t)(i / 320 * 4) * 1280 + (i % 320) * 4;
            for (int r = 0; r < 4; r++)
                for (int q = 0; q < 4; q++) rgba4[o + (size_t)r * 1280 + q] = rgba[i];
        }
}
static void render_4x(void) {
    if (DISPLAY_IS_HR(display_filter)) render_hr();
    else render_mmpx();
}
static void save_png_4x(const char *path, int hr) {
    if (hr) render_hr();
    else render_mmpx();
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return; }
    write_png(f, rgba4, 1280, 800);
    fclose(f);
    fprintf(stderr, "[shot] frame %llu -> %s (1280x800)\n", (unsigned long long)frame_no(), path);
}

/* US layout scan codes for printable ASCII */
static u16 ascii_to_bios(u8 c) {
    static const u8 letters[26] = {0x1E,0x30,0x2E,0x20,0x12,0x21,0x22,0x23,0x17,0x24,0x25,0x26,0x32,0x31,0x18,0x19,0x10,0x13,0x1F,0x14,0x16,0x2F,0x11,0x2D,0x15,0x2C};
    if (c >= 'a' && c <= 'z') return (u16)(letters[c - 'a'] << 8 | c);
    if (c >= 'A' && c <= 'Z') return (u16)(letters[c - 'A'] << 8 | c);
    if (c >= '1' && c <= '9') return (u16)((0x02 + c - '1') << 8 | c);
    if (c == '0') return 0x0B30;
    if (c == ' ') return 0x3920;
    if (c == '-') return 0x0C2D;
    if (c == '.') return 0x342E;
    return c;
}

static u16 key_from_name(const char *a) {
    if (!strcmp(a, "enter")) return 0x1C0D;
    if (!strcmp(a, "esc")) return 0x011B;
    if (!strcmp(a, "space")) return 0x3920;
    if (!strcmp(a, "up")) return 0x4800;
    if (!strcmp(a, "down")) return 0x5000;
    if (!strcmp(a, "left")) return 0x4B00;
    if (!strcmp(a, "right")) return 0x4D00;
    if (!strcmp(a, "bksp")) return 0x0E08;
    if (a[0] == 'F' && a[1]) { int n = atoi(a + 1); if (n >= 1 && n <= 10) return (u16)((0x3A + n) << 8); }
    return ascii_to_bios((u8)a[0]);
}

/* a click on its way: the move once the home motion is through, the press
 * once the cursor has arrived, the release at least 5 frames later -- a
 * screen that polls the mouse only every few frames would otherwise see the
 * cursor late, or press and release in the same poll */
static void pointer_step(ScriptPointer *p) {
    extern int mouse_pending_motion(void);
    if (p->move_at && frame_no() >= p->move_at && !mouse_pending_motion()) {
        mouse_move_rel(p->to_x, p->to_y); p->x = p->to_x; p->y = p->to_y; p->move_at = 0;
    }
    if (p->btn && !p->move_at && !mouse_pending_motion()) {
        if (p->down && frame_no() >= p->down) {
            mouse_set_buttons(mouse_buttons | p->btn); p->down = 0;
            if (p->up < frame_no() + 5) p->up = frame_no() + 5;
        }
        else if (!p->down && frame_no() >= p->up) { mouse_set_buttons(mouse_buttons & ~p->btn); p->btn = 0; }
    }
}

/* a line of input, into the keyboard and mouse now in memory (the running
 * seat's); 0 if it is not input */
static int input_line(const ScriptLine *s, ScriptPointer *p) {
    if (!strcmp(s->cmd, "key")) bios_key_push(key_from_name(s->a));
    else if (!strcmp(s->cmd, "kcode")) bios_key_push((u16)strtoul(s->a, NULL, 16));
    else if (!strcmp(s->cmd, "mouse")) { mouse_move_rel(s->x - p->x, s->y - p->y); p->x = s->x; p->y = s->y; }
    else if (!strcmp(s->cmd, "move")) { mouse_move_rel(s->x, s->y); p->x += s->x; p->y += s->y; }
    else if (!strcmp(s->cmd, "pointer")) {
        int dx, dy;
        pointer_motion(s->x, s->y, &dx, &dy);
        mouse_move_rel(dx, dy); p->x = s->x; p->y = s->y;
    }
    else if (!strcmp(s->cmd, "down")) mouse_set_buttons(mouse_buttons | (s->x == 2 ? 2 : 1));
    else if (!strcmp(s->cmd, "up")) mouse_set_buttons(mouse_buttons & ~(s->x == 2 ? 2 : 1));
    else if (!strcmp(s->cmd, "click") || !strcmp(s->cmd, "rclick") || !strcmp(s->cmd, "goto")) {
        /* home the cursor to (0,0), move to x,y, then press and release */
        mouse_move_rel(-400, -400);
        p->to_x = s->x; p->to_y = s->y;
        p->move_at = frame_no() + 5;
        p->btn = !strcmp(s->cmd, "goto") ? 0 : !strcmp(s->cmd, "rclick") ? 2 : 1;
        p->down = frame_no() + 10; p->up = frame_no() + 15;
    }
    else return 0;
    return 1;
}

/* "N arrowstate": "ARROW frame N state S", sys_arrow_state with the shown seat's state in memory,
 * as present_screen asks it (in seat 0's turn) -- a window's present without a window */
static void print_arrow_state(const char *unused) {
    (void)unused;
    fprintf(stdout, "ARROW frame %llu state %d\n", (unsigned long long)frame_no(), sys_arrow_state());
    fflush(stdout);
}

static int input_line_cmd(const char *cmd) {
    return !strcmp(cmd, "key") || !strcmp(cmd, "kcode") || !strcmp(cmd, "down") || !strcmp(cmd, "up") ||
           !strcmp(cmd, "click") || !strcmp(cmd, "rclick");
}

static int is_motion(const char *cmd) {
    return !strcmp(cmd, "mouse") || !strcmp(cmd, "move") || !strcmp(cmd, "pointer") || !strcmp(cmd, "goto");
}

static u64 vram_hash(void) {
    u64 hv = 1469598103934665603ull;
    for (u32 a = 0xA0000; a < 0xAFA00; a++) hv = (hv ^ M[a]) * 1099511628211ull;
    return hv;
}

/* seat S is about to run, its state in memory (runtime/seat.c): its script
 * lines that are due.  Lines before the seat joined are dropped. */
void host_seat_input(int seat, int spectator) {
    if (seat <= 0 || seat >= SEATS_MAX) return;
    static int started[SEATS_MAX];
    int dropped = 0;
    if (!started[seat]) {
        started[seat] = 1;
        while (seat_pos[seat] < script_n && script[seat_pos[seat]].frame < frame_no())
            dropped += script[seat_pos[seat]++].seat == seat;
        if (dropped) fprintf(stderr, "[script] seat %d joined at frame %llu: %d earlier line%s dropped\n",
                             seat, (unsigned long long)frame_no(), dropped, dropped > 1 ? "s" : "");
    }
    ScriptPointer *p = &spt[seat];
    pointer_step(p);
    if (net_active) net_inputs(seat, spectator);
    while (seat_pos[seat] < script_n && script[seat_pos[seat]].frame <= frame_no()) {
        const ScriptLine *s = &script[seat_pos[seat]++];
        if (s->seat != seat) continue;
        if (net_active && (input_line_cmd(s->cmd) || is_motion(s->cmd))) continue;   /* (sent to the server) */
        if (!strcmp(s->cmd, "shot")) save_png_or_ppm(s->a);
        else if (!strncmp(s->cmd, "peek", 4)) {
            /* N seat S peek8|peek16|peek32 SEG:OFF -> "PEEK seat S frame N SEG:OFF value": its state */
            unsigned sg = 0, of = 0;
            if (sscanf(s->a, "%x:%x", &sg, &of) == 2) {
                if (use_native) state_export();
                u32 a = ((u32)(LOADSEG + sg) << 4) + of;
                unsigned v = s->cmd[4] == '8' ? M[a] : (unsigned)(M[a] | M[a + 1] << 8);
                if (s->cmd[4] == '3') v |= (unsigned)(M[a + 2] | M[a + 3] << 8) << 16;
                fprintf(stdout, "PEEK seat %d frame %llu %04X:%04X %u\n", seat, (unsigned long long)frame_no(), sg, of, v);
                fflush(stdout);
            }
        }
        else if (!strcmp(s->cmd, "poke16")) {
            /* test harness only: N seat S poke16 SEG:OFF VALUE -- into its state (its seat block) */
            unsigned sg = 0, of = 0;
            if (sscanf(s->a, "%x:%x", &sg, &of) == 2) {
                if (use_native) state_export();
                u32 a = ((u32)(LOADSEG + sg) << 4) + of;
                M[a] = (u8)s->y; M[a + 1] = (u8)((u16)s->y >> 8);
                if (use_native) state_reload();
            }
        }
        else if (!strcmp(s->cmd, "hash")) {
            fprintf(stdout, "HASH seat %d frame %llu vram %016llx\n", seat, (unsigned long long)frame_no(), (unsigned long long)vram_hash());
            fflush(stdout);
        }
        else if (spectator && !is_motion(s->cmd))
            fprintf(stderr, "[script] frame %llu: seat %d only watches, %s ignored\n", (unsigned long long)frame_no(), seat, s->cmd);
        else if (!input_line(s, p))
            fprintf(stderr, "[script] frame %llu: %s is not a command for seat %d\n", (unsigned long long)frame_no(), s->cmd, seat);
    }
}

void host_net_line(u64 frame, int seat, const char *cmd, const char *args) {
    if (seat < 0 || seat >= SEATS_MAX) return;
    if (netq_n == netq_cap) {
        netq_cap = netq_cap ? netq_cap * 2 : 256;
        if (!(netq = realloc(netq, sizeof *netq * netq_cap))) { fprintf(stderr, "[net] out of memory\n"); exit(1); }
    }
    ScriptLine *s = &netq[netq_n++];
    memset(s, 0, sizeof *s);
    s->frame = frame;
    s->seat = seat;
    snprintf(s->cmd, sizeof s->cmd, "%s", cmd);
    sscanf(args, "%127s %d", s->a, &s->y);
    s->x = atoi(s->a);
}

/* the network game's inputs of `seat` that are due (seat 0 at the frame
 * boundary, the others as they are about to run) */
static void net_inputs(int seat, int spectator) {
    static int started[SEATS_MAX];
    int join = seat > 0 && !started[seat];      /* input before the seat joined is dropped, as a script's */
    started[seat] = 1;
    while (netq_pos[seat] < netq_n && netq[netq_pos[seat]].frame <= frame_no()) {
        const ScriptLine *s = &netq[netq_pos[seat]++];
        if (s->seat != seat || (join && s->frame < frame_no())) continue;
        if (spectator && !is_motion(s->cmd)) continue;
        if (!strcmp(s->cmd, "pointer")) {   /* (the system pointer shown here is this machine's own) */
            int dx, dy;
            /* --mouse system: the system arrow stands for the game's where the game's cursor
             * is heading to now -- the pointer as the bundle brought it, a few frames behind
             * the pointer itself; compared with the pointer itself, the two would part at
             * every motion and the arrows take turns (fixes.c sys_arrow_stands_in) */
            if (seat == host_shown_seat()) { sys_ptr_x = s->x; sys_ptr_y = s->y; }
            motion_to(s->x, s->y, &dx, &dy);
            mouse_move_rel(dx, dy); spt[seat].x = s->x; spt[seat].y = s->y;
        } else
            input_line(s, &spt[seat]);
    }
}

/* the game state's hash (the STATE line of the script's hash, a network game's
 * desync check): the data segment below the stack (DGROUP 0..A730), the
 * film/series/screenplay tables, and with --ad-offers / --film-offers the
 * ad agency's offers and the film dealer's slots past 10 (src/state.c exports
 * them to AD_OFFERS_SEG / FILM_OFFERS_SEG) */
static u64 state_hash(void) {
    if (use_native) state_export();
    u64 hs = 1469598103934665603ull;
    u32 dg = ((u32)LOADSEG + 0x39D4) << 4;
    for (u32 a = dg; a < dg + 0xA730; a++) hs = (hs ^ M[a]) * 1099511628211ull;
    for (u32 a = ((u32)LOADSEG + 0x4DA5) << 4; a < (((u32)LOADSEG + 0x4F7E) << 4) + 8 + 51 * 0x1B3; a++)
        hs = (hs ^ M[a]) * 1099511628211ull;
    if (AD_OFFERS_OPT() > AD_OFFERS_OPT_MIN)
        for (u32 a = (u32)AD_OFFERS_SEG << 4; a < ((u32)AD_OFFERS_SEG << 4) + (u32)(AD_OFFERS_OPT() - 10) * 7; a++)
            hs = (hs ^ M[a]) * 1099511628211ull;
    if (FILM_OFFERS_OPT() > FILM_OFFERS_OPT_MIN)   /* and with --film-offers the dealer's slots past 10 */
        for (u32 a = (u32)FILM_OFFERS_SEG << 4; a < ((u32)FILM_OFFERS_SEG << 4) + (u32)(FILM_OFFERS_OPT() - 10) * 8; a++)
            hs = (hs ^ M[a]) * 1099511628211ull;
    return hs;
}

/* a network game starts: connect, then this seat's script input to the server */
void host_net_start(const char *server, int seat, const char *params) {
    net_connect(server, seat, params);
    char m[256];
    for (int i = 0; i < script_n; i++) {
        const ScriptLine *s = &script[i];
        if (s->seat != seat || !(input_line_cmd(s->cmd) || is_motion(s->cmd))) continue;
        snprintf(m, sizeof m, "IN %llu %s %s %d", (unsigned long long)s->frame, s->cmd, s->a[0] ? s->a : "-", s->y);
        net_send(m);
    }
    net_ready();
}

static void present_shown(void);
static void display_toggle_fullscreen(void);
static int display_unseen(u64 now);
static u64 present_blocked_until;
uint64_t host_wall_ns(void) { return now_ns(); }

/* The game stands while the server's inputs are missing: the window stays alive and, once the
 * game runs (SHOW), keeps showing its last picture and the notices (a player's connection
 * broken: the time it is waited for) -- a few times a second.  1: the player pressed Enter
 * (net.c: go on without the one waited for). */
int host_net_wait(const char *why, int show) {
    static u64 last, last_present;
    u64 now = now_ns();
    if (now - last > 3000000000ull) { fprintf(stderr, "[net] %s ...\n", why); last = now; }
    int enter = 0;
    if (!host_headless) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) exit(0);
            if (e.type == SDL_KEYDOWN && !e.key.repeat) {
                SDL_Keycode sym = e.key.keysym.sym;
                Uint16 mod = e.key.keysym.mod;
                int ret = sym == SDLK_RETURN || sym == SDLK_KP_ENTER;
                if (sym == SDLK_F11 || (ret && (mod & KMOD_ALT)) || (sym == SDLK_f && (mod & KMOD_GUI)))
                    display_toggle_fullscreen();          /* (as in host_poll) */
                else if (ret && !(mod & (KMOD_ALT | KMOD_CTRL | KMOD_GUI | KMOD_SHIFT)))
                    enter = 1;                            /* Enter alone: go on without the one waited for */
            }
            if (e.type == SDL_WINDOWEVENT || e.type == SDL_MOUSEMOTION || e.type == SDL_KEYDOWN || e.type == SDL_MOUSEBUTTONDOWN)
                present_blocked_until = 0;
        }
        if (show && ren && now - last_present > 66000000) {
            last_present = now;
            if (!display_unseen(now)) present_shown();
        }
    }
    return enter;
}

static void run_script(void) {
    {   /* CLOCK_TRACE=F0-F1: log the logical clock's entries and I/O in that frame window */
        static int init; static unsigned long long f0, f1;
        if (!init) { init = 1; const char *a = getenv("CLOCK_TRACE"); if (a && sscanf(a, "%llu-%llu", &f0, &f1) != 2) f0 = f1 = 0; }
        if (f1) clock_trace = frame_no() >= f0 && frame_no() < f1;
    }
    pointer_step(&spt[0]);
    if (net_active) {                          /* a network game: frame N once the server has sent it all */
        net_frame(frame_no());
        net_inputs(0, seat_out[0]);
        if (frame_no() % 70 == 0) {
            char m[64];
            snprintf(m, sizeof m, "HASH %llu %016llx", (unsigned long long)frame_no(), (unsigned long long)state_hash());
            net_send(m);
        }
    }
    if (dscript_pos < dscript_n) {           /* day lines: the game's day and hour */
        if (use_native) state_export();
        u32 dg = ((u32)LOADSEG + 0x39D4) << 4;
        int day = M[dg + 0x71E4] | M[dg + 0x71E5] << 8, slot = M[dg + 0x71EA] | M[dg + 0x71EB] << 8;
        int hour = slot < 17 ? slot + 7 : slot - 17;   /* (clock_hour) */
        while (dscript_pos < dscript_n && (day > dscript[dscript_pos].day || (day == dscript[dscript_pos].day && hour >= dscript[dscript_pos].hour))) {
            DayLine *d = &dscript[dscript_pos++];
            if (!use_native) { fprintf(stderr, "[script] txbuy: the decompiled build only\n"); continue; }
            int held = difftest_active;        /* (no timer events meanwhile, as "call") */
            difftest_active = 1;
            if (!game_test_txbuy(atoi(d->a), d->y))
                fprintf(stderr, "[script] day %d: txbuy %s %d bought nothing\n", day, d->a, d->y);
            difftest_active = held;
        }
    }
    while (script_pos < script_n && script[script_pos].frame <= frame_no()) {
        ScriptLine *s = &script[script_pos++];
        if (s->seat) continue;                 /* another seat's: host_seat_input */
        if (net_active && (input_line_cmd(s->cmd) || is_motion(s->cmd))) continue;   /* (sent to the server) */
        if (seat_out[0] && !is_motion(s->cmd) && (input_line_cmd(s->cmd))) {   /* out of the game: it only watches */
            fprintf(stderr, "[script] frame %llu: seat 0 is out, %s ignored\n", (unsigned long long)frame_no(), s->cmd);
            continue;
        }
        /* the decompiled build keeps some game state in host structures
         * (src/state.c): the harness reads and writes the original layout */
        int touches = !strcmp(s->cmd, "hash") || !strcmp(s->cmd, "dump") || !strcmp(s->cmd, "call") ||
                      !strcmp(s->cmd, "poke16") || !strncmp(s->cmd, "peek", 4);
        if (touches && use_native) state_export();
        if (input_line(s, &spt[0])) ;
        else if (!strcmp(s->cmd, "poke16")) {
            /* test harness only: N poke16 SEG:OFF VALUE (SEG relative to the load segment) */
            unsigned sg = 0, of = 0;
            if (sscanf(s->a, "%x:%x", &sg, &of) == 2) {
                u32 a = ((u32)(LOADSEG + sg) << 4) + of;
                M[a] = (u8)s->y; M[a + 1] = (u8)((u16)s->y >> 8);
            }
            if (use_native) state_reload();
        }
        else if (!strcmp(s->cmd, "peek8") || !strcmp(s->cmd, "peek16") || !strcmp(s->cmd, "peek32")) {
            /* test harness: N peek8|peek16|peek32 SEG:OFF -> "PEEK frame N SEG:OFF value" on stdout */
            unsigned sg = 0, of = 0;
            if (sscanf(s->a, "%x:%x", &sg, &of) == 2) {
                u32 a = ((u32)(LOADSEG + sg) << 4) + of;
                unsigned v = s->cmd[4] == '8' ? M[a] : (unsigned)(M[a] | M[a + 1] << 8);
                if (s->cmd[4] == '3') v |= (unsigned)(M[a + 2] | M[a + 3] << 8) << 16;
                fprintf(stdout, "PEEK frame %llu %04X:%04X %u\n", (unsigned long long)frame_no(), sg, of, v);
                fflush(stdout);
            }
        }
        else if (!strcmp(s->cmd, "call")) {
            /* test harness: N call SEG:OFF[,ARG...] -- call a game function (far, C convention,
             * word arguments) and print "CALL frame N SEG:OFF,ARGS ax dx"; registers are restored */
            unsigned sg = 0, of = 0; u16 args[16]; int n = 0;
            if (sscanf(s->a, "%x:%x", &sg, &of) == 2) {
                for (char *p = strchr(s->a, ','); p && n < 16; p = strchr(p + 1, ','))
                    args[n++] = (u16)strtol(p + 1, NULL, 0);
                /* the call runs inside a frame boundary, wherever the game happens
                 * to be: give it DS = ES = DGROUP like a call from game code, and
                 * hold timers, interrupts and frame events (and so this script)
                 * until it returns, as the difftest does */
                CPU saved = R;
                R.ds = R.es = (u16)(LOADSEG + 0x39D4);
                for (int i = n - 1; i >= 0; i--) push16(args[i]);
                push16(R.cs); push16(0xFFFF);
                int held = difftest_active;
                difftest_active = 1;
                call_far((u16)(LOADSEG + sg), (u16)of);
                difftest_active = held;
                u16 ax = R.ax, dx = R.dx;
                R = saved;
                fprintf(stdout, "CALL frame %llu %s %u %u\n", (unsigned long long)frame_no(), s->a, ax, dx);
                fflush(stdout);
            }
        }
        else if (!strcmp(s->cmd, "txbuy")) {
            /* test harness: N txbuy PLAYER MAXFEE -- that player buys the site with the most new
             * viewers in a price class up to MAXFEE (src/ai_improve.c game_test_txbuy; the day
             * lines "@DAY.HOUR txbuy" above do it at a game time) */
            int held = difftest_active;
            difftest_active = 1;
            if (!use_native || !game_test_txbuy(s->x, s->y))
                fprintf(stderr, "[script] frame %llu: txbuy %s %d bought nothing\n", (unsigned long long)frame_no(), s->a, s->y);
            difftest_active = held;
        }
        else if (!strcmp(s->cmd, "shot")) {
            if (s->y <= 0) save_png_or_ppm(s->a);
            else if (!seat_view(s->y, save_png_or_ppm, s->a))
                fprintf(stderr, "[shot] frame %llu: seat %d is not playing\n", (unsigned long long)frame_no(), s->y);
        }
        else if (!strcmp(s->cmd, "shothd")) save_png_4x(s->a, 0);
        else if (!strcmp(s->cmd, "shothr")) {
            if (!DISPLAY_IS_HR(display_filter)) fprintf(stderr, "shothr: needs --filter hr-pixel (the decompiled build)\n");
            else save_png_4x(s->a, 1);
        }
        else if (!strcmp(s->cmd, "kitoggle")) {     /* Ctrl+K: the AI pictures off / on (tools/kitest.py) */
            fprintf(stderr, "[ki] frame %llu: AI pictures %d\n", (unsigned long long)frame_no(), hr_ki_toggle());
        }
        else if (!strcmp(s->cmd, "arrowstate")) {   /* the system pointer's state as a present sees it (tools/arrowtest.py) */
            int vs = host_shown_seat();
            if (vs <= 0 || !seat_view(vs, print_arrow_state, NULL)) print_arrow_state(NULL);
        }
        else if (!strcmp(s->cmd, "fuzz")) {
            extern void difftest_fuzz(const char *, int);
            difftest_fuzz(s->a, s->y ? s->y : 3);
        }
        else if (!strcmp(s->cmd, "dump")) {
            FILE *df = fopen(s->a, "wb");
            if (df) { fwrite(M, 1, 0x100000, df); fclose(df); fprintf(stderr, "[dump] %s SS:SP=%04X:%04X T=%llu CS:IP=%04X:%04X\n  stack:", s->a, R.ss, R.sp, (unsigned long long)T, R.cs, R.ip);
                for (int k = 0; k < 16; k++) fprintf(stderr, " %04X", rw(R.ss, R.sp + 2 * k));
                fprintf(stderr, "\n"); }
        }
        else if (!strcmp(s->cmd, "hash")) {
            /* FNV-1a over conventional memory except the stack segment (whose
             * dead area holds engine-specific scratch), and over VRAM */
            u32 ss_lo = (u32)R.ss << 4, ss_hi = ss_lo + 0x2000;
            u64 h = 1469598103934665603ull, hv = h;
            for (u32 a = 0x500; a < 0xA0000; a++) { if (a >= ss_lo && a < ss_hi) continue; h = (h ^ M[a]) * 1099511628211ull; }
            for (u32 a = 0xA0000; a < 0xAFA00; a++) hv = (hv ^ M[a]) * 1099511628211ull;
            fprintf(stdout, "HASH frame %llu mem %016llx vram %016llx\n", (unsigned long long)frame_no(), (unsigned long long)h, (unsigned long long)hv);
            /* game state (state_hash) -- what different engines must agree on;
             * the memory hash above also covers stack and heap bytes that the
             * engines leave differently (frames, copies of never-written locals) */
            fprintf(stdout, "STATE frame %llu %016llx vram %016llx\n", (unsigned long long)frame_no(), (unsigned long long)state_hash(), (unsigned long long)hv);
            fflush(stdout);
        }
        else if (!strcmp(s->cmd, "quit")) {
            extern u64 interp_insns;
            fprintf(stderr, "[script] quit at frame %llu, interpreted %llu insns\n", (unsigned long long)frame_no(), (unsigned long long)interp_insns);
            dump_state("quit");
            exit(0);
        }
        else fprintf(stderr, "[script] unknown command %s\n", s->cmd);
    }
}

#ifdef __APPLE__
static void mac_no_nap(void);
#endif
void host_init(int headless, int sc) {
    host_headless = headless;
    scale = sc;
    wall_start_ns = now_ns();
    if (script_path) load_script(script_path);
    if (headless) { realtime = 0; host_drag_ok = 1; return; }   /* (no window: the tests, as if it drew the carried item) */
    if (SDL_Init(SDL_INIT_VIDEO) < 0) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); exit(1); }
    if (scale < 1) scale = 1;
    win = SDL_CreateWindow("OpenMTV", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           320 * scale, (display_square ? 200 : 240) * scale,
                           SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI |
                           (display_fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
    /* in front: started by the launcher (a background process), Windows would
     * leave the window behind the browser the player clicked "Play" in */
#ifdef SDL_HINT_FORCE_RAISEWINDOW
    SDL_SetHint(SDL_HINT_FORCE_RAISEWINDOW, "1");
#endif
    if (win) SDL_RaiseWindow(win);
#ifdef __APPLE__
    if (win) mac_no_nap();
#endif
    /* no vsync: waiting for the display on each of the game's 70 Hz frames
     * throttled the whole game -- music included -- to a 60 Hz display's
     * pace (60/70); the audio or system clock paces, host_present skips */
#ifdef _WIN32
    /* Direct3D 11 before SDL 2's first choice, Direct3D 9: on the Windows test
     * machine (Windows 11, NVIDIA) captures of an SDL window drawn with
     * Direct3D 9 or OpenGL stayed empty, with Direct3D 11 they show the
     * picture.  SDL_RENDER_DRIVER in the environment still decides, and SDL
     * takes the next driver if this one fails. */
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "direct3d11");
    SDL_SetHint(SDL_HINT_RENDER_BATCHING, "1");      /* a named driver turns batching off otherwise */
#endif
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!ren) ren = SDL_CreateRenderer(win, -1, 0);
    tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 320, 200);
    if (display_filter == DISPLAY_SHARP && !SDL_RenderTargetSupported(ren)) display_filter = DISPLAY_NEAREST;
    SDL_SetTextureScaleMode(tex, display_filter == DISPLAY_LINEAR ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
    if (display_filter == DISPLAY_MMPX || DISPLAY_IS_HR(display_filter)) {
        tex4 = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 1280, 800);
        if (tex4) SDL_SetTextureScaleMode(tex4, SDL_ScaleModeLinear);
        else fprintf(stderr, "SDL: no 1280x800 texture (%s): plain display\n", SDL_GetError());
    }
    SDL_SetRelativeMouseMode(mouse_free ? SDL_FALSE : SDL_TRUE);
    if (adlib_enabled && SDL_InitSubSystem(SDL_INIT_AUDIO) == 0) {
        SDL_AudioSpec want = {0}, have;
        want.freq = 44100; want.format = AUDIO_S16SYS; want.channels = 2; want.samples = 1024;
        audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
        if (audio_dev) SDL_PauseAudioDevice(audio_dev, 0);
        else fprintf(stderr, "SDL audio: %s (continuing without sound)\n", SDL_GetError());
    }
}

static int net_gentle;                 /* a network game catching up gently (host_sleep_until_T): pieces left out */
/* How far the sound runs ahead of the device (the game paced by it, host_sleep_until_T):
 * 100 ms; after a gap -- the queue ran dry: the game stood (a window on another desktop or
 * being moved, a moment held up) -- half again as much, up to 250 ms, then 10 ms less for
 * every minute without one (a machine that stands now and then keeps it: a desktop switch
 * comes minutes apart).  A steady 70 ms broke off at every such moment; the sound now
 * follows the picture that much later, and only after a gap more. */
#define AUDIO_LEAD_MIN 100.0
#define AUDIO_LEAD_MAX 250.0
static double audio_lead_ms = AUDIO_LEAD_MIN;
static int audio_cleared;              /* the queue emptied on purpose (catching up): no gap */
static void audio_lead_adapt(double queued_ms) {
    static u64 pushes, calm_since;
    static int armed;                    /* the queue was full again since the last gap (not still refilling) */
    u64 now = now_ns();
    if (!calm_since) calm_since = now;
    if (queued_ms > audio_lead_ms / 2) armed = 1;
    if (++pushes > 100 && armed && queued_ms < 1.0 && !audio_cleared) {
        audio_lead_ms = audio_lead_ms * 1.5 > AUDIO_LEAD_MAX ? AUDIO_LEAD_MAX : audio_lead_ms * 1.5;
        calm_since = now;
        armed = 0;
        fprintf(stderr, "[audio] the sound ran dry (the game stood): %.0f ms ahead from now on\n", audio_lead_ms);
    } else if (now - calm_since > 60000000000ull) {
        calm_since = now;
        if (audio_lead_ms > AUDIO_LEAD_MIN) audio_lead_ms = audio_lead_ms - 10 < AUDIO_LEAD_MIN ? AUDIO_LEAD_MIN : audio_lead_ms - 10;
    }
    audio_cleared = 0;
}
void host_audio_push(const int16_t *s, size_t frames) {
    if (!audio_dev) return;
    audio_lead_adapt(SDL_GetQueuedAudioSize(audio_dev) / 4.0 / 44.1);
    static int log = -1;                 /* AUDIO_LOG=1: the queue level, every 20th push and when nearly empty */
    if (log < 0) log = getenv("AUDIO_LOG") != NULL;
    if (log) {
        static int n; static u64 t0; u64 t = now_ns(); if (!t0) t0 = t;
        double q = SDL_GetQueuedAudioSize(audio_dev) / 4 / 44.1;
        if (n++ % 20 == 0 || (n > 50 && q < 10)) fprintf(stderr, "AQ wall=%.3f frame=%llu frames_pushed=%zu queued_ms=%.1f%s\n", (t - t0) / 1e9, (unsigned long long)frame_no(), frames, q, q < 10 ? " LOW" : "");
    }
    /* keep latency bounded: drop audio queued well beyond the lead (catching up) */
    if (SDL_GetQueuedAudioSize(audio_dev) / 4.0 / 44.1 > audio_lead_ms + 130) return;
    static unsigned gentle_k;            /* a network game catching up gently: one piece in eight left out */
    if (net_gentle && ++gentle_k % 8 == 0) return;
    SDL_QueueAudio(audio_dev, s, (Uint32)(frames * 4));
}

/* the picture's place for the current window size (and fullscreen) */
static void display_layout(void) {
    int ow, oh, ww, wh;
    SDL_GetRendererOutputSize(ren, &ow, &oh);
    SDL_GetWindowSize(win, &ww, &wh);
    double ph = display_square ? 200.0 : 240.0;          /* height per 320 of width */
    double s = ow / 320.0 < oh / ph ? ow / 320.0 : oh / ph;
    if (display_integer && s >= 1) s = (int)s;
    int w = (int)(320 * s + 0.5), h = (int)(ph * s + 0.5);
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    view = (SDL_Rect){ (ow - w) / 2, (oh - h) / 2, w, h };
    view_wpt = (float)w * ww / ow;
    view_hpt = (float)h * wh / oh;
    view_xpt = (float)view.x * ww / ow;
    view_ypt = (float)view.y * wh / oh;
    if (display_filter == DISPLAY_SHARP && !tex4) {
        int k = (w + 319) / 320, kh = (h + 199) / 200;  /* whole multiple at least the view's size */
        if (kh > k) k = kh;
        if (k > 16) k = 16;
        if (k != up_k || !tex_up) {
            if (tex_up) SDL_DestroyTexture(tex_up);
            tex_up = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, 320 * k, 200 * k);
            if (tex_up) SDL_SetTextureScaleMode(tex_up, SDL_ScaleModeLinear);
            up_k = k;
        }
    }
}

static void display_toggle_fullscreen(void) {
    int fs = (SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN) != 0;
    SDL_SetWindowFullscreen(win, fs ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
    settings_set("display", "fullscreen", fs ? "no" : "yes");   /* remembered (with --settings) */
    display_layout();
}

/* the screen now in memory (the shown seat's) into the window */
/* Notices over the picture -- what the network game tells this player (net.c): a player left,
 * the connection to one is gone and the game waits, it goes on, the game's end.  The picture
 * itself is the game's and stays as it is: the notice is drawn over it, in the window only,
 * in a font of its own (runtime/notice_font.h), at the top, for a few seconds.  Headless (the
 * tests) it is a line "[notice] ..." on stderr, as it is everywhere. */
#include "notice_font.h"
static void present_screen(const char *unused);
static unsigned char notice_text[2][100];     /* Latin-1, at most two lines */
static u64 notice_until_ns;
static SDL_Texture *notice_tex;
void host_notice(const char *utf8, double seconds) {
    fprintf(stderr, "[notice] %s\n", utf8);
    host_notice_quiet(utf8, seconds);
}
/* the same without the line on stderr (a notice that counts down) */
void host_notice_quiet(const char *utf8, double seconds) {
    memset(notice_text, 0, sizeof notice_text);
    int line = 0, n = 0;
    for (const unsigned char *s = (const unsigned char *)utf8; *s && line < 2; s++) {
        unsigned c = *s;
        if (c == '\n') { line++; n = 0; continue; }
        if (c >= 0xC0 && c < 0xE0 && (s[1] & 0xC0) == 0x80) { c = ((c & 0x1F) << 6) | (s[1] & 0x3F); s++; }
        else if (c >= 0x80) continue;                /* (beyond Latin-1: left out) */
        if (c > 0xFF) c = '?';
        if (n < (int)sizeof notice_text[0] - 1) notice_text[line][n++] = (unsigned char)c;
    }
    notice_until_ns = now_ns() + (u64)(seconds * 1e9);
}
/* the notice over the picture (before the present) */
static void draw_notice(void) {
    if (!notice_until_ns || now_ns() > notice_until_ns) return;
    enum { W = 320, LINE = NOTICE_FONT_H + 2 };
    int lines = notice_text[1][0] ? 2 : 1, h = lines * LINE + 4;
    static uint32_t px[W * (2 * LINE + 4)];
    if (!notice_tex) {
        notice_tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, W, 2 * LINE + 4);
        if (!notice_tex) return;
        SDL_SetTextureBlendMode(notice_tex, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(notice_tex, SDL_ScaleModeNearest);
    }
    for (int i = 0; i < W * h; i++) px[i] = 0xD0101018u;          /* a dark band, the picture shines through a little */
    for (int l = 0; l < lines; l++) {
        int len = (int)strlen((const char *)notice_text[l]);
        if (len * NOTICE_FONT_W > W) len = W / NOTICE_FONT_W;
        int x0 = (W - len * NOTICE_FONT_W) / 2, y0 = 2 + l * LINE + 1;
        for (int k = 0; k < len; k++) {
            const unsigned char *g = notice_font[notice_text[l][k]];
            for (int r = 0; r < NOTICE_FONT_H; r++)
                for (int c = 0; c < NOTICE_FONT_W; c++)
                    if (g[r] & (0x80 >> c)) px[(y0 + r) * W + x0 + k * NOTICE_FONT_W + c] = 0xFFFFF4D0u;   /* warm white */
        }
    }
    SDL_Rect src = {0, 0, W, h};
    SDL_UpdateTexture(notice_tex, &src, px, W * 4);
    SDL_Rect dst = {view.x, view.y, view.w, view.h * h / 200};
    SDL_RenderCopy(ren, notice_tex, &src, &dst);
}
/* a human set the game speed (the Control-Center; every machine computes it): the other
 * players' windows say who and which -- the speed is everyone's */
void mp_notice_speed(int speed) {
    extern int seat_cur, seat_humans;
    if (!seat_humans || seat_cur < 0 || seat_cur == host_shown_seat()) return;
    char who[24], t[120];
    seat_name(seat_cur, who, sizeof who);
    const char *de = speed <= 20 ? "langsam" : speed <= 30 ? "normal" : speed <= 60 ? "schnell" : "sehr schnell";
    const char *en = speed <= 20 ? "slow" : speed <= 30 ? "normal" : speed <= 60 ? "fast" : "very fast";
    if (notice_german) snprintf(t, sizeof t, "%s stellt das Tempo auf %s.", who, de);
    else snprintf(t, sizeof t, "%s sets the game speed to %s.", who, en);
    host_notice(t, 4);
}

/* the notice stays a moment before the window closes (the network game's end) */
void host_notice_hold(double seconds) {
    if (host_headless || !ren) return;
    u64 end = now_ns() + (u64)(seconds * 1e9);
    if (notice_until_ns < end) notice_until_ns = end;
    while (now_ns() < end) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) if (e.type == SDL_QUIT || e.type == SDL_KEYDOWN || e.type == SDL_MOUSEBUTTONDOWN) return;
        present_screen(NULL);
        sleep_ns(30000000);
    }
}

/* A window nobody sees -- on another desktop, minimized: macOS lets a present into it block
 * (up to a second each), and in a network game the others wait for this machine meanwhile
 * (their sound breaks off).  So: no presents while the window is hidden or minimized, nor
 * for a while after one took long (100 ms) -- until the window shows signs of life (an event
 * of it, the pointer over it, a key) or 3 s have gone by.  The game, its sound and the
 * network go on all the while.  macOS tells SDL 2 nothing of another desktop: there the
 * window's occlusion state is asked directly (mac_window_unseen), before a present blocks
 * -- once only the first long present told, and the sound broke off at every switch. */
#ifdef __APPLE__
#include <SDL_syswm.h>
#include <objc/runtime.h>
#include <objc/message.h>
extern void *objc_autoreleasePoolPush(void);
extern void objc_autoreleasePoolPop(void *);
/* AppKit through the Objective-C runtime (plain C, no Objective-C compiler): the window's
 * occlusionState lacks NSWindowOcclusionStateVisible (on another desktop, covered whole) */
static int mac_window_unseen(void) {
    static id nswin;
    static int tried;
    if (!tried) {
        SDL_SysWMinfo i;
        tried = 1;
        SDL_VERSION(&i.version);
        if (SDL_GetWindowWMInfo(win, &i) && i.subsystem == SDL_SYSWM_COCOA) nswin = (id)i.info.cocoa.window;
    }
    if (!nswin) return 0;
    unsigned long st = ((unsigned long (*)(id, SEL))objc_msgSend)(nswin, sel_registerName("occlusionState"));
    return !(st & 2);
}
/* no App Nap, no timer coalescing for the game: a latency-critical activity the user started
 * (NSActivityUserInitiatedAllowingIdleSystemSleep | NSActivityLatencyCritical) -- macOS slows
 * an app whose windows nobody sees, and the sound runs dry */
static void mac_no_nap(void) {
    void *pool = objc_autoreleasePoolPush();
    id pi = ((id (*)(id, SEL))objc_msgSend)((id)objc_getClass("NSProcessInfo"), sel_registerName("processInfo"));
    id why = ((id (*)(id, SEL, const char *))objc_msgSend)((id)objc_getClass("NSString"), sel_registerName("stringWithUTF8String:"),
                                                          "Mad TV: the game and its sound in time");
    id act = pi && why ? ((id (*)(id, SEL, unsigned long long, id))objc_msgSend)(pi, sel_registerName("beginActivityWithOptions:reason:"),
                                                                                 0x00EFFFFFULL | 0xFF00000000ULL, why) : NULL;
    if (act) ((id (*)(id, SEL))objc_msgSend)(act, sel_registerName("retain"));   /* (held for the game's life) */
    else fprintf(stderr, "[display] macOS: no activity (App Nap stays)\n");
    objc_autoreleasePoolPop(pool);
}
#endif
/* SDL 2 hides the pointer on macOS by an invisible cursor rectangle of the window, which
 * AppKit gives up again after some seconds: the system's arrow came back over the game's
 * own cursor (the game's start, a film carried in the planner, a satellite on the map;
 * not on Windows).  There the cursor is hidden for the application as well, by NSCursor's
 * hide and unhide -- counted by AppKit, so each only when the state changes. */
#ifdef __APPLE__
static void mac_cursor(int show) {
    static int hidden;
    if (show == !hidden) return;
    void *pool = objc_autoreleasePoolPush();
    ((void (*)(id, SEL))objc_msgSend)((id)objc_getClass("NSCursor"), sel_registerName(show ? "unhide" : "hide"));
    objc_autoreleasePoolPop(pool);
    hidden = !show;
}
#endif
static void pointer_show(int show) {
    SDL_ShowCursor(show ? SDL_ENABLE : SDL_DISABLE);
#ifdef __APPLE__
    mac_cursor(show);
#endif
}

static void present_now(void) {
    u64 t0 = now_ns();
    SDL_RenderPresent(ren);
    u64 took = now_ns() - t0;
    if (took > 100000000) {
        present_blocked_until = now_ns() + 3000000000ull;
        fprintf(stderr, "[display] a present took %llu ms: none for a while (the window unseen?)\n", (unsigned long long)(took / 1000000));
    }
}
static int display_unseen(u64 now) {
    if (SDL_GetWindowFlags(win) & (SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED)) return 1;
#ifdef __APPLE__
    if (mac_window_unseen()) return 1;
#endif
    return now < present_blocked_until;
}

static void present_screen(const char *unused) {
    (void)unused;
    system_pointer();                   /* (the seat shown is the one loaded now: seat_view) */
    display_layout();
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    if (tex4) {
        render_4x();
        SDL_UpdateTexture(tex4, NULL, rgba4, 1280 * 4);
        SDL_RenderCopy(ren, tex4, NULL, &view);
        draw_drag();
        draw_notice();
        present_now();
        return;
    }
    render_rgba();
    SDL_UpdateTexture(tex, NULL, rgba, 320 * 4);
    if (display_filter == DISPLAY_SHARP && tex_up) {
        SDL_SetRenderTarget(ren, tex_up);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        SDL_SetRenderTarget(ren, NULL);
        SDL_RenderCopy(ren, tex_up, NULL, &view);
    } else {
        SDL_RenderCopy(ren, tex, NULL, &view);
    }
    draw_drag();
    draw_notice();
    present_now();
}

/* A network game: once a minute a line on how far this machine ran behind the server's front
 * (its own clicks come that much later; the game's log: build/game.log from the launcher)
 * and its longest frame */
static void net_minute(void) {
    static u32 hist[64];
    static u64 start, last, longest;
    u64 now = now_ns(), c = net_complete(), f = frame_no(), lag = c > f ? c - f : 0;
    if (!start) start = now;
    if (last && now - last > longest) longest = now - last;
    last = now;
    hist[lag < 63 ? lag : 63]++;
    if (now - start < 60000000000ull) return;
    u32 n = 0, k = 0, med = 0, p90 = 0, max = 0;
    for (int i = 0; i < 64; i++) n += hist[i];
    for (int i = 0; i < 64; i++) {
        if (hist[i]) max = i;
        if (k < n / 2 && k + hist[i] >= n / 2) med = i;
        if (k < n * 9 / 10 && k + hist[i] >= n * 9 / 10) p90 = i;
        k += hist[i];
    }
    fprintf(stderr, "[net] the last minute: behind the server median %u, p90 %u, at most %u%s frames (lead %d); longest frame %llu ms\n",
            med, p90, max, max == 63 ? "+" : "", net_lead, (unsigned long long)(longest / 1000000));
    memset(hist, 0, sizeof hist);
    start = now; longest = 0;
}

/* the measuring run of tools/hrsurvey.c (build/mtv-dec-hrs) and the self-test of
 * --filter hr (src/hr.c, HR_SELFTEST): the screen at every frame */
void (*hrs_frame_hook)(void);
void (*hr_frame_hook)(void);

void host_present(void) {
    if (hrs_frame_hook) hrs_frame_hook();
    if (hr_frame_hook) hr_frame_hook();
    {   /* FRAME_LOG=1: the wall time of every frame (tools: the pace's smoothness in a network game) */
        static int log = -1;
        if (log < 0) log = getenv("FRAME_LOG") != NULL;
        if (log) fprintf(stderr, "FT %llu %llu %llu\n", (unsigned long long)now_ns(), (unsigned long long)frame_no(),
                         (unsigned long long)(net_active ? net_complete() : 0));
    }
    if (!host_headless) apply_live_input();
    if (net_active && !host_headless) net_minute();
    {   /* STALL_TEST=MS:S: the game stands MS ms every S s, its sound goes on (a present that
         * blocks: a window on another desktop) -- tests of the sound's lead */
        static int ms = -1, every;
        static u64 next;
        if (ms < 0) { const char *e = getenv("STALL_TEST"); ms = 0; if (e) sscanf(e, "%d:%d", &ms, &every); }
        if (ms > 0 && every > 0) {
            u64 now = now_ns();
            if (!next) next = now + (u64)every * 1000000000ull;
            if (now >= next) { sleep_ns((u64)ms * 1000000ull); next = now + (u64)every * 1000000000ull; }
        }
    }
    run_script();
    if ((nclamp || dumpon_dir) && use_native) state_export();   /* game state in host memory (src/state.c) */
    if (nclamp) apply_clamps();
    if (dumpon_dir) dump_on_change();
    if (nclamp && use_native) state_reload();
    if (host_headless) return;
    /* at most ~66 presents per second: the game draws at the VGA's 70 Hz,
     * displays mostly show 60 (and a driver that waits for the display
     * anyway then waits briefly instead of once per game frame) */
    static u64 last_present_ns;
    u64 now = now_ns();
    if (now - last_present_ns < 15000000) return;
    last_present_ns = now;
    if (display_unseen(now)) return;
    present_shown();
}

/* the screen of this machine's seat */
static void present_shown(void) {
    int vs = host_shown_seat();
    if (vs > 0 && seat_view(vs, present_screen, NULL)) return;
    present_screen(NULL);
}

static u16 sdl_to_bios(SDL_Keysym ks) {
    SDL_Keycode k = ks.sym;
    int shift = ks.mod & KMOD_SHIFT;
    switch (k) {
    case SDLK_RETURN: return 0x1C0D;
    case SDLK_ESCAPE: return 0x011B;
    case SDLK_SPACE: return 0x3920;
    case SDLK_BACKSPACE: return 0x0E08;
    case SDLK_TAB: return 0x0F09;
    case SDLK_UP: return 0x4800; case SDLK_DOWN: return 0x5000;
    case SDLK_LEFT: return 0x4B00; case SDLK_RIGHT: return 0x4D00;
    }
    if (k >= SDLK_F1 && k <= SDLK_F10) return (u16)((0x3B + (k - SDLK_F1)) << 8);
    if (k >= SDLK_a && k <= SDLK_z) {
        static const u8 sc[26] = {0x1E,0x30,0x2E,0x20,0x12,0x21,0x22,0x23,0x17,0x24,0x25,0x26,0x32,0x31,0x18,0x19,0x10,0x13,0x1F,0x14,0x16,0x2F,0x11,0x2D,0x15,0x2C};
        u8 c = (u8)(shift ? k - 32 : k);
        return (u16)(sc[k - SDLK_a] << 8 | c);
    }
    if (k >= SDLK_0 && k <= SDLK_9) return (u16)(((k == SDLK_0 ? 0x0B : 0x02 + k - SDLK_1) << 8) | k);
    if (k < 128) return (u16)k;
    return 0;
}

void host_poll(void) {
    if (host_headless) return;
    SDL_Event e;
    static float accx, accy;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_WINDOWEVENT || e.type == SDL_MOUSEMOTION || e.type == SDL_KEYDOWN || e.type == SDL_MOUSEBUTTONDOWN)
            present_blocked_until = 0;           /* (the window seen again: present_now) */
        switch (e.type) {
        case SDL_QUIT: exit(0);
        case SDL_KEYDOWN: {
            /* fullscreen on and off: F11, Alt+Enter, and on the Mac Cmd+Ctrl+F or Cmd+F */
            SDL_Keycode sym = e.key.keysym.sym;
            Uint16 mod = e.key.keysym.mod;
            if (sym == SDLK_F11 || ((sym == SDLK_RETURN || sym == SDLK_KP_ENTER) && (mod & KMOD_ALT)) ||
                (sym == SDLK_f && (mod & KMOD_GUI))) {
                if (!e.key.repeat) display_toggle_fullscreen();
                break;
            }
            if (e.key.keysym.sym == SDLK_F12) { char n[64]; snprintf(n, sizeof n, "shot_%llu.ppm", (unsigned long long)frame_no()); save_png_or_ppm(n); break; }
            if (sym == SDLK_k && (mod & (KMOD_CTRL | KMOD_GUI))) {   /* the AI pictures off and on (src/hr.c) */
                int on = e.key.repeat ? -2 : hr_ki_toggle();
                if (on >= 0) host_notice(notice_german ? (on ? "KI-Upscaling an" : "KI-Upscaling aus") : (on ? "AI upscaling on" : "AI upscaling off"), 2);
                break;
            }
            if (e.key.keysym.sym == SDLK_PAUSE) { if (!mouse_free) SDL_SetRelativeMouseMode(!SDL_GetRelativeMouseMode()); break; }
            u16 k = sdl_to_bios(e.key.keysym);
            if (k && live_nkeys < 64) live_keys[live_nkeys++] = k;
        } break;
        case SDL_MOUSEMOTION: {
            if (view_wpt < 1) display_layout();
            if (mouse_free) {            /* the pointer's position counts; hidden over the picture */
                ptr_x = e.motion.x; ptr_y = e.motion.y; ptr_moved = 1;
                if (mouse_system) break;  /* (system_pointer, once the motion has gone to the game) */
                int inside = ptr_x >= view_xpt && ptr_x < view_xpt + view_wpt && ptr_y >= view_ypt && ptr_y < view_ypt + view_hpt;
                pointer_show(!inside);
                break;
            }
            /* across the picture (not the whole window) = across the game's screen */
            accx += e.motion.xrel * 320.0f / view_wpt; accy += e.motion.yrel * 200.0f / view_hpt;
            int dx = (int)accx, dy = (int)accy;
            accx -= dx; accy -= dy;
            live_dx += dx; live_dy += dy;
        } break;
        case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP: {
            int bit = e.button.button == SDL_BUTTON_LEFT ? 1 : e.button.button == SDL_BUTTON_RIGHT ? 2 : 0;
            int last = btn_n ? btn_queue[btn_n - 1] : applied_buttons;
            int st = e.type == SDL_MOUSEBUTTONDOWN ? (last | bit) : (last & ~bit);
            if (bit && st != last && btn_n < 32) btn_queue[btn_n++] = st;
            else if (bit && st != last)
                fprintf(stderr, "[input] frame %llu: a mouse button left out (32 changes waiting)\n", (unsigned long long)frame_no());
        } break;
        case SDL_WINDOWEVENT:   /* (the game's log: where clicks went astray -- a click that only focused the window) */
            if (e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED || e.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
                fprintf(stderr, "[window] frame %llu: focus %s; buttons held: SDL %u, the game %d\n", (unsigned long long)frame_no(),
                        e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED ? "gained" : "lost", (unsigned)SDL_GetMouseState(NULL, NULL), applied_buttons);
            break;
        }
    }
}

/* With sound on, the audio device's clock paces the game: virtual time may
 * run only as far ahead of the device as audio_lead_ms of queued audio.
 * Pacing by the system clock instead lets the two clocks drift apart (the
 * queue drained ~3.5 ms per second here, faster on macOS), and once it is
 * empty the sound stutters and the music drags. */
/* A network game: the server's clock paces the game (net.h); this machine
 * keeps its own pace as well -- the picture and the sound steady -- but
 * where it is more than NET_LAG frames behind what the server has sent, it
 * catches up at full speed (and drops sound queued too far ahead). */
/* How far behind the server's front this machine keeps: about the server's lead over the
 * fastest client (LEAD n, net.h: the longest round trip and 2 frames) and 2 more.  Further
 * behind -- its clock a little slower than the fastest machine's (sound cards never run
 * quite alike), or after a moment held up -- it catches up gently: one in eight of its
 * sound's pieces is left out (or the system clock runs an eighth faster), the game a
 * little faster, nothing jumps.  More than NET_FAR frames further (it was held up), at
 * full speed.  A machine that lagged until a fixed 20 frames before catching up played
 * its own inputs up to 0.3 s late: dragging an ad or a film felt spongy. */
#define NET_FAR 25
void host_sleep_until_T(u64 t) {
    if (!realtime) return;
    net_gentle = 0;
    if (net_active) {
        u64 c = net_complete(), f = frame_no(), lag = c > f ? c - f : 0, near = (u64)net_lead + 2;
        if (lag > near + NET_FAR) {
            if (audio_dev && SDL_GetQueuedAudioSize(audio_dev) / 4.0 / 44.1 > 4 * audio_lead_ms) { SDL_ClearQueuedAudio(audio_dev); audio_cleared = 1; }
            return;
        }
        net_gentle = lag > near;
    }
    if (audio_dev) {
        double queued_ms = SDL_GetQueuedAudioSize(audio_dev) / 4.0 / 44.1;
        if (queued_ms > audio_lead_ms + 2) {
            double ms = queued_ms - audio_lead_ms;
            if (ms > 50) ms = 50;
            sleep_ns((u64)(ms * 1e6));
        }
        /* keep the system-clock mapping current, for when there is no audio */
        wall_start_ns = now_ns() - (u64)(t / insns_per_us * 1000.0);
        return;
    }
    static double pace_scale = -1;               /* PACE_SCALE=1.003: this machine's clock 0.3 % slow (tests: two machines' clocks apart) */
    if (pace_scale < 0) { const char *e = getenv("PACE_SCALE"); pace_scale = e ? atof(e) : 1.0; if (pace_scale <= 0) pace_scale = 1.0; }
    u64 now = now_ns();
    {   static u64 last;               /* catching up gently without sound: the clock an eighth faster */
        if (net_gentle && last && now > last) wall_start_ns -= (now - last) / 8;
        last = now;
    }
    u64 target_ns = wall_start_ns + (u64)(t / insns_per_us * 1000.0 * pace_scale);
    if (target_ns > now + 1000000) {
        u64 d = target_ns - now;
        if (d > 50000000) { wall_start_ns -= d - 50000000; d = 50000000; }
        sleep_ns(d);
    } else if (now > target_ns + 200000000) {
        wall_start_ns += now - target_ns - 200000000;   /* fell behind: don't try to catch up */
    }
}
