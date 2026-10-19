/* Coroutines (coro.h): ucontext on Linux/macOS, fibers on Windows. */
#ifndef _WIN32
/* before any system header: macOS lays ucontext_t out for getcontext only
 * with it (else swapcontext fails with EINVAL) */
#define _XOPEN_SOURCE 700
#endif
#include <stdio.h>
#include <stdlib.h>
#include "coro.h"

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
