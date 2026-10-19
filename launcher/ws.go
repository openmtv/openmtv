package main

import (
	"bufio"
	"crypto/rand"
	"crypto/sha1"
	"crypto/tls"
	"encoding/base64"
	"encoding/binary"
	"errors"
	"fmt"
	"io"
	"net"
	"net/url"
	"strings"
	"sync"
	"time"
)

// The internet (stage 6, the former Python lobby (mplobby.py) WSConn, ws_connect, Bridge): the
// public server speaks WebSocket (wss://: TLS with the system's certificates);
// a lobby message is one binary WebSocket message, the plain protocol's
// 32-bit length and text inside.  The game speaks plain TCP to a bridge of
// its launcher, which passes everything on.

const wsGUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

// msgConn: a connection that carries the lobby's messages (TCP, or a WebSocket)
type msgConn interface {
	send(msg []byte) error // one message (without its length)
	recv() ([]byte, error) // one message
	close()
}

// tcpConn: the plain protocol: 32-bit length (little-endian), then the message
type tcpConn struct {
	c  net.Conn
	r  *bufio.Reader
	mu sync.Mutex
}

func (t *tcpConn) send(msg []byte) error {
	t.mu.Lock()
	defer t.mu.Unlock()
	b := binary.LittleEndian.AppendUint32(nil, uint32(len(msg)))
	_, err := t.c.Write(append(b, msg...))
	return err
}

func (t *tcpConn) recv() ([]byte, error) {
	var head [4]byte
	if _, err := io.ReadFull(t.r, head[:]); err != nil {
		return nil, err
	}
	n := binary.LittleEndian.Uint32(head[:])
	if n > 64<<20 {
		return nil, errors.New("a message too large")
	}
	msg := make([]byte, n)
	_, err := io.ReadFull(t.r, msg)
	return msg, err
}

func (t *tcpConn) close() { t.c.Close() }

// wsConn: a WebSocket to the server (a client's: its frames masked); pings answered
type wsConn struct {
	c      net.Conn
	r      *bufio.Reader
	mu     sync.Mutex
	closed bool
}

func (w *wsConn) frame(op byte, payload []byte) error {
	b := wsFrame(op, payload, true)
	w.mu.Lock()
	defer w.mu.Unlock()
	_, err := w.c.Write(b)
	return err
}

func (w *wsConn) send(msg []byte) error { return w.frame(2, msg) }

func (w *wsConn) recv() ([]byte, error) {
	return wsRead(w.r, 64<<20, func(p []byte) { w.frame(10, p) })
}

// wsFrame: one WebSocket frame, whole (FIN); MASKED: a client's, with a random mask
func wsFrame(op byte, payload []byte, masked bool) []byte {
	n := len(payload)
	var m byte
	if masked {
		m = 0x80
	}
	head := []byte{0x80 | op}
	switch {
	case n < 126:
		head = append(head, m|byte(n))
	case n < 65536:
		head = binary.BigEndian.AppendUint16(append(head, m|126), uint16(n))
	default:
		head = binary.BigEndian.AppendUint64(append(head, m|127), uint64(n))
	}
	if !masked {
		return append(head, payload...)
	}
	var mask [4]byte
	rand.Read(mask[:])
	out := append(head, mask[:]...)
	for i, b := range payload {
		out = append(out, b^mask[i%4])
	}
	return out
}

// wsRead: one message of a WebSocket, its fragments joined (masked or not), at most MAX
// bytes (errTooBig); a ping is answered (PONG gets its payload), a close ends it (io.EOF)
func wsRead(r *bufio.Reader, max uint64, pong func([]byte)) ([]byte, error) {
	var data []byte
	for {
		var hd [2]byte
		if _, err := io.ReadFull(r, hd[:]); err != nil {
			return nil, err
		}
		fin, op, n := hd[0]&0x80 != 0, hd[0]&0x0F, uint64(hd[1]&0x7F)
		switch n {
		case 126:
			var e [2]byte
			if _, err := io.ReadFull(r, e[:]); err != nil {
				return nil, err
			}
			n = uint64(binary.BigEndian.Uint16(e[:]))
		case 127:
			var e [8]byte
			if _, err := io.ReadFull(r, e[:]); err != nil {
				return nil, err
			}
			n = binary.BigEndian.Uint64(e[:])
		}
		if n > max || uint64(len(data))+n > max {
			return nil, errTooBig
		}
		var mask [4]byte
		masked := hd[1]&0x80 != 0
		if masked {
			if _, err := io.ReadFull(r, mask[:]); err != nil {
				return nil, err
			}
		}
		payload := make([]byte, n)
		if _, err := io.ReadFull(r, payload); err != nil {
			return nil, err
		}
		if masked {
			for i := range payload {
				payload[i] ^= mask[i%4]
			}
		}
		switch op {
		case 8:
			return nil, io.EOF
		case 9:
			pong(payload)
			continue
		case 10:
			continue
		}
		data = append(data, payload...)
		if fin {
			return data, nil
		}
	}
}

func (w *wsConn) close() {
	w.mu.Lock()
	closed := w.closed
	w.closed = true
	w.mu.Unlock()
	if !closed {
		w.frame(8, nil)
	}
	w.c.Close()
}

// wsConnect: a WebSocket to ADDR (ws:// or wss://host[:port]/path)
func wsConnect(addr string) (*wsConn, error) {
	u, err := url.Parse(addr)
	if err != nil {
		return nil, fmt.Errorf("%s: %v", addr, err)
	}
	secure := u.Scheme == "wss"
	host := u.Hostname()
	port := u.Port()
	if port == "" {
		port = map[bool]string{true: "443", false: "80"}[secure]
	}
	c, err := net.DialTimeout("tcp", net.JoinHostPort(host, port), 10*time.Second)
	if err != nil {
		return nil, fmt.Errorf("no connection to %s (%v)", host, err)
	}
	if tc, ok := c.(*net.TCPConn); ok {
		tc.SetNoDelay(true)
	}
	if secure {
		t := tls.Client(c, &tls.Config{ServerName: host})
		t.SetDeadline(time.Now().Add(10 * time.Second))
		if err := t.Handshake(); err != nil {
			c.Close()
			return nil, fmt.Errorf("no secure connection to %s (%v)", host, err)
		}
		t.SetDeadline(time.Time{})
		c = t
	}
	var k [16]byte
	rand.Read(k[:])
	key := base64.StdEncoding.EncodeToString(k[:])
	hostHeader := host
	if strings.Contains(host, ":") { // (an IPv6 address in brackets)
		hostHeader = "[" + host + "]"
	}
	if u.Port() != "" && u.Port() != map[bool]string{true: "443", false: "80"}[secure] {
		hostHeader += ":" + u.Port()
	}
	path := u.EscapedPath()
	if path == "" {
		path = "/"
	}
	fmt.Fprintf(c, "GET %s HTTP/1.1\r\nHost: %s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"+
		"Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\nUser-Agent: madtv-launcher/%s\r\n\r\n", path, hostHeader, key, lobbyVersion())
	c.SetReadDeadline(time.Now().Add(10 * time.Second))
	r := bufio.NewReader(c)
	var lines []string
	for {
		l, err := r.ReadString('\n')
		if err != nil {
			c.Close()
			return nil, fmt.Errorf("no answer from %s (%v)", host, err)
		}
		l = strings.TrimRight(l, "\r\n")
		if l == "" {
			break
		}
		lines = append(lines, l)
	}
	c.SetReadDeadline(time.Time{})
	sum := sha1.Sum([]byte(key + wsGUID))
	want := base64.StdEncoding.EncodeToString(sum[:])
	accepted := false
	for _, l := range lines {
		if k, v, ok := strings.Cut(l, ":"); ok && strings.EqualFold(strings.TrimSpace(k), "sec-websocket-accept") && strings.TrimSpace(v) == want {
			accepted = true
		}
	}
	if len(lines) == 0 || !strings.Contains(lines[0]+" ", " 101 ") || !accepted {
		c.Close()
		first := "no answer"
		if len(lines) > 0 {
			first = lines[0]
		}
		return nil, fmt.Errorf("%s: no game server there (%s)", addr, first)
	}
	return &wsConn{c: c, r: r}, nil
}

// bridge: the game speaks plain TCP to this machine (127.0.0.1, a free port); its connection
// goes on to the server by WebSocket (URL), both ways.  Either broken, both end -- and the game
// comes back on a new connection (runtime/net.c RESUME): the bridge takes it, with a new
// WebSocket, as long as the bridge lives.  FAILED hears why the server could not be reached
// for the game's first connection; close ends it (the lobby left)
type bridge struct {
	port   int
	mu     sync.Mutex
	closed bool
	conns  []io.Closer // the listener, then the game's connections and the WebSockets
}

func newBridge(serverURL string, wait time.Duration, failed func(string)) (*bridge, error) {
	ln, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		return nil, err
	}
	b := &bridge{port: ln.Addr().(*net.TCPAddr).Port, conns: []io.Closer{ln}}
	go func() {
		ln.(*net.TCPListener).SetDeadline(time.Now().Add(wait)) // (the game's first: within WAIT)
		for first := true; ; first = false {
			g, err := ln.Accept()
			if err != nil {
				ln.Close()
				return
			}
			ln.(*net.TCPListener).SetDeadline(time.Time{})
			if !b.keep(g) {
				return
			}
			f := failed
			if !first { // (a game back on a new connection: its failure is its own -- it tries again)
				f = func(string) {}
			}
			go b.carry(g, serverURL, f)
		}
	}()
	return b, nil
}

// carry: one connection of the game, to the server and back
func (b *bridge) carry(g net.Conn, serverURL string, failed func(string)) {
	if tc, ok := g.(*net.TCPConn); ok {
		tc.SetNoDelay(true)
	}
	ws, err := wsConnect(serverURL)
	if err != nil {
		g.Close()
		if !b.isClosed() {
			failed(err.Error())
		}
		return
	}
	if !b.keep(ws.c) {
		return
	}
	go func() { // the game -> the server: its messages, one WebSocket message each
		t := &tcpConn{c: g, r: bufio.NewReaderSize(g, 65536)}
		for {
			m, err := t.recv()
			if err != nil {
				break
			}
			if ws.send(m) != nil {
				break
			}
		}
		ws.close()
	}()
	for { // the server -> the game
		m, err := ws.recv()
		if err != nil {
			break
		}
		if _, err := g.Write(append(binary.LittleEndian.AppendUint32(nil, uint32(len(m))), m...)); err != nil {
			break
		}
	}
	if tc, ok := g.(*net.TCPConn); ok {
		tc.CloseWrite()
	}
	g.Close()
	ws.close()
	b.forget(g, ws.c)
}

// forget: connections that have ended (the bridge closes only what is open)
func (b *bridge) forget(cs ...io.Closer) {
	b.mu.Lock()
	defer b.mu.Unlock()
	keep := b.conns[:0]
	for _, c := range b.conns {
		gone := false
		for _, x := range cs {
			gone = gone || c == x
		}
		if !gone {
			keep = append(keep, c)
		}
	}
	b.conns = keep
}

// keep: C is closed with the bridge (false: the bridge is closed already, C too)
func (b *bridge) keep(c io.Closer) bool {
	b.mu.Lock()
	defer b.mu.Unlock()
	if b.closed {
		c.Close()
		return false
	}
	b.conns = append(b.conns, c)
	return true
}

func (b *bridge) isClosed() bool {
	b.mu.Lock()
	defer b.mu.Unlock()
	return b.closed
}

func (b *bridge) close() {
	b.mu.Lock()
	conns := b.conns
	b.closed, b.conns = true, nil
	b.mu.Unlock()
	for _, c := range conns {
		c.Close()
	}
}
