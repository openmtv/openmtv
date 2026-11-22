/* The rule "Unfertige Drehbuecher integrieren" (--original-screenplays, settings [game]
 * original_screenplays): the original's 23 unfinished screenplays -- the shows 25-33,
 * the series 37-50 -- filled and offered by the screenplay agency (docs/CHANGES.md).
 * The 1991 data has them without the flag the agency offers by (bit 1 of +5, which
 * nothing in the code sets), the shows without their roles' requirements and a
 * shopping list, the series with nothing but their texts; tools/origscripts.py has the
 * values and writes origscripts_data.h from them (the launcher's editor reads the same
 * values, launcher/origscripts.go).
 *
 * A savegame holds of a screenplay its state and its shopping lists (+4, +8..+0x165),
 * not the rest -- price, flags, target, shooting time, the roles' requirements and
 * categories, the texts; of a series not its length, episodes, card values or episode
 * price; of a film not its X rating.  So after every load (the start's savegame 0x2B
 * included, game_load):
 *   - the rule on, or a savegame that has the rule's lists (a game started with it,
 *     loaded without it: its screenplays keep their roles): a screenplay whose unsaved
 *     part is the original's gets the rule's, an empty shopping list (state 0, never
 *     taken) the rule's lists; the series and films likewise while theirs are the
 *     original's;
 *   - else what the rule changed goes back to the original's (a savegame without it,
 *     loaded after one with it).
 * A pack that changed one of them (its record is no longer the original's) keeps its
 * own.  The records' changes are untimed and no state of their own goes into a savegame:
 * without the rule and without a savegame of it nothing here changes a byte.  The texts
 * take the screenplay text cache's own (timed) allocation, as the data fix film-200's do --
 * only while the rule's screenplays are in the game, the same on every machine. */
#include <string.h>
#include "game.h"
#include "fixes.h"
#include "origscripts_data.h"

#define SP_SIZE 0x1B3
#define LIST_LO 8                         /* the episode blocks: +8 .. +0x165 */
#define LIST_HI 0x166
static const u16 head_runs[3][2] = { {0, 4}, {5, 8}, {0x166, SP_SIZE} };

static u8 sp_applied[ORIGSCRIPTS_N];
static u8 sp_backup[ORIGSCRIPTS_N][SP_SIZE];
static u8 se_applied[ORIGSCRIPTS_SERIES_N];
static u8 se_backup[ORIGSCRIPTS_SERIES_N][0x39];
static u8 fi_applied[ORIGSCRIPTS_FILMS_N];

int origscripts_on(void) { return orig_scripts_opt && !difftest_on; }
int origscripts_active(void) {
    for (int i = 0; i < ORIGSCRIPTS_N; i++)
        if (sp_applied[i]) return 1;
    return 0;
}

static u32 head_hash(u32 s) {
    u32 h = 0x811C9DC5u;
    for (int r = 0; r < 3; r++)
        for (u16 k = head_runs[r][0]; k < head_runs[r][1]; k++) h = (h ^ FP8(s, k)) * 0x01000193u;
    return h;
}
/* no entry in any of the ten shopping lists (the base quality may be there: 25 has its own) */
static int lists_empty(u32 s) {
    for (u16 e = 0; e < 10; e++)
        for (u16 k = 1; k <= 10; k++)
            if (FP8(s, LIST_LO + e * 0x23 + 1 + 3 * k)) return 0;
    return 1;
}
static int is_ours(int i, u32 s) { return sp_applied[i] || head_hash(s) == origscripts_sp[i].head; }
/* the first shopping list is the rule's (base quality, items and weights; what was bought aside) --
 * a savegame made with the rule; a pack's own list for one of them is no sign of it */
static int list_is_rules(int i, u32 s) {
    const u8 *rec = origscripts_sp[i].rec;
    if (FP8(s, LIST_LO) != rec[LIST_LO]) return 0;
    for (u16 k = 1; k <= 10; k++) {
        u16 o = LIST_LO + 1 + 3 * k;
        if (FP8(s, o) != rec[o] || FP8(s, o + 1) != rec[o + 1]) return 0;
    }
    return 1;
}
static u32 series_ptr(int i) { return series_at(origscripts_series[i].id); }
/* the series' part a savegame does not hold as the original's (its price, flag bits 5 and 7,
 * its episodes' values are a savegame's) */
static int series_original(int i, u32 s) {
    return FP8(s, 0x21) == origscripts_series[i].genre && FP8(s, 0x22) == origscripts_series[i].episodes_orig
        && !FP8(s, 0x23) && !(FP8(s, 0x25) & 0x1F) && !FP32(s, 0x2B) && !FP8(s, 0x35) && !FP8(s, 0x36);
}
#define FILM_SAVED2 0x03                  /* film flags2 bits a savegame holds */

/* a series screenplay is offered only with a series to produce: the rule's, or one that has
 * episodes of a length (a pack's) -- series 44 has 0 episodes in the original, and a computer
 * player's rating of a screenplay over 0 episodes divides by zero (2DD9:434F) */
static int target_ok(int i) {
    const u8 *rec = origscripts_sp[i].rec;
    if (!(rec[5] & 1)) return 1;                  /* a film (a show): its film is complete */
    u16 id = (u16)(rec[6] | rec[7] << 8);
    for (int k = 0; k < ORIGSCRIPTS_SERIES_N; k++)
        if (origscripts_series[k].id == id && se_applied[k]) return 1;
    u32 t = series_at(id);
    return FP8(t, 0x22) && FP8(t, 0x23);
}

void origscripts_after_load(void) {
    if (!fix_init()) return;                 /* (the machine-level build: the original's data) */
    int layer = origscripts_on();
    for (int i = 0; !layer && i < ORIGSCRIPTS_N; i++) {   /* a savegame of a game with the rule */
        u32 s = screenplays_at(origscripts_sp[i].id);
        if (is_ours(i, s) && list_is_rules(i, s)) layer = 1;
    }
    for (int i = 0; i < ORIGSCRIPTS_SERIES_N; i++) {
        u32 s = series_ptr(i);
        if (layer) {
            if (!se_applied[i] && series_original(i, s)) {
                for (u16 k = 0; k < 0x39; k++) se_backup[i][k] = FP8(s, k);
                FP8(s, 0x22) = origscripts_series[i].episodes;
                FP8(s, 0x23) = origscripts_series[i].blocks;
                FP8(s, 0x25) = (u8)((FP8(s, 0x25) & ~0x1F) | origscripts_series[i].family);
                FP32(s, 0x2B) = (u32)origscripts_series[i].episode_price;
                FP8(s, 0x35) = origscripts_series[i].critics;
                FP8(s, 0x36) = origscripts_series[i].press;
                se_applied[i] = 1;
            }
            if (se_applied[i] && !FP32(s, 0x27)) FP32(s, 0x27) = (u32)origscripts_series[i].price;   /* (a savegame holds the price) */
        } else if (se_applied[i]) {
            FP8(s, 0x22) = se_backup[i][0x22];
            FP8(s, 0x23) = se_backup[i][0x23];
            FP8(s, 0x25) = (u8)((FP8(s, 0x25) & ~0x1F) | (se_backup[i][0x25] & 0x1F));
            for (u16 k = 0x2B; k < 0x2F; k++) FP8(s, k) = se_backup[i][k];
            FP8(s, 0x35) = se_backup[i][0x35];
            FP8(s, 0x36) = se_backup[i][0x36];
            se_applied[i] = 0;
        }
    }
    for (int i = 0; i < ORIGSCRIPTS_N; i++) {
        u32 s = screenplays_at(origscripts_sp[i].id);
        const u8 *rec = origscripts_sp[i].rec;
        if (layer) {
            if (!sp_applied[i] && head_hash(s) == origscripts_sp[i].head && target_ok(i)) {
                for (int r = 0; r < 3; r++)
                    for (u16 k = head_runs[r][0]; k < head_runs[r][1]; k++) {
                        sp_backup[i][k] = FP8(s, k);
                        FP8(s, k) = rec[k];
                    }
                sp_applied[i] = 1;
            }
            if (sp_applied[i] && FP8(s, 4) == 0 && lists_empty(s))
                for (u16 k = LIST_LO; k < LIST_HI; k++) FP8(s, k) = rec[k];
        } else if (sp_applied[i]) {
            for (int r = 0; r < 3; r++)
                for (u16 k = head_runs[r][0]; k < head_runs[r][1]; k++) FP8(s, k) = sp_backup[i][k];
            sp_applied[i] = 0;
        }
    }
    for (int i = 0; i < ORIGSCRIPTS_FILMS_N; i++) {
        u32 f = films_at(origscripts_films[i].id);
        if (layer) {
            if (!fi_applied[i] && FP8(f, 0) == origscripts_films[i].genre
                && (FP8(f, 7) & ~FILM_SAVED2) == (origscripts_films[i].flags2_orig & ~FILM_SAVED2)) {
                FP8(f, 7) = (u8)((FP8(f, 7) & FILM_SAVED2) | (origscripts_films[i].flags2 & ~FILM_SAVED2));
                fi_applied[i] = 1;
            }
        } else if (fi_applied[i]) {
            FP8(f, 7) = (u8)((FP8(f, 7) & FILM_SAVED2) | (origscripts_films[i].flags2_orig & ~FILM_SAVED2));
            fi_applied[i] = 0;
        }
    }
}

/* the 16 texts of screenplay ID as read from the resource (length-prefixed strings): for
 * one the rule fills, an empty one of the game's is the rule's -- the new block to out
 * (its length; 0 = keep buf) */
int origscripts_texts(u16 id, const u8 *buf, u16 len, u8 *out, u16 cap) {
    int i = 0;
    while (i < ORIGSCRIPTS_N && origscripts_sp[i].id != id) i++;
    if (i == ORIGSCRIPTS_N || !sp_applied[i]) return 0;
    const char *const *ours = origscripts_txt[i][g_language == 0 ? 0 : 1];
    u16 p = 0, n = 0;
    for (int k = 0; k < 16; k++) {
        u8 l = p < len ? buf[p] : 0;
        const u8 *from = buf + p + 1;
        if (!l && ours[k]) { from = (const u8 *)ours[k]; l = (u8)strlen(ours[k]); }
        if (n + 1 + l > cap) return 0;
        out[n] = l;
        memcpy(out + n + 1, from, l);
        n = (u16)(n + 1 + l);
        if (p < len) p = (u16)(p + 1 + buf[p]);
    }
    for (; p < len; p++) {                    /* (whatever follows the 16: kept) */
        if (n >= cap) return 0;
        out[n++] = buf[p];
    }
    return n;
}
