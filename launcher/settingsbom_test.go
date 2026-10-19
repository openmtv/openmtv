package main

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
)

// TestSettingsBOM: a settings file with a UTF-8 byte order mark and CRLF (Windows'
// Notepad) reads like the plain one -- its first section counts -- and a change is
// written into its line, the mark kept (runtime/settings.c alike: tests/settings_test.c)
func TestSettingsBOM(t *testing.T) {
	p := filepath.Join(t.TempDir(), "settings.ini")
	text := "\uFEFF[display]\r\nfullscreen = yes\r\nscale = 5\r\n[game]\r\nfixes = all\r\n"
	if err := os.WriteFile(p, []byte(text), 0o666); err != nil {
		t.Fatal(err)
	}
	if v := settingsValue(p, "display", "scale"); v != "5" {
		t.Errorf("settingsValue display.scale = %q, not 5", v)
	}
	d, err := readSettings(p)
	if err != nil {
		t.Fatal(err)
	}
	vals := d["values"].(map[string]map[string]string)
	if v := vals["display"]["fullscreen"]; v != "yes" {
		t.Errorf("readSettings display.fullscreen = %q, not yes", v)
	}
	if _, ok := vals["\uFEFFdisplay"]; ok {
		t.Errorf("readSettings: a section named with the byte order mark")
	}
	if err := writeSettings([]settingChange{{"display", "scale", "4"}}, p); err != nil {
		t.Fatal(err)
	}
	got, _ := os.ReadFile(p)
	want := strings.Replace(text, "scale = 5\r\n", "scale = 4\n", 1)
	if string(got) != want {
		t.Errorf("written:\n%q\nwant:\n%q", got, want)
	}
}
