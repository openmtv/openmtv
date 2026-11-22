package main

import (
	_ "embed"
	"fmt"
	"hash/fnv"
	"sync"
)

// The rule "Unfertige Drehbücher integrieren" ([game] original_screenplays, --original-screenplays;
// the game: src/origscripts.c): the original's unfinished screenplays -- the shows 25-33,
// the series 37-50 -- filled and offered by the screenplay agency.  The game fills them
// while it plays, record by record: a screenplay whose part a savegame does not hold is the
// original's (a pack's own one stays its own), a series and a film whose values are the
// original's.  The editor shows the tables so when the rule is on (its requests carry
// "layer"), with the same checks -- the original's, a pack's, a selection's (layered) --
// and a pack saved there gets back the plain record wherever the page sent the layered one
// unchanged (unlayer): packs are stored without the rule, their builds too, so a pack plays
// the same with the rule on or off.
//
// origscripts.json comes from tools/origscripts.py, as the game's src/origscripts_data.h:
// per screenplay the hash (FNV-1a) of the original's part a savegame does not hold, the
// record as the rule has it and its texts (null: the game's own; a text of the rule's
// only where the original's is empty); the series' and films' values.

//go:embed origscripts.json
var origScriptsJSON []byte

var (
	spHead           = [][2]int{{0, 4}, {5, 8}, {0x166, 0x1B3}}
	origScriptsOnce  sync.Once
	origScriptsData  origScripts
	origScriptsErr   error
	origScriptTables = []string{"screenplays", "series", "films"}
)

type origScripts struct {
	screenplays []obj
	series      []obj
	films       []obj
}

func loadOrigScripts() (origScripts, error) {
	origScriptsOnce.Do(func() {
		v, err := parseJSON(origScriptsJSON)
		if err != nil {
			origScriptsErr = err
			return
		}
		d, _ := v.(obj)
		list := func(k string) []obj {
			var out []obj
			for _, x := range anyList(d[k]) {
				if o, ok := x.(obj); ok {
					out = append(out, o)
				}
			}
			return out
		}
		origScriptsData = origScripts{list("screenplays"), list("series"), list("films")}
	})
	return origScriptsData, origScriptsErr
}

func anyList(v any) []any {
	l, _ := v.([]any)
	return l
}

// spHeadHash: the hash of a screenplay's part a savegame does not hold, as the game takes it
func spHeadHash(b []byte) string {
	h := fnv.New32a()
	for _, r := range spHead {
		h.Write(b[r[0]:r[1]])
	}
	return fmt.Sprintf("%08x", h.Sum32())
}

// layerOrigScripts: a copy of tables (DOCS: the original's, or a pack's as it plays) with
// the rule's series, films and screenplays -- each only while it is the original's (a
// series screenplay only with a series to produce: the rule's, or one with episodes of a
// length); the numbers of the screenplays it filled
func layerOrigScripts(docs map[string]obj) (map[string]obj, []int64, error) {
	o, err := loadOrigScripts()
	if err != nil {
		return nil, nil, err
	}
	out := map[string]obj{}
	for k, v := range docs {
		out[k] = v
	}
	for _, t := range origScriptTables {
		if d, ok := docs[t]; ok {
			out[t] = deepCopy(d).(obj)
		}
	}
	byID := func(t string) map[int64]obj {
		m, _, _ := records(out[t])
		return m
	}
	sps, series, films := byID("screenplays"), byID("series"), byID("films")
	patched := map[int64]bool{}
	for _, p := range o.series {
		r, ok := series[intOf(p, "id")]
		if !ok || intOf(r, "genre") != intOf(p, "genre") || intOf(r, "episodes") != intOf(p, "episodes_orig") ||
			intOf(r, "blocks") != 0 || intOf(r, "flags")&0x1F != 0 || intOf(r, "episode_price") != 0 ||
			intOf(r, "critics") != 0 || intOf(r, "press") != 0 { // (as the game: not what a savegame holds -- the price is set where it is 0)
			continue
		}
		r["episodes"] = intOf(p, "episodes")
		r["blocks"] = intOf(p, "blocks")
		r["flags"] = intOf(r, "flags")&^0x1F | intOf(p, "family")
		r["episode_price"] = intOf(p, "episode_price")
		r["critics"] = intOf(p, "critics")
		r["press"] = intOf(p, "press")
		if intOf(r, "price") == 0 {
			r["price"] = intOf(p, "price")
		}
		patched[intOf(p, "id")] = true
	}
	for _, p := range o.films {
		r, ok := films[intOf(p, "id")]
		if !ok || intOf(r, "genre") != intOf(p, "genre") || intOf(r, "flags2")&^3 != intOf(p, "flags2_orig")&^3 { // (bits 0, 1: a savegame's)
			continue
		}
		r["flags2"] = intOf(r, "flags2")&3 | intOf(p, "flags2")&^3
	}
	spt := tables[tableIdx["screenplays"]]
	var filled []int64
	for _, s := range o.screenplays {
		id := intOf(s, "id")
		r, ok := sps[id]
		if !ok {
			continue
		}
		b, err := packRecord(spt, id, r)
		if err != nil || spHeadHash(b) != str(s["head"]) {
			continue
		}
		rec, _ := deepCopy(s["record"]).(obj)
		if intOf(rec, "flags")&1 != 0 { // a series production: its series first
			t := intOf(rec, "target")
			if sr := series[t]; !patched[t] && (sr == nil || intOf(sr, "episodes") == 0 || intOf(sr, "blocks") == 0) {
				continue
			}
		}
		text, _ := r["text"].(obj)
		ours, _ := s["text"].(obj)
		for k := range r {
			if k != "id" && k != "text" {
				delete(r, k)
			}
		}
		for k, v := range rec {
			r[k] = v
		}
		for _, lang := range langs {
			cur, _ := text[lang].([]any)
			mine := anyList(ours[lang])
			if cur == nil || len(mine) != len(cur) {
				continue
			}
			for k, v := range mine {
				if t, ok := v.(string); ok && str(cur[k]) == "" {
					cur[k] = t
				}
			}
		}
		filled = append(filled, id)
	}
	return out, filled, nil
}

// gameLayer: the original's tables with the rule's screenplays, made the first time the
// editor asks
type gameLayer struct {
	once sync.Once
	docs map[string]obj
	ids  []int64 // the screenplays it filled
}

// layered: the original's tables with the rule's screenplays
func (g *game) layered() map[string]obj {
	if g.lay == nil { // (a game made without openGame)
		return g.original
	}
	g.lay.once.Do(func() {
		docs, filled, err := layerOrigScripts(g.original)
		if err != nil {
			docs, filled = g.original, nil
		}
		g.lay.docs, g.lay.ids = docs, filled
	})
	return g.lay.docs
}

// layerIDs: the screenplays the rule fills in this game's original (the editor's note)
func (g *game) layerIDs() []int64 {
	g.layered()
	if g.lay == nil || g.lay.ids == nil {
		return []int64{}
	}
	return g.lay.ids
}

// layerOrdered: table T (N: as its file has it, in its order) with the records LAYERED
// (that table of layerOrigScripts) has otherwise -- the rest of N as it is
func layerOrdered(t string, n *onode, layered obj) *onode {
	l, _, err := records(layered)
	if n == nil || err != nil || n.get("records") == nil {
		return n
	}
	items := append([]*onode(nil), n.get("records").items...)
	swapped := false
	for i, it := range items {
		rec, ok := it.value().(obj)
		if !ok {
			continue
		}
		id, _ := rec["id"].(int64)
		if lr, ok := l[id]; ok && !equal(rec, lr) {
			items[i] = canonical(t, obj{"records": []any{lr}}).get("records").items[0]
			swapped = true
		}
	}
	if !swapped {
		return n
	}
	out := n.copy()
	out.set("records", &onode{kind: 'a', items: items})
	return out
}

// layerSelection: the tables of a pack or a selection (ordered, as the editor gets them)
// with the rule's screenplays, as the game plays them
func layerSelection(docs map[string]*onode) map[string]*onode {
	plain := map[string]obj{}
	for _, t := range origScriptTables {
		if n := docs[t]; n != nil {
			if v, ok := n.value().(obj); ok {
				plain[t] = v
			}
		}
	}
	lay, _, err := layerOrigScripts(plain)
	if err != nil {
		return docs
	}
	out := map[string]*onode{}
	for k, v := range docs {
		out[k] = v
	}
	for _, t := range origScriptTables {
		if n := docs[t]; n != nil {
			out[t] = layerOrdered(t, n, lay[t])
		}
	}
	return out
}

// unlayer: the records of table T the page sent (SENT, the layered view) with the plain
// record back wherever it sent the layered one unchanged -- BEFORE: the pack's tables as
// they play without the rule (its own records and the original's)
func unlayer(t string, sent *onode, before map[string]obj) *onode {
	if sent == nil || !contains(origScriptTables, t) {
		return sent
	}
	lay, _, err := layerOrigScripts(before)
	if err != nil {
		return sent
	}
	b, _, _ := records(before[t])
	l, _, _ := records(lay[t])
	items := append([]*onode(nil), sent.items...)
	swapped := false
	for i, it := range items {
		rec, ok := it.value().(obj)
		if !ok {
			continue
		}
		id, _ := rec["id"].(int64)
		if lr, pr := l[id], b[id]; lr != nil && pr != nil && !equal(lr, pr) && equal(rec, lr) {
			items[i] = canonical(t, obj{"records": []any{pr}}).get("records").items[0]
			swapped = true
		}
	}
	if !swapped {
		return sent
	}
	return &onode{kind: 'a', items: items}
}
