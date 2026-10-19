package main

import (
	"os"
	"path/filepath"
	"regexp"
	"strings"
	"testing"
)

// TestSettingsDefaults: settings_defaults.txt is the defaults[] string of
// runtime/settings.c (go test -update writes it from there)
func TestSettingsDefaults(t *testing.T) {
	src, err := os.ReadFile(filepath.Join("..", "runtime", "settings.c"))
	if err != nil {
		t.Skip(err)
	}
	s := string(src)
	i := strings.Index(s, "static const char defaults[] =")
	if i < 0 {
		t.Fatal("runtime/settings.c: no defaults[]")
	}
	body := s[i:]
	if j := strings.Index(body, ";\n"); j >= 0 {
		body = body[:j]
	}
	var want strings.Builder
	for _, lit := range regexp.MustCompile(`"((?:[^"\\]|\\.)*)"`).FindAllStringSubmatch(body, -1) {
		want.WriteString(cUnescape(lit[1]))
	}
	if *update {
		os.WriteFile("settings_defaults.txt", []byte(want.String()), 0o666)
		return
	}
	if want.String() != settingsDefaults {
		t.Fatal("settings_defaults.txt is not runtime/settings.c's defaults[] (go test -update)")
	}
}

// cUnescape: a C string literal's text
func cUnescape(s string) string {
	var b strings.Builder
	for i := 0; i < len(s); i++ {
		if s[i] != '\\' || i+1 >= len(s) {
			b.WriteByte(s[i])
			continue
		}
		i++
		switch c := s[i]; c {
		case 'n':
			b.WriteByte('\n')
		case 't':
			b.WriteByte('\t')
		case 'r':
			b.WriteByte('\r')
		default:
			b.WriteByte(c) // \" \\ \'
		}
	}
	return b.String()
}

// a value the settings file takes is one line: no other lines (a section, a key) put
// in through it -- also when the game is not built and its fixes' names are unknown
func TestSettingOneLine(t *testing.T) {
	keptRoot := root
	defer func() { root = keptRoot }()
	root = t.TempDir() // (no build/mtv-dec: gameFixes knows no names)
	fixesMu.Lock()
	keptFixes := fixesList
	fixesList = nil
	fixesMu.Unlock()
	defer func() { fixesMu.Lock(); fixesList = keptFixes; fixesMu.Unlock() }()
	if len(gameFixes()) != 0 {
		t.Fatal("gameFixes: names without the game")
	}
	for _, c := range []struct {
		sec, key string
		val      any
		want     string // "": refused
	}{
		{"game", "fixes", "all\n[input]\nmouse = captured", ""},
		{"game", "fixes", "intruder\r\n[input]", ""},
		{"game", "fixes", "intruder,mouse = captured", ""},
		{"game", "fixes", "intruder, ai-studio", "intruder,ai-studio"},
		{"game", "fixes", "Intruder", ""},
		{"game", "fixes", "all", "all"},
		{"game", "fixes", " none ", "none"},
		{"display", "fullscreen", "yes\n", ""},
		{"display", "filter", "linear\r", ""},
		{"display", "scale", "4", "4"},
	} {
		got, err := checkSetting(c.sec, c.key, c.val)
		if c.want == "" && err == nil {
			t.Errorf("%s.%s %q: taken as %q", c.sec, c.key, c.val, got)
		}
		if c.want != "" && (err != nil || got != c.want) {
			t.Errorf("%s.%s %q: %q, %v (not %q)", c.sec, c.key, c.val, got, err, c.want)
		}
	}
}

// the shared settings (the display, the sound) go from one file into the other -- the mouse,
// the game's rules and the role fit stay each file's own
func TestSyncShared(t *testing.T) {
	d := t.TempDir()
	sp, mp := filepath.Join(d, "settings.ini"), filepath.Join(d, "multiplayer.ini")
	if err := writeSettings([]settingChange{{"display", "filter", "mmpx"}, {"display", "fullscreen", "yes"}, {"sound", "sound", "off"},
		{"display", "ai_pictures", "ki/rebout.ki"}, {"display", "cast_fit", "yes"}, {"input", "mouse", "free"}, {"game", "start_money", "500000"}}, sp); err != nil {
		t.Fatal(err)
	}
	if err := writeSettings([]settingChange{{"input", "mouse", "system"}, {"game", "fixes", "none"}}, mp); err != nil {
		t.Fatal(err)
	}
	get := func(p, sec, key string) string {
		d, err := readSettings(p)
		if err != nil {
			t.Fatal(err)
		}
		return d["values"].(map[string]map[string]string)[sec][key]
	}
	if err := syncShared(sp, mp); err != nil {
		t.Fatal(err)
	}
	for _, c := range [][3]string{{"display", "filter", "mmpx"}, {"display", "fullscreen", "yes"}, {"sound", "sound", "off"}, {"display", "ai_pictures", "ki/rebout.ki"},
		{"display", "cast_fit", "no"}, {"input", "mouse", "system"}, {"game", "start_money", "200000"}, {"game", "fixes", "none"}} {
		if v := get(mp, c[0], c[1]); v != c[2] {
			t.Errorf("into the network games' file: %s.%s = %q, not %q", c[0], c[1], v, c[2])
		}
	}
	if err := writeSettings([]settingChange{{"display", "fullscreen", "no"}}, mp); err != nil { // (F11 in a network game)
		t.Fatal(err)
	}
	if err := syncShared(mp, sp); err != nil {
		t.Fatal(err)
	}
	if v, m := get(sp, "display", "fullscreen"), get(sp, "input", "mouse"); v != "no" || m != "free" {
		t.Errorf("back: fullscreen %q (not no), mouse %q (not free)", v, m)
	}
	if err := syncShared(filepath.Join(d, "none.ini"), mp); err != nil || exists(filepath.Join(d, "none.ini")) {
		t.Errorf("from a file not there: %v", err)
	}
}
