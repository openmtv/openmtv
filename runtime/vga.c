/* Planar VGA (EGA-compatible 16-colour modes, used by INTRO.EXE in mode 0Dh).
 *
 * Mode 13h stays a plain linear framebuffer in M[A0000..].  In planar modes
 * CPU accesses to A000:0000-FFFF are routed here (see rb/rw/wb/ww in cpu.h)
 * and go through the graphics controller: latches, write modes 0-3, map
 * mask, bit mask, set/reset, data rotate/function, read modes 0/1. */
#include <string.h>
#include "runtime.h"

int vga_planar;
u8 vga_planes[4][0x10000];
static u8 latch[4];
u8 vga_seq[8], vga_gc[16], vga_crtc[32], vga_attr[32];
u8 vga_attr_idx, vga_attr_flip;

int vga_state_blocks(StateBlock *b) {
    int n = 0;
    STATE_BLOCK(b, n, vga_planar); STATE_BLOCK(b, n, vga_planes); STATE_BLOCK(b, n, latch);
    STATE_BLOCK(b, n, vga_seq); STATE_BLOCK(b, n, vga_gc); STATE_BLOCK(b, n, vga_crtc);
    STATE_BLOCK(b, n, vga_attr); STATE_BLOCK(b, n, vga_attr_idx); STATE_BLOCK(b, n, vga_attr_flip);
    return n;
}

/* the same without the planar memory, which the game never uses (only the
 * intro): what a seat owns of the VGA (runtime/seat.c) */
int vga_seat_blocks(StateBlock *b) {
    int n = 0;
    STATE_BLOCK(b, n, vga_planar); STATE_BLOCK(b, n, latch);
    STATE_BLOCK(b, n, vga_seq); STATE_BLOCK(b, n, vga_gc); STATE_BLOCK(b, n, vga_crtc);
    STATE_BLOCK(b, n, vga_attr); STATE_BLOCK(b, n, vga_attr_idx); STATE_BLOCK(b, n, vga_attr_flip);
    return n;
}

void vga_set_planar(int on) {
    vga_planar = on;
    if (on) {
        memset(vga_planes, 0, sizeof vga_planes);
        memset(vga_gc, 0, sizeof vga_gc);
        vga_gc[8] = 0xFF;            /* bit mask */
        vga_seq[2] = 0x0F;           /* map mask */
        vga_crtc[0x13] = 20;         /* 40 bytes per line */
        vga_crtc[0x0C] = vga_crtc[0x0D] = 0;
        static const u8 ega_default[16] = {0, 1, 2, 3, 4, 5, 20, 7, 56, 57, 58, 59, 60, 61, 62, 63};
        memcpy(vga_attr, ega_default, 16);
        vga_attr[0x10] = 0x01;       /* mode control: graphics */
        vga_attr[0x12] = 0x0F;       /* colour plane enable */
        vga_attr[0x14] = 0;          /* colour select */
    }
}

u8 vga_read8(u32 a) {
    u16 o = (u16)(a - 0xA0000);
    for (int p = 0; p < 4; p++) latch[p] = vga_planes[p][o];
    if (vga_gc[5] & 0x08) {          /* read mode 1: colour compare */
        u8 res = 0xFF;
        for (int p = 0; p < 4; p++) {
            if (!(vga_gc[7] & (1 << p))) continue;
            u8 want = (vga_gc[2] >> p) & 1 ? 0xFF : 0x00;
            res &= ~(latch[p] ^ want);
        }
        return res;
    }
    return latch[vga_gc[4] & 3];
}

void vga_write8(u32 a, u8 v) {
    u16 o = (u16)(a - 0xA0000);
    int mode = vga_gc[5] & 3;
    u8 rot = vga_gc[3] & 7, fn = (vga_gc[3] >> 3) & 3;
    u8 mask = vga_gc[8];
    if (rot) v = (u8)((v >> rot) | (v << (8 - rot)));
    for (int p = 0; p < 4; p++) {
        if (!(vga_seq[2] & (1 << p))) continue;
        u8 d;
        switch (mode) {
        case 1: vga_planes[p][o] = latch[p]; continue;
        case 2: d = (v >> p) & 1 ? 0xFF : 0x00; break;
        case 3: {
            u8 m = mask & v;
            d = (vga_gc[0] >> p) & 1 ? 0xFF : 0x00;
            vga_planes[p][o] = (d & m) | (latch[p] & ~m);
            continue;
        }
        default:
            d = (vga_gc[1] >> p) & 1 ? ((vga_gc[0] >> p) & 1 ? 0xFF : 0x00) : v;
        }
        switch (fn) {
        case 1: d &= latch[p]; break;
        case 2: d |= latch[p]; break;
        case 3: d ^= latch[p]; break;
        }
        vga_planes[p][o] = (d & mask) | (latch[p] & ~mask);
    }
}

/* 320x200 indexed frame for the host: DAC index per pixel */
void vga_render_planar(u8 *out) {
    u16 start = (u16)((vga_crtc[0x0C] << 8) | vga_crtc[0x0D]);
    u16 pitch = (u16)(vga_crtc[0x13] * 2);
    if (!pitch) pitch = 40;
    for (int y = 0; y < 200; y++) {
        for (int x = 0; x < 320; x++) {
            u16 o = (u16)(start + y * pitch + (x >> 3));
            int bit = 7 - (x & 7), c = 0;
            for (int p = 0; p < 4; p++) c |= ((vga_planes[p][o] >> bit) & 1) << p;
            c &= vga_attr[0x12];
            u8 pal = vga_attr[c & 15] & 0x3F;
            u8 idx;
            if (vga_attr[0x10] & 0x80) idx = (u8)(((vga_attr[0x14] & 0x0F) << 4) | (pal & 0x0F));
            else idx = (u8)(((vga_attr[0x14] & 0x0C) << 4) | pal);
            out[y * 320 + x] = idx;
        }
    }
}
