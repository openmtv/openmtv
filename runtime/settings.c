/* The player's settings (ROADMAP Phase 6): an INI file, by default
 * $XDG_CONFIG_HOME/madtv/settings.ini or ~/.config/madtv/settings.ini
 * (Windows: %APPDATA%\madtv\settings.ini).
 *
 * Read only when the program is started with --settings[=FILE] (play.sh
 * does; the tests never do, so a player's settings cannot change a test).
 * Precedence: built-in defaults < the file < command-line options (main.c).
 * A missing file is created with the defaults and comments; a setting a
 * newer version knows and the file lacks is appended with its comment
 * (the player's values stay as they are).  What the player
 * changes while playing (fullscreen) is written back in place, keeping the
 * file's comments and order. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#endif
#include "runtime.h"

int settings_active;
static char path[1024];
#define MAXV 64
static struct { char sec[32], key[32], val[1024]; } vals[MAXV];   /* val: a fixes list of every name is 280 characters */
static int nvals;

static const char defaults[] =
    "# OpenMTV settings.  ./play.sh reads them; an option on its command line\n"
    "# overrides one for that run (e.g. ./play.sh --fullscreen, ./play.sh d).\n"
    "\n"
    "[display]\n"
    "# start in fullscreen (F11, Alt+Enter or Cmd+F switch while playing; remembered)\n"
    "fullscreen = no\n"
    "# window size: N times 320x240\n"
    "scale = 3\n"
    "# 4:3 like a VGA monitor, or square (square pixels, 16:10)\n"
    "aspect = 4:3\n"
    "# sharp (even pixels at any size), nearest, linear, mmpx (smoothed edges\n"
    "# and diagonals, only the picture's own colours), or hr-pixel (drawn at 4x:\n"
    "# a finer font, exact frames and bars, the pictures' pixels as they are);\n"
    "# hr-pixel only in the decompiled build\n"
    "filter = sharp\n"
    "# AI upscaling (docs/ki-bilder.md): a picture set the launcher made from your game\n"
    "# files (a .ki file; empty: none), shown with filter hr-pixel; Ctrl+K (on the\n"
    "# Mac also Cmd+K) switches them off and on while playing.  Only the decompiled build.\n"
    "ai_pictures =\n"
    "# whole-number scale factors only\n"
    "integer = no\n"
    "# the production shop shows how well each actor fits the role picked in the\n"
    "# shopping list, in per cent (the game's own rating), instead of the three bars\n"
    "# humor, appearance, craft (no: the original's panel).  Only the decompiled build.\n"
    "cast_fit = no\n"

    "\n"
    "[game]\n"
    "# english, german, or ask (the game's own prompt)\n"
    "language = english\n"
    "# classic: exactly the original game, none of its bugs fixed (yes), whatever\n"
    "# \"fixes\" below says -- it stays the choice for when classic is off again\n"
    "classic = no\n"
    "# fixes of the original's bugs: all, none (exactly the original game), or a\n"
    "# comma-separated list of names (build/mtv-dec --fixes=list prints them)\n"
    "fixes = all\n"
    "# your cash when a new game starts, in dollars, 0 to 100000000 (the original:\n"
    "# 200000; the credit stays 200000).  Only the decompiled build has it.\n"
    "start_money = 200000\n"
    "# the most the boss lends you (image 10 to 49), in dollars, 300000 to 100000000 in\n"
    "# steps of 100000: the $200000 of the start, then $100000 more a day from day 2\n"
    "# (the original: 300000, i.e. once).  original: the original's credit, and the\n"
    "# fix credit-repay off (also in a network game).  Only the decompiled build has it.\n"
    "credit_line = 700000\n"
    "# what a terrorist's bomb costs the owner of the room it hits (sabotage through\n"
    "# the information board), in dollars, 0 to 100000000 (the original: 50000; 0: no\n"
    "# repair costs and no message).  Only the decompiled build has it.\n"
    "terror_damage = 50000\n"
    "# the Sammy's prize money for its winner, by category (the best news, cultural\n"
    "# engagement, self-produced shows, self-produced series), in dollars, 0 to\n"
    "# 100000000 (the original: 0, the Sammy brings image only).  Only the decompiled build.\n"
    "sammy_news = 0\n"
    "sammy_culture = 0\n"
    "sammy_shows = 0\n"
    "sammy_series = 0\n"
    "# a fourth game speed in the planner's Control-Center, twice the fastest of\n"
    "# the original's three (no: the original's three).  Only the decompiled build.\n"
    "fourth_speed = no\n"
    "# room changes without the original's wipe and loading wait, as in a network\n"
    "# game (the game's clock stands in a room change either way; no: the original's\n"
    "# pace).  Only the decompiled build.\n"
    "fast_rooms = no\n"
    "# the series card shows how topical the series still is, a bar as on the film\n"
    "# card: all its episodes' topicality against their quality (no: the original's\n"
    "# card without it).  Only the decompiled build.\n"
    "series_topicality = yes\n"
    "# computer players build up their transmitter network when you build up yours,\n"
    "# a while later: off (the original), normal, or strong.  Only the decompiled build.\n"
    "ai_improve = off\n"
    "# the contracts the ad agency offers at a time, 10 to 17 (the original: 10, its\n"
    "# three stacks of folders; from 11 the folders stand in a front and a back\n"
    "# row).  Only the decompiled build has it.\n"
    "ad_offers = 10\n"
    "# the films and series the film dealer offers at a time, 10 to 17 (the\n"
    "# original: 10, the row of tapes on his counter; from 11 the row grows to the\n"
    "# left).  Only the decompiled build has it.\n"
    "film_offers = 10\n"
    "\n"
    "[sound]\n"
    "sound = on\n"
    "\n"
    "[input]\n"
    "# captured: the game takes the mouse (Pause releases it); free: the system\n"
    "# pointer moves the game's cursor, with the system's speed, and can leave the window;\n"
    "# system: like free, and the system's own arrow is the game's (no lag; the game's\n"
    "# other cursors stay; only the decompiled build)\n"
    "mouse = captured\n";

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = 0;
    return s;
}
/* the first line without a UTF-8 byte order mark (Windows' Notepad writes one): else
 * "\xEF\xBB\xBF[display]" is no section, its keys land in "", and add_missing appends a
 * [display] with the defaults, which win from then on */
static char *skip_bom(char *s) {
    return (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF ? s + 3 : s;
}

#ifdef _WIN32
static void default_path(void) {
    const char *a = getenv("APPDATA"), *h = getenv("USERPROFILE");
    if (a && *a) snprintf(path, sizeof path, "%s\\madtv\\settings.ini", a);
    else snprintf(path, sizeof path, "%s\\madtv\\settings.ini", h && *h ? h : ".");
}
#define IS_SEP(c) ((c) == '/' || (c) == '\\')
#define MKDIR(d) _mkdir(d)
#else
static void default_path(void) {
    const char *x = getenv("XDG_CONFIG_HOME"), *h = getenv("HOME");
    if (x && *x) snprintf(path, sizeof path, "%s/madtv/settings.ini", x);
    else snprintf(path, sizeof path, "%s/.config/madtv/settings.ini", h ? h : ".");
}
#define IS_SEP(c) ((c) == '/')
#define MKDIR(d) mkdir(d, 0755)
#endif

/* mkdir -p of the file's directory */
static void make_dirs(void) {
    char d[sizeof path];
    snprintf(d, sizeof d, "%s", path);
    for (char *p = d + 1; *p; p++)
        if (IS_SEP(*p)) { char c = *p; *p = 0; MKDIR(d); *p = c; }
}

/* text put together in memory (what open_memstream does where there is one) */
typedef struct { char *p; size_t n, cap; } Buf;
static void buf_add(Buf *b, const char *s, size_t n) {
    if (b->n + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 1024;
        while (cap < b->n + n + 1) cap *= 2;
        char *q = realloc(b->p, cap);
        if (!q) return;
        b->p = q; b->cap = cap;
    }
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = 0;
}
static void buf_puts(Buf *b, const char *s) { buf_add(b, s, strlen(s)); }
static void buf_printf(Buf *b, const char *fmt, ...) {
    char tmp[2048];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    if (n > 0) buf_add(b, tmp, (size_t)n < sizeof tmp ? (size_t)n : sizeof tmp - 1);
}

/* the new file over the old one (Windows' rename does not replace a file) */
static int replace_file(const char *from, const char *to) {
#ifdef _WIN32
    return MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING) ? 0 : -1;
#else
    return rename(from, to);
#endif
}

/* append the default settings the file does not have yet, with their comments */
static void add_missing(void) {
    Buf o = {0};
    char sec[32] = "", added_sec[32] = "", comment[1024] = "";
    int n = 0;
    const char *p = defaults;
    while (*p) {
        const char *e = strchr(p, '\n');
        size_t l = e ? (size_t)(e - p) : strlen(p);
        char line[256];
        snprintf(line, sizeof line, "%.*s", (int)l, p);
        p += l + (e ? 1 : 0);
        char *s = trim(line);
        if (*s == '[') {
            char *b = strchr(s, ']');
            if (b) { *b = 0; snprintf(sec, sizeof sec, "%s", s + 1); }
            comment[0] = 0;
        } else if (*s == '#') {
            if (strlen(comment) + strlen(s) + 2 < sizeof comment) { strcat(comment, s); strcat(comment, "\n"); }
        } else if (*s) {
            char *eq = strchr(s, '=');
            if (eq) {
                *eq = 0;
                char *key = trim(s), *val = trim(eq + 1);
                if (!settings_get(sec, key)) {
                    if (!n++) buf_puts(&o, "\n# added by a newer version (its defaults):\n");
                    if (strcmp(added_sec, sec)) {
                        buf_printf(&o, "%s[%s]\n", added_sec[0] ? "\n" : "", sec);
                        snprintf(added_sec, sizeof added_sec, "%s", sec);
                    }
                    buf_printf(&o, "%s%s = %s\n", comment, key, val);
                    if (nvals < MAXV) {
                        snprintf(vals[nvals].sec, sizeof vals[nvals].sec, "%s", sec);
                        snprintf(vals[nvals].key, sizeof vals[nvals].key, "%s", key);
                        snprintf(vals[nvals].val, sizeof vals[nvals].val, "%s", val);
                        nvals++;
                    }
                }
            }
            comment[0] = 0;
        } else {
            comment[0] = 0;
        }
    }
    if (n) {
        FILE *f = fopen(path, "a");
        if (f) { fwrite(o.p, 1, o.n, f); fclose(f); fprintf(stderr, "[settings] added %d new setting%s to %s\n", n, n > 1 ? "s" : "", path); }
    }
    free(o.p);
}

int settings_load(const char *file) {
    if (file && *file) snprintf(path, sizeof path, "%s", file);
    else default_path();
    FILE *f = fopen(path, "r");
    if (!f && errno == ENOENT) {
        make_dirs();
        FILE *w = fopen(path, "w");
        if (w) {
            fputs(defaults, w);
            fclose(w);
            fprintf(stderr, "[settings] created %s with the defaults\n", path);
        }
        f = fopen(path, "r");
    }
    if (!f) { fprintf(stderr, "[settings] %s: %s\n", path, strerror(errno)); return 0; }
    char line[2048], sec[32] = "";
    int n = 0;
    while (fgets(line, sizeof line, f)) {
        n++;
        char *s = trim(n == 1 ? skip_bom(line) : line);
        if (!*s || *s == '#' || *s == ';') continue;
        if (*s == '[') {
            char *e = strchr(s, ']');
            if (e) { *e = 0; snprintf(sec, sizeof sec, "%s", trim(s + 1)); }
            continue;
        }
        char *eq = strchr(s, '=');
        if (!eq) { fprintf(stderr, "[settings] %s:%d: not 'key = value'\n", path, n); continue; }
        *eq = 0;
        if (nvals == MAXV) break;
        snprintf(vals[nvals].sec, sizeof vals[nvals].sec, "%s", sec);
        snprintf(vals[nvals].key, sizeof vals[nvals].key, "%s", trim(s));
        snprintf(vals[nvals].val, sizeof vals[nvals].val, "%s", trim(eq + 1));
        nvals++;
    }
    fclose(f);
    add_missing();
    settings_active = 1;
    return 1;
}

const char *settings_get(const char *sec, const char *key) {
    for (int i = nvals - 1; i >= 0; i--)          /* the last one counts */
        if (!strcmp(vals[i].sec, sec) && !strcmp(vals[i].key, key)) return vals[i].val;
    return NULL;
}

/* fopen for a path from either place: the command line has it in the ANSI code page (as
   fopen takes it), this file in UTF-8 (the launcher writes it) -- on Windows a name with
   letters beyond ASCII (a user's folder: C:\Users\Jürgen\...) opens only one way or the other */
FILE *settings_fopen(const char *p, const char *mode) {
    FILE *f = fopen(p, mode);
#ifdef _WIN32
    wchar_t wp[1024], wmode[8];
    if (!f && MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, p, -1, wp, 1024)
        && MultiByteToWideChar(CP_UTF8, 0, mode, -1, wmode, 8))
        f = _wfopen(wp, wmode);
#endif
    return f;
}

/* yes/no, on/off, true/false, 1/0 -> 1/0; -1 if absent or neither */
int settings_bool(const char *v) {
    if (!v) return -1;
    if (!strcmp(v, "yes") || !strcmp(v, "on") || !strcmp(v, "true") || !strcmp(v, "1")) return 1;
    if (!strcmp(v, "no") || !strcmp(v, "off") || !strcmp(v, "false") || !strcmp(v, "0")) return 0;
    fprintf(stderr, "[settings] '%s' is not yes or no\n", v);
    return -1;
}

/* Remember a value: rewrite its line (or add it to its section, or the
 * section at the end), everything else as it was. */
void settings_set(const char *sec, const char *key, const char *val) {
    if (!settings_active) return;
    FILE *f = fopen(path, "r");
    if (!f) return;
    Buf o = {0};
    Buf h = {0};                         /* blank and comment lines after the section's last value */
    char line[2048], cur[32] = "";
    int done = 0, first = 1;
    while (fgets(line, sizeof line, f)) {
        char copy[2048];
        snprintf(copy, sizeof copy, "%s", line);
        char *s = trim(first ? skip_bom(copy) : copy);
        first = 0;
        int in_sec = !done && !strcmp(cur, sec);
        if (*s == '[') {
            if (in_sec) { buf_printf(&o, "%s = %s\n", key, val); done = 1; }   /* at the end of its section */
            char *e = strchr(s, ']');
            if (e) { *e = 0; snprintf(cur, sizeof cur, "%s", trim(s + 1)); }
        } else if (in_sec && (!*s || *s == '#' || *s == ';')) {
            buf_puts(&h, line);
            continue;
        } else if (in_sec) {
            char *eq = strchr(s, '=');
            if (eq) {
                *eq = 0;
                if (!strcmp(trim(s), key)) {
                    if (h.n) { buf_add(&o, h.p, h.n); h.n = 0; }
                    buf_printf(&o, "%s = %s\n", key, val);
                    done = 1;
                    continue;
                }
            }
        }
        if (h.n) { buf_add(&o, h.p, h.n); h.n = 0; }
        buf_puts(&o, line);
    }
    fclose(f);
    if (!done) {
        if (strcmp(cur, sec)) buf_printf(&o, "\n[%s]\n", sec);
        buf_printf(&o, "%s = %s\n", key, val);
    }
    if (h.n) buf_add(&o, h.p, h.n);
    free(h.p);
    char tmp[sizeof path + 8];
    snprintf(tmp, sizeof tmp, "%s.new", path);
    FILE *w = fopen(tmp, "w");
    if (w && fwrite(o.p, 1, o.n, w) == o.n && fclose(w) == 0) replace_file(tmp, path);
    else if (w) fclose(w);
    free(o.p);
    int i;
    for (i = nvals - 1; i >= 0; i--)
        if (!strcmp(vals[i].sec, sec) && !strcmp(vals[i].key, key)) break;
    if (i < 0 && nvals < MAXV) {
        i = nvals++;
        snprintf(vals[i].sec, sizeof vals[i].sec, "%s", sec);
        snprintf(vals[i].key, sizeof vals[i].key, "%s", key);
    }
    if (i >= 0) snprintf(vals[i].val, sizeof vals[i].val, "%s", val);
}
