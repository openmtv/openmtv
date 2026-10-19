/* The game's side of the sound driver in C (ROADMAP Phase 3, stage 6; the
 * conventions are those of rtl.h): module 371E, which links the PALLADIX
 * driver loaded from SOURCE1.RSC and holds its 13 far-jump stubs, and the
 * two functions of module 35F8 that the decompiler could not take (their
 * calls into the driver looked like register arguments).
 *
 * The driver (resource 98, "V1.3 - AdLib (TM) soundcard"; 95, "V1.1 -
 * PC-Beeper", when the detector finds no AdLib) and the hardware detector
 * (resource 99) are code inside the resource file: they stay interpreted.  The driver also
 * owns the timer: entry 0 hooks int 08h and reprograms the PIT; its handler
 * counts the game's ticks (entry 3) and plays the song.  Each driver entry
 * has a stub in 371E, "jmp far seg:off", that 371E:00E5 patches with the
 * driver's entry table; the C interface below is one function per entry:
 * it pushes the argument words and a far return address as the game's
 * callf did and jumps through the stub, so the driver sees the stack it
 * expects.  The driver reads no register arguments: every entry the game
 * uses (0, 1, 3, 5, 6, 9) takes its arguments from the stack only -- the
 * DX that the decompiler saw passed to entries 5, 6 and 9 is written before
 * any read on every path.  The glue wrappers of the stubs are the stub
 * itself (a jump: the driver returns to the stub's caller).
 *
 * Exact time like the run-time library: classic runs stay bit-identical. */
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "seat.h"

#define CS371E      SEGV(0x371E)
#define SND_DRVSEG  0x60        /* word in 371E: segment of the linked driver */
#define SND_LINKED  0x62        /* byte in 371E: a driver is linked */
#define SND_SIG     0x63        /* 371E:0063: the driver signature "PALLADIX-PSM\r\n..." (32 bytes) */
#define SND_STUB0   0x83        /* 13 stubs of 5 bytes: EA off seg (jmp far) */
#define SND_NONE    0xC4        /* 371E:00C4: the stubs' target while no driver is linked */

/* What the game still needs of its code segments when it runs without
 * MTV.EXE (runtime.h, struct builtin_image): the bytes of 371E that this
 * module reads -- the two signatures the detector and the driver are
 * compared with, the 13 stubs pointing at 371E:00C4 -- as the EXE has them
 * after loading; and the fallback's instructions, which C runs instead
 * (f_371E_00C4) but which the EXE has there too.  Nothing else of the code
 * segments is read by the decompiled build (tools/exefree.py). */
static const u8 snd_sig_detect[0x24] = {      /* 371E:0000 */
    'P','A','L','L','A','D','I','X','-','P','S','M','D','E','T','E','C','T','\r','\n','$',
    0x1A, 0x00, 0xBA, 0x00, 0x01, 0xB4, 0x09, 0xCD, 0x21, 0xB8, 0xFF, 0x4C, 0xCD, 0x21, 0x90 };
static const u8 snd_sig_driver[0x20] = {      /* 371E:0063, its last word = 13 entries */
    'P','A','L','L','A','D','I','X','-','P','S','M','\r','\n', 0x1A, '$',
    0x90, 0xBA, 0x00, 0x01, 0xB4, 0x09, 0xCD, 0x21, 0xB8, 0xFF, 0x4C, 0xCD, 0x21, 0x90, 0x0D, 0x00 };
static const u8 snd_none_code[5] = { 0xB8, 0xFF, 0xFF, 0x99, 0xCB };   /* 371E:00C4 mov ax, -1 / cwd / retf */
void snd_code_init(void) {
    for (u16 i = 0; i < sizeof snd_sig_detect; i++) wb(CS371E, i, snd_sig_detect[i]);
    for (u16 i = 0; i < sizeof snd_sig_driver; i++) wb(CS371E, (u16)(SND_SIG + i), snd_sig_driver[i]);
    for (u16 n = 0; n < 13; n++) {                                     /* jmp far 371E:00C4 */
        u16 s = (u16)(SND_STUB0 + 5 * n);
        wb(CS371E, s, 0xEA); ww(CS371E, (u16)(s + 1), SND_NONE); ww(CS371E, (u16)(s + 3), CS371E);
    }
    for (u16 i = 0; i < sizeof snd_none_code; i++) wb(CS371E, (u16)(SND_NONE + i), snd_none_code[i]);
}

/* ------------------------------------------------------------------ driver entries */

/* the stub's jmp far, with the caller's frame on the stack: into the driver
 * entry patched there (or 371E:00C4), which returns to the stub's caller */
static void stub_jump(u16 stub) {
    T += 1;
    call_far(rw(CS371E, (u16)(stub + 3)), rw(CS371E, (u16)(stub + 1)));
}
/* a callf's frame for a driver entry: the argument words (w[0] lowest) and
 * a far return address (call_lifted's) */
static void stub_frame(int n, const u16 *w) {
    for (int i = n - 1; i >= 0; i--) push16(w[i]);
    push16(CS371E); push16(0xFFFF);
}
static u32 stub_done(int n) {
    R.sp = (u16)(R.sp + 2 * n);
    return MK32(R.dx, R.ax);
}

/* Several seats (runtime/seat.h): the driver is the world's -- seat 0's,
 * whose timer interrupt counts the game's ticks.  Another seat's call does
 * not reach it (it would point the world's driver at a song in that seat's
 * own heap, which is not there when the interrupt plays it): it returns 0,
 * the same on every machine, and that seat's sound is heard where the seat
 * is played, from a copy of the driver (seat_sound_call, below) */
static void seat_sound_call(u16 stub, int n, const u16 *w);
static u32 drv_entry(u16 stub, int n, const u16 *w) {
    stub_frame(n, w);
    RTL_ENTRY();
    if (seats_n && seat_cur > 0) {
        R.sp = (u16)(R.sp + 4);          /* the far return address the driver's retf pops */
        R.ax = 0; R.dx = 0;
        T += 1;
        seat_sound_call(stub, n, w);
        return stub_done(n);
    }
    stub_jump(stub);
    return stub_done(n);
}

/* 371E:0083  driver entry 0, init(port, opt): AdLib at port (the detector's
 * word +48h of the device found; 388h), delay loops timed with PIT channel
 * 2, all OPL registers reset, int 08h hooked and the PIT set to 2E9Ah
 * (100 Hz); song and fade state cleared, output muted.  The driver reads
 * its arguments from [sp+8]: the caller (371E:00E5) reserves two words
 * below them that nobody writes (sub sp, 4).  Returns 0 */
u32 rtl_snd_drv_init(u16 port, u16 port_hi, u16 opt_lo, u16 opt_hi) {
    u16 w[4] = { port, port_hi, opt_lo, opt_hi };
    for (int i = 3; i >= 0; i--) push16(w[i]);
    R.sp = (u16)(R.sp - 4);
    push16(CS371E); push16(0xFFFF);
    RTL_ENTRY();
    stub_jump(0x83);
    return stub_done(6);
}
void f_371E_0083(void) { RTL_ENTRY(); stub_jump(0x83); }

/* 371E:0088  driver entry 1, shutdown: int 08h and the PIT rate restored,
 * OPL registers reset */
u32 rtl_snd_drv_shutdown(void) {
    stub_frame(0, NULL);
    RTL_ENTRY();
    stub_jump(0x88);
    return stub_done(0);
}
void f_371E_0088(void) { RTL_ENTRY(); stub_jump(0x88); }

/* 371E:008D  driver entry 2: DX:AX = the driver's name ("V1.3 - AdLib (TM)
 * soundcard"); not used by the game */
u32 rtl_snd_drv_name(void) {
    stub_frame(0, NULL);
    RTL_ENTRY();
    stub_jump(0x8D);
    return stub_done(0);
}
void f_371E_008D(void) { RTL_ENTRY(); stub_jump(0x8D); }

/* 371E:0092  driver entry 3: the tick block the timer interrupt counts in,
 * 16 words: four each counting at 100, 50, 25 and 10 Hz, whatever rate the
 * song sets the PIT to (the game's at DS:902C; g_timer_ticks is word 4) */
u32 rtl_snd_drv_set_ticks_w(u16 off, u16 seg) {
    u16 w[2] = { off, seg };
    stub_frame(2, w);
    RTL_ENTRY();
    stub_jump(0x92);
    return stub_done(2);
}
void f_371E_0092(void) { RTL_ENTRY(); stub_jump(0x92); }

/* 371E:0097  driver entry 4: DX:AX = the tick block; not used by the game */
u32 rtl_snd_drv_get_ticks(void) {
    stub_frame(0, NULL);
    RTL_ENTRY();
    stub_jump(0x97);
    return stub_done(0);
}
void f_371E_0097(void) { RTL_ENTRY(); stub_jump(0x97); }

/* 371E:009C  driver entry 5, output(on): 1 sound on (the channels get
 * their volumes back unless the song is stopped), 0 muted (every channel
 * silenced, the song keeps its place).  Returns the status from before:
 * bit 0 output on, bit 1 song stopped, bit 2 fading in, bit 3 fading out */
u32 rtl_snd_drv_output(u16 on) {
    return drv_entry(0x9C, 1, &on);
}
void f_371E_009C(void) { RTL_ENTRY(); stub_jump(0x9C); }

/* 371E:00A1  driver entry 6, load_song(song, size): silences the channels
 * and takes the song at song (byte 3 its type: 0 nine melodic channels, 1
 * six and the rhythm section, 2 a bank of sound effects; byte 4 timer
 * interrupts per step, word 5 the PIT divisor), sets up the channels; a
 * song stays stopped until entry 9 starts it.  Returns 0, or FFFC for an
 * unknown type or a song not at offset 0 mod 16 */
u32 rtl_snd_drv_load_song_w(u16 off, u16 seg, u16 size) {
    u16 w[3] = { off, seg, size };
    return drv_entry(0xA1, 3, w);
}
void f_371E_00A1(void) { RTL_ENTRY(); stub_jump(0xA1); }

/* 371E:00A6  driver entry 7: DX:AX = the song; not used by the game */
u32 rtl_snd_drv_song(void) {
    stub_frame(0, NULL);
    RTL_ENTRY();
    stub_jump(0xA6);
    return stub_done(0);
}
void f_371E_00A6(void) { RTL_ENTRY(); stub_jump(0xA6); }

/* 371E:00AB  driver entry 8: the song's size; not used by the game */
u32 rtl_snd_drv_song_size(void) {
    stub_frame(0, NULL);
    RTL_ENTRY();
    stub_jump(0xAB);
    return stub_done(0);
}
void f_371E_00AB(void) { RTL_ENTRY(); stub_jump(0xAB); }

/* 371E:00B0  driver entry 9, play(cmd, fade): cmd 4 start the song (fading
 * in over about fade/100 s if fade > 0), 8 fade it out likewise and stop,
 * 2 stop it at once.  Returns the status (entry 5) */
u32 rtl_snd_drv_play(u16 cmd, u16 fade) {
    u16 w[2] = { cmd, fade };
    return drv_entry(0xB0, 2, w);
}
void f_371E_00B0(void) { RTL_ENTRY(); stub_jump(0xB0); }

/* 371E:00B5  driver entry 10: the status (entry 5); not used by the game */
u32 rtl_snd_drv_status(void) {
    stub_frame(0, NULL);
    RTL_ENTRY();
    stub_jump(0xB5);
    return stub_done(0);
}
void f_371E_00B5(void) { RTL_ENTRY(); stub_jump(0xB5); }

/* 371E:00BA  driver entry 11, effect(n, count), with a bank of effects
 * loaded (type 2): n > 0 plays effect n now (count: repeats), n < 0 queues
 * effect -n, 0 stops effects (?); FFFB for a song of another type, FFFD
 * for an unknown effect.  Not used by the game */
u32 rtl_snd_drv_effect(u16 n, u16 count) {
    u16 w[2] = { n, count };
    stub_frame(2, w);
    RTL_ENTRY();
    stub_jump(0xBA);
    return stub_done(2);
}
void f_371E_00BA(void) { RTL_ENTRY(); stub_jump(0xBA); }

/* 371E:00BF  driver entry 12, repeat(counter, n): plays the song from the
 * start, n + 1 times (a bank of effects: FFFB); the word at counter (if not
 * null) is cleared and counts the passes played (entry 6 leaves the count
 * at 65536).  Not used by the game */
u32 rtl_snd_drv_repeat_w(u16 off, u16 seg, u16 n) {
    u16 w[3] = { off, seg, n };
    stub_frame(3, w);
    RTL_ENTRY();
    stub_jump(0xBF);
    return stub_done(3);
}
void f_371E_00BF(void) { RTL_ENTRY(); stub_jump(0xBF); }

/* 371E:00C4  where the stubs jump while no driver is linked: DX:AX = -1 */
u32 rtl_snd_no_driver(void) {
    RTL_ENTRY();
    R.ax = 0xFFFF; R.dx = 0xFFFF;
    T += 3;
    return MK32(R.dx, R.ax);
}
void f_371E_00C4(void) {
    rtl_snd_no_driver();
    R.sp += 4;
}

/* ------------------------------------------------------------------ a seat's own sound */

/* The machine that plays a seat but 0 (the network game's seat, or
 * --view-seat) hears that seat: a copy of the driver -- its code, data and
 * stack, the block at g_snd_drv_mem -- plays the seat's songs into a chip
 * of its own (runtime/adlib.c, chip 1) on a timer of its own.  The copy
 * runs in the emulated memory in the driver's place, for a moment: at each
 * of the world's frames (its timer interrupts until then; the seat's songs
 * lie in its own heap, its seat window, which is put in for it) and at each
 * of the seat's calls.  The memory, the registers, the devices and time are
 * put back afterwards.  So the game, every seat of it, is the same on every
 * machine, whoever hears what (tools/mptest.py --sound). */
#define DRV_LEN 0x1574                   /* the larger driver's allocation (AdLib; the beeper's is 0xE1E) */
static u8 sh_drv[DRV_LEN];               /* the copy, between its runs */
static int sh_on;                        /* it plays (from the seat's joining) */
static u16 sh_pit;                       /* its timer's divisor (PIT channel 0, as its songs set it) */
static u64 sh_next_irq;                  /* T of its next timer interrupt */
static u8 *sh_mem, *sh_hw;               /* the emulated memory and the devices, kept while it runs */

/* its chip hears it two frames late: its timer interrupts of a frame run
 * at the frame's end (or at the seat's turn), and come in time so */
static u64 sh_delay(void) { return (u64)(2 * 14268.0 * insns_per_us); }   /* (a frame: 14268 us, runtime/hw.c) */

static u64 sh_period(void) {
    return (u64)((sh_pit ? sh_pit : 65536) * 1e6 / 1193182.0 * insns_per_us);
}

static u32 drv_lin(void) { return ((u32)g_snd_drv_mem_hi << 4) + g_snd_drv_mem_lo; }

static void sh_run(u16 stub, int n, const u16 *w) {
    u32 lin = drv_lin();
    if (lin + DRV_LEN > MEM_SIZE) return;
    state_export();                      /* the game state the decompiled build keeps in host memory, as it is */
    StateBlock b[64];
    int nb = hw_state_blocks(b);
    u32 hw_len = 0;
    for (int j = 0; j < nb; j++) hw_len += b[j].n;
    if (!sh_mem) sh_mem = malloc(MEM_SIZE);
    if (!sh_hw) sh_hw = malloc(hw_len);
    if (!sh_mem || !sh_hw) return;
    memcpy(sh_mem, M, MEM_SIZE);
    for (int j = 0, k = 0; j < nb; j++) { memcpy(sh_hw + k, b[j].p, b[j].n); k += b[j].n; }
    CPU r = R;
    u64 t = T;
    const u8 *win = seat_window_of(seat_sound_seat());
    if (win) memcpy(M + ((u32)SEAT_HEAP_SEG << 4), win, ((u32)SEAT_HEAP_END - SEAT_HEAP_SEG) << 4);
    memcpy(M + lin, sh_drv, DRV_LEN);
    pit_reload[0] = sh_pit;
    adlib_select(1);
    for (int k = 0; sh_next_irq <= t && k < 50; k++) {   /* its timer interrupts until now */
        adlib_shift((int64_t)(sh_next_irq + sh_delay()) - (int64_t)T);
        R = r;
        do_int(8);
        sh_pit = pit_reload[0];
        sh_next_irq += sh_period();
    }
    if (sh_next_irq <= t) sh_next_irq = t + sh_period();   /* far behind: not all of it */
    if (stub) {                          /* the seat's call */
        adlib_shift((int64_t)sh_delay());
        R = r;
        stub_frame(n, w);
        stub_jump(stub);
        sh_pit = pit_reload[0];
    }
    adlib_shift(0);
    adlib_select(0);
    memcpy(sh_drv, M + lin, DRV_LEN);
    memcpy(M, sh_mem, MEM_SIZE);
    for (int j = 0, k = 0; j < nb; j++) { memcpy(b[j].p, sh_hw + k, b[j].n); k += b[j].n; }
    R = r;
    T = t;
    state_reload();
}

/* the copy is made once the seat has joined, from the world's driver as
 * it is, stopped (the seat's songs come with its calls); its chip is the
 * one heard from then on */
static int sh_start(void) {
    if (sh_on) return 1;
    if (!rb(CS371E, SND_LINKED) || !g_snd_drv_mem) return 0;
    u32 lin = drv_lin();
    if (lin + DRV_LEN > MEM_SIZE) return 0;
    memcpy(sh_drv, M + lin, DRV_LEN);
    sh_pit = pit_reload[0];
    sh_next_irq = T;
    adlib_seat_start();
    sh_on = 1;
    u16 stop[2] = { 2, 0 };
    sh_run(0xB0, 2, stop);
    return 1;
}

/* a frame of the world's (runtime/hw.c): the copy's timer up to now */
void seat_sound_frame(void) {
    int seat = seat_sound_seat();
    if (seat <= 0 || seat_frozen || interp_depth || !adlib_enabled || !seat_joined(seat)) return;
    if (sh_on) sh_run(0, 0, NULL);
    else sh_start();
}
/* the seat's call: to the copy (the seat's first call may come before the
 * world's next frame) */
static void seat_sound_call(u16 stub, int n, const u16 *w) {
    if (!adlib_enabled || seat_cur != seat_sound_seat() || interp_depth) return;
    if (sh_start()) sh_run(stub, n, w);
}

/* ------------------------------------------------------------------ linking */

/* 371E:0024  run the hardware detector loaded at det (resource 99): if its
 * first 36 bytes are the signature at 371E:0000 ("PALLADIX-PSMDETECT\r\n$"
 * and a DOS stub), enter it at offset 24h with this function's frame (BP,
 * DS, SI, DI pushed, BP its frame): it probes the AdLib at 388h and returns
 * through our caller's return address, DX:AX = its device table (records
 * of 50h bytes).  Otherwise DX:AX = 0.  The pointer is normalized to
 * offset 0 first; one with an offset not a multiple of 16 is refused */
static void detect_enter(void) {      /* R.sp at the far return address */
    u16 off = rw(R.ss, (u16)(R.sp + 4)), seg = rw(R.ss, (u16)(R.sp + 6));
    push16(R.bp); R.bp = R.sp;
    push16(R.ds); push16(R.si); push16(R.di);
    R.ax = off; R.es = seg; R.dx = seg;
    T += 9;
    if (R.al & 0xF) goto fail;
    R.ax = logic16(R.ax);
    T += 2;
    if (R.ax != 0) {
        R.cl = 4;
        R.ax = shift16(SH_SHR, R.ax, R.cl);
        R.dx = (u16)(R.dx + R.ax);
        R.es = R.dx;
        T += 4;
    }
    R.di = logic16(0); R.cx = 0x24; R.si = 0;
    R.ax = CS371E; R.ds = R.ax; R.df = 0;
    T += 7;
    while (R.cx != 0) {                  /* repe cmpsb */
        sub8(rb(R.ds, R.si), rb(R.es, R.di), 0);
        R.si++; R.di++; R.cx--;
        T += 1;
        if (!R.zf) break;
    }
    T += 1;
    if (!R.zf) goto fail;
    T += 3;
    call_far(R.es, R.di);                /* push es; push di; retf */
    return;
fail:
    R.ax = 0; R.dx = 0;
    R.di = pop16(); R.si = pop16(); R.ds = pop16(); R.bp = pop16();
    T += 7;
    R.sp += 4;                           /* retf */
}
u32 rtl_snd_detect_w(u16 off, u16 seg) {
    u16 w[2] = { off, seg };
    stub_frame(2, w);                    /* the detector returns through it */
    RTL_ENTRY();
    detect_enter();
    return stub_done(2);
}
void f_371E_0024(void) { RTL_ENTRY(); detect_enter(); }

/* 371E:00C9  every stub back to "jmp far 371E:00C4", the linked flag cleared */
u32 rtl_snd_stubs_reset(void) {
    RTL_ENTRY();
    wb(CS371E, SND_LINKED, 0);
    R.df = 0;
    R.ax = CS371E; R.es = R.ax;
    R.di = SND_STUB0; R.cx = 0xD; R.dx = SND_NONE;
    T += 7;
    do {
        POLL();
        R.di++;                          /* over the EA opcode */
        T += 3;
        ww(R.es, R.di, R.dx); R.di += 2;
        T += 2;
        ww(R.es, R.di, R.ax); R.di += 2;
        T += 1;
    } while (--R.cx);
    T += 1;
    return MK32(R.dx, R.ax);
}
void f_371E_00C9(void) {                 /* near */
    rtl_snd_stubs_reset();
    R.sp += 2;
}

/* 371E:0164  unlink the driver: entry 1 (shutdown), then the stubs back to
 * 371E:00C4 (the game's exit handler 35F8:0376 calls it too) */
u32 rtl_snd_drv_unlink(void) {
    RTL_ENTRY();
    push16(R.ds); push16(R.si); push16(R.di);
    wb(CS371E, SND_LINKED, 0);
    T += 6;
    rtl_snd_drv_shutdown();
    T += 1;
    push16(0x0174); rtl_snd_stubs_reset(); R.sp += 2;
    R.di = pop16(); R.si = pop16(); R.ds = pop16();
    T += 4;
    return MK32(R.dx, R.ax);
}
void f_371E_0164(void) {
    rtl_snd_drv_unlink();
    R.sp += 4;
}

/* 371E:00E5  link the driver loaded at drv (resource 98 or 95; offset a
 * multiple of 16, normalized to 0): unlink a linked one first; if its first
 * 32 bytes are the signature at 371E:0063, point the 13 stubs at the entry
 * offsets that follow it (in its segment) and call entry 0 with
 * (port, opt); a negative result resets the stubs (and the linked flag)
 * without shutting the driver down.  Returns entry 0's result, or FFFE for
 * a bad pointer or signature (stubs reset) */
u32 rtl_snd_drv_link_w(u16 off, u16 seg, u16 port, u16 port_hi, u16 opt_lo, u16 opt_hi) {
    RTL_ENTRY();
    push16(R.bp); R.bp = R.sp;
    push16(R.ds); push16(R.si); push16(R.di);
    R.al = rb(CS371E, SND_LINKED);
    T += 8;
    if (R.al != 0) {
        push16(CS371E);
        T += 2;
        push16(0x00F7); rtl_snd_drv_unlink(); R.sp += 4;
    }
    R.si = off; R.ds = seg;
    T += 3;
    if (R.si & 0xF) goto bad;
    R.ax = R.ds;
    T += 3;
    if (R.ax == 0) goto bad;
    R.si = logic16(R.si);
    T += 2;
    if (R.si != 0) {
        R.si = (u16)(R.si >> 4);
        R.ax = add16(R.ax, R.si, 0);
        T += 6;
        if (R.cf) goto bad;
        R.ds = R.ax;
        R.si = logic16(0);
        T += 2;
    }
    ww(CS371E, SND_DRVSEG, R.ax);
    R.bx = CS371E; R.es = R.bx;
    R.di = SND_SIG; R.cx = 0x10; R.df = 0;
    T += 7;
    while (R.cx != 0) {                  /* repe cmpsw */
        sub16(rw(R.ds, R.si), rw(R.es, R.di), 0);
        R.si += 2; R.di += 2; R.cx--;
        T += 1;
        if (!R.zf) break;
    }
    T += 1;
    if (!R.zf) goto bad;
    R.cx = 0xD;
    T += 1;
    do {                                 /* the entry table: 13 offsets after the signature */
        POLL();
        R.di++;                          /* over the EA opcode */
        T += 2;
        ww(R.es, R.di, rw(R.ds, R.si)); R.si += 2; R.di += 2;
        T += 1;
        ww(R.es, R.di, R.ax); R.di += 2;
        T += 1;
    } while (--R.cx);
    T += 7;
    rtl_snd_drv_init(port, port_hi, opt_lo, opt_hi);
    wb(CS371E, SND_LINKED, (u8)(rb(CS371E, SND_LINKED) + 1));
    T += 4;
    if ((i16)R.ax < 0) {
        T += 1;
        push16(0x0157); rtl_snd_stubs_reset(); R.sp += 2;
        T += 1;
    }
    goto out;
bad:
    T += 1;
    push16(0x015C); rtl_snd_stubs_reset(); R.sp += 2;
    R.ax = 0xFFFE;
    T += 1;
out:
    R.di = pop16(); R.si = pop16(); R.ds = pop16(); R.bp = pop16();
    T += 5;
    return MK32(R.dx, R.ax);
}
void f_371E_00E5(void) {
    rtl_snd_drv_link_w(WARG16(0), WARG16(1), WARG16(2), WARG16(3), WARG16(4), WARG16(5));
    R.sp += 4;
}

/* ------------------------------------------------------------------ module 35F8 */

/* a far call's frame from module 35F8 to a C function: argument words
 * (C order) and the return address, as the machine code pushed them, so
 * the callee's frame lies where it did; pop with call_pop */
static void call_frame(u16 ip, int n, ...) {
    va_list ap;
    u16 w[8];
    va_start(ap, n);
    for (int i = 0; i < n; i++) w[i] = (u16)va_arg(ap, int);
    va_end(ap);
    for (int i = n - 1; i >= 0; i--) push16(w[i]);
    push16(SEGV(0x35F8)); push16(ip);
}
static void call_pop(int n) { R.sp = (u16)(R.sp + 4 + 2 * n); }

/* 35F8:024C  snd_load_sample(id): load song resource id (the room music;
 * LZW-packed, the first word its unpacked size) into a new buffer
 * (g_snd_sample_mem, paragraph-aligned g_snd_sample) and hand it to the
 * driver (entry 6: stopped, snd_play_sample starts it); the packed copy
 * and the 8 KB dictionary are freed.  The DX that the decompiler saw
 * passed to the driver is left over from mem_free: the driver does not
 * read it */
u32 rtl_snd_load_song(u16 id) {
    u16 bp_;
    u32 r;
    RTL_ENTRY();
    push16(R.bp); R.bp = R.sp; bp_ = R.sp;   /* size bp-10, packed bp-8, dictionary bp-4 */
    R.sp = (u16)(R.sp - 0xA);
    push16(R.si);
    R.si = id;
    R.ax = (u16)(bp_ - 10);
    T += 14;
    call_frame(0x0268, 5, id, (u16)(bp_ - 10), R.ss, 0, 2);
    res_read_part(id, LOCP(10), 0, 2, R.ax);       /* the unpacked size */
    call_pop(5);
    LOC16(10) = (u16)(LOC16(10) + 0xF);
    R.ax = 1;
    T += 6;
    call_frame(0x027B, 2, LOC16(10), 1);
    r = mem_calloc(LOC16(10), 1);
    R.ax = LO16(r); R.dx = HI16(r);
    call_pop(2);
    g_snd_sample_mem = MK32(R.dx, R.ax);
    T += 6;
    call_frame(0x0292, 2, g_snd_sample_mem_lo, g_snd_sample_mem_hi);
    r = ptr_align_para(g_snd_sample_mem_lo, g_snd_sample_mem_hi);
    R.ax = LO16(r); R.dx = HI16(r);
    call_pop(2);
    g_snd_sample = MK32(R.dx, R.ax);
    R.ax = 1;
    T += 7;
    call_frame(0x02A6, 2, id, 1);        /* the 1 stays: calloc's second argument */
    R.ax = res_exists(id, R.ax);         /* the packed size */
    R.sp = (u16)(R.sp + 4 + 2);
    push16(R.ax);
    T += 4;
    push16(SEGV(0x35F8)); push16(0x02AE);
    r = mem_calloc(R.ax, 1);
    R.ax = LO16(r); R.dx = HI16(r);
    call_pop(2);
    LOC16(6) = R.dx; LOC16(8) = R.ax;
    R.ax = 0x2000;
    T += 8;
    call_frame(0x02C4, 2, 0x2000, 1);
    r = mem_calloc(0x2000, 1);
    R.ax = LO16(r); R.dx = HI16(r);
    call_pop(2);
    LOC16(2) = R.dx; LOC16(4) = R.ax;
    T += 7;
    call_frame(0x02D9, 3, id, LOC16(8), LOC16(6));
    R.ax = res_read_raw(id, HPTR(MK32(LOC16(6), LOC16(8))), R.ax);
    call_pop(3);
    T += 8;
    call_frame(0x02F5, 6, FPOFF(g_snd_sample), FPSEG(g_snd_sample), LOC16(8), LOC16(6), LOC16(4), LOC16(2));
    rtl_lzw_decode_w(FPOFF(g_snd_sample), FPSEG(g_snd_sample), LOC16(8), LOC16(6), LOC16(4), LOC16(2));
    call_pop(6);
    T += 4;
    call_frame(0x0303, 2, LOC16(4), LOC16(2));
    R.ax = mem_free(LOC16(4), LOC16(2));
    call_pop(2);
    T += 4;
    call_frame(0x0311, 2, LOC16(8), LOC16(6));
    R.ax = mem_free(LOC16(8), LOC16(6));
    call_pop(2);
    R.ax = (u16)(LOC16(10) + 0xFFF1);
    T += 7;
    rtl_snd_drv_load_song_w(FPOFF(g_snd_sample), FPSEG(g_snd_sample), R.ax);
    R.si = pop16();
    R.sp = bp_; R.bp = pop16();
    T += 5;
    return MK32(R.dx, R.ax);
}
void f_35F8_024C(void) {
    rtl_snd_load_song(WARG16(0));
    R.sp += 4;
}

/* 35F8:058F  snd_room_music_stop(room, owner), on leaving for room (of
 * owner): records the room in g_snd_room_off and stops and frees the room
 * song (if one is loaded) -- unless the music goes on: the player's own
 * transmitter map (19h) or save room (3Ah), his office (1) unless the room
 * before was none or the planner (1Fh), the sub-screens 3Eh, 3Dh, 0Dh and
 * 40h, the estate agent (0Ah) or another player's room when a room is
 * recorded, and room 0 after 3Dh, 0Dh or 40h.  The DX that the decompiler
 * saw passed through to snd_stop_sample is not read by the driver */
u32 rtl_snd_room_music_stop(u16 room, u16 owner) {
    RTL_ENTRY();
    push16(R.bp); R.bp = R.sp;
    push16(R.si); push16(R.di);
    R.di = owner; R.si = room;
    T += 8;
    if (owner != g_player) goto others;
    T += 2;
    if (room == 0x19) { T += 1; goto keep; }
    T += 2;
    if (room == 0x3A) { T += 1; goto keep; }
    T += 2;
    if (room != 1) goto others;
    T += 2;
    if (g_snd_room_off == 0) goto others;
    T += 2;
    if (g_snd_room_off != 0x1F) goto keep;
others:
    T += 2;
    if (room == 0x3E) goto keep;
    T += 2;
    if (room == 0xA) {
        T += 2;
        if (g_snd_room_off != 0) goto keep;
    }
    T += 2;
    if (owner == 0) goto screens;
    T += 2;
    if (owner == g_player) goto screens;
    T += 2;
    if (room == 0) goto screens;
    T += 2;
    if (g_snd_room_off != 0) goto keep;
screens:
    T += 2;
    if (room == 0x3D) goto keep;
    T += 2;
    if (room == 0xD) goto keep;
    T += 2;
    if (room == 0x40) goto keep;
    T += 2;
    if (room != 0) goto stop;
    T += 2;
    if (g_snd_room_off == 0x3D) goto keep;
    T += 2;
    if (g_snd_room_off == 0xD) goto keep;
    T += 2;
    if (g_snd_room_off != 0x40) goto stop;
keep:
    g_snd_room_off = room;
    T += 2;
    goto out;
stop:
    g_snd_room_off = room;
    T += 3;
    if (g_snd_sample_res != 0) {
        push16(SEGV(0x35F8));
        T += 2;
        push16(0x0628); snd_stop_sample(); R.sp += 4;
        push16(SEGV(0x35F8));
        T += 2;
        push16(0x062C); snd_free_sample(); R.sp += 4;
        g_snd_sample_res = 0;
        T += 1;
    }
out:
    R.di = pop16(); R.si = pop16(); R.bp = pop16();
    T += 4;
    return MK32(R.dx, R.ax);
}
void f_35F8_058F(void) {
    rtl_snd_room_music_stop(WARG16(0), WARG16(1));
    R.sp += 4;
}
