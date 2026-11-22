/* More auction lots (--auction-lots=N, settings [game] auction_lots = N,
 * 10..20): the film auction holds N lots instead of the original's 10 (a
 * game option, fixes.h).
 *
 * The original's board (ui_draw_auction_board 26A7:2F27, Mr. Jabble's
 * popup): 2 columns of 5 lots fill the room's picture, x 16..303, y 8..111.
 * From 11 the board gets a second page with lots 11..N in the same places
 * (the layout chosen with the user): a tab at the board's right edge shows
 * the other page's number and turns the page on a click.  The game's cursor
 * reaches x 303 at most, so with the tab the board moves 16 pixels to the
 * left (x 0..287) and the tab stands at x 288..303.
 * Every board opens on page 1.  The page belongs to the human looking at the
 * board (one per seat of a network game; only one human at a time is in the
 * film agency, and the others' machines follow that human's clicks the
 * same).
 *
 * The lots 11..N themselves: g_lots_extra behind AUCTION_LOTS_SEG
 * (game_types.h auction_at), a host region (src/state.c).  The night's
 * refill (auction_daily) fills lots 1..8 as the original and 11..N; 9 and
 * 10 stay free for what the players give Mr. Jabble, as in the original.
 *
 * The computer players' bid lists (g_ai_bids, 11 places each, a place per
 * lot with the fix ai-bids, which is always on here) are never emptied in
 * the original but by a human outbidding them: with 10 lots the 11 places
 * suffice, with 20 a computer player would soon bid on 11 lots only.  So
 * with more lots the night empties them, before the bids are settled
 * (auction_lots_night_bids). */
#include <string.h>
#include "game.h"
#include "fixes.h"
#include "seat.h"

u16 auction_lots_count(void) { return difftest_on ? 10 : (u16)AUCTION_LOTS_OPT(); }   /* (main.c takes 10..20 only) */

/* ---- the board's pages ---- */
static u8 page[SEATS_MAX];               /* 0: lots 1..10, 1: lots 11..20 */
static int seat_ix(void) { return seat_cur > 0 && seat_cur < SEATS_MAX ? seat_cur : 0; }

void auction_page_reset(void) { page[seat_ix()] = 0; }

/* the lot at place k (1..10, column * 5 + row) of the page shown; 0 for a
 * place past the game's lots */
u16 auction_lot_at_place(u16 k) {
    if (g_nlots <= 10) return k;
    u16 lot = (u16)(k + page[seat_ix()] * 10);
    return lot <= g_nlots ? lot : 0;
}

/* the board's shift to the left (with the tab) and the tab: x 288..303, y 8..55 */
int auction_board_dx(void) { return g_nlots > 10 ? -16 : 0; }
#define TAB_X0 0x120
#define TAB_X1 0x130
#define TAB_Y0 8
#define TAB_Y1 0x38
int auction_tab_shown(void) { return g_nlots > 10; }

void auction_tab_draw(void) {
    if (!auction_tab_shown()) return;
    ui_tile_box(TAB_X0, TAB_Y0, TAB_X1, TAB_Y1, 0);
    char t[4];
    t[0] = page[seat_ix()] ? '1' : '2';  /* the other page's number */
    t[1] = 0;
    u32 h = host_handle(t, sizeof t);
    text_draw(TAB_X0 + 4, TAB_Y0 + 20, LO16(h), HI16(h));   /* (not text_draw_free: the handle is no heap block) */
    host_handle_release(h);
}

/* a click on the board: on the tab, the other page (1); else 0 */
int auction_tab_click(void) {
    if (!auction_tab_shown()) return 0;
    if (!mouse_in_rect(TAB_X0, TAB_Y0, TAB_X1 - 1, TAB_Y1 - 1)) return 0;
    page[seat_ix()] ^= 1;
    return 1;
}

/* ---- new games and savegames ---- */
void auction_lots_clear(void) { memset(g_lots_extra, 0, (AUCTION_LOTS_MAX - 10) * 9); }

/* a lot of a savegame past this game's lots: its programme back to the market,
 * as the night gives back a lot nobody bid on (auction_daily 24B8:0905) -- else
 * it would stay off the market and with the lot's code as its owner.  A
 * computer player leading it gets its bid back to the film budget, as a human
 * outbidding it gives it back (auction_toggle_bid 229E:0088). */
void auction_lot_return_to_market(const u8 *lot) {
    u16 program = (u16)(lot[0] | lot[1] << 8);
    i8 bidder = (i8)lot[6];
    if (!program) return;
    if (bidder >= 1 && bidder <= 3 && !mp_is_human(bidder))
        ai(bidder).films = ai(bidder).films + (u32)(lot[2] | lot[3] << 8 | lot[4] << 16 | (u32)lot[5] << 24);
    if ((i16)program >= 1000) {
        if (program - 1000 >= 1 && program - 1000 <= g_nseries) {
            series(program - 1000).owner = 0;
            series_flags(program + 0xFC18) = series_flags(program + 0xFC18) | 0x20;
        }
    } else if (program <= g_nfilms) {   /* (a bigger mod's: not here) */
        films(program).owner = 0;
        films(program).flags_16 = films(program).flags_16 | 0x20;
    }
}

/* after a savegame with more lots than this game was loaded: what the computer
 * players hold of lots past this game's goes -- their bid lists' entries
 * (g_ai_bids, 6 bytes each: lot, u32 reserve; the money came back with the lot,
 * auction_lot_return_to_market) and their shopping plans' auction bids not yet
 * made (DS:2512, the price reserved from the film budget when planned:
 * back) -- else ai_execute_purchases and ai_auction_rebid would bid on such a
 * lot through the original's address, the contracts at DS:1B70.. */
void auction_lots_drop_ai_bids(void) {
    for (u16 p = 1; p <= 3; p++) {
        u16 base = (u16)(p * 0x42 + 0x756A), n = g_ai_bid_count(p), k = 0;
        if ((i16)n >= 0 && n <= AI_BIDS_MAX) {
            while (k < n) {
                if (G16(base + k * 6) > g_nlots) {
                    memmove(dg_addr((u16)(base + k * 6)), dg_addr((u16)(base + (k + 1) * 6)), (u16)((n - k - 1) * 6));
                    n--;
                    memset(dg_addr((u16)(base + n * 6)), 0, 6);
                } else {
                    k++;
                }
            }
            g_ai_bid_count(p) = n;
        }
        for (u16 j = 0; j < 7; j++) {    /* (0x4D bytes a player: 7 entries) */
            u32 e = ai_buy_plan_at(p, j);
            if (AT(ai_buy, e).program != 0 && AT(ai_buy, e).auction != 0 && AT(ai_buy, e).slot > g_nlots) {
                ai(p).films = ai(p).films + AT(ai_buy, e).price;
                AT(ai_buy, e).program = 0;
                AT(ai_buy, e).slot = 0;
                AT(ai_buy, e).price = 0;
                AT(ai_buy, e).score = 0;
                AT(ai_buy, e).auction = 0;
            }
        }
    }
}

/* the night, before the auction's lots are settled (auction_daily, more than
 * 10 lots): every computer player's bid list empties -- the bids are settled
 * tonight.  No money moves: a lot the computer player leads is paid tonight
 * from its reserve; one the other computer player or a human took had its
 * bid given back to the film budget then (ai_execute_purchases,
 * auction_toggle_bid 229E:0088). */
void auction_lots_night_bids(void) {
    for (u16 p = 0; p <= 3; p++) {
        u16 base = (u16)(p * 0x42 + 0x756A);
        if (!g_ai_bid_count(p)) continue;
        memset(dg_addr(base), 0, (u16)(AI_BIDS_MAX * 6));
        g_ai_bid_count(p) = 0;
    }
}
