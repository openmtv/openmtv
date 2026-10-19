//go:build wails

package main

import (
	"bytes"
	"context"
	"crypto/sha256"
	"encoding/hex"
	"fmt"
	"io"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"runtime"
	"strconv"
	"strings"
	"sync"
	"sync/atomic"
	"time"

	"github.com/wailsapp/wails/v2"
	"github.com/wailsapp/wails/v2/pkg/menu"
	wo "github.com/wailsapp/wails/v2/pkg/options"
	"github.com/wailsapp/wails/v2/pkg/options/assetserver"
	"github.com/wailsapp/wails/v2/pkg/options/linux"
	"github.com/wailsapp/wails/v2/pkg/options/mac"
	winopt "github.com/wailsapp/wails/v2/pkg/options/windows"
	wr "github.com/wailsapp/wails/v2/pkg/runtime"

	_ "madtv/launcher/winres" // (Windows: the icon, the manifest -- make launcher-win)
)

// The launcher in a window of its own (docs/launcher-fenster.md): Wails v2 shows the
// same page in the system's web view -- WebView2 (Windows), WKWebView (macOS),
// WebKitGTK 4.1 (Linux) -- and hands its requests to the server's handler directly,
// without a port.  Built with -tags wails (make launcher-native); without it, or with
// --browser, the page is the browser's as before.  The window's close is the
// launcher's end -- but while a game it started runs (its network game may go
// through this launcher), the window only hides and the launcher ends after the
// game; a second start shows the window again (at its page: openmtv editor).

const windowBuilt = true

// (the help says where the launcher is: its window, no browser)
func init() {
	for lang, from := range map[string]string{"en": "in the browser", "de": "im Browser"} {
		to := map[string]string{"en": "in its own window", "de": "im eigenen Fenster"}[lang]
		for i := range helpTop[lang] {
			helpTop[lang][i].text = strings.Replace(helpTop[lang][i].text, from, to, 1)
		}
	}
}

// window: the launcher's page in its window until it is closed -> the exit code;
// false: no window could be made (the browser's page then).  MISSING: why the game
// files were not found in DIR -- the window then asks for their directory first
// (window_setup.go)
func (s *server) window(page, dir string, missing error) (int, bool) {
	if runtime.GOOS == "linux" && os.Getenv("DISPLAY") == "" && os.Getenv("WAYLAND_DISPLAY") == "" {
		warn("launcher: no window (no display) -- the page in the browser\n")
		return 0, false
	}
	s.native = true
	var ctx atomic.Pointer[context.Context]
	var hidden atomic.Bool
	show := func(page string) {
		c := ctx.Load()
		if c == nil {
			return
		}
		hidden.Store(false)
		wr.WindowUnminimise(*c)
		wr.WindowShow(*c)
		if page != "" {
			wr.WindowExecJS(*c, "if (location.hash !== "+strconv.Quote(page)+") location.hash = "+strconv.Quote(page)+";")
		}
	}
	icon, macIcon := iconPNG(nil, 256, false), iconPNG(nil, 256, true) // (icon.go: OpenMTV's television)
	place := newPlaces(loadState().Window)
	setup := &gameSetup{s: s, dir: dir, missing: missing}
	app := &wo.App{
		Title:     "OpenMTV",
		Width:     place.start.Width,
		Height:    place.start.Height,
		MinWidth:  minWidth,
		MinHeight: minHeight,
		// (the page's ground, --bg in editor.css: no white flash before it is drawn)
		BackgroundColour: wo.NewRGB(0x11, 0x12, 0x14),
		AssetServer:      &assetserver.Options{Handler: windowPage(s, page, setup)},
		// (no browser's menu on a right click: Back, Reload, ... -- copy and paste go by the keys)
		EnableDefaultContextMenu: false,
		OnStartup: func(c context.Context) {
			ctx.Store(&c)
			setup.ctx = c
			s.onQuit = func() { wr.Quit(c) } // the page's "Quit the launcher": as the window's close
			go place.follow(c, &hidden)
			go func() { // hidden for a game: the end after it
				for {
					time.Sleep(time.Second)
					if hidden.Load() && s.games.Load() == 0 {
						wr.Quit(c)
						return
					}
				}
			}()
		},
		OnShutdown: func(context.Context) { place.ended.Store(true) },
		OnBeforeClose: func(c context.Context) bool {
			if hidden.Load() { // (the game it waited for has ended)
				return false
			}
			if s.dirty.Load() && !discardChanges(c, s.uiLang()) {
				return true
			}
			if s.games.Load() > 0 {
				hidden.Store(true)
				wr.WindowHide(c)
				return true
			}
			return false
		},
		SingleInstanceLock: &wo.SingleInstanceLock{
			UniqueId:               instanceID(),
			OnSecondInstanceLaunch: func(d wo.SecondInstanceData) { show(secondPage(d.Args)) },
		},
		Linux: &linux.Options{Icon: icon, ProgramName: "openmtv", WebviewGpuPolicy: linux.WebviewGpuPolicyOnDemand},
		// (the title bars dark as the page; no zoom by pinching -- the page is laid out for its size;
		// WebView2's own files beside the settings, not Wails' %APPDATA%\openmtv.exe)
		Windows: &winopt.Options{Theme: winopt.Dark, DisablePinchZoom: true,
			WebviewUserDataPath: filepath.Join(filepath.Dir(settingsPath()), "webview")},
		Mac: &mac.Options{Appearance: mac.NSAppearanceNameDarkAqua, DisableZoom: true,
			About: &mac.AboutInfo{Title: "OpenMTV", Message: versionString(), Icon: macIcon}},
	}
	if place.start.Maximised {
		app.WindowStartState = wo.Maximised
	}
	if runtime.GOOS == "darwin" { // (macOS: Cmd+C/V/Z reach the page's fields only through an Edit menu)
		app.Menu = menu.NewMenuFromItems(menu.AppMenu(), menu.EditMenu(), menu.WindowMenu())
	}
	if s.ready.Load() {
		say("OpenMTV: in its window  (game files: %s; mods: %s)\n", s.g.dir, modsDir())
	} else {
		say("OpenMTV: in its window, asking for the game files (%v)\n", missing)
	}
	if err := runWindow(app); err != nil {
		warn("launcher: no window (%v) -- the page in the browser\n", err)
		s.native = false
		return 0, false
	}
	place.keep()
	s.stop()
	kiStop()
	return 0, true
}

// runWindow: wails.Run -- a panic of its start (Linux: GTK that cannot open its display)
// is its error
func runWindow(app *wo.App) (err error) {
	defer func() {
		if r := recover(); r != nil {
			err = fmt.Errorf("%v", r)
		}
	}()
	return wails.Run(app)
}

// discardChanges: the page has unsaved changes -- close anyway?
func discardChanges(c context.Context, lang string) bool {
	title, m, yes, no := "Unsaved changes", "The pack has unsaved changes. Close anyway? They will be lost.", "Close", "Cancel"
	if lang == "de" {
		title, m, yes, no = "Ungespeicherte Änderungen", "Das Paket hat ungespeicherte Änderungen. Trotzdem schließen? Sie gehen verloren.", "Schließen", "Abbrechen"
	}
	r, err := wr.MessageDialog(c, wr.MessageDialogOptions{Type: wr.QuestionDialog, Title: title, Message: m,
		Buttons: []string{yes, no}, DefaultButton: no, CancelButton: no})
	return err == nil && (r == "Yes" || r == yes) // (Windows and Linux answer Yes/No, the Mac its button)
}

// ---------------------------------------------------------------- the window's place

const minWidth, minHeight = 960, 600

// places: the window's place and size -- as last left (launcher.json), followed while
// it is open, kept at its end
type places struct {
	start windowPlace
	mu    sync.Mutex
	now   *windowPlace // the last seen (unmaximised: its own size; maximised: only that)
	ended atomic.Bool  // the window has gone: no more asking
}

func newPlaces(kept *windowPlace) *places {
	p := &places{start: windowPlace{Width: 1280, Height: 860, X: -1, Y: -1}}
	if kept != nil {
		p.start = *kept
		p.start.Width, p.start.Height = max(p.start.Width, minWidth), max(p.start.Height, minHeight)
	}
	return p
}

// follow: the kept place restored where it fits the screen, then the window's place
// seen every second (not while minimised or hidden)
func (p *places) follow(c context.Context, hidden *atomic.Bool) {
	time.Sleep(300 * time.Millisecond)
	if st := p.start; st.X >= 0 && st.Y >= 0 && !st.Maximised {
		if w, h := screenSize(c); w > 0 {
			if st.Width > w || st.Height > h {
				st.Width, st.Height = min(st.Width, w), min(st.Height, h)
				wr.WindowSetSize(c, st.Width, st.Height)
			}
			if st.X+100 <= w && st.Y+60 <= h { // (else: where the system puts it -- a screen gone)
				wr.WindowSetPosition(c, st.X, st.Y)
			}
		}
	}
	for {
		time.Sleep(time.Second)
		// (asked without the lock: after the window's end the answers never come, and
		// keep must not wait for them)
		if p.ended.Load() || hidden.Load() || wr.WindowIsMinimised(c) {
			continue
		}
		maxed := wr.WindowIsMaximised(c) || wr.WindowIsFullscreen(c)
		var w, h, x, y int
		if !maxed {
			w, h = wr.WindowGetSize(c)
			x, y = wr.WindowGetPosition(c)
		}
		if p.ended.Load() {
			return
		}
		p.mu.Lock()
		now := windowPlace{X: -1, Y: -1, Width: p.start.Width, Height: p.start.Height}
		if p.now != nil {
			now = *p.now
		}
		if now.Maximised = maxed; !maxed {
			now.Width, now.Height, now.X, now.Y = w, h, x, y
		}
		p.now = &now
		p.mu.Unlock()
	}
}

// keep: the place last seen into launcher.json
func (p *places) keep() {
	p.ended.Store(true)
	p.mu.Lock()
	now := p.now
	p.mu.Unlock()
	if now != nil && now.Width >= minWidth/2 && now.Height >= minHeight/2 {
		changeState(func(st *launcherState) { st.Window = now })
	}
}

// screenSize: the size of the screen the window is on (0, 0: not known)
func screenSize(c context.Context) (int, int) {
	screens, err := wr.ScreenGetAll(c)
	if err != nil {
		return 0, 0
	}
	for _, want := range []func(wr.Screen) bool{func(x wr.Screen) bool { return x.IsCurrent }, func(x wr.Screen) bool { return x.IsPrimary }} {
		for _, x := range screens {
			if want(x) {
				return x.Size.Width, x.Size.Height
			}
		}
	}
	return 0, 0
}

// windowPage: the server's handler for the window -- the page's start at PAGE (the
// window always opens at "/": the hash is set before the page's scripts read it).
// For the tests, $MADTV_WINDOW_TEST=/?selftest#editor: the window goes there once and
// prints each title the page gets ("TITLE ..." -- the self-test's result is one)
func windowPage(s *server, page string, setup *gameSetup) http.Handler {
	test := os.Getenv("MADTV_WINDOW_TEST")
	js := ""
	if page != "" {
		js = "if (!location.hash) history.replaceState(null, '', " + strconv.Quote(page) + ");"
	}
	if test != "" {
		js += "if (!sessionStorage.getItem('madtv-window-test')) { sessionStorage.setItem('madtv-window-test', '1'); location.replace(" + strconv.Quote(test) + "); }" +
			"(() => { let last = null; setInterval(() => { if (document.title !== last) { last = document.title; fetch('/window-test/title', { method: 'POST', body: last }); } }, 500); })();"
	}
	script := []byte("<script>" + js + "</script>")
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if !s.ready.Load() || strings.HasPrefix(r.URL.Path, "/setup/") { // no game files yet: the page that asks for them
			setup.ServeHTTP(w, r)
			return
		}
		if test != "" && r.Method == "POST" && r.URL.Path == "/window-test/title" {
			b, _ := io.ReadAll(r.Body)
			say("TITLE %s\n", b)
			w.WriteHeader(204)
			return
		}
		if r.Method != "GET" || (r.URL.Path != "/" && r.URL.Path != "/index.html") {
			s.ServeHTTP(w, r)
			return
		}
		rec := httptest.NewRecorder()
		s.ServeHTTP(rec, r)
		b := rec.Body.Bytes()
		if i := bytes.Index(b, []byte("<head>")); i >= 0 && rec.Code == 200 {
			b = append(b[:i+6:i+6], append(script, b[i+6:]...)...)
		}
		for k, v := range rec.Header() {
			w.Header()[k] = v
		}
		w.Header().Set("Content-Length", strconv.Itoa(len(b)))
		w.WriteHeader(rec.Code)
		w.Write(b)
	})
}

// instanceID: one launcher per installation (its directory) -- a second start
// shows the first's window
func instanceID() string {
	h := sha256.Sum256([]byte(root))
	return bundleID + "." + hex.EncodeToString(h[:6])
}

// secondPage: the page a second start asks for (openmtv editor, --page NAME)
func secondPage(args []string) string {
	page := ""
	if len(args) > 0 && args[0] == "editor" {
		page = "#editor"
	}
	for i := 0; i+1 < len(args); i++ {
		if args[i] == "--page" {
			page = "#" + args[i+1]
		}
	}
	return page
}
