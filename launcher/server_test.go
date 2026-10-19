package main

import (
	"crypto/sha256"
	"encoding/hex"
	"net"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"testing"
)

// testServer: a launcher's server on a directory of pages of its own (no game)
func testServer(t *testing.T) *server {
	static := t.TempDir()
	os.WriteFile(filepath.Join(static, "index.html"), []byte("<html><head></head></html>"), 0o666)
	os.WriteFile(filepath.Join(static, "app.js"), []byte("1"), 0o666)
	return &server{ed: &editorData{}, static: static, quit: make(chan struct{}), lobbies: map[string]*lobby{}}
}

func get(s *server, host, path string, hdr map[string]string) *httptest.ResponseRecorder {
	r := httptest.NewRequest("GET", path, nil)
	r.Host = host
	for k, v := range hdr {
		r.Header.Set(k, v)
	}
	w := httptest.NewRecorder()
	s.ServeHTTP(w, r)
	return w
}

// every answer -- the pages, the API, the refusals -- forbids being shown in another
// site's frame (clickjacking)
func TestNoFraming(t *testing.T) {
	s := testServer(t)
	lang := map[string]string{"X-UI-Lang": "en"}
	for _, c := range []struct {
		host, path string
		hdr        map[string]string
		code       int
	}{
		{"127.0.0.1:7650", "/", nil, 200},
		{"127.0.0.1:7650", "/app.js", nil, 200},
		{"localhost:7650", "/nothing.js", nil, 404},
		{"127.0.0.1:7650", "/api/hello", lang, 200},
		{"127.0.0.1:7650", "/api/hello", nil, 403},
		{"evil.example:7650", "/", nil, 403},
	} {
		w := get(s, c.host, c.path, c.hdr)
		if w.Code != c.code {
			t.Errorf("%s %s: %d, not %d", c.host, c.path, w.Code, c.code)
		}
		if got := w.Header().Get("X-Frame-Options"); got != "DENY" {
			t.Errorf("%s %s: X-Frame-Options %q", c.host, c.path, got)
		}
		if got := w.Header().Get("Content-Security-Policy"); got != "frame-ancestors 'none'" {
			t.Errorf("%s %s: Content-Security-Policy %q", c.host, c.path, got)
		}
	}
}

// an answering launcher is this one's only when it is of the same installation: a
// second installation's start does not open the first one's page
func TestRunningSameInstall(t *testing.T) {
	keptRoot := root
	defer func() { root = keptRoot }()
	root = t.TempDir()
	serveOn := func(h http.Handler) int {
		ln, err := net.Listen("tcp", "127.0.0.1:0")
		if err != nil {
			t.Fatal(err)
		}
		srv := &http.Server{Handler: h}
		go srv.Serve(ln)
		t.Cleanup(func() { srv.Close() })
		return ln.Addr().(*net.TCPAddr).Port
	}
	port := serveOn(testServer(t))
	if !running(port) {
		t.Error("this installation's launcher: not running")
	}
	hello := func(text string) int {
		return serveOn(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) { w.Write([]byte(text)) }))
	}
	sum := sha256.Sum256([]byte(filepath.Join(root, "other")))
	if running(hello(`{"launcher": "madtv", "version": "1", "pid": 1, "install": "` + hex.EncodeToString(sum[:12]) + `"}`)) {
		t.Error("another installation's launcher taken as this one's")
	}
	if running(hello(`{"launcher": "madtv", "version": "1", "pid": 1}`)) { // (a launcher before the installation's hash)
		t.Error("a launcher that does not say its installation taken as this one's")
	}
}

// only plain files are served: not a device (Windows' CON, NUL ..., here a link to
// /dev/null) or a pipe, whose reading may never end
func TestStaticOnlyFiles(t *testing.T) {
	if windows {
		t.Skip("(a link to a device)")
	}
	s := testServer(t)
	if err := os.Symlink(os.DevNull, filepath.Join(s.static, "dev.js")); err != nil {
		t.Skip(err)
	}
	if w := get(s, "127.0.0.1:7650", "/dev.js", nil); w.Code != 404 {
		t.Errorf("a device: %d, not 404", w.Code)
	}
	if w := get(s, "127.0.0.1:7650", "/app.js", nil); w.Code != 200 {
		t.Errorf("a file: %d, not 200", w.Code)
	}
}

// TestInstallIDSameFolder: one installation reached two ways (a link to it; on Windows and
// macOS another case) is one installation -- its second start opens the first one's page
func TestInstallIDSameFolder(t *testing.T) {
	d := t.TempDir()
	inst := filepath.Join(d, "OpenMTV")
	os.MkdirAll(inst, 0o777)
	link := filepath.Join(d, "link")
	if err := os.Symlink(inst, link); err != nil {
		t.Skip("no symlinks here")
	}
	if installIDOf(inst, "linux") != installIDOf(link, "linux") {
		t.Errorf("through a link: another installation")
	}
	if installIDOf(inst, "windows") != installIDOf(filepath.Join(d, "OPENMTV"), "windows") {
		t.Errorf("windows, another case: another installation")
	}
	if installIDOf(inst, "linux") == installIDOf(filepath.Join(d, "other"), "linux") {
		t.Errorf("two folders: one installation")
	}
}

// a game's traces: build/logs/ keeps the last keepLogs games; a game alone records its input
// beside its settings and game files, a network game records nothing
func TestGameLogs(t *testing.T) {
	keptRoot := root
	defer func() { root = keptRoot }()
	root = t.TempDir()
	logs := filepath.Join(root, "build", "logs")
	for i := 0; i < keepLogs+3; i++ {
		os.MkdirAll(filepath.Join(logs, "2026-01-01_00-00-"+string(rune('a'+i))), 0o777)
	}
	dir, err := gameLogDir()
	if err != nil {
		t.Fatal(err)
	}
	es, _ := os.ReadDir(logs)
	if len(es) != keepLogs || es[0].Name() != "2026-01-01_00-00-"+string(rune('a'+4)) || es[len(es)-1].Name() != filepath.Base(dir) {
		t.Errorf("build/logs after a start: %d folders, the first %s, the last %s", len(es), es[0].Name(), es[len(es)-1].Name())
	}
	if again, _ := gameLogDir(); again == dir {
		t.Error("two games started in the same second share a folder")
	}
	files := filepath.Join(root, "files")
	os.MkdirAll(files, 0o777)
	os.WriteFile(filepath.Join(files, "SOURCE1.RSC"), []byte("one"), 0o666)
	os.WriteFile(filepath.Join(files, "SOURCE2.RSC"), []byte("two"), 0o666)
	ini := filepath.Join(root, "my.ini")
	os.WriteFile(ini, []byte("[display]\n"), 0o666)
	args := recordArgs([]string{"mtv-dec", "--game", files, "--logical-time", "--settings=" + ini}, dir)
	if len(args) != 2 || args[0] != "--record" || args[1] != filepath.Join(dir, "input.txt") {
		t.Errorf("a game alone: %q", args)
	}
	for f, want := range map[string]string{"game/SOURCE1.RSC": "one", "game/SOURCE2.RSC": "two", "settings.ini": "[display]\n"} {
		if b, err := os.ReadFile(filepath.Join(dir, f)); err != nil || string(b) != want {
			t.Errorf("%s: %q, %v", f, b, err)
		}
	}
	if args := recordArgs([]string{"mtv-dec", "--game", files, "--mp", "127.0.0.1:1"}, t.TempDir()); args != nil {
		t.Errorf("a network game: %q", args)
	}
}
