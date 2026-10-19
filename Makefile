CC      ?= gcc
# sdl2-config from PATH, else Homebrew's (not on PATH in non-login shells)
ifndef SDL2_CONFIG
SDL2_CONFIG := $(firstword $(shell command -v sdl2-config) $(wildcard /opt/homebrew/bin/sdl2-config /usr/local/bin/sdl2-config) sdl2-config)
endif
CFLAGS  ?= -O2 -g -Wall -Wno-unused-function -Iruntime $(shell $(SDL2_CONFIG) --cflags)
# SDL2_STATIC=1: SDL2 built in (the Linux and macOS packages: tools/sdl2_static.py, tools/package.py)
LDLIBS  = $(shell $(SDL2_CONFIG) $(if $(SDL2_STATIC),--static-libs,--libs)) -lm
ifeq ($(shell uname),Darwin)
LDLIBS += -lobjc                     # (runtime/host.c: the window's occlusion, App Nap -- AppKit through the Objective-C runtime)
endif
# Same results with every compiler and CPU (all objects): no a*b+c -> FMA
# contraction (clang's default on arm64).  The PIT and AdLib timer timing in
# runtime/ uses doubles, where one rounding difference would shift every
# hash; no current expression is contracted, the flag keeps it that way.
# -fno-common is gcc's default; Apple clang still makes tentative
# definitions common symbols (and ld64 then warns about their alignment).
XFLAGS  = -ffp-contract=off -fno-common
# Decompiled C (src/): a division by zero traps on every CPU (x86 traps by
# itself, arm64 silently yields 0); runtime/divtrap.c turns the trap into the
# 8086's divide error.  gcc and clang both accept these two flags.
DIVTRAP = -fsanitize=integer-divide-by-zero -fsanitize-undefined-trap-on-error
RT_SRC  = runtime/alu.c runtime/strops.c runtime/interp.c runtime/dispatch.c runtime/hw.c runtime/bios.c runtime/dos.c runtime/platform.c runtime/host.c runtime/main.c runtime/difftest.c runtime/adlib.c runtime/vga.c runtime/divtrap.c runtime/clock.c runtime/clock_none.c runtime/settings.c runtime/mmpx.c runtime/coro.c runtime/seat.c runtime/net.c
RT_OBJ  = $(patsubst runtime/%.c,build/obj/%.o,$(RT_SRC))
GEN_SRC = $(wildcard gen/*.c)
GEN_OBJ = $(patsubst gen/%.c,build/obj/gen_%.o,$(GEN_SRC))

all: build/mtv build/mtv-dec build/mtv-intro

# ---- first build from a fresh checkout: unpack the original EXEs and
# regenerate the machine-level C (gen/, gen_intro/ are not in git).
#   make setup GAME=/path/to/original/files
GAME ?= ..
build/MTV_U.EXE:
	mkdir -p build && python3 tools/unlzexe.py $(GAME)/MTV.EXE $@
build/INTRO_U.EXE:
	mkdir -p build && python3 tools/unlzexe.py $(GAME)/INTRO.EXE $@
gen: build/MTV_U.EXE
	python3 tools/lift.py build/MTV_U.EXE gen
gen_intro: build/INTRO_U.EXE
	python3 tools/lift.py build/INTRO_U.EXE gen_intro
setup: gen gen_intro
	mkdir -p run run_intro
	for f in $(GAME)/SOURCE1.RSC $(GAME)/SOURCE2.RSC build/MTV_U.EXE; do [ -f run/$${f##*/} ] || cp $$f run/; done
	for f in $(GAME)/MCV.BIN $(GAME)/DATAV.PCK build/INTRO_U.EXE; do [ -f run_intro/$${f##*/} ] || cp $$f run_intro/; done
	$(MAKE) all

# src/ is hand-maintained since the freeze (git tag generated-final).
# The decompiler output is only a reference now: it goes to build/generated/
# (compare with diff -r; DECOMP_WHY=all explains refused functions).
decomp-ref: build/MTV_U.EXE
	mkdir -p build/generated
	cp src/dec.h src/dec_rt.c build/generated/
	python3 tools/decomp.py build/MTV_U.EXE names/rtl_names.json build/generated

check: all
	./verify.sh

# runtime/settings.c on its own: a settings file with a byte order mark, CRLF (verify.sh)
build/settings-test: tests/settings_test.c runtime/settings.c runtime/runtime.h
	mkdir -p build && $(CC) -O1 -Wall -Iruntime $(shell $(SDL2_CONFIG) --cflags) tests/settings_test.c runtime/settings.c -o $@
settings-test: build/settings-test
	./build/settings-test
.PHONY: settings-test

build/obj/%.o: runtime/%.c runtime/*.h | build/obj
	$(CC) $(CFLAGS) $(XFLAGS) -DSTACKCHK -c $< -o $@
build/obj/gen_%.o: gen/%.c runtime/*.h gen/*.h | build/obj
	$(CC) $(GENCFLAGS) $(CFLAGS) $(XFLAGS) -c $< -o $@
build/obj:
	mkdir -p build/obj

# OPL3_SHARED=1: Nuked-OPL3 (LGPL-2.1) as a library of its own beside the game, which a
# player may replace with their own build of it (the players' packages, tools/package.py);
# without it, linked in.  (A different OPL3_SHARED needs a fresh link: rm build/mtv-dec.)
ifdef OPL3_SHARED
ifeq ($(shell uname),Darwin)
OPL3_LIB      = build/libopl3.dylib
OPL3_LIBFLAGS = -dynamiclib -install_name @rpath/libopl3.dylib
OPL3_RPATH    = -Wl,-rpath,@executable_path
else
OPL3_LIB      = build/libopl3.so
OPL3_LIBFLAGS = -shared -Wl,-soname,libopl3.so
OPL3_RPATH    = -Wl,-rpath,'$$ORIGIN'
endif
$(OPL3_LIB): third_party/nuked-opl3/opl3.c third_party/nuked-opl3/opl3.h
	mkdir -p build && $(CC) -O2 $(XFLAGS) -fPIC $(OPL3_LIBFLAGS) $< -o $@
LDLIBS += -Lbuild -lopl3 $(OPL3_RPATH)
else
RT_OBJ += build/obj/opl3.o
endif
build/obj/opl3.o: third_party/nuked-opl3/opl3.c | build/obj
	$(CC) -O2 $(XFLAGS) -c $< -o $@

build/mtv-interp: $(RT_OBJ) build/obj/native_none.o
	$(CC) -o $@ $^ $(LDLIBS)
build/obj/native_none.o: runtime/native_none.c runtime/*.h | build/obj
	$(CC) $(CFLAGS) $(XFLAGS) -c $< -o $@

# entries counted by the logical clock: the decompiled functions (src/)
SRC_SRC = $(wildcard src/*.c)
build/clock_entries.c: tools/clock_table.py $(SRC_SRC) | build/obj
	python3 tools/clock_table.py $@ $(SRC_SRC)
build/obj/clock_entries.o: build/clock_entries.c runtime/*.h | build/obj
	$(CC) $(CFLAGS) $(XFLAGS) -c $< -o $@

build/mtv: $(RT_OBJ) $(GEN_OBJ) build/obj/clock_entries.o
	$(CC) -o $@ $^ $(LDLIBS)

SRC_OBJ = $(patsubst src/%.c,build/obj/src_%.o,$(SRC_SRC))
build/obj/src_%.o: src/%.c src/*.h runtime/*.h gen/lifted.h | build/obj
	$(CC) $(CFLAGS) $(XFLAGS) $(DIVTRAP) -fno-strict-aliasing -Isrc -Wno-unused-label -Wno-unused-variable -Wno-unused-but-set-variable -c $< -o $@
# what the decompiled build needs of MTV.EXE (its initial data segment), so
# that it runs without it: made from the EXE at build time, like gen/
build/exe_image.c: tools/exeimage.py build/MTV_U.EXE
	python3 tools/exeimage.py build/MTV_U.EXE $@
build/obj/exe_image.o: build/exe_image.c runtime/*.h | build/obj
	$(CC) $(CFLAGS) $(XFLAGS) -c $< -o $@
# --filter hr's rules and 4x font (src/hr_data.h), from their text files
# (the directory too: a trial font removed from try/ changes it)
HR_DATA_SRC = tools/hrdata.py tools/hrfont.py runtime/hr_rules.txt runtime/ki_parts.txt tools/hrfont $(wildcard tools/hrfont/*.txt tools/hrfont/try/*/*.txt)
build/hr_data.c: $(HR_DATA_SRC) | build/obj
	python3 tools/hrdata.py $@
build/obj/hr_data.o: build/hr_data.c src/hr_data.h | build/obj
	$(CC) $(CFLAGS) $(XFLAGS) -Isrc -c $< -o $@
build/mtv-dec: $(RT_OBJ) $(GEN_OBJ) $(SRC_OBJ) build/obj/clock_entries.o build/obj/exe_image.o build/obj/hr_data.o | $(OPL3_LIB)
	$(CC) -o $@ $^ $(LDLIBS)

# the inventory for the multiplayer plan (tools/dgtrace.c, tools/mpinventory.py):
# every data-segment access with the function and frame part making it
TRACE_OBJ = $(patsubst src/%.c,build/obj/trace_%.o,$(SRC_SRC))
TRACE_FLAGS = -DDG_TRACE -finstrument-functions -finstrument-functions-exclude-file-list=src/dec.h,src/rtl.h,src/game_types.h,src/game_globals.h,runtime/
build/obj/trace_%.o: src/%.c src/*.h runtime/*.h gen/lifted.h | build/obj
	$(CC) $(CFLAGS) $(XFLAGS) $(DIVTRAP) $(TRACE_FLAGS) -fno-strict-aliasing -Isrc -Wno-unused-label -Wno-unused-variable -Wno-unused-but-set-variable -c $< -o $@
build/obj/dgtrace.o: tools/dgtrace.c src/*.h runtime/*.h | build/obj
	$(CC) $(CFLAGS) $(XFLAGS) -DDG_TRACE -fno-strict-aliasing -Isrc -c $< -o $@
build/mtv-dec-trace: $(RT_OBJ) $(GEN_OBJ) $(TRACE_OBJ) build/obj/clock_entries.o build/obj/exe_image.o build/obj/hr_data.o build/obj/dgtrace.o
	$(CC) -o $@ $^ $(LDLIBS)

# MMPX for the tools (tools/hrfont.py: the 4x font's first draft)
build/mmpx4: tools/mmpx4.c runtime/mmpx.c runtime/*.h
	mkdir -p build && $(CC) -O2 -Wall -Iruntime tools/mmpx4.c runtime/mmpx.c -o $@

# the measuring run for drawing at 4x resolution (docs/hochaufloesend.md, stage 1;
# tools/hrsurvey.c, tools/hrsurvey.py): src/ with the hooks of src/hrs.h and a shadow stack
HRS_OBJ = $(patsubst src/%.c,build/obj/hrs_%.o,$(SRC_SRC))
HRS_FLAGS = -DHR_SURVEY -finstrument-functions -finstrument-functions-exclude-file-list=src/dec.h,src/rtl.h,src/game_types.h,src/game_globals.h,src/hrs.h,runtime/
build/obj/hrs_%.o: src/%.c src/*.h runtime/*.h gen/lifted.h | build/obj
	$(CC) $(CFLAGS) $(XFLAGS) $(DIVTRAP) $(HRS_FLAGS) -fno-strict-aliasing -Isrc -Wno-unused-label -Wno-unused-variable -Wno-unused-but-set-variable -c $< -o $@
build/obj/hrsurvey.o: tools/hrsurvey.c src/*.h runtime/*.h | build/obj
	$(CC) $(CFLAGS) $(XFLAGS) -DHR_SURVEY -fno-strict-aliasing -Isrc -c $< -o $@
build/mtv-dec-hrs: $(RT_OBJ) $(GEN_OBJ) $(HRS_OBJ) build/obj/clock_entries.o build/obj/exe_image.o build/obj/hr_data.o build/obj/hrsurvey.o | $(OPL3_LIB)
	$(CC) -o $@ $^ $(LDLIBS)

clean:
	rm -rf build/obj build/mtv build/mtv-interp build/win
.PHONY: all clean setup decomp-ref check gen gen_intro win

# ---- Windows x86-64, cross-built with MinGW-w64 (Debian: gcc-mingw-w64-x86-64):
#   make win     ->  build/mtv-dec.exe + build/SDL2.dll (the game; objects in build/win/)
# The same sources and flags as build/mtv-dec.  SDL2: the official MinGW
# development package, fetched and checked by tools/fetch_sdl2_mingw.py.  No
# SDL2main: main() stays main() (a console program: stdout for --headless).
# The differential tester needs POSIX signals: runtime/difftest_none.c instead.
# __USE_MINGW_ANSI_STDIO: printf/scanf as in C99 (%llu, %zu) instead of msvcrt's.
# An 8 MB stack like Linux (the PE default is 2 MB).
WIN_CC   ?= x86_64-w64-mingw32-gcc
WIN_NM   ?= x86_64-w64-mingw32-nm
WIN_SDL2 ?= build/sdl2-mingw/x86_64-w64-mingw32
WIN_CFLAGS = -O2 -g -Wall -Wno-unused-function -Iruntime -I$(WIN_SDL2)/include/SDL2 -D__USE_MINGW_ANSI_STDIO=1
WIN_LDLIBS = -L$(WIN_SDL2)/lib -lSDL2 -lws2_32 -Wl,--stack,8388608
WIN_RT_OBJ = $(patsubst runtime/%.c,build/win/obj/%.o,$(filter-out runtime/difftest.c,$(RT_SRC)) runtime/difftest_none.c)
ifdef OPL3_SHARED
WIN_OPL3 = build/opl3.dll
WIN_LDLIBS += -Lbuild/win -lopl3
build/opl3.dll: third_party/nuked-opl3/opl3.c third_party/nuked-opl3/opl3.h | build/win/obj
	$(WIN_CC) -O2 $(XFLAGS) -shared $< -o $@ -Wl,--out-implib,build/win/libopl3.dll.a
else
WIN_RT_OBJ += build/win/obj/opl3.o
endif
WIN_GEN_OBJ = $(patsubst gen/%.c,build/win/obj/gen_%.o,$(GEN_SRC))
WIN_SRC_OBJ = $(patsubst src/%.c,build/win/obj/src_%.o,$(SRC_SRC))
WIN_SDL2_H = $(WIN_SDL2)/include/SDL2/SDL.h

win: build/mtv-dec.exe build/SDL2.dll
$(WIN_SDL2_H) $(WIN_SDL2)/bin/SDL2.dll: tools/fetch_sdl2_mingw.py
	python3 tools/fetch_sdl2_mingw.py
	touch $(WIN_SDL2_H)
build/SDL2.dll: $(WIN_SDL2)/bin/SDL2.dll
	cp $< $@
build/win/obj:
	mkdir -p build/win/obj
build/win/obj/%.o: runtime/%.c runtime/*.h | build/win/obj $(WIN_SDL2_H)
	$(WIN_CC) $(WIN_CFLAGS) $(XFLAGS) -DSTACKCHK -c $< -o $@
build/win/obj/gen_%.o: gen/%.c runtime/*.h gen/*.h | build/win/obj
	$(WIN_CC) $(GENCFLAGS) $(WIN_CFLAGS) $(XFLAGS) -c $< -o $@
build/win/obj/src_%.o: src/%.c src/*.h runtime/*.h gen/lifted.h | build/win/obj
	$(WIN_CC) $(WIN_CFLAGS) $(XFLAGS) $(DIVTRAP) -fno-strict-aliasing -Isrc -Wno-unused-label -Wno-unused-variable -Wno-unused-but-set-variable -c $< -o $@
build/win/obj/opl3.o: third_party/nuked-opl3/opl3.c | build/win/obj
	$(WIN_CC) -O2 $(XFLAGS) -c $< -o $@
build/win/obj/clock_entries.o: build/clock_entries.c runtime/*.h | build/win/obj
	$(WIN_CC) $(WIN_CFLAGS) $(XFLAGS) -c $< -o $@
build/win/obj/exe_image.o: build/exe_image.c runtime/*.h | build/win/obj
	$(WIN_CC) $(WIN_CFLAGS) $(XFLAGS) -c $< -o $@
# GNU ld leaves a weak definition that nothing overrides undefined on PE-COFF:
# tools/winweak.py defines each such one as its fallback (ELF's rule)
build/win/obj/hr_data.o: build/hr_data.c src/hr_data.h | build/win/obj
	$(WIN_CC) $(WIN_CFLAGS) $(XFLAGS) -Isrc -c $< -o $@
WIN_OBJ = $(WIN_RT_OBJ) $(WIN_GEN_OBJ) $(WIN_SRC_OBJ) build/win/obj/clock_entries.o build/win/obj/exe_image.o build/win/obj/hr_data.o
build/win/weak.rsp: tools/winweak.py $(WIN_OBJ)
	python3 tools/winweak.py $(WIN_NM) $(WIN_OBJ) > $@
build/mtv-dec.exe: $(WIN_OBJ) build/win/weak.rsp | $(WIN_OPL3)
	$(WIN_CC) -o $@ $(WIN_OBJ) @build/win/weak.rsp $(WIN_LDLIBS)

# ---- INTRO.EXE (lifted into gen_intro/)
GENI_SRC = $(wildcard gen_intro/*.c)
GENI_OBJ = $(patsubst gen_intro/%.c,build/obj/genI_%.o,$(GENI_SRC))
build/obj/genI_%.o: gen_intro/%.c runtime/*.h gen_intro/*.h | build/obj
	$(CC) -O1 -Igen_intro $(CFLAGS) $(XFLAGS) -c $< -o $@
build/mtv-intro: $(RT_OBJ) $(GENI_OBJ)
	$(CC) -o $@ $^ $(LDLIBS)

# ---- the players' start in Go (launcher/: the start, the packs' build, the launcher page,
# multiplayer and its server, the content editor, without Python).  Not part of `all` -- it
# needs Go; ./play.sh needs it.  Every system is built from here, without cgo:
#   make launcher        build/openmtv for this system
#   make launcher-win    build/openmtv.exe (Windows x86-64: the launcher in its own window, below)
#   make launcher-mac    build/darwin-arm64/openmtv (Apple silicon; copy it to the Mac's build/openmtv)
# Checks (tests/golden/, the test's content packs tests/packs): python3 tools/playtest.py (on
# Windows with the game's stand-in for the starts: make playtest-win -> build/playstub.exe
# beside openmtv.exe), its packs: python3 tools/buildtest.py, the launcher page and the editor:
# python3 tools/launchertest.py; cd launcher && go test (the original's tables through export
# and build, as tools/content.py)
GO       ?= go
GO_SRC    = launcher/go.mod $(wildcard launcher/*.go) launcher/settings_defaults.txt launcher/web/index.html
# the pages built in (launcher/web/, a copy of tools/editor/: launcher/server.go webFS)
launcher/web/index.html: $(wildcard tools/editor/* tools/editor/fonts/*)
	rm -rf launcher/web && mkdir -p launcher/web && cp -R tools/editor/. launcher/web/ && touch launcher/web/.keep
# the version built in: git's, or build/COMMIT.txt where there is no git (a copy made by git archive)
VERSION12 := $(shell [ "$$(git rev-parse --show-toplevel 2>/dev/null)" = "$$(pwd -P)" ] && git rev-parse --short=12 HEAD || cat build/COMMIT.txt 2>/dev/null)
GO_BUILD  = CGO_ENABLED=0 $(GO) build -trimpath -ldflags "-s -w -X main.version=$(VERSION12)"
launcher: build/openmtv
launcher-win: build/openmtv.exe
launcher-mac: build/darwin-arm64/openmtv
playtest-win: build/openmtv.exe build/playstub.exe
build/openmtv: $(GO_SRC)
	cd launcher && $(GO_BUILD) -o ../$@ .
build/darwin-arm64/openmtv: $(GO_SRC)
	cd launcher && GOOS=darwin GOARCH=arm64 $(GO_BUILD) -o ../$@ .
build/playstub.exe: launcher/stub/main.go
	cd launcher && GOOS=windows GOARCH=amd64 $(GO_BUILD) -o ../$@ ./stub
# the launcher in a window of its own (launcher/window_wails.go, Wails v2 on the system's web
# view; docs/launcher-fenster.md) -- the players' on Windows and the Mac; build/openmtv stays
# the browser's (Linux's default where WebKitGTK is missing, the server's image):
#   make launcher-win         build/openmtv.exe: the window (WebView2, without cgo), a program
#                             of the window subsystem -- no console window; with arguments it
#                             takes the console of the command line it was started from
#   make launcher-native      build/openmtv-native for this system, with cgo -- Linux: WebKitGTK 4.1
#                             (to build: libwebkit2gtk-4.1-dev; to run: libwebkit2gtk-4.1-0,
#                             ./play.sh takes build/openmtv where it is missing); macOS: on the
#                             Mac itself (the SDK; Wails v2 misses the framework
#                             UniformTypeIdentifiers, added here)
#   make launcher-native-app  build/OpenMTV.app (on the Mac: the program with its icon, Dock and Finder)
# The icons: OpenMTV's own television, drawn by launcher/icon.go (openmtv icons) -- made here, never kept.
WAILS_VERSION = -X main.version=$(VERSION12)
ifeq ($(shell uname),Darwin)
WAILS_NATIVE = CGO_LDFLAGS="-framework UniformTypeIdentifiers" CGO_ENABLED=1 $(GO) build -tags "wails production"
else
WAILS_NATIVE = CGO_ENABLED=1 $(GO) build -tags "wails production webkit2_41"
endif
launcher-native: build/openmtv-native
launcher-native-app: build/OpenMTV.app
WAILS_SRC = $(GO_SRC) $(wildcard launcher/winres/*.go)
build/openmtv-native: $(WAILS_SRC)
	cd launcher && $(WAILS_NATIVE) -trimpath -ldflags "-s -w $(WAILS_VERSION)" -o ../$@ .
build/icons/icon.ico: build/openmtv
	build/openmtv icons build/icons
build/icons/icon.icns: build/icons/icon.ico
launcher/winres/rsrc_windows_amd64.syso: launcher/winres/madtv.rc launcher/winres/madtv.manifest build/icons/icon.ico
	x86_64-w64-mingw32-windres -I build/icons -I launcher/winres -i $< -O coff -o $@
build/openmtv.exe: $(WAILS_SRC) launcher/winres/rsrc_windows_amd64.syso
	cd launcher && GOOS=windows GOARCH=amd64 CGO_ENABLED=0 $(GO) build -tags "wails production" -trimpath \
	  -ldflags "-s -w -H windowsgui $(WAILS_VERSION)" -o ../$@ .
build/OpenMTV.app: build/openmtv-native build/icons/icon.icns launcher/Info.plist
	rm -rf $@ && mkdir -p $@/Contents/MacOS $@/Contents/Resources
	cp build/openmtv-native $@/Contents/MacOS/openmtv && cp build/icons/icon.icns $@/Contents/Resources/icon.icns
	sed 's/@VERSION@/$(VERSION12)/' launcher/Info.plist > $@/Contents/Info.plist
	codesign --force -s - $@   # (ad hoc: its own name for macOS, not the linker's "a.out"; no Developer ID)
.PHONY: launcher launcher-win launcher-mac launcher-native launcher-native-app playtest-win
