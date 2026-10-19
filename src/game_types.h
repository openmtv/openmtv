/* Enumerations and record tables (structs, accessors).  Started from
 * names/enums.txt and names/structs.txt; hand-maintained since the freeze. */
#ifndef GAME_TYPES_H
#define GAME_TYPES_H

/* enumerations (names/enums.txt) */
enum room { ROOM_BUILDING = 0x0, ROOM_OFFICE = 0x1, ROOM_NEWS = 0x2, ROOM_BOSS = 0x3, ROOM_LIBRARY = 0x4, ROOM_STUDIO = 0x5, ROOM_FILM_AGENCY = 0x8, ROOM_SCRIPT_AGENCY = 0x9, ROOM_ESTATE_AGENT = 0xA, ROOM_AD_AGENCY = 0xB, ROOM_SUPERMARKET = 0xC, ROOM_INFO_BOARD = 0xD, ROOM_BETTY = 0xE, ROOM_TX_MAP = 0x19, ROOM_PLANNER = 0x1F, ROOM_SAVE_ROOM = 0x3A, ROOM_INFO_BOARD_GO = 0x3D, ROOM_ESTATE_PLAN = 0x3E, ROOM_SHOP = 0x3F, ROOM_PORTER = 0x40, ROOM_NEW_GAME = 0x41 };
enum genre { GENRE_LOVE = 0x1, GENRE_ACTION = 0x2, GENRE_EPIC = 0x3, GENRE_COMEDY = 0x4, GENRE_WHODUNIT = 0x5, GENRE_CULTURE = 0x6, GENRE_LIVE = 0x7, GENRE_SHOW = 0x8, GENRE_SF = 0x9, GENRE_SUNDRY = 0xA };
enum series_genre { SGENRE_FAMILY = 0x1, SGENRE_SF = 0x2, SGENRE_WESTERN = 0x3, SGENRE_WHODUNIT = 0x4, SGENRE_SUNDRY = 0x5 };
enum ledger { LEDGER_FILMS_BOUGHT = 0x1, LEDGER_SERIES_BOUGHT = 0x2, LEDGER_TRANSMITTERS = 0x3, LEDGER_SCRIPTS = 0x4, LEDGER_DAMAGES = 0x5, LEDGER_SUNDRY = 0x6, LEDGER_CAST = 0x7, LEDGER_STUDIO_RENT = 0x8, LEDGER_TX_FEES = 0x9, LEDGER_NEWS = 0xA, LEDGER_FILMS_SOLD = 0xB, LEDGER_SERIES_SOLD = 0xC, LEDGER_SUNDRY_INCOME = 0xD, LEDGER_AD_INCOME = 0xE, LEDGER_INTEREST = 0xF, LEDGER_CREDIT_REPAID = 0x10 };
enum news_category { NEWS_POLITICS = 0x1, NEWS_SHOWBIZ = 0x2, NEWS_SUNDRY = 0x3 };
enum ai_state { AI_OFFICE_BOOK = 0x0, AI_AD_AGENCY = 0x1, AI_MOVIE_AGENCY = 0x2, AI_NEWS_ROOM = 0x3, AI_OFFICE_TX = 0x4, AI_RIVAL_1 = 0x5, AI_RIVAL_2 = 0x6, AI_PRODUCTION = 0x7, AI_OFFICE_PLAN = 0x8, AI_SUPERMARKET = 0x9, AI_BETTY = 0xA, AI_INFO_BOARD = 0xB, AI_PORTER = 0xC };

/* record tables (names/structs.txt): table(i).field or table(i, j).field */
#pragma pack(push, 1)
struct player {   /* player records 1..3 (0 unused), 0x17 bytes */
    u32 name;   /* far ptr: manager name */
    u32 station;   /* far ptr: station name */
    u32 money;   /* cash */
    u16 betty_love;   /* Betty's affection (0..100, capped at image) */
    u8 _0E[2];
    u8 news_agency;   /* +0x11..0x13: news agency 1..3 subscribed */
    u8 news_sub;   /* news agency subscriptions +0x11..+0x13 (1 political, 2 show, 3 sundry) */
    u8 _12[2];
    u8 out;   /* player out of the game */
    u16 image;   /* image %, the 3 players sum to 100 (< 5 fired) */
};
_Static_assert(sizeof(struct player) == 0x17, "struct player");
struct film {   /* movies 1..200, 0x17 bytes */
    u8 genre;   /* 1 love, 2 action, 3 epic, 4 comedy, 5 whodunit, 6 culture, 7 live, 8 show, 9 SF, 10 sundry */
    u8 quality;   /* base quality = maximum topicality */
    u8 wear_rate;   /* wear factor: topicality -= quality*wear/5 per airing */
    u8 topicality;   /* current topicality (min 1), drives audience and value */
    u8 blocks;   /* length in hour blocks */
    u8 owner;   /* owning player (0 = market) */
    union {
        struct {
            u16 flags_16;   /* bit5 on the market (set when withdrawn/sold) */
            u32 price;   /* price (i32) */
        };
        struct {
            u8 flags;   /* bit5 on market, bit6 auction pool, bit7 ?; bits 0..4 never read */
            u16 flags2_16;   /* bit1 scheduled from now on, bit2 used up when aired (shows, live; a production's price 0), bit3 X-rated (only hour slot%7 >= 4, i.e. from 21:00) */
        u8 _09;
            u16 price_hi;   /* price (u32, high word) */
        };
        struct {
        u8 _06;
            u8 flags2;   /* bit1 scheduled (not sellable), bit2 used up when aired (shows, live; a production's price 0), bit3 X-rated */
            u16 price_lo;   /* price (u32, low word) */
        };
    };
    u16 jingle;   /* sound id when broadcast (0 = by genre) */
    u8 _0E;
    u16 live_day;   /* day of a live broadcast (genre 7) */
    u16 live_slot;   /* hour slot 1..7 of a live broadcast */
    u8 _13[4];
};
_Static_assert(sizeof(struct film) == 0x17, "struct film");
struct series {   /* series 1..50, 0x39 bytes */
    u8 ep_quality;   /* per episode n: quality bytes at n*3, n*3+1 */
    u8 _01[32];
    u8 genre;   /* 1 family, 2 SF, 3 western, 4 whodunit, 5 sundry (ad/trend genre = 10 + value) */
    u8 episodes;   /* number of episodes; episode e at e*3: +0 quality, +1 topicality, +2 bit0 exists */
    u8 blocks;   /* episode length in hour blocks */
    u8 owner;   /* owning player */
    union {
        struct {
            u16 flags_16;   /* bit5 on the market */
            u32 price;   /* price (i32) */
        };
        struct {
            u8 flags;   /* bit5 on market, bit6 auction pool */
            u16 flags2_16;   /* bit1 scheduled from now on */
        };
        struct {
        u8 _25;
            u8 flags2;   /* bit1 scheduled */
        };
    };
    u16 ep_price_lo;   /* price per episode (u32, low word) */
    u16 ep_price_hi;   /* price per episode (u32, high word) */
    u16 jingle;   /* sound id */
    u16 next_ep;   /* next episode to air (1-based) */
    u8 wear_rate;   /* episode wear: topicality -= quality*wear/20 per airing */
    u8 _34;
    u8 critics;   /* critics rating (x4 = bar length) */
    u8 press;   /* press rating (x4 = bar length) */
    u8 _37[2];
};
_Static_assert(sizeof(struct series) == 0x39, "struct series");
struct screenplay {   /* screenplays / productions 1..50, 0x1B3 bytes */
    u32 price;   /* screenplay price */
    u8 state;   /* ? 0x63 = screenplay thrown away */
    union {
        struct {
            u16 flags_16;   /* bit0 series screenplay, bit2 all episodes share episode block 0 */
        };
        struct {
            u8 flags;   /* bit0 series, bit1 available, bit2 multi-production (max 10) */
            u16 target;   /* film / series produced */
        };
    };
    u8 quality;   /* base quality of episode block 0 (blocks of 0x23 bytes, items at +9+3k type / +0xB+3k bought) */
    u8 _09;
    u8 ep_mult;   /* episode block: cost multiplier for actors/props */
    u8 _0B;
    u8 slot_type;   /* episode slots 1..10 at +8+3*i: +1 type (1-15 role, >=16 scene), +2 weight, +3 chosen item */
    u8 _0D[345];
    u16 duration;   /* production time in hours (0 = 10) */
    u8 _168[59];
    u8 role_category;   /* per role type: actor category 1..7 */
    u8 role_category_2;   /* per role type: actor category (>=8 any) */
    u8 _1A5[14];
};
_Static_assert(sizeof(struct screenplay) == 0x1B3, "struct screenplay");
struct ad {   /* advertisers 1..50, 0x18 bytes */
    u32 name;   /* advertiser name (far text) */
    u32 client;   /* second text (far) */
    u8 days;   /* days allowed to fulfil */
    u8 spots;   /* spots required */
    u8 min_class;   /* minimum audience class (g_ad_min_viewers) */
    u8 target;   /* 3 target genre bytes (negative = excluded) */
    u8 genre_rule;   /* 3 bytes +0xC..0xE: negative = genre excluded, positive = genre required (series 10+g) */
    u8 _0D[2];
    u8 holder;   /* player holding the contract */
    u32 fee;   /* payment */
    u32 penalty;   /* penalty when not fulfilled */
};
_Static_assert(sizeof(struct ad) == 0x18, "struct ad");
struct contract {   /* signed ad contracts [player][1..10], 0x7 bytes */
    u16 ad;   /* ad id (per player stride 0x46) */
    u8 _02[2];
    u8 aired;   /* spots aired so far */
    u16 deadline;   /* last day (today + ad_days when signed) */
};
_Static_assert(sizeof(struct contract) == 0x7, "struct contract");
struct transmitter {   /* transmitters [player][slot]; slot 0 = totals, 0xE bytes */
    u32 fee;   /* transmitter fee */
    u32 reach;   /* viewers in range (entry 0 = total reach of the player; viewers = reach*2464) */
    union {
        struct {
            u32 value;   /* value used for resale bids */
        };
        struct {
            u16 x;   /* map x */
            u16 y;   /* map y */
        };
    };
    u16 state;   /* 1 bought today (unpaid), 2 removed */
};
_Static_assert(sizeof(struct transmitter) == 0xE, "struct transmitter");
struct offer {   /* film dealer shelf 1..10, 0x8 bytes */
    u16 program;   /* film id or series id+1000 */
    u32 price;   /* price */
    u8 _06;
    u8 shelf;   /* >= 0 on the shelf (-100 when taken) */
};
_Static_assert(sizeof(struct offer) == 0x8, "struct offer");
struct lot {   /* auction lots 1..10, 0x9 bytes */
    u16 program;   /* film id or series id+1000 */
    u32 bid;   /* current bid */
    u8 bidder;   /* high bidder (< 0 none / human's provisional bid) */
    u8 seller;   /* player who put it in */
    u8 idle_days;   /* days without a bid (3 = withdrawn) */
};
_Static_assert(sizeof(struct lot) == 0x9, "struct lot");
struct ad_offer {   /* ad agency offers 1..10, 0x7 bytes */
    u16 ad;   /* ad id offered */
    u8 shelf;   /* >= 0 on the shelf (-100 when taken) */
    u8 _03[4];
};
_Static_assert(sizeof(struct ad_offer) == 0x7, "struct ad_offer");
struct script_offer {   /* screenplay agency offers 1..5, 0x9 bytes */
    u16 script;   /* screenplay id */
    u8 status;   /* <0 taken */
    u8 _03;
    u32 price;   /* price */
    u8 aux;   /* ? byte copied to the player's script (+2) */
};
_Static_assert(sizeof(struct script_offer) == 0x9, "struct script_offer");
struct owned_script {   /* screenplays owned [player][0..9], 0x5 bytes */
    u16 script;   /* screenplay id (per player stride 0x32) */
    u8 aux;   /* ? byte from shelf */
    u16 episode;   /* next episode to produce */
};
_Static_assert(sizeof(struct owned_script) == 0x5, "struct owned_script");
struct news_item {   /* today's news items, 0x1E bytes */
    u16 node;   /* chain node (per player inbox/slots: same layout) */
    u16 parts;   /* 5 x (topic, phrase variant) u16 pairs at +2+4k */
    u8 _04[18];
    u16 value;   /* value / quality (aged 85% per hour) */
    u16 hour;   /* hour of the news */
    u16 day;   /* day of the news */
    u8 category;   /* agency/category 1..3 */
    u8 price_class;   /* price class */
};
_Static_assert(sizeof(struct news_item) == 0x1E, "struct news_item");
struct inbox_item {   /* news inbox [player][0..19], 0x1E bytes */
    u16 headline;   /* news text (per player stride 0x258) */
    u8 _02[20];
    u16 quality;   /* news quality */
    u16 hour;   /* hour of the news */
    u16 day;   /* day of the news */
    u8 category;   /* news category / colour */
    u8 price_class;   /* price class (1DA8_3F48) */
};
_Static_assert(sizeof(struct inbox_item) == 0x1E, "struct inbox_item");
struct studio {   /* studios 1..12, 0x10 bytes */
    u16 script;   /* screenplay in production */
    u8 size;   /* studio size 1..3 */
    u8 renter;   /* renting player */
    u8 episode;   /* first/current episode of the production */
    u8 episode_last;   /* last episode of the production */
    u32 rent;   /* rent per day */
    u16 end_day;   /* production end day */
    u16 end_hour;   /* production end hour */
    u8 running;   /* production running */
    u8 reserved;   /* pending: 0 nothing, 0xFF give-up deferred, 1..3 renter queued (never cleared: FINDINGS) */
};
_Static_assert(sizeof(struct studio) == 0x10, "struct studio");
struct ai_budget {   /* computer player budget [player], 0x3C bytes */
    u32 pot0;   /* ?budget pot 0 */
    u32 gifts;   /* Betty gift budget (pot 1) */
    u32 films;   /* film/auction budget (pot 2; 80% of the start money) */
    u32 news;   /* news budget (pot 3) */
    u32 production;   /* screenplays, studio rent, cast (pot 4) */
    u32 transmitters;   /* transmitter budget (pot 5; 49500 at start) */
    u32 other;   /* pot 6 (weight 0) */
    u8 pct;   /* +0x1C bonus, +0x1D..0x22 pot percentages 1..6 */
    u8 _1D[6];
    u8 prio;   /* +0x24..0x29 priority 1..3 per pot (from ai_plan_budget) */
    u8 _24[6];
    u8 max_deals;   /* contracts per ad agency visit / films per shopping plan (= 2 * image rank) */
    u8 image_rank;   /* 1 best .. 3 worst image of the 3 players */
    u32 studio_avg;   /* 3-day average production budget */
    u32 studio_hist;   /* [3] production budget per day (g_ai_period) */
    u8 _34[8];
};
_Static_assert(sizeof(struct ai_budget) == 0x3C, "struct ai_budget");
struct ai_buy {   /* computer player purchase plan [player][slot], 0xB bytes */
    u16 slot;   /* dealer shelf slot or auction lot (per player stride 0x4D) */
    u16 score;   /* hours*topicality*20/(price/1000) */
    u16 program;   /* film id or series id+1000 */
    u32 price;   /* price / planned bid (reserved from the film budget) */
    u8 auction;   /* 1 = auction bid, 0 = dealer purchase */
};
_Static_assert(sizeof(struct ai_buy) == 0xB, "struct ai_buy");
struct ai_ad {   /* computer player booking counters [player][contract], 0x7 bytes */
    u8 today;   /* spots planned per day for today (remaining spots / remaining days) */
    u8 tomorrow;   /* spots planned for tomorrow */
    u8 booked_today;   /* spots booked in slots 1..7 */
    u8 booked_tomorrow;   /* spots booked in slots 8..14 */
    u8 booked_total;   /* spots booked in total */
    u8 _05;
    u8 flags;   /* bit0 today ok, bit1 tomorrow ok, bit2 total >= spots */
};
_Static_assert(sizeof(struct ai_ad) == 0x7, "struct ai_ad");
struct gift {   /* Betty gifts 1..10, 0x8 bytes */
    u8 _00[2];
    u32 price;   /* present price */
    u8 love;   /* love points (decays to 70% after each gift) */
    u8 given;   /* times given */
};
_Static_assert(sizeof(struct gift) == 0x8, "struct gift");
struct shoplist {   /* studio shopping lists [player][0..4], 0xF bytes */
    u16 script;   /* shopping list for screenplay (per player stride 0x4B) */
    u16 episode;   /* episode */
    u8 booked;   /* items already booked to the ledger (+4+i, i 1..10) */
    u8 _05[10];
};
_Static_assert(sizeof(struct shoplist) == 0xF, "struct shoplist");
struct figure {   /* walking figures, 0x1E bytes */
    u16 x;   /* current x */
    u16 floor;   /* current floor */
    u16 target_x;   /* walk target x (0x99 = elevator) */
    u16 target_floor;   /* walk target floor */
    u16 final_x;   /* x to walk to after the elevator ride */
    u8 _0A[2];
    u16 anim;   /* animation frame / facing */
    u16 motion;   /* motion state (3 = in elevator, 5 = entering door) */
    u16 task;   /* nonzero = busy / inside */
    u16 target_door;   /* target door */
    u16 last_door;   /* previous target door */
    u16 lift_phase;   /* elevator boarding phase 0..4 */
    u16 kind;   /* 0/1 NPC walker types, 2 player figure */
    u16 speed;   /* px per step (human 4, computer 3) */
    u16 in_room;   /* inside a room */
};
_Static_assert(sizeof(struct figure) == 0x1E, "struct figure");
struct rating {   /* audience share per player, 0xA bytes */
    u16 share;   /* audience share % */
    u32 viewers;   /* viewers */
    u8 _06[4];
};
_Static_assert(sizeof(struct rating) == 0xA, "struct rating");
struct hotspot {   /* screen hotspots, 0xC bytes */
    u16 x1;   /* hotspot rect (x1,y1,x2,y2 at +0..+6) */
    u8 _02[6];
    u8 room;   /* room id */
    u8 state;   /* required g_room_state (0xFF any) */
    u8 owner_only;   /* only in own rooms */
    u8 id;   /* hotspot id */
};
_Static_assert(sizeof(struct hotspot) == 0xC, "struct hotspot");
struct actor {   /* actor pool [group][actor], 0xC bytes */
    u16 name;   /* actor name id (role stride 0xC0) */
    u32 fee;   /* fee per block */
    u8 _06[4];
    u16 icon;   /* portrait sprite */
};
_Static_assert(sizeof(struct actor) == 0xC, "struct actor");
struct message {   /* office messages, 0x1A bytes */
    u16 text_id;   /* message text id (0x10E production complete, 0x10F damages, 0x110 cancelled, 0x113 missed live, 0x133/0x136 studio lost, 0x152, 0x158/0x159 Sammy, 0x15B confiscated, 0x15F Trendy) */
    u16 arg;   /* first argument (player, studio, ad...) */
    u8 _04[22];
};
_Static_assert(sizeof(struct message) == 0x1A, "struct message");
struct schedule {   /* programme schedule per player: slots 1..14 = today and tomorrow 18:00-24:00, 0x5A bytes */
    u16 program[15];   /* film id, series id + 1000, 9999 = continuation of the previous slot */
    u16 episode[15];   /* series episode aired in the slot */
    u16 ad[15];   /* ad contract booked after the programme (+2000 aired, +3000 planned) */
};
_Static_assert(sizeof(struct schedule) == 0x5A, "struct schedule");
struct library {   /* film and series library per player, 0x6C bytes */
    u16 items[12][4];   /* programme ids per genre 1..11 (11 = series), 4 slots each */
    u8 count[12];   /* number of programmes per genre */
};
_Static_assert(sizeof(struct library) == 0x6C, "struct library");
struct floor {   /* building floors 0..13, up to 10 doors each, 0x7A bytes */
    u8 door_x[10];   /* door x position / 8 */
    u8 door_y_ofs[10];
    u8 door_kind[10];   /* 1..3 rooms, 6 information board, 10 porter */
    u8 door_room[10];   /* room type */
    u8 door_name[10];   /* sign text id */
    u8 door_name_alt[10];
    u8 door_open[10];
    u8 door_owner[10];   /* owning player */
    u8 door_tip_w[10];
    u8 door_tip_h[10];
    u8 door_icon[10];
    u8 door_studio[10];   /* studio number */
    u16 wall_style;
};
_Static_assert(sizeof(struct floor) == 0x7A, "struct floor");
struct story {   /* running news stories 0..6, 0x15 bytes */
    u16 template;   /* news template */
    u16 last_template;
    u16 node;   /* current chain node */
    u16 variant[5];   /* phrase variant per template part */
    u8 _10[2];
    u16 countdown;   /* hours until the next node */
    u8 category;
};
_Static_assert(sizeof(struct story) == 0x15, "struct story");
#pragma pack(pop)
/* a far pointer to a table element: AT(struct, p).field */
#define AT(T, p) (*(struct T *)host_addr(FPSEG(p), FPOFF(p)))
#define players_at(i) MK32(DSEG, (u16)((i) * 0x17 + 0x8BDC))
#define films_at(i) MK32(FPSEG(G32(0x24A9)), (u16)(FPOFF(G32(0x24A9)) + (i) * 0x17))
#define series_at(i) MK32(FPSEG(G32(0x2646)), (u16)(FPOFF(G32(0x2646)) + (i) * 0x39))
#define screenplays_at(i) MK32(FPSEG(G32(0x715C)), (u16)(FPOFF(G32(0x715C)) + (i) * 0x1B3))
/* ads 1..50 are a DGROUP array (DS:3F4F); a mod's ads 51.. live in a host array
 * (src/state.c) behind the far-pointer segment ADS_EXTRA_SEG (an extension) */
#define ADS_MAX 999                      /* ad ids < 1000: the schedule marks aired spots +1000/+2000/+3000 */
#define ADS_EXTRA_SEG 0xE800
extern u16 g_nads;                       /* 50, or a mod's ad count (state.c) */
static inline u32 ads_at_id(u16 i) {     /* any other id (a marked schedule slot): the original's address */
    return i <= 50 || i > g_nads ? MK32(DSEG, (u16)(i * 0x18 + 0x3F4F)) : MK32(ADS_EXTRA_SEG, (u16)((i - 51) * 0x18));
}
#define ads_at(i) ads_at_id((u16)(i))
#define contracts_at(i, j) MK32(DSEG, (u16)((i) * 0x46 + (j) * 0x7 + 0x1B70))
#define tx_at(i, j) MK32(DSEG, (u16)((i) * 0x1CE + (j) * 0xE + 0x1D09))
/* the film dealer's slots 1..10 are a DGROUP array (DS:19ED, the players'
 * screenplays follow at 1A45); with --film-offers (src/film_offers.c) slots
 * 11..g_nfilmoffers live in a host array (src/state.c) behind the far-pointer
 * segment FILM_OFFERS_SEG (runtime.h).  Any other index keeps the original's
 * address. */
#define FILM_OFFERS_MAX FILM_OFFERS_OPT_MAX
extern u16 g_nfilmoffers;                /* 10, or the option's 11..17 (state.c) */
extern u8 g_dealer_extra[];
extern u16 g_filmshelf_extra[];              /* their shelf rectangles (src/film_offers.c) */
#define FILM_OFFER_EXTRA(i) ((u16)(i) > 10 && (u16)(i) <= g_nfilmoffers)
static inline u32 dealer_at_id(u16 i) {
    return FILM_OFFER_EXTRA(i) ? MK32(FILM_OFFERS_SEG, (u16)((i - 11) * 0x8)) : MK32(DSEG, (u16)(i * 0x8 + 0x19ED));
}
#define dealer_at(i) dealer_at_id((u16)(i))
#define auction_at(i) MK32(DSEG, (u16)((i) * 0x9 + 0x1B0D))
/* the ad agency's offers 1..10 are a DGROUP array (DS:19A0, the film dealer's
 * slots follow at 19ED); with --ad-offers (an option, src/fixes.c) offers
 * 11..g_nadoffers live in a host array (src/state.c) behind the far-pointer
 * segment AD_OFFERS_SEG (runtime.h).  Any other index (0, or one past the last
 * a search loop runs into) keeps the original's address. */
#define AD_OFFERS_MAX AD_OFFERS_OPT_MAX
extern u16 g_nadoffers;                  /* 10, or the option's 11..17 (state.c) */
extern u8 g_ad_offers_extra[];
#define AD_OFFER_EXTRA(i) ((u16)(i) > 10 && (u16)(i) <= g_nadoffers)
static inline u32 ad_offers_at_id(u16 i) {
    return AD_OFFER_EXTRA(i) ? MK32(AD_OFFERS_SEG, (u16)((i - 11) * 0x7)) : MK32(DSEG, (u16)(i * 0x7 + 0x19A0));
}
#define ad_offers_at(i) ad_offers_at_id((u16)(i))
#define script_offers_at(i) MK32(DSEG, (u16)((i) * 0x9 + 0x196A))
#define owned_scripts_at(i, j) MK32(DSEG, (u16)((i) * 0x32 + (j) * 0x5 + 0x1A45))
#define news_pool_at(i) MK32(DSEG, (u16)((i) * 0x1E + 0x36DB))
#define inbox_at(i, j) MK32(DSEG, (u16)((i) * 0x258 + (j) * 0x1E + 0x457F))
#define studios_at(i) MK32(DSEG, (u16)((i) * 0x10 + 0x6165))
#define ai_at(i) MK32(DSEG, (u16)((i) * 0x3C + 0x6E29))
#define ai_buy_plan_at(i, j) MK32(DSEG, (u16)((i) * 0x4D + (j) * 0xB + 0x2512))
#define ai_ad_plan_at(i, j) MK32(DSEG, (u16)((i) * 0x4D + (j) * 0x7 + 0x4EDF))
#define shoplists_at(i, j) MK32(DSEG, (u16)((i) * 0x4B + (j) * 0xF + 0x734C))
#define figures_at(i) MK32(DSEG, (u16)((i) * 0x1E + 0x7B42))
#define hotspots_at(i) MK32(DSEG, (u16)((i) * 0xC + 0x0638))
#define actors_at(i, j) MK32(DSEG, (u16)((i) * 0xC0 + (j) * 0xC + 0x5013))
#define messages_at(i) MK32(DSEG, (u16)((i) * 0x1A + 0x60BF))
#define schedule_at(i) MK32(DSEG, (u16)((i) * 0x5A + 0x4417))
#define library_at(i) MK32(DSEG, (u16)((i) * 0x6C + 0x6F19))
#define floors_at(i) MK32(DSEG, (u16)((i) * 0x7A + 0x7C82))
#define stories_at(i) MK32(DSEG, (u16)((i) * 0x15 + 0x70C9))
/* array fields: table_field(i, k) computes the 16-bit offset like the original */
#define schedule_ad(i, k0) G16((u16)((i) * 0x5A + (k0) * 0x2 + 0x4453))
#define schedule_episode(i, k0) G16((u16)((i) * 0x5A + (k0) * 0x2 + 0x4435))
#define schedule_program(i, k0) G16((u16)((i) * 0x5A + (k0) * 0x2 + 0x4417))
#define library_count(i, k0) G8((u16)((i) * 0x6C + (k0) * 0x1 + 0x6F79))
#define library_items(i, k0, k1) G16((u16)((i) * 0x6C + (k0) * 0x8 + (k1) * 0x2 + 0x6F19))
#define floors_door_kind(i, k0) G8((u16)((i) * 0x7A + (k0) * 0x1 + 0x7C96))
#define floors_door_name(i, k0) G8((u16)((i) * 0x7A + (k0) * 0x1 + 0x7CAA))
#define floors_door_open(i, k0) G8((u16)((i) * 0x7A + (k0) * 0x1 + 0x7CBE))
#define floors_door_owner(i, k0) G8((u16)((i) * 0x7A + (k0) * 0x1 + 0x7CC8))
#define floors_door_room(i, k0) G8((u16)((i) * 0x7A + (k0) * 0x1 + 0x7CA0))
#define floors_door_studio(i, k0) G8((u16)((i) * 0x7A + (k0) * 0x1 + 0x7CF0))
#define floors_door_x(i, k0) G8((u16)((i) * 0x7A + (k0) * 0x1 + 0x7C82))
#define players(i) (*(struct player *)dg_addr((u16)((i) * 0x17 + 0x8BDC)))
#define films(i) (*(struct film *)host_addr(FPSEG(G32(0x24A9)), (u16)(FPOFF(G32(0x24A9)) + (i) * 0x17)))
#define series(i) (*(struct series *)host_addr(FPSEG(G32(0x2646)), (u16)(FPOFF(G32(0x2646)) + (i) * 0x39)))
#define screenplays(i) (*(struct screenplay *)host_addr(FPSEG(G32(0x715C)), (u16)(FPOFF(G32(0x715C)) + (i) * 0x1B3)))
#define ads(i) (*(struct ad *)HPTR(ads_at(i)))
#define contracts(i, j) (*(struct contract *)dg_addr((u16)((i) * 0x46 + (j) * 0x7 + 0x1B70)))
#define tx(i, j) (*(struct transmitter *)dg_addr((u16)((i) * 0x1CE + (j) * 0xE + 0x1D09)))
#define dealer(i) (*(struct offer *)(FILM_OFFER_EXTRA(i) ? g_dealer_extra + ((u16)(i) - 11) * 0x8 : dg_addr((u16)((i) * 0x8 + 0x19ED))))
#define auction(i) (*(struct lot *)dg_addr((u16)((i) * 0x9 + 0x1B0D)))
#define ad_offers(i) (*(struct ad_offer *)(AD_OFFER_EXTRA(i) ? g_ad_offers_extra + ((u16)(i) - 11) * 0x7 : dg_addr((u16)((i) * 0x7 + 0x19A0))))
#define script_offers(i) (*(struct script_offer *)dg_addr((u16)((i) * 0x9 + 0x196A)))
#define owned_scripts(i, j) (*(struct owned_script *)dg_addr((u16)((i) * 0x32 + (j) * 0x5 + 0x1A45)))
#define news_pool(i) (*(struct news_item *)dg_addr((u16)((i) * 0x1E + 0x36DB)))
#define inbox(i, j) (*(struct inbox_item *)dg_addr((u16)((i) * 0x258 + (j) * 0x1E + 0x457F)))
#define studios(i) (*(struct studio *)dg_addr((u16)((i) * 0x10 + 0x6165)))
#define ai(i) (*(struct ai_budget *)dg_addr((u16)((i) * 0x3C + 0x6E29)))
#define ai_buy_plan(i, j) (*(struct ai_buy *)dg_addr((u16)((i) * 0x4D + (j) * 0xB + 0x2512)))
#define ai_ad_plan(i, j) (*(struct ai_ad *)dg_addr((u16)((i) * 0x4D + (j) * 0x7 + 0x4EDF)))
#define gifts(i) (*(struct gift *)dg_addr((u16)((i) * 0x8 + 0x724A)))
#define shoplists(i, j) (*(struct shoplist *)dg_addr((u16)((i) * 0x4B + (j) * 0xF + 0x734C)))
#define figures(i) (*(struct figure *)dg_addr((u16)((i) * 0x1E + 0x7B42)))
#define ratings(i) (*(struct rating *)dg_addr((u16)((i) * 0xA + 0x24AD)))
#define hotspots(i) (*(struct hotspot *)dg_addr((u16)((i) * 0xC + 0x0638)))
#define actors(i, j) (*(struct actor *)dg_addr((u16)((i) * 0xC0 + (j) * 0xC + 0x5013)))
#define messages(i) (*(struct message *)dg_addr((u16)((i) * 0x1A + 0x60BF)))
#define floors(i) (*(struct floor *)dg_addr((u16)((i) * 0x7A + 0x7C82)))
#define stories(i) (*(struct story *)dg_addr((u16)((i) * 0x15 + 0x70C9)))

#endif
