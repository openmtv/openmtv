/* Turbo C 2.0 start-up code: C0 (large model), SETARGV and SETENVP, module
 * 0000 of MTV.EXE (0000:0000..0312), in C (ROADMAP Phase 3).
 *
 * DOS enters the program at 0000:0000 with DS = ES = the PSP.  The start-up
 * notes the DOS version, the PSP, the environment and the top of memory,
 * saves interrupt vectors 0, 4, 5 and 6 and points 0 (divide error) at its
 * own handler, counts the environment's strings (and reads the 8087
 * variable "87=Y/N"), cuts the program's memory block down to code + data +
 * stack, clears the BSS, builds argv on the stack and envp in a heap block,
 * notes the BIOS tick count and calls main(argc, argv, envp), then exit()
 * with its result.  A failure (environment or stack too big, no memory)
 * ends in "Abnormal program termination".
 *
 * Exact time, like the rest of the run-time library (rtl.h): each T += counts
 * the original's instructions since the previous count, a rep string
 * instruction one more per element, and POLL() (RTL_ENTRY) stands where the
 * machine-level code took interrupts: function entries, loop heads, after
 * sti.  What it leaves is what the original leaves -- the variables,
 * DGROUP@ and SETARGV's parked return address in the code segment, the
 * stack (argv's strings and array, the call frames) and the registers
 * main() starts with -- so classic runs stay bit-identical
 * (tools/nativebase.py). */
#include "game.h"

#define CODE0 SEGV(0x0000)             /* this module's code segment */

/* Turbo C's start-up variables (DGROUP) */
#define C0_VECTORS   0x5B              /* _Int0Vector, _Int4Vector, _Int5Vector, _Int6Vector */
#define C0_ARGC      0x6B
#define C0_ARGV      0x6D              /* far */
#define C0_ENVIRON   0x71              /* far: envp */
#define C0_ENVLNG    0x75              /* the environment's size (before: its offset, 0) */
#define C0_ENVSEG    0x77
#define C0_ENVSIZE   0x79              /* bytes for the envp array */
#define C0_PSP       0x7B
#define C0_VERSION   0x7D              /* AL major, AH minor */
#define C0_8087      0x81
#define C0_STARTTIME 0x83              /* BIOS ticks at the start (low, high) */
#define C0_HEAPBASE  0x89              /* segment words of _heapbase, _brklvl, _heaptop */
#define C0_BRKLVL    0x8D
#define C0_HEAPTOP   0x91
#define C0_STKLEN    0x1820
#define C0_HOOK_INIT 0x1964            /* far functions of this segment, called as */
#define C0_HOOK_MAIN 0x1968            /* push cs + call near [hook] (exit: 0x1966) */
#define C0_BSS_LO    0x196A
#define C0_BSS_HI    0xA722

/* repne scasb with AL = 0: past the next 0 byte at seg:*di, at most *cx bytes */
static void scan_to_zero(u16 seg, u16 *di, u16 *cx) {
    while (*cx) {
        u8 b = MEM8(seg, *di);
        (*di)++; (*cx)--; T += 1;
        if (b == 0) break;
    }
}

/* 0000:01A7  write CX bytes at DS:DX to stderr (near) */
static void c0_write_stderr(void) {
    RTL_ENTRY();
    T += 3;
    plat_write(2, HPTR(MK32(R.ds, R.dx)), R.cx);
    T += 1;
    R.sp += 2;                           /* ret */
}

/* 0000:01AF  "Abnormal program termination" (DS:003D, 30 bytes), exit code 3 */
static void __attribute__((noreturn)) c0_abort(void) {
    RTL_ENTRY();
    R.cx = 0x1E; R.dx = 0x3D;
    R.ds = MEM16(CODE0, 0x1C7);
    T += 5;
    push16(0x01BE); c0_write_stderr();
    R.ax = 3;
    push16(3);
    T += 3;
    push16(CODE0); push16(0x01C7); f_0000_010D();     /* __exit(3) */
    __builtin_unreachable();
}

/* 0000:0125  the divide-error handler (interrupt 0): "Divide error"
 * (DS:002F, 14 bytes), exit code 3 */
void f_0000_0125(void) {
    RTL_ENTRY_NOPOLL();
    R.cx = 0xE; R.dx = 0x2F;
    T += 4;
    R.ds = MEM16(CODE0, 0x1C7);
    T += 2;
    push16(0x01BE); c0_write_stderr();
    R.ax = 3;
    push16(3);
    T += 3;
    push16(CODE0); push16(0x01C7); f_0000_010D();
}

/* 0000:019F  start-up/exit hook without a floating-point emulator: _8087 = 0 */
void f_0000_019F(void) {
    RTL_ENTRY_NOPOLL();
    G16(C0_8087) = 0;
    T += 2;
    R.sp += 4;                           /* retf */
}

/* 0000:01A6  empty start-up hook */
void f_0000_01A6(void) {
    RTL_ENTRY_NOPOLL();
    T += 1;
    R.sp += 4;                           /* retf */
}

/* push cs + call near [hook]: a far function of this segment by its offset */
static void c0_hook(u16 hook, u16 ret_ip) {
    push16(CODE0); push16(ret_ip);
    call_near(CODE0, G16(hook));
}

/* 0000:012F  save interrupt vectors 0, 4, 5 and 6, point 0 at 0000:0125 (near) */
static void c0_save_vectors(void) {
    RTL_ENTRY();
    push16(R.ds);
    T += 3; G32(C0_VECTORS) = plat_getvect(0);
    T += 4; G32(C0_VECTORS + 4) = plat_getvect(4);
    T += 4; G32(C0_VECTORS + 8) = plat_getvect(5);
    T += 4; G32(C0_VECTORS + 12) = plat_getvect(6);
    T += 7; plat_setvect(0, MK32(CODE0, 0x0125));
    R.ds = pop16();
    T += 2;
    R.sp += 2;                           /* ret */
}

/* SETARGV's scan of the command tail (in the PSP from 0081): one character
 * is held back (`held`, AH) until the next shows whether it ends an
 * argument; a blank, tab or CR becomes the 0 that does */
typedef struct {
    u8 cur, held;                        /* AL, AH */
    u16 left;                            /* CX: tail bytes left */
    u16 src;                             /* SI: next tail byte */
    u16 dst;                             /* DI: next byte of the copy on the stack */
    u16 nchars;                          /* DX: bytes stored */
    u16 nargs;                           /* BX */
} ArgScan;
enum { STEP_CHAR, STEP_QUOTE, STEP_END };

/* 0000:025F  store the held character (a 0 ends an argument: one more),
 * hold the current one and read the next: a character, a quote (\" is a
 * quote character), or the end of the tail (near) */
static int c0_argv_step(ArgScan *s, u16 psp) {
    int r;
    RTL_ENTRY();
    T += 2;
    if (s->held || s->cur) {
        s->nchars++;
        T += 2;
        MEM8(R.ss, s->dst) = s->cur; s->dst++;
        T += 2;
        if (s->cur == 0) { s->nargs++; T += 1; }
    }
    s->held = s->cur; s->cur = 0;
    T += 4;
    if (s->left == 0) {
        r = STEP_END;
    } else {
        T += 1;
        s->cur = MEM8(psp, s->src); s->src++;
        s->left--;
        T += 3;
        if (s->cur == '"') {
            s->cur = 0;
            r = STEP_QUOTE;
        } else {
            T += 3;
            if (s->cur == '\\') {
                T += 2;
                if (MEM8(psp, s->src) == '"') {
                    T += 1;
                    s->cur = MEM8(psp, s->src); s->src++;
                    s->left--;
                    T += 1;
                }
            }
            T += 1;
            r = STEP_CHAR;
        }
    }
    T += 1;
    R.sp += 2;                           /* ret */
    return r;
}

/* 0000:01D1  __setargv: _argc/_argv from the command tail and, from DOS 3
 * on, argv[0] = the program's path behind the environment.  Arguments are
 * separated by blanks, tabs and CRs; "..." keeps blanks.  The strings and
 * then the argv array go onto the stack below the caller's frame: a far
 * function that returns with SP lowered -- it parks its return address in
 * the code segment (01C9, DS at 01CD) and jumps back through it. */
static void c0_setargv(void) {
    ArgScan s;
    u16 psp, pathseg, path, pathlen, tail, len, size, sp, p, n;
    u8 c;
    int r;
    RTL_ENTRY();
    MEM16(CODE0, 0x1C9) = pop16();
    MEM16(CODE0, 0x1CB) = pop16();
    MEM16(CODE0, 0x1CD) = R.ds;
    R.df = 0;
    psp = G16(C0_PSP);
    T += 8;
    len = MEM8(psp, 0x80);               /* the tail's length (the CR behind it is read too) */
    tail = 0x81;
    path = (u16)(G16(C0_ENVLNG) + 2);    /* behind the environment's last 0 and a word */
    pathseg = psp;
    pathlen = 1;
    T += 9;
    if (G8(C0_VERSION) >= 3) {
        u16 end = path, cx = 0x7F;
        pathseg = G16(C0_ENVSEG);
        T += 5;
        scan_to_zero(pathseg, &end, &cx);
        T += 1;
        if (cx == 0) goto fail;
        pathlen = (u16)(cx ^ 0x7F);      /* with its 0 */
        T += 1;
    }
    /* the path and the tail's bytes (at most) onto the stack, even size */
    R.sp = (u16)(R.sp - 2);
    size = (u16)((1 + (len + 1) + pathlen) & 0xFFFE);
    sp = R.sp;
    T += 8;
    if (sp < size) goto fail;
    sp = (u16)(sp - size);
    R.sp = sp;
    push16(pathlen);
    T += 8;
    for (n = 0; n < (u16)(pathlen - 1); n++) {          /* rep movsb */
        MEM8(R.ss, (u16)(sp + n)) = MEM8(pathseg, (u16)(path + n));
        T += 1;
    }
    T += 2;
    MEM8(R.ss, (u16)(sp + n)) = 0;
    s.cur = 0; s.held = 0;
    s.left = (u16)(len + 1);
    s.src = tail;
    s.dst = (u16)(sp + n + 1);
    s.nchars = 0;
    s.nargs = 1;                         /* argv[0], the path */
    T += 6;
    for (;;) {                           /* 0243: outside quotes */
        POLL();
        T += 1;
        push16(0x0246); r = c0_argv_step(&s, psp);
        T += 1;
        if (r != STEP_CHAR) {
            for (;;) {                   /* 0248: a quote starts (or the tail ends) */
                POLL();
                T += 1;
                if (r == STEP_END) goto done;
                T += 1;
                push16(0x024D); r = c0_argv_step(&s, psp);
                T += 1;
                if (r != STEP_CHAR) break;   /* closing quote or end: on as outside */
            }
        }
        c = s.cur;
        T += 2;
        if (c != ' ') {
            T += 2;
            if (c != '\r') {
                T += 2;
                if (c != '\t') continue;
            }
        }
        s.cur = 0;                       /* a separator ends the argument */
        T += 2;
    }
done:
    /* argv: far pointers to the strings on the stack, below them */
    n = (u16)(pop16() + s.nchars);       /* bytes of all the strings */
    R.ds = MEM16(CODE0, 0x1CD);
    G16(C0_ARGC) = s.nargs;
    size = (u16)((s.nargs + 1) * 4);
    p = R.sp;                            /* SI: the first string */
    T += 11;
    if (R.sp < size) goto fail;
    R.bp = (u16)(R.sp - size);
    R.sp = R.bp;
    G16(C0_ARGV) = R.bp;
    G16(C0_ARGV + 2) = R.ss;
    T += 3;
    for (;;) {
        POLL();
        T += 1;
        if (n == 0) break;
        MEM16(R.ss, R.bp) = p;
        MEM16(R.ss, (u16)(R.bp + 2)) = R.ss;
        R.bp += 4;
        T += 3;
        for (;;) {                       /* lodsb / loopne: past the string's 0 */
            POLL();
            T += 1;
            c = MEM8(R.ss, p); p++;
            T += 2;
            if (--n && c != 0) continue;
            break;
        }
        T += 1;
        if (c != 0) break;
    }
    MEM16(R.ss, R.bp) = 0;
    MEM16(R.ss, (u16)(R.bp + 2)) = 0;
    T += 4;
    /* the registers it leaves */
    R.ax = 0; R.bx = size; R.cx = n; R.dx = s.nchars; R.si = p; R.di = s.dst; R.es = R.ss;
    return;                              /* jmp far cs:[01C9]: back to the caller */
fail:
    POLL();
    T += 1;
    c0_abort();
}

/* 0000:02CF  __setenvp: envp = malloc(_envSize) holding a far pointer to
 * each environment string, then NULL */
static void c0_setenvp(void) {
    u16 envseg, di, cx, bx, ds;
    u32 envp;
    RTL_ENTRY();
    envseg = G16(C0_ENVSEG);
    di = 0;
    push16(envseg);
    push16(G16(C0_ENVSIZE));
    T += 5;
    push16(CODE0); push16(0x02DF);
    envp = rtl_malloc(G16(C0_ENVSIZE));
    R.sp += 4;                           /* (malloc's far return) */
    R.sp += 2;                           /* add sp, 2 */
    R.es = pop16();
    G32(C0_ENVIRON) = envp;
    push16(R.ds);
    ds = HI16(envp);
    bx = LO16(envp);
    T += 9;
    if (envp == 0) {                     /* (only both words 0: or ax, dx) */
        T += 1;
        c0_abort();
    }
    cx = 0xFFFF;
    T += 2;
    do {
        POLL();
        MEM16(ds, bx) = di;
        MEM16(ds, (u16)(bx + 2)) = envseg;
        bx += 4;
        T += 4;
        scan_to_zero(envseg, &di, &cx);
        T += 2;
    } while (MEM8(envseg, di) != 0);
    MEM16(ds, bx) = 0;
    MEM16(ds, (u16)(bx + 2)) = 0;
    R.ds = pop16();
    T += 4;
    R.sp += 4;                           /* retf */
    /* the registers it leaves */
    R.ax = 0; R.bx = bx; R.cx = cx; R.dx = ds; R.di = di; R.es = envseg;
}

/* 0000:0000  C0: the program's entry (from DOS: DS = ES = the PSP) */
void rtl_startup(void) {
    u16 version, top, envseg, env, di, cx, nstr, dx, bp, paras, max;
    u32 ticks;
    u8 midnight;
    RTL_ENTRY();
    MEM16(CODE0, 0x1C7) = DSEG;          /* DGROUP@: the data segment, for code that has only CS */
    T += 4;
    version = plat_dos_version();
    top = MEM16(R.ds, 2);                /* PSP: the top of memory (segment) */
    envseg = MEM16(R.ds, 0x2C);          /* PSP: the environment */
    R.ds = DSEG;
    G16(C0_VERSION) = version;
    G16(C0_PSP) = R.es;
    G16(C0_ENVSEG) = envseg;
    G16(C0_HEAPTOP) = top;
    G16(C0_8087) = 0xFFFF;
    T += 9;
    push16(0x002E); c0_save_vectors();

    /* the environment: count its strings up to the empty one; "87=" sets
     * _8087 (0 for "87=N", 1 for "87=Y", after the -1 above) */
    env = G16(C0_ENVLNG);
    di = env;
    nstr = 0;
    cx = 0x7FFF;
    T += 4;
    for (;;) {
        POLL();
        T += 2;
        if (MEM16(envseg, di) == 0x3738) {           /* "87" */
            u16 w = MEM16(envseg, (u16)(di + 2));
            T += 3;
            if ((u8)w == '=') {
                G16(C0_8087)++;
                T += 4;
                if ((u8)((w >> 8) & 0xDF) == 'Y') { G16(C0_8087)++; T += 1; }
            }
        }
        T += 1;
        scan_to_zero(envseg, &di, &cx);
        T += 1;
        if (cx == 0) { T += 1; c0_abort(); }         /* no end within 32 KB */
        nstr++;
        T += 3;
        if (MEM8(envseg, di) == 0) break;
    }
    G16(C0_ENVLNG) = (u16)-(u16)(cx | 0x8000);        /* its bytes, the last 0 included */
    G16(C0_ENVSIZE) = (u16)(((nstr << 2) + 0x10) & 0xFFF0);

    /* the stack (_stklen, at least 512 bytes) behind the data; the memory
     * block is cut down to its end, the heap starts there */
    dx = R.ss;
    bp = (u16)(top - dx);
    paras = G16(C0_STKLEN);
    T += 13;
    if (paras < 0x200) { paras = 0x200; G16(C0_STKLEN) = paras; T += 2; }
    paras = (u16)((paras >> 4) + 1);
    T += 5;
    if (bp < paras) { T += 1; c0_abort(); }
    G16(C0_HEAPBASE) = (u16)(paras + dx);
    G16(C0_BRKLVL) = (u16)(paras + dx);
    push16(paras);
    T += 10;
    plat_setblock(G16(C0_PSP), (u16)(paras + dx - G16(C0_PSP)), &max);
    R.sp += 2;
    R.ifl = 0;
    R.ss = dx;
    R.sp = (u16)(paras << 4);
    T += 6;
    R.ifl = 1;
    POLL();

    /* the BSS */
    T += 6;
    for (u16 o = C0_BSS_LO; o < C0_BSS_HI; o++) G8(o) = 0;
    T += C0_BSS_HI - C0_BSS_LO;
    T += 2;
    c0_hook(C0_HOOK_INIT, 0x00D3);
    T += 1;
    push16(CODE0); push16(0x00D8); c0_setargv();
    T += 1;
    push16(CODE0); push16(0x00DD); c0_setenvp();
    T += 2;
    ticks = plat_ticks(&midnight);
    G16(C0_STARTTIME) = (u16)ticks;
    G16(C0_STARTTIME + 2) = (u16)(ticks >> 16);
    T += 4;
    c0_hook(C0_HOOK_MAIN, 0x00EE);

    /* main(argc, argv, envp), then exit() with its result */
    push16(G16(C0_ENVIRON + 2)); push16(G16(C0_ENVIRON));
    push16(G16(C0_ARGV + 2)); push16(G16(C0_ARGV));
    push16(G16(C0_ARGC));
    R.ax = midnight; R.cx = (u16)(ticks >> 16); R.dx = (u16)ticks;
    T += 6;
    push16(CODE0); push16(0x0107); f_0054_0005();
    push16(R.ax);
    T += 2;
    push16(CODE0); push16(0x010D);
    rtl_exit(rw(R.ss, (u16)(R.sp + 4)));
}

void f_0000_0000(void) { rtl_startup(); }

const native_entry src_native_table[] = {
    { 0x00000125, f_0000_0125 },
    { 0x0000019F, f_0000_019F },
    { 0x000001A6, f_0000_01A6 },
};
const int src_native_count = sizeof src_native_table / sizeof src_native_table[0];
