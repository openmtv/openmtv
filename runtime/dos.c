/* INT 21h DOS services backed by a host directory. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include "runtime.h"

/* Windows: every file in binary mode (text mode would translate line ends in
 * the .rsc files and the saved games) */
#ifndef O_BINARY
#define O_BINARY 0
#endif

static char gamedir[1024];
static int handles[64];          /* DOS handle -> host fd, -1 free */
static u16 dta_seg = PSP_SEG, dta_off = 0x80;   /* as int 21h AH=2Fh reports it */
static u8 *dta_ptr;                              /* where find results go (NULL: PSP:0080) */
static u16 mem_block_paras;      /* size of the program's memory block */
int dos_trace;

/* ---- path mapping: DOS name -> existing host file, case-insensitive ---- */
static int host_path(const u8 *dosname, char *out, size_t outsz, int must_exist) {
    char name[128]; int n = 0;
    for (;;) { u8 c = dosname[n]; if (!c || n >= 127) break; name[n++] = (char)c; }
    name[n] = 0;
    const char *p = name;
    if (p[0] && p[1] == ':') p += 2;
    while (*p == '\\' || *p == '/') p++;
    const char *base = strrchr(p, '\\');
    base = base ? base + 1 : p;
    DIR *d = opendir(gamedir);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            if (!strcasecmp(e->d_name, base)) {
                snprintf(out, outsz, "%s/%s", gamedir, e->d_name);
                closedir(d);
                return 1;
            }
        }
        closedir(d);
    }
    if (must_exist) return 0;
    char up[128]; size_t i;
    for (i = 0; base[i] && i < sizeof up - 1; i++) up[i] = (char)toupper((u8)base[i]);
    up[i] = 0;
    snprintf(out, outsz, "%s/%s", gamedir, up);
    return 1;
}

/* The game's files as a network game compares them (runtime/net.h): one
 * hash over every resource of SOURCE1.RSC and SOURCE2.RSC (the same index
 * as the game's resource manager: 600 entries of offset and size, the
 * second file's entries over the first's) but the savegame slots D1h..DBh
 * -- saving appends to SOURCE1.RSC, and a player who saved has the same
 * game.  0 if a file is missing. */
u64 dos_game_files_hash(void) {
    static const char *const files[2] = { "SOURCE1.RSC", "SOURCE2.RSC" };
    struct { int file; u32 off; u16 size; } idx[600];
    FILE *f[2] = { NULL, NULL };
    u64 h = 1469598103934665603ull;
    memset(idx, 0, sizeof idx);
    for (int k = 0; k < 2; k++) {
        char path[512];
        if (!host_path((const u8 *)files[k], path, sizeof path, 1) || !(f[k] = fopen(path, "rb"))) { h = 0; goto out; }
        u8 e[6 * 600];
        if (fread(e, 1, sizeof e, f[k]) != sizeof e) { h = 0; goto out; }
        for (int i = 0; i < 600; i++) {
            u32 off = e[6 * i] | e[6 * i + 1] << 8 | e[6 * i + 2] << 16 | (u32)e[6 * i + 3] << 24;
            if (off) { idx[i].file = k; idx[i].off = off; idx[i].size = (u16)(e[6 * i + 4] | e[6 * i + 5] << 8); }
        }
    }
    for (int i = 0; i < 600; i++) {
        if (!idx[i].off || (i >= 0xD1 && i <= 0xDB)) continue;
        u8 buf[4096];
        u8 head[4] = { (u8)i, (u8)(i >> 8), (u8)idx[i].size, (u8)(idx[i].size >> 8) };
        for (int j = 0; j < 4; j++) h = (h ^ head[j]) * 1099511628211ull;
        if (fseek(f[idx[i].file], (long)idx[i].off, SEEK_SET)) { h = 0; goto out; }
        for (u32 left = idx[i].size; left; ) {
            size_t n = fread(buf, 1, left < sizeof buf ? left : sizeof buf, f[idx[i].file]);
            if (!n) { h = 0; goto out; }
            for (size_t j = 0; j < n; j++) h = (h ^ buf[j]) * 1099511628211ull;
            left -= (u32)n;
        }
    }
out:
    for (int k = 0; k < 2; k++) if (f[k]) fclose(f[k]);
    return h;
}

static int alloc_handle(int fd) {
    for (int h = 5; h < 64; h++) if (handles[h] < 0) { handles[h] = fd; return h; }
    return -1;
}

static void ok(void) { R.cf = 0; }
static void err(u16 code) { R.cf = 1; R.ax = code; }

/* ---- FindFirst/FindNext over the game directory ---- */
static char find_pat[32];
static DIR *find_dir;

int dos_state_blocks(StateBlock *b) {
    int n = 0;
    STATE_BLOCK(b, n, dta_seg); STATE_BLOCK(b, n, dta_off); STATE_BLOCK(b, n, dta_ptr); STATE_BLOCK(b, n, mem_block_paras);
    return n;
}

static int wild_match(const char *pat, const char *s) {
    /* DOS 8.3 matching: name and extension separately, '*' and '?' */
    char pn[16] = "", pe[8] = "", sn[16] = "", se[8] = "";
    const char *d;
    d = strchr(pat, '.'); if (d) { snprintf(pn, sizeof pn, "%.*s", (int)(d - pat), pat); snprintf(pe, sizeof pe, "%s", d + 1); } else snprintf(pn, sizeof pn, "%s", pat);
    d = strchr(s, '.');   if (d) { snprintf(sn, sizeof sn, "%.*s", (int)(d - s), s); snprintf(se, sizeof se, "%s", d + 1); } else snprintf(sn, sizeof sn, "%s", s);
    for (int part = 0; part < 2; part++) {
        const char *a = part ? pe : pn, *b = part ? se : sn;
        int i = 0;
        for (; a[i]; i++) {
            if (a[i] == '*') goto next;
            if (a[i] == '?') { if (!b[i]) continue; else continue; }
            if (toupper((u8)a[i]) != toupper((u8)b[i])) return 0;
        }
        if (b[i]) return 0;
    next:;
    }
    return 1;
}

static int find_next_entry(void) {
    struct dirent *e;
    while (find_dir && (e = readdir(find_dir))) {
        if (e->d_name[0] == '.') continue;
        if (strlen(e->d_name) > 12) continue;
        if (!wild_match(find_pat, e->d_name)) continue;
        char path[1200]; struct stat st;
        snprintf(path, sizeof path, "%s/%s", gamedir, e->d_name);
        if (stat(path, &st) || !S_ISREG(st.st_mode)) continue;
        u8 *d = dta_ptr ? dta_ptr : M + lin(PSP_SEG, 0x80);
        d[0x15] = 0x20;
        d[0x16] = 0; d[0x17] = 0; d[0x18] = 0x21; d[0x19] = 0x1A;
        d[0x1A] = (u8)st.st_size; d[0x1B] = (u8)(st.st_size >> 8); d[0x1C] = (u8)(st.st_size >> 16); d[0x1D] = (u8)(st.st_size >> 24);
        int i = 0;
        for (; e->d_name[i] && i < 12; i++) d[0x1E + i] = (u8)toupper((u8)e->d_name[i]);
        d[0x1E + i] = 0;
        return 1;
    }
    return 0;
}

int dos_snapshot_pos(long *pos, int n) {
    for (int h = 0; h < n && h < 64; h++) pos[h] = handles[h] >= 0 ? (long)lseek(handles[h], 0, SEEK_CUR) : -1;
    return n;
}
void dos_restore_pos(const long *pos, int n) {
    for (int h = 0; h < n && h < 64; h++) if (handles[h] >= 0 && pos[h] >= 0) lseek(handles[h], pos[h], SEEK_SET);
}

/* ---- file services as C functions (platform.c; int21 below calls them):
 * a handle or count >= 0, or minus the DOS error code.  A per-function
 * difftest cannot replay file side effects: those abort the test first. */
static void side_effect(const char *what) { if (difftest_active) difftest_abort(what); }

/* AH=3Ch: create or truncate */
int dos_create(const u8 *name) {
    char path[1200];
    side_effect("dos side effect");
    host_path(name, path, sizeof path, 0);
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC | O_BINARY, 0644);
    if (fd < 0) return -3;
    int h = alloc_handle(fd); if (h < 0) { close(fd); return -4; }
    if (dos_trace) fprintf(stderr, "  create %s -> %d\n", path, h);
    return h;
}
/* AH=3Dh: open an existing file; mode 0 read, 1 write, 2 both */
int dos_open(const u8 *name, u8 mode) {
    char path[1200];
    side_effect("dos side effect");
    if (!host_path(name, path, sizeof path, 1)) { if (dos_trace) fprintf(stderr, "  open fail\n"); return -2; }
    mode &= 3;
    int fd = open(path, (mode == 0 ? O_RDONLY : mode == 1 ? O_WRONLY : O_RDWR) | O_BINARY);
    if (fd < 0) return -5;
    int h = alloc_handle(fd); if (h < 0) { close(fd); return -4; }
    if (dos_trace) fprintf(stderr, "  open %s -> %d\n", path, h);
    return h;
}
/* AH=3Eh */
int dos_close(u16 h) {
    side_effect("dos side effect");
    if (h < 5) return 0;
    if (h >= 64 || handles[h] < 0) return -6;
    close(handles[h]); handles[h] = -1;
    return 0;
}
/* AH=3Fh: n bytes to seg:off; the count read */
int dos_read(u16 h, u8 *dst, u16 n) {
    if (h == 0) return 0;
    if (h >= 64 || handles[h] < 0) return -6;
    u8 buf[65536];
    ssize_t got = read(handles[h], buf, n);
    if (got < 0) return -5;
    memcpy(dst, buf, (size_t)got);
    time_advance(got / 4);
    return (int)got;
}
/* AH=40h: n bytes from seg:off (n = 0: truncate here); the count written */
int dos_write(u16 h, const u8 *src, u16 n) {
    if (h >= 5) side_effect("file write");
    if (h == 1 || h == 2) {
        fwrite(src, 1, n, stderr);
        return n;
    }
    if (h >= 64 || handles[h] < 0) return -6;
    const u8 *buf = src;
    if (n == 0) {
        off_t pos = lseek(handles[h], 0, SEEK_CUR);
        if (ftruncate(handles[h], pos)) return -5;
        return 0;
    }
    ssize_t w = write(handles[h], buf, n);
    if (w < 0) return -5;
    return (int)w;
}
/* AH=41h */
int dos_unlink(const u8 *name) {
    char path[1200];
    side_effect("dos side effect");
    if (!host_path(name, path, sizeof path, 1)) return -2;
    unlink(path);
    return 0;
}
/* AH=42h: whence 0 start, 1 current, 2 end; the new position, or -1 and *err */
s32 dos_lseek(u16 h, s32 pos, u8 whence, u16 *e) {
    if (h >= 64 || (h >= 5 && handles[h] < 0)) { *e = 6; return -1; }
    if (h < 5) return 0;
    off_t r = lseek(handles[h], (off_t)pos, whence == 0 ? SEEK_SET : whence == 1 ? SEEK_CUR : SEEK_END);
    if (r < 0) { *e = 25; return -1; }
    return (s32)r;
}
/* AH=44h AL=00h: device information word */
int dos_ioctl_info(u16 h) {
    if (h < 5) return (h == 0) ? 0x81 : (h <= 2 ? 0x82 : 0x80);    /* char device */
    if (h < 64 && handles[h] >= 0) return 0x0002;                   /* file on C: */
    return -6;
}
/* AH=43h: func 0 gets the attributes (*attr), 1 sets them; 0 or -2 */
int dos_chmod(const u8 *name, u8 func, u16 *attr) {
    char path[1200];
    if (!host_path(name, path, sizeof path, 1)) return -2;
    if (func == 0) *attr = 0x20;
    return 0;
}
/* AH=44h: func 0 device information (also *dx), 1 set it, 8 removable;
 * AX afterwards, or minus the error */
int dos_ioctl(u16 h, u8 func, u16 *dx) {
    if (func == 0x00) {
        int r = dos_ioctl_info(h);
        if (r < 0) return -6;
        *dx = (u16)r;
        return r;
    }
    if (func == 0x01) return 0x4400 | func;            /* AX unchanged */
    if (func == 0x08) return 1;
    return -1;
}
/* AH=1Ah */
void dos_set_dta(u8 *dta) { dta_ptr = dta; }
/* AH=4Eh / 4Fh: the next match into the DTA; 0 or -18 (no more files) */
int dos_findfirst(const u8 *pattern) {
    side_effect("dos side effect");
    char pat[128]; int n = 0;
    for (;;) { u8 c = pattern[n]; if (!c || n >= 127) break; pat[n++] = (char)c; }
    pat[n] = 0;
    const char *b = strrchr(pat, '\\'); b = b ? b + 1 : pat;
    if (b[0] && b[1] == ':') b += 2;
    snprintf(find_pat, sizeof find_pat, "%s", b);
    if (find_dir) closedir(find_dir);
    find_dir = opendir(gamedir);
    return find_next_entry() ? 0 : -18;
}
int dos_findnext(void) {
    side_effect("dos side effect");
    return find_next_entry() ? 0 : -18;
}
/* AH=25h */
void dos_setvect(u8 n, u16 seg, u16 off) { ww(0, n * 4, off); ww(0, n * 4 + 2, seg); }
u32 dos_getvect(u8 n) { return ((u32)rw(0, n * 4 + 2) << 16) | rw(0, n * 4); }
u16 dos_version(void) { return 5; }     /* DOS 5.0: AL major, AH minor */

/* AH=4Ch */
void dos_exit(u8 code) {
    side_effect("dos side effect");
    fprintf(stderr, "\n[DOS] program exit code %d at T=%llu\n", code, (unsigned long long)T);
    exit(code);
}

/* AH=4Ah: resize the program's memory block; 0, or the DOS error: 8 not
 * enough memory (*max: the most there is), 9 not the program's block */
int dos_setblock(u16 seg, u16 paras, u16 *max) {
    u32 maxp = MEM_TOP_SEG - seg;
    if (seg != PSP_SEG) return 9;
    if (paras > maxp) { *max = (u16)maxp; return 8; }
    mem_block_paras = paras;
    return 0;
}

void int21(void) {
    if (difftest_active) {
        switch (R.ah) {
        case 0x56:
            difftest_abort("dos side effect"); break;
        }
    }
    if (dos_trace) fprintf(stderr, "int21 AH=%02X AL=%02X BX=%04X CX=%04X DX=%04X DS=%04X\n", R.ah, R.al, R.bx, R.cx, R.dx, R.ds);
    switch (R.ah) {
    case 0x02: fputc(R.dl, stderr); break;
    case 0x06: if (R.dl != 0xFF) fputc(R.dl, stderr); else R.zf = 1; break;
    case 0x09: { u16 o = R.dx; u8 c; while ((c = rb(R.ds, o++)) != '$') fputc(c, stderr); } break;
    case 0x0B: R.al = bios_key_available() ? 0xFF : 0; break;
    case 0x19: R.al = 2; break;                                     /* C: */
    case 0x1A: dta_seg = R.ds; dta_off = R.dx; dos_set_dta(M + lin(R.ds, R.dx)); break;
    case 0x25: dos_setvect(R.al, R.ds, R.dx); break;
    case 0x2A: R.cx = 1991; R.dh = 6; R.dl = 1; R.al = 6; break;
    case 0x2C: { u64 cs = (u64)(T / insns_per_us / 10000.0); R.ch = (u8)(12 + (cs / 360000) % 12); R.cl = (cs / 6000) % 60; R.dh = (cs / 100) % 60; R.dl = cs % 100; } break;
    case 0x2F: R.es = dta_seg; R.bx = dta_off; break;
    case 0x30: R.ax = dos_version(); R.bx = 0; R.cx = 0; break;
    case 0x33: if (R.al == 0) R.dl = 0; break;
    case 0x35: { u32 v = dos_getvect(R.al); R.bx = (u16)v; R.es = (u16)(v >> 16); } break;
    case 0x36: R.ax = 64; R.bx = 4000; R.cx = 512; R.dx = 8000; break;
    case 0x3B: ok(); break;
    case 0x3C: { int r = dos_create(M + lin(R.ds, R.dx)); if (r < 0) err((u16)-r); else { R.ax = (u16)r; ok(); } } break;
    case 0x3D: { int r = dos_open(M + lin(R.ds, R.dx), R.al); if (r < 0) err((u16)-r); else { R.ax = (u16)r; ok(); } } break;
    case 0x3E: { int r = dos_close(R.bx); if (r < 0) err((u16)-r); else ok(); } break;
    case 0x3F: { int r = dos_read(R.bx, M + lin(R.ds, R.dx), R.cx); if (r < 0) err((u16)-r); else { R.ax = (u16)r; ok(); } } break;
    case 0x40: { int r = dos_write(R.bx, M + lin(R.ds, R.dx), R.cx); if (r < 0) err((u16)-r); else { R.ax = (u16)r; ok(); } } break;
    case 0x41: { int r = dos_unlink(M + lin(R.ds, R.dx)); if (r < 0) err((u16)-r); else ok(); } break;
    case 0x42: {
        u16 e;
        s32 r = dos_lseek(R.bx, (s32)(((u32)R.cx << 16) | R.dx), R.al, &e);
        if (r < 0) err(e); else { R.ax = (u16)r; R.dx = (u16)((u32)r >> 16); ok(); }
    } break;
    case 0x43: { u16 a = R.cx; int r = dos_chmod(M + lin(R.ds, R.dx), R.al, &a); if (r < 0) err((u16)-r); else { R.cx = a; ok(); } } break;
    case 0x44: { u16 dx = R.dx; int r = dos_ioctl(R.bx, R.al, &dx); if (r < 0) err((u16)-r); else { R.dx = dx; R.ax = (u16)r; ok(); } } break;
    case 0x45: case 0x46: err(4); break;
    case 0x47: wb(R.ds, R.si, 0); ok(); break;
    case 0x48: {  /* the program owns all memory: allocation fails like in a full DOS */
        R.bx = 0; err(8);
    } break;
    case 0x49: ok(); break;
    case 0x4A: {
        u16 mx;
        int e = dos_setblock(R.es, R.bx, &mx);
        if (e == 8) R.bx = mx;
        if (e) err((u16)e); else ok();
    } break;
    case 0x4C:
        dos_exit(R.al);
    case 0x4E: { int r = dos_findfirst(M + lin(R.ds, R.dx)); if (r < 0) err((u16)-r); else ok(); } break;
    case 0x4F: { int r = dos_findnext(); if (r < 0) err((u16)-r); else ok(); } break;
    case 0x51: case 0x62: R.bx = PSP_SEG; break;
    case 0x56: err(5); break;
    case 0x57: if (R.al == 0) { R.cx = 0; R.dx = 0x1A21; } ok(); break;
    case 0x59: R.ax = 0; R.bx = 0; R.cx = 0; break;
    default:
        fprintf(stderr, "int21 unhandled AH=%02X AL=%02X\n", R.ah, R.al);
        err(1);
    }
}

void dos_set_args(const char *args) {
    u32 p = lin(PSP_SEG, 0);
    size_t n = strlen(args);
    if (n > 125) n = 125;
    M[p + 0x80] = (u8)(n + 1);
    M[p + 0x81] = ' ';
    memcpy(M + p + 0x82, args, n);
    M[p + 0x82 + n] = 0x0D;
}

void dos_init(const char *dir, const char *progname) {
    snprintf(gamedir, sizeof gamedir, "%s", dir);
    for (int i = 0; i < 64; i++) handles[i] = -1;
    /* environment block: one variable, then the program path for argv[0] */
    u32 e = lin(ENV_SEG, 0);
    const char *env = "PATH=C:\\";
    size_t n = strlen(env);
    memcpy(M + e, env, n + 1);
    M[e + n + 1] = 0;                                   /* end of variables */
    ww(ENV_SEG, (u16)(n + 2), 1);
    char full[64]; snprintf(full, sizeof full, "C:\\%s", progname);
    memcpy(M + e + n + 4, full, strlen(full) + 1);
    /* PSP */
    u32 p = lin(PSP_SEG, 0);
    M[p] = 0xCD; M[p + 1] = 0x20;
    ww(PSP_SEG, 0x02, MEM_TOP_SEG);                     /* top of memory */
    ww(PSP_SEG, 0x2C, ENV_SEG);
    M[p + 0x80] = 0; M[p + 0x81] = 0x0D;                /* empty command tail */
    mem_block_paras = MEM_TOP_SEG - PSP_SEG;
}
