/* Integer division by zero in C code: the same divide error on every CPU.
 *
 * The 8086 raises INT 0 when a DIV/IDIV divides by zero; the interpreter and
 * the machine-level code emulate that explicitly (alu.c).  The decompiled
 * functions (src/) divide with plain C operators, and a C division by zero
 * traps on x86 (SIGFPE) but silently yields 0 on arm64.  So src/ is compiled
 * with -fsanitize=integer-divide-by-zero -fsanitize-undefined-trap-on-error
 * (Makefile: DIVTRAP): each division checks its divisor first and executes a
 * trap instruction on zero.  This handler turns that trap -- or an x86
 * hardware divide fault -- into what the original program does:
 *   - normally: INT 0, i.e. the program's own divide-error handler (Turbo C
 *     prints "Divide error" and exits with code 3);
 *   - in the native half of a difftest: report the function and recover.
 * Any other SIGILL/SIGTRAP keeps its default action.
 * Windows has no signals for this: a vectored exception handler sees the
 * hardware fault (EXCEPTION_INT_DIVIDE_BY_ZERO) and the trap instruction
 * (EXCEPTION_ILLEGAL_INSTRUCTION at a UD2) instead.
 */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "runtime.h"

int difftest_native_halt(const char *msg);

/* the 8086's divide error: the program's handler; Turbo C's exits */
static void divide_error(void) {
    difftest_native_halt("divide error (C division by zero)");   /* returns unless in a difftest's native half */
    do_int(0);
    fprintf(stderr, "Divide error\n");       /* it returned: the C code cannot resume */
    exit(3);
}

/* the trap instruction the compiler puts in for a division by zero */
static int is_trap_insn(const u8 *pc) {
    if (!pc) return 0;
#if defined(__aarch64__)
    u32 insn;
    memcpy(&insn, pc, 4);
    if ((insn & 0xFFE0001Fu) != 0xD4200000u) return 0;   /* BRK #imm */
    u32 imm = (insn >> 5) & 0xFFFF;
    return imm == 0x5503        /* clang: UBSan trap, kind 3 = divrem overflow */
        || imm == 0x3E8;        /* gcc: __builtin_trap */
#elif defined(__x86_64__) || defined(__i386__)
    if (pc[0] == 0x67) pc++;                 /* clang: ud1l kind(%eax), %eax */
    return pc[0] == 0x0F && (pc[1] == 0x0B || pc[1] == 0xB9);   /* UD2 / UD1 */
#else
    return 0;
#endif
}

#ifdef _WIN32
#include <windows.h>

static LONG CALLBACK on_exception(EXCEPTION_POINTERS *ep) {
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    if (code == EXCEPTION_INT_DIVIDE_BY_ZERO || code == EXCEPTION_INT_OVERFLOW
        || (code == EXCEPTION_ILLEGAL_INSTRUCTION && is_trap_insn(ep->ExceptionRecord->ExceptionAddress)))
        divide_error();
    return EXCEPTION_CONTINUE_SEARCH;        /* not ours: the default handling */
}

__attribute__((constructor)) static void divtrap_init(void) {
    AddVectoredExceptionHandler(1, on_exception);
}
#else

static int is_divide_trap(int sig, const siginfo_t *si) {
    if (sig == SIGFPE) return si->si_code == FPE_INTDIV || si->si_code == FPE_INTOVF;
#if defined(__aarch64__)
    if (sig != SIGTRAP) return 0;
#else
    if (sig != SIGILL) return 0;
#endif
    return is_trap_insn(si->si_addr);        /* the trapping instruction */
}

static void on_trap(int sig, siginfo_t *si, void *uc) {
    (void)uc;
    if (!is_divide_trap(sig, si)) {
        signal(sig, SIG_DFL);                /* not ours: re-executing it takes the default action */
        return;
    }
    divide_error();
}

__attribute__((constructor)) static void divtrap_init(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = on_trap;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGFPE, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGTRAP, &sa, NULL);
}
#endif
