/* OpenMTV's sign at the foot of the tower: the gold "Mad-TV" the game shows right of
 * the building at the start (sprite 0 of resource 16, 46 x 11, drawn by
 * building.c at 262,92 over the gold bar, sprite 1) becomes "Open" in the game's
 * gold and "MTV" dark in a yellow box -- the name's wordmark of the launcher and
 * the release page.  The mock-up chosen with the user (variant 3):
 * https://claude.ai/artifact/1qTPbtEH6fAb8yqTXjzXB3
 *
 * The same size and the same palette colours as the original (0 clear, 31 its
 * black shadow, 26/20/25/6 dark gold, pale gold, yellow, white): only the
 * sprite's pixels change, once, right after the bank is loaded (main_loop.c);
 * the drawing, its place and its timing stay the game's.  The classic mode keeps
 * the original's "Mad-TV" (no fixes: exactly the 1991 game), and so does the
 * differential tester (it compares against the original program). */
#include "game.h"
#include "hr.h"
#include "fixes.h"

static const char *const sign[11] = {
    ".DCEK...................DBBBBBBBBBBBBBBBBBBBB.",
    "DBKDBK..................DBBBBBBBBBBBBBBBBBBBBK",
    "DBKDBK..................DBKBBBKBKKKKKBKBBBKBBK",
    "DBKDBKDCCEK..DCEK.DCCEK.DBKKBKKBBBKBBBKBBBKBBK",
    "DBKDBKDBKDBKDBKDBKDBKDBKDBKBKBKBBBKBBBKBBBKBBK",
    "DCKDCKDCKDCKDCCCCKDCKDCKDBKBBBKBBBKBBBBKBKBBBK",
    "DCKDCKDCKDCKDCKKK.DCKDCKDBKBBBKBBBKBBBBKBKBBBK",
    "DCKDCKDCKDCKDCK...DCKDCKDBKBBBKBBBKBBBBBKBBBBK",
    "DCKDCKDCCCK.DCK.DKDCKDCKDBBBBBBBBBBBBBBBBBBBBK",
    "KDCCK.DCKK..KDCCCKDCKDCKDDDDDDDDDDDDDDDDDDDDDK",
    ".KKK..DCK....KKKK.KK.KK..KKKKKKKKKKKKKKKKKKKKK",
};

static u8 colour(char c)
{
    switch (c) {
    case 'D': return 26;    /* dark gold: a stroke's left edge */
    case 'C': return 20;    /* pale gold */
    case 'B': return 25;    /* yellow: right edges, the box */
    case 'E': return 6;     /* white glint */
    case 'K': return 31;    /* the black shadow, right of and under the strokes */
    default: return 0;      /* clear */
    }
}

/* BANK: resource 16 as res_load returns it (u16 count, then 8-byte headers: two
 * u16 sizes -- the width in 8-pixel units and the height -- and the data's far
 * pointer; rows padded to a multiple of 8 pixels) */
void openmtv_sign(u32 bank)
{
    u32 hdr, data;
    u16 a, b, stride;
    int x, y;
    if (!fix_spec || difftest_on || !bank || FP16(bank, 0) < 1)
        return;
    hdr = MK32(FPSEG(bank), FPOFF(bank) + 2);
    a = FP16(hdr, 0);
    b = FP16(hdr, 2);
    if (a == 6 && b == 11) stride = 48;
    else if (a == 11 && b == 6) stride = 48;
    else return;                /* not the original's sign: leave it */
    data = FP32(hdr, 4);
    for (y = 0; y < 11; y++)
        for (x = 0; x < 46; x++)
            FP8(data, y * stride + x) = colour(sign[y][x]);
    hr_bank_redo(16, bank);     /* --filter hr: the cells of the new pixels */
}
