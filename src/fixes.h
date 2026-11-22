/* Fixes of original bugs, off in classic mode (the default).
 *   --fixes            all fixes          --fixes=NAME,NAME  selected ones
 *   --classic          none (default)     --fixes=list       print the table
 * Only the decompiled build (src/) knows them; the machine-level build is
 * always the original.  Under --difftest every fix is off, so the
 * comparison with the original stays meaningful.  The table: fixes.c. */
#ifndef FIXES_H
#define FIXES_H
/* the fixes' bits in fix_mask (fixes.c), 64 of them (32 were full with trendy-studios) */
#define FIX_INTRUDER (1ull << 0)         /* 1DA8:2D3B intruder hang on a series-only library */
#define FIX_RANDOM0 (1ull << 1)          /* 0D5E:0229 random_n(0) divides by zero */
#define FIX_AI_STUDIO (1ull << 2)        /* 348B:098D AI rents studio 13 instead of the chosen one */
#define FIX_AI_FILL_HOUR (1ull << 3)     /* 2DD9:0DD9 AI audience estimate gets the category instead of the hour */
#define FIX_AI_SERIES_RULE (1ull << 4)   /* 2DD9:162A AI series genre check uses id-24 instead of id+1000 */
#define FIX_SERIES_AVG (1ull << 5)       /* 2DD9:3085 series average topicality over episodes 2..n+1 */
#define FIX_ESTATE_TEXT (1ull << 6)      /* 05FF:3AB7 estate agent shows room names 0x77/0x78 as messages */
#define FIX_TX_PRICE (1ull << 7)         /* 05FF:2C02 transmitter purchase checked against less than its price */
#define FIX_LIBRARY_OWNER (1ull << 8)    /* 0AA4:24C3 a film put back on the library shelf stays on the market */
#define FIX_NEWS_CACHE (1ull << 9)       /* 26A7:292F headline lookup searches 20 of 24 cache entries */
#define FIX_AI_ODD_RANGE (1ull << 10)    /* 2DD9:0DD9 AI odd-length check reads its half-day range from stack garbage */
#define FIX_BOSS_MUSIC (1ull << 11)      /* 0AA4:000E boss calls snd_room_music_stop without its arguments */
#define FIX_FILM200 (1ull << 12)         /* data: film 200's texts damaged ("xxx", German cut off), never offered */
#define FIX_CATCH_BOUNTY (1ull << 13)    /* 1DA8:3643 catch bounty only for figure 0 (the human on station 1) */
#define FIX_AI_VISIT (1ull << 14)        /* 2DD9:3C0D AI "visit the human" (state 5) goes to station 1's rooms */
#define FIX_AI_TRENDY (1ull << 15)       /* 24B8:0C4A "Trendy" aired by an AI: trends written by player number, not AI index */
#define FIX_AI_RIVAL (1ull << 16)        /* 2DD9:26EA AI budget plan compares a computer player with itself */
#define FIX_AI_ARRIVED (1ull << 17)      /* 2DD9:4126 "already at the door" only for AI index 0: AI 1 waits again */
#define FIX_TITLE_CACHE (1ull << 18)     /* 048C:025B film text cache: slots 49.. written over the series cache */
#define FIX_CASE_OWNER (1ull << 19)      /* 05FF:090A a library tape carried in the case is ownerless and on the market */
#define FIX_AI_FILL_GENRE (1ull << 20)   /* 2DD9:12C1/1316 AI fill: genre bit and audience row of the best, not the candidate */
#define FIX_AI_LIB_QUALITY (1ull << 21)  /* 348B:0895 AI library average over genres 0..11, slots 0..count */
#define FIX_STUDIO_TAKEOVER (1ull << 22) /* 24B8:170C studio hand-over gets the player loop index as the studio */
#define FIX_STUDIO_RESERVED (1ull << 23) /* 229E:1080/1264 a studio's pending renter/give-up byte is never cleared */
#define FIX_CASE_CORNER (1ull << 24)     /* 05FF:0359/14E1:000E the film agency's case hands out slots past the carried tapes */
#define FIX_CREDIT_REPAY (1ull << 25)    /* 0AA4:000E repaying the boss's credit is not limited by the credit, nor by a negative cash */
#define FIX_AI_RATING (1ull << 26)       /* 2DD9:2B8D AI yesterday's rating: viewers x 100 overflows 32 bits above 21.5M viewers */
#define FIX_AI_BIDS (1ull << 27)         /* 2DD9:1DDA AI auction bid list: a new entry per bid, 11 places, never emptied but by the human */
#define FIX_AI_WARNING (1ull << 28)      /* 229E:0225 a computer player's refused placement opens a warning box on the human's screen */
#define FIX_LIVE_AID (1ull << 29)        /* data: film 164 "Live Aid" is 0 hours long */
#define FIX_VIDEO_CONN (1ull << 30)      /* data: screenplay 20 "Video-Connection" asks nothing of its two roles */
#define FIX_TRENDY_STUDIOS (1ull << 31)  /* 24B8:0C4A "Trendy" ending ends the production lock of every running studio */
#define FIX_PLANNER_ROW (1ull << 32)     /* 1C18:12F2 a programme off the shelf leaves a stale slot; 05FF:0D0E the planner's list takes a click on an empty row as it */
#define FIX_LOAD_MUSIC (1ull << 33)      /* 05FF:44F4 a game loaded from the setup screen starts without its room's music */
/* fixes a network game always has, whatever the host chose (Classic included):
 * money cheats for whoever finds them -- the corner of the case, the boss's repayment
 * (but credit-repay is off with the original's credit system: --credit-line=original) */
#define FIX_MP_ALWAYS (FIX_CASE_CORNER | FIX_CREDIT_REPAY)
int fix_on(unsigned long long fix);
/* fix "ai-bids": the places of a computer player's auction bid list (DS:756A + player x 0x42, 6 bytes each) */
#define AI_BIDS_MAX 11
/* fix "case-corner": the film agency's case slot under the cursor (column, slot =
 * column + 5 x row) is a carried tape -- one of the 5 drawn columns, below the count */
int case_slot_ok(u16 col, u16 slot);
/* data fix film-200 (fixes.c): the damaged texts of film 200 restored, while they are the original's */
int fix_film200_text(u8 *buf, u16 len, u8 *out, u16 cap);
int fix_film200_record(void);
/* data fix live-aid (fixes.c): film 164's length, while the record is the original's */
int fix_live_aid_record(void);
/* data fix video-connection (fixes.c): screenplay 20's role requirements, while they are the original's */
int fix_video_conn_record(void);
/* a savegame's rules (src/saverules.c): the fixes' choice anew (spec as --fixes, NULL Classic; unknown
 * names passed over), the data fixes in or out as it says */
void fix_respec(const char *spec);
void fix_data_records_sync(void);
/* A single player's savegame carries its rules (src/saverules.c): the line of them in the
 * extension (the eighth "MTVX" count its length), at most SAVERULES_MAX bytes -- the launcher
 * (launcher/content.go savegameOptions) and tools/content.py count it into a savegame's largest.
 *   saverules_needed      a rule is not the build's default: the savegame gets the line
 *   saverules_text        the line ("fixes=all speed4=0 ..."), its length (0: does not fit)
 *   saverules_apply       a savegame's line being loaded (slot: its resource): the game takes its rules
 *   saverules_after_load  after such a load: what follows the rules (the data fixes) */
#define SAVERULES_MAX 768
#define SAVE_SLOT_RES 0xD1            /* the safe's names; tape n (1..10) is resource SAVE_SLOT_RES + n */
#define START_SAVE_RES 0x2B           /* the start savegame: a new game (game_main, and after game over) */
int saverules_needed(void);
u16 saverules_text(u8 *out, u16 cap);
void saverules_apply(u16 slot, const u8 *line, u16 n);
void saverules_after_load(void);
/* fix "title-cache" (text_cache.c): the film text cache's slots past the 49
 * of g_film_txt_cache (DS:63E5), in host memory; the texts in the game's heap */
#define FILM_TXT_MORE 64
typedef struct { u16 id; u32 txt[2]; } FilmTxtMore;   /* film id; title, description */
extern FilmTxtMore film_txt_more[FILM_TXT_MORE];
u32 film_txt_more_text(u16 film_id, int which);       /* 0: title, 1: description; 0 if not there */
/* Game options (not bug fixes): the player's choice, in any mode but --difftest.
 * start_money_human: the human's cash in a new game (--start-money, settings
 * [game] start_money), else the original's $200 000 */
u32 start_money_human(void);
/* credit_haggle_max: the boss haggles for another $100 000 (image 10..49, from day 2)
 * while the credit is at most this -- the original's $200 000; with --credit-line
 * (settings [game] credit_line) the line grows $100 000 a day up to the option;
 * --credit-line=original: the original's, and fix credit-repay off (fix_on) */
u32 credit_haggle_max(void);
/* terror_damage: what a terrorist's bomb costs the owner of the room (--terror-damage,
 * settings [game] terror_damage), else the original's $50 000; terror_text puts the
 * amount into message 0x135 (text_dup's buffer) */
u32 terror_damage(void);
void terror_text(u32 buf);
/* sammy_prize: the Sammy's prize money for category 1..4 (--sammy-prize, settings [game]
 * sammy_news ... sammy_series), else none; sammy_pay gives it to the winner, and
 * sammy_prize_text adds it to the winner's message 0x159 (str_format's buffer) */
u32 sammy_prize(u16 cat);
void sammy_pay(u16 winner, u16 cat);
void sammy_prize_text(u32 buf, u32 prize);
/* The system arrow (--mouse system, settings [input] mouse = system): the
 * system's own pointer stands for the game's arrow (the picture only; the
 * game plays and takes its time as with the free mouse).
 * arrow_cover / arrow_uncover around each place that draws the cursor
 * sprite into a bitmap (header far pointer, the sprite's first row there):
 * the arrow is drawn as the original draws it, then what it covered is put
 * back.  arrow_refresh: the arrow comes or goes where the cursor stands. */
void arrow_cover(u32 bitmap, int y);
void arrow_uncover(void);
void arrow_refresh(void);
/* A network game: the system's cursor shows the carried item (fixes.c drag_stands_in);
 * drag_cover / drag_uncover around the places that draw it, drag_refresh as arrow_refresh. */
void drag_cover(u32 bitmap);
void drag_uncover(void);
void drag_refresh(void);
/* The fourth speed (--fourth-speed, settings [game] fourth_speed): a fourth
 * button in the planner's Control-Center (options_draw 2B74:0E63), twice the
 * fastest of the original's three.  The panel grows to hold four buttons of
 * the original's size; the fourth shows the third's car with a flame.
 *   speed4_on          the option is on (never under --difftest)
 *   speed4_panel       the panel's and its boxes' x-coordinates
 *   speed4_draw_buttons  the four buttons into g_screen_buf
 *   speed4_hotspot     the speed button under the cursor in the Control-Center
 *                      (0x3F..0x42), 0 elsewhere: the original's hotspot table
 *                      knows only three
 *   speed4_fine_clock  at the fourth speed the clock counts whole seconds,
 *                      not whole 50-tick steps (else it would jump two minutes
 *                      at a time and skip the :00/:05/:55 refreshes)
 *   speed4_after_load  a game saved at the fourth speed, loaded without the
 *                      option, goes on at the third */
/* The series card's topicality (--series-topicality, settings [game]
 * series_topicality): the series card (tooltip_show 14E1:16BB, id 1000..)
 * shows "Topical" with its bar as the film card does (text 417, 40 pixels
 * at full topicality) -- on a light band of its own under the next episode,
 * the last row dark, so the bands go on alternating.  Its 12 rows stay 12;
 * when the next episode's title takes two lines the card grows by one.
 *   series_top_on      the option is on (never under --difftest)
 *   series_top_bar     the bar's length: all episodes' topicality against
 *                      their quality, as series_price 1DA8:4035 weighs them
 *   series_top_layout  the card's row layout with the band under the
 *                      episode's lines (2 as a rule; each more: a row more)
 *                      and its number of rows (the card's g_tip_h) -- a far
 *                      pointer to release with host_handle_release
 *   series_top_window_buf  the size of the planner's window bitmap: the
 *                      original's fits its cards (18 x 13 cells) exactly,
 *                      the taller series card would write past its end */
int series_top_on(void);
/* The rule "Unfertige Drehbuecher integrieren" (--original-screenplays, settings [game]
 * original_screenplays; src/origscripts.c): the original's unfinished screenplays
 * 25-33 and 37-50 filled and offered.
 *   origscripts_after_load  after every game_load: the rule's screenplays, series and
 *                      films in (the rule on, or a savegame made with it) or out
 *   origscripts_texts  a screenplay's 16 texts as read: the rule's where the game's
 *                      are empty (txt_script_cache_load_one) */
int origscripts_on(void);
int origscripts_active(void);       /* its screenplays are in the game (the rule, or a savegame of it) */
void origscripts_after_load(void);
int origscripts_texts(u16 id, const u8 *buf, u16 len, u8 *out, u16 cap);
u16 series_top_bar(u32 series);
u32 series_top_layout(u32 layout, u16 lines, u16 *rows);
u16 series_top_window_buf(u16 size);
#define SPEED4 0x78               /* game seconds per 50 timer ticks: 120 (the original's 20, 30, 60) */
int speed4_on(void);
void speed4_panel(u16 *x0, u16 *x1, u16 *b0, u16 *b1);
void speed4_draw_buttons(void);
u16 speed4_hotspot(void);
int speed4_fine_clock(void);
void speed4_after_load(void);
/* More ad offers (--ad-offers=N, settings [game] ad_offers = N, 10..17): the
 * ad agency offers N contracts instead of the original's 10 (offers 11..N:
 * g_ad_offers_extra, game_types.h ad_offers_at).  10 keeps the original's
 * desk, three stacks of 3/4/3 folders; from 11 the folders stand in a front
 * row (up to 13) and a back row (14..17).
 *   ad_offers_count    the number of offers (10 under --difftest)
 *   adoffer_pos        offer i's folder in the rows (draw_sprite's x, y)
 *   adoffers_bg        the background ui_draw_ad_offers saves: its x and width
 *   adoffer_at         the offer whose folder answers at (x, y) in the rows
 *                      (the cursor's coordinates as the original's click test
 *                      takes them), 0 for none
 *   adoffer_hotspot    0x26 (the desk: give the carried folder back) over the
 *                      rows while a folder is carried, 0 elsewhere: the
 *                      original's hotspot table knows only its stacks' desk */
u16 ad_offers_count(void);
void adoffer_pos(u16 i, u16 *x, u16 *y);
void adoffers_bg(u16 *x, u16 *w);
u16 adoffer_at(int x, int y);
u16 adoffer_hotspot(void);
/* More film offers (--film-offers=N, settings [game] film_offers = N, 10..17,
 * src/film_offers.c): the film dealer offers N films and series instead of the
 * original's 10 (slots 11..N: g_dealer_extra, game_types.h dealer_at).  10
 * keeps the original's counter; from 11 the row of tapes grows to the left,
 * the new tapes built from resource 155 and drawn by film_offers.c.
 *   film_offers_count      the number of offers (10 under --difftest)
 *   filmshelf_rect_at      slot si's shelf rectangle (a far pointer, x1 y1 x2 y2)
 *   filmoffers_prepare     the new tapes and their rectangles, out of the room's bank (once)
 *   filmoffers_draw        the new tapes on the counter into a bitmap showing the screen from (ox, oy)
 *   filmoffer_at_cursor    the new slot under the cursor (the front one first), 0 for none
 *   filmoffer_hotspot      0x18 (give a fresh film back) over the longer row while one is carried
 *   filmoffers_shelf_bg    the background ui_draw_dealer_shelf saves: its x, width, height
 *   filmoffers_clear       slots from..17 empty, off the counter (the next refill fills them)
 *   filmoffers_return_to_market  an offer of a savegame past this game's slots, back to the market
 *   filmoffers_tape        new tape j out of the ten original tapes' pixels, without a game (src/ki_make.c) */
u16 film_offers_count(void);
u32 filmshelf_rect_at(u16 si);
void filmoffers_prepare(void);
void filmoffers_draw(u32 bmp, int ox, int oy);
u16 filmoffer_at_cursor(void);
u16 filmoffer_hotspot(void);
void filmoffers_shelf_bg(u16 *x, u16 *w, u16 *h);
void filmoffers_clear(u16 from);
void filmoffers_return_to_market(const u8 *slot);
int filmoffers_tape(const u8 *const orig[10], int j, u8 *px, int *w, int *h);
/* More auction lots (--auction-lots=N, settings [game] auction_lots = N, 10..20,
 * src/auction_lots.c): the film auction holds N lots instead of the original's
 * 10 (lots 11..N: g_lots_extra, game_types.h auction_at).  10 keeps the
 * original's board; from 11 it has a second page with lots 11..N, turned by a
 * tab at its right edge.
 *   auction_lots_count     the number of lots (10 under --difftest)
 *   auction_page_reset     the board opens: page 1
 *   auction_lot_at_place   the lot at place k (1..10) of the page shown, 0 past the game's lots
 *   auction_board_dx       the board's x shift: 0, with the tab -16 (the cursor reaches x 303 at most)
 *   auction_tab_shown      the board has the tab (more than 10 lots)
 *   auction_tab_draw       the tab, with the other page's number
 *   auction_tab_click      a click on the board: on the tab, the other page (1)
 *   auction_lots_clear     lots 11..20 empty (a new game, a savegame without them)
 *   auction_lot_return_to_market  a lot of a savegame past this game's, its programme back to the market
 *   auction_lots_drop_ai_bids     the computer players' bids on lots past this game's, their money back
 *   auction_lots_night_bids       the night: the computer players' bid lists empty (more than 10 lots) */
u16 auction_lots_count(void);
void auction_page_reset(void);
u16 auction_lot_at_place(u16 k);
int auction_board_dx(void);
int auction_tab_shown(void);
void auction_tab_draw(void);
int auction_tab_click(void);
void auction_lots_clear(void);
void auction_lot_return_to_market(const u8 *lot);
void auction_lots_drop_ai_bids(void);
void auction_lots_night_bids(void);
/* Role fit in the shop (--cast-fit, settings [display] cast_fit, src/cast_fit.c): the
 * actor panel's bars give way to a row "Eignung" -- each actor's fit for the picked role, in per cent.
 * Display only (no time, no game state).
 *   castfit_pick   the shop's slot click: slot (1..10) of the current list picked
 *   castfit_room_enter  the shop entered: keep the room's picture under the panel
 *   castfit_draw   the row, at the end of ui_draw_actor_card */
void castfit_pick(u16 slot);
void castfit_room_enter(void);

/* src/openmtv_sign.c: the sign at the tower's foot reads "Open" + a yellow "MTV" box
 * instead of "Mad-TV" (not in the classic mode): resource 16's sprite 0 as loaded */
void openmtv_sign(u32 bank);
void castfit_draw(void);
#endif
