/* Emulated PC hardware: PIC, PIT, VGA (mode 13h), keyboard controller, speaker.
 * Timing is driven by the virtual instruction clock T. */
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "runtime.h"
#include "seat.h"

u64 T;
u64 next_event_T;
int logical_time;
u64 clock_entries_n, clock_io_n, clock_extra;
double insns_per_us = 10.0;          /* ~10 MIPS virtual CPU */

/* ---------------- PIC ---------------- */
static u8 pic_mask = 0xB8;           /* timer, keyboard, cascade enabled */
static u8 pic_irr, pic_isr;
volatile int irq_pending;

static void pic_update(void) {
    u8 ready = pic_irr & ~pic_mask;
    /* an IRQ is deliverable only if no equal/higher-priority one is in service */
    int ok = 0;
    for (int i = 0; i < 8; i++) {
        if (pic_isr & (1 << i)) break;
        if (ready & (1 << i)) { ok = 1; break; }
    }
    irq_pending = ok;
}
void pic_raise(int irq) { pic_irr |= 1 << irq; pic_update(); }

/* Returns the vector to deliver, or -1.  Marks it in service. */
int pic_ack(void) {
    u8 ready = pic_irr & ~pic_mask;
    for (int i = 0; i < 8; i++) {
        if (pic_isr & (1 << i)) break;
        if (ready & (1 << i)) {
            pic_irr &= ~(1 << i);
            pic_isr |= 1 << i;
            pic_update();
            return 8 + i;
        }
    }
    return -1;
}
static void pic_eoi(void) {
    for (int i = 0; i < 8; i++)
        if (pic_isr & (1 << i)) { pic_isr &= ~(1 << i); break; }
    pic_update();
}

/* Deliver a pending IRQ to code running natively (or via do_int). */
void irq_poll(void) {
    if (!irq_pending || !R.ifl || difftest_active || seat_frozen) return;
    int v = pic_ack();
    if (v >= 0) do_int((u8)v);
}

/* ---------------- PIT ---------------- */
u16 pit_reload[3] = {0, 0, 0};       /* 0 means 65536 */
static u8 pit_mode[3], pit_access[3], pit_flip[3];
static u16 pit_latch[3]; static int pit_latched[3];
static u64 pit0_next_T;
u8 port61;

static double pit_period_us(int ch) {
    u32 r = pit_reload[ch] ? pit_reload[ch] : 65536;
    return r / 1.193182;
}
static u64 us_to_T(double us) { return (u64)(us * insns_per_us); }

static u16 pit_count_now(int ch) {
    double per = pit_period_us(ch);
    double t_us = T / insns_per_us;
    double ph = fmod(t_us, per) / per;
    u32 r = pit_reload[ch] ? pit_reload[ch] : 65536;
    return (u16)(r - (u32)(ph * r));
}

/* ---------------- VGA ---------------- */
u8 vga_dac[256][3];
static u8 dac_widx, dac_ridx, dac_wsub, dac_rsub;
static u8 seq_idx, gc_idx, crtc_idx;
extern u8 vga_seq[8], vga_gc[16], vga_crtc[32], vga_attr[32];
extern u8 vga_attr_idx, vga_attr_flip;
#define seq_regs vga_seq
#define gc_regs vga_gc
#define crtc_regs vga_crtc
#define attr_flip vga_attr_flip
#define attr_idx vga_attr_idx
int vga_mode = 3;
#define FRAME_US 14268.0               /* 70.086 Hz */
#define VRETRACE_US 1200.0
static u64 frame_count;
static u64 next_frame_T;
u64 frame_no(void) { return frame_count; }

u8 vga_status_read(void) {
    if (difftest_active) { u32 s = difftest_port_seq++; return ((s >> 2) & 1) ? 0x09 : ((s & 1) ? 0x01 : 0); }
    double t = fmod(T / insns_per_us, FRAME_US);
    u8 v = 0;
    if (t < VRETRACE_US) v |= 0x08 | 0x01;
    else if (fmod(t, 31.78) < 6.0) v |= 0x01;
    attr_flip = 0;
    time_advance(2);
    return v;
}

/* ---------------- keyboard controller ---------------- */
u8 kbd_port60;

u32 port_hist_out[0x10000], port_hist_in[0x10000];
static void port_report(void) {
    if (!getenv("PORT_STATS")) return;
    for (int p = 0; p < 0x10000; p++)
        if (port_hist_out[p] || port_hist_in[p]) fprintf(stderr, "port %04X: out %u in %u\n", p, port_hist_out[p], port_hist_in[p]);
}
__attribute__((constructor)) static void port_report_init(void) { atexit(port_report); }

/* fast room changes (runtime.h fast_rooms): while the host -- alone the player -- changes
 * rooms, the sound driver starting the room's music writes the AdLib chip's registers and
 * waits after each by reading its status port -- on the original machine some 7000
 * accesses, at the logical clock's price (10 each) two seconds: with several humans the
 * world stood still for everyone (runtime/seat.c), alone the player waited.  There they cost
 * a tenth: time still goes on (the chip's timers run on it), only faster.  Without the
 * option, and at any other time, the original's price. */
static inline void port_clock(u16 p) {
    if ((p == 0x388 || p == 0x389) && fast_rooms() && (seat_humans ? seat_cur == 0 : seat_cur <= 0) && seat_disk_busy()) time_advance(1);
    else clock_io(p);
}

u8 port_in8(u16 p) {
    port_clock(p);
    port_hist_in[p]++;
    switch (p) {
    case 0x20: return pic_irr;
    case 0x21: return pic_mask;
    case 0x40: case 0x41: case 0x42: {
        int ch = p - 0x40;
        u16 c = pit_latched[ch] ? pit_latch[ch] : pit_count_now(ch);
        u8 v;
        if (pit_access[ch] == 1) v = (u8)c;
        else if (pit_access[ch] == 2) v = c >> 8;
        else { v = pit_flip[ch] ? c >> 8 : (u8)c; pit_flip[ch] ^= 1; if (!pit_flip[ch]) pit_latched[ch] = 0; }
        return v;
    }
    case 0x60: return kbd_port60;
    case 0x61: return port61 ^ ((T >> 6) & 0x10);   /* refresh bit toggles */
    case 0x64: return 0x1C;
    case 0x201: return 0xFF;               /* no joystick */
    case 0x388: case 0x389: return adlib_read(p);
    case 0x3C4: return seq_idx;
    case 0x3C5: return seq_regs[seq_idx & 7];
    case 0x3C7: return 3;
    case 0x3C8: return dac_widx;
    case 0x3C9: { u8 v = vga_dac[dac_ridx][dac_rsub]; if (++dac_rsub == 3) { dac_rsub = 0; dac_ridx++; } return v; }
    case 0x3CC: return 0x63;
    case 0x3CE: return gc_idx;
    case 0x3CF: return gc_regs[gc_idx & 15];
    case 0x3D4: return crtc_idx;
    case 0x3D5: return crtc_regs[crtc_idx & 31];
    case 0x3DA: case 0x3BA: return vga_status_read();
    }
    return 0xFF;
}
u16 port_in16(u16 p) { return port_in8(p) | (port_in8(p + 1) << 8); }

void port_out8(u16 p, u8 v) {
    port_clock(p);
    port_hist_out[p]++;
    switch (p) {
    case 0x20: if ((v & 0xE0) == 0x20 || v == 0x20) pic_eoi(); return;
    case 0x21: pic_mask = v; pic_update(); return;
    case 0x40: case 0x41: case 0x42: {
        int ch = p - 0x40;
        if (pit_access[ch] == 1) pit_reload[ch] = (pit_reload[ch] & 0xFF00) | v;
        else if (pit_access[ch] == 2) pit_reload[ch] = (pit_reload[ch] & 0xFF) | (v << 8);
        else {
            if (!pit_flip[ch]) pit_reload[ch] = (pit_reload[ch] & 0xFF00) | v;
            else pit_reload[ch] = (pit_reload[ch] & 0xFF) | (v << 8);
            pit_flip[ch] ^= 1;
        }
        if (ch == 0 && !pit_flip[0]) { pit0_next_T = T + us_to_T(pit_period_us(0)); runtime_sched(); }
        return;
    }
    case 0x43: {
        int ch = v >> 6;
        if (ch == 3) return;
        if (((v >> 4) & 3) == 0) { pit_latch[ch] = pit_count_now(ch); pit_latched[ch] = 1; pit_flip[ch] = 0; return; }
        pit_access[ch] = (v >> 4) & 3; pit_mode[ch] = (v >> 1) & 7; pit_flip[ch] = 0;
        return;
    }
    case 0x61: port61 = v; return;
    case 0x388: case 0x389: adlib_write(p, v); return;
    case 0x3C4: seq_idx = v; return;
    case 0x3C5: seq_regs[seq_idx & 7] = v; return;
    case 0x3C7: dac_ridx = v; dac_rsub = 0; return;
    case 0x3C8: dac_widx = v; dac_wsub = 0; return;
    case 0x3C9: vga_dac[dac_widx][dac_wsub] = v & 0x3F; if (++dac_wsub == 3) { dac_wsub = 0; dac_widx++; } return;
    case 0x3CE: gc_idx = v; return;
    case 0x3CF: gc_regs[gc_idx & 15] = v; return;
    case 0x3D4: crtc_idx = v; return;
    case 0x3D5: crtc_regs[crtc_idx & 31] = v; return;
    case 0x3C0:
        if (!attr_flip) attr_idx = v & 0x1F; else vga_attr[attr_idx] = v;
        attr_flip ^= 1; return;
    }
}
void port_out16(u16 p, u16 v) { port_out8(p, (u8)v); port_out8(p + 1, v >> 8); }

/* ---------------- scheduling ---------------- */
static u64 next_host_T;

int hw_state_blocks(StateBlock *b) {
    int n = 0;
    STATE_BLOCK(b, n, next_event_T); STATE_BLOCK(b, n, irq_pending);
    STATE_BLOCK(b, n, pic_mask); STATE_BLOCK(b, n, pic_irr); STATE_BLOCK(b, n, pic_isr);
    STATE_BLOCK(b, n, pit_reload); STATE_BLOCK(b, n, pit_mode); STATE_BLOCK(b, n, pit_access);
    STATE_BLOCK(b, n, pit_flip); STATE_BLOCK(b, n, pit_latch); STATE_BLOCK(b, n, pit_latched);
    STATE_BLOCK(b, n, pit0_next_T); STATE_BLOCK(b, n, port61);
    STATE_BLOCK(b, n, vga_dac); STATE_BLOCK(b, n, dac_widx); STATE_BLOCK(b, n, dac_ridx);
    STATE_BLOCK(b, n, dac_wsub); STATE_BLOCK(b, n, dac_rsub);
    STATE_BLOCK(b, n, seq_idx); STATE_BLOCK(b, n, gc_idx); STATE_BLOCK(b, n, crtc_idx);
    STATE_BLOCK(b, n, vga_mode); STATE_BLOCK(b, n, frame_count); STATE_BLOCK(b, n, next_frame_T);
    STATE_BLOCK(b, n, kbd_port60); STATE_BLOCK(b, n, next_host_T);
    STATE_BLOCK(b, n, clock_entries_n); STATE_BLOCK(b, n, clock_io_n); STATE_BLOCK(b, n, clock_extra);
    return n;
}

/* For the seats (runtime/seat.c): what belongs to one seat -- the palette
 * and the VGA register indexes -- and what to the world, which only seat 0
 * may move on (timers, interrupt controller, time, the clock counters). */
int hw_seat_blocks(StateBlock *b) {
    int n = 0;
    STATE_BLOCK(b, n, vga_dac); STATE_BLOCK(b, n, dac_widx); STATE_BLOCK(b, n, dac_ridx);
    STATE_BLOCK(b, n, dac_wsub); STATE_BLOCK(b, n, dac_rsub);
    STATE_BLOCK(b, n, seq_idx); STATE_BLOCK(b, n, gc_idx); STATE_BLOCK(b, n, crtc_idx);
    STATE_BLOCK(b, n, vga_mode);
    return n;
}
int hw_world_blocks(StateBlock *b) {
    int n = 0;
    STATE_BLOCK(b, n, next_event_T); STATE_BLOCK(b, n, irq_pending);
    STATE_BLOCK(b, n, pic_mask); STATE_BLOCK(b, n, pic_irr); STATE_BLOCK(b, n, pic_isr);
    STATE_BLOCK(b, n, pit_reload); STATE_BLOCK(b, n, pit_mode); STATE_BLOCK(b, n, pit_access);
    STATE_BLOCK(b, n, pit_flip); STATE_BLOCK(b, n, pit_latch); STATE_BLOCK(b, n, pit_latched);
    STATE_BLOCK(b, n, pit0_next_T); STATE_BLOCK(b, n, port61);
    STATE_BLOCK(b, n, frame_count); STATE_BLOCK(b, n, next_frame_T);
    STATE_BLOCK(b, n, kbd_port60); STATE_BLOCK(b, n, next_host_T);
    STATE_BLOCK(b, n, clock_entries_n); STATE_BLOCK(b, n, clock_io_n); STATE_BLOCK(b, n, clock_extra);
    return n;
}

void runtime_sched(void) {
    u64 n = next_frame_T;
    if (pit0_next_T < n) n = pit0_next_T;
    if (next_host_T < n) n = next_host_T;
    next_event_T = n;
}

void hw_init(void) {
    pit0_next_T = us_to_T(pit_period_us(0));
    next_frame_T = us_to_T(FRAME_US);
    next_host_T = us_to_T(5000);
    runtime_sched();
}

void runtime_events(void) {
    if (difftest_active || seat_frozen) return;   /* a seat other than 0 runs: the world's time stands */
    while (T >= next_event_T) {
        if (T >= pit0_next_T) {
            pit0_next_T += us_to_T(pit_period_us(0));
            if (pit0_next_T <= T) pit0_next_T = T + 1;
            pic_raise(0);
        }
        if (T >= next_frame_T) {
            next_frame_T += us_to_T(FRAME_US);
            frame_count++;
            host_present();
            if (seats_n) seat_sound_frame();
            if (seats_n > 1) seat_disk_pass();   /* seat 0 loads: the others go on (seat.c) */
        }
        if (T >= next_host_T) {
            next_host_T += us_to_T(5000);
            adlib_flush();
            host_poll();
            host_sleep_until_T(T);
        }
        runtime_sched();
    }
}

/* HLT: skip ahead to the next event */
void runtime_halt_wait(void) {
    if (difftest_active) { difftest_abort("halt/wait"); return; }
    time_skip_to(next_event_T);
    runtime_events();
}
