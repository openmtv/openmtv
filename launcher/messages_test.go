package main

import (
	"bytes"
	"encoding/json"
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strconv"
	"strings"
	"testing"
)

// The Go program's own words, kept (it is the reference: docs/launcher-ohne-python.md,
// "Nacharbeit"): what packing a pack refuses and how it says so, in English and German
// (testdata/pack-messages.json), and the command line's answers (testdata/cli.json).
// go test -update writes them anew -- then read the change.

var update = flag.Bool("update", false, "write the kept results anew")

func golden(t *testing.T, name string, got map[string]any) {
	t.Helper()
	path := filepath.Join("testdata", name)
	var b bytes.Buffer
	e := json.NewEncoder(&b)
	e.SetEscapeHTML(false)
	e.SetIndent("", " ")
	e.Encode(got)
	if *update {
		os.MkdirAll("testdata", 0o777)
		if err := os.WriteFile(path, b.Bytes(), 0o666); err != nil {
			t.Fatal(err)
		}
		return
	}
	want, err := os.ReadFile(path)
	if err != nil {
		t.Fatalf("%s: %v (go test -update writes it)", path, err)
	}
	var w map[string]any
	json.Unmarshal(want, &w)
	var g map[string]any
	json.Unmarshal(b.Bytes(), &g)
	keys := map[string]bool{}
	for k := range w {
		keys[k] = true
	}
	for k := range g {
		keys[k] = true
	}
	var names []string
	for k := range keys {
		names = append(names, k)
	}
	sort.Strings(names)
	for _, k := range names {
		if fmt.Sprint(w[k]) != fmt.Sprint(g[k]) {
			t.Errorf("%s: %s\n  kept: %v\n  now:  %v", name, k, w[k], g[k])
		}
	}
}

// original: the original's tables (with the data fixes), as the editor has them
func original(t *testing.T) map[string]obj {
	res, err := readResources(gamePaths(t))
	if err != nil {
		t.Fatal(err)
	}
	docs, err := export(res)
	if err != nil {
		t.Fatal(err)
	}
	fixed, _ := applyDataFixes(docs)
	return fixed
}

func copyDocs(docs map[string]obj) map[string]obj {
	out := map[string]obj{}
	for k, v := range docs {
		out[k] = deepCopy(v).(obj)
	}
	return out
}

func record(docs map[string]obj, table string, id int64) obj {
	for _, x := range docs[table]["records"].([]any) {
		if r := x.(obj); r["id"] == id {
			return r
		}
	}
	return nil
}

func TestPackMessages(t *testing.T) {
	orig := original(t)
	got := map[string]any{}
	say := func(name string, err error) {
		if err == nil {
			got[name] = "(no error)"
			return
		}
		got[name] = []string{render(err, "en"), render(err, "de")}
	}
	change := func(name string, f func(d map[string]obj)) {
		d := copyDocs(orig)
		f(d)
		say(name, checkDocs(d))
	}
	setText := func(table string, id int64, lang string, k int, v any) func(map[string]obj) {
		return func(d map[string]obj) { record(d, table, id)["text"].(obj)[lang].([]any)[k] = v }
	}
	set := func(table string, id int64, key string, v any) func(map[string]obj) {
		return func(d map[string]obj) { record(d, table, id)[key] = v }
	}
	change("value out of range", set("films", 5, "quality", int64(300)))
	change("value a fraction", set("films", 5, "price", 1.5))
	change("value a text", set("films", 5, "genre", "x"))
	change("value true", set("films", 5, "blocks", true))
	change("list too short", func(d map[string]obj) {
		r := record(d, "series", 3)
		r["episodes_qtf"] = r["episodes_qtf"].([]any)[:32]
	})
	change("list not a list", set("series", 3, "episodes_qtf", int64(5)))
	change("list value out of range", func(d map[string]obj) { record(d, "ads", 3)["genre_rule"].([]any)[1] = int64(200) })
	change("text over 255 bytes", setText("films", 12, "de", 1, strings.Repeat("x", 300)))
	change("text an episode's", setText("series", 4, "en", 5, strings.Repeat("y", 256)))
	change("text not code page 437", setText("ads", 3, "de", 1, "Preis: 5 €✓✓ ok, ☺"))
	change("text a number", setText("films", 7, "en", 0, int64(7)))
	change("texts a string", func(d map[string]obj) { record(d, "films", 9)["text"].(obj)["de"] = "Titel" })
	change("texts raw, not hex", func(d map[string]obj) { record(d, "films", 8)["text"].(obj)["de"] = obj{"raw": "zz"} })
	change("phrases nine", func(d map[string]obj) {
		r := record(d, "news_topics", 3)
		r["text"].(obj)["de"] = r["text"].(obj)["de"].([]any)[:9]
	})
	change("ad one text", func(d map[string]obj) { record(d, "ads", 4)["text"].(obj)["de"] = []any{"nur einer"} })
	change("headline not code page 437", func(d map[string]obj) { record(d, "news_nodes", 2)["text"].(obj)["en"] = "#1: Börse ☂" })
	change("unknown not hex", set("films", 5, "unknown", obj{"0x0E": "zz"}))
	change("unknown beyond", set("films", 5, "unknown", obj{"0x16": "0000"}))
	change("unknown no offset", set("films", 5, "unknown", obj{"x": "00"}))
	change("number too high", func(d map[string]obj) {
		r := deepCopy(record(d, "screenplays", 5)).(obj)
		r["id"] = int64(60)
		d["screenplays"]["records"] = append(d["screenplays"]["records"].([]any), r)
	})
	change("number too high, growing", func(d map[string]obj) {
		r := deepCopy(record(d, "series", 5)).(obj)
		r["id"] = int64(1000)
		d["series"]["records"] = append(d["series"]["records"].([]any), r)
	})
	change("number negative", func(d map[string]obj) {
		r := deepCopy(record(d, "films", 5)).(obj)
		r["id"] = int64(-3)
		d["films"]["records"] = append(d["films"]["records"].([]any), r)
	})
	change("no records", func(d map[string]obj) { d["ads"]["records"] = "none" })
	change("an entry without its number", func(d map[string]obj) {
		d["ads"]["records"] = append(d["ads"]["records"].([]any), obj{"days": int64(3)})
	})
	change("German texts over 64 KB", func(d map[string]obj) {
		for k := int64(0); k < 600; k++ {
			r := deepCopy(record(d, "films", 1+k%150)).(obj)
			r["id"], r["flags"], r["owner"], r["flags2"] = 201+k, int64(0), int64(0), int64(0)
			r["text"].(obj)["de"].([]any)[1] = strings.Repeat("Eine sehr lange Beschreibung ", 6)
			d["films"]["records"] = append(d["films"]["records"].([]any), r)
		}
	})
	change("ads' texts over 64 KB", func(d map[string]obj) {
		for k := int64(0); k < 940; k++ {
			r := deepCopy(record(d, "ads", 1+k%50)).(obj)
			r["id"], r["fee"], r["holder"] = 51+k, int64(0), int64(0)
			r["text"] = obj{"de": []any{fmt.Sprintf("Werbekunde mit langem Namen Nummer %05d", k), fmt.Sprintf("Ein Slogan, lang genug für die Grenze %05d", k)},
				"en": []any{"A", "B"}}
			d["ads"]["records"] = append(d["ads"]["records"].([]any), r)
		}
	})
	change("no start films", func(d map[string]obj) {
		for _, x := range d["films"]["records"].([]any) {
			r := x.(obj)
			r["flags2"] = intOf(r, "flags2") &^ 0x10
			if intOf(r, "genre") == 6 {
				r["flags"] = intOf(r, "flags") &^ 0x20
			}
		}
	})
	inc := copyDocs(orig)
	r := deepCopy(record(inc, "films", 5)).(obj)
	r["id"], r["genre"], r["flags"] = int64(202), int64(0), intOf(r, "flags")|0x20
	r["text"].(obj)["de"].([]any)[0] = "  "
	inc["films"]["records"] = append(inc["films"]["records"].([]any), r)
	if list := incomplete(inc, orig); len(list) > 0 {
		say("incomplete", newMsg("not playable yet -- {list}", "list", joined{"; ", list}))
	}
	// screenplays offered: an unfinished show and series; one with a list of 4 sets and roles
	// without category or requirements; 20 changed (its two roles without requirements are the
	// original's own gap)
	sp := copyDocs(orig)
	record(sp, "screenplays", 26)["flags"] = int64(2)
	record(sp, "screenplays", 38)["flags"] = int64(3)
	record(sp, "screenplays", 20)["price"] = int64(31000)
	r27 := record(sp, "screenplays", 27)
	r27["flags"], r27["price"] = int64(6), int64(40000)
	r27["text"].(obj)["en"].([]any)[0], r27["text"].(obj)["de"].([]any)[0] = "Wine quiz.", "Weinquiz."
	l27 := "1e000000" + "010500" + "020500" + "110300" + "120300" + "130300" + "140300"
	r27["unknown"].(obj)["0x08"] = l27 + strings.Repeat("00", 0x15E-len(l27)/2)
	// 37: a series whose lists name a role without category or requirements in episode 2 only
	// (the episodes' words stay a series' after the roles)
	r37 := record(sp, "screenplays", 37)
	r37["flags"] = int64(3)
	l37 := "1e000000" + "010500" + strings.Repeat("00", 0x23-7) + "1e000000" + "020500"
	r37["unknown"] = obj{"0x08": l37 + strings.Repeat("00", 0x15E-len(l37)/2)}
	if list := incomplete(sp, orig); len(list) > 0 {
		say("incomplete screenplays", newMsg("not playable yet -- {list}", "list", joined{"; ", list}))
	}
	twice := copyDocs(orig)
	for k := 0; k < 2; k++ {
		r := deepCopy(record(twice, "films", 9)).(obj)
		r["id"] = int64(201)
		twice["films"]["records"] = append(twice["films"]["records"].([]any), r)
	}
	_, _, err := combine(orig, []pack{{"doppelt", twice}})
	say("pack: a number twice", err)
	high, low := copyDocs(orig), copyDocs(orig)
	for _, x := range []struct {
		d  map[string]obj
		id int64
	}{{high, 999}, {low, 201}} {
		r := deepCopy(record(x.d, "films", 9)).(obj)
		r["id"] = x.id
		x.d["films"]["records"] = append(x.d["films"]["records"].([]any), r)
	}
	_, _, err = combine(orig, []pack{{"f999", high}, {"f201", low}})
	say("packs: more than 999", err)
	say("the original", checkDocs(copyDocs(orig)))
	golden(t, "pack-messages.json", got)
}

// TestMessagesHaveGerman: every message's key in the sources (newMsg("...")) has its
// German words in msgDE -- the page shows them in its language
func TestMessagesHaveGerman(t *testing.T) {
	files, _ := filepath.Glob("*.go")
	keyRE := regexp.MustCompile(`newMsg\("((?:[^"\\]|\\.)*)"`)
	for _, f := range files {
		if strings.HasSuffix(f, "_test.go") {
			continue
		}
		src, _ := os.ReadFile(f)
		for _, m := range keyRE.FindAllStringSubmatch(string(src), -1) {
			key, err := strconv.Unquote(`"` + m[1] + `"`)
			if err != nil {
				t.Fatalf("%s: %s: %v", f, m[1], err)
			}
			if _, ok := msgDE[key]; !ok {
				t.Errorf("%s: no German for %q", f, key)
			}
		}
	}
}

// TestMessageFilledOnce: a value is filled in as it is, its braces not read again (Python's
// str.format), in any order of the values
func TestMessageFilledOnce(t *testing.T) {
	m := newMsg("{a} and {b} {unknown}", "a", "{b}", "b", newMsg("{setting}: \"{value}\" is not a value the game takes", "setting", "x.y", "value", "{a}"))
	for i := 0; i < 50; i++ {
		if got, want := m.in("de"), "{b} and x.y: \"{a}\" nimmt das Spiel nicht {unknown}"; got != want {
			t.Fatalf("%q, want %q", got, want)
		}
	}
}
