/* Coroutines: one stack per seat, all on the main OS thread (SDL wants its
 * window and events there on macOS; the game calls SDL from deep inside its
 * code).  ucontext on Linux and macOS, fibers on Windows.  Deterministic:
 * a coroutine runs only between coro_resume() and its next coro_yield(). */
#ifndef MTV_CORO_H
#define MTV_CORO_H
#include <stddef.h>

typedef struct coro coro;
coro *coro_new(void (*fn)(void *), void *arg, size_t stack_bytes);
void coro_resume(coro *c);        /* from the main context: run c until it yields or ends */
void coro_yield(void);            /* from inside a coroutine: back to the main context */
int coro_done(const coro *c);     /* its function has returned */
coro *coro_current(void);         /* the running coroutine, NULL in the main context */
#endif
