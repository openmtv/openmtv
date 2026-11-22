/* A single player's savegame carries its rules (docs/CHANGES.md, "Spielstände
 * mit Regeln"; ROADMAP Phase 6).
 *
 * Until now a tape of the office safe held only how much data it has (the
 * "MTVX" counts: films, series, ads, the AI improvement's state, the offers,
 * the lots) and played on with the rules the game was started with: a game
 * saved with 20 auction lots and loaded with 10 gave lots 11..20 back to the
 * market without a word, one saved with the fixes played on as Classic 1991
 * if Classic was set.  Now the savegame keeps the game's rules -- the eighth
 * count of the extension is the length of a line of them, written after the
 * auction's lots -- and the load (from the safe in the game as well as
 * --load-slot, the launcher's "Continue") plays on with them.
 *
 * The line: "key=value" pairs, one space between, the keys and values of a
 * network game's parameters (runtime/main.c, --mp: the same rules a host
 * fixes for a network game, the same names), e.g.
 *     fixes=all speed4=0 aiimp=1 credit=700000 terror=50000
 *     sammy=0,0,0,0 offers=10 filmoffers=17 lots=20 sertop=1 scripts=0
 * fixes: "classic", "all" or the names of the fixes on (an own choice -- a fix
 * added later stays off for it, as in the settings).  A key this game does not
 * know is passed over (a later game's rule), a key missing is the original's
 * (a rule that came later: an older game played without it).  What is not a
 * rule stays the player's: display, sound, mouse, window, language, the pace
 * of the room changes, the role fit in the shop; the start money and the
 * difficulty are game state already.
 *
 * Written only when a rule is not the build's own default (the original's):
 * a game started without settings and options -- the tests, the classic
 * runs -- saves the original's bytes, as before.  The launcher's settings
 * always give rules (its default credit line is the staircase), so every tape
 * saved from a launcher's game has them; a tape without them is an older one
 * and plays with the rules set, as before.  A network game never writes or
 * applies them: its rules are the host's, its savegame's description has
 * them (src/mpsave.c). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "seat.h"
#include "fixes.h"

/* the rules this game has: written into a savegame when one is not the build's default */
int saverules_needed(void) {
    if (difftest_on || seat_humans) return 0;
    if (fix_spec || fourth_speed_opt || ai_improve_opt || credit_line_opt != -1 || terror_damage_opt != -1) return 1;
    for (int i = 0; i < 4; i++)
        if (SAMMY_PRIZE_OPT(i)) return 1;
    return AD_OFFERS_OPT() != AD_OFFERS_OPT_MIN || FILM_OFFERS_OPT() != FILM_OFFERS_OPT_MIN
        || AUCTION_LOTS_OPT() != AUCTION_LOTS_OPT_MIN || series_top_opt || orig_scripts_opt;
}

/* the line into out (cap bytes, no 0 at its end): its length, 0 if it does not fit */
u16 saverules_text(u8 *out, u16 cap) {
    char t[SAVERULES_MAX + 1];
    int n = snprintf(t, sizeof t, "fixes=%s speed4=%d aiimp=%d credit=%ld terror=%ld sammy=%ld,%ld,%ld,%ld offers=%d filmoffers=%d lots=%d sertop=%d scripts=%d",
                     fix_spec ? fix_spec : "classic", fourth_speed_opt ? 1 : 0, ai_improve_opt, credit_line_opt, terror_damage_opt,
                     SAMMY_PRIZE_OPT(0), SAMMY_PRIZE_OPT(1), SAMMY_PRIZE_OPT(2), SAMMY_PRIZE_OPT(3),
                     AD_OFFERS_OPT(), FILM_OFFERS_OPT(), AUCTION_LOTS_OPT(), series_top_opt ? 1 : 0, orig_scripts_opt ? 1 : 0);
    if (n < 0 || n >= (int)sizeof t || n > cap) {   /* (tools/saverulestest.py: every fix's name fits) */
        fprintf(stderr, "[save-rules] the rules do not fit in %u bytes: saved without them\n", (unsigned)cap);
        return 0;
    }
    memcpy(out, t, (size_t)n);
    return (u16)n;
}

/* a number of the line: v from text s, if it is one within lo..hi */
static int num_in(const char *s, long lo, long hi, long *v) {
    char *e;
    long n = strtol(s, &e, 10);
    if (e == s || *e || n < lo || n > hi) return 0;
    *v = n;
    return 1;
}

/* the rules the game was started with (the launcher's settings, the command line): a tape
 * without rules plays with them -- also after a tape with rules was loaded in the same game */
typedef struct {
    char *fixes;
    int speed4, aiimp, offers, filmoffers, lots, sertop, scripts;
    long credit, terror, sammy[4];
} Rules;
static Rules start;
static int start_kept, changed, applied;
static void rules_get(Rules *r) {
    r->fixes = fix_spec ? strdup(fix_spec) : NULL;
    r->speed4 = fourth_speed_opt; r->aiimp = ai_improve_opt;
    r->credit = credit_line_opt; r->terror = terror_damage_opt;
    for (int i = 0; i < 4; i++) r->sammy[i] = sammy_prize_opt[i];
    r->offers = ad_offers_opt; r->filmoffers = film_offers_opt; r->lots = auction_lots_opt;
    r->sertop = series_top_opt; r->scripts = orig_scripts_opt;
}
static void rules_set(const Rules *r) {
    fix_respec(r->fixes);                /* (copied) */
    fourth_speed_opt = r->speed4; ai_improve_opt = r->aiimp;
    credit_line_opt = r->credit; terror_damage_opt = r->terror;
    for (int i = 0; i < 4; i++) sammy_prize_opt[i] = r->sammy[i];
    ad_offers_opt = r->offers; film_offers_opt = r->filmoffers; auction_lots_opt = r->lots;
    series_top_opt = r->sertop; orig_scripts_opt = r->scripts;
    tables_offers_init();                /* the counts and their host memory (state.c) */
    applied = 1;
}

/* The rules of a savegame of the safe being loaded (slot: its resource; the line, n bytes;
 * NULL: it has none): the game plays on with them -- the counts of the offers and lots and
 * their host memory set anew before the extension's data are read (savegame_extras,
 * src/actions.c), the fixes' choice read anew; the data fixes follow after the load
 * (saverules_after_load).  A savegame without rules, and the start savegame of a new game
 * (START_SAVE_RES: after game over the setup's new game loads it again): the start's, if a
 * savegame's had come in.  The pairs are split at spaces only (launcher/saves.go alike). */
void saverules_apply(u16 slot, const u8 *line, u16 n) {
    if (difftest_on || seat_humans) return;
    if (!line || !n || n > SAVERULES_MAX) {
        if (changed) {
            rules_set(&start);
            changed = 0;
            if (slot == START_SAVE_RES) fprintf(stderr, "[save-rules] a new game: the start's rules again\n");
            else fprintf(stderr, "[save-rules] slot %u: no rules in the savegame -- the start's again\n", (unsigned)(slot - SAVE_SLOT_RES));
        }
        return;
    }
    if (!start_kept) { rules_get(&start); start_kept = 1; }
    char t[SAVERULES_MAX + 1];
    memcpy(t, line, n);
    t[n] = 0;
    /* the original's, for a rule the line lacks */
    Rules r = { NULL, 0, 0, 0, 0, 0, 0, 0, -1, -1, { -1, -1, -1, -1 } };
    long v;
    for (char *k = strtok(t, " "); k; k = strtok(NULL, " ")) {
        char *val = strchr(k, '=');
        if (!val) continue;
        *val++ = 0;
        if (!strcmp(k, "fixes")) r.fixes = strcmp(val, "classic") ? val : NULL;
        else if (!strcmp(k, "speed4") && num_in(val, 0, 1, &v)) r.speed4 = (int)v;
        else if (!strcmp(k, "aiimp") && num_in(val, 0, 2, &v)) r.aiimp = (int)v;
        else if (!strcmp(k, "credit") && num_in(val, -1, CREDIT_LINE_MAX, &v)
                 && (v <= CREDIT_LINE_ORIGINAL || (v >= CREDIT_LINE_MIN && v % 100000 == 0))) r.credit = v;   /* (as --credit-line takes it) */
        else if (!strcmp(k, "terror") && num_in(val, -1, MONEY_OPT_MAX, &v)) r.terror = v;
        else if (!strcmp(k, "sammy")) {
            char *p = val;
            for (int i = 0; i < 4 && p; i++) {
                char *c = strchr(p, ',');
                if (c) *c = 0;
                if (num_in(p, -1, MONEY_OPT_MAX, &v)) r.sammy[i] = v ? v : -1;
                p = c ? c + 1 : NULL;
            }
        }
        else if (!strcmp(k, "offers") && num_in(val, AD_OFFERS_OPT_MIN, AD_OFFERS_OPT_MAX, &v)) r.offers = (int)v;
        else if (!strcmp(k, "filmoffers") && num_in(val, FILM_OFFERS_OPT_MIN, FILM_OFFERS_OPT_MAX, &v)) r.filmoffers = (int)v;
        else if (!strcmp(k, "lots") && num_in(val, AUCTION_LOTS_OPT_MIN, AUCTION_LOTS_OPT_MAX, &v)) r.lots = (int)v;
        else if (!strcmp(k, "sertop") && num_in(val, 0, 1, &v)) r.sertop = (int)v;
        else if (!strcmp(k, "scripts") && num_in(val, 0, 1, &v)) r.scripts = (int)v;
    }
    rules_set(&r);
    changed = 1;
    char now[SAVERULES_MAX + 1];
    u16 m = saverules_text((u8 *)now, SAVERULES_MAX);
    now[m] = 0;
    fprintf(stderr, "[save-rules] slot %u: the savegame's rules: %s\n", (unsigned)(slot - SAVE_SLOT_RES), now);
}

/* after a load that took a savegame's rules: the data fixes in or out as its fixes say
 * (they change records a savegame does not hold) */
void saverules_after_load(void) {
    if (!applied) return;
    applied = 0;
    fix_data_records_sync();
}
