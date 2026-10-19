/* Fixes of original bugs, off in classic mode (the default).
 *   --fixes            all fixes          --fixes=NAME,NAME  selected ones
 *   --classic          none (default)     --fixes=list       print the table
 * Only the decompiled build (src/) knows them; the machine-level build is
 * always the original.  Under --difftest every fix is off, so the
 * comparison with the original stays meaningful.  The table: fixes.c. */
#ifndef FIXES_H
#define FIXES_H
enum {
    FIX_INTRUDER  = 1u << 0,   /* 1DA8:2D3B intruder hang on a series-only library */
    FIX_RANDOM0   = 1u << 1,   /* 0D5E:0229 random_n(0) divides by zero */
    FIX_AI_STUDIO = 1u << 2,   /* 348B:098D AI rents studio 13 instead of the chosen one */
    FIX_AI_FILL_HOUR = 1u << 3,   /* 2DD9:0DD9 AI audience estimate gets the category instead of the hour */
    FIX_AI_SERIES_RULE = 1u << 4, /* 2DD9:162A AI series genre check uses id-24 instead of id+1000 */
    FIX_SERIES_AVG = 1u << 5,     /* 2DD9:3085 series average topicality over episodes 2..n+1 */
    FIX_ESTATE_TEXT = 1u << 6,    /* 05FF:3AB7 estate agent shows room names 0x77/0x78 as messages */
    FIX_TX_PRICE = 1u << 7,       /* 05FF:2C02 transmitter purchase checked against less than its price */
    FIX_LIBRARY_OWNER = 1u << 8,  /* 0AA4:24C3 a film put back on the library shelf stays on the market */
    FIX_NEWS_CACHE = 1u << 9,     /* 26A7:292F headline lookup searches 20 of 24 cache entries */
    FIX_AI_ODD_RANGE = 1u << 10,  /* 2DD9:0DD9 AI odd-length check reads its half-day range from stack garbage */
    FIX_BOSS_MUSIC = 1u << 11,    /* 0AA4:000E boss calls snd_room_music_stop without its arguments */
    FIX_FILM200 = 1u << 12,       /* data: film 200's texts damaged ("xxx", German cut off), never offered */
    FIX_CATCH_BOUNTY = 1u << 13,  /* 1DA8:3643 catch bounty only for figure 0 (the human on station 1) */
    FIX_AI_VISIT = 1u << 14,      /* 2DD9:3C0D AI "visit the human" (state 5) goes to station 1's rooms */
    FIX_AI_TRENDY = 1u << 15,     /* 24B8:0C4A "Trendy" aired by an AI: trends written by player number, not AI index */
    FIX_AI_RIVAL = 1u << 16,      /* 2DD9:26EA AI budget plan compares a computer player with itself */
    FIX_AI_ARRIVED = 1u << 17,    /* 2DD9:4126 "already at the door" only for AI index 0: AI 1 waits again */
    FIX_TITLE_CACHE = 1u << 18,   /* 048C:025B film text cache: slots 49.. written over the series cache */
    FIX_CASE_OWNER = 1u << 19,    /* 05FF:090A a library tape carried in the case is ownerless and on the market */
    FIX_AI_FILL_GENRE = 1u << 20, /* 2DD9:12C1/1316 AI fill: genre bit and audience row of the best, not the candidate */
    FIX_AI_LIB_QUALITY = 1u << 21, /* 348B:0895 AI library average over genres 0..11, slots 0..count */
    FIX_STUDIO_TAKEOVER = 1u << 22, /* 24B8:170C studio hand-over gets the player loop index as the studio */
    FIX_STUDIO_RESERVED = 1u << 23, /* 229E:1080/1264 a studio's pending renter/give-up byte is never cleared */
    FIX_CASE_CORNER = 1u << 24,   /* 05FF:0359/14E1:000E the film agency's case hands out slots past the carried tapes */
    FIX_CREDIT_REPAY = 1u << 25,  /* 0AA4:000E repaying the boss's credit is not limited by the credit, nor by a negative cash */
    FIX_AI_RATING = 1u << 26,     /* 2DD9:2B8D AI yesterday's rating: viewers x 100 overflows 32 bits above 21.5M viewers */
    FIX_AI_BIDS = 1u << 27,       /* 2DD9:1DDA AI auction bid list: a new entry per bid, 11 places, never emptied but by the human */
    FIX_AI_WARNING = 1u << 28,    /* 229E:0225 a computer player's refused placement opens a warning box on the human's screen */
    FIX_LIVE_AID = 1u << 29,      /* data: film 164 "Live Aid" is 0 hours long */
    FIX_VIDEO_CONN = 1u << 30,    /* data: screenplay 20 "Video-Connection" asks nothing of its two roles */
};
/* fixes a network game always has, whatever the host chose (Classic included):
 * money cheats for whoever finds them -- the corner of the case, the boss's repayment
 * (but credit-repay is off with the original's credit system: --credit-line=original) */
#define FIX_MP_ALWAYS (FIX_CASE_CORNER | FIX_CREDIT_REPAY)
int fix_on(unsigned fix);
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
