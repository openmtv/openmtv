package main

import (
	"bytes"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

// The command line's answers -- help, the start's command line (--print), what it
// refuses -- in a decomp directory of the test's own (testdata/cli.json; go test
// -update).  No game is started: only --print and refused starts.

func TestCLI(t *testing.T) {
	paths := gamePaths(t)
	base := t.TempDir()
	troot := filepath.Join(base, "decomp")
	game := filepath.Join(base, "game")
	partial := filepath.Join(base, "game-without-source2")
	for _, d := range []string{filepath.Join(troot, "build"), game, partial, filepath.Join(base, "cfg-de", "madtv")} {
		os.MkdirAll(d, 0o777)
	}
	for _, p := range paths {
		copyFile(p, filepath.Join(game, filepath.Base(p)))
	}
	copyFile(paths[0], filepath.Join(partial, "SOURCE1.RSC"))
	os.WriteFile(filepath.Join(base, "cfg-de", "madtv", "settings.ini"), []byte("[game]\nlanguage = german\n"), 0o666)
	shipped, _ := filepath.Abs(filepath.Join("..", "tests", "packs")) // (the test's content packs)
	filepath.Walk(shipped, func(p string, info os.FileInfo, err error) error {
		if err != nil {
			return err
		}
		to := filepath.Join(troot, "mods", strings.TrimPrefix(p, shipped))
		if info.IsDir() {
			return os.MkdirAll(to, 0o777)
		}
		return copyFile(p, to)
	})
	stub := filepath.Join(troot, "build", "mtv-dec"+exe)
	os.WriteFile(stub, nil, 0o755)

	keptRoot, keptOut, keptErr := root, stdout, stderr
	defer func() { root, stdout, stderr = keptRoot, keptOut, keptErr }()
	root = troot
	t.Setenv("MADTV_CMD", "")
	got := map[string]any{}
	runCase := func(name string, env map[string]string, args ...string) {
		t.Setenv("GAME", game)
		t.Setenv("XDG_CONFIG_HOME", filepath.Join(base, "cfg-en"))
		for k, v := range env {
			t.Setenv(k, v)
		}
		var o, e bytes.Buffer
		stdout, stderr = &o, &e
		code := run(args)
		norm := func(s string) string {
			s = strings.ReplaceAll(s, troot, "ROOT")
			s = strings.ReplaceAll(s, base, "BASE")
			return s
		}
		got[name] = []any{code, norm(o.String()), norm(e.String())}
	}
	runCase("help", nil, "-h")
	runCase("help, German", map[string]string{"XDG_CONFIG_HOME": filepath.Join(base, "cfg-de")}, "--help")
	runCase("help, OpenMTV.cmd", map[string]string{"MADTV_CMD": "OpenMTV.cmd"}, "-h")
	runCase("print", nil, "--print")
	runCase("print, options", nil, "--print", "d", "--classic", "--scale", "4", "--fullscreen", "--fourth-speed=yes",
		"--start-money=500000", "--settings=/x/other.ini")
	runCase("print, a network game", nil, "--print", "--game-files", "build/mp/abc", "--mp", "127.0.0.1:7640", "--seat", "1",
		"--humans", "1,2", "--new-game", "--names", "Ann,Bo,", "--seed", "5", "--mouse=system")
	runCase("print, a pack", nil, "--print", "--mod", "test-filme", "e")
	runCase("unknown option", nil, "--print", "--frobnicate")
	runCase("a stray word", nil, "--print", "x")
	runCase("bare --settings", nil, "--print", "--settings", "e")
	runCase("empty --settings=", nil, "--print", "--settings=")
	runCase("--mod without a value", nil, "--print", "--mod")
	runCase("--scale without a value", nil, "--print", "--scale")
	runCase("--lifted", nil, "--print", "--lifted")
	runCase("--record", nil, "--record", "r.txt")
	runCase("intro", nil, "intro")
	runCase("a pack unknown", nil, "--print", "--mod", "gibtsnicht")
	runCase("a pack twice", nil, "--print", "--mod", "test-filme+test-filme")
	os.RemoveAll(filepath.Join(troot, "run")) // (the earlier starts copied them in)
	runCase("no SOURCE2.RSC", map[string]string{"GAME": partial}, "--print")
	os.Remove(stub)
	runCase("the game not built", nil, "--print", "d")
	runCase("build: no name", nil, "build")
	runCase("build: unknown pack", nil, "build", "gibtsnicht")
	runCase("build: a pack", nil, "build", "test-serien")
	golden(t, "cli.json", got)
}
