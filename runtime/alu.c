/* 8086 ALU operations that are too large to inline. */
#include <stdlib.h>
#include "cpu.h"
void dump_state(const char *why);

u8 shift8(int op, u8 v, u8 cnt) {
    cnt &= 0x1F;
    if (!cnt) return v;
    u32 r = v;
    switch (op) {
    case SH_ROL:
        for (int i = 0; i < cnt; i++) r = ((r << 1) | (r >> 7)) & 0xFF;
        R.cf = r & 1; R.of = ((r >> 7) ^ R.cf) & 1; return (u8)r;
    case SH_ROR:
        for (int i = 0; i < cnt; i++) r = ((r >> 1) | (r << 7)) & 0xFF;
        R.cf = r >> 7; R.of = ((r >> 7) ^ (r >> 6)) & 1; return (u8)r;
    case SH_RCL:
        for (int i = 0; i < cnt; i++) { u32 c = R.cf; R.cf = (r >> 7) & 1; r = ((r << 1) | c) & 0xFF; }
        R.of = ((r >> 7) ^ R.cf) & 1; return (u8)r;
    case SH_RCR:
        for (int i = 0; i < cnt; i++) { u32 c = R.cf; R.cf = r & 1; r = (r >> 1) | (c << 7); }
        R.of = ((r >> 7) ^ (r >> 6)) & 1; return (u8)r;
    case SH_SHL: case SH_SAL:
        R.cf = cnt <= 8 ? (r >> (8 - cnt)) & 1 : 0;
        r = (r << cnt) & 0xFF;
        R.of = ((r >> 7) ^ R.cf) & 1; R.af = 0; SZP8(r); return (u8)r;
    case SH_SHR:
        R.cf = cnt <= 8 ? (r >> (cnt - 1)) & 1 : 0;
        R.of = cnt == 1 ? (v >> 7) & 1 : 0;
        r = cnt < 8 ? r >> cnt : 0; R.af = 0; SZP8(r); return (u8)r;
    case SH_SAR: {
        s32 sv = (s8)v;
        int c = cnt > 8 ? 8 : cnt;
        R.cf = (sv >> (c - 1)) & 1;
        r = (u8)(sv >> c); R.of = 0; R.af = 0; SZP8(r); return (u8)r;
    }
    }
    return v;
}

u16 shift16(int op, u16 v, u8 cnt) {
    cnt &= 0x1F;
    if (!cnt) return v;
    u32 r = v;
    switch (op) {
    case SH_ROL:
        for (int i = 0; i < cnt; i++) r = ((r << 1) | (r >> 15)) & 0xFFFF;
        R.cf = r & 1; R.of = ((r >> 15) ^ R.cf) & 1; return (u16)r;
    case SH_ROR:
        for (int i = 0; i < cnt; i++) r = ((r >> 1) | (r << 15)) & 0xFFFF;
        R.cf = r >> 15; R.of = ((r >> 15) ^ (r >> 14)) & 1; return (u16)r;
    case SH_RCL:
        for (int i = 0; i < cnt; i++) { u32 c = R.cf; R.cf = (r >> 15) & 1; r = ((r << 1) | c) & 0xFFFF; }
        R.of = ((r >> 15) ^ R.cf) & 1; return (u16)r;
    case SH_RCR:
        for (int i = 0; i < cnt; i++) { u32 c = R.cf; R.cf = r & 1; r = (r >> 1) | (c << 15); }
        R.of = ((r >> 15) ^ (r >> 14)) & 1; return (u16)r;
    case SH_SHL: case SH_SAL:
        R.cf = cnt <= 16 ? (r >> (16 - cnt)) & 1 : 0;
        r = (r << cnt) & 0xFFFF;
        R.of = ((r >> 15) ^ R.cf) & 1; R.af = 0; SZP16(r); return (u16)r;
    case SH_SHR:
        R.cf = cnt <= 16 ? (r >> (cnt - 1)) & 1 : 0;
        R.of = cnt == 1 ? (v >> 15) & 1 : 0;
        r = cnt < 16 ? r >> cnt : 0; R.af = 0; SZP16(r); return (u16)r;
    case SH_SAR: {
        s32 sv = (s16)v;
        int c = cnt > 16 ? 16 : cnt;
        R.cf = (sv >> (c - 1)) & 1;
        r = (u16)(sv >> c); R.of = 0; R.af = 0; SZP16(r); return (u16)r;
    }
    }
    return v;
}

void mul8(u8 v) { R.ax = (u16)R.al * v; R.cf = R.of = R.ah != 0; SZP8(R.al); }
void imul8(u8 v) {
    s16 r = (s16)(s8)R.al * (s8)v; R.ax = (u16)r;
    R.cf = R.of = r != (s8)r; SZP8(R.al);
}
void mul16(u16 v) {
    u32 r = (u32)R.ax * v; R.ax = (u16)r; R.dx = r >> 16;
    R.cf = R.of = R.dx != 0; SZP16(R.ax);
}
void imul16(u16 v) {
    s32 r = (s32)(s16)R.ax * (s16)v; R.ax = (u16)r; R.dx = (u32)r >> 16;
    R.cf = R.of = r != (s16)r; SZP16(R.ax);
}
void div8(u8 v) {
    if (!v) { do_int(0); return; }
    u16 q = R.ax / v, rem = R.ax % v;
    if (q > 0xFF) { do_int(0); return; }
    R.al = (u8)q; R.ah = (u8)rem;
}
void idiv8(u8 v) {
    if (!v) { do_int(0); return; }
    s16 a = (s16)R.ax; s16 q = a / (s8)v, rem = a % (s8)v;
    if (q > 127 || q < -128) { do_int(0); return; }
    R.al = (u8)q; R.ah = (u8)rem;
}
void div16(u16 v) {
    if (!v) { if (getenv("TRACE_DIV0")) dump_state("div16 by zero"); do_int(0); return; }
    u32 a = ((u32)R.dx << 16) | R.ax; u32 q = a / v, rem = a % v;
    if (q > 0xFFFF) { do_int(0); return; }
    R.ax = (u16)q; R.dx = (u16)rem;
}
void idiv16(u16 v) {
    if (!v) { if (getenv("TRACE_DIV0")) dump_state("idiv16 by zero"); do_int(0); return; }
    s32 a = (s32)(((u32)R.dx << 16) | R.ax);
    if (a == INT32_MIN && (s16)v == -1) { do_int(0); return; }
    s32 q = a / (s16)v, rem = a % (s16)v;
    if (q > 32767 || q < -32768) { do_int(0); return; }
    R.ax = (u16)q; R.dx = (u16)rem;
}
void daa(void) {
    u8 al = R.al; int cf = R.cf;
    if ((al & 0xF) > 9 || R.af) { R.al += 6; R.af = 1; } else R.af = 0;
    if (al > 0x99 || cf) { R.al += 0x60; R.cf = 1; } else R.cf = 0;
    SZP8(R.al);
}
void das(void) {
    u8 al = R.al; int cf = R.cf;
    if ((al & 0xF) > 9 || R.af) { R.al -= 6; R.af = 1; } else R.af = 0;
    if (al > 0x99 || cf) { R.al -= 0x60; R.cf = 1; } else R.cf = 0;
    SZP8(R.al);
}
void aaa(void) {
    if ((R.al & 0xF) > 9 || R.af) { R.al += 6; R.ah++; R.af = R.cf = 1; } else R.af = R.cf = 0;
    R.al &= 0xF;
}
void aas(void) {
    if ((R.al & 0xF) > 9 || R.af) { R.al -= 6; R.ah--; R.af = R.cf = 1; } else R.af = R.cf = 0;
    R.al &= 0xF;
}
void aam(u8 base) {
    if (!base) { do_int(0); return; }
    R.ah = R.al / base; R.al = R.al % base; SZP8(R.al);
}
void aad(u8 base) { R.al = (u8)(R.ah * base + R.al); R.ah = 0; SZP8(R.al); }
