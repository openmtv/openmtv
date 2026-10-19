package main

import (
	"bufio"
	"encoding/binary"
	"encoding/json"
	"io"
	"net"
	"strings"
	"testing"
	"time"
)

// A seat's secret: the lobby gives each player's game its own in "go"; the game's HELLO must
// carry it -- nobody else takes the seat (the room's code is in the public list).

// skHub: a server on 127.0.0.1 -- public (rooms, WebSocket) or, with SEATS, one game
// without a lobby (TCP, as tools/seattest.py and tools/mptest.py run it)
func skHub(t *testing.T, seats int) string {
	a := &mpArgs{public: seats == 0, seats: seats, pace: "realtime", lead: 12, margin: 2, window: 70, dropAft: 60, silent: 8, startWait: 60, host: "127.0.0.1"}
	h := &mpHub{a: a, t0: time.Now(), rooms: map[string]*gameServer{}}
	if seats > 0 {
		h.one = newGameServer(h, "")
	}
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
	return ln.Addr().String()
}

// skConn: a connection to the server, its messages in a channel
type skConn struct {
	send func(string)
	in   chan string
}

func skWS(t *testing.T, url string) *skConn {
	ws, err := wsConnect(url)
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { ws.close() })
	c := &skConn{send: func(s string) { ws.send([]byte(s)) }, in: make(chan string, 1000)}
	go func() {
		defer close(c.in)
		for {
			m, err := ws.recv()
			if err != nil {
				return
			}
			c.in <- string(m)
		}
	}()
	return c
}

func skTCP(t *testing.T, addr string) *skConn {
	nc, err := net.Dial("tcp", addr)
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { nc.Close() })
	c := &skConn{in: make(chan string, 1000)}
	c.send = func(s string) {
		b := binary.LittleEndian.AppendUint32(nil, uint32(len(s)))
		nc.Write(append(b, s...))
	}
	go func() {
		defer close(c.in)
		r := bufio.NewReader(nc)
		for {
			var head [4]byte
			if _, err := io.ReadFull(r, head[:]); err != nil {
				return
			}
			b := make([]byte, binary.LittleEndian.Uint32(head[:]))
			if _, err := io.ReadFull(r, b); err != nil {
				return
			}
			c.in <- string(b)
		}
	}()
	return c
}

func (c *skConn) json(v obj) { b, _ := json.Marshal(v); c.send(string(b)) }

// wait: the first message with one of SUBS in it
func (c *skConn) wait(t *testing.T, subs ...string) string {
	t.Helper()
	to := time.After(5 * time.Second)
	for {
		select {
		case m, ok := <-c.in:
			if !ok {
				t.Fatalf("closed waiting for %q", subs)
			}
			for _, p := range subs {
				if strings.Contains(m, p) {
					return m
				}
			}
		case <-to:
			t.Fatalf("timeout waiting for %q", subs)
		}
	}
}

// skLobby: a public room, host and guest joined (SEATKEY: their launchers know the
// seat's secret), the guest ready, started -> the server's address, the room, the two
// "go" messages
func skLobby(t *testing.T, hostKey, guestKey bool) (string, string, [2]obj) {
	addr := skHub(t, 0)
	url := "ws://" + addr
	join := func(c *skConn, m obj, seatkey bool) {
		if seatkey {
			m["seatkey"] = true
		}
		c.json(m)
	}
	host := skWS(t, url+"/lobby")
	join(host, obj{"t": "join", "create": true, "name": "Host", "version": "v", "files": "f"}, hostKey)
	var st map[string]any
	json.Unmarshal([]byte(host.wait(t, `"room"`)), &st)
	room, _ := st["room"].(string)
	if room == "" {
		t.Fatalf("no room: %v", st)
	}
	guest := skWS(t, url+"/lobby")
	join(guest, obj{"t": "join", "room": room, "name": "Guest", "version": "v", "files": "f"}, guestKey)
	guest.wait(t, `"state"`)
	guest.json(obj{"t": "me", "ready": true})
	time.Sleep(100 * time.Millisecond)
	host.json(obj{"t": "start"})
	var gos [2]obj
	for i, c := range []*skConn{host, guest} {
		var m map[string]any
		json.Unmarshal([]byte(c.wait(t, `"go"`)), &m)
		gos[i] = m
	}
	return url, room, gos
}

const skParams = "humans=1,2 args=e fixes=all files=0000"

func skKey(t *testing.T, g obj) string {
	k, _ := g["key"].(string)
	return k
}

// TestSeatKeyHijack: an outsider who knows only the room's code takes no seat
func TestSeatKeyHijack(t *testing.T) {
	url, room, gos := skLobby(t, true, true)
	k0, k1 := skKey(t, gos[0]), skKey(t, gos[1])
	evil := skWS(t, url+"/game/"+room)
	for _, hello := range []string{"HELLO 0 1 evil=1", "HELLO 0 1:" + strings.Repeat("0", 32) + " evil=1", "HELLO 0 1:" + k1 + " evil=1", "HELLO 1 1 evil=1"} {
		evil.send(hello)
		if m := evil.wait(t, "REFUSE", "WELCOME"); !strings.HasPrefix(m, "REFUSE") {
			t.Fatalf("the outsider's %q: %s", hello, m)
		}
	}
	if !seatKeyOK(k0) || !seatKeyOK(k1) || k0 == k1 {
		t.Fatalf("the go messages' secrets: %q %q", k0, k1)
	}
	g0 := skWS(t, url+"/game/"+room)
	g0.send("HELLO 0 1:" + k0 + " " + skParams)
	if m := g0.wait(t, "REFUSE", "WELCOME"); !strings.HasPrefix(m, "WELCOME seat 0 of 2") {
		t.Fatalf("seat 0: %s", m)
	}
	g1 := skWS(t, url+"/game/"+room)
	g1.send("HELLO 1 1:" + k1 + " " + skParams)
	if m := g1.wait(t, "REFUSE", "WELCOME"); !strings.HasPrefix(m, "WELCOME seat 1 of 2") {
		t.Fatalf("seat 1: %s", m)
	}
}

// TestSeatKeyOldLaunchers: launchers that do not announce "seatkey" get none, their games'
// HELLO without one are taken as before; a mixed room: the secret only for the new one's seat
func TestSeatKeyOldLaunchers(t *testing.T) {
	url, room, gos := skLobby(t, false, false)
	if _, ok := gos[0]["key"]; ok {
		t.Fatalf("a key for an older launcher: %v", gos[0])
	}
	for seat := 0; seat < 2; seat++ {
		g := skWS(t, url+"/game/"+room)
		g.send("HELLO " + string(rune('0'+seat)) + " 1 " + skParams)
		if m := g.wait(t, "REFUSE", "WELCOME"); !strings.HasPrefix(m, "WELCOME") {
			t.Fatalf("seat %d: %s", seat, m)
		}
	}

	url, room, gos = skLobby(t, true, false) // the host's launcher new, the guest's older
	if !seatKeyOK(skKey(t, gos[0])) || gos[1]["key"] != nil {
		t.Fatalf("the go messages: %v / %v", gos[0], gos[1])
	}
	evil := skWS(t, url+"/game/"+room)
	evil.send("HELLO 0 1 evil=1")
	if m := evil.wait(t, "REFUSE", "WELCOME"); !strings.HasPrefix(m, "REFUSE") {
		t.Fatalf("the outsider on the new launcher's seat: %s", m)
	}
	g0 := skWS(t, url+"/game/"+room)
	g0.send("HELLO 0 1:" + skKey(t, gos[0]) + " " + skParams)
	g1 := skWS(t, url+"/game/"+room)
	g1.send("HELLO 1 1 " + skParams)
	for i, g := range []*skConn{g0, g1} {
		if m := g.wait(t, "REFUSE", "WELCOME"); !strings.HasPrefix(m, "WELCOME") {
			t.Fatalf("seat %d: %s", i, m)
		}
	}
}

// TestSeatKeyNoLobby: a server without a lobby (--seats N: the test harnesses) asks for no
// secret; a game that sends one anyway is taken too
func TestSeatKeyNoLobby(t *testing.T) {
	addr := skHub(t, 2)
	g0 := skTCP(t, addr)
	g0.send("HELLO 0 1 " + skParams)
	g1 := skTCP(t, addr)
	g1.send("HELLO 1 1:00ff " + skParams)
	for i, g := range []*skConn{g0, g1} {
		if m := g.wait(t, "REFUSE", "WELCOME"); !strings.HasPrefix(m, "WELCOME") {
			t.Fatalf("seat %d: %s", i, m)
		}
	}
}

// TestSeatKeyCommand: the launcher passes the go message's secret on to the game (--mp-key),
// none or a broken one not at all
func TestSeatKeyCommand(t *testing.T) {
	l := newLobby(t.TempDir(), "test")
	l.address = "127.0.0.1:1"
	for _, c := range []struct {
		key  any
		want bool
	}{{"0123abcd", true}, {nil, false}, {"", false}, {"--fixes", false}, {"ABCD", false}, {strings.Repeat("a", 65), false}, {int64(5), false}} {
		m := obj{"t": "go", "seat": int64(1), "humans": "1,2", "names": []any{"A", "B", ""}, "seed": int64(7), "settings": obj{}}
		if c.key != nil {
			m["key"] = c.key
		}
		args, err := l.command(m)
		if err != nil {
			t.Fatal(err)
		}
		cmd := strings.Join(args, " ")
		if c.want != strings.Contains(cmd, "--mp-key") || (c.want && !strings.Contains(cmd, "--mp-key "+c.key.(string))) {
			t.Errorf("key %v: %s", c.key, cmd)
		}
		if _, err := parse(args); err != nil { // (play.go takes it)
			t.Errorf("key %v: %v", c.key, err)
		}
	}
}

// TestSeatKeyStartOnce: a second "start" of the host (a double click) starts nothing
// again -- no second "go", the seats' secrets stay
func TestSeatKeyStartOnce(t *testing.T) {
	addr := skHub(t, 0)
	url := "ws://" + addr
	host := skWS(t, url+"/lobby")
	host.json(obj{"t": "join", "create": true, "name": "Host", "version": "v", "files": "f", "seatkey": true})
	var st map[string]any
	json.Unmarshal([]byte(host.wait(t, `"room"`)), &st)
	room, _ := st["room"].(string)
	guest := skWS(t, url+"/lobby")
	guest.json(obj{"t": "join", "room": room, "name": "Guest", "version": "v", "files": "f", "seatkey": true})
	guest.wait(t, `"state"`)
	guest.json(obj{"t": "me", "ready": true})
	time.Sleep(100 * time.Millisecond)
	host.json(obj{"t": "start"})
	var g map[string]any
	json.Unmarshal([]byte(guest.wait(t, `"go"`)), &g)
	host.json(obj{"t": "start"})
	if m := host.wait(t, `"error"`); !strings.Contains(m, "started") {
		t.Fatalf("a second start: %s", m)
	}
	for quiet := time.After(300 * time.Millisecond); quiet != nil; {
		select {
		case m := <-guest.in:
			if strings.Contains(m, `"go"`) {
				t.Fatalf("a second go: %s", m)
			}
		case <-quiet:
			quiet = nil
		}
	}
	g1 := skWS(t, url+"/game/"+room)
	g1.send("HELLO 1 1:" + g["key"].(string) + " " + skParams)
	if m := g1.wait(t, "REFUSE", "WELCOME"); !strings.HasPrefix(m, "WELCOME") {
		t.Fatalf("seat 1 with the first go's secret: %s", m)
	}
}
