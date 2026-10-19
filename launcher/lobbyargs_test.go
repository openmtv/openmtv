package main

import (
	"strings"
	"testing"
)

// TestLobbyCommandHostSettings: the host's language and fixes reach the game's command
// line only as what they are -- no other option through them
func TestLobbyCommandHostSettings(t *testing.T) {
	l := newLobby(t.TempDir(), "test")
	l.address = "127.0.0.1:1"
	for _, c := range []struct {
		lang, fixes any
		want        []string // the command line's first arguments
	}{
		{"d", "all", []string{"d", "--fixes"}},
		{"e", "none", []string{"e", "--classic"}},
		{nil, nil, []string{"e", "--fixes"}},
		{"d", "intruder,random0,ai-studio", []string{"d", "--fixes=intruder,random0,ai-studio"}},
		{"--mod=../../x", "all", []string{"e", "--fixes"}},
		{"--settings=/etc/passwd", "all", []string{"e", "--fixes"}},
		{int64(3), "list", []string{"e", "--fixes"}},
		{"e", "intruder --mp-load x", []string{"e", "--fixes"}},
		{"e", "intruder,,random0", []string{"e", "--fixes"}},
		{"e", "x=y", []string{"e", "--fixes"}},
		{"e", strings.Repeat("intruder,", 60) + "intruder", []string{"e", "--fixes"}}, // (too long)
	} {
		args, err := l.command(obj{"t": "go", "seat": int64(0), "humans": "1,2", "names": []any{"A", "B", ""}, "seed": int64(7),
			"settings": obj{"lang": c.lang, "fixes": c.fixes}})
		if err != nil {
			t.Fatal(err)
		}
		if strings.Join(args[:2], " ") != strings.Join(c.want, " ") {
			t.Errorf("lang %v fixes %v: %q", c.lang, c.fixes, args[:2])
		}
		if _, err := parse(args); err != nil {
			t.Errorf("lang %v fixes %v: %v", c.lang, c.fixes, err)
		}
	}
}
