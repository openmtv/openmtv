/* Borland run-time library and assembler helpers in C: see rtl.h. */
#include "game.h"
#include "seat.h"

/* 3971:000D  strlen: length of the far string seg:off (repne scasb, at most 65535) */
u32 rtl_strlen_w(u16 off, u16 seg) {
    RTL_ENTRY();
    u16 cx = 0xFFFF, di = off, n = 0;
    while (cx != 0) {                    /* repne scasb with AL = 0 */
        u8 c = rb(seg, di);
        di++; cx--; n++;
        if (c == 0) break;
    }
    T += 9 + n + 8;                      /* 9 up to the scan, 1 per byte, 8 after */
    R.df = 0; R.es = seg; R.cx = cx; R.ax = (u16)(~cx - 1);
    return MK32(R.dx, R.ax);
}
void f_3971_000D(void) {
    rtl_strlen_w(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

/* 39D0:0006  strncpy(dest, src, n): copies up to n bytes of src, pads with 0 to n */
u32 rtl_strncpy_w(u16 doff, u16 dseg, u16 soff, u16 sseg, u16 n) {
    RTL_ENTRY();
    u16 len = 0;                         /* repne scasb over src: bytes up to and with its 0 */
    while (len < n) { u8 c = rb(sseg, (u16)(soff + len)); len++; if (c == 0) break; }
    for (u16 i = 0; i < len; i++) wb(dseg, (u16)(doff + i), rb(sseg, (u16)(soff + i)));
    for (u16 i = len; i < n; i++) wb(dseg, (u16)(doff + i), 0);
    T += 11 + len + 7 + len + 2 + (n - len) + 8;
    R.df = 0; R.es = dseg; R.cx = 0; R.bx = (u16)(n - len);
    R.dx = dseg; R.ax = doff;
    return MK32(R.dx, R.ax);
}
void f_39D0_0006(void) {
    u16 sp = R.sp;
    rtl_strncpy_w(rw(R.ss, (u16)(sp + 4)), rw(R.ss, (u16)(sp + 6)), rw(R.ss, (u16)(sp + 8)), rw(R.ss, (u16)(sp + 10)), rw(R.ss, (u16)(sp + 12)));
    R.sp += 4;
}

/* 3973:0008  strcpy(dest, src) */
u32 rtl_strcpy_w(u16 doff, u16 dseg, u16 soff, u16 sseg) {
    RTL_ENTRY();
    u16 len = 0;                         /* repne scasb, CX = FFFF: bytes with the 0 */
    do { len++; } while (rb(sseg, (u16)(soff + len - 1)) != 0 && len != 0xFFFF);
    for (u16 i = 0; i < len; i++) wb(dseg, (u16)(doff + i), rb(sseg, (u16)(soff + i)));
    T += 10 + len + 6 + len + 8;
    R.df = 0; R.es = dseg; R.cx = 0;
    R.dx = dseg; R.ax = doff;
    return MK32(R.dx, R.ax);
}
void f_3973_0008(void) {
    u16 sp = R.sp;
    rtl_strcpy_w(rw(R.ss, (u16)(sp + 4)), rw(R.ss, (u16)(sp + 6)), rw(R.ss, (u16)(sp + 8)), rw(R.ss, (u16)(sp + 10)));
    R.sp += 4;
}

/* 39B8:0001  abs */
u32 rtl_abs(u16 v) {
    RTL_ENTRY();
    T += 5;
    if ((i16)v < 0) { v = (u16)-v; T += 1; }
    T += 3;
    R.ax = v;
    return MK32(R.dx, R.ax);
}
void f_39B8_0001(void) {
    rtl_abs(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

/* 39B9:0001  atol: optional white space (_ctype at DS:1833, bit 0), sign,
 * decimal digits; 16-bit arithmetic while the value fits, then 32-bit */
static u32 rtl_atol(u16 off, u16 seg) {
    RTL_ENTRY();
    u16 ax = 0, dx = 0, cx = 10, bx;
    int neg = 0;
    u8 c;
    T += 13;
    for (;;) {                           /* skip white space */
        POLL();
        c = rb(seg, off++);
        T += 4;
        if (!(G8(0x1833 + c) & 1)) break;
    }
    T += 3;
    if (c == '+' || (T += 2, c == '-')) {
        if (c == '-') { neg = 1; T += 1; }
        goto next16;
    }
    for (;;) {                           /* 16 bits: AX = AX * 10 + digit */
        T += 2;
        if (c > '9') { bx = c; goto done; }
        T += 2;
        bx = (u8)(c - '0');
        if (c < '0') goto done;
        {
            u32 p = (u32)ax * cx;
            u32 s = (p & 0xFFFF) + bx;
            ax = (u16)s;
            dx = (u16)((p >> 16) + (s >> 16));   /* adc dl, dh: DH is 0 here */
        }
        T += 4;
        if (dx != 0) { T += 1; break; }
    next16:
        POLL();
        c = rb(seg, off++);
        T += 2;
    }
    for (;;) {                           /* 32 bits: DX:AX = DX:AX * 10 + digit */
        c = rb(seg, off++);
        T += 4;
        if (c > '9') { bx = c; goto done; }
        T += 2;
        bx = (u8)(c - '0');
        if (c < '0') goto done;
        POLL();
        u32 lo = (u32)ax * 10, hi = (u32)dx * 10;
        u32 s = (lo & 0xFFFF) + bx;
        ax = (u16)s;
        cx = (u16)(lo >> 16);
        dx = (u16)(hi + cx + (s >> 16));
        T += 10;
    }
done:
    T += 2;
    if (neg) { u32 v = -MK32(dx, ax); ax = (u16)v; dx = (u16)(v >> 16); T += 3; }
    T += 7;
    R.ax = ax; R.dx = dx; R.cx = cx; R.bx = bx;
    return MK32(dx, ax);
}

/* 39B9:0078  atoi = (int)atol */
u32 rtl_atoi_w(u16 off, u16 seg) {
    RTL_ENTRY();
    T += 6;
    rtl_atol(off, seg);
    T += 4;
    return MK32(R.dx, R.ax);
}
void f_39B9_0078(void) {
    rtl_atoi_w(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

/* 3962:0001  __longtoa(value, buf, radix, is_signed, hexbase): digits of
 * value in radix 2..36 into buf (a '-' first if is_signed and negative),
 * 0-terminated; returns buf */
static u32 rtl_longtoa(u32 v, u16 doff, u16 dseg, u16 radix, int is_signed, u8 hexbase) {
    RTL_ENTRY();
    u16 di = doff;
    R.bx = radix;
    T += 10;
    if (radix > 36 || (T += 2, (u8)radix < 2)) goto end;
    T += 4;
    if ((i32)v < 0 && (T += 2, is_signed)) {
        wb(dseg, di++, '-');
        v = -v;
        T += 5;
    }
    u8 digits[34];
    int n = 0;
    u16 hi = (u16)(v >> 16), lo = (u16)v;
    T += 2;
    if (hi != 0) {                       /* 32-bit division while the high word is nonzero */
        for (;;) {
            POLL();
            u32 x = ((u32)(hi % radix) << 16) | lo;
            hi = hi / radix;
            lo = (u16)(x / radix);
            digits[n++] = (u8)(x % radix);
            T += 8;
            if (hi == 0) break;
            T += 1;
        }
        T += 2;
        if (lo == 0) goto out;
    }
    for (;;) {
        POLL();
        digits[n++] = (u8)(lo % radix);
        lo = lo / radix;
        T += 4 + 2;
        if (lo == 0) break;
    }
out:
    T += 4;
    while (n > 0) {
        POLL();
        u8 d = digits[--n];
        T += 4;
        if (d >= 10) { d = (u8)(d - 10 + hexbase); T += 1; } else { d = (u8)(d + '0'); T += 2; }
        wb(dseg, di++, d);
        T += 2;
    }
    R.df = 0; R.cx = 0;
end:
    wb(dseg, di, 0);
    T += 2 + 4 + 5;
    R.dx = dseg; R.ax = doff;
    return MK32(R.dx, R.ax);
}

/* 3962:0085  itoa(value, buf, radix): signed in radix 10 */
u32 rtl_itoa_w(u16 v, u16 doff, u16 dseg, u16 radix) {
    RTL_ENTRY();
    T += 4;
    u32 x;
    if (radix == 10) { x = (u32)(i32)(i16)v; T += 3; } else { x = v; T += 2; }
    T += 11;
    rtl_longtoa(x, doff, dseg, radix, 1, 'a');
    T += 3;
    return MK32(R.dx, R.ax);
}
void f_3962_0085(void) {
    u16 sp = R.sp;
    rtl_itoa_w(rw(R.ss, (u16)(sp + 4)), rw(R.ss, (u16)(sp + 6)), rw(R.ss, (u16)(sp + 8)), rw(R.ss, (u16)(sp + 10)));
    R.sp += 4;
}

/* 3962:00D2  ltoa(value, buf, radix): signed in radix 10 */
u32 rtl_ltoa_w(u16 vlo, u16 vhi, u16 doff, u16 dseg, u16 radix) {
    RTL_ENTRY();
    T += 9;
    if (radix == 10) T += 2; else T += 1;
    T += 5;
    rtl_longtoa(MK32(vhi, vlo), doff, dseg, radix, radix == 10, 'a');
    T += 3;
    return MK32(R.dx, R.ax);
}
void f_3962_00D2(void) {
    u16 sp = R.sp;
    rtl_ltoa_w(rw(R.ss, (u16)(sp + 4)), rw(R.ss, (u16)(sp + 6)), rw(R.ss, (u16)(sp + 8)), rw(R.ss, (u16)(sp + 10)), rw(R.ss, (u16)(sp + 12)));
    R.sp += 4;
}

/* 39CC:000D  srand: the 32-bit seed at DS:1960 */
u32 rtl_srand(u16 seed) {
    RTL_ENTRY();
    G16(0x1962) = 0; G16(0x1960) = seed;
    T += 8;
    R.ax = seed; R.dx = 0;
    return MK32(R.dx, R.ax);
}
void f_39CC_000D(void) {
    rtl_srand(rw(R.ss, (u16)(R.sp + 4)));
    R.sp += 4;
}

/* 39CC:001E  rand: seed = seed * 0x015A4E35 + 1, returns bits 16..30 */
u32 rtl_rand(void) {
    RTL_ENTRY();
    u32 s = MK32(G16(0x1962), G16(0x1960));
    T += 5;
    POLL();                              /* 0000:0412 long multiply */
    T += 5 + (G16(0x1962) != 0) + 3 + 2 + 5;
    R.cx = (u16)(G16(0x1962) * 0x4E35u + G16(0x1960) * 0x015Au);
    R.bx = 0x4E35;
    s = s * 0x015A4E35u + 1;
    G16(0x1962) = (u16)(s >> 16); G16(0x1960) = (u16)s;
    T += 8;
    R.dx = (u16)(s >> 16); R.ax = R.dx & 0x7FFF;
    return MK32(R.dx, R.ax);
}
void f_39CC_001E(void) {
    rtl_rand();
    R.sp += 4;
}

/* 3859:0002  key_read: the next key from the BIOS keyboard buffer (int 16h)
 * as a game key code -- table DS:16AE by ASCII code, DS:1730 by scan code
 * for extended keys -- FFFF if there is none or it is unmapped */
u32 rtl_key_read(void) {
    RTL_ENTRY();
    u16 ds = R.ds, bx = R.bx;
    R.ds = DSEG;
    u16 k;
    R.ah = 1;                            /* key waiting? */
    T += 6;
    int have = plat_key_peek(&k);
    if (have) R.ax = k;
    T += 1;
    if (!have) goto none;
    R.ah = 0;                            /* read it */
    T += 2;
    R.ax = plat_key_read();
    T += 2;
    if (R.al != 0) {
        R.ax = G8(0x16AE + (R.al & 0x7F));
        T += 6;
    } else {
        R.ax = G8(0x1730 + (R.ah & 0x7F));
        T += 7;
    }
    if (R.al == 0xFF) goto none;
    T += 3;
    R.bx = bx; R.ds = ds;
    return MK32(R.dx, R.ax);
none:
    R.ax = 0xFFFF;
    T += 4;
    R.bx = bx; R.ds = ds;
    return MK32(R.dx, R.ax);
}
void f_3859_0002(void) {
    rtl_key_read();
    R.sp += 4;
}

/* 373B:0009  mouse_read: buttons (int 33h function 3); the movement since
 * the last call, in pixels, into DS:16A4/16A6 -- the driver's cursor is put
 * back to (1600, 1600) each time, so the position minus 1600 is the delta */
u32 rtl_mouse_read(void) {
    RTL_ENTRY();
    u16 ds = R.ds, dx = R.dx, cx = R.cx, bx = R.bx;
    u16 x, y;
    T += 8;
    u16 buttons = plat_mouse_read(&x, &y);
    g_mouse_rel_x = (u16)((x >> 3) - 200);
    g_mouse_rel_y = (u16)((y >> 3) - 200);
    T += 15;
    plat_mouse_set_pos(0x640, 0x640);
    R.ax = buttons;
    R.bx = bx; R.cx = cx; R.dx = dx; R.ds = ds;
    T += 6;
    return MK32(R.dx, R.ax);
}
void f_373B_0009(void) {
    rtl_mouse_read();
    R.sp += 4;
}

/* 3742:0001  wait for the start of the next vertical retrace (port 3DA bit 3) */
u32 rtl_wait_retrace(void) {
    if (seat_cur > 0) {                  /* seats: only seat 0 waits for the frame; the others pass */
        seat_pass();                     /* their turn on (runtime/seat.h) */
        return MK32(R.dx, R.ax);
    }
    seat_pass();                         /* seat 0: the others run while it waits */
    RTL_ENTRY();
    T += 2;
    do { POLL(); T += 1; R.al = plat_vga_status(); T += 2; } while (R.al & 8);
    do { POLL(); T += 1; R.al = plat_vga_status(); T += 2; } while (!(R.al & 8));
    T += 2;
    return MK32(R.dx, R.ax);
}
void f_3742_0001(void) {
    rtl_wait_retrace();
    R.sp += 4;
}

/* 38C3:000B  atexit(fn): register a far function for exit (at most 32,
 * table DS:A6A2, count DS:1822); 0, or 1 if the table is full */
u32 rtl_atexit(u16 off, u16 seg) {
    RTL_ENTRY();
    T += 4;
    if (G16(0x1822) == 0x20) {
        R.ax = 1;
        T += 2;
    } else {
        u16 n = G16(0x1822);
        G16((u16)(0xA6A2 + n * 4)) = off;
        G16((u16)(0xA6A4 + n * 4)) = seg;
        G16(0x1822) = (u16)(n + 1);
        R.dx = seg; R.bx = (u16)(n << 2);
        R.ax = 0;
        T += 10;
    }
    T += 2;
    return MK32(R.dx, R.ax);
}
void f_38C3_000B(void) {
    rtl_atexit(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

/* 373F:0005  video adapter check: always 0x100 (VGA) */
u32 rtl_video_detect(void) {
    RTL_ENTRY();
    R.ax = 0x100;
    T += 2;
    return MK32(R.dx, R.ax);
}
void f_373F_0005(void) {
    rtl_video_detect();
    R.sp += 4;
}

/* 373F:0009  remember the BIOS video mode (DS:16AC), switch to mode 13h */
u32 rtl_video_mode13(void) {
    RTL_ENTRY();
    T += 2;
    u8 mode = plat_video_mode();
    G8(0x16AC) = mode;
    R.bh = 0;                            /* the BIOS's page */
    T += 10;
    plat_video_set_mode(0x13);
    R.ax = 0x13;
    T += 1;
    return MK32(R.dx, R.ax);
}
void f_373F_0009(void) {
    rtl_video_mode13();
    R.sp += 4;
}

/* 3741:0000  back to the remembered video mode (an atexit function) */
u32 rtl_video_restore(void) {
    RTL_ENTRY();
    T += 6;
    plat_video_set_mode(G8(0x16AC));
    R.ax = 0;
    T += 3;
    return MK32(R.dx, R.ax);
}
void f_3741_0000(void) {
    rtl_video_restore();
    R.sp += 4;
}

/* 3743:0001  set the VGA palette from a block { u16 count; rgb[count] } (int 10h AX=1012h) */
u32 rtl_palette_set(u16 off, u16 seg) {
    RTL_ENTRY();
    T += 16;
    plat_palette_set(0, HP16(HPTR(MK32(seg, off)), 0), HPTR(MK32(seg, off + 2)));
    R.ax = 0x1012;
    T += 7;
    return MK32(R.dx, R.ax);
}
void f_3743_0001(void) {
    rtl_palette_set(rw(R.ss, (u16)(R.sp + 4)), rw(R.ss, (u16)(R.sp + 6)));
    R.sp += 4;
}

/* 3745:0008  video segments for the blit helpers: DS:16A8 = DS:16AA = A000 */
u32 rtl_video_segs(void) {
    RTL_ENTRY();
    g_vga_seg = 0xA000;
    G16(0x16AA) = 0xA000;
    R.ax = DSEG;
    T += 16;
    return MK32(R.dx, R.ax);
}
void f_3745_0008(void) {
    rtl_video_segs();
    R.sp += 4;
}

/* 3858:0000  keyboard check: always 1 */
u32 rtl_kbd_detect(void) {
    RTL_ENTRY();
    R.ax = 1;
    T += 2;
    return MK32(R.dx, R.ax);
}
void f_3858_0000(void) {
    rtl_kbd_detect();
    R.sp += 4;
}

/* 3858:0004  empty the BIOS keyboard buffer */
u32 rtl_kbd_flush(void) {
    for (;;) {
        RTL_ENTRY();
        u16 k;
        R.ah = 1;
        T += 2;
        int have = plat_key_peek(&k);
        if (have) R.ax = k;
        T += 1;
        if (!have) break;
        R.ah = 0;
        T += 2;
        R.ax = plat_key_read();
        T += 1;
    }
    T += 1;
    return MK32(R.dx, R.ax);
}
void f_3858_0004(void) {
    rtl_kbd_flush();
    R.sp += 4;
}

/* 3736:0007  mouse driver present? (int 33h reset) 1 or 0 */
u32 rtl_mouse_detect(void) {
    RTL_ENTRY();
    u16 b;
    R.ax = 0;
    T += 2;
    if (plat_mouse_reset(&b)) R.ax = 0xFFFF;
    R.bx = b;
    T += 2;
    if (R.ax == 0xFFFF) { R.ax = 1; T += 2; }
    else T += 1;
    return MK32(R.dx, R.ax);
}
void f_3736_0007(void) {
    rtl_mouse_detect();
    R.sp += 4;
}

/* 3737:0005  mouse setup: range 0..3200 both ways, mickey ratio 3, cursor
 * to (1600, 1600), no motion pending (see mouse_read) */
u32 rtl_mouse_setup(void) {
    RTL_ENTRY();
    T += 7;
    plat_mouse_range_x(0, 0xC80);
    T += 4;
    plat_mouse_range_y(0, 0xC80);
    T += 4;
    plat_mouse_ratio(3, 3);
    T += 4;
    plat_mouse_set_pos(0x640, 0x640);
    g_mouse_rel_x = 0;
    g_mouse_rel_y = 0;
    R.ax = DSEG;
    T += 8;
    return MK32(R.dx, R.ax);
}
void f_3737_0005(void) {
    rtl_mouse_setup();
    R.sp += 4;
}
