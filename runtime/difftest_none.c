/* The differential tester (difftest.c) where it is not built: Windows, which
 * has none of the POSIX signals and jumps it is made of (sigsetjmp, SIGALRM,
 * SIGSEGV on an alternate stack, backtrace).  It is a test tool; the game
 * does not need it.  --difftest and the script's fuzz command say so and
 * stop. */
#include <stdio.h>
#include <stdlib.h>
#include "runtime.h"

int difftest_on;
int difftest_active;
u32 difftest_port_seq;

static void none(const char *what) {
    fprintf(stderr, "%s: not in this build (the differential tester needs POSIX signals; build it on Linux or macOS)\n", what);
    exit(2);
}

int difftest_native_halt(const char *msg) { (void)msg; return 0; }
void difftest_abort(const char *why) { (void)why; }
int difftest_call(u16 seg, u16 off, native_fn f, int far) { (void)seg; (void)off; (void)f; (void)far; none("--difftest"); return 0; }
void difftest_summary(void) {}
void difftest_fuzz(const char *path, int reps) { (void)path; (void)reps; none("fuzz"); }
