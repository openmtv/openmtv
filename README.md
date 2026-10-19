# OpenMTV — Mad TV (1991), decompiled and playable again

OpenMTV plays **Mad TV**, the TV station management game by Rainbow Arts (1991),
natively on **Windows, macOS and Linux**. The original DOS program was
decompiled function by function into readable C and checked against the
original machine code: same memory, same picture. On top of that come things
1991 did not have: multiplayer over the internet, fixes for 31 of the
original's bugs, a content editor, a 4× resolution display and AI-upscaled
pictures.

**Download and play:** <https://openmtv.net> (German) — packages for
Windows, macOS (Apple silicon) and Linux.

**You need your own copy of Mad TV.** OpenMTV contains none of the original
game's files; it reads `SOURCE1.RSC` and `SOURCE2.RSC` from your copy (and
`MTV.EXE`, `INTRO.EXE`, `MCV.BIN`, `DATAV.PCK` to build from source).
OpenMTV is a fan project and not affiliated with Rainbow Arts or the holders
of the rights to Mad TV — see [Legal](#legal).


## Contents

- [What it offers](#what-it-offers)
- [Playing](#playing)
- [Building from source](#building-from-source)
- [How it works](#how-it-works)
- [The original's bugs, fixed](#the-originals-bugs-fixed)
- [Options and settings](#options-and-settings)
- [Content editor and packs](#content-editor-and-packs)
- [Multiplayer](#multiplayer)
- [Testing and verification](#testing-and-verification)
- [Platforms](#platforms)
- [Repository layout](#repository-layout)
- [Documentation](#documentation)
- [Legal](#legal)

## What it offers

- **The original, rebuilt.** 591 of the 601 game functions are readable C
  (about 36 000 lines, with names for functions, globals and record fields);
  the rest — hand-written assembly modules, the Borland runtime — runs as
  recompiled machine-level C. *Classic mode* plays exactly the original,
  bugs included.
- **Multiplayer for 2–3 people**, in the LAN or over the internet without
  port forwarding: a lobby with invite codes, the host sets the rules and the
  content packs, the game is saved every night, a free station stays with the
  computer. Windows, macOS and Linux play together.
- **31 of the original's bugs fixed** — crashes and hangs, money tricks,
  computer players that miscalculate, broken data — each one switchable on
  its own ([list](#the-originals-bugs-fixed), details in
  [docs/CHANGES.md](docs/CHANGES.md)).
- **Game options**: start money, credit line, more offers at the film dealer
  and the ad agency (up to 17 instead of 10), a fourth game speed, fast room
  changes, a stronger computer opponent, Sammy prize money, the role fit of
  actors in per cent, the topicality of series.
- **Display**: any window size, fullscreen, 4:3 or square pixels; *HR Pixel*
  draws the game at 4× resolution (1280×800: a finer font, sharp frames and
  bars); *AI upscaling* redraws the pictures at 4× with an AI model, made on
  your machine from your game files (Ctrl+K switches while playing).
- **Content editor** for films, series, ads, news and screenplays, showing
  each entry as the game draws it. Changes are saved as packs that can be
  combined — **up to 999 films, 999 series and 999 ads** instead of 200, 50
  and 50 (see the [limits](#content-editor-and-packs)).
- **One program**: the launcher (game start, settings, multiplayer lobby,
  editor) in a window of its own, no installation, no Python.

## Playing

From <https://openmtv.net>: unpack the package anywhere you may write, put
`SOURCE1.RSC` and `SOURCE2.RSC` from your Mad TV into the `OpenMTV` folder
(or show the launcher their folder on the first start), then start
`OpenMTV.cmd` (Windows), `OpenMTV.app` (macOS) or `./openmtv` (Linux).

From a build of this repository: `./play.sh` (Windows: `play.cmd`) opens the
launcher; `./play.sh --direct` starts the game at once; `./play.sh -h` lists
every option.

In the game: F11 (or Alt+Enter, Cmd+F) toggles fullscreen, Ctrl+K switches
the AI pictures, F12 saves a screenshot, Pause releases a captured mouse.
Saved games live inside the game's `.rsc` files (`run/`, and
`build/mods/PACKS/` for games with packs) and the network games' saves beside
the settings (`~/.config/madtv/`, `%APPDATA%\madtv\`).

## Building from source

Requirements:

| | |
|---|---|
| C compiler and make | gcc or clang; Windows is cross-built with MinGW-w64 |
| SDL 2 | `libsdl2-dev` (Debian/Ubuntu), `brew install sdl2` (macOS); Windows: fetched by `make win` |
| Python 3.8+ with capstone | `make setup` (disassembler) and the tests: `apt install python3-capstone` or `pip install capstone` |
| Go 1.25+ | the launcher (`make launcher`); its window build needs WebKitGTK 4.1 on Linux |
| Your Mad TV files | `MTV.EXE`, `INTRO.EXE`, `SOURCE1.RSC`, `SOURCE2.RSC`, `MCV.BIN`, `DATAV.PCK` |

```sh
make setup GAME=/path/to/madtv   # once: unpack the EXEs, generate gen*/ from them, copy the game files to run/
make                             # build/mtv (machine-level), build/mtv-dec (decompiled: the game), build/mtv-intro
make launcher                    # build/openmtv: the launcher, in Go
./play.sh                        # the launcher in the browser (make launcher-native: in its own window)
./verify.sh --quick              # a quick equivalence check (~2 minutes; the full ./verify.sh ~15-25)
```

`make setup` reads your `MTV.EXE` and `INTRO.EXE`, unpacks them (LZEXE) and
recompiles their machine code into C (`gen/`, `gen_intro/`); nothing of the
original is in this repository. More targets: `make launcher-native`
(Linux window build), `make launcher-native-app` (`build/OpenMTV.app` on the
Mac), `make win` and `make launcher-win` (Windows, cross-built),
`python3 tools/package.py win|linux|mac` (the players' packages).

The developers' ways to start: `./play.sh --lifted` (the machine-level build,
always classic), `./play.sh --record FILE` (record a session as a replayable
script), `./play.sh intro` (the cinematic intro first).

## How it works

1. **Recompiled (`gen/`)** — every function of the EXE as C operating on an
   emulated 8086 state, generated by a static recompiler (`tools/lift.py`)
   from your `MTV.EXE`. Bit-exact with the original: identical memory and
   video contents at every checkpoint of a 30 000-frame randomized session.
2. **Decompiled (`src/`)** — 591 of the 601 game functions as structured C
   with real parameters, return values, locals, loops, 32-bit values, names,
   29 record tables as C structs (`players(i).money`, `tx(p, slot).fee`),
   global arrays and enums. Each overrides its machine-level twin at link time
   (weak symbols), so any mix builds and runs. `src/` is hand-maintained since
   the decompiler's output was frozen (tag `generated-final`); the decompiler
   (`tools/decomp.py`) stays as a reference (`make decomp-ref`).
3. **Interpreter** — runs the code the game loads at runtime (the PALLADIX
   AdLib driver inside `SOURCE1.RSC`; the intro's music driver `MCV.BIN`) and
   serves as the reference for the tests.
4. **The intro** (`INTRO.EXE`: small memory model, EGA mode 0Dh) is lifted
   the same way into `gen_intro/` (`build/mtv-intro`).

The runtime (`runtime/`) emulates exactly what the two programs use: DOS file
and memory calls, BIOS keyboard/clock/video, the mouse driver, PIT/PIC timer
interrupts, VGA mode 13h and planar mode 0Dh, and an AdLib OPL2 (Nuked-OPL3).
`build/mtv-dec` carries what it needs of `MTV.EXE` (its initial data segment,
built from your file at `make` time) and needs only the two `.rsc` files to
play.

The game runs on a *logical clock*: time comes from function entries and
I/O, which every engine counts alike, so the decompiled build plays exactly
the game the original machine code plays on that clock, at the original's
pace, and deterministically — the basis of the multiplayer (lockstep) and of
replayable recordings.

## The original's bugs, fixed

Classic mode (`--classic`) plays the original exactly; by default every fix is
on (`--fixes=NAME,...` picks some, `build/mtv-dec --fixes=list` lists them).
Each fix has a test in `tests/fixes/` that shows the bug in classic mode and
its absence with the fix; [docs/CHANGES.md](docs/CHANGES.md) explains each
one with the addresses in the original.

| Fix | What the original does |
|-----|------------------------|
| `intruder` | the game hangs for good when a bailiff or terrorist strikes a library holding only series; love films are never seized |
| `random0` | `random_n(0)` stops the game with "Divide error" |
| `ai-studio` | a computer player that lost its studio "rents" studio 13 and never produces again |
| `ai-fill-hour` | computer players estimate a programme's audience for the wrong hour |
| `ai-series-rule` | computer players check an ad's genre rule against the wrong programme |
| `series-avg` | computer players rate series by episodes 2..n+1 and undervalue them |
| `estate-text` | the estate agent shows "Warning: Psychiatrist" instead of a message |
| `tx-price` | a transmitter can be bought with less money than its price |
| `library-owner` | a film taken off the library shelf and put back stays "on the market" |
| `news-cache` | with a full news inbox, broadcast slots 2 and 3 show slot 1's headline |
| `ai-odd-range` | the computer players' odd-length rule reads stale stack bytes and never rejects a film |
| `boss-music` | before the end sequence the boss room's music plays on |
| `catch-bounty` | catching a rival in your own room pays the bounty only on station 1 |
| `ai-visit` | computer players sneak into station 1's rooms instead of the human's |
| `ai-trendy` | a computer player airing "Trendy" writes the trends over other variables |
| `ai-rival` | a computer player's daily budget plan takes itself as its rival |
| `ai-arrived` | the second computer player waits again at a door it already stands at |
| `title-cache` | with 47 or more films in archive and case, the last films lose their titles |
| `case-owner` | a tape carried in the case has no owner: the dealer can offer it to anyone |
| `ai-fill-genre` | computer players test a programme against the wrong genre when they fill an hour |
| `ai-lib-quality` | computer players' film budget follows library size, not quality |
| `studio-takeover` | a queued studio renter gets a public room's door instead of his studio |
| `studio-reserved` | a studio's pending renter is released or charged again after every production |
| `credit-repay` | in debt, "repay credit" gives you money, past every credit line (always on in network games) |
| `case-corner` | the film agency's case hands out an untitled film to sell again and again (always on in network games) |
| `ai-rating` | a computer player's rating overflows above 21.5 million viewers and it takes almost no ad contracts |
| `ai-bids` | a computer player's auction bids run past their list: contract counts break, then the game crashes |
| `ai-warning` | a computer player's live film opens "Warning" boxes on the human's screen until the game crashes |
| `film-200` | data: film 200 is titled "xxx", its German text cut off, and it is never offered |
| `live-aid` | data: film 164 "Live Aid" is 0 hours long and can never be planned |
| `video-connection` | data: the show "Video-Connection" asks nothing of its cast, so it never finds viewers |

## Options and settings

`./play.sh` and the launcher read `~/.config/madtv/settings.ini` (Windows:
`%APPDATA%\madtv\settings.ini`), created with commented defaults on the first
run; an option on the command line overrides it for one run (`./play.sh -h`).
The main ones:

- **Display**: `--fullscreen`, `--scale N` (window N × 320×240), `--aspect
  4:3|square`, `--filter sharp|nearest|linear|mmpx|hr-pixel`, `--integer`,
  `--ai-pictures SET.ki` (made in the launcher, page *Settings*).
- **Game**: `d`/`e` (German/English), `--classic`, `--fixes=...`,
  `--start-money=N`, `--credit-line=N|original`, `--terror-damage=N`,
  `--sammy-prize=N[,N,N,N]`, `--ad-offers=N`, `--film-offers=N` (10–17),
  `--fourth-speed`, `--fast-rooms`, `--ai-improve=off|normal|strong`,
  `--cast-fit`, `--series-topicality`.
- **Input**: `--mouse captured|free|system` (system: the system's own arrow).

The binaries read the settings file only with `--settings[=FILE]`, so tests
and recordings never depend on a player's settings. Display options are
presentation only: game state and recordings do not depend on them.

## Content editor and packs

The launcher's *Editor* edits the films, series, ads, screenplays (roles and
shopping lists), news stories and topics, in English and German. Films,
series and ads open on their card, drawn as the game draws it over the room
they are offered in; beside it what the card does not show (the TV family
that watches it, the audience from 18 to 24 o'clock, the checks). A table
mode changes many entries at once.

Changes are saved as **packs** (`mods/NAME/`: the tables of
`tools/content.py` as JSON). A pack keeps only its own entries — the ones it
adds and the original's it changes; the rest comes from your game files.
Packs can be played together (`./play.sh --mod A+B+C`): where two change the
same entry the later one counts, and the films, series and ads each pack adds
are appended after the ones before. In a network game the host's packs go to
every player.

Limits: **up to 999 films, 999 series and 999 ads** (the original: 200, 50
and 50), as long as each language's texts of a table fit in 64 KB and a
saved game in its 64 KB (for example 999 films with up to 374 series); the
editor checks this when saving. Screenplays and news stay within the
original's 50 and 400. The editor refuses what would keep a new game from
starting (e.g. too few programmes for the starting libraries) and marks the
required fields of a film in the game.

## Multiplayer

Two or three humans play one game, each on their own machine (Windows, macOS
and Linux mixed). Every machine computes the whole game; the server only
orders the players' inputs (lockstep on the logical clock), so a game needs a
few KB per second. In the launcher's page *Multiplayer*: open a game in the
LAN or on the internet server (`wss://openmtv.net`, or your own:
`build/openmtv serve --public`), the others join by code or from the list,
everyone picks a station and says ready. A lost connection comes back on its
own; the game is saved every night on every machine and goes on from the
lobby.

## Testing and verification

- **`./verify.sh`** runs everything (`--quick`: the core in about two
  minutes): machine-level build against the interpreter, every decompiled
  function against the original (differential testing), the scenarios on the
  logical clock, every fix test, packs and content, multiplayer (seats,
  network, saving at night), the launcher and editor, display filters.
- **Whole-program equivalence**: `--interp` and native runs of the same
  script print identical `hash` lines (memory + VRAM).
- **Per-function differential testing**: `--interp --difftest` runs each
  recompiled or decompiled function twice from the same snapshot (interpreted
  and native) and compares memory and registers; `tools/dtblame.py` reduces
  failures to root causes. Differential fuzzing calls every decompiled
  function directly with arguments from call sites and boundary values.
- **The launcher**: `tools/playtest.py`, `buildtest.py`, `launchertest.py`,
  `lobbytest.py`, `mpedgetest.py` compare its answers, files and games with
  recorded expectations (`tests/golden/`, `tools/golden.py --record` after an
  intended change), using test packs with made-up titles (`tests/packs/`);
  `cd launcher && go test`.
- **Scripts**: lines `FRAME command args` — `key`, `click x y`, `rclick`,
  `goto`, `pointer`, `shot file`, `hash`, `dump`, `seat S ...`, `fuzz`, `quit`
  — drive the game headless (`--headless --script FILE`); `--record FILE`
  writes one while you play.

Status: the machine-level build is bit-identical to the original over long
randomized sessions and the whole intro; the decompiled build shows no
failures in differential tests from real play and in fuzzing of all 591
decompiled functions. Where the original reads stack bytes nothing wrote,
classic mode reproduces the value where its source is known.

## Platforms

- **Linux**: native, the reference build.
- **macOS (Apple silicon)**: native with Apple clang; reproduces every hash
  of the Linux build (full `./verify.sh`). `xcode-select --install`, `brew
  install sdl2`, capstone for the system Python. Mac keyboards have no Pause
  key: Cmd+Tab releases the mouse, fn+F12 saves a screenshot.
- **Windows (x86-64)**: cross-built with MinGW-w64 (`make win`, which fetches
  and checks SDL 2's MinGW package); reproduces every hash of the Linux build.
  The machine-level build and the intro stay on Linux and macOS.

What keeps the builds identical across compilers and CPUs: `-ffp-contract=off`
(no fused multiply-add), division by zero trapped on every CPU as on the 8086
("Divide error", `runtime/divtrap.c`), weak default definitions instead of weak
references, files in binary mode on Windows.

## Repository layout

| Path | Contents |
|------|----------|
| `src/` | the game as readable C, hand-maintained (30 modules by subsystem, [src/README.md](src/README.md)) |
| `runtime/` | DOS/BIOS/VGA/timer/mouse/AdLib platform layer, 8086 interpreter, differential tester, network seats |
| `launcher/` | the launcher in Go: game start, packs' build, launcher page, multiplayer lobby and server, content editor, window |
| `tools/editor/` | the launcher's and editor's pages (built into the launcher) |
| `tools/` | recompiler (`lift.py`, `disasm.py`, `x86dec.py`, `regflow.py`), decompiler (`decomp.py`, `structure.py`, `cleanup.py`), `unlzexe.py`, resource tools (`rsc.py`, `content.py`), packaging, and the tests |
| `names/` | names for functions, globals and record fields; [names/FINDINGS.md](names/FINDINGS.md): game rules read from the code |
| `tests/` | scenarios, fix tests, multiplayer tests, baselines, test packs and expectations |
| `third_party/` | Nuked-OPL3 (LGPL-2.1), ONNX Runtime's C API headers (MIT) |
| `gen/`, `gen_intro/` | *generated* by `make setup` from your EXEs — not in the repository |

## Documentation

| Document | About |
|---|---|
| [docs/CHANGES.md](docs/CHANGES.md) | everything OpenMTV does differently from the original: fixes, data fixes, options, engine, editor |
| [src/README.md](src/README.md) | the decompiled code: its modules, conventions, how a change is made |
| [names/FINDINGS.md](names/FINDINGS.md) | the game's rules read from the code: economy, ratings, computer players, news, bugs |


## Legal

OpenMTV is a fan project. It is not affiliated with Rainbow Arts or with the
holders of the rights to *Mad TV*, and it contains none of the original game's
files: you need your own copy to build and to play. *Mad TV*, its program,
pictures, music and texts belong to their rights holders.

The work of this project's contributors is licensed under the **GNU General
Public License, version 3 or later** ([LICENSE](LICENSE),
[NOTICE.md](NOTICE.md)); it grants no rights in the original game.
Third-party parts keep their own licences: Nuked-OPL3 (LGPL-2.1, built as a
library of its own), SDL 2 (zlib), the ONNX Runtime headers (MIT), the
launcher's Go modules (see `launcher/go.mod`), the IBM Plex and notice fonts
(SIL Open Font License). The AI models for upscaling are downloaded only when
asked, from openmtv.net (CC BY-NC-SA 4.0, authors in
<https://openmtv.net/ki/1/README.txt>).

Each release is published here as one commit; issues and suggestions are
welcome.
