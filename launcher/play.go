package main

import (
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
)

// The player's start: the arguments of ./play.sh (play.cmd), the game files in
// place (run/, a mod's packs built), the game's command line.  The developers' ways
// too: the machine-level build (--lifted), a recording (--record FILE), the intro
// first (intro) -- they need the unpacked EXEs of make setup (build/MTV_U.EXE,
// build/INTRO_U.EXE) and the Linux or macOS builds.

// display and input options passed on to the game: alone, NAME=VALUE, or NAME VALUE
var (
	flags  = []string{"--fullscreen", "--integer", "--cast-fit", "--fourth-speed", "--fast-rooms", "--series-topicality", "--new-game"}
	valued = []string{"--scale", "--aspect", "--filter", "--mouse", "--start-money", "--credit-line", "--terror-damage", "--sammy-prize", "--ai-improve", "--ad-offers", "--film-offers",
		"--mp", "--seat", "--humans", "--difficulty", "--names", "--seed", "--knock", // (a network game: lobby.go)
		"--mp-save", "--mp-load", // (its savegames: src/mpsave.c)
		"--mp-key",    // (the seat's secret from the lobby: runtime/net.c)
		"--load-slot"} // (a saved game of the safe: src/main_loop.c)
	optional = []string{"--cast-fit", "--fourth-speed", "--fast-rooms", "--series-topicality"} // also NAME=VALUE, never NAME VALUE
)

// program: build/NAME (build/NAME.exe on Windows)
func program(name string) string {
	return filepath.Join(root, "build", name+exe)
}

// resolve: P as seen from BASE (an absolute P as it is; on Windows \x without a drive
// is on BASE's drive, as Python's os.path.abspath has it)
func resolve(base, p string) string {
	if filepath.IsAbs(p) {
		return filepath.Clean(p)
	}
	if p != "" && os.IsPathSeparator(p[0]) && filepath.VolumeName(p) == "" {
		return filepath.Join(filepath.VolumeName(base), p)
	}
	return filepath.Join(base, p)
}

// gameDir: the original game files -- $GAME (from the decomp directory), else the
// directory above it, else (no game files there) the one chosen in the launcher's
// window (state.go)
func gameDir() string {
	if g, ok := os.LookupEnv("GAME"); ok {
		return resolve(root, g)
	}
	d := resolve(root, "..")
	if !hasGameFiles(d) {
		if kept := loadState().Game; kept != "" && hasGameFiles(kept) {
			return kept
		}
	}
	return d
}

func exists(p string) bool {
	_, err := os.Stat(p)
	return err == nil
}

// copyFile: SRC's contents and permission bits to DST
func copyFile(src, dst string) error {
	in, err := os.Open(src)
	if err != nil {
		return err
	}
	defer in.Close()
	st, err := in.Stat()
	if err != nil {
		return err
	}
	out, err := os.OpenFile(dst, os.O_WRONLY|os.O_CREATE|os.O_TRUNC, 0o666)
	if err != nil {
		return err
	}
	if _, err = io.Copy(out, in); err != nil {
		out.Close()
		return err
	}
	if err = out.Close(); err != nil {
		return err
	}
	return os.Chmod(dst, st.Mode().Perm())
}

func copyMissing(names []string, src, dst string) error {
	for _, f := range names {
		if !exists(filepath.Join(dst, f)) {
			p := filepath.Join(src, f)
			if !exists(p) {
				return fmt.Errorf("%s not found in %s (set GAME to the directory with the original game files)", f, src)
			}
			if err := copyFile(p, filepath.Join(dst, f)); err != nil {
				return err
			}
		}
	}
	return nil
}

type options struct {
	lang, fixes, settings, mod, files string // ("": none)
	opts                              []string
	lifted, intro                     bool   // the developers' ways
	record                            string // ("": none)
}

func contains(list []string, s string) bool {
	for _, x := range list {
		if x == s {
			return true
		}
	}
	return false
}

func withValue(a string) bool {
	for _, v := range append(append([]string{}, valued...), optional...) {
		if strings.HasPrefix(a, v+"=") {
			return true
		}
	}
	return false
}

// usageError: an argument the start does not take
type usageError struct{ msg string }

func (e usageError) Error() string { return e.msg + " -- the options: " + play + " -h" }

// parse: the arguments of play.sh -> the options (what it does not take: an error)
func parse(args []string) (options, error) {
	o := options{settings: "--settings"}
	value := func(i int, a string) (string, error) {
		if i+1 >= len(args) || args[i+1] == "" {
			return "", usageError{a + " needs a value"}
		}
		return args[i+1], nil
	}
	for i := 0; i < len(args); i++ {
		a := args[i]
		switch {
		case a == "d" || a == "e":
			o.lang = a
		case a == "--classic":
			o.fixes = "--classic"
		case a == "--fixes" || strings.HasPrefix(a, "--fixes="):
			o.fixes = a
		case a == "--mod" || a == "--game-files": // (--game-files: a network game's packs, built elsewhere: build/mp/ID)
			v, err := value(i, a)
			if err != nil {
				return o, err
			}
			if a == "--mod" {
				o.mod = v
			} else {
				o.files = v
			}
			i++
		case strings.HasPrefix(a, "--mod="):
			if o.mod = a[len("--mod="):]; o.mod == "" {
				return o, usageError{"--mod needs a value"}
			}
		case contains(flags, a) || withValue(a):
			o.opts = append(o.opts, a)
		case contains(valued, a):
			v, err := value(i, a)
			if err != nil {
				return o, err
			}
			o.opts = append(o.opts, a, v)
			i++
		case strings.HasPrefix(a, "--settings="):
			if a == "--settings=" {
				return o, usageError{"--settings=FILE needs a file"}
			}
			o.settings = a
		case a == "--settings":
			return o, usageError{"--settings=FILE (with =)"}
		case a == "--lifted":
			o.lifted = true
		case a == "--record":
			v, err := value(i, a)
			if err != nil {
				return o, err
			}
			o.record = v
			i++
		case a == "intro":
			o.intro = true
		default:
			return o, usageError{fmt.Sprintf("unknown option %q", a)}
		}
	}
	return o, nil
}

// prepare: the game files in place (run/; a mod's packs built unless BUILT) -> the game's
// command line
func prepare(args []string, built bool) ([]string, error) {
	return prepareStart(args, built, false)
}

// needBuilt: the program at P is built, else why not (WHAT: what needs it)
func needBuilt(p, what string) error {
	if exists(p) {
		return nil
	}
	rel, _ := filepath.Rel(root, p)
	if windows {
		return fmt.Errorf("%s: %s is not built on Windows (only the decompiled build: make win)", what, rel)
	}
	return fmt.Errorf("%s: %s is not built yet (make)", what, rel)
}

// playIntro: INTRO.EXE before the game (its end is waited for; how it ends does not matter)
func playIntro(game string) error {
	if err := needBuilt(program("mtv-intro"), "intro"); err != nil {
		return err
	}
	d := filepath.Join(root, "run_intro")
	if err := os.MkdirAll(d, 0o777); err != nil {
		return err
	}
	if err := copyMissing([]string{"MCV.BIN", "DATAV.PCK"}, game, d); err != nil {
		return err
	}
	if exe := filepath.Join(root, "build", "INTRO_U.EXE"); exists(exe) && !exists(filepath.Join(d, "INTRO_U.EXE")) {
		if err := copyFile(exe, filepath.Join(d, "INTRO_U.EXE")); err != nil {
			return err
		}
	}
	c := exec.Command(program("mtv-intro"), "--game", d, "--exe", filepath.Join(d, "INTRO_U.EXE"))
	c.Dir, c.Stdin, c.Stdout, c.Stderr = root, os.Stdin, stdout, stderr
	c.Run()
	return nil
}

// prepareStart: prepare, the intro played first when INTRO (a start, not --print) and asked for
func prepareStart(args []string, built, intro bool) ([]string, error) {
	o, err := parse(args)
	if err != nil {
		return nil, err
	}
	game := gameDir()
	run := filepath.Join(root, "run")
	if err := os.MkdirAll(run, 0o777); err != nil {
		return nil, err
	}
	gamedir := run
	// MTV_U.EXE: only the machine-level build (--lifted, --record) needs it (build/mtv-dec
	// carries what it needs of it: tools/exeimage.py)
	unpacked := filepath.Join(root, "build", "MTV_U.EXE")
	if (o.lifted || o.record != "") && exists(unpacked) && !exists(filepath.Join(run, "MTV_U.EXE")) {
		if err := copyFile(unpacked, filepath.Join(run, "MTV_U.EXE")); err != nil {
			return nil, err
		}
	}
	if o.intro && intro {
		if err := playIntro(game); err != nil {
			return nil, err
		}
	}
	if o.files != "" {
		gamedir = resolve(root, o.files) // (game files built elsewhere need no copy of the originals)
	} else {
		if err := copyMissing([]string{"SOURCE1.RSC", "SOURCE2.RSC"}, game, run); err != nil {
			return nil, err
		}
		if o.mod != "" && o.record != "" {
			return nil, fmt.Errorf("--record plays the original game files, not a mod")
		}
		if o.mod != "" {
			if !built {
				g, err := openGame(game)
				if err == nil {
					_, err = buildMod(g, o.mod)
				}
				if err != nil {
					return nil, fmt.Errorf("the mod %s was not built: %v", o.mod, err)
				}
			}
			gamedir = filepath.Join(root, "build", "mods", o.mod)
		}
	}
	var lang []string
	if o.lang != "" {
		lang = []string{"--args", o.lang}
	}
	if o.record != "" {
		if err := needBuilt(program("mtv"), "--record"); err != nil {
			return nil, err
		}
		// fresh game files: saved games inside the .rsc files would change the start
		gamedir = filepath.Join(root, "build", "record_game")
		os.RemoveAll(gamedir)
		if err := os.MkdirAll(gamedir, 0o777); err != nil {
			return nil, err
		}
		for _, f := range []string{filepath.Join(game, "SOURCE1.RSC"), filepath.Join(game, "SOURCE2.RSC"), unpacked} {
			if err := copyFile(f, filepath.Join(gamedir, filepath.Base(f))); err != nil {
				return nil, err
			}
		}
		g, ok := os.LookupEnv("GAME")
		if !ok {
			g = ".."
		}
		say("recording to %s (machine-level build)\n", o.record)
		say("replay: mkdir -p /tmp/g && cp %s/SOURCE*.RSC build/MTV_U.EXE /tmp/g && build/mtv --headless --game /tmp/g "+
			"--script %s --args <language from its first line>\n", g, o.record)
		cmd := append(append([]string{program("mtv"), "--game", gamedir}, lang...), "--record", o.record, o.settings)
		return append(cmd, o.opts...), nil
	}
	if o.lifted { // (always the original: no fixes, its own timing)
		if err := needBuilt(program("mtv"), "--lifted"); err != nil {
			return nil, err
		}
		cmd := append(append([]string{program("mtv"), "--game", gamedir}, lang...), o.settings)
		return append(cmd, o.opts...), nil
	}
	if !exists(program("mtv-dec")) {
		rel, _ := filepath.Rel(root, program("mtv-dec"))
		if windows {
			return nil, fmt.Errorf("the game: %s is not built yet (make win)", rel)
		}
		return nil, fmt.Errorf("the game: %s is not built yet (make)", rel)
	}
	cmd := append([]string{program("mtv-dec"), "--game", gamedir}, lang...)
	if o.fixes != "" {
		cmd = append(cmd, o.fixes)
	}
	cmd = append(cmd, "--logical-time", o.settings)
	return append(cmd, o.opts...), nil
}
