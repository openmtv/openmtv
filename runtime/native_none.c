/* Empty native table: pure interpreter build. */
#include "runtime.h"
const native_entry native_table[] = {{0xFFFFFFFF, 0}};
const int native_count = 0;
void native_entry_point(void) {}
const u8 native_retflags[] = {0};
const u32 poll_points[] = {0};
const int poll_count = 0;
