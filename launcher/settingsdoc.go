package main

import (
	"context"
	_ "embed"
	"encoding/json"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
	"sync"
	"time"
)

// The settings file for the launcher page, as the former Python settings module (gamesettings.py) has it: read
// over the game's defaults, each value checked against what the game takes,
// written in place (comments, order and unknown keys stay).

type settingKind struct {
	section, key, kind string
	opts               []any // choice/bool: the values (the first the default); int: default, min, max (, step (, a word it also takes))
}

var schema = []settingKind{
	{"display", "fullscreen", "bool", []any{"no", "yes"}},
	{"display", "scale", "int", []any{3, 1, 8}},
	{"display", "aspect", "choice", []any{"4:3", "square"}},
	{"display", "filter", "choice", []any{"sharp", "nearest", "linear", "mmpx", "hr-pixel"}}, // (hr went 2026-10-18; the game reads it as hr-pixel)
	{"display", "integer", "bool", []any{"no", "yes"}},
	{"display", "cast_fit", "bool", []any{"no", "yes"}},
	{"display", "ai_pictures", "path", []any{""}}, // a picture set (ki.go), empty: none
	{"game", "language", "choice", []any{"english", "german", "ask"}},
	{"game", "classic", "bool", []any{"no", "yes"}},
	{"game", "fixes", "fixes", []any{"all"}},
	{"game", "start_money", "int", []any{200000, 0, 100000000}},
	{"game", "credit_line", "int", []any{700000, 300000, 100000000, 100000, "original"}}, // original: the original's credit, fix credit-repay off
	{"game", "terror_damage", "int", []any{50000, 0, 100000000}},
	{"game", "sammy_news", "int", []any{0, 0, 100000000}},
	{"game", "sammy_culture", "int", []any{0, 0, 100000000}},
	{"game", "sammy_shows", "int", []any{0, 0, 100000000}},
	{"game", "sammy_series", "int", []any{0, 0, 100000000}},
	{"game", "fourth_speed", "bool", []any{"no", "yes"}},
	{"game", "fast_rooms", "bool", []any{"no", "yes"}},
	{"game", "series_topicality", "bool", []any{"yes", "no"}},
	{"game", "ai_improve", "choice", []any{"off", "normal", "strong"}},
	{"game", "ad_offers", "int", []any{int(offersLow), int(offersLow), int(offersHigh)}}, // (lobby.go)
	{"game", "film_offers", "int", []any{int(offersLow), int(offersLow), int(offersHigh)}},
	{"sound", "sound", "bool", []any{"on", "off"}},
	{"input", "mouse", "choice", []any{"captured", "free", "system"}},
}

var (
	fixNameRE = regexp.MustCompile(`\A[a-z0-9-]+\z`) // a fix's name (src/fixes.c)
	boolTrue  = []string{"yes", "on", "true", "1"}
	boolFalse = []string{"no", "off", "false", "0"}
)

// settingsDefaults: the game's defaults with their comments -- the defaults[] string of
// runtime/settings.c, kept in settings_defaults.txt (settings_test.go checks it is
// that string; go test -update writes it anew)
//
//go:embed settings_defaults.txt
var settingsDefaults string

func defaultText() (string, error) { return settingsDefaults, nil }

// sectioned: {section: {key: value}} of a settings text; the last one counts, like the game
func sectioned(text string) map[string]map[string]string {
	out := map[string]map[string]string{}
	sec := ""
	for _, line := range strings.Split(strings.ReplaceAll(trimBOM(text), "\r\n", "\n"), "\n") {
		s := strings.TrimSpace(line)
		if s == "" || s[0] == '#' || s[0] == ';' {
			continue
		}
		if s[0] == '[' {
			if i := strings.IndexByte(s, ']'); i >= 0 {
				sec = strings.TrimSpace(s[1:i])
			}
			continue
		}
		if k, v, ok := strings.Cut(s, "="); ok {
			if out[sec] == nil {
				out[sec] = map[string]string{}
			}
			out[sec][strings.TrimSpace(k)] = strings.TrimSpace(v)
		}
	}
	return out
}

// readSettings: {path, exists, values} -- the file over the game's defaults
func readSettings(p string) (obj, error) {
	def, err := defaultText()
	if err != nil {
		return nil, err
	}
	values := sectioned(def)
	data, err := os.ReadFile(p)
	exists := err == nil
	if exists {
		for sec, kv := range sectioned(strings.ToValidUTF8(string(data), "�")) {
			if values[sec] == nil {
				values[sec] = map[string]string{}
			}
			for k, v := range kv {
				values[sec][k] = v
			}
		}
	}
	return obj{"path": p, "exists": exists, "values": values}, nil
}

var (
	fixesMu   sync.Mutex
	fixesList [][2]string // (kept once the game said them: a game not built yet is asked again)
)

// gameFixes: [(name, description)] of build/mtv-dec --fixes=list
func gameFixes() [][2]string {
	fixesMu.Lock()
	defer fixesMu.Unlock()
	if fixesList != nil {
		return fixesList
	}
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()
	c := exec.CommandContext(ctx, program("mtv-dec"), "--fixes=list")
	hideWindow(c)
	out, err := c.Output()
	if err != nil {
		return nil
	}
	list := [][2]string{}
	for _, l := range strings.Split(strings.ReplaceAll(string(out), "\r\n", "\n"), "\n") {
		if strings.TrimSpace(l) == "" {
			continue
		}
		f := strings.Fields(l)
		if len(f) >= 2 {
			name := f[0]
			rest := strings.TrimSpace(strings.TrimLeft(l, " \t")[len(name):])
			list = append(list, [2]string{name, rest})
		} else {
			list = append(list, [2]string{strings.TrimSpace(l), ""})
		}
	}
	fixesList = list
	return list
}

// settingValue: what a value of the page reads as text (Python's str())
func settingValue(v any) string {
	switch x := v.(type) {
	case string:
		return x
	case json.Number:
		return x.String()
	case bool:
		if x {
			return "True"
		}
		return "False"
	case nil:
		return "None"
	}
	return fmt.Sprint(v)
}

func isDigits(s string) bool {
	if s == "" {
		return false
	}
	for _, c := range s {
		if c < '0' || c > '9' {
			return false
		}
	}
	return true
}

// checkSetting: the value as the file stores it, or the message why the game does not take it
func checkSetting(sec, key string, v any) (string, error) {
	for _, k := range schema {
		if k.section != sec || k.key != key {
			continue
		}
		val := strings.TrimSpace(settingValue(v))
		bad := newMsg("{setting}: \"{value}\" is not a value the game takes", "setting", sec+"."+key, "value", val)
		if strings.ContainsAny(settingValue(v), "\r\n") { // (one line of the file: no section or key of its own put in with it)
			return "", bad
		}
		switch k.kind {
		case "bool":
			onoff := k.opts[0] == "on" || k.opts[0] == "off"
			if contains(boolTrue, strings.ToLower(val)) {
				if onoff {
					return "on", nil
				}
				return "yes", nil
			}
			if contains(boolFalse, strings.ToLower(val)) {
				if onoff {
					return "off", nil
				}
				return "no", nil
			}
		case "int":
			if len(k.opts) > 4 && val == k.opts[4] {
				return val, nil
			}
			if isDigits(val) {
				n, err := strconv.ParseInt(val, 10, 64)
				lo, hi := int64(k.opts[1].(int)), int64(k.opts[2].(int))
				if err == nil && lo <= n && n <= hi && (len(k.opts) < 4 || n%int64(k.opts[3].(int)) == 0) {
					return strconv.FormatInt(n, 10), nil
				}
			}
		case "choice":
			for _, o := range k.opts {
				if o == val {
					return val, nil
				}
			}
		case "path": // (any one line: the game finds out whether it is one)
			if !strings.ContainsAny(val, "\r\n") {
				return val, nil
			}
		case "fixes":
			if val == "all" || val == "none" {
				return val, nil
			}
			var names []string
			for _, x := range strings.Split(val, ",") {
				if x = strings.TrimSpace(x); x != "" {
					names = append(names, x)
				}
			}
			known := map[string]bool{}
			for _, f := range gameFixes() {
				known[f[0]] = true
			}
			all := true
			for _, n := range names {
				all = all && known[n]
				if !fixNameRE.MatchString(n) { // (the game not built, its names unknown: at least a name's letters)
					return "", bad
				}
			}
			if len(names) > 0 && (len(known) == 0 || all) {
				return strings.Join(names, ","), nil
			}
			if len(names) == 0 {
				return "none", nil
			}
		}
		return "", bad
	}
	return "", newMsg("no setting {setting}", "setting", sec+"."+key)
}

type settingChange struct{ sec, key, val string }

// writeSettings: each value's line rewritten (or added at the end of its section, or
// the section at the end), everything else as it was; a missing file starts from
// the game's defaults.  CHANGES in the order the page sent them
func writeSettings(changes []settingChange, p string) error {
	settingsMu.Lock()
	defer settingsMu.Unlock()
	return writeSettingsLocked(changes, p)
}

// settingsMu: the settings files' read-modify-write, one at a time (two of the page's
// changes at once both kept)
var settingsMu sync.Mutex

// writeSettingsLocked: writeSettings, settingsMu held
func writeSettingsLocked(changes []settingChange, p string) error {
	var text string
	if data, err := os.ReadFile(p); err == nil {
		text = strings.ToValidUTF8(string(data), "�")
	} else if text, err = defaultText(); err != nil {
		return err
	}
	lines := splitKeepEnds(text)
	for _, c := range changes {
		lines = setLine(lines, c.sec, c.key, c.val)
	}
	if err := os.MkdirAll(filepath.Dir(p), 0o777); err != nil {
		return err
	}
	return replaceFile(p, []byte(strings.Join(lines, "")))
}

func splitKeepEnds(s string) []string {
	var out []string
	for s != "" {
		i := strings.IndexByte(s, '\n')
		if i < 0 {
			out = append(out, s)
			break
		}
		out = append(out, s[:i+1])
		s = s[i+1:]
	}
	return out
}

func setLine(lines []string, sec, key, val string) []string {
	cur, lastInSec, found := "", -1, -1
	for i, line := range lines {
		s := strings.TrimSpace(line)
		if i == 0 {
			s = strings.TrimSpace(trimBOM(line))
		}
		if strings.HasPrefix(s, "[") && strings.Contains(s, "]") {
			cur = strings.TrimSpace(s[1:strings.IndexByte(s, ']')])
			continue
		}
		if cur == sec && s != "" && s[0] != '#' && s[0] != ';' && strings.Contains(s, "=") {
			lastInSec = i
			if k, _, _ := strings.Cut(s, "="); strings.TrimSpace(k) == key {
				found = i
			}
		}
	}
	line := key + " = " + val + "\n"
	out := append([]string{}, lines...)
	switch {
	case found >= 0:
		out[found] = line
		return out
	case lastInSec >= 0:
		return append(out[:lastInSec+1], append([]string{line}, lines[lastInSec+1:]...)...)
	}
	tail := ""
	if len(lines) > 0 && !strings.HasSuffix(lines[len(lines)-1], "\n") {
		tail = "\n"
	}
	return append(out, tail+"\n["+sec+"]\n", line)
}

// settingsDoc: the settings file for the launcher page, with what the page needs beside
// it -- the schema, the game's fixes, how the start is written
func settingsDoc(p string) (obj, error) {
	d, err := readSettings(p)
	if err != nil {
		return nil, err
	}
	sch := make([]obj, len(schema))
	for i, k := range schema {
		sch[i] = obj{"section": k.section, "key": k.key, "kind": k.kind, "opts": k.opts}
	}
	fixes := []obj{}
	for _, f := range gameFixes() {
		fixes = append(fixes, obj{"name": f[0], "desc": f[1]})
	}
	d["schema"], d["fixes"], d["play"] = sch, fixes, play
	return d, nil
}
