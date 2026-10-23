/* Loader and entry point: builds a DOS process image in emulated memory and
 * starts it, either through the recompiled code or the interpreter. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "runtime.h"
#include "seat.h"

extern char *script_path;
extern int host_view_seat;
void host_net_start(const char *server, int seat, const char *params);
extern const char *net_key;
static const char *mp_server;
static int mp_seat = -1;
extern int dos_trace;
extern int bios_trace;
void interp_run_forever(void);
void native_entry_point(void);

static u32 image_bytes;
static void load_exe(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); exit(1); }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    u8 *d = malloc(sz);
    if (fread(d, 1, sz, f) != (size_t)sz) { perror("read"); exit(1); }
    fclose(f);
#define W(o) (d[o] | (d[(o) + 1] << 8))
    if (d[0] != 'M' || d[1] != 'Z') { fprintf(stderr, "%s: not an MZ exe\n", path); exit(1); }
    u32 hdr = W(8) * 16, total = (W(4) - 1) * 512 + (W(2) ? W(2) : 512);
    u32 img = total - hdr;
    memcpy(M + lin(LOADSEG, 0), d + hdr, img);
    for (int i = 0; i < W(6); i++) {
        u32 r = W(0x18) + 4 * i;
        u16 o = W(r), s = W(r + 2);
        u16 v = rw(LOADSEG + s, o);
        ww(LOADSEG + s, o, v + LOADSEG);
    }
    R.ss = LOADSEG + W(0x0E); R.sp = W(0x10);
    R.cs = LOADSEG + W(0x16); R.ip = W(0x14);
    R.ds = R.es = PSP_SEG;
    R.ax = 0; R.ifl = 1;
    image_bytes = img;
    fprintf(stderr, "[load] %s: %u bytes at %04X, %d relocs, entry %04X:%04X\n", path, img, LOADSEG, W(6), R.cs, R.ip);
    free(d);
}

/* The same start without the EXE (runtime.h, struct builtin_image): the data
 * segment as the EXE has it, relocated like load_exe does, the start values;
 * the code segments stay empty but for what code_init writes. */
static void load_builtin(const struct builtin_image *b) {
    u32 base = lin(LOADSEG, 0) + b->data_off;
    memcpy(M + base, b->data, b->data_len);
    for (u32 i = 0; i < b->nrel; i++) {
        u32 a = base + b->rel[i];
        u16 v = (u16)(M[a] | M[a + 1] << 8) + LOADSEG;
        M[a] = (u8)v; M[a + 1] = (u8)(v >> 8);
    }
    b->code_init();
    R.ss = LOADSEG + b->ss; R.sp = b->sp;
    R.cs = LOADSEG + b->cs; R.ip = b->ip;
    R.ds = R.es = PSP_SEG;
    R.ax = 0; R.ifl = 1;
    image_bytes = b->image_bytes;
    exe_code_loaded = 0;
    fprintf(stderr, "[load] built in: data segment %u bytes at %04X, %u of %u relocs, entry %04X:%04X (no MTV.EXE needed)\n",
            b->data_len, (u16)(LOADSEG + (b->data_off >> 4)), b->nrel, b->nrel_exe, R.cs, R.ip);
}

/* --write-data FILE: that data segment, relocated as loaded, into a file --
 * for the tools that read the original's data (the editor's font) where
 * there is no MTV.EXE */
static int write_data_image(const char *path) {
    const struct builtin_image *b = builtin_image;
    if (!b) { fprintf(stderr, "--write-data: this build has no built-in image (build/mtv-dec has)\n"); return 2; }
    u8 *d = malloc(b->data_len);
    memcpy(d, b->data, b->data_len);
    for (u32 i = 0; i < b->nrel; i++) {
        u16 v = (u16)(d[b->rel[i]] | d[b->rel[i] + 1] << 8) + LOADSEG;
        d[b->rel[i]] = (u8)v; d[b->rel[i] + 1] = (u8)(v >> 8);
    }
    FILE *f = fopen(path, "wb");
    int bad = !f || fwrite(d, 1, b->data_len, f) != b->data_len;
    if (f && fclose(f)) bad = 1;
    free(d);
    if (bad) { perror(path); return 1; }
    return 0;
}

const char *fix_spec;                       /* --fixes[=LIST]; NULL = classic */
long start_money_opt = -1;                  /* --start-money=N; -1 = the original's $200 000 */
long credit_line_opt = -1;                  /* --credit-line=N: the boss's credit line (image 10..49); -1 = the original's $300 000; CREDIT_LINE_ORIGINAL: that one chosen */
long terror_damage_opt = -1;                /* --terror-damage=N: what a terrorist's bomb costs the room's owner; -1 = the original's $50 000 */
long sammy_prize_opt[4] = {-1, -1, -1, -1}; /* --sammy-prize=N[,N,N,N]: the Sammy's prize money (news, culture, shows, series); -1 = none */
int fourth_speed_opt;                       /* --fourth-speed[=yes|no]: a fourth, fastest speed button */
int fast_rooms_opt;                         /* --fast-rooms[=yes|no]: room changes without the original's waits (runtime.h) */
int ai_improve_opt;                         /* --ai-improve=off|normal|strong: 0, 1, 2 -- computer players follow the human's transmitters (src/ai_improve.c) */
int ad_offers_opt;                          /* --ad-offers=N: the ad agency's offers, 10..17; 0 = the original's 10 (src/fixes.c) */
int film_offers_opt;                        /* --film-offers=N: the film dealer's offers, 10..17; 0 = the original's 10 (src/film_offers.c) */
int series_top_opt;                         /* --series-topicality[=yes|no]: the series card shows the series' topicality (runtime.h) */
int new_game_opt;                           /* --new-game: straight into a new game (no intro, no setup) */
int load_slot_opt;                          /* --load-slot N: straight into the saved game of slot N (1..10; src/main_loop.c) */
int difficulty_opt;                         /* --difficulty N: 1 easy, 2 normal, 3 difficult (with --new-game) */
char new_game_names[3][9];                  /* --names A,B,C: the managers' names (8 characters, the setup's limit) */
int notice_german;                          /* the game in German: the window's notices too (host.c) */
const char *ai_pictures_path;               /* --ai-pictures FILE, [display] ai_pictures (src/hr.c) */
int knock_opt;                              /* --knock=N: knocking at an occupied shared room gives its human N game minutes (multiplayer; 0 off) */
long seed_opt;                              /* --seed N (0..65535): the setup's time, for --new-game */
const char *mp_save_dir;                    /* --mp-save DIR: a network game saves each night (src/mpsave.c) */
int mp_load_opt;                            /* --mp-load FILE: a network game from that savegame */
const u8 *mp_load_data;
u32 mp_load_size;
u64 mp_files_hash;                          /* the game files' hash (a network game's files=) */
static u64 mp_load_hash;                    /* FNV-1a of the savegame file (a network game's start=load:) */

/* --mp-load FILE: read it whole; 0 and a message if it is no savegame */
static int read_mp_load(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return 0; }
    u8 *d = NULL;
    size_t n = 0, cap = 0, r;
    do {
        if (n == cap) { u8 *g = realloc(d, cap = cap ? cap * 2 : 65536); if (!g) { free(d); fclose(f); return 0; } d = g; }
        r = fread(d + n, 1, cap - n, f);
        n += r;
    } while (r > 0 && n < (64u << 20));
    fclose(f);
    char why[160];
    if (!mpsave_check(d, (u32)n, why, sizeof why)) { fprintf(stderr, "--mp-load %s: %s\n", path, why); free(d); return 0; }
    u64 h = 1469598103934665603ull;
    for (size_t i = 0; i < n; i++) { h ^= d[i]; h *= 1099511628211ull; }
    mp_load_data = d; mp_load_size = (u32)n; mp_load_hash = h;
    return 1;
}

/* --names A,B,C: up to 8 printable ASCII characters each, an empty one keeps the game's */
static int parse_names(const char *s) {
    int k = 0, n = 0;
    memset(new_game_names, 0, sizeof new_game_names);
    for (; *s; s++) {
        if (*s == ',') { if (++k > 2) return 0; n = 0; continue; }
        if ((unsigned char)*s < 0x20 || (unsigned char)*s > 0x7E || n >= 8) return 0;
        new_game_names[k][n++] = *s;
    }
    return 1;
}
/* the names in a network game's parameters: %XX for space, comma and percent */
static void names_param(char *out, size_t cap) {
    size_t o = 0;
    for (int k = 0; k < 3; k++) {
        if (k && o + 1 < cap) out[o++] = ',';
        for (const char *c = new_game_names[k]; *c && o + 4 < cap; c++) {
            if (*c == ' ' || *c == ',' || *c == '%') o += (size_t)snprintf(out + o, cap - o, "%%%02X", (unsigned char)*c);
            else out[o++] = *c;
        }
    }
    out[o] = 0;
}
/* the decompiled build overrides this (src/fixes.c) */
__attribute__((weak)) int fix_init(void) { return 0; }

/* --aspect / --filter values, from the command line or the settings file */
static int parse_aspect(const char *v) {           /* 1 square, 0 4:3, -1 bad */
    if (!strcmp(v, "square") || !strcmp(v, "1:1") || !strcmp(v, "16:10")) return 1;
    if (!strcmp(v, "4:3")) return 0;
    fprintf(stderr, "aspect: 4:3 or square, not '%s'\n", v);
    return -1;
}
static int parse_mouse(const char *v) {            /* 2 system, 1 free, 0 captured, -1 bad */
    if (!strcmp(v, "system")) return 2;
    if (!strcmp(v, "free")) return 1;
    if (!strcmp(v, "captured")) return 0;
    fprintf(stderr, "mouse: captured, free or system, not '%s'\n", v);
    return -1;
}
static int parse_ai_improve(const char *v) {       /* 0 off, 1 normal, 2 strong, -1 bad */
    if (!strcmp(v, "off") || !strcmp(v, "no") || !strcmp(v, "0")) return 0;
    if (!strcmp(v, "normal") || !strcmp(v, "1")) return 1;
    if (!strcmp(v, "strong") || !strcmp(v, "2")) return 2;
    return -1;
}
static int parse_offers(const char *what, const char *v, int lo, int hi) {   /* lo..hi offers, -1 bad */
    char *e;
    long n = strtol(v, &e, 10);
    if (*v && !*e && n >= lo && n <= hi) return (int)n;
    fprintf(stderr, "%s: from %d to %d offers, not '%s'\n", what, lo, hi, v);
    return -1;
}
#define parse_ad_offers(what, v) parse_offers((what), (v), AD_OFFERS_OPT_MIN, AD_OFFERS_OPT_MAX)
#define parse_film_offers(what, v) parse_offers((what), (v), FILM_OFFERS_OPT_MIN, FILM_OFFERS_OPT_MAX)
static long parse_money(const char *v) {           /* dollars 0..START_MONEY_MAX, -1 bad */
    char *e;
    long n = strtol(v, &e, 10);
    if (*v && !*e && n >= 0 && n <= START_MONEY_MAX) return n;
    fprintf(stderr, "start money: dollars from 0 to %ld, not '%s'\n", START_MONEY_MAX, v);
    return -1;
}
static long parse_credit_line(const char *v) {     /* dollars CREDIT_LINE_MIN..CREDIT_LINE_MAX in steps of 100 000, or "original" (0: a network game's parameter), -1 bad */
    char *e;
    if (!strcmp(v, "original")) return CREDIT_LINE_ORIGINAL;
    long n = strtol(v, &e, 10);
    if (*v && !*e && (n == CREDIT_LINE_ORIGINAL || (n >= CREDIT_LINE_MIN && n <= CREDIT_LINE_MAX && n % 100000 == 0))) return n;
    fprintf(stderr, "credit line: dollars from %ld to %ld in steps of 100000, or original, not '%s'\n", CREDIT_LINE_MIN, CREDIT_LINE_MAX, v);
    return -1;
}
static long parse_dollars(const char *what, const char *v) {   /* dollars 0..MONEY_OPT_MAX, -1 bad */
    char *e;
    long n = strtol(v, &e, 10);
    if (*v && !*e && n >= 0 && n <= MONEY_OPT_MAX) return n;
    fprintf(stderr, "%s: dollars from 0 to %ld, not '%s'\n", what, MONEY_OPT_MAX, v);
    return -1;
}
static int parse_sammy_prize(const char *v, long out[4]) {   /* N (every category) or NEWS,CULTURE,SHOWS,SERIES; 0 ok, -1 bad */
    char part[24];
    long n[4];
    int k = 0;
    for (const char *p = v;; k++) {
        const char *e = strchr(p, ',');
        size_t len = e ? (size_t)(e - p) : strlen(p);
        if (k == 4 || len >= sizeof part) { k = -1; break; }
        memcpy(part, p, len);
        part[len] = 0;
        if ((n[k] = parse_dollars("sammy prize", part)) < 0) return -1;
        if (!e) { k++; break; }
        p = e + 1;
    }
    if (k != 1 && k != 4) {
        fprintf(stderr, "sammy prize: one amount or four (news, culture, shows, series), not '%s'\n", v);
        return -1;
    }
    for (int i = 0; i < 4; i++) out[i] = n[k == 1 ? 0 : i];
    return 0;
}
static int parse_filter(const char *v) {
    if (!strcmp(v, "sharp")) return DISPLAY_SHARP;
    if (!strcmp(v, "nearest")) return DISPLAY_NEAREST;
    if (!strcmp(v, "linear")) return DISPLAY_LINEAR;
    if (!strcmp(v, "mmpx")) return DISPLAY_MMPX;
    if (!strcmp(v, "hr-pixel") || !strcmp(v, "hr")) return DISPLAY_HR_PIXEL;   /* (hr: an older setting, the pictures smoothed -- gone) */
    fprintf(stderr, "filter: sharp, nearest, linear, mmpx or hr-pixel, not '%s'\n", v);
    return -1;
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++)       /* --make-ki: AI pictures, no game (src/ki_make.c, docs/ki-bilder.md) */
        if (!strcmp(argv[i], "--make-ki")) return ki_make_main(argc - 1, argv + 1);
    const char *gamedir = ".", *exe = NULL;
    int headless = 0, sc = 3;
    double mips = 10.0;
    static struct option opts[] = {
        {"interp", no_argument, 0, 'i'}, {"headless", no_argument, 0, 'H'},
        {"script", required_argument, 0, 's'}, {"game", required_argument, 0, 'g'},
        {"exe", required_argument, 0, 'e'}, {"mips", required_argument, 0, 'm'},
        {"trace-dos", no_argument, 0, 'D'}, {"trace-calls", no_argument, 0, 'C'},
        {"scale", required_argument, 0, 'S'}, {"fast", no_argument, 0, 'F'},
        {"difftest", no_argument, 0, 'T'}, {"wav", required_argument, 0, 'W'},
        {"nosound", no_argument, 0, 'N'}, {"args", required_argument, 0, 'A'},
        {"record", required_argument, 0, 'R'}, {"dump-on", required_argument, 0, 'O'},
        {"clamp", required_argument, 0, 'K'}, {"stackchk", no_argument, 0, 'U'},
        {"fixes", optional_argument, 0, 'X'}, {"classic", no_argument, 0, 'L'},
        {"write-data", required_argument, 0, 'w'},
        {"seats", required_argument, 0, 'P'}, {"humans", required_argument, 0, 'J'},
        {"mp", required_argument, 0, 'Q'}, {"seat", required_argument, 0, 'q'}, {"view-seat", required_argument, 0, 'Z'},
        {"logical-time", no_argument, 0, 'Y'},
        {"fullscreen", no_argument, 0, 'f'}, {"aspect", required_argument, 0, 'a'},
        {"filter", required_argument, 0, 'r'}, {"ai-pictures", required_argument, 0, 'y'}, {"integer", no_argument, 0, 'I'}, {"cast-fit", optional_argument, 0, 'x'},
        {"settings", optional_argument, 0, 'G'}, {"mouse", required_argument, 0, 'u'},
        {"start-money", required_argument, 0, 'M'}, {"fourth-speed", optional_argument, 0, 'V'},
        {"fast-rooms", optional_argument, 0, 'o'}, {"ai-improve", required_argument, 0, 'b'},
        {"credit-line", required_argument, 0, 'c'}, {"terror-damage", required_argument, 0, 't'},
        {"sammy-prize", required_argument, 0, 'p'}, {"ad-offers", required_argument, 0, 'B'},
        {"film-offers", required_argument, 0, 'E'}, {"series-topicality", optional_argument, 0, 'j'},
        {"new-game", no_argument, 0, 'n'}, {"difficulty", required_argument, 0, 'd'},
        {"names", required_argument, 0, 'k'}, {"seed", required_argument, 0, 'z'},
        {"mp-save", required_argument, 0, 0x100}, {"mp-load", required_argument, 0, 0x101},
        {"knock", required_argument, 0, 0x102}, {"mp-key", required_argument, 0, 0x103},
        {"load-slot", required_argument, 0, 0x104}, {0, 0, 0, 0}};
#ifdef _WIN32
    setvbuf(stderr, NULL, _IONBF, 0);     /* as on Linux and macOS: a crash or a kill keeps the messages */
#endif
    int c, fast = 0;
    const char *wav_path = NULL, *prog_args = NULL, *record_path = NULL, *dump_on = NULL;
    char given[128] = {0};                /* options on the command line: the settings file does not override them */
    const char *settings_file = NULL;
    int fixes_from_settings = 0, money_from_settings = 0, credit_from_settings = 0, terror_from_settings = 0, sammy_from_settings = 0, mouse_from_settings = 0, speed_from_settings = 0, rooms_from_settings = 0, aiimp_from_settings = 0, offers_from_settings = 0, films_from_settings = 0, fit_from_settings = 0, sertop_from_settings = 0, m;
    while ((c = getopt_long(argc, argv, "", opts, NULL)) != -1) {
        if (c > 0 && c < 128) given[c] = 1;
        switch (c) {
        case 'i': use_native = 0; break;
        case 'Y': logical_time = 1; break;
        case 'H': headless = 1; break;
        case 's': script_path = optarg; break;
        case 'g': gamedir = optarg; break;
        case 'e': exe = optarg; break;
        case 'w': return write_data_image(optarg);
        case 'P':                            /* the seat scheduler (runtime/seat.h; multiplayer stage 1) */
            seats_n = atoi(optarg);
            if (seats_n < 1 || seats_n > SEATS_MAX) { fprintf(stderr, "--seats: 1..%d\n", SEATS_MAX); return 2; }
            if (seats_n > 2) { fprintf(stderr, "--seats: 1, or 2 with a spectator (multiplayer stage 2)\n"); return 2; }
            break;
        case 'Q': mp_server = optarg; break;    /* the network game's server (multiplayer stage 4, net.h) */
        case 'n': new_game_opt = 1; break;
        case 0x100: mp_save_dir = optarg; break;       /* a network game saves each night there (src/mpsave.c) */
        case 0x103:                                     /* the seat's secret from the lobby (net.c's HELLO) */
            if (!*optarg || strlen(optarg) > 64 || strspn(optarg, "0123456789abcdef") != strlen(optarg)) { fprintf(stderr, "--mp-key: up to 64 hex digits\n"); return 2; }
            net_key = optarg;
            break;
        case 0x102: {                                   /* knocking at the film dealer, ad agency, supermarket (src/seatctx.c) */
            char *e; long n = strtol(optarg, &e, 10);
            if (!*optarg || *e || n < 0 || n > KNOCK_OPT_MAX) { fprintf(stderr, "--knock: game minutes from 0 to %d, not '%s'\n", KNOCK_OPT_MAX, optarg); return 2; }
            knock_opt = (int)n;
            break;
        }
        case 0x104: {                                   /* a saved game of the safe (room 58): no intro, the setup's "Load game" */
            char *e; long n = strtol(optarg, &e, 10);
            if (!*optarg || *e || n < 1 || n > 10) { fprintf(stderr, "--load-slot: a slot of the safe, 1 to 10, not '%s'\n", optarg); return 2; }
            load_slot_opt = (int)n;
            break;
        }
        case 0x101:                                     /* a network game from a savegame: no intro, no setup */
            if (!read_mp_load(optarg)) return 2;
            mp_load_opt = 1; new_game_opt = 1;
            break;
        case 'k':
            if (!parse_names(optarg)) { fprintf(stderr, "--names A,B,C: up to 8 characters each (letters, digits, space, punctuation)\n"); return 2; }
            break;
        case 'z':
            seed_opt = atol(optarg);
            if (seed_opt < 0 || seed_opt > 65535) { fprintf(stderr, "--seed: 0..65535\n"); return 2; }
            break;
        case 'd':
            difficulty_opt = atoi(optarg);
            if (difficulty_opt < 1 || difficulty_opt > 3) { fprintf(stderr, "--difficulty: 1 (easy), 2 (normal) or 3 (difficult)\n"); return 2; }
            break;
        case 'q': mp_seat = atoi(optarg); break;
        case 'Z': host_view_seat = atoi(optarg); break;   /* the seat whose screen is shown */
        case 'J': {                          /* human seats: their stations (multiplayer stage 3) */
            int n = 0;
            for (const char *p = optarg; *p && n < SEATS_MAX; ) {
                int st = atoi(p);
                if (st < 1 || st > 3) break;
                for (int k = 0; k < n; k++) if (seat_station[k] == st) st = 0;
                if (!st) break;
                seat_station[n++] = st;
                p = strchr(p, ','); if (!p) break; p++;
            }
            if (n < 2) { fprintf(stderr, "--humans A,B[,C]: the stations (1..3) of seat 0, seat 1 (and seat 2), all different\n"); return 2; }
            seats_n = n;
            seat_humans = n;
            break;
        }
        case 'm': mips = atof(optarg); break;
        case 'D': dos_trace = 1; bios_trace = 1; break;
        case 'C': trace_calls = 1; break;
        case 'S': sc = atoi(optarg); break;
        case 'f': display_fullscreen = 1; break;
        case 'a':                       /* 4:3 like a VGA monitor (default) or square pixels */
            if ((display_square = parse_aspect(optarg)) < 0) return 2;
            break;
        case 'r':
            if ((display_filter = parse_filter(optarg)) < 0) return 2;
            break;
        case 'y': ai_pictures_path = optarg; break;
        case 'G': settings_file = optarg ? optarg : ""; break;   /* the player's settings (settings.c) */
        case 'u':
            if ((m = parse_mouse(optarg)) < 0) return 2;
            mouse_free = m > 0; mouse_system = m == 2;
            break;
        case 'M': if ((start_money_opt = parse_money(optarg)) < 0) return 2; break;   /* the human's cash in a new game */
        case 'c': if ((credit_line_opt = parse_credit_line(optarg)) < 0) return 2; break;   /* the boss's credit line */
        case 't': if ((terror_damage_opt = parse_dollars("terror damage", optarg)) < 0) return 2; break;   /* a terrorist's bomb */
        case 'p': if (parse_sammy_prize(optarg, sammy_prize_opt) < 0) return 2; break;   /* the Sammy's prize money */
        case 'V':                       /* a fourth speed button in the planner's Control-Center */
            if ((fourth_speed_opt = optarg ? settings_bool(optarg) : 1) < 0) { fprintf(stderr, "--fourth-speed: yes or no, not '%s'\n", optarg); return 2; }
            break;
        case 'x':                       /* the actors' fit for the role in the shop */
            if ((cast_fit_opt = optarg ? settings_bool(optarg) : 1) < 0) { fprintf(stderr, "--cast-fit: yes or no, not '%s'\n", optarg); return 2; }
            break;
        case 'o':                       /* room changes without the original's waits */
            if ((fast_rooms_opt = optarg ? settings_bool(optarg) : 1) < 0) { fprintf(stderr, "--fast-rooms: yes or no, not '%s'\n", optarg); return 2; }
            break;
        case 'B': if ((ad_offers_opt = parse_ad_offers("--ad-offers", optarg)) < 0) return 2; break;   /* the ad agency's offers */
        case 'E': if ((film_offers_opt = parse_film_offers("--film-offers", optarg)) < 0) return 2; break;   /* the film dealer's offers */
        case 'j':                       /* the series card's topicality bar */
            if ((series_top_opt = optarg ? settings_bool(optarg) : 1) < 0) { fprintf(stderr, "--series-topicality: yes or no, not '%s'\n", optarg); return 2; }
            break;
        case 'b':                       /* computer players follow the human's transmitter network */
            if ((ai_improve_opt = parse_ai_improve(optarg)) < 0) { fprintf(stderr, "--ai-improve: off, normal or strong, not '%s'\n", optarg); return 2; }
            break;
        case 'I': display_integer = 1; break;
        case 'F': fast = 1; break;
        case 'T': difftest_on = 1; use_native = 0; stackchk_on = 1; atexit(difftest_summary); break;
        case 'U': stackchk_on = 1; stk_who_on = 1; use_native = 0; break;   /* report reads of never-written locals */
        case 'X': fix_spec = optarg ? optarg : "all"; break; /* original-bug fixes (decompiled build) */
        case 'L': fix_spec = NULL; break;
        case 'W': wav_path = optarg; break;
        case 'N': adlib_enabled = 0; break;
        case 'A': prog_args = optarg; break;
        case 'R': record_path = optarg; break;
        case 'O': dump_on = optarg; break;
        case 'K': {   /* SEG:OFF:MIN (test harness) */
            extern void host_clamp(u32, int);
            unsigned s_, o_; int mn;
            if (sscanf(optarg, "%x:%x:%d", &s_, &o_, &mn) != 3) { fprintf(stderr, "--clamp SEG:OFF:MIN\n"); return 2; }
            host_clamp(((u32)(LOADSEG + s_) << 4) + o_, mn);
        } break;
        default: fprintf(stderr, "usage: %s [--interp] [--stackchk] [--fixes[=LIST]|--classic] [--logical-time] [--headless] [--script f] [--record f] [--dump-on SEG:OFF:DIR] [--game dir] [--exe unpacked.exe] [--scale N] [--fullscreen] [--aspect 4:3|square] [--filter sharp|nearest|linear|mmpx|hr-pixel] [--ai-pictures SET.ki] [--integer] [--cast-fit[=yes|no]] [--mouse captured|free|system] [--start-money N] [--credit-line N|original] [--terror-damage N] [--sammy-prize N[,N,N,N]] [--ad-offers N] [--film-offers N] [--fourth-speed[=yes|no]] [--fast-rooms[=yes|no]] [--series-topicality[=yes|no]] [--ai-improve=off|normal|strong] [--settings[=FILE]] [--write-data FILE]\n", argv[0]); return 2;
        }
    }
    if (settings_file && settings_load(settings_file)) {      /* for what the command line left open */
        const char *v;
        int b;
        long l;
        if (!given['f'] && (b = settings_bool(settings_get("display", "fullscreen"))) >= 0) display_fullscreen = b;
        if (!given['S'] && (v = settings_get("display", "scale")) && atoi(v) > 0) sc = atoi(v);
        if (!given['a'] && (v = settings_get("display", "aspect")) && (b = parse_aspect(v)) >= 0) display_square = b;
        if (!given['r'] && (v = settings_get("display", "filter")) && (b = parse_filter(v)) >= 0) display_filter = b;
        if (!given['y'] && (v = settings_get("display", "ai_pictures")) && *v) ai_pictures_path = strdup(v);
        if (!given['I'] && (b = settings_bool(settings_get("display", "integer"))) >= 0) display_integer = b;
        if (!given['x'] && (b = settings_bool(settings_get("display", "cast_fit"))) >= 0) { cast_fit_opt = b; fit_from_settings = 1; }
        if (!given['X'] && !given['L'] && (v = settings_get("game", "fixes"))) {
            fix_spec = (!strcmp(v, "none") || !strcmp(v, "classic") || !*v) ? NULL : strdup(v);
            fixes_from_settings = 1;
        }
        if (!given['X'] && !given['L'] && settings_bool(settings_get("game", "classic")) == 1) {
            fix_spec = NULL;                /* classic: the original's game; "fixes" stays the choice for later */
            fixes_from_settings = 1;
        }
        if (!given['A'] && (v = settings_get("game", "language"))) {
            if (!strcmp(v, "english") || !strcmp(v, "e")) prog_args = "e";
            else if (!strcmp(v, "german") || !strcmp(v, "d")) prog_args = "d";
            else if (!strcmp(v, "ask")) prog_args = NULL;
            else fprintf(stderr, "[settings] language: english, german or ask, not '%s'\n", v);
        }
        if (!given['N'] && (b = settings_bool(settings_get("sound", "sound"))) == 0) adlib_enabled = 0;
        if (!given['u'] && (v = settings_get("input", "mouse")) && (b = parse_mouse(v)) >= 0) {
            mouse_free = b > 0; mouse_system = b == 2;
            mouse_from_settings = 1;
        }
        if (!given['M'] && (v = settings_get("game", "start_money")) && (l = parse_money(v)) >= 0) {
            start_money_opt = l;
            money_from_settings = 1;
        }
        if (!given['c'] && (v = settings_get("game", "credit_line")) && (l = parse_credit_line(v)) >= 0) {
            credit_line_opt = l;
            credit_from_settings = 1;
        }
        if (!given['t'] && (v = settings_get("game", "terror_damage")) && (l = parse_dollars("[settings] terror_damage", v)) >= 0) {
            terror_damage_opt = l;
            terror_from_settings = 1;
        }
        if (!given['p']) {                  /* one key per category, a missing one: none */
            static const char *const keys[4] = {"sammy_news", "sammy_culture", "sammy_shows", "sammy_series"};
            for (int i = 0; i < 4; i++)
                if ((v = settings_get("game", keys[i])) && (l = parse_dollars(keys[i], v)) >= 0) {
                    sammy_prize_opt[i] = l;
                    sammy_from_settings = 1;
                }
        }
        if (!given['B'] && (v = settings_get("game", "ad_offers")) && (b = parse_ad_offers("[settings] ad_offers", v)) >= 0) {
            ad_offers_opt = b;
            offers_from_settings = 1;
        }
        if (!given['E'] && (v = settings_get("game", "film_offers")) && (b = parse_film_offers("[settings] film_offers", v)) >= 0) {
            film_offers_opt = b;
            films_from_settings = 1;
        }
        if (!given['V'] && (b = settings_bool(settings_get("game", "fourth_speed"))) >= 0) {
            fourth_speed_opt = b;
            speed_from_settings = 1;
        }
        if (!given['o'] && (b = settings_bool(settings_get("game", "fast_rooms"))) >= 0) {
            fast_rooms_opt = b;
            rooms_from_settings = 1;
        }
        if (!given['j'] && (b = settings_bool(settings_get("game", "series_topicality"))) >= 0) {
            series_top_opt = b;
            sertop_from_settings = 1;
        }
        if (!given['b'] && (v = settings_get("game", "ai_improve"))) {
            if ((b = parse_ai_improve(v)) >= 0) { ai_improve_opt = b; aiimp_from_settings = 1; }
            else fprintf(stderr, "[settings] ai_improve: off, normal or strong, not '%s'\n", v);
        }
    }
    if (!fix_init() && fix_spec) {      /* not in this build: and not in a recording's header either */
        if (!fixes_from_settings) fprintf(stderr, "--fixes: only the decompiled build (build/mtv-dec) has them\n");
        fix_spec = NULL;
    }
    if (!fix_init() && start_money_opt >= 0) {     /* the machine-level build: always the original's */
        if (!money_from_settings) fprintf(stderr, "--start-money: only the decompiled build (build/mtv-dec) has it\n");
        start_money_opt = -1;
    }
    if (!fix_init() && credit_line_opt >= 0) {     /* the machine-level build: the original's credit line */
        if (!credit_from_settings) fprintf(stderr, "--credit-line: only the decompiled build (build/mtv-dec) has it\n");
        credit_line_opt = -1;
    }
    if (!fix_init() && terror_damage_opt >= 0) {   /* the machine-level build: the original's $50 000 */
        if (!terror_from_settings) fprintf(stderr, "--terror-damage: only the decompiled build (build/mtv-dec) has it\n");
        terror_damage_opt = -1;
    }
    if (!fix_init() && (sammy_prize_opt[0] >= 0 || sammy_prize_opt[1] >= 0 || sammy_prize_opt[2] >= 0 || sammy_prize_opt[3] >= 0)) {   /* the original: no prize */
        if (!sammy_from_settings) fprintf(stderr, "--sammy-prize: only the decompiled build (build/mtv-dec) has it\n");
        for (int i = 0; i < 4; i++) sammy_prize_opt[i] = -1;
    }
    if (!fix_init() && new_game_opt) {             /* the machine-level build: the original's start */
        fprintf(stderr, "--new-game: the decompiled build only (build/mtv-dec)\n");
        new_game_opt = 0;
    }
    if (!fix_init() && load_slot_opt) {
        fprintf(stderr, "--load-slot: the decompiled build only (build/mtv-dec)\n");
        load_slot_opt = 0;
    }
    if (load_slot_opt && (new_game_opt || seat_humans)) {   /* (a network game loads its own savegames: --mp-load) */
        fprintf(stderr, "--load-slot: a single player's game, not with --new-game, --mp-load or --humans\n");
        return 2;
    }
    if (mp_load_opt && !seat_humans) {             /* a network game's savegame: its humans' stations */
        fprintf(stderr, "--mp-load needs --humans (the savegame's stations)\n");
        return 2;
    }
    if (!fix_init() && ad_offers_opt > AD_OFFERS_OPT_MIN) {   /* the machine-level build: the original's 10 offers */
        if (!offers_from_settings) fprintf(stderr, "--ad-offers: only the decompiled build (build/mtv-dec) has it\n");
        ad_offers_opt = 0;
    }
    if (!fix_init() && film_offers_opt > FILM_OFFERS_OPT_MIN) {   /* the machine-level build: the original's 10 offers */
        if (!films_from_settings) fprintf(stderr, "--film-offers: only the decompiled build (build/mtv-dec) has it\n");
        film_offers_opt = 0;
    }
    if (!fix_init() && fourth_speed_opt) {         /* the machine-level build: the original's three speeds */
        if (!speed_from_settings) fprintf(stderr, "--fourth-speed: only the decompiled build (build/mtv-dec) has it\n");
        fourth_speed_opt = 0;
    }
    if (!fix_init() && fast_rooms_opt) {           /* the machine-level build: the original's room changes */
        if (!rooms_from_settings) fprintf(stderr, "--fast-rooms: only the decompiled build (build/mtv-dec) has it\n");
        fast_rooms_opt = 0;
    }
    if (!fix_init() && series_top_opt) {           /* the machine-level build: the original's series card */
        if (!sertop_from_settings) fprintf(stderr, "--series-topicality: only the decompiled build (build/mtv-dec) has it\n");
        series_top_opt = 0;
    }
    if (!fix_init() && ai_improve_opt) {           /* the machine-level build: the original's computer players */
        if (!aiimp_from_settings) fprintf(stderr, "--ai-improve: only the decompiled build (build/mtv-dec) has it\n");
        ai_improve_opt = 0;
    }
    if (!fix_init() && cast_fit_opt) {             /* the machine-level build: the original's shop */
        if (!fit_from_settings) fprintf(stderr, "--cast-fit: only the decompiled build (build/mtv-dec) has it\n");
        cast_fit_opt = 0;
    }
    if (!fix_init() && mouse_system) {             /* the machine-level build: its own arrow, eased */
        if (!mouse_from_settings) fprintf(stderr, "--mouse system: only the decompiled build (build/mtv-dec) has it; free here\n");
        mouse_system = 0;
    }
    /* --filter hr / hr-pixel: cells only where the decompiled code draws (not under the
     * interpreter or the difftest, which run the original's machine code); else MMPX, or
     * the even pixels of sharp for hr-pixel */
    if (DISPLAY_IS_HR(display_filter) && (!use_native || difftest_on || !hr_init())) {
        fprintf(stderr, "--filter hr-pixel: only the decompiled build (build/mtv-dec) has it; sharp here\n");
        display_filter = DISPLAY_SHARP;
    }
    insns_per_us = mips;
    { extern u64 trace_from, trace_to; const char *a = getenv("TRACE_T");
      if (a) { trace_from = strtoull(a, NULL, 0); const char *b = strchr(a, ':'); trace_to = b ? strtoull(b + 1, NULL, 0) : trace_from + 200; } }
    M = calloc(1, MEM_SIZE);
    bios_init();
    clock_init();
    {   /* argv[0] for the program: basename of --exe without the _U suffix */
        static char prog[64] = "MTV.EXE";
        if (exe) {
            const char *b = strrchr(exe, '/'); b = b ? b + 1 : exe;
#ifdef _WIN32
            if (strrchr(b, '\\')) b = strrchr(b, '\\') + 1;
#endif
            snprintf(prog, sizeof prog, "%s", b);
            char *u = strstr(prog, "_U."); if (u) memmove(u, u + 2, strlen(u + 2) + 1);
        }
        dos_init(gamedir, prog);
    }
    { extern void dos_set_args(const char *); if (prog_args) dos_set_args(prog_args); }
    notice_german = prog_args && (prog_args[0] == 'd' || prog_args[0] == 'D');   /* (the window's notices, host.c) */
    hw_init();
    adlib_init();
    if (wav_path) adlib_wav_open(wav_path);
    host_init(headless, sc);
    if (fast) realtime = 0;
    if (mp_server) {                     /* a network game: every machine the same game (net.h) */
        if (!seat_humans || mp_seat < 0 || mp_seat >= seats_n) { fprintf(stderr, "--mp needs --humans and --seat K (0..%d)\n", seats_n - 1); return 2; }
        char params[2048], hum[16] = "";     /* the fixes list alone: up to 280 characters */
        for (int i = 0; i < seats_n; i++) snprintf(hum + strlen(hum), sizeof hum - strlen(hum), "%s%d", i ? "," : "", seat_station[i]);
        extern u64 dos_game_files_hash(void);
        u64 files = mp_files_hash = dos_game_files_hash();   /* the same game data everywhere (a mod's too), saves aside */
        if (!files) { fprintf(stderr, "--mp: SOURCE1.RSC and SOURCE2.RSC not found in the game directory\n"); return 2; }
        char names[128], start[40];
        names_param(names, sizeof names);
        if (mp_load_opt) snprintf(start, sizeof start, "load:%016llx", (unsigned long long)mp_load_hash);
        else snprintf(start, sizeof start, "%s", new_game_opt ? "new" : "setup");
        int plen = snprintf(params, sizeof params, "humans=%s args=%s fixes=%s time=%s speed4=%d aiimp=%d money=%ld credit=%ld terror=%ld sammy=%ld,%ld,%ld,%ld offers=%d filmoffers=%d sertop=%d knock=%d start=%s difficulty=%d names=%s seed=%ld files=%016llx", hum,
                 prog_args ? prog_args : "-", fix_spec ? fix_spec : "classic", logical_time ? "logical" : "classic",
                 fourth_speed_opt, ai_improve_opt, start_money_opt, credit_line_opt, terror_damage_opt,
                 SAMMY_PRIZE_OPT(0), SAMMY_PRIZE_OPT(1), SAMMY_PRIZE_OPT(2), SAMMY_PRIZE_OPT(3), AD_OFFERS_OPT(), FILM_OFFERS_OPT(), series_top_opt, knock_opt, start, difficulty_opt, names, seed_opt, (unsigned long long)files);
        if (plen < 0 || plen >= (int)sizeof params) { fprintf(stderr, "--mp: the game's parameters are too long\n"); return 2; }   /* cut, the server would compare a part */
        for (char *p = params; *p; p++) if (*p == '\n') *p = ' ';
        host_net_start(mp_server, mp_seat, params);
    }
    if (record_path) { extern void host_record_open(const char *, const char *, int, char **); host_record_open(record_path, prog_args, argc, argv); }
    if (dump_on) {   /* SEG:OFF:DIR, SEG relative to the load segment (e.g. 39D4 = DGROUP) */
        extern void host_dump_on(u32, const char *);
        unsigned s_, o_; int n = 0;
        if (sscanf(dump_on, "%x:%x:%n", &s_, &o_, &n) != 2 || !n) { fprintf(stderr, "--dump-on SEG:OFF:DIR\n"); return 2; }
        host_dump_on(((u32)(LOADSEG + s_) << 4) + o_, dump_on + n);
    }
    /* the decompiled build runs without the EXE; the original's machine code
     * (interpreter, difftest) and --exe still load it */
    if (!exe && builtin_image && use_native) load_builtin(builtin_image);
    else {
        if (!exe) { static char p[1100]; snprintf(p, sizeof p, "%s/MTV_U.EXE", gamedir); exe = p; }
        load_exe(exe);
    }
    { extern void engine_stats_init(void); engine_stats_init(); }
    { extern u32 watch_entry_lin; const char *w = getenv("WATCH_ENTRY");
      if (w) { unsigned s_, o_; sscanf(w, "%x:%x", &s_, &o_); watch_entry_lin = ((u32)(LOADSEG + s_) << 4) + o_; } }
    { extern void interp_init_polls(u32); interp_init_polls(image_bytes); }
    if (use_native && native_count > 0 && !difftest_on) {
        if (seats_n) seats_run(native_entry_point);   /* the program as seat 0's coroutine */
        else native_entry_point();
    }
    else interp_run_forever();
    return 0;
}
