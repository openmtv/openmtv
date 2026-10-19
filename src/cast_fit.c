/* Role fit in the shop (--cast-fit, settings [display] cast_fit = yes): in the
 * production shop's actor panel (ui_draw_actor_card 26A7:3B04) the rows Humor /
 * Aussehen / Handwerk with their bars give way to one row "Eignung" ("Fit"):
 * how well each of the five actors shown fits the role picked in the shopping
 * list, in per cent, as the game itself rates it when the production starts
 * (script_role_match_pct 1DA8:2560: the sum of min(requirement, attribute)
 * over humor, appearance and craft, over the sum of the requirements).  The
 * best of the five in green, below 60 in red, the rest black.  A role without
 * requirements (some shows) shows "-": every actor scores 0 there.  The
 * mock-up chosen with the user: https://claude.ai/artifact/Kt9rUAFxEjVKWLMV59HzU5
 * (B; then, as the user asked, without the bars, the panel as short as the
 * row needs: it ends 14 rows higher, the room's picture under it again).
 *
 * Display only: the game draws its panel as always (same calls, same time),
 * then this paints over it, pixel by pixel into the screen buffer, without the
 * game's drawing calls -- no time taken, no game state touched (a recording
 * replays the same with or without it; a network game's machines may differ
 * in it).
 *
 * The original knows only the category of the slot picked (g_shop_category:
 * Schauspieler, ...), not the slot: the shop's slot clicks tell
 * castfit_pick which one (per seat).  When the panel shows a category the
 * picked slot does not have (the list changed), the first slot of that
 * category counts. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "hr.h"
#include "fixes.h"
#include "seat.h"

static struct { u16 script, episode, slot; } picked[SEATS_MAX];

static int seat_ix(void) { return seat_cur > 0 && seat_cur < SEATS_MAX ? seat_cur : 0; }
static int castfit_on(void) { return cast_fit_opt && !difftest_on; }

/* the shop's slot click: slot (1..10) of the current shopping list was picked */
void castfit_pick(u16 slot) {
    int s = seat_ix();
    picked[s].script = shoplists(g_player, g_shop_list).script;
    picked[s].episode = shoplists(g_player, g_shop_list).episode;
    picked[s].slot = slot;
}

/* the role type (1..15) of slot k of the list (script, episode); 0 none */
static int slot_role(u16 script, u16 episode, int k) {
    i8 t = (i8)FP8(g_scripts, (u16)(script * 0x1B3 + (episode - 1) * 0x23 + k * 3 + 9));
    return t >= 1 && t <= 15 ? t : 0;
}
static int role_category(u16 script, int role) { return (i8)FP8(g_scripts, (u16)(script * 0x1B3 + role + 0x1A3)); }

/* the role the panel is about: the picked slot's, else the first of this category */
static int current_role(u16 script, u16 episode) {
    int s = seat_ix(), cat = (i16)g_shop_category;
    if (picked[s].script == script && picked[s].episode == episode && picked[s].slot >= 1 && picked[s].slot <= 10) {
        int r = slot_role(script, episode, picked[s].slot);
        if (r && role_category(script, r) == cat) return r;
    }
    for (int k = 1; k <= 10; k++) {
        int r = slot_role(script, episode, k);
        if (r && role_category(script, r) == cat) return r;
    }
    return 0;
}

/* script_role_match_pct's sum for role r and actor (category cat, index a); -1: no requirements */
static int fit_pct(u16 script, int r, int cat, int a) {
    int need = 0, got = 0;
    for (int i = 0; i < 4; i++) {
        if (i == 1) continue;            /* (the fourth attribute counts nowhere, and shows nowhere) */
        int q = (i8)FP8(g_scripts, (u16)(script * 0x1B3 + (r - 1) * 4 + i + 0x168));
        int v = (i8)G8((u16)((cat - 1) * 0xC0 + 0x5013 + a * 0xC + i + 6));
        got += q < v ? q : v;
        need += q;
    }
    return need ? (i16)(got * 100) / (i16)need : -1;
}

/* ---- pixels ---- */
typedef struct { u16 seg, off; int w, h; } Bmp;
static Bmp bmp_of(u32 b) {
    Bmp m;
    u32 pix = FP32(b, 4);
    m.w = FP16(b, 0) << 3; m.h = FP16(b, 2); m.seg = FPSEG(pix); m.off = FPOFF(pix);
    return m;
}
static u8 px_get(const Bmp *m, int x, int y) { return src_rb(m->seg, (u16)(m->off + y * m->w + x)); }
static void px_set(const Bmp *m, int x, int y, u8 v) {
    if (x >= 0 && x < m->w && y >= 0 && y < m->h) {
        hr_flat_px(m->seg, (u16)(m->off + y * m->w + x), v);
        src_wb(m->seg, (u16)(m->off + y * m->w + x), v);
    }
}

/* the palette entry nearest to a 6-bit colour */
static u8 nearest(int r, int g, int b) {
    int best = 0, bd = 1 << 30;
    for (int i = 0; i < 256; i++) {
        int dr = vga_dac[i][0] - r, dg = vga_dac[i][1] - g, db = vga_dac[i][2] - b, d = dr * dr + dg * dg + db * db;
        if (d < bd) { bd = d; best = i; }
    }
    return (u8)best;
}

/* the game's font (g_font_2, resource 3): glyph k's bitmap header at bank + k * 8 - 6 */
static const char pct_glyph[7][6] = { "##...", "##..#", "...#.", "..#..", ".#...", "#..##", "...##" };
static int glyph_top = -1;                 /* the first ink row of the digits */
static int char_w(u8 c) { return c == ' ' ? 4 : c == '%' ? 6 : g_font_map(c) ? g_glyph_width(c) + 1 : 0; }
static int text_w(const char *s) { int w = 0; for (; *s; s++) w += char_w((u8)*s); return w ? w - 1 : 0; }
/* s with its digits' first ink row at y; ink in colour col (0: the font's own) */
static void text_put(const Bmp *m, int x, int y, const char *s, u8 col) {
    u32 bank = g_font_2;
    if (glyph_top < 0) {
        glyph_top = 0;
        u8 k = g_font_map('0');
        if (k) {
            u32 hd = MK32(FPSEG(bank), (u16)(FPOFF(bank) + k * 8 - 6)), p = FP32(hd, 4);
            int gw = FP16(hd, 0) << 3, gh = FP16(hd, 2);
            for (int r = 0, found = 0; r < gh && !found; r++)
                for (int c = 0; c < 8; c++)
                    if (FP8(p, (u16)(r * gw + c))) { glyph_top = r; found = 1; break; }
        }
    }
    for (; *s; s++) {
        u8 c = (u8)*s;
        if (c == '%') {
            for (int r = 0; r < 7; r++)
                for (int k = 0; k < 5; k++)
                    if (pct_glyph[r][k] == '#') px_set(m, x + k, y + r, col ? col : nearest(0, 0, 0));
        } else if (c != ' ' && g_font_map(c)) {
            u8 k = g_font_map(c);
            u32 hd = MK32(FPSEG(bank), (u16)(FPOFF(bank) + k * 8 - 6)), p = FP32(hd, 4);
            int gw = FP16(hd, 0) << 3, gh = FP16(hd, 2);
            for (int r = 0; r < gh; r++)
                for (int q = 0; q < 8; q++) {
                    u8 v = FP8(p, (u16)(r * gw + q));
                    int dx = x + q, dy = y + r - glyph_top;
                    if (dx < 0 || dx >= m->w || dy < 0 || dy >= m->h) continue;
                    hr_glyph_px(FPSEG(p), (u16)(FPOFF(p) + r * gw + q), m->seg, (u16)(m->off + dy * m->w + dx), v, col);
                    if (v) src_wb(m->seg, (u16)(m->off + dy * m->w + dx), col ? col : v);
                }
        }
        x += char_w(c);
    }
}

/* the panel (ui_frame_px 0x34,3 - 0x132,0x3E): interior to row 66, its grey edge 67, the
 * shadow 68..69 (x 56..313); its edges: columns 53 and 311, the right shadow 312..313 */
#define PANEL_X0 53                        /* its left edge (52 and left: the room) */
#define PANEL_X1 313
#define LABEL_X 0x38
#define BARS_Y0 41                         /* Humor / Aussehen / Handwerk and their bars: rows 41..64 */
#define ROW_Y 44                           /* the row's digits: 2 rows below Humor's (some room under the prices) */
#define SHRINK 14                          /* the panel ends 14 rows higher: interior to 52, edge 53, shadow 54..55 */
#define BG_Y0 (67 - SHRINK)                /* the room's picture kept from here to the panel's old bottom */
#define BG_Y1 69
#define BG_X1 (PANEL_X1 + 2)

/* the room's picture under the panel's lower part, as the room was entered (per seat): the
 * game draws its picture once, then the panel over it; a shorter panel shows it again */
static u8 room_bg[SEATS_MAX][BG_Y1 - BG_Y0 + 1][BG_X1 - PANEL_X0 + 1];
static int room_bg_ok[SEATS_MAX];
/* --filter hr: the cells of those rows (src/hr.h), and of the panel's edge rows */
#define BG_W (BG_X1 - PANEL_X0 + 1)
static HrCell *room_bg_cells[SEATS_MAX], *edge_cells;
static u16 px_off(const Bmp *m, int x, int y) { return (u16)(m->off + y * m->w + x); }

/* the shop entered (main_loop: room 63, before its first panel): keep the picture */
void castfit_room_enter(void) {
    int s = seat_ix();
    room_bg_ok[s] = 0;
    if (!castfit_on() || !g_screen_buf) return;
    Bmp m = bmp_of(g_screen_buf);
    if (m.w < 320 || m.h <= BG_Y1) return;
    for (int y = BG_Y0; y <= BG_Y1; y++)
        for (int x = PANEL_X0; x <= BG_X1; x++) room_bg[s][y - BG_Y0][x - PANEL_X0] = px_get(&m, x, y);
    if (hr_on && (room_bg_cells[s] || (room_bg_cells[s] = malloc(sizeof (HrCell) * BG_W * (BG_Y1 - BG_Y0 + 1)))))
        for (int y = BG_Y0; y <= BG_Y1; y++) hr_get(room_bg_cells[s] + (y - BG_Y0) * BG_W, m.seg, px_off(&m, PANEL_X0, y), BG_W);
    room_bg_ok[s] = 1;
}

void castfit_draw(void) {
    if (!castfit_on()) return;
    int cat = (i16)g_shop_category;
    if (cat < 1 || cat > 7 || !g_screen_buf || !g_font_2) return;
    u16 script = shoplists(g_player, g_shop_list).script, episode = shoplists(g_player, g_shop_list).episode;
    int role = current_role(script, episode);
    if (!role) return;
    Bmp m = bmp_of(g_screen_buf);
    if (m.w < 320 || m.h < 80) return;
    /* the three rows of bars give way: the panel's empty row 65 (its edges included) over
     * them; the panel ends after the new row, its edge and shadow moved up, the room's picture
     * below (without the picture: the panel as long as the original's) */
    int s = seat_ix(), short_ = room_bg_ok[s];
    static u8 edge[3][BG_X1 + 1];
    for (int r = 0; r < 3; r++)
        for (int x = PANEL_X0; x <= BG_X1; x++) edge[r][x] = px_get(&m, x, 67 + r);
    if (hr_on && (edge_cells || (edge_cells = malloc(sizeof (HrCell) * 3 * BG_W))))
        for (int r = 0; r < 3; r++) hr_get(edge_cells + r * BG_W, m.seg, px_off(&m, PANEL_X0, 67 + r), BG_W);
    for (int y = BARS_Y0; y <= 66; y++)
        for (int x = PANEL_X0; x <= PANEL_X1; x++) {
            px_set(&m, x, y, px_get(&m, x, 65));
            hr_copy1(m.seg, px_off(&m, x, 65), m.seg, px_off(&m, x, y));
        }
    if (short_) {
        for (int y = BG_Y0; y <= BG_Y1; y++) {
            for (int x = PANEL_X0; x <= BG_X1; x++) px_set(&m, x, y, room_bg[s][y - BG_Y0][x - PANEL_X0]);
            if (hr_on && room_bg_cells[s]) hr_restore(room_bg_cells[s] + (y - BG_Y0) * BG_W, m.seg, px_off(&m, PANEL_X0, y), BG_W);
        }
        for (int x = PANEL_X0; x <= PANEL_X1; x++) px_set(&m, x, 67 - SHRINK, edge[0][x]);
        if (hr_on && edge_cells) hr_restore(edge_cells, m.seg, px_off(&m, PANEL_X0, 67 - SHRINK), PANEL_X1 - PANEL_X0 + 1);
        for (int r = 1; r < 3; r++) {
            for (int x = 56; x <= PANEL_X1; x++) px_set(&m, x, 67 - SHRINK + r, edge[r][x]);
            if (hr_on && edge_cells) hr_restore(edge_cells + r * BG_W + (56 - PANEL_X0), m.seg, px_off(&m, 56, 67 - SHRINK + r), PANEL_X1 - 56 + 1);
        }
    }
    text_put(&m, LABEL_X, ROW_Y, g_language == 0 ? "Eignung" : "Fit", 0);
    /* the five actors' fit */
    int pct[6], best = -1;
    for (int si = 1; si <= 5; si++) {
        int a = si + (i16)g_shop_actor_scroll;
        u16 at = (u16)((cat - 1) * 0xC0 + 0x5013 + a * 0xC);
        pct[si] = (a >= 1 && a <= 15 && (G16(at) || G32((u16)(at + 2)))) ? fit_pct(script, role, cat, a) : -2;
        if (pct[si] > best) best = pct[si];
    }
    u8 green = nearest(8, 32, 8), red = nearest(55, 9, 9);
    for (int si = 1; si <= 5; si++) {
        if (pct[si] == -2) continue;
        char txt[12];
        if (pct[si] < 0) strcpy(txt, "-");
        else snprintf(txt, sizeof txt, "%d%%", pct[si]);
        u8 col = pct[si] < 0 ? 0 : pct[si] == best ? green : pct[si] < 60 ? red : 0;
        int x = si * 0x24 + 0x54;
        text_put(&m, x + (33 - text_w(txt)) / 2, ROW_Y, txt, col);
    }
}
