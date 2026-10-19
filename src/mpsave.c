/* Multiplayer: saving a network game at night, and starting one from such a
 * savegame (docs/multiplayer.md, stage 7).
 *
 * The save: at the day change, when every human has gone home and the new
 * day is made (game_time_tick, after new_day) -- the rest point where no
 * human is in a room, in a dialog or carrying anything.  Every machine
 * computes every seat, so every machine writes the same file (--mp-save DIR:
 * DIR/tag-NNN.mtvmp, NNN the day that begins).  It holds what the
 * original's savegame holds (mp_savegame_write, src/actions.c: the same
 * bytes, written without changing the game) and, for every human, what
 * only that human has -- credit, bag, ledgers, office, the boss's
 * complaints, Madris: the single-player savegame holds that once, as the
 * running seat has it.
 *
 * The load: --mp-load FILE instead of the setup's Start (game_main, mode 0):
 * the savegame as the setup's "Load" loads it, the seats' stations, then the
 * game's first morning (mode 2) -- the saved day's morning, the humans walk
 * to their bosses -- where every other human seat joins with its own data
 * (seat_human_main).  Every machine starts the same file at frame 0, so it is
 * the same game everywhere; not the saved game's own continuation (the
 * computer players draw from the timer, as after a single-player load), the
 * same day.  A fired human comes back the way they left: watching.
 *
 * The file: "MTVMPSV1", then three parts, each a u32 length (little-endian)
 * and its bytes: the description (JSON, for the launcher), the savegame
 * (the original's format with a mod's "MTVX" extension), the humans (u16
 * version 1, u8 count, then for each: u8 station, u8 out, u16 length and
 * the bytes of human_ranges in their order). */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "seat.h"
#include "mpsave.h"

int mp_loaded;

/* what only a human has, of what a savegame holds (the single-player
 * savegame's blocks that are a seat's own, src/seatctx.c): kept from one
 * night to the next morning -- not the views and the cursor, which every
 * seat sets up anew when it joins */
typedef struct { u16 off, len; } Range;
static const Range human_ranges[] = {
    {0x2483, 0x1E},     /* the bag */
    {0x734A, 0x02},     /* bag count */
    {0x60BF, 0x82},     /* the office messages */
    {0x7247, 0x01},     /* message list position */
    {0x71F1, 0x54},     /* ledger today */
    {0x7492, 0x54},     /* ledger yesterday */
    {0x6147, 0x1E},     /* the dragged news record */
    {0x79F8, 0x06},     /* statistics chart: mode, player, scale */
    {0x79FE, 0x02},     /* the office TV's channel */
    {0x7A02, 0x04},     /* yesterday's cancelled hours and missed ads (the boss complains) */
    {0x7A0E, 0x04},     /* the boss's speech step, the TV cover */
    {0x7A14, 0x02},     /* topicality of the human's program */
    {0x7A24, 0x04},     /* papers in the office, the dealer's pose */
    {0x7A2E, 0x02},     /* the estate agent's decoration */
    {0x7A34, 0x02},     /* alien look */
    {0x7A52, 0x04},     /* the estate agent's portrait; the player to be thrown out */
    {0x7A6E, 0x18},     /* chart of image, news scroll and category, Madris */
    {0x7ADE, 0x08},     /* credit, catch fine */
    {0x7AFE, 0x04},     /* bank credit */
    {0x8AD4, 0xC8},     /* Madris board */
};
#define NHUMAN (int)(sizeof human_ranges / sizeof human_ranges[0])
static u32 human_len(void) {
    u32 n = 0;
    for (int k = 0; k < NHUMAN; k++) n += human_ranges[k].len;
    return n;
}

static const char MAGIC[8] = { 'M', 'T', 'V', 'M', 'P', 'S', 'V', '1' };
#define SAVE_BASE 37605u                 /* the original's savegame: 200 films, 50 series, ... (FINDINGS "Savegames") */
#define EXTRA_MAX 4000u                  /* films, series, ads a mod may have, at most (the loader's u16 loops stay finite) */
enum { HUMANS_VERSION = 1 };

static void put32(u8 *p, u32 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24); }
static u32 get32(const u8 *p) { return p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }

/* the parts of a savegame file: 1 and the three, or 0 with the reason */
typedef struct { const u8 *meta, *game, *humans; u32 meta_n, game_n, humans_n; } Parts;
static int parts_of(const u8 *d, u32 n, Parts *p, char *why, unsigned whylen) {
    u32 at = 8, len[3];
    const u8 *part[3];
    if (n < 8 || memcmp(d, MAGIC, 8)) { snprintf(why, whylen, "not a network game's savegame"); return 0; }
    for (int k = 0; k < 3; k++) {
        if (n - at < 4) { snprintf(why, whylen, "the file is cut short"); return 0; }
        len[k] = get32(d + at); at += 4;
        if (n - at < len[k]) { snprintf(why, whylen, "the file is cut short"); return 0; }
        part[k] = d + at; at += len[k];
    }
    p->meta = part[0]; p->meta_n = len[0];
    p->game = part[1]; p->game_n = len[1];
    p->humans = part[2]; p->humans_n = len[2];
    if (p->game_n < SAVE_BASE || p->game_n > 0xFFFF) { snprintf(why, whylen, "the savegame part has %u bytes", (unsigned)p->game_n); return 0; }
    if (p->game_n > SAVE_BASE) {         /* a mod's films 201.., series 51.., ads 51.., the computer players' state ("MTVX", src/actions.c) */
        const u8 *x = p->game + SAVE_BASE;
        u32 left = p->game_n - SAVE_BASE, c[6] = { 0, 0, 0, 0, 10, 10 }, cnt = left >= 6 ? (u32)(x[4] | x[5] << 8) : 0;
        if (left < 12 || memcmp(x, "MTVX", 4) || cnt < 3 || cnt > 6 || left < 6 + 2 * cnt) { snprintf(why, whylen, "the savegame's extension is not MTVX with 3 to 6 counts"); return 0; }
        for (u32 k = 0; k < cnt; k++) c[k] = x[6 + 2 * k] | (u32)x[7 + 2 * k] << 8;
        if (c[0] > EXTRA_MAX || c[1] > EXTRA_MAX || c[2] > EXTRA_MAX || c[3] > 1024 || c[4] > AD_OFFERS_MAX || c[5] > FILM_OFFERS_MAX) { snprintf(why, whylen, "the savegame's extension counts too many"); return 0; }
        u32 need = 6 + 2 * cnt + (c[0] > 200 ? (c[0] - 200) * 0x12 : 0) + (c[1] > 50 ? (c[1] - 50) * 0x29 : 0) + (c[2] > 50 ? c[2] - 50 : 0) + c[3]
                 + (c[4] > 10 ? (c[4] - 10) * 0x7 : 0)    /* the ad agency's offers past 10 (--ad-offers) */
                 + (c[5] > 10 ? (c[5] - 10) * 0x8 : 0);   /* the film dealer's past 10 (--film-offers) */
        if (left != need) { snprintf(why, whylen, "the savegame's extension has %u bytes, its counts %u", (unsigned)left, (unsigned)need); return 0; }
    }
    if (p->humans_n < 3 || (p->humans[0] | p->humans[1] << 8) != HUMANS_VERSION) {
        snprintf(why, whylen, "the humans' part is of another version"); return 0;
    }
    u32 h = 3;
    for (int k = 0; k < p->humans[2]; k++) {
        if (p->humans_n - h < 4) { snprintf(why, whylen, "the humans' part is cut short"); return 0; }
        u32 hl = p->humans[h + 2] | (u32)p->humans[h + 3] << 8;
        if (hl != human_len() || p->humans_n - h - 4 < hl) { snprintf(why, whylen, "a human's data has %u bytes, not %u", (unsigned)hl, (unsigned)human_len()); return 0; }
        h += 4 + hl;
    }
    return 1;
}
int mpsave_check(const u8 *data, u32 size, char *why, unsigned whylen) {
    Parts p;
    return parts_of(data, size, &p, why, whylen);
}

/* the description, piece by piece into o (cap bytes): *n never passes cap - 1 */
static void jput(char *o, u32 cap, u32 *n, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int k = vsnprintf(o + *n, cap - *n, fmt, ap);
    va_end(ap);
    *n = k < 0 ? *n : (*n + (u32)k < cap ? *n + (u32)k : cap - 1);
}
/* a string as JSON (at most max bytes of it; the setup takes printable ASCII, other bytes as \u00XX) */
static void jstr(char *o, u32 cap, u32 *n, const u8 *s, u32 max) {
    jput(o, cap, n, "\"");
    for (u32 i = 0; i < max && s[i]; i++) {
        if (s[i] == '"' || s[i] == '\\') jput(o, cap, n, "\\%c", s[i]);
        else if (s[i] >= 0x20 && s[i] < 0x7F) jput(o, cap, n, "%c", s[i]);
        else jput(o, cap, n, "\\u%04x", s[i]);
    }
    jput(o, cap, n, "\"");
}

/* the running seat's human's data, or another seat's (mp_enter_player): 1 if
 * written to out */
static int human_bytes(int seat, u8 *out) {
    int entered = 0;
    if (seat != seat_cur) {
        if (!mp_enter_player(seat_station[seat])) return 0;   /* (not joined) */
        entered = 1;
    }
    u32 n = 0;
    for (int k = 0; k < NHUMAN; k++) {
        memcpy(out + n, dg_addr(human_ranges[k].off), human_ranges[k].len);
        n += human_ranges[k].len;
    }
    if (entered) mp_leave();
    return 1;
}

u64 dos_game_files_hash(void);

void mpsave_night(void) {
    if (!seat_humans || !mp_save_dir || !mp_save_dir[0]) return;
    static u8 game[0x10000];
    u32 game_n = mp_savegame_write(game, sizeof game);
    if (!game_n) { fprintf(stderr, "[mpsave] day %u: the savegame does not fit in 64 KB, not saved\n", g_day); return; }
    /* the humans (a seat that never joined -- it cannot be, at night -- is left out) */
    u32 hl = human_len();
    u8 *hum = calloc(1, 3 + SEATS_MAX * (4 + hl));
    if (!hum) return;
    u32 hn = 3;
    int saved[SEATS_MAX] = { 0 };
    hum[0] = HUMANS_VERSION; hum[1] = 0;
    for (int i = 0; i < seats_n; i++) {
        if (!seat_station[i]) continue;
        u8 *h = hum + hn;
        if (!human_bytes(i, h + 4)) { fprintf(stderr, "[mpsave] seat %d has not joined: its human is not saved\n", i); continue; }
        h[0] = (u8)seat_station[i]; h[1] = (u8)(seat_out[i] != 0);
        h[2] = (u8)hl; h[3] = (u8)(hl >> 8);
        hn += 4 + hl;
        hum[2]++;
        saved[i] = 1;
    }
    /* the description: the same on every machine (no clock time, no paths) */
    if (!mp_files_hash) mp_files_hash = dos_game_files_hash();
    char meta[4096];
    u32 mn = 0;
    jput(meta, sizeof meta, &mn, "{\"format\": 1, \"day\": %u, \"difficulty\": %u, \"files\": \"%016llx\", \"fixes\": ",
         g_day, g_difficulty, (unsigned long long)mp_files_hash);
    jstr(meta, sizeof meta, &mn, (const u8 *)(fix_spec ? fix_spec : "classic"), 1024);
    jput(meta, sizeof meta, &mn, ", \"speed4\": %d, \"aiimp\": %d, \"credit\": %ld, \"terror\": %ld, \"sammy\": [%ld, %ld, %ld, %ld], ",
         fourth_speed_opt, ai_improve_opt, credit_line_opt, terror_damage_opt,
         SAMMY_PRIZE_OPT(0), SAMMY_PRIZE_OPT(1), SAMMY_PRIZE_OPT(2), SAMMY_PRIZE_OPT(3));
    if (AD_OFFERS_OPT() > AD_OFFERS_OPT_MIN)   /* (none: the original's 10 -- a savegame from before the option alike) */
        jput(meta, sizeof meta, &mn, "\"offers\": %d, ", AD_OFFERS_OPT());
    if (FILM_OFFERS_OPT() > FILM_OFFERS_OPT_MIN)   /* (as the ad offers) */
        jput(meta, sizeof meta, &mn, "\"filmoffers\": %d, ", FILM_OFFERS_OPT());
    jput(meta, sizeof meta, &mn, "\"humans\": [");
    int first = 1;
    for (int i = 0; i < seats_n; i++) {
        if (!saved[i]) continue;
        jput(meta, sizeof meta, &mn, "%s{\"station\": %d, \"out\": %s, \"name\": ", first ? "" : ", ",
             seat_station[i], seat_out[i] ? "true" : "false");
        jstr(meta, sizeof meta, &mn, HPTR(MK32(pl_name_hi((u16)seat_station[i]), pl_name_lo((u16)seat_station[i]))), 0x28);
        jput(meta, sizeof meta, &mn, ", \"money\": %ld}", (long)(i32)players(seat_station[i]).money);
        first = 0;
    }
    jput(meta, sizeof meta, &mn, "]}\n");
    if (mn >= sizeof meta - 1) { fprintf(stderr, "[mpsave] day %u: the description does not fit, not saved\n", g_day); free(hum); return; }
    /* the file, written to the side and renamed: never half a savegame */
    char path[1100], tmp[1110];
    snprintf(path, sizeof path, "%s/tag-%03u.mtvmp", mp_save_dir, g_day);
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    int ok = f != NULL;
    u8 len[4];
    if (ok) {
        ok &= fwrite(MAGIC, 1, 8, f) == 8;
        put32(len, mn); ok &= fwrite(len, 1, 4, f) == 4 && fwrite(meta, 1, mn, f) == mn;
        put32(len, game_n); ok &= fwrite(len, 1, 4, f) == 4 && fwrite(game, 1, game_n, f) == game_n;
        put32(len, hn); ok &= fwrite(len, 1, 4, f) == 4 && fwrite(hum, 1, hn, f) == hn;
        ok &= fclose(f) == 0;
    }
    remove(path);                         /* (Windows: rename does not replace) */
    if (ok && rename(tmp, path) == 0)
        fprintf(stderr, "[mpsave] day %u saved at frame %llu: %s\n", g_day, (unsigned long long)frame_no(), path);
    else {
        fprintf(stderr, "[mpsave] day %u: cannot write %s\n", g_day, path);
        remove(tmp);
    }
    free(hum);
}

/* the human of that station's data from the savegame into memory */
int mpsave_apply_human(int station) {
    Parts p;
    char why[128];
    if (!mp_loaded || !mp_load_data || !parts_of(mp_load_data, mp_load_size, &p, why, sizeof why)) return 0;
    u32 h = 3, hl = human_len();
    for (int k = 0; k < p.humans[2]; k++, h += 4 + hl) {
        if (p.humans[h] != station) continue;
        u32 n = 0;
        for (int r = 0; r < NHUMAN; r++) {
            memcpy(dg_addr(human_ranges[r].off), p.humans + h + 4 + n, human_ranges[r].len);
            n += human_ranges[r].len;
        }
        return 1;
    }
    return 0;
}

void mpsave_load_start(void) {
    Parts p;
    char why[128];
    if (!mp_load_data || !parts_of(mp_load_data, mp_load_size, &p, why, sizeof why)) {
        fprintf(stderr, "--mp-load: %s\n", why);
        exit(2);
    }
    cursor_hide();
    /* the savegame, as the setup's "Load" loads it (room_saveload_click) */
    mp_load_buf = p.game;
    mp_load_len = (u16)p.game_n;
    /* the seats' stations must be the savegame's humans' (the lobby checks it; --humans by hand
     * may not): another station would be a computer player's still, and one of the save's nobody's */
    u32 h0 = 3, hl0 = human_len();
    int match = p.humans[2] == seats_n;
    for (int k = 0; k < p.humans[2] && match; k++, h0 += 4 + hl0) {
        int found = 0;
        for (int i = 0; i < seats_n; i++) found |= seat_station[i] == p.humans[h0];
        match = found;
    }
    if (!match) {
        fprintf(stderr, "--mp-load: the savegame's humans are on other stations than --humans\n");
        exit(2);
    }
    game_load(0xD2, 0xD2);
    mp_load_buf = NULL;
    mp_load_len = 0;
    if (savegame_load_refused()) {
        fprintf(stderr, "--mp-load: the savegame has more films, series or ads than this game\n");
        exit(2);
    }
    if (getenv("MPSAVE_CHECK")) {          /* the round trip: what was loaded, written again */
        static u8 again[0x10000];
        u32 n = mp_savegame_write(again, sizeof again);
        u32 d = 0;
        while (d < n && d < p.game_n && again[d] == p.game[d]) d++;
        const char *to = getenv("MPSAVE_CHECK");
        if (strlen(to) > 4 && !strcmp(to + strlen(to) - 4, ".bin")) {   /* (and the bytes, for a comparison) */
            FILE *f = fopen(to, "wb");
            if (f) { fwrite(again, 1, n, f); fclose(f); }
        }
        if (n == p.game_n && d == n) fprintf(stderr, "[mpsave] round trip: the same %u bytes\n", (unsigned)n);
        else fprintf(stderr, "[mpsave] round trip: %u bytes written again, %u loaded, the first difference at %u\n",
                     (unsigned)n, (unsigned)p.game_n, (unsigned)d);
    }
    mp_loaded = 1;
    /* the seats' stations: the savegame's humans (the lobby has checked) */
    u32 h = 3, hl = human_len();
    for (int k = 0; k < p.humans[2]; k++, h += 4 + hl)
        for (int i = 0; i < seats_n; i++)
            if (seat_station[i] == p.humans[h]) seat_out[i] = p.humans[h + 1] != 0;
    g_player = (u16)seat_station[0];
    g_player_figure = g_player - 1;
    g_cur_player_rec = players_at(g_player);
    mpsave_apply_human(seat_station[0]);
    /* the figures' sprites as the setup's Load reloads them (after its g_player_figure): the
     * day's intruder, seat 0's alien look -- and the other humans' (figures_load_sprites knows
     * only g_player_figure's; the sprites are the world's, for every seat) */
    figures_load_sprites();
    for (int i = 1; i < seats_n; i++)
        if (seat_station[i] && figures(seat_station[i] - 1).speed == 2) figure_load_sprite_slot(0x18, (u16)(seat_station[i] - 1));
    /* the start's randomness and clock as the setup's Start makes them (hotspot 0x4A) */
    seat_clock_world = 1;
    game_clock_stop(0 /* unused ax */);
    g_cursor_sprites(6) = g_clock_acc + (u32)seed_opt;
    game_clock_start();
    seat_clock_world = 0;
    fprintf(stderr, "[mpsave] loaded day %u, %u humans\n", g_day, p.humans[2]);
    g_game_mode = 2;                     /* the morning (game_main) */
}
