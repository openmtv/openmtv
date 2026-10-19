package main

import (
	"encoding/binary"
	"encoding/json"
	"fmt"
	"io"
	"net"
	"strings"
	"sync"
	"testing"
	"time"
)

// TestPyEq: the lobby compares a station as Python does (2.0 == 2, True == 1, "2" != 2)
func TestPyEq(t *testing.T) {
	for _, c := range []struct {
		a, b any
		want bool
	}{{2.0, int64(2), true}, {true, int64(1), true}, {"2", int64(2), false}, {nil, int64(2), false}, {nil, nil, true}, {2.5, int64(2), false}} {
		if got := pyEq(c.a, c.b); got != c.want {
			t.Errorf("pyEq(%#v, %#v) = %v", c.a, c.b, got)
		}
	}
}

// ---------------------------------------------------------------- the frame server, in the test

// capture: what the server writes to one client (TCP framing), kept
type capture struct {
	mu   sync.Mutex
	msgs []string
}

func (k *capture) all() []string {
	k.mu.Lock()
	defer k.mu.Unlock()
	return append([]string(nil), k.msgs...)
}

// waitFor: the first message that starts with PREFIX (5 s at most)
func (k *capture) waitFor(t *testing.T, prefix string) string {
	t.Helper()
	for end := time.Now().Add(5 * time.Second); time.Now().Before(end); time.Sleep(5 * time.Millisecond) {
		for _, m := range k.all() {
			if strings.HasPrefix(m, prefix) {
				return m
			}
		}
	}
	t.Fatalf("no %q among %q", prefix, k.all())
	return ""
}

func captureClient() (*mpClient, *capture) {
	a, b := net.Pipe()
	k := &capture{}
	go func() {
		for {
			var n [4]byte
			if _, err := io.ReadFull(b, n[:]); err != nil {
				return
			}
			m := make([]byte, binary.LittleEndian.Uint32(n[:]))
			if _, err := io.ReadFull(b, m); err != nil {
				return
			}
			k.mu.Lock()
			k.msgs = append(k.msgs, string(m))
			k.mu.Unlock()
		}
	}()
	return &mpClient{out: newOut(a, false), seat: -1, lostAt: -1}, k
}

// testSeated: a frame server (no ticker: the test is its loop) with two seats taken, not started
func testSeated(pace string, window int) (*gameServer, []*mpClient, []*capture) {
	a := &mpArgs{seats: 2, pace: pace, lead: 12, margin: 2, window: window, dropAft: 60, silent: 8, startWait: 60, host: "127.0.0.1"}
	s := newGameServer(&mpHub{a: a, t0: time.Now(), rooms: map[string]*gameServer{}}, "")
	var cs []*mpClient
	var ks []*capture
	for i := 0; i < 2; i++ {
		c, k := captureClient()
		s.message(c, fmt.Sprintf("HELLO %d 1 humans=1,2", i))
		cs, ks = append(cs, c), append(ks, k)
	}
	return s, cs, ks
}

// testGame: testSeated, started
func testGame(t *testing.T, pace string, window int) (*gameServer, []*mpClient, []*capture) {
	t.Helper()
	s, cs, ks := testSeated(pace, window)
	for _, c := range cs {
		s.message(c, "GO")
	}
	if !s.started {
		t.Fatal("the game did not start")
	}
	return s, cs, ks
}

// TestResumeNoDoubleInput: a live input the server holds unbundled (the window) when its
// client's connection breaks comes once, though the client sends it again after RESUMED
func TestResumeNoDoubleInput(t *testing.T) {
	s, cs, ks := testGame(t, "fast", 5)
	ks[1].waitFor(t, "B 5")
	s.message(cs[0], "IN 0 kcode 001C 0") // (held: nobody is past frame 0, the window ends at 5)
	nc, nk := captureClient()             // the break; a new connection
	s.message(nc, fmt.Sprintf("RESUME 0 %s 5", cs[0].token))
	nk.waitFor(t, "RESUMED 5")
	s.message(nc, "IN 0 kcode 001C 0") // (runtime/net.c: its inputs that have not come back, again)
	s.message(nc, "AT 5")
	s.message(cs[1], "AT 5")
	for _, k := range []*capture{nk, ks[1]} {
		b := k.waitFor(t, "B 10")
		if n := strings.Count(b, "kcode 001C"); n != 1 {
			t.Errorf("the live input came %d times: %q", n, b)
		}
	}
}

func heldInputs(s *gameServer) (n, bytes int) {
	for _, ins := range s.pending {
		for _, in := range ins {
			n, bytes = n+1, bytes+len(in.args)
		}
	}
	return
}

// TestInputBounds: the inputs the server holds are bounded -- no frame far ahead once the game
// runs, no long line, no endless script
func TestInputBounds(t *testing.T) {
	s, cs, _ := testSeated("fast", 70)
	for f := 1; f <= maxInputs+100; f++ { // a script (before the start: any frame)
		s.message(cs[0], fmt.Sprintf("IN %d kcode 001C 0", f*7))
	}
	if n, _ := heldInputs(s); n != maxInputs || s.held[0] != maxInputs {
		t.Errorf("a script of %d inputs: %d held (%d counted)", maxInputs+100, n, s.held[0])
	}
	s.message(cs[1], "IN 900000000 kcode 001C 0")
	s.message(cs[1], "IN 5 pointer 10 20")
	s.message(cs[0], "GO")
	s.message(cs[1], "GO")
	if !s.started {
		t.Fatal("the game did not start")
	}
	before, bytes := heldInputs(s)
	big := strings.Repeat("Z", 1<<20)
	for i := 0; i < 50; i++ {
		s.message(cs[1], fmt.Sprintf("IN %d k %s", 900000000+i, big)) // far ahead, and long
		s.message(cs[1], "IN 0 k "+big)                               // long
		s.message(cs[1], fmt.Sprintf("IN %d k x", 900000000+i))       // far ahead
	}
	if n, b := heldInputs(s); n != before || b != bytes {
		t.Errorf("after far and long inputs: %d inputs held (%d before), %d bytes (%d before)", n, before, b, bytes)
	}
	s.message(cs[1], "IN 0 kcode 001C 0") // live, and a frame within the window: kept
	s.message(cs[1], fmt.Sprintf("IN %d move 1 1", s.sent+s.a.window))
	if n, _ := heldInputs(s); n != before+2 || s.held[1] != 3 {
		t.Errorf("live input and a near frame: %d held (%d before), seat 1 counted %d", n, before, s.held[1])
	}
	s.message(cs[0], fmt.Sprintf("AT %d", 7*maxInputs))
	s.message(cs[1], fmt.Sprintf("AT %d", 7*maxInputs))
	for s.sent < 7*maxInputs+70 { // (fast: the bundles follow the clients)
		s.message(cs[0], fmt.Sprintf("AT %d", s.sent))
		s.message(cs[1], fmt.Sprintf("AT %d", s.sent))
	}
	if s.held[0] != 0 || s.held[1] != 1 { // (seat 1's frame 900000000 of the script stays)
		t.Errorf("bundled: still counted %v", s.held)
	}
}

// TestHashBounds: the hashes the server keeps are bounded -- frames played (not far ahead),
// as the client writes them, pruned once too old to compare; a difference is still found
func TestHashBounds(t *testing.T) {
	s, cs, ks := testGame(t, "fast", 70)
	s.a.dropAft = 0 // (the frames kept for a RESUME: 60 s)
	for i := 0; i < 5000; i++ {
		s.message(cs[0], fmt.Sprintf("HASH %d 00ff00ff", 1000000+i)) // far ahead
	}
	for _, h := range []string{"x", "0123456789abcdef0", "zz"} {
		s.message(cs[0], "HASH 70 "+h)
	}
	if len(s.hashes) != 0 {
		t.Fatalf("far frames and bad hashes kept: %d frames", len(s.hashes))
	}
	for f := 1; f <= 70; f++ { // (seat 0 only: kept, waiting for seat 1's)
		s.message(cs[0], fmt.Sprintf("HASH %d 0123456789abcdef", f))
	}
	s.message(cs[1], "HASH 70 0123456789abcdef")
	if s.agreed[70] != "0123456789abcdef" || len(s.hashes) != 69 {
		t.Fatalf("agreed %q, %d frames waiting", s.agreed[70], len(s.hashes))
	}
	for s.sent < 70*70 {
		s.message(cs[0], fmt.Sprintf("AT %d", s.sent))
		s.message(cs[1], fmt.Sprintf("AT %d", s.sent))
	}
	if len(s.agreed) != 0 || len(s.hashes) != 0 {
		t.Errorf("old frames kept: %d agreed, %d waiting (kept from frame %d)", len(s.agreed), len(s.hashes), s.keptTo)
	}
	s.message(cs[0], fmt.Sprintf("HASH %d 1", s.sent))
	s.message(cs[1], fmt.Sprintf("HASH %d 2", s.sent))
	ks[0].waitFor(t, fmt.Sprintf("DESYNC %d", s.sent))
}

// TestATBounds: a client's AT far ahead (near the int limit) does not make the server bundle
// that far: fast pace, the loop over the frames would never end
func TestATBounds(t *testing.T) {
	s, cs, ks := testGame(t, "fast", 70)
	done := make(chan bool)
	go func() {
		for _, c := range cs {
			s.message(c, "AT 9223372036854775737")
		}
		close(done)
	}()
	select {
	case <-done:
	case <-time.After(3 * time.Second):
		t.Fatal("the AT still bundles after 3 s")
	}
	if s.sent > 3*70 || cs[0].at > 3*70 {
		t.Errorf("bundled up to %d (at %d)", s.sent, cs[0].at)
	}
	s.message(cs[0], fmt.Sprintf("AT %d", s.sent)) // (and the game goes on)
	s.message(cs[1], fmt.Sprintf("AT %d", s.sent))
	ks[1].waitFor(t, fmt.Sprintf("B %d", s.sent))
}

// TestSettingsBounds: a room's host cannot store large values in its settings
func TestSettingsBounds(t *testing.T) {
	a := &mpArgs{public: true, pace: "realtime", lead: 12, margin: 2, window: 70, dropAft: 60, silent: 8, startWait: 60}
	l := newGameServer(&mpHub{a: a, t0: time.Now(), rooms: map[string]*gameServer{}}, "abcdef").lobby
	c := &lobbyConn{out: pipeOut()}
	l.message(c, obj{"t": "join", "create": true, "name": "Host"})
	packs := []any{}
	for i := 0; i < 20; i++ {
		packs = append(packs, fmt.Sprintf("a pack with a long name %d", i))
	}
	l.message(c, obj{"t": "settings", "settings": obj{"lang": strings.Repeat("A", 1<<20), "fixes": "classic", "packs": packs}})
	if l.settings["lang"] != "e" || l.settings["fixes"] != "classic" || len(l.settings["packs"].([]any)) != 20 {
		t.Errorf("settings: %v", pyDumps(l.settings))
	}
}

// pipeOut: a connection whose messages nobody needs
func pipeOut() *outConn {
	a, b := net.Pipe()
	go io.Copy(io.Discard, b)
	return newOut(a, false)
}

// ---------------------------------------------------------------- the public server, in the test

// startPublic: a public server (--public) on 127.0.0.1, its handshake, idle and ping times TIMES
func startPublic(t *testing.T, times [4]time.Duration) (string, *mpHub) {
	t.Helper()
	a := &mpArgs{public: true, pace: "realtime", lead: 12, margin: 2, window: 70, dropAft: 60, silent: 8, startWait: 60, host: "127.0.0.1"}
	h := &mpHub{a: a, t0: time.Now(), rooms: map[string]*gameServer{}, times: times}
	ln, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { ln.Close() })
	go func() {
		for {
			c, err := ln.Accept()
			if err != nil {
				return
			}
			go h.handle(c)
		}
	}()
	return "ws://" + ln.Addr().String(), h
}

// wsClient: a WebSocket to the test's server, its messages read (pings answered) into in
type wsClient struct {
	ws *wsConn
	in chan string
}

func dialWS(t *testing.T, url string) *wsClient {
	t.Helper()
	ws, err := wsConnect(url)
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(ws.close)
	c := &wsClient{ws: ws, in: make(chan string, 1000)}
	go func() {
		for {
			m, err := ws.recv()
			if err != nil {
				close(c.in)
				return
			}
			c.in <- string(m)
		}
	}()
	return c
}

func (c *wsClient) send(v any) {
	b, _ := json.Marshal(v)
	c.ws.send(b)
}

// wait: the first message containing SUB (5 s at most)
func (c *wsClient) wait(t *testing.T, sub string) string {
	t.Helper()
	to := time.After(5 * time.Second)
	for {
		select {
		case m, ok := <-c.in:
			if !ok {
				t.Fatalf("closed waiting for %q", sub)
			}
			if strings.Contains(m, sub) {
				return m
			}
		case <-to:
			t.Fatalf("timeout waiting for %q", sub)
		}
	}
}

// closedWithin: the server closes the connection within D (its messages up to then skipped)
func (c *wsClient) closedWithin(d time.Duration) bool {
	to := time.After(d)
	for {
		select {
		case _, ok := <-c.in:
			if !ok {
				return true
			}
		case <-to:
			return false
		}
	}
}

// openRoom: a host opens a room -> the host, the room's code
func openRoom(t *testing.T, url string) (*wsClient, string) {
	t.Helper()
	host := dialWS(t, url+"/lobby")
	host.send(obj{"t": "join", "create": true, "listed": false, "name": "Host"})
	var m map[string]any
	json.Unmarshal([]byte(host.wait(t, `"room"`)), &m)
	return host, m["room"].(string)
}

func roomsOpen(h *mpHub) int {
	h.mu.Lock()
	defer h.mu.Unlock()
	return len(h.rooms)
}

// TestConnLifetime: a connection that never joins the lobby nor takes a seat ends after the
// handshake's time; one that joined lives on while it answers the pings; a room's end closes
// every connection it served (a silent one held the closed room before, its memory too)
func TestConnLifetime(t *testing.T) {
	url, h := startPublic(t, [4]time.Duration{400 * time.Millisecond, 600 * time.Millisecond, 100 * time.Millisecond})
	host, room := openRoom(t, url)
	silent := dialWS(t, url+"/game/"+room)  // says nothing
	refused := dialWS(t, url+"/game/"+room) // the lobby has not started the game: refused
	refused.ws.send([]byte("HELLO 0 1 humans=1,2"))
	refused.wait(t, "REFUSE")
	notJoined := dialWS(t, url+"/lobby") // a message to the room before joining it
	notJoined.send(obj{"t": "join", "room": room, "name": "Guest"})
	notJoined.wait(t, `"state"`)
	stranger := dialWS(t, url+"/game/"+room)
	stranger.send(obj{"t": "me", "ready": true})
	stranger.wait(t, "join first")
	for name, c := range map[string]*wsClient{"silent": silent, "refused": refused, "not joined": stranger} {
		if !c.closedWithin(2 * time.Second) {
			t.Errorf("%s: still open after the handshake's time", name)
		}
	}
	if host.closedWithin(time.Second) || notJoined.closedWithin(10*time.Millisecond) {
		t.Fatal("a player's connection (pings answered) closed")
	}
	stray := dialWS(t, url+"/game/"+room) // says nothing: the room's end closes it
	host.ws.close()                       // the host goes: the room closes
	if !stray.closedWithin(time.Second) || !notJoined.closedWithin(time.Second) {
		t.Error("a closed room's connection still open")
	}
	if n := roomsOpen(h); n != 0 {
		t.Errorf("%d rooms open", n)
	}
	// a player that does not answer the pings: its connection ends after the idle time
	openRoom(t, url)
	ws, err := wsConnect(url + "/lobby")
	if err != nil {
		t.Fatal(err)
	}
	defer ws.close()
	b, _ := json.Marshal(obj{"t": "join", "create": true, "listed": false})
	ws.send(b)
	time.Sleep(100 * time.Millisecond)
	if n := roomsOpen(h); n != 2 {
		t.Fatalf("%d rooms open, not 2", n)
	}
	for end := time.Now().Add(3 * time.Second); roomsOpen(h) != 1 && time.Now().Before(end); time.Sleep(20 * time.Millisecond) {
	}
	if n := roomsOpen(h); n != 1 {
		t.Errorf("the room of a host that does not answer: still open (%d rooms)", n)
	}
}

// TestLobbyLifetime: a public room whose game does not start closes after its time (64 rooms
// held open by idle hosts left nobody else a game)
func TestLobbyLifetime(t *testing.T) {
	url, h := startPublic(t, [4]time.Duration{0, 0, 0, 500 * time.Millisecond})
	host, room := openRoom(t, url)
	guest := dialWS(t, url+"/lobby")
	guest.send(obj{"t": "join", "room": room, "name": "Guest"})
	guest.wait(t, `"state"`)
	st := guest.wait(t, `"closed"`)
	if !strings.Contains(st, "did not start") {
		t.Errorf("closed: %s", st)
	}
	if !host.closedWithin(time.Second) || roomsOpen(h) != 0 {
		t.Errorf("the room still open (%d rooms)", roomsOpen(h))
	}
}

// TestLobbyLifetimeActive: a lobby in use is no forgotten one -- its time counts from the
// last word in it, not from its opening (a group still choosing packs stays)
func TestLobbyLifetimeActive(t *testing.T) {
	url, _ := startPublic(t, [4]time.Duration{0, 0, 0, 600 * time.Millisecond})
	host, room := openRoom(t, url)
	guest := dialWS(t, url+"/lobby")
	guest.send(obj{"t": "join", "room": room, "name": "Guest"})
	guest.wait(t, `"state"`)
	for i := 0; i < 6; i++ { // (1.2 s, twice the lobby's time, with a word every 200 ms)
		time.Sleep(200 * time.Millisecond)
		host.send(obj{"t": "me", "name": "Host"})
	}
	if host.closedWithin(100 * time.Millisecond) {
		t.Fatalf("a lobby in use closed")
	}
	if !host.closedWithin(2 * time.Second) {
		t.Errorf("a lobby left alone stays open")
	}
}
