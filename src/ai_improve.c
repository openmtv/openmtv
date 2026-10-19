/* The AI improvement, part 1: the computer players follow the human's
 * transmitter network (src/ai_improve.h; the plan and its reasons:
 * docs/ki-verbesserung.md).
 *
 * Every computer player has a reference reach B (made at its first step:
 * the strongest human's reach then).  Once the strongest human's reach is
 * 10 % or more above B, the computer player notices it: it remembers that
 * reach S, draws a wait (played hours: a day is 17:00-01:00, eight hours)
 * and a factor F, and ignores further expansions meanwhile.  When the wait
 * is over, the target is Z = S x F and the gap L = Z - its own reach; under
 * a million viewers it buys nothing, else at its next office visit up to
 * one (normal) or two (strong) transmitters by the site rule below, paid
 * from the cash (which may go negative); their daily fees do not touch the
 * pots for films, news and productions -- the transmitter pot pays half of
 * them, as the original's for every transmitter (else the network's higher
 * income filled it untouched, and the purchases from it ran far ahead of the
 * human: measured with tools/ai_run.py --human-plan).  If the gap is still
 * more than 15 % of Z after them, it follows up: a new wait, half one of
 * the level's, then the same target again (and so on, until the gap is
 * smaller or there is nothing left to buy) -- a satellite of the human's
 * leaves a gap two transmitters do not close.  Then B = S, and it looks
 * again (a staircase: one step behind the human).  Its own purchases from
 * the transmitter pot go on as in the original.
 *
 * The satellites (slots 1 and 2: the whole west / east half of the map,
 * $2.5M / $4.5M): the original's computer players never buy one.  Here a
 * computer player may own as many as the strongest human has paid for
 * (strong), or one fewer (normal); an allowed one is one more site for the
 * rule below, its N the half's viewers it does not reach yet, P its price
 * -- only if N <= 1.3 L or it stays at most at the human's S with it (never
 * far past the target, never past the human).  Such a satellite above 1.3 L
 * is no "point above 1.3 L" of rule 2: rule 3 takes it.
 *
 * The site rule: every one of the 1 500 map points (the original's grid),
 * 30 a step instead of 3; for each its new viewers N and price class P (of
 * all viewers in the circle -- overlap with the own network costs too):
 *   1. points with 0.8 L <= N <= 1.3 L: the most new viewers per dollar;
 *   2. else, where some bring more than 1.3 L: normal the smallest N above
 *      1.3 L, strong the most per dollar among them;
 *   3. else (none reaches 0.8 L): the most per dollar among the points with
 *      at least half the largest N.
 * A tie: the point found first.
 *
 * A hidden personality per computer player, drawn with its state: cautious
 * (waits from the upper half of the range, factors from the lower) or
 * ambitious (the other way round).  Nothing in the game shows it; the
 * diagnostics ("[ai-improve]" lines on stderr: the launcher's game.log, the
 * test runs' log.txt) name it.
 *
 * Everything depends on the game's state only (the draws through random_n
 * like the rest of the game, mixed with the played time), so every machine of a network game computes
 * the same; the state goes into savegames.  F is drawn with the wait when
 * the expansion is noticed (not when the wait ends), so a savegame made
 * during the wait holds the whole reaction. */
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "seat.h"
#include "ai_improve.h"

#define POINTS 1500                      /* the map's grid: x 0..147, y 0..87, step 3 (ai_scan_transmitter_site) */
#define SATS 2                           /* the satellites, slots 1 and 2: sites POINTS and POINTS + 1 of the rule */
#define FOLLOW_PCT 15                    /* a gap above this share of the target after a reaction: a follow-up */
#define PER_STEP 30                      /* points a step (the original: 3) */
#define MIN_GAP 1000000u                 /* viewers: a smaller gap buys nothing */
#define VIEWERS 2464u                    /* viewers per unit of reach */

typedef struct { u16 wait_lo, wait_hi, f_lo, f_hi; u8 masts; } Level;
static const Level level_of[3] = {
    { 0, 0, 0, 0, 0 },
    { 8 * 60, 20 * 60, 500, 800, 1 },    /* normal: 8-20 played hours, F 0.50-0.80, one transmitter */
    { 4 * 60, 12 * 60, 800, 1020, 2 },   /* strong: 4-12 hours, F 0.80-1.02, up to two */
    /* (the plan's first ranges, 0.75-1.00 and 0.90-1.15, measured with tools/ai_run.py
     * --human-plan: the computer players came out above F x the human's reach -- the site
     * rule buys up to 130 % of the gap, the pot buys on top, the more the stronger the
     * station's own business -- and the stronger one ahead on 25 % / 93 % of the days) */
};

typedef struct {
    u8 init;          /* made: at its first step after a new game or a load */
    u8 ambitious;     /* the hidden personality: 0 cautious, 1 ambitious */
    u8 waiting;       /* noticed an expansion, waits until due */
    u8 pending;       /* transmitters still to buy in the running reaction */
    u16 factor;       /* F in thousandths */
    u32 ref;          /* B */
    u32 seen;         /* S */
    u32 noticed;      /* when it noticed (played minutes) */
    u32 due;          /* when the reaction is due */
    u32 target;       /* Z */
    u32 follow;       /* bit k: slot 3 + k was bought by following; bits 30, 31: the satellites (follow_bit) */
} Imp;
static Imp imp[4];                       /* by player 1..3; only the computer players' are used */

/* the follow-up search: not saved, it starts anew with every visit to the
 * office (the rebuild of the coverage map) and after a load.  It measures
 * new viewers against a coverage map of the player's own, made when it
 * starts: the game has one (g_coverage_map) for everybody -- in a network
 * game another human's transmitter map may use it between two steps -- so
 * every step puts the player's own in its place and the shared one back. */
#define MAP_BYTES 0x11E2                 /* g_coverage_map (mem_zero in coverage_build) */
static struct {
    u16 pos;                             /* the next point */
    u32 n[POINTS + SATS], p[POINTS + SATS];   /* new viewers (reach units) and price class of every point (and satellite) */
    u8 map[MAP_BYTES];                   /* the player's coverage map */
} scan[4];
static struct { u8 map[MAP_BYTES]; u32 west, east; } shared;   /* the shared map, aside meanwhile */
static void shared_aside(void) {
    memcpy(shared.map, HPTR(g_coverage_map), MAP_BYTES);
    shared.west = g_coverage_west;
    shared.east = g_coverage_east;
}
static void shared_back(void) {
    memcpy(HPTR(g_coverage_map), shared.map, MAP_BYTES);
    g_coverage_west = shared.west;
    g_coverage_east = shared.east;
}

int ai_improve_on(void) { return ai_improve_opt && !difftest_on; }

static int lvl(void) { return ai_improve_opt >= 2 ? 2 : 1; }

/* played minutes since the game began, by the game's clock (17:00-01:00: g_time_slot's hour,
 * g_minute; g_clock_acc holds the time only up to the clock's last stop) */
static u32 played(void) {
    i16 slot = (i16)g_time_slot, h = slot < 17 ? slot + 7 : slot - 17;   /* (clock_hour) */
    return (u32)g_day * 480 + (u32)(h * 60 + (i16)g_minute);
}

static const char *when(u32 m, char *b) {
    unsigned h = 17 + (m % 480) / 60;
    sprintf(b, "day %u %02u:%02u", (unsigned)(m / 480), h % 24, (unsigned)(m % 60));
    return b;
}
static double mio(u32 reach) { return reach * (double)VIEWERS / 1e6; }
static const char *pers(const Imp *s) { return s->ambitious ? "ambitious" : "cautious"; }

/* the strongest human still in the game: player number (0: none) and reach; *busy
 * if a human has transmitters bought on the map and not yet paid (state 1: on the
 * map now -- leaving it pays them, tx_commit_day -- and free to take them back) */
static int strongest_human(u32 *reach, int *busy) {
    int who = 0;
    *busy = 0;
    for (int p = 1; p <= 3; p++) {
        if (!mp_is_human(p) || players(p).out) continue;
        if (!who || tx(p, 0).reach > *reach) { who = p; *reach = tx(p, 0).reach; }
        for (int k = 1; k <= 32; k++)
            if (tx(p, k).state == 1 && tx(p, k).reach != 0) *busy = 1;
    }
    return who;
}

/* the satellites a player has: of a human only those paid for (state 0), of a computer player
 * every one (a reach, not scrapped -- coverage_build's test) */
static int sats_of(u16 p, int paid) {
    int c = 0;
    for (int k = 1; k <= SATS; k++)
        if (tx(p, k).reach != 0 && tx(p, k).state != 2 && (!paid || tx(p, k).state == 0)) c++;
    return c;
}
/* how many more satellites the computer player may buy: the strongest human's, strong;
 * one fewer, normal */
static int sats_wanted(u16 player) {
    u32 h = 0;
    int busy, who = strongest_human(&h, &busy);
    int n = who ? sats_of((u16)who, 1) - (lvl() == 2 ? 0 : 1) : 0;
    return n - sats_of(player, 0);
}
/* a satellite's reach: its half's viewers, as the transmitter map's buttons set it (room_txmap_click) */
static u32 sat_reach(int k) {
    return map_sum_population((u16)((k - 1) * 0x54), 0, (u16)((k - 1) * 0x54 + 0x53), 0x6D, g_population_map);
}
/* the new viewers satellite k would bring the player: its reach with it, less without */
static u32 sat_new(u16 player, int k) {
    u32 keep = tx(player, k).reach, before, after;
    u8 state = tx(player, k).state;
    transmitters_recalc(player);
    before = tx(player, 0).reach;
    tx(player, k).reach = sat_reach(k);
    tx(player, k).state = 0;
    transmitters_recalc(player);
    after = tx(player, 0).reach;
    tx(player, k).reach = keep;
    tx(player, k).state = state;
    transmitters_recalc(player);
    return after > before ? after - before : 0;
}
static u32 follow_bit(int k) { return 1u << (k >= 3 ? k - 3 : 29 + k); }

/* a draw of 0..n-1: random_n's number (the game's: seeded from the timer)
 * mixed with the played time and the player -- random_n alone repeats
 * itself when called at the same moment of the timer, e.g. at the same hour
 * of every day */
static u16 roll(u16 n, u16 player, u16 salt) {
    u64 z = (u64)random_n(0x7FFF, salt) << 32 ^ (u64)played() << 8 ^ (u64)player << 4 ^ salt;
    z += 0x9E3779B97F4A7C15ull;          /* splitmix64's finaliser */
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    z ^= z >> 31;
    return (u16)(z % n);
}
/* a draw of lo..hi, from the lower or the upper half of the range */
static u16 draw(u16 lo, u16 hi, int upper, u16 player, u16 salt) {
    u16 mid = (u16)((lo + hi) / 2);
    if (upper) lo = mid; else hi = mid;
    return (u16)(lo + roll((u16)(hi - lo + 1), player, salt));
}

static u32 gap_of(u16 player) {
    u32 own = tx(player, 0).reach;
    return imp[player].target > own ? imp[player].target - own : 0;
}
static int gap_small(u32 gap) { return (u64)gap * VIEWERS < MIN_GAP; }
static int free_slot(u16 player) {
    for (int k = 3; k <= 32; k++)
        if ((i32)tx(player, k).reach <= 0) return k;
    return 0;
}

/* the reaction is over: the remembered reach becomes the reference */
static void finish(u16 player, const char *why) {
    Imp *s = &imp[player];
    char b[32];
    fprintf(stderr, "[ai-improve] %s player %u: reaction over (%s), reference %.1f M\n", when(played(), b), player, why, mio(s->seen));
    s->pending = 0;
    s->ref = s->seen;
}

/* after the reaction's purchases: a gap still above FOLLOW_PCT % of the target is followed
 * up -- a new wait (half a drawn one), then the same target again; 0 if not */
static int follow_up(u16 player) {
    Imp *s = &imp[player];
    u32 gap = gap_of(player);
    char b[32], b2[32];
    if (gap_small(gap) || (u64)gap * 100 <= (u64)s->target * FOLLOW_PCT) return 0;
    const Level *l = &level_of[lvl()];
    u16 wait = (u16)(draw(l->wait_lo, l->wait_hi, !s->ambitious, player, 0x44) / 2);
    s->pending = 0;
    s->waiting = 1;
    s->due = played() + wait;
    fprintf(stderr, "[ai-improve] %s player %u: gap still %.1f M (%.0f %% of the target), follows up after %u:%02u h at %s\n",
            when(played(), b), player, mio(gap), 100.0 * gap / s->target, wait / 60, wait % 60, when(s->due, b2));
    return 1;
}

void ai_improve_step(u16 player) {
    if (!ai_improve_on() || player < 1 || player > 3) return;
    Imp *s = &imp[player];
    u32 h = 0, now = played();
    int busy;
    char b[32], b2[32];
    if (!strongest_human(&h, &busy)) return;
    if (!s->init) {
        memset(s, 0, sizeof *s);
        s->init = 1;
        s->ambitious = roll(2, player, 0x41) == 1;
        s->ref = h;
        fprintf(stderr, "[ai-improve] %s player %u: %s, level %s, reference %.1f M\n", when(now, b), player, pers(s),
                lvl() == 2 ? "strong" : "normal", mio(h));
        return;
    }
    if (s->pending) return;              /* a reaction runs: its purchases at the next office visit */
    if (s->waiting) {
        if (now < s->due) return;
        s->waiting = 0;
        s->target = (u32)((u64)s->seen * s->factor / 1000);
        u32 gap = gap_of(player);
        fprintf(stderr, "[ai-improve] %s player %u: due, target %.1f M (%.2f x %.1f M), own %.1f M, gap %.1f M\n", when(now, b), player,
                mio(s->target), s->factor / 1000.0, mio(s->seen), mio(tx(player, 0).reach), mio(gap));
        if (gap_small(gap)) finish(player, "gap under a million");
        else s->pending = level_of[lvl()].masts;
        return;
    }
    if (!busy && (u64)h * 10 >= (u64)s->ref * 11) {
        const Level *l = &level_of[lvl()];
        u16 wait = draw(l->wait_lo, l->wait_hi, !s->ambitious, player, 0x42);
        s->factor = draw(l->f_lo, l->f_hi, s->ambitious, player, 0x43);
        s->seen = h;
        s->noticed = now;
        s->due = now + wait;
        s->waiting = 1;
        fprintf(stderr, "[ai-improve] %s player %u (%s): noticed %.1f M (reference %.1f M), waits %u:%02u h until %s, factor %.3f\n",
                when(now, b), player, pers(s), mio(h), mio(s->ref), wait / 60, wait % 60, when(s->due, b2), s->factor / 1000.0);
    }
}

void ai_improve_rebuild(u16 player) {
    if (!ai_improve_on() || player > 3) return;
    scan[player].pos = 0;
}

int ai_improve_wants(u16 player) {
    if (!ai_improve_on() || player > 3 || !imp[player].pending) return 0;
    if (gap_small(gap_of(player))) { finish(player, "gap closed"); return 0; }
    if (!free_slot(player) && sats_wanted(player) <= 0) { finish(player, "no free slot"); return 0; }
    return 1;
}

/* point i beats point j by new viewers per dollar (a tie keeps j, the one found first) */
static int better(u16 player, int i, int j) {
    return j < 0 || (u64)scan[player].n[i] * scan[player].p[j] > (u64)scan[player].n[j] * scan[player].p[i];
}

static int choose(u16 player, u32 gap, int *rule) {
    const u32 *n = scan[player].n;
    int best = -1, over = 0;
    u32 maxn = 0;
    for (int i = 0; i < POINTS + SATS; i++) {   /* (a satellite is never "over": rule 2 takes points only) */
        if (!n[i]) continue;
        u64 t = (u64)n[i] * 10;
        if (t >= (u64)gap * 8 && t <= (u64)gap * 13 && better(player, i, best)) best = i;
        if (t > (u64)gap * 13 && i < POINTS) over = 1;
        if (n[i] > maxn) maxn = n[i];
    }
    if (best >= 0) { *rule = 1; return best; }
    if (over) {
        *rule = 2;
        for (int i = 0; i < POINTS; i++) {
            if ((u64)n[i] * 10 <= (u64)gap * 13) continue;
            if (lvl() == 2 ? better(player, i, best) : (best < 0 || n[i] < n[best])) best = i;
        }
        return best;
    }
    *rule = 3;
    for (int i = 0; i < POINTS + SATS; i++)
        if (n[i] && (u64)n[i] * 2 >= maxn && better(player, i, best)) best = i;
    return best;
}

int ai_improve_scan(u16 player) {
    if (!ai_improve_on() || player > 3 || !imp[player].pending) return 0;
    Imp *s = &imp[player];
    char b[32];
    shared_aside();
    if (scan[player].pos == 0) {         /* the player's own coverage map (and its reach, fresh) */
        transmitters_recalc(player);
        memcpy(scan[player].map, HPTR(g_coverage_map), MAP_BYTES);
    } else
        memcpy(HPTR(g_coverage_map), scan[player].map, MAP_BYTES);
    for (int k = 0; k < PER_STEP && scan[player].pos < POINTS; k++, scan[player].pos++) {
        u16 i = scan[player].pos, x = (u16)(i % 50 * 3), y = (u16)(i / 50 * 3);
        u32 n = transmitter_new_reach(x, y);
        scan[player].n[i] = (i32)n > 0 ? n : 0;
        scan[player].p[i] = 0;
        if ((i32)n > 0) {
            u32 v = (u32)((i32)((u32)(transmitter_reach(x, y) * 0x268)) / (i32)0xA);
            scan[player].p[i] = transmitter_fee_bracket(LO16(v), HI16(v));
        }
    }
    if (scan[player].pos < POINTS) { shared_back(); return 1; }
    u32 gap = gap_of(player);
    int want = sats_wanted(player);
    for (int k = 1; k <= SATS; k++) {    /* the satellites it may buy: two more sites */
        u32 n = 0;
        if (want > 0 && !(tx(player, k).reach != 0 && tx(player, k).state != 2)) n = sat_new(player, k);
        u32 own = tx(player, 0).reach;    /* (sat_new leaves it fresh) */
        scan[player].n[POINTS + k - 1] = (u64)n * 10 <= (u64)gap * 13 || own + n <= s->seen ? n : 0;
        scan[player].p[POINTS + k - 1] = tx(player, k).fee;
    }
    int rule = 0, i = choose(player, gap, &rule), si = free_slot(player);
    if (i < 0 || (i < POINTS && !si)) {
        finish(player, i < 0 ? "no site with new viewers" : "no free slot");
    } else {
        u32 n = scan[player].n[i];
        if (i >= POINTS) {               /* a satellite: as the transmitter map's button (room_txmap_click) */
            si = i - POINTS + 1;
            tx(player, si).reach = sat_reach(si);
            tx(player, si).state = 1;
            players(player).money = players(player).money - tx(player, si).fee;
            transmitters_recalc(player);
        } else {
            tx(player, si).reach = n;
            tx(player, si).fee = round_to(scan[player].p[i], 0x3E8);
            tx(player, si).x = (u16)(i % 50 * 3);
            tx(player, si).y = (u16)(i / 50 * 3);
            tx(player, si).state = 1;
            players(player).money = players(player).money - tx(player, si).fee;
            coverage_stamp((u16)si, player, g_population_map, g_coverage_map);
            transmitters_recalc(player);
        }
        s->follow |= follow_bit(si);
        memset(dg_addr((u16)(0x2443 + player * 16)), 0, 16);   /* the pot's search (g_ai_tx_scan) starts anew too */
        s->pending--;
        char site[32];
        if (si <= SATS) sprintf(site, "the %s satellite", si == 1 ? "west" : "east");
        else sprintf(site, "%u,%u", tx(player, si).x, tx(player, si).y);
        fprintf(stderr, "[ai-improve] %s player %u: bought %s rule %d, new %.2f M for a gap of %.2f M (%.0f %%), price %lu, cash %ld, reach %.1f M\n",
                when(played(), b), player, site, rule, mio(n), mio(gap), gap ? 100.0 * n / gap : 0.0,
                (unsigned long)tx(player, si).fee, (long)(i32)players(player).money, mio(tx(player, 0).reach));
        if (s->pending && !gap_small(gap_of(player)) && (free_slot(player) || sats_wanted(player) > 0)) {
            scan[player].pos = 0;        /* the second: the points again, against the map with the first */
            shared_back();
            return 1;
        }
        if (s->pending || !follow_up(player)) finish(player, s->pending ? "gap closed" : "bought");
    }
    shared_back();
    g_ai_tx_busy(player) = 0;
    ai_next_goal();
    return 1;
}

u32 ai_improve_follow_fees(u16 player) {
    if (!ai_improve_on() || player > 3) return 0;
    u32 sum = 0;
    for (int k = 1; k <= 32; k++)
        if ((imp[player].follow & follow_bit(k)) && tx(player, k).reach != 0) sum += tx(player, k).fee;
    return sum;
}

/* the savegame's part: u8 version, u8 level, then players 1..3: u8 flags
 * (1 made, 2 ambitious, 4 waiting), u8 pending, u16 factor, u32 reference,
 * seen, noticed, due, target, follow (little-endian) */
#define SAVE_VERSION 1
#define REC 28
_Static_assert(2 + 3 * REC <= 256, "the savegame's buffers for the state (src/actions.c) hold 256 bytes");
u16 ai_improve_save_size(void) { return ai_improve_on() ? 2 + 3 * REC : 0; }

static void put16(u8 *p, u16 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); }
static void put32(u8 *p, u32 v) { put16(p, (u16)v); put16(p + 2, (u16)(v >> 16)); }
static u16 get16(const u8 *p) { return (u16)(p[0] | p[1] << 8); }
static u32 get32(const u8 *p) { return get16(p) | (u32)get16(p + 2) << 16; }

/* a computer player's state as the diagnostics name it (the tests compare a save's with the load's) */
static void state_line(const char *what) {
    char b[32];
    for (int p = 1; p <= 3; p++) {
        const Imp *s = &imp[p];
        if (!s->init) continue;
        fprintf(stderr, "[ai-improve] %s %s player %d: %s, reference %lu, waiting %d, seen %lu, due %lu, factor %u, pending %d, target %lu, follow %08lx\n",
                when(played(), b), what, p, pers(s), (unsigned long)s->ref, s->waiting, (unsigned long)s->seen, (unsigned long)s->due,
                s->factor, s->pending, (unsigned long)s->target, (unsigned long)s->follow);
    }
}

void ai_improve_save(u8 *buf) {
    state_line("saved");
    buf[0] = SAVE_VERSION;
    buf[1] = (u8)ai_improve_opt;
    for (int p = 1; p <= 3; p++) {
        const Imp *s = &imp[p];
        u8 *r = buf + 2 + (p - 1) * REC;
        r[0] = (u8)(s->init | s->ambitious << 1 | s->waiting << 2);
        r[1] = s->pending;
        put16(r + 2, s->factor);
        put32(r + 4, s->ref); put32(r + 8, s->seen); put32(r + 12, s->noticed);
        put32(r + 16, s->due); put32(r + 20, s->target); put32(r + 24, s->follow);
    }
}

void ai_improve_load(const u8 *buf, u16 len) {
    memset(imp, 0, sizeof imp);
    for (int p = 0; p < 4; p++) scan[p].pos = 0;
    if (!buf || len < 2 + 3 * REC || buf[0] != SAVE_VERSION) return;
    for (int p = 1; p <= 3; p++) {
        Imp *s = &imp[p];
        const u8 *r = buf + 2 + (p - 1) * REC;
        s->init = r[0] & 1; s->ambitious = r[0] >> 1 & 1; s->waiting = r[0] >> 2 & 1;
        s->pending = r[1] > level_of[lvl()].masts ? level_of[lvl()].masts : r[1];   /* (saved at another level) */
        s->factor = get16(r + 2);
        s->ref = get32(r + 4); s->seen = get32(r + 8); s->noticed = get32(r + 12);
        s->due = get32(r + 16); s->target = get32(r + 20); s->follow = get32(r + 24);
    }
    if (ai_improve_on()) state_line("loaded");
}

/* Test harness (runtime/host.c, a script's "@DAY[.HOUR] txbuy PLAYER MAXFEE";
 * tools/ai_run.py --human-plan): player p buys the site with the most new
 * viewers whose price class is at most maxfee -- what the human's
 * transmitter map does, and leaving it (tx_commit_day: paid now) -- with the
 * coverage map as it was afterwards.  A plan of a good player for the AI
 * tests; 1 if bought.  MAXFEE -1 / -2: the west / east satellite instead. */
int game_test_txbuy(int p, long maxfee) {
    static u8 keep[0x11E2];
    u8 *map = HPTR(g_coverage_map);
    u32 west = g_coverage_west, east = g_coverage_east, bn = 0, bf = 0;
    int si, best = -1;
    if (p < 1 || p > 3) return 0;
    if (maxfee == -1 || maxfee == -2) {
        int k = (int)-maxfee;
        if (tx(p, k).reach != 0 && tx(p, k).state != 2) return 0;
        memcpy(keep, map, sizeof keep);
        u32 before = (transmitters_recalc((u16)p), tx(p, 0).reach);
        tx(p, k).reach = sat_reach(k);
        tx(p, k).state = 1;
        tx_commit_day((u16)p);
        fprintf(stderr, "[human-plan] day %u player %d bought the %s satellite: new %.2f M, price %lu, reach %.1f M\n", g_day, p,
                k == 1 ? "west" : "east", mio(tx(p, 0).reach - before), (unsigned long)tx(p, k).fee, mio(tx(p, 0).reach));
        memcpy(map, keep, sizeof keep);
        g_coverage_west = west;
        g_coverage_east = east;
        return 1;
    }
    if (!(si = free_slot((u16)p))) return 0;
    memcpy(keep, map, sizeof keep);
    transmitters_recalc((u16)p);
    for (int i = 0; i < POINTS; i++) {
        u16 x = (u16)(i % 50 * 3), y = (u16)(i / 50 * 3);
        u32 n = transmitter_new_reach(x, y);
        if ((i32)n <= (i32)bn) continue;
        u32 v = (u32)((i32)((u32)(transmitter_reach(x, y) * 0x268)) / (i32)0xA), f = transmitter_fee_bracket(LO16(v), HI16(v));
        if ((long)f > maxfee) continue;
        best = i; bn = n; bf = f;
    }
    if (best >= 0) {
        tx(p, si).reach = bn;
        tx(p, si).fee = round_to(bf, 0x3E8);
        tx(p, si).x = (u16)(best % 50 * 3);
        tx(p, si).y = (u16)(best / 50 * 3);
        tx(p, si).state = 1;
        tx_commit_day((u16)p);           /* paid, as on leaving the transmitter map */
        fprintf(stderr, "[human-plan] day %u player %d bought %u,%u: new %.2f M, price %lu, reach %.1f M\n", g_day, p,
                tx(p, si).x, tx(p, si).y, mio(bn), (unsigned long)tx(p, si).fee, mio(tx(p, 0).reach));
    }
    memcpy(map, keep, sizeof keep);
    g_coverage_west = west;
    g_coverage_east = east;
    return best >= 0;
}
