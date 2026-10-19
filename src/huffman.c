/* Sprite decompressor: order-1 context Huffman decoder of the sprite banks.
 *
 * Original module 37B4 of MTV.EXE (Rainbow Arts, 1991).  Decompiled by
 * tools/decomp.py and hand-maintained since the freeze (git tag
 * generated-final).  Every function keeps its original address in the
 * comment above it; that address ties it to the machine-level twin in gen/
 * and to the difftest tables -- keep it when editing.
 */
#include "game.h"
#include "hrs.h"
#include "hr.h"

/* 37B4:0002  next byte of the compressed stream */
u16 huff_read_byte(void) {
    u16 t1;
    FUNC_TICK_FAST_ROOMS(10);
    t1 = g_huff_src_pos;
    g_huff_src_pos = g_huff_src_pos + 1;
    return FP8(g_huff_src, t1);
}

/* 37B4:0019  store one decoded byte into the sprite bank: on a new sprite write its header {wcols=ceil(w/8), h, far ptr data} from the (w,h) table, then fill rows of stride wcols*8 left to right */
void huff_put_pixel(u16 a0) {
    FUNC_TICK_FAST_ROOMS(100);
    if (g_huff_new_sprite != 0) {
        g_huff_w = FP8(g_huff_hdr, (g_huff_sprite << 1) + 1);
        g_huff_wcols = ((i16)(((i16)(((i16)g_huff_w) >> 1)) >> 1)) >> 1;
        if ((g_huff_w & 7) != 0) {
            g_huff_wcols = g_huff_wcols + 1;
        }
        FP16(g_huff_out, (g_huff_sprite << 3) + 2) = g_huff_wcols;
        g_huff_h = FP8(g_huff_hdr, (g_huff_sprite << 1) + 2);
        FP16(g_huff_out, (g_huff_sprite << 3) + 4) = g_huff_h;
        MEM32((u16)(FPSEG(g_huff_out)), (u16)(FPOFF(g_huff_out) + (g_huff_sprite << 3)) + 6) = MK32(g_huff_out_hi, g_huff_out_lo + g_huff_out_ofs);
        g_huff_sprite = g_huff_sprite + 1;
        g_huff_new_sprite = 0;
        g_huff_y = 0;
        g_huff_x = 0;
    }
    g_huff_dst = MK32(FPSEG(g_huff_out), ((FPOFF(g_huff_out) + g_huff_out_ofs) + ((g_huff_y * g_huff_wcols) << 3)) + g_huff_x);
    FP8(g_huff_dst, 0) = (u8)a0;
    HRS(hrs_bank_pixel(g_huff_dst, (u8)a0, g_huff_sprite - 1));
    g_huff_x = g_huff_x + 1;
    if (g_huff_x == g_huff_w) {
        g_huff_x = 0;
        g_huff_y = g_huff_y + 1;
        if (g_huff_y == g_huff_h) {
            g_huff_new_sprite = 1;
            g_huff_out_ofs = g_huff_out_ofs + ((g_huff_wcols * g_huff_h) << 3);
            g_huff_pixels = g_huff_pixels + (g_huff_w * g_huff_h);
        }
    }
    return;
}

/* 37B4:013D  next bit, LSB first */
u16 huff_read_bit(void) {
    u16 t1;
    FUNC_TICK_FAST_ROOMS(15);
    if (g_huff_bitcnt != 0) {
        g_huff_bitbuf = ((i16)g_huff_bitbuf) >> 1;
        g_huff_bitcnt = g_huff_bitcnt - 1;
    } else {
        t1 = huff_read_byte();
        g_huff_bitbuf = t1;
        g_huff_bitcnt = 7;
    }
    return g_huff_bitbuf & 1;
}

/* 37B4:0166  read a prefix tree recursively: bit 1 = leaf (first leaf of each tree = escape symbol 0x100, else 8-bit literal), bit 0 = new internal node (ids from 0x101) with child-0 and child-1 subtrees */
void huff_read_tree(u32 a0) {
    u16 si;
    u16 t4;
    FUNC_TICK_FAST_ROOMS(48);
    if ((huff_read_bit()) != 0) {
        if (g_huff_escape_done == 0) {
            g_huff_escape_done = 1;
            FP16(a0, 0) = 0x100;
        } else {
            FP16(a0, 0) = 0;
            si = 0;
            while (((i16)si) < 8) {
                LOOP_TICK(14);
                t4 = huff_read_bit();
                FP16(a0, 0) = (t4 << 7) | (FP16(a0, 0) >> 1);
                si = si + 1;
            }
        }
    } else {
        FP16(a0, 0) = g_huff_next_node;
        g_huff_next_node = g_huff_next_node + 1;
        huff_read_tree(MK32(g_huff_child0_hi, g_huff_child0_lo + (FP16(a0, 0) << 1)));
        huff_read_tree(MK32(g_huff_child1_hi, g_huff_child1_lo + (FP16(a0, 0) << 1)));
    }
    return;
}

/* 37B4:01FE  read order-0 tree + 256 context trees (one per previous byte), then decode g_huff_remaining bytes: symbol from tree of previous byte; escape -> order-0 tree; escape there -> raw 8-bit literal */
void huff_decode(void) {
    u16 di;
    u16 si;
    u16 l_2;
    u32 t2;
    u32 t3;
    u32 t13;
    u32 t21;
    u32 t26;
    u32 t31;
    u32 t39;
    FUNC_TICK(112);
    g_huff_bitbuf = 0;
    g_huff_bitcnt = 0;
    g_huff_next_node = 0x101;
    g_huff_root = 0;
    di = 0;
    while (di != 0x100) {
        LOOP_TICK(6);
        g_huff_ctx_root(di) = 0;
        di = di + 1;
    }
    g_huff_escape_done = 0;
    t2 = rtl_calloc(g_huff_nodes, 2);
    g_huff_child0 = t2;
    g_huff_child0_mem = t2;
    t3 = rtl_calloc(g_huff_nodes, 2);
    g_huff_child1 = t3;
    g_huff_child1_mem = t3;
    if (!((g_huff_child0_lo | g_huff_child0_hi) == 0 || (g_huff_child1_lo | g_huff_child1_hi) == 0)) {
        g_huff_child0_lo = g_huff_child0_lo - 0x202;
        g_huff_child1_lo = g_huff_child1_lo - 0x202;
        g_huff_escape_done = 0;
        huff_read_tree(DSP(0xA682));
        di = 0;
        while (((i16)di) < 256) {
            LOOP_TICK(13);
            g_huff_escape_done = 0;
            huff_read_tree(MK32(DSEG, (di << 1) + 0xA44A));
            di = di + 1;
        }
        si = g_huff_root;
        while (((i16)si) > 256) {
            LOOP_TICK(17);
            if ((huff_read_bit()) != 0) {
                si = FP16(g_huff_child1, si << 1);
                continue;
            } else {
                si = FP16(g_huff_child0, si << 1);
                continue;
            }
        }
        if (si == 0x100) {
            l_2 = 0;
            di = 0;
            while (((i16)di) < 8) {
                LOOP_TICK(11);
                l_2 = (huff_read_bit() << 7) | (l_2 >> 1);
                di = di + 1;
            }
            huff_put_pixel((u16)(i8)((u8)l_2));
            t21 = g_huff_remaining + 0xFFFFFFFF;
            g_huff_remaining = t21;
            if ((HI16(t21) | LO16(t21)) != 0) {
                si = g_huff_ctx_root(l_2);
                for (;;) {
L_0387:
                    LOOP_TICK(109);
                    if (((i16)si) > 256) {
                        if ((huff_read_bit()) != 0) {
                            si = FP16(g_huff_child1, si << 1);
                            continue;
                        } else {
                            si = FP16(g_huff_child0, si << 1);
                            continue;
                        }
                        continue;
                    }
                    if (si == 0x100) {
                        si = g_huff_root;
                        while (((i16)si) > 256) {
                            LOOP_TICK(17);
                            if ((huff_read_bit()) != 0) {
                                si = FP16(g_huff_child1, si << 1);
                                continue;
                            } else {
                                si = FP16(g_huff_child0, si << 1);
                                continue;
                            }
                        }
                        if (si == 0x100) {
                            l_2 = 0;
                            di = 0;
                            while (((i16)di) < 8) {
                                LOOP_TICK(11);
                                l_2 = (huff_read_bit() << 7) | (l_2 >> 1);
                                di = di + 1;
                            }
                            huff_put_pixel((u16)(i8)((u8)l_2));
                            t39 = g_huff_remaining + 0xFFFFFFFF;
                            g_huff_remaining = t39;
                            if ((HI16(t39) | LO16(t39)) == 0) break;
                            si = g_huff_ctx_root(l_2);
                            continue;
                        } else {
                            huff_put_pixel((u16)(i8)((u8)si));
                            t31 = g_huff_remaining + 0xFFFFFFFF;
                            g_huff_remaining = t31;
                            if ((HI16(t31) | LO16(t31)) == 0) break;
                            si = g_huff_ctx_root(si);
                            continue;
                        }
                    } else {
                        huff_put_pixel((u16)(i8)((u8)si));
                        t26 = g_huff_remaining + 0xFFFFFFFF;
                        g_huff_remaining = t26;
                        if ((HI16(t26) | LO16(t26)) == 0) break;
                        si = g_huff_ctx_root(si);
                        continue;
                    }
                }
            }
        } else {
            huff_put_pixel((u16)(i8)((u8)si));
            t13 = g_huff_remaining + 0xFFFFFFFF;
            g_huff_remaining = t13;
            if ((HI16(t13) | LO16(t13)) != 0) {
                si = g_huff_ctx_root(si);
                goto L_0387;
            }
        }
    }
    return;
}

/* 37B4:04AC  decompress a sprite bank (src: u8 count, count x (w,h) bytes, stream: BE u16 node count, BE u32 output size, LSB-first bitstream) into dst (u16 count, 8-byte headers, pixels); 3rd arg unused */
void huff_unpack_bank(u32 a0, u32 a2, u16 a4) {
    u16 t5;
    u16 t6;
    u16 t10;
    u32 t11;
    u16 t12;
    u32 t13;
    u16 t14;
    u32 t15;
    u16 t16;
    FUNC_TICK(111);
    HRS(hrs_bank_begin(a0));
    hr_bank_begin(a0);
    g_huff_count = FP8(a0, 0);
    g_huff_src = MK32(FPSEG(a0), (FPOFF(a0) + (g_huff_count << 1)) + 1);
    g_huff_hdr = a0;
    g_huff_src_pos = 0;
    G16(0xA64E) = 0;
    t5 = huff_read_byte();
    g_huff_nodes = t5;
    t6 = huff_read_byte();
    g_huff_nodes = t6 | (g_huff_nodes << 8);
    t10 = huff_read_byte();
    g_huff_remaining = MK32(((i16)t10) >> 0xF, t10);
    t11 = (8 >= 32 ? 0u : (u32)g_huff_remaining << 8);
    t12 = huff_read_byte();
    g_huff_remaining = MK32(HI16(t11) | (((i16)t12) >> 0xF), LO16(t11) | t12);
    t13 = (8 >= 32 ? 0u : (u32)g_huff_remaining << 8);
    t14 = huff_read_byte();
    g_huff_remaining = MK32(HI16(t13) | (((i16)t14) >> 0xF), LO16(t13) | t14);
    t15 = (8 >= 32 ? 0u : (u32)g_huff_remaining << 8);
    t16 = huff_read_byte();
    g_huff_remaining = MK32(HI16(t15) | (((i16)t16) >> 0xF), LO16(t15) | t16);
    g_huff_size_lo = g_huff_remaining_lo;
    g_huff_out = a2;
    FP16(g_huff_out, 0) = g_huff_count;
    g_huff_new_sprite = 1;
    g_huff_sprite = 0;
    g_huff_pixels = 0;
    g_huff_out_ofs = (g_huff_count << 3) + 2;
    huff_decode();
    hr_bank_done(g_huff_out);
    rtl_free(g_huff_child0_mem_lo, g_huff_child0_mem_hi);
    rtl_free(g_huff_child1_mem_lo, g_huff_child1_mem_hi);
    return;
}

/* 37B4:05DD  like huff_unpack_bank but computes the bank size, allocates it and frees src (unused) */
void huff_unpack_bank_alloc(u32 a0) {
    int cc;
    u16 di;
    u16 si;
    u16 t5;
    u16 t6;
    u16 t10;
    u32 t11;
    u16 t12;
    u32 t13;
    u16 t14;
    u32 t15;
    u16 t16;
    u32 t35;
    FUNC_TICK(133);
    HRS(hrs_bank_begin(a0));
    hr_bank_begin(a0);
    g_huff_count = FP8(a0, 0);
    g_huff_src = MK32(FPSEG(a0), (FPOFF(a0) + (g_huff_count << 1)) + 1);
    g_huff_hdr = a0;
    g_huff_src_pos = 0;
    G16(0xA64E) = 0;
    t5 = huff_read_byte();
    g_huff_nodes = t5;
    t6 = huff_read_byte();
    g_huff_nodes = t6 | (g_huff_nodes << 8);
    t10 = huff_read_byte();
    g_huff_remaining = MK32(((i16)t10) >> 0xF, t10);
    t11 = (8 >= 32 ? 0u : (u32)g_huff_remaining << 8);
    t12 = huff_read_byte();
    g_huff_remaining = MK32(HI16(t11) | (((i16)t12) >> 0xF), LO16(t11) | t12);
    t13 = (8 >= 32 ? 0u : (u32)g_huff_remaining << 8);
    t14 = huff_read_byte();
    g_huff_remaining = MK32(HI16(t13) | (((i16)t14) >> 0xF), LO16(t13) | t14);
    t15 = (8 >= 32 ? 0u : (u32)g_huff_remaining << 8);
    t16 = huff_read_byte();
    g_huff_remaining = MK32(HI16(t15) | (((i16)t16) >> 0xF), LO16(t15) | t16);
    g_huff_size_lo = g_huff_remaining_lo;
    g_huff_out_ofs = (g_huff_count << 3) + 2;
    si = 0;
    while (si != g_huff_count) {
        LOOP_TICK(31);
        cc = ((FP8(a0, (si << 1) + 1)) & 7) == 0;
        di = ((i16)(((i16)(((i16)(FP8(a0, (si << 1) + 1))) >> 1)) >> 1)) >> 1;
        if (!(cc)) {
            di = di + 1;
        }
        g_huff_out_ofs = g_huff_out_ofs + (((FP8(a0, (si << 1) + 2)) * di) << 3);
        si = si + 1;
    }
    t35 = rtl_calloc(g_huff_out_ofs, 1);
    g_huff_out = t35;
    FP16(g_huff_out, 0) = g_huff_count;
    g_huff_new_sprite = 1;
    g_huff_sprite = 0;
    g_huff_pixels = 0;
    g_huff_out_ofs = (g_huff_count << 3) + 2;
    huff_decode();
    hr_bank_done(g_huff_out);
    rtl_free(g_huff_child0_mem_lo, g_huff_child0_mem_hi);
    rtl_free(g_huff_child1_mem_lo, g_huff_child1_mem_hi);
    rtl_free(LO16(a0), HI16(a0));
    return;
}
