package main

import (
	"bufio"
	"bytes"
	"crypto/rand"
	"crypto/sha1"
	"crypto/subtle"
	"encoding/base64"
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io"
	mrand "math/rand"
	"net"
	"os"
	"runtime"
	"sort"
	"strconv"
	"strings"
	"sync"
	"time"
	"unicode/utf8"
)

// The network game's server (docs/multiplayer.md stages 4-6; what the former Python server (mpserver.py)
// does, the same protocol to the byte, see its docstring): `openmtv serve` with its
// options.  The frame server orders the games' inputs and bundles them, at the
// clients' pace (realtime) or as fast as the slowest (fast), compares their
// hashes, waits for a lost seat; the lobby (--lobby) is where the launchers meet
// before the game; --public is the internet's server: HTTP and WebSocket on its
// port, many rooms, each a lobby and its game.  Python's asyncio runs it in one
// thread; here one lock stands for that loop (every message and every tick
// under it) and every connection has a queue of its own that a goroutine writes
// out, so a slow client holds up nobody.

const (
	frameS     = 1.0 / 70
	maxPlayers = 3
	saveDataMx = 256 << 10 // a savegame as base64 of zlib
	saveMetaMx = 8 << 10   // its description
	modDataMx  = 8 << 20   // a selection's tables as base64 of zlib
	maxMsg     = 16 << 20
	maxRooms   = 64
	maxQueued  = 4 * maxMsg // bytes waiting for a connection that does not read: it is dropped
	handshakeS = 30         // seconds a connection has to join the lobby or take a seat
	idleS      = 600        // seconds a WebSocket that joined may be silent (pings answered count; not dead yet: a laptop's short sleep, a slow line's 8 MB pack before the ping)
	pingS      = 25         // seconds between the server's pings of a WebSocket
	lobbyS     = 2 * 3600   // seconds a public room's lobby stays open with nobody speaking in it
	maxSetting = 4 << 10    // a lobby setting's value as JSON (the longest: the packs' names)
	maxInput   = 256        // an input's command and arguments (runtime/host.c sends 255 bytes at most)
	maxInputs  = 1 << 14    // the inputs of a seat the server holds (its script's, sent before the start)
	inputAhead = 4          // windows past the frames sent: the latest frame a running game's input may name
)

type mpArgs struct {
	seats                         int
	lobby, public                 bool
	key, host, pace, log          string
	startWait, port               int
	lead, margin, window, dropAft int
	silent                        int
}

// ---------------------------------------------------------------- a connection's way out

// outConn: what the server writes to a connection, queued; a goroutine writes it out
// (TCP: 32-bit length and the text; WebSocket: one binary message each)
type outConn struct {
	c       net.Conn
	ws      bool
	mu      sync.Mutex
	buf     [][]byte
	queued  int // the bytes in buf
	closing bool
	wake    chan struct{}
	done    chan struct{}
}

func newOut(c net.Conn, ws bool) *outConn {
	o := &outConn{c: c, ws: ws, wake: make(chan struct{}, 1), done: make(chan struct{})}
	go o.run()
	return o
}

func (o *outConn) enqueue(b []byte) {
	o.mu.Lock()
	if !o.closing {
		o.buf = append(o.buf, b)
		o.queued += len(b)
		if o.queued > maxQueued { // (it does not read: it goes, the others' game does not wait on it)
			o.closing, o.buf = true, nil
			o.c.Close()
		}
	}
	o.mu.Unlock()
	select {
	case o.wake <- struct{}{}:
	default:
	}
}

// write: one message
func (o *outConn) write(msg []byte) {
	if o.ws {
		o.enqueue(wsFrame(2, msg, false))
	} else {
		o.enqueue(append(binary.LittleEndian.AppendUint32(nil, uint32(len(msg))), msg...))
	}
}

func (o *outConn) ping() { o.enqueue(wsFrame(9, nil, false)) }

// close: after what is queued (and a WebSocket's close)
func (o *outConn) close() {
	if o.ws {
		o.enqueue(wsFrame(8, nil, false))
	}
	o.mu.Lock()
	o.closing = true
	o.mu.Unlock()
	select {
	case o.wake <- struct{}{}:
	default:
	}
}

func (o *outConn) run() {
	defer close(o.done)
	for range o.wake {
		for {
			o.mu.Lock()
			b, closing := o.buf, o.closing
			o.buf, o.queued = nil, 0
			o.mu.Unlock()
			if len(b) == 0 {
				if closing {
					o.c.Close()
					return
				}
				break
			}
			o.c.SetWriteDeadline(time.Now().Add(30 * time.Second))
			if _, err := o.c.Write(bytes.Join(b, nil)); err != nil {
				o.c.Close()
				o.mu.Lock()
				o.closing, o.buf = true, nil
				o.mu.Unlock()
				return
			}
		}
	}
}

// ---------------------------------------------------------------- the hub: one port

type mpHub struct {
	a     *mpArgs
	mu    sync.Mutex // (the event loop)
	t0    time.Time
	rooms map[string]*gameServer
	one   *gameServer
	times [4]time.Duration // (the tests' shorter) handshakeS, idleS, pingS, lobbyS
}

func (h *mpHub) mono() float64 { return time.Since(h.t0).Seconds() }

// timeOf: handshakeS, idleS, pingS or lobbyS (I 0-3), or the test's
func (h *mpHub) timeOf(i int) time.Duration {
	if h.times[i] > 0 {
		return h.times[i]
	}
	return time.Duration([]int{handshakeS, idleS, pingS, lobbyS}[i]) * time.Second
}

// lifeConn: a connection's reads -- the handshake's deadline until it joined the lobby or took
// a seat (join), then IDLE without a byte from it ends it (0: nothing ends it, plain TCP: nothing
// pings it; a WebSocket answers the server's pings, so only a dead one ends)
type lifeConn struct {
	net.Conn
	joined bool
	idle   time.Duration
}

func (l *lifeConn) Read(p []byte) (int, error) {
	if l.joined && l.idle > 0 {
		l.Conn.SetReadDeadline(time.Now().Add(l.idle))
	}
	return l.Conn.Read(p)
}

func (l *lifeConn) join() {
	if !l.joined {
		l.joined = true
		if l.idle == 0 {
			l.Conn.SetReadDeadline(time.Time{})
		}
	}
}

func (h *mpHub) note(m string) { fmt.Fprintf(os.Stderr, "[mpserver] %s\n", m) }

func serveCommand(args []string) int {
	fs := flag.NewFlagSet("serve", flag.ContinueOnError)
	a := &mpArgs{}
	fs.IntVar(&a.seats, "seats", 0, "the human seats (clients) of the game (without --lobby)")
	fs.BoolVar(&a.lobby, "lobby", false, "the launchers meet first (stage 5)")
	fs.BoolVar(&a.public, "public", false, "the internet's server: many games, HTTP and WebSocket (stage 6)")
	fs.StringVar(&a.key, "key", "", "the host's key (--lobby)")
	fs.IntVar(&a.startWait, "start-wait", 60, "seconds the started games have to say hello (--lobby)")
	fs.StringVar(&a.host, "host", "0.0.0.0", "the addresses to listen on, A[,B...]")
	fs.IntVar(&a.port, "port", lobbyPort, "the port")
	fs.StringVar(&a.pace, "pace", "realtime", "realtime or fast")
	fs.IntVar(&a.lead, "lead", 12, "frames ahead of the fastest client at most")
	fs.IntVar(&a.margin, "margin", 2, "frames of the lead beyond the longest round trip")
	fs.IntVar(&a.window, "window", 70, "frames ahead of the slowest client")
	fs.IntVar(&a.dropAft, "drop-after", 60, "seconds a lost client is waited for")
	fs.IntVar(&a.silent, "silent", 8, "seconds without a word from a client (--pace realtime): it counts as lost until it speaks again (0: never)")
	fs.StringVar(&a.log, "log", "", "the game as a script")
	if err := fs.Parse(args); err != nil {
		return 2
	}
	if a.pace != "realtime" && a.pace != "fast" {
		warn("serve: --pace realtime or fast\n")
		return 2
	}
	if !a.lobby && a.seats == 0 && !a.public {
		warn("serve: --seats N, --lobby or --public\n")
		return 2
	}
	if os.Getenv("GOMAXPROCS") == "" { // the hub's lock lets one goroutine work at a time: more threads only
		runtime.GOMAXPROCS(1) //            wake and spin (realtime: 43 against 71 ms CPU a second, perfcompare)
	}
	h := &mpHub{a: a, t0: time.Now(), rooms: map[string]*gameServer{}}
	var lns []net.Listener
	for _, addr := range strings.Split(a.host, ",") {
		ln, err := net.Listen("tcp", net.JoinHostPort(strings.TrimSpace(addr), strconv.Itoa(a.port)))
		if err != nil {
			h.note(fmt.Sprintf("cannot listen: %v", err))
			return 1
		}
		lns = append(lns, ln)
	}
	if !a.public { // (before the first connection: the public server has none)
		h.one = newGameServer(h, "")
	}
	for _, ln := range lns {
		go func(ln net.Listener) {
			for {
				c, err := ln.Accept()
				if err != nil {
					return
				}
				go h.handle(c)
			}
		}(ln)
	}
	if a.public {
		h.note(fmt.Sprintf("public server on %s:%d: HTTP and WebSocket (/lobby, /game/ROOM, /rooms, /healthz)", a.host, a.port))
		select {}
	}
	srv := h.one
	if a.lobby {
		if pc, err := net.ListenPacket("udp4", fmt.Sprintf("0.0.0.0:%d", a.port)); err != nil { // (broadcasts reach only 0.0.0.0)
			srv.note(fmt.Sprintf("no LAN search (UDP %d: %v)", a.port, err))
		} else {
			go h.finder(pc, srv)
			defer pc.Close()
		}
		srv.note(fmt.Sprintf("lobby on %s:%d", a.host, a.port))
	} else {
		srv.note(fmt.Sprintf("listening on %s:%d for %d seats (pace %s)", a.host, a.port, a.seats, a.pace))
	}
	outs := srv.ticker()
	for _, ln := range lns {
		ln.Close()
	}
	for _, o := range outs { // (what is queued goes out first)
		select {
		case <-o.done:
		case <-time.After(2 * time.Second):
		}
	}
	return 0
}

// finder: the LAN search -- "MADTV?" on UDP -> the open game
func (h *mpHub) finder(pc net.PacketConn, srv *gameServer) {
	buf := make([]byte, 2048)
	for {
		n, addr, err := pc.ReadFrom(buf)
		if err != nil {
			return
		}
		if string(bytes.TrimSpace(buf[:n])) != "MADTV?" {
			continue
		}
		h.mu.Lock()
		var reply []byte
		if srv.lobby != nil {
			reply, _ = json.Marshal(srv.lobby.found())
		}
		h.mu.Unlock()
		if reply != nil {
			pc.WriteTo(reply, addr)
		}
	}
}

var errTooBig = errors.New("message too big")

// handle: a connection -- plain TCP (a LAN game's, a launcher's), or HTTP and WebSocket
func (h *mpHub) handle(c net.Conn) {
	c.SetReadDeadline(time.Now().Add(h.timeOf(0))) // (until it joined)
	lc := &lifeConn{Conn: c}
	r := bufio.NewReaderSize(lc, 65536)
	first := make([]byte, 4)
	if _, err := io.ReadFull(r, first); err != nil {
		c.Close()
		return
	}
	if tc, ok := c.(*net.TCPConn); ok {
		tc.SetNoDelay(true)
	}
	if string(first) != "GET " {
		if h.one == nil { // the public server speaks HTTP only
			c.Close()
			return
		}
		pending := first
		read := func() (string, error) {
			head := pending
			pending = nil
			if head == nil {
				head = make([]byte, 4)
				if _, err := io.ReadFull(r, head); err != nil {
					return "", err
				}
			}
			n := binary.LittleEndian.Uint32(head)
			if n > maxMsg {
				return "", errTooBig
			}
			b := make([]byte, n)
			if _, err := io.ReadFull(r, b); err != nil {
				return "", err
			}
			return strings.ToValidUTF8(string(b), "�"), nil
		}
		h.one.handle(read, newOut(c, false), lc.join)
		return
	}
	var head bytes.Buffer
	head.Write(first)
	for !bytes.HasSuffix(head.Bytes(), []byte("\r\n\r\n")) {
		b, err := r.ReadByte()
		if err != nil || head.Len() > 65536 {
			c.Close()
			return
		}
		head.WriteByte(b)
	}
	lines := strings.Split(strings.ToValidUTF8(head.String(), "�"), "\r\n")
	path := "/"
	if f := strings.Split(lines[0], " "); len(f) > 1 {
		path = f[1]
	}
	hdr := map[string]string{}
	for _, l := range lines[1:] {
		k, v, _ := strings.Cut(l, ":")
		hdr[strings.ToLower(strings.TrimSpace(k))] = strings.TrimSpace(v)
	}
	if strings.ToLower(hdr["upgrade"]) != "websocket" {
		h.http(c, path)
		return
	}
	h.mu.Lock()
	srv := h.one
	if strings.HasPrefix(path, "/game/") && h.one == nil {
		srv = h.rooms[path[len("/game/"):]]
	} else if (path != "/lobby" && path != "/game") || (h.one == nil && path == "/game") {
		srv = nil
	}
	h.mu.Unlock()
	key := hdr["sec-websocket-key"]
	sum := sha1.Sum([]byte(key + wsGUID))
	accept := base64.StdEncoding.EncodeToString(sum[:])
	if key == "" || (srv == nil && !(h.one == nil && path == "/lobby")) {
		c.Write([]byte("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"))
		c.Close()
		return
	}
	c.Write([]byte("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n" +
		"Sec-WebSocket-Accept: " + accept + "\r\n\r\n"))
	lc.idle = h.timeOf(1)
	out := newOut(c, true)
	read := func() (string, error) { // a WebSocket's messages (text); pings answered
		data, err := wsRead(r, maxMsg, func(p []byte) { out.enqueue(wsFrame(10, p, false)) })
		return strings.ToValidUTF8(string(data), "�"), err
	}
	stop := make(chan struct{})
	defer close(stop)
	go func() { // (proxies end quiet connections: a lobby may be quiet long)
		for {
			select {
			case <-stop:
				return
			case <-out.done:
				return
			case <-time.After(h.timeOf(2)):
				out.ping()
			}
		}
	}()
	if srv == nil { // the public server's lobby: the first message picks the room
		msg, err := read()
		if err != nil {
			out.close()
			return
		}
		m := obj{}
		if v, err := parseJSON([]byte(msg)); err == nil {
			if x, ok := v.(obj); ok {
				m = x
			}
		}
		h.mu.Lock()
		if m["t"] == "join" && truthy(m["create"]) {
			listed := true
			if v, ok := m["listed"]; ok && v == false {
				listed = false
			}
			srv = h.openRoom(listed)
		} else {
			srv = h.rooms[pyStr(m["room"])]
			if !truthy(m["room"]) {
				srv = nil
			}
		}
		h.mu.Unlock()
		if srv == nil {
			why := "the server is full"
			if truthy(m["room"]) {
				why = "no such game (any more)"
			}
			b, _ := json.Marshal(obj{"t": "error", "code": "room", "msg": why})
			out.write(b)
			out.close()
			return
		}
		replay := &msg
		srv.handle(func() (string, error) {
			if replay != nil {
				s := *replay
				replay = nil
				return s, nil
			}
			return read()
		}, out, lc.join)
		return
	}
	srv.handle(read, out, lc.join)
}

func (h *mpHub) openRoom(listed bool) *gameServer {
	if len(h.rooms) >= maxRooms {
		return nil
	}
	var b [3]byte
	room := ""
	for room == "" || h.rooms[room] != nil {
		rand.Read(b[:])
		room = hex.EncodeToString(b[:])
	}
	srv := newGameServer(h, room)
	srv.listed = listed
	h.rooms[room] = srv
	go func() {
		srv.ticker()
		h.note("room " + room + " closed")
	}()
	h.note("room " + room + " opened")
	return srv
}

func (h *mpHub) openGames() []any {
	out := []any{}
	ids := make([]string, 0, len(h.rooms))
	for id := range h.rooms {
		ids = append(ids, id)
	}
	sort.Strings(ids)
	for _, id := range ids {
		srv := h.rooms[id]
		if srv.listed && srv.lobby != nil && srv.lobby.phase == "lobby" {
			out = append(out, srv.lobby.found())
		}
	}
	return out
}

func (h *mpHub) http(c net.Conn, path string) {
	code, body, ctype := "404 Not Found", []byte{}, "text/plain"
	h.mu.Lock()
	switch {
	case path == "/healthz":
		code, body = "200 OK", []byte("ok\n")
	case path == "/rooms" && h.one == nil:
		code, ctype = "200 OK", "application/json"
		body, _ = json.Marshal(h.openGames())
	case path == "/" || path == "/status": // (/status: the public server's, where "/" is the release page)
		code, body = "200 OK", []byte(fmt.Sprintf("OpenMTV network game server (openmtv serve): %d open games\n", len(h.openGames())))
	}
	h.mu.Unlock()
	fmt.Fprintf(c, "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n", code, ctype, len(body))
	c.Write(body)
	c.Close()
}

// ---------------------------------------------------------------- the frame server

type mpClient struct {
	out        *outConn
	seat       int // -1: none yet
	at         int
	ready, bye bool
	gone       bool
	lostAt     float64 // lost without a BYE: since when (-1: not)
	heard      float64 // its last message: when
	silent     bool    // lost by its silence (its connection may still be there: it can come back)
	dropNow    bool    // a player does not want to wait for it any longer
	token      string  // its secret: a new connection takes the seat over with it (RESUME)
	rtts       []float64
}

func (c *mpClient) send(text string) {
	if !c.gone && !c.bye {
		c.out.write([]byte(text))
	}
}

type input struct {
	seat      int
	cmd, args string
	live      bool // "IN 0": its client keeps it until a bundle brings it back, and after a RESUME sends it again
}

type sentLine struct {
	f    int
	line string
}

type gameServer struct {
	h        *mpHub
	a        *mpArgs
	room     string // the public server's room, else ""
	seats    int    // the game's clients (a lobby sets it at its start)
	lobby    *mpLobby
	goAt     float64 // the lobby started the game: when (-1: not yet)
	born     float64 // when it opened, or a player last spoke in its lobby (lobbyS counts from it)
	clients  map[int]*mpClient
	params   *string
	started  bool
	outs     map[*outConn]bool // every connection it serves
	pending  map[int][]input
	held     map[int]int // the inputs in pending, per seat
	sent     int
	recent   []sentLine // the inputs of the last frames sent (from recentAt on): a client back after a break
	recentAt int        //  gets what it missed
	keptTo   int        // recent[recentAt:] holds the frames after this one
	hashes   map[int]map[int]string
	agreed   map[int]string
	stopped  bool
	ended    bool // the ticker is through: its connections are closed
	listed   bool
	log      *os.File
	keys     map[int]string // a seat's secret (the lobby's start, to that player alone): its HELLO must carry it
}

func newGameServer(h *mpHub, room string) *gameServer {
	s := &gameServer{h: h, a: h.a, room: room, seats: h.a.seats, goAt: -1, born: h.mono(), clients: map[int]*mpClient{}, outs: map[*outConn]bool{},
		pending: map[int][]input{}, held: map[int]int{}, hashes: map[int]map[int]string{}, agreed: map[int]string{}, listed: true}
	if h.a.lobby || room != "" {
		s.lobby = newMpLobby(s, h.a.key, room != "")
	}
	if h.a.log != "" && room == "" {
		s.log, _ = os.Create(h.a.log)
	}
	return s
}

func (s *gameServer) note(m string) {
	if s.room != "" {
		fmt.Fprintf(os.Stderr, "[mpserver] %s %s\n", s.room, m)
	} else {
		fmt.Fprintf(os.Stderr, "[mpserver] %s\n", m)
	}
}

func (s *gameServer) broadcast(text string) {
	for _, c := range s.clients {
		c.send(text)
	}
}

// computing: the clients the game waits for
func (s *gameServer) computing() []*mpClient {
	var out []*mpClient
	for _, c := range s.clients {
		if !c.bye && !c.gone {
			out = append(out, c)
		}
	}
	return out
}

// handle: one connection -- a launcher's (JSON, the lobby) or a game's; JOINED: it joined the
// lobby or took a seat (the handshake is over)
func (s *gameServer) handle(read func() (string, error), out *outConn, joined func()) {
	c := &mpClient{out: out, seat: -1, lostAt: -1}
	var lc *lobbyConn
	s.h.mu.Lock()
	if s.ended { // (it found the room as the game ended)
		s.h.mu.Unlock()
		out.close()
		return
	}
	s.outs[out] = true // (closed with the game)
	s.h.mu.Unlock()
	for {
		msg, err := read()
		if err != nil {
			if err == errTooBig {
				s.h.mu.Lock()
				s.note(fmt.Sprintf("seat %s: %v", seatName(c.seat), err))
				s.h.mu.Unlock()
			}
			break
		}
		s.h.mu.Lock()
		if s.ended { // (it found the room as the game ended)
			s.h.mu.Unlock()
			out.close()
			return
		}
		if lc != nil || (strings.HasPrefix(strings.TrimLeft(msg, " \t\r\n"), "{") && s.lobby != nil && c.seat < 0) {
			if lc == nil {
				lc = &lobbyConn{out: out}
			}
			v, perr := parseJSON([]byte(msg))
			m, ok := v.(obj)
			if perr != nil || !ok {
				err = fmt.Errorf("not a message: %v", perr)
			} else {
				err = s.lobby.message(lc, m)
				s.born = s.h.mono() // (a lobby in use stays: only a forgotten one closes)
			}
		} else {
			err = s.message(c, msg)
		}
		if err != nil { // a broken message: drop that client
			s.note(fmt.Sprintf("seat %s: %v", seatName(c.seat), err))
		}
		in := (lc != nil && lc.pid != 0) || c.seat >= 0
		s.h.mu.Unlock()
		if err != nil {
			break
		}
		if in {
			joined()
		}
	}
	s.h.mu.Lock()
	defer s.h.mu.Unlock()
	delete(s.outs, out)
	if lc != nil {
		s.lobby.lost(lc)
		out.close()
		return
	}
	s.lost(c)
	out.close()
}

func seatName(seat int) string {
	if seat < 0 {
		return "None"
	}
	return strconv.Itoa(seat)
}

func paramsMap(p string) map[string]string {
	m := map[string]string{}
	for _, w := range strings.Fields(p) {
		if k, v, ok := strings.Cut(w, "="); ok {
			m[k] = v
		}
	}
	return m
}

func (s *gameServer) message(c *mpClient, msg string) error {
	kind, rest, _ := strings.Cut(msg, " ")
	c.heard = s.h.mono()
	if c.silent && !c.bye && !c.gone { // (silent for a while, now it speaks again: the game goes on)
		c.silent, c.lostAt = false, -1
		s.note(fmt.Sprintf("seat %d is back", c.seat))
	}
	switch {
	case kind == "HELLO":
		f := append(strings.SplitN(rest, " ", 3), "", "")
		seat, err := strconv.Atoi(strings.TrimSpace(f[0]))
		if err != nil {
			return err
		}
		params := f[2]
		_, key, _ := strings.Cut(f[1], ":") // "1:KEY": the seat's secret from the lobby (an older game: "1")
		if s.lobby != nil && s.seats == 0 {
			c.send("REFUSE the lobby has not started the game yet")
			return nil
		}
		if _, taken := s.clients[seat]; s.started || seat < 0 || seat >= s.seats || taken {
			c.send(fmt.Sprintf("REFUSE seat %d is not free", seat))
			return nil
		}
		if want := s.keys[seat]; want != "" && subtle.ConstantTimeCompare([]byte(key), []byte(want)) != 1 {
			c.send(fmt.Sprintf("REFUSE seat %d is another player's", seat))
			s.note(fmt.Sprintf("seat %d refused: not its secret", seat))
			return nil
		}
		if s.params != nil && params != *s.params {
			mine, theirs := paramsMap(params), paramsMap(*s.params)
			keys := map[string]bool{}
			for k := range mine {
				keys[k] = true
			}
			for k := range theirs {
				keys[k] = true
			}
			sorted := make([]string, 0, len(keys))
			for k := range keys {
				sorted = append(sorted, k)
			}
			sort.Strings(sorted)
			var why []string
			get := func(m map[string]string, k string) string {
				if v, ok := m[k]; ok {
					return v
				}
				return "-"
			}
			for _, k := range sorted {
				if get(mine, k) != get(theirs, k) {
					why = append(why, fmt.Sprintf("%s %s (the others: %s)", k, get(mine, k), get(theirs, k)))
				}
			}
			w := strings.Join(why, ", ")
			if strings.Contains(w, "files") {
				w += " -- other game files: another mod, or not the original files"
			}
			if w == "" {
				w = params
			}
			c.send("REFUSE another game: " + w)
			s.note(fmt.Sprintf("seat %d refused: %s", seat, strings.Join(why, ", ")))
			return nil
		}
		s.params = &params
		c.seat = seat
		var k [12]byte
		rand.Read(k[:])
		c.token = hex.EncodeToString(k[:])
		s.clients[seat] = c
		c.send(fmt.Sprintf("WELCOME seat %d of %d token %s", seat, s.seats, c.token))
		s.note(fmt.Sprintf("seat %d joined (%s)", seat, params))
	case kind == "RESUME" && c.seat < 0: // a client back after a break: "RESUME seat token complete"
		s.resume(c, rest)
	case c.seat < 0 || c.bye || s.clients[c.seat] != c: // (no longer waited for, or its seat taken over
		return nil //                                             by a RESUME: what it still says does not count)
	case kind == "GO":
		c.ready = true
		all := len(s.clients) == s.seats
		for _, cl := range s.clients {
			all = all && cl.ready
		}
		if all {
			s.started = true
			for _, cl := range s.clients { // (the silence counts from the start)
				cl.heard = s.h.mono()
			}
			if s.log != nil {
				fmt.Fprintf(s.log, "# network game (%s)\n# play it again: %s\n", *s.params, replayCommand(*s.params))
			}
			s.broadcast("START")
			s.note("the game starts")
			if s.lobby != nil {
				s.lobby.playing()
			}
			if s.a.pace == "fast" {
				s.advance(nil)
			}
		}
	case kind == "IN":
		fs, line, _ := strings.Cut(rest, " ")
		cmd, args, _ := strings.Cut(line, " ")
		f, err := strconv.Atoi(strings.TrimSpace(fs))
		if err != nil {
			return err
		}
		if len(line) > maxInput || s.held[c.seat] >= maxInputs || (s.started && f > s.sent+inputAhead*s.a.window) {
			return nil // (no client sends that: not kept)
		}
		live := f == 0
		if f <= s.sent { // live input, or a script's line late: the next frame
			f = s.sent + 1
		}
		s.pending[f] = append(s.pending[f], input{c.seat, cmd, args, live})
		s.held[c.seat]++
	case kind == "PONG": // its answer to a PING: the round trip
		if t, err := strconv.ParseFloat(strings.TrimSpace(rest), 64); err == nil {
			c.rtts = append(c.rtts, max(0, s.h.mono()-t))
			if len(c.rtts) > 8 {
				c.rtts = c.rtts[len(c.rtts)-8:]
			}
		}
	case kind == "AT":
		n, err := strconv.Atoi(strings.TrimSpace(rest))
		if err != nil {
			return err
		}
		// (a client asks for the frame after those it has: never further -- the bundles follow it)
		c.at = max(c.at, min(n, s.sent+s.a.window))
		if s.a.pace == "fast" {
			s.advance(nil)
		}
	case kind == "HASH":
		f := strings.Fields(rest)
		if len(f) != 2 {
			return fmt.Errorf("HASH: %q", rest)
		}
		n, err := strconv.Atoi(f[0])
		if err != nil {
			return err
		}
		if n < s.keptTo || n > s.sent+s.a.window || !hashText(f[1]) { // (no client sends that: a frame not
			return nil //                                                    played yet, or too old to compare)
		}
		s.hash(c.seat, n, f[1])
	case kind == "DROP": // a player does not want to wait for that seat any longer: no longer waited for
		if seat, err := strconv.Atoi(strings.TrimSpace(rest)); err == nil {
			if o := s.clients[seat]; o != nil && o != c && o.lostAt >= 0 {
				s.note(fmt.Sprintf("seat %d does not wait for seat %d any longer", c.seat, seat))
				o.dropNow = true
				s.paused()
			}
		}
	case kind == "OUT":
		s.note(fmt.Sprintf("seat %d is out of the game", c.seat))
	case kind == "BYE":
		c.bye = true
		s.note(fmt.Sprintf("seat %d left the game", c.seat))
		for _, o := range s.clients { // (the others show it: runtime/net.c)
			if o != c {
				o.send(fmt.Sprintf("LEFT %d bye", c.seat))
			}
		}
		s.checkHashes()
		c.out.close() // (the client waits for that before it closes)
	}
	return nil
}

// replayCommand: the command line that plays the log again, from the hello's parameters
func replayCommand(params string) string {
	p := paramsMap(params)
	get := func(k, d string) string {
		if v, ok := p[k]; ok {
			return v
		}
		return d
	}
	cmd := []string{"build/mtv-dec", "--script", "LOG", "--humans", get("humans", "?")}
	if get("args", "-") != "-" {
		cmd = append(cmd, "--args", p["args"])
	}
	if get("fixes", "classic") == "classic" {
		cmd = append(cmd, "--classic")
	} else {
		cmd = append(cmd, "--fixes="+p["fixes"])
	}
	if p["time"] == "logical" {
		cmd = append(cmd, "--logical-time")
	}
	if s := get("speed4", "0"); s != "0" && s != "-1" {
		cmd = append(cmd, "--fourth-speed=yes")
	}
	if a := get("aiimp", "0"); a == "1" || a == "2" {
		cmd = append(cmd, "--ai-improve="+map[string]string{"1": "normal", "2": "strong"}[a])
	}
	for _, k := range [][2]string{{"money", "--start-money"}, {"credit", "--credit-line"}, {"terror", "--terror-damage"}} {
		if v := get(k[0], "-1"); v != "-1" {
			cmd = append(cmd, k[1], v)
		}
	}
	if v := get("sammy", "0,0,0,0"); v != "0,0,0,0" {
		cmd = append(cmd, "--sammy-prize", v)
	}
	if v := get("offers", "10"); v != "10" {
		cmd = append(cmd, "--ad-offers", v)
	}
	if v := get("filmoffers", "10"); v != "10" {
		cmd = append(cmd, "--film-offers", v)
	}
	if get("sertop", "0") == "1" {
		cmd = append(cmd, "--series-topicality")
	}
	if v := get("knock", "0"); v != "0" {
		cmd = append(cmd, "--knock", v)
	}
	return strings.Join(cmd, " ")
}

// hashText: a hash as runtime/host.c sends it -- 16 hex digits (tools: fewer)
func hashText(h string) bool {
	if len(h) == 0 || len(h) > 16 {
		return false
	}
	for _, r := range h {
		if !strings.ContainsRune("0123456789abcdefABCDEF", r) {
			return false
		}
	}
	return true
}

func (s *gameServer) hash(seat, f int, h string) {
	if want, ok := s.agreed[f]; ok { // a client late with it
		if h != want {
			s.desync(f, fmt.Sprintf("{%d: '%s', 'the others': '%s'}", seat, h, want))
		}
		return
	}
	if s.hashes[f] == nil {
		s.hashes[f] = map[int]string{}
	}
	s.hashes[f][seat] = h
	s.checkFrame(f, s.wantHashes()) // (only that frame's are new)
}

// wantHashes: the seats whose hashes a frame needs
func (s *gameServer) wantHashes() []int {
	var want []int
	for _, c := range s.computing() {
		want = append(want, c.seat)
	}
	return want
}

// checkHashes: every frame's hashes again (the seats computing changed)
func (s *gameServer) checkHashes() {
	want := s.wantHashes()
	frames := make([]int, 0, len(s.hashes))
	for f := range s.hashes {
		frames = append(frames, f)
	}
	sort.Ints(frames)
	for _, f := range frames {
		if s.checkFrame(f, want) {
			return
		}
	}
}

// checkFrame: frame F's hashes compared once WANT's are there (true: they differ, DESYNC)
func (s *gameServer) checkFrame(f int, want []int) bool {
	got := s.hashes[f]
	for _, w := range want {
		if _, ok := got[w]; !ok {
			return false
		}
	}
	var first string
	differ := false
	seats := make([]int, 0, len(got))
	for st := range got {
		seats = append(seats, st)
	}
	sort.Ints(seats)
	for i, st := range seats {
		if i == 0 {
			first = got[st]
		} else if got[st] != first {
			differ = true
		}
	}
	if differ {
		var parts []string
		for _, st := range seats {
			parts = append(parts, fmt.Sprintf("%d: '%s'", st, got[st]))
		}
		s.desync(f, "{"+strings.Join(parts, ", ")+"}")
		return true
	}
	s.agreed[f] = first
	delete(s.hashes, f)
	return false
}

func (s *gameServer) desync(f int, got string) {
	s.note(fmt.Sprintf("DESYNC at frame %d: %s", f, got))
	s.broadcast(fmt.Sprintf("DESYNC %d", f))
	s.stopped = true
}

func (s *gameServer) lost(c *mpClient) {
	c.gone = true
	if s.room != "" && s.lobby != nil && s.lobby.phase == "lobby" && len(s.lobby.players) == 0 {
		s.stopped = true // (a public room nobody joined: it ends with the connection that made it)
	}
	if c.seat < 0 || s.clients[c.seat] != c {
		return
	}
	allGone := true
	for _, cl := range s.clients { // (a started game waits for those lost without a BYE: they may come back)
		allGone = allGone && cl.gone && (cl.bye || !s.started)
	}
	if allGone {
		s.note("every client is gone")
		s.stopped = true
		return
	}
	if !c.bye && s.started && c.lostAt < 0 { // (silent before: the wait is on already)
		s.waitFor(c)
	}
	s.checkHashes()
}

// waitFor: the game waits for C (lost) -- --drop-after seconds at most; everyone is told
// (runtime/net.c reads the text: "WAIT seat K lost its connection (S s)")
func (s *gameServer) waitFor(c *mpClient) {
	c.lostAt = s.h.mono()
	m := fmt.Sprintf("WAIT seat %d lost its connection (%d s)", c.seat, s.a.dropAft)
	s.note(m)
	s.broadcast(m)
}

func (s *gameServer) paused() bool {
	seats := make([]int, 0, len(s.clients))
	for st := range s.clients {
		seats = append(seats, st)
	}
	sort.Ints(seats)
	waiting := false
	now := s.h.mono()
	for _, st := range seats {
		c := s.clients[st]
		if s.started && s.a.pace == "realtime" && s.a.silent > 0 && !c.bye && !c.gone && c.lostAt < 0 &&
			now-c.heard >= float64(s.a.silent) { // (a connection that broke quietly: nothing tells)
			c.silent = true
			s.note(fmt.Sprintf("seat %d silent for %d s", c.seat, s.a.silent))
			s.waitFor(c)
		}
		if c.lostAt >= 0 && (c.dropNow || now-c.lostAt >= float64(s.a.dropAft)) {
			s.note(fmt.Sprintf("seat %d: no longer waited for", c.seat))
			if !c.gone { // (silent, its connection still there: it is told, should it come back -- and
				c.send("END you were away too long") //  closes itself: a close here would reset
				out := c.out                         //  the connection under the END once it speaks)
				time.AfterFunc(10*time.Minute, out.close)
			}
			c.bye, c.lostAt = true, -1
			s.broadcast(fmt.Sprintf("LEFT %d gone", c.seat))
			s.checkHashes()
		}
	}
	over := s.started && len(s.clients) > 0
	for _, c := range s.clients {
		waiting = waiting || c.lostAt >= 0
		over = over && c.bye
	}
	if over && !s.stopped {
		s.note("every client is gone")
		s.stopped = true
	}
	return waiting
}

// advance: bundle the frames the slowest client (and the clock) allow
func (s *gameServer) advance(clock *float64) {
	if !s.started || s.stopped || s.paused() {
		return
	}
	live := s.computing()
	if len(live) == 0 {
		return
	}
	low := live[0].at
	for _, c := range live {
		low = min(low, c.at)
	}
	target := low + s.a.window
	if clock != nil {
		target = min(target, int(*clock))
	}
	if target <= s.sent {
		return
	}
	var b strings.Builder
	fmt.Fprintf(&b, "B %d", target)
	for f := s.sent + 1; f <= target; f++ {
		for _, in := range s.pending[f] {
			s.held[in.seat]--
			line := fmt.Sprintf("%d %d %s %s", f, in.seat, in.cmd, in.args)
			b.WriteString("\n" + line)
			s.recent = append(s.recent, sentLine{f, line})
			if s.log != nil {
				if in.seat == 0 {
					fmt.Fprintf(s.log, "%d %s %s\n", f, in.cmd, in.args)
				} else {
					fmt.Fprintf(s.log, "%d seat %d %s %s\n", f, in.seat, in.cmd, in.args)
				}
			}
		}
		delete(s.pending, f)
	}
	s.sent = target
	s.broadcast(b.String())
	if keep := (s.a.dropAft + 60) * 70; s.sent-keep > s.keptTo { // (a break longer than the wait is not resumed)
		s.keptTo = s.sent - keep
		for s.recentAt < len(s.recent) && s.recent[s.recentAt].f <= s.keptTo {
			s.recentAt++
		}
		if s.recentAt > len(s.recent)/2 { // (copied now and then, not every frame)
			s.recent = append([]sentLine(nil), s.recent[s.recentAt:]...)
			s.recentAt = 0
		}
		for f := range s.agreed { // (the hashes of those frames are no longer compared)
			if f < s.keptTo {
				delete(s.agreed, f)
			}
		}
		for f := range s.hashes {
			if f < s.keptTo {
				delete(s.hashes, f)
			}
		}
	}
}

// resume: a client's new connection takes its seat over -- the old one (broken, or not yet
// known to be) goes; it gets every input after the frame it has complete, then plays on
func (s *gameServer) resume(c *mpClient, rest string) {
	f := strings.Fields(rest)
	seat, err1 := strconv.Atoi(append(f, "")[0])
	complete, err2 := strconv.Atoi(append(f, "", "", "")[2])
	old := s.clients[seat]
	switch {
	case len(f) < 3 || err1 != nil || err2 != nil:
		c.send("REFUSE resume: " + rest)
		return
	case !s.started || old == nil || old.token == "" || old.token != f[1]:
		c.send(fmt.Sprintf("REFUSE seat %d is not yours", seat))
		s.note(fmt.Sprintf("seat %d: a resume refused (not its secret)", seat))
		return
	case old.bye:
		c.send("END you were away too long")
		return
	case complete < s.keptTo || complete > s.sent:
		c.send("REFUSE too far behind")
		return
	}
	c.seat, c.token, c.ready, c.at, c.rtts = seat, old.token, true, max(old.at, complete), old.rtts
	c.heard = s.h.mono()
	old.gone = true // (its own end is no longer the seat's: lost() sees it replaced)
	old.out.close()
	s.clients[seat] = c
	for f, ins := range s.pending { // its live inputs not bundled yet go: the client sends again every one
		keep := ins[:0] //                 that has not come back (runtime/net.c), else they came twice
		for _, in := range ins {
			if !in.live || in.seat != seat {
				keep = append(keep, in)
			} else {
				s.held[seat]--
			}
		}
		if len(keep) == 0 {
			delete(s.pending, f)
		} else {
			s.pending[f] = keep
		}
	}
	s.note(fmt.Sprintf("seat %d is back on a new connection (had frame %d, sent %d)", seat, complete, s.sent))
	var b strings.Builder
	fmt.Fprintf(&b, "RESUMED %d", s.sent)
	c.send(b.String())
	b.Reset()
	fmt.Fprintf(&b, "B %d", s.sent)
	for _, l := range s.recent[s.recentAt:] {
		if l.f > complete {
			b.WriteString("\n" + l.line)
		}
	}
	c.send(b.String())
	s.checkHashes()
}

// lead: the lead over the fastest client -- its longest recent round trip and --margin
// frames (--lead at most; --lead itself while no client answers PINGs)
func (s *gameServer) lead() int {
	worst, any := 0.0, false
	for _, c := range s.computing() {
		for _, r := range c.rtts {
			worst, any = max(worst, r), true
		}
	}
	if !any {
		return s.a.lead
	}
	return max(s.a.margin, min(s.a.lead, int(worst/frameS+0.999)+s.a.margin))
}

// ticker: the game's clock (the server's loop); its end -> the connections to wait for
func (s *gameServer) ticker() []*outConn {
	clock, lastT, lastPing, leadSent := 0.0, s.h.mono(), 0.0, -1 // (-1: no LEAD said yet)
	for {
		s.h.mu.Lock()
		stopped := s.stopped
		s.h.mu.Unlock()
		if stopped {
			break
		}
		if s.a.pace == "realtime" {
			time.Sleep(time.Second / 70)
		} else {
			time.Sleep(50 * time.Millisecond)
		}
		s.h.mu.Lock()
		now := s.h.mono()
		dt := now - lastT
		lastT = now
		if dt > 2 { // (the server itself stood -- a stalled machine: the clients' silence is its own)
			for _, cl := range s.clients {
				cl.heard = now
			}
		}
		if s.started && s.a.pace == "realtime" && now-lastPing > 0.5 {
			lastPing = now // (the clients answer PONG at once)
			s.broadcast(fmt.Sprintf("PING %.6f", now))
			if l := s.lead(); l != leadSent { // (the clients keep that far behind the front: runtime/host.c)
				leadSent = l
				s.broadcast(fmt.Sprintf("LEAD %d", l))
			}
		}
		if s.room != "" && s.lobby.phase == "lobby" && now-s.born > s.h.timeOf(3).Seconds() { // (the rooms are few:
			why := fmt.Sprintf("the game did not start within %.0f minutes", s.h.timeOf(3).Minutes()) // none held forever)
			s.note(why)
			s.lobby.phase, s.lobby.why = "closed", why
			s.lobby.publish()
			s.stopped = true
			s.h.mu.Unlock()
			break
		}
		if s.goAt >= 0 && !s.started && now-s.goAt > float64(s.a.startWait) {
			s.note(fmt.Sprintf("not every player's game came within %d s", s.a.startWait))
			s.lobby.ended("not every player's game started")
			s.broadcast("END not every player's game started")
			s.stopped = true
			s.h.mu.Unlock()
			break
		}
		if s.started && !s.paused() {
			if s.a.pace == "realtime" {
				if live := s.computing(); len(live) > 0 { // the clock runs, no further than the lead ahead of the fastest
					lo, hi := live[0].at, live[0].at //  client, --window ahead of the slowest
					for _, c := range live {
						lo, hi = min(lo, c.at), max(hi, c.at)
					}
					lead := leadSent
					if lead <= 0 {
						lead = s.a.lead
					}
					clock = min(clock+dt/frameS*1.05, float64(hi+lead), float64(lo+s.a.window))
				}
				c := clock
				s.advance(&c)
			} else {
				s.advance(nil)
			}
		}
		s.h.mu.Unlock()
	}
	s.h.mu.Lock()
	defer s.h.mu.Unlock()
	s.ended = true
	if s.room != "" { // (no new connection finds it from now on)
		delete(s.h.rooms, s.room)
	}
	s.broadcast("END the game is over")
	var outs []*outConn
	if s.lobby != nil {
		if s.lobby.phase == "starting" || s.lobby.phase == "playing" {
			s.lobby.ended("the game is over")
		}
		for _, c := range s.lobby.conns { // (the launchers' connections end: the public server's room, or
			c.out.close() //  the LAN's server, whose process ends now)
			outs = append(outs, c.out)
		}
	}
	for _, c := range s.clients {
		c.out.close()
		outs = append(outs, c.out)
	}
	for o := range s.outs { // (and the others: a refused one, one that never said a word)
		o.close()
		outs = append(outs, o)
	}
	if s.log != nil {
		s.log.Close()
	}
	return outs
}

// ---------------------------------------------------------------- the lobby

type lobbyConn struct {
	out  *outConn
	pid  int // 0: not joined
	gone bool
}

func (c *lobbyConn) send(doc obj) {
	if !c.gone {
		b, _ := json.Marshal(doc)
		c.out.write(b)
	}
}

type lobbyPlayer struct {
	id                       int
	name                     string
	station                  any // (a number; none when every station is taken)
	ready, host              bool
	version, files, platform string
	mods, save               any
	seatKey                  bool // its launcher takes a seat's secret (join "seatkey"; an older one does not)
}

func (p *lobbyPlayer) doc() obj {
	return obj{"id": p.id, "name": p.name, "station": p.station, "ready": p.ready, "host": p.host, "version": p.version,
		"files": p.files, "platform": p.platform, "mods": p.mods, "save": p.save}
}

type mpLobby struct {
	srv      *gameServer
	key      string
	public   bool // a room of the public server: whoever opens it is its host
	conns    []*lobbyConn
	players  map[int]*lobbyPlayer
	nextID   int
	host     int // 0: none
	settings obj
	mod      obj // hash, data
	save     obj // hash, data, meta
	phase    string
	why      string
}

func defaultSettings() obj {
	return obj{"lang": "e", "fixes": "all", "difficulty": int64(2), "speed4": false, "aiimp": "off", "money": int64(200000),
		"credit": int64(700000), "terror": int64(50000), "sammy": []any{int64(0), int64(0), int64(0), int64(0)}, "offers": int64(10), "filmoffers": int64(10), "sertop": true, "knock": knockDef, "castfit": false, "packs": []any{}, "mods": nil}
}

func newMpLobby(srv *gameServer, key string, public bool) *mpLobby {
	return &mpLobby{srv: srv, key: key, public: public, players: map[int]*lobbyPlayer{}, nextID: 1, settings: defaultSettings(), phase: "lobby"}
}

func (l *mpLobby) note(m string) { l.srv.note("lobby: " + m) }

func (l *mpLobby) sortedPlayers() []*lobbyPlayer {
	ps := make([]*lobbyPlayer, 0, len(l.players))
	for _, p := range l.players {
		ps = append(ps, p)
	}
	sort.SliceStable(ps, func(i, j int) bool {
		a, aok := ps[i].station.(int64)
		b, bok := ps[j].station.(int64)
		if aok != bok {
			return !aok
		}
		if a != b {
			return a < b
		}
		return ps[i].id < ps[j].id
	})
	return ps
}

// problems: why the host cannot start yet (the launcher says it in its language)
func (l *mpLobby) problems() []any {
	out := []any{}
	add := func(code, name string) { out = append(out, obj{"code": code, "name": name}) }
	ps := make([]*lobbyPlayer, 0, len(l.players))
	ids := make([]int, 0, len(l.players))
	for id := range l.players {
		ids = append(ids, id)
	}
	sort.Ints(ids) // (Python: the players in the order they joined)
	for _, id := range ids {
		ps = append(ps, l.players[id])
	}
	hp := l.players[l.host]
	if len(ps) < 2 {
		add("alone", "")
	}
	count := func(st any) int {
		n := 0
		for _, q := range ps {
			if equal(q.station, st) {
				n++
			}
		}
		return n
	}
	for _, p := range ps {
		if count(p.station) > 1 {
			add("station", p.name)
		}
		if hp != nil && p.version != hp.version {
			add("version", p.name)
		}
		if hp != nil && p.files != hp.files {
			add("files", p.name)
		}
		if truthy(l.settings["mods"]) && p != hp && !equal(p.mods, l.settings["mods"]) {
			add("mods", p.name)
		}
		if l.save != nil && p != hp && !equal(p.save, l.save["hash"]) {
			add("savehave", p.name)
		}
		if p != hp && !p.ready {
			add("ready", p.name)
		}
	}
	if l.save != nil { // a saved game: its humans' stations, each taken, and no other
		humans, _ := l.save["meta"].(obj)["humans"].([]any)
		want := []any{}
		for _, h := range humans {
			want = append(want, h.(obj)["station"])
		}
		for _, h := range humans {
			ho := h.(obj)
			taken := false
			for _, p := range ps {
				taken = taken || equal(p.station, ho["station"])
			}
			if !taken {
				name := pyStr(ho["name"])
				if !truthy(ho["name"]) {
					name = pyStr(ho["station"])
				}
				add("savefree", name)
			}
		}
		for _, p := range ps {
			in := false
			for _, w := range want {
				in = in || equal(w, p.station)
			}
			if !in {
				add("savestation", p.name)
			}
		}
	}
	return out
}

func (l *mpLobby) state() obj {
	players := []any{}
	for _, p := range l.sortedPlayers() {
		players = append(players, p.doc())
	}
	var host, room, mod, save any
	if l.host != 0 {
		host = l.host
	}
	if l.srv.room != "" {
		room = l.srv.room
	}
	if l.mod != nil {
		mod = obj{"hash": l.mod["hash"], "size": len(l.mod["data"].(string))}
	}
	if l.save != nil {
		save = obj{"hash": l.save["hash"], "size": len(l.save["data"].(string)), "meta": l.save["meta"]}
	}
	return obj{"t": "state", "phase": l.phase, "why": l.why, "host": host, "settings": l.settings, "room": room,
		"players": players, "problems": l.problems(), "mod": mod, "save": save}
}

func (l *mpLobby) publish() {
	st := l.state()
	for _, c := range l.conns {
		st["you"] = c.pid
		c.send(st)
	}
}

func (l *mpLobby) error(c *lobbyConn, code, m string) {
	c.send(obj{"t": "error", "code": code, "msg": m})
}

func (l *mpLobby) freeStation(prefer int64) any {
	taken := map[int64]bool{}
	for _, p := range l.players {
		if st, ok := p.station.(int64); ok {
			taken[st] = true
		}
	}
	for _, s := range []int64{prefer, 1, 2, 3} {
		if !taken[s] {
			return s
		}
	}
	return nil
}

// cleanName: a manager's name as the game takes it: 8 printable ASCII characters
func cleanName(v any, fallback string) string {
	s := ""
	if truthy(v) {
		s = pyStr(v)
	}
	var b strings.Builder
	for _, r := range s {
		if r >= 32 && r < 127 && r != ',' {
			b.WriteRune(r)
		}
	}
	n := strings.TrimSpace(b.String())
	if len(n) > 8 {
		n = n[:8]
	}
	if n == "" {
		return fallback
	}
	return n
}

// shortStr: a player's report of a hash (mods, save): a short string or none
func shortStr(v any, n int) any {
	if s, ok := v.(string); ok && utf8.RuneCountInString(s) <= n {
		return s
	}
	return nil
}

// pyStr: str() of a JSON value, as Python writes it
func pyStr(v any) string {
	switch x := v.(type) {
	case nil:
		return "None"
	case bool:
		if x {
			return "True"
		}
		return "False"
	case int64:
		return strconv.FormatInt(x, 10)
	case float64:
		s := strconv.FormatFloat(x, 'g', -1, 64)
		if !strings.ContainsAny(s, ".eEnN") {
			s += ".0"
		}
		return s
	case string:
		return x
	}
	return pyDumps(v)
}

// pyEq: Python's == of two JSON values: numbers by their value (2.0 == 2, True == 1)
func pyEq(a, b any) bool {
	num := func(v any) (float64, bool) {
		switch x := v.(type) {
		case int64:
			return float64(x), true
		case float64:
			return x, true
		case bool:
			if x {
				return 1, true
			}
			return 0, true
		}
		return 0, false
	}
	if x, ok := num(a); ok {
		if y, ok := num(b); ok {
			return x == y
		}
	}
	return equal(a, b)
}

// pyInt: int() of a JSON value (a station), or an error as Python's
func pyInt(v any) (int64, error) {
	switch x := v.(type) {
	case int64:
		return x, nil
	case bool:
		if x {
			return 1, nil
		}
		return 0, nil
	case float64:
		return int64(x), nil
	case string:
		if n, err := strconv.ParseInt(strings.TrimSpace(x), 10, 64); err == nil {
			return n, nil
		}
		return 0, fmt.Errorf("invalid literal for int() with base 10: %s", pyRepr(x))
	}
	return 0, fmt.Errorf("int() argument must be a string, a bytes-like object or a real number, not '%s'", pyTypeName(v))
}

// saveMetaOK: a savegame's description as the lobby checks and shows it -- small, its
// humans a list of up to three on different stations 1..3 -- cleaned, or nil
func saveMetaOK(v any) obj {
	meta, ok := v.(obj)
	if !ok || len(pyDumpsASCII(meta)) > saveMetaMx {
		return nil
	}
	humans, ok := meta["humans"].([]any)
	if !ok || len(humans) < 1 || len(humans) > maxPlayers {
		return nil
	}
	var clean []any
	seen := map[int64]bool{}
	for _, x := range humans {
		h, _ := x.(obj)
		st, ok := h["station"].(int64)
		if !ok || st < 1 || st > 3 || seen[st] {
			return nil
		}
		seen[st] = true
		clean = append(clean, obj{"station": st, "name": cleanName(h["name"], fmt.Sprintf("Station %d", st)), "out": truthy(h["out"])})
	}
	return with(meta, "humans", clean)
}

// pyDumpsASCII: json.dumps(v) as Python writes it by default (ASCII, ", " and ": ")
func pyDumpsASCII(v any) string {
	s := pyDumpsSpaced(v)
	var b strings.Builder
	for _, r := range s {
		switch {
		case r < 0x80:
			b.WriteRune(r)
		case r < 0x10000:
			fmt.Fprintf(&b, `\u%04x`, r)
		default:
			r -= 0x10000
			fmt.Fprintf(&b, `\u%04x\u%04x`, 0xD800+(r>>10), 0xDC00+(r&0x3FF))
		}
	}
	return b.String()
}

func (l *mpLobby) othersNotReady() {
	for _, p := range l.players {
		if p.id != l.host {
			p.ready = false
		}
	}
}

func (l *mpLobby) message(c *lobbyConn, m obj) error {
	t := m["t"]
	if t == "join" {
		if l.phase != "lobby" {
			l.error(c, "started", "the game has started")
			return nil
		}
		if c.pid != 0 {
			return nil
		}
		if len(l.players) >= maxPlayers {
			l.error(c, "full", fmt.Sprintf("the game is full (%d players)", maxPlayers))
			return nil
		}
		pid := l.nextID
		l.nextID++
		key, _ := m["key"].(string)
		isHost := l.host == 0 && ((l.key != "" && equal(m["key"], l.key) && key != "") || l.public)
		prefer := int64(2)
		if isHost {
			prefer = 1
		}
		str := func(k string) string {
			if v, ok := m[k]; ok {
				return pyStr(v)
			}
			return "?"
		}
		l.players[pid] = &lobbyPlayer{id: pid, name: cleanName(m["name"], fmt.Sprintf("Player%d", pid)), station: l.freeStation(prefer),
			ready: isHost, host: isHost, version: str("version"), files: str("files"), platform: str("platform"), seatKey: m["seatkey"] == true}
		c.pid = pid
		l.conns = append(l.conns, c)
		if isHost {
			l.host = pid
		}
		suffix := ""
		if isHost {
			suffix = " (the host)"
		}
		l.note(l.players[pid].name + " joined" + suffix)
		l.publish()
		return nil
	}
	if _, ok := l.players[c.pid]; !ok {
		l.error(c, "join", "join first")
		return nil
	}
	isHost := c.pid == l.host
	switch t {
	case "me":
		p := l.players[c.pid]
		if v, ok := m["station"]; ok && !pyEq(v, p.station) {
			st, err := pyInt(v)
			if err != nil {
				return err
			}
			taken := false
			for _, q := range l.players {
				taken = taken || equal(q.station, st)
			}
			if st < 1 || st > 3 || taken {
				l.error(c, "station", fmt.Sprintf("station %s is taken", pyStr(v)))
				return nil
			}
			p.station = st
		}
		if v, ok := m["name"]; ok {
			p.name = cleanName(v, p.name)
		}
		if v, ok := m["mods"]; ok {
			p.mods = shortStr(v, 64)
		}
		if v, ok := m["save"]; ok {
			p.save = shortStr(v, 64)
		}
		if v, ok := m["ready"]; ok && !isHost {
			p.ready = truthy(v)
		}
		l.publish()
	case "settings":
		if !isHost {
			l.error(c, "host", "only the host sets the game")
			return nil
		}
		s, ok := m["settings"].(obj)
		if !ok && truthy(m["settings"]) {
			return fmt.Errorf("'%s' object has no attribute 'items'", pyTypeName(m["settings"]))
		}
		for k, v := range s {
			if _, known := l.settings[k]; known && k != "mods" && len(pyDumps(v)) <= maxSetting {
				l.settings = with(l.settings, k, v)
			}
		}
		l.othersNotReady() // a new game: everyone looks again
		l.publish()
	case "mod":
		if !isHost {
			l.error(c, "host", "only the host sets the game")
			return nil
		}
		if truthy(m["hash"]) {
			data, ok := m["data"].(string)
			if shortStr(m["hash"], 32) == nil || !ok || len(data) > modDataMx {
				l.error(c, "mod", "not a selection the lobby takes")
				return nil
			}
			l.mod = obj{"hash": m["hash"], "data": data}
			l.settings = with(l.settings, "mods", m["hash"])
		} else {
			l.mod = nil
			l.settings = with(l.settings, "mods", nil)
		}
		l.players[l.host].mods = l.settings["mods"]
		l.othersNotReady()
		l.publish()
	case "getmod":
		if l.mod != nil {
			c.send(obj{"t": "mod", "hash": l.mod["hash"], "data": l.mod["data"]})
		} else {
			c.send(obj{"t": "mod", "hash": nil, "data": nil})
		}
	case "save": // the host goes on from a savegame (src/mpsave.c), or not (hash none)
		if !isHost {
			l.error(c, "host", "only the host sets the game")
			return nil
		}
		if truthy(m["hash"]) {
			meta := saveMetaOK(m["meta"])
			data, ok := m["data"].(string)
			if shortStr(m["hash"], 32) == nil || !ok || len(data) > saveDataMx || meta == nil {
				l.error(c, "save", "not a savegame the lobby takes")
				return nil
			}
			l.save = obj{"hash": m["hash"], "data": data, "meta": meta}
			l.players[l.host].save = m["hash"]
		} else {
			l.save = nil
			l.players[l.host].save = nil
		}
		l.othersNotReady()
		l.publish()
	case "getsave":
		if l.save != nil {
			c.send(obj{"t": "save", "hash": l.save["hash"], "data": l.save["data"], "meta": l.save["meta"]})
		} else {
			c.send(obj{"t": "save", "hash": nil, "data": nil, "meta": nil})
		}
	case "start":
		if !isHost {
			l.error(c, "host", "only the host starts the game")
			return nil
		}
		if l.phase != "lobby" { // (once: another start would give every launcher a second game, new secrets)
			l.error(c, "started", "the game has started")
			return nil
		}
		if probs := l.problems(); len(probs) > 0 {
			var parts []string
			for _, p := range probs {
				parts = append(parts, fmt.Sprintf("%s %s", p.(obj)["code"], p.(obj)["name"]))
			}
			l.error(c, "not_yet", "not everyone is ready: "+strings.Join(parts, ", "))
			return nil
		}
		l.goStart()
	case "leave":
		l.lost(c)
	}
	return nil
}

func (l *mpLobby) goStart() {
	ps := l.sortedPlayers()
	var stations []string
	names := []any{"", "", ""}
	for _, p := range ps {
		stations = append(stations, pyStr(p.station))
		if st, ok := p.station.(int64); ok {
			names[st-1] = p.name
		}
	}
	humans := strings.Join(stations, ",")
	seed := int64(1 + mrand.Intn(65535))
	// the game's name for its savegames: a saved game's own, else a new one
	game := ""
	if l.save != nil {
		if g := l.save["meta"].(obj)["game"]; truthy(g) {
			game = pyStr(g)
		}
	}
	if game == "" {
		var b [2]byte
		rand.Read(b[:])
		game = time.Now().Format("20060102-1504") + "-" + hex.EncodeToString(b[:])
	}
	l.phase = "starting"
	l.srv.seats = len(ps)
	l.srv.goAt = l.srv.h.mono()
	// each seat's secret, in that player's "go" alone: nobody else takes the seat (the room's
	// code is public); a launcher that does not know it (older) gets none, its seat is as before
	l.srv.keys = map[int]string{}
	for i, p := range ps {
		if p.seatKey {
			var k [16]byte
			rand.Read(k[:])
			l.srv.keys[i] = hex.EncodeToString(k[:])
		}
	}
	l.note(fmt.Sprintf("the game starts: stations %s, seed %d", humans, seed))
	var load, file any
	if l.save != nil {
		load, file = l.save["hash"], l.save["meta"].(obj)["file"]
	}
	for _, c := range l.conns {
		p := l.players[c.pid]
		if p == nil {
			continue
		}
		seat := 0
		for i, q := range ps {
			if q.id == p.id {
				seat = i
			}
		}
		g := obj{"t": "go", "seat": seat, "humans": humans, "names": names, "seed": seed, "settings": l.settings,
			"game": game, "load": load, "file": file}
		if k := l.srv.keys[seat]; k != "" {
			g["key"] = k
		}
		c.send(g)
	}
	l.publish()
}

func (l *mpLobby) playing() {
	l.phase = "playing"
	l.publish()
}

func (l *mpLobby) ended(why string) {
	l.phase, l.why = "ended", why
	l.publish()
}

func (l *mpLobby) lost(c *lobbyConn) {
	if c.gone {
		return
	}
	c.gone = true
	for i, x := range l.conns {
		if x == c {
			l.conns = append(l.conns[:i], l.conns[i+1:]...)
			break
		}
	}
	p := l.players[c.pid]
	if p == nil {
		return
	}
	if l.phase != "lobby" { // the games play on without their launchers
		return
	}
	delete(l.players, c.pid)
	l.note(p.name + " left")
	if c.pid == l.host {
		l.phase, l.why = "closed", "the host closed the game"
		l.srv.stopped = true // (first: the room ends even if telling the others fails)
		l.publish()
		return
	}
	l.publish()
}

// found: the answer to a launcher's search
func (l *mpLobby) found() obj {
	hp := l.players[l.host]
	name, ver := "", ""
	if hp != nil {
		name, ver = hp.name, hp.version
	}
	g := obj{"t": "game", "name": name, "players": len(l.players), "max": maxPlayers, "phase": l.phase, "version": ver}
	if l.srv.room != "" {
		g["room"] = l.srv.room
	} else {
		var hosts []any
		for _, h := range strings.Split(l.srv.a.host, ",") {
			hosts = append(hosts, h)
		}
		g["port"], g["hosts"] = l.srv.a.port, hosts
	}
	return g
}
