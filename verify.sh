#!/bin/sh
# Equivalence checks for the recompiled and decompiled builds.
#   ./verify.sh          full (steps 4-6 run alongside 1-3, each on all
#                        cores; the wall time is about that of the 8-day
#                        scenario's difftest: ~12 min on 6 cores)
#   ./verify.sh --quick  steps 1-3 only (about 2 minutes)
# 1. machine-level build vs interpreter: identical memory/VRAM hashes
# 2. decompiled build: per-function differential test, no root-cause failures
# 3. INTRO.EXE native vs interpreter
# 4. recorded scenarios (tests/scenarios/): machine-level hashes + difftest,
#    and the native decompiled build against the machine-level one on the
#    logical clock (--logical-time)
# 5. original-bug fixes (tests/fixes/): bug in classic mode, gone with the fix
# 6. changed content (tools/modtest.py): more films, series and ads, starting pools, packs
# 7. the MMPX display filter (tools/mmpxtest.py): presentation only, its picture
# 8. the decompiled build without MTV.EXE (tools/exefree.py): the same game
# 9. the system arrow (tools/arrowtest.py): the same game, only the arrow left out
# 10. the seat scheduler (tools/seattest.py, multiplayer): --seats 1 plays the same game, and so does seat 0 with a spectator seat (--seats 2)
# 11. a network game saved at night and loaded (tools/mpsavetest.py): saving changes nothing, every machine the same savegame and game
# 12. the AI improvement (tools/aiimptest.py): computer players follow the human's transmitters, saved and loaded, off the original
# 13. the launcher and editor in Go, where Go is installed (make launcher), against what they gave when recorded
#     (tests/golden/, with the test's content packs tests/packs): tools/playtest.py (the start), tools/buildtest.py
#     (the packs, byte for byte), tools/launchertest.py (the launcher page's and the editor's answers and files; the
#     page's self-tests and the editor's in headless Chrome, where it is), tools/lobbytest.py (two launchers in a
#     lobby), tools/mpedgetest.py (the network game's edge cases)
# 14. the role fit in the shop (tools/castfittest.py): the same game, only the panel's picture
# 15. the series card's topicality (tools/sertoptest.py): the bar on a band of its own, a row more for a long episode title
# 16. the settings file (tests/settings_test.c, runtime/settings.c on its own; also with --quick): a byte
#     order mark and CRLF read like a plain file
# 17. drawing at 4x resolution (tools/hrtest.py, --filter hr-pixel): the same game, the cells against the plain
#     4x enlargement, no more pixels without a cell than the original's own
# 18. the AI pictures (tools/kitest.py, without a model; where Pillow and numpy are): mtv-dec --make-ki
#     makes what tools/hrai.py makes, and a set is display only (a network game with and without one)
set -e
cd "$(dirname "$0")"
make -s build/mtv build/mtv-dec build/mtv-intro build/mmpx4 GENCFLAGS="-O1 -Igen"
# the Go program first, where Go is installed: the network tests' server (tools/netserver.py)
GO_BUILT=
if [ "$1" != "--quick" ] && command -v "${GO:-go}" >/dev/null 2>&1 && make -s launcher; then GO_BUILT=1; fi
# steps 4-8 use their own temporary game directories: start them now
# (and stop them if an earlier step fails)
P4= P5= P6= P7= P8= P9= P10= P11= P12= P13= P14= P15= P17= P18= P19=
trap '[ -n "$P4$P5$P6$P7$P8$P9$P10$P11$P12$P13$P14$P15$P17$P18$P19" ] && kill $P4 $P5 $P6 $P7 $P8 $P9 $P10 $P11 $P12 $P13 $P14 $P15 $P17 $P18 $P19 2>/dev/null; true' EXIT
if [ "$1" != "--quick" ]; then
  if [ -d tests/scenarios ] && ls tests/scenarios/*.txt >/dev/null 2>&1; then
    python3 tools/scenario.py check > build/v_scenarios.log 2>&1 & P4=$!
  fi
  if [ -d tests/fixes ] && ls tests/fixes/*.txt >/dev/null 2>&1; then
    python3 tools/fixtest.py > build/v_fixes.log 2>&1 & P5=$!
  fi
  python3 tools/modtest.py > build/v_mods.log 2>&1 & P6=$!
  python3 tools/mmpxtest.py > build/v_mmpx.log 2>&1 & P7=$!
  python3 tools/exefree.py > build/v_exefree.log 2>&1 & P8=$!
  python3 tools/arrowtest.py > build/v_arrow.log 2>&1 & P9=$!
  python3 tools/seattest.py > build/v_seats.log 2>&1 & P10=$!
  python3 tools/mpsavetest.py > build/v_mpsave.log 2>&1 & P11=$!
  python3 tools/aiimptest.py > build/v_aiimp.log 2>&1 & P12=$!
  python3 tools/castfittest.py > build/v_castfit.log 2>&1 & P14=$!
  python3 tools/sertoptest.py > build/v_sertop.log 2>&1 & P15=$!
  python3 tools/hrtest.py > build/v_hr.log 2>&1 & P17=$!
  python3 tools/loadslottest.py > build/v_loadslot.log 2>&1 & P19=$!
  if python3 -c 'import numpy, PIL' 2>/dev/null; then python3 tools/kitest.py > build/v_ki.log 2>&1 & P18=$!; fi
  if [ -n "$GO_BUILT" ]; then
    sh -c 'python3 tools/playtest.py && python3 tools/buildtest.py && python3 tools/launchertest.py && python3 tools/lobbytest.py && python3 tools/mpedgetest.py' > build/v_launcher.log 2>&1 & P13=$!
  fi
fi
S=tests/session1.txt
./build/mtv          --headless --game run --script $S > build/v_native.txt 2>/dev/null
./build/mtv --interp --headless --game run --script $S > build/v_interp.txt 2>/dev/null
if cmp -s build/v_native.txt build/v_interp.txt; then
  echo "[1] machine-level == interpreter at $(grep -c "^HASH" build/v_interp.txt) checkpoints (memory, screen and game state)"
else
  echo "[1] FAIL: hashes differ"; diff build/v_native.txt build/v_interp.txt | head; exit 1
fi
./build/mtv-dec --interp --difftest --headless --game run --script $S > /dev/null 2> build/v_diff.err
grep "difftest\]" build/v_diff.err
python3 tools/dtblame.py build/v_diff.err | tee build/v_blame.txt | head -5
grep -q "^0 failing" build/v_blame.txt || { echo "[2] FAIL"; exit 1; }
echo "[2] decompiled functions match the original"
if [ -f run_intro/INTRO_U.EXE ]; then
  ./build/mtv-intro          --headless --game run_intro --exe run_intro/INTRO_U.EXE --script tests/intro_hash.txt > build/vi_n.txt 2>/dev/null
  ./build/mtv-intro --interp --headless --game run_intro --exe run_intro/INTRO_U.EXE --script tests/intro_hash.txt > build/vi_i.txt 2>/dev/null
  cmp -s build/vi_n.txt build/vi_i.txt && echo "[3] intro native == interpreter" || { echo "[3] FAIL"; exit 1; }
fi
if make -s build/settings-test && ./build/settings-test > build/v_settings.log 2>&1; then
  echo "[16] the settings file reads with a byte order mark and CRLF"
else
  cat build/v_settings.log; echo "[16] FAIL"; exit 1
fi
RC=0
if [ -n "$P4" ]; then
  if wait $P4; then cat build/v_scenarios.log; echo "[4] recorded scenarios match"
  else cat build/v_scenarios.log; echo "[4] FAIL"; RC=1; fi
  P4=
fi
if [ -n "$P5" ]; then
  if wait $P5; then cat build/v_fixes.log; echo "[5] original-bug fixes work"
  else cat build/v_fixes.log; echo "[5] FAIL"; RC=1; fi
  P5=
fi
if [ -n "$P6" ]; then
  if wait $P6; then cat build/v_mods.log; echo "[6] changed content works (more films, series and ads, starting pools, packs)"
  else cat build/v_mods.log; echo "[6] FAIL"; RC=1; fi
fi
if [ -n "$P7" ]; then
  if wait $P7; then cat build/v_mmpx.log; echo "[7] the MMPX filter works (presentation only)"
  else cat build/v_mmpx.log; echo "[7] FAIL"; RC=1; fi
fi
if [ -n "$P8" ]; then
  if wait $P8; then cat build/v_exefree.log; echo "[8] the decompiled build plays the same game without MTV.EXE"
  else cat build/v_exefree.log; echo "[8] FAIL"; RC=1; fi
fi
if [ -n "$P9" ]; then
  if wait $P9; then cat build/v_arrow.log; echo "[9] the system arrow works (the picture only)"
  else cat build/v_arrow.log; echo "[9] FAIL"; RC=1; fi
fi
if [ -n "$P10" ]; then
  if wait $P10; then cat build/v_seats.log; echo "[10] the seat scheduler plays the same game (--seats 1, and with a spectator)"
  else cat build/v_seats.log; echo "[10] FAIL"; RC=1; fi
fi
if [ -n "$P11" ]; then
  if wait $P11; then cat build/v_mpsave.log; echo "[11] a network game saved at night and loaded: the same game on every machine"
  else cat build/v_mpsave.log; echo "[11] FAIL"; RC=1; fi
fi
if [ -n "$P12" ]; then
  if wait $P12; then cat build/v_aiimp.log; echo "[12] the AI improvement works (follows, saved and loaded; off the original)"
  else cat build/v_aiimp.log; echo "[12] FAIL"; RC=1; fi
fi
if [ -n "$P13" ]; then
  if wait $P13; then grep "^playtest:\|^buildtest:\|^launchertest:\|^all ok\|^mpedgetest:\|^all [0-9]* ok" build/v_launcher.log; echo "[13] the launcher and editor in Go do what they did when recorded (tests/golden/)"
  else cat build/v_launcher.log; echo "[13] FAIL"; RC=1; fi
fi
if [ -n "$P14" ]; then
  if wait $P14; then cat build/v_castfit.log; echo "[14] the role fit in the shop works (the picture only)"
  else cat build/v_castfit.log; echo "[14] FAIL"; RC=1; fi
fi
if [ -n "$P15" ]; then
  if wait $P15; then cat build/v_sertop.log; echo "[15] the series card shows its topicality"
  else cat build/v_sertop.log; echo "[15] FAIL"; RC=1; fi
fi
if [ -n "$P17" ]; then
  if wait $P17; then tail -3 build/v_hr.log; echo "[17] drawing at 4x resolution works (the picture only)"
  else cat build/v_hr.log; echo "[17] FAIL"; RC=1; fi
fi
if [ -n "$P18" ]; then
  if wait $P18; then cat build/v_ki.log; echo "[18] the AI pictures are made as tools/hrai.py makes them (the picture only)"
  else cat build/v_ki.log; echo "[18] FAIL"; RC=1; fi
fi
if [ -n "$P19" ]; then
  if wait $P19; then tail -1 build/v_loadslot.log; echo "[19] a saved game of the safe straight from the start (--load-slot), as the setup loads it"
  else cat build/v_loadslot.log; echo "[19] FAIL"; RC=1; fi
fi
P4= P5= P6= P7= P8= P9= P10= P11= P12= P13= P14= P15= P17= P18= P19=
exit $RC
