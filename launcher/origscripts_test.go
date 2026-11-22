package main

import (
	"encoding/hex"
	"os"
	"os/exec"
	"path/filepath"
	"testing"
)

// The rule "Unfertige Drehbücher integrieren" in the editor (origscripts.go): the original's tables with
// its screenplays, series and films, record by record as the game does it; the original
// itself untouched; a pack's own screenplay stays its own; a series screenplay only with a
// series to produce; a pack saved with the rule gets the plain records back.

func rec(t *testing.T, docs map[string]obj, table string, id int64) obj {
	t.Helper()
	m, _, err := records(docs[table])
	if err != nil || m[id] == nil {
		t.Fatalf("%s %d: not there (%v)", table, id, err)
	}
	return m[id]
}

func copyTables(docs map[string]obj) map[string]obj {
	out := map[string]obj{}
	for k, v := range docs {
		out[k] = deepCopy(v).(obj)
	}
	return out
}

func TestOrigScriptsLayer(t *testing.T) {
	g, err := openGame(filepath.Dir(gamePaths(t)[0]))
	if err != nil {
		t.Fatal(err)
	}
	l := g.layered()
	if len(g.layerIDs()) != 23 {
		t.Fatalf("the rule filled %d screenplays, not 23", len(g.layerIDs()))
	}
	s30, o30 := rec(t, l, "screenplays", 30), rec(t, g.original, "screenplays", 30)
	if intOf(s30, "flags") != 6 || intOf(s30, "price") != 480000 || intOf(s30, "duration_hours") != 14 {
		t.Errorf("screenplay 30 (Golden Highlight): flags %v price %v time %v", s30["flags"], s30["price"], s30["duration_hours"])
	}
	if intOf(o30, "flags") != 0 || intOf(o30, "price") != 0 {
		t.Errorf("the original's screenplay 30 changed: flags %v price %v", o30["flags"], o30["price"])
	}
	// a show's ten productions: each block the first one's (as script_init_episodes copies them)
	if b, err := packRecord(tables[tableIdx["screenplays"]], 30, s30); err != nil || hex.EncodeToString(b[8:8+0x23]) != hex.EncodeToString(b[8+9*0x23:8+10*0x23]) || b[8] != 64 {
		t.Errorf("screenplay 30: the tenth production's list is not the first's (%v)", err)
	}
	tx := func(r obj, lang string, k int) string { return str(anyList(r["text"].(obj)[lang])[k]) }
	s25 := rec(t, l, "screenplays", 25)
	if tx(s25, "de", 0) != tx(rec(t, g.original, "screenplays", 25), "de", 0) || tx(s25, "de", 0) == "" ||
		tx(s25, "en", 0) != "Contestants have to guess words and can win a lot of money." {
		t.Errorf("screenplay 25: the German text the original's, the English the rule's: %q / %q", tx(s25, "de", 0), tx(s25, "en", 0))
	}
	if s37 := rec(t, l, "screenplays", 37); tx(s37, "de", 1) != "Ben Williams, Treckführer" || intOf(s37, "flags") != 3 {
		t.Errorf("screenplay 37: role 1 %q, flags %v", tx(s37, "de", 1), s37["flags"])
	}
	if s := rec(t, l, "series", 44); intOf(s, "episodes") != 8 || intOf(s, "blocks") != 1 || intOf(s, "episode_price") != 40000 {
		t.Errorf("series 44 (Mary & John): episodes %v blocks %v episode price %v", s["episodes"], s["blocks"], s["episode_price"])
	}
	if f := rec(t, l, "films", 140); intOf(f, "flags2") != 12 {
		t.Errorf("film 140 (Die Massaker-Show): flags2 %v, not X rated", f["flags2"])
	}
	if f := rec(t, g.original, "films", 140); intOf(f, "flags2") != 4 {
		t.Errorf("the original's film 140 changed: flags2 %v", f["flags2"])
	}
	// a pack's own screenplay 30 (its price) stays its own; a pack that changed series 44's
	// press but left it 0 episodes: no screenplay 44 (no "Divide error"); the rest filled
	docs := copyTables(g.original)
	m, _, _ := records(docs["screenplays"])
	m[30]["price"] = int64(1)
	ms, _, _ := records(docs["series"])
	ms[44]["press"] = int64(3)
	out, filled, err := layerOrigScripts(docs)
	if err != nil {
		t.Fatal(err)
	}
	if p := rec(t, out, "screenplays", 30); intOf(p, "price") != 1 || intOf(p, "flags") != 0 {
		t.Errorf("a pack's own screenplay 30: price %v flags %v", p["price"], p["flags"])
	}
	if p := rec(t, out, "screenplays", 44); intOf(p, "flags") != 1 || len(filled) != 21 {
		t.Errorf("series 44 without episodes: screenplay 44 flags %v (not offered: 1), %d filled (21)", p["flags"], len(filled))
	}
	// a pack's table as the editor shows it: its plain records layered, its own as they are
	n := canonical("screenplays", docs["screenplays"])
	shown := layerOrdered("screenplays", n, out["screenplays"])
	got, _, _ := records(shown.value().(obj))
	if intOf(got[31], "flags") != 2|4 || intOf(got[30], "price") != 1 {
		t.Errorf("a pack shown with the rule: 31 flags %v (6), its own 30 price %v (1)", got[31]["flags"], got[30]["price"])
	}
	// saved with the rule: what the page sent unchanged goes back to the plain record,
	// a change stays the pack's own
	sent := shown.get("records").copy()
	for _, it := range sent.items {
		if r, _ := it.value().(obj); intOf(r, "id") == 32 {
			r["price"] = int64(99000)
			*it = *canonical("screenplays", obj{"records": []any{r}}).get("records").items[0]
		}
	}
	back := unlayer("screenplays", sent, docs)
	br := map[int64]obj{}
	for _, it := range back.items {
		r, _ := it.value().(obj)
		br[intOf(r, "id")] = r
	}
	if !equal(br[31], rec(t, g.original, "screenplays", 31)) || intOf(br[32], "price") != 99000 || intOf(br[32], "flags") != 6 {
		t.Errorf("saved with the rule: 31 the plain record %v, 32 changed (price %v, flags %v)", equal(br[31], rec(t, g.original, "screenplays", 31)), br[32]["price"], br[32]["flags"])
	}
}

// the tables the game and the launcher take the rule from are tools/origscripts.py's
func TestOrigScriptsTables(t *testing.T) {
	paths := gamePaths(t)
	if _, err := exec.LookPath("python3"); err != nil {
		t.Skip("no python3")
	}
	cmd := exec.Command("python3", filepath.Join("..", "tools", "origscripts.py"), "--check")
	cmd.Env = append(os.Environ(), "GAME="+filepath.Dir(paths[0]))
	if out, err := cmd.CombinedOutput(); err != nil {
		t.Fatalf("tools/origscripts.py --check: %v\n%s", err, out)
	}
}
