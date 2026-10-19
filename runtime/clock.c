/* Logical clock (runtime.h): the bitmap of counted function entries.  The
 * table comes from src/ (tools/clock_table.py -> build/clock_entries.c) and
 * is linked into the game builds; other programs (INTRO.EXE) get the empty
 * default of clock_none.c -- in its own file, so that the compiler cannot
 * fold the weak default into this code. */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "runtime.h"

extern const u32 clock_entry_table[];
extern const int clock_entry_count;

u8 *clock_map;

void clock_init(void) {
    clock_map = calloc(1, 0x50000 / 8);
    for (int i = 0; i < clock_entry_count; i++)
        if (clock_entry_table[i] < 0x50000) clock_map[clock_entry_table[i] >> 3] |= 1 << (clock_entry_table[i] & 7);
}

int clock_trace;
void clock_trace_line(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}
