/* Original-bug fixes: names for --fixes, and the switch (fixes.h). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "hr.h"
#include "fixes.h"
#include "seat.h"

static const struct { const char *name; unsigned bit; const char *what; } fix_table[] = {
    {"intruder",  FIX_INTRUDER,  "bailiff/terrorist hang on a library that holds only series; love films never seized (1DA8:2D3B)"},
    {"random0",   FIX_RANDOM0,   "random_n(0) divides by zero -> \"Divide error\" (0D5E:0229)"},
    {"ai-studio", FIX_AI_STUDIO, "computer player rents studio 13 instead of the chosen one (348B:098D)"},
    {"ai-fill-hour", FIX_AI_FILL_HOUR, "computer player estimates a programme's audience for the hour = its category (2DD9:0DD9)"},
    {"ai-series-rule", FIX_AI_SERIES_RULE, "computer player checks an ad's genre rule on film id-24 instead of the series (2DD9:162A)"},
    {"series-avg", FIX_SERIES_AVG, "series average topicality sums episodes 2..n+1 (2DD9:3085)"},
    {"estate-text", FIX_ESTATE_TEXT, "estate agent shows 'Warning: Psychiatrist'/'Bio-control' (05FF:3AB7)"},
    {"tx-price", FIX_TX_PRICE, "transmitter purchase checked against new viewers x 61.6, not the price it costs (05FF:2C02)"},
    {"library-owner", FIX_LIBRARY_OWNER, "a film taken off the library shelf and put back stays on the market; the dealer sells it (0AA4:24C3)"},
    {"news-cache", FIX_NEWS_CACHE, "full news inbox: broadcast slots 2 and 3 show slot 1's headline (26A7:292F)"},
    {"ai-odd-range", FIX_AI_ODD_RANGE, "computer player's odd-length check scans the half-day range 1..0 (stack garbage), i.e. nothing (2DD9:0DD9)"},
    {"boss-music", FIX_BOSS_MUSIC, "boss room calls snd_room_music_stop without arguments before the end sequence (0AA4:000E)"},
    {"film-200", FIX_FILM200, "data: film 200 \"Prince of Darkness\" has the title \"xxx\" and a cut-off German text, and is never offered"},
    {"catch-bounty", FIX_CATCH_BOUNTY, "catching a rival in your own room pays the bounty only on station 1 (1DA8:3643)"},
    {"ai-visit", FIX_AI_VISIT, "computer players sneak into station 1's rooms instead of the human's (2DD9:3C0D)"},
    {"ai-trendy", FIX_AI_TRENDY, "a computer player airing \"Trendy\" writes the trends over other variables (a free Betty gift) instead of learning them (24B8:0C4A)"},
    {"ai-rival", FIX_AI_RIVAL, "a computer player's daily budget plan takes itself as its rival: never best in image, its reach counted twice (2DD9:26EA)"},
    {"ai-arrived", FIX_AI_ARRIVED, "a computer player whose next goal is the door it stands at may act at once -- only the first one; the second waits 300 ticks again (2DD9:4126)"},
    {"title-cache", FIX_TITLE_CACHE, "47+ films in archive and case: the library's film text cache runs past its 49 slots into the series cache -- films without title, a series with a film's title and garbage episodes (048C:025B)"},
    {"case-owner", FIX_CASE_OWNER, "a tape taken off the library shelf into the case is ownerless and on the market: the dealer's refill can offer it while it is carried -- bought back it is paid twice, bought by a computer player it is in two libraries (05FF:090A)"},
    {"ai-fill-genre", FIX_AI_FILL_GENRE, "a computer player filling an hour tests each candidate against the genre bit of the best so far (love films aired where its contracts forbid them) and scores a series best on the family-series row (2DD9:12C1, 2DD9:1316)"},
    {"ai-lib-quality", FIX_AI_LIB_QUALITY, "a computer player's library average counts 12 phantom entries (genres 0..11, slots 0..count): its 'worst quality' budget flag follows library size (348B:0895)"},
    {"studio-takeover", FIX_STUDIO_TAKEOVER, "a studio handed to its queued renter when its show airs: the player number is taken as the studio -- the renter gets studio 1, 2 or 3 instead (24B8:170C)"},
    {"studio-reserved", FIX_STUDIO_RESERVED, "a studio's pending renter / deferred give-up is never cleared: the next renter loses it after every production, or it stays blocked and is charged again and again (229E:1080, 229E:1264)"},
    {"credit-repay", FIX_CREDIT_REPAY, "repaying the boss's credit takes the smaller of the amount and your cash, but not the credit: in debt that is a negative repayment -- the boss lends you the debt, without a limit (cash -230 000 -> -30 000, credit +200 000) -- and paying back more than you owe makes the credit negative and the boss pays you 5 % a day on it; always on in network games (0AA4:000E)"},
    {"case-corner", FIX_CASE_CORNER, "the film agency's case hands out slots past the carried tapes: in its lower right corner lies your own programme score of the hour as a film id -- an untitled film to sell to the dealer again and again, each time the case count drops below zero and a tape put in later is lost; always on in network games (05FF:0359)"},
    {"ai-rating", FIX_AI_RATING, "a computer player's average rating of yesterday multiplies the viewers by 100 in 32 bits: above 21.5 million viewers an hour (a satellite's reach) it overflows -- a rating of 0 or below, so it takes almost no ad contracts and its budget plan ranks it last (2DD9:2B8D)"},
    {"ai-bids", FIX_AI_BIDS, "a computer player's auction bids go into a list of 11 places, one new entry per bid -- the same lot again and again -- emptied only when the human outbids it: a busy bidder writes past it over the contract counts and more (a crash: the ad agency then writes into the sound driver) (2DD9:1DDA)"},
    {"ai-warning", FIX_AI_WARNING, "a computer player placing a programme it may not (a live film whose hour is over) gets the human's warning box: tried at every office visit, the boxes pile up on the human's screen until one saves the screen through a null pointer over the interrupt vectors (a crash) (229E:0225, 2DD9:4719)"},
    {"live-aid", FIX_LIVE_AID, "data: film 164 \"Live Aid\", a live broadcast in the auction pool, is 0 hours long: no computer player bids on it, placed it clears nothing (the programme it covers airs on as Live Aid), aired it stays in the archive and is taken away at night as \"missed\"; its date can be 1 a.m., which cannot be planned"},
    {"trendy-studios", FIX_TRENDY_STUDIOS, "the live show \"Trendy\" (film 138), aired by anyone, counts as every running studio's show: each studio in production or waiting for its live show is let go -- a deferred give-up or a queued renter takes it mid-production, the production is lost, and the studio later \"finishes\" screenplay 0 into player -1's library (a computer player's news budget -> 0) (24B8:0C4A)"},
    {"video-connection", FIX_VIDEO_CONN, "data: screenplay 20 \"Video-Connection\" (a show) asks nothing of its two roles: every cast fits 0 %, only the set counts -- produced, the show has quality 3 of 20 at best and next to no viewers"},
};
#define NFIX (int)(sizeof fix_table / sizeof fix_table[0])

static unsigned fix_mask;
static int fix_parsed;

static void fix_parse(void) {
    fix_parsed = 1;
    if (!fix_spec) return;                       /* classic */
    if (!strcmp(fix_spec, "list")) {
        for (int i = 0; i < NFIX; i++) printf("%-15s %s\n", fix_table[i].name, fix_table[i].what);
        exit(0);
    }
    if (!strcmp(fix_spec, "all")) { for (int i = 0; i < NFIX; i++) fix_mask |= fix_table[i].bit; return; }
    char *buf = strdup(fix_spec);                /* the whole list (all names: 280 characters, more than the old 255) */
    if (!buf) { fprintf(stderr, "out of memory\n"); exit(1); }
    for (char *t = strtok(buf, ","); t; t = strtok(NULL, ",")) {
        int i = 0;
        while (i < NFIX && strcmp(t, fix_table[i].name)) i++;
        if (i == NFIX) { fprintf(stderr, "unknown fix '%s' (--fixes=list)\n", t); exit(2); }
        fix_mask |= fix_table[i].bit;
    }
    free(buf);
}

int fix_on(unsigned fix) {
    if (difftest_on) return 0;                   /* compare with the original */
    if (credit_line_opt == CREDIT_LINE_ORIGINAL && !(fix &= ~FIX_CREDIT_REPAY)) return 0;   /* the original's credit, its repayment too */
    if (seat_humans && (fix & FIX_MP_ALWAYS)) return 1;   /* a network game */
    if (!fix_parsed) fix_parse();
    return (fix_mask & fix) != 0;
}

/* Fix "case-corner" (room_filmagency_click 05FF:0359, tooltip_update 14E1:000E).
 * The case's hotspot (216,24)-(279,103) is its whole frame: 7 columns of 9 and 3
 * rows of 30 pixels from (224,28), slot = column + 5 x row = 0..16, while the tapes
 * are drawn 5 by 2.  The original only asks whether the word at g_bag + 2 x slot is
 * non-zero.  Behind the 10 used slots: 5 never written (DS:2497..24A0), then
 * g_program_score (DS:24A1, index 0 unused): slot 16 -- x 278..279, y 88..103 -- is
 * player 1's programme score this hour, taken for a film id.  The library asks
 * slot < g_bag_count; so does this. */
int case_slot_ok(u16 col, u16 slot) {
    return (i16)col < 5 && (i16)slot < (i16)g_bag_count;
}

/* Data fix "film-200".  Film 200 is John Carpenter's "Prince of Darkness" (1987): the
 * English title is the placeholder "xxx"; the German text resource is cut off after
 * "Die Fürsten der" (its length byte, 26, is that of "Die Fürsten der Dunkelheit") and
 * a table of numbers follows, the description is lost; the film is never put on the market
 * (flags 5: bit 5 clear).  Only the original's damaged data is replaced, so a mod that
 * changed film 200 keeps its own.  Code page 437 (0x81 = u umlaut). */
static const u8 de_damaged[] = "\x1a" "Die F\x81rsten der" "f";
static const u8 de_fixed[] = "\x1a" "Die F\x81rsten der Dunkelheit"
                              "\x4e" "Ein Horrorfilm von John Carpenter mit Teufel, Tod und Schlachthaus-Atmosph\x84re.";
/* the texts of film 200 as read from the resource (title, description as length-prefixed
 * strings): if they are the damaged ones, the restored ones go to out; 0 = keep buf */
int fix_film200_text(u8 *buf, u16 len, u8 *out, u16 cap) {
    if (!fix_on(FIX_FILM200)) return 0;
    if (len >= sizeof de_damaged - 1 && !memcmp(buf, de_damaged, sizeof de_damaged - 1)) {
        if (cap < sizeof de_fixed - 1) return 0;
        memcpy(out, de_fixed, sizeof de_fixed - 1);
        return sizeof de_fixed - 1;
    }
    if (len >= 4 && buf[0] == 3 && !memcmp(buf + 1, "xxx", 3)) {      /* English: the title only */
        static const char title[] = "Prince of Darkness";
        u16 rest = (u16)(len - 4);
        if (cap < 1 + sizeof title - 1 + rest) return 0;
        out[0] = sizeof title - 1;
        memcpy(out + 1, title, sizeof title - 1);
        memcpy(out + sizeof title, buf + 4, rest);
        return (int)(sizeof title + rest);
    }
    return 0;
}
/* after the start savegame: the film goes on the market, if it is still the original's */
int fix_film200_record(void) {
    if (!fix_on(FIX_FILM200)) return 0;
    if (films(200).flags != 5 || films(200).genre != 2 || MK32(films(200).price_hi, films(200).price_lo) != 90000) return 0;
    films(200).flags |= 0x20;
    return 1;
}

/* Data fix "live-aid".  Film 164, "Live Aid" (live, quality and topicality 50, $700 000,
 * in the auction pool), has length 0 -- the only film without one.  The other ten live
 * broadcasts cost about $7 000 per hour and quality point (59: 1 hour, quality 50,
 * $100 000 ... 187: 4 hours, quality 80, $3 000 000; the best fit 6 965); $700 000 at
 * quality 50 is 2 hours (2.01), which also puts the computer players' value of it
 * (hours x topicality x 20 / thousands of dollars: 2.9) among the others' (2.0 .. 3.7).
 * What the length 0 does (code unchanged, docs/CHANGES.md): the computer players' value is 0, so they
 * never bid (2DD9:20BA, 2DD9:2386); its date is drawn as hour 1..8 - 0 (1DA8:0CB3), and
 * hour 8 (1 a.m.) can never be planned; placed (229E:0225) it clears no hour -- a programme
 * it is put on keeps its continuation hours, which then air as Live Aid; aired it is never
 * the last block (24B8:0C4A counts from 1), so it stays in the archive and is taken away
 * at night with "you missed" (1DA8:135D).  Only the original's record is changed: a mod that
 * changed film 164 keeps its own. */
int fix_live_aid_record(void) {
    if (!fix_on(FIX_LIVE_AID) || (i16)g_nfilms < 164) return 0;
    if (films(164).blocks != 0 || films(164).genre != GENRE_LIVE || films(164).quality != 50
        || MK32(films(164).price_hi, films(164).price_lo) != 700000) return 0;
    films(164).blocks = 2;
    return 1;
}

/* Data fix "video-connection".  Screenplay 20, "Video-Connection" (film 134, a show: "a
 * low budget music video show -- a few star guests and a sparkling host make it a
 * success"), asks nothing of its two roles, the host (category 5, slot 1, weight 10) and
 * the star guests (category 7, slots 2 and 3, weight 4 each) -- the only screenplay with
 * such roles.  script_role_match_pct 1DA8:2560 then rates every cast 0 %, and the shooting
 * (229E:0B98) counts only the set (weight 4 of 22): quality 20 x 4 / 22 x 125 % = 3 at
 * best, which draws no viewers.  The fix gives both roles the requirements of "Talk for
 * Two" (screenplay 16, also a host and a guest): humor 20, looks 18, craft 16 (the second
 * byte counts nowhere).  Only the original's record is changed: a mod that gave the roles
 * requirements keeps its own.  The requirements are no part of a savegame (only the
 * episode blocks are), so the fix holds for a loaded game as well. */
static const u8 video_conn_needs[4] = { 20, 0, 18, 16 };
int fix_video_conn_record(void) {
    if (!fix_on(FIX_VIDEO_CONN)) return 0;
    u32 s = screenplays_at(20);
    if (AT(screenplay, s).target != 134 || FP8(s, 0x1A4) != 5 || FP8(s, 0x1A5) != 7) return 0;
    for (u16 i = 0; i < 8; i++)
        if (FP8(s, 0x168 + i)) return 0;
    for (u16 i = 0; i < 8; i++) FP8(s, 0x168 + i) = video_conn_needs[i & 3];
    return 1;
}

/* the new-game setup (0AA4:0AC5) gives the human this much cash; the credit stays $200 000 */
u32 start_money_human(void) {
    if (difftest_on || start_money_opt < 0) return 0x30D40;      /* the original's $200 000 */
    return (u32)start_money_opt;
}

/* the boss's haggling (room_boss_click 0AA4:000E, image 10..49 with a credit): the
 * original allows another $100 000 from day 2 while the credit is <= $200 000, a line of
 * $300 000.  Option --credit-line=N: the line is $200 000 + $100 000 per day since day 1
 * (day 2 300 000, day 3 400 000, ...) up to N; N = 300 000 is the original.  The day check
 * (refused on day 1) and the Sammy winner's $200 000 stay the original's.  --credit-line=original:
 * the original's line, and fix credit-repay off (fix_on), also in a network game. */
u32 credit_haggle_max(void) {
    if (difftest_on || credit_line_opt <= CREDIT_LINE_ORIGINAL) return 200000;
    long line = 100000L * ((long)(i16)g_day + 1);
    if (line > credit_line_opt) line = credit_line_opt;
    return line < 100000 ? 0 : (u32)(line - 100000);
}

/* a terrorist's bomb in a room with an owner (npc_event_strike 1DA8:2D3B): the owner pays
 * the original's $50 000 for the repairs -- option --terror-damage=N (settings [game]
 * terror_damage); 0: no costs and no message (the room's other losses stay) */
u32 terror_damage(void) {
    if (difftest_on || terror_damage_opt < 0) return (u32)TERROR_DAMAGE_ORIGINAL;
    return (u32)terror_damage_opt;
}

/* the Sammy's prize money for category 1..4 (the best news, cultural engagement, own shows,
 * own series): the original pays none -- option --sammy-prize (settings [game] sammy_news,
 * sammy_culture, sammy_shows, sammy_series) */
u32 sammy_prize(u16 cat) {
    if (difftest_on || cat < 1 || cat > 4 || sammy_prize_opt[cat - 1] < 0) return 0;
    return (u32)sammy_prize_opt[cat - 1];
}

/* the Sammy's winner (sammy_daily_update 1DA8:4481, at the day change) gets the prize: the
 * cash, and a human the ledger's sundry income -- booked like the credit's interest, before
 * the ledger's new day, so it shows in the finance report of the day that ends */
void sammy_pay(u16 winner, u16 cat) {
    u32 prize = sammy_prize(cat);
    if (!prize || winner < 1 || winner > 3) return;
    players(winner).money = players(winner).money + prize;
    int as_human = mp_enter_player(winner);     /* multiplayer: the ledger of the human who won */
    if (winner == g_player) ledger_add(LEDGER_SUNDRY_INCOME, prize);
    if (as_human) mp_leave();
}

/* dollars as the original's texts write them, in both languages: "$50.000", "$1.250.000" */
static void money_dots(char out[20], u32 v) {
    char d[12];
    int n = snprintf(d, sizeof d, "%lu", (unsigned long)v), o = 0;
    out[o++] = '$';
    for (int i = 0; i < n; i++) {
        if (i && (n - i) % 3 == 0) out[o++] = '.';
        out[o++] = d[i];
    }
    out[o] = 0;
}

/* message 0x135 as msg_show_next (2B74:1875) shows it, in text_dup's 500-byte buffer: the
 * original's "$50.000" becomes the option's amount (a pack's own text without it stays) */
void terror_text(u32 buf) {
    u32 dmg = terror_damage();
    char *s = (char *)HPTR(buf), *at, amt[20];
    if (dmg == (u32)TERROR_DAMAGE_ORIGINAL || !(at = strstr(s, "$50.000"))) return;
    money_dots(amt, dmg);
    size_t al = strlen(amt);
    if (strlen(s) - 7 + al >= 0x1F4) return;
    memmove(at + al, at + 7, strlen(at + 7) + 1);
    memcpy(at, amt, al);
}

/* message 0x159 (the human won the Sammy) in str_format's 1000-byte buffer: the prize money
 * the message was posted with, if any */
void sammy_prize_text(u32 buf, u32 prize) {
    char *s = (char *)HPTR(buf), amt[20];
    if (!prize) return;
    money_dots(amt, prize);
    size_t len = strlen(s);
    snprintf(s + len, 0x3E8 - len, g_language == 0 ? "~Dazu erhalten Sie ein Preisgeld von %s!" : "~It comes with prize money of %s!", amt);
}

/* The system arrow (fixes.h; runtime/host.c shows and hides the system
 * pointer by sys_arrow_state).  The system pointer stands for the game's
 * arrow only while the game's cursor is where the pointer is, or heading
 * there (the host feeds the difference as motion, the game has eased its
 * cursor there by the end of the frame).  The game's arrow comes back where
 * the two part: the arrow keys moved the cursor (until the pointer moves
 * again), or the game holds its cursor inside narrower limits than the whole
 * screen and the pointer is beyond them.  Other cursors (busy, carrying) and
 * dragged items (a transmitter on the map) are always the game's.  The game still
 * draws the arrow, with the same calls (time counts their entries and
 * pixels: a recording replays the same with or without it); what the arrow
 * covered is put back without taking time.  With several seats only the
 * screen this machine shows has the system pointer over it: the others'
 * arrows are drawn as they are (and the host asks sys_arrow_state in the
 * shown seat's context). */
static int sys_arrow_on(void) { return mouse_system && !difftest_on; }
/* the seat in memory is the one this machine shows: in its own turn, or its screen being shown
 * (seat_view: a present comes in seat 0's turn -- the running seat was once asked, and the
 * guest's carried item was never drawn by the window) */
static int sys_arrow_here(void) {
    extern int seats_n, seat_cur;
    extern int seat_in_memory(void);
    return !seats_n || seat_cur < 0 || seat_in_memory() == host_shown_seat();
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static int sys_arrow_stands_in(void) {
    if (!sys_arrow_on() || sys_ptr_x < 0) return 0;
    int x0 = (i16)g_mouse_min_x, x1 = (i16)g_mouse_max_x, y0 = (i16)g_mouse_min_y, y1 = (i16)g_mouse_max_y;
    int tx = clampi(sys_ptr_x, x0, x1), ty = clampi(sys_ptr_y, y0, y1);
    /* beyond limits the whole-screen ones (mouse_bounds_full: 0..303, 0..189) do not set */
    if (tx != clampi(sys_ptr_x, 0, 303) || ty != clampi(sys_ptr_y, 0, 189)) return 0;
    int dx, dy;
    mouse_pending(&dx, &dy);
    return clampi((i16)g_mouse_x + (i16)g_mouse_pending_dx + dx, x0, x1) == tx &&
           clampi((i16)g_mouse_y + (i16)g_mouse_pending_dy + dy, y0, y1) == ty;
}

static int arrow_left_out;           /* the arrow last drawn was taken out again */
static u8 cover[11 * 320];           /* what it covered: up to 11 rows of a bitmap */
static u16 cover_seg, cover_off, cover_len;
static HrCell *cover_cells;          /* --filter hr: their cells */

void arrow_cover(u32 bitmap, int y) {
    cover_len = 0;
    if (!sys_arrow_here()) return;
    arrow_left_out = g_cursor_shape == 1 && sys_arrow_stands_in();
    if (!arrow_left_out) return;
    u16 w = (u16)(FP16(bitmap, 0) << 3), h = FP16(bitmap, 2);
    u32 pix = FP32(bitmap, 4);
    int y0 = clampi(y, 0, h), y1 = clampi(y + 11, 0, h);
    if (!w || w > 320 || y1 <= y0) return;
    cover_seg = FPSEG(pix);
    cover_off = (u16)(FPOFF(pix) + y0 * w);
    cover_len = (u16)((y1 - y0) * w);
    for (u16 i = 0; i < cover_len; i++) cover[i] = rb(cover_seg, (u16)(cover_off + i));
    hr_save(&cover_cells, sizeof cover, cover_seg, cover_off, cover_len);
}

void arrow_uncover(void) {
    for (u16 i = 0; i < cover_len; i++) wb(cover_seg, (u16)(cover_off + i), cover[i]);
    hr_restore(cover_cells, cover_seg, cover_off, cover_len);
    cover_len = 0;
}

/* the pointer came or went while the cursor stood still: take the arrow off
 * the screen (the background under it, g_cursor_bg) or put it there */
void arrow_refresh(void) {
    if (!sys_arrow_on() || !sys_arrow_here() || g_cursor_shown != 1 || g_cursor_shape != 1 || (g_drag_image_lo | g_drag_image_hi)) return;
    if (g_mouse_pending_dx || g_mouse_pending_dy || g_cursor_drawn_x != g_mouse_x || g_cursor_drawn_y != g_mouse_y) return;
    int out = sys_arrow_stands_in();
    if (out == arrow_left_out) return;
    arrow_left_out = out;
    int x = (i16)g_cursor_drawn_x, y = (i16)g_cursor_drawn_y;
    u32 b = out ? g_cursor_bg : g_cursor_sprites(1);
    u16 w = (u16)(FP16(b, 0) << 3), h = FP16(b, 2);
    u32 pix = FP32(b, 4);
    int left = out ? x & ~7 : x;
    for (int r = 0; r < h && y + r < 200; r++)
        for (int c = 0; c < w && left + c < 320; c++) {
            u16 so = (u16)(FPOFF(pix) + r * w + c), vo = (u16)((y + r) * 320 + left + c);
            u8 v = rb(FPSEG(pix), so);
            if (out) hr_copy1(FPSEG(pix), so, g_vga_seg, vo);
            else hr_tput(FPSEG(pix), so, g_vga_seg, vo, v);
            if (out || v) wb(g_vga_seg, vo, v);
        }
}

/* The carried item over the picture at the pointer (a network game).  An input goes to the
 * server and comes back in a bundle before the game moves its cursor -- some frames even in
 * a LAN, hidden by the system arrow, but not while something is carried (an ad, a film into
 * the planner): the game draws the item, and it trailed the mouse.  So, in a network game
 * with the system mouse, while the seat this machine shows carries something, the window
 * draws the item where the pointer is now (host.c draw_drag, clamped as the game clamps it;
 * host_drag_ok: it does) -- always, not only where the game's cursor has caught up: that
 * flipped between the two pictures.  The game still draws its own with the same calls, and
 * what it covered is put back untimed, as for the arrow.  The pointer still goes to the
 * game (its hints in the status line), and a click goes with the pointer before it: the
 * item lands where the button was released.  Alone the game's own item, as ever. */
static int carry_wanted(void) {
    extern int net_active;
    return sys_arrow_on() && net_active && sys_arrow_here() && g_cursor_shape == 2 && (g_drag_image_lo | g_drag_image_hi);
}
extern int host_drag_ok;             /* the window draws the carried item (host.c draw_drag) */
static int carry_local(void) { return carry_wanted() && host_drag_ok; }

static int drag_left_out;            /* the item last drawn was taken out again */
static u8 dcover[328 * 121];         /* what it covered: the cursor buffer (the item's width + 8, its height) */
static u16 dcover_seg, dcover_off, dcover_len;
static HrCell *dcover_cells;         /* --filter hr: their cells */

void drag_cover(u32 bitmap) {
    dcover_len = 0;
    if (!sys_arrow_here()) return;
    drag_left_out = carry_local();
    if (!drag_left_out) return;
    u16 w = (u16)(FP16(bitmap, 0) << 3), h = FP16(bitmap, 2);
    u32 pix = FP32(bitmap, 4);
    if (!w || (u32)w * h > sizeof dcover) { drag_left_out = 0; return; }
    dcover_seg = FPSEG(pix);
    dcover_off = FPOFF(pix);
    dcover_len = (u16)(w * h);
    for (u16 i = 0; i < dcover_len; i++) dcover[i] = rb(dcover_seg, (u16)(dcover_off + i));
    hr_save(&dcover_cells, sizeof dcover, dcover_seg, dcover_off, dcover_len);
}

void drag_uncover(void) {
    for (u16 i = 0; i < dcover_len; i++) wb(dcover_seg, (u16)(dcover_off + i), dcover[i]);
    hr_restore(dcover_cells, dcover_seg, dcover_off, dcover_len);
    dcover_len = 0;
}

/* the system cursor came or went while the item rests: on the screen the background under
 * it (g_cursor_bg, the item's width + 8 from x & ~7) or the item itself */
void drag_refresh(void) {
    if (!sys_arrow_on() || !sys_arrow_here() || g_cursor_shown != 1 || g_cursor_shape != 2 || !(g_drag_image_lo | g_drag_image_hi)) return;
    if (g_mouse_pending_dx || g_mouse_pending_dy) return;
    int dx = clampi((i16)g_mouse_x - (i16)g_drag_hot_x, 0, 319 - (i16)g_drag_w);
    int dy = clampi((i16)g_mouse_y - (i16)g_drag_hot_y, 0, 120 - (i16)g_drag_h);
    if ((i16)g_cursor_drawn_x != dx || (i16)g_cursor_drawn_y != dy) return;
    int out = carry_local();
    if (out == drag_left_out) return;
    drag_left_out = out;
    u32 b = out ? g_cursor_bg : g_drag_image;
    u16 w = (u16)(FP16(b, 0) << 3), h = FP16(b, 2);
    u32 pix = FP32(b, 4);
    int left = out ? dx & ~7 : dx, cols = out ? w : (i16)g_drag_w, rows = out ? h : (i16)g_drag_h;
    for (int r = 0; r < rows && dy + r < 200; r++)
        for (int c = 0; c < cols && left + c < 320; c++) {
            u16 so = (u16)(FPOFF(pix) + r * w + c), vo = (u16)((dy + r) * 320 + left + c);
            u8 v = rb(FPSEG(pix), so);
            if (out) hr_copy1(FPSEG(pix), so, g_vga_seg, vo);
            else hr_tput(FPSEG(pix), so, g_vga_seg, vo, v);
            if (out || v) wb(g_vga_seg, vo, v);
        }
}

/* for the host: the carried item's picture (8-bit, 0 transparent) and its hot spot */
int sys_drag_image(u8 *pix, int cap, int *w, int *h, int *hx, int *hy, int lim[4]) {
    if (!(g_drag_image_lo | g_drag_image_hi)) return 0;
    u32 b = g_drag_image;
    int stride = FP16(b, 0) << 3;
    *w = (i16)g_drag_w; *h = (i16)g_drag_h; *hx = (i16)g_drag_hot_x; *hy = (i16)g_drag_hot_y;
    lim[0] = (i16)g_mouse_min_x; lim[1] = (i16)g_mouse_min_y;   /* (the cursor's: a transmitter stays on the map, rooms_click.c) */
    lim[2] = (i16)g_mouse_max_x; lim[3] = (i16)g_mouse_max_y;
    if (*w <= 0 || *h <= 0 || *w > stride || *w * *h > cap) return 0;
    u32 p = FP32(b, 4);
    for (int r = 0; r < *h; r++)
        for (int c = 0; c < *w; c++) pix[r * *w + c] = rb(FPSEG(p), (u16)(FPOFF(p) + r * stride + c));
    return 1;
}

/* for the host: SYS_ARROW_ON the system pointer shows the game's arrow;
 * SYS_ARROW_HIDDEN the game has hidden its arrow (a moment, while it draws
 * there, or a longer time); SYS_ARROW_OFF the game draws its own cursor;
 * SYS_ARROW_DRAG the window draws the carried item -- shown or hidden the game's cursor: a
 * seat other than 0 is seen where it gave up its turn, its cursor hidden there while it
 * carries something (the next frame draws it again) */
int sys_arrow_state(void) {
    if (carry_wanted()) return SYS_ARROW_DRAG;   /* (a network game, host.c) */
    if (!sys_arrow_stands_in() || g_cursor_shape != 1 || (g_drag_image_lo | g_drag_image_hi)) return SYS_ARROW_OFF;
    return g_cursor_shown == 1 ? SYS_ARROW_ON : SYS_ARROW_HIDDEN;
}

/* The fourth speed (fixes.h).  The original's Control-Center: panel
 * 88..199, boxes and the speed row 96..191, three 28-pixel buttons
 * (atlas tiles 0x18 pressed / 0x19 raised of the planner's res 0xAF) 34
 * pixels apart with a 26-pixel car (tiles 0x1A..0x1C) inside, hotspots
 * 0x3F..0x41 32 pixels wide.  Four buttons of that size do not fit the 96
 * pixels, so the panel and its boxes grow by 16 pixels on each side (the
 * frames are drawn in 8-pixel tiles) and the buttons move closer together
 * (5 or 6 pixels instead of 6); the texts and switches stay where they were. */
int speed4_on(void) { return fourth_speed_opt && !difftest_on; }

/* The series card's topicality (fixes.h).  An episode e (1..10) of the series
 * record holds its quality at e*3, its topicality at e*3+1 and in bit 0 of
 * the word at e*3+2 whether it exists; series_price counts the episodes up
 * to the first that does not.  The layout: 4 characters a row (band style,
 * text slot, alignment, space), the original's 12 rows ending in
 * "2-- 151" (the episode's second line, slot 5 on a light band). */
int series_top_on(void) { return series_top_opt && !difftest_on; }

u16 series_top_bar(u32 series) {
    u16 top = 0, qual = 0;
    for (u16 e = 1; e <= 10 && (FP16(series, e * 3 + 2) & 1); e++) {
        top += FP8(series, e * 3 + 1);
        qual += FP8(series, e * 3);
    }
    return qual ? (u16)((u32)top * 40 / qual) : 40;   /* (the film card: quality 0 -> a full bar) */
}

#define SERIES_TOP_ROWS_MAX 14
static char series_top_rows[SERIES_TOP_ROWS_MAX * 4 + 1];
u32 series_top_layout(u32 layout, u16 lines, u16 *rows) {
    const u8 *s = HPTR(layout);
    u16 at, n;
    if (lines < 2) lines = 2;                  /* "n episodes/n blocks" and the episode */
    if (lines > SERIES_TOP_ROWS_MAX - 10) lines = SERIES_TOP_ROWS_MAX - 10;
    at = 8 + lines;                            /* the band's row: under the episode's lines (from row 8) */
    n = at + 2 > 12 ? at + 2 : 12;
    for (int i = 0; i < 12 * 4; i++) series_top_rows[i] = (char)s[i];
    for (int i = 12; i < n; i++) memcpy(series_top_rows + i * 4, "2-- ", 4);
    memcpy(series_top_rows + at * 4, "161 251", 8);
    series_top_rows[n * 4 - 1] = 0;
    *rows = n;
    return host_handle(series_top_rows, sizeof series_top_rows);
}

/* The planner draws its cards into one window bitmap it allocates on entry
 * (room_enter, room 0x1F: 0x3A88 bytes -- the bitmap's 8-byte header and 64
 * bytes a cell for 18 x 13 cells, the film and series cards exactly).  With
 * the band the series card is up to SERIES_TOP_ROWS_MAX + 1 rows high. */
u16 series_top_window_buf(u16 size) {
    return series_top_on() ? 8 + 64 * 18 * (SERIES_TOP_ROWS_MAX + 1) : size;
}

static const u16 speed4_speed[4] = { 0x14, 0x1E, 0x3C, SPEED4 };
static const u16 speed4_x[4] = { 80, 113, 147, 180 };          /* the buttons: 80..207 */
static const u16 speed4_hot[5] = { 80, 112, 145, 179, 208 };   /* hotspot i: [hot[i], hot[i+1]) */

void speed4_panel(u16 *x0, u16 *x1, u16 *b0, u16 *b1) {
    *x0 = 0x48; *x1 = 0xD8;       /* the original's 0x58..0xC8 */
    *b0 = 0x50; *b1 = 0xD0;       /* 0x60..0xC0 */
}

/* the fourth button's car: the third's (tile 0x1C) with a flame out of its
 * back, between the speed lines (palette 19 red, 26 gold, 25 yellow) */
static const struct { u8 x, y, c; } speed4_flame[] = {
    {3, 4, 19}, {4, 4, 26}, {5, 4, 25}, {6, 4, 25},
    {0, 5, 19}, {1, 5, 26}, {2, 5, 26}, {3, 5, 25}, {4, 5, 25}, {5, 5, 25}, {6, 5, 25},
    {3, 6, 19}, {4, 6, 26}, {5, 6, 25}, {6, 6, 25},
};

void speed4_draw_buttons(void) {
    for (int i = 0; i < 4; i++) {
        draw_atlas_tile(col_to_px(g_game_speed == speed4_speed[i] ? 0x18 : 0x19), speed4_x[i], 0x62);
        draw_atlas_tile(col_to_px(0x1A + (i < 3 ? i : 2)), speed4_x[i] + 1, 0x63);
    }
    u32 b = g_screen_buf, pix = FP32(b, 4);
    u16 w = (u16)(FP16(b, 0) << 3);
    for (unsigned k = 0; k < sizeof speed4_flame / sizeof speed4_flame[0]; k++)
        wb(FPSEG(pix), (u16)(FPOFF(pix) + (0x63 + speed4_flame[k].y) * w + speed4_x[3] + 1 + speed4_flame[k].x), speed4_flame[k].c);
}

u16 speed4_hotspot(void) {
    if (!speed4_on() || g_room != ROOM_PLANNER || g_room_mode != 0xA) return 0;
    int x = (i16)g_mouse_x, y = (i16)g_mouse_y;   /* (no drag image, the arrow: cursor_x/cursor_y) */
    if (y < 98 || y > 110) return 0;
    for (int i = 0; i < 4; i++)
        if (x >= speed4_hot[i] && x < speed4_hot[i + 1]) return (u16)(0x3F + i);
    return 0;
}

int speed4_fine_clock(void) { return speed4_on() && g_game_speed == SPEED4; }

void speed4_after_load(void) {
    if (!speed4_on() && g_game_speed == SPEED4) g_game_speed = 0x3C;
}

/* More ad offers (fixes.h).  The original's desk (ui_draw_ad_offers
 * 26A7:14FB): three stacks of 3/4/3 folders (12 x 31 pixels, 8 apart), the
 * front one at y 76, x 107, the middle at y 61, x 127, the back at y 43,
 * x 153; a click (room_adagency_click 05FF:33F6) takes the folder at X for
 * the cursor at X+5..X+12, y Y..Y+30 (the last of a stack to X+9), the tip
 * (status_panel) with the mouse one pixel further left.  From 11 offers: a
 * front row at y 66 from x 68 (offers 1..13) and a back row at y 43 from
 * x 148 (14..17), drawn as the original draws, the back first and each row
 * from the right -- inside the background the function saves and restores
 * (64..207 x 43..112 with the rows, the original's 72..207 without).  The
 * rows stand 15 pixels left of the mock-up chosen first (83, 163): the tip
 * of a folder opens at x 184 (window column 23) and covered the right end
 * of both rows (the user, 2026-10-13).  The front row covers the agent's
 * hands, the back row the clock on the desk (decoration: no hotspot, no
 * animation). */
u16 ad_offers_count(void) { return difftest_on ? 10 : (u16)AD_OFFERS_OPT(); }   /* (main.c takes 10..17 only) */

#define ADROW_FRONT 13           /* offers in the front row */
#define ADROW_X_FRONT 68         /* the front row's first folder; its last (13) ends at 175 */
#define ADROW_X_BACK 148         /* the back row's first folder; its last (17) ends at 183, before the tip */
void adoffer_pos(u16 i, u16 *x, u16 *y) {
    if (i <= ADROW_FRONT) { *x = (u16)(ADROW_X_FRONT + 8 * (i - 1)); *y = 66; }
    else { *x = (u16)(ADROW_X_BACK + 8 * (i - ADROW_FRONT - 1)); *y = 43; }
}

/* the background ui_draw_ad_offers saves and puts back (x a multiple of 8: bitmap_put_screen) */
void adoffers_bg(u16 *x, u16 *w) {
    if (g_nadoffers > 10) { *x = 0x40; *w = 0x90; }
    else { *x = 0x48; *w = 0x88; }
}

/* the row of offers first..last (draw_sprite's x of the first, its y) */
static u16 adoffer_row_at(int x, int y, u16 first, u16 last) {
    u16 x0, y0, xl, yl;
    if (last < first) return 0;
    adoffer_pos(first, &x0, &y0);
    adoffer_pos(last, &xl, &yl);
    if (y < y0 || y > y0 + 30 || x < x0 + 5 || x > xl + 9) return 0;
    return (u16)(first + (x - x0 - 5) / 8);
}
/* the front row covers the back where they overlap (y 66..73) -- unless its folder
 * there is gone (taken, or an empty place): then the back row's shows through */
u16 adoffer_at(int x, int y) {
    u16 n = g_nadoffers, f, b;
    if (n <= 10) return 0;
    f = adoffer_row_at(x, y, 1, n < ADROW_FRONT ? n : ADROW_FRONT);
    if (f && (i8)ad_offers(f).shelf >= 0 && ad_offers(f).ad != 0) return f;
    b = adoffer_row_at(x, y, ADROW_FRONT + 1, n);
    return b ? b : f;
}

/* the original's desk hotspots 0x26 (05FF:33F6 gives the carried folder back):
 * (105,62)-(163,106) and (151,43)-(181,74), for the dragged folder's corner
 * (cursor_x/cursor_y while it is carried) -- here the same around the rows
 * (2 left of the first, 12 right of the last), holding the original's */
u16 adoffer_hotspot(void) {
    if (g_nadoffers <= 10 || g_room != ROOM_AD_AGENCY || (g_room_mode != 1 && g_room_mode != 2)) return 0;
    if (mouse_in_rect(ADROW_X_FRONT - 2, 62, ADROW_X_FRONT + 8 * 12 + 12, 106) || mouse_in_rect(ADROW_X_BACK - 2, 43, ADROW_X_BACK + 8 * 3 + 12, 74)) return 0x26;
    return 0;
}

/* called from main() so that --fixes=list and typos are handled at start-up */
int fix_init(void) { if (!fix_parsed) fix_parse(); return 1; }
