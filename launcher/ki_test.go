package main

import (
	"bytes"
	"compress/gzip"
	"context"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"net/http"
	"net/http/httptest"
	"os"
	"path"
	"path/filepath"
	"runtime"
	"strings"
	"sync/atomic"
	"testing"
	"time"
)

// TestKiFetch: a file loaded is kept only with its checksum, one there already that
// checks is not loaded again
func TestKiFetch(t *testing.T) {
	body := []byte("the model's bytes")
	var asked atomic.Int32
	srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		asked.Add(1)
		w.Write(body)
	}))
	defer srv.Close()
	t.Setenv("KI_BASE", srv.URL+"/")
	h := sha256.Sum256(body)
	good := kiFile{"models/x.onnx", int64(len(body)), hex.EncodeToString(h[:])}
	dst := filepath.Join(t.TempDir(), "download", "x.onnx")

	bad := good
	bad.sha = "00" + good.sha[2:]
	if err := kiFetch(context.Background(), bad, dst); err == nil || exists(dst) || exists(dst+".part") {
		t.Fatalf("a wrong checksum: %v, kept %v/%v", err, exists(dst), exists(dst+".part"))
	}
	short := good
	short.size--
	if err := kiFetch(context.Background(), short, dst); err == nil || exists(dst) {
		t.Fatalf("too long: %v, kept %v", err, exists(dst))
	}
	if err := kiFetch(context.Background(), good, dst); err != nil {
		t.Fatal(err)
	}
	if b, _ := os.ReadFile(dst); string(b) != string(body) {
		t.Fatalf("loaded %q", b)
	}
	n := asked.Load()
	if err := kiFetch(context.Background(), good, dst); err != nil || asked.Load() != n {
		t.Fatalf("the file there again: %v, asked %d more", err, asked.Load()-n)
	}
}

// TestKiUse: "use" writes the set into both settings files and turns the HR display on;
// "delete" removes it, what a making left behind (not while the other model is made) and
// uses it no more -- in each settings file that has it
func TestKiUse(t *testing.T) {
	cfg := t.TempDir()
	t.Setenv("XDG_CONFIG_HOME", cfg)
	t.Setenv("APPDATA", cfg) // (Windows)
	if err := kiUse("rebout"); err == nil {
		t.Fatal("a set not made yet is used")
	}
	set := filepath.Join(kiDir(), "rebout.ki")
	model := filepath.Join(kiDir(), "download", "4x-Rebout-Blend.onnx")
	for _, p := range []string{set, model} {
		os.MkdirAll(filepath.Dir(p), 0o755)
		os.WriteFile(p, []byte("x"), 0o644)
	}
	if err := kiUse("rebout"); err != nil {
		t.Fatal(err)
	}
	mp := filepath.Join(filepath.Dir(settingsPath()), "multiplayer.ini")
	for _, p := range []string{settingsPath(), mp} {
		if v := settingsValue(p, "display", "ai_pictures"); v != set {
			t.Fatalf("%s: ai_pictures = %q", p, v)
		}
		if v := settingsValue(p, "display", "filter"); v != "hr-pixel" {
			t.Fatalf("%s: filter = %q", p, v)
		}
	}
	st := kiStatus("en")
	if m := st["models"].([]obj)[0]; m["state"] != "ready" || m["used"] != true {
		t.Fatalf("status: %v", m)
	}
	// made by an earlier --make-ki: old (it still shows); for other game files: stale
	gh, _ := contentHash(gameDir())
	here := fmt.Sprintf("%016x", gh)
	for _, c := range []struct {
		tool       int
		game, want string
	}{{kiTool - 1, here, "old"}, {kiTool - 1, "other", "stale"}, {kiTool, here, "ready"}} {
		b, _ := json.Marshal(kiMade{SHA: kiModels[0].model.sha, Game: c.game, Tool: c.tool})
		os.WriteFile(filepath.Join(kiDir(), "rebout.json"), b, 0o644)
		if m := kiStatus("en")["models"].([]obj)[0]; m["state"] != c.want {
			t.Fatalf("made with tool %d for %s: state %v, not %s", c.tool, c.game, m["state"], c.want)
		}
	}
	// the other model being made: its download stays
	kiNow.set(func(j *kiJob) { j.model, j.phase = "msu", "make" })
	if err := kiDelete("rebout"); err != nil {
		t.Fatal(err)
	}
	kiNow.set(func(j *kiJob) { j.model, j.phase = "", "" })
	for _, p := range []string{settingsPath(), mp} {
		if v := settingsValue(p, "display", "ai_pictures"); v != "" {
			t.Fatalf("%s after delete: ai_pictures = %q", p, v)
		}
	}
	if exists(set) || !exists(model) {
		t.Fatalf("while the other is made: the set there %v, the download gone %v", exists(set), !exists(model))
	}
	// the network games' file holds the set, the single player's another one: only that one changes
	other := filepath.Join(kiDir(), "msu.ki")
	os.WriteFile(set, []byte("x"), 0o644)
	os.WriteFile(other, []byte("x"), 0o644)
	if err := kiUse("msu"); err != nil {
		t.Fatal(err)
	}
	writeSettings([]settingChange{{"display", "ai_pictures", set}}, mp)
	if err := kiDelete("rebout"); err != nil {
		t.Fatal(err)
	}
	if v, w := settingsValue(settingsPath(), "display", "ai_pictures"), settingsValue(mp, "display", "ai_pictures"); v != other || w != "" {
		t.Fatalf("after delete: single player %q, network games %q", v, w)
	}
	if exists(set) || exists(filepath.Join(kiDir(), "download")) || !exists(other) {
		t.Fatalf("after delete: the set %v, download/ %v, the other set %v", exists(set), exists(filepath.Join(kiDir(), "download")), exists(other))
	}
	if err := kiDelete("msu"); err != nil {
		t.Fatal(err)
	}
	if exists(kiDir()) {
		t.Fatal("ki/ still there with nothing in it")
	}
}

// TestKiLeftovers: at the start, what a launcher ended outright left in ki/download/ goes --
// not while a making runs
func TestKiLeftovers(t *testing.T) {
	cfg := t.TempDir()
	t.Setenv("XDG_CONFIG_HOME", cfg)
	t.Setenv("APPDATA", cfg) // (Windows)
	d := filepath.Join(kiDir(), "download", "ort-1.31.0")
	os.MkdirAll(d, 0o755)
	os.WriteFile(filepath.Join(d, "lib"), []byte("x"), 0o644)
	kiNow.set(func(j *kiJob) { j.phase = "make" })
	kiLeftovers()
	kiNow.set(func(j *kiJob) { j.phase = "" })
	if !exists(d) {
		t.Fatal("removed while making")
	}
	kiLeftovers()
	if exists(filepath.Join(kiDir(), "download")) {
		t.Fatal("ki/download/ still there")
	}
}

func kiFileOf(p string, b []byte) kiFile {
	h := sha256.Sum256(b)
	return kiFile{p, int64(len(b)), hex.EncodeToString(h[:])}
}

// TestKiMake: the making with a stand-in for mtv-dec (a shell script) and a server of its
// own -- two at once refused, delete while making refused, cancel leaves no .part, a whole
// run gives a set (with the runtime unpacked into ort-1.31.0/ while it ran), a failure's
// reason is its last "--make-ki: " line, a crash its end; however it ends, ki/download/ is gone
func TestKiMake(t *testing.T) {
	if runtime.GOOS == "windows" {
		t.Skip("the stand-in is a shell script")
	}
	game := os.Getenv("GAME")
	for _, d := range []string{game, filepath.Join("..", "run"), filepath.Join("..", "..")} {
		if _, ok := contentHash(d); d != "" && ok {
			game = d
			break
		}
	}
	if _, ok := contentHash(game); !ok {
		t.Skip("no game files")
	}
	game, _ = filepath.Abs(game)
	var gz bytes.Buffer
	z := gzip.NewWriter(&gz)
	z.Write([]byte("the library"))
	z.Close()
	files := map[string][]byte{"rt.gz": gz.Bytes(), "m.onnx": []byte("model"), "m.pal": []byte("pal")}
	srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) { w.Write(files[path.Base(r.URL.Path)]) }))
	defer srv.Close()
	cfg := t.TempDir()
	t.Setenv("KI_BASE", srv.URL+"/")
	t.Setenv("XDG_CONFIG_HOME", cfg)
	t.Setenv("GAME", game)
	keptRoot, key := root, runtime.GOOS+"/"+runtime.GOARCH
	keptRT, keptModel := kiRuntimes[key], kiModels[0]
	defer func() { root, kiRuntimes[key], kiModels[0] = keptRoot, keptRT, keptModel }()
	root = t.TempDir()
	os.MkdirAll(filepath.Join(root, "build"), 0o755)
	stand := func(body string) {
		os.WriteFile(filepath.Join(root, "build", "mtv-dec"), []byte("#!/bin/sh\nout='' ort='' parent=''\nwhile [ $# -gt 0 ]; do [ \"$1\" = --ki-out ] && out=$2; [ \"$1\" = --ki-ort ] && ort=$2; [ \"$1\" = --ki-parent ] && parent=$2; shift; done\n"+body), 0o755)
	}
	kiRuntimes[key] = struct {
		file kiFile
		name string
		lib  kiFile
	}{kiFileOf("ort-1.31.0/rt.gz", files["rt.gz"]), "lib", kiFileOf("", []byte("the library"))}
	kiModels[0].model, kiModels[0].pal = kiFileOf("models/m.onnx", files["m.onnx"]), kiFileOf("models/m.pal", files["m.pal"])
	wait := func() obj {
		for i := 0; i < 500 && kiNow.running(""); i++ {
			time.Sleep(20 * time.Millisecond)
		}
		if exists(filepath.Join(kiDir(), "download")) {
			t.Fatal("ki/download/ left after the making")
		}
		return kiStatus("en")["job"].(obj)
	}

	stand("for i in 1 2 3 4 5; do echo \"ki $i of 5 ($i of 5)\"; sleep 0.2; done\necho x > \"$out\"\n")
	if err := kiMake("rebout", "en"); err != nil {
		t.Fatal(err)
	}
	if err := kiMake("rebout", "en"); err == nil {
		t.Fatal("two at once")
	}
	if err := kiDelete("rebout"); err == nil {
		t.Fatal("delete while making")
	}
	for i := 0; i < 500; i++ {
		if j := kiStatus("en")["job"].(obj); j["phase"] == "make" && j["done"].(int64) >= 2 {
			break
		}
		time.Sleep(10 * time.Millisecond)
	}
	kiCancel()
	if j := wait(); j["phase"] != "cancelled" || exists(filepath.Join(kiDir(), "rebout.ki.part")) || exists(filepath.Join(kiDir(), "rebout.ki")) {
		t.Fatalf("cancelled: %v", j)
	}
	stand("case \"$ort\" in */ort-1.31.0/lib) ;; *) echo \"--make-ki: runtime at $ort\" >&2; exit 1;; esac\n" +
		"[ \"$(cat \"$ort\")\" = 'the library' ] || { echo '--make-ki: runtime not unpacked' >&2; exit 1; }\n" +
		"[ \"$parent\" = \"$PPID\" ] || { echo \"--make-ki: --ki-parent $parent, not $PPID\" >&2; exit 1; }\n" +
		"for i in 1 2 3 4 5; do echo \"ki $i of 5 ($i of 5)\"; done\necho x > \"$out\"\n")
	if err := kiMake("rebout", "en"); err != nil {
		t.Fatal(err)
	}
	if j := wait(); j["phase"] != "done" || j["done"].(int64) != 5 {
		t.Fatalf("made: %v", j)
	}
	if m := kiStatus("en")["models"].([]obj)[0]; m["state"] != "ready" || !exists(filepath.Join(kiDir(), "rebout.json")) {
		t.Fatalf("the set: %v", m)
	}

	stand("echo '[ki] CoreML' >&2\necho '--make-ki: no game files here' >&2\necho '[ki] after' >&2\nexit 1\n")
	kiMake("rebout", "en")
	if j := wait(); j["phase"] != "failed" || j["error"] != "AI upscaling failed: no game files here" {
		t.Fatalf("failed: %v", j)
	}
	// no progress for kiMakeIdle: stopped, with its reason
	kiMakeIdle = 300 * time.Millisecond
	stand("echo 'ki 1 of 5 (1 of 5)'\nexec sleep 5\n")
	kiMake("rebout", "en")
	j := wait()
	kiMakeIdle = 10 * time.Minute
	if j["phase"] != "failed" || j["error"] != "AI upscaling stopped: no progress in 0 min" || exists(filepath.Join(kiDir(), "rebout.ki.part")) {
		t.Fatalf("stalled: %v", j)
	}
	stand("echo '[ki] CoreML' >&2\nkill -9 $$\n")
	kiMake("rebout", "en")
	if j := wait(); j["phase"] != "failed" || !strings.Contains(j["error"].(string), "killed") {
		t.Fatalf("crashed: %v", j)
	}
}
