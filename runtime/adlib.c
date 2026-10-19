/* AdLib (YM3812 / OPL2) on ports 388h/389h, backed by Nuked-OPL3.
 *
 * Samples are rendered up to the current virtual time before every register
 * write, so timing is exact relative to the game -- but no write closer than
 * WRITE_GAP_US to the one before (below).  The OPL timers (used by the sound
 * driver's detection routine) are emulated here from T. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "runtime.h"
#include "../third_party/nuked-opl3/opl3.h"

#define RATE 44100
#define WRITE_GAP_US 28.0          /* time per register write on a real AdLib (adlib_write) */

/* Two chips: 0 is the game's (the world's), 1 a seat's own sound on the
 * machine that plays that seat (src/rtl_snd.c: a copy of the driver plays
 * that seat's songs into it).  Register writes go to the selected one; the
 * audible one is rendered and heard. */
typedef struct {
    opl3_chip chip;
    u8 addr;
    u8 reg_t1, reg_t2, reg_ctl;
    u64 t1_start, t2_start;        /* T when timer was started */
    int t1_run, t2_run;
    u8 status;
    u64 samples_done;              /* samples rendered since start */
    u64 next_write_T;              /* the sound: earliest time of the next register write */
    u8 regs[256];                  /* the last value written to each register */
} Opl;
static Opl opl[2];
static Opl *cur = &opl[0];
static int audible;                /* the chip heard */
static int16_t *buf;               /* stereo interleaved */
static size_t buf_n, buf_cap;
static FILE *wav;
static u32 wav_bytes;
int adlib_enabled = 1;

int adlib_state_blocks(StateBlock *b) {
    int n = 0;
    Opl *o = &opl[0];
    STATE_BLOCK(b, n, o->chip); STATE_BLOCK(b, n, o->addr); STATE_BLOCK(b, n, o->reg_t1); STATE_BLOCK(b, n, o->reg_t2);
    STATE_BLOCK(b, n, o->reg_ctl); STATE_BLOCK(b, n, o->t1_start); STATE_BLOCK(b, n, o->t2_start);
    STATE_BLOCK(b, n, o->t1_run); STATE_BLOCK(b, n, o->t2_run); STATE_BLOCK(b, n, o->status);
    STATE_BLOCK(b, n, o->regs);
    return n;
}

void adlib_init(void) { OPL3_Reset(&opl[0].chip, RATE); }

static u64 samples_due(u64 t) {
    return (u64)((double)t / insns_per_us * RATE / 1e6);
}

/* render the audible chip's audio up to virtual time t */
static void render_to(Opl *o, u64 t) {
    if (difftest_active || !adlib_enabled || o != &opl[audible]) return;
    u64 due = samples_due(t);
    if (due <= o->samples_done) return;
    u64 n = due - o->samples_done;
    if (n > RATE) { o->samples_done = due - RATE; n = RATE; }   /* never render >1s at once */
    if (buf_n + n * 2 > buf_cap) {
        buf_cap = (buf_n + n * 2) * 2;
        buf = realloc(buf, buf_cap * sizeof *buf);
    }
    for (u64 i = 0; i < n; i++) {
        OPL3_GenerateResampled(&o->chip, buf + buf_n);
        buf_n += 2;
    }
    o->samples_done = due;
}

/* register writes to chip k from now on (0: the game's) */
void adlib_select(int k) { cur = &opl[k ? 1 : 0]; }
/* chip 1 hears its writes d later than T (a timer interrupt of the seat's
 * driver copy run late, at its own time) */
static int64_t shift1;
void adlib_shift(int64_t d) { shift1 = d; }
/* chip 1 starts with the game's chip's registers (the driver copied with
 * it expects them; the chip itself holds pointers into itself, it is set
 * up anew) and is the one heard from now on */
void adlib_seat_start(void) {
    Opl *o = &opl[1];
    memset(o, 0, sizeof *o);
    OPL3_Reset(&o->chip, RATE);
    for (int r = 1; r < 256; r++)
        if (r < 2 || r > 4) { o->regs[r] = opl[0].regs[r]; OPL3_WriteReg(&o->chip, (u16)r, o->regs[r]); }
    o->samples_done = samples_due(T);
    audible = 1;
}
void adlib_sync(void) { render_to(&opl[audible], T); }

/* hand rendered audio to the host (SDL queue or WAV file) */
void host_audio_push(const int16_t *s, size_t frames);
void adlib_flush(void) {
    adlib_sync();
    if (!buf_n) return;
    if (wav) { fwrite(buf, 2, buf_n, wav); wav_bytes += buf_n * 2; }
    else host_audio_push(buf, buf_n / 2);
    buf_n = 0;
}

static void update_status(void) {
    /* timer 1: 80 us per tick, timer 2: 320 us per tick, count up to 256 */
    Opl *o = cur;
    double us_now = T / insns_per_us;
    if (o->t1_run && !(o->reg_ctl & 0x40)) {
        double per = (256 - o->reg_t1) * 80.0;
        if (us_now - o->t1_start / insns_per_us >= per) o->status |= 0xC0;
    }
    if (o->t2_run && !(o->reg_ctl & 0x20)) {
        double per = (256 - o->reg_t2) * 320.0;
        if (us_now - o->t2_start / insns_per_us >= per) o->status |= 0xA0;
    }
}

u8 adlib_read(u16 port) {
    if (port != 0x388) return 0xFF;
    update_status();
    return cur->status | 0x06;     /* OPL2 reads back 0x06 in the low bits */
}

void adlib_write(u16 port, u8 v) {
    Opl *o = cur;
    if (port == 0x388) { o->addr = v; return; }
    if (port != 0x389) return;
    static int log = -1;
    if (log < 0) log = getenv("OPL_LOG") != NULL;
    switch (o->addr) {
    case 0x02: o->reg_t1 = v; break;
    case 0x03: o->reg_t2 = v; break;
    case 0x04:
        if (v & 0x80) { o->status = 0; break; }
        o->reg_ctl = v;
        if ((v & 1) && !o->t1_run) o->t1_start = T;
        if ((v & 2) && !o->t2_run) o->t2_start = T;
        o->t1_run = v & 1; o->t2_run = (v >> 1) & 1;
        break;
    }
    /* The chip needs time after each write: 3.3 us after the address, 23 us
     * after the data.  The driver waits by reading the status port as often
     * as 4 and 28 PIT ticks take (it times 256 reads with PIT channel 2 at
     * start-up), about 28 us per register on the cards it was made for.  The
     * emulated PIT gives that calibration a wrong count (the counter's phase
     * since boot, not since the gate opened), so the driver's writes come
     * ~5 us apart here.  The driver strikes a note again with a key-off and
     * the key-on right after it; the chip reads the key bit once per sample
     * (20 us), so here it mostly missed the key-off and did not strike the
     * note again -- ~400 of ~700 such notes in the title music.  The game's
     * time stays as it is (tests and recordings depend on it): only the sound
     * hears each write at least WRITE_GAP_US after the one before, as the
     * card did. */
    u64 at = T;
    if (o == &opl[1]) at = (u64)((int64_t)at + shift1);
    if (!difftest_active && adlib_enabled) {
        if (at < o->next_write_T) at = o->next_write_T;
        render_to(o, at);
        o->next_write_T = at + (u64)(WRITE_GAP_US * insns_per_us);
    }
    if (log) fprintf(stderr, "OPL%d T=%llu %02X=%02X heard T=%llu\n", (int)(o - opl), (unsigned long long)T, o->addr, v, (unsigned long long)at);
    o->regs[o->addr] = v;
    OPL3_WriteReg(&o->chip, o->addr, v);
}

static void wav_header(FILE *f, u32 data) {
    u32 v;
    fwrite("RIFF", 1, 4, f); v = 36 + data; fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f); v = 16; fwrite(&v, 4, 1, f);
    u16 fmt = 1, ch = 2, bits = 16, align = 4; u32 rate = RATE, bps = RATE * 4;
    fwrite(&fmt, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&rate, 4, 1, f); fwrite(&bps, 4, 1, f);
    fwrite(&align, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
}
static void wav_close(void) {
    if (!wav) return;
    adlib_flush();
    fseek(wav, 0, SEEK_SET);
    wav_header(wav, wav_bytes);
    fclose(wav);
    wav = NULL;
}
void adlib_wav_open(const char *path) {
    wav = fopen(path, "wb");
    if (!wav) { perror(path); return; }
    wav_header(wav, 0);
    atexit(wav_close);
}
