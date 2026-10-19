/* The platform services for the game code: see platform.h. */
#include <string.h>
#include "runtime.h"
#include "platform.h"

u16 bios_key_read(void);
int bios_key_peek(u16 *key);
u8 bios_video_mode(void);
void bios_video_set_mode(u8 al);
void bios_dac_set(u16 first, u16 n, const u8 *rgb);
u32 bios_ticks(u8 *midnight);
int mouse_reset(u16 *buttons);
u16 mouse_read(u16 *x, u16 *y);
void mouse_set_pos(u16 x, u16 y);
void mouse_range_x(u16 lo, u16 hi);
void mouse_range_y(u16 lo, u16 hi);
void mouse_set_ratio(u16 mx, u16 my);
int dos_setblock(u16 seg, u16 paras, u16 *max);
int dos_create(const u8 *name);
int dos_open(const u8 *name, u8 mode);
int dos_close(u16 h);
int dos_read(u16 h, u8 *buf, u16 n);
int dos_write(u16 h, const u8 *buf, u16 n);
int dos_unlink(const u8 *name);
s32 dos_lseek(u16 h, s32 pos, u8 whence, u16 *e);
int dos_ioctl_info(u16 h);
void dos_exit(u8 code);
int dos_ioctl(u16 h, u8 func, u16 *dx);
int dos_chmod(const u8 *name, u8 func, u16 *attr);
void dos_set_dta(u8 *dta);
int dos_findfirst(const u8 *pattern);
int dos_findnext(void);
void dos_setvect(u8 n, u16 seg, u16 off);
u32 dos_getvect(u8 n);
u16 dos_version(void);
u8 vga_status_read(void);

/* DOS through the interrupt (should int 21h ever be hooked): AX or -error;
 * a host pointer into the emulated memory as seg:off for it (the game never
 * hooks int 21h; a pointer outside M cannot be handed to a hook) */
static int dos_int(void) { do_int(0x21); return R.cf ? -(int)R.ax : R.ax; }
static u32 far_of(const u8 *p) {
    size_t a = (size_t)(p - M);
    if (p < M || a >= MEM_SIZE) cpu_halt("platform: a hooked DOS service got a buffer outside the emulated memory");
    return ((u32)(a >> 4) << 16) | (a & 15);
}
#define SEG_OF(p) ((u16)(far_of(p) >> 16))
#define OFF_OF(p) ((u16)far_of(p))

/* the vector still points at its BIOS stub: serve the call directly (as
 * do_int would, through bios_dispatch, which counts the I/O first) */
static int direct(u8 n) {
    if (!bios_is_stub(rw(0, n * 4 + 2), rw(0, n * 4))) return 0;
    clock_io(0x10000u | n);
    return 1;
}

u8 plat_video_mode(void) {
    if (direct(0x10)) return bios_video_mode();
    R.ah = 0x0F; do_int(0x10); return R.al;
}
void plat_video_set_mode(u8 mode) {
    if (direct(0x10)) { bios_video_set_mode(mode); return; }
    R.ah = 0; R.al = mode; do_int(0x10);
}
void plat_palette_set(u16 first, u16 n, const u8 *rgb) {
    if (direct(0x10)) { bios_dac_set(first, n, rgb); return; }
    R.ax = 0x1012; R.bx = first; R.cx = n; R.es = SEG_OF(rgb); R.dx = OFF_OF(rgb); do_int(0x10);
}
u8 plat_vga_status(void) { return port_in8(0x3DA); }

int plat_screen(u8 *indices, u8 palette[256][3]) {
    extern int vga_planar;
    extern void vga_render_planar(u8 *);
    if (vga_planar) vga_render_planar(indices);                 /* INTRO.EXE: mode 0Dh */
    else if (vga_mode == 0x13) memcpy(indices, M + 0xA0000, 320 * 200);
    else return 0;
    memcpy(palette, vga_dac, sizeof vga_dac);
    return 1;
}

int plat_key_peek(u16 *key) {
    if (direct(0x16)) return bios_key_peek(key);
    R.ah = 1; do_int(0x16);
    if (R.zf) return 0;
    *key = R.ax;
    return 1;
}
u16 plat_key_read(void) {
    if (direct(0x16)) return bios_key_read();
    R.ah = 0; do_int(0x16); return R.ax;
}

int plat_mouse_reset(u16 *buttons) {
    if (direct(0x33)) return mouse_reset(buttons);
    R.ax = 0; do_int(0x33); *buttons = R.bx; return R.ax == 0xFFFF;
}
u16 plat_mouse_read(u16 *x, u16 *y) {
    if (direct(0x33)) return mouse_read(x, y);
    R.ax = 3; do_int(0x33); *x = R.cx; *y = R.dx; return R.bx;
}
void plat_mouse_set_pos(u16 x, u16 y) {
    if (direct(0x33)) { mouse_set_pos(x, y); return; }
    R.ax = 4; R.cx = x; R.dx = y; do_int(0x33);
}
void plat_mouse_range_x(u16 lo, u16 hi) {
    if (direct(0x33)) { mouse_range_x(lo, hi); return; }
    R.ax = 7; R.cx = lo; R.dx = hi; do_int(0x33);
}
void plat_mouse_range_y(u16 lo, u16 hi) {
    if (direct(0x33)) { mouse_range_y(lo, hi); return; }
    R.ax = 8; R.cx = lo; R.dx = hi; do_int(0x33);
}
void plat_mouse_ratio(u16 mx, u16 my) {
    if (direct(0x33)) { mouse_set_ratio(mx, my); return; }
    R.ax = 0xF; R.cx = mx; R.dx = my; do_int(0x33);
}

u32 plat_ticks(u8 *midnight) {
    if (direct(0x1A)) return bios_ticks(midnight);
    R.ah = 0; do_int(0x1A); *midnight = R.al; return ((u32)R.cx << 16) | R.dx;
}

int plat_setblock(u16 seg, u16 paras, u16 *max) {
    if (direct(0x21)) return dos_setblock(seg, paras, max);
    R.ah = 0x4A; R.es = seg; R.bx = paras; do_int(0x21);
    if (!R.cf) return 0;
    *max = R.bx;
    return R.ax;
}


int plat_create(const u8 *name) {
    if (direct(0x21)) return dos_create(name);
    R.ah = 0x3C; R.cx = 0; R.ds = SEG_OF(name); R.dx = OFF_OF(name); return dos_int();
}
int plat_open(const u8 *name, u8 mode) {
    if (direct(0x21)) return dos_open(name, mode);
    R.ah = 0x3D; R.al = mode; R.ds = SEG_OF(name); R.dx = OFF_OF(name); return dos_int();
}
int plat_close(u16 h) {
    if (direct(0x21)) return dos_close(h);
    R.ah = 0x3E; R.bx = h; return dos_int();
}
int plat_read(u16 h, u8 *buf, u16 n) {
    if (direct(0x21)) return dos_read(h, buf, n);
    R.ah = 0x3F; R.bx = h; R.cx = n; R.ds = SEG_OF(buf); R.dx = OFF_OF(buf); return dos_int();
}
int plat_write(u16 h, const u8 *buf, u16 n) {
    if (direct(0x21)) return dos_write(h, buf, n);
    R.ah = 0x40; R.bx = h; R.cx = n; R.ds = SEG_OF(buf); R.dx = OFF_OF(buf); return dos_int();
}
int plat_unlink(const u8 *name) {
    if (direct(0x21)) return dos_unlink(name);
    R.ah = 0x41; R.ds = SEG_OF(name); R.dx = OFF_OF(name); return dos_int();
}
s32 plat_lseek(u16 h, s32 pos, u8 whence, u16 *err) {
    if (direct(0x21)) return dos_lseek(h, pos, whence, err);
    R.ah = 0x42; R.al = whence; R.bx = h; R.cx = (u16)((u32)pos >> 16); R.dx = (u16)pos;
    do_int(0x21);
    if (R.cf) { *err = R.ax; return -1; }
    return (s32)(((u32)R.dx << 16) | R.ax);
}
int plat_ioctl_info(u16 h) {
    if (direct(0x21)) return dos_ioctl_info(h);
    R.ax = 0x4400; R.bx = h; do_int(0x21); return R.cf ? -(int)R.ax : R.dx;
}
void plat_exit(u8 code) {
    if (direct(0x21)) dos_exit(code);
    R.ah = 0x4C; R.al = code; do_int(0x21);
}
int plat_ioctl(u16 h, u8 func, u16 *dx) {
    if (direct(0x21)) return dos_ioctl(h, func, dx);
    R.ah = 0x44; R.al = func; R.bx = h; R.dx = *dx; do_int(0x21);
    if (R.cf) return -(int)R.ax;
    *dx = R.dx;
    return R.ax;
}
int plat_chmod(const u8 *name, u8 func, u16 *attr) {
    if (direct(0x21)) return dos_chmod(name, func, attr);
    R.ah = 0x43; R.al = func; R.cx = *attr; R.ds = SEG_OF(name); R.dx = OFF_OF(name); do_int(0x21);
    if (R.cf) return -(int)R.ax;
    *attr = R.cx;
    return 0;
}
void plat_set_dta(u8 *dta) {
    if (direct(0x21)) { dos_set_dta(dta); return; }
    R.ah = 0x1A; R.ds = SEG_OF(dta); R.dx = OFF_OF(dta); do_int(0x21);
}
int plat_findfirst(const u8 *pattern) {
    if (direct(0x21)) return dos_findfirst(pattern);
    R.ah = 0x4E; R.ds = SEG_OF(pattern); R.dx = OFF_OF(pattern); return dos_int() < 0 ? -(int)R.ax : 0;
}
int plat_findnext(void) {
    if (direct(0x21)) return dos_findnext();
    R.ah = 0x4F; return dos_int() < 0 ? -(int)R.ax : 0;
}
u16 plat_dos_version(void) {
    if (direct(0x21)) return dos_version();
    R.ah = 0x30; do_int(0x21); return R.ax;
}
u32 plat_getvect(u8 n) {
    if (direct(0x21)) return dos_getvect(n);
    R.ax = (u16)(0x3500 | n); do_int(0x21); return ((u32)R.es << 16) | R.bx;
}
void plat_setvect(u8 n, u32 handler) {
    if (direct(0x21)) { dos_setvect(n, (u16)(handler >> 16), (u16)handler); return; }
    R.ah = 0x25; R.al = n; R.ds = (u16)(handler >> 16); R.dx = (u16)handler; do_int(0x21);
}
