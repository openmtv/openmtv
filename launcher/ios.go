//go:build ios

package main

// The iPhone and iPad app (docs/ios.md): this launcher is a library inside it
// (go build -buildmode=c-archive -tags ios, make ios), without a window and without a port.
// The app's own pages (ios/Launcher, SwiftUI) ask it what the launcher's web page asks: the
// same /api/ server.go answers, by OMTVRequest instead of HTTP.  A start (POST /api/launch)
// builds the packs' game files as always and hands the game's command line back
// (OMTVTakeLaunch): the app runs the game in its own process -- iOS starts no other.
//
// Its files, in the app's Documents folder (the Files app: "On my iPhone > OpenMTV"):
//   SOURCE1.RSC, SOURCE2.RSC   the original game files (GAME), as the player put them there
//   OpenMTV/                   what decomp/ holds on a computer (root): run/ (the original
//                              game's files to play, with its saved games), build/mods/ (each
//                              selection of packs'), build/logs/, build/game.log, mods/ (the
//                              packs: the app's own copied in, the player's beside them),
//                              config/madtv/settings.ini (XDG_CONFIG_HOME: the game reads it too)

/*
#include <stdlib.h>
int fix_list_entry(int i, const char **name, const char **what);
*/
import "C"

import (
	"bytes"
	"fmt"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"sync"
	"unsafe"
)

var (
	iosSrv  *server
	iosMu   sync.Mutex
	iosArgv []string // the game's command line, once a start asked for one
	iosLog  string   // and the file for its messages
)

func init() {
	inProcess = func(cmd []string, log string) {
		iosMu.Lock()
		iosArgv, iosLog = cmd, log
		iosMu.Unlock()
	}
	fixesBuiltIn = func() [][2]string {
		var list [][2]string
		for i := 0; ; i++ {
			var name, what *C.char
			if C.fix_list_entry(C.int(i), &name, &what) == 0 {
				break
			}
			list = append(list, [2]string{C.GoString(name), C.GoString(what)})
		}
		return list
	}
}

func cstr(s string) *C.char { return C.CString(s) }

// OMTVInit: the launcher over DOCS (the app's Documents folder); BUNDLED: the packs the app
// brings (copied into OpenMTV/mods/ when this app's are not there yet).  -> NULL, or why not
// (the original game files missing: "nofiles"; else the error) -- free with OMTVFree
//
//export OMTVInit
func OMTVInit(docs, bundled *C.char) (out *C.char) {
	defer func() {
		if r := recover(); r != nil {
			out = cstr(fmt.Sprint("the launcher failed: ", r))
		}
	}()
	d, b := C.GoString(docs), C.GoString(bundled)
	root = filepath.Join(d, "OpenMTV")
	cfg := filepath.Join(root, "config")
	for _, p := range []string{filepath.Join(root, "run"), filepath.Join(root, "build"), filepath.Join(root, "mods"), filepath.Join(cfg, "madtv")} {
		if err := os.MkdirAll(p, 0o777); err != nil {
			return cstr(err.Error())
		}
	}
	os.Setenv("XDG_CONFIG_HOME", cfg) // (the game's settings.c looks there as well)
	os.Setenv("GAME", d)
	// the first app wrote Documents/settings.ini: its settings move in once
	if old := filepath.Join(d, "settings.ini"); exists(old) && !exists(settingsPath()) {
		os.Rename(old, settingsPath())
	}
	if err := iosPacks(b); err != nil {
		return cstr(err.Error())
	}
	if !hasGameFiles(d) {
		return cstr("nofiles")
	}
	s := &server{ed: &editorData{}, static: filepath.Join(root, "web"), quit: make(chan struct{}), lobbies: map[string]*lobby{}}
	g, err := openGame(d)
	if err == nil {
		err = s.useGame(g)
	}
	if err != nil {
		return cstr(err.Error())
	}
	iosSrv = s
	return nil
}

// iosPacks: the app's packs (BUNDLED/NAME) in OpenMTV/mods/ -- each replaced whole when this
// app's version differs (.app-version in it, the app's mark): they are the app's, protected.
// A folder of that name without the mark is the player's own: it stays, the app's is left out.
// (A build without a version copies them at every start.)
func iosPacks(bundled string) error {
	es, err := os.ReadDir(bundled)
	if err != nil {
		return nil // (an app without packs)
	}
	v := versionString()
	for _, e := range es {
		if !e.IsDir() {
			continue
		}
		dst := filepath.Join(root, "mods", e.Name())
		mark := filepath.Join(dst, ".app-version")
		if exists(dst) {
			b, err := os.ReadFile(mark)
			if err != nil {
				warn("ios: mods/%s is the player's own (no .app-version): the app's pack of that name is left out\n", e.Name())
				continue
			}
			if v != "" && strings.TrimSpace(string(b)) == v {
				continue
			}
			if err := os.RemoveAll(dst); err != nil {
				return err
			}
		}
		if err := copyTree(filepath.Join(bundled, e.Name()), dst); err != nil {
			return err
		}
		if err := os.WriteFile(mark, []byte(v+"\n"), 0o666); err != nil {
			return err
		}
	}
	return nil
}

// failJSON: the answer of OMTVRequest for a request that never reached the server
func failJSON(code int, msg string) *C.char {
	return cstr(`{"status":` + strconv.Itoa(code) + `,"body":{"error":` + strconv.Quote(msg) + `}}`)
}

func copyTree(src, dst string) error {
	return filepath.Walk(src, func(p string, fi os.FileInfo, err error) error {
		if err != nil {
			return err
		}
		r, _ := filepath.Rel(src, p)
		t := filepath.Join(dst, r)
		if fi.IsDir() {
			return os.MkdirAll(t, 0o777)
		}
		return copyFile(p, t)
	})
}

// OMTVRequest: METHOD /api/PATH (with its query) with the JSON BODY, the page's language LANG
// ("de", "en") -> {"status": N, "body": the answer} -- free with OMTVFree
//
//export OMTVRequest
func OMTVRequest(method, path, body, lang *C.char) (out *C.char) {
	defer func() { // (a Go panic in an exported function would end the app: an answer instead)
		if r := recover(); r != nil {
			out = failJSON(500, fmt.Sprint("the launcher failed: ", r))
		}
	}()
	if iosSrv == nil {
		return failJSON(503, "OMTVInit first")
	}
	req, err := http.NewRequest(C.GoString(method), "/api/"+strings.TrimPrefix(C.GoString(path), "/"), bytes.NewReader([]byte(C.GoString(body))))
	if err != nil {
		return failJSON(400, err.Error())
	}
	req.Host = "127.0.0.1" // (the server's own check: a request of this machine's page)
	req.RemoteAddr = "127.0.0.1:0"
	req.Header.Set("X-UI-Lang", C.GoString(lang))
	req.Header.Set("Content-Type", "application/json")
	rec := httptest.NewRecorder()
	iosSrv.ServeHTTP(rec, req)
	text := strings.TrimSpace(rec.Body.String())
	if text == "" || (text[0] != '{' && text[0] != '[' && text[0] != '"') {
		text = strconv.Quote(text)
	}
	return cstr(`{"status":` + strconv.Itoa(rec.Code) + `,"body":` + text + `}`)
}

// OMTVTakeLaunch: the game's start, once POST /api/launch asked for it -> {"argv": [...],
// "log": FILE} (the game's messages: build/logs/<start>/game.log, build/game.log the same
// file), or NULL -- free with OMTVFree
//
//export OMTVTakeLaunch
func OMTVTakeLaunch() *C.char {
	iosMu.Lock()
	defer iosMu.Unlock()
	if iosArgv == nil {
		return nil
	}
	var b strings.Builder
	b.WriteString(`{"argv":[`)
	for i, a := range iosArgv {
		if i > 0 {
			b.WriteByte(',')
		}
		b.WriteString(strconv.Quote(a))
	}
	b.WriteString(`],"log":` + strconv.Quote(iosLog) + `}`)
	iosArgv = nil
	return cstr(b.String())
}

// OMTVFree: a text the functions above returned
//
//export OMTVFree
func OMTVFree(p *C.char) { C.free(unsafe.Pointer(p)) }
