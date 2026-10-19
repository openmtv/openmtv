package main

import (
	"context"
	"encoding/json"
	"fmt"
	"net"
	"os"
	"strings"
	"sync"
	"syscall"
	"time"
)

// The LAN (the former Python lobby (mplobby.py) addresses, find): this machine's addresses as a guest
// would type them, and the search for open games -- "MADTV?" by UDP broadcast on
// the lobby's port, every server answers with its game.

const lobbyPort = 7640

var (
	addrMu   sync.Mutex
	addrTime time.Time
	addrList []string
	addrBusy bool // a refresh runs
)

// lanAddresses: this machine's addresses in the LAN (best effort; a minute -- then read
// anew beside, the old ones meanwhile: a slow name lookup holds up nothing)
func lanAddresses() []string {
	addrMu.Lock()
	if addrTime.IsZero() {
		addrMu.Unlock()
		list := readAddresses()
		addrMu.Lock()
		if addrTime.IsZero() {
			addrTime, addrList = time.Now(), list
		}
	} else if time.Since(addrTime) > time.Minute && !addrBusy {
		addrBusy = true
		go func() {
			list := readAddresses()
			addrMu.Lock()
			addrTime, addrList, addrBusy = time.Now(), list, false
			addrMu.Unlock()
		}()
	}
	defer addrMu.Unlock()
	return append([]string{}, addrList...)
}

func readAddresses() []string {
	out := []string{}
	if c, err := net.Dial("udp4", "192.0.2.1:9"); err == nil { // (no packet goes out: it only picks the interface)
		out = append(out, c.LocalAddr().(*net.UDPAddr).IP.String())
		c.Close()
	}
	if name, err := os.Hostname(); err == nil {
		if ips, err := net.LookupIP(name); err == nil {
			for _, ip := range ips {
				if v4 := ip.To4(); v4 != nil && !v4.IsLoopback() && !contains(out, v4.String()) {
					out = append(out, v4.String())
				}
			}
		}
	}
	return out
}

// findGames: the open games of the LAN and of this machine
func findGames(timeout time.Duration, port int) []obj {
	lc := net.ListenConfig{Control: func(_, _ string, c syscall.RawConn) error {
		var err error
		c.Control(func(fd uintptr) { err = setBroadcast(fd) })
		return err
	}}
	pc, err := lc.ListenPacket(context.Background(), "udp4", ":0")
	if err != nil {
		return []obj{}
	}
	defer pc.Close()
	// 255.255.255.255 may leave by another adapter (Windows: a virtual one of WSL or
	// Hyper-V): also each of this machine's networks' own broadcast (/24 and /16)
	targets := []string{"255.255.255.255", "127.0.0.1"}
	for _, a := range lanAddresses() {
		q := strings.Split(a, ".")
		if len(q) == 4 {
			for _, b := range []string{q[0] + "." + q[1] + "." + q[2] + ".255", q[0] + "." + q[1] + ".255.255"} {
				if !contains(targets, b) {
					targets = append(targets, b)
				}
			}
		}
	}
	for _, t := range targets {
		pc.WriteTo([]byte("MADTV?"), &net.UDPAddr{IP: net.ParseIP(t), Port: port})
	}
	found := map[string]obj{}
	var order []string
	end := time.Now().Add(timeout)
	buf := make([]byte, 4096)
	for time.Now().Before(end) {
		pc.SetReadDeadline(end)
		n, from, err := pc.ReadFrom(buf)
		if err != nil {
			if ne, ok := err.(net.Error); ok && ne.Timeout() {
				break
			}
			continue // (Windows: a port nobody listens on -- read on)
		}
		var g obj
		v, err := parseJSON(buf[:n])
		if g, _ = v.(obj); err != nil || g == nil || g["t"] != "game" {
			continue
		}
		ip := from.(*net.UDPAddr).IP.String()
		var hosts []string
		if l, ok := g["hosts"].([]any); ok {
			for _, h := range l {
				if s, ok := h.(string); ok {
					hosts = append(hosts, s)
				}
			}
		}
		if len(hosts) == 0 {
			hosts = []string{"0.0.0.0"}
		}
		if !contains(hosts, "0.0.0.0") && !contains(hosts, ip) { // it answered here, but takes games elsewhere only
			ip = hosts[0]
		}
		gp := int64(port)
		if p, ok := g["port"].(int64); ok {
			gp = p
		}
		g["address"] = fmt.Sprintf("%s:%d", ip, gp)
		kb, _ := json.Marshal([]any{g["name"], g["port"], g["version"]})
		key := string(kb)
		if old, ok := found[key]; !ok || strings.HasPrefix(old["address"].(string), "127.") { // this machine's: the LAN address
			if !ok {
				order = append(order, key)
			}
			found[key] = g
		}
	}
	out := []obj{}
	for _, k := range order {
		out = append(out, found[k])
	}
	return out
}
