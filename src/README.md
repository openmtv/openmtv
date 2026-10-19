# src/ — the game code (hand-maintained)

The decompiled C of `MTV.EXE`, frozen on 2026-10-08 (git tag
`generated-final`).  From that tag on these files are edited by hand;
`tools/decomp.py` no longer writes here (`make decomp-ref` puts its output into
`build/generated/` for comparison only).

## Modules

Turbo C gives every source file its own code segment, so each original
segment is one original source module.  The files keep that split and are
named by subsystem:

| File | Segment | Contents |
|------|---------|----------|
| `main_loop.c` | 0054 | main loop, entering and leaving rooms, room assets, game clock tick, hotkeys, TV picture |
| `text_cache.c` | 048C | film, series, screenplay and news texts loaded on demand |
| `rooms_click.c` | 05FF | left-click dispatcher and most room click handlers |
| `rooms_people.c` | 0AA4 | boss, new-game setup, supermarket, shop, Betty, presents |
| `system.c` | 0D5E | resource loading, disk switching, `random_n`, game clock |
| `text_draw.c` | 0DDD | text rendering |
| `strings.c` | 0E35 | `str_format`, number formatting, string helpers |
| `frame.c` | 0ED7 | per-frame update and presentation, text input, Madris step |
| `ui.c` | 105E | cursor, mouse, frames, popups, message boxes, TV family, endings |
| `status_panel.c` | 14E1 | status panel, hover info, TV info, hourly broadcast evaluation |
| `building.c` | 193D | building view, elevator, walking figures |
| `news.c` | 1C18 | content tables, news engine, hour/day schedule transitions |
| `rules.c` | 1DA8 | rule helpers: schedule, audience, screenplays, topicality, studios, events |
| `settlement.c` | 222A | end-of-day settlement |
| `actions.c` | 229E | player actions, hour tick, save/load |
| `market.c` | 24B8 | daily refill of agencies, dealer, auction; trends; boss rating |
| `screens.c` | 26A7 | planner, transmitter map, auction board, information board, news room |
| `dialogs.c` | 2B74 | shopping lists, Betty, boss dialog, Madris, save slots, language prompt |
| `ai.c`, `ai_estimate.c`, `ai_plan.c` | 2DD9, 32BA, 348B | computer players |
| `sound.c`, `timer.c`, `intro.c`, `huffman.c`, `bitmap.c`, `resfile.c`, `heap.c`, `bits.c`, `util.c` | 35F8, 365D, 365F, 37B4, 382C–3848, 385D, 38C6/3985, 35F1, 2DD4 | platform-near code and the Borland far heap |
| `rtl.c`, `rtl_heap.c` | 3736–3973, 0000 | Borland run-time library and assembler helpers rewritten in C (conventions in `rtl.h`): string and number functions, `rand`, atexit, keyboard/mouse/video/palette/retrace, far heap, huge-pointer helpers |
| `state.c` | -- | game state in host memory: the data segment and the heap blocks a savegame holds (films, series, screenplays, managers' names) plus the coverage map, written back to their original places where the emulated memory is read; host handles for C buffers |
| `rtl_start.c` | 0000 | Turbo C's start-up code in C (same conventions): C0, SETARGV, SETENVP, the divide-error handler and the start-up hooks |
| `rtl_io.c` | 0000, 385D, 38C0, 390E–39C8 | file I/O, exit, the `.rsc` file layer (same conventions) |
| `rtl_gfx.c` | 36F4–37A2, 383E, 4456 | the graphics assembler helpers in C (same conventions): bitmap/screen copies, rectangle fill, transparent sprites and glyphs, the intro's LZW and delta-frame decoders |
| `rtl_snd.c` | 371E, 35F8 | the game's side of the sound driver in C (stage 6, same conventions): detector, linking the driver, one function per driver entry (the driver stays interpreted), loading the room song, stopping it on leaving a room |

Headers: `game.h` (include this) pulls in `dec.h` (memory macros, virtual
time), `game_types.h` (enums, record structs and accessors),
`game_globals.h` (named globals, arrays, record-field macros) and
`game_protos.h` (prototypes by module).

## Rules for editing

1. **Keep the address comment** `/* SSSS:OOOO description */` above every
   function: it ties the function to its machine-level twin in `gen/` and
   to the difftest tables.
2. **Changing a signature?**  Update its wrapper `f_SSSS_OOOO` in `glue.c`
   (reads the original stack arguments) and, if the number of argument words
   or the return width changes, its entry in `game_table.c`.
3. **Keep `FUNC_TICK` / `LOOP_TICK`** unless you mean to change timing: they
   make runs deterministic and comparable.  `FUNC_TICK` also counts the
   function's entry for the logical clock (`--logical-time`, runtime.h), which
   the machine-level build and the interpreter count alike; a function whose
   call count depends on original stack garbage uses `FUNC_TICK_NOCLOCK`
   instead (tools/clock_table.py then leaves it out everywhere).  A new
   decompiled function needs its `/* SEG:OFF */` comment, as now.  The
   run-time library in `rtl*.c` is different: exact time (`RTL_ENTRY()` and
   the original's instruction counts, checked by the difftest), not a clock
   entry -- see `rtl.h`.  So is `game_main` (0054:0005), which was
   machine-level code until Phase 3: it keeps the original's exact time and
   call frames, so classic runs stay bit-identical.
4. **Behaviour changes go behind a switch** so that classic mode stays exact.
   For an original-bug fix: a bit
   and a name in `fixes.h` / `fixes.c`, the change under `if (fix_on(FIX_…))`
   with an `ORIGINAL BUG (fix "name")` comment, and a test in `tests/fixes/`
   that shows the bug classic and its absence fixed.  `--fixes[=LIST]`
   switches fixes on (`play.sh` does by default), classic is the default of
   the binaries and of every test; `--difftest` turns all fixes off.
5. **Check every change:** `make check` (equivalence with the original), and
   for behaviour work `tools/ai_run.py` against `tests/baseline/`.  A pure
   refactoring must keep `tests/session1.txt` hashes identical:
   `build/mtv-dec --headless --fast --game run --script tests/session1.txt`.
6. The machine-level C in `gen/` is generated (`make setup` / `make gen`):
   never edit it; it is the bit-exact reference build `build/mtv`.

## Stack garbage: where classic mode reproduces it, and where not

Some original functions read stack bytes that nothing wrote for them -- a
local that no path has written yet, or the arguments of a call that pushed
none; the value is whatever earlier code left at that stack address.  The
decompiled build keeps its locals in C (since Phase 3 stage 4) and pushes no
arguments, return addresses or saved registers, so it would read different
garbage.  To find such reads and their source:

    build/mtv --stackchk --headless --game run --script FILE   # [uninit] lines

Each line names the reading instruction, its caller, the value and the last
writer of the byte (instruction and the return address of its frame);
`STACKCHK_ALL=1` reports every read instead of the first per instruction.

Where the source is fixed by the code, classic mode reproduces the value and
an `ORIGINAL BUG` comment says where it comes from; a fix switch gives the
evident intention: the AI's half-day range (`2DD9:1147`, fix
`ai-odd-range`, the word is tracked in `ai.c`) and the boss room's call
without arguments (`0AA4:000E`, fix `boss-music`).  Where it depends on
arbitrary earlier stack use (`32BA:07EC`) or has not been traced
(`193D:2424`, `32BA:1800`), `src/` uses a defined value in every mode.  The
difftest uses the same tracking: a mismatch where the original's result
depended on such a read is reported as `UNDEF`, not counted as a failure.
