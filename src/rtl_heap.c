/* Borland far heap and huge-pointer helpers in C (see rtl.h): the parts of
 * the far heap that src/heap.c did not have yet -- farmalloc and its
 * free-list walk, brk/sbrk on the DOS memory block, calloc -- and the
 * register-convention helpers PADD@/PCMP@/LXMUL@ the heap code calls.
 * Same data structures in the same memory as the original: a block header
 * is { u32 size (bit 0 = used), u32 previous block, u32 next free, u32
 * previous free } at the block's start, data at +8. */
#include "game.h"
#include "hrs.h"
#include "hr.h"
#include "seat.h"

/* run-time library variables in DGROUP */
#define rtl_psp           G16(0x007B)   /* _psp: segment of the program segment prefix */
#define rtl_errno         G16(0x007F)
#define rtl_heapbase      G32(0x0087)   /* lowest address the break may take */
#define rtl_brklvl        G32(0x008B)   /* the break: end of the heap */
#define rtl_heaptop       G32(0x008F)   /* highest address the break may take */
#define rtl_heaptop_hi    G16(0x0091)
#define rtl_blocks_1k     G16(0x1830)   /* size of the DOS block last set, 1 KB units */
#define rtl_doserrno      G16(0x17B8)
#define rtl_dos_to_errno(i) G8((u16)(0x17BA + (i)))

/* 0000:03F1  PCMP@: compare huge pointers a and b (DX:AX, CX:BX) once
 * normalized (segment + offset / 16, offset & 15); result in CF/ZF, here
 * also returned: -1, 0, 1 */
int rtl_pcmp(u32 a, u32 b) {
    RTL_ENTRY();
    u16 sa = (u16)(HI16(a) + (LO16(a) >> 4)), oa = LO16(a) & 0xF;
    u16 sb = (u16)(HI16(b) + (LO16(b) >> 4)), ob = LO16(b) & 0xF;
    R.dx = sa; R.ax = oa; R.cx = sb; R.bx = ob;
    T += 15;
    sub16(sa, sb, 0);
    if (sa == sb) { sub16(oa, ob, 0); T += 1; }
    T += 1;
    if (sa != sb) return sa < sb ? -1 : 1;
    return oa < ob ? -1 : oa > ob;
}
void f_0000_03F1(void) {
    rtl_pcmp(MK32(R.dx, R.ax), MK32(R.cx, R.bx));
    R.sp += 4;
}

/* 0000:0396  PADD@: huge pointer p (DX:AX) + signed 32-bit d (CX:BX),
 * normalized (offset 0..15) */
u32 rtl_padd(u32 p, u32 d) {
    RTL_ENTRY();
    u16 dx = HI16(p), ax = LO16(p), cx = HI16(d), bx = LO16(d);
    T += 2;
    if ((i16)cx >= 0) {
        u32 s = (u32)ax + bx;
        ax = (u16)s;
        T += 2;
        if (s >> 16) { dx += 0x1000; T += 1; }
        dx += (u16)((cx & 0xF) << 12);
        T += 10;
    } else {
        u32 n = -d;                      /* subtract |d| */
        cx = HI16(n); bx = LO16(n);
        T += 5;
        u16 old = ax;
        ax = (u16)(ax - bx);
        T += 2;
        if (old < bx) { dx -= 0x1000; T += 1; }
        bx = (u16)((cx & 0xF) << 12);
        dx -= bx;
        T += 11;
    }
    dx += ax >> 4;
    R.cx = (u16)(((ax & 0xFF) << 8) | 4);
    R.bx = bx;
    R.dx = dx;
    R.ax = logic16(ax & 0xF);
    return MK32(R.dx, R.ax);
}
void f_0000_0396(void) {
    rtl_padd(MK32(R.dx, R.ax), MK32(R.cx, R.bx));
    R.sp += 4;
}

/* 0000:0412  LXMUL@: DX:AX * CX:BX, low 32 bits */
u32 rtl_lxmul(u32 a, u32 b) {
    RTL_ENTRY();
    u16 ahi = HI16(a), alo = LO16(a), bhi = HI16(b), blo = LO16(b);
    u16 cx = 0;
    T += 5;
    if (ahi != 0) { cx = (u16)(ahi * blo); T += 1; }
    T += 3;
    if (bhi != 0) { cx = (u16)(cx + bhi * alo); T += 2; }
    u32 r = (u32)alo * blo;
    R.dx = add16(HI16(r), cx, 0);
    R.ax = LO16(r);
    R.cx = cx; R.bx = blo;
    T += 5;
    return MK32(R.dx, R.ax);
}
void f_0000_0412(void) {
    rtl_lxmul(MK32(R.dx, R.ax), MK32(R.cx, R.bx));
    R.sp += 4;
}

/* 38BC:000C  __IOerror(code): DOS error code (or -errno) -> errno and
 * _doserrno; returns -1 (pascal: pops its argument) */
u32 rtl_ioerror(u16 code) {
    RTL_ENTRY();
    u16 si = code;
    T += 6;
    if ((i16)si < 0) {
        si = (u16)-si;
        T += 3;
        if (si > 0x23) goto unknown;
        rtl_doserrno = 0xFFFF;
        T += 1;
    } else {
        T += 2;
        if (si > 0x58) {
        unknown:
            POLL();
            si = 0x57;
            T += 1;
        }
        rtl_doserrno = si;
        si = (u16)(i16)(i8)rtl_dos_to_errno(si);
        T += 5;
    }
    rtl_errno = si;
    R.ax = 0xFFFF;
    T += 7;
    return MK32(R.dx, R.ax);
}

/* 390C:000C  setblock(seg, paragraphs): resize a DOS memory block (int 21h
 * AH=4Ah); -1 on success, else the largest size available */
static u16 rtl_setblock(u16 seg, u16 paras) {
    RTL_ENTRY();
    u16 avail = paras;
    T += 6;
    int e = plat_setblock(seg, paras, &avail);
    R.es = seg; R.bx = avail;
    T += 1;
    if (!e) {
        R.ax = 0xFFFF;
        T += 2;
    } else {
        T += 3;
        rtl_ioerror((u16)e);
        R.ax = avail;
        T += 2;
    }
    T += 2;
    return R.ax;
}

/* 38F7:000A  __brk(addr): move the break, growing or shrinking the
 * program's DOS block in 1 KB steps; 1 = done, 0 = no memory */
static u16 rtl_brk_set(u32 addr) {
    if (seat_cur > 0) {                  /* a seat's own heap, in the seat window: no DOS block to grow */
        rtl_brklvl = addr;
        u32 used = ((u32)HI16(addr) << 4) + LO16(addr) - ((u32)SEAT_HEAP_SEG << 4);
        if (used > seat_heap_peak[seat_cur]) seat_heap_peak[seat_cur] = used;
        R.bx = LO16(addr); R.es = HI16(addr);
        R.ax = 1;
        return 1;
    }
    RTL_ENTRY();
    u16 si = (u16)((u16)(HI16(addr) + 1 - rtl_psp + 0x3F) >> 6);
    R.cl = 6;
    T += 14;
    if (si == rtl_blocks_1k) {
        rtl_brklvl = addr;
        R.bx = LO16(addr); R.es = HI16(addr);
        R.ax = 1;
        T += 5;
    } else {
        si = (u16)(si << 6);
        T += 7;
        if ((u16)(si + rtl_psp) > rtl_heaptop_hi) { si = (u16)(rtl_heaptop_hi - rtl_psp); T += 2; }
        T += 3;
        u16 r = rtl_setblock(rtl_psp, si);
        R.cx = si;
        T += 5;
        if (r == 0xFFFF) {
            rtl_blocks_1k = si >> 6;
            rtl_brklvl = addr;
            R.cl = 6;
            R.bx = LO16(addr); R.es = HI16(addr);
            R.ax = 1;
            T += 9;
        } else {
            rtl_heaptop = MK32(rtl_psp + r, 0);
            R.dx = (u16)(rtl_psp + r);
            R.ax = 0;
            T += 9;
        }
    }
    T += 4;
    return R.ax;
}

/* 38F7:009A  brk(addr): 0, or -1 if addr is outside the heap or DOS has no
 * memory */
u32 rtl_brk(u32 addr) {
    RTL_ENTRY();
    T += 7;
    int c = rtl_pcmp(addr, rtl_heapbase);
    T += 1;
    if (c >= 0) {
        T += 5;
        c = rtl_pcmp(addr, rtl_heaptop);
        T += 1;
        if (c <= 0) {
            T += 3;
            u16 ok = rtl_brk_set(addr);
            R.ax = logic16(ok);
            T += 2;
            if (ok) { R.ax = logic16(0); T += 4; return MK32(R.dx, R.ax); }
        }
    }
    R.ax = 0xFFFF;
    T += 4;
    return MK32(R.dx, R.ax);
}
void f_38F7_009A(void) {
    rtl_brk(rd(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

/* 38F7:00E1  sbrk(incr): move the break by incr; the old break, or -1 */
u32 rtl_sbrk(u32 incr) {
    RTL_ENTRY();
    T += 8;
    u32 nb = rtl_padd(rtl_brklvl, incr);
    T += 7;
    int c = rtl_pcmp(nb, rtl_heapbase);
    T += 1;
    if (c >= 0) {
        T += 5;
        c = rtl_pcmp(nb, rtl_heaptop);
        T += 1;
        if (c <= 0) {
            u32 old = rtl_brklvl;
            R.bx = LO16(old); R.es = HI16(old);
            T += 6;
            u16 ok = rtl_brk_set(nb);
            T += 2;
            if (ok) {
                R.dx = HI16(old); R.ax = LO16(old);
                T += 6;
                return MK32(R.dx, R.ax);
            }
            R.dx = 0xFFFF; R.ax = 0xFFFF;
            T += 6;
            return MK32(R.dx, R.ax);
        }
    }
    R.dx = 0xFFFF; R.ax = 0xFFFF;
    T += 6;
    return MK32(R.dx, R.ax);
}
void f_38F7_00E1(void) {
    rtl_sbrk(rd(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

/* 38C6:001F  unlink free block b from the free list; the rover moves to its
 * successor (0 if b was the only free block) */
u32 rtl_pull_free_block(u32 b) {
    RTL_ENTRY();
    u32 next = FP32(b, 0xC);
    g_heap_rover = next;
    T += 11;
    int c = rtl_pcmp(next, b);
    T += 1;
    if (c == 0) {
        g_heap_rover = 0;
        T += 3;
    } else {
        u32 prev = FP32(b, 8);
        FP32(g_heap_rover, 8) = prev;
        FP32(prev, 0xC) = g_heap_rover;
        R.dx = g_heap_rover_hi; R.ax = g_heap_rover_lo;
        R.bx = LO16(prev); R.es = HI16(prev);
        T += 14;
    }
    T += 3;
    return MK32(R.dx, R.ax);
}
void f_38C6_001F(void) {
    rtl_pull_free_block(rd(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

/* 38C6:020B  farmalloc(nbytes): first fit from the rover; a block at
 * least 48 bytes larger is split, a fitting one taken whole, else the heap
 * grows */
u32 rtl_farmalloc(u32 nbytes) {
    RTL_ENTRY();
    T += 6;
    if (nbytes == 0) {
        R.dx = 0; R.ax = 0;
        T += 3 + 3;
        return 0;
    }
    u32 n = (nbytes + 0x17) & 0xFFFFFFF0u;   /* with the 8-byte header, in paragraphs */
    u32 r;
    T += 13;
    int c = rtl_pcmp(g_heap_first, 0);
    T += 1;
    if (c == 0) {
        T += 4;
        r = heap_create_first_block(n);
        R.cx = HI16(n);
        T += 3;
        goto out;
    }
    u32 b = g_heap_rover;
    T += 7;
    c = rtl_pcmp(b, 0);
    T += 1;
    if (c == 0) { T += 1; goto grow; }
    for (;;) {
        POLL();
        u32 size = FP32(b, 0);
        u32 need = n + 0x30;
        T += 9;
        if (HI16(size) >= HI16(need) &&
            (T += 1, HI16(size) != HI16(need) || (T += 2, LO16(size) >= LO16(need)))) {
            T += 6;                      /* split it */
            r = heap_allocate_partial_block(b, n);
            T += 2;
            goto out;
        }
        T += 5;
        if (HI16(size) >= HI16(n) &&
            (T += 1, HI16(size) != HI16(n) || (T += 2, LO16(size) >= LO16(n)))) {
            T += 4;                      /* take it whole */
            rtl_pull_free_block(b);
            R.cx = HI16(b);
            FP32(b, 0) = FP32(b, 0) + 1;
            R.bx = LO16(b); R.es = HI16(b);
            r = MK32(HI16(b), LO16(b) + 8);
            T += 9;
            goto out;
        }
        b = FP32(b, 0xC);
        T += 9;
        c = rtl_pcmp(b, g_heap_rover);
        T += 1;
        if (c == 0) break;
        T += 1;
    }
grow:
    T += 4;
    r = heap_create_block(n);
    R.cx = HI16(n);
    T += 3;
out:
    R.dx = HI16(r); R.ax = LO16(r);
    T += 3;
    return r;
}
void f_38C6_020B(void) {
    rtl_farmalloc(rd(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

/* 38C6:000A  malloc(size) = farmalloc */
u32 rtl_malloc(u16 size) {
    RTL_ENTRY();
    T += 7;
    rtl_farmalloc(size);
    T += 4;
    return MK32(R.dx, R.ax);
}
void f_38C6_000A(void) {
    rtl_malloc(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

/* 3976:0001  setmem(p, n, value): bytes, words where aligned */
static void rtl_setmem(u32 p, u16 n, u8 v) {
    RTL_ENTRY();
    u16 seg = HI16(p), di = LO16(p);
    HRS(hrs_setmem(p, n, v));
    hr_setmem(p, n, v);
    T += 11;
    if (di & 1) {
        T += 1;
        if (n == 0) goto done;
        T += 1;
        wb(seg, di++, v); n--;
        T += 1;
    }
    T += 2;
    for (u16 k = n >> 1; k; k--) { ww(seg, di, (u16)(v << 8 | v)); di += 2; T += 1; }
    T += 1;
    if (n & 1) { T += 1; wb(seg, di++, v); }
    n = 0;
done:
    R.es = seg; R.cx = n; R.ax = (u16)(v << 8 | v); R.df = 0;
    T += 4;
}

/* 39C1:000B  calloc(n, size): zeroed; 0 if n * size exceeds 64 KB */
u32 rtl_calloc(u16 n, u16 size) {
    RTL_ENTRY();
    T += 8;
    u32 total = rtl_lxmul(n, size);
    u32 p;
    T += 4;
    if (HI16(total) != 0) {
        T += 1 + 4;
        p = 0;
        R.bx = 0; R.es = 0;
    } else {
        T += 1 + 2 + 2;
        p = rtl_malloc(LO16(total));
        R.cx = LO16(total); R.bx = LO16(p); R.es = HI16(p);
        T += 3;
    }
    R.ax = LO16(p);
    T += 5;
    if (p != 0) {
        T += 6;
        rtl_setmem(p, LO16(total), 0);
        T += 1;
    }
    R.dx = HI16(p); R.ax = LO16(p);
    T += 6;
    return p;
}
void f_39C1_000B(void) {
    rtl_calloc(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

/* PADA@ arithmetic and time, after its interrupt check: p + d normalized */
static u32 pada_math(u32 p, u32 d) {
    u16 dx = HI16(p), bx = LO16(p), cx = HI16(d), ax = LO16(d);
    T += 7;
    if ((i16)cx >= 0) {
        u32 s = (u32)bx + ax;
        bx = (u16)s;
        T += 2;
        if (s >> 16) { dx += 0x1000; T += 1; }
        dx += (u16)((cx & 0xF) << 12);
    } else {
        u32 n = -d;
        cx = HI16(n); ax = LO16(n);
        T += 5;
        u16 old = bx;
        bx = (u16)(bx - ax);
        T += 2;
        if (old < ax) { dx -= 0x1000; T += 1; }
        dx -= (u16)((cx & 0xF) << 12);
    }
    dx += bx >> 4;
    R.cx = (u16)(((bx & 0xFF) << 8) | 4);
    R.ax = bx & 0xF;
    R.dx = dx;
    T += 14;
    return MK32(R.dx, R.ax);
}

/* 0000:0313  PADA@: *pp += d for the huge pointer at pp (DX:AX), d in CX:BX */
u32 rtl_pada(u16 off, u16 seg, u32 d) {
    RTL_ENTRY();
    u32 r = pada_math(MK32(rw(seg, (u16)(off + 2)), rw(seg, off)), d);
    R.bx = off; R.es = seg;
    ww(seg, off, LO16(r));
    ww(seg, (u16)(off + 2), HI16(r));
    return r;
}
void f_0000_0313(void) {
    rtl_pada(R.ax, R.dx, MK32(R.cx, R.bx));
    R.sp += 4;
}

/* 397C:0007  farcalloc(n, size): zeroed, cleared in pieces of at most 0xFA00 bytes */
u32 rtl_farcalloc(u32 n, u32 size) {
    RTL_ENTRY();
    T += 9;
    u32 total = rtl_lxmul(n, size);
    T += 5;
    u32 p = rtl_farmalloc(total);
    R.cx = HI16(total);
    T += 6;
    if (p != 0) {
        u32 q = p;
        T += 4;
        for (;;) {
            T += 3;
            if (total == 0) break;
            POLL();
            u16 chunk;
            T += 2;
            if (HI16(total) != 0) { T += 1 + 2; chunk = 0xFA00; }
            else { T += 1 + 2; if (LO16(total) <= 0xFA00) { chunk = LO16(total); T += 1; } else { chunk = 0xFA00; T += 2; } }
            T += 6;
            rtl_setmem(q, chunk, 0);
            T += 6;
            POLL();                      /* PADA@ on q, a local in the original's frame */
            q = pada_math(q, chunk);
            total -= chunk;
            T += 4;
        }
    }
    R.dx = HI16(p); R.ax = LO16(p);
    T += 3 + 4;
    return p;
}
void f_397C_0007(void) {
    rtl_farcalloc(rd(R.ss, (u16)(R.sp + 4)), rd(R.ss, (u16)(R.sp + 8)));
    R.sp += 4;
}
