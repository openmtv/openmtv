package main

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
)

// a pack's name: 1-40 letters, digits, - or _, nothing after (no final newline); the
// same pack twice in a selection also when the case differs (Windows' and macOS'
// directories do not tell them apart)
func TestPackNames(t *testing.T) {
	keptRoot := root
	defer func() { root = keptRoot }()
	root = t.TempDir()
	for _, n := range []string{"abc", "ABC", "x"} {
		os.MkdirAll(filepath.Join(root, "mods", n), 0o777)
	}
	for _, n := range []string{"abc\n", "abc\r\n", "\nabc", "a/b", "", strings.Repeat("a", 41)} {
		if _, err := modDir(n); err == nil {
			t.Errorf("modDir(%q): taken", n)
		}
	}
	if _, err := modDir("A-b_9"); err != nil {
		t.Errorf("modDir(A-b_9): %v", err)
	}
	if _, err := packNames("abc+ABC"); err == nil || !strings.Contains(err.Error(), "twice") {
		t.Errorf("abc+ABC: %v", err)
	}
	if _, err := packNames("abc+x"); err != nil {
		t.Errorf("abc+x: %v", err)
	}
}

// a pack keeps only its own entries on disk (the shipped ones: their new films, series
// and ads); the original's come from the player's game files.  It plays as the same pack
// holding the whole table did (every pack before 2026-10-19), and the editor saves it
// back to its own entries -- byte for byte; a complete pack keeps the whole table
func TestPackOwnEntries(t *testing.T) {
	paths := gamePaths(t)
	g, err := openGame(filepath.Dir(paths[0]))
	if err != nil {
		t.Fatal(err)
	}
	keptRoot := root
	defer func() { root = keptRoot }()
	shipped := map[string]string{"test-filme": "films", "test-serien": "series", "test-werbung": "ads"}
	own := map[string][]byte{}
	for n, tb := range shipped {
		data, err := os.ReadFile(filepath.Join("..", "tests", "packs", n, tb+".json"))
		if err != nil {
			t.Skipf("no mods/%s", n)
		}
		own[n] = data
	}
	root = t.TempDir()
	write := func(name, file string, data []byte) {
		os.MkdirAll(filepath.Join(root, "mods", name), 0o777)
		if err := os.WriteFile(filepath.Join(root, "mods", name, file), data, 0o666); err != nil {
			t.Fatal(err)
		}
	}
	for n, tb := range shipped {
		write(n, tb+".json", own[n])
		whole, err := orderedTable(g, n, tb)
		if err != nil {
			t.Fatal(err)
		}
		_, list, _ := records(whole.value().(obj))
		_, olist, _ := records(g.original[tb])
		if len(list) <= len(olist) {
			t.Fatalf("%s: %d entries, the original alone %d", n, len(list), len(olist))
		}
		write(n+"-ganz", tb+".json", whole.pyIndent())
		write(n+"-kopie", tb+".json", whole.pyIndent())
		write(n+"-komplett", "mod.json", []byte(`{"complete": true}`))
		write(n+"-komplett", tb+".json", whole.pyIndent())
		body := &onode{kind: 'o'}
		body.set("records", whole.get("records"))
		for _, k := range []string{"-kopie", "-komplett"} {
			if _, err := saveTable(g, n+k, tb, body.pyIndent()); err != nil {
				t.Fatalf("%s%s: %v", n, k, err)
			}
		}
		if got, _ := os.ReadFile(filepath.Join(root, "mods", n+"-kopie", tb+".json")); string(got) != string(own[n]) {
			t.Errorf("%s-kopie saved: %d bytes, not its own entries (%d bytes)", n, len(got), len(own[n]))
		}
		if got, _ := os.ReadFile(filepath.Join(root, "mods", n+"-komplett", tb+".json")); string(got) != string(whole.pyIndent()) {
			t.Errorf("%s-komplett saved: not the whole table", n)
		}
	}
	for _, spec := range []string{"test-filme", "test-serien", "test-werbung", "test-filme+test-serien+test-werbung", "test-werbung+test-filme"} {
		a, _, err := packsDocs(g, spec)
		if err != nil {
			t.Fatal(err)
		}
		b, _, err := packsDocs(g, strings.ReplaceAll(spec, "+", "-ganz+")+"-ganz")
		if err != nil {
			t.Fatal(err)
		}
		for _, tb := range tables {
			if !equal(a[tb.name], b[tb.name]) {
				t.Errorf("%s: %s differs from the packs holding the whole table", spec, tb.name)
			}
		}
	}
}
