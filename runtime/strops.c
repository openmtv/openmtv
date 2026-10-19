/* String instructions, shared by the interpreter and recompiled code.
 * rep: 0, 0xF3 (REP/REPE) or 0xF2 (REPNE). */
#include "runtime.h"

void string_op_ex(u8 op, int rep, u16 src_seg) {
    int d = R.df ? -1 : 1;
    for (;;) {
        if (rep && R.cx == 0) break;
        switch (op) {
        case 0xA4: wb(R.es, R.di, rb(src_seg, R.si)); R.si += d; R.di += d; break;
        case 0xA5: ww(R.es, R.di, rw(src_seg, R.si)); R.si += 2 * d; R.di += 2 * d; break;
        case 0xA6: sub8(rb(src_seg, R.si), rb(R.es, R.di), 0); R.si += d; R.di += d; break;
        case 0xA7: sub16(rw(src_seg, R.si), rw(R.es, R.di), 0); R.si += 2 * d; R.di += 2 * d; break;
        case 0xAA: wb(R.es, R.di, R.al); R.di += d; break;
        case 0xAB: ww(R.es, R.di, R.ax); R.di += 2 * d; break;
        case 0xAC: R.al = rb(src_seg, R.si); R.si += d; break;
        case 0xAD: R.ax = rw(src_seg, R.si); R.si += 2 * d; break;
        case 0xAE: sub8(R.al, rb(R.es, R.di), 0); R.di += d; break;
        case 0xAF: sub16(R.ax, rw(R.es, R.di), 0); R.di += 2 * d; break;
        case 0x6C: wb(R.es, R.di, port_in8(R.dx)); R.di += d; break;
        case 0x6D: ww(R.es, R.di, port_in16(R.dx)); R.di += 2 * d; break;
        case 0x6E: port_out8(R.dx, rb(src_seg, R.si)); R.si += d; break;
        case 0x6F: port_out16(R.dx, rw(src_seg, R.si)); R.si += 2 * d; break;
        }
        if (!rep) break;
        R.cx--;
        T += 1;
        if (op == 0xA6 || op == 0xA7 || op == 0xAE || op == 0xAF) {
            if (rep == 0xF3 && !R.zf) break;
            if (rep == 0xF2 && R.zf) break;
        }
    }
}
