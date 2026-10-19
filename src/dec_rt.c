/* Calls from decompiled C into machine-level code (lifted or interpreted).
 * Arguments are pushed on the emulated stack exactly as the original caller
 * did; the callee pops its far return address; we remove the arguments. */
#include <stdio.h>
#include "dec.h"

extern u32 watch_entry_lin;
static u32 do_call(native_fn f, u16 seg, int far, int nwords, va_list ap) {
    u16 w[64];
    for (int i = 0; i < nwords; i++) w[i] = (u16)va_arg(ap, int);
    for (int i = nwords - 1; i >= 0; i--) push16(w[i]);
    if (far) push16(seg);
    push16(0xFFFF);                      /* return IP: never used by callees */
    if (watch_entry_lin && f && native_lookup_any(seg, (u16)(watch_entry_lin - ((u32)seg << 4))) == f)
        fprintf(stderr, "[entry native] AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X DF=%d SP=%04X args %04X %04X %04X %04X\n",
                R.ax, R.bx, R.cx, R.dx, R.si, R.di, R.ds, R.es, R.df, R.sp, rw(R.ss, R.sp + 4), rw(R.ss, R.sp + 6), rw(R.ss, R.sp + 8), rw(R.ss, R.sp + 10)),
        fprintf(stderr, "    header %04X %04X %04X %04X\n", rw(rw(R.ss, R.sp + 6), rw(R.ss, R.sp + 4)), rw(rw(R.ss, R.sp + 6), rw(R.ss, R.sp + 4) + 2), rw(rw(R.ss, R.sp + 6), rw(R.ss, R.sp + 4) + 4), rw(rw(R.ss, R.sp + 6), rw(R.ss, R.sp + 4) + 6));
    u16 cs = R.cs;
    R.cs = seg;
    if (f) f(); else if (far) call_far(seg, 0); else call_near(seg, 0);
    R.cs = cs;
    R.sp += 2 * nwords;
    return ((u32)R.dx << 16) | R.ax;
}

u32 call_lifted(native_fn f, u16 seg, int nwords, ...) {
    va_list ap; va_start(ap, nwords);
    u32 r = do_call(f, seg, 1, nwords, ap);
    va_end(ap);
    return r;
}

u32 call_lifted_near(native_fn f, u16 seg, int nwords, ...) {
    va_list ap; va_start(ap, nwords);
    u32 r = do_call(f, seg, 0, nwords, ap);
    va_end(ap);
    return r;
}

/* call through a far function pointer held by the game */
u32 call_far_addr(fptr target, int nwords, ...) {
    va_list ap; va_start(ap, nwords);
    u16 w[64];
    for (int i = 0; i < nwords; i++) w[i] = (u16)va_arg(ap, int);
    va_end(ap);
    for (int i = nwords - 1; i >= 0; i--) push16(w[i]);
    push16(R.cs); push16(0xFFFF);
    call_far(FPSEG(target), FPOFF(target));
    R.sp += 2 * nwords;
    return ((u32)R.dx << 16) | R.ax;
}
