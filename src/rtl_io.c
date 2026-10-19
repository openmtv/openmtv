/* Borland file I/O and program exit in C, and the two resource-manager
 * functions that were still machine-level (see rtl.h): open, read, write,
 * lseek, close with their DOS layer (__open, __read, __write, __close,
 * ioctl, _chmod, open's create and truncate helpers), findfirst/findnext,
 * exit with the atexit chain and __exit/__restorezero, and res_open_all /
 * res_create of the .RSC manager (module 385D; its other functions are in
 * resfile.c).  DOS services go through the platform layer
 * (runtime/platform.h); names and buffers are host pointers (a glue
 * wrapper resolves the original's far pointers with HPTR).  Buffers the
 * original keeps in its stack frame (the ffblk/DTA, write's text-mode
 * buffer, res_create's header words, read's CR lookahead) are C locals. */
#include "game.h"

/* a DOS call through the platform layer (runtime/platform.h), leaving CF
 * and AX as int 21h does: r >= 0 the result in AX (keep: AX unchanged on
 * success, as DOS leaves it), r < 0 the error code */
static void dos_result(int r, int keep) {
    if (r < 0) { R.cf = 1; R.ax = (u16)-r; }
    else { R.cf = 0; if (!keep) R.ax = (u16)r; }
}

/* run-time library variables in DGROUP */
#define rtl_openfd(fd)    G16((u16)(0x1934 + 2 * (fd)))   /* _openfd[]: O_* flags per handle, FFFF = free */
#define rtl_fmode         G16(0x195C)   /* _fmode: O_TEXT / O_BINARY default */
#define rtl_notumask      G16(0x195E)   /* _notUmask: permission bits open may grant */
#define rtl_atexit_count  G16(0x1822)   /* _atexitcnt */
#define RTL_ATEXIT_TBL    0xA6A2        /* _atexittbl[]: far function pointers */

/* open flags (Turbo C fcntl.h); after open, 0x0200 means "end of file seen"
 * (read's Ctrl-Z) and 0x1000 "written to" */
#define O_WRONLY_  0x0002
#define O_RDWR_    0x0004
#define O_CREAT_   0x0100
#define O_TRUNC_   0x0200
#define O_EOF_     0x0200
#define O_EXCL_    0x0400
#define O_APPEND_  0x0800
#define O_CHANGED_ 0x1000
#define O_DEVICE_  0x2000
#define O_BINARY_  0x8000

/* glue for __IOerror (38BC:000C, rtl_ioerror in rtl_heap.c): pascal, pops its
 * argument.  No address comment of its own here: that would make it a
 * logical-clock entry (tools/clock_table.py) */
void f_38BC_000C(void) {
    rtl_ioerror(WARG16(0));
    R.sp += 6;
}

/* 395F:0004  lseek(fd, offset, whence): int 21h AH=42h; the new position,
 * or -1; clears the end-of-file mark */
u32 rtl_lseek(u16 fd, u16 off_lo, u16 off_hi, u16 whence) {
    RTL_ENTRY();
    rtl_openfd(fd) &= (u16)~O_EOF_;
    R.ah = 0x42; R.al = (u8)whence;
    R.bx = fd; R.cx = off_hi; R.dx = off_lo;
    T += 11;
    {
        u16 e;
        s32 pos = plat_lseek(fd, (s32)MK32(off_hi, off_lo), (u8)whence, &e);
        if (pos < 0) { R.cf = 1; R.ax = e; }
        else { R.cf = 0; R.ax = (u16)pos; R.dx = (u16)((u32)pos >> 16); }
    }
    T += 1;
    if (R.cf) {
        T += 2;
        rtl_ioerror(R.ax);
        R.dx = (R.ax & 0x8000) ? 0xFFFF : 0;     /* cwd */
        T += 2;
    } else {
        T += 1;
    }
    T += 2;
    return MK32(R.dx, R.ax);
}
void f_395F_0004(void) {
    rtl_lseek(WARG16(0), WARG16(1), WARG16(2), WARG16(3));
    R.sp += 4;
}

/* 3942:0002  __read(fd, buf, n): int 21h AH=3Fh, no translation; the
 * bytes read, or -1 */
static u32 rtl_dos_read(u16 fd, u8 *buf, u16 n) {
    RTL_ENTRY();
    R.ah = 0x3F; R.bx = fd; R.cx = n;
    T += 8;
    dos_result(plat_read(fd, buf, n), 0);
    T += 2;
    if (R.cf) {
        T += 2;
        rtl_ioerror(R.ax);
        T += 1;
    } else {
        T += 1;
    }
    T += 2;
    return MK32(R.dx, R.ax);
}
void f_3942_0002(void) {
    rtl_dos_read(WARG16(0), HPTR(MK32(WARG16(2), WARG16(1))), WARG16(3));
    R.sp += 4;
}

/* 3935:000D  read(fd, buf, n): __read; in text mode CR LF becomes LF (a CR
 * at the end of the buffer reads one more byte) and Ctrl-Z ends the file:
 * the end-of-file mark is set and the file positioned before the Ctrl-Z --
 * by lseek(fd, -(bytes from the Ctrl-Z on), SEEK_END), i.e. relative to the
 * end of the file, not to the position just read.  A buffer that comes out
 * empty is read again.  The bytes delivered, 0 at end of file, or -1 */
u32 rtl_read(u16 fd, u8 *buf, u16 n) {
    RTL_ENTRY();
    u16 r, cx;
    u8 *si, *di, c, next = 0;                /* next: the byte after a final CR (in the original's frame) */
    T += 9;
    if ((u16)(n + 1) < 2) goto zero;
    R.bx = (u16)(fd << 1);
    T += 4;
    if (!(rtl_openfd(fd) & O_EOF_)) goto again;
zero:
    R.ax = 0;
    T += 2;
    goto out;
again:
    POLL();
    T += 5;
    r = LO16(rtl_dos_read(fd, buf, n));
    T += 6;
    if ((u16)(r + 1) < 2) goto as_is;
    R.bx = (u16)(fd << 1);
    T += 4;
    if (!(rtl_openfd(fd) & O_BINARY_)) goto text;
as_is:
    R.ax = r;
    T += 2;
    goto out;
text:
    cx = r; si = buf; di = buf;
    R.df = 0;
    T += 5;
    for (;;) {
        POLL();
        T += 1;
        c = *si++;                           /* lodsb (es:) */
        T += 2;
        if (c == 0x1A) goto ctrl_z;
        T += 2;
        if (c == 0x0D) {
            T += 1;
            if (--cx) continue;              /* a CR is dropped */
            T += 9;                          /* the last byte was a CR: read the next one */
            rtl_dos_read(fd, &next, 1);
            R.df = 0;
            c = next;
            T += 6;
            *di++ = c;
            break;
        }
        T += 1;
        *di++ = c;                           /* stosb */
        T += 1;
        if (--cx) continue;
        T += 1;
        break;
    }
    R.cx = cx;
    T += 2;
    if (di == buf) {                        /* nothing delivered: read again */
        T += 1;
        goto again;
    }
    T += 1;
    goto count;
ctrl_z: {
        u16 back = (u16)-cx, hi = cx ? 0xFFFF : 0;   /* neg cx; sbb ax, ax */
        T += 9;
        rtl_lseek(fd, back, hi, 2);
        rtl_openfd(fd) |= O_EOF_;
        T += 5;
    }
count:
    R.ax = (u16)(di - buf);
    T += 3;
out:
    T += 5;
    return MK32(R.dx, R.ax);
}
void f_3935_000D(void) {
    rtl_read(WARG16(0), HPTR(MK32(WARG16(2), WARG16(1))), WARG16(3));
    R.sp += 4;
}

/* 395A:0008  __write(fd, buf, n): O_APPEND seeks to the end first; int 21h
 * AH=40h; marks the handle written to; the bytes written, or -1 */
static u32 rtl_dos_write(u16 fd, const u8 *buf, u16 n) {
    RTL_ENTRY();
    R.bx = (u16)(fd << 1);
    T += 6;
    if (rtl_openfd(fd) & O_APPEND_) {
        T += 7;
        rtl_lseek(fd, 0, 0, 2);
        T += 1;
    }
    R.ah = 0x40; R.bx = fd; R.cx = n;
    T += 6;
    dos_result(plat_write(fd, buf, n), 0);
    T += 2;
    if (R.cf) {
        T += 2;
        rtl_ioerror(R.ax);
        T += 1;
    } else {
        R.bx = (u16)(fd << 1);
        rtl_openfd(fd) |= O_CHANGED_;
        T += 6;
    }
    T += 2;
    return MK32(R.dx, R.ax);
}
void f_395A_0008(void) {
    rtl_dos_write(WARG16(0), HPTR(MK32(WARG16(2), WARG16(1))), WARG16(3));
    R.sp += 4;
}

/* 3944:0002  write(fd, buf, n): a binary handle goes straight to __write;
 * in text mode LF becomes CR LF through a 128-byte buffer in the stack
 * frame, flushed when full and at the end.  n, or after a short write the
 * bytes of buf consumed minus those not written.  ORIGINAL BUG (Turbo C):
 * the error test after a short write is "or di, di; jae", which is always
 * taken, so a failed __write (-1) is folded into that count instead of
 * returning -1 */
u32 rtl_write(u16 fd, const u8 *src, u16 n) {
    RTL_ENTRY();
    u8 buf[130], *dst;                       /* the original's 128-byte buffer (in its frame) */
    u16 left, len, r;
    T += 9;
    if ((u16)(n + 1) < 2) {
        R.ax = 0;
        T += 2;
        goto out;
    }
    R.bx = (u16)(fd << 1);
    T += 4;
    if (rtl_openfd(fd) & O_BINARY_) {
        T += 5;
        rtl_dos_write(fd, src, n);
        T += 2;
        goto out;
    }
    rtl_openfd(fd) &= (u16)~O_EOF_;
    left = n; dst = buf;
    T += 14;
    goto check;
next: {
        POLL();
        left--;
        u8 c = *src++;
        T += 7;
        if (c == '\n') {
            *dst++ = '\r';
            T += 3;
        }
        *dst++ = c;
        u16 ax = (u16)(dst - buf), dx = 0;   /* 32-bit dst - buf */
        R.ax = ax; R.dx = dx;
        T += 12;
        if (dx & 0x8000) goto check;
        T += 1;
        if (dx == 0) {
            T += 2;
            if (ax < 0x80) goto check;
        }
    }
    len = (u16)(dst - buf);                  /* buffer full: flush it */
    T += 13;
    r = LO16(rtl_dos_write(fd, buf, len));
    T += 5;
    if (r == len) {
        dst = buf;
        T += 5;
        goto check;
    }
    T += 2;
    R.ax = (u16)(n - left + r - len);
    T += 4 + 1;
    goto out;
check:
    T += 2;
    if (left != 0) {
        T += 1;
        goto next;
    }
    len = (u16)(dst - buf);
    R.ax = len; R.dx = 0;
    T += 10;
    if (len != 0) {
        T += 6;
        r = LO16(rtl_dos_write(fd, buf, len));
        T += 5;
        if (r != len) {
            T += 2;
            R.ax = (u16)(n + r - len);
            T += 3 + 1;
            goto out;
        }
    }
    R.ax = n;
    T += 2;
out:
    T += 5;
    return MK32(R.dx, R.ax);
}
void f_3944_0002(void) {
    rtl_write(WARG16(0), HPTR(MK32(WARG16(2), WARG16(1))), WARG16(3));
    R.sp += 4;
}

/* 3933:0007  __close(fd): int 21h AH=3Eh, the handle freed in _openfd; 0,
 * or -1 */
static u32 rtl_dos_close(u16 fd) {
    RTL_ENTRY();
    R.ah = 0x3E; R.bx = fd;
    T += 7;
    dos_result(plat_close(fd), 1);
    T += 1;
    if (R.cf) {
        T += 2;
        rtl_ioerror(R.ax);
        T += 1;
    } else {
        R.bx = (u16)(fd << 1);
        rtl_openfd(fd) = 0xFFFF;
        R.ax = 0;
        T += 4;
    }
    T += 3;
    return MK32(R.dx, R.ax);
}
void f_3933_0007(void) {
    rtl_dos_close(WARG16(0));
    R.sp += 4;
}

/* 3930:0006  close(fd): handles 0..19 are freed in _openfd and closed
 * (__close), others fail with EBADF; 0, or -1 */
u32 rtl_close(u16 fd) {
    RTL_ENTRY();
    T += 6;
    if ((i16)fd >= 0 && (T += 2, (i16)fd < 0x14)) {
        R.bx = (u16)(fd << 1);
        rtl_openfd(fd) = 0xFFFF;
        T += 5;
        rtl_dos_close(fd);
        R.cx = fd;                           /* pop cx */
        T += 2;
    } else {
        R.ax = 6;
        T += 3;
        rtl_ioerror(6);
        T += 1;
    }
    T += 3;
    return MK32(R.dx, R.ax);
}
void f_3930_0006(void) {
    rtl_close(WARG16(0));
    R.sp += 4;
}

/* 390E:000C  open's create helper (near, pops its arguments): int 21h
 * AH=3Ch with attributes attr; the handle, or -1 */
static u32 rtl_dos_creat(const u8 *path, u16 attr) {
    RTL_ENTRY();
    R.cx = attr; R.ah = 0x3C;
    T += 7;
    dos_result(plat_create(path), 0);
    T += 2;
    if (R.cf) {
        T += 2;
        rtl_ioerror(R.ax);
        T += 1;
    } else {
        T += 1;
    }
    T += 2;
    return MK32(R.dx, R.ax);
}
void f_390E_000C(void) {
    rtl_dos_creat(HPTR(MK32(rw(R.ss, (u16)(R.sp + 6)), rw(R.ss, (u16)(R.sp + 4)))), rw(R.ss, (u16)(R.sp + 2)));
    R.sp += 2 + 6;
}

/* 390E:002B  open's truncate helper (near, pops its argument): int 21h
 * AH=40h with 0 bytes cuts the file at the current position */
static u32 rtl_dos_truncate(u16 fd) {
    RTL_ENTRY();
    R.bx = fd; R.cx = 0; R.dx = 0; R.ah = 0x40;
    T += 7;
    dos_result(plat_write(fd, HPTR(MK32(R.ds, 0)), 0), 0);
    T += 1 + 2;
    return MK32(R.dx, R.ax);
}
void f_390E_002B(void) {
    rtl_dos_truncate(rw(R.ss, (u16)(R.sp + 2)));
    R.sp += 2 + 2;
}

/* 3928:000C  __open(path, oflag): int 21h AH=3Dh, access 0 (read), 1
 * (O_WRONLY) or 2 (O_RDWR) with the sharing bits 4..7 of oflag;
 * _openfd[handle] = oflag without the create bits, | O_BINARY; the handle,
 * or -1 */
static u32 rtl_dos_open(const u8 *path, u16 oflag) {
    RTL_ENTRY();
    u8 mode = 1;
    R.cx = oflag;
    T += 7;
    if (!(oflag & O_WRONLY_)) {
        mode = 2;
        T += 3;
        if (!(oflag & O_RDWR_)) {
            mode = 0;
            T += 1;
        }
    }
    R.cl = (u8)(oflag & 0xF0);
    R.al = (u8)(mode | R.cl); R.ah = 0x3D;
    T += 7;
    dos_result(plat_open(path, R.al), 0);
    T += 2;
    if (R.cf) {
        T += 2;
        rtl_ioerror(R.ax);
        T += 1;
    } else {
        u16 h = R.ax;
        R.bx = (u16)(h << 1);
        rtl_openfd(h) = (u16)((oflag & 0xF8FF) | O_BINARY_);
        R.ax = h;
        T += 9;
    }
    T += 3;
    return MK32(R.dx, R.ax);
}
void f_3928_000C(void) {
    rtl_dos_open(HPTR(MK32(WARG16(1), WARG16(0))), WARG16(2));
    R.sp += 4;
}

/* 392D:0009  ioctl(fd, func, argdx, argcx): int 21h AH=44h with DS:DX =
 * argdx, CX = argcx; function 0 returns the device information (DX), the
 * others AX; -1 on error */
static u32 rtl_ioctl(u16 fd, u16 func, u16 dxoff, u16 dxseg, u16 argcx) {
    RTL_ENTRY();
    u16 ds = R.ds;
    R.ah = 0x44; R.al = (u8)func; R.bx = fd; R.cx = argcx;
    R.dx = dxoff; R.ds = dxseg;
    T += 9;
    {
        u16 dx = R.dx;
        int r = plat_ioctl(fd, (u8)func, &dx);
        if (r >= 0) R.dx = dx;
        dos_result(r, 0);
    }
    R.ds = ds;
    T += 2;
    if (R.cf) {
        T += 2;
        rtl_ioerror(R.ax);
        T += 1;
    } else {
        T += 2;
        if (func != 0) {
            T += 1;
        } else {
            R.ax = R.dx;
            T += 2;
        }
    }
    T += 2;
    return MK32(R.dx, R.ax);
}
void f_392D_0009(void) {
    rtl_ioctl(WARG16(0), WARG16(1), WARG16(2), WARG16(3), WARG16(4));
    R.sp += 4;
}

/* 397A:0006  _chmod(path, func, attrib): int 21h AH=43h (func 0 gets, 1
 * sets the attributes); the attributes, or -1 */
static u32 rtl_chmod(const u8 *path, u16 func, u16 attrib) {
    RTL_ENTRY();
    R.ah = 0x43; R.al = (u8)func; R.cx = attrib;
    T += 8;
    {
        u16 a = R.cx;
        int r = plat_chmod(path, (u8)func, &a);
        if (r >= 0) R.cx = a;
        dos_result(r, 1);
    }
    T += 2;
    if (R.cf) {
        T += 2;
        rtl_ioerror(R.ax);
        T += 1;
    } else {
        u16 t = R.ax; R.ax = R.cx; R.cx = t;      /* xchg ax, cx */
        T += 2;
    }
    T += 2;
    return MK32(R.dx, R.ax);
}
void f_397A_0006(void) {
    rtl_chmod(HPTR(MK32(WARG16(1), WARG16(0))), WARG16(2), WARG16(3));
    R.sp += 4;
}

/* 390E:003F  open(path, oflag, pmode): without a text/binary bit oflag
 * gets _fmode's.  O_CREAT: pmode is masked with _notUmask (EINVAL without
 * read/write bits); a missing file is created (int 21h AH=3Ch, read-only
 * when pmode lacks S_IWRITE; with sharing bits it is created, closed and
 * opened again, then made read-only), an existing one fails with O_EXCL.
 * The file is opened (__open); a device gets O_DEVICE and, in binary mode,
 * raw mode (ioctl 1); O_TRUNC cuts a file.  _openfd[handle] = oflag
 * without the create bits, O_CHANGED if O_CREAT or O_TRUNC was given.  The
 * handle, or -1.  Where the original passes fewer argument words than its
 * callee reads (ioctl(fd, 0), _chmod(path, 0)), the callee reads open's
 * saved DI/SI and its local: they are passed here (only DOS sees them) */
static u32 rtl_open(const u8 *path, u16 oflag, u16 pmode) {
    RTL_ENTRY();
    u16 di_in = R.di, si_in = R.si;
    u16 di = oflag, si = 0, made = 0, info, ax;
    T += 8;
    if (!(di & 0xC000)) {
        di |= rtl_fmode & 0xC000;
        T += 3;
    }
    T += 2;
    if (!(di & O_CREAT_)) {
        T += 1;
        goto plain;
    }
    pmode &= rtl_notumask;
    T += 5;
    if (!(pmode & 0x180)) {
        T += 3;
        rtl_ioerror(1);
    }
    T += 5;
    made = LO16(rtl_chmod(path, 0, di_in));     /* file attributes: does it exist? */
    T += 4;
    if (made != 0xFFFF) {
        T += 2;
        if (di & O_EXCL_) {
            T += 3;
            rtl_ioerror(0x50);
            T += 1;
            goto out;
        }
        T += 1;
        goto plain;
    }
    T += 2;
    if (pmode & 0x80) {                      /* S_IWRITE */
        made = 0;
        T += 2;
    } else {
        made = 1;                            /* read-only */
        T += 1;
    }
    T += 2;
    T += 2;
    if (di & 0xF0) {                         /* sharing bits: create, close, open below */
        T += 5;
        si = LO16(rtl_dos_creat(path, 0));
        T += 4;
        if ((i16)si < 0) {
            R.ax = si;
            T += 2;
            goto out;
        }
        T += 2;
        rtl_dos_close(si);
        R.cx = si;
        T += 2;
        goto open_it;
    }
    T += 4;
    si = LO16(rtl_dos_creat(path, made));
    T += 4;
    if ((i16)si < 0) {
        R.ax = si;
        T += 2;
        goto out;
    }
    T += 1;
    goto set_flags;
plain:
    made = 0;
    T += 1;
open_it:
    T += 4;
    si = LO16(rtl_dos_open(path, di));
    T += 5;
    if ((i16)si < 0) goto set_flags;
    T += 4;
    info = LO16(rtl_ioctl(si, 0, di_in, si_in, made));
    R.cx = 0;
    T += 5;
    if (info & 0x80) {                       /* a device */
        di |= O_DEVICE_;
        T += 3;
        if (di & O_BINARY_) {
            T += 10;
            rtl_ioctl(si, 1, (u16)((info & 0xFF) | 0x20), 0, di_in);   /* raw mode */
            T += 1;
        }
        T += 1;
    } else {
        T += 2;
        if (di & O_TRUNC_) {
            T += 2;
            rtl_dos_truncate(si);
        }
    }
    T += 2;
    if (made != 0 && (T += 2, di & 0xF0)) {
        T += 7;
        rtl_chmod(path, 1, 1);               /* make the new file read-only */
        T += 1;
    }
set_flags:
    T += 2;
    if ((i16)si >= 0) {
        T += 2;
        if (di & 0x300) {
            ax = O_CHANGED_;
            T += 2;
        } else {
            ax = 0;
            T += 1;
        }
        R.dx = di & 0xF8FF;
        R.bx = (u16)(si << 1);
        rtl_openfd(si) = ax | R.dx;
        T += 6;
    }
    R.ax = si;
    T += 2;
out:
    T += 5;
    return MK32(R.dx, R.ax);
}
void f_390E_003F(void) {
    rtl_open(HPTR(MK32(WARG16(1), WARG16(0))), WARG16(2), WARG16(3));
    R.sp += 4;
}

/* 39C8:0007  findfirst(path, ffblk, attrib): the DTA set to ffblk (int 21h
 * AH=1Ah), then AH=4Eh; 0, or -1 */
static u32 rtl_findfirst(const u8 *path, u8 *ffblk, u16 attrib) {
    RTL_ENTRY();
    R.ah = 0x1A;
    T += 6;
    plat_set_dta(ffblk);
    R.ah = 0x4E; R.cx = attrib;
    T += 4;
    dos_result(plat_findfirst(path), 1);
    T += 2;
    if (R.cf) {
        T += 2;
        rtl_ioerror(R.ax);
        T += 1;
    } else {
        R.ax = 0;
        T += 2;
    }
    T += 2;
    return MK32(R.dx, R.ax);
}
void f_39C8_0007(void) {
    rtl_findfirst(HPTR(MK32(WARG16(1), WARG16(0))), HPTR(MK32(WARG16(3), WARG16(2))), WARG16(4));
    R.sp += 4;
}

/* 39C8:002D  findnext(ffblk): the DTA set to ffblk, int 21h AH=4Fh; 0, or -1 */
static u32 rtl_findnext(u8 *ffblk) {
    RTL_ENTRY();
    R.ah = 0x1A;
    T += 6;
    plat_set_dta(ffblk);
    R.ah = 0x4F;
    T += 2;
    dos_result(plat_findnext(), 1);
    T += 2;
    if (R.cf) {
        T += 2;
        rtl_ioerror(R.ax);
        T += 1;
    } else {
        R.ax = 0;
        T += 2;
    }
    T += 2;
    return MK32(R.dx, R.ax);
}
void f_39C8_002D(void) {
    rtl_findnext(HPTR(MK32(WARG16(1), WARG16(0))));
    R.sp += 4;
}

/* 0000:0172  __restorezero: interrupt vectors 0, 4, 5 and 6 back to the
 * values saved at startup (DS:5B..6A), int 21h AH=25h */
static u32 rtl_restorezero(void) {
    static const u8 vec[4] = {0, 4, 5, 6};
    RTL_ENTRY();
    u16 ds = R.ds;
    for (int i = 0; i < 4; i++) {
        u16 o = (u16)(0x5B + 4 * i);
        R.ax = (u16)(0x2500 | vec[i]);
        R.dx = rw(ds, o); R.ds = rw(ds, (u16)(o + 2));
        T += i == 0 ? 4 : 5;
        plat_setvect(vec[i], MK32(R.ds, R.dx));
        R.ds = ds;
    }
    T += 2;
    return MK32(R.dx, R.ax);
}
void f_0000_0172(void) {
    rtl_restorezero();
    R.sp += 4;
}

/* 0000:010D  __exit(status): DS = DGROUP, __restorezero, the hook at
 * DS:1966 (a far function of the startup segment, called as push cs +
 * near call), int 21h AH=4Ch AL=status.  That does not return: the
 * runtime's DOS ends the host process (runtime/dos.c, exit(status)) */
static void __attribute__((noreturn)) rtl_dos_exit(u16 status) {
    RTL_ENTRY();
    R.ds = rw(SEGV(0x0000), 0x1C7);          /* DGROUP@ */
    T += 2;
    rtl_restorezero();
    push16(SEGV(0x0000));
    T += 2;
    push16(0x011C); call_near(SEGV(0x0000), rw(R.ds, 0x1966));
    R.bp = R.sp;
    R.ah = 0x4C; R.al = (u8)status;
    T += 4;
    plat_exit((u8)status);
    cpu_halt("DOS terminate (int 21h AH=4Ch) returned");
}
void f_0000_010D(void) {
    rtl_dos_exit(WARG16(0));
}

/* 38C0:0008  exit(status): the atexit functions, last registered first
 * (_atexitcnt at DS:1822, far pointers at DS:A6A2), the cleanup hooks
 * _exitbuf, _exitfopen, _exitopen (far pointers at DS:1814/1818/181C),
 * then __exit, which ends the program (no return) */
void rtl_exit(u16 status) {
    RTL_ENTRY();
    T += 3;
    for (;;) {
        u16 n = rtl_atexit_count;
        rtl_atexit_count = (u16)(n - 1);
        T += 4;
        if (n == 0) break;
        POLL();
        R.bx = (u16)(rtl_atexit_count << 2);
        T += 4;
        call_far_addr(G32((u16)(RTL_ATEXIT_TBL + R.bx)), 0);
    }
    T += 1;
    call_far_addr(G32(0x1814), 0);
    T += 1;
    call_far_addr(G32(0x1818), 0);
    T += 1;
    call_far_addr(G32(0x181C), 0);
    T += 2;
    rtl_dos_exit(status);
}
void f_38C0_0008(void) {
    rtl_exit(WARG16(0));
}

/* 385D:0070  open the .RSC files: closes the open ones, clears the
 * 600-entry index, then for every *.RSC file (findfirst/findnext, the DTA
 * in the stack frame) opens it read/write binary, reads its 600 x 6-byte
 * header {u32 offset, u16 size} and enters each resource with a nonzero
 * offset as {handle, size, offset} -- a later file overrides an earlier
 * one -- and keeps the handle in the list of open files */
u32 rtl_res_open_all(void) {
    RTL_ENTRY();
    u8 ffblk[43];                            /* the original's ffblk (in its frame; name at +30) */
    u16 si, di, found;
    u32 hdr;
    si = 0;
    T += 7;
    while ((T += 2, si != g_res_nfiles)) {
        POLL();
        T += 4;
        rtl_close(g_res_handles(si));
        si++;
        T += 3;
    }
    si = 0;
    T += 2;
    while ((T += 2, si != 0x258)) {
        POLL();
        u16 seg = FPSEG(g_res_index), o = (u16)(FPOFF(g_res_index) + si * 8);
        ww(seg, o, 0);
        ww(seg, (u16)(o + 2), 0);
        ww(seg, (u16)(o + 6), 0);
        ww(seg, (u16)(o + 4), 0);
        si++;
        T += 23;
    }
    g_res_nfiles = 0;
    T += 10;
    found = LO16(rtl_findfirst(HPTR(DSP(0x17B2)), ffblk, 0));   /* "*.rsc" */
    T += 5;
    hdr = rtl_malloc(0xE10);
    T += 4;
    for (;;) {
        POLL();
        T += 2;
        if (found != 0) {
            T += 1;
            break;
        }
        T += 8;
        di = LO16(rtl_open(ffblk + 30, 0x8004, 0x80));   /* O_RDWR | O_BINARY */
        T += 8;
        rtl_read(di, HPTR(hdr), 0xE10);
        si = 0;
        T += 3;
        while ((T += 2, si != 0x258)) {
            T += 1;
            POLL();
            u16 h = (u16)(LO16(hdr) + si * 6);
            R.dx = 0;                        /* mul: si * 6 < 65536 */
            T += 8;
            if ((rw(HI16(hdr), h) | rw(HI16(hdr), (u16)(h + 2))) != 0) {
                u16 seg = FPSEG(g_res_index), o = (u16)(FPOFF(g_res_index) + si * 8);
                ww(seg, o, di);
                ww(seg, (u16)(o + 2), rw(HI16(hdr), (u16)(h + 4)));
                u16 lo = rw(HI16(hdr), h), hi = rw(HI16(hdr), (u16)(h + 2));
                ww(seg, (u16)(o + 6), hi);
                ww(seg, (u16)(o + 4), lo);
                T += 41;
            }
            si++;
            T += 1;
        }
        g_res_handles(g_res_nfiles) = di;
        g_res_nfiles++;
        T += 8;
        found = LO16(rtl_findnext(ffblk));
        T += 3;
    }
    T += 3;
    R.ax = rtl_free(LO16(hdr), HI16(hdr));
    g_res_open = 1;
    T += 7;
    return MK32(R.dx, R.ax);
}
void f_385D_0070(void) {
    rtl_res_open_all();
    R.sp += 4;
}

/* 385D:04F3  res_create(id, size): appends a resource of size bytes to the
 * first .RSC file (opening the files if needed): header entry id = {old end
 * of the file, size}; the bytes written are whatever lies in the stack from
 * the frame's handle word upwards -- res_write fills the resource after
 * this; FINDINGS "Savegames": every save appends a new copy.  Enters it in
 * the index; returns the bytes written.  The callers pass size as a long:
 * its high word (size_hi) is not used */
u32 rtl_res_create(u16 id, u16 size, u16 size_hi) {
    RTL_ENTRY();
    u16 bp_ = (u16)(R.sp - 6);               /* where the original's frame was (see below) */
    u16 opened, fd, n;
    u8 head[4], sz[2];                       /* the header words (end of file, size: its frame) */
    u32 end, hdr;
    (void)size_hi;
    T += 8;
    if (g_res_open == 0) {
        T += 2;
        rtl_res_open_all();
        opened = 1;
        T += 2;
    } else {
        opened = 0;
        T += 1;
    }
    fd = g_res_handles(0);
    sz[0] = (u8)size; sz[1] = (u8)(size >> 8);
    T += 12;
    end = rtl_lseek(fd, 0, 0, 2);
    head[0] = (u8)end; head[1] = (u8)(end >> 8); head[2] = (u8)(end >> 16); head[3] = (u8)(end >> 24);
    hdr = (u32)(i32)(i16)(u16)(id * 6);      /* mul si; cwd */
    T += 12;
    rtl_lseek(fd, LO16(hdr), HI16(hdr), 0);
    T += 8;
    rtl_write(fd, head, 4);
    T += 8;
    rtl_write(fd, sz, 2);
    T += 8;
    rtl_lseek(fd, 0, 0, 2);
    T += 7;
    /* the resource's bytes: whatever lies in the emulated stack from the
     * original frame's handle word upwards (res_write fills it next) */
    n = LO16(rtl_write(fd, HPTR(MK32(R.ss, (u16)(bp_ - 4))), size));
    {
        u16 seg = FPSEG(g_res_index), o = (u16)(FPOFF(g_res_index) + id * 8);
        ww(seg, o, fd);
        ww(seg, (u16)(o + 2), n);
        ww(seg, (u16)(o + 6), HI16(end));
        ww(seg, (u16)(o + 4), LO16(end));
        R.bx = o; R.es = seg; R.dx = HI16(end); R.ax = LO16(end);
    }
    T += 36;
    if (opened) {
        T += 2;
        res_close_all(R.ax);
    }
    R.ax = n; R.dx = 0;
    T += 7;
    return MK32(R.dx, R.ax);
}
void f_385D_04F3(void) {
    rtl_res_create(WARG16(0), WARG16(1), WARG16(2));
    R.sp += 4;
}
