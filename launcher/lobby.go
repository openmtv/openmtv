package main

import (
	"bufio"
	"crypto/rand"
	"encoding/base64"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net"
	"net/http"
	"net/url"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strconv"
	"strings"
	"sync"
	"time"
)

// One player's view of a network game's lobby (multiplayer stage 5,
// docs/multiplayer.md; the former Python lobby (mplobby.py) Lobby, whose protocol this speaks, so
// that Go and Python launchers meet in one lobby): the connection to the lobby
// server (TCP in the LAN, WebSocket on the internet), the server's state, the
// host's packs (their tables through the server, built here from this
// machine's own original files), the savegame a game goes on from, the start.
// The state maps are never changed in place: a change puts a new one (the page
// reads them while the connection's goroutine writes).

var (
	aiImprove  = []string{"off", "normal", "strong"} // the game's --ai-improve, by a savegame's "aiimp" 0..2
	creditLow  = int64(300000)                       // the game's --credit-line: lowest (the original's),
	creditHigh = int64(100000000)                    // highest,
	creditDef  = int64(700000)                       // the default,
	creditOrig = int64(0)                            // the original's credit system (--credit-line=original): fix credit-repay off
	moneyMax   = int64(100000000)                    // the game's highest amount (--terror-damage, --sammy-prize)
	terrorDef  = int64(50000)                        // the game's --terror-damage: the original's, the default
	offersLow  = int64(10)                           // the game's --ad-offers and --film-offers: lowest (the original's, the default),
	offersHigh = int64(17)                           // highest
	knockDef   = int64(15)                           // the game's --knock (network games): game minutes for the one inside a knocked-at room
	knockHigh  = int64(120)                          // (0: no knocking; the game's KNOCK_OPT_MAX)
)

func internetServer() string {
	if s := os.Getenv("MADTV_SERVER"); s != "" {
		return s
	}
	return "wss://openmtv.net"
}

var (
	versionOnce sync.Once
	versionText string
)

// lobbyVersion: this build's version as the lobby compares it -- its git commit, else
// build/COMMIT.txt (as the former Python lobby (mplobby.py)), else the one built in
func lobbyVersion() string {
	versionOnce.Do(func() {
		c := exec.Command("git", "-C", root, "rev-parse", "--short=12", "HEAD")
		hideWindow(c)
		if out, err := c.Output(); err == nil && strings.TrimSpace(string(out)) != "" {
			versionText = strings.TrimSpace(string(out))
			return
		}
		if b, err := os.ReadFile(filepath.Join(root, "build", "COMMIT.txt")); err == nil {
			if f := strings.Fields(string(b)); len(f) > 0 {
				versionText = f[0]
				if len(versionText) > 12 {
					versionText = versionText[:12]
				}
				return
			}
		}
		versionText = version
		if versionText == "" {
			versionText = "unknown"
		}
	})
	return versionText
}

// creditLine, dollars, sammyPrize: the values the game takes -- else the fallback
func creditLine(v any, fallback int64) int64 {
	x, ok := v.(int64)
	if ok && (x == creditOrig || x >= creditLow && x <= creditHigh && x%100000 == 0) {
		return x
	}
	return fallback
}

// creditArg: the game's --credit-line for a lobby's credit (creditOrig: original)
func creditArg(x int64) string {
	if x == creditOrig {
		return "--credit-line=original"
	}
	return fmt.Sprintf("--credit-line=%d", x)
}

func dollars(v any, fallback int64) int64 {
	if x, ok := v.(int64); ok && x >= 0 && x <= moneyMax {
		return x
	}
	return fallback
}

// offersCount: the ad agency's offers (the game's --ad-offers) or the film dealer's
// (--film-offers), offersLow (the original's) to offersHigh
func offersCount(v any) int64 {
	if x, ok := v.(int64); ok && x >= offersLow && x <= offersHigh {
		return x
	}
	return offersLow
}

// knockMinutes: the host's game minutes for knocking (--knock), 0 (off) to knockHigh; knockDef if none
func knockMinutes(v any) int64 {
	if x, ok := v.(int64); ok && x >= 0 && x <= knockHigh {
		return x
	}
	return knockDef
}

func sammyPrize(v any) []any {
	if l, ok := v.([]any); ok && len(l) == 4 {
		good := true
		for _, x := range l {
			good = good && dollars(x, -1) != -1 && equal(dollars(x, -1), x)
		}
		if good {
			return append([]any{}, l...)
		}
	}
	return []any{int64(0), int64(0), int64(0), int64(0)}
}

// fixesArgs: the host's fixes as the game's option -- "none" (classic), "all", or a list
// of fix names; anything else (a broken setting, or a hostile host's): all
func fixesArgs(f any) []string {
	s, _ := f.(string)
	switch {
	case s == "none":
		return []string{"--classic"}
	case s == "all" || !fixListOK(s):
		return []string{"--fixes"}
	}
	return []string{"--fixes=" + s}
}

// fixListOK: a list of fix names as src/fixes.c takes them (names of a-z, 0-9 and "-")
func fixListOK(s string) bool {
	if s == "" || s == "list" || len(s) > 512 {
		return false
	}
	for _, n := range strings.Split(s, ",") {
		if n == "" || strings.Trim(n, "abcdefghijklmnopqrstuvwxyz0123456789-") != "" {
			return false
		}
	}
	return true
}

func platformName() string {
	sys := map[string]string{"linux": "Linux", "windows": "Windows", "darwin": "Darwin"}[runtime.GOOS]
	if sys == "" {
		sys = runtime.GOOS
	}
	arch := runtime.GOARCH
	switch {
	case arch == "amd64" && runtime.GOOS == "windows":
		arch = "AMD64"
	case arch == "amd64":
		arch = "x86_64"
	case arch == "arm64" && runtime.GOOS == "linux":
		arch = "aarch64"
	}
	return sys + " " + arch
}

// saveRefused: a savegame the host cannot go on from (a message for the page's language)
type saveRefused = msg

type lobby struct {
	mu          sync.Mutex
	originalDir string
	build       func(docs map[string]obj, mid string) (string, error)    // docs -> the built game files' directory
	startGame   func(args []string, ended func(why string)) (int, error) // the game in the background; ENDED at its end
	dry         bool                                                     // the start only told, not done (tests)
	label       string
	settings    string // the network games' settings file (display, sound)
	savesDir    string // where this player's network games are saved
	conn        msgConn
	server      *exec.Cmd // the host's child process
	serverEnded chan struct{}

	role, address, internet, room string // ("" = none)
	state                         obj
	err                           obj
	mods                          obj // id, hash, dir, status, msg (, want): the content packs here
	modData                       any // their tables as the server has them (kept with the game's savegames)
	save                          obj // the savegame the game goes on from, here
	goInfo                        obj // the start: what the server said, the command, the game
	log                           []string
	bridge                        *bridge
}

func noMods() obj { return obj{"id": nil, "hash": nil, "dir": nil, "status": "none", "msg": ""} }
func noSave() obj { return obj{"hash": nil, "status": "none", "msg": ""} }

func newLobby(originalDir, label string) *lobby {
	l := &lobby{originalDir: originalDir, label: label}
	l.reset()
	return l
}

func (l *lobby) reset() {
	l.mu.Lock()
	defer l.mu.Unlock()
	l.role, l.address, l.internet, l.room = "", "", "", ""
	l.state, l.err, l.goInfo, l.modData = nil, nil, nil, nil
	l.mods, l.save = noMods(), noSave()
	l.log = nil
}

// with: a copy of M with the changes KV (the maps are not changed in place)
func with(m obj, kv ...any) obj {
	out := obj{}
	for k, v := range m {
		out[k] = v
	}
	for i := 0; i+1 < len(kv); i += 2 {
		out[kv[i].(string)] = kv[i+1]
	}
	return out
}

func nullable(s string) any {
	if s == "" {
		return nil
	}
	return s
}

// ------------------------------------------------------------ the connection

func (l *lobby) send(doc obj) error {
	b, _ := json.Marshal(doc)
	l.mu.Lock()
	c := l.conn
	l.mu.Unlock()
	if c == nil {
		return errors.New("not in a game")
	}
	return c.send(b)
}

func (l *lobby) reader(c msgConn) {
	for {
		m, err := c.recv()
		if err != nil {
			break
		}
		func() {
			defer func() { // one bad message: noted, the lobby goes on
				if r := recover(); r != nil {
					l.note(fmt.Sprintf("error: %v", r))
				}
			}()
			v, err := parseJSON(m)
			if doc, ok := v.(obj); err == nil && ok {
				l.message(doc)
			} else {
				l.note(fmt.Sprintf("error: %v", err))
			}
		}()
	}
	l.mu.Lock()
	defer l.mu.Unlock()
	if l.conn == c {
		l.conn = nil
		if l.state != nil && l.state["phase"] == "lobby" {
			why := l.state["why"]
			if !truthy(why) {
				why = "the connection to the game is gone"
			}
			l.state = with(l.state, "phase", "closed", "why", why)
		}
	}
}

func (l *lobby) note(m string) {
	l.mu.Lock()
	defer l.mu.Unlock()
	l.log = append(l.log, m)
	if len(l.log) > 50 {
		l.log = l.log[len(l.log)-50:]
	}
}

func (l *lobby) message(m obj) {
	switch m["t"] {
	case "state":
		l.mu.Lock()
		l.state = m
		if truthy(m["room"]) {
			l.room, _ = m["room"].(string)
		}
		l.mu.Unlock()
		l.modsCheck()
		l.saveCheck()
	case "error":
		l.mu.Lock()
		l.err = obj{"code": m["code"], "msg": m["msg"]}
		l.mu.Unlock()
		l.note(fmt.Sprintf("error: %v", m["msg"]))
	case "mod":
		go l.modsBuild(m, nil)
	case "save":
		go l.saveStore(m)
	case "go":
		l.startFrom(m)
	}
}

func (l *lobby) connect(address string) (string, error) {
	if strings.HasPrefix(address, "ws://") || strings.HasPrefix(address, "wss://") { // the public server: its lobby
		ws, err := wsConnect(strings.TrimRight(address, "/") + "/lobby")
		if err != nil {
			return "", err
		}
		l.mu.Lock()
		l.conn = ws
		l.mu.Unlock()
		go l.reader(ws)
		return strings.TrimRight(address, "/"), nil
	}
	host, port := address, strconv.Itoa(lobbyPort)
	if i := strings.LastIndexByte(address, ':'); i >= 0 {
		host, port = address[:i], address[i+1:]
		if host == "" {
			host, port = address, strconv.Itoa(lobbyPort)
		}
	}
	var c net.Conn
	var last error
	for k := 0; k < 25; k++ { // (a server just started needs a moment)
		if c, last = net.DialTimeout("tcp", net.JoinHostPort(strings.Trim(host, "[]"), port), 5*time.Second); last == nil {
			break
		}
		time.Sleep(200 * time.Millisecond)
	}
	if c == nil {
		return "", fmt.Errorf("no game at %s (%v)", address, last)
	}
	if tc, ok := c.(*net.TCPConn); ok {
		tc.SetNoDelay(true)
	}
	tc := &tcpConn{c: c, r: bufio.NewReaderSize(c, 65536)}
	l.mu.Lock()
	l.conn = tc
	l.mu.Unlock()
	go l.reader(tc)
	return host + ":" + port, nil
}

func (l *lobby) hello(name, key string, more obj) error {
	h, ok := contentHash(l.originalDir)
	if !ok {
		return fmt.Errorf("no SOURCE1.RSC and SOURCE2.RSC in %s", l.originalDir)
	}
	doc := obj{"t": "join", "name": name, "key": key, "version": lobbyVersion(), "files": fmt.Sprintf("%016x", h), "platform": platformName(),
		"seatkey": true} // (the game takes its seat's secret: --mp-key)
	for k, v := range more {
		doc[k] = v
	}
	return l.send(doc)
}

// ------------------------------------------------------------ what the page does

// host: open a game -- the lobby server (this program's: openmtv serve, mpserver.go) and
// this launcher in it as the host
func (l *lobby) host(name string, port int, listen, logPath, pace string) error {
	l.leave()
	var k [8]byte
	rand.Read(k[:])
	key := hex.EncodeToString(k[:])
	self, err := os.Executable()
	if err != nil {
		return err
	}
	args := []string{"serve", "--lobby", "--key", key, "--host", listen, "--port", strconv.Itoa(port), "--pace", pace}
	if logPath != "" {
		args = append(args, "--log", logPath)
	}
	os.MkdirAll(filepath.Join(root, "build", "mp"), 0o777)
	c := exec.Command(self, args...)
	c.Dir = root
	if log, err := os.Create(filepath.Join(root, "build", "mp", "server.log")); err == nil {
		c.Stdout, c.Stderr = log, log
		defer log.Close()
	}
	detach(c)
	if err := c.Start(); err != nil {
		return err
	}
	ended := make(chan struct{})
	go func() { c.Wait(); close(ended) }()
	l.reset()
	l.mu.Lock()
	l.server, l.serverEnded = c, ended
	l.mu.Unlock()
	addr, err := l.connect(fmt.Sprintf("127.0.0.1:%d", port))
	if err == nil {
		err = l.hello(name, key, nil)
	}
	if err != nil {
		c.Process.Kill()
		l.mu.Lock()
		l.server = nil
		l.mu.Unlock()
		return err
	}
	// the server's first state: none if it did not start (the port taken: then this launcher
	// may have reached another program there) -- refused, with the server's last word
	until := time.Now().Add(10 * time.Second)
	for time.Now().Before(until) {
		l.mu.Lock()
		got, conn := l.state != nil, l.conn != nil
		l.mu.Unlock()
		ended := false
		select {
		case <-l.serverEnded:
			ended = true
		default:
		}
		if got || !conn || ended {
			break
		}
		time.Sleep(50 * time.Millisecond)
	}
	time.Sleep(50 * time.Millisecond)
	l.mu.Lock()
	got := l.state != nil
	l.mu.Unlock()
	if !got {
		why := serverSaid()
		if why == "" {
			why = "no answer"
		}
		l.leave()
		return fmt.Errorf("no game opened on port %d (%s)", port, why)
	}
	l.mu.Lock()
	l.address, l.role = addr, "host"
	l.mu.Unlock()
	l.note(fmt.Sprintf("opened a game on port %d", port))
	return nil
}

// serverSaid: the last line of the LAN server's messages (build/mp/server.log): why it
// did not start
func serverSaid() string {
	b, err := os.ReadFile(filepath.Join(root, "build", "mp", "server.log"))
	if err != nil {
		return ""
	}
	lines := strings.Split(strings.ReplaceAll(string(b), "\r\n", "\n"), "\n")
	for i := len(lines) - 1; i >= 0; i-- {
		if s := strings.TrimSpace(lines[i]); s != "" {
			return s
		}
	}
	return ""
}

func (l *lobby) join(address, name string) error {
	l.leave()
	l.reset()
	addr, err := l.connect(strings.TrimSpace(address))
	if err != nil {
		return err
	}
	l.mu.Lock()
	l.address = addr
	l.mu.Unlock()
	if err := l.hello(name, "", nil); err != nil {
		return err
	}
	l.mu.Lock()
	l.role = "guest"
	l.mu.Unlock()
	l.note("joined " + addr)
	return nil
}

// hostInternet: open a game on the public server -- a new room, this launcher its host
func (l *lobby) hostInternet(name, server string, listed bool) error {
	l.leave()
	l.reset()
	addr, err := l.connect(server)
	if err != nil {
		return err
	}
	l.mu.Lock()
	l.address, l.internet = addr, addr
	l.mu.Unlock()
	if err := l.hello(name, "", obj{"create": true, "listed": listed}); err != nil {
		return err
	}
	l.mu.Lock()
	l.role = "host"
	l.mu.Unlock()
	until := time.Now().Add(10 * time.Second)
	for time.Now().Before(until) {
		l.mu.Lock()
		room, c := l.room, l.conn
		l.mu.Unlock()
		if room != "" || c == nil {
			break
		}
		time.Sleep(50 * time.Millisecond)
	}
	l.mu.Lock()
	room, e := l.room, l.err
	l.mu.Unlock()
	if room == "" {
		why := "no answer"
		if e != nil && truthy(e["msg"]) {
			why = fmt.Sprint(e["msg"])
		}
		l.leave()
		return fmt.Errorf("the server %s opened no game (%s)", server, why)
	}
	l.note(fmt.Sprintf("opened the game %s on %s", room, server))
	return nil
}

// joinInternet: join a game of the public server by its room (its code)
func (l *lobby) joinInternet(room, name, server string) error {
	l.leave()
	l.reset()
	addr, err := l.connect(server)
	if err != nil {
		return err
	}
	room = strings.ToLower(strings.TrimSpace(room))
	l.mu.Lock()
	l.address, l.internet, l.room = addr, addr, room
	l.mu.Unlock()
	if err := l.hello(name, "", obj{"room": room}); err != nil {
		return err
	}
	l.mu.Lock()
	l.role = "guest"
	l.mu.Unlock()
	l.note(fmt.Sprintf("joined the game %s on %s", room, server))
	return nil
}

func (l *lobby) me(kw obj) error {
	doc := obj{"t": "me"}
	for _, k := range []string{"name", "station", "ready"} {
		if v, ok := kw[k]; ok && v != nil {
			doc[k] = v
		}
	}
	return l.send(doc)
}

const keepDocs = 2 // setGame: the packs as they are

// setGame: the host -- the game's settings; the chosen packs' tables (docsMode 0: the
// original game, 1: DOCS, keepDocs: unchanged)
func (l *lobby) setGame(settings obj, docs map[string]obj, docsMode int, packs []any) error {
	l.mu.Lock()
	role := l.role
	l.mu.Unlock()
	if role != "host" {
		return errors.New("only the host sets the game")
	}
	s := with(settings)
	if packs != nil {
		s["packs"] = packs
	}
	if len(s) > 0 {
		if err := l.send(obj{"t": "settings", "settings": s}); err != nil {
			return err
		}
	}
	switch docsMode {
	case 0:
		l.mu.Lock()
		l.mods, l.modData = noMods(), nil
		l.mu.Unlock()
		return l.send(obj{"t": "mod", "hash": nil, "data": nil})
	case 1:
		mid, data := packDocs(docs)
		l.mu.Lock()
		mods := l.mods
		l.mu.Unlock()
		if mods["id"] != mid || mods["status"] != "ok" {
			l.modsBuild(obj{"hash": nil, "data": data, "id": mid}, docs)
		}
		l.mu.Lock()
		mods = l.mods
		if mods["status"] == "ok" {
			l.modData = data
		}
		l.mu.Unlock()
		if mods["status"] == "ok" {
			return l.send(obj{"t": "mod", "hash": mods["hash"], "data": data})
		}
	}
	return nil
}

// chooseSave: the host -- go on from a savegame of this machine's (the game's directory,
// tag-NNN.mtvmp), with the game's own rules and packs; or ("") start a new game
func (l *lobby) chooseSave(game, file any) error {
	l.mu.Lock()
	role, savesDir := l.role, l.savesDir
	l.mu.Unlock()
	if role != "host" {
		return errors.New("only the host sets the game")
	}
	if !truthy(game) {
		l.mu.Lock()
		l.save = noSave()
		l.mu.Unlock()
		return l.send(obj{"t": "save", "hash": nil})
	}
	d := filepath.Join(savesDir, safeID(game))
	f, err := safeFile(file)
	if err != nil {
		return err
	}
	p := filepath.Join(d, f)
	data, err := os.ReadFile(p)
	var meta obj
	if err == nil {
		meta, err = saveMeta(data)
	}
	if err != nil {
		return saveRefused(newMsg("this savegame cannot be read"))
	}
	info := gameInfo(filepath.Join(d, "game.json"))
	packs := info["packs"].([]any)
	// the saved game's rules: as its start had them (game.json), else as the savegame says
	s := info["settings"].(obj)
	rules := obj{}
	for _, k := range []string{"fixes", "speed4", "aiimp", "credit", "terror", "sammy", "offers", "filmoffers"} {
		if v, ok := s[k]; ok {
			rules[k] = v
		}
	}
	if _, ok := rules["fixes"]; !ok {
		if f, ok := meta["fixes"].(string); ok {
			rules["fixes"] = map[string]string{"classic": "none", "all": "all"}[f]
			if rules["fixes"] == "" {
				rules["fixes"] = f
			}
		}
	}
	if _, ok := rules["speed4"]; !ok {
		if v, ok := meta["speed4"]; ok {
			rules["speed4"] = truthy(v)
		}
	}
	if a, _ := rules["aiimp"].(string); !contains(aiImprove, a) { // (a savegame from before the option: off)
		rules["aiimp"] = "off"
		if v, ok := meta["aiimp"].(int64); ok && v >= 0 && v <= 2 {
			rules["aiimp"] = aiImprove[v]
		}
	}
	if _, ok := rules["credit"]; !ok { // (from the savegame: -1 or none = the original's line; 0 the original's credit system)
		rules["credit"] = creditLine(meta["credit"], creditLow)
	}
	// (from the savegame: -1 or none = the original's $50 000, and no Sammy prize)
	if v, ok := rules["terror"]; ok {
		rules["terror"] = dollars(v, terrorDef)
	} else {
		rules["terror"] = dollars(meta["terror"], terrorDef)
	}
	if v, ok := rules["sammy"]; ok {
		rules["sammy"] = sammyPrize(v)
	} else {
		rules["sammy"] = sammyPrize(meta["sammy"])
	}
	for _, k := range []string{"offers", "filmoffers"} { // (from the savegame: none = the original's 10)
		if v, ok := rules[k]; ok {
			rules[k] = offersCount(v)
		} else {
			rules[k] = offersCount(meta[k])
		}
	}
	// its packs (their tables as its start had them): built and compared before anything is sent
	var docs map[string]obj
	var mid, dataB64 string
	if len(packs) > 0 {
		md, ok := info["mod_data"].(string)
		if !ok || md == "" {
			return saveRefused(newMsg("this saved game was played with packs whose tables are not kept here"))
		}
		if docs, err = unpackDocs(md); err != nil {
			return saveRefused(newMsg("the packs of this saved game cannot be read (game.json)"))
		}
		mid, dataB64 = packDocs(docs)
	}
	l.mu.Lock()
	keptMods, keptData := l.mods, l.modData
	l.mu.Unlock()
	restore := func() {
		l.mu.Lock()
		l.mods, l.modData = keptMods, keptData // (the lobby's packs stay as they were)
		l.mu.Unlock()
	}
	var files any
	if docs != nil {
		if keptMods["id"] != mid || keptMods["status"] != "ok" {
			l.modsBuild(obj{"hash": nil, "data": dataB64, "id": mid}, docs)
		}
		l.mu.Lock()
		mods := l.mods
		l.mu.Unlock()
		if mods["status"] != "ok" {
			why := mods["msg"]
			if !truthy(why) {
				why = mods["status"]
			}
			restore()
			return saveRefused(newMsg("the packs of this saved game could not be built: {why}", "why", why))
		}
		files = mods["hash"]
	} else {
		h, _ := contentHash(l.originalDir)
		files = fmt.Sprintf("%016x", h)
	}
	if truthy(meta["files"]) && !equal(meta["files"], files) {
		restore()
		return saveRefused(newMsg("this saved game was played with other game files ({saved}, here {here})", "saved", meta["files"], "here", files))
	}
	mode := 0
	if docs != nil {
		mode = 1
	}
	if err := l.setGame(rules, docs, mode, packs); err != nil {
		return err
	}
	h := saveHash(data)
	l.mu.Lock()
	l.save = obj{"hash": h, "status": "ok", "msg": "", "path": p, "game": safeID(game), "file": f, "day": meta["day"]}
	l.mu.Unlock()
	return l.send(obj{"t": "save", "hash": h, "data": base64.StdEncoding.EncodeToString(deflate(data)),
		"meta": with(meta, "game", safeID(game), "file", f, "created", info["created"], "packs", packs, "settings", s)})
}

func (l *lobby) start() error {
	l.mu.Lock()
	role := l.role
	if role == "host" {
		l.err = nil
	}
	l.mu.Unlock()
	if role != "host" {
		return errors.New("only the host starts the game")
	}
	return l.send(obj{"t": "start"})
}

func (l *lobby) leave() {
	l.mu.Lock()
	c := l.conn
	l.conn = nil
	server, ended, started := l.server, l.serverEnded, l.goInfo != nil && l.goInfo["pid"] != nil
	br := l.bridge
	l.server, l.bridge = nil, nil
	l.role = ""
	l.mu.Unlock()
	if br != nil && !started { // (a game that runs keeps its way to the server)
		br.close()
	}
	if c != nil {
		b, _ := json.Marshal(obj{"t": "leave"})
		c.send(b)
		c.close()
	}
	if server != nil && !started { // the lobby closed before a game: the server goes too -- it ends by
		select { //                        itself once it has told the others, else after a second
		case <-ended:
		case <-time.After(time.Second):
			server.Process.Kill()
		}
	}
}

// ------------------------------------------------------------ the content packs

// modsCheck: a guest -- the host's packs changed: fetch and build them
func (l *lobby) modsCheck() {
	l.mu.Lock()
	settings, _ := l.state["settings"].(obj)
	want := settings["mods"]
	role, mods := l.role, l.mods
	l.mu.Unlock()
	if role != "guest" {
		return
	}
	if !truthy(want) {
		l.mu.Lock()
		l.modData = nil
		reset := truthy(mods["hash"])
		if reset {
			l.mods = noMods()
		}
		l.mu.Unlock()
		if reset {
			l.send(obj{"t": "me"})
		}
		return
	}
	if equal(mods["want"], want) { // fetched, built (or failed) for these packs already
		return
	}
	l.mu.Lock()
	l.mods = with(noMods(), "status", "fetching", "want", want)
	l.modData = nil
	l.mu.Unlock()
	l.send(obj{"t": "getmod"})
}

// modsBuild: the packs' tables built from this machine's original files; a guest reports
// the hash
func (l *lobby) modsBuild(m obj, docs map[string]obj) {
	data, _ := m["data"].(string)
	if data == "" {
		return
	}
	mid := m["id"]
	l.mu.Lock()
	l.mods = with(l.mods, "status", "building", "msg", "")
	l.mu.Unlock()
	err := func() error {
		var err error
		if docs == nil {
			if docs, err = unpackDocs(data); err != nil {
				return err
			}
			mid, _ = packDocs(docs)
		}
		if l.build == nil {
			return errors.New("this launcher cannot build packs")
		}
		id, _ := mid.(string)
		out, err := l.build(docs, id)
		if err != nil {
			return err
		}
		h, _ := contentHash(out)
		mods := obj{"id": mid, "hash": fmt.Sprintf("%016x", h), "dir": out, "status": "ok", "msg": "", "want": m["hash"]}
		if truthy(m["hash"]) && !equal(mods["hash"], m["hash"]) {
			mods = with(mods, "status", "differ", "msg", "built, but other files than the host's: other original files?")
		}
		l.mu.Lock()
		l.mods, l.modData = mods, data
		l.mu.Unlock()
		l.note("built the packs in " + rel(out))
		return nil
	}()
	if err != nil {
		l.mu.Lock()
		l.mods = with(l.mods, "status", "failed", "msg", err.Error(), "want", m["hash"])
		l.mu.Unlock()
		l.note("the packs not built: " + err.Error())
	}
	l.mu.Lock()
	role, mods := l.role, l.mods
	l.mu.Unlock()
	if role == "guest" {
		var h any
		if mods["status"] == "ok" {
			h = mods["hash"]
		}
		l.send(obj{"t": "me", "mods": h})
	}
}

// ------------------------------------------------------------ the savegame

// saveCheck: a guest -- the host goes on from a savegame: this machine's copy, or fetch it
func (l *lobby) saveCheck() {
	l.mu.Lock()
	role, save, savesDir := l.role, l.save, l.savesDir
	sv, _ := l.state["save"].(obj)
	l.mu.Unlock()
	if role != "guest" {
		return
	}
	if sv == nil {
		if truthy(save["hash"]) || truthy(save["want"]) {
			l.mu.Lock()
			l.save = noSave()
			l.mu.Unlock()
			l.send(obj{"t": "me", "save": nil})
		}
		return
	}
	if equal(save["want"], sv["hash"]) { // (here, fetching, or failed: for this savegame already)
		return
	}
	l.mu.Lock()
	l.save = obj{"hash": nil, "status": "fetching", "msg": "", "want": sv["hash"]}
	l.mu.Unlock()
	meta, _ := sv["meta"].(obj)
	if f, err := safeFile(meta["file"]); err == nil { // a game this machine played too: its own copy
		p := filepath.Join(filepath.Join(savesDir, safeID(meta["game"])), f)
		if data, err := os.ReadFile(p); err == nil && equal(saveHash(data), sv["hash"]) {
			l.saveHave(p, saveHash(data), meta)
			return
		}
	}
	l.send(obj{"t": "getsave"})
}

// saveStore: a guest -- the host's savegame, kept with this machine's (another player may
// host it next time)
func (l *lobby) saveStore(m obj) {
	l.mu.Lock()
	role, want, savesDir := l.role, l.save["want"], l.savesDir
	l.mu.Unlock()
	h, _ := m["hash"].(string)
	if role != "guest" || h == "" || !equal(h, want) {
		return
	}
	err := func() error {
		d64, _ := m["data"].(string)
		data, err := inflate(d64, saveMax)
		if err != nil {
			return err
		}
		if saveHash(data) != h {
			return errors.New("the savegame came damaged")
		}
		meta, _ := m["meta"].(obj)
		if _, err := saveMeta(data); err != nil {
			return err
		}
		if savesDir == "" {
			return errors.New("no place for savegames")
		}
		game := safeID(meta["game"])
		if game == "" {
			game = "game"
		}
		d := filepath.Join(savesDir, game)
		if err := os.MkdirAll(d, 0o777); err != nil {
			return err
		}
		f, err := safeFile(meta["file"])
		if err != nil {
			return err
		}
		p := filepath.Join(d, f)
		// (two copies of one night at once: one after the other)
		defer lockDir(d)()
		if old, err := os.ReadFile(p); err == nil { // another copy of that night: kept beside it, not lost
			if oh := saveHash(old); oh != h {
				os.Rename(p, p+"."+oh+".bak")
			}
		}
		if err := replaceFile(p, data); err != nil {
			return err
		}
		if !exists(filepath.Join(d, "game.json")) {
			var packs []any
			if l, ok := meta["packs"].([]any); ok {
				for _, x := range l {
					if s, ok := x.(string); ok && len(packs) < 16 {
						packs = append(packs, s)
					}
				}
			}
			if packs == nil {
				packs = []any{}
			}
			settings := obj{}
			if s, ok := meta["settings"].(obj); ok {
				for k, v := range s {
					if !contains([]string{"lang", "fixes", "difficulty", "speed4", "aiimp", "money", "credit", "terror", "sammy", "offers", "filmoffers", "sertop", "knock", "castfit"}, k) {
						continue
					}
					switch v.(type) {
					case string, int64, bool:
						settings[k] = v
					default:
						if k == "sammy" && equal(v, sammyPrize(v)) {
							settings[k] = v
						}
					}
				}
			}
			var created any
			if c, ok := meta["created"].(string); ok {
				created = c
			}
			writeJSONFile(filepath.Join(d, "game.json"), obj{"game": game, "created": created, "packs": packs, "settings": settings})
		}
		l.note(fmt.Sprintf("the savegame %s/%s fetched", game, f))
		l.saveHave(p, h, meta)
		return nil
	}()
	if err != nil {
		l.mu.Lock()
		l.save = with(l.save, "status", "failed", "msg", err.Error())
		l.mu.Unlock()
		l.note("the savegame not fetched: " + err.Error())
	}
}

func (l *lobby) saveHave(path, h string, meta obj) {
	l.mu.Lock()
	l.save = obj{"hash": h, "status": "ok", "msg": "", "path": path, "want": h, "game": safeID(meta["game"]),
		"file": filepath.Base(path), "day": meta["day"]}
	l.mu.Unlock()
	l.send(obj{"t": "me", "save": h})
}

// ------------------------------------------------------------ the start

// command: the arguments of the start the server announced (as ./play.sh takes them)
func (l *lobby) command(m obj) ([]string, error) {
	s, _ := m["settings"].(obj)
	lang, _ := s["lang"].(string)
	if lang != "d" { // (the host's setting, the game's first argument: nothing else)
		lang = "e"
	}
	ai, _ := s["aiimp"].(string)
	if !contains(aiImprove, ai) {
		ai = "off"
	}
	var prize []string
	for _, x := range sammyPrize(s["sammy"]) {
		prize = append(prize, fmt.Sprint(x))
	}
	speed := "no"
	if truthy(s["speed4"]) {
		speed = "yes"
	}
	sertop := "yes" // the series card's topicality: on unless the host turned it off (a lobby without the key: on)
	if v, ok := s["sertop"]; ok && !truthy(v) {
		sertop = "no"
	}
	fit := "no"
	if truthy(s["castfit"]) {
		fit = "yes"
	}
	args := append([]string{lang}, fixesArgs(s["fixes"])...)
	args = append(args, "--fourth-speed="+speed, "--ai-improve="+ai, creditArg(creditLine(s["credit"], creditDef)),
		fmt.Sprintf("--terror-damage=%d", dollars(s["terror"], terrorDef)), "--sammy-prize="+strings.Join(prize, ","), fmt.Sprintf("--ad-offers=%d", offersCount(s["offers"])),
		fmt.Sprintf("--film-offers=%d", offersCount(s["filmoffers"])), "--series-topicality="+sertop, fmt.Sprintf("--knock=%d", knockMinutes(s["knock"])),
		"--cast-fit="+fit) // (the host's for everyone: over each player's own [display] cast_fit)
	l.mu.Lock()
	save, mods, settings := l.save, l.mods, l.settings
	l.mu.Unlock()
	num := func(v any, def int64) string {
		switch x := v.(type) {
		case int64:
			return strconv.FormatInt(x, 10)
		case float64:
			return strconv.FormatInt(int64(x), 10)
		case string:
			if n, err := strconv.ParseInt(strings.TrimSpace(x), 10, 64); err == nil {
				return strconv.FormatInt(n, 10)
			}
		case nil:
			return strconv.FormatInt(def, 10)
		}
		return strconv.FormatInt(def, 10)
	}
	if truthy(m["load"]) { // on from a savegame: the same file on every machine
		if !equal(save["hash"], m["load"]) || !truthy(save["path"]) {
			return nil, errors.New("the savegame is not here")
		}
		args = append(args, "--mp-load", save["path"].(string))
	} else {
		var names []string
		if l, ok := m["names"].([]any); ok {
			for _, n := range l {
				s, _ := n.(string)
				names = append(names, s)
			}
		}
		args = append(args, "--start-money", num(s["money"], 200000), "--new-game", "--difficulty", num(s["difficulty"], 2),
			"--names", strings.Join(names, ","))
	}
	humans, _ := m["humans"].(string)
	addr, err := l.gameAddress()
	if err != nil {
		return nil, err
	}
	args = append(args, "--seed", num(m["seed"], 0), "--humans", humans, "--seat", num(m["seat"], 0), "--mp", addr, "--mouse=system")
	if k, _ := m["key"].(string); seatKeyOK(k) { // the seat's secret (an older server: none)
		args = append(args, "--mp-key", k)
	}
	if gdir := l.gameDir(m); gdir != "" { // every night's savegame (src/mpsave.c)
		args = append(args, "--mp-save", gdir)
	}
	if settings != "" { // this player's own display and sound for network games
		args = append(args, "--settings="+settings)
	}
	if truthy(s["mods"]) {
		if mods["status"] != "ok" || !equal(mods["hash"], s["mods"]) {
			return nil, errors.New("the packs are not built here")
		}
		args = append(args, "--game-files", mods["dir"].(string))
	}
	return args, nil
}

// seatKeyOK: a seat's secret as the game takes it: 1..64 hex digits
func seatKeyOK(k string) bool {
	if k == "" || len(k) > 64 {
		return false
	}
	for _, r := range k {
		if !strings.ContainsRune("0123456789abcdef", r) {
			return false
		}
	}
	return true
}

// gameAddress: where the game connects -- the server, or (on the internet) this
// launcher's bridge to it
func (l *lobby) gameAddress() (string, error) {
	l.mu.Lock()
	internet, address, room := l.internet, l.address, l.room
	l.mu.Unlock()
	if internet == "" {
		return address, nil
	}
	b, err := newBridge(internet+"/game/"+room, 180*time.Second, func(why string) {
		l.mu.Lock()
		l.err = obj{"code": "bridge", "msg": "your game could not reach the server: " + why}
		l.mu.Unlock()
		l.note("bridge: " + why)
	})
	if err != nil {
		return "", err
	}
	l.mu.Lock()
	old := l.bridge
	l.bridge = b
	l.mu.Unlock()
	if old != nil { // (a game before: its bridge ends)
		old.close()
	}
	return fmt.Sprintf("127.0.0.1:%d", b.port), nil
}

// gameDir: the game's directory for its savegames, made, with game.json (its packs'
// tables and rules: so a savegame can go on later, hosted by any of its players)
func (l *lobby) gameDir(m obj) string {
	game := safeID(m["game"])
	l.mu.Lock()
	savesDir, mods, modData := l.savesDir, l.mods, l.modData
	l.mu.Unlock()
	if savesDir == "" || game == "" {
		return ""
	}
	d := filepath.Join(savesDir, game)
	os.MkdirAll(d, 0o777)
	p := filepath.Join(d, "game.json")
	info := readJSONObj(p)
	s, _ := m["settings"].(obj)
	if len(info) == 0 {
		packs := s["packs"]
		if !truthy(packs) {
			packs = []any{}
		}
		settings := obj{}
		for _, k := range []string{"lang", "fixes", "difficulty", "speed4", "aiimp", "money", "credit", "terror", "sammy", "offers", "filmoffers", "sertop", "knock", "castfit"} {
			if v, ok := s[k]; ok {
				settings[k] = v
			}
		}
		info = obj{"game": game, "created": nowStamp(), "packs": packs, "settings": settings, "names": m["names"], "version": lobbyVersion()}
	}
	if truthy(s["mods"]) && modData != nil && equal(mods["hash"], s["mods"]) && !truthy(info["mod_data"]) {
		info["mod_data"] = modData // (the tables of the packs this game is played with, no others)
	}
	writeJSONFile(p, info)
	return d
}

// gameEnded: this machine's game PID has ended -- why, if not as a game ends (the page shows
// it, if that game is still the lobby's); its bridge to the server (BR, an internet game's) ends
func (l *lobby) gameEnded(pid int, why string, br *bridge) {
	l.mu.Lock()
	if l.goInfo != nil && why != "" && l.goInfo["pid"] == pid {
		l.goInfo = with(l.goInfo, "ended", why)
	}
	if br != nil && l.bridge == br {
		l.bridge = nil
	}
	l.mu.Unlock()
	if br != nil {
		br.close()
	}
	if why != "" {
		l.note("the game ended: " + why)
	}
}

func (l *lobby) startFrom(m obj) {
	g, err := func() (obj, error) {
		args, err := l.command(m)
		if err != nil {
			return nil, err
		}
		cmd, err := prepare(args, true)
		if err != nil {
			return nil, err
		}
		g := obj{"seat": m["seat"], "humans": m["humans"], "seed": m["seed"], "args": args, "cmd": cmd, "pid": nil}
		if !l.dry {
			l.mu.Lock()
			br := l.bridge // (the game's way to the server: it ends with the game)
			l.mu.Unlock()
			started := make(chan int, 1)
			pid, err := l.startGame(args, func(why string) { l.gameEnded(<-started, why, br) })
			if err != nil {
				return nil, err
			}
			started <- pid
			g["pid"] = pid
		}
		return g, nil
	}()
	if err != nil {
		l.mu.Lock()
		l.goInfo = obj{"error": err.Error()}
		l.mu.Unlock()
		l.note("the game did not start: " + err.Error())
		return
	}
	l.mu.Lock()
	l.goInfo = g
	l.mu.Unlock()
	if l.dry {
		l.note(fmt.Sprintf("the game would start (seat %v)", m["seat"]))
	} else {
		l.note(fmt.Sprintf("the game started (seat %v)", m["seat"]))
	}
}

// ------------------------------------------------------------ for the page

func (l *lobby) snapshot() obj {
	addrs := lanAddresses() // (not under the lock)
	l.mu.Lock()
	defer l.mu.Unlock()
	strip := func(m obj, k string) obj {
		out := obj{}
		for a, v := range m {
			if a != k {
				out[a] = v
			}
		}
		return out
	}
	var you, goInfo, cmd, argv any
	if l.state != nil {
		you = l.state["you"]
	}
	if l.goInfo != nil {
		goInfo = strip(l.goInfo, "cmd")
		if c, ok := l.goInfo["cmd"].([]string); ok {
			cmd, argv = strings.Join(c, " "), c
		}
	}
	var state, errv any
	if l.state != nil {
		state = l.state
	}
	if l.err != nil {
		errv = l.err
	}
	return obj{"role": nullable(l.role), "address": nullable(l.address), "internet": nullable(l.internet), "room": nullable(l.room),
		"server": internetServer(), "state": state, "error": errv, "you": you, "mods": strip(l.mods, "dir"),
		"save": strip(l.save, "path"), "go": goInfo, "cmd": cmd, "argv": argv, "log": append([]string{}, l.log...),
		"version": lobbyVersion(), "addresses": addrs, "connected": l.conn != nil}
}

// rooms: the open games of the public server
func rooms(server string) ([]obj, error) {
	u, err := url.Parse(server)
	if err != nil {
		return nil, fmt.Errorf("the server %s does not answer (%v)", server, err)
	}
	if sc, ok := map[string]string{"wss": "https", "ws": "http"}[u.Scheme]; ok { // (http, https as they are)
		u.Scheme = sc
	}
	u.Path, u.RawQuery = "/rooms", ""
	c := http.Client{Timeout: 8 * time.Second}
	r, err := c.Get(u.String())
	if err != nil {
		return nil, fmt.Errorf("the server %s does not answer (%v)", server, err)
	}
	defer r.Body.Close()
	if r.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("the server %s does not answer (%s)", server, http.StatusText(r.StatusCode)) // (Python: the reason)
	}
	var list []obj
	data, err := io.ReadAll(io.LimitReader(r.Body, 4<<20)) // (64 games' descriptions are a few KB)
	if err != nil {
		return nil, fmt.Errorf("the server %s does not answer (%v)", server, err)
	}
	v, err := parseJSON(data)
	l, ok := v.([]any)
	if err != nil || !ok {
		return nil, fmt.Errorf("the server %s does not answer (%v)", server, err)
	}
	for _, x := range l {
		if g, ok := x.(obj); ok && truthy(g["room"]) {
			list = append(list, g)
		}
	}
	if list == nil {
		list = []obj{}
	}
	return list, nil
}
