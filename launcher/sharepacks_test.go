package main

import (
	"encoding/base64"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"
)

// the host's packs handed on in the lobby: packed file for file, checked, kept by a guest --
// a new one under its name, the same one not again, another one of that name as NAME-2 with
// "(from HOST)" in its title; what is not a pack's is refused
func TestSharePacks(t *testing.T) {
	keptRoot := root
	defer func() { root = keptRoot }()
	host := t.TempDir()
	root = host
	write := func(name, file, text string) {
		d := filepath.Join(root, "mods", name)
		os.MkdirAll(d, 0o777)
		if err := os.WriteFile(filepath.Join(d, file), []byte(text), 0o666); err != nil {
			t.Fatal(err)
		}
	}
	studios := `{"table": "studios", "records": [{"id": 11, "size": 3, "renter": 255, "rent": 14000}]}`
	write("mehr", "mod.json", "{\n \"title\": \"Mehr Studios\",\n \"protected\": true\n}")
	write("mehr", "studios.json", studios)
	write("mehr", "notes.txt", "not a pack's file") // (left out)
	offer, err := packOffer([]string{"mehr"})
	if err != nil || len(offer) != 1 {
		t.Fatalf("offer: %v %v", offer, err)
	}
	if _, ok := offerOK(offer); !ok {
		t.Fatal("the server refuses the offer")
	}
	files, err := decodePack(offer[0].(obj)["data"].(string))
	if err != nil || len(files) != 2 || string(files["studios.json"]) != studios {
		t.Fatalf("decoded: %v %v", files, err)
	}
	// the guest
	root = t.TempDir()
	if s := packState("mehr", files); s != packStateNew {
		t.Errorf("state %s, want new", s)
	}
	as, err := keepPack("mehr", files, "Anna", "de")
	if err != nil || as != "mehr" || packState("mehr", files) != packStateSame {
		t.Errorf("kept as %s: %v", as, err)
	}
	if as, _ := keepPack("mehr", files, "Anna", "de"); as != "mehr" {
		t.Errorf("kept again as %s", as)
	}
	write("mehr", "studios.json", `{"table": "studios", "records": []}`) // the guest's own pack of that name
	if packState("mehr", files) != packStateDiff {
		t.Error("another pack of that name: not 'differs'")
	}
	as, err = keepPack("mehr", files, "Anna", "de")
	meta, _ := modMeta("mehr-2")
	if err != nil || as != "mehr-2" || meta["title"] != "Mehr Studios (von Anna)" || meta["protected"] != true {
		t.Errorf("kept as %s, %v: %v", as, meta, err)
	}
	if as, _ := keepPack("mehr", files, "Anna", "de"); as != "mehr-2" {
		t.Errorf("kept a second time as %s", as)
	}
	if s := packState("mehr", files); s != packStateSame { // (the copy kept under mehr-2)
		t.Errorf("state after keeping beside another one: %s, want same", s)
	}
	if left, _ := filepath.Glob(filepath.Join(root, "mods", ".keep-*")); len(left) > 0 {
		t.Errorf("left behind: %v", left)
	}
	// refused
	bad := func(files obj) string {
		return deflateB64(pyDumps(files))
	}
	for what, data := range map[string]string{
		"a path":          bad(obj{"mod.json": "{}", "../x.json": "{}"}),
		"not JSON":        bad(obj{"mod.json": "{}", "films.json": "nope"}),
		"no records":      bad(obj{"mod.json": "{}", "films.json": "{\"records\": 3}"}),
		"no mod.json":     bad(obj{"films.json": "{\"records\": []}"}),
		"not base64/zlib": "!!!",
	} {
		if _, err := decodePack(data); err == nil {
			t.Errorf("%s: taken", what)
		}
	}
	for what, v := range map[string]any{
		"a name with a slash": []any{obj{"name": "../x", "title": "x", "data": "x"}},
		"no data":             []any{obj{"name": "x", "title": "x"}},
		"too large":           []any{obj{"name": "x", "title": "x", "data": strings.Repeat("A", packOfferMx+1)}},
		"not a list":          obj{},
	} {
		if _, ok := offerOK(v); ok {
			t.Errorf("server: %s taken", what)
		}
	}
}

func deflateB64(s string) string { return base64.StdEncoding.EncodeToString(deflate([]byte(s))) }

// the server keeps the host's packs with the selection and hands them to whoever asks
// ("getmod"); a bad offer is refused, the selection stays as it was
func TestServerRelaysPacks(t *testing.T) {
	a := &mpArgs{public: true, pace: "realtime", lead: 12, margin: 2, window: 70, dropAft: 60, silent: 8, startWait: 60}
	l := newGameServer(&mpHub{a: a, t0: time.Now(), rooms: map[string]*gameServer{}}, "abcdef").lobby
	c := &lobbyConn{out: pipeOut()}
	l.message(c, obj{"t": "join", "create": true, "name": "Host"})
	offer := []any{obj{"name": "mehr", "title": "Mehr Studios", "data": "eJyrrgUAAXUA+Q=="}}
	l.message(c, obj{"t": "mod", "hash": "0123456789abcdef", "data": "eJyrrgUAAXUA+Q==", "packs": offer})
	if l.mod == nil || !equal(l.mod["packs"], offer) {
		t.Fatalf("the server's selection: %v", l.mod)
	}
	l.message(c, obj{"t": "mod", "hash": "fedcba9876543210", "data": "eJyrrgUAAXUA+Q==", "packs": []any{obj{"name": "../x", "title": "x", "data": "x"}}})
	if l.mod["hash"] != "0123456789abcdef" {
		t.Errorf("a bad offer taken: %v", l.mod)
	}
	l.message(c, obj{"t": "mod", "hash": "fedcba9876543210", "data": "eJyrrgUAAXUA+Q=="}) // (an older host: no packs)
	if _, ok := l.mod["packs"]; ok || l.mod["hash"] != "fedcba9876543210" {
		t.Errorf("an older host's selection: %v", l.mod)
	}
}
