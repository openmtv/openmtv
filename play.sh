#!/bin/sh
# Play Mad TV using the decompiled build, with the original's bugs fixed.
# The settings in ~/.config/madtv/settings.ini (created on the first run:
# display, language, fixes, start money, fourth speed, AI improvement, sound) apply; options here
# override them.
# The decompiled build runs on the logical clock (--logical-time: time from
# function entries and I/O), where it plays exactly the game the original
# machine code plays on that clock, at the original's pace; the machine-level
# build (--lifted, --record) keeps the original's instruction timing.
#   ./play.sh                 the launcher: the page "Play" in its own window, or in the
#                             browser (packs, settings, start, "Multiplayer"; the content
#                             editor in the same page)
#   ./play.sh editor          the launcher, at the content editor: films, series, ads, news
#   ./play.sh --direct        the game at once: the decompiled C build with the settings
#                             file's choices (any other argument starts it at once too)
#   ./play.sh d / e           German / English this time
#   ./play.sh --classic       exactly the original (no fixes) this time
#   ./play.sh --fixes=a,b     only the named fixes (build/mtv-dec --fixes=list)
#   ./play.sh e --lifted      run the machine-level build instead (always classic)
#   ./play.sh intro           run the INTRO.EXE cinematic first
#   ./play.sh -h / --help     the options, for players
# Display (any build; F11, Alt+Enter or Cmd+F switch fullscreen while playing):
#   --fullscreen              start in fullscreen
#   --scale N                 window N times 320x240 (default 3)
#   --aspect 4:3|square       4:3 like a VGA monitor (default) or square pixels
#   --filter sharp|nearest|linear|mmpx|hr-pixel   sharp (default): even pixels at any size;
#                             mmpx: smoothed edges and diagonals, only the picture's colours;
#                             hr-pixel: drawn at 4x -- a finer font, exact frames and bars,
#                             the pictures' pixels as they are
#                             (decompiled build; docs/hochaufloesend.md)
#   --integer                 whole-number scale factors only
#   --mouse captured|free|system   free: the system pointer moves the game's cursor;
#                             system: and the system's arrow is the game's (decompiled build)
#   --start-money=N           your cash in a new game, in dollars (the original: 200000;
#                             the decompiled build only)
#   --credit-line=N           the most the boss lends (image below 50), in dollars:
#                             +100000 a day from day 2 (the original: 300000;
#                             the decompiled build only)
#   --terror-damage=N         what a terrorist's bomb costs the owner of the room, in
#                             dollars (the original: 50000; 0: nothing; the decompiled build only)
#   --sammy-prize=N[,N,N,N]   the Sammy winner's prize money, in dollars: one amount, or news,
#                             culture, shows, series (the original: none; the decompiled build only)
#   --fourth-speed[=yes|no]   a fourth game speed in the planner's Control-Center, twice
#                             the original's fastest (the decompiled build only)
#   --fast-rooms[=yes|no]     room changes at once, without the original's wipe and
#                             loading wait (the decompiled build only)
#   --series-topicality[=yes|no]   the series card shows how topical the series still is,
#                             a bar as on the film card (the decompiled build only)
#   --cast-fit[=yes|no]       the production shop shows each actor's fit for the role
#                             picked, in per cent, instead of the three bars (the
#                             decompiled build only)
#   --ai-improve=off|normal|strong   computer players build up their transmitter network
#                             when you build up yours, a while later (off: the original;
#                             the decompiled build only; docs/ki-verbesserung.md)
#   --ad-offers=N             the contracts the ad agency offers at a time, 10 to 17
#                             (the original: 10; the decompiled build only)
#   --film-offers=N           the films and series the film dealer offers at a time, 10 to 17
#                             (the original: 10; the decompiled build only)
#   --settings=FILE           another settings file
#   ./play.sh --mod NAME      play the mod mods/NAME (made in the content editor); its
#                             game files are built into build/mods/NAME
#   ./play.sh --mod A+B+C     several mods together as packs, in this order: each
#                             pack's new films, series and ads follow the ones before
#                             (tools/modpacks.py); built into build/mods/A+B+C
#   ./play.sh --record FILE   record a test session: machine-level build (original
#                             timing, so the recording stays valid while src/
#                             changes) on a fresh copy of the game files
# Needs the original game files one directory up (or set GAME=/path): SOURCE1.RSC
# and SOURCE2.RSC; MTV.EXE only for the machine-level build (--lifted, --record).
# The work is done by build/openmtv (launcher/, in Go: make launcher; no Python) -- or by its
# window build, build/openmtv-native or build/OpenMTV.app (below); the developers' ways
# (--lifted, --record, intro) by build/openmtv, with make setup's unpacked EXEs.
# build/openmtv --print ... shows the game's command line.
here="$(dirname "$0")"
for a in "$@"; do                   # the developers' ways: the program that runs in this terminal
  case "$a" in --lifted|--record|intro) [ -x "$here/build/openmtv" ] && exec "$here/build/openmtv" "$@";; esac
done
# the launcher in its own window where it is built (the Mac: build/OpenMTV.app, make
# launcher-native-app; Linux: build/openmtv-native, make launcher-native), else build/openmtv
# with the page in the browser -- also where the window's library is missing: Linux's
# needs WebKitGTK 4.1 (Debian/Ubuntu: apt install libwebkit2gtk-4.1-0)
for p in "$here/build/OpenMTV.app/Contents/MacOS/openmtv" "$here/build/openmtv-native"; do
  [ -x "$p" ] || continue
  "$p" -h >/dev/null 2>&1 && exec "$p" "$@"
  echo "OpenMTV: ${p#"$here/"} does not start (Linux: it needs WebKitGTK 4.1, libwebkit2gtk-4.1-0) -- build/openmtv instead, the launcher in the browser" >&2
done
[ -x "$here/build/openmtv" ] && exec "$here/build/openmtv" "$@"
echo "OpenMTV: build/openmtv is not built -- make launcher (it needs Go)" >&2
exit 2
