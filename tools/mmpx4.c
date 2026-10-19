/* MMPX 4x of a picture for the tools (tools/hrfont.py): stdin "W H WRAP\n"
 * and W*H 32-bit pixels (little-endian), stdout 16*W*H pixels.  The same
 * code as the game's (runtime/mmpx.c): make build/mmpx4 */
#include <stdio.h>
#include <stdlib.h>
#include "runtime.h"

int main(void) {
    int w, h, wrap;
    if (scanf("%d %d %d", &w, &h, &wrap) != 3 || w <= 0 || h <= 0 || getchar() != '\n') return 2;
    u32 *in = malloc(sizeof *in * (size_t)w * h), *out = malloc(sizeof *out * (size_t)16 * w * h);
    if (!in || !out || fread(in, 4, (size_t)w * h, stdin) != (size_t)w * h) return 2;
    mmpx_scale4(in, w, h, wrap, out);
    fwrite(out, 4, (size_t)16 * w * h, stdout);
    return 0;
}
