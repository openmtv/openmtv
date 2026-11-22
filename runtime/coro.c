/* Coroutines (coro.h): ucontext on Linux/macOS, fibers on Windows, a switch by hand on iOS (arm64). */
#ifndef _WIN32
/* before any system header: macOS lays ucontext_t out for getcontext only
 * with it (else swapcontext fails with EINVAL) */
#define _XOPEN_SOURCE 700
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "coro.h"
#ifdef __APPLE__
#include <TargetConditionals.h>
#define CORO_SYM(n) "_" n                /* (C names in assembly: with an underscore on Apple's) */
#else
#define CORO_SYM(n) n
#endif
#ifndef CORO_ASM
#define CORO_ASM 0
#endif

#ifdef _WIN32
#include <windows.h>

struct coro { void *fiber; void (*fn)(void *); void *arg; int done; };
static void *main_fiber;
static coro *current;

static void WINAPI fiber_main(void *p) {
    coro *c = p;
    c->fn(c->arg);
    c->done = 1;
    current = NULL;
    SwitchToFiber(main_fiber);           /* a fiber must not return */
}
coro *coro_new(void (*fn)(void *), void *arg, size_t stack_bytes) {
    if (!main_fiber) main_fiber = ConvertThreadToFiber(NULL);
    coro *c = calloc(1, sizeof *c);
    if (!c || !main_fiber) { fprintf(stderr, "coro: out of memory\n"); exit(1); }
    c->fn = fn; c->arg = arg;
    c->fiber = CreateFiber(stack_bytes, fiber_main, c);
    if (!c->fiber) { fprintf(stderr, "coro: CreateFiber failed\n"); exit(1); }
    return c;
}
void coro_resume(coro *c) {
    if (c->done || current) return;
    current = c;
    SwitchToFiber(c->fiber);
}
void coro_yield(void) {
    if (!current) return;
    current = NULL;
    SwitchToFiber(main_fiber);
}

#elif defined(__aarch64__) && (CORO_ASM || (defined(__APPLE__) && TARGET_OS_IPHONE))
/* arm64 by hand: iOS has getcontext/swapcontext, but they fail ("Operation not supported"),
 * and a network game runs every seat in a coroutine of its own.  coro_switch saves what a
 * function must keep for its caller (AAPCS64: x19-x30, d8-d15) on the stack it leaves, keeps
 * that stack's pointer, and goes on from the other one's (x18 is the system's: untouched).  A
 * new coroutine's stack holds such a frame whose return address is coro_boot.  -DCORO_ASM
 * takes this way on any arm64 (the Mac: tools/seattest.py tries it there). */
struct coro { void *sp; void (*fn)(void *); void *arg; int done; void *stack; };
static void *main_sp;
static coro *current;

void coro_switch(void **save_sp, void *load_sp);
void coro_boot(void);
__asm__(
    ".text\n"
    ".globl _coro_switch\n.globl coro_switch\n"
    ".p2align 2\n"
    "_coro_switch:\ncoro_switch:\n"
    "  sub sp, sp, #160\n"
    "  stp x19, x20, [sp, #0]\n"
    "  stp x21, x22, [sp, #16]\n"
    "  stp x23, x24, [sp, #32]\n"
    "  stp x25, x26, [sp, #48]\n"
    "  stp x27, x28, [sp, #64]\n"
    "  stp x29, x30, [sp, #80]\n"
    "  stp d8, d9, [sp, #96]\n"
    "  stp d10, d11, [sp, #112]\n"
    "  stp d12, d13, [sp, #128]\n"
    "  stp d14, d15, [sp, #144]\n"
    "  mov x9, sp\n"
    "  str x9, [x0]\n"
    "  mov sp, x1\n"
    "  ldp x19, x20, [sp, #0]\n"
    "  ldp x21, x22, [sp, #16]\n"
    "  ldp x23, x24, [sp, #32]\n"
    "  ldp x25, x26, [sp, #48]\n"
    "  ldp x27, x28, [sp, #64]\n"
    "  ldp x29, x30, [sp, #80]\n"
    "  ldp d8, d9, [sp, #96]\n"
    "  ldp d10, d11, [sp, #112]\n"
    "  ldp d12, d13, [sp, #128]\n"
    "  ldp d14, d15, [sp, #144]\n"
    "  add sp, sp, #160\n"
    "  ret\n"
    ".globl _coro_boot\n.globl coro_boot\n"
    ".p2align 2\n"
    "_coro_boot:\ncoro_boot:\n"
    "  mov x0, x19\n"                  /* (the coroutine, put there by coro_new) */
    "  bl " CORO_SYM("coro_entry") "\n"
    "  brk #0\n");

void coro_entry(coro *c) {               /* (on the coroutine's own stack, from coro_boot) */
    c->fn(c->arg);
    c->done = 1;
    current = NULL;
    coro_switch(&c->sp, main_sp);        /* never resumed again: it is done */
    abort();
}
coro *coro_new(void (*fn)(void *), void *arg, size_t stack_bytes) {
    coro *c = calloc(1, sizeof *c);
    if (!c || !(c->stack = malloc(stack_bytes))) { fprintf(stderr, "coro: out of memory\n"); exit(1); }
    c->fn = fn; c->arg = arg;
    uintptr_t top = ((uintptr_t)c->stack + stack_bytes) & ~(uintptr_t)15;
    uint64_t *frame = (uint64_t *)(top - 160);
    memset(frame, 0, 160);
    frame[0] = (uint64_t)(uintptr_t)c;          /* x19 */
    frame[11] = (uint64_t)(uintptr_t)coro_boot; /* x30: where coro_switch returns to first */
    c->sp = frame;
    return c;
}
void coro_resume(coro *c) {
    if (c->done || current) return;
    current = c;
    coro_switch(&main_sp, c->sp);
}
void coro_yield(void) {
    coro *c = current;
    if (!c) return;
    current = NULL;
    coro_switch(&c->sp, main_sp);
}

#else
#ifdef __APPLE__                         /* ucontext: deprecated on macOS, still there */
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
#include <ucontext.h>

struct coro { ucontext_t ctx; void (*fn)(void *); void *arg; int done; void *stack; };
static ucontext_t main_ctx;
static coro *current;

static coro *starting;
static void trampoline(void) {
    coro *c = starting;
    c->fn(c->arg);
    c->done = 1;
    current = NULL;                      /* uc_link returns to the main context */
}
coro *coro_new(void (*fn)(void *), void *arg, size_t stack_bytes) {
    coro *c = calloc(1, sizeof *c);
    if (!c || !(c->stack = malloc(stack_bytes))) { fprintf(stderr, "coro: out of memory\n"); exit(1); }
    c->fn = fn; c->arg = arg;
    getcontext(&c->ctx);
    c->ctx.uc_stack.ss_sp = c->stack;
    c->ctx.uc_stack.ss_size = stack_bytes;
    c->ctx.uc_link = &main_ctx;
    starting = c;
    makecontext(&c->ctx, trampoline, 0);
    return c;
}
void coro_resume(coro *c) {
    if (c->done || current) return;
    current = c;
    starting = c;                        /* read by trampoline on the first resume */
    if (swapcontext(&main_ctx, &c->ctx) != 0) { perror("coro: swapcontext into a coroutine"); exit(1); }
}
void coro_yield(void) {
    coro *c = current;
    if (!c) return;
    current = NULL;
    if (swapcontext(&c->ctx, &main_ctx) != 0) { perror("coro: swapcontext out of a coroutine"); exit(1); }
}
#endif

int coro_done(const coro *c) { return c->done; }
coro *coro_current(void) { return current; }
