/* BIOS services and mouse driver, implemented natively.
 * Every IVT entry initially points at BIOS_SEG:n; executing there (or an INT
 * whose vector still points there) calls bios_dispatch(n). */
#include <stdio.h>
#include <string.h>
#include "runtime.h"

int pic_ack(void);
int bios_trace;

#define BDA 0x40
/* the keyboard buffer: a ring of 16 words like the BIOS's (BDA 0040:001E),
 * one slot always free; kept here, not in the emulated BIOS data area --
 * nothing in the game reads that area */
static u16 kb_ring[16];
static int kb_head, kb_tail;
static void kb_init(void) { kb_head = kb_tail = 0; }
int bios_key_available(void) { return kb_head != kb_tail; }
void bios_key_push(u16 k) {
    int nt = (kb_tail + 1) & 15;
    if (nt == kb_head) return;                 /* full */
    kb_ring[kb_tail] = k; kb_tail = nt;
}
static u16 kb_peek(void) { return kb_ring[kb_head]; }
static u16 kb_pop(void) {
    u16 k = kb_ring[kb_head];
    kb_head = (kb_head + 1) & 15;
    return k;
}

void bios_init(void) {
    for (int n = 0; n < 256; n++) {
        ww(0, n * 4, (u16)n);
        ww(0, n * 4 + 2, BIOS_SEG);
        wb(BIOS_SEG, (u16)n, 0xCF);              /* IRET, should anything execute it raw */
    }
    memset(M + 0x400, 0, 0x100);
    ww(BDA, 0x10, 0x0021);                       /* equipment: FPU-less, VGA, mouse port */
    ww(BDA, 0x13, 640);                          /* KB of conventional memory */
    wb(BDA, 0x49, 3);                            /* video mode */
    ww(BDA, 0x4A, 80);
    wb(BDA, 0x84, 24);
    ww(BDA, 0x63, 0x3D4);
    kb_init();
}

/* ---------------- INT 10h ---------------- */
/* AH=00h: set the video mode (bit 7: keep the screen contents) */
void bios_video_set_mode(u8 al) {
    vga_mode = al & 0x7F;
    { extern void vga_set_planar(int); vga_set_planar(vga_mode == 0x0D || vga_mode == 0x0E || vga_mode == 0x10 || vga_mode == 0x12); }
    wb(BDA, 0x49, (u8)vga_mode);
    if (!(al & 0x80)) {
        if (vga_mode == 0x13) memset(M + 0xA0000, 0, 0x10000);
        else memset(M + 0xB8000, 0, 0x8000);
    }
    if (vga_mode == 0x13) {
        /* default VGA palette is irrelevant to the game (it loads its own) */
        for (int i = 0; i < 256; i++) vga_dac[i][0] = vga_dac[i][1] = vga_dac[i][2] = (u8)((i & 15) * 4);
    }
}
u8 bios_video_mode(void) { return (u8)vga_mode; }
/* AX=1012h: DAC registers first..first+n-1 from seg:off (r, g, b; 6 bits) */
void bios_dac_set(u16 first, u16 n, const u8 *rgb) {
    for (int i = 0; i < n; i++) {
        int c = (first + i) & 0xFF;
        for (int k = 0; k < 3; k++) vga_dac[c][k] = *rgb++ & 63;
    }
}

void int10(void) {
    switch (R.ah) {
    case 0x00: bios_video_set_mode(R.al); break;
    case 0x0F: R.al = (u8)vga_mode; R.ah = vga_mode == 0x13 ? 40 : 80; R.bh = 0; break;
    case 0x10:
        switch (R.al) {
        case 0x00: { extern u8 vga_attr[32]; vga_attr[R.bl & 0x1F] = R.bh; } break;
        case 0x01: { extern u8 vga_attr[32]; vga_attr[0x11] = R.bh; } break;
        case 0x02: { extern u8 vga_attr[32]; for (int i = 0; i < 16; i++) vga_attr[i] = rb(R.es, (u16)(R.dx + i)); vga_attr[0x11] = rb(R.es, (u16)(R.dx + 16)); } break;
        case 0x03: break;
        case 0x13: { extern u8 vga_attr[32];
            if (R.bl == 0) { if (R.bh) vga_attr[0x10] |= 0x80; else vga_attr[0x10] &= 0x7F; }
            else vga_attr[0x14] = (vga_attr[0x10] & 0x80) ? (R.bh & 0x0F) : (u8)((R.bh & 3) << 2);
        } break;
        case 0x10: vga_dac[R.bx & 0xFF][0] = R.dh & 63; vga_dac[R.bx & 0xFF][1] = R.ch & 63; vga_dac[R.bx & 0xFF][2] = R.cl & 63; break;
        case 0x12: bios_dac_set(R.bx, R.cx, M + lin(R.es, R.dx)); break;
        case 0x15: R.dh = vga_dac[R.bl][0]; R.ch = vga_dac[R.bl][1]; R.cl = vga_dac[R.bl][2]; break;
        case 0x17: {
            u16 o = R.dx;
            for (int i = 0; i < R.cx; i++) {
                int c = (R.bx + i) & 0xFF;
                for (int k = 0; k < 3; k++) wb(R.es, o++, vga_dac[c][k]);
            }
        } break;
        default: break;
        }
        break;
    case 0x0E: if (R.al >= 32 || R.al == '\n') fputc(R.al, stderr); break;
    case 0x02: case 0x03: case 0x01: case 0x05: case 0x06: case 0x07: case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D:
        break;
    case 0x1A: R.al = 0x1A; R.bx = 0x0008; break;    /* VGA with colour display */
    case 0x11:
        if (R.al == 0x30) {           /* font information: VGA 8x16, 25 rows */
            R.cx = 16; R.dl = 24; R.es = 0xC000; R.bp = 0x0000;
        }
        break;
    case 0x12: if (R.bl == 0x10) { R.bh = 0; R.bl = 3; R.cx = 9; } break;
    default:
        fprintf(stderr, "int10 unhandled AH=%02X AL=%02X\n", R.ah, R.al);
    }
}

/* ---------------- INT 16h ---------------- */
/* AH=00h: the next key (scan code << 8 | ASCII), waiting for one */
u16 bios_key_read(void) {
    while (!bios_key_available()) {
        /* blocked on the keyboard: let virtual time pass */
        runtime_halt_wait();
    }
    return kb_pop();
}
/* AH=01h: is a key waiting?  Its code without taking it */
int bios_key_peek(u16 *key) {
    time_advance(20);                     /* polling costs time, lets events run */
    if (T >= next_event_T) runtime_events();
    if (!bios_key_available()) return 0;
    *key = kb_peek();
    return 1;
}

void int16(void) {
    switch (R.ah) {
    case 0x00: case 0x10:
        R.ax = bios_key_read();
        break;
    case 0x01: case 0x11: {
        u16 k;
        if (bios_key_peek(&k)) { R.zf = 0; R.ax = k; }
        else R.zf = 1;
        if (bios_trace) fprintf(stderr, "  int16/1 -> zf=%d ax=%04X\n", R.zf, R.ax);
        break;
    }
    case 0x02: R.al = rb(BDA, 0x17); break;
    default: fprintf(stderr, "int16 unhandled AH=%02X\n", R.ah);
    }
}

/* ---------------- INT 1Ah ---------------- */
/* AH=00h: timer ticks since midnight; *midnight: the day rolled over since the last call */
u32 bios_ticks(u8 *midnight) {
    *midnight = rb(BDA, 0x70); wb(BDA, 0x70, 0);
    return ((u32)rw(BDA, 0x6E) << 16) | rw(BDA, 0x6C);
}

void int1a(void) {
    switch (R.ah) {
    case 0x00: { u8 m; u32 t = bios_ticks(&m); R.cx = (u16)(t >> 16); R.dx = (u16)t; R.al = m; }
        break;
    case 0x01: ww(BDA, 0x6E, R.cx); ww(BDA, 0x6C, R.dx); break;
    case 0x02: R.ch = 0x12; R.cl = 0; R.dh = 0; R.cf = 0; break;
    case 0x04: R.ch = 0x19; R.cl = 0x91; R.dh = 0x06; R.dl = 0x01; R.cf = 0; break;
    default: fprintf(stderr, "int1a unhandled AH=%02X\n", R.ah);
    }
}

/* ---------------- INT 33h mouse driver ---------------- */
int mouse_x = 320, mouse_y = 100, mouse_buttons;
int mouse_minx = 0, mouse_maxx = 639, mouse_miny = 0, mouse_maxy = 199;
static int mouse_mx = 8, mouse_my = 16;     /* mickeys per 8 pixels */
static int mouse_press_cnt[3], mouse_rel_cnt[3];
static int mouse_press_x[3], mouse_press_y[3];
static int mouse_visible;

static void mouse_clamp(void) {
    if (mouse_x < mouse_minx) mouse_x = mouse_minx;
    if (mouse_x > mouse_maxx) mouse_x = mouse_maxx;
    if (mouse_y < mouse_miny) mouse_y = mouse_miny;
    if (mouse_y > mouse_maxy) mouse_y = mouse_maxy;
}

/* Host motion in game pixels (320x200).  The game reads the driver position
 * divided by 8, so feed it driver units that map 1:1 onto game pixels. */
static int pend_dx, pend_dy;     /* motion not yet seen by the game */

int bios_state_blocks(StateBlock *b) {
    int n = 0;
    STATE_BLOCK(b, n, mouse_x); STATE_BLOCK(b, n, mouse_y); STATE_BLOCK(b, n, mouse_buttons);
    STATE_BLOCK(b, n, mouse_minx); STATE_BLOCK(b, n, mouse_maxx);
    STATE_BLOCK(b, n, mouse_miny); STATE_BLOCK(b, n, mouse_maxy);
    STATE_BLOCK(b, n, mouse_mx); STATE_BLOCK(b, n, mouse_my);
    STATE_BLOCK(b, n, mouse_press_cnt); STATE_BLOCK(b, n, mouse_rel_cnt);
    STATE_BLOCK(b, n, mouse_press_x); STATE_BLOCK(b, n, mouse_press_y);
    STATE_BLOCK(b, n, mouse_visible); STATE_BLOCK(b, n, pend_dx); STATE_BLOCK(b, n, pend_dy);
    STATE_BLOCK(b, n, kb_ring); STATE_BLOCK(b, n, kb_head); STATE_BLOCK(b, n, kb_tail);
    return n;
}
void mouse_move_rel(int dx, int dy) { pend_dx += dx; pend_dy += dy; }
/* a new seat's input starts empty (runtime/seat.c) */
void bios_input_clear(void) {
    kb_init();
    pend_dx = pend_dy = 0;
    mouse_buttons = 0;
    for (int i = 0; i < 3; i++) mouse_press_cnt[i] = mouse_rel_cnt[i] = 0;
}
/* motion the game has not read yet, in game pixels */
void mouse_pending(int *dx, int *dy) { *dx = pend_dx; *dy = pend_dy; }
int mouse_pending_motion(void) { return pend_dx || pend_dy; }
static int take(int *p) {
    int v = *p;
    if (v > 150) v = 150;
    if (v < -150) v = -150;
    *p -= v;
    return v;
}
static void mouse_apply_pending(void) {
    mouse_x += take(&pend_dx) * 8; mouse_y += take(&pend_dy) * 8;
    mouse_clamp();
}
void mouse_set_buttons(int b) {
    for (int i = 0; i < 3; i++) {
        int was = mouse_buttons & (1 << i), now = b & (1 << i);
        if (now && !was) { mouse_press_cnt[i]++; mouse_press_x[i] = mouse_x; mouse_press_y[i] = mouse_y; }
        if (!now && was) mouse_rel_cnt[i]++;
    }
    mouse_buttons = b;
}

/* function 0: reset; 1 if a driver is installed, *buttons */
int mouse_reset(u16 *buttons) {
    mouse_visible = 0;
    mouse_minx = 0; mouse_maxx = 639; mouse_miny = 0; mouse_maxy = 199;
    mouse_x = 320; mouse_y = 100;
    *buttons = 2;
    return 1;
}
/* function 3: buttons, *x, *y (driver units) */
u16 mouse_read(u16 *x, u16 *y) {
    mouse_apply_pending();
    *x = (u16)mouse_x; *y = (u16)mouse_y;
    time_advance(50);
    return (u16)mouse_buttons;
}
/* functions 4, 7, 8, 0Fh */
void mouse_set_pos(u16 x, u16 y) { mouse_x = (s16)x; mouse_y = (s16)y; mouse_clamp(); }
void mouse_range_x(u16 lo, u16 hi) {
    mouse_minx = (s16)lo; mouse_maxx = (s16)hi;
    if (mouse_minx > mouse_maxx) { int t = mouse_minx; mouse_minx = mouse_maxx; mouse_maxx = t; }
    mouse_clamp();
}
void mouse_range_y(u16 lo, u16 hi) {
    mouse_miny = (s16)lo; mouse_maxy = (s16)hi;
    if (mouse_miny > mouse_maxy) { int t = mouse_miny; mouse_miny = mouse_maxy; mouse_maxy = t; }
    mouse_clamp();
}
void mouse_set_ratio(u16 mx, u16 my) { mouse_mx = mx; mouse_my = my; }

void int33(void) {
    switch (R.ax) {
    case 0x00: { u16 b; mouse_reset(&b); R.ax = 0xFFFF; R.bx = b; } break;
    case 0x01: mouse_visible++; break;
    case 0x02: mouse_visible--; break;
    case 0x03: { u16 x, y; R.bx = mouse_read(&x, &y); R.cx = x; R.dx = y; }
        if (bios_trace) fprintf(stderr, "  int33/3 -> buttons=%d x=%d y=%d\n", R.bx, R.cx, R.dx);
        break;
    case 0x04: mouse_set_pos(R.cx, R.dx); break;
    case 0x05: { int b = R.bx & 3; R.ax = (u16)mouse_buttons; R.bx = (u16)mouse_press_cnt[b];
        R.cx = (u16)mouse_press_x[b]; R.dx = (u16)mouse_press_y[b]; mouse_press_cnt[b] = 0; } break;
    case 0x06: { int b = R.bx & 3; R.ax = (u16)mouse_buttons; R.bx = (u16)mouse_rel_cnt[b];
        R.cx = (u16)mouse_x; R.dx = (u16)mouse_y; mouse_rel_cnt[b] = 0; } break;
    case 0x07: mouse_range_x(R.cx, R.dx); break;
    case 0x08: mouse_range_y(R.cx, R.dx); break;
    case 0x0B: R.cx = 0; R.dx = 0; break;
    case 0x0F: mouse_set_ratio(R.cx, R.dx); break;
    case 0x1A: case 0x1D: break;
    case 0x21: R.ax = 0xFFFF; R.bx = 2; break;
    case 0x24: R.bx = 0x0626; R.ch = 4; R.cl = 0; break;
    default: fprintf(stderr, "int33 unhandled AX=%04X\n", R.ax);
    }
    (void)mouse_mx; (void)mouse_my;
}

/* ---------------- hardware interrupt handlers ---------------- */
static void int08(void) {
    u32 t = rw(BDA, 0x6C) | ((u32)rw(BDA, 0x6E) << 16);
    if (++t >= 0x1800B0) { t = 0; wb(BDA, 0x70, 1); }
    ww(BDA, 0x6C, (u16)t); ww(BDA, 0x6E, t >> 16);
    do_int(0x1C);
    port_out8(0x20, 0x20);
}
static void int09(void) {
    port_in8(0x60);
    port_out8(0x20, 0x20);
}

/* Services that report status in CF/ZF; everything else (hardware IRQs in
 * particular) must hand the interrupted code its own flags back. */
static int returns_flags(u8 n) {
    switch (n) {
    case 0x13: case 0x15: case 0x16: case 0x1A: case 0x21: case 0x2F: case 0x10: case 0x33:
        return 1;
    }
    return 0;
}

void bios_stub_iret(u8 n) {
    bios_dispatch(n);
    u8 cf = R.cf, zf = R.zf;
    u16 ip = pop16(), cs = pop16(), fl = pop16();
    scrub_popped(6);
    set_flags(fl);
    if (returns_flags(n)) { R.cf = cf; R.zf = zf; }
    R.ip = ip; R.cs = cs;
}

void bios_dispatch(u8 n) {
    clock_io(0x10000u | n);
    if (bios_trace && n != 0x08 && n != 0x1C) fprintf(stderr, "bios int %02X AX=%04X BX=%04X CX=%04X DX=%04X T=%llu frame=%llu\n", n, R.ax, R.bx, R.cx, R.dx, (unsigned long long)T, (unsigned long long)frame_no());
    switch (n) {
    case 0x08: int08(); break;
    case 0x09: int09(); break;
    case 0x10: int10(); break;
    case 0x11: R.ax = rw(BDA, 0x10); break;
    case 0x12: R.ax = rw(BDA, 0x13); break;
    case 0x16: int16(); break;
    case 0x1A: int1a(); break;
    case 0x1C: break;
    case 0x21: int21(); break;
    case 0x33: int33(); break;
    case 0x00: cpu_halt("divide error (INT 0) with no handler");
    case 0x2F: if (R.ax == 0x1600) R.al = 0; break;   /* no Windows */
    case 0x28: break;
    default:
        if (n >= 0x70 && n < 0x78) { port_out8(0xA0, 0x20); port_out8(0x20, 0x20); break; }
        fprintf(stderr, "unhandled interrupt %02X AX=%04X\n", n, R.ax);
    }
}
