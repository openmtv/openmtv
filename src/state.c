/* Game state in host memory (ROADMAP Phase 3, stage 5).
 *
 * The game's state -- everything a savegame holds (names/FINDINGS.md
 * "Savegames") and all the program's variables -- lives here, not in the
 * emulated memory: DGROUP 0000..A72F in g_dgroup (byte layout of the
 * original data segment, accessed through the named globals and struct
 * accessors of game_globals.h / game_types.h), and the heap blocks a
 * savegame holds in host arrays: the film, series and screenplay tables and
 * the three managers' names; with them the coverage map, which the game
 * computes from the transmitters and keeps between frames.  Their original places in the
 * emulated memory stay allocated (same heap layout) and are where the state
 * is written back for readers of the emulated memory: the test harness's
 * hashes, peeks, dumps, calls and pokes (runtime/host.c), the difftest
 * around each native run (runtime/difftest.c), machine-level code and the
 * interpreted sound driver (runtime/dispatch.c, interp.c).  Far pointers
 * remain valid handles: host_addr() (dec.h) resolves one into a moved area
 * to the host copy.  Not moved: the tick block DS:902C..904B, which the
 * sound driver's timer interrupt counts in the emulated memory, and what is
 * not game state: the emulated stack, the resources and the text caches
 * built from them, bitmaps, VRAM.
 *
 * state_activate() copies in what is allocated and not yet moved: DGROUP at
 * the start of game_main, each heap block once it is allocated.  A block
 * goes back to the emulated memory when the game frees it (rtl_farfree). */
#include <string.h>
#include "game.h"
#include "fixes.h"

u8 g_dgroup[DG_END];
int dg_active;
HostRegion host_regions[HOST_REGIONS];

struct film g_film_table[FILMS_MAX + 1];
struct series g_series_table[SERIES_MAX + 1];
struct screenplay g_script_table[51];
u8 g_manager_names[3][0x29];
u8 g_ads_extra[(ADS_MAX - 50) * 0x18];   /* a mod's ads 51.. (ads 1..50: DGROUP, DS:3F4F) */
u8 g_ad_offers_extra[(AD_OFFERS_MAX - 10) * 0x7];   /* the ad agency's offers 11.. (--ad-offers; 1..10: DGROUP, DS:19A0) */
u8 g_dealer_extra[(FILM_OFFERS_MAX - 10) * 0x8];   /* the film dealer's slots 11.. (--film-offers; 1..10: DGROUP, DS:19ED) */
u16 g_filmshelf_extra[(FILM_OFFERS_MAX - 10) * 4];  /* their shelf rectangles x1, y1, x2, y2 (src/film_offers.c) */
u8 g_coverage[0x11E2];

_Static_assert(sizeof(struct film) == 0x17, "struct film has the original record layout");
_Static_assert(sizeof(struct series) == 0x39, "struct series has the original record layout");
_Static_assert(sizeof(struct screenplay) == 0x1B3, "struct screenplay has the original record layout");

/* The number of films, series and ads (an extension, docs/CHANGES.md): the
 * original has 200 films, 50 series and 50 ads, a mod can have up to
 * FILMS_MAX films (ids below 1000, where the series begin), SERIES_MAX series
 * (programme ids 1000 + n below 2000, where other kinds begin) and ADS_MAX
 * ads.  Each is the size of its table resource (0xC9: records 0..n of 0x17
 * bytes, 0xCA: of 0x39, 0xCC: of 0x18), read from the resource index the
 * game loaded at its start -- no file access, so the original data plays
 * with the original timing.  Ads 51.. live in g_ads_extra, addressed through
 * the far-pointer segment ADS_EXTRA_SEG (host region ADS_REGION). */
u16 g_nfilms = 200, g_nseries = 50, g_nads = 50;
u16 g_nadoffers = 10;                    /* the ad agency's offers: the original's 10, or --ad-offers (src/fixes.c) */
u16 g_nfilmoffers = 10;                  /* the film dealer's offers: the original's 10, or --film-offers (src/film_offers.c) */

/* the moved heap blocks: the DGROUP far pointer the game keeps to each, its
 * host copy and size (0: the film or series table, sized by g_nfilms /
 * g_nseries); host_regions[i] is block i while it is moved */
static const struct { u16 ptr; void *host; u16 len; } blocks[] = {
    { 0x24A9, g_film_table, 0 },                            /* g_movies */
    { 0x2646, g_series_table, 0 },                          /* g_series */
    { 0x715C, g_script_table, sizeof g_script_table },      /* g_scripts */
    { 0x8BF3, g_manager_names[0], 0x29 },                   /* pl_name(1) */
    { 0x8C0A, g_manager_names[1], 0x29 },                   /* pl_name(2) */
    { 0x8C21, g_manager_names[2], 0x29 },                   /* pl_name(3) */
    { 0x8BAA, g_coverage, sizeof g_coverage },              /* g_coverage_map */
};
#define NBLOCKS ((int)(sizeof blocks / sizeof blocks[0]))
#define ADS_REGION NBLOCKS               /* then a mod's ads 51.. (tables_count_init) */
#define OFFERS_REGION (NBLOCKS + 1)      /* then the ad agency's offers 11.. (tables_count_init) */
#define DEALER_REGION (NBLOCKS + 2)      /* then the film dealer's slots 11.. */
#define RECTS_REGION (NBLOCKS + 3)       /* and their shelf rectangles */
#define R_HANDLES (NBLOCKS + 4)          /* then handles for C buffers */
_Static_assert(R_HANDLES < HOST_REGIONS, "room for handles");

static u16 table_count(u16 res, u16 size, u16 orig, u16 max) {
    u16 n = (u16)(FP16(g_res_index, (res << 3) + 2) / size);
    return n > orig + 1 && n <= max + 1 ? (u16)(n - 1) : orig;
}
void tables_count_init(void) {
    g_nfilms = table_count(0xC9, 0x17, 200, FILMS_MAX);
    g_nseries = table_count(0xCA, 0x39, 50, SERIES_MAX);
    g_nads = table_count(0xCC, 0x18, 50, ADS_MAX);
    if (g_nads > 50) {
        host_regions[ADS_REGION].lin = (u32)ADS_EXTRA_SEG << 4;
        host_regions[ADS_REGION].host = g_ads_extra;
        host_regions[ADS_REGION].len = (u32)(g_nads - 50) * 0x18;
    }
    g_nadoffers = ad_offers_count();
    if (g_nadoffers > 10) {
        host_regions[OFFERS_REGION].lin = (u32)AD_OFFERS_SEG << 4;
        host_regions[OFFERS_REGION].host = g_ad_offers_extra;
        host_regions[OFFERS_REGION].len = (u32)(g_nadoffers - 10) * 0x7;
    }
    g_nfilmoffers = film_offers_count();
    if (g_nfilmoffers > 10) {
        host_regions[DEALER_REGION].lin = (u32)FILM_OFFERS_SEG << 4;
        host_regions[DEALER_REGION].host = g_dealer_extra;
        host_regions[DEALER_REGION].len = (u32)(g_nfilmoffers - 10) * 0x8;
        host_regions[RECTS_REGION].lin = (u32)FILM_RECTS_SEG << 4;
        host_regions[RECTS_REGION].host = (u8 *)g_filmshelf_extra;
        host_regions[RECTS_REGION].len = (u32)(g_nfilmoffers - 10) * 0x8;
    }
}

/* fp is the data of a live block of the far heap holding len bytes (Borland:
 * an 8-byte header before the data, its u32 size with bit 0 set while in
 * use; the blocks follow each other from g_heap_first to g_heap_last).  A
 * pointer the game keeps after freeing its block is not one. */
static int heap_live(u32 fp, u32 len) {
    if (!fp || !g_heap_first) return 0;
    u32 want = lin(FPSEG(fp), FPOFF(fp)) - 8;
    u32 b = lin(FPSEG(g_heap_first), FPOFF(g_heap_first));
    u32 end = lin(FPSEG(g_heap_last), FPOFF(g_heap_last));
    while (b <= end && b + 8 <= MEM_SIZE) {
        u32 size = *(u32 *)(M + b);
        if (b == want) return (size & 1) && (size & ~1u) >= len + 8;
        if (!(size & ~1u)) return 0;
        b += size & ~1u;
    }
    return 0;
}

/* copy in what is allocated and not moved yet */
void state_activate(void) {
    if (!dg_active) {
        memcpy(g_dgroup, M + DSLIN, DG_END);
        dg_active = 1;
    }
    for (int i = 0; i < NBLOCKS; i++) {
        HostRegion *r = &host_regions[i];
        u32 fp = G32(blocks[i].ptr);
        u16 len = blocks[i].len ? blocks[i].len : i == 0 ? (u16)((g_nfilms + 1) * 0x17) : (u16)((g_nseries + 1) * 0x39);
        if (r->len || !heap_live(fp, len)) continue;
        r->lin = lin(FPSEG(fp), FPOFF(fp));
        r->host = blocks[i].host;
        memcpy(r->host, M + r->lin, len);
        r->len = len;
    }
}

/* host state -> the original layout in the emulated memory */
void state_export(void) {
    if (dg_active) {
        memcpy(M + DSLIN, g_dgroup, DG_SHARED_LO);
        memcpy(M + DSLIN + DG_SHARED_HI, g_dgroup + DG_SHARED_HI, DG_END - DG_SHARED_HI);
    }
    for (int i = 0; i < R_HANDLES; i++)
        if (host_regions[i].len) memcpy(M + host_regions[i].lin, host_regions[i].host, host_regions[i].len);
}

/* the original layout changed under us (a test harness poke, machine-level
 * code): read it in again */
void state_reload(void) {
    if (dg_active) {
        memcpy(g_dgroup, M + DSLIN, DG_SHARED_LO);
        memcpy(g_dgroup + DG_SHARED_HI, M + DSLIN + DG_SHARED_HI, DG_END - DG_SHARED_HI);
    }
    for (int i = 0; i < R_HANDLES; i++)
        if (host_regions[i].len) memcpy(host_regions[i].host, M + host_regions[i].lin, host_regions[i].len);
}

/* stop resolving to host memory (the end of a difftest run) */
void state_drop(void) {
    dg_active = 0;
    for (int i = 0; i < HOST_REGIONS; i++) host_regions[i].len = 0;
}

/* where the game's cursor is heading (runtime/host.c, the free mouse) */
void game_cursor(int *x, int *y) {
    *x = (i16)g_mouse_x + (i16)g_mouse_pending_dx;
    *y = (i16)g_mouse_y + (i16)g_mouse_pending_dy;
}

/* the game frees the heap block at fp: a moved one goes back first (the
 * original's freed memory keeps its last contents) */
void state_block_freed(u32 fp) {
    u32 a = lin(FPSEG(fp), FPOFF(fp));
    for (int i = 0; i < R_HANDLES; i++)
        if (host_regions[i].len && host_regions[i].lin == a) {
            memcpy(M + a, host_regions[i].host, host_regions[i].len);
            host_regions[i].len = 0;
        }
}

/* A far pointer for a C buffer, for code that takes far pointers (the text
 * routines): a handle at a segment the game never addresses (F800 + 100h
 * per slot), resolved by host_addr() while it is held. */
u32 host_handle(void *p, u16 n) {
    for (int i = R_HANDLES; i < HOST_REGIONS; i++)
        if (!host_regions[i].len) {
            u16 seg = (u16)(0xF800 + (i - R_HANDLES) * 0x100);
            host_regions[i].lin = (u32)seg << 4;
            host_regions[i].host = p;
            host_regions[i].len = n;
            return (u32)seg << 16;
        }
    cpu_halt("host_handle: no free slot");
    return 0;
}
void host_handle_release(u32 fp) {
    u32 a = lin(FPSEG(fp), FPOFF(fp));
    for (int i = R_HANDLES; i < HOST_REGIONS; i++)
        if (host_regions[i].len && host_regions[i].lin == a) host_regions[i].len = 0;
}
