/* The platform as the game code sees it (ROADMAP Phase 3, stage 2): video,
 * keyboard, mouse, clock and DOS memory services as C functions instead of
 * software interrupts with registers.
 *
 * Each call is one I/O operation for the logical clock and costs the same
 * virtual time as the BIOS/DOS service it replaces (bios.c, dos.c, hw.c),
 * so classic runs stay bit-identical.  The game itself never hooks these
 * vectors (only int 00h, its divide-error handler, and int 08h, the sound
 * driver's timer); should one be hooked anyway, the call goes through the
 * interrupt as before.  Names and buffers are host pointers (the caller
 * resolves a far pointer with HPTR, src/dec.h). */
#ifndef PLATFORM_H
#define PLATFORM_H
#include "cpu.h"

/* video */
u8   plat_video_mode(void);                      /* int 10h AH=0Fh */
void plat_video_set_mode(u8 mode);               /* int 10h AH=00h */
void plat_palette_set(u16 first, u16 n, const u8 *rgb);   /* int 10h AX=1012h: n (r, g, b) */
u8   plat_vga_status(void);                      /* port 3DAh: bit 3 vertical retrace */
/* the screen for the host: 320x200 colour indices and the palette (6 bits
 * per component); 0 if the video mode shows nothing (text mode) */
int  plat_screen(u8 *indices, u8 palette[256][3]);
/* keyboard */
int  plat_key_peek(u16 *key);                    /* int 16h AH=01h: 1 and the key if one waits */
u16  plat_key_read(void);                        /* int 16h AH=00h: scan << 8 | ASCII, waits */
/* mouse (driver units, 8 per game pixel) */
int  plat_mouse_reset(u16 *buttons);             /* int 33h 0: 1 if present */
u16  plat_mouse_read(u16 *x, u16 *y);            /* int 33h 3: buttons */
void plat_mouse_set_pos(u16 x, u16 y);           /* int 33h 4 */
void plat_mouse_range_x(u16 lo, u16 hi);         /* int 33h 7 */
void plat_mouse_range_y(u16 lo, u16 hi);         /* int 33h 8 */
void plat_mouse_ratio(u16 mx, u16 my);           /* int 33h 0Fh: mickeys per 8 pixels */
/* clock */
u32  plat_ticks(u8 *midnight);                   /* int 1Ah AH=00h: 18.2 Hz ticks since midnight */
/* DOS memory */
int  plat_setblock(u16 seg, u16 paras, u16 *max);   /* int 21h AH=4Ah: 0 or the DOS error */
/* DOS files: a handle or count >= 0, or minus the DOS error code */
int  plat_create(const u8 *name);               /* int 21h AH=3Ch */
int  plat_open(const u8 *name, u8 mode);         /* int 21h AH=3Dh: 0 read, 1 write, 2 both */
int  plat_close(u16 h);                          /* int 21h AH=3Eh */
int  plat_read(u16 h, u8 *buf, u16 n);           /* int 21h AH=3Fh */
int  plat_write(u16 h, const u8 *buf, u16 n);    /* int 21h AH=40h (n = 0: truncate) */
int  plat_unlink(const u8 *name);                /* int 21h AH=41h */
s32  plat_lseek(u16 h, s32 pos, u8 whence, u16 *err);   /* int 21h AH=42h: position, or -1 and *err */
int  plat_ioctl_info(u16 h);                     /* int 21h AX=4400h: device information */
int  plat_ioctl(u16 h, u8 func, u16 *dx);        /* int 21h AH=44h: AX afterwards, or -error */
int  plat_chmod(const u8 *name, u8 func, u16 *attr);   /* int 21h AH=43h: 0 get (*attr), 1 set */
void plat_set_dta(u8 *dta);                      /* int 21h AH=1Ah */
int  plat_findfirst(const u8 *pattern);          /* int 21h AH=4Eh: into the DTA */
int  plat_findnext(void);                        /* int 21h AH=4Fh */
void plat_setvect(u8 n, u32 handler);            /* int 21h AH=25h */
u32  plat_getvect(u8 n);                         /* int 21h AH=35h: seg << 16 | off */
u16  plat_dos_version(void);                     /* int 21h AH=30h: AL major, AH minor */
void plat_exit(u8 code);                         /* int 21h AH=4Ch */
#endif
