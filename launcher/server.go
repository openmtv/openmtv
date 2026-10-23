package main

import (
	"context"
	"crypto/sha256"
	"embed"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"io/fs"
	"net"
	"net/http"
	"os"
	"os/exec"
	"os/signal"
	"path/filepath"
	"runtime"
	"strconv"
	"strings"
	"sync"
	"sync/atomic"
	"syscall"
	"time"
)

// The launcher page's server (docs/launcher-ohne-python.md, stage 3): the pages of
// tools/editor/ -- they start as the player's (/api/meta says player: no editor data,
// the pages "Play" and "Multiplayer") and become the content editor on the page's
// "Editor" (/api/meta?editor=1, editor.go: its data, the packs made and changed) --
// with the API they need: the settings file, the packs and their report, build,
// start (the game in the background, its messages in build/game.log), the lobby,
// quit.  build/openmtv editor is the launcher opened at the editor.  Only on 127.0.0.1, only for its own page (the Host is this
// machine, the API but its pictures wants the page's header X-UI-Lang, which
// another site cannot send without the browser asking first).  One runs at a time: a second
// start shows the first.  It ends when its page has not been heard of for a
// while (the page asks every 20 s) and no game it started runs, or by the
// page's "Quit the launcher".

const launcherPort = 8641 // (the Python editor it replaced had 8642)

var version = "" // the build's (make: -ldflags -X main.version=...); else build/COMMIT.txt

func versionString() string {
	if version != "" {
		return version
	}
	if b, err := os.ReadFile(filepath.Join(root, "build", "COMMIT.txt")); err == nil {
		return strings.TrimSpace(string(b))
	}
	return "dev"
}

type server struct {
	ed       *editorData // the font, the game's strings, the editor's meta (made when first asked)
	g        *game
	pics     *pictures
	static   string
	port     int
	native   bool         // in the launcher's own window (window_wails.go): its requests come without a port
	ready    atomic.Bool  // g and pics are there (the window may start without: it asks for the game's directory)
	dirty    atomic.Bool  // the page has unsaved changes (it says so in the window: /api/window)
	pageLang atomic.Value // its interface language then (the window's questions in it)
	onQuit   func()       // the page's "Quit the launcher" in the window (else: stop)
	lastSeen atomic.Int64 // unix seconds of the page's last request
	games    atomic.Int32 // games started here that still run
	mpGames  atomic.Int32 //  of them network games started by a lobby (an internet one's bridge is here)
	quit     chan struct{}
	quitOnce sync.Once
	lobbyMu  sync.Mutex
	lobbies  map[string]*lobby // this launcher's ("me"), the self-test's second player ("bot")
}

// launcherCommand: launcher [--port N] [--no-browser] [--browser] [--game DIR] [--idle SECONDS] [--page NAME]
// (--browser: the page in the browser where the program has a window of its own)
func launcherCommand(args []string) int { return serve(args, "launcher", "#launcher") }

// editorCommand: editor [the same options] -- the launcher, opened at the content editor
// (editor.go); one running already only shows it
func editorCommand(args []string) int { return serve(args, "editor", "#editor") }

func serve(args []string, what, page string) int {
	port, browser, inBrowser, idle, dir := launcherPort, true, !windowBuilt, 180, gameDir()
	usage := func() int {
		warn("openmtv %s [--port N] [--no-browser] [--browser] [--game DIR] [--idle SECONDS] [--page NAME]\n", what)
		return 2
	}
	for i := 0; i < len(args); i++ {
		next := func() string {
			if i+1 < len(args) {
				i++
				return args[i]
			}
			return ""
		}
		switch args[i] {
		case "--port":
			var err error
			if port, err = strconv.Atoi(next()); err != nil || port < 1 || port > 65535 {
				return usage()
			}
		case "--no-browser":
			browser = false
		case "--browser":
			inBrowser = true
		case "--game":
			dir = next()
		case "--idle":
			var err error
			if idle, err = strconv.Atoi(next()); err != nil || idle < 0 {
				return usage()
			}
		case "--page": // another page: the launcher's "Editor" opens the editor at its own
			if i+1 >= len(args) {
				return usage()
			}
			if page = ""; next() != "" {
				page = "#" + args[i]
			}
		default:
			return usage()
		}
	}
	detachConsole() // (Windows: a console of its own goes; one shared with play.cmd stays)
	s := &server{ed: &editorData{}, static: filepath.Join(root, "tools", "editor"), quit: make(chan struct{}), lobbies: map[string]*lobby{}}
	g, err := openGame(dir)
	if err == nil {
		err = s.useGame(g)
	}
	if browser && !inBrowser { // (the window, without the game files, asks for their directory)
		if code, ok := s.window(page, dir, err); ok {
			return code
		}
	}
	if err != nil {
		warn("%s: %v\n", what, err)
		return 1
	}
	var ln net.Listener
	for k := 0; k < 10 && port+k <= 65535; k++ {
		if ln, err = net.Listen("tcp", fmt.Sprintf("127.0.0.1:%d", port+k)); err == nil {
			s.port = port + k
			break
		}
		if running(port + k) { // ours already: show it
			url := fmt.Sprintf("http://127.0.0.1:%d/%s", port+k, page)
			say("OpenMTV: already running at %s\n", url)
			if browser {
				openBrowser(url)
			}
			return 0
		}
	}
	if ln == nil {
		warn("%s: no free port from %d (%v)\n", what, port, err)
		return 1
	}
	url := fmt.Sprintf("http://127.0.0.1:%d/%s", s.port, page)
	kiLeftovers() // (the one launcher now: what one ended outright left goes)
	say("OpenMTV: %s  (game files: %s; mods: %s)  Ctrl+C ends it\n", url, g.dir, modsDir())
	s.lastSeen.Store(time.Now().Unix())
	srv := &http.Server{Handler: s, ReadHeaderTimeout: 10 * time.Second}
	go func() {
		<-s.quit
		ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
		defer cancel()
		srv.Shutdown(ctx)
	}()
	if idle > 0 {
		go s.watch(time.Duration(idle) * time.Second)
	}
	if browser {
		go func() { time.Sleep(300 * time.Millisecond); openBrowser(url) }()
	}
	// Ctrl+C or a TERM ends it at once, as before -- and a making with it: mtv-dec would go on
	// alone (Windows starts it outside the console's group)
	sig := make(chan os.Signal, 1)
	signal.Notify(sig, os.Interrupt, syscall.SIGTERM)
	go func() { <-sig; kiStop(); os.Exit(1) }()
	err = srv.Serve(ln)
	kiStop()
	if err != nil && !errors.Is(err, http.ErrServerClosed) {
		warn("%s: %v\n", what, err)
		return 1
	}
	if s.mpGames.Load() > 0 { // (a network game it started still runs: an internet game's way to the
		say("OpenMTV: the network game still runs -- its connection goes through here: ending with it\n")
		for s.mpGames.Load() > 0 { //  server is this process's bridge -- it stays until the game ends)
			time.Sleep(time.Second)
		}
	}
	return 0
}

// uiLang: the page's language as it last said ("en" before)
func (s *server) uiLang() string {
	if l, ok := s.pageLang.Load().(string); ok {
		return l
	}
	return "en"
}

// useGame: the game the launcher shows and starts
func (s *server) useGame(g *game) error {
	res, err := readResources(g.paths)
	if err != nil {
		return err
	}
	os.Setenv("GAME", g.dir) // the game the launcher starts is this one
	s.g, s.pics = g, &pictures{res: res, banks: map[int][]sprite{}, pngs: map[string][]byte{}}
	s.ready.Store(true)
	return nil
}

// installID: this installation (its decomp directory), as /api/hello says it -- a hash,
// not the path
func installID() string { return installIDOf(root, runtime.GOOS) }

// installIDOf: the id of the installation at DIR on OS -- the same folder whatever way it
// was reached (a link; on Windows and macOS any case: their file names ignore it)
func installIDOf(dir, goos string) string {
	if d, err := filepath.EvalSymlinks(dir); err == nil {
		dir = d
	}
	dir = filepath.Clean(dir)
	if goos == "windows" || goos == "darwin" {
		dir = strings.ToLower(dir)
	}
	h := sha256.Sum256([]byte(dir))
	return hex.EncodeToString(h[:12])
}

// running: a launcher of this installation answers on PORT (another one's -- a second
// copy of the game, a launcher too old to say its installation -- is a busy port:
// its page would start that installation's game)
func running(port int) bool {
	req, _ := http.NewRequest("GET", fmt.Sprintf("http://127.0.0.1:%d/api/hello", port), nil)
	req.Header.Set("X-UI-Lang", "en")
	c := http.Client{Timeout: 2 * time.Second}
	r, err := c.Do(req)
	if err != nil {
		return false
	}
	defer r.Body.Close()
	var hello struct{ Launcher, Install string }
	return json.NewDecoder(r.Body).Decode(&hello) == nil && hello.Launcher == "madtv" && hello.Install == installID()
}

func (s *server) stop() { s.quitOnce.Do(func() { close(s.quit) }) }

// watch: the end when the page has not been heard of for IDLE and no game runs
func (s *server) watch(idle time.Duration) {
	for {
		select {
		case <-s.quit:
			return
		case <-time.After(5 * time.Second):
		}
		if s.games.Load() == 0 && !kiNow.running("") && time.Since(time.Unix(s.lastSeen.Load(), 0)) > idle {
			say("OpenMTV: no page for %s, ended\n", idle)
			s.stop()
			return
		}
	}
}

// ---------------------------------------------------------------- HTTP

type reply struct {
	code int
	body any
}

func fail(code int, m string) reply { return reply{code, obj{"error": m}} }

// needsHeader: a GET of the API, but not of its pictures (/api/sprite, /api/picture, /api/room)
func needsHeader(path string) bool {
	if !strings.HasPrefix(path, "/api/") {
		return false
	}
	for _, p := range []string{"/api/sprite/", "/api/picture/", "/api/room/"} {
		if strings.HasPrefix(path, p) {
			return false
		}
	}
	return true
}

func (s *server) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	s.lastSeen.Store(time.Now().Unix())
	// no answer is shown in another site's frame (a page of its own over ours, clicking
	// through it: clickjacking) -- the browser's tab and the window load it whole
	w.Header().Set("X-Frame-Options", "DENY")
	w.Header().Set("Content-Security-Policy", "frame-ancestors 'none'")
	// a request not from this launcher's page: the Host must be this machine (no DNS
	// rebinding); the API needs the page's own header -- a change, and what reaches
	// out (the server's games, the LAN search) or writes (the settings file) -- all
	// but the pictures, which the page loads as <img>
	host := r.Host
	if h, _, err := net.SplitHostPort(host); err == nil {
		host = h
	}
	host = strings.ToLower(strings.Trim(host, "[]"))
	if host != "127.0.0.1" && host != "localhost" && host != "::1" && !(s.native && (host == "wails" || host == "wails.localhost")) {
		s.send(w, fail(403, "not this machine's page"))
		return
	}
	if r.Header.Get("X-UI-Lang") == "" && (r.Method != "GET" || needsHeader(r.URL.Path)) {
		s.send(w, fail(403, "not this launcher's page"))
		return
	}
	lang := "en"
	if r.Header.Get("X-UI-Lang") == "de" {
		lang = "de"
	}
	var parts []string
	for _, p := range strings.Split(r.URL.Path, "/") {
		if p != "" {
			parts = append(parts, p)
		}
	}
	if len(parts) == 0 || parts[0] != "api" {
		if r.Method != "GET" {
			s.send(w, fail(404, newMsg("no such thing").in(lang)))
			return
		}
		if len(parts) == 1 && parts[0] == "font.ttf" { // the game's font (editor.go)
			s.serveFont(w, lang)
			return
		}
		s.serveStatic(w, strings.Join(parts, "/"), lang)
		return
	}
	var rep reply
	switch r.Method {
	case "GET":
		rep = s.get(r, parts[1:], lang)
	case "POST":
		rep = s.post(r, parts[1:], lang)
	case "PUT":
		rep = s.put(r, parts[1:], lang)
	case "DELETE":
		rep = s.editorDelete(parts[1:], lang)
	default:
		rep = fail(404, newMsg("no such thing").in(lang))
	}
	if b, ok := rep.body.([]byte); ok { // a picture
		w.Header().Set("Content-Type", "image/png")
		w.Header().Set("Cache-Control", "max-age=86400")
		w.Header().Set("Content-Length", strconv.Itoa(len(b)))
		w.WriteHeader(rep.code)
		w.Write(b)
		return
	}
	s.send(w, rep)
	if len(parts) == 2 && parts[1] == "quit" && rep.code == 200 {
		if s.onQuit != nil {
			go s.onQuit()
		} else {
			go s.stop()
		}
	}
}

func (s *server) send(w http.ResponseWriter, rep reply) {
	b, err := json.Marshal(rep.body)
	if err != nil {
		rep.code, b = 500, []byte(`{"error":"internal"}`)
	}
	w.Header().Set("Content-Type", "application/json")
	w.Header().Set("Cache-Control", "no-store")
	w.Header().Set("Content-Length", strconv.Itoa(len(b)))
	w.WriteHeader(rep.code)
	w.Write(b)
}

var contentTypes = map[string]string{".html": "text/html; charset=utf-8", ".js": "text/javascript; charset=utf-8",
	".css": "text/css; charset=utf-8", ".png": "image/png", ".ttf": "font/ttf", ".woff2": "font/woff2",
	".txt": "text/plain; charset=utf-8", ".json": "application/json"}

// webFS: the pages built in (make copies tools/editor/ into web/); served when there
// is no tools/editor/ beside the program -- a package of this program alone
//
//go:embed all:web
var webFS embed.FS

func (s *server) serveStatic(w http.ResponseWriter, rel, lang string) {
	if rel == "" {
		rel = "index.html"
	}
	var data []byte
	var err error
	if exists(filepath.Join(s.static, "index.html")) { // (the pages on disk: the repository's, the package's)
		p := filepath.Clean(filepath.Join(s.static, filepath.FromSlash(rel)))
		st, serr := os.Stat(p)
		// (a plain file only: not a directory, a device -- Windows' CON, NUL, COM1 in any
		// directory -- or a pipe, whose reading may not end)
		if !strings.HasPrefix(p, s.static+string(os.PathSeparator)) || serr != nil || !st.Mode().IsRegular() {
			s.send(w, fail(404, newMsg("not found").in(lang)))
			return
		}
		data, err = os.ReadFile(p)
	} else if strings.Contains(rel, "..") {
		err = os.ErrNotExist
	} else {
		data, err = webFS.ReadFile("web/" + rel)
	}
	if err != nil {
		s.send(w, fail(404, newMsg("not found").in(lang)))
		return
	}
	ct, ok := contentTypes[strings.ToLower(filepath.Ext(rel))]
	if !ok {
		ct = "application/octet-stream"
	}
	w.Header().Set("Content-Type", ct)
	w.Header().Set("Cache-Control", "no-store")
	w.Header().Set("Content-Length", strconv.Itoa(len(data)))
	w.Write(data)
}

func query(r *http.Request, k string) bool { return r.URL.Query().Get(k) == "1" }

// settingsFile: the settings file -- the player's (single player) or the network games'
// own (mp), made from it the first time; the self-test's two of its own (test)
func settingsFile(test, mp bool) (string, error) {
	sp := settingsPath()
	if test {
		sp = filepath.Join(root, "build", "selftest-settings.ini")
	}
	if !mp {
		return sp, nil
	}
	p := filepath.Join(filepath.Dir(settingsPath()), "multiplayer.ini")
	if test {
		p = filepath.Join(root, "build", "selftest-multiplayer.ini")
	}
	return p, ensureMP(p, sp)
}

// ensureMP: the network games' file, made the first time from the single player's (or the
// game's defaults): from then on the two are kept apart -- but the shared settings (display,
// sound: sharedSettings).  The mouse is System there -- in a network game every input goes to
// the server and back first, only the system's own arrow is at the mouse at once (the start
// passes --mouse=system as well)
func ensureMP(mp, sp string) error {
	settingsMu.Lock()
	defer settingsMu.Unlock()
	if !exists(mp) {
		text, err := defaultText()
		if err != nil {
			return err
		}
		if data, err := os.ReadFile(sp); err == nil {
			text = strings.ToValidUTF8(string(data), "\uFFFD")
		}
		os.MkdirAll(filepath.Dir(mp), 0o777)
		if err := replaceFile(mp, []byte(text)); err != nil {
			return err
		}
	}
	d, err := readSettings(mp)
	if err != nil {
		return err
	}
	if d["values"].(map[string]map[string]string)["input"]["mouse"] != "system" {
		return writeSettingsLocked([]settingChange{{"input", "mouse", "system"}}, mp)
	}
	return nil
}

func (s *server) get(r *http.Request, parts []string, lang string) reply {
	if rep, ok := s.editorGet(parts, lang); ok {
		return rep
	}
	if rep, ok := s.kiGet(parts, lang); ok { // AI pictures (ki.go)
		return rep
	}
	switch {
	case len(parts) == 1 && parts[0] == "hello":
		return reply{200, obj{"launcher": "madtv", "version": versionString(), "pid": os.Getpid(), "install": installID()}}
	case len(parts) == 1 && parts[0] == "meta":
		if query(r, "editor") { // the content editor's: the page becomes the editor (editor.go)
			return s.editorMeta(lang)
		}
		return reply{200, obj{"player": true, "version": versionString(), "play": play, "window": s.native}}
	case len(parts) == 1 && parts[0] == "mods":
		mods, err := listMods(s.g, lang)
		if err != nil {
			return fail(400, render(err, lang))
		}
		return reply{200, mods}
	case len(parts) == 1 && parts[0] == "settings":
		p, err := settingsFile(query(r, "test"), query(r, "mp"))
		if err != nil {
			return failure(err, lang)
		}
		d, err := settingsDoc(p)
		if err != nil {
			return fail(400, render(err, lang))
		}
		return reply{200, d}
	case len(parts) == 1 && parts[0] == "mp": // the lobby, as this launcher sees it
		return reply{200, s.lobby("me").snapshot()}
	case len(parts) == 2 && parts[0] == "mp" && parts[1] == "saves": // the network games saved here (?test=1: the self-test's)
		return reply{200, savesList(mpSavesDir(query(r, "test"), "me"))}
	case len(parts) == 2 && parts[0] == "mp" && parts[1] == "find": // the open games of the LAN (?port=N: not 7640)
		port := lobbyPort
		if p, err := strconv.Atoi(r.URL.Query().Get("port")); err == nil && p > 0 {
			port = p
		}
		return reply{200, findGames(800*time.Millisecond, port)}
	case len(parts) == 2 && parts[0] == "mp" && parts[1] == "rooms": // the open games of the public server (?server=wss://...)
		server := r.URL.Query().Get("server")
		if server == "" {
			server = internetServer()
		}
		list, err := rooms(server)
		if err != nil {
			return fail(400, err.Error())
		}
		return reply{200, list}
	case len(parts) == 1 && parts[0] == "saves": // the safe of a selection (?mod=A+B; none: the original game's): saves.go
		d, err := safeDoc(r.URL.Query().Get("mod"))
		if err != nil {
			return failure(err, lang)
		}
		return reply{200, d}
	case len(parts) == 2 && parts[0] == "packs": // /api/packs/A+B: the launcher's report
		return reply{200, s.packsInfo(parts[1], lang)}
	case len(parts) == 3 && parts[0] == "sprite": // /api/sprite/BANK/K.png
		b, err1 := strconv.Atoi(parts[1])
		k, err2 := strconv.Atoi(strings.TrimSuffix(parts[2], ".png"))
		if err1 != nil || err2 != nil {
			return fail(400, newMsg("no such thing").in(lang))
		}
		return s.picture(s.pics.spritePNG(b, k, true))
	case len(parts) == 2 && parts[0] == "picture": // an opaque single picture /api/picture/RES.png
		n, err := strconv.Atoi(strings.TrimSuffix(parts[1], ".png"))
		if err != nil {
			return fail(400, newMsg("no such thing").in(lang))
		}
		return s.picture(s.pics.spritePNG(n, 0, false))
	case len(parts) == 2 && parts[0] == "room": // /api/room/N.png (N = 101..127)
		n, err := strconv.Atoi(strings.TrimSuffix(parts[1], ".png"))
		if err != nil {
			return fail(400, newMsg("no such thing").in(lang))
		}
		return s.picture(s.pics.roomPNG(n))
	}
	return fail(404, newMsg("no such thing").in(lang))
}

func (s *server) picture(b []byte, err error) reply {
	if err != nil {
		return fail(400, err.Error())
	}
	return reply{200, b}
}

func body(r *http.Request) obj {
	var b obj
	data, _ := io.ReadAll(io.LimitReader(r.Body, 1<<20))
	if len(data) > 0 {
		v, err := parseJSON(data)
		if err == nil {
			b, _ = v.(obj)
		}
	}
	if b == nil {
		b = obj{}
	}
	return b
}

func str(v any) string { s, _ := v.(string); return s }

// rel: a path below decomp/ as the page shows it: relative, with /
func rel(p string) string {
	r, err := filepath.Rel(root, p)
	if err != nil {
		return p
	}
	return filepath.ToSlash(r)
}

// playArgs: the arguments of play.sh for a start
func playArgs(mod, lang string, classic bool) []string {
	var a []string
	if mod != "" {
		a = append(a, "--mod", mod)
	}
	if lang == "d" || lang == "e" {
		a = append(a, lang)
	}
	if classic {
		a = append(a, "--classic")
	}
	return a
}

// failure: an error for the page -- a system error marked as one (German page)
func failure(err error, lang string) reply {
	var pe *fs.PathError
	var le *os.LinkError
	if (errors.As(err, &pe) || errors.As(err, &le)) && lang == "de" {
		return fail(400, "Systemfehler: "+err.Error())
	}
	return fail(400, render(err, lang))
}

func (s *server) post(r *http.Request, parts []string, lang string) reply {
	b := body(r)
	if rep, ok := s.editorPost(parts, b, lang); ok {
		return rep
	}
	if rep, ok := s.kiPost(parts, b, lang); ok { // AI pictures (ki.go)
		return rep
	}
	switch {
	case len(parts) == 1 && parts[0] == "window": // in the window: the page's unsaved changes, asked for on its close
		s.dirty.Store(truthy(b["dirty"]))
		s.pageLang.Store(lang)
		return reply{200, obj{}}
	case len(parts) == 1 && parts[0] == "quit": // the launcher ends (a game it started plays on)
		s.lobbyMu.Lock()
		me := s.lobbies["me"]
		s.lobbyMu.Unlock()
		if me != nil {
			me.leave()
		}
		return reply{200, obj{"quit": true}}
	case len(parts) == 3 && parts[0] == "mp" && parts[1] == "saves": // a saved game: rename, delete, open (?test=1)
		return s.mpSavesPost(parts[2], b, query(r, "test"), lang)
	case len(parts) == 2 && parts[0] == "mp": // the lobby: host, join, me, game, save, start, leave
		return s.mpPost(parts[1], b, lang)
	case len(parts) == 1 && parts[0] == "launch": // start the game
		slot := 0 // (a saved game of the safe: 1..10; none: a new game)
		if v, ok := b["slot"]; ok && v != nil {
			n, whole := v.(int64)
			if !whole || n < 1 || n > saveSlots {
				return fail(400, newMsg("no such thing").in(lang))
			}
			slot = int(n)
		}
		return s.launch(str(b["mod"]), str(b["lang"]), truthy(b["classic"]), slot, truthy(b["dry"]), lang)
	case len(parts) == 3 && parts[0] == "mods" && parts[2] == "build": // build only, no start
		out, err := buildMod(s.g, parts[1])
		if err != nil {
			return failure(err, lang)
		}
		return reply{200, obj{"built": rel(out), "files": []string{"SOURCE1.RSC", "SOURCE2.RSC"},
			"cmd": strings.Join(append([]string{play}, playArgs(parts[1], str(b["lang"]), false)...), " ")}}
	case len(parts) == 3 && parts[0] == "mods" && parts[2] == "play":
		out, err := buildMod(s.g, parts[1])
		if err != nil {
			return failure(err, lang)
		}
		args := playArgs(parts[1], str(b["lang"]), false)
		pid, err := s.startGame(args)
		if err != nil {
			return failure(err, lang)
		}
		return reply{200, obj{"built": rel(out), "cmd": strings.Join(append([]string{play}, args...), " "), "pid": pid}}
	}
	return fail(404, newMsg("no such thing").in(lang))
}

// launch: the game with the settings file, what play.sh starts: the original game files,
// or packs (built first); a language or classic only for this start; dry: only the
// command (the self-test)
func (s *server) launch(mod, gameLang string, classic bool, slot int, dry bool, lang string) reply {
	if !exists(program("mtv-dec")) {
		return fail(400, newMsg("the game is not built yet (make build/mtv-dec)").in(lang))
	}
	var built any
	if mod != "" && !dry {
		out, err := buildMod(s.g, mod)
		if err != nil {
			return failure(err, lang)
		}
		built = rel(out)
	}
	args := playArgs(mod, gameLang, classic)
	if slot > 0 {
		args = append(args, "--load-slot", strconv.Itoa(slot))
	}
	line := strings.Join(append([]string{play}, args...), " ")
	if dry {
		return reply{200, obj{"built": nil, "cmd": line, "pid": nil}}
	}
	pid, err := s.startGame(args)
	if err != nil {
		return failure(err, lang)
	}
	return reply{200, obj{"built": built, "cmd": line, "pid": pid}}
}

// startGame: the game in the background (packs built already), its messages in
// build/game.log (the last game's) -> its pid
func (s *server) startGame(args []string) (int, error) { return s.startGameThen(args, nil) }

// keepLogs: the games whose traces build/logs/ keeps
const keepLogs = 10

// gameLogDir: a new game's folder in build/logs/, named by the time (the oldest beyond keepLogs
// removed)
func gameLogDir() (string, error) {
	logs := filepath.Join(root, "build", "logs")
	if err := os.MkdirAll(logs, 0o777); err != nil {
		return "", err
	}
	if es, err := os.ReadDir(logs); err == nil {
		var dirs []string
		for _, e := range es {
			if e.IsDir() {
				dirs = append(dirs, e.Name()) // (the names sort as the times)
			}
		}
		for i := 0; i+keepLogs-1 < len(dirs); i++ {
			os.RemoveAll(filepath.Join(logs, dirs[i]))
		}
	}
	name := time.Now().Format("2006-01-02_15-04-05")
	d := filepath.Join(logs, name)
	for k := 2; exists(d); k++ {
		d = filepath.Join(logs, fmt.Sprintf("%s-%d", name, k))
	}
	return d, os.Mkdir(d, 0o777)
}

// recordArgs: a game alone records its input into DIR (--record: input.txt) beside what it
// starts from -- the settings file (settings.ini) and the game files (game/) --, enough for
// tools/replay.py to play it again; a network game records nothing (its input comes from the server)
func recordArgs(cmd []string, dir string) []string {
	for _, a := range cmd {
		if a == "--mp" {
			return nil
		}
	}
	for i, a := range cmd {
		switch {
		case a == "--game" && i+1 < len(cmd):
			game := filepath.Join(dir, "game")
			if os.Mkdir(game, 0o777) == nil {
				for _, f := range []string{"SOURCE1.RSC", "SOURCE2.RSC"} {
					copyFile(filepath.Join(resolve(root, cmd[i+1]), f), filepath.Join(game, f))
				}
			}
		case a == "--settings":
			copyFile(settingsPath(), filepath.Join(dir, "settings.ini"))
		case strings.HasPrefix(a, "--settings="):
			copyFile(resolve(root, a[len("--settings="):]), filepath.Join(dir, "settings.ini"))
		}
	}
	return []string{"--record", filepath.Join(dir, "input.txt")}
}

// startGameThen: the same; at the game's end THEN gets why it ended, if it said ("[end] ..." in its
// log: a network game's end -- the connection broke off, ...), or its exit code if not 0; else "".
// Every game leaves its traces in build/logs/<its start>/ (the last keepLogs games): its messages
// (game.log -- build/game.log is the same file, a second name for it, so the next game's start
// leaves it there; where the file system has no such names, a copy at the game's end), how it
// ended (end.txt) and what recordArgs records
func (s *server) startGameThen(args []string, then func(why string)) (int, error) {
	cmd, err := prepare(args, true)
	if err != nil {
		return 0, err
	}
	logPath := filepath.Join(root, "build", "game.log")
	os.MkdirAll(filepath.Dir(logPath), 0o777)
	dir, err := gameLogDir()
	var log *os.File
	linked := false
	if err != nil {
		dir = "" // (no traces then: the game all the same)
	} else {
		cmd = append(cmd, recordArgs(cmd, dir)...)
		if log, err = os.Create(filepath.Join(dir, "game.log")); err == nil {
			os.Remove(logPath)
			linked = os.Link(filepath.Join(dir, "game.log"), logPath) == nil
		}
	}
	if !linked {
		if log != nil {
			log.Close()
		}
		if log, err = os.Create(logPath); err != nil {
			return 0, err
		}
	}
	c := exec.Command(cmd[0], cmd[1:]...)
	c.Dir, c.Stdout, c.Stderr = root, log, log
	detach(c)
	if err := c.Start(); err != nil {
		log.Close()
		return 0, err
	}
	s.games.Add(1)
	go func() {
		werr := c.Wait()
		log.Close()
		if dir != "" {
			if !linked {
				copyFile(logPath, filepath.Join(dir, "game.log"))
			}
			end := "exit status 0"
			if werr != nil {
				end = werr.Error() // ("exit status 3": a halt; "signal: segmentation fault")
			}
			os.WriteFile(filepath.Join(dir, "end.txt"), []byte(time.Now().Format("2006-01-02 15:04:05")+" "+end+"\n"), 0o666)
		}
		if then != nil {
			why := ""
			if b, err := os.ReadFile(logPath); err == nil {
				for _, l := range strings.Split(string(b), "\n") {
					if strings.HasPrefix(l, "[end] ") {
						why = strings.TrimSpace(strings.TrimPrefix(l, "[end] "))
					}
				}
			}
			if e, ok := werr.(*exec.ExitError); ok && why == "" {
				why = e.Error() // (a crash: "exit status 1", "signal: segmentation fault")
			}
			then(why)
		}
		s.games.Add(-1)
		s.lastSeen.Store(time.Now().Unix()) // (the page gets its while to come back)
	}()
	return c.Process.Pid, nil
}

// packsInfo: for the launcher -- what the packs add and where they collide, and what
// keeps them off the air
func (s *server) packsInfo(spec, lang string) any {
	docs, report, err := packsDocs(s.g, spec)
	if err != nil {
		return obj{"error": render(err, lang)}
	}
	report.Incomplete = []string{}
	for _, m := range incomplete(docs, s.g.original) {
		report.Incomplete = append(report.Incomplete, m.in(lang))
	}
	return report
}

// listMods, modInfo: the packs in mods/, each with what it changes (records per table
// that differ from the original's)
func listMods(g *game, lang string) ([]obj, error) {
	entries, err := os.ReadDir(modsDir())
	if err != nil {
		return []obj{}, nil
	}
	out := []obj{}
	for _, e := range entries {
		if nameRE.MatchString(e.Name()) && isDir(filepath.Join(modsDir(), e.Name())) {
			m, err := cachedModInfo(g, e.Name())
			if err != nil {
				return nil, err
			}
			if pe, ok := m["error"].(msg); ok {
				m["error"] = pe.in(lang)
			}
			out = append(out, m)
		}
	}
	return out, nil
}

// cachedModInfo: modInfo, kept while the pack's files stay as they are (their size and
// time): the page asks for the list often, reading every table each time cost ms
func cachedModInfo(g *game, name string) (obj, error) {
	d, err := modDir(name)
	if err != nil {
		return nil, err
	}
	var key strings.Builder
	key.WriteString(g.dir)
	for _, f := range append([]string{"mod.json"}, tableFiles()...) {
		if st, err := os.Stat(filepath.Join(d, f)); err == nil {
			fmt.Fprintf(&key, "|%s %d %d", f, st.Size(), st.ModTime().UnixNano())
		}
	}
	modCacheMu.Lock()
	c, ok := modCache[name]
	modCacheMu.Unlock()
	if !ok || c.key != key.String() {
		info, err := modInfo(g, name)
		if err != nil {
			return nil, err
		}
		c = modCached{key.String(), info}
		modCacheMu.Lock()
		modCache[name] = c
		modCacheMu.Unlock()
	}
	out := obj{} // (a copy: the caller puts the message in its language in)
	for k, v := range c.info {
		out[k] = v
	}
	return out, nil
}

type modCached struct {
	key  string
	info obj
}

var (
	modCacheMu sync.Mutex
	modCache   = map[string]modCached{}
)

func tableFiles() []string {
	out := make([]string, len(tables))
	for i, t := range tables {
		out[i] = t.name + ".json"
	}
	return out
}

// modInfo: a pack for the lists; one whose files cannot be read shows with why
// ("error"), the others stay playable
func modInfo(g *game, name string) (obj, error) {
	d, err := modDir(name)
	if err != nil {
		return nil, err
	}
	changed := obj{}
	out := obj{"name": name, "title": name, "description": "", "changed": changed, "protected": false, "complete": false}
	p := filepath.Join(d, "mod.json")
	meta := obj{}
	if exists(p) {
		v, err := readJSON(p)
		if err != nil {
			out["error"] = packError(p, err)
			return out, nil
		}
		if meta, _ = v.(obj); meta == nil {
			out["error"] = packError(p, nil)
			return out, nil
		}
	}
	if title, ok := meta["title"]; ok {
		out["title"] = title
	}
	if desc, ok := meta["description"]; ok {
		out["description"] = desc
	}
	out["protected"], out["complete"] = truthy(meta["protected"]), truthy(meta["complete"])
	if truthy(meta["from"]) {
		out["from"] = fromList(meta)
	}
	for _, t := range tables {
		p := filepath.Join(d, t.name+".json")
		if !exists(p) {
			continue
		}
		v, err := readJSON(p)
		if err != nil {
			out["error"] = packError(p, err)
			return out, nil
		}
		doc, _ := v.(obj)
		_, list, err := records(doc)
		if err != nil {
			out["error"] = packError(p, nil)
			return out, nil
		}
		orig, _, _ := records(g.original[t.name])
		n := 0
		for _, r := range list {
			if o, ok := orig[r["id"].(int64)]; !ok || !equal(o, r) {
				n++
			}
		}
		if n > 0 {
			changed[t.name] = n
		}
	}
	return out, nil
}

// packError: why a pack's file cannot be read (a JSON error: where), as a message
func packError(p string, err error) msg {
	var je jsonError
	if errors.As(err, &je) {
		line, col := je.where()
		return newMsg("\"{file}\" cannot be read (line {line}, column {col})", "file", rel(p), "line", line, "col", col)
	}
	return newMsg("\"{file}\" has the wrong form", "file", rel(p))
}

// put: the settings file, its values in the order the page sent them
func (s *server) put(r *http.Request, parts []string, lang string) reply {
	if rep, ok := s.editorPut(r, parts, lang); ok {
		return rep
	}
	if !(len(parts) == 1 && parts[0] == "settings") {
		return fail(404, newMsg("no such thing").in(lang))
	}
	data, _ := io.ReadAll(io.LimitReader(r.Body, 1<<20))
	values, err := orderedValues(data)
	if err != nil {
		return fail(400, err.Error())
	}
	mp := query(r, "mp")
	for _, v := range values {
		if mp && v.sec == "input" && v.key == "mouse" && settingValue(v.val) != "system" {
			return fail(400, newMsg("In a network game the mouse is always System.").in(lang))
		}
	}
	var changes []settingChange
	for _, v := range values {
		val, err := checkSetting(v.sec, v.key, v.val)
		if err != nil {
			return fail(400, render(err, lang))
		}
		changes = append(changes, settingChange{v.sec, v.key, val})
	}
	p, err := settingsFile(query(r, "test"), mp)
	if err != nil {
		return failure(err, lang)
	}
	if err := writeSettings(changes, p); err != nil {
		return failure(err, lang)
	}
	var shared []settingChange // (the display and the sound: the other file's too)
	for _, c := range changes {
		if sharedSetting(c.sec, c.key) {
			shared = append(shared, c)
		}
	}
	if len(shared) > 0 {
		q, err := settingsFile(query(r, "test"), !mp)
		if err == nil {
			err = writeSettings(shared, q)
		}
		if err != nil {
			return failure(err, lang)
		}
	}
	d, err := settingsDoc(p)
	if err != nil {
		return failure(err, lang)
	}
	return reply{200, d}
}

// sharedSettings: the player's, not the game's -- the display (but the role fit: a rule of
// the game, the host's in a network game) and the sound, one choice for the single player and
// network games (the page "Settings"): written to both files at once.  The mouse stays the
// single player's (System in network games)
var sharedSettings = [][2]string{{"display", "fullscreen"}, {"display", "scale"}, {"display", "aspect"},
	{"display", "filter"}, {"display", "integer"}, {"display", "ai_pictures"}, {"sound", "sound"}}

func sharedSetting(sec, key string) bool {
	for _, k := range sharedSettings {
		if k[0] == sec && k[1] == key {
			return true
		}
	}
	return false
}

// syncShared: the shared settings of FROM written into TO where they differ -- before a network
// game the single player's into its file, after it back (the game remembers a switch to
// fullscreen in the file it was started with)
func syncShared(from, to string) error {
	settingsMu.Lock()
	defer settingsMu.Unlock()
	if !exists(from) || !exists(to) {
		return nil
	}
	a, err := readSettings(from)
	if err != nil {
		return err
	}
	b, err := readSettings(to)
	if err != nil {
		return err
	}
	av, bv := a["values"].(map[string]map[string]string), b["values"].(map[string]map[string]string)
	var ch []settingChange
	for _, k := range sharedSettings {
		if v := av[k[0]][k[1]]; v != bv[k[0]][k[1]] {
			ch = append(ch, settingChange{k[0], k[1], v})
		}
	}
	if len(ch) == 0 {
		return nil
	}
	return writeSettingsLocked(ch, to)
}

type sentValue struct {
	sec, key string
	val      any
}

// orderedValues: {"values": {section: {key: value}}} in the order of the text
func orderedValues(data []byte) ([]sentValue, error) {
	d := json.NewDecoder(strings.NewReader(string(data)))
	d.UseNumber()
	var out []sentValue
	expect := func(want json.Delim) error {
		t, err := d.Token()
		if err != nil {
			return err
		}
		if t != want {
			return fmt.Errorf("expected %v", want)
		}
		return nil
	}
	if len(strings.TrimSpace(string(data))) == 0 {
		return nil, nil
	}
	if err := expect('{'); err != nil {
		return nil, err
	}
	for d.More() {
		t, err := d.Token()
		if err != nil {
			return nil, err
		}
		if t != "values" {
			var skip any
			if err := d.Decode(&skip); err != nil {
				return nil, err
			}
			continue
		}
		if err := expect('{'); err != nil {
			return nil, err
		}
		for d.More() {
			st, err := d.Token()
			if err != nil {
				return nil, err
			}
			if err := expect('{'); err != nil {
				return nil, err
			}
			for d.More() {
				kt, err := d.Token()
				if err != nil {
					return nil, err
				}
				var v any
				if err := d.Decode(&v); err != nil {
					return nil, err
				}
				out = append(out, sentValue{fmt.Sprint(st), fmt.Sprint(kt), v})
			}
			if err := expect('}'); err != nil {
				return nil, err
			}
		}
		if err := expect('}'); err != nil {
			return nil, err
		}
	}
	return out, nil
}

// ---------------------------------------------------------------- the network game

// lobby: this launcher's lobby ("me"), or the self-test's second player ("bot")
func (s *server) lobby(which string) *lobby {
	s.lobbyMu.Lock()
	defer s.lobbyMu.Unlock()
	if s.lobbies[which] == nil {
		l := newLobby(s.g.dir, which)
		l.build = func(docs map[string]obj, mid string) (string, error) { return buildMP(s.g, docs, mid) }
		l.startGame = func(args []string, ended func(string)) (int, error) {
			l.mu.Lock()
			mp, dry := l.settings, l.dry
			l.mu.Unlock()
			sp, _ := settingsFile(dry, false)
			if mp != "" { // the shared display and sound as the single player's (the page "Settings")
				if err := syncShared(sp, mp); err != nil {
					warn("settings: %v\n", err)
				}
			}
			s.mpGames.Add(1)
			pid, err := s.startGameThen(args, func(why string) {
				if mp != "" { // (back: a switch to fullscreen while playing)
					if err := syncShared(mp, sp); err != nil {
						warn("settings: %v\n", err)
					}
				}
				s.mpGames.Add(-1)
				ended(why)
			})
			if err != nil {
				s.mpGames.Add(-1)
			}
			return pid, err
		}
		s.lobbies[which] = l
	}
	return s.lobbies[which]
}

// buildMP: a network game's packs -- their tables, as the host sent them, written into
// fresh copies of this machine's original files: build/mp/ID
func buildMP(g *game, docs map[string]obj, mid string) (string, error) {
	out := filepath.Join(root, "build", "mp", mid)
	done := filepath.Join(out, "built")
	defer lockDir(out)() // (one build at a time: a second one waits for the first one's)
	if exists(done) {
		return out, nil
	}
	if err := os.MkdirAll(out, 0o777); err != nil {
		return "", err
	}
	if err := buildDocs(g, docs, g.paths, out); err != nil {
		return "", err
	}
	return out, os.WriteFile(done, []byte(mid+"\n"), 0o666)
}

// mpSavesDir: where the network games are saved (src/mpsave.c: a directory per game, a
// savegame per night) -- beside the settings files; the self-test's (dry) in build/, apart
// for its second player; $MADTV_MP_SAVES for tests that run launchers of their own
func mpSavesDir(test bool, which string) string {
	if d := os.Getenv("MADTV_MP_SAVES"); d != "" {
		return absPath(d)
	}
	if test {
		return filepath.Join(root, "build", "selftest-mp-saves", which)
	}
	return absPath(filepath.Join(filepath.Dir(settingsPath()), "mp-saves"))
}

// mpSavesPost: the network games saved here -- a game renamed, a night or the game deleted
// (on this machine only), its directory shown in the system's file manager
func (s *server) mpSavesPost(what string, b obj, test bool, lang string) reply {
	top := mpSavesDir(test, "me")
	var err error
	switch what {
	case "rename":
		err = saveRename(top, b["game"], b["title"])
	case "delete":
		err = saveDelete(top, b["game"], b["file"])
	case "open":
		d := filepath.Join(top, safeID(b["game"]))
		if safeID(b["game"]) == "" || !isDir(d) {
			return fail(400, newMsg("no such saved game").in(lang))
		}
		if !test {
			openBrowser(d) // (the system's opener: Explorer, Finder, the desktop's)
		}
		return reply{200, obj{"opened": d}}
	default:
		return fail(400, newMsg("no such thing").in(lang))
	}
	if err != nil {
		return failure(err, lang)
	}
	return reply{200, savesList(top)}
}

// mpGame: the host's game -- settings, and the packs (the launcher's selection) as their tables
func (s *server) mpGame(b obj, lang string) reply {
	spec, _ := b["packs"].(string)
	var docs map[string]obj
	titles := []any{}
	mode := 0
	if spec != "" && spec != "original" {
		var err error
		if docs, _, err = packsDocs(s.g, spec); err == nil {
			err = checkDocs(docs)
		}
		if err != nil {
			return failure(err, lang)
		}
		if inc := incomplete(docs, s.g.original); len(inc) > 0 {
			return fail(400, newMsg("not playable yet -- {list}", "list", joined{"; ", inc}).in(lang))
		}
		names, _ := packNames(spec)
		for _, n := range names {
			meta, _ := modMeta(n)
			t, _ := meta["title"].(string)
			if t == "" {
				t = n
			}
			titles = append(titles, t)
		}
		mode = 1
	}
	settings := obj{}
	if bs, ok := b["settings"].(obj); ok {
		for _, k := range []string{"lang", "fixes", "difficulty", "speed4", "aiimp", "money", "credit", "terror", "sammy", "offers", "filmoffers", "sertop", "knock", "castfit"} {
			if v, ok := bs[k]; ok {
				settings[k] = v
			}
		}
	}
	l := s.lobby("me")
	l.mu.Lock()
	onSave := truthy(l.save["hash"]) && l.state != nil && truthy(l.state["save"])
	l.mu.Unlock()
	var err error
	if onSave { // on from a savegame (here and on the server): its packs and rules stay
		keep := obj{}
		if v, ok := settings["lang"]; ok {
			keep["lang"] = v
		}
		err = l.setGame(keep, nil, keepDocs, nil)
	} else {
		err = l.setGame(settings, docs, mode, titles)
	}
	if err != nil {
		return failure(err, lang)
	}
	return reply{200, l.snapshot()}
}

func (s *server) mpPost(what string, b obj, lang string) reply {
	l := s.lobby("me")
	name := func(def string) string {
		if n, _ := b["name"].(string); n != "" {
			return n
		}
		return def
	}
	server := func() string {
		if v, _ := b["server"].(string); v != "" {
			return v
		}
		return internetServer()
	}
	prepareFiles := func() error {
		dry := truthy(b["dry"])
		p, err := settingsFile(dry, true) // (dry: the tests' files, never the player's)
		l.mu.Lock()
		l.dry, l.settings, l.savesDir = dry, p, mpSavesDir(dry, "me")
		l.mu.Unlock()
		return err
	}
	var err error
	switch what {
	case "host":
		if err = prepareFiles(); err != nil {
			break
		}
		if truthy(b["internet"]) { // on the public server (stage 6): a room there
			listed := true
			if v, ok := b["listed"]; ok {
				listed = truthy(v)
			}
			err = l.hostInternet(name("Host"), server(), listed)
		} else {
			port := int64(lobbyPort)
			switch p := b["port"].(type) {
			case int64:
				if p != 0 {
					port = p
				}
			case string:
				if p == "free" { // (the self-test: any free port of this machine)
					if ln, e := net.Listen("tcp", "127.0.0.1:0"); e == nil {
						port = int64(ln.Addr().(*net.TCPAddr).Port)
						ln.Close()
					}
				} else if n, e := strconv.ParseInt(p, 10, 64); e == nil {
					port = n
				}
			}
			listen, _ := b["listen"].(string)
			if listen == "" {
				listen = "0.0.0.0"
			}
			pace := "realtime"
			if b["pace"] == "fast" {
				pace = "fast"
			}
			err = l.host(name("Host"), int(port), listen, "", pace)
		}
		if err == nil {
			if v, ok := b["settings"]; (ok && v != nil) || truthy(b["packs"]) {
				if r := s.mpGame(b, lang); r.code != 200 {
					return r
				}
			}
		}
	case "join":
		if err = prepareFiles(); err != nil {
			break
		}
		if room, _ := b["room"].(string); room != "" {
			err = l.joinInternet(room, name("Guest"), server())
		} else {
			addr, _ := b["address"].(string)
			err = l.join(addr, name("Guest"))
		}
	case "me":
		err = l.me(b)
	case "game":
		return s.mpGame(b, lang)
	case "save": // the host: on from a savegame ({game, file}), or a new game ({})
		err = l.chooseSave(b["game"], b["file"])
	case "testsave": // the self-test's savegame (dry only)
		l.mu.Lock()
		dry := l.dry
		l.mu.Unlock()
		if !dry {
			return fail(400, newMsg("no such thing").in(lang))
		}
		list, err := testSave(mpSavesDir(true, "me"), mpSavesDir(true, "bot"), s.g.dir, b)
		if err != nil {
			return failure(err, lang)
		}
		return reply{200, list}
	case "start":
		err = l.start()
	case "leave":
		l.leave()
		l.reset()
	case "bot": // the self-test's second player, on this machine (dry)
		bot := s.lobby("bot")
		bot.mu.Lock()
		bot.dry, bot.savesDir = true, mpSavesDir(true, "bot")
		bot.mu.Unlock()
		switch b["action"] {
		case "join":
			if room, _ := b["room"].(string); room != "" {
				err = bot.joinInternet(room, name("Bot"), server())
			} else {
				addr, _ := b["address"].(string)
				err = bot.join(addr, name("Bot"))
			}
		case "me":
			err = bot.me(b)
		case "leave":
			bot.leave()
			bot.reset()
		}
		if err != nil {
			return failure(err, lang)
		}
		return reply{200, bot.snapshot()}
	default:
		return fail(400, newMsg("no such thing").in(lang))
	}
	if err != nil {
		return failure(err, lang)
	}
	return reply{200, l.snapshot()}
}
