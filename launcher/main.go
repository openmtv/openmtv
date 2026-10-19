// Command openmtv: the start of Mad TV for players -- what the former Python launcher (play.py) does,
// the packs' build of the former Python editor (editor.py) and the launcher page's server, in Go, so
// that a single player needs no Python (docs/launcher-ohne-python.md, stages 1-3).
//
//	build/openmtv                    the launcher: the page "Play" in the browser (server.go)
//	build/openmtv launcher [--port N] [--no-browser] [--game DIR] [--idle SECONDS]
//	build/openmtv --direct           the game at once, with the settings file
//	build/openmtv [ARG ...]          the game at once: the arguments of ./play.sh
//	build/openmtv --print [ARG ...]  the same preparation (game files, a mod built), then
//	                               the game's command line printed instead of started
//	build/openmtv -h | --help        the options, for players (in German when the settings
//	                               say german)
//	build/openmtv build NAME|A+B+C   a mod, or packs played together, built into
//	          [--game DIR]         build/mods/NAME (the former Python editor (editor.py) build)
//	build/openmtv serve OPTIONS      the network game's server (the former Python server (mpserver.py)'s
//	                               options: --seats N | --lobby | --public, ...)
//
// The same arguments, messages, files and command lines as the former Python launcher (play.py), on
// every system: tools/playtest.py runs both and compares them; the packs it
// builds are the editor's, byte for byte (tools/buildtest.py).  The program
// lives in build/ (make launcher; make launcher-win and launcher-mac cross-build
// build/openmtv.exe and build/darwin-arm64/openmtv); the decomp directory is the one
// above it.  The content editor is still the former Python editor (editor.py)'s (the launcher page
// starts it).  Standard library only.
package main

import (
	"fmt"
	"io"
	"os"
	"path/filepath"
	"runtime"
	"strings"
)

const windows = runtime.GOOS == "windows"

var (
	root = findRoot()  // the decomp directory
	exe  = ""          // what the system adds to a program's name
	play = "./play.sh" // how a player starts it, as the help shows it
)

func init() {
	if windows {
		exe, play = ".exe", "play.cmd"
	}
}

func findRoot() string {
	p, err := os.Executable()
	if err != nil {
		warn("play: %v\n", err)
		os.Exit(2)
	}
	// (in a Mac application, X.app/Contents/MacOS/openmtv: the application stands
	// where the program would, build/OpenMTV.app -- or, in the players' package
	// (tools/package.py mac), beside the decomp directory: OpenMTV/OpenMTV.app)
	if app := macApp(p); app != "" {
		if d := filepath.Join(filepath.Dir(app), "decomp"); exists(filepath.Join(d, "play.sh")) {
			return d
		}
		p = app
	}
	return filepath.Dir(filepath.Dir(p))
}

// macApp: X.app when the program P is a Mac application's (X.app/Contents/MacOS/P), else ""
func macApp(p string) string {
	if d := filepath.Dir(p); filepath.Base(d) == "MacOS" && filepath.Base(filepath.Dir(d)) == "Contents" &&
		strings.HasSuffix(filepath.Dir(filepath.Dir(d)), ".app") {
		return filepath.Dir(filepath.Dir(d))
	}
	return ""
}

// quarantineTargets: what the Mac application APP takes the quarantine off
// (macpkg_darwin.go) -- itself and the package's decomp directory beside it, not the
// folder they lie in: put straight into Downloads, every other file there would lose it
func quarantineTargets(app string) []string {
	return []string{app, filepath.Join(filepath.Dir(app), "decomp")}
}

func main() {
	if macRelaunched() { // (a Mac package started from where macOS moved it: started again from its place)
		return
	}
	os.Exit(run(os.Args[1:]))
}

func run(argv []string) int {
	if len(argv) == 0 { // the default: the launcher; any argument starts the game
		return launcher()
	}
	attachConsole() // (Windows' window build: play.cmd -h, --print, --direct write to its console)
	if f, ok := commands[argv[0]]; ok {
		return f(argv[1:])
	}
	switch argv[0] {
	case "-h", "--help", "/?", "-?", "help":
		return showHelp()
	case "build":
		return buildCommand(argv[1:])
	case "launcher":
		return launcherCommand(argv[1:])
	case "editor":
		return editorCommand(argv[1:])
	case "serve":
		return serveCommand(argv[1:])
	}
	show := false
	var args []string
	for _, a := range argv {
		switch a {
		case "--print":
			show = true
		case "--direct":
		default:
			args = append(args, a)
		}
	}
	cmd, err := prepareStart(args, false, !show)
	if err != nil {
		warn("play: %v\n", err)
		return 2
	}
	if show {
		if windows {
			say("%s\n", winQuote(cmd))
		} else {
			say("%s\n", strings.Join(cmd, " "))
		}
		return 0
	}
	return startGame(cmd) // (POSIX: in place of this process, as play.py's exec)
}

// commands: more of them in some builds (icon.go: icons, with -tags wails)
var commands = map[string]func([]string) int{}

// launcher: no arguments -- the launcher page in the browser (server.go); one running
// already is only shown again
func launcher() int {
	return launcherCommand(nil)
}

// say, warn: text on stdout, stderr -- on Windows with CRLF line ends, as Python's
// text mode (and the game's C runtime) writes them
func say(format string, a ...any)  { write(stdout, format, a...) }
func warn(format string, a ...any) { write(stderr, format, a...) }

var stdout, stderr io.Writer = os.Stdout, os.Stderr // (the tests read them)

func write(f io.Writer, format string, a ...any) {
	s := fmt.Sprintf(format, a...)
	if windows {
		s = strings.ReplaceAll(s, "\n", "\r\n")
	}
	io.WriteString(f, s)
}

// buildCommand: build NAME [--game DIR] -- a mod, or packs A+B+C, built into
// build/mods/NAME (what the former Python editor (editor.py) build does); prints the directory
func buildCommand(args []string) int {
	dir := gameDir()
	var rest []string
	for i := 0; i < len(args); i++ {
		if args[i] == "--game" && i+1 < len(args) {
			dir = args[i+1]
			i++
		} else {
			rest = append(rest, args[i])
		}
	}
	if len(rest) != 1 {
		warn("openmtv build NAME (or A+B+C) [--game DIR]\n")
		return 2
	}
	g, err := openGame(dir)
	if err != nil {
		warn("openmtv build: %v\n", err)
		return 1
	}
	out, err := buildMod(g, rest[0])
	if err != nil {
		warn("openmtv build: %v\n", err)
		return 1
	}
	say("%s\n", out)
	return 0
}
