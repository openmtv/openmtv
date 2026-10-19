/* What one seat (one human) owns of the game's memory: the multiplayer
 * seat block (docs/multiplayer-inventar.md §1).  runtime/seat.c saves and
 * loads it when the scheduler switches seats.
 *
 * Two parts:
 *   - ranges of the data segment: the UI state (room and room stack,
 *     cursor, mouse, dragging, tooltips, popups, the boss dialog, Madris,
 *     the planner's and the chart's view, room music) and the data only the
 *     human has (credit, bag, ledger, office messages); from the trace
 *     inventory (tools/mpinventory.py: touched by seat code only, plus the
 *     UI variables the world also reads), checked by hand.  Tables indexed
 *     by player (shopping lists, news inboxes, gifts, the archive...) stay
 *     shared even where only seat code touches them: each seat writes its
 *     own row.  Two traps nearby: g_clock_acc (DS:6143, the game clock)
 *     right after g_frame_timer, the elevator cabin (DS:8BDA) after the
 *     save-slot names.
 *   - the contents of buffers allocated once and drawn per view: the back
 *     buffer, the TV family's sofa, the window buffer, the cursor sprite
 *     the status panel's texts are drawn in.  Their size comes
 *     from the far heap's block header (u32 size | 1 used, 8 bytes before
 *     the data; src/rtl_heap.c).
 *   (and with the fix "title-cache" the film text cache's slots past 48,
 *     film_txt_more: outside the data segment, the texts in the seat's heap)
 * And a far heap of its own for every seat but 0, in the seat window
 * (B000:0000..E7FF:000F, swapped by runtime/seat.c): the heap's variables
 * are in the block, so whatever a seat allocates -- the building picture,
 * status window and background it frees when it enters a room, room
 * graphics, texts, str_format's buffers -- lands in its own heap.  Seat 0
 * keeps the game's heap as it is. */
#include <stdio.h>
#include <stdlib.h>
#include "game.h"
#include "seat.h"
#include "mpsave.h"
#include "fixes.h"
#include "net.h"

typedef struct { u16 off, len; } Range;
static const Range ranges[] = {
    {0x0087, 0x0C},     /* the far heap: base, break, top (each seat has its own heap) */
    {0x00F4, 0x04},     /* building background (day or night) */
    {0x00FC, 0x2C},     /* room graphics pointers 00FC..0127, window buffer pointer */
    {0x032E, 0x58},     /* film agency shelf rectangles */
    {0x0386, 0x16},     /* clip rectangle, drag image and its size and hot spot */
    {0x059C, 0x04},     /* the minute the clock shows, the minute the room was updated (game_time_tick) */
    {0x05A0, 0x46},     /* TV picture cache, TV picture, TV family */
    {0x05E6, 0x04},     /* boss complained, player quit */
    {0x0638, 0x6CC},    /* the room's hot spots */
    {0x0D28, 0x1C},     /* building scroll drawn, cursor shape stack depth, room animation */
    {0x0D68, 0x14},     /* room animation (supermarket ...) */
    {0x0DAF, 0x03}, {0x0E2B, 0x05},   /* office paper pile */
    {0x0E69, 0x2A},     /* ending animation, tooltip state */
    {0x0F7B, 0x2B},     /* tooltip rendering */
    {0x1118, 0x96},     /* building drawing */
    {0x1274, 0x07},     /* transmitter map density */
    {0x12E8, 0x1C},     /* bag drawing */
    {0x1492, 0x60},     /* boss replies, last text, greeted; ui box depth */
    {0x1824, 0x0E},     /* the far heap: first and last block, rover, DOS block size */
    {0x1960, 0x04},     /* rand()'s seed: random_n seeds it anew on every call, what is left is scratch */
    {0x1606, 0x06},     /* room music: sample resource, room on/off */
    {0x16A4, 0x04},     /* mouse motion since the last poll */
    {0x2483, 0x1E},     /* the bag */
    {0x60BB, 0x04},     /* g_txt_index: a text resource's index while it loads (freed after) */
    {0x60BF, 0x82},     /* the office messages */
    {0x6141, 0x02},     /* g_frame_timer (g_clock_acc follows at 6143: the world's) */
    {0x6147, 0x1E},     /* the dragged news record */
    {0x63E5, 0x1EA},    /* text caches, their texts in the seat's heap: films, */
    {0x65CF, 0x546},    /* series, */
    {0x6B15, 0x294},    /* screenplays */
    {0x71F1, 0x54},     /* ledger today */
    {0x7247, 0x01},     /* message list position */
    {0x72BA, 0x90},     /* news text cache */
    {0x734A, 0x02},     /* bag count */
    {0x7492, 0x54},     /* ledger yesterday */
    {0x777C, 0x12C},    /* the TV's title and episode title (tvinfo_fetch_titles) */
    {0x79C6, 0x14},     /* planner scroll, selection, genre popup, cursor shape, redraw */
    {0x79DE, 0x0A},     /* studio and shop view */
    {0x79EA, 0x16},     /* status message, mouse limits, TV family view, chart view, TV channel */
    {0x7A02, 0x04},     /* boss: cancelled hours, missed ads */
    {0x7A0E, 0x04},     /* boss talk step, TV cover */
    {0x7A14, 0x06},     /* TV program quality, caught figure, tooltip visible */
    {0x7A24, 0x04},     /* office papers, dealer pose */
    {0x7A2C, 0x04},     /* the first morning's building; estate agent decoration */
    {0x7A32, 0x02},     /* the boss's mode: 1 the morning's talk (each human's own), 2 greeted */
    {0x7A34, 0x06},     /* alien look, room owner, room animation dirty */
    {0x7A44, 0x0A},     /* present mode, pending mouse motion, latched click */
    {0x7A52, 0x04},     /* estate agent portrait; the player to be thrown out (g_fired_player, each human's) */
    {0x7A60, 0x02},     /* the TV's titles must be fetched again */
    {0x7A6C, 0x1A},     /* TV family frozen, chart image, news view, Madris */
    {0x7A8A, 0x04},     /* back in the building (the clock waits), the time it entered the info board */
    {0x7A8E, 0x04},     /* boss menu and choice */
    {0x7A96, 0x06},     /* news placed, save error, pause request */
    {0x7A9E, 0x01},
    {0x7AB6, 0x19},     /* popup and cursor bookkeeping */
    {0x7ADE, 0x08},     /* credit, catch fine */
    {0x7AFE, 0x04},     /* bank credit */
    {0x7B12, 0x12},     /* player figure, click, drag begin/end, the cursor buffers kept while dragging */
    {0x7B26, 0x1C},     /* cursor drawn at, popup columns and rows */
    {0x7BF6, 0x04},     /* mouse position */
    {0x8336, 0x4C},     /* TV family viewers */
    {0x838C, 0x02},     /* the room */
    {0x8A48, 0x14},     /* room stack */
    {0x8A5C, 0x02},     /* top floor of the building view (the door index follows at 8A60: the world's) */
    {0x8ACC, 0x02},     /* cursor shown */
    {0x8AD4, 0xC8},     /* Madris board */
    {0x8BA4, 0x06},     /* room mode, font, the human's player number */
    {0x8BB2, 0x28},     /* save-slot names (the elevator cabin follows at 8BDA: the world's) */
    {0x8C3A, 0x16},     /* popup sizes, building scroll */
    {0x8C60, 0x0C},     /* popups in use, building redraw */
    {0x8E7E, 0x04},     /* g_bg_half_bmp: half a room background while it is drawn (freed after) */
    {0x8E86, 0x03},     /* status text */
    {0x8E8A, 0x04},     /* status window bitmap (allocated by each seat for the building view) */
    {0x8E8E, 0x02},     /* room stack top */
    {0x8E90, 0x04},     /* cursor composite (each seat its own; dragging allocates bigger ones) */
    {0x8EE2, 0x33},     /* popup backgrounds, drag state, hot spot, transmitter bids */
    {0x8ED6, 0x0C},     /* cursor background copy, building picture, cursor background (each seat its own) */
    {0x8F19, 0x04},     /* the human's player record */
    {0x8F1E, 0x0E},     /* drag image position, cursor shape stack */
    {0x8F3A, 0x21},     /* frame and tile drawing: target, atlas, frame tileset (the drawing's scratch) */
    {0x8F5E, 0x52},     /* tooltip contents and size; g_tmp_b, g_tmp_a (scratch the drawing shares with the world) */
    {0x8FCA, 0x15},     /* savegame buffer, position, direction; drawing scratch */
    {0x8FE0, 0x1A},     /* genre count popup */
    {0x9008, 0x0C},     /* ui box stack, transmitter dialog background */
    {0x9018, 0x04},     /* room sample */
    {0x9020, 0x04},     /* room sample allocation */
    {0x906C, 0xB2},     /* the LZW decoder (src/rtl_gfx.c): its string stack, dictionary pointer, bit */
    {0xA128, 0x08},     /*   position, code; width, bytes left, output pointer; */
    {0xA230, 0x04},     /*   input pointer; */
    {0xA434, 0x02},     /*   the previous code -- a seat unpacks its own pictures */
    {0xA438, 0x24E},    /* the sprite decoder's state (g_huff_*): a seat unpacks its own pictures */
};
#define NRANGES (int)(sizeof ranges / sizeof ranges[0])

/* buffers drawn per view: far pointers in the data segment */
static const u16 bufs[] = { 0x8E82, 0x7BFC, 0x0124,
                            0x8EA8 };       /* cursor sprite 5: ui_status_text draws the panel's texts in it */
#define NBUFS (int)(sizeof bufs / sizeof bufs[0])

static u32 buf_len(u16 ptr_off, u8 **data) {
    u32 p = G32(ptr_off);
    if (!p) return 0;
    u32 lin = ((u32)FPSEG(p) << 4) + FPOFF(p);
    if (lin < 8 || lin >= MEM_SIZE) return 0;
    u32 hdr = M[lin - 8] | (u32)M[lin - 7] << 8 | (u32)M[lin - 6] << 16 | (u32)M[lin - 5] << 24;
    u32 n = (hdr & ~1u) >= 8 ? (hdr & ~1u) - 8 : 0;
    if (!(hdr & 1) || n > 0x10000 || lin + n > MEM_SIZE) return 0;   /* not a used heap block */
    *data = M + lin;
    return n;
}

/* buffer i of those (0..): its linear address and length; 0 past the last
 * (--filter hr carries their cells with the seat, src/hr.c) */
int seat_buf_range(int i, u32 *lin, u32 *len) {
    if (i < 0 || i >= NBUFS) return 0;
    u8 *d = NULL;
    *len = buf_len(bufs[i], &d);
    *lin = d ? (u32)(d - M) : 0;
    return 1;
}

/* SEAT_STATS=1: the buffers found (once) */
static void bufs_report(void) {
    static int done;
    if (done || !getenv("SEAT_STATS")) return;
    done = 1;
    for (int i = 0; i < NBUFS; i++) {
        u8 *d = NULL;
        u32 p = G32(bufs[i]);
        fprintf(stderr, "[seats] buffer DS:%04X -> %04X:%04X (linear %05X): %u bytes\n", bufs[i], FPSEG(p), FPOFF(p),
                ((u32)FPSEG(p) << 4) + FPOFF(p), buf_len(bufs[i], &d));
    }
}

u32 seat_game_size(void) {
    bufs_report();
    u32 n = 0;
    for (int i = 0; i < NRANGES; i++) n += ranges[i].len;
    for (int i = 0; i < NBUFS; i++) { u8 *d; n += 4 + buf_len(bufs[i], &d); }
    return n + sizeof film_txt_more;
}

/* the seat block into buf (at most cap bytes): the bytes written */
u32 seat_game_save(u8 *buf, u32 cap) {
    u32 n = 0;
    for (int i = 0; i < NRANGES; i++) {
        if (n + ranges[i].len > cap) return 0;
        memcpy(buf + n, dg_addr(ranges[i].off), ranges[i].len);
        n += ranges[i].len;
    }
    for (int i = 0; i < NBUFS; i++) {
        u8 *d = NULL;
        u32 len = buf_len(bufs[i], &d);
        if (n + 4 + len > cap) return 0;
        memcpy(buf + n, &len, 4);
        memcpy(buf + n + 4, d, len);
        n += 4 + len;
    }
    if (n + sizeof film_txt_more > cap) return 0;
    memcpy(buf + n, film_txt_more, sizeof film_txt_more);    /* fix "title-cache": the film texts past slot 48 */
    n += sizeof film_txt_more;
    return n;
}

/* back from seat_game_save; the buffers' contents only where the block is
 * still the same size (it is: they are allocated once at the start) */
void seat_game_load(const u8 *buf) {
    u32 n = 0;
    for (int i = 0; i < NRANGES; i++) {
        memcpy(dg_addr(ranges[i].off), buf + n, ranges[i].len);
        n += ranges[i].len;
    }
    for (int i = 0; i < NBUFS; i++) {
        u32 len;
        memcpy(&len, buf + n, 4);
        u8 *d = NULL;
        if (buf_len(bufs[i], &d) == len && len) memcpy(d, buf + n + 4, len);
        n += 4 + len;
    }
    memcpy(film_txt_more, buf + n, sizeof film_txt_more);
}

/* The figures, for the other seats: fig_animate (the world, seat 0) moves
 * them and draws those in seat 0's view; what it would draw -- each figure
 * with its animation frame and the two fields figure_draw reads, which it
 * resets right after drawing -- is noted here, outside the game's memory
 * and time.  Another seat draws the list in its own view (bld_update_frame),
 * as fig_animate and figure_draw (105E:15B6) do. */
static struct { u16 fig, frame, anim, motion; } fig_list[8];
static int fig_n;

void seat_fig_begin(void) { fig_n = 0; }

void seat_fig_note(unsigned fig, unsigned anim_frame) {
    if (!seats_n || fig_n >= 8) return;
    fig_list[fig_n].fig = (u16)fig;
    fig_list[fig_n].frame = (u16)anim_frame;
    fig_list[fig_n].anim = figures(fig).anim;
    fig_list[fig_n].motion = figures(fig).motion;
    fig_n++;
}

static void fig_draw_noted(int k) {
    u16 si = fig_list[k].fig;
    i16 floor = (i16)figures(si).floor, top = (i16)g_bld_top_floor;
    if (floor > top || floor < (i16)(top - 4)) return;
    u16 y = (((0x96 - ((((figures(si).floor) - g_bld_top_floor) + 4) * 0x1E)) + 0xFFEB) - ((g_bld_top_floor * 0x1E) - (g_bld_scroll + 0x6D))) + 4;
    u16 motion = fig_list[k].motion, anim = fig_list[k].anim;
    if (motion == 2 ? (i16)anim >= 4 : motion == 4 && (i16)anim <= 2) y--;    /* the walking bob */
    blit_transparent(MK32(g_figure_sprites_hi, g_figure_sprites_lo + figure_sprite_ofs(si, fig_list[k].frame)),
                     g_screen_buf, figures(si).x, y, 0, 0, 0x10, 0x13, 0);
}

void seat_figures_draw(void) {
    g_clip_on = 1;
    g_clip_y1 = 4;
    g_clip_y2 = 0x71;
    for (int k = 0; k < fig_n; k++)
        if (g_room == ROOM_BUILDING) fig_draw_noted(k);
    g_clip_on = 0;
}

/* Doors, for the other seats: fig_think (the world) opens and closes them
 * by drawing into the building picture of seat 0 (door_draw, 105E:1366);
 * whether a door is open is nowhere else.  Each draw is queued for the
 * other seats, which draw it into their own picture where fig_think would
 * have (bld_update_frame).  Overflowing the queue redraws the building. */
#define DOOR_Q 32
static struct { u16 floor, door, mode; } door_q[SEATS_MAX][DOOR_Q];
static int door_n[SEATS_MAX], door_lost[SEATS_MAX];

void seat_req_door(unsigned floor, unsigned door, unsigned mode) {
    if (seats_n < 2 || seat_cur != 0) return;
    for (int i = 1; i < seats_n && i < SEATS_MAX; i++) {
        if (door_n[i] == DOOR_Q) { door_lost[i] = 1; continue; }
        door_q[i][door_n[i]].floor = (u16)floor;
        door_q[i][door_n[i]].door = (u16)door;
        door_q[i][door_n[i]].mode = (u16)mode;
        door_n[i]++;
    }
}

void seat_doors_draw(void) {
    int s = seat_cur;
    if (s <= 0 || s >= SEATS_MAX) return;
    for (int k = 0; k < door_n[s]; k++) door_draw(door_q[s][k].floor, door_q[s][k].door, door_q[s][k].mode);
    door_n[s] = 0;
    if (door_lost[s]) { door_lost[s] = 0; g_bld_full_redraw = 1; }
}

/* The status panel, the TV and the TV family, for the other seats, as
 * game_time_tick (0054:20E6) draws them for seat 0: the clock when the
 * minute changes; at each broadcast phase (time_hour_tick, 229E:0828: :00
 * news, :05 programme -- both with the viewers --, :55 ads) the money, the
 * TV's picture and titles and the TV family. */
static struct { i16 hour; i8 phase; } seen[SEATS_MAX];

static void slot_redraw(int viewers) {
    if (viewers) ui_draw_viewers();
    if (!(((i16)g_minute) < 55 || ((i16)(clock_hour())) < 1 || g_opt_tv_family != 0)) tv_family_stop();
    g_redraw = 1;
    g_now_title_stale = 1;
    if (g_tv_off == 0) tvinfo_fetch_titles();
    tv_update_picture();
    if (!(((i16)g_minute) < 5 || ((i16)g_minute) >= 55 || g_opt_tv_family != 0)) tv_family_update(0);
    ui_draw_money();
}

/* another human paused: "Pause" in this seat's clock field, its clock again
 * afterwards (the pauser's own field: room_paused, rooms_click.c) */
static int pause_shown[SEATS_MAX];
static void mp_pause_show(void) {
    int s = seat_cur;
    if (s < 0 || s >= SEATS_MAX) return;
    int other = seat_paused_by_other();
    if (other && !pause_shown[s] && g_room != 0xFFFB) {
        pause_shown[s] = 1;
        ui_status_text(G16(0x8814), G16(0x8816), 0xB4);     /* "Pause" */
    } else if (!other && pause_shown[s]) {
        pause_shown[s] = 0;
        G16(0x59C) = 0xFFFF;             /* the clock again at the next tick */
    }
}

void seat_status_tick(void) {
    int s = seat_cur;
    if (seat_humans) mp_pause_show();
    if (seat_humans && seat_paused_by_other()) return;
    if (s <= 0 || s >= SEATS_MAX) return;
    if (seen[s].hour != (i16)g_last_hour || seen[s].phase != (i8)g_broadcast_phase) {
        int viewers = (i8)g_broadcast_phase != seen[s].phase && ((i8)g_broadcast_phase == 1 || (i8)g_broadcast_phase == 2);
        seen[s].hour = (i16)g_last_hour;
        seen[s].phase = (i8)g_broadcast_phase;
        slot_redraw(viewers);
    }
    if (g_minute == G16(0x59C)) return;
    u32 t = str_format(DSP(0x5B7) /* "#02d:#02d" */, W2(G32(0x71EA)));
    ui_status_text(LO16(t), HI16(t), 0xB4);
    mem_free(LO16(t), HI16(t));
    G16(0x59C) = g_minute;
    room_minute_update();                /* its own room's updates (src/main_loop.c) */
}

/* Seat requests: what the world does to every human's view, given by the
 * world in seat 0 -- where it does it for seat 0 -- and carried out by each
 * other seat at the start of its next frame (game_frame).  The day's end, as
 * game_time_tick (0054:20E6) and day_end_go_home (0054:32CD) show it: "Done"
 * in the clock field, the TV off, the viewers; the view going dark once the
 * human is home; the new day's view wiped in, "Start", viewers and money.
 * (The morning at the boss is seat 0's own: a spectator has no station.) */
#define REQ_Q 16
static struct { u8 kind; u16 arg; } req_q[SEATS_MAX][REQ_Q];
static int req_n[SEATS_MAX];
static int caught_pending[SEATS_MAX];   /* a "caught" request is waiting for that seat */

static void req_push(int i, int kind, u16 arg) {
    if (req_n[i] < REQ_Q) { req_q[i][req_n[i]].kind = (u8)kind; req_q[i][req_n[i]].arg = arg; req_n[i]++; }
}

void seat_request(int kind) {
    if (seats_n < 2 || seat_cur != 0) return;
    for (int i = 1; i < seats_n && i < SEATS_MAX; i++) req_push(i, kind, 0);
}

/* Multiplayer: the human of station `player` was caught in another's room
 * (by a computer player: ai_ready_at_target; by a human: door_check_entry,
 * `by` = the catcher's figure + 1).  It pays and is thrown out -- room
 * changes with their wipes, so in its own seat, at the top of its main
 * loop (SEAT_REQ_CAUGHT).  1 while that is still to come. */
int mp_catch_human(int player, int by) {
    for (int i = 0; i < seats_n; i++) {
        if (seat_station[i] != player || seat_out[i]) continue;
        if (!caught_pending[i]) {
            caught_pending[i] = 1;
            req_push(i, SEAT_REQ_CAUGHT, (u16)by);
        }
        return 1;
    }
    return 0;
}

/* Knocking (--knock=N, multiplayer).  The film dealer, the ad agency and the
 * supermarket hold one figure at a time (door_check_entry: "someone inside",
 * 0x12B); a human could stay there as long as it liked while another waited.
 * Now a human who finds a human inside knocks.  The one inside has N game
 * minutes, then it is out -- at the top of its main loop, as when caught.
 * The room then waits as long for the knocker: nobody else gets in (the one
 * thrown out neither, nor a computer player), and the knocker walks in by
 * itself if it still waits at the door.  Without a knock nobody is hurried.
 * A knocker who goes anywhere else (a click on the floor, another door, the
 * lift -- anything but that door again) takes the knock back, and the room it
 * waited for is free again; a second knocker meanwhile finds the room
 * occupied, as before.
 * All of it follows from the door checks and the world's clock, the same on
 * every machine; it is outside the game's memory: the day it was made in
 * ends it (saving is at night).  The notices go to the two humans' windows,
 * one line each -- the knocker's at its turn only: at the knock its status line
 * tells it. */
#define KNOCK_ROOMS 3
static const u8 knock_rooms[KNOCK_ROOMS] = { ROOM_FILM_AGENCY, ROOM_AD_AGENCY, ROOM_SUPERMARKET };
static struct {
    u8 inside, knocker;                  /* stations; inside 0: no knock */
    u8 held_for;                         /* after it: the station the room waits for, 0 none */
    u8 walked;                           /* the knocker sent to the door already (once) */
    u16 floor, door, day;
    u16 at_x;                            /* the knocker's walk target x at its knock (its last click on the door): where it stands */
    u16 until;                           /* game seconds (g_clock_seconds): the one inside out, the room held */
} knock[KNOCK_ROOMS];

static int knock_seat(int station) {
    for (int i = 0; i < seats_n; i++)
        if (seat_station[i] == station && !seat_out[i]) return i;
    return -1;
}

static int knock_past(u16 t) { return (i16)(u16)(g_clock_seconds - t) >= 0; }

/* the knocker still at it: standing where it knocked (the door check left its
 * target door 0xFFFF), or walking to that door again (a click on it, the walk in
 * at its turn).  Anything else -- a click on the floor, another door, the lift --
 * and it went elsewhere. */
static int knock_waits(int k) {
    u16 kf = (u16)(knock[k].knocker - 1);
    if (knock_seat(knock[k].knocker) < 0 || figures(kf).floor != knock[k].floor ||
        figures(kf).target_floor != knock[k].floor) return 0;
    return figures(kf).target_door == knock[k].door ||
           (figures(kf).target_door == 0xFFFF && figures(kf).target_x == knock[k].at_x);
}

/* a notice in that station's window (only the machine that shows it has one) */
static void knock_notice(int station, int name_station, const char *de, const char *en) {
    int s = knock_seat(station), n = knock_seat(name_station);
    if (s < 0 || s != host_shown_seat()) return;
    char who[24], t[160];
    seat_name(n, who, sizeof who);
    snprintf(t, sizeof t, notice_german ? de : en, who, knock_opt);
    host_notice(t, 6);
}

/* The knocker's status line: its "someone is in there" (0x12B) says instead how many game
 * minutes the one inside has left -- at every click on the door anew, and as long as it
 * stands (the status line draws it from the world's clock).  The knock is this seat's own
 * (its station the knocker) while it waits: a knocker who goes elsewhere takes it back. */
void mp_knock_status_text(u32 buf) {
    int me = seat_station[seat_cur], k = 0;
    while (k < KNOCK_ROOMS && !(knock[k].inside && knock[k].knocker == me && knock[k].day == g_day &&
                                knock_waits(k))) k++;
    if (k == KNOCK_ROOMS) return;
    int secs = (i16)(u16)(knock[k].until - g_clock_seconds), mins = secs > 0 ? (secs + 59) / 60 : 0;
    char who[24];                         /* the manager's name in the game (as 0x12A/0x12E) */
    int n = 0;
    for (u8 c; n < (int)sizeof who - 1 && (c = FP8(players(knock[k].inside).name, n)); n++)
        who[n] = c == '~' ? ' ' : (char)c;   /* ('~' breaks the line in the game's texts) */
    who[n] = 0;
    char *s = (char *)HPTR(buf);
    if (!mins) snprintf(s, 0x3E8, g_language == 0 ? "Besetzt! %s muss jetzt raus." : "Occupied! %s has to leave now.", who);
    else if (g_language == 0) snprintf(s, 0x3E8, "Besetzt! Noch %d Min., dann muss %s raus.", mins, who);
    else snprintf(s, 0x3E8, "Occupied! %s has to leave in %d min.", who, mins);
}

void mp_knock(int knocker, int inside, unsigned floor, unsigned door, unsigned room) {
    int k = 0;
    while (k < KNOCK_ROOMS && knock_rooms[k] != room) k++;
    if (k == KNOCK_ROOMS || knock_seat(knocker) < 0 || knock_seat(inside) < 0) return;
    if (knock[k].inside && knock[k].day == g_day) {   /* knocked already (this one or another) */
        if (knock[k].knocker == knocker) knock[k].at_x = figures(knocker - 1).target_x;   /* at the door anew */
        return;
    }
    knock[k].inside = (u8)inside;
    knock[k].knocker = (u8)knocker;
    knock[k].held_for = 0;
    knock[k].floor = (u16)floor;
    knock[k].door = (u16)door;
    knock[k].at_x = figures(knocker - 1).target_x;
    knock[k].day = g_day;
    knock[k].until = (u16)(g_clock_seconds + knock_opt * 60);
    /* (the knocker: its status line says it, a notice over the picture would cover it) */
    knock_notice(inside, knocker, "%s klopft an: In %d Min. musst du raus.", "%s knocks: you must leave in %d min.");
}

int mp_knock_held(unsigned fig, unsigned floor, unsigned door) {
    for (int k = 0; k < KNOCK_ROOMS; k++) {
        if (!knock[k].held_for || knock[k].floor != floor || knock[k].door != door) continue;
        if (knock[k].day != g_day || knock_past(knock[k].until) || !knock_waits(k)) {   /* (the knocker went elsewhere) */
            knock[k].held_for = 0;
            continue;
        }
        if (fig + 1 == knock[k].held_for) { knock[k].held_for = 0; return 0; }   /* the knocker: in it goes */
        return 1;
    }
    return 0;
}

/* at the top of a human's main loop: a knock at its door that is due; a room that
 * waits for it, as its knocker (gone elsewhere meanwhile: the room is free again) */
static void mp_knock_tick(void) {
    int me = seat_station[seat_cur];
    if (!knock_opt || !me || seat_out[seat_cur]) return;
    for (int k = 0; k < KNOCK_ROOMS; k++) {
        if (knock[k].held_for == me && knock[k].day == g_day && !knock_past(knock[k].until)) {
            u16 f = (u16)(me - 1);       /* waiting at the door, in the building, not in the lift: */
            if (!knock_waits(k))         /* to the door, as a click on it (room_building_click) */
                knock[k].held_for = 0;
            else if (!knock[k].walked && g_room == ROOM_BUILDING &&
                     (i16)figures(f).lift_phase <= 1 && (i16)figures(f).motion <= 1) {
                knock[k].walked = 1;
                figures(f).target_floor = knock[k].floor;
                figures(f).target_door = knock[k].door;
                figures(f).target_x = floors(knock[k].floor).door_x[knock[k].door] + 4;
                figures(f).lift_phase = 0;
                figures(f).final_x = 0;
                g_status_msg = 0;
            }
        }
        if (knock[k].inside != me) continue;
        u16 f = (u16)(me - 1);
        if (knock[k].day != g_day || figures(f).task != 0 || figures(f).target_floor != knock[k].floor ||
            figures(f).target_door != knock[k].door) {   /* gone out by itself (or the day is over) */
            knock[k].inside = 0;
            continue;
        }
        if (!knock_waits(k)) {           /* the knocker went elsewhere (or out of the game) */
            knock[k].inside = 0;
            knock_notice(me, knock[k].knocker, "%s wartet nicht mehr.", "%s is no longer waiting.");
            continue;
        }
        if (!knock_past(knock[k].until)) continue;
        knock[k].inside = 0;
        knock[k].held_for = knock[k].knocker;
        knock[k].walked = 0;
        knock[k].until = (u16)(g_clock_seconds + knock_opt * 60);
        knock_notice(me, knock[k].knocker, "Die Zeit ist um: %s ist dran.", "Time is up: it is %s's turn.");
        knock_notice(knock[k].knocker, me, "%s ist raus, du bist dran.", "%s is out, it is your turn.");
        while (g_room != ROOM_BUILDING) room_escape();
    }
}

/* one request, popped before it runs: a human seat's walk home plays frames
 * (game_frame), and the requests given meanwhile wait their turn */
static u16 req_arg;
static int req_pop(int s) {
    if (req_n[s] <= 0) return 0;
    int kind = req_q[s][0].kind;
    req_arg = req_q[s][0].arg;
    memmove(req_q[s], req_q[s] + 1, sizeof req_q[s][0] * (size_t)(--req_n[s]));
    return kind;
}

static void request_run(int kind, int human) {
    switch (kind) {
    case SEAT_REQ_CAUGHT:                /* caught in another's room: as ai_ready_at_target does it for seat 0 */
        if (human) {
            g_caught_figure = req_arg;
            g_status_msg = 0x12A;
            g_status_amount = g_catch_fine;
            players(g_player).money = players(g_player).money - g_catch_fine;
            ledger_add(LEDGER_CREDIT_REPAID, g_catch_fine);
            ui_draw_money();
            g_catch_fine = g_catch_fine << 1;
            if ((i32)g_catch_fine > 200000) g_catch_fine = 0x30D40;
            while (g_room != ROOM_BUILDING) room_escape();
        }
        caught_pending[seat_cur] = 0;
        break;
    case SEAT_REQ_DAY_END:
        g_status_msg = 0;
        tv_show_picture(0xDC);
        ui_status_text(G16(0x8764), G16(0x8766), 0xB4);     /* "Done" */
        ui_draw_viewers();
        if (human) player_walk_out();    /* out of the rooms, the figure home (0054:32FC) */
        break;
    case SEAT_REQ_DAY_DARK:
        if (g_room != ROOM_BUILDING) break;
        rtl_bitmap_fill_w(W2(g_screen_buf), 0, 0, 0x28, 0x78, 0);
        screen_transition(1, 2, 4, 0);
        room_draw_border();
        break;
    case SEAT_REQ_DAY_START:
        if (g_room != ROOM_BUILDING) break;
        g_bld_full_redraw = 1;
        bld_update_frame();
        screen_transition(2, 2, 4, 0);
        clock_show_start();
        ui_draw_viewers();
        ui_draw_money();
        G16(0x59C) = g_minute;           /* "Start" stays until the clock runs */
        if (human) {
            G16(0x7A32) = 1;             /* its boss talks the morning's talk (new_day does it for seat 0) */
            day_start_to_boss();         /* the morning at its boss (0054:33B3) */
        }
        break;
    }
}

/* seat 0's (with --humans): at the top of game_main's loop */
void mp_requests_seat0(void) {
    if (!seat_humans || seat_cur != 0) return;
    int kind;
    while ((kind = req_pop(0))) request_run(kind, !seat_out[0]);
    mp_knock_tick();
}

/* a spectator's requests, at the start of its frame (game_frame); a human
 * seat's wait for the top of its main loop (seat_main_loop) */
void seat_requests_run(void) {
    int s = seat_cur;
    if (s <= 0 || s >= SEATS_MAX || seat_station[s]) return;
    int kind;
    while ((kind = req_pop(s))) request_run(kind, 0);
}

static void seat_requests_human(void) {
    int s = seat_cur, kind;
    while ((kind = req_pop(s))) request_run(kind, !seat_out[s]);
    mp_knock_tick();
}

/* multiplayer: the world's day end waits until every human is home
 * (day_end_go_home): the walk home is each seat's own (its requests) */
void mp_wait_humans_home(void) {
    if (seats_n < 2 || !seat_humans) return;
    for (;;) {
        int home = 1;
        for (int i = 1; i < seats_n; i++) {
            if (!seat_station[i] || seat_out[i]) continue;   /* (out of the game: not coming) */
            u16 f = (u16)(seat_station[i] - 1);
            if ((i16)figures(f).floor != 1 || figures(f).motion != 0 || figures(f).x != 0x48) home = 0;
        }
        if (home) return;
        game_frame();
    }
}

/* where each other seat looks, as at the start of its last turn (mp_nobody_watches) */
static u16 view_room[SEATS_MAX], view_scroll[SEATS_MAX];

/* A seat but 0 is about to run.  The world restarts the timer (the clock's
 * pause and resume, every game hour: g_timer_ticks = 0) and sets seat 0's
 * frame timer with it; another seat's frame timer would wait until the ticks
 * caught up with it again.  So: ticks lower than at the seat's last turn --
 * the timer was restarted -- and its next frame is due at once. */
static u16 last_ticks[SEATS_MAX];

void seat_game_turn(int seat) {
    if (seat <= 0 || seat >= SEATS_MAX) return;
    view_room[seat] = g_room;
    view_scroll[seat] = g_bld_scroll;
    seat_paused[seat] = seat_humans && g_room == 0xFFFB;
    u16 now = g_timer_ticks;
    if (now < last_ticks[seat]) {
        u16 step = g_frame_divisor ? (u16)((i32)50 / (i16)g_frame_divisor) : 0;
        g_frame_timer = (u16)(now - step);
    }
    last_ticks[seat] = now;
}

/* Seat 0 about to run (several humans): another seat may have restarted the world's
 * timer meanwhile -- a human's change of the game speed, its pause (game_clock_stop and
 * start under seat_clock_world, in its own turn).  game_clock_start set that seat's frame
 * timer, not seat 0's: seat 0's still held the tick of its last frame before, far ahead
 * of the restarted timer, and the world's animation (the elevator, fig_think, fig_animate:
 * bld_update_frame) waited until the timer caught up -- a minute and more in which the
 * figures stood or went astray.  A frame timer ahead of the timer (and not game_clock_start's
 * own "-step", due at once) is such a leftover: the next frame is due at once. */
void seat_frame_timer_fresh(void) {
    if (!seat_humans || seat_cur != 0) return;
    u16 now = g_timer_ticks;
    u16 step = g_frame_divisor ? (u16)((i32)50 / (i16)g_frame_divisor) : 0;
    if (g_frame_timer > now && (u32)g_frame_timer + step <= 0xFFFF)
        g_frame_timer = (u16)(now - step);
}

/* SEAT_PTRCHECK=1: after another seat's turn, far pointers into the seat
 * window (its own heap) left in the shared part of the data segment -- the
 * world or seat 0 would read them while that heap is not in memory */
static u8 ptr_before[0xA730];
void seat_game_ptrcheck_before(void) {
    static int on = -1;
    if (on < 0) on = getenv("SEAT_PTRCHECK") != NULL;
    if (on) memcpy(ptr_before, dg_addr(0), sizeof ptr_before);
}
void seat_game_ptrcheck(void) {
    static int on = -1, reports;
    if (on < 0) on = getenv("SEAT_PTRCHECK") != NULL;
    if (!on || reports > 30) return;
    static u8 own[0xA730];
    static int init;
    if (!init) {
        init = 1;
        for (int i = 0; i < NRANGES; i++)
            for (u16 o = ranges[i].off; o < ranges[i].off + ranges[i].len && o < sizeof own; o++) own[o] = 1;
    }
    for (u16 o = 0; o + 4 <= 0xA730; o += 2) {
        if (own[o] || own[o + 2]) continue;
        /* no pointers, values that may look like one: the clock the pause keeps (DS:72AE,
         * a 32-bit count) and the tick block's counters (DS:9034.., the sound driver's) */
        if (o == 0x72AC || o == 0x72AE || o == 0x9034 || o == 0x9036) continue;
        if (o >= 0x3D6A && o < 0x3D6B + 4 * 0x78) continue;   /* the news slots: values only (struct news_item) */
        if (!memcmp(ptr_before + o + 2, dg_addr((u16)(o + 2)), 2)) continue;   /* its segment not set in this turn */
        u16 seg = G16((u16)(o + 2));
        if (seg >= SEAT_HEAP_SEG && seg < SEAT_HEAP_END && G16(o) < 0x10) {
            fprintf(stderr, "[ptrcheck] frame %llu seat %d: DS:%04X = %04X:%04X (shared, into the seat window)\n",
                    (unsigned long long)frame_no(), seat_cur, o, seg, G16(o));
            if (++reports > 30) return;
        }
    }
}

/* a seat may join now: a game is running and seat 0 is in the building;
 * human seats with the first morning of a new game */
int seat_game_join_ok(void) {
    if (seat_humans) return g_game_mode == 2 && g_room == ROOM_BUILDING;
    return g_game_mode == 3 && g_room == ROOM_BUILDING;
}

/* a spectator seat: the building view, again and again (multiplayer
 * stage 2, the first test of the seat switch) */
/* a new seat's view of its own, copied from seat 0's: a heap of its own,
 * its building pictures and cursor, the panel as seat 0's is now */
static void seat_view_init(void) {
    /* a heap of its own in the seat window, empty */
    G32(0x0087) = MK32(SEAT_HEAP_SEG, 0);    /* heap base */
    G32(0x008B) = MK32(SEAT_HEAP_SEG, 0);    /* break */
    G32(0x008F) = MK32(SEAT_HEAP_END, 0);    /* heap top */
    g_heap_first = 0; g_heap_last = 0; g_heap_rover = 0;
    /* the blocks of seat 0 it was copied from are not its own: forget them */
    for (u16 o = 0x00FC; o < 0x0124; o += 4) G32(o) = 0;      /* room graphics */
    g_building_bg = 0; g_status_win_bmp = 0; g_building_buf = 0;
    memset(dg_addr(0x63E5), 0, 0x1EA); memset(dg_addr(0x65CF), 0, 0x546);
    memset(film_txt_more, 0, sizeof film_txt_more);
    memset(dg_addr(0x6B15), 0, 0x294); memset(dg_addr(0x72BA), 0, 0x90);
    memset(dg_addr(0x05A0), 0, 0x10);                         /* TV picture cache */
    memset(dg_addr(0x8EE2), 0, 0x16);                         /* popup backgrounds */
    g_snd_sample = 0; g_snd_sample_mem = 0; g_snd_sample_res = 0;
    /* the building's pictures, as returning to the building allocates them
     * (room_return, src/main_loop.c) */
    g_status_win_bmp = bitmap_alloc(0x88, 0x38);
    g_building_buf = bitmap_alloc(0xD0, 0x96);
    /* the cursor's buffers, as the start of the game allocates them (game_main):
     * dragging replaces them with bigger ones, each seat its own */
    g_cursor_comp = bitmap_alloc(0x18, 0xB);
    g_cursor_bg = bitmap_alloc(0x18, 0xB);
    g_cursor_tmp = bitmap_alloc(0x18, 0xB);
    g_drag_image = 0;
    g_cursor_drawn_x = 0xFFFF;           /* nothing of its own on its screen yet */
    G32(0xFC) = res_cache_unpack(2, 1);
    g_building_bg = res_cache_unpack(((i16)clock_hour()) >= 4 ? 1 : 0, 0);
    g_daylight = ((i16)clock_hour()) >= 4 ? 2 : 1;
    g_bld_full_redraw = 1;
    g_redraw = 1;
    G16(0x7A8A) = 0;                     /* seat 0 may be in room_return: this seat is not */
    G16(0x7A2C) = 0;                     /* nor in its first morning */
    door_n[seat_cur] = 0;                /* the building is drawn anew: queued doors are in it */
    req_n[seat_cur] = 0;                 /* and the panel is seat 0's, as it is now */
    seen[seat_cur].hour = (i16)g_last_hour;             /* the panel is seat 0's, as it is now */
    seen[seat_cur].phase = (i8)g_broadcast_phase;
    g_mouse_pending_dx = 0;              /* seat 0's motion and click stay seat 0's */
    g_mouse_pending_dy = 0;
    g_mouse_click = 0;
    /* its own cursor, shown, in the middle of the building (where the
     * script's pointer of a new seat starts: runtime/host.c) */
    g_mouse_x = 160;
    g_mouse_y = 100;
    g_cursor_shown = 1;
}

void seat_spectator_main(void) {
    seat_view_init();
    for (;;) game_frame();
}

/* Multiplayer stage 3: the stations' humans.  Without --humans exactly the
 * game's test (== g_player, the one human); with it every human seat's
 * station. */
int mp_is_human(int player) {
    if (!seat_humans) return player == (i16)g_player;
    for (int i = 0; i < seats_n; i++)
        if (seat_station[i] == player) return 1;
    return 0;
}

int mp_third(int player) { return 6 - player - (i16)g_player; }

int mp_bailiff_target(void) {
    if (!seat_humans) return (i32)players(g_player).money < -100000 ? (i16)g_player : 0;
    int best = 0;
    for (int i = 0; i < seats_n; i++) {
        int p = seat_station[i];
        if (!p || seat_out[i] || (i32)players(p).money >= -100000) continue;
        if (!best || (i32)players(p).money < (i32)players(best).money) best = p;
    }
    return best;
}

int mp_leading_human(void) {
    if (!seat_humans) return (i16)g_player;
    int best = 0;
    for (int i = 0; i < seats_n; i++) {
        int p = seat_station[i];
        if (!p || seat_out[i]) continue;
        if (!best || (i16)players(p).image > (i16)players(best).image) best = p;
    }
    return best ? best : (i16)g_player;
}

int mp_nobody_watches(void) {
    int away = g_room != ROOM_BUILDING || ((i16)g_bld_scroll) > 30;
    if (!seat_humans) return away;
    if (seat_out[0]) away = 1;
    for (int i = 1; i < seats_n; i++)
        if (seat_station[i] && !seat_out[i] && view_room[i] == ROOM_BUILDING && (i16)view_scroll[i] <= 30) away = 0;
    return away;
}

/* Multiplayer: the boss's daily update (daily_boss_update, 1CB1:1D69) for
 * every human.  Each human's empty hours and missed ads, counted for its
 * boss; the station with the lowest image, if a computer player, gets money
 * (days 2..39); every station in the game whose image fell below 5 is out:
 * its image goes to the others in halves (all of it if only one is left),
 * as the original does for a computer player.  Each human then knows the
 * one to be thrown out (g_fired_player, the seat's): itself if its image
 * fell, else the first other station fired -- the sequence it sees at its
 * boss. */
static void boss_count_own(void) {
    G32(0x7A02) = 0;
    for (u16 h = 1; h <= 7; h++) {
        if (schedule_program(g_player, h) == 0) g_boss_cancelled_hours = g_boss_cancelled_hours + 1;
        if (schedule_ad(g_player, h) == 0) g_boss_missed_ads = g_boss_missed_ads + 1;
    }
}
static u16 fired_first;
static void boss_fired_own(void) {
    g_fired_player(0) = (i16)players(g_player).image < 5 && !players(g_player).out ? g_player : fired_first;
}
void mp_daily_boss_update(void) {
    boss_count_own();
    mp_for_other_humans(boss_count_own);
    u16 low = 0, img = 0x64;
    for (u16 p = 1; p <= 3; p++)
        if ((i16)players(p).image <= (i16)img) { img = players(p).image; low = p; }
    if (!(low == 0 || mp_is_human(low) || (i16)g_day <= 1 || (i16)g_day >= 40))
        ai_budget_distribute(low, 0x86A0, 1, 0xFFFF);
    int fired[4] = {0}, nf = 0;
    for (u16 p = 1; p <= 3; p++)
        if ((i16)players(p).image < 5 && !players(p).out) fired[p] = 1, nf++;
    fired_first = 0;
    for (u16 f = 1; f <= 3; f++) {
        if (!fired[f]) continue;
        if (!fired_first) fired_first = f;
        u16 t = players(f).image, rest[3];
        int nr = 0;
        if (!fired[g_player] && !players(g_player).out) rest[nr++] = g_player;   /* the original: g_player first */
        for (u16 p = 1; p <= 3; p++)
            if (p != g_player && !fired[p] && !players(p).out) rest[nr++] = p;
        if (nr >= 2) {
            players(rest[0]).image = players(rest[0]).image + (u16)((i16)t >> 1);
            players(rest[1]).image = players(rest[1]).image + (u16)(t - ((i16)t >> 1));
        } else if (nr == 1) {
            players(rest[0]).image = players(rest[0]).image + t;
        }
        players(f).image = 0;
    }
    boss_fired_own();
    mp_for_other_humans(boss_fired_own);
}

void mp_office_new_day(void) { g_office_papers = g_office_papers + 1; }

void mp_for_other_humans(void (*fn)(void)) {
    if (!seat_humans) return;
    int me = (i16)g_player;
    for (int i = 0; i < seats_n; i++) {
        int p = seat_station[i];
        if (!p || p == me || !mp_enter_player(p)) continue;
        fn();
        mp_leave();
    }
}

/* the main loop of a human seat but 0: game_main's (0054:0005, src/main_loop.c),
 * one game frame per pass -- a door clicked is entered, 15 minutes at the
 * info board end the visit, a click goes to the room's handler */
static void seat_main_loop(void) {
    int pressed = 0;                     /* one action per press (game_main keeps it in DI) */
    for (;;) {
        if (seat_out[seat_cur]) for (;;) game_frame();   /* out of the game: it only watches */
        seat_requests_human();           /* what the world asked of this seat (the day's end, the morning) */
        u16 door = game_frame();
        if (seat_paused_by_other()) { door = 0; g_click = 0; }   /* another human paused: wait */
        if (door) {
            u16 st = door_check_entry(g_player_figure, 0);
            g_status_msg = st;
            if (st) {
                figures(g_player_figure).task = 2;
                figures(g_player_figure).target_door = 0xFFFF;
            } else {
                room_enter(door);
            }
            continue;
        }
        if (g_room == ROOM_BUILDING && figures(g_player_figure).motion == 3 && figures(g_player_figure).target_door == 0xFFFE)
            room_enter(ROOM_INFO_BOARD_GO);
        if (g_room == ROOM_INFO_BOARD_GO) {
            u8 now[2];
            game_clock_get(now);
            if ((i16)(G16(0x7A8C) + 0x384) < (i16)(now[0] | now[1] << 8))
                while ((i16)g_room > ROOM_BUILDING) room_escape();
        }
        if (g_click != 0 && !pressed) {
            pressed = 1;
            if (g_click == 2) {
                if (g_room != ROOM_BUILDING) room_escape();
            } else if (g_click == 1) {
                ui_on_left_click();
            }
        }
        if (g_click == 0 && pressed) pressed = 0;
    }
}

/* A human seat but 0 (--humans): its station from the start of the game --
 * its player, figure, TV channel, credit, an empty bag, ledger and office --
 * then its first morning as game_main has it (game mode 2: to the boss),
 * then the main loop. */
void seat_human_main(void) {
    /* seat 0's cursor as it is now, in the middle of its own first morning:
     * shown or not, where it is drawn on the screen this seat copied, and
     * the background under it -- into this seat's own buffers */
    u16 shown = g_cursor_shown, drawn_x = g_cursor_drawn_x, drawn_y = g_cursor_drawn_y;
    u32 bg = g_cursor_bg, comp = g_cursor_comp, tmp = g_cursor_tmp;
    seat_view_init();
    g_cursor_shown = shown;
    g_cursor_drawn_x = drawn_x;
    g_cursor_drawn_y = drawn_y;
    bitmap_copy(LO16(bg), HI16(bg), LO16(g_cursor_bg), HI16(g_cursor_bg));
    bitmap_copy(LO16(comp), HI16(comp), LO16(g_cursor_comp), HI16(g_cursor_comp));
    bitmap_copy(LO16(tmp), HI16(tmp), LO16(g_cursor_tmp), HI16(g_cursor_tmp));
    int st = seat_station[seat_cur];
    g_player = (u16)st;
    g_player_figure = (u16)(st - 1);
    g_tv_channel = (u16)st;
    g_cur_player_rec = players_at((u16)st);
    if (!mpsave_apply_human(st)) {       /* a saved game: its own credit, bag, ledger, office ... (src/mpsave.c) */
        g_credit = 0x30D40;
        memset(dg_addr(0x2483), 0, 0x1E); g_bag_count = 0;          /* the bag */
        memset(dg_addr(0x71F1), 0, 0x54); memset(dg_addr(0x7492), 0, 0x54);   /* the ledger */
        memset(dg_addr(0x60BF), 0, 0x82); G8(0x7247) = 0;           /* the office messages */
    }
    g_redraw = 1;
    G16(0x7A2C) = !mp_loaded;            /* the first morning's building (not a saved game's morning) */
    G16(0x7A32) = 1;                     /* the boss's morning talk */
    ui_draw_money();
    ui_draw_viewers();
    if (mp_loaded) {                     /* a saved game: its Betty's meter, its stations' icons (as seat 0) */
        ui_draw_betty_meter();
        ui_draw_player_icons();
    }
    /* its own street sound while it walks to its boss: seat 0 started its
     * own on coming back to the building (room_return), before this seat
     * joined; the sound of a seat is that seat's (src/rtl_snd.c) */
    snd_room_music_start(ROOM_BUILDING, 0, 0);
    if (!seat_out[seat_cur]) day_start_to_boss();   /* (a saved game's fired human: watching, no boss) */
    G16(0x7A2C) = 0;
    /* seat 0 joined it inside room_return, between its cursor_hide and
     * cursor_show: the main loop runs with the cursor shown once */
    g_cursor_shown = 0;
    cursor_show(0);
    seat_main_loop();
}
